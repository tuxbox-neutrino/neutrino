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

/* ==== The wire, on nhttpd (drift: replaces the source's MHD fixture) ====

   The fixture below is the production path minus the accept loop of
   CWebserver::run(): a real listening socket, CySocket::accept() (which is
   what puts the cork on), one thread per connection running the same wiring
   WebThread does - CWebserverConnection::HandleConnection() end to end -
   with the bridge attached. What it proves is the whole new chain: request
   reader, stream pass, header without a length, uncork, pump. */

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

int64_t wireNowMs()
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

struct ServeArgs
{
	CWebserver *ws;
	CySocket   *c;
};

void *serveOne(void *arg)
{
	ServeArgs *a = (ServeArgs *) arg;
	CWebserverConnection *con = new CWebserverConnection(a->ws);
	con->Request.UrlData["clientaddr"] = a->c->get_client_ip();
	con->sock = a->c;
	a->c->handling = true;
	con->HandleConnection();
	if (!con->keep_alive)
		con->sock->isValid = false;
	a->c->handling = false;
	delete a->c;
	delete con;
	delete a;
	return NULL;
}

struct WireServer
{
	CApiBridge  bridge;
	CWebserver  ws;
	CySocket   *listener;
	int         port;
	pthread_t   acceptor;
	pthread_mutex_t          workers_lock;
	std::vector<pthread_t>   workers;

	explicit WireServer(unsigned max_streams = 4, unsigned beat_ms = 100)
	{
		WebConfig c = defaultWebConfig();
		c.sse_max_streams = max_streams;
		setConfigForTest(c);

		CyhookHandler::clearApiPrefixes();
		CApiBridge::registerPrefixes();
		CyhookHandler::attach(&bridge);
		events::start();
		events::setHeartbeatMs(beat_ms);

		int lfd = socket(AF_INET, SOCK_STREAM, 0);
		REQUIRE(lfd >= 0);
		int one = 1;
		setsockopt(lfd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));
		struct sockaddr_in a;
		std::memset(&a, 0, sizeof(a));
		a.sin_family = AF_INET;
		a.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
		REQUIRE(bind(lfd, (struct sockaddr *) &a, sizeof(a)) == 0);
		socklen_t alen = sizeof(a);
		REQUIRE(getsockname(lfd, (struct sockaddr *) &a, &alen) == 0);
		port = (int) ntohs(a.sin_port);
		REQUIRE(listen(lfd, 8) == 0);

		listener = new CySocket(lfd);
		listener->isOpened = true;
		listener->isValid = true;

		pthread_mutex_init(&workers_lock, NULL);
		REQUIRE(pthread_create(&acceptor, NULL, &WireServer::acceptLoop, this) == 0);
	}

	static void *acceptLoop(void *arg)
	{
		WireServer *self = (WireServer *) arg;
		for (;;)
		{
			CySocket *c = self->listener->accept();
			if (c == NULL)
				return NULL;
			ServeArgs *sa = new ServeArgs;
			sa->ws = &self->ws;
			sa->c = c;
			pthread_t t;
			if (pthread_create(&t, NULL, &serveOne, sa) != 0)
			{
				delete sa->c;
				delete sa;
				continue;
			}
			pthread_mutex_lock(&self->workers_lock);
			self->workers.push_back(t);
			pthread_mutex_unlock(&self->workers_lock);
		}
	}

	~WireServer()
	{
		/* The order the shutdown contract states: close and wait out every
		   stream first, so no connection thread is inside the bridge or the
		   events module when they go; then the acceptor; then every worker
		   is joined, because the hooks and the config below must outlive
		   the last HandleConnection. */
		events::stop();
		::shutdown(listener->get_socket(), SHUT_RDWR);
		pthread_join(acceptor, NULL);
		for (;;)
		{
			pthread_t t;
			{
				pthread_mutex_lock(&workers_lock);
				if (workers.empty())
				{
					pthread_mutex_unlock(&workers_lock);
					break;
				}
				t = workers.back();
				workers.pop_back();
				pthread_mutex_unlock(&workers_lock);
			}
			pthread_join(t, NULL);
		}
		pthread_mutex_destroy(&workers_lock);
		listener->close();
		delete listener;
		CyhookHandler::detach(&bridge);
		CyhookHandler::clearApiPrefixes();
		setConfigForTest(defaultWebConfig());
		events::setHeartbeatMs(kShippedHeartbeatMs);
		events::setPendingCeiling(kShippedCeiling);
	}

private:
	WireServer(const WireServer &);
	WireServer &operator=(const WireServer &);
};

/* One client of the stream. HTTP/1.0 and close-delimited, so there is no
   framing to decode (the source's reader undoes MHD's chunking here): the header
   runs to the blank line and the body is bytes until the connection closes. */
struct WireStream
{
	int         fd;
	bool        opened;
	int         status;
	std::string header;
	std::string body;
	size_t      taken;

	explicit WireStream(int port, const char *request =
			"GET /api/v1/events HTTP/1.0\r\nHost: box\r\n\r\n",
			int rcvbuf = 0)
		: fd(-1), opened(false), status(0), taken(0)
	{
		fd = socket(AF_INET, SOCK_STREAM, 0);
		if (fd < 0)
			return;
		// a small window, where a case wants the far end to ask for little
		if (rcvbuf > 0)
			setsockopt(fd, SOL_SOCKET, SO_RCVBUF, &rcvbuf, sizeof(rcvbuf));
		struct sockaddr_in a;
		std::memset(&a, 0, sizeof(a));
		a.sin_family = AF_INET;
		a.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
		a.sin_port = htons((uint16_t) port);
		if (connect(fd, (struct sockaddr *) &a, sizeof(a)) != 0)
			return;
		const size_t n = std::strlen(request);
		if (send(fd, request, n, MSG_NOSIGNAL) != (ssize_t) n)
			return;

		// the header, up to the blank line
		std::string got;
		const int64_t deadline = wireNowMs() + 5000;
		for (;;)
		{
			const size_t at = got.find("\r\n\r\n");
			if (at != std::string::npos)
			{
				header = got.substr(0, at + 4);
				body = got.substr(at + 4);
				break;
			}
			if (!readSome(got, deadline))
				return;
		}
		status = (int) std::strtol(header.c_str() + 9, NULL, 10);
		opened = true;
	}

	~WireStream()
	{
		if (fd >= 0)
			close(fd);
	}

	bool ok() const { return opened; }
	int code() const { return status; }

	// case-insensitive, value up to the line's end
	std::string headerOf(const char *name) const
	{
		std::string want(name);
		want += ":";
		for (size_t at = 0; at < header.size();)
		{
			size_t end = header.find("\r\n", at);
			if (end == std::string::npos)
				break;
			if (header.size() - at > want.size()
					&& strncasecmp(header.c_str() + at, want.c_str(), want.size()) == 0)
			{
				std::string v = header.substr(at + want.size(), end - at - want.size());
				while (!v.empty() && v[0] == ' ')
					v.erase(0, 1);
				return v;
			}
			at = end + 2;
		}
		return "";
	}

	bool hasHeader(const char *name) const
	{
		return !headerOf(name).empty();
	}

	/* The next frame, terminator included, or "" at the deadline. A frame
	   ends at the blank line, which in this format is two newlines. */
	std::string readFrame(int wait_ms)
	{
		const int64_t deadline = wireNowMs() + wait_ms;
		for (;;)
		{
			const size_t at = body.find("\n\n", taken);
			if (at != std::string::npos)
			{
				std::string out = body.substr(taken, at + 2 - taken);
				taken = at + 2;
				return out;
			}
			if (!readSome(body, deadline))
				return "";
		}
	}

	// Frames until every wanted piece was seen, or the deadline; answers
	// everything read.
	std::string readUntilAll(const std::vector<std::string> &wanted, int wait_ms)
	{
		const int64_t deadline = wireNowMs() + wait_ms;
		for (;;)
		{
			bool all = true;
			for (size_t i = 0; all && i < wanted.size(); ++i)
				all = body.find(wanted[i]) != std::string::npos;
			if (all)
				return body;
			if (!readSome(body, deadline))
				return body;
		}
	}

	// Everything until the peer closes, within the deadline.
	std::string readToEof(int wait_ms)
	{
		const int64_t deadline = wireNowMs() + wait_ms;
		while (readSome(body, deadline))
		{
		}
		return body.substr(taken);
	}

private:
	bool readSome(std::string &into, int64_t deadline)
	{
		const int64_t left = deadline - wireNowMs();
		if (left <= 0)
			return false;
		struct pollfd p;
		p.fd = fd;
		p.events = POLLIN;
		p.revents = 0;
		if (poll(&p, 1, (int) left) <= 0)
			return false;
		char buf[4096];
		const ssize_t n = read(fd, buf, sizeof(buf));
		if (n <= 0)
			return false;
		into.append(buf, (size_t) n);
		return true;
	}

	WireStream(const WireStream &);
	WireStream &operator=(const WireStream &);
};

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

TEST_CASE("the configured ceiling is cut to what the connection threads can carry", "[sse]")
{
	// Each open stream holds one of nhttpd's 50 connection threads. The
	// configuration allows 64, for a server that spends no thread on a
	// stream; here a third of the threads is the most that goes to streams,
	// so ordinary requests always find one.
	WireServer serving(64);
	std::vector<WireStream *> open;
	for (int i = 0; i < 16; i++) {
		open.push_back(new WireStream(serving.port));
		REQUIRE(open.back()->ok());
		REQUIRE(open.back()->code() == 200);
	}
	WireStream one_more(serving.port);
	REQUIRE(one_more.ok());
	CHECK(one_more.code() == 503);
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
	REQUIRE(s.headerOf("Content-Length") == "0");
	// no body, and no seat taken
	REQUIRE(s.readToEof(2000).empty());
	REQUIRE(events::openStreams() == 0);
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

		// Leaving here stops the server, which takes every stream down, and a
		// stream in that state is taken down by its descriptor.
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
