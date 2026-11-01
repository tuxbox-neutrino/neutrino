/*
 * livestream.cpp - an answer whose body is a running program's output
 *
 * Copyright (C) 2026 NI-Team
 * Copyright (C) 2026 Tuxbox-Project (carrier, child setup, supervision)
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

#include "livestream.h"

#include "http.h"
#include "status.h"

#include "coreapi/base/errors.h"

#include <atomic>
#include <cerrno>
#include <cstring>
#include <string>
#include <vector>

#include <fcntl.h>
#include <poll.h>
#include <pthread.h>
#include <signal.h>
#include <sys/prctl.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

namespace httpd
{

// The table the browser stream route is in (ep_stream.cpp).
extern const RouteTable streamTable;

namespace livestream
{

namespace
{

/* The most one read hands on. A transport stream at eight megabits is a
   megabyte a second, so this is read from the pipe a few dozen times a
   second and never sits in this process. */
const size_t kBlockSize = 32768;

/* How long a program has for its first bytes. A converter reads the channel
   from this box's own streaming port, which answers once a tuner is tuned
   and the tables are read; a Unicable line retunes more than once. */
const unsigned kFirstByteDefaultMs = 15000;

/* How long one wait for the program's output lasts before the job asks
   whether its reader is still there: a reader that left frees the seat within
   this. */
const int kTurnMs = 250;

std::atomic<const char *> converter_override(NULL);
std::atomic<unsigned> first_byte_ms(0);

unsigned firstByteMs()
{
	const unsigned ms = first_byte_ms.load();
	return ms ? ms : kFirstByteDefaultMs;
}

int64_t nowMs()
{
	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	return (int64_t) ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}

// The command to run: the answer's, with the program a case put in its place.
std::vector<std::string> commandOf(const Response &r)
{
	std::vector<std::string> argv = r.stream_argv;
	const char *path = converter_override.load();
	if (path != NULL && !argv.empty())
		argv[0] = path;
	return argv;
}

Response unreachable(const char *why)
{
	return problemResponse(StatusServiceUnavailable, coreapi::ErrorCode::UpstreamUnreachable, why);
}

/* Starts one program with its output on a pipe and answers the reading end,
   or -1.

   This process has other threads, so between the fork and the exec the child
   may only do what a signal handler may do, and everything it needs is made
   before the fork. All signals are blocked on this thread across the fork, so
   no handler of this program runs in the child before it has put every signal
   back to its default; the parent's mask is back right after.

   In the child, in this order: every signal to its default (this program
   ignores SIGPIPE and SIGHUP, and a converter inheriting that would neither
   end on a closed pipe nor on a hang up); SIGTERM when the thread that started
   it ends, so a child is never left behind by a job that died; ending at once
   if that thread is gone already; a session of its own, so the whole group it
   starts can be signalled; its output to the pipe; everything else closed,
   the listening socket and other callers' connections included; an empty
   signal mask; the exec. */
int spawn(const std::vector<std::string> &argv, pid_t &child)
{
	if (argv.empty())
		return -1;

	std::vector<char *> raw;
	raw.reserve(argv.size() + 1);
	for (size_t i = 0; i < argv.size(); ++i)
		raw.push_back(const_cast<char *>(argv[i].c_str()));
	raw.push_back(NULL);

	long open_max = sysconf(_SC_OPEN_MAX);
	if (open_max < 0 || open_max > 65536)
		open_max = 65536;
	const pid_t parent = getpid();

	struct sigaction dfl;
	std::memset(&dfl, 0, sizeof(dfl));
	dfl.sa_handler = SIG_DFL;
	sigemptyset(&dfl.sa_mask);
	sigset_t none;
	sigemptyset(&none);

	int pipefd[2] = { -1, -1 };
	if (pipe2(pipefd, O_CLOEXEC) != 0)
		return -1;

	sigset_t all, old;
	sigfillset(&all);
	pthread_sigmask(SIG_SETMASK, &all, &old);
	const pid_t pid = fork();
	if (pid == 0)
	{
		for (int s = 1; s < NSIG; ++s)
			if (s != SIGKILL && s != SIGSTOP)
				sigaction(s, &dfl, NULL);
		if (prctl(PR_SET_PDEATHSIG, SIGTERM) != 0)
			_exit(126);
		if (getppid() != parent)
			_exit(126);
		setsid();
		if (dup2(pipefd[1], STDOUT_FILENO) < 0)
			_exit(126);
		for (long fd = STDERR_FILENO + 1; fd < open_max; ++fd)
			close((int) fd);
		sigprocmask(SIG_SETMASK, &none, NULL);
		execv(raw[0], &raw[0]);
		_exit(127);
	}
	pthread_sigmask(SIG_SETMASK, &old, NULL);

	close(pipefd[1]);
	if (pid < 0)
	{
		close(pipefd[0]);
		return -1;
	}

	/* Non blocking, so a read never waits past the poll that said it may. */
	const int flags = fcntl(pipefd[0], F_GETFL, 0);
	if (flags >= 0)
		fcntl(pipefd[0], F_SETFL, flags | O_NONBLOCK);

	child = pid;
	return pipefd[0];
}

// The reading end of the program's output, closed on every way out.
struct Pipe
{
	int fd;
	explicit Pipe(int f) : fd(f) {}
	~Pipe()
	{
		if (fd >= 0)
			close(fd);
	}

private:
	Pipe(const Pipe &);
	Pipe &operator=(const Pipe &);
};

enum Got
{
	Bytes,    // n bytes were read
	Ended,    // the program closed its output
	Gone,     // the reader left or the server stops
	Silent    // nothing came before the deadline
};

/* Waits for the program's output until deadline_ms (0 for one turn), asking
   about the reader every turn, and reads what is there. */
Got readFrom(carry::Job &job, int fd, char *buf, size_t &n, int64_t deadline_ms)
{
	for (;;)
	{
		if (!carry::waitPeer(job, 0))
			return Gone;

		struct pollfd p;
		p.fd = fd;
		p.events = POLLIN;
		p.revents = 0;
		const int r = poll(&p, 1, kTurnMs);
		if (r < 0 && errno != EINTR)
			return Ended;
		if (r > 0)
		{
			const ssize_t got = read(fd, buf, kBlockSize);
			if (got > 0)
			{
				n = (size_t) got;
				return Bytes;
			}
			if (got == 0 || (errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR))
				return Ended;
		}
		if (deadline_ms != 0 && nowMs() >= deadline_ms)
			return Silent;
	}
}

bool checkProgram(const Response &answer, Response &refusal)
{
	if (!supervisable(refusal))
		return false;
	const std::vector<std::string> argv = commandOf(answer);
	/* Asked before anything is started, because a box without the converter
	   is a box that cannot do this at all and ought to say so rather than hand
	   out an answer with no bytes in it. Not every box carries one: the ffmpeg
	   some of them build has neither an AAC encoder nor a command line
	   program. */
	if (argv.empty() || access(argv[0].c_str(), X_OK) != 0)
	{
		refusal = problemResponse(StatusNotImplemented, coreapi::ErrorCode::NotPlayableInBrowser,
		                          "this box carries nothing that can convert a channel for a browser");
		return false;
	}
	return true;
}

void carryProgram(carry::Job &job)
{
	Response &answer = carry::answerOf(job);
	{
		Response refusal;
		if (!checkProgram(answer, refusal))
		{
			carry::refuse(job, refusal);
			return;
		}
	}

	pid_t pid = -1;
	Pipe out(spawn(commandOf(answer), pid));
	if (out.fd < 0)
	{
		carry::refuse(job, unreachable("the converter could not be started"));
		return;
	}
	/* Entered for the job, the child is ended and collected by the
	   transport when the job ends, whichever way out it takes. False when a
	   stop came in between: the transport has ended and collected it
	   already. */
	if (!carry::adoptChild(job, pid))
		return;

	std::vector<char> buf(kBlockSize);
	size_t n = 0;
	switch (readFrom(job, out.fd, &buf[0], n, nowMs() + firstByteMs()))
	{
		case Bytes:
			break;
		case Gone:
			return;
		case Ended:
			carry::refuse(job, unreachable("the converter ended before it sent anything: "
			                               "the channel could not be read"));
			return;
		case Silent:
			carry::refuse(job, unreachable("the converter sent nothing in time: "
			                               "the channel could not be read"));
			return;
	}

	if (!carry::sendHead(job, -1) || !carry::sendBody(job, &buf[0], n))
		return;
	for (;;)
	{
		if (readFrom(job, out.fd, &buf[0], n, 0) != Bytes)
			return;
		if (!carry::sendBody(job, &buf[0], n))
			return;
	}
}

const carry::Carrier kProgramCarrier = { &checkProgram, &carryProgram };

} // namespace

const carry::Carrier *carrier()
{
	return &kProgramCarrier;
}

bool carriedRoute(const Endpoint &ep)
{
	for (size_t i = 0; i < streamTable.count; ++i)
	{
		const Endpoint &e = streamTable.endpoints[i];
		if (&e == &ep && std::strcmp(e.path, "/api/v1/stream/browser/{id}") == 0)
			return true;
	}
	return false;
}

bool supervisable(Response &refusal)
{
	struct sigaction chld;
	if (sigaction(SIGCHLD, NULL, &chld) != 0)
		return true;
	if (chld.sa_handler != SIG_IGN && (chld.sa_flags & SA_NOCLDWAIT) == 0)
		return true;
	refusal = problemResponse(StatusInternalServerError, coreapi::ErrorCode::NoSupervision,
	                          "this process discards what its children exit with, so a converter "
	                          "cannot be supervised");
	return false;
}

void setConverterForTest(const char *path)
{
	converter_override.store(path);
}

const char *converterForTest()
{
	return converter_override.load();
}

void setFirstByteMsForTest(unsigned ms)
{
	first_byte_ms.store(ms);
}

} // namespace livestream
} // namespace httpd
