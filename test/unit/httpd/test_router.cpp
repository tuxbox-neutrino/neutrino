/*
 * test_router.cpp - tests for route matching
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
#include "httpd/router.h"
#include "httpd/endpoint.h"

#include <cstddef>

#include <string>
#include <vector>

using namespace httpd;

static bool handler_ran = false;
static uint64_t seen_id = 0;

/* Reset by every case and never by the router, so a case that expects no handler is
   answering for this run and not for what the run before it left behind. handler_calls
   is the other half of that and is never reset: a case that forgot to clear handler_ran
   would still be caught by a count that only ever goes up. */
static int handler_calls = 0;

static Response probe(const Request &r)
{
	handler_ran = true;
	handler_calls++;
	seen_id = r.has("id") ? r.asChannelId("id") : 0;
	Response out;
	out.code = 200;
	out.body = "{}";
	out.content_type = "application/json";
	return out;
}

static const Param probe_params[] = {
	HTTPD_SEGMENT("id", ParamType::ChannelId, "channel id"),
	HTTPD_QUERY_IN("limit", ParamType::Int, "page size", 1, 500),
	HTTPD_QUERY_FROM_SET("mode", "list mode", "tv,radio"),
};

static const Endpoint probe_endpoints[] = {
	{ Method::Get, "/api/v1/probe/{id}", AuthLevel::Read, "probe",
	  HTTPD_PARAMS(probe_params), NULL, &probe, false },
};

static const RouteTable probe_table = { HTTPD_TABLE_N("probe", probe_endpoints, 1) };

TEST_CASE("a path that matches nothing is 404 and no handler runs", "[router]")
{
	handler_ran = false;
	const int before = handler_calls;
	Response r = dispatchIn(probe_table, Method::Get, "/api/v1/nope", "", "", "127.0.0.1", AuthLevel::System);
	REQUIRE(r.code == 404);
	REQUIRE_FALSE(handler_ran);
	REQUIRE(handler_calls == before);
}

TEST_CASE("the right path with the wrong method is 405 and names what is allowed", "[router]")
{
	handler_ran = false;
	const int before = handler_calls;
	Response r = dispatchIn(probe_table, Method::Post, "/api/v1/probe/2b66", "", "", "127.0.0.1", AuthLevel::System);
	REQUIRE(r.code == 405);
	REQUIRE_FALSE(handler_ran);
	REQUIRE(handler_calls == before);
	REQUIRE(r.body.find("no-such-method") != std::string::npos);
	bool allow = false;
	for (size_t i = 0; i < r.headers.size(); ++i)
		if (r.headers[i].first == "Allow" && r.headers[i].second.find("GET") != std::string::npos)
			allow = true;
	REQUIRE(allow);
}

TEST_CASE("a path parameter reaches the handler as its declared type", "[router]")
{
	handler_ran = false;
	seen_id = 0;
	const int before = handler_calls;
	Response r = dispatchIn(probe_table, Method::Get, "/api/v1/probe/2b66", "", "", "127.0.0.1", AuthLevel::System);
	REQUIRE(r.code == 200);
	REQUIRE(handler_ran);
	REQUIRE(handler_calls == before + 1);
	REQUIRE(seen_id == 0x2b66);
}

TEST_CASE("a query value outside its declared range never reaches the handler", "[router]")
{
	handler_ran = false;
	const int before = handler_calls;
	Response r = dispatchIn(probe_table, Method::Get, "/api/v1/probe/1", "limit=501", "", "127.0.0.1", AuthLevel::System);
	REQUIRE(r.code == 400);
	REQUIRE_FALSE(handler_ran);
	REQUIRE(handler_calls == before);
	REQUIRE(r.body.find("out-of-range") != std::string::npos);
}

TEST_CASE("a value that is not one the enum lists never reaches the handler", "[router]")
{
	handler_ran = false;
	const int before = handler_calls;
	Response r = dispatchIn(probe_table, Method::Get, "/api/v1/probe/1", "mode=ultraviolet", "", "127.0.0.1", AuthLevel::System);
	REQUIRE(r.code == 400);
	REQUIRE_FALSE(handler_ran);
	REQUIRE(handler_calls == before);
	REQUIRE(r.body.find("bad-enum") != std::string::npos);
}

TEST_CASE("a parameter the table does not declare is refused, not ignored", "[router]")
{
	handler_ran = false;
	const int before = handler_calls;
	Response r = dispatchIn(probe_table, Method::Get, "/api/v1/probe/1", "colour=blue", "", "127.0.0.1", AuthLevel::System);
	REQUIRE(r.code == 400);
	REQUIRE_FALSE(handler_ran);
	REQUIRE(handler_calls == before);
	REQUIRE(r.body.find("no-such-parameter") != std::string::npos);
}

TEST_CASE("the same name given twice is refused", "[router]")
{
	handler_ran = false;
	const int before = handler_calls;
	Response r = dispatchIn(probe_table, Method::Get, "/api/v1/probe/1", "limit=2&limit=3", "", "127.0.0.1", AuthLevel::System);
	REQUIRE(r.code == 400);
	REQUIRE_FALSE(handler_ran);
	REQUIRE(handler_calls == before);
	REQUIRE(r.body.find("duplicate-parameter") != std::string::npos);
}

TEST_CASE("percent encoding is decoded before the value is checked", "[router]")
{
	handler_ran = false;
	Response r = dispatchIn(probe_table, Method::Get, "/api/v1/probe/1", "mode=%74%76", "", "127.0.0.1", AuthLevel::System);
	REQUIRE(r.code == 200);
	REQUIRE(handler_ran);
}

TEST_CASE("a path segment cannot be smuggled through an encoded slash", "[router]")
{
	handler_ran = false;
	const int before = handler_calls;
	// %2f inside a segment stays data; it must not create a new segment and it
	// must not match a route with more segments than the request has.
	Response r = dispatchIn(probe_table, Method::Get, "/api/v1/probe/1%2f2/3", "", "", "127.0.0.1", AuthLevel::System);
	REQUIRE(r.code == 404);
	REQUIRE_FALSE(handler_ran);
	REQUIRE(handler_calls == before);
}

TEST_CASE("a table with two routes for one method and path is refused at start", "[router]")
{
	// A duplicate is a table defect and the server must not start on one,
	// because which of the two answers would otherwise depend on link order.
	static const Endpoint dup[] = {
		{ Method::Get, "/api/v1/probe/{id}", AuthLevel::Read, "a", HTTPD_PARAMS(probe_params), NULL, &probe, false },
		{ Method::Get, "/api/v1/probe/{id}", AuthLevel::Read, "b", HTTPD_PARAMS(probe_params), NULL, &probe, false },
	};
	static const RouteTable t = { HTTPD_TABLE_N("dup", dup, 2) };
	REQUIRE_FALSE(tableIsSane(t));
}

/* Everything above is the shape the endpoint table was designed against.
   Everything below is what a request can be spelt as, which is where a router
   is fooled if it is going to be. */

TEST_CASE("the table the rest of these are written against is one the server would start on", "[router]")
{
	std::string why;
	const bool sane = tableIsSane(probe_table, &why);
	INFO(why);
	REQUIRE(sane);
	REQUIRE(why.empty());
}

TEST_CASE("a request path is matched segment by segment and not as text", "[router]")
{
	struct Case { const char *path; const char *why; };
	static const Case refused[] = {
		{ "/api/v1/probe/1/",     "a trailing separator is a segment more than the route has" },
		{ "/api/v1//probe/1",     "a doubled separator is a segment more than the route has" },
		{ "/api/v1/probe/",       "the segment the route binds is empty and an empty segment is no value" },
		{ "/api/v1/probe",        "the request stops where the route goes on" },
		{ "/api/v1/probe/1/2",    "the request goes on where the route stops" },
		{ "/api/v1/PROBE/1",      "a segment written out is matched as it is written" },
		{ "",                     "an empty target is not an absolute path" },
		{ "api/v1/probe/1",       "a target with no leading separator is not an absolute path" },
		{ "/",                    "the whole of the path is one empty segment" },
	};

	for (size_t i = 0; i < sizeof(refused) / sizeof(refused[0]); ++i)
	{
		handler_ran = false;
		const int before = handler_calls;
		INFO(refused[i].path << ": " << refused[i].why);
		Response r = dispatchIn(probe_table, Method::Get, refused[i].path, "", "", "127.0.0.1", AuthLevel::System);
		REQUIRE(r.code == 404);
		REQUIRE_FALSE(handler_ran);
		REQUIRE(handler_calls == before);
	}
}

TEST_CASE("a segment written out is matched after it is decoded", "[router]")
{
	// The decoding is what happens after the split, so a segment spelt with an
	// escape is the segment it spells and not a second name for it.
	handler_ran = false;
	seen_id = 0;
	Response r = dispatchIn(probe_table, Method::Get, "/api/v1/pro%62e/2b66", "", "", "127.0.0.1", AuthLevel::System);
	REQUIRE(r.code == 200);
	REQUIRE(handler_ran);
	REQUIRE(seen_id == 0x2b66);
}

TEST_CASE("an encoded separator cannot make one segment into two", "[router]")
{
	/* Decoded before the split, this is the route's own path spelt out and
	   would answer 200 with the identifier bound. Decoded after it, the escape
	   is a byte inside one segment, so the request carries three segments
	   where the route has four and matches nothing. */
	handler_ran = false;
	const int before = handler_calls;
	Response r = dispatchIn(probe_table, Method::Get, "/api%2fv1/probe/2b66", "", "", "127.0.0.1", AuthLevel::System);
	REQUIRE(r.code == 404);
	REQUIRE_FALSE(handler_ran);
	REQUIRE(handler_calls == before);
}

TEST_CASE("an escape that is not one is refused rather than read as its bytes", "[router]")
{
	handler_ran = false;
	const int before = handler_calls;
	Response r = dispatchIn(probe_table, Method::Get, "/api/v1/probe/%zz", "", "", "127.0.0.1", AuthLevel::System);
	REQUIRE(r.code == 400);
	REQUIRE_FALSE(handler_ran);
	REQUIRE(handler_calls == before);

	handler_ran = false;
	Response cut = dispatchIn(probe_table, Method::Get, "/api/v1/probe/1", "mode=%7", "", "127.0.0.1", AuthLevel::System);
	REQUIRE(cut.code == 400);
	REQUIRE_FALSE(handler_ran);
	REQUIRE(handler_calls == before);
}

TEST_CASE("a method this server does not have is not a path it does not have", "[router]")
{
	handler_ran = false;
	const int before = handler_calls;
	Response r = dispatchIn(probe_table, UnknownMethod, "/api/v1/probe/1", "", "", "127.0.0.1", AuthLevel::System);
	REQUIRE(r.code == 501);
	REQUIRE(r.body.find("no-such-method") != std::string::npos);
	REQUIRE_FALSE(handler_ran);
	REQUIRE(handler_calls == before);
}

TEST_CASE("a query is read component by component", "[router]")
{
	struct Case { const char *query; int code; const char *why; };
	static const Case cases[] = {
		{ "limit=2&",        200, "a trailing separator carries nothing and is not a parameter" },
		{ "limit=2&&mode=tv", 200, "a doubled separator carries nothing either" },
		{ "limit",           200, "a name with no value is a value that is nothing, which is no value" },
		{ "limit=",          200, "and so is a name with an empty one" },
		{ "colour",          400, "a name with no value is still a name, and this one is not declared" },
		{ "=5",              400, "a value with no name is not a parameter any table could declare" },
		{ "mode=tv&limit=9", 200, "two parameters, each declared once" },
	};

	for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); ++i)
	{
		handler_ran = false;
		INFO(cases[i].query << ": " << cases[i].why);
		Response r = dispatchIn(probe_table, Method::Get, "/api/v1/probe/1", cases[i].query, "", "127.0.0.1", AuthLevel::System);
		REQUIRE(r.code == cases[i].code);
		REQUIRE(handler_ran == (cases[i].code == 200));
	}
}

TEST_CASE("the answer names the first of several things wrong with a request", "[router]")
{
	/* The order is what a client acts on: a name nobody declared is further
	   from a request this endpoint could answer than a value out of range, so
	   it is what the answer says even when both are true of one request. */
	handler_ran = false;
	Response unknown = dispatchIn(probe_table, Method::Get, "/api/v1/probe/1",
	                              "colour=blue&limit=501&limit=502", "", "127.0.0.1", AuthLevel::System);
	REQUIRE(unknown.code == 400);
	REQUIRE(unknown.body.find("no-such-parameter") != std::string::npos);

	Response twice = dispatchIn(probe_table, Method::Get, "/api/v1/probe/1",
	                            "limit=501&limit=502", "", "127.0.0.1", AuthLevel::System);
	REQUIRE(twice.code == 400);
	REQUIRE(twice.body.find("duplicate-parameter") != std::string::npos);
	REQUIRE_FALSE(handler_ran);
}

TEST_CASE("a refusal is a problem document and says so", "[router]")
{
	Response r = dispatchIn(probe_table, Method::Get, "/api/v1/nope", "", "", "127.0.0.1", AuthLevel::System);
	REQUIRE(r.content_type == "application/problem+json");
	REQUIRE(r.body.find("\"status\":404") != std::string::npos);
	// A path this server does not route and a method it does not answer are
	// two faults, so they are two words and not one.
	REQUIRE(r.body.find("no-such-route") != std::string::npos);
	// The target is not quoted back: an answer that echoes what was asked for
	// is one a caller can read its own words out of.
	REQUIRE(r.body.find("/api/v1/nope") == std::string::npos);
}

TEST_CASE("a value a caller wrote is not quoted back in the refusal", "[router]")
{
	Response r = dispatchIn(probe_table, Method::Get, "/api/v1/probe/1", "colour=blue", "", "127.0.0.1", AuthLevel::System);
	REQUIRE(r.code == 400);
	REQUIRE(r.body.find("colour") == std::string::npos);
	REQUIRE(r.body.find("blue") == std::string::npos);
}

// A second table, for the types and the shapes the first one does not carry.

static bool wide_ran = false;
// Reached through a function rather than declared beside the others, so that
// nothing in this file needs a constructor to have run before main, which is
// the rule the tables under test are written to.
static std::string &seenName() { static std::string s; return s; }
static std::string &seenBody() { static std::string s; return s; }
static std::string &seenPeer() { static std::string s; return s; }
static AuthLevel seen_granted = AuthLevel::Public;
static bool seen_on = false;
static unsigned long seen_count = 0;
static time_t seen_when = 0;

static Response wide(const Request &r)
{
	wide_ran = true;
	handler_calls++;
	seenName() = r.asString("name");
	seen_on = r.asBool("on");
	seen_count = r.asUInt("count");
	seen_when = r.asTime("when");
	seenBody() = r.body();
	seenPeer() = r.peer();
	seen_granted = r.granted();
	Response out;
	out.code = 200;
	out.content_type = "application/json";
	out.body = "{}";
	return out;
}

static const Param wide_params[] = {
	HTTPD_QUERY_REQUIRED_IN("count", ParamType::UInt, "how many", 1, 100),
	HTTPD_QUERY_TEXT("name", "a name", 8),
	HTTPD_QUERY("on", ParamType::Bool, "switched on"),
	HTTPD_QUERY("when", ParamType::Time, "a moment"),
	// No ceiling of its own, so what bounds this is the one every value has.
	HTTPD_QUERY("note", ParamType::String, "free text"),
};

static const Endpoint wide_endpoints[] = {
	{ Method::Post, "/api/v1/wide", AuthLevel::Write, "wide", HTTPD_PARAMS(wide_params), NULL, &wide, false },
};

static const RouteTable wide_table = { HTTPD_TABLE_N("wide", wide_endpoints, 1) };

TEST_CASE("the second table is one the server would start on too", "[router]")
{
	std::string why;
	const bool sane = tableIsSane(wide_table, &why);
	INFO(why);
	REQUIRE(sane);
}

TEST_CASE("a required parameter left out never reaches the handler", "[router]")
{
	wide_ran = false;
	const int before = handler_calls;
	Response r = dispatchIn(wide_table, Method::Post, "/api/v1/wide", "name=x", "{}", "127.0.0.1", AuthLevel::System);
	REQUIRE(r.code == 400);
	REQUIRE_FALSE(wide_ran);
	REQUIRE(handler_calls == before);
	REQUIRE(r.body.find("missing-parameter") != std::string::npos);
}

TEST_CASE("a required parameter handed over empty is the same as left out", "[router]")
{
	wide_ran = false;
	Response r = dispatchIn(wide_table, Method::Post, "/api/v1/wide", "count=", "{}", "127.0.0.1", AuthLevel::System);
	REQUIRE(r.code == 400);
	REQUIRE_FALSE(wide_ran);
	REQUIRE(r.body.find("missing-parameter") != std::string::npos);
}

TEST_CASE("each declared type is read as itself before the handler", "[router]")
{
	wide_ran = false;
	seenName().clear();
	seen_on = false;
	seen_count = 0;
	seen_when = 0;
	Response r = dispatchIn(wide_table, Method::Post, "/api/v1/wide",
	                        "count=7&name=a+b&on=yes&when=1735689600", "{\"a\":1}", "10.0.0.9", AuthLevel::System);
	REQUIRE(r.code == 200);
	REQUIRE(wide_ran);
	REQUIRE(seen_count == 7u);
	// A browser building a query out of a form writes a space as a plus.
	REQUIRE(seenName() == "a b");
	REQUIRE(seen_on);
	REQUIRE(seen_when == (time_t) 1735689600);
	// The body is handed over whole and the peer and the level with it.
	REQUIRE(seenBody() == "{\"a\":1}");
	REQUIRE(seenPeer() == "10.0.0.9");
	REQUIRE(seen_granted == AuthLevel::System);
}

TEST_CASE("a value of the wrong shape for its type never reaches the handler", "[router]")
{
	struct Case { const char *query; const char *code; };
	static const Case refused[] = {
		{ "count=7&name=123456789",  "value-too-long" },
		{ "count=7&name=%00",        "value-has-zero-byte" },
		{ "count=7&on=maybe",        "bad-bool" },
		{ "count=7&when=noon",       "bad-int" },
		{ "count=abc",               "bad-int" },
		{ "count=-1",                "bad-int" },
		{ "count=0",                 "out-of-range" },
		{ "count=101",               "out-of-range" },
		{ "count=99999999999999999999", "bad-int" },
	};

	for (size_t i = 0; i < sizeof(refused) / sizeof(refused[0]); ++i)
	{
		wide_ran = false;
		const int before = handler_calls;
		INFO(refused[i].query << " should answer " << refused[i].code);
		Response r = dispatchIn(wide_table, Method::Post, "/api/v1/wide", refused[i].query, "{}", "127.0.0.1", AuthLevel::System);
		REQUIRE(r.code == 400);
		REQUIRE_FALSE(wide_ran);
		REQUIRE(handler_calls == before);
		REQUIRE(r.body.find(refused[i].code) != std::string::npos);
	}
}

static long seen_size = 0;
static bool seen_flag = false;

static Response bodied(const Request &r)
{
	wide_ran = true;
	handler_calls++;
	seenName() = r.asString("what");
	seen_size = r.asInt("size");
	seen_flag = r.asBool("flag");
	Response out;
	out.code = 200;
	out.content_type = "application/json";
	out.body = "{}";
	return out;
}

static const Param body_params[] = {
	HTTPD_BODY_REQUIRED_TEXT("what", "the payload", 8),
	HTTPD_BODY_IN("size", ParamType::Int, "how big", 1, 100),
	HTTPD_BODY("flag", ParamType::Bool, "switched on"),
};

static const Endpoint body_endpoints[] = {
	{ Method::Post, "/api/v1/body", AuthLevel::Write, "a body", HTTPD_PARAMS(body_params), NULL, &bodied, false },
};

static const RouteTable body_table = { HTTPD_TABLE_N("body", body_endpoints, 1) };

TEST_CASE("a table declaring what its body carries is one the server starts on", "[router]")
{
	std::string why;
	const bool sane = tableIsSane(body_table, &why);
	INFO(why);
	REQUIRE(sane);
}

TEST_CASE("a value carried in a body reaches the handler as its declared type", "[router]")
{
	/* A member arrives as text whatever it was written as, and goes through the
	   same row and the same check a value off the query does, so a number
	   written as a number and one written as text are one value here. */
	wide_ran = false;
	seenName().clear();
	seen_size = 0;
	seen_flag = false;
	Response r = dispatchIn(body_table, Method::Post, "/api/v1/body", "",
	                        "{\"what\":\"a b\",\"size\":7,\"flag\":true}",
	                        "127.0.0.1", AuthLevel::System);
	REQUIRE(r.code == 200);
	REQUIRE(wide_ran);
	REQUIRE(seenName() == "a b");
	REQUIRE(seen_size == 7);
	REQUIRE(seen_flag);

	// And the same three written the other way round, which is the point of
	// carrying them as text: one reader answers for both spellings.
	wide_ran = false;
	seen_size = 0;
	Response text = dispatchIn(body_table, Method::Post, "/api/v1/body", "",
	                           "{\"what\":\"a b\",\"size\":\"7\",\"flag\":\"yes\"}",
	                           "127.0.0.1", AuthLevel::System);
	REQUIRE(text.code == 200);
	REQUIRE(seen_size == 7);
}

TEST_CASE("a body value is held to its row before the handler is entered", "[router]")
{
	/* The whole of what lifting the refusal on a body had to buy: a value
	   carried in one is decoded, checked and refused where a value off the
	   query is, and a required one that is not there is missed. Without that a
	   handler would be entered without a value its own row says it gets. */
	struct Case { const char *body; int code; const char *why; };
	static const Case refused[] = {
		{ "{}",                              400, "missing-parameter" },
		{ "{\"size\":7}",                     400, "missing-parameter" },
		{ "{\"what\":\"123456789\"}",          400, "value-too-long" },
		{ "{\"what\":\"a\",\"size\":0}",        400, "out-of-range" },
		{ "{\"what\":\"a\",\"size\":\"x\"}",    400, "bad-int" },
		{ "{\"what\":\"a\",\"flag\":\"maybe\"}",400, "bad-bool" },
		{ "{\"what\":\"a\",\"nope\":1}",       400, "no-such-parameter" },
		{ "{\"what\":\"a\",\"what\":\"b\"}",    400, "duplicate-parameter" },
		// A name the route declares as carried somewhere else is no more one
		// the body offers than a name nobody declared at all.
		{ "{\"what\":\"a\",\"count\":1}",      400, "no-such-parameter" },
		// Everything the reader turns down, under one code and one sentence
		// that names none of what arrived.
		{ "{\"what\":null}",                 400, "bad-string" },
		{ "{\"what\":{\"a\":1}}",             400, "bad-string" },
		{ "{\"what\":[1]}",                  400, "bad-string" },
		{ "{\"what\":\"a\",}",               400, "bad-string" },
		{ "{\"what\":\"a\"} {\"what\":\"b\"}", 400, "bad-string" },
		{ "[]",                              400, "bad-string" },
		{ "not a document at all",           400, "bad-string" },
	};

	for (size_t i = 0; i < sizeof(refused) / sizeof(refused[0]); ++i)
	{
		wide_ran = false;
		const int before = handler_calls;
		INFO(refused[i].body << " should answer " << refused[i].why);
		Response r = dispatchIn(body_table, Method::Post, "/api/v1/body", "",
		                        refused[i].body, "127.0.0.1", AuthLevel::System);
		REQUIRE(r.code == refused[i].code);
		REQUIRE_FALSE(wide_ran);
		REQUIRE(handler_calls == before);
		REQUIRE(r.body.find(refused[i].why) != std::string::npos);
	}
}

TEST_CASE("a body is not read at all for a route that declares none", "[router]")
{
	/* A route that names no value carried in a body is answered whatever it
	   sent. Refusing over bytes nothing reads would turn every stray body into
	   an error about a request that was otherwise right. */
	wide_ran = false;
	Response r = dispatchIn(wide_table, Method::Post, "/api/v1/wide", "count=7",
	                        "not a document at all", "127.0.0.1", AuthLevel::System);
	REQUIRE(r.code == 200);
	REQUIRE(wide_ran);
	// And it is handed over whole, unread and unchanged.
	REQUIRE(seenBody() == "not a document at all");
}

TEST_CASE("a level below the one the endpoint asks for never reaches the handler", "[router]")
{
	wide_ran = false;
	const int before = handler_calls;
	Response r = dispatchIn(wide_table, Method::Post, "/api/v1/wide", "count=7", "{}", "127.0.0.1", AuthLevel::Read);
	REQUIRE(r.code == 403);
	REQUIRE_FALSE(wide_ran);
	REQUIRE(handler_calls == before);
	REQUIRE(r.body.find("not-permitted") != std::string::npos);

	// The level the endpoint asks for is enough, and so is one above it.
	wide_ran = false;
	Response at = dispatchIn(wide_table, Method::Post, "/api/v1/wide", "count=7", "{}", "127.0.0.1", AuthLevel::Write);
	REQUIRE(at.code == 200);
	REQUIRE(wide_ran);
}

// A third table, for two routes that both match one request.

static bool current_ran = false;

static Response current(const Request &r)
{
	(void) r;
	current_ran = true;
	handler_calls++;
	Response out;
	out.code = 200;
	out.content_type = "application/json";
	out.body = "{}";
	return out;
}

static const Param two_params[] = {
	HTTPD_SEGMENT("id", ParamType::ChannelId, "channel id"),
};

static const Endpoint two_endpoints[] = {
	// The binding route is written first on purpose: if link order decided,
	// this is the one that would answer both requests.
	{ Method::Get, "/api/v1/two/{id}",    AuthLevel::Read, "by id",          HTTPD_PARAMS(two_params), NULL, &probe, false },
	{ Method::Get, "/api/v1/two/current", AuthLevel::Read, "the current one", NULL, 0, NULL, &current, false },
	{ Method::Delete, "/api/v1/two/{id}", AuthLevel::Write, "forget one",    HTTPD_PARAMS(two_params), NULL, &probe, false },
};

static const RouteTable two_table = { HTTPD_TABLE_N("two", two_endpoints, 3) };

TEST_CASE("a segment written out answers ahead of one that binds anything", "[router]")
{
	std::string why;
	const bool sane = tableIsSane(two_table, &why);
	INFO(why);
	REQUIRE(sane);

	current_ran = false;
	handler_ran = false;
	Response named = dispatchIn(two_table, Method::Get, "/api/v1/two/current", "", "", "127.0.0.1", AuthLevel::System);
	REQUIRE(named.code == 200);
	REQUIRE(current_ran);
	REQUIRE_FALSE(handler_ran);

	current_ran = false;
	handler_ran = false;
	seen_id = 0;
	Response bound = dispatchIn(two_table, Method::Get, "/api/v1/two/2b66", "", "", "127.0.0.1", AuthLevel::System);
	REQUIRE(bound.code == 200);
	REQUIRE(handler_ran);
	REQUIRE_FALSE(current_ran);
	REQUIRE(seen_id == 0x2b66);
}

TEST_CASE("what is allowed names every method the path answers", "[router]")
{
	/* This path and not one that binds, because two GET routes match it, the
	   one that names the segment and the one that binds it. A path only one
	   route matches would leave the list with nothing to say twice. */
	handler_ran = false;
	current_ran = false;
	Response r = dispatchIn(two_table, Method::Put, "/api/v1/two/current", "", "", "127.0.0.1", AuthLevel::System);
	REQUIRE(r.code == 405);
	REQUIRE_FALSE(handler_ran);
	REQUIRE_FALSE(current_ran);

	std::string allow;
	for (size_t i = 0; i < r.headers.size(); ++i)
		if (r.headers[i].first == "Allow")
			allow = r.headers[i].second;
	REQUIRE(allow.find("GET") != std::string::npos);
	REQUIRE(allow.find("DELETE") != std::string::npos);
	/* Once each, however many routes at that path carry the method, and HEAD
	   beside the GET that answers it whether or not any route names it. */
	REQUIRE(allow == "GET, HEAD, DELETE");
}

/* Two routes that read as different routes, answer one request the same, and
   tie on the count that would settle which of them answers. */

static const Param tie_id_params[] = {
	HTTPD_SEGMENT("id", ParamType::ChannelId, "channel id"),
};

static const Param tie_what_params[] = {
	HTTPD_SEGMENT("what", ParamType::String, "what to read"),
};

static const Param tie_kind_params[] = {
	HTTPD_SEGMENT("kind", ParamType::String, "a kind of thing"),
};

static const Endpoint tie_endpoints[] = {
	{ Method::Get, "/api/v1/tie/{id}/epg",       AuthLevel::Read, "the guide of one",  HTTPD_PARAMS(tie_id_params),   NULL, &probe, false },
	{ Method::Get, "/api/v1/tie/current/{what}", AuthLevel::Read, "one of the current", HTTPD_PARAMS(tie_what_params), NULL, &current, false },
};

static const RouteTable tie_table = { HTTPD_TABLE_N("tie", tie_endpoints, 2) };

static const Endpoint tie2_endpoints[] = {
	{ Method::Get, "/api/v2/tie/{id}",       AuthLevel::Read, "one of them",     HTTPD_PARAMS(tie_id_params),   NULL, &probe, false },
	{ Method::Get, "/api/v2/{kind}/current", AuthLevel::Read, "the current one", HTTPD_PARAMS(tie_kind_params), NULL, &current, false },
};

static const RouteTable tie2_table = { HTTPD_TABLE_N("tie2", tie2_endpoints, 2) };

TEST_CASE("two routes that would answer one request equally well are refused at start", "[router]")
{
	/* Neither pair reads as one route: the two paths differ, and they differ
	   in where the segment that is written out sits. They still tie, because
	   there is a request each of them matches with the same number of segments
	   written out, and nothing is left to say which of them should have it. */
	std::string why;
	REQUIRE_FALSE(tableIsSane(tie_table, &why));
	INFO(why);
	REQUIRE(why.find("/api/v1/tie/{id}/epg") != std::string::npos);
	REQUIRE(why.find("/api/v1/tie/current/{what}") != std::string::npos);

	std::string why2;
	REQUIRE_FALSE(tableIsSane(tie2_table, &why2));
	INFO(why2);
	REQUIRE(why2.find("/api/v2/tie/{id}") != std::string::npos);
	REQUIRE(why2.find("/api/v2/{kind}/current") != std::string::npos);
}

TEST_CASE("the tie is what is refused, not the overlap", "[router]")
{
	// Both routes of that table answer for themselves. It is the one request
	// they both reach that has no answer, which is why the table is refused
	// rather than either route.
	handler_ran = false;
	current_ran = false;
	seen_id = 0;
	Response mine = dispatchIn(tie_table, Method::Get, "/api/v1/tie/2b66/epg", "", "", "127.0.0.1", AuthLevel::System);
	REQUIRE(mine.code == 200);
	REQUIRE(handler_ran);
	REQUIRE(seen_id == 0x2b66);

	handler_ran = false;
	current_ran = false;
	Response theirs = dispatchIn(tie_table, Method::Get, "/api/v1/tie/current/name", "", "", "127.0.0.1", AuthLevel::System);
	REQUIRE(theirs.code == 200);
	REQUIRE(current_ran);
	REQUIRE_FALSE(handler_ran);
}

TEST_CASE("a request two routes answer equally well is answered by neither", "[router]")
{
	/* A table that got past the check, which is the only way here, still must
	   not let link order decide: a server that picked one of the two would
	   answer differently after a rebuild that changed nothing anybody wrote. */
	handler_ran = false;
	current_ran = false;
	const int before = handler_calls;
	Response r = dispatchIn(tie_table, Method::Get, "/api/v1/tie/current/epg", "", "", "127.0.0.1", AuthLevel::System);
	REQUIRE(r.code == 500);
	REQUIRE_FALSE(handler_ran);
	REQUIRE_FALSE(current_ran);
	REQUIRE(handler_calls == before);
	REQUIRE(r.body.find("bad-table") != std::string::npos);
}

TEST_CASE("two routes that write out different segments in one place never meet", "[router]")
{
	// Same length and the same number of segments written out, and no request
	// reaches both, because the one place they both write out they write out
	// differently. Refusing this pair would refuse most tables.
	static const Endpoint apart[] = {
		{ Method::Get, "/api/v1/a/{id}", AuthLevel::Read, "one of a", HTTPD_PARAMS(tie_id_params),   NULL, &probe, false },
		{ Method::Get, "/api/v1/b/{id}", AuthLevel::Read, "one of b", HTTPD_PARAMS(tie_id_params),   NULL, &probe, false },
	};
	static const RouteTable t = { HTTPD_TABLE_N("apart", apart, 2) };
	std::string why;
	const bool sane = tableIsSane(t, &why);
	INFO(why);
	REQUIRE(sane);
}

TEST_CASE("two tables can carry one answer between them", "[router]")
{
	/* Neither table is wrong read alone, which is why the rule is over every
	   route of every table rather than over each table on its own. */
	static const Endpoint first[] = {
		{ Method::Get, "/api/v1/tie/{id}/epg", AuthLevel::Read, "the guide of one", HTTPD_PARAMS(tie_id_params), NULL, &probe, false },
	};
	static const Endpoint second[] = {
		{ Method::Get, "/api/v1/tie/current/{what}", AuthLevel::Read, "one of the current", HTTPD_PARAMS(tie_what_params), NULL, &current, false },
	};
	static const RouteTable first_table = { HTTPD_TABLE_N("first", first, 1) };
	static const RouteTable second_table = { HTTPD_TABLE_N("second", second, 1) };
	static const RouteTable *const both[] = { &first_table, &second_table };

	std::string alone;
	REQUIRE(tableIsSane(first_table, &alone));
	REQUIRE(tableIsSane(second_table, &alone));

	std::string why;
	REQUIRE_FALSE(tablesAreSane(both, 2, &why));
	INFO(why);
	REQUIRE(why.find("/api/v1/tie/{id}/epg") != std::string::npos);
	REQUIRE(why.find("/api/v1/tie/current/{what}") != std::string::npos);
}

TEST_CASE("a list naming a table that is not there is refused and answers nothing", "[router]")
{
	/* The list is addresses of tables, so a member that is not one is a list
	   this layer wrote wrong. Refused where the tables are checked, which is
	   where the server would decline to start, and passed over where a request
	   is answered, because reading through the address anyway would take the
	   process down over one request. */
	static const RouteTable *const missing[] = { NULL, &probe_table };

	std::string why;
	REQUIRE_FALSE(tablesAreSane(missing, 2, &why));
	INFO(why);
	REQUIRE(why.find("not there") != std::string::npos);

	handler_ran = false;
	const int before = handler_calls;
	Response r = dispatchIn(missing, 2, Method::Get, "/api/v1/probe/2b66", "", "", "127.0.0.1", AuthLevel::System);
	REQUIRE(r.code == 200);
	REQUIRE(handler_ran);
	REQUIRE(handler_calls == before + 1);
}

TEST_CASE("a request is answered out of whichever table carries the route", "[router]")
{
	// The list the server answers from holds more than one table, so the walk
	// over it has to reach past the first.
	static const RouteTable probe_only = { HTTPD_TABLE_N("probe", probe_endpoints, 1) };
	static const RouteTable two_only = { HTTPD_TABLE_N("two", two_endpoints, 3) };
	static const RouteTable *const both[] = { &probe_only, &two_only };

	handler_ran = false;
	current_ran = false;
	Response second = dispatchIn(both, 2, Method::Get, "/api/v1/two/current", "", "", "127.0.0.1", AuthLevel::System);
	REQUIRE(second.code == 200);
	REQUIRE(current_ran);

	handler_ran = false;
	seen_id = 0;
	Response first = dispatchIn(both, 2, Method::Get, "/api/v1/probe/2b66", "", "", "127.0.0.1", AuthLevel::System);
	REQUIRE(first.code == 200);
	REQUIRE(handler_ran);
	REQUIRE(seen_id == 0x2b66);

	handler_ran = false;
	const int before = handler_calls;
	Response neither = dispatchIn(both, 2, Method::Get, "/api/v1/nope", "", "", "127.0.0.1", AuthLevel::System);
	REQUIRE(neither.code == 404);
	REQUIRE(handler_calls == before);
}

TEST_CASE("a value with no ceiling of its own is held to the one every value has", "[router]")
{
	/* Every other value in this file is bounded by the row that declares it,
	   so nothing here reaches the ceiling this layer applies whatever a row
	   says, and a request could hand a handler a value the size of the
	   request. */
	wide_ran = false;
	const std::string room(4096, 'a');
	Response fits = dispatchIn(wide_table, Method::Post, "/api/v1/wide", "count=7&note=" + room, "{}", "127.0.0.1", AuthLevel::System);
	REQUIRE(fits.code == 200);
	REQUIRE(wide_ran);

	wide_ran = false;
	const int before = handler_calls;
	const std::string over(4097, 'a');
	Response r = dispatchIn(wide_table, Method::Post, "/api/v1/wide", "count=7&note=" + over, "{}", "127.0.0.1", AuthLevel::System);
	REQUIRE(r.code == 400);
	REQUIRE_FALSE(wide_ran);
	REQUIRE(handler_calls == before);
	REQUIRE(r.body.find("value-too-long") != std::string::npos);
}

// What a table has to be for the server to start on it. Each of these is one
// rule, and each table below breaks that one rule and nothing else.

static Response nothing_handler(const Request &r)
{
	(void) r;
	handler_calls++;
	Response out;
	out.code = 200;
	return out;
}

static const Param one_path_param[] = {
	HTTPD_SEGMENT("id", ParamType::ChannelId, "channel id"),
};

// What a row whose set the build decides names. The set itself does not matter
// here; what the rows below are about is what a row may say beside it.
static void probe_choices(std::vector<std::string> &out)
{
	out.push_back("one");
	out.push_back("two");
}

static const Param asked_set[] = {
	HTTPD_SEGMENT_FROM_ASKED_SET("id", "which one", &probe_choices),
};

TEST_CASE("a table this layer wrote wrong is refused, and says what is wrong with it", "[router]")
{
	static const Param undeclared[] = {
		HTTPD_QUERY("other", ParamType::Int, "something else"),
	};
	static const Param optional_path[] = {
		HTTPD_PARAM_AS_WRITTEN("id", ParamType::ChannelId, In::Path, false, "channel id", 0, 0,
			NULL),
	};
	static const Param unlisted_enum[] = {
		HTTPD_QUERY("mode", ParamType::Enum, "list mode"),
	};
	static const Param empty_listed[] = {
		HTTPD_QUERY_FROM_SET("mode", "list mode", "tv,,radio"),
	};
	static const Param upside_down[] = {
		HTTPD_QUERY_IN("limit", ParamType::Int, "page size", 500, 1),
	};
	static const Param unread_bounds[] = {
		HTTPD_PARAM_AS_WRITTEN("on", ParamType::Bool, In::Query, false, "switched on", 1, 2, NULL),
	};
	static const Param negative_unsigned[] = {
		HTTPD_QUERY_IN("count", ParamType::UInt, "how many", -1, 10),
	};
	static const Param nameless[] = {
		HTTPD_QUERY(NULL, ParamType::Int, "nothing"),
	};
	static const Param twice[] = {
		HTTPD_QUERY_IN("limit", ParamType::Int, "page size", 1, 10),
		HTTPD_QUERY_IN("limit", ParamType::Int, "page size", 1, 10),
	};
	static const Param floored_string[] = {
		HTTPD_PARAM_AS_WRITTEN("s", ParamType::String, In::Query, false, "a string", 1, 8, NULL),
	};
	static const Param roomy_string[] = {
		HTTPD_QUERY_TEXT("s", "a string", 5000),
	};
	static const Param negative_ceiling[] = {
		HTTPD_QUERY_TEXT("s", "a string", -1),
	};
	static const Param listed_number[] = {
		HTTPD_PARAM_AS_WRITTEN("n", ParamType::Int, In::Query, false, "a number", 1, 10, "a,b"),
	};
	static const Param wordless[] = {
		HTTPD_QUERY_IN("n", ParamType::Int, NULL, 1, 10),
	};
	static const Param empty_words[] = {
		HTTPD_QUERY_IN("n", ParamType::Int, "", 1, 10),
	};

	struct Case { const char *what; Endpoint ep; };
	static const Case broken[] = {
		{ "a segment bound to a name nothing declares",
		  { Method::Get, "/api/v1/x/{id}", AuthLevel::Read, "x", HTTPD_PARAMS(undeclared), NULL, &nothing_handler, false } },
		{ "a path parameter the path does not bind",
		  { Method::Get, "/api/v1/x", AuthLevel::Read, "x", HTTPD_PARAMS(one_path_param), NULL, &nothing_handler, false } },
		{ "a path parameter that says it is optional",
		  { Method::Get, "/api/v1/x/{id}", AuthLevel::Read, "x", HTTPD_PARAMS(optional_path), NULL, &nothing_handler, false } },
		{ "a set with nothing in it",
		  { Method::Get, "/api/v1/x", AuthLevel::Read, "x", HTTPD_PARAMS(unlisted_enum), NULL, &nothing_handler, false } },
		{ "a set with an empty value in it",
		  { Method::Get, "/api/v1/x", AuthLevel::Read, "x", HTTPD_PARAMS(empty_listed), NULL, &nothing_handler, false } },
		{ "a floor above its ceiling",
		  { Method::Get, "/api/v1/x", AuthLevel::Read, "x", HTTPD_PARAMS(upside_down), NULL, &nothing_handler, false } },
		{ "bounds on a type that does not read them",
		  { Method::Get, "/api/v1/x", AuthLevel::Read, "x", HTTPD_PARAMS(unread_bounds), NULL, &nothing_handler, false } },
		{ "a floor below zero on a number that holds none or more",
		  { Method::Get, "/api/v1/x", AuthLevel::Read, "x", HTTPD_PARAMS(negative_unsigned), NULL, &nothing_handler, false } },
		{ "a parameter with no name",
		  { Method::Get, "/api/v1/x", AuthLevel::Read, "x", HTTPD_PARAMS(nameless), NULL, &nothing_handler, false } },
		{ "one name declared twice",
		  { Method::Get, "/api/v1/x", AuthLevel::Read, "x", HTTPD_PARAMS(twice), NULL, &nothing_handler, false } },
		{ "no handler",
		  { Method::Get, "/api/v1/x", AuthLevel::Read, "x", NULL, 0, NULL, NULL, false } },
		{ "nothing said about the route",
		  { Method::Get, "/api/v1/x", AuthLevel::Read, NULL, NULL, 0, NULL, &nothing_handler, false } },
		{ "a path that is not absolute",
		  { Method::Get, "api/v1/x", AuthLevel::Read, "x", NULL, 0, NULL, &nothing_handler, false } },
		{ "an empty segment in the path",
		  { Method::Get, "/api/v1//x", AuthLevel::Read, "x", NULL, 0, NULL, &nothing_handler, false } },
		{ "a trailing separator, which is an empty segment",
		  { Method::Get, "/api/v1/x/", AuthLevel::Read, "x", NULL, 0, NULL, &nothing_handler, false } },
		{ "an escape in a path a request is never matched against",
		  { Method::Get, "/api/v1/%78", AuthLevel::Read, "x", NULL, 0, NULL, &nothing_handler, false } },
		{ "a segment bound to no name at all",
		  { Method::Get, "/api/v1/{}", AuthLevel::Read, "x", NULL, 0, NULL, &nothing_handler, false } },
		{ "a method this server does not have",
		  { UnknownMethod, "/api/v1/x", AuthLevel::Read, "x", NULL, 0, NULL, &nothing_handler, false } },
		{ "one name bound twice in the path",
		  { Method::Get, "/api/v1/x/{id}/{id}", AuthLevel::Read, "x", HTTPD_PARAMS(one_path_param), NULL, &nothing_handler, false } },
		{ "a floor on the length of a string, which nothing reads",
		  { Method::Get, "/api/v1/x", AuthLevel::Read, "x", HTTPD_PARAMS(floored_string), NULL, &nothing_handler, false } },
		{ "a ceiling above the one every value here has",
		  { Method::Get, "/api/v1/x", AuthLevel::Read, "x", HTTPD_PARAMS(roomy_string), NULL, &nothing_handler, false } },
		{ "a ceiling below nothing",
		  { Method::Get, "/api/v1/x", AuthLevel::Read, "x", HTTPD_PARAMS(negative_ceiling), NULL, &nothing_handler, false } },
		{ "a set of accepted values beside a type that is not one of a set",
		  { Method::Get, "/api/v1/x", AuthLevel::Read, "x", HTTPD_PARAMS(listed_number), NULL, &nothing_handler, false } },
		{ "a parameter that says nothing about itself",
		  { Method::Get, "/api/v1/x", AuthLevel::Read, "x", HTTPD_PARAMS(wordless), NULL, &nothing_handler, false } },
		{ "a parameter whose words are empty",
		  { Method::Get, "/api/v1/x", AuthLevel::Read, "x", HTTPD_PARAMS(empty_words), NULL, &nothing_handler, false } },
		{ "a route that declares parameters it does not carry",
		  { Method::Get, "/api/v1/x", AuthLevel::Read, "x", NULL, 2, NULL, &nothing_handler, false } },
		/* A body is read for the methods written with one and for no others,
		   so a value carried in the body of any of these three would never be
		   extracted and a required one never missed. */
		{ "a parameter carried in the body of a read",
		  { Method::Get, "/api/v1/x", AuthLevel::Read, "x", HTTPD_PARAMS(body_params), NULL, &nothing_handler, false } },
		{ "a parameter carried in the body of a removal",
		  { Method::Delete, "/api/v1/x", AuthLevel::Write, "x", HTTPD_PARAMS(body_params), NULL, &nothing_handler, false } },
		{ "a parameter carried in the body of an options",
		  { Method::Options, "/api/v1/x", AuthLevel::Read, "x", HTTPD_PARAMS(body_params), NULL, &nothing_handler, false } },
		{ "open to everybody and not the route that hands out a credential",
		  { Method::Get, "/api/v1/x", AuthLevel::Public, "x", NULL, 0, NULL, &nothing_handler, false } },
		{ "open to everybody at the credential path under another method",
		  { Method::Get, "/api/v1/login", AuthLevel::Public, "x", NULL, 0, NULL, &nothing_handler, false } },
		/* The second route open to everybody answers a GET and nothing else,
		   and it is excused as that pair. A POST there would be a route that
		   changes something reachable by anybody, at a path whose name reads
		   as though somebody had already decided it was safe. */
		{ "open to everybody at the path that says what a request carries, under another method",
		  { Method::Post, "/api/v1/session", AuthLevel::Public, "x", NULL, 0, NULL, &nothing_handler, false } },
		/* A caller on the box's own network is granted a read and is asked for
		   no credential, so each of these four is a route any page a browser
		   on that network visits can reach in that browser's name. */
		{ "a change that asks for no more than a read",
		  { Method::Post, "/api/v1/x", AuthLevel::Read, "x", NULL, 0, NULL, &nothing_handler, false } },
		{ "a replacement that asks for no more than a read",
		  { Method::Put, "/api/v1/x", AuthLevel::Read, "x", NULL, 0, NULL, &nothing_handler, false } },
		{ "a correction that asks for no more than a read",
		  { Method::Patch, "/api/v1/x", AuthLevel::Read, "x", NULL, 0, NULL, &nothing_handler, false } },
		{ "a removal that asks for no more than a read",
		  { Method::Delete, "/api/v1/x", AuthLevel::Read, "x", NULL, 0, NULL, &nothing_handler, false } },
	};

	for (size_t i = 0; i < sizeof(broken) / sizeof(broken[0]); ++i)
	{
		const RouteTable t = { HTTPD_TABLE_N("broken", &broken[i].ep, 1) };
		std::string why;
		INFO(broken[i].what);
		REQUIRE_FALSE(tableIsSane(t, &why));
		// A server that refuses to start and says only that it refused leaves
		// whoever wrote the table reading all of it.
		REQUIRE_FALSE(why.empty());
	}
}

TEST_CASE("what a route may declare beside the method it answers", "[router]")
{
	/* The other side of the rows above. A rule that refused every changing
	   route, or every route at all, would pass all of them and leave the
	   server unable to start on anything it is meant to serve. */
	struct Fine
	{
		const char *what;
		Endpoint    ep;
	};

	static const Fine fine[] = {
		{ "a read asking for a read",
		  { Method::Get, "/api/v1/x", AuthLevel::Read, "x", NULL, 0, NULL, &nothing_handler, false } },
		{ "a question about the route itself asking for a read",
		  { Method::Options, "/api/v1/x", AuthLevel::Read, "x", NULL, 0, NULL, &nothing_handler, false } },
		{ "a change asking for a write",
		  { Method::Post, "/api/v1/x", AuthLevel::Write, "x", NULL, 0, NULL, &nothing_handler, false } },
		{ "a replacement asking for a write",
		  { Method::Put, "/api/v1/x", AuthLevel::Write, "x", NULL, 0, NULL, &nothing_handler, false } },
		{ "a removal asking for the most there is",
		  { Method::Delete, "/api/v1/x", AuthLevel::System, "x", NULL, 0, NULL, &nothing_handler, false } },
		// Being reached grants nothing, so there is nothing for another site
		// to gain by reaching it.
		{ "the route that hands out a credential",
		  { Method::Post, "/api/v1/login", AuthLevel::Public, "sign in", NULL, 0, NULL, &nothing_handler, false } },
		// And the one that says what the request asking carries, which hands
		// back nothing that request did not already send.
		{ "the route that says what a request carries",
		  { Method::Get, "/api/v1/session", AuthLevel::Public, "what this request carries", NULL, 0, NULL, &nothing_handler, false } },
		// The three methods a body is read for, so the rule that refuses one
		// on the others is not a rule that refuses every body there is.
		{ "a change carrying its values in a body",
		  { Method::Post, "/api/v1/x", AuthLevel::Write, "x", HTTPD_PARAMS(body_params), NULL, &nothing_handler, false } },
		{ "a replacement carrying its values in a body",
		  { Method::Put, "/api/v1/x", AuthLevel::Write, "x", HTTPD_PARAMS(body_params), NULL, &nothing_handler, false } },
		{ "a correction carrying its values in a body",
		  { Method::Patch, "/api/v1/x", AuthLevel::Write, "x", HTTPD_PARAMS(body_params), NULL, &nothing_handler, false } },
		// And a row whose set the build decides, so that the rule above is
		// about a row saying two things and not about asking at all.
		{ "a segment out of a set this table does not state",
		  { Method::Get, "/api/v1/x/{id}", AuthLevel::Read, "x", HTTPD_PARAMS(asked_set), NULL, &nothing_handler, false } },
	};

	for (size_t i = 0; i < sizeof(fine) / sizeof(fine[0]); ++i)
	{
		const RouteTable t = { HTTPD_TABLE_N("fine", &fine[i].ep, 1) };
		std::string why;
		INFO(fine[i].what);
		REQUIRE(tableIsSane(t, &why));
		REQUIRE(why.empty());
	}
}

TEST_CASE("a table that says it carries routes and carries none is refused", "[router]")
{
	// The count and the routes are one pair and a count too large reads an
	// endpoint that is not there, which nothing at run time can see.
	static const RouteTable t = { HTTPD_TABLE_N("empty", NULL, 1) };
	std::string why;
	REQUIRE_FALSE(tableIsSane(t, &why));
	INFO(why);
	REQUIRE_FALSE(why.empty());
}

TEST_CASE("a table nobody checked is answered rather than walked into", "[router]")
{
	/* Every one of these is refused where tables are checked, so the only way
	   here is a table that was never checked. They answer rather than reaching
	   a handler, or a null one, because a fault of the table is not something
	   a caller can do anything about and is not something to find out by
	   jumping through it. */
	static const Endpoint handlerless[] = {
		{ Method::Get, "/api/v1/x", AuthLevel::Read, "x", NULL, 0, NULL, NULL, false },
	};
	static const RouteTable no_handler = { HTTPD_TABLE_N("handlerless", handlerless, 1) };

	static const Endpoint paramless[] = {
		{ Method::Get, "/api/v1/x", AuthLevel::Read, "x", NULL, 3, NULL, &nothing_handler, false },
	};
	static const RouteTable no_params = { HTTPD_TABLE_N("paramless", paramless, 1) };

	const int before = handler_calls;

	Response a = dispatchIn(no_handler, Method::Get, "/api/v1/x", "", "", "127.0.0.1", AuthLevel::System);
	REQUIRE(a.code == 500);
	REQUIRE(a.body.find("bad-table") != std::string::npos);
	REQUIRE(a.body.find("names no handler") != std::string::npos);

	Response b = dispatchIn(no_params, Method::Get, "/api/v1/x", "", "", "127.0.0.1", AuthLevel::System);
	REQUIRE(b.code == 500);
	REQUIRE(b.body.find("bad-table") != std::string::npos);
	REQUIRE(b.body.find("does not carry") != std::string::npos);

	REQUIRE(handler_calls == before);
}

static const Endpoint miscounted[] = {
	{ Method::Post, "/api/v1/miscounted", AuthLevel::Write, "x", NULL, 3, NULL, &nothing_handler, false },
};

static const RouteTable miscounted_table = { HTTPD_TABLE_N("miscounted", miscounted, 1) };

TEST_CASE("a table defect is answered before anything the caller sent is read", "[router]")
{
	// A request that is also wrong would otherwise be told that it is the one
	// at fault, and whoever wrote the table would never hear about it.
	const int before = handler_calls;
	Response r = dispatchIn(miscounted_table, Method::Post, "/api/v1/miscounted",
	                        "colour=blue", "{\"colour\":\"blue\"}", "127.0.0.1", AuthLevel::System);
	REQUIRE(r.code == 500);
	REQUIRE(r.body.find("bad-table") != std::string::npos);
	// And what the caller wrote is not quoted back, in either place it wrote
	// it, because the answer is not about anything the caller sent.
	REQUIRE(r.body.find("colour") == std::string::npos);
	REQUIRE(handler_calls == before);
}

TEST_CASE("the route that hands out a credential is the one that may be open", "[router]")
{
	static const Endpoint login[] = {
		{ Method::Post, "/api/v1/login", AuthLevel::Public, "sign in", NULL, 0, NULL, &nothing_handler, false },
	};
	static const RouteTable t = { HTTPD_TABLE_N("login", login, 1) };
	std::string why;
	const bool sane = tableIsSane(t, &why);
	INFO(why);
	REQUIRE(sane);
}

TEST_CASE("two routes that spell one shape differently are the one route they are", "[router]")
{
	// The text of the two paths differs and the requests they match do not, so
	// a comparison of the text would let both into the table and leave which
	// of them answered to the linker.
	static const Param other_path[] = {
		HTTPD_SEGMENT("name", ParamType::String, "a name"),
	};
	static const Endpoint spelt[] = {
		{ Method::Get, "/api/v1/x/{id}",   AuthLevel::Read, "a", HTTPD_PARAMS(one_path_param), NULL, &nothing_handler, false },
		{ Method::Get, "/api/v1/x/{name}", AuthLevel::Read, "b", HTTPD_PARAMS(other_path),     NULL, &nothing_handler, false },
	};
	static const RouteTable t = { HTTPD_TABLE_N("spelt", spelt, 2) };
	std::string why;
	REQUIRE_FALSE(tableIsSane(t, &why));
	INFO(why);
	REQUIRE(why.find("/api/v1/x/{id}") != std::string::npos);
	REQUIRE(why.find("/api/v1/x/{name}") != std::string::npos);
}

TEST_CASE("one method and one shape twice is refused and one shape under two methods is not", "[router]")
{
	static const Endpoint both[] = {
		{ Method::Get,    "/api/v1/x/{id}", AuthLevel::Read,  "read one",   HTTPD_PARAMS(one_path_param), NULL, &nothing_handler, false },
		{ Method::Delete, "/api/v1/x/{id}", AuthLevel::Write, "forget one", HTTPD_PARAMS(one_path_param), NULL, &nothing_handler, false },
	};
	static const RouteTable t = { HTTPD_TABLE_N("both", both, 2) };
	std::string why;
	const bool sane = tableIsSane(t, &why);
	INFO(why);
	REQUIRE(sane);
}

TEST_CASE("the list of tables the server ships is the one it names and sane", "[router]")
{
	/* Each table is stated by the module that owns it and named by the file that owns
	   the list. The check runs where the server starts, so a list this refuses is a
	   server that refuses to start. How many there are is the endpoint modules'
	   business; what this asks is that the list is one at all and that every route in it
	   keeps the rules a route has to keep. */
	size_t count = 0;
	const RouteTable *const *tables = allRoutes(&count);
	REQUIRE(count > 0);
	REQUIRE(tables != NULL);

	std::string why;
	const bool sane = routesAreSane(&why);
	INFO(why);
	REQUIRE(sane);

	// A count is not required, so a caller that only wants the tables does not
	// have to declare one.
	REQUIRE(allRoutes(NULL) == tables);
}

TEST_CASE("a path no shipped route names is answered by nothing", "[router]")
{
	handler_ran = false;
	const int before = handler_calls;
	Response r = dispatch(Method::Get, "/api/v1/probe/1", "", "", "127.0.0.1", AuthLevel::System);
	REQUIRE(r.code == 404);
	REQUIRE_FALSE(handler_ran);
	REQUIRE(handler_calls == before);
}

TEST_CASE("what the handler answers is what the router answers", "[router]")
{
	// The router does not rewrite a reply it did not make: everything it puts
	// on a wire that a handler did not is a refusal, and this is not one.
	handler_ran = false;
	Response r = dispatchIn(probe_table, Method::Get, "/api/v1/probe/1", "", "", "127.0.0.1", AuthLevel::System);
	REQUIRE(handler_ran);
	REQUIRE(r.code == 200);
	REQUIRE(r.body == "{}");
	REQUIRE(r.content_type == "application/json");
	REQUIRE(r.headers.empty());
}

TEST_CASE("a name the endpoint does not declare answers an empty value rather than ending the process", "[router]")
{
	/* The fault this layer replaces was a handler reading a parameter that was
	   not there and taking the program down with it. An accessor that ended
	   the process over a name nobody declared would be that fault written one
	   floor up, so the answer here is the empty value of the type asked for. */
	Request r;
	REQUIRE_FALSE(r.has("nothing"));
	REQUIRE(r.asInt("nothing") == 0);
	REQUIRE(r.asUInt("nothing") == 0);
	REQUIRE_FALSE(r.asBool("nothing"));
	REQUIRE(r.asChannelId("nothing") == 0);
	REQUIRE(r.asTime("nothing") == (time_t) 0);
	REQUIRE(r.asString("nothing").empty());
	REQUIRE(r.asString(NULL).empty());
	REQUIRE_FALSE(r.has(NULL));
	REQUIRE(r.body().empty());
	REQUIRE(r.peer().empty());
	// Nothing until the router says otherwise, so a request that reached a
	// handler without one is the least it could be and not the most.
	REQUIRE(r.granted() == AuthLevel::Public);
}

TEST_CASE("a value is read as the type its row declares and not as another one", "[router]")
{
	// The row is what says which of the members carries the value, so reading
	// it as a second type is a handler asking for something no table promised.
	Value v;
	v.type = ParamType::ChannelId;
	v.text = "2b66";
	v.unumber = 0x2b66;

	Request r;
	r.bind("id", v);
	REQUIRE(r.has("id"));
	REQUIRE(r.asChannelId("id") == 0x2b66);
	REQUIRE(r.asInt("id") == 0);
	REQUIRE(r.asUInt("id") == 0);
	// The text of any value is there whatever the type, which is what a
	// handler that wants the bytes as they arrived reads.
	REQUIRE(r.asString("id") == "2b66");
}

static const Endpoint head_endpoints[] = {
	{ Method::Head, "/api/v1/probe", AuthLevel::Read, "the head of it",
	  NULL, 0, NULL, &probe, false },
};

static const RouteTable head_table = { HTTPD_TABLE_N("head", head_endpoints, 1) };

TEST_CASE("a HEAD is answered by the route that answers the GET", "[router]")
{
	/* The whole answer and not a shortened one: what the transport sends is
	   the head of it, and an answer built without its body could not state the
	   length the GET would have stated. */
	handler_ran = false;
	const int before = handler_calls;
	Response r = dispatchIn(probe_table, Method::Head, "/api/v1/probe/2b66", "", "", "127.0.0.1", AuthLevel::System);
	REQUIRE(r.code == 200);
	REQUIRE(handler_ran);
	REQUIRE(handler_calls == before + 1);
	REQUIRE(r.body == "{}");
	REQUIRE(r.content_type == "application/json");
}

TEST_CASE("the methods a path answers name HEAD wherever they name GET", "[router]")
{
	// A list that leaves it out sends a caller away from an answer this server
	// would have given it.
	Response r = dispatchIn(probe_table, Method::Post, "/api/v1/probe/2b66", "", "", "127.0.0.1", AuthLevel::System);
	REQUIRE(r.code == 405);
	std::string allow;
	for (size_t i = 0; i < r.headers.size(); ++i)
	{
		if (r.headers[i].first == "Allow")
			allow = r.headers[i].second;
	}
	INFO(allow);
	REQUIRE(allow.find("GET") != std::string::npos);
	REQUIRE(allow.find("HEAD") != std::string::npos);
	// Not offered and so not named: a server that lists a verb it turns down
	// is worse than one that lists neither.
	REQUIRE(allow.find("OPTIONS") == std::string::npos);
}

static const Endpoint head_first_endpoints[] = {
	{ Method::Head, "/api/v1/probe", AuthLevel::Read, "the head of it", NULL, 0, NULL, &probe, false },
	{ Method::Get,  "/api/v1/probe", AuthLevel::Read, "the whole of it", NULL, 0, NULL, &probe, false },
};

static const RouteTable head_first_table = { HTTPD_TABLE_N("head-first", head_first_endpoints, 2) };

TEST_CASE("HEAD is named once however a table spells it", "[router]")
{
	/* A table naming HEAD itself is one the check refuses and the server will
	   not start on, but nothing stops this from being asked directly, and a
	   list naming one verb twice is a list a client reads as two. */
	Response r = dispatchIn(head_first_table, Method::Post, "/api/v1/probe", "", "", "127.0.0.1", AuthLevel::System);
	REQUIRE(r.code == 405);
	std::string allow;
	for (size_t i = 0; i < r.headers.size(); ++i)
	{
		if (r.headers[i].first == "Allow")
			allow = r.headers[i].second;
	}
	INFO(allow);
	size_t at = allow.find("HEAD");
	REQUIRE(at != std::string::npos);
	REQUIRE(allow.find("HEAD", at + 4) == std::string::npos);
}

TEST_CASE("a route that answers a HEAD of its own is refused", "[router]")
{
	/* No request reaches it, because a HEAD is answered by the route that
	   answers the GET, and a table holding one would name the verb twice in
	   the list of what its path takes. */
	std::string why;
	REQUIRE_FALSE(tableIsSane(head_table, &why));
	INFO(why);
	REQUIRE(why.find("HEAD") != std::string::npos);
}

TEST_CASE("a row may state its set or name one to ask for, and not both", "[router]")
{
	/* Written by spoiling a row the macros do write rather than by spelling
	   one out field by field. A row spelled out is a row every later field of
	   Param has to be added to, and this case is about one of them, so it
	   would be the one place that goes on compiling while saying something
	   else. */
	Param both[] = {
		HTTPD_QUERY_FROM_SET("mode", "list mode", "tv,radio"),
	};
	both[0].choices = &probe_choices;
	const Endpoint two_sets[] = {
		{ Method::Get, "/api/v1/x", AuthLevel::Read, "x", HTTPD_PARAMS(both), NULL, &nothing_handler, false },
	};
	const RouteTable t = { HTTPD_TABLE_N("two-sets", two_sets, 1) };
	std::string why;
	REQUIRE_FALSE(tableIsSane(t, &why));
	REQUIRE(why.find("as well") != std::string::npos);

	// The set is text whatever answers it, so a row that names one beside a
	// number is a row nothing could put the answer in.
	Param numeric[] = {
		HTTPD_QUERY("n", ParamType::Int, "a number"),
	};
	numeric[0].choices = &probe_choices;
	const Endpoint asked_number[] = {
		{ Method::Get, "/api/v1/x", AuthLevel::Read, "x", HTTPD_PARAMS(numeric), NULL, &nothing_handler, false },
	};
	const RouteTable n = { HTTPD_TABLE_N("asked-number", asked_number, 1) };
	REQUIRE_FALSE(tableIsSane(n, &why));
	REQUIRE(why.find("is not text") != std::string::npos);

	// And the row itself, so that what is refused above is the second set and
	// the wrong kind, and not asking for a set at all.
	const Endpoint alone[] = {
		{ Method::Get, "/api/v1/x/{id}", AuthLevel::Read, "x", HTTPD_PARAMS(asked_set), NULL, &nothing_handler, false },
	};
	const RouteTable a = { HTTPD_TABLE_N("asked", alone, 1) };
	REQUIRE(tableIsSane(a, &why));
	REQUIRE(why.empty());
}

// A fifth table, for a route whose body is the whole of what it carries.

static bool whole_ran = false;
static std::string &wholeBody() { static std::string s; return s; }

static Response wholly(const Request &r)
{
	whole_ran = true;
	handler_calls++;
	wholeBody() = r.body();
	Response out;
	out.code = 200;
	out.content_type = "application/json";
	out.body = "{}";
	return out;
}

static const Param whole_list_params[] = {
	HTTPD_SEGMENT("id", ParamType::ChannelId, "which one"),
	HTTPD_BODY_IS_LIST_OF("items", ParamType::String, "the list itself", 1, 9),
};

static const Endpoint whole_list_endpoints[] = {
	{ Method::Put, "/api/v1/whole/{id}", AuthLevel::Write, "takes a list",
	  HTTPD_PARAMS(whole_list_params), NULL, &wholly, false },
};

static const RouteTable whole_list_table = { HTTPD_TABLE_N("whole", whole_list_endpoints, 1) };

TEST_CASE("a table whose route names the whole of its body is one the server starts on", "[router]")
{
	std::string why;
	const bool sane = tableIsSane(whole_list_table, &why);
	INFO(why);
	REQUIRE(sane);
}

TEST_CASE("a route naming the whole of its body on a method that carries none is refused", "[router]")
{
	// A GET, which is written with no body at all: the row would be read by
	// nothing, and a caller told to send something nothing looks at.
	static const Param on_a_read[] = {
		HTTPD_SEGMENT("id", ParamType::ChannelId, "which one"),
		HTTPD_BODY_IS_LIST_OF("items", ParamType::String, "the list", 0, 0),
	};
	static const Endpoint reading[] = {
		{ Method::Get, "/api/v1/whole/{id}", AuthLevel::Read, "reads",
		  HTTPD_PARAMS(on_a_read), NULL, &nothing_handler, false },
	};
	static const RouteTable t = { HTTPD_TABLE_N("reading", reading, 1) };
	std::string why;
	REQUIRE_FALSE(tableIsSane(t, &why));
	INFO(why);
	REQUIRE(why.find("items") != std::string::npos);
	REQUIRE(why.find("not written with one") != std::string::npos);
}

TEST_CASE("a route naming the whole of its body and a value inside it is refused", "[router]")
{
	/* Whichever of the two a caller sent, the other row is unanswered: a body
	   that is an array carries no member called what, and a body that is an
	   object of the caller's own names is not the list. */
	static const Param both_ways[] = {
		HTTPD_BODY_IS_LIST_OF("items", ParamType::String, "the list", 0, 0),
		HTTPD_BODY_REQUIRED_TEXT("what", "a member", 8),
	};
	static const Endpoint mixed[] = {
		{ Method::Post, "/api/v1/whole", AuthLevel::Write, "both",
		  HTTPD_PARAMS(both_ways), NULL, &nothing_handler, false },
	};
	static const RouteTable t = { HTTPD_TABLE_N("mixed", mixed, 1) };
	std::string why;
	REQUIRE_FALSE(tableIsSane(t, &why));
	INFO(why);
	REQUIRE(why.find("inside it") != std::string::npos);
}

TEST_CASE("a route naming the whole of its body twice is refused", "[router]")
{
	static const Param twice[] = {
		HTTPD_BODY_IS_LIST_OF("items", ParamType::String, "the list", 0, 0),
		HTTPD_BODY_IS_MAP_OF("named", ParamType::String, "the map", 0, 0),
	};
	static const Endpoint two[] = {
		{ Method::Post, "/api/v1/whole", AuthLevel::Write, "twice",
		  HTTPD_PARAMS(twice), NULL, &nothing_handler, false },
	};
	static const RouteTable t = { HTTPD_TABLE_N("twice", two, 1) };
	std::string why;
	REQUIRE_FALSE(tableIsSane(t, &why));
	INFO(why);
	REQUIRE(why.find("twice") != std::string::npos);
}

TEST_CASE("a list of a kind that does not travel as text is refused", "[router]")
{
	// The array reader takes strings and nothing else, so a row saying the
	// elements are numbers puts into the document a body this server refuses.
	static const Param numbers[] = {
		HTTPD_BODY_IS_LIST_OF("items", ParamType::Int, "numbers", 0, 0),
	};
	static const Endpoint t_ep[] = {
		{ Method::Post, "/api/v1/whole", AuthLevel::Write, "numbers",
		  HTTPD_PARAMS(numbers), NULL, &nothing_handler, false },
	};
	static const RouteTable t = { HTTPD_TABLE_N("numbers", t_ep, 1) };
	std::string why;
	REQUIRE_FALSE(tableIsSane(t, &why));
	INFO(why);
	REQUIRE(why.find("travel as text") != std::string::npos);
}

TEST_CASE("what a row naming the whole of a body counts is read as a count", "[router]")
{
	/* The pair means something else on such a row, so what the check refuses
	   is something else too: a floor on a string is refused everywhere but
	   here, and a ceiling beside an identifier likewise, and both of those are
	   what these two routes carry. What is left to refuse is a count no body
	   has and a pair that describes a body nobody could write. */
	static const Param below[] = {
		HTTPD_BODY_IS_MAP_OF("named", ParamType::String, "the map", -1, 4),
	};
	static const Endpoint under[] = {
		{ Method::Post, "/api/v1/whole", AuthLevel::Write, "under",
		  HTTPD_PARAMS(below), NULL, &nothing_handler, false },
	};
	static const RouteTable u = { HTTPD_TABLE_N("under", under, 1) };
	std::string why;
	REQUIRE_FALSE(tableIsSane(u, &why));
	INFO(why);
	REQUIRE(why.find("below zero") != std::string::npos);

	static const Param upside[] = {
		HTTPD_BODY_IS_LIST_OF("items", ParamType::ChannelId, "the list", 9, 2),
	};
	static const Endpoint over[] = {
		{ Method::Put, "/api/v1/whole", AuthLevel::Write, "over",
		  HTTPD_PARAMS(upside), NULL, &nothing_handler, false },
	};
	static const RouteTable o = { HTTPD_TABLE_N("over", over, 1) };
	REQUIRE_FALSE(tableIsSane(o, &why));
	INFO(why);
	REQUIRE(why.find("at least") != std::string::npos);
}

TEST_CASE("a row naming the whole of a body binds nothing and is never missed", "[router]")
{
	/* Both halves in one case, because they are one mistake: a router that
	   looked for such a row would look in the query, find nothing, and turn
	   every request down for a required value that never arrives under a name. */
	whole_ran = false;
	wholeBody().clear();
	const int before = handler_calls;
	const Response r = dispatchIn(whole_list_table, Method::Put, "/api/v1/whole/2b66", "",
	                              "[\"a\",\"b\"]", "127.0.0.1", AuthLevel::System);
	REQUIRE(r.code == 200);
	REQUIRE(whole_ran);
	REQUIRE(handler_calls == before + 1);
	// The handler got the bytes, unread and unchanged: nothing here takes such
	// a body apart, which is the point of the row.
	REQUIRE(wholeBody() == "[\"a\",\"b\"]");

	/* And the name is not a query component either: a caller writing one is
	   told this endpoint declares no parameter of that name, which is what it
	   was told before the row existed. */
	whole_ran = false;
	const Response q = dispatchIn(whole_list_table, Method::Put, "/api/v1/whole/2b66",
	                              "items=a", "[\"a\"]", "127.0.0.1", AuthLevel::System);
	REQUIRE(q.code == 400);
	REQUIRE_FALSE(whole_ran);
	REQUIRE(q.body.find("no-such-parameter") != std::string::npos);
}

TEST_CASE("which routes take a credential out of an address, asked the way the transport asks", "[router]")
{
	/* The transport reads this off the route before it reads the request, because what a
	   request carries is worth nothing until it is known what it would reach. So the
	   routes that answer yes here are the whole of what a leaked address can be used on,
	   and the list is written down rather than left to be read off a table.

	   Against the routes this server ships and not against a table written here, which is
	   the only way this case can be about the answer rather than about itself. */
	setRoutesForTest(NULL);

	bool takes = false;
	AuthLevel need = AuthLevel::Public;

	REQUIRE(routeLevelFor(Get, "/api/v1/storage/file", &need, &takes));
	REQUIRE(need == AuthLevel::System);
	REQUIRE(takes);

	/* A HEAD, which is among them and has to be: it is answered by the route that
	   answers the GET, and a player that asks how large a file is before it fetches one
	   asks with a HEAD. Refused here, the address would work in one player and not in
	   the next. */
	REQUIRE(routeLevelFor(Head, "/api/v1/storage/file", &need, &takes));
	REQUIRE(takes);

	// The route that writes a file shares that path and answers no, so this is read off
	// the route and never off the path.
	REQUIRE(routeLevelFor(Put, "/api/v1/storage/file", &need, &takes));
	REQUIRE_FALSE(takes);

	// The removal, which is the one that must never get it, and the listing beside it.
	REQUIRE(routeLevelFor(Delete, "/api/v1/storage/path", &need, &takes));
	REQUIRE_FALSE(takes);
	REQUIRE(routeLevelFor(Get, "/api/v1/storage/files", &need, &takes));
	REQUIRE_FALSE(takes);

	/* A path no route answers and the legacy prefix, which has no route table at all.
	   Both are set to true first, so a function that simply never wrote the answer would
	   be caught rather than agreed with. */
	takes = true;
	REQUIRE_FALSE(routeLevelFor(Get, "/api/v1/nothing-answers-this", &need, &takes));
	REQUIRE_FALSE(takes);
	takes = true;
	REQUIRE_FALSE(routeLevelFor(Get, "/control/standby", &need, &takes));
	REQUIRE_FALSE(takes);

	// And one route in the whole tree, counted rather than trusted.
	size_t tables = 0;
	const RouteTable *const *t = allRoutes(&tables);
	REQUIRE(tables > 0);
	size_t saying_yes = 0;
	for (size_t i = 0; i < tables; ++i)
	{
		for (size_t j = 0; j < t[i]->count; ++j)
		{
			if (t[i]->endpoints[j].query_token_ok)
				++saying_yes;
		}
	}
	REQUIRE(saying_yes == 1);

	setRoutesForTest(NULL);
}
