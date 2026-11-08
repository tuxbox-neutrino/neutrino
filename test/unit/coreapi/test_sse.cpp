/*
 * test_sse.cpp - tests for the event stream
 *
 * Copyright (C) 2026 NI-Team
 * Copyright (C) 2026 Tuxbox-Project (fixes, tests)
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
#include "support/wireserver.h"

#include "httpd/carry.h"
#include "httpd/endpoint.h"
#include "httpd/events.h"
#include "httpd/http.h"
#include "httpd/nhttpd_bridge.h"
#include "httpd/router.h"
#include "httpd/webconfig.h"

#include "yconnection.h"
#include "yhook.h"
#include "ysocket.h"
#include "ywebserver.h"

#include "coreapi/base/eventbus.h"

#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <atomic>
#include <set>
#include <string>
#include <vector>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <poll.h>
#include <pthread.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>

using namespace httpd;

TEST_CASE("every event type has a name and no two share one", "[sse]")
{
	std::set<std::string> seen;
	for (int i = 0; i <= (int) coreapi::EventType::Playback; ++i)
	{
		const char *n = events::typeName((coreapi::EventType) i);
		INFO("type value " << i);
		REQUIRE(n != NULL);
		REQUIRE(std::string(n).size() > 0);
		REQUIRE(seen.insert(n).second);
	}
	REQUIRE(seen.size() == 12);
	REQUIRE(std::string(events::typeName(coreapi::EventType::Playback)) == "playback");
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

/* ==== The wire, on nhttpd (support/wireserver.h): request reader, stream
   pass, header without a length, uncork, pump. ==== */

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

/* Waits until the count of open streams is the wanted one, or the budget is
   spent. Polled, because giving a stream back is the pump's own last act. */
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

} // namespace

TEST_CASE("a published event reaches a connected stream", "[sse]")
{
	WireServer serving;
	WireStream s(serving.port);
	REQUIRE(s.ok());
	REQUIRE(s.code() == 200);
	REQUIRE(s.headerOf("Content-Type") == "text/event-stream");
	REQUIRE(s.headerOf("Cache-Control") == "no-cache, no-transform");
	REQUIRE(s.headerOf("X-Accel-Buffering") == "no");
	/* The framing this tree runs on: HTTP/1.0, the body delimited by the
	   close. The source pins Transfer-Encoding: chunked here; on nhttpd there
	   is no chunking and no length, and both absences are the contract. */
	REQUIRE_FALSE(s.hasHeader("Transfer-Encoding"));
	REQUIRE_FALSE(s.hasHeader("Content-Length"));

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
	WireServer serving;
	WireStream s(serving.port);
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
	WireServer serving;
	WireStream a(serving.port);
	WireStream b(serving.port);
	REQUIRE(a.ok());
	REQUIRE(b.ok());
	REQUIRE(a.readFrame(2000) != b.readFrame(2000));
}

TEST_CASE("a comment frame arrives without waiting for anything to happen", "[sse]")
{
	/* A proxy or a NAT box drops a stream that says nothing for between
	   thirty and sixty seconds. Driven at a tenth of a second, because a
	   case that waited out the shipped interval would add twenty seconds to
	   every run. */
	WireServer serving(4, 100);
	WireStream s(serving.port);
	REQUIRE(s.ok());
	REQUIRE(s.readFrame(2000).find("retry:") != std::string::npos);

	const std::string beat = s.readFrame(2000);
	INFO(beat);
	REQUIRE(beat == ":\n\n");
}

TEST_CASE("an event wakes the stream itself and not the next beat", "[sse]")
{
	/* Driven with a beat five seconds away and read for two, so that what
	   carries the event is the delivery and cannot be the thread that beats. */
	WireServer serving(4, 5000);
	WireStream s(serving.port);
	REQUIRE(s.ok());
	REQUIRE(s.readFrame(2000).find("retry:") != std::string::npos);

	const int64_t asked = wireNowMs();
	publish(coreapi::EventType::Volume, 0, 42);
	const std::string frame = s.readFrame(2000);
	const int64_t took = wireNowMs() - asked;
	INFO(frame << " took " << took << "ms");
	REQUIRE(frame.find("event: volume") != std::string::npos);
	/* Faster than the pump's own one-second belt, so what carried it was
	   the broadcast and cannot have been the timeout. */
	REQUIRE(took < 800);
}

TEST_CASE("over the ceiling the answer is 503 and not a silent queue", "[sse]")
{
	WireServer serving(1);
	WireStream first(serving.port);
	REQUIRE(first.ok());
	REQUIRE(first.readFrame(2000).find("retry:") != std::string::npos);

	WireStream second(serving.port);
	REQUIRE(second.ok());
	REQUIRE(second.code() == 503);
	REQUIRE(second.headerOf("Content-Type") == "application/problem+json");
	REQUIRE_FALSE(second.headerOf("Retry-After").empty());
	// a finite answer, with its length, and then the close
	REQUIRE_FALSE(second.headerOf("Content-Length").empty());
	const std::string all = second.readToEof(2000);
	REQUIRE(all.find("\"status\":503") != std::string::npos);
}

TEST_CASE("the configured ceiling is the one that holds, with nothing cut below it", "[sse]")
{
	/* A stream holds a thread of the transport's and none of nhttpd's
	   connection threads, so the setting is taken as it stands. Twenty is
	   more than a third of the fifty connection threads and more than any
	   kind of the transport has seats: neither may cut the setting, and the
	   twenty-first is refused by the module, with its own words and its own
	   wait. Without connection threads, as boxes ship, so all twenty are
	   open beside an accept thread that is free. */
	WireServer serving(20, 5000, WireServer::Serial);
	std::vector<WireStream *> open;
	for (int i = 0; i < 20; i++) {
		open.push_back(new WireStream(serving.port));
		REQUIRE(open.back()->ok());
		INFO("stream " << i);
		REQUIRE(open.back()->code() == 200);
	}
	CHECK(events::openStreams() == 20);
	WireStream one_more(serving.port);
	REQUIRE(one_more.ok());
	CHECK(one_more.code() == 503);
	CHECK(one_more.headerOf("Retry-After") == "30");
	CHECK(one_more.readToEof(2000).find(events::streamsFullDetail()) != std::string::npos);
	for (size_t i = 0; i < open.size(); i++)
		delete open[i];
}

TEST_CASE("a ceiling of none means no stream is opened at all", "[sse]")
{
	WireServer serving(0);
	WireStream s(serving.port);
	REQUIRE(s.ok());
	REQUIRE(s.code() == 503);
}

TEST_CASE("a head of the stream is answered without opening one", "[sse]")
{
	WireServer serving;
	WireStream s(serving.port, "HEAD /api/v1/events HTTP/1.0\r\nHost: box\r\n\r\n");
	REQUIRE(s.ok());
	REQUIRE(s.code() == 200);
	REQUIRE(s.headerOf("Content-Type") == "text/event-stream");
	// no length: the GET it stands for has none (RFC 9110 8.6)
	REQUIRE(s.headerOf("Content-Length").empty());
	// no body, and no seat taken
	REQUIRE(s.readToEof(2000).empty());
	REQUIRE(events::openStreams() == 0);
}

/* ==== Without connection threads (webserver.threading=false), which is how
   boxes ship nhttpd.conf: the accept thread reads every request itself, and
   a stream runs on a thread of the transport's all the same. ==== */

TEST_CASE("without connection threads a stream opens with its head and carries an event", "[sse]")
{
	WireServer serving(4, 5000, WireServer::Serial);
	WireStream s(serving.port);
	REQUIRE(s.ok());
	REQUIRE(s.code() == 200);
	// the head a stream has with connection threads, line for line
	CHECK(s.headerOf("Content-Type") == "text/event-stream");
	CHECK(s.headerOf("Cache-Control") == "no-cache, no-transform");
	CHECK(s.headerOf("X-Accel-Buffering") == "no");
	CHECK(s.headerOf("X-Content-Type-Options") == "nosniff");
	CHECK(s.headerOf("Connection") == "close");
	CHECK_FALSE(s.hasHeader("Content-Length"));
	CHECK_FALSE(s.hasHeader("Transfer-Encoding"));
	REQUIRE(s.readFrame(2000).find("retry:") != std::string::npos);

	publish(coreapi::EventType::Volume, 0, 42);
	const std::string frame = s.readFrame(2000);
	INFO(frame);
	REQUIRE(frame.find("event: volume") != std::string::npos);
	REQUIRE(frame.find("\"value\":42") != std::string::npos);
}

TEST_CASE("without connection threads the next request is answered while streams are open", "[sse]")
{
	WireServer serving(4, 5000, WireServer::Serial);
	WireStream a(serving.port);
	WireStream b(serving.port);
	WireStream c(serving.port);
	REQUIRE(a.ok());
	REQUIRE(b.ok());
	REQUIRE(c.ok());
	REQUIRE(a.code() == 200);
	REQUIRE(b.code() == 200);
	REQUIRE(c.code() == 200);
	REQUIRE(events::openStreams() == 3);

	const int64_t asked = wireNowMs();
	WireStream next(serving.port, "GET /api/v1/session HTTP/1.0\r\nHost: box\r\n\r\n");
	REQUIRE(next.ok());
	CHECK(next.code() == 200);
	CHECK_FALSE(next.readToEof(2000).empty());
	/* Generous for a loaded host: an accept thread a stream held would leave
	   this request without a head, which ok() above already fails. */
	CHECK(wireNowMs() - asked <= 2000);

	// and the three still carry what happens
	REQUIRE(a.readFrame(2000).find("retry:") != std::string::npos);
	REQUIRE(c.readFrame(2000).find("retry:") != std::string::npos);
	publish(coreapi::EventType::Mute, 0, 1);
	CHECK(a.readFrame(2000).find("event: mute") != std::string::npos);
	CHECK(c.readFrame(2000).find("event: mute") != std::string::npos);
}

TEST_CASE("without connection threads the ceiling refuses with a wait and a closed stream frees its place at once", "[sse]")
{
	/* The beat is a minute away, so the place cannot come back because
	   something was written down the closed stream: the module found the
	   peer gone. */
	WireServer serving(1, 60000, WireServer::Serial);
	{
		WireStream s(serving.port);
		REQUIRE(s.ok());
		REQUIRE(s.code() == 200);
		REQUIRE(events::openStreams() == 1);

		WireStream over(serving.port);
		REQUIRE(over.ok());
		CHECK(over.code() == 503);
		CHECK(over.headerOf("Content-Type") == "application/problem+json");
		CHECK(over.headerOf("Retry-After") == "30");
		CHECK_FALSE(over.headerOf("Content-Length").empty());
		CHECK(over.readToEof(2000).find("too-many-streams") != std::string::npos);

		// a HEAD is the stream's head whether a place is free or not, and takes none
		WireStream head(serving.port, "HEAD /api/v1/events HTTP/1.0\r\nHost: box\r\n\r\n");
		REQUIRE(head.ok());
		CHECK(head.code() == 200);
		CHECK(head.headerOf("Content-Type") == "text/event-stream");
		CHECK(head.readToEof(2000).empty());
		CHECK(events::openStreams() == 1);
	}

	const int64_t closed = wireNowMs();
	REQUIRE(waitForStreams(0, 3000) == 0);
	CHECK(wireNowMs() - closed < 1000);

	WireStream again(serving.port);
	REQUIRE(again.ok());
	CHECK(again.code() == 200);
	CHECK(events::openStreams() == 1);
}

namespace
{

/* The end of a connection this process serves, found by the address of its
   client: the stream's own socket, on which its options are set. Asked once
   nhttpd has let the connection go, so the job's copy is the only one. */
int servedEndOf(const WireStream &s)
{
	struct sockaddr_in client;
	socklen_t len = sizeof(client);
	if (getsockname(s.fd, (struct sockaddr *) &client, &len) != 0)
		return -1;
	for (int fd = 3; fd < 4096; ++fd)
	{
		if (fd == s.fd)
			continue;
		struct sockaddr_in peer;
		socklen_t plen = sizeof(peer);
		if (getpeername(fd, (struct sockaddr *) &peer, &plen) != 0)
			continue;
		if (peer.sin_family == AF_INET && peer.sin_port == client.sin_port
				&& peer.sin_addr.s_addr == client.sin_addr.s_addr)
			return fd;
	}
	return -1;
}

} // namespace

TEST_CASE("a stream's socket gives up on a peer that acknowledges nothing for a minute", "[sse]")
{
	/* A peer that vanished acknowledges nothing, and a beat is too little to
	   fill a send buffer: without the limit the place stays taken for the
	   quarter of an hour the retransmissions take. */
#ifdef TCP_USER_TIMEOUT
	WireServer serving(4, 5000, WireServer::Serial);
	WireStream s(serving.port);
	REQUIRE(s.ok());
	REQUIRE(s.code() == 200);
	REQUIRE(s.readFrame(2000).find("retry:") != std::string::npos);
	for (int waited = 0; waited < 2000 && serving.served.load() < 1u; waited += 10)
		napMs(10);
	REQUIRE(serving.served.load() >= 1u);

	const int served = servedEndOf(s);
	REQUIRE(served >= 0);
	unsigned int ms = 0;
	socklen_t len = sizeof(ms);
	REQUIRE(getsockopt(served, IPPROTO_TCP, TCP_USER_TIMEOUT, &ms, &len) == 0);
	CHECK(ms == 60000u);
#else
	WARN("this build has no TCP_USER_TIMEOUT; a vanished peer keeps its place longer");
	SUCCEED();
#endif
}

namespace
{

std::atomic<int> writes_seen(0);

// Fails the first write it is handed, which is a stream's head, and passes
// every later one to the socket.
ssize_t failFirstWrite(int fd, const char *bytes, size_t n)
{
	if (++writes_seen == 1)
	{
		errno = EPIPE;
		return -1;
	}
	return ::send(fd, bytes, n, MSG_NOSIGNAL);
}

} // namespace

TEST_CASE("a stream whose head could not be written gives its place back", "[sse]")
{
	/* A peer that resets right after its request: the head fails, and the
	   place registered before it has to come back, or as many such peers as
	   there are places leave the box refusing every stream. The writes after
	   the first go out again, so a stream that pumped on past a failed head
	   would hold its place and be seen here. */
	struct Writer
	{
		Writer()
		{
			writes_seen = 0;
			carry::setWriterForTest(&failFirstWrite);
		}
		~Writer() { carry::setWriterForTest(NULL); }
	} writer;
	WireServer serving(1, 60000, WireServer::Serial);
	{
		WireStream s(serving.port);
		CHECK_FALSE(s.ok());
		CHECK(s.eof);
	}
	REQUIRE(waitForStreams(0, 3000) == 0);

	carry::setWriterForTest(NULL);
	WireStream again(serving.port);
	REQUIRE(again.ok());
	CHECK(again.code() == 200);
	CHECK(events::openStreams() == 1);
}

TEST_CASE("a server that is stopping refuses a stream until it starts again", "[sse]")
{
	/* Between a stop and the next start, as a reload has it: the stream is
	   refused by the transport like every carried answer, before the module
	   is asked, and comes back with the server. */
	WireServer serving;
	CApiBridge::stopStreams();
	{
		WireStream refused(serving.port);
		REQUIRE(refused.ok());
		CHECK(refused.code() == 503);
		CHECK(refused.readToEof(2000).find("server-stopping") != std::string::npos);
	}
	CHECK(events::openStreams() == 0);

	CApiBridge::startStreams();
	WireStream again(serving.port);
	REQUIRE(again.ok());
	CHECK(again.code() == 200);
	CHECK(again.readFrame(2000).find("retry:") != std::string::npos);
}

namespace
{

/* Whether the reaper names a wedged stream's descriptor while this module's
   lock is held. Ported as written; what it watches is the same lock-window
   property, the pump's close standing in for the library's. */
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
	WireServer serving(2);
	const size_t before = events::openStreams();
	REQUIRE(before == 0);
	{
		WireStream s(serving.port);
		REQUIRE(s.ok());
		REQUIRE(events::openStreams() == before + 1);
	}
	REQUIRE(waitForStreams(before, 3000) == before);
	REQUIRE(events::openStreams() == before);

	// And the place it held is free again, which is the half of it that a
	// count alone would not show.
	WireStream again(serving.port);
	REQUIRE(again.ok());
	REQUIRE(events::openStreams() == 1);
}

TEST_CASE("a stream whose peer went away gives its place back at once", "[sse]")
{
	/* The beat here is a minute, far longer than this case waits, so a place
	   that comes back inside it cannot have come back because anything was
	   written down the stream: the tending thread's poll found the peer. */
	WireServer serving(1, 60000);
	{
		WireStream s(serving.port);
		REQUIRE(s.ok());
		REQUIRE(s.code() == 200);
		REQUIRE(events::openStreams() == 1);
	}

	REQUIRE(waitForStreams(0, 3000) == 0);

	WireStream again(serving.port);
	REQUIRE(again.ok());
	REQUIRE(again.code() == 200);
	REQUIRE(events::openStreams() == 1);
}

TEST_CASE("publishing with no stream open costs nothing and does not crash", "[sse]")
{
	WireServer serving;
	events::stop();
	publish(coreapi::EventType::Standby, 0, 1);
	REQUIRE(events::openStreams() == 0);
	events::start();
	publish(coreapi::EventType::Standby, 0, 0);
	REQUIRE(events::openStreams() == 0);
}

TEST_CASE("events published while a stream is being set up are not lost", "[sse]")
{
	/* The server half of the no-loss contract: the head of the answer is
	   seeded when the stream is registered, so a client that has read the
	   head is a client nothing can be lost from. */
	WireServer serving;
	WireStream s(serving.port);
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
	WireServer serving;
	WireStream s(serving.port);
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
	// trigger. Nothing reads this stream after the open; the window is small.
	WireServer serving;
	WireStream s(serving.port,
			"GET /api/v1/events HTTP/1.0\r\nHost: box\r\n\r\n", 4096);
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
	WireServer serving(4, 5000);
	WireStream s(serving.port);
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
		coreapi::EventType::SettingsChanged,
		coreapi::EventType::Playback
	};
	const size_t count = sizeof(kAll) / sizeof(kAll[0]);
	REQUIRE(count == 12);

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
	/* The interval is how long ONE stream may be silent. Four intervals pass
	   here, and events arrive twenty times faster than that. */
	WireServer serving(4, 400);
	WireStream s(serving.port);
	REQUIRE(s.ok());
	REQUIRE(s.readFrame(2000).find("retry:") != std::string::npos);

	size_t comments = 0;
	size_t frames = 0;
	const int64_t until = wireNowMs() + 1600;
	while (wireNowMs() < until)
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
	const int64_t drained = wireNowMs() + 600;
	while (wireNowMs() < drained)
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
	/* Ported as written: the descriptor belongs to the connection thread,
	   which closes it only after the pump gave the stream back, and that
	   waits on the module's lock. Named under the lock, it cannot have been
	   reissued; named outside it, it can. The probe watches the door. */
	LockWatch watch;
	/* Everything the case installs is taken back by unwinding, in this
	   order: the server scope inside goes first (the reap may still fire
	   the probe, the watch is alive), then this guard clears the probe,
	   releases and joins the asker and clears the global, then the watch
	   itself goes. A failing REQUIRE inside the scope leaves nothing
	   dangling. */
	struct ProbeGuard
	{
		LockWatch *w;
		explicit ProbeGuard(LockWatch *x) : w(x) {}
		~ProbeGuard()
		{
			events::setReapProbeForTest(NULL);
			w->join();
			LockWatch::watched = NULL;
		}
	private:
		ProbeGuard(const ProbeGuard &);
		ProbeGuard &operator=(const ProbeGuard &);
	} probe_guard(&watch);
	LockWatch::watched = &watch;
	watch.start();
	REQUIRE(watch.started);
	events::setReapProbeForTest(&LockWatch::probe);

	struct Keep
	{
		WireStream *reader;
		Keep() : reader(NULL) {}
		~Keep() { delete reader; }
	private:
		Keep(const Keep &);
		Keep &operator=(const Keep &);
	} keep;

	bool wedged = false;
	size_t held_back = 0;
	{
		WireServer serving(4, 5000);
		/* Above anything published below, so that the peer is wedged rather
		   than dropped: what is wanted is the state where the pump is blocked
		   in a send the peer is not taking. */
		events::setPendingCeiling(64u * 1024u * 1024u);

		keep.reader = new WireStream(serving.port,
				"GET /api/v1/events HTTP/1.0\r\nHost: box\r\n\r\n", 4096);
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

		/* This module's own stop, which takes every stream down, and a stream
		   in that state by its descriptor. Asked before the server's: that one
		   has the transport shut every carried descriptor down first, which
		   ends the pump before this module's reap could reach it. */
		events::stop();
	}

	events::setReapProbeForTest(NULL);
	watch.join();

	REQUIRE(watch.reached);      // the reaper named a descriptor
	REQUIRE(watch.door);         // somebody was there to be kept out
	REQUIRE(watch.held);         // and was kept out
	REQUIRE(events::openStreams() == 0);
}

TEST_CASE("a stream still open when the server stops is closed with it", "[sse]")
{
	{
		WireServer serving;
		WireStream s(serving.port);
		REQUIRE(s.ok());
		REQUIRE(events::openStreams() == 1);
	}
	REQUIRE(events::openStreams() == 0);
}

TEST_CASE("a server started a second time is subscribed a second time", "[sse]")
{
	{
		WireServer first;
		WireStream s(first.port);
		REQUIRE(s.ok());
	}

	WireServer second;
	WireStream s(second.port);
	REQUIRE(s.ok());
	REQUIRE(s.readFrame(2000).find("retry:") != std::string::npos);

	publish(coreapi::EventType::Volume, 0, 7);

	const std::string one = s.readFrame(2000);
	INFO(one);
	REQUIRE(one.find("\"value\":7") != std::string::npos);
}

namespace
{

struct DrawRecorder
{
	std::vector<size_t> sizes;

	static bool take(void *ctx, const char *, size_t n)
	{
		DrawRecorder *r = (DrawRecorder *) ctx;
		r->sizes.push_back(n);
		// enough turns to see the bound, then end the stream
		return r->sizes.size() < 12;
	}
};

} // namespace

TEST_CASE("the pump draws at most one block per turn", "[sse]")
{
	/* The bound is what keeps pendingBytes() meaning something while a send
	   blocks on a peer that is not reading; the wedged case depends on it
	   without measuring it. Measured here at the pump itself. */
	WebConfig c = defaultWebConfig();
	c.sse_max_streams = 2;
	setConfigForTest(c);
	events::start();

	int sp[2];
	REQUIRE(socketpair(AF_UNIX, SOCK_STREAM, 0, sp) == 0);
	events::StreamHandle h = NULL;
	Response refusal;
	REQUIRE(events::openStream(sp[0], h, refusal) == events::StreamOpened);

	// far more than one block pending before the first draw
	for (int i = 0; i < 400; ++i)
		publish(coreapi::EventType::Volume, 0, i);
	REQUIRE(events::pendingBytes() > 3 * 4096);

	DrawRecorder rec;
	events::pumpStream(h, &DrawRecorder::take, &rec);

	REQUIRE(rec.sizes.size() >= 3);
	REQUIRE(rec.sizes[0] == 4096);
	for (size_t i = 0; i < rec.sizes.size(); ++i)
	{
		INFO("draw " << i << " = " << rec.sizes[i]);
		REQUIRE(rec.sizes[i] <= 4096);
	}
	REQUIRE(events::openStreams() == 0);

	events::stop();
	setConfigForTest(defaultWebConfig());
	events::setHeartbeatMs(kShippedHeartbeatMs);
	events::setPendingCeiling(kShippedCeiling);
	close(sp[0]);
	close(sp[1]);
}
