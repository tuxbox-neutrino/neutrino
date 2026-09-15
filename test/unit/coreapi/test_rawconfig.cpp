/*
 * test_rawconfig.cpp - tests for the raw configuration routes
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

#include "httpd/endpoint.h"
#include "httpd/http.h"
#include "httpd/router.h"
#include "httpd/status.h"

#include "coreapi/base/errors.h"
#include "coreapi/base/result.h"
#include "coreapi/system.h"

#include <neutrinoMessages.h>

#include <cstdio>
#include <cstdlib>
#include <string>

#include <fcntl.h>
#include <unistd.h>

using namespace httpd;

namespace
{

/* Cleared going in as well as coming out, so a case elsewhere that failed and
   unwound past its own restore cannot decide what these read. */
struct ShippedRoutes
{
	ShippedRoutes() { setRoutesForTest(NULL); }
	~ShippedRoutes() { setRoutesForTest(NULL); }

	private:
		ShippedRoutes(const ShippedRoutes &);
		ShippedRoutes &operator=(const ShippedRoutes &);
};

/* A directory standing in for the one the image keeps these documents in, and
   the configuration root put back the way it was found.

   Without a root a case can set, every case here would be run against
   ZAPITDIR, which is a name no host running these has: one expecting an answer
   could never pass and one expecting nothing there could never fail. */
struct Fixture
{
	std::string dir;
	std::string root_before;

	Fixture()
	{
		char tmpl[] = "/tmp/coreapi_rawconfig_XXXXXX";
		if (mkdtemp(tmpl) != NULL)
			dir = tmpl;
		root_before = coreapi::config::configRoot();
		coreapi::config::setConfigRoot(dir);
	}

	~Fixture()
	{
		coreapi::config::setConfigRoot(root_before);
		if (!dir.empty())
			(void) system(("rm -rf " + dir).c_str());
	}

	void write(const char *name, const std::string &content) const
	{
		const std::string path = dir + "/" + name;
		FILE *f = fopen(path.c_str(), "w");
		REQUIRE(f != NULL);
		REQUIRE(fwrite(content.data(), 1, content.size(), f) == content.size());
		fclose(f);
	}

	private:
		Fixture(const Fixture &);
		Fixture &operator=(const Fixture &);
};

// The answer and the bytes behind it. A document is handed over as a
// descriptor, so what a case has to compare is what comes off that descriptor
// and not the response body, which carries none of it.
struct Fetched
{
	int         code;
	std::string content_type;
	std::string content;
	std::string problem;
	bool        by_descriptor;
	uint64_t    length;

	Fetched() : code(0), by_descriptor(false), length(0) {}
};

Fetched fetch(const std::string &document)
{
	Response r = dispatch(Get, "/api/v1/config/" + document, "", "",
			      "127.0.0.1", AuthLevel::Read);

	Fetched out;
	out.code = r.code;
	out.content_type = r.content_type;
	out.problem = r.body;
	out.length = r.length;
	out.by_descriptor = (r.fd >= 0);

	if (r.fd >= 0)
	{
		char buf[8192];
		ssize_t n;
		while ((n = ::read(r.fd, buf, sizeof(buf))) > 0)
			out.content.append(buf, (size_t) n);
		::close(r.fd);
	}
	return out;
}

} // namespace

TEST_CASE("a document the box has arrives whole", "[rawconfig]")
{
	ShippedRoutes shipped;
	Fixture fx;
	fx.write("services.xml", "<ZAPIT/>\n");

	const Fetched r = fetch("services");
	REQUIRE(r.code == StatusOk);
	REQUIRE(r.content == "<ZAPIT/>\n");
	REQUIRE(r.content_type == "application/xml");

	/* Out of the file and not through a body. An answer of unknown length goes
	   out chunked, and the length stated off the descriptor is what keeps this
	   one from being one. */
	REQUIRE(r.by_descriptor);
	REQUIRE(r.problem.empty());
	REQUIRE(r.length == r.content.size());
}

TEST_CASE("a document the box does not have is not found", "[rawconfig]")
{
	/* Only says something because the case above proves one can be found in
	   the same root. Without that pair this is the green-for-any-reason case. */
	ShippedRoutes shipped;
	Fixture fx;
	fx.write("services.xml", "<ZAPIT/>\n");

	REQUIRE(fetch("services").code == StatusOk);

	const Fetched missing = fetch("bouquets");
	REQUIRE(missing.code == StatusNotFound);
	REQUIRE(missing.problem.find("config-document-unreadable") != std::string::npos);
}

TEST_CASE("each of the three names stands for a document of its own", "[rawconfig]")
{
	// One file per name, each saying which it is: a route that answered every
	// name out of one document would pass every case above and fail here.
	ShippedRoutes shipped;
	Fixture fx;
	fx.write("services.xml", "<services/>\n");
	fx.write("bouquets.xml", "<bouquets/>\n");
	fx.write("ubouquets.xml", "<ubouquets/>\n");

	REQUIRE(fetch("services").content == "<services/>\n");
	REQUIRE(fetch("bouquets").content == "<bouquets/>\n");
	REQUIRE(fetch("ubouquets").content == "<ubouquets/>\n");
}

TEST_CASE("a name the enumeration does not carry never reaches a handler",
          "[rawconfig]")
{
	ShippedRoutes shipped;
	Fixture fx;
	fx.write("neutrino.conf", "secret=1\n");

	/* Refused where the parameter is checked and not where the document is
	   looked up: bad-enum is the router's own refusal and no handler of this
	   server writes it, so an answer carrying it is an answer given before a
	   handler was entered. */
	const Fetched named = fetch("neutrino");
	REQUIRE(named.code == StatusBadRequest);
	REQUIRE(named.problem.find("bad-enum") != std::string::npos);
	REQUIRE_FALSE(named.by_descriptor);
	REQUIRE(named.content.empty());

	// One segment and not three, because a request is split before anything in
	// it is decoded, so this reaches the row that declares the segment.
	const Fetched walked = fetch("..%2F..%2Fetc%2Fpasswd");
	REQUIRE(walked.code == StatusBadRequest);
	REQUIRE(walked.problem.find("bad-enum") != std::string::npos);
	REQUIRE_FALSE(walked.by_descriptor);
}

TEST_CASE("the root a caller sets is the one the documents are looked for under",
          "[rawconfig]")
{
	const std::string before = coreapi::config::configRoot();

	coreapi::config::setConfigRoot("/opt/zapit/");
	REQUIRE(coreapi::config::configRoot() == "/opt/zapit");
	REQUIRE(coreapi::config::pathOf(coreapi::config::Document::Services).value()
		== "/opt/zapit/services.xml");
	REQUIRE(coreapi::config::pathOf(coreapi::config::Document::Bouquets).value()
		== "/opt/zapit/bouquets.xml");
	REQUIRE(coreapi::config::pathOf(coreapi::config::Document::UserBouquets).value()
		== "/opt/zapit/ubouquets.xml");

	coreapi::config::setConfigRoot("/opt/zapit///");
	REQUIRE(coreapi::config::configRoot() == "/opt/zapit");

	// The whole of the root is a separator, and dropping it would leave
	// nothing for the tail to be joined to.
	coreapi::config::setConfigRoot("/");
	REQUIRE(coreapi::config::configRoot() == "/");

	coreapi::config::setConfigRoot(before);
	REQUIRE(coreapi::config::configRoot() == before);
}

TEST_CASE("a document that is none of the three names no path", "[rawconfig]")
{
	// The only way past the three, which is a number cast into the
	// enumeration. It answers as no document rather than as the first one.
	const coreapi::Result<std::string> none =
		coreapi::config::pathOf((coreapi::config::Document) 99);
	REQUIRE_FALSE(none.ok());
	REQUIRE(none.error().status == coreapi::Status::InvalidArgument);
}

TEST_CASE("asking the box to read its settings again sends the one command",
          "[rawconfig]")
{
	InstalledDependencies deps;

	REQUIRE(coreapi::system::reloadSetup().ok());
	REQUIRE(deps.events.sent.size() == 1);
	REQUIRE(deps.events.sent[0].id == (unsigned) NeutrinoMessages::RELOAD_SETUP);
	REQUIRE(deps.events.sent[0].body.empty());
}

TEST_CASE("the route that asks for it says it was taken and nothing more",
          "[rawconfig]")
{
	/* Over the route and not over the call below it, because what a case
	   driving the call alone cannot say is that the route reaches that call and
	   not another one. */
	ShippedRoutes shipped;
	InstalledDependencies deps;

	const Response r = dispatch(Post, "/api/v1/system/reload-setup", "", "",
				    "127.0.0.1", AuthLevel::Write);
	REQUIRE(r.code == StatusAccepted);
	REQUIRE(r.body.empty());
	REQUIRE(deps.events.sent.size() == 1);
	REQUIRE(deps.events.sent[0].id == (unsigned) NeutrinoMessages::RELOAD_SETUP);
}
