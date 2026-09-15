/*
 * test_basicclient.cpp - tests for the basic HTTP client
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

#include "support/catch.hpp"

#include <csignal>
#include <cstdio>
#include <cstring>
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <sched.h>
#include <string>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>
#include <vector>

#include "connection/basicclient.h"
#include "connection/basicmessage.h"
#include "timerdclient/timerdclient.h"
#include "timerdclient/timerdmsg.h"

namespace
{

/* Exercises the lock, not the socket: the guard must serialise, so a counter
   raised and lowered inside it can never be seen above one.
   Nothing here asserts, because a test assertion throws and this code runs
   below a pthread entry point that has no handler. */
class Probe : public CBasicClient
{
	public:
		Probe() : depth(0), max_depth(0)
		{
			pthread_barrier_init(&start, 0, 2);
		}

		~Probe()
		{
			pthread_barrier_destroy(&start);
		}

		void hammer()
		{
			/* both threads inside the loop at the same time, otherwise the
			   first one finishes before the second is even created */
			pthread_barrier_wait(&start);

			for (int i = 0; i < 2000; i++)
			{
				CBasicClient::RequestGuard guard(*this);
				int d = ++depth;
				if (d > max_depth)
					max_depth = d;
				/* hands the cpu over from inside, so an unserialised second
				   caller is observed rather than merely possible */
				sched_yield();
				--depth;
			}
		}

		/* the counters are read and written under the lock, but a broken lock
		   makes that a race the compiler may otherwise keep in a register */
		volatile int depth;
		volatile int max_depth;
		pthread_barrier_t start;

	protected:
		unsigned char getVersion() const { return 1; }
		const char * getSocketName() const { return "/tmp/coreapi-probe"; }
};

void * run(void * arg)
{
	static_cast<Probe *>(arg)->hammer();
	return 0;
}

} // namespace

TEST_CASE("a request guard serialises concurrent users of one client", "[ipc]")
{
	Probe p;
	pthread_t a, b;

	REQUIRE(pthread_create(&a, 0, run, &p) == 0);
	REQUIRE(pthread_create(&b, 0, run, &p) == 0);
	pthread_join(a, 0);
	pthread_join(b, 0);

	REQUIRE(p.max_depth == 1);
	REQUIRE(p.depth == 0);
}

/* A guard that never unlocks hangs the second scope instead of failing it;
   there is no way to ask the mutex whether it is free. */
TEST_CASE("a request guard releases when it goes out of scope", "[ipc]")
{
	Probe p;

	{
		CBasicClient::RequestGuard first(p);
	}
	{
		CBasicClient::RequestGuard second(p);
	}

	REQUIRE(p.depth == 0);
}

namespace
{

void takeGuardTwice()
{
	Probe p;
	CBasicClient::RequestGuard first(p);
	CBasicClient::RequestGuard second(p);
}

/* The child restores the default SIGABRT action and silences its output because
   it inherits the test runner's fatal condition handler, which would otherwise
   report the deliberate abort as a failure. */
bool abortsInChild(void (*body)())
{
	/* the child's freopen would otherwise flush the runner's buffered output
	   a second time and make one run look like two */
	fflush(0);

	pid_t pid = fork();
	/* not a pid, and kill(-1) below would signal everything this process may */
	if (pid < 0)
		return false;
	if (pid == 0)
	{
		signal(SIGABRT, SIG_DFL);
		freopen("/dev/null", "w", stdout);
		freopen("/dev/null", "w", stderr);
		body();
		_exit(0);
	}
	/* A child that never stops is deadlocked on the guard, which is exactly
	   what this test guards against, so it must not be waited on forever. */
	int st = 0;
	for (int i = 0; i < 500; i++)
	{
		if (waitpid(pid, &st, WNOHANG) == pid)
			return WIFSIGNALED(st) && WTERMSIG(st) == SIGABRT;
		usleep(10000);
	}
	kill(pid, SIGKILL);
	waitpid(pid, &st, 0);
	return false;
}

} // namespace

/* Without this the second guard would block forever and the suite would still
   be green, which is how a nested guarded method reaches the box. */
TEST_CASE("taking a request guard twice on one thread aborts", "[ipc]")
{
	REQUIRE(abortsInChild(takeGuardTwice));
}

namespace
{

/* A unix socket server of its own thread. It accepts and then hands over one
   byte every gap microseconds, keeping the connection open afterwards. Zero
   bytes is a peer that accepts and then says nothing at all; a slow drip is a
   peer that says just enough, just often enough, that a timeout which bounds
   one wait never fires. Both are what a daemon that has stopped keeping up
   looks like from this side, and only the second gets past the old bound. */
class Peer
{
	public:
		Peer(const std::string & p, unsigned bytes, long gap_us)
			: path(p), to_send(bytes), gap(gap_us), stop(false), listen_fd(-1)
		{
			unlink(path.c_str());

			listen_fd = socket(AF_UNIX, SOCK_STREAM, 0);
			sockaddr_un addr;
			memset(&addr, 0, sizeof(addr));
			addr.sun_family = AF_UNIX;
			strncpy(addr.sun_path, path.c_str(), sizeof(addr.sun_path) - 1);
			bind(listen_fd, (sockaddr *) &addr, sizeof(addr));
			listen(listen_fd, 4);

			pthread_create(&thread, 0, serve, this);
		}

		~Peer()
		{
			stop = true;
			pthread_join(thread, 0);
			if (listen_fd != -1)
				close(listen_fd);
			unlink(path.c_str());
		}

	private:
		static void * serve(void * arg)
		{
			static_cast<Peer *>(arg)->loop();
			return 0;
		}

		void loop()
		{
			std::vector<int> live;
			while (!stop)
			{
				/* the listener is polled rather than blocked on, or the
				   destructor could not join a peer nobody connected to */
				fd_set r;
				FD_ZERO(&r);
				FD_SET(listen_fd, &r);
				timeval tv;
				tv.tv_sec = 0;
				tv.tv_usec = 5000;
				if (select(listen_fd + 1, &r, 0, 0, &tv) <= 0)
					continue;

				int fd = accept(listen_fd, 0, 0);
				if (fd < 0)
					continue;
				feed(fd);
				live.push_back(fd);
			}
			for (size_t i = 0; i < live.size(); i++)
				close(live[i]);
		}

		void feed(int fd)
		{
			char b = 'x';
			for (unsigned i = 0; i < to_send && !stop; i++)
			{
				usleep(gap);
				/* the client gives up mid drip in two of the cases, and a
				   plain write to what it left behind would end the runner
				   rather than fail the case */
				if (send(fd, &b, 1, MSG_NOSIGNAL) != 1)
					return;
			}
		}

		std::string   path;
		unsigned      to_send;
		long          gap;
		volatile bool stop;
		int           listen_fd;
		pthread_t     thread;
};

/* Reaches the transport's own connect and read, which are protected, so the
   deadline is exercised where a client uses it and not through a copy of it. */
class SocketProbe : public CBasicClient
{
	public:
		explicit SocketProbe(const std::string & p) : path(p) {}

		bool open() { return open_connection(); }
		bool read(char * buf, size_t n) { return receive_data(buf, n); }
		void shut() { close_connection(); }

	protected:
		unsigned char getVersion() const { return 1; }
		const char * getSocketName() const { return path.c_str(); }

	private:
		std::string path;
};

double secondsSince(const struct timespec & t0)
{
	struct timespec now;
	clock_gettime(CLOCK_MONOTONIC, &now);
	return (double) (now.tv_sec - t0.tv_sec) + (double) (now.tv_nsec - t0.tv_nsec) / 1e9;
}

std::string socketPath(const char * name)
{
	char buf[64];
	snprintf(buf, sizeof(buf), "/tmp/coreapi-%s-%d", name, (int) getpid());
	return std::string(buf);
}

} // namespace

/* The transport's own wait is sixty seconds, so a case that only asserted the
   read failed would pass on a build with no deadline at all after a minute.
   The time is asserted for that reason. */
TEST_CASE("a deadline ends a read from a peer that never answers", "[ipc]")
{
	std::string path = socketPath("silent");
	Peer peer(path, 0, 0);
	SocketProbe client(path);

	CBasicClient::Deadline bound(200);
	REQUIRE(client.open());

	struct timespec t0;
	clock_gettime(CLOCK_MONOTONIC, &t0);
	char buf[4];
	REQUIRE_FALSE(client.read(buf, sizeof(buf)));
	REQUIRE(secondsSince(t0) < 5.0);
}

/* This is the one the old bound could not catch: every wait ends well inside
   its sixty seconds, so nothing ever times out and the read goes on for as long
   as the peer keeps dripping. The peer needs 2.5 s to finish, so a read that
   comes back under two has been cut off rather than served. */
TEST_CASE("a deadline ends a read from a peer that answers a byte at a time", "[ipc]")
{
	std::string path = socketPath("slow");
	Peer peer(path, 64, 40000);
	SocketProbe client(path);

	CBasicClient::Deadline bound(200);
	REQUIRE(client.open());

	struct timespec t0;
	clock_gettime(CLOCK_MONOTONIC, &t0);
	char buf[64];
	REQUIRE_FALSE(client.read(buf, sizeof(buf)));
	REQUIRE(secondsSince(t0) < 2.0);
}

/* The GUI sets none, so this is the path every screen takes: a slow peer is
   waited for to the end, exactly as before. */
TEST_CASE("without a deadline a slow peer is waited for", "[ipc]")
{
	std::string path = socketPath("nobound");
	Peer peer(path, 20, 20000);
	SocketProbe client(path);

	REQUIRE(client.open());
	char buf[20];
	REQUIRE(client.read(buf, sizeof(buf)));
	client.shut();
}

namespace
{

volatile bool bounded_thread_running = false;

void * holdDeadline(void *)
{
	CBasicClient::Deadline bound(50);
	bounded_thread_running = true;
	usleep(600000);
	bounded_thread_running = false;
	return 0;
}

} // namespace

/* One client is shared between the message loop and the web threads, so a
   deadline the web thread sets must not end an exchange the message loop
   started. Held per thread for that reason, and this is what says so. */
TEST_CASE("a deadline does not reach another thread", "[ipc]")
{
	std::string path = socketPath("perthread");
	Peer peer(path, 20, 20000);
	SocketProbe client(path);

	pthread_t other;
	REQUIRE(pthread_create(&other, 0, holdDeadline, 0) == 0);
	while (!bounded_thread_running)
		usleep(1000);

	REQUIRE(client.open());
	char buf[20];
	REQUIRE(client.read(buf, sizeof(buf)));
	client.shut();

	pthread_join(other, 0);
}

/* A call inside a bounded call must not outlive the bound it was made under,
   and it must not leave the outer one shortened behind it. */
TEST_CASE("an inner deadline may only shorten", "[ipc]")
{
	std::string path = socketPath("inner");
	Peer peer(path, 64, 40000);
	SocketProbe client(path);

	CBasicClient::Deadline outer(200);
	CBasicClient::Deadline inner(60000);

	REQUIRE(client.open());
	struct timespec t0;
	clock_gettime(CLOCK_MONOTONIC, &t0);
	char buf[64];
	REQUIRE_FALSE(client.read(buf, sizeof(buf)));
	REQUIRE(secondsSince(t0) < 2.0);
}

/* Both halves are read through the socket rather than asked of the class: the
   inner one has to be seen to bite, or a restore of something that was never
   armed would look like a restore. */
TEST_CASE("an inner deadline is put back when it ends", "[ipc]")
{
	std::string path = socketPath("restore");
	Peer peer(path, 20, 20000);
	char buf[20];

	CBasicClient::Deadline outer(60000);
	{
		SocketProbe under_inner(path);
		CBasicClient::Deadline inner(10);
		REQUIRE(under_inner.open());
		REQUIRE_FALSE(under_inner.read(buf, sizeof(buf)));
	}

	SocketProbe after_inner(path);
	REQUIRE(after_inner.open());
	REQUIRE(after_inner.read(buf, sizeof(buf)));
	after_inner.shut();
}

TEST_CASE("a connect under a deadline still connects and still fails where it must", "[ipc]")
{
	std::string path = socketPath("connect");
	{
		Peer peer(path, 0, 0);
		SocketProbe client(path);
		CBasicClient::Deadline bound(2000);
		REQUIRE(client.open());
		client.shut();
	}

	SocketProbe gone(path);
	CBasicClient::Deadline bound(2000);
	struct timespec t0;
	clock_gettime(CLOCK_MONOTONIC, &t0);
	REQUIRE_FALSE(gone.open());
	REQUIRE(secondsSince(t0) < 2.0);
}

namespace
{

std::string backlog_path;

void connectToFullBacklog()
{
	SocketProbe client(backlog_path);
	CBasicClient::Deadline bound(200);
	_exit(client.open() ? 0 : 1);
}

/* Runs the body in a child and requires it to give up within the span. The
   parent cannot do this itself: a connect with no bound never returns, and a
   case that hangs is one nobody can tell from a case that is slow. */
bool refusedInChild(void (*body)(), double seconds)
{
	fflush(0);

	pid_t pid = fork();
	if (pid < 0)
		return false;
	if (pid == 0)
	{
		freopen("/dev/null", "w", stdout);
		freopen("/dev/null", "w", stderr);
		body();
		_exit(2);
	}

	struct timespec t0;
	clock_gettime(CLOCK_MONOTONIC, &t0);
	int st = 0;
	while (secondsSince(t0) < seconds)
	{
		if (waitpid(pid, &st, WNOHANG) == pid)
			return WIFEXITED(st) && WEXITSTATUS(st) == 1;
		usleep(10000);
	}
	kill(pid, SIGKILL);
	waitpid(pid, &st, 0);
	return false;
}

} // namespace

/* The connect is the leg that had no bound of its own at all, and the only way
   to hold one on a unix socket is to fill the queue of connections nobody has
   accepted yet. A blocking connect waits there for as long as the peer takes to
   come back for them, which for a daemon busy elsewhere is unbounded. */
TEST_CASE("a deadline ends a connect that cannot be made", "[ipc]")
{
	std::string path = socketPath("backlog");
	unlink(path.c_str());

	int listen_fd = socket(AF_UNIX, SOCK_STREAM, 0);
	REQUIRE(listen_fd >= 0);
	sockaddr_un addr;
	memset(&addr, 0, sizeof(addr));
	addr.sun_family = AF_UNIX;
	strncpy(addr.sun_path, path.c_str(), sizeof(addr.sun_path) - 1);
	REQUIRE(bind(listen_fd, (sockaddr *) &addr, sizeof(addr)) == 0);
	REQUIRE(listen(listen_fd, 1) == 0);

	/* Filled without blocking, so the case cannot hang itself while setting up
	   what it is about to measure. */
	std::vector<int> queued;
	bool full = false;
	for (int i = 0; i < 64 && !full; i++)
	{
		int fd = socket(AF_UNIX, SOCK_STREAM, 0);
		fcntl(fd, F_SETFL, O_NONBLOCK);
		if (connect(fd, (sockaddr *) &addr, sizeof(addr)) == 0)
			queued.push_back(fd);
		else if (errno == EAGAIN)
		{
			full = true;
			close(fd);
		}
		else
		{
			close(fd);
			break;
		}
	}
	/* Asserted rather than skipped: a queue that never fills would let this
	   case pass without measuring anything. */
	REQUIRE(full);

	/* In a child, because the whole point is a connect that would otherwise
	   never come back: without the bound this case would hang the runner
	   instead of failing, and a case that can only hang is not a case. */
	backlog_path = path;
	REQUIRE(refusedInChild(connectToFullBacklog, 5.0));

	for (size_t i = 0; i < queued.size(); i++)
		close(queued[i]);
	close(listen_fd);
	unlink(path.c_str());
}

namespace
{

/* Speaks just enough of the timer daemon's side to answer one list request: it
   reads the request header, says how many timers follow, and then sends that
   many or fewer before closing. Fewer is the case that matters, because the
   count is the only thing that says how long the answer is. */
class TimerdPeer
{
	public:
		/* count_first false is the peer that closes before it has said how
		   many timers follow, which is the read one step earlier than the
		   truncated one and has an answer of its own. */
		TimerdPeer(const std::string & p, int announce, int deliver, bool count_first = true)
			: path(p), announced(announce), delivered(deliver),
			  say_count(count_first), listen_fd(-1)
		{
			unlink(path.c_str());

			listen_fd = socket(AF_UNIX, SOCK_STREAM, 0);
			sockaddr_un addr;
			memset(&addr, 0, sizeof(addr));
			addr.sun_family = AF_UNIX;
			strncpy(addr.sun_path, path.c_str(), sizeof(addr.sun_path) - 1);
			bind(listen_fd, (sockaddr *) &addr, sizeof(addr));
			listen(listen_fd, 4);

			pthread_create(&thread, 0, serve, this);
		}

		~TimerdPeer()
		{
			pthread_join(thread, 0);
			if (listen_fd != -1)
				close(listen_fd);
			unlink(path.c_str());
		}

	private:
		static void * serve(void * arg)
		{
			static_cast<TimerdPeer *>(arg)->loop();
			return 0;
		}

		/* One connection and then done, so the destructor joins without being
		   told to stop: every case here makes exactly one request. */
		void loop()
		{
			int fd = accept(listen_fd, 0, 0);
			if (fd < 0)
				return;

			CBasicMessage::Header head;
			if (recv(fd, &head, sizeof(head), MSG_WAITALL) == (ssize_t) sizeof(head) && say_count)
			{
				CTimerdMsg::generalInteger count;
				count.number = announced;
				if (send(fd, &count, sizeof(count), MSG_NOSIGNAL) == (ssize_t) sizeof(count))
				{
					for (int i = 0; i < delivered; i++)
					{
						CTimerd::responseGetTimer timer;
						memset(&timer, 0, sizeof(timer));
						timer.eventID = i + 1;
						timer.eventType = CTimerd::TIMER_SHUTDOWN;
						/* zeroed above, which is SCHEDULED, so none of them is
						   one the client drops on the way in */
						if (send(fd, &timer, sizeof(timer), MSG_NOSIGNAL) != (ssize_t) sizeof(timer))
							break;
					}
				}
			}
			close(fd);
		}

		std::string path;
		int         announced;
		int         delivered;
		bool        say_count;
		int         listen_fd;
		pthread_t   thread;
};

/* The real client, pointed at the peer above. Only the socket it opens is
   different: the version it announces and everything the answer goes through
   are the shipped code. The one it replaces is private in the client, which
   does not stop a derived class from overriding it. */
class ProbeTimerd : public CTimerdClient
{
	public:
		explicit ProbeTimerd(const std::string & p) : path(p) {}

	private:
		const char * getSocketName() const { return path.c_str(); }

		std::string path;
};

} // namespace

/* The removal above rests on reading the list back, so a list that is missing
   timers has to be a failure: otherwise a timer that is still there reads as
   gone and the caller is told the removal worked. */
TEST_CASE("a timer list cut short is not answered as a shorter list", "[ipc]")
{
	std::string path = socketPath("timerd-short");
	TimerdPeer peer(path, 12, 3);
	ProbeTimerd client(path);

	CTimerd::TimerList out;
	REQUIRE_FALSE(client.getTimerList(out));
	/* what was read is still handed over, so the difference between this and
	   the whole answer below is the answer and not the list */
	REQUIRE(out.size() == 3);
}

TEST_CASE("a whole timer list is answered as whole", "[ipc]")
{
	std::string path = socketPath("timerd-whole");
	TimerdPeer peer(path, 3, 3);
	ProbeTimerd client(path);

	CTimerd::TimerList out;
	REQUIRE(client.getTimerList(out));
	REQUIRE(out.size() == 3);
}

/* The same question one step earlier: without the count there is nothing that
   says how long the answer should have been, so nothing about the list can be
   believed. */
TEST_CASE("a timer list whose count never arrives is a failure", "[ipc]")
{
	std::string path = socketPath("timerd-nocount");
	TimerdPeer peer(path, 0, 0, false);
	ProbeTimerd client(path);

	CTimerd::TimerList out;
	REQUIRE_FALSE(client.getTimerList(out));
	REQUIRE(out.empty());
}

/* And the other side of that, or the two above would be satisfied by a client
   that called every short answer a failure: a box with no timers has to stay
   an answer. */
TEST_CASE("a box with no timers is an answer and not a failure", "[ipc]")
{
	std::string path = socketPath("timerd-empty");
	TimerdPeer peer(path, 0, 0);
	ProbeTimerd client(path);

	CTimerd::TimerList out;
	REQUIRE(client.getTimerList(out));
	REQUIRE(out.empty());
}
