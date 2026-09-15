/*
 * test_requesthost.cpp - tests for the host a request names
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

#include "httpd/endpoint.h"
#include "httpd/router.h"
#include "httpd/server.h"
#include "httpd/status.h"

#include "coreapi/base/deps.h"

#include <neutrinoMessages.h>

#include <cstddef>
#include <cstdio>
#include <cstring>
#include <string>
#include <utility>
#include <vector>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

using namespace httpd;

namespace
{

// Writes back the authority the request named, which is what makes a value
// nothing else reads something a case can say anything about at all.
Response theHost(const Request &req)
{
	Response out;
	out.code = StatusOk;
	out.content_type = "text/plain";
	out.body = req.host();
	return out;
}

/* Both, separated by a byte no authority and no address carries, so that a case
   comparing them compares two values and not one read twice. They are both
   strings taken off one connection, and a wiring that bound the wrong one would
   answer every case above this one. */
Response theHostAndThePeer(const Request &req)
{
	Response out;
	out.code = StatusOk;
	out.content_type = "text/plain";
	out.body = req.host() + " " + req.peer();
	return out;
}

const Endpoint host_endpoints[] = {
	{ Method::Get, "/api/v1/test/host",        AuthLevel::Read, "answers with the authority the request named",
	  NULL, 0, NULL, &theHost, false },
	{ Method::Get, "/api/v1/test/hostandpeer", AuthLevel::Read, "answers with that authority and the address it came from",
	  NULL, 0, NULL, &theHostAndThePeer, false },
};

const RouteTable host_table = { HTTPD_TABLE("host", host_endpoints) };

/* Puts the table above in front of the shipped ones and starts a daemon on a
   port the kernel picks. Everything it did is undone from the destructor,
   whichever line the case left through: a failed check unwinds past everything
   after it, and a case that left a daemon bound or a table installed would take
   both with it for the rest of the run. */
struct ServingHost
{
	// Declared first, so the daemon starts after the seams are filled and stops
	// before they are cleared.
	InstalledDependencies wired_;
	int port;

	ServingHost() : port(0)
	{
		setRoutesForTest(&host_table);

		ServerConfig c = defaultConfig();
		c.port = 0;
		c.bind_address = "127.0.0.1";
		if (start(c))
			port = boundPort();
	}

	~ServingHost()
	{
		stop();
		setRoutesForTest(NULL);
	}

private:
	ServingHost(const ServingHost &);
	ServingHost &operator=(const ServingHost &);
};

std::vector<std::pair<std::string, std::string> > headers(const char *name, const char *value)
{
	std::vector<std::pair<std::string, std::string> > out;
	out.push_back(std::make_pair(std::string(name), std::string(value)));
	return out;
}

/* A request with no Host line at all, written to the socket by hand.

   The client beside this cannot send one: it writes the line itself unless the case
   names the header, and naming it is how a case sets the value rather than how a case
   leaves it out. The two are different requests on the wire and the library tells them
   apart, which was measured rather than assumed: the one naming the header with nothing
   after it is passed on with an empty value, and the one with no such line at all is
   refused where it arrives.

   The whole answer and not the body alone, because the answer to this one is a status
   rather than a document, and a case reading only the body could not tell an answer
   this server wrote from one it never saw.

   Small on purpose: it speaks only enough of the protocol to get one answer back off a
   connection this closes. */
std::string replyToRequestWithoutHost(int port, const char *path)
{
	const int fd = ::socket(AF_INET, SOCK_STREAM, 0);
	if (fd < 0)
		return std::string("this case could not open a socket");

	struct sockaddr_in to;
	std::memset(&to, 0, sizeof(to));
	to.sin_family = AF_INET;
	to.sin_port = htons((uint16_t) port);
	to.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
	if (::connect(fd, (const struct sockaddr *) &to, sizeof(to)) != 0)
	{
		::close(fd);
		return std::string("this case could not reach the server");
	}

	std::string req = "GET ";
	req += path;
	req += " HTTP/1.1\r\nConnection: close\r\n\r\n";

	size_t written = 0;
	while (written < req.size())
	{
		const ssize_t n = ::write(fd, req.data() + written, req.size() - written);
		if (n <= 0)
		{
			::close(fd);
			return std::string("this case could not write its request");
		}
		written += (size_t) n;
	}

	std::string got;
	char buf[4096];
	for (;;)
	{
		const ssize_t n = ::read(fd, buf, sizeof(buf));
		if (n <= 0)
			break;
		got.append(buf, (size_t) n);
	}
	::close(fd);
	return got;
}

} // namespace

TEST_CASE("a handler is told the authority the request named", "[requesthost]")
{
	// Over the wire, because what is under test is the plumbing and not the
	// accessor: a case that calls setHost by hand proves that a setter sets.
	ServingHost serving;
	REQUIRE(serving.port > 0);

	const testhttp::Reply r = testhttp::request(serving.port, "GET", "/api/v1/test/host",
	                                            headers("Host", "box.example:8080"));
	REQUIRE(r.transport_ok);
	REQUIRE(r.code == 200);
	REQUIRE(r.body == "box.example:8080");
}

/* The two shapes side by side, because they are answered by different layers and a
   case driving only one would say nothing about the other. Naming the header with
   nothing after it is a request that carries an empty authority, and that one is this
   server's to answer. Leaving the line out is a request that carries none, and
   libmicrohttpd refuses that one itself before any handler runs.

   The refusal is the library's since 1.0.0. Its parse_connection_headers
   (src/microhttpd/connection.c) gates the check on the daemon's client_discipline,
   which defaults to 0, and only the lowest setting of MHD_OPTION_CLIENT_DISCIPLINE_LVL
   turns it off; through 0.9.75 the same check was gated on strict_for_client, which
   defaults to 0 and left it switched off. RFC 9112 asks for the refusal and nothing
   this box talks to omits the header, so the library is welcome to it.

   Only the one answer is stated, because configure.ac will not configure against a
   library older than 1.0.10 and there is no longer a build in which the other one
   happens.

   Not a word of what the library writes in place of the answer is read here: that text
   is worded differently between versions and is empty altogether in a build without
   messages. What is checked is whether this server saw the request, and that is also
   the guard: a later version that hands the case back sends it to the handler, which
   answers 200 and puts its own media type on it, and both checks below turn over
   rather than the old answer quietly returning. */
TEST_CASE("an empty authority reaches the handler and no authority at all does not",
          "[requesthost]")
{
	ServingHost serving;
	REQUIRE(serving.port > 0);

	const testhttp::Reply named_empty =
		testhttp::request(serving.port, "GET", "/api/v1/test/host", headers("Host", ""));
	REQUIRE(named_empty.transport_ok);
	REQUIRE(named_empty.code == 200);
	REQUIRE(named_empty.body.empty());

	/* The handler names this media type on the one answer it has, so an answer
	   carrying it is one that reached it and an answer without it is one that did
	   not. */
	const std::string raw = replyToRequestWithoutHost(serving.port, "/api/v1/test/host");
	REQUIRE(raw.compare(0, 12, "HTTP/1.1 400") == 0);
	REQUIRE(raw.find("text/plain") == std::string::npos);
}

TEST_CASE("the authority is not the peer", "[requesthost]")
{
	// Stated because the two are both strings off one connection and a
	// plumbing that bound the wrong one would pass every case above.
	ServingHost serving;
	REQUIRE(serving.port > 0);

	const testhttp::Reply r = testhttp::request(serving.port, "GET", "/api/v1/test/hostandpeer",
	                                            headers("Host", "box.example:8080"));
	REQUIRE(r.transport_ok);
	REQUIRE(r.code == 200);
	REQUIRE(r.body.find("box.example:8080") != std::string::npos);
	REQUIRE(r.body.find("127.0.0.1") != std::string::npos);
}

TEST_CASE("the two new commands travel under the numbers the loop reads",
          "[requesthost]")
{
	// Not a transcription: the numbers come from neutrinoMessages.h, and a
	// number written out here would be somebody typing one thing twice.
	REQUIRE(coreapi::eventNumber(coreapi::BoxEvent::StartPlugin)
	        == (unsigned) NeutrinoMessages::EVT_START_PLUGIN);
	REQUIRE(coreapi::eventNumber(coreapi::BoxEvent::ReloadSetup)
	        == (unsigned) NeutrinoMessages::RELOAD_SETUP);
	REQUIRE(coreapi::eventNumber(coreapi::BoxEvent::Count) == 0);
}
