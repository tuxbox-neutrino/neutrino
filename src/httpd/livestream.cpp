/*
 * livestream.cpp - an answer whose body is a running program's output
 *
 * Copyright (C) 2026 NI-Team
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

/* POLLRDHUP, which says that the peer has closed its end. It is Linux's and
   glibc only offers it under this name, which has to be asked for before
   anything pulls in the header. This program runs on Linux and nowhere else. */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE 1
#endif

#include "livestream.h"

#include "http.h"
#include "status.h"

#include "coreapi/base/errors.h"

#include <cerrno>
#include <cstdio>
#include <cstring>
#include <vector>

#include <fcntl.h>
#include <poll.h>
#include <pthread.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#include <OpenThreads/Mutex>
#include <OpenThreads/ScopedLock>

namespace httpd
{

namespace livestream
{

namespace
{

/* HOW MANY MAY RUN AT ONCE, and why it is this small.

   Every listener costs a converter for as long as they listen, because almost all
   German radio on a satellite is Layer II and no browser decodes it. So this is not
   a burst that passes: it is a steady load one person adds by opening a page.

   The number is two. What it buys is that a box which is recording and showing a
   picture stays one: the weakest boxes this program runs on have two cores, and a
   third converter is the one that starts costing the television. A refusal that says
   so is a page that tells somebody to wait; a box that accepts everybody is a picture
   that stutters and nothing anywhere saying why.

   Not a setting, because a setting is a number somebody raises once and never looks
   at again on the box where it was already too high. */
const size_t kMaxSessions = 2;

/* The most one read hands the library. A transport stream at eight megabits is
   a megabyte a second, so this is read from the pipe a few dozen times a
   second and never sits in this process. */
const size_t kBlockSize = 32768;

/* How long the watcher waits before it looks again even though nothing woke
   it. Only reached while every session is being read as fast as it is written,
   so it costs one wakeup a second on an idle box. */
const int kWatchIdleMs = 1000;

// How long a program is given to end on its own after it has been asked to,
// before it is ended for it. It is being asked because nobody is reading it
// any more, so there is nothing to lose by being quick.
const unsigned kEndWaitMs = 200;
const unsigned kEndStepMs = 10;

struct Session
{
	struct MHD_Connection *conn;
	// The reading end of the program's output, non blocking, this object's own.
	int   fd;
	/* The socket the answer goes out on. Never read from and never written to
	   here: it is watched, and the one thing it is watched for is the peer
	   going away while this session is suspended. See watchOnce. */
	int   sock;
	pid_t child;
	/* The library is holding this connection out of its event loop and will
	   not touch it again until it is resumed. Set only by the reader and
	   cleared only by whoever is about to resume, so that one suspension is
	   resumed exactly once. */
	bool  suspended;
	/* This server is stopping and is not going to wake anybody again, so the
	   reader is to end rather than wait. Set once and never cleared: a session
	   past this point is going, and clearing it would put it back to waiting
	   for a watcher that is no longer there. */
	bool  closing;

	Session() : conn(NULL), fd(-1), sock(-1), child(-1), suspended(false), closing(false) {}
};

OpenThreads::Mutex &lock()
{
	static OpenThreads::Mutex m;
	return m;
}

std::vector<Session *> &sessions()
{
	static std::vector<Session *> v;
	return v;
}

bool serving_ = false;
size_t ceiling_ = kMaxSessions;

void sleepMs(unsigned ms)
{
	struct timespec ts;
	ts.tv_sec = (time_t)(ms / 1000u);
	ts.tv_nsec = (long)(ms % 1000u) * 1000000L;
	while (nanosleep(&ts, &ts) != 0 && errno == EINTR)
		;
}

/* Stops a program and collects it, so a box running this for a month does not fill
   its process table with programs nobody reads.

   Asked first and killed after, because a converter asked to stop closes what it was
   reading and leaves the box's own streaming server one client lighter, and one
   killed outright leaves that server waiting for a socket to time out.

   waitpid may find nothing to collect: something else in this program sets SIGCHLD to
   be ignored when it starts a command in the background, and a process whose parent
   ignores that signal is collected by the system. */
void endChild(pid_t child)
{
	if (child <= 0)
		return;

	/* THE WHOLE GROUP AND NOT ONLY THE ONE THAT WAS STARTED. spawn puts the child in
	   a session of its own, so it leads a process group holding it and anything it
	   starts; a converter that is one program in front of another would otherwise
	   leave the second one running with nobody left to end it. The negative number is
	   that group, and the plain one after it is for the case where the session could
	   not be made. */
	if (::kill(-child, SIGTERM) != 0)
		::kill(child, SIGTERM);

	for (unsigned waited = 0; waited < kEndWaitMs; waited += kEndStepMs)
	{
		const pid_t seen = ::waitpid(child, NULL, WNOHANG);
		/* Collected, or never this process's to collect. Either way the number
		   is free for the system to hand to somebody else from here on, so
		   nothing is signalled with it again. */
		if (seen == child || (seen < 0 && errno == ECHILD))
			return;
		sleepMs(kEndStepMs);
	}

	if (::kill(-child, SIGKILL) != 0)
		::kill(child, SIGKILL);
	::waitpid(child, NULL, 0);
}

/* Starts one program with its output on a pipe. Nothing between the fork and
   the exec allocates or takes a lock, because this process has other threads
   and a child of a threaded process may only do what a signal handler may do
   until it has execed.

   Answers the reading end, or below zero. */
int spawn(const std::vector<std::string> &argv, pid_t &child)
{
	if (argv.empty())
		return -1;

	/* Built before the fork, because building it in the child would be the
	   child allocating. The vector is this call's own and outlives the exec by
	   definition: an exec that worked never comes back to read it. */
	std::vector<char *> raw;
	raw.reserve(argv.size() + 1);
	for (size_t i = 0; i < argv.size(); ++i)
		raw.push_back(const_cast<char *>(argv[i].c_str()));
	raw.push_back(NULL);

	int pipefd[2] = { -1, -1 };
	if (::pipe(pipefd) != 0)
		return -1;

	const pid_t pid = ::fork();
	if (pid < 0)
	{
		::close(pipefd[0]);
		::close(pipefd[1]);
		return -1;
	}

	if (pid == 0)
	{
		::dup2(pipefd[1], STDOUT_FILENO);
		::close(pipefd[0]);
		::close(pipefd[1]);
		/* Everything else this process has open, shut. A converter that
		   inherited the listening socket would hold the port after this
		   program has stopped, and one that inherited another caller's
		   connection would hold that caller open too. */
		for (int i = STDERR_FILENO + 1; i < 1024; ++i)
			::close(i);
		/* What the program says about itself goes where this program's own
		   output goes, so that a converter that refused a channel says why in
		   the same log as everything else. */
		::setsid();
		::execv(raw[0], &raw[0]);
		_exit(127);
	}

	::close(pipefd[1]);

	/* Non blocking, because the reader below runs on a thread the library owns
	   and shares with every other connection that thread is polling. A read
	   that waited there would stop answering all of them. */
	const int flags = ::fcntl(pipefd[0], F_GETFL, 0);
	if (flags < 0 || ::fcntl(pipefd[0], F_SETFL, flags | O_NONBLOCK) != 0)
	{
		::close(pipefd[0]);
		endChild(pid);
		return -1;
	}
	::fcntl(pipefd[0], F_SETFD, FD_CLOEXEC);

	child = pid;
	return pipefd[0];
}

} // namespace

size_t openSessions()
{
	OpenThreads::ScopedLock<OpenThreads::Mutex> held(lock());
	return sessions().size();
}

size_t maxSessions()
{
	return ceiling_;
}

void setMaxSessionsForTest(size_t n)
{
	ceiling_ = n;
}

bool isStream(const Response &r)
{
	return r.code == StatusOk && !r.stream_argv.empty();
}

} // namespace livestream
} // namespace httpd
