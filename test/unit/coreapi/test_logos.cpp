/*
 * test_logos.cpp - tests for channel logos
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
#include "httpd/http.h"
#include "httpd/router.h"
#include "httpd/server.h"
#include "httpd/webconfig.h"

#include "coreapi/channels.h"
#include "coreapi/base/deps.h"
#include "coreapi/base/errors.h"
#include "coreapi/base/result.h"
#include "coreapi/base/types.h"

#include "jsoncpp/json/json.h"

#include <cstdio>
#include <cstdlib>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include <dirent.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

using namespace coreapi;

namespace
{

/* A directory of its own per case, and real files in it. What these cases are
   about is what the layer above the seam does with a path, and following a
   link is the one part of that which nothing but the filesystem can answer, so
   a case that means a link makes one rather than describing one. */
struct Fixture
{
	std::string dir;

	Fixture()
	{
		char pattern[] = "/tmp/coreapi-logos-XXXXXX";
		const char *made = ::mkdtemp(pattern);
		REQUIRE(made != NULL);
		dir = made;
	}

	~Fixture()
	{
		for (size_t i = names.size(); i > 0; i--)
			::unlink(names[i - 1].c_str());
		::rmdir(dir.c_str());
	}

	std::string write(const std::string &name, const std::string &bytes)
	{
		const std::string path = dir + "/" + name;
		std::FILE *f = std::fopen(path.c_str(), "wb");
		REQUIRE(f != NULL);
		const bool whole = bytes.empty() ||
			std::fwrite(bytes.data(), 1, bytes.size(), f) == bytes.size();
		std::fclose(f);
		REQUIRE(whole);
		names.push_back(path);
		return path;
	}

	std::string link(const std::string &name, const std::string &target)
	{
		const std::string path = dir + "/" + name;
		REQUIRE(::symlink(target.c_str(), path.c_str()) == 0);
		names.push_back(path);
		return path;
	}

	private:
		std::vector<std::string> names;

		Fixture(const Fixture &);
		Fixture &operator=(const Fixture &);
};

ChannelInfo channel(ChannelId id, const std::string &name)
{
	ChannelInfo c;
	c.id = id;
	c.name = name;
	c.kind = ServiceKind::Tv;
	return c;
}

// Nothing decodes it, so what matters is that the same bytes come back. The
// first eight are a real picture's so that a reader of this file is not left
// wondering whether the shape is being tested.
std::string pngBytes()
{
	const char magic[] = "\x89" "PNG\r\n\x1a\n and then some bytes";
	return std::string(magic, sizeof(magic) - 1);
}

/* How many of this process's descriptors are on one file, found by asking what
   each of them is on rather than by counting them: connections opening and
   closing move a bare number while a case runs. Written here rather than
   shared, because the other file that counts this way keeps its own copy file
   local for the same reason a fixture is.  */
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
   reached. The library gives the descriptor back once the client has read the
   answer, so a count taken the instant the last reply arrives can still be one
   ahead of a server that is right. One that is wrong never comes back and this
   spends the budget saying so. */
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

/* The fakes every seam the server checks before it starts, a daemon on a port
   the kernel picks, and the shipped tables in front of it. Everything is undone
   from the destructor, whichever line a case leaves through: a failed check
   unwinds past whatever came after it, and a case that left a daemon bound
   would take it with it for the rest of the run. */
struct Serving
{
	InstalledDependencies wired;
	int port;

	Serving() : port(0)
	{
		httpd::setRoutesForTest(NULL);

		httpd::ServerConfig c = httpd::defaultConfig();
		c.port = 0;
		c.bind_address = "127.0.0.1";
		if (httpd::start(c))
			port = httpd::boundPort();
	}

	~Serving()
	{
		httpd::stop();
		httpd::setRoutesForTest(NULL);
	}

	private:
		Serving(const Serving &);
		Serving &operator=(const Serving &);
};

/* This server set not to hand channel pictures over. What is in effect is
   process wide and is put back from a destructor, because a check that fails
   unwinds past a last line and the case after this one would otherwise be
   answered out of the policy this one installed. */
struct NoLogos
{
	httpd::WebConfig before;

	NoLogos() : before(httpd::config())
	{
		httpd::WebConfig c = httpd::defaultWebConfig();
		c.channel_logos = false;
		httpd::setConfigForTest(c);
	}

	~NoLogos() { httpd::setConfigForTest(before); }

	private:
		NoLogos(const NoLogos &);
		NoLogos &operator=(const NoLogos &);
};

// The same tables a request off a socket is answered from, for the cases that
// go through the router rather than through a daemon.
struct ShippedRoutes
{
	ShippedRoutes() { httpd::setRoutesForTest(NULL); }
	~ShippedRoutes() { httpd::setRoutesForTest(NULL); }

	private:
		ShippedRoutes(const ShippedRoutes &);
		ShippedRoutes &operator=(const ShippedRoutes &);
};

::Json::Value parsed(const std::string &doc)
{
	::Json::CharReaderBuilder builder;
	std::unique_ptr< ::Json::CharReader> reader(builder.newCharReader());
	::Json::Value root;
	std::string errs;
	REQUIRE(reader->parse(doc.data(), doc.data() + doc.size(), &root, &errs));
	return root;
}

} // namespace

TEST_CASE("a channel with a logo answers where it is", "[logos]")
{
	InstalledDependencies deps;
	deps.channels.channels.push_back(channel(0xb544000800850088ULL, "Sky"));
	FakeLogoSource lg;
	InstalledLogoSource in(&lg);
	lg.put(0xb544000800850088ULL, "/var/logo/800850088.png");

	const Result<std::string> r = channels::logo(0xb544000800850088ULL);
	REQUIRE(r.ok());
	REQUIRE(r.value() == "/var/logo/800850088.png");
	// The name the box has for the channel reaches the search, which is half
	// of what the search goes by and is not derivable from the identifier.
	REQUIRE(lg.last_name == "Sky");
}

TEST_CASE("a channel with none is not found, and that is not a fault", "[logos]")
{
	/* Green without a seam whether or not anything searches, which is why the
	   count below is here: with it the case is red the moment the search stops
	   being asked for. */
	InstalledDependencies deps;
	FakeLogoSource lg;
	InstalledLogoSource in(&lg);
	// Something is there to find, so an empty search cannot be what answers.
	lg.put(1, "/var/logo/1.png");

	const Result<std::string> r = channels::logo(2);
	REQUIRE_FALSE(r.ok());
	REQUIRE(r.error().status == Status::NotFound);
	REQUIRE(r.error().code == ErrorCode::NoSuchLogo);
	REQUIRE(lg.searches == 1u);
}

TEST_CASE("a symbolic link is answered with what it points at", "[logos]")
{
	Fixture fx;
	const std::string target = fx.write("real.png", "x");
	const std::string link = fx.link("used.png", target);

	InstalledDependencies deps;
	deps.channels.channels.push_back(channel(1, "One"));
	FakeLogoSource lg;
	InstalledLogoSource in(&lg);
	lg.put(1, link);

	const LogoList l = channels::logos(true, true).value();
	REQUIRE(l.size() == 1u);
	REQUIRE(l[0].path == link);
	REQUIRE(l[0].resolved == target);
}

TEST_CASE("a plain file resolves to nothing rather than to itself", "[logos]")
{
	// The second field is cleared when the two are the same, because a caller
	// reading two identical paths would think it had been handed a link.
	Fixture fx;
	const std::string plain = fx.write("plain.png", "x");

	InstalledDependencies deps;
	deps.channels.channels.push_back(channel(1, "One"));
	FakeLogoSource lg;
	InstalledLogoSource in(&lg);
	lg.put(1, plain);

	const LogoList l = channels::logos(true, true).value();
	REQUIRE(l.size() == 1u);
	REQUIRE(l[0].path == plain);
	REQUIRE(l[0].resolved.empty());
}

TEST_CASE("without files nothing on the filesystem is touched", "[logos]")
{
	/* What the parameter is for. The walk of the picture directories per
	   channel is what makes the listing expensive, and a listing that did it
	   whatever it was asked would be a parameter that means nothing. */
	InstalledDependencies deps;
	deps.channels.channels.push_back(channel(1, "One"));
	FakeLogoSource lg;
	InstalledLogoSource in(&lg);
	lg.put(1, "/var/logo/1.png");

	const LogoList l = channels::logos(true, false).value();
	REQUIRE(l.size() == 1u);
	REQUIRE(l[0].path.empty());
	REQUIRE(l[0].resolved.empty());
	REQUIRE(lg.searches == 0u);
}

TEST_CASE("the short id is the lower forty eight bits", "[logos]")
{
	InstalledDependencies deps;
	deps.channels.channels.push_back(channel(0xb544000800850088ULL, "Sky"));
	FakeLogoSource lg;
	InstalledLogoSource in(&lg);

	const LogoList l = channels::logos(true, false).value();
	REQUIRE(l.size() == 1u);
	REQUIRE(l[0].id == 0xb544000800850088ULL);
	REQUIRE(l[0].short_id == 0x000800850088ULL);
	REQUIRE(l[0].name == "Sky");
}

TEST_CASE("a logo arrives as the file it is", "[logos]")
{
	Fixture fx;
	const std::string p = fx.write("1.png", pngBytes());

	Serving serving;
	REQUIRE(serving.port > 0);
	serving.wired.logos.put(1, p);

	const testhttp::Reply r =
		testhttp::request(serving.port, "GET", "/api/v1/channels/1/logo");
	REQUIRE(r.transport_ok);
	REQUIRE(r.code == 200);
	REQUIRE(r.header("Content-Type") == "image/png");
	REQUIRE(r.body == pngBytes());
	// Chunked would mean the client above could not have read it: it decodes
	// none, so an answer sent that way is one no case here can say anything
	// about.
	REQUIRE(r.header("Transfer-Encoding").empty());
}

TEST_CASE("the file is still there afterwards", "[logos]")
{
	/* The difference from the picture the box takes of its own screen and from
	   the guide it writes out, which are the request's own files and go away
	   with it. A picture of a channel belongs to the box. */
	Fixture fx;
	const std::string p = fx.write("1.png", pngBytes());

	Serving serving;
	REQUIRE(serving.port > 0);
	serving.wired.logos.put(1, p);

	REQUIRE(testhttp::request(serving.port, "GET", "/api/v1/channels/1/logo").code == 200);
	REQUIRE(::access(p.c_str(), R_OK) == 0);
	REQUIRE(testhttp::request(serving.port, "GET", "/api/v1/channels/1/logo").code == 200);
}

TEST_CASE("an extension nobody knows is not called a picture", "[logos]")
{
	Fixture fx;
	const std::string p = fx.write("1.bin", "xx");

	Serving serving;
	REQUIRE(serving.port > 0);
	serving.wired.logos.put(1, p);

	const testhttp::Reply r =
		testhttp::request(serving.port, "GET", "/api/v1/channels/1/logo");
	REQUIRE(r.code == 200);
	REQUIRE(r.header("Content-Type") == "application/octet-stream");
	REQUIRE(r.body == "xx");
}

TEST_CASE("a channel with no logo is not found", "[logos]")
{
	Serving serving;
	REQUIRE(serving.port > 0);
	serving.wired.logos.put(1, "/does/not/exist.png");

	const testhttp::Reply r =
		testhttp::request(serving.port, "GET", "/api/v1/channels/2/logo");
	REQUIRE(r.transport_ok);
	REQUIRE(r.code == 404);
	REQUIRE(parsed(r.body)["type"].asString() == "/errors/no-such-logo");
}

TEST_CASE("a server set not to hand pictures over hands none over", "[logos]")
{
	/* And to the caller the switch was written for, which is the one this used
	   to miss: a reader on the home network who presented nothing. The route
	   that answers how the server is set up is System, so that caller cannot
	   learn the switch and would draw the pictures anyway, and the refusal has
	   to be here or it holds for the owner alone.

	   The request goes over a socket from the loopback and presents nothing,
	   which is what the seeded networks grant a read, so what is being driven
	   is that caller and not a level a case handed in. */
	Fixture fx;
	const std::string p = fx.write("1.png", pngBytes());

	Serving serving;
	REQUIRE(serving.port > 0);
	serving.wired.logos.put(1, p);

	// The picture is there and is handed over while the switch says so, so the
	// case below is about the switch and not about a box with no picture.
	REQUIRE(testhttp::request(serving.port, "GET", "/api/v1/channels/1/logo").code == 200);

	NoLogos off;

	const testhttp::Reply r =
		testhttp::request(serving.port, "GET", "/api/v1/channels/1/logo");
	REQUIRE(r.transport_ok);
	REQUIRE(r.code == 404);
	// None of the file went out.
	REQUIRE(r.body.find(pngBytes()) == std::string::npos);
	/* Its own code beside the shared status: a page meets this where it meets a
	   channel with no picture and does the same thing about both, and whoever
	   is wondering where the pictures went is told it is a line of a file and
	   not a file that has gone. */
	REQUIRE(parsed(r.body)["type"].asString() == "/errors/logos-not-offered");

	// And the file the box named is still there: nothing was removed, only not
	// handed over.
	REQUIRE(::access(p.c_str(), R_OK) == 0);
}

TEST_CASE("a channel with no picture is still told from a server that offers none", "[logos]")
{
	// One status for both, which is what a page wants, and two codes, which is
	// what anybody asking why wants.
	Fixture fx;

	Serving serving;
	REQUIRE(serving.port > 0);
	serving.wired.logos.put(1, fx.write("1.png", pngBytes()));

	const testhttp::Reply absent =
		testhttp::request(serving.port, "GET", "/api/v1/channels/2/logo");
	REQUIRE(absent.code == 404);
	REQUIRE(parsed(absent.body)["type"].asString() == "/errors/no-such-logo");

	NoLogos off;

	const testhttp::Reply refused =
		testhttp::request(serving.port, "GET", "/api/v1/channels/2/logo");
	REQUIRE(refused.code == absent.code);
	REQUIRE(parsed(refused.body)["type"].asString() == "/errors/logos-not-offered");
}

TEST_CASE("the listing goes on saying what the box has", "[logos]")
{
	/* The other route that answers about the same pictures, and the switch is
	   deliberately not read there: it hands over no picture, and emptying what
	   it says would read as a box with no pictures where the truth is a server
	   that will not hand them over. */
	Fixture fx;
	const std::string p = fx.write("1.png", pngBytes());

	ShippedRoutes shipped;
	InstalledDependencies deps;
	deps.channels.channels.push_back(channel(1, "One"));
	deps.logos.put(1, p);

	NoLogos off;

	const httpd::Response r = httpd::dispatch(httpd::Get, "/api/v1/channels/logos",
	                                          "files=1", "", "127.0.0.1",
	                                          httpd::AuthLevel::Read);
	REQUIRE(r.code == 200);

	const ::Json::Value items = parsed(r.body)["items"];
	REQUIRE(items.size() == 1u);
	REQUIRE(items[0]["path"].asString() == p);
}

TEST_CASE("a picture in a text format goes out as the file it is", "[logos]")
{
	/* Measured and not assumed. The one answer this server keeps a second,
	   compressed form of is picked on the way out by a header, and a picture
	   written as text is the first thing here a reader would expect that to
	   reach. The second form is built by copying the head of an answer, so an
	   answer that is an open file would come out of it with nothing to send. */
	Fixture fx;
	const std::string drawing = "<svg xmlns=\"http://www.w3.org/2000/svg\"></svg>";
	const std::string p = fx.write("1.svg", drawing);

	Serving serving;
	REQUIRE(serving.port > 0);
	serving.wired.logos.put(1, p);

	std::vector<std::pair<std::string, std::string> > head;
	head.push_back(std::make_pair(std::string("Accept-Encoding"),
	                              std::string("gzip, deflate")));

	const testhttp::Reply r =
		testhttp::request(serving.port, "GET", "/api/v1/channels/1/logo", head);
	REQUIRE(r.transport_ok);
	REQUIRE(r.code == 200);
	REQUIRE(r.header("Content-Type") == "image/svg+xml");
	REQUIRE(r.header("Content-Encoding").empty());
	REQUIRE(r.body == drawing);

	char stated[32];
	std::snprintf(stated, sizeof(stated), "%u", (unsigned) drawing.size());
	REQUIRE(r.header("Content-Length") == stated);
}

TEST_CASE("a thousand logos leak no descriptors", "[logos]")
{
	/* The answer is sent out of an open file, and every way out of the transport has to
	   give that file back. A thousand of them is what finds a way out that does not.
	   The count is of descriptors on this one file and not of descriptors, so a
	   connection on its way out cannot read as a leak, and it is shown to find the one
	   this case holds before finding none means anything. */
	Fixture fx;
	const std::string p = fx.write("1.png", pngBytes());

	Serving serving;
	REQUIRE(serving.port > 0);
	serving.wired.logos.put(1, p);

	const int held = ::open(p.c_str(), O_RDONLY | O_CLOEXEC);
	REQUIRE(held >= 0);

	const size_t before = openCountFor(p);
	REQUIRE(before == 1u);

	size_t answered = 0;
	for (int i = 0; i < 1000; ++i)
	{
		const testhttp::Reply r =
			testhttp::request(serving.port, "GET", "/api/v1/channels/1/logo");
		if (r.transport_ok && r.code == 200 && r.body == pngBytes())
			++answered;
	}
	REQUIRE(answered == 1000u);

	const size_t after = waitForCount(p, before, 5000);
	::close(held);
	REQUIRE(after == before);
}

TEST_CASE("the listing names both identifiers and says where the file is", "[logos]")
{
	Fixture fx;
	const std::string target = fx.write("real.png", pngBytes());
	const std::string used = fx.link("used.png", target);

	ShippedRoutes shipped;
	InstalledDependencies deps;
	deps.channels.channels.push_back(channel(0xb544000800850088ULL, "Sky"));
	deps.logos.put(0xb544000800850088ULL, used);

	const httpd::Response r = httpd::dispatch(httpd::Get, "/api/v1/channels/logos",
	                                          "files=1", "", "127.0.0.1",
	                                          httpd::AuthLevel::Read);
	REQUIRE(r.code == 200);

	const ::Json::Value doc = parsed(r.body);
	REQUIRE(doc["items"].size() == 1u);
	REQUIRE(doc["items"][0]["id"].asString() == "b544000800850088");
	REQUIRE(doc["items"][0]["short_id"].asString() == "800850088");
	REQUIRE(doc["items"][0]["name"].asString() == "Sky");
	REQUIRE(doc["items"][0]["path"].asString() == used);
	REQUIRE(doc["items"][0]["resolved"].asString() == target);
}

TEST_CASE("a listing that was not asked to look does not", "[logos]")
{
	ShippedRoutes shipped;
	InstalledDependencies deps;
	deps.channels.channels.push_back(channel(1, "One"));
	deps.logos.put(1, "/var/logo/1.png");

	const httpd::Response r = httpd::dispatch(httpd::Get, "/api/v1/channels/logos",
	                                          "", "", "127.0.0.1",
	                                          httpd::AuthLevel::Read);
	REQUIRE(r.code == 200);

	const ::Json::Value doc = parsed(r.body);
	REQUIRE(doc["items"].size() == 1u);
	REQUIRE(doc["items"][0]["path"].asString().empty());
	REQUIRE(deps.logos.searches == 0u);
}
