/*
 * test_openapi_basic.cpp - the document of the first slice on nhttpd
 *
 * Copyright (C) 2026 Thilo Graf
 *
 * License: GPL-2.0-or-later
 *
 * What test_openapi.cpp checks against the source's full table set is checked
 * here against the three tables this tree ships, and the auth the document
 * has to state until the session layer arrives: nhttpd's Basic, on every
 * route above Public, with no second token anywhere. The excluded cases of
 * the imported test are listed in support/run-tests.sh; each assertion here
 * is the part of one of them that holds for this slice.
 */

#include "support/catch.hpp"

#include "httpd/doc/openapi.h"
#include "httpd/endpoint.h"
#include "httpd/router.h"

#include "jsoncpp/json/json.h"

#include <set>
#include <string>
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

TEST_CASE("the document names nhttpd's Basic and nothing a caller cannot present", "[openapi-basic]")
{
	const Json::Value doc = shipped();
	const Json::Value schemes = doc["components"]["securitySchemes"];
	REQUIRE(schemes.isObject());
	REQUIRE(schemes.getMemberNames().size() == 1);
	REQUIRE(schemes.isMember("basic"));
	CHECK(schemes["basic"]["type"].asString() == "http");
	CHECK(schemes["basic"]["scheme"].asString() == "basic");
	CHECK(schemes["basic"]["description"].asString().find("mod_auth") != std::string::npos);

	const Json::Value paths = doc["paths"];
	size_t guarded = 0, open = 0;
	for (Json::ValueConstIterator p = paths.begin(); p != paths.end(); ++p)
	{
		for (Json::ValueConstIterator op = p->begin(); op != p->end(); ++op)
		{
			CHECK_FALSE(op->isMember("x-csrf-header"));
			const Json::Value params = (*op)["parameters"];
			for (Json::ArrayIndex i = 0; i < params.size(); i++)
				CHECK(params[i]["name"].asString() != "X-CSRF-Token");

			const Json::Value sec = (*op)["security"];
			REQUIRE(sec.isArray());
			if ((*op)["x-auth-level"].asString() == "public")
			{
				CHECK(sec.size() == 0);
				open++;
				continue;
			}
			REQUIRE(sec.size() == 1);
			REQUIRE(sec[0].isMember("basic"));
			guarded++;
		}
	}
	CHECK(guarded > 0);
	CHECK(open == 0);
}

TEST_CASE("the groups of this slice are declared in the order somebody meets this box in", "[openapi-basic]")
{
	/* The order the generator states (doc/openapi.cpp, the list above the
	   tags: sign in, see what is on, change what the box does, the document
	   last), with the five groups this tree does not serve yet left out:
	   auth, events, stream, webtv and webserver. The source's own case for the
	   full order comes back with the last of them. */
	static const char *const meant[] = {
		"channels", "epg", "timers", "recordings", "osd", "settings", "system",
		"daemons", "storage", "netfs", "tuner", "plugins", "config", "openapi",
	};
	const size_t count = sizeof(meant) / sizeof(meant[0]);

	const Json::Value doc = shipped();
	const Json::Value tags = doc["tags"];
	REQUIRE(tags.isArray());
	REQUIRE(tags.size() == count);
	for (size_t i = 0; i < count; ++i)
		CHECK(tags[(Json::ArrayIndex) i]["name"].asString() == meant[i]);
}

TEST_CASE("the one route of this slice that takes a whole body says so", "[openapi-basic]")
{
	const Json::Value doc = shipped();
	size_t whole = 0;
	std::string where;
	const Json::Value paths = doc["paths"];
	for (Json::ValueConstIterator p = paths.begin(); p != paths.end(); ++p)
		for (Json::ValueConstIterator op = p->begin(); op != p->end(); ++op)
			if (op->isMember("requestBody"))
			{
				const Json::Value schema = (*op)["requestBody"]["content"]["application/json"]["schema"];
				if (schema["type"].asString() == "array")
				{
					whole++;
					where = p.name();
				}
			}
	CHECK(whole == 1);
	CHECK(where == "/api/v1/bouquets/{bouquet}/channels");
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
