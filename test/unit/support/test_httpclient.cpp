/*
 * test_httpclient.cpp - tests for the test HTTP client
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

#include "catch.hpp"
#include "httpclient.h"

#include <cstring>
#include <string>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <pthread.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

namespace
{

// A server that answers with the bytes a case chose and then closes. A real
// server sends whole replies, so it is the only thing that can produce the
// shapes below, and those shapes are what decides whether the client's answer
// means a reply arrived or only that some of one did.
struct RawServer
{
	int         listener;
	int         port;
	std::string payload;
	pthread_t   thread;
	bool        serving;

	explicit RawServer(const std::string &p)
		: listener(-1), port(0), payload(p), thread(0), serving(false)
	{
		listener = ::socket(AF_INET, SOCK_STREAM, 0);
		if (listener < 0)
			return;

		// A case whose client never connects would otherwise leave the thread
		// in accept and the join at the end of the case waiting on it.
		struct timeval tv;
		std::memset(&tv, 0, sizeof(tv));
		tv.tv_sec = 5;
		::setsockopt(listener, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

		struct sockaddr_in addr;
		std::memset(&addr, 0, sizeof(addr));
		addr.sin_family = AF_INET;
		addr.sin_port = 0;

		struct sockaddr_in bound;
		socklen_t bound_len = sizeof(bound);
		std::memset(&bound, 0, sizeof(bound));

		if (::inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr) != 1
		    || ::bind(listener, (const struct sockaddr *) &addr, sizeof(addr)) != 0
		    || ::listen(listener, 1) != 0
		    || ::getsockname(listener, (struct sockaddr *) &bound, &bound_len) != 0)
		{
			::close(listener);
			listener = -1;
			return;
		}

		port = (int) ntohs(bound.sin_port);
		serving = (::pthread_create(&thread, NULL, &serve, this) == 0);
		if (!serving)
		{
			::close(listener);
			listener = -1;
			port = 0;
		}
	}

	~RawServer()
	{
		if (serving)
			::pthread_join(thread, NULL);
		if (listener >= 0)
			::close(listener);
	}

	static void *serve(void *arg);

private:
	RawServer(const RawServer &);
	RawServer &operator=(const RawServer &);
};

void *RawServer::serve(void *arg)
{
	RawServer *self = (RawServer *) arg;

	const int c = ::accept(self->listener, NULL, NULL);
	if (c < 0)
		return NULL;

	// The request is read and dropped. What the client sent is not what these
	// cases are about, but a peer that never reads it can make the client's
	// send fail instead of its read.
	char scratch[4096];
	const ssize_t got = ::recv(c, scratch, sizeof(scratch), 0);
	(void) got;

	if (!self->payload.empty())
	{
		const ssize_t sent = ::send(c, self->payload.data(), self->payload.size(), 0);
		(void) sent;
	}

	::close(c);
	return NULL;
}

} // namespace

TEST_CASE("a whole reply is a reply", "[httpclient]")
{
	RawServer s("HTTP/1.1 200 OK\r\nContent-Length: 10\r\n\r\nabcdefghij");
	REQUIRE(s.port > 0);

	testhttp::Reply r = testhttp::request(s.port, "GET", "/");
	REQUIRE(r.transport_ok);
	REQUIRE(r.code == 200);
	REQUIRE(r.body == "abcdefghij");
	REQUIRE(r.header("Content-Length") == "10");
	REQUIRE(r.header("content-length") == "10");
	REQUIRE(r.header("X-Absent") == "");
}

TEST_CASE("a status line that stopped early is not a reply", "[httpclient]")
{
	// Read as far as it goes this says the code is twenty, which is not a code
	// and not what the server was going to send.
	RawServer s("HTTP/1.1 20");
	REQUIRE(s.port > 0);

	testhttp::Reply r = testhttp::request(s.port, "GET", "/");
	REQUIRE_FALSE(r.transport_ok);
	REQUIRE(r.code == 0);
}

TEST_CASE("a head that stopped early is not a reply", "[httpclient]")
{
	RawServer s("HTTP/1.1 200 OK\r\nContent-Ty");
	REQUIRE(s.port > 0);

	testhttp::Reply r = testhttp::request(s.port, "GET", "/");
	REQUIRE_FALSE(r.transport_ok);
	REQUIRE(r.code == 0);
}

TEST_CASE("a body shorter than the length it declared is not a reply", "[httpclient]")
{
	// The head is whole and the code is real, which is what makes this the one
	// that reads as a reply if nothing counts the body.
	RawServer s("HTTP/1.1 200 OK\r\nContent-Length: 10\r\n\r\nabc");
	REQUIRE(s.port > 0);

	testhttp::Reply r = testhttp::request(s.port, "GET", "/");
	REQUIRE_FALSE(r.transport_ok);
	REQUIRE(r.code == 0);
	REQUIRE(r.body.empty());
}

TEST_CASE("a peer that accepts and says nothing is not a reply", "[httpclient]")
{
	RawServer s("");
	REQUIRE(s.port > 0);

	testhttp::Reply r = testhttp::request(s.port, "GET", "/");
	REQUIRE_FALSE(r.transport_ok);
	REQUIRE(r.code == 0);
}

TEST_CASE("bytes that are not a status line are not a reply", "[httpclient]")
{
	RawServer s("not http at all\r\n\r\n");
	REQUIRE(s.port > 0);

	testhttp::Reply r = testhttp::request(s.port, "GET", "/");
	REQUIRE_FALSE(r.transport_ok);
	REQUIRE(r.code == 0);
}

TEST_CASE("a body ended by the close rather than by a length is a reply", "[httpclient]")
{
	// Nothing states how long it is, so the close is what says it is whole.
	RawServer s("HTTP/1.1 200 OK\r\nX-Thing: y\r\n\r\nsome bytes");
	REQUIRE(s.port > 0);

	testhttp::Reply r = testhttp::request(s.port, "GET", "/");
	REQUIRE(r.transport_ok);
	REQUIRE(r.code == 200);
	REQUIRE(r.body == "some bytes");
}

TEST_CASE("nothing listening is not a reply", "[httpclient]")
{
	// The port a raw server had and gave up, so the connect is refused rather
	// than left hanging.
	int gone = 0;
	{
		RawServer s("HTTP/1.1 200 OK\r\nContent-Length: 0\r\n\r\n");
		REQUIRE(s.port > 0);
		gone = s.port;
		testhttp::Reply alive = testhttp::request(gone, "GET", "/");
		REQUIRE(alive.transport_ok);
	}

	testhttp::Reply r = testhttp::request(gone, "GET", "/");
	REQUIRE_FALSE(r.transport_ok);
	REQUIRE(r.code == 0);
}

TEST_CASE("an answer to HEAD declares a length it does not send and is a reply", "[httpclient]")
{
	// What the protocol says a HEAD answer looks like: the length the body
	// would have had, and no body. Counting bytes against that length refuses
	// every one of them.
	RawServer s("HTTP/1.1 200 OK\r\nContent-Length: 4096\r\n\r\n");
	REQUIRE(s.port > 0);

	testhttp::Reply r = testhttp::request(s.port, "HEAD", "/something-large");
	REQUIRE(r.transport_ok);
	REQUIRE(r.code == 200);
	REQUIRE(r.header("Content-Length") == "4096");
	REQUIRE(r.body.empty());
}

TEST_CASE("a 304 declares a length it does not send and is a reply", "[httpclient]")
{
	// The length is stated on purpose: without it the reply would be whole by
	// the close alone and the case would pass whether the exemption exists or
	// not.
	RawServer s("HTTP/1.1 304 Not Modified\r\nETag: \"v1\"\r\nContent-Length: 512\r\n\r\n");
	REQUIRE(s.port > 0);

	testhttp::Reply r = testhttp::request(s.port, "GET", "/unchanged");
	REQUIRE(r.transport_ok);
	REQUIRE(r.code == 304);
	REQUIRE(r.header("ETag") == "\"v1\"");
	REQUIRE(r.body.empty());
}

TEST_CASE("a 204 declares a length it does not send and is a reply", "[httpclient]")
{
	RawServer s("HTTP/1.1 204 No Content\r\nContent-Length: 99\r\n\r\n");
	REQUIRE(s.port > 0);

	testhttp::Reply r = testhttp::request(s.port, "DELETE", "/gone");
	REQUIRE(r.transport_ok);
	REQUIRE(r.code == 204);
	REQUIRE(r.body.empty());
}

TEST_CASE("an answer to HEAD whose head stopped early is not a reply", "[httpclient]")
{
	// The exemption covers the body and nothing else, so the head still has to
	// have terminated.
	RawServer s("HTTP/1.1 200 OK\r\nContent-Le");
	REQUIRE(s.port > 0);

	testhttp::Reply r = testhttp::request(s.port, "HEAD", "/something-large");
	REQUIRE_FALSE(r.transport_ok);
	REQUIRE(r.code == 0);
}

TEST_CASE("an answer to HEAD without a code is not a reply", "[httpclient]")
{
	// Nor does the exemption reach the code, which still has to be one.
	RawServer s("HTTP/1.1 20\r\n\r\n");
	REQUIRE(s.port > 0);

	testhttp::Reply r = testhttp::request(s.port, "HEAD", "/something-large");
	REQUIRE_FALSE(r.transport_ok);
	REQUIRE(r.code == 0);
}
