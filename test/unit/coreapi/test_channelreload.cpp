/*
 * test_channelreload.cpp - tests for reloading the channel lists
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

#include "coreapi/channels.h"
#include "coreapi/base/deps.h"
#include "coreapi/base/errors.h"
#include "coreapi/base/result.h"
#include "coreapi/base/types.h"

#include "jsoncpp/json/json.h"

#include <memory>
#include <string>
#include <vector>

#include <stdint.h>

using namespace coreapi;

namespace
{

std::vector<uint16_t> caids(uint16_t a)
{
	std::vector<uint16_t> out;
	out.push_back(a);
	return out;
}

std::vector<uint16_t> caids(uint16_t a, uint16_t b)
{
	std::vector<uint16_t> out;
	out.push_back(a);
	out.push_back(b);
	return out;
}

// The same tables a request off a socket is answered from, and never one a
// case installed.
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

TEST_CASE("the running channel answers what it is scrambled under", "[crypt]")
{
	InstalledDependencies deps;
	deps.channels.setCurrentCaids(caids(0x0622, 0x1801));

	const CaidList c = channels::currentCaids().value();
	REQUIRE(c.size() == 2u);
	// The number the stream carries and the name this program has for it, side
	// by side, so that neither has to be got out of the other.
	REQUIRE(c[0].caid == 0x0622);
	REQUIRE(c[0].system == "Irdeto");
	REQUIRE(c[1].caid == 0x1801);
	REQUIRE(c[1].system == "Nagra");
}

TEST_CASE("a free channel answers an empty list and not a fault", "[crypt]")
{
	/* Stated because nothing is scrambling this and nothing is playing are two
	   answers, and a caller comes back for only one of them. */
	InstalledDependencies deps;
	deps.channels.setCurrentCaids(std::vector<uint16_t>());

	const Result<CaidList> r = channels::currentCaids();
	REQUIRE(r.ok());
	REQUIRE(r.value().empty());
}

TEST_CASE("an idle box is not found", "[crypt]")
{
	InstalledDependencies deps;
	deps.channels.current_status = Status::NotFound;

	const Result<CaidList> r = channels::currentCaids();
	REQUIRE_FALSE(r.ok());
	REQUIRE(r.error().status == Status::NotFound);
	REQUIRE(r.error().code == ErrorCode::NoRunningChannel);
}

TEST_CASE("an identifier nobody has a name for keeps its number", "[crypt]")
{
	// The table holds thirteen systems, and a stream carrying a fourteenth
	// must not lose the one thing about it that is certain.
	InstalledDependencies deps;
	deps.channels.setCurrentCaids(caids(0x3F00));

	const CaidList c = channels::currentCaids().value();
	REQUIRE(c.size() == 1u);
	REQUIRE(c[0].caid == 0x3F00);
	REQUIRE(c[0].system.empty());
}

TEST_CASE("a soft reload writes the services out first and a hard one does not",
          "[channelreload]")
{
	InstalledDependencies deps;

	REQUIRE(channels::reloadChannels(false).ok());
	REQUIRE(deps.channels.services_saved == 1u);
	REQUIRE(deps.channels.reinits == 1u);
	REQUIRE(deps.channels.reload_reason == "webif_reload");

	REQUIRE(channels::reloadChannels(true).ok());
	// Unchanged, which is the whole of what the hard reading is: the file on
	// the disc wins because nothing was written over it first.
	REQUIRE(deps.channels.services_saved == 1u);
	REQUIRE(deps.channels.reinits == 2u);
	REQUIRE(deps.channels.reload_reason == "webif_hardreload");
}

TEST_CASE("the crypt route answers the number beside the name", "[crypt]")
{
	/* The shape that changed on purpose. The copied endpoint printed the two
	   into one line, so a caller that wanted the number had to take a string
	   apart to get at it and a caller that wanted the name was bound to a
	   table that lives at one place in this program. */
	ShippedRoutes shipped;
	InstalledDependencies deps;
	deps.channels.setCurrentCaids(caids(0x0622, 0x3F00));

	const httpd::Response r =
		httpd::dispatch(httpd::Get, "/api/v1/channels/current/crypt", "", "",
		                "127.0.0.1", httpd::AuthLevel::Read);
	REQUIRE(r.code == 200);

	const ::Json::Value doc = parsed(r.body);
	REQUIRE(doc["items"].size() == 2u);
	REQUIRE(doc["items"][0]["caid"].asUInt() == 0x0622u);
	REQUIRE(doc["items"][0]["system"].asString() == "Irdeto");
	REQUIRE(doc["items"][1]["caid"].asUInt() == 0x3F00u);
	REQUIRE(doc["items"][1]["system"].asString().empty());
}

TEST_CASE("an idle box answers the crypt route with a refusal that names why", "[crypt]")
{
	ShippedRoutes shipped;
	InstalledDependencies deps;
	deps.channels.current_status = Status::NotFound;

	const httpd::Response r =
		httpd::dispatch(httpd::Get, "/api/v1/channels/current/crypt", "", "",
		                "127.0.0.1", httpd::AuthLevel::Read);
	REQUIRE(r.code == 404);
	REQUIRE(parsed(r.body)["type"].asString() == "/errors/no-running-channel");
}

TEST_CASE("the reload route carries which of the two readings was meant",
          "[channelreload]")
{
	ShippedRoutes shipped;
	InstalledDependencies deps;

	const httpd::Response soft =
		httpd::dispatch(httpd::Post, "/api/v1/channels/reload", "", "{}",
		                "127.0.0.1", httpd::AuthLevel::Write);
	REQUIRE(soft.code == 202);
	REQUIRE(deps.channels.services_saved == 1u);
	REQUIRE(deps.channels.reinits == 1u);

	const httpd::Response hard =
		httpd::dispatch(httpd::Post, "/api/v1/channels/reload", "",
		                "{\"hard\":true}", "127.0.0.1", httpd::AuthLevel::Write);
	REQUIRE(hard.code == 202);
	// Unchanged, which is what the flag is for: nothing was written over the
	// file before it was read back.
	REQUIRE(deps.channels.services_saved == 1u);
	REQUIRE(deps.channels.reinits == 2u);
}
