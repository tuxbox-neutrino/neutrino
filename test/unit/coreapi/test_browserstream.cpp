/*
 * test_browserstream.cpp - tests for the answer that is a running program
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
#include "support/fakes.h"
#include "support/httpclient.h"

#include "httpd/livestream.h"
#include "httpd/router.h"
#include "httpd/server.h"

#include "coreapi/base/types.h"

#include <cstring>
#include <string>
#include <vector>

#include <stdint.h>
#include <unistd.h>

#include <netinet/in.h>
#include <sys/socket.h>

/* The half of playing in a browser that a pair of codec names cannot answer:
   the connection stays open after the handler has returned, the bytes come out
   of a program this server started, and the program has to be gone again when
   the answer is.

   WHAT THESE CASES ARE REALLY ABOUT is the two ways that goes wrong and both
   of which look like nothing at all. A reader that suspends a connection
   nobody wakes again is a request that never answers, which shows here as a
   case that hangs and nowhere else. A program nobody collects is a box that
   works perfectly for a week and then cannot start anything, which shows here
   as a count that does not come back to nothing.

   The converter is pointed at a port nothing answers on, so it ends by itself
   and the whole of the road is walked in a moment: started, read from, found
   to be finished, and collected. What the bytes would have been is decided by
   a pair of codec names and is driven where those are, on their own.

   AND THE PORT IS TAKEN AND NOT NAMED. It used to be written down as 31339 on
   the strength of nothing answering there, and 31339 is the port a running
   Neutrino streams its channels on: on a machine where the box is up, the
   converter reached the box, read a channel that never ends, and the case
   waited for a body with no end until its own client gave up. Green on one
   machine and red on the next, with nothing between them but whether the box
   happened to be running.

   DeadPort below takes a port off the kernel and holds it without listening on
   it, so a connect is refused at once and nothing else can move in while the
   case runs. */

using namespace httpd;

namespace
{

const uint64_t kTv = 0x1001;
const uint64_t kRadio = 0x2001;
const uint64_t kUnknown = 0x9999;

coreapi::ChannelInfo makeChannel(uint64_t id, const char *name, coreapi::ServiceKind kind)
{
	coreapi::ChannelInfo c;
	c.id = id;
	c.name = name;
	c.kind = kind;
	return c;
}

/* A port that is certain to refuse. Bound and never listened on, so the kernel
   answers a connect with a reset rather than a queue, and held for as long as
   this object lives so that nothing else can take it in between. */
struct DeadPort
{
	int fd;
	int port;

	DeadPort() : fd(-1), port(0)
	{
		fd = ::socket(AF_INET, SOCK_STREAM, 0);
		if (fd < 0)
			return;
		struct sockaddr_in a;
		std::memset(&a, 0, sizeof(a));
		a.sin_family = AF_INET;
		a.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
		a.sin_port = 0;
		if (::bind(fd, (struct sockaddr *) &a, sizeof(a)) != 0)
		{
			::close(fd);
			fd = -1;
			return;
		}
		socklen_t len = sizeof(a);
		if (::getsockname(fd, (struct sockaddr *) &a, &len) == 0)
			port = (int) ntohs(a.sin_port);
	}

	~DeadPort()
	{
		if (fd >= 0)
			::close(fd);
	}

	private:
		DeadPort(const DeadPort &);
		DeadPort &operator=(const DeadPort &);
};

struct Serving
{
	InstalledDependencies deps;
	FakeSettingsSource settings;
	InstalledSettingsSource in_settings;
	DeadPort dead;
	int port;

	Serving() : in_settings(&settings), port(0)
	{
		/* A port nothing on this machine can answer on, so that the converter
		   is refused at once and this suite never waits on a network. */
		REQUIRE(dead.port > 0);
		settings.ints["streaming_port"] = dead.port;

		deps.channels.channels.push_back(makeChannel(kTv, "Erstes", coreapi::ServiceKind::Tv));
		deps.channels.channels.push_back(makeChannel(kRadio, "Toene", coreapi::ServiceKind::Radio));

		setRoutesForTest(NULL);
		ServerConfig c = defaultConfig();
		c.port = 0;
		c.bind_address = "127.0.0.1";
		if (start(c))
			port = boundPort();
	}

	~Serving()
	{
		stop();
		setRoutesForTest(NULL);
	}

	private:
		Serving(const Serving &);
		Serving &operator=(const Serving &);
};

std::vector<std::pair<std::string, std::string> > hostHeader()
{
	std::vector<std::pair<std::string, std::string> > out;
	out.push_back(std::make_pair(std::string("Host"), std::string("box.example")));
	return out;
}

/* The library gives an answer's program back on a thread of its own, which is
   not always before the reply has been read out here. So the count is waited on
   rather than taken once, and what it was when the wait gave up is what a case
   reads. */
/* Whether this machine has the converter at all.

   A box that has none says so rather than handing out an answer with no bytes
   in it, and that is a different code, so a case that asserted one of the two
   would be a case that reports the machine it ran on. Both are asserted, and
   which one applies is decided here by asking the same question the server
   asks. */
bool hasConverter()
{
	return ::access("/usr/bin/ffmpeg", X_OK) == 0;
}

/* A stream whose program says nothing and does not end, which is the state a
   converter is in whenever its source has a dry moment. The reader suspends on
   the first empty read and the connection leaves the library's event loop,
   which is the state this suite exists to hold: while it lasts, nothing looks
   at the connection at all. */
Response silentStream(const Request &)
{
	Response out;
	out.code = StatusOk;
	out.content_type = "video/mp2t";
	out.stream_argv.push_back("/bin/sh");
	out.stream_argv.push_back("-c");
	/* exec, so that the one thing started is the one thing that waits: a shell
	   that forks and waits would be two processes where this suite means one. */
	out.stream_argv.push_back("exec sleep 3600");
	return out;
}

const Endpoint silent_endpoints[] = {
	{ Get, "/api/v1/silent", AuthLevel::Read, "answers a program that says nothing",
	  NULL, 0, NULL, &silentStream, false },
};

const RouteTable silent_table = { HTTPD_TABLE("silent", silent_endpoints) };

size_t waitForNoSessions(int timeout_ms)
{
	for (int waited = 0; waited < timeout_ms; waited += 10)
	{
		if (livestream::openSessions() == 0)
			return 0;
		::usleep(10 * 1000);
	}
	return livestream::openSessions();
}

} // namespace

TEST_CASE("a channel no browser can play is refused and nothing is started", "[browserstream]")
{
	Serving serving;
	REQUIRE(serving.port > 0);

	const testhttp::Reply r =
		testhttp::request(serving.port, "GET",
		                  "/api/v1/stream/browser/1001?video=mpeg2&audio=mp2", hostHeader());
	REQUIRE(r.transport_ok);
	CHECK(r.code == 501);
	CHECK(r.body.find("not-playable-in-browser") != std::string::npos);
	// Above all: the refusal cost the box nothing.
	CHECK(livestream::openSessions() == 0);
}

TEST_CASE("a channel a browser can play answers and gives the program back", "[browserstream]")
{
	Serving serving;
	REQUIRE(serving.port > 0);

	const testhttp::Reply r =
		testhttp::request(serving.port, "GET",
		                  "/api/v1/stream/browser/1001?video=h264&audio=mp2", hostHeader());
	REQUIRE(r.transport_ok);

	if (!hasConverter())
	{
		WARN("no converter on this machine, so the answer under test is the refusal");
		CHECK(r.code == 501);
		CHECK(r.body.find("not-playable-in-browser") != std::string::npos);
		CHECK(livestream::openSessions() == 0);
		return;
	}

	CHECK(r.code == 200);
	CHECK(r.header("Content-Type") == "video/mp2t");
	/* Of unknown length by definition, so it goes out in chunks. An answer
	   that stated a length would be an answer this server knows the end of,
	   and a live channel has none. */
	CHECK(r.header("Content-Length").empty());
	CHECK(r.header("Cache-Control") == "no-store");

	/* The whole point of the case. A program left behind is a box that works
	   for a week and then cannot start anything, and nothing else in this
	   suite would say so. */
	CHECK(waitForNoSessions(5000) == 0);
}

TEST_CASE("a listener who goes away while nothing is coming takes the program with them",
          "[browserstream]")
{
	/* THE ONE FAILURE THAT LOOKS LIKE NOTHING AT ALL, and the reason it needs a
	   program that says nothing rather than a converter.

	   A reader with an empty pipe suspends its connection, and a suspended
	   connection is out of the library's event loop: nothing is written on it,
	   so nothing finds out that the far end has gone, and the library's own
	   connection timeout does not reach a suspended connection either.
	   Measured before this was held here: a client that exited cleanly left the
	   program running past ninety seconds, with its place on the ceiling taken,
	   and two of those are a box that refuses everybody until it is restarted.

	   The client below gives up after its own deadline and closes, which is a
	   person closing a tab. What has to happen then is that the program is
	   ended and the place comes back. */
	Serving serving;
	REQUIRE(serving.port > 0);
	setRoutesForTest(&silent_table);

	/* What comes back is not read for a status. A body that never begins is a
	   reply this client cannot finish, so it waits out its own deadline and
	   closes, which is exactly the departure under test; what it made of the
	   half of an answer it saw is not the question. */
	(void) testhttp::request(serving.port, "GET", "/api/v1/silent", hostHeader());

	/* Longer than the deadline the client waited, and far shorter than the hour
	   the program would otherwise sit there for. */
	CHECK(waitForNoSessions(10000) == 0);
}

TEST_CASE("radio is answered as sound alone", "[browserstream]")
{
	Serving serving;
	REQUIRE(serving.port > 0);

	const testhttp::Reply r =
		testhttp::request(serving.port, "GET",
		                  "/api/v1/stream/browser/2001?video=none&audio=mp2", hostHeader());
	REQUIRE(r.transport_ok);
	CHECK(r.code == (hasConverter() ? 200 : 501));
	// ADTS and not a transport stream, which is what lets an audio element
	// play it with nothing in the page in front of it.
	if (hasConverter())
		CHECK(r.header("Content-Type") == "audio/aac");
	CHECK(waitForNoSessions(5000) == 0);
}

TEST_CASE("a head asks what a get would send and starts nothing", "[browserstream]")
{
	Serving serving;
	REQUIRE(serving.port > 0);

	const testhttp::Reply r =
		testhttp::request(serving.port, "HEAD",
		                  "/api/v1/stream/browser/1001?video=h264&audio=mp2", hostHeader());
	REQUIRE(r.transport_ok);
	CHECK(r.code == 200);
	CHECK(r.header("Content-Type") == "video/mp2t");
	/* A head is a question about the answer and not a request for it. Starting
	   a converter for one would be a program nobody is going to read, held for
	   as long as the caller keeps the connection. */
	CHECK(livestream::openSessions() == 0);
}

TEST_CASE("the codecs have to be named", "[browserstream]")
{
	Serving serving;
	REQUIRE(serving.port > 0);

	const testhttp::Reply none =
		testhttp::request(serving.port, "GET", "/api/v1/stream/browser/1001", hostHeader());
	REQUIRE(none.transport_ok);
	CHECK(none.code == 400);

	const testhttp::Reply half =
		testhttp::request(serving.port, "GET",
		                  "/api/v1/stream/browser/1001?video=h264", hostHeader());
	REQUIRE(half.transport_ok);
	CHECK(half.code == 400);

	// A name outside the set the channel route writes is not a name to act on.
	const testhttp::Reply invented =
		testhttp::request(serving.port, "GET",
		                  "/api/v1/stream/browser/1001?video=h264&audio=flac", hostHeader());
	REQUIRE(invented.transport_ok);
	CHECK(invented.code == 400);

	CHECK(livestream::openSessions() == 0);
}

TEST_CASE("a channel the box does not have is not converted", "[browserstream]")
{
	Serving serving;
	REQUIRE(serving.port > 0);

	const testhttp::Reply r =
		testhttp::request(serving.port, "GET",
		                  "/api/v1/stream/browser/9999?video=h264&audio=mp2", hostHeader());
	REQUIRE(r.transport_ok);
	CHECK(r.code == 404);
	CHECK(livestream::openSessions() == 0);
	(void) kUnknown;
}

TEST_CASE("there is a ceiling and it is small", "[browserstream]")
{
	/* Read rather than assumed, because the number is the whole of the
	   protection: every listener costs a converter for as long as they listen,
	   and a box that accepts everybody is a television picture that stutters
	   with nothing anywhere saying why. */
	CHECK(livestream::maxSessions() > 0);
	CHECK(livestream::maxSessions() <= 4);
}
