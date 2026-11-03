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
 * where a case can stop the server or take a frontend back.
 */

#include "support/catch.hpp"

#include <driver/streamsetup.h>

#include <algorithm>
#include <atomic>
#include <cerrno>
#include <cstdio>
#include <map>
#include <set>
#include <string>
#include <vector>

#include <fcntl.h>
#include <poll.h>
#include <pthread.h>
#include <sched.h>
#include <sys/socket.h>
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
	int fe;
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
	Read read;
	bool ok_written;
	std::vector<int> refusals;   // the statuses refused with, in order
	std::vector<std::string> reasons;
	bool refused_under_mutex;
	std::map<uint32_t, int> drops_at_refusal;
	bool recorded;
	uint32_t hold_answer;
	bool adopt_ok;
	bool pids_ok;
	bool start_ok;
	int fe;          // the frontend adopt hands out; its number is its value

	Point at_hold, at_pids, at_start;
	bool busy_at_hold, busy_at_pids, busy_at_start;
	/* Odd from the moment the gate let a setup in until the case starts to
	   tear its stream down; one step per change, so a reader can tell one
	   setup from the next. */
	std::atomic<int> phase;

	Server()
		: running(true), gate(mutex, running), read(ReadOk), ok_written(true),
		  refused_under_mutex(false), recorded(false),
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

	Read readTarget(int, Target &t)
	{
		phase++;
		t = target;
		return read;
	}
	bool writeOk(int) { return ok_written; }
	void refuse(int, int status, const char *reason)
	{
		refusals.push_back(status);
		reasons.push_back(reason ? reason : "");
		// The mutex is not recursive: a refusal written under it fails here.
		if (mutex.trylock() == 0)
			mutex.unlock();
		else
			refused_under_mutex = true;
		drops_at_refusal = drops;
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
	Hold hold(uint64_t)
	{
		busy_at_hold = busy();
		at_hold.pass();
		Hold h;
		h.id = hold_answer;
		h.fenumber = hold_answer ? fe : -1;
		return h;
	}
	void *adopt(uint32_t) { return adopt_ok ? &fe : 0; }
	void drop(uint32_t id) { drops[id]++; }
	bool preparePids(const Target &)
	{
		busy_at_pids = busy();
		at_pids.pass();
		return pids_ok;
	}
	void *buildLocked(int fd, const Target &t, void *frontend, uint32_t hold_id)
	{
		Stream *s = new Stream;
		s->fd = fd;
		s->channel = t.channel;
		s->hold = hold_id;
		s->fe = frontend ? *static_cast<int *>(frontend) : -1;
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

	/* What the stream manager does to stop the streams on a frontend or of
	   a channel: in one hold of the mutex it takes the frontend or channel
	   back and the streams out; it tears them down after. */
	size_t stopOn(int number)
	{
		std::vector<Stream *> taken;
		{
			OpenThreads::ScopedLock<OpenThreads::Mutex> l(mutex);
			gate.cancelFrontendLocked(number);
			for (std::map<uint64_t, Stream *>::iterator it = streams.begin(); it != streams.end(); )
			{
				if (it->second->fe == number)
				{
					taken.push_back(it->second);
					streams.erase(it++);
				}
				else
					++it;
			}
		}
		for (size_t i = 0; i < taken.size(); ++i)
			destroy(taken[i]);
		return taken.size();
	}
	size_t stopChannel(uint64_t channel)
	{
		Stream *taken = 0;
		{
			OpenThreads::ScopedLock<OpenThreads::Mutex> l(mutex);
			gate.cancelChannelLocked(channel);
			std::map<uint64_t, Stream *>::iterator it = streams.find(channel);
			if (it != streams.end())
			{
				taken = it->second;
				streams.erase(it);
			}
		}
		if (taken)
			destroy(taken);
		return taken ? 1 : 0;
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
	SECTION("the request is garbage or did not come in time: no answer")
	{
		Server s;
		s.read = Ops::ReadGarbage;
		const int fd = newFd();
		REQUIRE(s.accept(fd).result == Refused);
		REQUIRE(s.refusals.empty());
		checkOneWayOut(s, fd, 0);
		REQUIRE(s.drops.empty());
	}
	SECTION("it names a channel the box does not have: 404, and live is not tried")
	{
		Server s;
		s.read = Ops::ReadUnknown;
		const int fd = newFd();
		REQUIRE(s.accept(fd).result == Refused);
		REQUIRE(s.refusals == std::vector<int>(1, 404));
		checkOneWayOut(s, fd, 0);
		REQUIRE(s.drops.empty());
	}
	SECTION("it names none and no channel is shown: 404")
	{
		Server s;
		s.read = Ops::ReadNoLive;
		const int fd = newFd();
		REQUIRE(s.accept(fd).result == Refused);
		REQUIRE(s.refusals == std::vector<int>(1, 404));
		checkOneWayOut(s, fd, 0);
	}
	SECTION("the channel is being recorded: 503 without asking zapit")
	{
		Server s;
		s.recorded = true;
		const int fd = newFd();
		REQUIRE(s.accept(fd).result == Refused);
		REQUIRE(s.refusals == std::vector<int>(1, 503));
		checkOneWayOut(s, fd, 0);
		REQUIRE(s.drops.empty());
	}
	SECTION("zapit holds no frontend for it: 503")
	{
		Server s;
		s.hold_answer = 0;
		const int fd = newFd();
		REQUIRE(s.accept(fd).result == Refused);
		REQUIRE(s.refusals == std::vector<int>(1, 503));
		checkOneWayOut(s, fd, 0);
		REQUIRE(s.drops.empty());
	}
	SECTION("the hold cannot be adopted: dropped before the 503 goes out")
	{
		Server s;
		s.adopt_ok = false;
		const int fd = newFd();
		REQUIRE(s.accept(fd).result == Refused);
		REQUIRE(s.refusals == std::vector<int>(1, 503));
		REQUIRE(s.drops_at_refusal[41] == 1);
		checkOneWayOut(s, fd, 41);
	}
	SECTION("there is nothing to stream: 503")
	{
		Server s;
		s.pids_ok = false;
		const int fd = newFd();
		REQUIRE(s.accept(fd).result == Refused);
		REQUIRE(s.refusals == std::vector<int>(1, 503));
		REQUIRE(s.drops_at_refusal[41] == 1);
		checkOneWayOut(s, fd, 41);
	}
	SECTION("the 200 does not go out: nothing is published")
	{
		Server s;
		s.ok_written = false;
		const int fd = newFd();
		REQUIRE(s.accept(fd).result == Refused);
		REQUIRE(s.refusals.empty());
		REQUIRE(s.streams.empty());
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

	// A client whose 200 does not go out is not joined.
	s.ok_written = false;
	const int lost = newFd();
	REQUIRE(s.accept(lost).result == Refused);
	REQUIRE(s.closes[lost] == 1);
	s.ok_written = true;
	s.closes.erase(lost);   // its number is free for the next client

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
		const int fd = newFd();
		s.running = false;
		REQUIRE(s.accept(fd).result == Refused);
		checkOneWayOut(s, fd, 0);
	}
	SECTION("its 200 does not go out")
	{
		Server s;
		s.target.channel = kWebTv;
		s.target.kind = Web;
		s.ok_written = false;
		const int fd = newFd();
		REQUIRE(s.accept(fd).result == Refused);
		REQUIRE(s.streams.empty());
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

TEST_CASE("G1: a frontend taken back while the stream is set up refuses it with 503", "[driver][streamsetup]")
{
	Server s;
	s.at_pids.armed = true;       // after the hold was adopted
	const int fd = newFd();
	Setup setup(s, fd);
	s.at_pids.waitArrived();

	REQUIRE(s.stopOn(1) == 0);    // nothing published yet: only the mark
	s.at_pids.letGo();
	setup.join();

	REQUIRE(setup.got.result == Refused);
	REQUIRE(s.streams.empty());
	REQUIRE(s.refusals == std::vector<int>(1, 503));
	// The same bytes as when no frontend was free.
	REQUIRE(statusBytes(503, s.reasons[0].c_str()) == statusBytes(503, "no tuner is free for this channel"));
	REQUIRE_FALSE(s.refused_under_mutex);
	REQUIRE(s.drops_at_refusal[41] == 1);
	checkOneWayOut(s, fd, 41);
}

TEST_CASE("G2: another frontend taken back leaves the stream to be published", "[driver][streamsetup]")
{
	Server s;
	s.at_pids.armed = true;
	const int fd = newFd();
	Setup setup(s, fd);
	s.at_pids.waitArrived();

	s.stopOn(2);
	s.at_pids.letGo();
	setup.join();

	REQUIRE(setup.got.result == Consumed);
	REQUIRE(setup.got.published);
	REQUIRE(s.refusals.empty());
	checkOneWayOut(s, fd, 41);
}

TEST_CASE("G3: its channel stopped while the stream is set up refuses it with 503", "[driver][streamsetup]")
{
	Server s;
	s.at_pids.armed = true;
	const int fd = newFd();
	Setup setup(s, fd);
	s.at_pids.waitArrived();

	s.stopChannel(kTv);
	s.at_pids.letGo();
	setup.join();

	REQUIRE(setup.got.result == Refused);
	REQUIRE(s.refusals == std::vector<int>(1, 503));
	REQUIRE_FALSE(s.refused_under_mutex);
	checkOneWayOut(s, fd, 41);
}

TEST_CASE("G4: a frontend taken back before the setup came in does not touch it", "[driver][streamsetup]")
{
	Server s;
	SECTION("its frontend")
	{
		REQUIRE(s.stopOn(1) == 0);
	}
	SECTION("its channel")
	{
		REQUIRE(s.stopChannel(kTv) == 0);
	}
	const int fd = newFd();
	const Outcome o = s.accept(fd);
	REQUIRE(o.result == Consumed);
	REQUIRE(o.published);
	REQUIRE(s.refusals.empty());
	checkOneWayOut(s, fd, 41);
}

TEST_CASE("G5: a stream published before its frontend is taken back is stopped with it", "[driver][streamsetup]")
{
	Server s;
	const int fd = newFd();
	REQUIRE(s.accept(fd).published);

	REQUIRE(s.stopOn(1) == 1);
	REQUIRE(s.streams.empty());
	REQUIRE(s.closes[fd] == 1);
	REQUIRE(s.drops[41] == 1);
	REQUIRE(s.pending() == 0);
}

TEST_CASE("G6: a server that stops closes the setup without an answer, taken back or not", "[driver][streamsetup]")
{
	Server s;
	s.at_pids.armed = true;
	const int fd = newFd();
	Setup setup(s, fd);
	s.at_pids.waitArrived();

	s.stopOn(1);
	s.stop();
	s.at_pids.letGo();
	setup.join();

	REQUIRE(setup.got.result == Refused);
	REQUIRE(s.refusals.empty());
	REQUIRE(s.streams.empty());
	checkOneWayOut(s, fd, 41);
}

TEST_CASE("I1: a refused setup with nothing else about announces that no stream runs", "[driver][streamsetup]")
{
	Server s;
	s.hold_answer = 0;
	const int fd = newFd();
	const Outcome o = s.accept(fd);
	REQUIRE(o.result == Refused);
	REQUIRE(idleAfter(o, s.streams.empty(), s.pending()));

	// A stream that did not start is announced the same way.
	Server t;
	t.start_ok = false;
	const Outcome p = t.accept(newFd());
	REQUIRE(p.result == Consumed);
	REQUIRE(idleAfter(p, t.streams.empty(), t.pending()));
}

TEST_CASE("I2: a published stream is not announced as gone", "[driver][streamsetup]")
{
	Server s;
	const int fd = newFd();
	const Outcome o = s.accept(fd);
	REQUIRE(o.published);
	REQUIRE_FALSE(idleAfter(o, s.streams.empty(), s.pending()));
	checkOneWayOut(s, fd, 41);
}

TEST_CASE("I3: another stream running or another setup under way announces nothing", "[driver][streamsetup]")
{
	Server s;
	const int first = newFd();
	REQUIRE(s.accept(first).published);

	s.target.channel = kTv + 1;
	s.hold_answer = 0;
	const Outcome o = s.accept(newFd());
	REQUIRE(o.result == Refused);
	REQUIRE_FALSE(idleAfter(o, s.streams.empty(), s.pending()));

	// The rule alone: a setup still under way.
	REQUIRE_FALSE(idleAfter(o, true, 1));
	checkOneWayOut(s, first, 41);
}

TEST_CASE("I4: a setup that asked zapit for a frontend says so, also when it started nothing", "[driver][streamsetup]")
{
	/* zapit may have tuned the frontend before the hold failed or the
	   setup after it did: the end announced carries that. */
	SECTION("zapit refused the hold")
	{
		Server s;
		s.hold_answer = 0;
		const Outcome o = s.accept(newFd());
		REQUIRE(o.result == Refused);
		REQUIRE(o.asked_hold);
		REQUIRE(idleAfter(o, s.streams.empty(), s.pending()));
	}
	SECTION("no pids after the hold")
	{
		Server s;
		s.pids_ok = false;
		const Outcome o = s.accept(newFd());
		REQUIRE(o.result == Refused);
		REQUIRE(o.asked_hold);
	}
	SECTION("the stream did not start")
	{
		Server s;
		s.start_ok = false;
		const Outcome o = s.accept(newFd());
		REQUIRE(o.result == Consumed);
		REQUIRE_FALSE(o.published);
		REQUIRE(o.asked_hold);
	}
	SECTION("the 200 did not go out")
	{
		Server s;
		s.ok_written = false;
		const Outcome o = s.accept(newFd());
		REQUIRE(o.result == Refused);
		REQUIRE(o.asked_hold);
	}
	SECTION("published")
	{
		Server s;
		const Outcome o = s.accept(newFd());
		REQUIRE(o.published);
		REQUIRE(o.asked_hold);
	}
}

TEST_CASE("I5: a setup that never asked zapit for a frontend says it did not", "[driver][streamsetup]")
{
	Server s;
	SECTION("garbage")
	{
		s.read = Ops::ReadGarbage;
		REQUIRE_FALSE(s.accept(newFd()).asked_hold);
	}
	SECTION("no such channel")
	{
		s.read = Ops::ReadUnknown;
		REQUIRE_FALSE(s.accept(newFd()).asked_hold);
	}
	SECTION("the channel is recorded")
	{
		s.recorded = true;
		REQUIRE_FALSE(s.accept(newFd()).asked_hold);
	}
	SECTION("a web channel")
	{
		s.target.kind = Web;
		REQUIRE_FALSE(s.accept(newFd()).asked_hold);
	}
	SECTION("the server stops")
	{
		s.running = false;
		REQUIRE_FALSE(s.accept(newFd()).asked_hold);
	}
	SECTION("a client joins a running stream")
	{
		REQUIRE(s.accept(newFd()).published);
		const Outcome o = s.accept(newFd());
		REQUIRE(o.result == Consumed);
		REQUIRE_FALSE(o.published);
		REQUIRE_FALSE(o.asked_hold);
	}
}

/* The request and the answer on the raw port, over socket pairs: what a
   player sends, what it gets back, and how long either side waits. */

namespace
{

struct Pair
{
	int box;      // the end the stream server holds
	int client;   // the player's end

	Pair()
	{
		int sv[2];
		REQUIRE(socketpair(AF_UNIX, SOCK_STREAM, 0, sv) == 0);
		box = sv[0];
		client = sv[1];
	}
	~Pair()
	{
		if (box >= 0)
			close(box);
		if (client >= 0)
			close(client);
	}
	void say(const std::string &s)
	{
		REQUIRE(send(client, s.data(), s.size(), MSG_NOSIGNAL) == (ssize_t) s.size());
	}
	std::string heard()
	{
		std::string all;
		char buf[512];
		for (;;)
		{
			struct pollfd p;
			p.fd = client;
			p.events = POLLIN;
			p.revents = 0;
			if (poll(&p, 1, 2000) <= 0)
				break;
			const ssize_t n = recv(client, buf, sizeof(buf), 0);
			if (n <= 0)
				break;
			all.append(buf, (size_t) n);
		}
		return all;
	}
};

int64_t msNow()
{
	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	return (int64_t) ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}

// Sends one byte at a time with a pause between them, from a thread of its own.
struct Trickle
{
	int fd;
	std::string text;
	unsigned gap_ms;
	pthread_t thread;

	Trickle(int f, const std::string &t, unsigned gap) : fd(f), text(t), gap_ms(gap)
	{
		REQUIRE(pthread_create(&thread, 0, &go, this) == 0);
	}
	~Trickle() { pthread_join(thread, 0); }
	static void *go(void *arg)
	{
		Trickle *t = static_cast<Trickle *>(arg);
		for (size_t i = 0; i < t->text.size(); ++i)
		{
			if (send(t->fd, &t->text[i], 1, MSG_NOSIGNAL) != 1)
				break;
			usleep(t->gap_ms * 1000);
		}
		return 0;
	}
};

} // namespace

TEST_CASE("a client that sends the request line alone is answered at once", "[driver][streamsetup][request]")
{
	Pair p;
	p.say("GET /id=1001 HTTP/1.1\r\n");
	std::string line;
	const int64_t start = msNow();
	REQUIRE(readRequest(p.box, 2000, line));
	REQUIRE(msNow() - start < 1000);
	REQUIRE(line == "GET /id=1001 HTTP/1.1\r\n");
}

TEST_CASE("the header behind the request line is taken and let go, up to 8 KiB", "[driver][streamsetup][request]")
{
	Pair p;
	std::string request = "GET /id=1001 HTTP/1.1\r\nHost: box\r\nUser-Agent: player\r\n";
	request += std::string(6000, 'x') + "\r\n\r\n";
	p.say(request);
	std::string line;
	REQUIRE(readRequest(p.box, 2000, line));
	REQUIRE(line == "GET /id=1001 HTTP/1.1\r\n");
	// Nothing of it is left to be read.
	char c;
	REQUIRE(recv(p.box, &c, 1, MSG_DONTWAIT) == -1);
}

TEST_CASE("more than 8 KiB behind the line is left, and does not hold the reader up", "[driver][streamsetup][request]")
{
	Pair p;
	p.say("GET / HTTP/1.1\r\n" + std::string(9000, 'h'));
	std::string line;
	const int64_t start = msNow();
	REQUIRE(readRequest(p.box, 2000, line));
	REQUIRE(msNow() - start < 1000);
	char buf[2048];
	REQUIRE(recv(p.box, buf, sizeof(buf), MSG_DONTWAIT) == 9000 - 8192);
}

TEST_CASE("a request line that trickles in within the time is read whole", "[driver][streamsetup][request]")
{
	Pair p;
	std::string line;
	{
		Trickle t(p.client, "GET /id=2a\r\n", 40);
		REQUIRE(readRequest(p.box, 2000, line));
	}
	REQUIRE(line == "GET /id=2a\r\n");
}

TEST_CASE("a client that sends nothing is given up when the time is over", "[driver][streamsetup][request]")
{
	Pair p;
	std::string line;
	const int64_t start = msNow();
	REQUIRE_FALSE(readRequest(p.box, 300, line));
	const int64_t took = msNow() - start;
	REQUIRE(took >= 280);
	REQUIRE(took < 1000);
}

TEST_CASE("a request line still incomplete when the time is over is given up", "[driver][streamsetup][request]")
{
	Pair p;
	std::string line;
	Trickle t(p.client, "GET /id=1001 HTTP/1.1\r\n", 60);
	REQUIRE_FALSE(readRequest(p.box, 300, line));
}

TEST_CASE("a client that closes before the line end leaves what it sent", "[driver][streamsetup][request]")
{
	Pair p;
	p.say("GET /id=1001");
	shutdown(p.client, SHUT_WR);
	std::string line;
	REQUIRE(readRequest(p.box, 2000, line));
	REQUIRE(line == "GET /id=1001");

	Pair q;
	shutdown(q.client, SHUT_WR);
	REQUIRE_FALSE(readRequest(q.box, 2000, line));
}

TEST_CASE("a request line ends at 511 bytes", "[driver][streamsetup][request]")
{
	Pair p;
	p.say("GET /" + std::string(600, 'a') + "\r\n");
	std::string line;
	REQUIRE(readRequest(p.box, 2000, line));
	REQUIRE(line.size() == 511);
}

TEST_CASE("the two forms of a request name a channel", "[driver][streamsetup][request]")
{
	Request r = parseRequestLine("GET /id=b544000800850088 HTTP/1.1\r\n");
	REQUIRE(r.form == ById);
	REQUIRE(r.id == 0xb544000800850088ULL);

	r = parseRequestLine("GET /?id=1001&x=1 HTTP/1.0\r\n");
	REQUIRE(r.form == ById);
	REQUIRE(r.id == 0x1001);

	r = parseRequestLine("GET /1:0:19:2B66:3F3:1:C00000:0:0:0: HTTP/1.1\r\n");
	REQUIRE(r.form == E2);
	REQUIRE(r.id == ((0x3F3ULL << 32) | (0x1ULL << 16) | 0x2B66ULL));
	REQUIRE(r.satpos == 0xC0);
}

TEST_CASE("a request without a target asks for the live channel", "[driver][streamsetup][request]")
{
	REQUIRE(parseRequestLine("GET / HTTP/1.1\r\n").form == NoTarget);
	REQUIRE(parseRequestLine("GET /?foo=bar HTTP/1.1\r\n").form == NoTarget);
	REQUIRE(parseRequestLine("GET /").form == NoTarget);
}

TEST_CASE("an id that is not one, and a path that names nothing, are not found", "[driver][streamsetup][request]")
{
	REQUIRE(parseRequestLine("GET /id= HTTP/1.1\r\n").form == BadTarget);
	REQUIRE(parseRequestLine("GET /id=xyz HTTP/1.1\r\n").form == BadTarget);
	REQUIRE(parseRequestLine("GET /id=1001x HTTP/1.1\r\n").form == BadTarget);
	REQUIRE(parseRequestLine("GET /id=11112222333344445 HTTP/1.1\r\n").form == BadTarget);
	REQUIRE(parseRequestLine("GET /favicon.ico HTTP/1.1\r\n").form == BadTarget);
	REQUIRE(parseRequestLine("GET /1:0:19 HTTP/1.1\r\n").form == BadTarget);
}

TEST_CASE("anything but a GET of a path is garbage", "[driver][streamsetup][request]")
{
	REQUIRE(parseRequestLine("").form == Garbage);
	REQUIRE(parseRequestLine("POST /id=1001 HTTP/1.1\r\n").form == Garbage);
	REQUIRE(parseRequestLine("GET id=1001\r\n").form == Garbage);
	REQUIRE(parseRequestLine("\x16\x03\x01\x02").form == Garbage);
}

TEST_CASE("the status heads are these bytes", "[driver][streamsetup][request]")
{
	REQUIRE(statusBytes(200, 0) == "HTTP/1.1 200 OK\r\nServer: streamts (ts)\r\n\r\n");
	REQUIRE(statusBytes(404, "no such channel") ==
		"HTTP/1.1 404 Not Found\r\n"
		"Server: streamts (ts)\r\n"
		"Content-Type: text/plain\r\n"
		"Content-Length: 16\r\n"
		"Connection: close\r\n"
		"\r\n"
		"no such channel\n");
	REQUIRE(statusBytes(503, "no tuner is free for this channel") ==
		"HTTP/1.1 503 Service Unavailable\r\n"
		"Server: streamts (ts)\r\n"
		"Content-Type: text/plain\r\n"
		"Content-Length: 34\r\n"
		"Retry-After: 30\r\n"
		"Connection: close\r\n"
		"\r\n"
		"no tuner is free for this channel\n");
}

TEST_CASE("a refusal reaches the client whole and then the end of the stream", "[driver][streamsetup][request]")
{
	Pair p;
	p.say("GET /id=1001 HTTP/1.1\r\n");
	refuse(p.box, 503, "no tuner is free for this channel");
	// The end comes with the refusal, not with the close that follows later.
	const int64_t start = msNow();
	REQUIRE(p.heard() == statusBytes(503, "no tuner is free for this channel"));
	REQUIRE(msNow() - start < 500);
}

TEST_CASE("a refusal reads what the client still sends for half a second at most", "[driver][streamsetup][request]")
{
	Pair p;
	Trickle t(p.client, std::string(200, 'z'), 10);   // two seconds of bytes
	const int64_t start = msNow();
	refuse(p.box, 404, "no such channel");
	const int64_t took = msNow() - start;
	REQUIRE(took >= 400);
	REQUIRE(took < 1200);
}

TEST_CASE("a 200 goes out whole", "[driver][streamsetup][request]")
{
	Pair p;
	REQUIRE(writeOk(p.box));
	close(p.box);
	p.box = -1;
	REQUIRE(p.heard() == statusBytes(200, 0));
}

TEST_CASE("a 200 to a client that is gone fails without a signal", "[driver][streamsetup][request]")
{
	Pair p;
	close(p.client);
	p.client = -1;
	REQUIRE_FALSE(writeOk(p.box));
}

namespace
{

int sendBufferOf(int fd)
{
	int size = 0;
	socklen_t len = sizeof(size);
	REQUIRE(getsockopt(fd, SOL_SOCKET, SO_SNDBUF, &size, &len) == 0);
	return size;
}

} // namespace

TEST_CASE("a client's socket gets the send buffer asked for", "[driver][streamsetup][sndbuf]")
{
	Pair p;
	const int asked = 10 * 250 * 188;   // what the stream server asks for
	const int before = sendBufferOf(p.box);
	setSendBuffer(p.box, asked);
	const int after = sendBufferOf(p.box);
	REQUIRE(after > before);
	// The kernel caps the request at net.core.wmem_max and doubles it for
	// its own bookkeeping; where the cap can be read, the figure is exact.
	FILE *f = fopen("/proc/sys/net/core/wmem_max", "r");
	int wmem_max = 0;
	if (f)
	{
		if (fscanf(f, "%d", &wmem_max) != 1)
			wmem_max = 0;
		fclose(f);
	}
	if (wmem_max > 0)
		REQUIRE(after == 2 * std::min(asked, wmem_max));
}
