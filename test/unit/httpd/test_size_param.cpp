/*
 * test_size_param.cpp - a size read as sixty four bits on every box
 *
 * Copyright (C) 2026 Thilo Graf
 *
 * License: GPL-2.0-or-later
 */

#include "support/catch.hpp"
#include "httpd/router.h"
#include "httpd/endpoint.h"

#include <stdint.h>

#include <string>
#include <utility>
#include <vector>

using namespace httpd;

namespace
{

bool size_ran = false;
uint64_t seen_at = 0;
unsigned long seen_as_uint = 1;

Response sized(const Request &r)
{
	size_ran = true;
	seen_at = r.asSize("at");
	seen_as_uint = r.asUInt("at");
	Response out;
	out.code = 204;
	return out;
}

const Param size_params[] = {
	HTTPD_QUERY_REQUIRED("at", ParamType::Size, "where the piece goes"),
};

const Endpoint size_endpoints[] = {
	{ Method::Patch, "/api/v1/sized", AuthLevel::Write, "sized", NULL, HTTPD_PARAMS(size_params), NULL, &sized, false, Answers204, HTTPD_NO_REFUSALS },
};

const RouteTable size_table = { HTTPD_TABLE_N("sized", size_endpoints, 1) };

Response send(const std::string &query)
{
	size_ran = false;
	seen_at = 0;
	seen_as_uint = 1;
	return dispatchIn(size_table, Method::Patch, "/api/v1/sized", query, "", "127.0.0.1", AuthLevel::System);
}

} // namespace

TEST_CASE("a table with a size is one the server starts on", "[router][size]")
{
	std::string why;
	const bool sane = tableIsSane(size_table, &why);
	INFO(why);
	REQUIRE(sane);
}

/* The same path on every box: nothing between the text and the handler depends on how
   wide a long is, so what this host reads past 4 GiB is what a box with a four byte
   long reads, where UInt refuses it as too wide. */
TEST_CASE("a size past 4 GiB reaches the handler whole", "[router][size]")
{
	Response r = send("at=4294967297");
	REQUIRE(r.code == 204);
	REQUIRE(size_ran);
	REQUIRE(seen_at == 4294967297ULL);
	// a size is no UInt, so asUInt does not read it
	REQUIRE(seen_as_uint == 0);

	r = send("at=18446744073709551615");
	REQUIRE(r.code == 204);
	REQUIRE(seen_at == 18446744073709551615ULL);

	r = send("at=0");
	REQUIRE(r.code == 204);
	REQUIRE(seen_at == 0);
}

TEST_CASE("a size of the wrong shape never reaches the handler", "[router][size]")
{
	static const char *const refused[] = {
		"at=-1", "at=abc", "at=1.5", "at=+1", "at=%201", "at=18446744073709551616",
	};
	for (size_t i = 0; i < sizeof(refused) / sizeof(refused[0]); ++i)
	{
		INFO(refused[i]);
		const Response r = send(refused[i]);
		REQUIRE(r.code == 400);
		REQUIRE_FALSE(size_ran);
		REQUIRE(r.body.find("bad-int") != std::string::npos);
		REQUIRE(r.body.find("is not a whole number of none or more") != std::string::npos);
	}
}

TEST_CASE("a size the row does not bound states no range it could leave", "[router][size]")
{
	std::vector<std::pair<coreapi::ErrorCode, std::string> > out;
	parameterRefusals(size_endpoints[0], out);
	bool bad_int = false;
	for (size_t i = 0; i < out.size(); ++i)
	{
		INFO(out[i].second);
		REQUIRE(out[i].first != coreapi::ErrorCode::OutOfRange);
		if (out[i].first == coreapi::ErrorCode::BadInt)
			bad_int = true;
	}
	REQUIRE(bad_int);
}

TEST_CASE("a size bounded below zero is a table written wrong", "[router][size]")
{
	static const Param below[] = {
		HTTPD_QUERY_IN("at", ParamType::Size, "where", -1, 10),
	};
	static const Endpoint eps[] = {
		{ Method::Patch, "/api/v1/sized", AuthLevel::Write, "sized", NULL, HTTPD_PARAMS(below), NULL, &sized, false, Answers204, HTTPD_NO_REFUSALS },
	};
	static const RouteTable t = { HTTPD_TABLE_N("below", eps, 1) };
	std::string why;
	REQUIRE_FALSE(tableIsSane(t, &why));
	REQUIRE(why.find("bounded below zero") != std::string::npos);
}
