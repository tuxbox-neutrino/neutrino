/*
 * test_openapi_basic.cpp - the document of the first slice on nhttpd
 *
 * Copyright (C) 2026 Thilo Graf
 *
 * License: GPL-2.0-or-later
 *
 * What test_openapi.cpp checks against the source's full table set is checked
 * here against the tables this tree ships. The Basic interim case this file
 * carried until the session layer arrived is gone with the interim: the
 * imported cases for the schemes and the second token run again, and what
 * stays here is the part the source's own cases cannot state for this tree -
 * the group order with the tables still out, and the one whole-body route.
 */

#include "support/catch.hpp"

#include "httpd/doc/openapi.h"
#include "httpd/endpoint.h"
#include "httpd/router.h"

#include "jsoncpp/json/json.h"

#include <set>
#include <string>
#include <algorithm>
#include <vector>

namespace {

Json::Value parsed(const std::string &text)
{
	Json::Value doc;
	Json::Reader reader;
	REQUIRE(reader.parse(text, doc));
	return doc;
}

Json::Value shipped()
{
	return parsed(httpd::openapi::document());
}

std::vector<std::string> keysOf(const Json::Value &v)
{
	std::vector<std::string> out = v.getMemberNames();
	return out;
}

} // namespace

TEST_CASE("the groups of this slice are declared in the order somebody meets this box in", "[openapi-basic]")
{
	/* The order the generator states (doc/openapi.cpp, the list above the
	   tags: sign in, see what is on, change what the box does, the document
	   last), for the groups this tree serves. */
	static const char *const meant[] = {
		"auth", "channels", "epg", "timers", "recordings", "playback", "osd",
		"settings", "system", "webserver", "webui", "network", "daemons", "storage", "netfs",
		"tuner", "plugins", "stream", "webtv", "config", "events", "openapi",
	};
	const size_t count = sizeof(meant) / sizeof(meant[0]);

	const Json::Value doc = shipped();
	const Json::Value tags = doc["tags"];
	REQUIRE(tags.isArray());
	REQUIRE(tags.size() == count);
	for (size_t i = 0; i < count; ++i)
		CHECK(tags[(Json::ArrayIndex) i]["name"].asString() == meant[i]);
}

TEST_CASE("the routes of this slice that take a whole list as their body say so", "[openapi-basic]")
{
	const Json::Value doc = shipped();
	std::vector<std::string> where;
	const Json::Value paths = doc["paths"];
	for (Json::ValueConstIterator p = paths.begin(); p != paths.end(); ++p)
		for (Json::ValueConstIterator op = p->begin(); op != p->end(); ++op)
			if (op->isMember("requestBody"))
			{
				const Json::Value schema = (*op)["requestBody"]["content"]["application/json"]["schema"];
				if (schema["type"].asString() == "array")
					where.push_back(p.name());
			}
	std::sort(where.begin(), where.end());
	REQUIRE(where.size() == 3);
	CHECK(where[0] == "/api/v1/bouquets/{bouquet}/channels");
	CHECK(where[1] == "/api/v1/channels/sources/{kind}");
	CHECK(where[2] == "/api/v1/webui/extensions/{dir}/scripts/{name}");
}

TEST_CASE("a build without the prose keeps every path and every shape of this slice", "[openapi-basic]")
{
	size_t count = 0;
	const httpd::RouteTable *const *tables = httpd::allRoutes(&count);

	std::string prose, bare;
	httpd::openapi::appendDocument(prose, tables, count, true);
	httpd::openapi::appendDocument(bare, tables, count, false);
	const Json::Value a = parsed(prose);
	const Json::Value b = parsed(bare);

	CHECK(keysOf(a["paths"]) == keysOf(b["paths"]));
	CHECK(keysOf(a["components"]["schemas"]) == keysOf(b["components"]["schemas"]));
	CHECK(a["paths"].size() >= 25);
	CHECK(bare.size() < prose.size());
}

TEST_CASE("every reference of this slice's document points at a shape it carries", "[openapi-basic]")
{
	const std::string text = httpd::openapi::document();
	const Json::Value doc = parsed(text);
	const Json::Value schemas = doc["components"]["schemas"];
	std::set<std::string> pointed;
	const std::string marker = "\"$ref\":\"#/components/schemas/";
	for (size_t at = text.find(marker); at != std::string::npos; at = text.find(marker, at + 1))
	{
		const size_t from = at + marker.size();
		const size_t to = text.find('"', from);
		REQUIRE(to != std::string::npos);
		const std::string name = text.substr(from, to - from);
		CHECK(schemas.isMember(name));
		pointed.insert(name);
	}
	CHECK(pointed.size() > 10);
	CHECK(pointed.size() <= schemas.size());
}
