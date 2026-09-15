/*
 * test_eventbus.cpp - tests for the event bus
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

#include <cerrno>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <pthread.h>
#include <sys/wait.h>
#include <unistd.h>

#include <new>
#include <vector>

#include "coreapi/base/eventbus.h"

using namespace coreapi;

namespace
{

struct Collector : public Subscriber
{
	std::vector<Event> seen;
	void onEvent(const Event &e) { seen.push_back(e); }
};

struct Recurser : public Subscriber
{
	int calls;
	Recurser() : calls(0) {}
	void onEvent(const Event &)
	{
		calls++;
		/* bounded, so a bus that lets this through is counted rather than run
		   out of stack, which would take the whole runner with it */
		if (calls > 4)
			return;

		Event e;
		e.type = EventType::Zap;
		EventBus::instance().publish(e);   // must not recurse
	}
};

} // namespace

/* First case in the file: a bus that fails to unsubscribe leaves this one's
   collector behind, and every later case then publishes into freed stack. */
TEST_CASE("an unsubscribed collector stops receiving", "[eventbus]")
{
	Collector c;
	EventBus::instance().subscribe(&c);
	EventBus::instance().unsubscribe(&c);

	Event e;
	e.type = EventType::Standby;
	EventBus::instance().publish(e);

	REQUIRE(c.seen.empty());
}

TEST_CASE("subscribers receive published events", "[eventbus]")
{
	Collector c;
	EventBus::instance().subscribe(&c);

	Event e;
	e.type = EventType::Zap;
	e.channel_id = 0x2b66;
	EventBus::instance().publish(e);

	/* unsubscribed before anything may throw: the bus outlives the test, and a
	   subscriber left in it is a dangling pointer the next case walks into */
	EventBus::instance().unsubscribe(&c);

	REQUIRE(c.seen.size() == 1);
	REQUIRE(c.seen[0].type == EventType::Zap);
	REQUIRE(c.seen[0].channel_id == 0x2b66);
}

TEST_CASE("publishing from inside a subscriber does not recurse", "[eventbus]")
{
	Recurser r;
	EventBus::instance().subscribe(&r);

	Event e;
	e.type = EventType::Mode;
	EventBus::instance().publish(e);

	EventBus::instance().unsubscribe(&r);
	REQUIRE(r.calls == 1);
}

/* An event is filled field by field at the publish site, so anything left
   unset has to read as absent rather than as whatever the stack held. */
TEST_CASE("a fresh event carries no payload of its own", "[eventbus]")
{
	Event e;

	REQUIRE(e.type == EventType::Zap);
	REQUIRE(e.channel_id == 0);
	REQUIRE(e.value == 0);
	REQUIRE(e.text.empty());
}

TEST_CASE("subscribing twice delivers once", "[eventbus]")
{
	Collector c;
	EventBus::instance().subscribe(&c);
	EventBus::instance().subscribe(&c);

	Event e;
	e.type = EventType::Volume;
	EventBus::instance().publish(e);

	EventBus::instance().unsubscribe(&c);
	REQUIRE(c.seen.size() == 1);
}

/* The guard against re-entry is lowered again on the way out, otherwise the
   first event on a thread would also be the last one. */
TEST_CASE("delivery resumes after a publish returns", "[eventbus]")
{
	Collector c;
	EventBus::instance().subscribe(&c);

	Event e;
	e.type = EventType::Volume;
	EventBus::instance().publish(e);
	e.value = 7;
	EventBus::instance().publish(e);

	EventBus::instance().unsubscribe(&c);
	REQUIRE(c.seen.size() == 2);
	REQUIRE(c.seen[1].value == 7);
}

namespace
{

struct Marker : public Subscriber
{
	int *dead;
	Marker(int *d) : dead(d) {}
	~Marker() { (*dead)++; }
	void onEvent(const Event &) {}
};

} // namespace

/* Owners hold their subscriber by the base type and delete it through that,
   which reaches the derived destructor only if the base one is virtual. */
TEST_CASE("a subscriber deleted through the base runs its own destructor", "[eventbus]")
{
	int dead = 0;
	Subscriber *s = new Marker(&dead);
	delete s;

	REQUIRE(dead == 1);
}

namespace
{

struct SelfRemover : public Subscriber
{
	int calls;
	SelfRemover() : calls(0) {}
	void onEvent(const Event &)
	{
		calls++;
		EventBus::instance().unsubscribe(this);
	}
};

struct Thrower : public Subscriber
{
	void onEvent(const Event &) { throw 1; }
};

struct Quiet : public Subscriber
{
	void onEvent(const Event &) {}
};

/* Exit codes rather than assertions, because a failed assertion throws and
   this runs in a child that has nothing to catch it. */
void dropSelfDuringDelivery()
{
	SelfRemover head;
	Collector tail;
	EventBus::instance().subscribe(&head);
	EventBus::instance().subscribe(&tail);

	Event e;
	e.type = EventType::EpgUpdated;
	EventBus::instance().publish(e);

	if (tail.seen.size() != 1)
		_exit(2);

	EventBus::instance().publish(e);

	if (head.calls != 1)
		_exit(3);
	if (tail.seen.size() != 2)
		_exit(4);
}

void publishAfterASubscriberDies()
{
	Collector tail;
	EventBus::instance().subscribe(&tail);

	/* Built in storage this function owns so the bytes can be cleared after the
	   destructor: a bus that still holds the pointer then calls through a
	   zeroed vtable instead of through memory that happens to survive. */
	alignas(Collector) static char storage[sizeof(Collector)];
	Collector *doomed = new (storage) Collector;
	EventBus::instance().subscribe(doomed);
	doomed->~Collector();
	memset(storage, 0, sizeof(storage));

	Event e;
	e.type = EventType::TimerChanged;
	EventBus::instance().publish(e);

	if (tail.seen.size() != 1)
		_exit(2);
}

void deliverPastAThrow()
{
	Thrower head;
	Collector tail;
	EventBus::instance().subscribe(&head);
	EventBus::instance().subscribe(&tail);

	/* stderr into a file this body can measure: the note the bus writes is the
	   only trace a dropped event leaves, so it is read back rather than assumed */
	char path[] = "/tmp/coreapi-eventbus-XXXXXX";
	int fd = mkstemp(path);
	if (fd < 0 || dup2(fd, STDERR_FILENO) < 0)
		_exit(6);
	unlink(path);

	Event e;
	e.type = EventType::RecordStart;
	EventBus::instance().publish(e);

	if (lseek(fd, 0, SEEK_END) <= 0)
		_exit(7);
	if (tail.seen.size() != 1)
		_exit(2);

	/* the guard has to be down again, or the throw would cost every later
	   event on this thread as well */
	EventBus::instance().publish(e);

	if (tail.seen.size() != 2)
		_exit(3);
}

struct Churn
{
	Quiet a;
	Quiet b;
};

void *churnList(void *arg)
{
	Churn *c = static_cast<Churn *>(arg);
	for (int i = 0; i < 20000; i++)
	{
		EventBus::instance().subscribe(&c->a);
		EventBus::instance().subscribe(&c->b);
		EventBus::instance().unsubscribe(&c->a);
		EventBus::instance().unsubscribe(&c->b);
	}
	return 0;
}

// Two threads on the list at once, see the case below for what that is worth.
void hammerTheSubscriberList()
{
	/* never given back: the publisher may still hold a copy of the list naming
	   them when the churning thread has finished with them */
	Churn *c = new Churn;

	pthread_t churner;
	if (pthread_create(&churner, 0, churnList, c) != 0)
		_exit(5);

	Event e;
	e.type = EventType::Zap;
	for (int i = 0; i < 20000; i++)
		EventBus::instance().publish(e);

	pthread_join(churner, 0);
}

/* Not zero, which is what a run that matched no case at all ends with, so a
   body that was never reached cannot be read as one that came through. */
const int EXIT_HANDLERS_RAN = 42;

/* Ends through exit rather than _exit, unlike the other bodies here, so the
   exit handlers run while a subscriber is still registered and the publish in
   publishes_at_exit has a list to read. */
void publishFromAnExitHandler()
{
	/* Enough of them that the list buffer is a mapping of its own: a bus torn
	   down by an earlier exit handler has already given that mapping back, so
	   the read faults instead of finding bytes that happen to survive. */
	for (int i = 0; i < 20000; i++)
		EventBus::instance().subscribe(new Quiet);

	exit(EXIT_HANDLERS_RAN);
}

/* Bounded, because a body that fails by not returning would otherwise take the
   runner with it. */
int waitFor(pid_t pid)
{
	int st = 0;
	for (int i = 0; i < 500; i++)
	{
		if (waitpid(pid, &st, WNOHANG) == pid)
			return WIFEXITED(st) ? WEXITSTATUS(st) : -1;
		usleep(10000);
	}
	kill(pid, SIGKILL);
	waitpid(pid, &st, 0);
	return -1;
}

/* In a child, because these failures are a deadlock or a crash rather than a
   wrong answer, and either would take the runner with it. */
int exitCodeOf(void (*body)())
{
	/* the child would otherwise flush the runner's buffered output a second
	   time and make one run look like two */
	fflush(0);

	pid_t pid = fork();
	if (pid < 0)
		return -1;
	if (pid == 0)
	{
		/* the child is a copy of the runner, so an escaping exception would
		   land in the enclosing test case and carry on running the suite */
		try { body(); }
		catch (...) { _exit(9); }
		_exit(0);
	}

	return waitFor(pid);
}

/* A body that ends through exit needs a process that has built nothing else:
   exit runs every static destructor the binary registered, and the fixtures the
   other files create on first use unlink real paths when theirs run. A fork of
   the runner carries those fixtures and would delete them; a fresh run of the
   same binary has none of them. */
int exitCodeOfFreshRun(const char *test_name)
{
	fflush(0);

	pid_t pid = fork();
	if (pid < 0)
		return -1;
	if (pid == 0)
	{
		/* the report of a run nobody asked to see */
		freopen("/dev/null", "w", stdout);
		freopen("/dev/null", "w", stderr);

		char self[] = "/proc/self/exe";
		std::vector<char> name(test_name, test_name + strlen(test_name) + 1);
		char *argv[3];
		argv[0] = self;
		argv[1] = &name[0];
		argv[2] = 0;
		execv(self, argv);
		_exit(9);
	}

	return waitFor(pid);
}

} // namespace

/* Selected by name from the case below and hidden from every other run, because
   the body never returns. */
TEST_CASE("a bus published to from an exit handler", "[.][eventbus]")
{
	publishFromAnExitHandler();
}

/* The list is walked from a copy, so removing the caller's own entry neither
   skips the subscriber behind it nor walks off the end. */
TEST_CASE("a subscriber may unsubscribe itself while it is being called", "[eventbus]")
{
	REQUIRE(exitCodeOf(dropSelfDuringDelivery) == 0);
}

/* The registration ends with the subscriber, which is the only thing that can
   keep an owner from leaving a pointer to freed memory in the bus. */
TEST_CASE("a destroyed subscriber is off the list", "[eventbus]")
{
	REQUIRE(exitCodeOf(publishAfterASubscriberDies) == 0);
}

/* Caught at the delivery site because nothing above it can catch: the callers
   are built without exceptions. */
TEST_CASE("a subscriber that throws does not stop the ones behind it", "[eventbus]")
{
	REQUIRE(exitCodeOf(deliverPastAThrow) == 0);
}

/* A smoke test, and it says so rather than reading as a proof. The window is the first
   reallocation, where the publisher copies a buffer that is being freed underneath it.
   A vector never hands capacity back, so from the second cycle on there is nothing left
   to race against: this catches an unlocked bus about four times in five, and a bus
   whose list already has capacity not at all. */
TEST_CASE("the list survives being churned while it is published to", "[eventbus]")
{
	REQUIRE(exitCodeOf(hammerTheSubscriberList) == 0);
}

/* The bus is never destroyed, because a subscriber or another static may still
   publish while the exit handlers run and the bus is torn down before them. */
TEST_CASE("the bus outlives the exit handlers", "[eventbus]")
{
	REQUIRE(exitCodeOfFreshRun("a bus published to from an exit handler") == EXIT_HANDLERS_RAN);
}

namespace
{

/* A gate rather than a sleep: the second publish has to begin while the first
   is still inside a subscriber, or the guard is never asked the question. */
struct Gate
{
	pthread_mutex_t m;
	pthread_cond_t  c;
	bool            open;

	Gate() : open(false)
	{
		pthread_mutex_init(&m, 0);
		pthread_cond_init(&c, 0);
	}

	~Gate()
	{
		pthread_cond_destroy(&c);
		pthread_mutex_destroy(&m);
	}

	void raise()
	{
		pthread_mutex_lock(&m);
		open = true;
		pthread_cond_broadcast(&c);
		pthread_mutex_unlock(&m);
	}

	/* false once the deadline passes, so a gate that never opens ends the test
	   rather than stopping it */
	bool waitFor(int seconds)
	{
		struct timespec until;
		clock_gettime(CLOCK_REALTIME, &until);
		until.tv_sec += seconds;

		pthread_mutex_lock(&m);
		while (!open && pthread_cond_timedwait(&c, &m, &until) != ETIMEDOUT)
			;
		bool opened = open;
		pthread_mutex_unlock(&m);
		return opened;
	}
};

struct Concurrent : public Subscriber
{
	Gate      entered;
	Gate      answered;
	pthread_t first;
	int       first_calls;
	int       second_calls;
	bool      met_the_other;

	Concurrent() : first(0), first_calls(0), second_calls(0), met_the_other(false) {}

	void onEvent(const Event &)
	{
		if (pthread_equal(pthread_self(), first))
		{
			first_calls++;
			entered.raise();
			met_the_other = answered.waitFor(5);
		}
		else
		{
			second_calls++;
			answered.raise();
		}
	}
};

void *publishOnOwnThread(void *arg)
{
	Concurrent *c = static_cast<Concurrent *>(arg);
	c->first = pthread_self();

	Event e;
	e.type = EventType::RecordStart;
	EventBus::instance().publish(e);
	return 0;
}

} // namespace

/* The guard belongs to the thread that raised it. Only a publish nested inside
   another one on the same thread is dropped; two threads publishing at once
   both deliver. */
TEST_CASE("a publish from a second thread is delivered during the first", "[eventbus]")
{
	Concurrent c;
	EventBus::instance().subscribe(&c);

	pthread_t first;
	bool started = pthread_create(&first, 0, publishOnOwnThread, &c) == 0;
	bool inside = started && c.entered.waitFor(5);

	if (inside)
	{
		Event e;
		e.type = EventType::RecordStop;
		EventBus::instance().publish(e);
	}

	/* joined before anything may throw, the thread reads the subscriber */
	if (started)
		pthread_join(first, 0);
	EventBus::instance().unsubscribe(&c);

	REQUIRE(inside);
	REQUIRE(c.first_calls == 1);
	REQUIRE(c.second_calls == 1);
	REQUIRE(c.met_the_other);
}

namespace
{

/* Built before the bus and therefore torn down after it, which is what makes
   the case above able to tell a destroyed bus from a live one. */
struct PublishesAtExit
{
	~PublishesAtExit()
	{
		Event e;
		e.type = EventType::Standby;
		EventBus::instance().publish(e);
	}
};

PublishesAtExit publishes_at_exit;

} // namespace
