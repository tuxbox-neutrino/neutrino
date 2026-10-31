/*
 * test_streamhold.cpp - the register of the frontends held for streams
 *
 * Copyright (C) 2026 Thilo Graf
 *
 * License: GPL-2.0-or-later
 *
 * The register is compiled into this file with its test points, which the
 * program's build does not have. A point stops one thread right after it
 * read a place, or right after it adopted one; the case then does what
 * another thread could do in that moment and lets it go on. Every case
 * counts how often each frontend is given back: exactly once is the claim.
 */

#define STREAMHOLD_TEST_POINTS 1
#include "zapit/streamhold.cpp"

#include "support/catch.hpp"

#include <atomic>
#include <cerrno>
#include <csignal>
#include <cstring>
#include <map>
#include <vector>

#include <pthread.h>
#include <signal.h>
#include <time.h>

using namespace streamhold;

namespace
{

/* Two frontends; what they are does not matter, only that they differ. */
int fe_a;
int fe_b;
void *const A = &fe_a;
void *const B = &fe_b;

std::map<void *, int> given_back;
pthread_mutex_t given_back_lock = PTHREAD_MUTEX_INITIALIZER;

void release(void *fe)
{
	pthread_mutex_lock(&given_back_lock);
	given_back[fe]++;
	pthread_mutex_unlock(&given_back_lock);
}

int givenBack(void *fe)
{
	pthread_mutex_lock(&given_back_lock);
	const int n = given_back[fe];
	pthread_mutex_unlock(&given_back_lock);
	return n;
}

/* One stop: the first thread that is not the case's own and passes the point
   waits there until the case lets it go. */
struct Stop
{
	pthread_mutex_t m;
	pthread_cond_t  c;
	TestPoint       point;
	bool            armed;
	bool            arrived;
	bool            go;
	pthread_t       owner;

	Stop() : point(AfterAdoptLoad), armed(false), arrived(false), go(false), owner(pthread_self())
	{
		pthread_mutex_init(&m, 0);
		pthread_cond_init(&c, 0);
	}
	~Stop()
	{
		pthread_cond_destroy(&c);
		pthread_mutex_destroy(&m);
	}

	void arm(TestPoint p)
	{
		pthread_mutex_lock(&m);
		point = p;
		armed = true;
		arrived = false;
		go = false;
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

	void at(TestPoint p)
	{
		if (pthread_equal(pthread_self(), owner))
			return;
		pthread_mutex_lock(&m);
		if (!armed || p != point)
		{
			pthread_mutex_unlock(&m);
			return;
		}
		armed = false;
		arrived = true;
		pthread_cond_broadcast(&c);
		while (!go)
			pthread_cond_wait(&c, &m);
		pthread_mutex_unlock(&m);
	}
};

Stop *the_stop = 0;

void hook(TestPoint p, unsigned)
{
	if (the_stop)
		the_stop->at(p);
}

/* Each case starts on an empty register with its counts at zero. */
struct Fresh
{
	Stop stop;

	Fresh()
	{
		resetForTest();
		pthread_mutex_lock(&given_back_lock);
		given_back.clear();
		pthread_mutex_unlock(&given_back_lock);
		the_stop = &stop;
		setTestHook(&hook);
	}
	~Fresh()
	{
		setTestHook(0);
		the_stop = 0;
		resetForTest();
	}
};

// A thread that runs one call on the register and keeps its answer.
struct Call
{
	enum What { Adopt, Drop } what;
	HoldId id;
	uint32_t now;
	void *got;
	pthread_t thread;

	Call(What w, HoldId i, uint32_t t) : what(w), id(i), now(t), got(0)
	{
		REQUIRE(pthread_create(&thread, 0, &run, this) == 0);
	}

	void join()
	{
		pthread_join(thread, 0);
	}

	static void *run(void *arg)
	{
		Call *c = static_cast<Call *>(arg);
		if (c->what == Adopt)
			c->got = adopt(c->id, c->now);
		else
			drop(c->id);
		return 0;
	}
};

const uint32_t T0 = 1000;
const uint32_t InTime = T0 + 1;
const uint32_t TooLate = T0 + kAdoptWithinMs + 1;

} // namespace

TEST_CASE("a hold is offered, adopted with its frontend, dropped and given back once", "[zapit][streamhold]")
{
	Fresh fresh;
	const HoldId id = offer(A, T0);
	REQUIRE(id != 0);
	REQUIRE(adopt(id, InTime) == A);
	// Adopted once: a second owner gets nothing.
	REQUIRE(adopt(id, InTime) == 0);

	collect(InTime, &release);
	REQUIRE(givenBack(A) == 0);

	drop(id);
	collect(InTime, &release);
	REQUIRE(givenBack(A) == 1);
	collect(TooLate, &release);
	REQUIRE(givenBack(A) == 1);

	// A number that is done with matches nothing.
	REQUIRE(adopt(id, InTime) == 0);
	drop(id);
	collect(InTime, &release);
	REQUIRE(givenBack(A) == 1);
}

TEST_CASE("B1: a drop that lost its swap to an adopt tries again and the hold ends", "[zapit][streamhold]")
{
	Fresh fresh;
	const HoldId id = offer(A, T0);

	fresh.stop.arm(AfterDropLoad);
	Call dropper(Call::Drop, id, 0);
	fresh.stop.waitArrived();     // it read OFFERED

	REQUIRE(adopt(id, InTime) == A);
	fresh.stop.letGo();           // its swap from OFFERED fails
	dropper.join();

	collect(InTime, &release);
	REQUIRE(givenBack(A) == 1);
}

TEST_CASE("B2: a drop that lost its swap to the expiry ends with nothing to do", "[zapit][streamhold]")
{
	Fresh fresh;
	const HoldId id = offer(A, T0);

	fresh.stop.arm(AfterDropLoad);
	Call dropper(Call::Drop, id, 0);
	fresh.stop.waitArrived();

	collect(TooLate, &release);   // nobody came for it in time
	REQUIRE(givenBack(A) == 1);
	fresh.stop.letGo();
	dropper.join();

	collect(TooLate, &release);
	REQUIRE(givenBack(A) == 1);
}

TEST_CASE("B3: an adopt that lost its swap to a drop answers nothing", "[zapit][streamhold]")
{
	Fresh fresh;
	const HoldId id = offer(A, T0);

	fresh.stop.arm(AfterAdoptLoad);
	Call adopter(Call::Adopt, id, InTime);
	fresh.stop.waitArrived();     // it read OFFERED

	drop(id);
	fresh.stop.letGo();
	adopter.join();
	REQUIRE(adopter.got == 0);

	collect(InTime, &release);
	REQUIRE(givenBack(A) == 1);
}

TEST_CASE("B4: a hold past its time cannot be adopted, and collect gives it back", "[zapit][streamhold]")
{
	Fresh fresh;
	const HoldId id = offer(A, T0);

	REQUIRE(adopt(id, TooLate) == 0);
	// No swap was made: it is still offered, and still counted as held.
	void *in_use[8];
	REQUIRE(held(in_use, 8) == 1);
	REQUIRE(in_use[0] == A);

	collect(InTime, &release);    // not due by the clock of this call
	REQUIRE(givenBack(A) == 0);
	collect(TooLate, &release);
	REQUIRE(givenBack(A) == 1);
	REQUIRE(held(in_use, 8) == 0);
}

TEST_CASE("B5: shutdown gives back offered, adopted and dropped holds and closes the register", "[zapit][streamhold]")
{
	Fresh fresh;
	int fe_c;
	void *const C = &fe_c;

	const HoldId offered = offer(A, T0);
	const HoldId adopted = offer(B, T0);
	const HoldId dropped = offer(C, T0);
	REQUIRE(adopt(adopted, InTime) == B);
	REQUIRE(adopt(dropped, InTime) == C);
	drop(dropped);

	shutdown(&release);
	REQUIRE(givenBack(A) == 1);
	REQUIRE(givenBack(B) == 1);
	REQUIRE(givenBack(C) == 1);

	REQUIRE(adopt(offered, InTime) == 0);
	drop(adopted);
	drop(offered);
	collect(TooLate, &release);
	REQUIRE(givenBack(A) == 1);
	REQUIRE(givenBack(B) == 1);
	REQUIRE(givenBack(C) == 1);

	REQUIRE(offer(A, T0) == 0);
	REQUIRE_FALSE(canOffer());
}

TEST_CASE("B6: a drop that read an adopted hold ends after shutdown gave it back", "[zapit][streamhold]")
{
	Fresh fresh;
	const HoldId id = offer(A, T0);
	REQUIRE(adopt(id, InTime) == A);

	fresh.stop.arm(AfterDropLoad);
	Call dropper(Call::Drop, id, 0);
	fresh.stop.waitArrived();     // it read ADOPTED

	shutdown(&release);
	REQUIRE(givenBack(A) == 1);
	fresh.stop.letGo();
	dropper.join();

	collect(TooLate, &release);
	REQUIRE(givenBack(A) == 1);
}

TEST_CASE("B7: a place whose generation reached the top is retired, not wrapped", "[zapit][streamhold]")
{
	Fresh fresh;
	/* offer takes the first free place, so the last place is the one left
	   once the others are in use. */
	const unsigned top = kPlacesForTest - 1;
	setGenerationForTest(top, kTopGenerationForTest - 1);
	std::vector<HoldId> others;
	for (unsigned i = 0; i < top; ++i)
		others.push_back(offer(B, T0));
	REQUIRE(others.back() != 0);
	REQUIRE((others.back() & 7u) == top - 1);

	const HoldId last = offer(A, T0);
	REQUIRE(last != 0);
	REQUIRE((last & 7u) == top);
	// The highest number a place can carry is still a valid one.
	REQUIRE((last >> 3) == kTopGenerationForTest);
	REQUIRE(adopt(last, InTime) == A);
	drop(last);
	collect(InTime, &release);
	REQUIRE(givenBack(A) == 1);

	// The place is free and never offered again.
	REQUIRE_FALSE(canOffer());
	REQUIRE(offer(A, T0) == 0);
	REQUIRE(adopt(last, InTime) == 0);

	// A place given back below the top is offered again, with a new number.
	drop(others[0]);
	collect(InTime, &release);
	REQUIRE(canOffer());
	const HoldId again = offer(A, T0);
	REQUIRE(again != 0);
	REQUIRE(again != others[0]);
	REQUIRE((again & 7u) == (others[0] & 7u));
}

namespace
{

HoldId signalled_id = 0;
volatile sig_atomic_t signal_ran = 0;

void dropFromHandler(int)
{
	drop(signalled_id);
	signal_ran = 1;
}

struct SignalAtAdopt
{
	static void at(TestPoint p, unsigned)
	{
		if (p == AfterAdoptLoad && !signal_ran)
			raise(SIGUSR1);   // delivered to this thread, before it returns
	}
};

} // namespace

TEST_CASE("B8: a drop from a signal handler at adopt's stop ends the hold", "[zapit][streamhold]")
{
	Fresh fresh;
	signalled_id = offer(A, T0);
	signal_ran = 0;

	struct sigaction act, old;
	memset(&act, 0, sizeof(act));
	act.sa_handler = &dropFromHandler;
	sigemptyset(&act.sa_mask);
	REQUIRE(sigaction(SIGUSR1, &act, &old) == 0);
	setTestHook(&SignalAtAdopt::at);

	void *got = adopt(signalled_id, InTime);

	setTestHook(&hook);
	sigaction(SIGUSR1, &old, 0);
	REQUIRE(signal_ran == 1);
	// The handler's drop came between the read and the swap: nothing adopted.
	REQUIRE(got == 0);
	collect(InTime, &release);
	REQUIRE(givenBack(A) == 1);
}

TEST_CASE("an adopt hands back the frontend it adopted, whatever the place holds after", "[zapit][streamhold]")
{
	/* The adopter stops right after its swap. In that moment its hold is
	   dropped, collected, and the place offered again with another
	   frontend. What it hands back is still the frontend it adopted. */
	Fresh fresh;
	const HoldId first = offer(A, T0);

	fresh.stop.arm(AfterAdoptCas);
	Call adopter(Call::Adopt, first, InTime);
	fresh.stop.waitArrived();

	drop(first);
	collect(InTime, &release);
	REQUIRE(givenBack(A) == 1);
	const HoldId second = offer(B, T0);
	REQUIRE(second != 0);
	REQUIRE((second & 7u) == (first & 7u));   // the same place, reused

	fresh.stop.letGo();
	adopter.join();
	REQUIRE(adopter.got == A);

	REQUIRE(adopt(second, InTime) == B);
	REQUIRE(adopt(first, InTime) == 0);
	drop(second);
	collect(InTime, &release);
	REQUIRE(givenBack(B) == 1);
	REQUIRE(givenBack(A) == 1);
}

TEST_CASE("a collect that lost its swap to a drop looks again in the same pass", "[zapit][streamhold]")
{
	Fresh fresh;
	const HoldId id = offer(A, T0);

	// The collect runs on its own thread here and stops after its read.
	struct Collector
	{
		static void *run(void *)
		{
			collect(TooLate, &release);
			return 0;
		}
	};
	fresh.stop.arm(AfterCollectLoad);
	pthread_t t;
	REQUIRE(pthread_create(&t, 0, &Collector::run, 0) == 0);
	fresh.stop.waitArrived();     // it read OFFERED, past its time

	REQUIRE(adopt(id, InTime) == A);   // the owner came, by its own clock
	drop(id);
	fresh.stop.letGo();
	pthread_join(t, 0);

	REQUIRE(givenBack(A) == 1);
}

namespace
{

/* The load case: owners adopt and drop from their threads while zapit's
   thread offers and collects. Every frontend offered is given back once, by
   the end of a shutdown at the latest. */
const int kOwners = 4;
const int kRounds = 3000;

struct Shared
{
	std::atomic<HoldId> ids[kOwners];
	std::atomic<bool> done;
	std::atomic<int> adopted;
};

Shared shared;
int frontends[kOwners * kRounds];

void *owner(void *arg)
{
	const int me = (int) (intptr_t) arg;
	while (!shared.done.load())
	{
		const HoldId id = shared.ids[me].exchange(0);
		if (!id)
			continue;
		if (adopt(id, nowMs()))
			shared.adopted++;
		// Every other one is dropped twice, from the owner and its signal path.
		drop(id);
		if (id & 8u)
			drop(id);
	}
	return 0;
}

} // namespace

TEST_CASE("under load every offered frontend is given back exactly once", "[zapit][streamhold]")
{
	Fresh fresh;
	setTestHook(0);
	shared.done = false;
	shared.adopted = 0;
	for (int i = 0; i < kOwners; ++i)
		shared.ids[i] = 0;

	pthread_t threads[kOwners];
	for (int i = 0; i < kOwners; ++i)
		REQUIRE(pthread_create(&threads[i], 0, &owner, (void *) (intptr_t) i) == 0);

	int offered = 0;
	int next = 0;
	while (offered < kOwners * kRounds)
	{
		collect(nowMs(), &release);
		const int to = next++ % kOwners;
		if (shared.ids[to].load() != 0)
			continue;
		// One in five is never handed to an owner: offered past its time, it
		// has to expire.
		const bool orphan = offered % 5 == 0;
		void *fe = &frontends[offered];
		const HoldId id = offer(fe, nowMs() - (orphan ? kAdoptWithinMs + 10 : 0));
		if (!id)
			continue;
		offered++;
		if (!orphan)
			shared.ids[to] = id;
	}

	shared.done = true;
	for (int i = 0; i < kOwners; ++i)
		pthread_join(threads[i], 0);
	// Holds handed to an owner that stopped before it came for them expire.
	collect(nowMs() + kAdoptWithinMs + 10, &release);
	shutdown(&release);

	int wrong = 0;
	for (int i = 0; i < offered; ++i)
		if (givenBack(&frontends[i]) != 1)
			wrong++;
	REQUIRE(wrong == 0);
	REQUIRE(shared.adopted.load() > 0);
}
