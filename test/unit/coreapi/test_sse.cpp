/*
 * test_sse.cpp - tests for the event stream
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
 *
 * DELIBERATE DRIFT from the source's test_sse.cpp: their wire cases drive the
 * libmicrohttpd server this tree does not carry. The transport-free cases
 * are here as written; the wire cases follow with nhttpd's own harness in
 * the bridge slice.
 */

#include "support/catch.hpp"

#include "httpd/endpoint.h"
#include "httpd/events.h"
#include "httpd/http.h"
#include "httpd/router.h"
#include "httpd/webconfig.h"


#include "coreapi/base/eventbus.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <atomic>
#include <set>
#include <string>
#include <vector>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <poll.h>
#include <pthread.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>

using namespace httpd;

namespace
{

// What the header beside the code says these are, so that a case restoring
// them puts back what ships and a case reading them fails when the shipped
// value moves without anybody saying so.
const unsigned kShippedHeartbeatMs = 20000;
const size_t   kShippedCeiling     = 256u * 1024u;

} // namespace

TEST_CASE("every event type has a name and no two share one", "[sse]")
{
	std::set<std::string> seen;
	for (int i = 0; i <= (int) coreapi::EventType::SettingsChanged; ++i)
	{
		const char *n = events::typeName((coreapi::EventType) i);
		INFO("type value " << i);
		REQUIRE(n != NULL);
		REQUIRE(std::string(n).size() > 0);
		REQUIRE(seen.insert(n).second);
	}
	REQUIRE(seen.size() == 11);
}

TEST_CASE("the shipped beat and the shipped ceiling are what the header says", "[sse]")
{
	/* Both are settable, and both are set by the cases below to something a
	   run can afford to wait for. What ships is the pair a box runs on, and
	   nothing else in this file would notice it moving. */
	REQUIRE(events::heartbeatMs() == kShippedHeartbeatMs);
	REQUIRE(events::pendingCeiling() == kShippedCeiling);
}

TEST_CASE("the route is a read and answers with a request for a stream", "[sse]")
{
	// The gate is asked before the handler, so a caller with less than the
	// route declares never reaches the answer that would open one.
	Response denied = dispatchIn(eventsTable, Method::Get, "/api/v1/events",
	                             "", "", "127.0.0.1", AuthLevel::Public);
	REQUIRE(denied.code == 403);
	REQUIRE_FALSE(events::isStream(denied));

	Response allowed = dispatchIn(eventsTable, Method::Get, "/api/v1/events",
	                              "", "", "127.0.0.1", AuthLevel::Read);
	REQUIRE(allowed.code == 200);
	REQUIRE(events::isStream(allowed));
	REQUIRE(allowed.body.empty());
}


namespace
{

void publish(coreapi::EventType t, uint64_t channel_id, int value)
{
	coreapi::Event e;
	e.type = t;
	e.channel_id = channel_id;
	e.value = value;
	coreapi::EventBus::instance().publish(e);
}

{
	struct timespec t;
	if (clock_gettime(CLOCK_MONOTONIC, &t) != 0)
		return 0;
	return (int64_t) t.tv_sec * 1000 + t.tv_nsec / 1000000;
}

void napMs(int ms)
{
	struct timespec t;
	t.tv_sec = ms / 1000;
	t.tv_nsec = (long) (ms % 1000) * 1000 * 1000;
	nanosleep(&t, NULL);
}

size_t waitForStreams(size_t want, int budget_ms)
{
	for (int waited = 0; waited < budget_ms; waited += 10)
	{
		if (events::openStreams() == want)
			return want;
		napMs(10);
	}
	return events::openStreams();
}

	return NULL;
}

	{
		WebConfig c = defaultWebConfig();
		c.sse_max_streams = max_streams;
		setConfigForTest(c);

		events::start();
		events::setHeartbeatMs(beat_ms);

		struct sockaddr_in a;
		std::memset(&a, 0, sizeof(a));
		a.sin_family = AF_INET;
		for (;;)
		{
				return NULL;
		events::stop();
		for (;;)
		{
		setConfigForTest(defaultWebConfig());
		events::setHeartbeatMs(kShippedHeartbeatMs);
		events::setPendingCeiling(kShippedCeiling);
	}

private:
		if (fd < 0)
			return;
		if (rcvbuf > 0)
		struct sockaddr_in a;
		std::memset(&a, 0, sizeof(a));
		a.sin_family = AF_INET;
		a.sin_port = htons((uint16_t) port);
		for (;;)
		{
		for (;;)
		{
				return out;
			}
		for (;;)
		{
			bool all = true;
			if (all)
		if (left <= 0)
			return false;
		struct pollfd p;
		p.events = POLLIN;
			return false;
		char buf[4096];
		if (n <= 0)
			return false;
		return true;
	}

};

} // namespace

TEST_CASE("a published event reaches a connected stream", "[sse]")
{
	REQUIRE(s.ok());
	REQUIRE(s.code() == 200);
	REQUIRE(s.headerOf("Content-Type") == "text/event-stream");
	REQUIRE(s.headerOf("Cache-Control") == "no-cache, no-transform");
	REQUIRE(s.headerOf("X-Accel-Buffering") == "no");

	// The stream opens with the field that says how long to wait before coming
	// back, so the first frame is that and the event is the one after it.
	REQUIRE(s.readFrame(2000).find("retry:") != std::string::npos);

	publish(coreapi::EventType::Zap, 0x2b66, 0);

	const std::string frame = s.readFrame(2000);
	INFO(frame);
	REQUIRE(frame.find("event: zap") != std::string::npos);
	REQUIRE(frame.find("\"channel_id\"") != std::string::npos);
	// Read as hexadecimal where it is read, so written as hexadecimal here.
	REQUIRE(frame.find("\"channel_id\":\"2b66\"") != std::string::npos);
	// The frame ends right after the document, which is what a reader watching
	// for a blank line takes as the end of the event.
	REQUIRE(frame.size() > 3);
	REQUIRE(frame.compare(frame.size() - 3, 3, "}\n\n") == 0);
}

TEST_CASE("the stream opens with a retry field", "[sse]")
{
	// The browser default is a fixed few seconds, which turns a restart into a
	// stampede from every open tab at once.
	REQUIRE(s.ok());

	const std::string first = s.readFrame(2000);
	INFO(first);
	REQUIRE(first.find("retry:") != std::string::npos);

	// Longer than the default it replaces, or it would be a stampede sooner
	// rather than no stampede.
	const size_t at = first.find("retry:");
	const long ms = std::strtol(first.c_str() + at + 6, NULL, 10);
	REQUIRE(ms >= 10000);
}

TEST_CASE("two streams are told to come back at different moments", "[sse]")
{
	/* Every tab watching a box that restarted is disconnected at the same
	   moment, so one number for all of them brings them all back at the same
	   moment too, however large it is. */
	REQUIRE(a.ok());
	REQUIRE(b.ok());
	REQUIRE(a.readFrame(2000) != b.readFrame(2000));
}

TEST_CASE("a comment frame arrives without waiting for anything to happen", "[sse]")
{
	REQUIRE(s.ok());
	REQUIRE(s.readFrame(2000).find("retry:") != std::string::npos);

	const std::string beat = s.readFrame(2000);
	INFO(beat);
	REQUIRE(beat == ":\n\n");
}

TEST_CASE("an event wakes the stream itself and not the next beat", "[sse]")
{
	/* Driven with a beat five seconds away and read for two, so that what
	REQUIRE(s.ok());
	REQUIRE(s.readFrame(2000).find("retry:") != std::string::npos);

	const std::string frame = s.readFrame(2000);
	INFO(frame);
	REQUIRE(frame.find("event: volume") != std::string::npos);
}

TEST_CASE("over the ceiling the answer is 503 and not a silent queue", "[sse]")
{
}

TEST_CASE("a ceiling of none means no stream is opened at all", "[sse]")
{
	REQUIRE(s.ok());
}

TEST_CASE("a head of the stream is answered without opening one", "[sse]")
{
	REQUIRE(s.ok());
	REQUIRE(s.code() == 200);
	REQUIRE(s.headerOf("Content-Type") == "text/event-stream");
	REQUIRE(events::openStreams() == 0);
}

namespace
{

struct LockWatch
{
	std::atomic<bool> go;
	std::atomic<bool> asking;
	std::atomic<bool> answered;
	std::atomic<bool> reached;
	std::atomic<bool> door;
	std::atomic<bool> held;
	pthread_t         asker;
	bool              started;

	LockWatch()
		: go(false), asking(false), answered(false), reached(false), door(false),
		  held(false), started(false)
	{
	}

	void start()
	{
		started = pthread_create(&asker, NULL, &ask, this) == 0;
	}

	void join()
	{
		if (!started)
			return;
		go = true;                 // in case the reaper never reached the point
		pthread_join(asker, NULL);
		started = false;
	}

	static void probe()
	{
		LockWatch *w = watched;
		if (w == NULL || w->reached.exchange(true))
			return;                // once, however many streams are reaped

		w->go = true;

		for (int waited = 0; waited < 5000 && !w->asking; waited += 2)
			napMs(2);
		if (!w->asking)
			return;
		w->door = true;

		napMs(200);
		w->held = !w->answered;
	}

	static LockWatch *watched;

private:
	LockWatch(const LockWatch &);
	LockWatch &operator=(const LockWatch &);

	static void *ask(void *cls)
	{
		LockWatch *w = (LockWatch *) cls;
		while (!w->go)
			napMs(2);
		w->asking = true;
		events::openStreams();
		w->answered = true;
		return NULL;
	}
};

LockWatch *LockWatch::watched = NULL;

} // namespace

TEST_CASE("a closed stream is not counted and its buffer is gone", "[sse]")
{
	const size_t before = events::openStreams();
	REQUIRE(before == 0);
	{
		REQUIRE(s.ok());
		REQUIRE(events::openStreams() == before + 1);
	}
	REQUIRE(waitForStreams(before, 3000) == before);
	REQUIRE(events::openStreams() == before);

	// And the place it held is free again, which is the half of it that a
	// count alone would not show.
	REQUIRE(again.ok());
	REQUIRE(events::openStreams() == 1);
}

TEST_CASE("a stream whose peer went away gives its place back at once", "[sse]")
{
		REQUIRE(s.ok());
		REQUIRE(s.code() == 200);
		REQUIRE(events::openStreams() == 1);
	}

	REQUIRE(waitForStreams(0, 3000) == 0);

	REQUIRE(again.ok());
	REQUIRE(again.code() == 200);
	REQUIRE(events::openStreams() == 1);
}

TEST_CASE("publishing with no stream open costs nothing and does not crash", "[sse]")
{
	events::stop();
	publish(coreapi::EventType::Standby, 0, 1);
	REQUIRE(events::openStreams() == 0);
	events::start();
	publish(coreapi::EventType::Standby, 0, 0);
	REQUIRE(events::openStreams() == 0);
}

TEST_CASE("events published while a stream is being set up are not lost", "[sse]")
{
	   head is a client nothing can be lost from. */
	REQUIRE(s.ok());

	for (int i = 0; i < 5; ++i)
		publish(coreapi::EventType::Volume, 0, i);

	std::vector<std::string> wanted;
	for (int i = 0; i < 5; ++i)
	{
		char want[32];
		std::snprintf(want, sizeof(want), "\"value\":%d", i);
		wanted.push_back(want);
	}

	const std::string all = s.readUntilAll(wanted, 3000);
	for (size_t i = 0; i < wanted.size(); ++i)
	{
		INFO(wanted[i] << " in " << all);
		REQUIRE(all.find(wanted[i]) != std::string::npos);
	}
}

TEST_CASE("a value a caller could not have written cannot break a frame", "[sse]")
{
	REQUIRE(s.ok());
	REQUIRE(s.readFrame(2000).find("retry:") != std::string::npos);

	coreapi::Event e;
	e.type = coreapi::EventType::Mode;
	e.text = "one\n\ntwo";
	coreapi::EventBus::instance().publish(e);

	const std::string frame = s.readFrame(2000);
	INFO(frame);
	REQUIRE(frame.find("event: mode") != std::string::npos);
	REQUIRE(frame.find("one\\n\\ntwo") != std::string::npos);
	REQUIRE(frame.size() > 3);
	REQUIRE(frame.compare(frame.size() - 3, 3, "}\n\n") == 0);

	// And what follows is not the rest of the value read as a frame of its own.
	const std::string next = s.readFrame(2000);
	INFO(next);
	REQUIRE(next.find("two") == std::string::npos);
}

TEST_CASE("a stream that stops reading is dropped rather than buffered forever", "[sse]")
{
	// A NAT box that vanishes leaves a socket that never reports an error, and
	// a per-stream buffer with no ceiling is a memory leak with a network
	REQUIRE(s.ok());

	for (int i = 0; i < 100000 && events::openStreams() > 0; ++i)
		publish(coreapi::EventType::Volume, 0, i);

	REQUIRE(waitForStreams(0, 5000) == 0);
	REQUIRE(events::openStreams() == 0);
}

TEST_CASE("every event type reaches a stream under its own name", "[sse]")
{
	/* The names are walked elsewhere; this is the wire. A beat five seconds
	   away, so that what comes back between two publishes is the event and
	   never a comment. */
	REQUIRE(s.ok());
	REQUIRE(s.readFrame(2000).find("retry:") != std::string::npos);

	static const coreapi::EventType kAll[] = {
		coreapi::EventType::Zap,
		coreapi::EventType::Mode,
		coreapi::EventType::Standby,
		coreapi::EventType::Volume,
		coreapi::EventType::Mute,
		coreapi::EventType::RecordStart,
		coreapi::EventType::RecordStop,
		coreapi::EventType::TimerChanged,
		coreapi::EventType::EpgUpdated,
		coreapi::EventType::BouquetsChanged,
		coreapi::EventType::SettingsChanged
	};
	const size_t count = sizeof(kAll) / sizeof(kAll[0]);
	REQUIRE(count == 11);

	for (size_t i = 0; i < count; ++i)
	{
		coreapi::Event e;
		e.type = kAll[i];
		e.channel_id = 0xFFFFFFFFFFFFFFFFull;
		e.value = (int) i;
		e.text = "a\"b\\c\td";
		coreapi::EventBus::instance().publish(e);

		const std::string frame = s.readFrame(2000);
		INFO(events::typeName(kAll[i]) << " frame: " << frame);
		REQUIRE(frame.find(std::string("event: ") + events::typeName(kAll[i]) + "\n")
		        != std::string::npos);
		REQUIRE(frame.find("\"channel_id\":\"ffffffffffffffff\"") != std::string::npos);
		REQUIRE(frame.find("a\\\"b\\\\c\\td") != std::string::npos);
		char want[32];
		std::snprintf(want, sizeof(want), "\"value\":%d", (int) i);
		REQUIRE(frame.find(want) != std::string::npos);
		REQUIRE(frame.size() > 3);
		REQUIRE(frame.compare(frame.size() - 3, 3, "}\n\n") == 0);
	}
}

TEST_CASE("a stream that is carrying events is not sent comments as well", "[sse]")
{
	REQUIRE(s.ok());
	REQUIRE(s.readFrame(2000).find("retry:") != std::string::npos);

	size_t comments = 0;
	size_t frames = 0;
	{
		publish(coreapi::EventType::Volume, 0, 1);
		napMs(20);
		for (;;)
		{
			const std::string f = s.readFrame(1);
			if (f.empty())
				break;
			++frames;
			if (f == ":\n\n")
				++comments;
		}
	}
	{
		const std::string f = s.readFrame(100);
		if (f.empty())
			break;
		++frames;
		if (f == ":\n\n")
			++comments;
	}

	INFO("frames " << frames << " of which comments " << comments);
	REQUIRE(frames > 20);
	REQUIRE(comments == 0);
}

TEST_CASE("a wedged stream's descriptor is named while the lock is held", "[sse]")
{
	LockWatch watch;
	LockWatch::watched = &watch;
	watch.start();
	REQUIRE(watch.started);
	events::setReapProbeForTest(&LockWatch::probe);

	struct Keep
	{
		Keep() : reader(NULL) {}
		~Keep() { delete reader; }
	private:
		Keep(const Keep &);
		Keep &operator=(const Keep &);
	} keep;

	bool wedged = false;
	size_t held_back = 0;
	{
		/* Above anything published below, so that the peer is wedged rather
		events::setPendingCeiling(64u * 1024u * 1024u);

		REQUIRE(keep.reader->ok());

		for (int round = 0; round < 12 && !wedged; ++round)
		{
			for (int i = 0; i < 50000; ++i)
				publish(coreapi::EventType::Volume, 0, i);

			held_back = events::pendingBytes();
			napMs(250);
			wedged = held_back > 0 && events::pendingBytes() == held_back;
		}

		INFO("held back " << held_back << " bytes");
		REQUIRE(wedged);
		REQUIRE(events::openStreams() == 1);

		// Leaving here stops the server, which takes every stream down, and a
		// stream in that state is taken down by its descriptor.
	}

	events::setReapProbeForTest(NULL);
	watch.join();
	LockWatch::watched = NULL;

	REQUIRE(watch.reached);      // the reaper named a descriptor
	REQUIRE(watch.door);         // somebody was there to be kept out
	REQUIRE(watch.held);         // and was kept out
	REQUIRE(events::openStreams() == 0);
}

TEST_CASE("a stream still open when the server stops is closed with it", "[sse]")
{
	{
		REQUIRE(s.ok());
		REQUIRE(events::openStreams() == 1);
	}
	REQUIRE(events::openStreams() == 0);
}

TEST_CASE("a server started a second time is subscribed a second time", "[sse]")
{
	{
		REQUIRE(s.ok());
	}

	REQUIRE(s.ok());
	REQUIRE(s.readFrame(2000).find("retry:") != std::string::npos);

	publish(coreapi::EventType::Volume, 0, 7);

	const std::string one = s.readFrame(2000);
	INFO(one);
	REQUIRE(one.find("\"value\":7") != std::string::npos);
}
