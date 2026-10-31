/*
 * test_streamsetup.cpp - from an accepted client to a running stream
 *
 * Copyright (C) 2026 Thilo Graf
 *
 * License: GPL-2.0-or-later
 *
 * streamsetup::run is the order the stream server follows for one client.
 * These cases drive the shipped object over a model of the server: real
 * descriptors whose closes are counted, a register of holds whose drops are
 * counted, a list of published streams, and stops in the middle of a setup
 * where a case can stop the server.
 */

#include "support/catch.hpp"

#include <driver/streamsetup.h>

#include <atomic>
#include <cerrno>
#include <map>
#include <set>
#include <vector>

#include <fcntl.h>
#include <pthread.h>
#include <sched.h>
#include <time.h>
#include <unistd.h>

#include <OpenThreads/Mutex>
#include <OpenThreads/ScopedLock>

using namespace streamsetup;

namespace
{

const uint64_t kTv = 0x1001;
const uint64_t kWebTv = 0xffffffff00000001ULL;

bool isOpen(int fd)
{
	return fcntl(fd, F_GETFD) != -1;
}

int newFd()
{
	const int fd = open("/dev/null", O_RDONLY | O_CLOEXEC);
	REQUIRE(fd >= 0);
	return fd;
}

// One point a setup can be held at, until the case lets it go.
struct Point
{
	pthread_mutex_t m;
	pthread_cond_t c;
	bool armed, arrived, go;

	Point() : armed(false), arrived(false), go(false)
	{
		pthread_mutex_init(&m, 0);
		pthread_cond_init(&c, 0);
	}
	~Point()
	{
		pthread_cond_destroy(&c);
		pthread_mutex_destroy(&m);
	}
	void pass()
	{
		pthread_mutex_lock(&m);
		if (armed)
		{
			armed = false;
			arrived = true;
			pthread_cond_broadcast(&c);
			while (!go)
				pthread_cond_wait(&c, &m);
		}
		pthread_mutex_unlock(&m);
	}
	void waitArrived()
	{
		struct timespec until;
		clock_gettime(CLOCK_REALTIME, &until);
		until.tv_sec += 10;
		pthread_mutex_lock(&m);
		while (!arrived)
			if (pthread_cond_timedwait(&c, &m, &until) == ETIMEDOUT)
				break;
		const bool got = arrived;
		pthread_mutex_unlock(&m);
		REQUIRE(got);
	}
	void letGo()
	{
		pthread_mutex_lock(&m);
		go = true;
		pthread_cond_broadcast(&c);
		pthread_mutex_unlock(&m);
	}
};

struct Stream
{
	int fd;
	uint64_t channel;
	uint32_t hold;
};

/* The server: the mutex and the running flag the gate sits on, the streams it
   published, and every close and drop done on its behalf. */
struct Server : public Ops
{
	OpenThreads::Mutex mutex;
	std::atomic<bool> running;
	StartGate gate;

	std::map<uint64_t, Stream *> streams;
	std::map<int, int> closes;     // fd -> how often closed
	std::map<uint32_t, int> drops; // hold -> how often dropped

	Target target;
	bool read_ok;
	bool recorded;
	uint32_t hold_answer;
	bool adopt_ok;
	bool pids_ok;
	bool start_ok;
	int fe;

	Point at_hold, at_pids, at_start;
	bool busy_at_hold, busy_at_pids, busy_at_start;
	/* Odd from the moment the gate let a setup in until the case starts to
	   tear its stream down; one step per change, so a reader can tell one
	   setup from the next. */
	std::atomic<int> phase;

	Server()
		: running(true), gate(mutex, running), read_ok(true), recorded(false),
		  hold_answer(41), adopt_ok(true), pids_ok(true), start_ok(true), fe(1),
		  busy_at_hold(false), busy_at_pids(false), busy_at_start(false), phase(0)
	{
		target.channel = kTv;
		target.kind = Broadcast;
	}

	~Server()
	{
		stopAll();
	}

	void closeFd(int fd)
	{
		closes[fd]++;
		close(fd);
	}

	// What StreamStatus answers: a stream runs or a setup is under way.
	bool busy()
	{
		OpenThreads::ScopedLock<OpenThreads::Mutex> l(mutex);
		return !streams.empty() || gate.pendingLocked() > 0;
	}

	// What Stop does: the flag first, then every published stream.
	void stop()
	{
		running = false;
		stopAll();
	}

	size_t stopAll()
	{
		std::map<uint64_t, Stream *> taken;
		{
			OpenThreads::ScopedLock<OpenThreads::Mutex> l(mutex);
			taken.swap(streams);
		}
		for (std::map<uint64_t, Stream *>::iterator it = taken.begin(); it != taken.end(); ++it)
			destroy(it->second);
		return taken.size();
	}

	void destroy(Stream *s)
	{
		closeFd(s->fd);
		if (s->hold)
			drop(s->hold);
		delete s;
	}

	bool readTarget(int, Target &t)
	{
		phase++;
		t = target;
		return read_ok;
	}
	bool runningLocked(uint64_t channel)
	{
		return streams.find(channel) != streams.end();
	}
	void joinLocked(uint64_t, int fd)
	{
		// A second client of a running stream: the model keeps the first fd
		// only, so the joining one is closed as the stream would close it.
		closeFd(fd);
	}
	bool recording(uint64_t) { return recorded; }
	uint32_t hold(uint64_t)
	{
		busy_at_hold = busy();
		at_hold.pass();
		return hold_answer;
	}
	void *adopt(uint32_t) { return adopt_ok ? &fe : 0; }
	void drop(uint32_t id) { drops[id]++; }
	bool preparePids(const Target &)
	{
		busy_at_pids = busy();
		at_pids.pass();
		return pids_ok;
	}
	void *buildLocked(int fd, const Target &t, void *, uint32_t hold_id)
	{
		Stream *s = new Stream;
		s->fd = fd;
		s->channel = t.channel;
		s->hold = hold_id;
		return s;
	}
	bool startLocked(void *)
	{
		// The mutex is held here, so busy() cannot be asked: the count is.
		busy_at_start = gate.pendingLocked() > 0;
		at_start.pass();
		return start_ok;
	}
	void publishLocked(void *p)
	{
		Stream *s = static_cast<Stream *>(p);
		streams[s->channel] = s;
	}
	void discardLocked(void *p)
	{
		destroy(static_cast<Stream *>(p));
	}

	/* What the accept loop does with the answer: closes a refused client. */
	Outcome accept(int fd)
	{
		const Outcome o = run(fd, gate, *this);
		if (o.result == Refused)
			closeFd(fd);
		return o;
	}

	int pending()
	{
		OpenThreads::ScopedLock<OpenThreads::Mutex> l(mutex);
		return gate.pendingLocked();
	}
};

struct Setup
{
	Server &server;
	int fd;
	Outcome got;
	pthread_t thread;

	Setup(Server &s, int f) : server(s), fd(f)
	{
		got.result = Refused;
		got.published = false;
		REQUIRE(pthread_create(&thread, 0, &go, this) == 0);
	}
	void join() { pthread_join(thread, 0); }
	static void *go(void *arg)
	{
		Setup *s = static_cast<Setup *>(arg);
		s->got = s->server.accept(s->fd);
		return 0;
	}
};

/* One way out: the fd is closed exactly once, a number opened after it is
   not closed by whatever is torn down later, the hold is dropped exactly
   once, and nothing is left counted as under way. */
void checkOneWayOut(Server &server, int fd, uint32_t hold)
{
	REQUIRE(server.pending() == 0);
	const int reuse = newFd();          // the lowest free number, fd's if it is free
	server.stopAll();                   // tears down whatever was published
	REQUIRE(server.closes[fd] == 1);
	REQUIRE_FALSE((isOpen(fd) && reuse != fd));
	REQUIRE(isOpen(reuse));
	if (hold)
		REQUIRE(server.drops[hold] == 1);
	close(reuse);
}

} // namespace

TEST_CASE("a refused setup leaves the client to the accept loop and its hold dropped", "[driver][streamsetup]")
{
	SECTION("the request cannot be read")
	{
		Server s;
		s.read_ok = false;
		const int fd = newFd();
		REQUIRE(s.accept(fd).result == Refused);
		checkOneWayOut(s, fd, 0);
		REQUIRE(s.drops.empty());
	}
	SECTION("it names no channel")
	{
		Server s;
		s.target.kind = Unknown;
		const int fd = newFd();
		REQUIRE(s.accept(fd).result == Refused);
		checkOneWayOut(s, fd, 0);
	}
	SECTION("the channel is being recorded")
	{
		Server s;
		s.recorded = true;
		const int fd = newFd();
		REQUIRE(s.accept(fd).result == Refused);
		checkOneWayOut(s, fd, 0);
		REQUIRE(s.drops.empty());
	}
	SECTION("zapit holds no frontend for it")
	{
		Server s;
		s.hold_answer = 0;
		const int fd = newFd();
		REQUIRE(s.accept(fd).result == Refused);
		checkOneWayOut(s, fd, 0);
		REQUIRE(s.drops.empty());
	}
	SECTION("the hold cannot be adopted")
	{
		Server s;
		s.adopt_ok = false;
		const int fd = newFd();
		REQUIRE(s.accept(fd).result == Refused);
		checkOneWayOut(s, fd, 41);
	}
	SECTION("there is nothing to stream")
	{
		Server s;
		s.pids_ok = false;
		const int fd = newFd();
		REQUIRE(s.accept(fd).result == Refused);
		checkOneWayOut(s, fd, 41);
	}
	SECTION("the server stopped before the setup came in")
	{
		Server s;
		s.running = false;
		const int fd = newFd();
		REQUIRE(s.accept(fd).result == Refused);
		checkOneWayOut(s, fd, 0);
	}
}

TEST_CASE("a stream that does not start takes its client and hold with it", "[driver][streamsetup]")
{
	Server s;
	s.start_ok = false;
	const int fd = newFd();
	const Outcome o = s.accept(fd);
	REQUIRE(o.result == Consumed);
	REQUIRE_FALSE(o.published);
	REQUIRE(s.streams.empty());
	checkOneWayOut(s, fd, 41);
}

TEST_CASE("a stream that starts is published with its client and hold", "[driver][streamsetup]")
{
	Server s;
	const int fd = newFd();
	const Outcome o = s.accept(fd);
	REQUIRE(o.result == Consumed);
	REQUIRE(o.published);
	REQUIRE(s.streams.size() == 1);
	REQUIRE(s.streams[kTv]->hold == 41);
	REQUIRE(isOpen(fd));
	REQUIRE(s.drops.empty());
	checkOneWayOut(s, fd, 41);
}

TEST_CASE("a client of a running stream joins it and is not published again", "[driver][streamsetup]")
{
	Server s;
	const int first = newFd();
	REQUIRE(s.accept(first).published);

	s.hold_answer = 77;
	const int second = newFd();
	const Outcome o = s.accept(second);
	REQUIRE(o.result == Consumed);
	REQUIRE_FALSE(o.published);
	REQUIRE(s.closes[second] == 1);
	// No hold was asked for the second client.
	REQUIRE(s.drops[77] == 0);
	checkOneWayOut(s, first, 41);
}

TEST_CASE("a web channel needs no hold", "[driver][streamsetup]")
{
	SECTION("it starts")
	{
		Server s;
		s.target.channel = kWebTv;
		s.target.kind = Web;
		s.hold_answer = 0;
		const int fd = newFd();
		const Outcome o = s.accept(fd);
		REQUIRE(o.result == Consumed);
		REQUIRE(o.published);
		REQUIRE(s.streams[kWebTv]->hold == 0);
		checkOneWayOut(s, fd, 0);
	}
	SECTION("it does not start")
	{
		Server s;
		s.target.channel = kWebTv;
		s.target.kind = Web;
		s.start_ok = false;
		const int fd = newFd();
		REQUIRE(s.accept(fd).result == Consumed);
		checkOneWayOut(s, fd, 0);
	}
	SECTION("the server stopped")
	{
		Server s;
		s.target.channel = kWebTv;
		s.target.kind = Web;
		s.at_start.armed = false;
		const int fd = newFd();
		s.running = false;
		REQUIRE(s.accept(fd).result == Refused);
		checkOneWayOut(s, fd, 0);
	}
}

TEST_CASE("P1: a stop while zapit is asked keeps the stream from being published", "[driver][streamsetup]")
{
	Server s;
	s.at_hold.armed = true;
	const int fd = newFd();
	Setup setup(s, fd);
	s.at_hold.waitArrived();

	s.stop();
	s.at_hold.letGo();
	setup.join();

	REQUIRE(setup.got.result == Refused);
	REQUIRE(s.streams.empty());
	checkOneWayOut(s, fd, 41);
}

namespace
{

struct Stopper
{
	Server &server;
	size_t stopped;
	pthread_t thread;

	explicit Stopper(Server &s) : server(s), stopped(0)
	{
		REQUIRE(pthread_create(&thread, 0, &go, this) == 0);
	}
	void join() { pthread_join(thread, 0); }
	static void *go(void *arg)
	{
		Stopper *st = static_cast<Stopper *>(arg);
		st->server.running = false;
		st->stopped = st->server.stopAll();
		return 0;
	}
};

} // namespace

TEST_CASE("P2: a stop that comes while the stream is published stops that stream", "[driver][streamsetup]")
{
	Server s;
	s.at_start.armed = true;
	const int fd = newFd();
	Setup setup(s, fd);
	s.at_start.waitArrived();     // inside the publishing hold of the mutex

	Stopper stopper(s);           // clears the flag, then waits for the mutex
	s.at_start.letGo();
	setup.join();
	stopper.join();

	REQUIRE(setup.got.result == Consumed);
	REQUIRE(setup.got.published);
	REQUIRE(stopper.stopped == 1);
	REQUIRE(s.streams.empty());
	REQUIRE(s.closes[fd] == 1);
	REQUIRE(s.drops[41] == 1);
	REQUIRE(s.pending() == 0);
}

TEST_CASE("P3: a setup counts as a stream from the moment it is let in until it runs", "[driver][streamsetup]")
{
	Server s;
	REQUIRE_FALSE(s.busy());
	s.at_hold.armed = true;
	s.at_pids.armed = true;
	s.at_start.armed = true;
	const int fd = newFd();
	Setup setup(s, fd);

	s.at_hold.waitArrived();
	REQUIRE(s.busy());
	s.at_hold.letGo();
	s.at_pids.waitArrived();
	REQUIRE(s.busy());
	s.at_pids.letGo();
	s.at_start.waitArrived();
	s.at_start.letGo();
	setup.join();

	REQUIRE(s.busy_at_hold);
	REQUIRE(s.busy_at_pids);
	REQUIRE(s.busy_at_start);
	REQUIRE(s.busy());            // now as a published stream
	REQUIRE(s.pending() == 0);
	checkOneWayOut(s, fd, 41);
	REQUIRE_FALSE(s.busy());
}

TEST_CASE("P3: the count is never seen at zero between the setup and its stream", "[driver][streamsetup]")
{
	/* A reader asking all the time while setups run one after another:
	   between the moment a setup is let in and the moment its stream is
	   published, the answer is never no. */
	Server s;
	std::atomic<bool> stop_watch(false);
	std::atomic<int> seen_idle(0);
	std::atomic<int> looked(0);

	struct Watch
	{
		Server *server;
		std::atomic<bool> *stop;
		std::atomic<int> *bad;
		std::atomic<int> *looked;
		static void *go(void *arg)
		{
			Watch *w = static_cast<Watch *>(arg);
			while (!w->stop->load())
			{
				// One setup let in, and its stream not yet torn down, both
				// before and after the question: the answer must be yes.
				const int before = w->server->phase.load();
				const bool busy = w->server->busy();
				const int after = w->server->phase.load();
				if (before == after && (before & 1))
				{
					(*w->looked)++;
					if (!busy)
						(*w->bad)++;
				}
			}
			return 0;
		}
	} watch = { &s, &stop_watch, &seen_idle, &looked };
	pthread_t t;
	REQUIRE(pthread_create(&t, 0, &Watch::go, &watch) == 0);

	for (int i = 0; i < 300; ++i)
	{
		s.target.channel = kTv + (uint64_t) i;
		const int fd = newFd();
		REQUIRE(s.accept(fd).published);
		// Keep this setup's window open until the reader has asked inside it
		// (at most a second): on a loaded host the reader thread may not run
		// at all while the loop goes through its setups.
		const int asked = looked.load();
		struct timespec now, until;
		clock_gettime(CLOCK_MONOTONIC, &until);
		until.tv_sec += 1;
		do
		{
			sched_yield();
			clock_gettime(CLOCK_MONOTONIC, &now);
		} while (looked.load() == asked && (now.tv_sec < until.tv_sec
		         || (now.tv_sec == until.tv_sec && now.tv_nsec < until.tv_nsec)));
		s.phase++;             // before the teardown below
		s.stopAll();
	}
	stop_watch = true;
	pthread_join(t, 0);
	REQUIRE(seen_idle.load() == 0);
	REQUIRE(looked.load() > 0);
}
