/*
 * test_filebody.cpp - tests for answering with a file body
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

#include "coreapi/base/errors.h"

#include <cstddef>
#include <cstdio>
#include <string>
#include <utility>
#include <vector>

#include <errno.h>
#include <stdint.h>

#include <dirent.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

using namespace httpd;

namespace
{

const char kBody[] = "hello";
const size_t kBodySize = 5;

/* The file every case here answers out of, and the one descriptor on it that the
   fixture holds open. Reached through functions rather than written at namespace scope,
   so nothing in this file needs a constructor to have run before main.

   One descriptor, opened once and held: a route that kept the name instead and opened
   it again per request would open whatever carries that name by then, and one case
   below takes the name away to say so. */
std::string &tempPath()
{
	static std::string path;
	return path;
}

int &heldFd()
{
	static int fd = -1;
	return fd;
}

/* How many of this process's descriptors are on one file, found by asking what each of
   them is on rather than by counting them. A number on its own answers a different
   question: connections opening and closing move it while a case runs, and three
   descriptors a case once reported as leaked were three connections on their way out.

   A name that has been taken away reads back with a word after it and is the same file,
   which is the whole point of holding the descriptor, so that word is taken off before
   the comparison. */
size_t openCountFor(const std::string &path)
{
	DIR *d = ::opendir("/proc/self/fd");
	if (d == NULL)
		return 0;

	const std::string gone = " (deleted)";
	size_t n = 0;
	for (struct dirent *e = ::readdir(d); e != NULL; e = ::readdir(d))
	{
		if (e->d_name[0] == '.')
			continue;

		std::string link = "/proc/self/fd/";
		link += e->d_name;

		char target[4096];
		const ssize_t got = ::readlink(link.c_str(), target, sizeof(target) - 1);
		if (got <= 0)
			continue;

		std::string what(target, (size_t) got);
		if (what.size() > gone.size() &&
		    what.compare(what.size() - gone.size(), gone.size(), gone) == 0)
			what.erase(what.size() - gone.size());

		if (what == path)
			++n;
	}
	::closedir(d);
	return n;
}

/* Waits for the count to come back to what it should be and answers what it
   reached. The library gives the descriptor back when it is done with the
   answer, which is after the client has read it, so a count taken the instant
   the last reply arrives can still be one ahead of a server that is right. A
   server that is wrong never comes back and this spends the budget saying so. */
size_t waitForCount(const std::string &path, size_t want, int budget_ms)
{
	for (int waited = 0; waited < budget_ms; waited += 20)
	{
		const size_t now = openCountFor(path);
		if (now == want)
			return now;

		struct timespec ts;
		ts.tv_sec = 0;
		ts.tv_nsec = 20 * 1000 * 1000;
		nanosleep(&ts, NULL);
	}
	return openCountFor(path);
}

Response outOfTheFile(const Request &)
{
	Response out;
	out.code = StatusOk;
	out.content_type = "text/plain";

	/* A copy of the held descriptor per answer, because the transport closes
	   what it is given and the file cannot be opened by name again once one
	   case has taken the name away. */
	if (!answerFromDescriptor(out, ::dup(heldFd())))
		return problemResponse(StatusInternalServerError, coreapi::ErrorCode::BoxUnreadable,
		                       "this case could not copy its descriptor");
	return out;
}

/* A length no answer can carry, which is what makes the library refuse to build a
   response out of the descriptor at all. That refusal is one of the two ways out where
   the descriptor is never taken over, and the only one a case can reach: the others are
   allocations failing inside the library. Written by hand rather than through the
   builder above, because the builder states the length off the descriptor. */
Response tooLongForTheLibrary(const Request &)
{
	Response out;
	out.code = StatusOk;
	out.content_type = "text/plain";
	out.fd = ::dup(heldFd());
	out.length = ((uint64_t) 1) << 63;
	return out;
}

// A header name the library will not take, on an answer whose descriptor it has
// already taken. That is the other way out, and what gives the descriptor back
// there is the response being destroyed rather than anything this side does.
Response withAHeaderTheLibraryRefuses(const Request &)
{
	Response out;
	out.code = StatusOk;
	out.content_type = "text/plain";
	if (!answerFromDescriptor(out, ::dup(heldFd())))
		return problemResponse(StatusInternalServerError, coreapi::ErrorCode::BoxUnreadable,
		                       "this case could not copy its descriptor");

	// A space is not allowed in one, and the library says so rather than
	// sending it.
	out.headers.push_back(std::make_pair(std::string("Bad Header"), std::string("x")));
	return out;
}

// An answer built out of nothing but a string, so that what the transport does
// about a range can be asked of an answer that is not a file.
Response outOfAString(const Request &)
{
	Response out;
	out.code = StatusOk;
	out.content_type = "text/plain";
	out.body = "hello";
	return out;
}

const Endpoint file_endpoints[] = {
	{ Method::Get, "/api/v1/test/file",         AuthLevel::Read, "answers out of a file",
	  NULL, 0, NULL, &outOfTheFile, false },
	{ Method::Get, "/api/v1/test/file-toolong", AuthLevel::Read, "states a length no answer carries",
	  NULL, 0, NULL, &tooLongForTheLibrary, false },
	{ Method::Get, "/api/v1/test/file-badhead", AuthLevel::Read, "writes a header the library refuses",
	  NULL, 0, NULL, &withAHeaderTheLibraryRefuses, false },
	{ Method::Get, "/api/v1/test/string",       AuthLevel::Read, "answers out of a string",
	  NULL, 0, NULL, &outOfAString, false },
};

const RouteTable file_table = { HTTPD_TABLE("file", file_endpoints) };

/* Writes the file, holds one descriptor on it, puts the table above in front of the
   shipped ones and starts a daemon on a port the kernel picks. Everything it did is
   undone from the destructor, whichever line the case left through: a case that left a
   daemon bound or a table installed would take both with it for the rest of the run. */
struct ServingFile
{
	// Declared first, so the daemon starts after the seams are filled and stops
	// before they are cleared.
	InstalledDependencies wired_;
	int port;

	ServingFile() : port(0)
	{
		char name[128];
		std::snprintf(name, sizeof(name), "/tmp/coreapi-filebody.%d.%u",
		              (int) ::getpid(), nextOne());
		tempPath() = name;

		const int made = ::open(tempPath().c_str(), O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, 0600);
		if (made >= 0)
		{
			const ssize_t wrote = ::write(made, kBody, kBodySize);
			(void) wrote;
			::close(made);
		}

		heldFd() = ::open(tempPath().c_str(), O_RDONLY | O_CLOEXEC);

		setRoutesForTest(&file_table);

		ServerConfig c = defaultConfig();
		c.port = 0;
		c.bind_address = "127.0.0.1";
		if (start(c))
			port = boundPort();
	}

	~ServingFile()
	{
		stop();
		setRoutesForTest(NULL);
		if (heldFd() >= 0)
		{
			::close(heldFd());
			heldFd() = -1;
		}
		if (!tempPath().empty())
		{
			::unlink(tempPath().c_str());
			tempPath().clear();
		}
	}

private:
	// A name of its own per case, so that one case taking the name away cannot
	// reach into the next.
	static unsigned nextOne()
	{
		static unsigned n = 0;
		return ++n;
	}

	ServingFile(const ServingFile &);
	ServingFile &operator=(const ServingFile &);
};

testhttp::Reply get(int port, const char *path)
{
	return testhttp::request(port, "GET", path);
}

// The same with one header, which is the only thing every case below this file's
// first half needs that the call above does not offer.
testhttp::Reply getRanged(int port, const char *path, const char *range)
{
	std::vector<std::pair<std::string, std::string> > headers;
	headers.push_back(std::make_pair(std::string("Range"), std::string(range)));
	return testhttp::request(port, "GET", path, headers);
}

} // namespace

TEST_CASE("a file backed answer carries the file and its length", "[filebody]")
{
	ServingFile serving;
	REQUIRE(serving.port > 0);
	REQUIRE(heldFd() >= 0);

	const testhttp::Reply r = get(serving.port, "/api/v1/test/file");
	REQUIRE(r.transport_ok);
	REQUIRE(r.code == 200);
	REQUIRE(r.body == "hello");
	REQUIRE(r.header("Content-Length") == "5");

	// Chunked would mean the client above could not have read it: it decodes
	// none, so an answer sent that way is an answer no case here can say
	// anything about.
	REQUIRE(r.header("Transfer-Encoding").empty());
}

TEST_CASE("a file that vanished between answer and send is still the file", "[filebody]")
{
	ServingFile serving;
	REQUIRE(serving.port > 0);
	REQUIRE(heldFd() >= 0);

	// The descriptor is held, so taking the name away changes nothing about
	// what the answer is made of or how long it is.
	REQUIRE(::unlink(tempPath().c_str()) == 0);

	const testhttp::Reply r = get(serving.port, "/api/v1/test/file");
	REQUIRE(r.transport_ok);
	REQUIRE(r.code == 200);
	REQUIRE(r.body == "hello");
	REQUIRE(r.header("Content-Length") == "5");
}

TEST_CASE("a thousand file answers leak no descriptors", "[filebody]")
{
	ServingFile serving;
	REQUIRE(serving.port > 0);
	REQUIRE(heldFd() >= 0);

	/* The counter has to find something before finding nothing means anything.
	   The fixture holds exactly one descriptor on this file, and a count that
	   cannot see that one would report every leak as none, which is a case that
	   cannot go red. */
	const size_t before = openCountFor(tempPath());
	REQUIRE(before == 1);

	/* Counted rather than checked a thousand times over: what a case here says
	   is that a thousand answers went out and left nothing behind, and a
	   thousand assertions saying one of them arrived would say the same thing
	   at a thousand times the cost. A run where none of them arrived leaves
	   nothing behind either, so the count is what keeps this from being green
	   over a server that answered nothing. */
	size_t answered = 0;
	for (int i = 0; i < 1000; ++i)
	{
		const testhttp::Reply r = get(serving.port, "/api/v1/test/file");
		if (r.transport_ok && r.code == 200 && r.body == "hello")
			++answered;
	}
	REQUIRE(answered == 1000u);

	REQUIRE(waitForCount(tempPath(), before, 5000) == before);
}

TEST_CASE("answers the library will not build out of leak no descriptors", "[filebody]")
{
	ServingFile serving;
	REQUIRE(serving.port > 0);
	REQUIRE(heldFd() >= 0);

	const size_t before = openCountFor(tempPath());
	REQUIRE(before == 1);

	/* Nothing goes back on this one: the answer could not be built, so the
	   connection is closed without one. What the case is about is the
	   descriptor that answer was going to be made of, which nothing else is
	   ever going to close. */
	size_t refused = 0;
	for (int i = 0; i < 1000; ++i)
	{
		const testhttp::Reply r = get(serving.port, "/api/v1/test/file-toolong");
		if (r.code != 200)
			++refused;
	}
	REQUIRE(refused == 1000u);

	REQUIRE(waitForCount(tempPath(), before, 5000) == before);
}

TEST_CASE("answers refused over a header leak no descriptors", "[filebody]")
{
	ServingFile serving;
	REQUIRE(serving.port > 0);
	REQUIRE(heldFd() >= 0);

	const size_t before = openCountFor(tempPath());
	REQUIRE(before == 1);

	// The answer that goes back instead says the fault is here, which is what
	// the transport answers with for every header it could not write.
	size_t refused = 0;
	for (int i = 0; i < 1000; ++i)
	{
		const testhttp::Reply r = get(serving.port, "/api/v1/test/file-badhead");
		if (r.transport_ok && r.code == 500)
			++refused;
	}
	REQUIRE(refused == 1000u);

	REQUIRE(waitForCount(tempPath(), before, 5000) == before);
}

TEST_CASE("a descriptor no answer can be made of is given back by the builder", "[filebody]")
{
	/* The builder takes the descriptor over whatever it answers, so a caller
	   never has to work out whether it still owns one. A directory is a
	   descriptor no answer can be sent out of, and it needs no server to say
	   so. */
	const int dir = ::open("/tmp", O_RDONLY | O_CLOEXEC);
	REQUIRE(dir >= 0);

	Response out;
	REQUIRE_FALSE(answerFromDescriptor(out, dir));
	REQUIRE(out.fd == -1);
	REQUIRE(out.length == 0u);

	// Given back means the number names nothing any more, asked of the number
	// itself rather than of what the next open happens to be handed.
	errno = 0;
	REQUIRE(::fcntl(dir, F_GETFD) == -1);
	REQUIRE(errno == EBADF);
}

/* One stretch of a file rather than the whole of it.

   The whole point is the size of what these routes hand out. A recording runs to
   gigabytes. A player that jumps into the middle of one asks for the bytes it landed
   on, and a download that broke off asks for the rest; a server that only ever answers
   the whole file makes each of those cost the whole file again. The file behind these
   cases is five bytes, which is enough to say which bytes came back.

   Answered by the transport and not by a handler, so what is driven here is every route
   that answers out of a file. */

TEST_CASE("a file backed answer says it takes ranges", "[filebody][range]")
{
	/* Said on the answer that carried no range too, because what it states is a
	   property of the answer rather than of the request: a client that has to
	   find out by asking and being refused pays a whole request to learn it. */
	ServingFile serving;
	REQUIRE(serving.port > 0);

	const testhttp::Reply r = get(serving.port, "/api/v1/test/file");
	REQUIRE(r.code == 200);
	REQUIRE(r.header("Accept-Ranges") == "bytes");
	REQUIRE(r.header("Content-Range").empty());
}

TEST_CASE("an answer that is not a file says nothing about ranges", "[filebody][range]")
{
	// A document about itself has no stretches worth naming, and a range asked
	// of one is neither answered nor refused.
	ServingFile serving;
	REQUIRE(serving.port > 0);

	const testhttp::Reply plain = get(serving.port, "/api/v1/test/string");
	REQUIRE(plain.code == 200);
	REQUIRE(plain.header("Accept-Ranges").empty());

	const testhttp::Reply ranged = getRanged(serving.port, "/api/v1/test/string", "bytes=1-3");
	REQUIRE(ranged.code == 200);
	REQUIRE(ranged.body == "hello");
	REQUIRE(ranged.header("Content-Range").empty());
}

TEST_CASE("a stretch of a file comes back as that stretch and says which", "[filebody][range]")
{
	ServingFile serving;
	REQUIRE(serving.port > 0);

	const testhttp::Reply r = getRanged(serving.port, "/api/v1/test/file", "bytes=1-3");
	REQUIRE(r.transport_ok);
	REQUIRE(r.code == 206);
	REQUIRE(r.body == "ell");
	// The length of what was sent, and the stretch it was taken from, which is
	// how a client works out where the rest of the file is.
	REQUIRE(r.header("Content-Length") == "3");
	REQUIRE(r.header("Content-Range") == "bytes 1-3/5");
	REQUIRE(r.header("Content-Type") == "text/plain");
	// Chunked would mean the client above could not have read it at all.
	REQUIRE(r.header("Transfer-Encoding").empty());
}

TEST_CASE("a stretch with no end runs to the end of the file", "[filebody][range]")
{
	// What a download resumed from where it stopped asks for.
	ServingFile serving;
	REQUIRE(serving.port > 0);

	const testhttp::Reply r = getRanged(serving.port, "/api/v1/test/file", "bytes=2-");
	REQUIRE(r.code == 206);
	REQUIRE(r.body == "llo");
	REQUIRE(r.header("Content-Range") == "bytes 2-4/5");
}

TEST_CASE("the last so many bytes of a file come back as those bytes", "[filebody][range]")
{
	ServingFile serving;
	REQUIRE(serving.port > 0);

	const testhttp::Reply r = getRanged(serving.port, "/api/v1/test/file", "bytes=-2");
	REQUIRE(r.code == 206);
	REQUIRE(r.body == "lo");
	REQUIRE(r.header("Content-Range") == "bytes 3-4/5");
}

TEST_CASE("the whole file as a stretch is still a stretch", "[filebody][range]")
{
	// A client that asks for all of it by naming all of it is answered what it
	// asked, and not 200, because the two say different things about whether
	// the range was read at all.
	ServingFile serving;
	REQUIRE(serving.port > 0);

	const testhttp::Reply r = getRanged(serving.port, "/api/v1/test/file", "bytes=0-4");
	REQUIRE(r.code == 206);
	REQUIRE(r.body == "hello");
	REQUIRE(r.header("Content-Range") == "bytes 0-4/5");
}

TEST_CASE("a stretch that begins past the end of the file is refused", "[filebody][range]")
{
	/* And refused rather than answered with the file from the start, which
	   would hand a client bytes it did not ask for with nothing in the answer
	   saying so. The length travels with the refusal, so a client that guessed
	   wrong learns what to ask for without fetching anything. */
	ServingFile serving;
	REQUIRE(serving.port > 0);

	const testhttp::Reply r = getRanged(serving.port, "/api/v1/test/file", "bytes=9-");
	REQUIRE(r.transport_ok);
	REQUIRE(r.code == 416);
	REQUIRE(r.header("Content-Range") == "bytes */5");
	// The refusal is the one document every other refusal in this server is
	// written as, and not the file.
	REQUIRE(r.header("Content-Type") == "application/problem+json");
	REQUIRE(r.body.find("range-outside-file") != std::string::npos);
	REQUIRE(r.body.find("hello") == std::string::npos);
}

TEST_CASE("a range this server does not read leaves the answer whole", "[filebody][range]")
{
	/* Three headers that name no stretch of this file, each answered with the
	   whole of it. A caller handed the whole file has what it asked for and
	   more; one refused has nothing, and none of these is wrong enough to
	   deserve that. */
	ServingFile serving;
	REQUIRE(serving.port > 0);

	const char *const ignored[] = { "bytes=0-1,3-4", "items=0-1", "bytes=3-1" };
	for (size_t i = 0; i < sizeof(ignored) / sizeof(ignored[0]); ++i)
	{
		INFO(ignored[i]);
		const testhttp::Reply r = getRanged(serving.port, "/api/v1/test/file", ignored[i]);
		REQUIRE(r.code == 200);
		REQUIRE(r.body == "hello");
		REQUIRE(r.header("Content-Range").empty());
	}
}

TEST_CASE("a thousand ranged answers leak no descriptors", "[filebody][range]")
{
	/* The refusal is the one path where an answer is built out of a descriptor and then
	   sent out of something else, so the descriptor is given back by nothing the library
	   does and the transport has to do it. A thousand of them is what makes that
	   visible. The whole file is asked for in between, so what is counted is the two
	   paths together. */
	ServingFile serving;
	REQUIRE(serving.port > 0);
	REQUIRE(heldFd() >= 0);

	const size_t before = openCountFor(tempPath());
	REQUIRE(before == 1);

	size_t refused = 0;
	size_t partial = 0;
	for (int i = 0; i < 500; ++i)
	{
		if (getRanged(serving.port, "/api/v1/test/file", "bytes=9-").code == 416)
			++refused;
		if (getRanged(serving.port, "/api/v1/test/file", "bytes=1-2").code == 206)
			++partial;
	}
	REQUIRE(refused == 500u);
	REQUIRE(partial == 500u);

	REQUIRE(waitForCount(tempPath(), before, 5000) == before);
}
