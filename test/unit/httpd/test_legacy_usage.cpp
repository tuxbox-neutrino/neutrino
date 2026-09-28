/*
 * test_legacy_usage.cpp - the counting seam behind /api/v1/system/legacy-usage
 *
 * Copyright (C) 2026 Thilo Graf
 *
 * License: GPL-2.0-or-later
 *
 * The seam is this tree's own (src/httpd/compat/mount.h): nhttpd's
 * CControlAPI seeds the known table and records what it is asked for,
 * and the system module answers with the result. The semantics pinned
 * here are the source's usage(): the known table first, in its own order,
 * zeros carried, unknown names appended in the order first seen and
 * bounded. The wire proof that CControlAPI actually feeds the seam is
 * test/http/check-v1.sh's, not this file's.
 */

#include "support/catch.hpp"

#include "httpd/compat/mount.h"

#include <cstdio>
#include <string>
#include <vector>

using httpd::compat::Usage;

namespace
{

// Every case starts and ends empty, so no order of cases can lend one a
// count another produced.
struct CleanSlate
{
	CleanSlate() { httpd::compat::resetForTest(); }
	~CleanSlate() { httpd::compat::resetForTest(); }
};

void seedThree()
{
	httpd::compat::knowName("getmode");
	httpd::compat::knowName("setmode");
	httpd::compat::knowName("zapto");
}

} // anonymous namespace

TEST_CASE("the known table comes first, in its own order, each carried even at zero", "[legacyusage]")
{
	CleanSlate slate;
	seedThree();

	const std::vector<Usage> all = httpd::compat::usage();
	REQUIRE(all.size() == 3);
	CHECK(all[0].name == "getmode");
	CHECK(all[1].name == "setmode");
	CHECK(all[2].name == "zapto");
	for (size_t i = 0; i < all.size(); ++i)
		CHECK(all[i].calls == 0);
}

TEST_CASE("a recorded known name counts on its own seat", "[legacyusage]")
{
	CleanSlate slate;
	seedThree();

	httpd::compat::record("setmode");
	httpd::compat::record("setmode");
	httpd::compat::record("zapto");

	const std::vector<Usage> all = httpd::compat::usage();
	REQUIRE(all.size() == 3);
	CHECK(all[0].name == "getmode");
	CHECK(all[0].calls == 0);
	CHECK(all[1].name == "setmode");
	CHECK(all[1].calls == 2);
	CHECK(all[2].name == "zapto");
	CHECK(all[2].calls == 1);
}

TEST_CASE("a name the table never had is appended after the known, in the order first seen", "[legacyusage]")
{
	CleanSlate slate;
	seedThree();

	httpd::compat::record("standby");
	httpd::compat::record("shutdown");
	httpd::compat::record("standby");

	const std::vector<Usage> all = httpd::compat::usage();
	REQUIRE(all.size() == 5);
	CHECK(all[3].name == "standby");
	CHECK(all[3].calls == 2);
	CHECK(all[4].name == "shutdown");
	CHECK(all[4].calls == 1);
}

TEST_CASE("seeding the same name twice does not add a second seat", "[legacyusage]")
{
	CleanSlate slate;
	seedThree();
	httpd::compat::knowName("setmode");

	const std::vector<Usage> all = httpd::compat::usage();
	REQUIRE(all.size() == 3);
}

TEST_CASE("asking for ever new names cannot grow the answer past the bound", "[legacyusage]")
{
	CleanSlate slate;
	seedThree();

	const size_t past = httpd::compat::kMaxUnknownLegacyNames + 5;
	char name[32];
	for (size_t i = 0; i < past; ++i)
	{
		std::snprintf(name, sizeof(name), "made-up-%zu", i);
		httpd::compat::record(name);
	}

	const std::vector<Usage> all = httpd::compat::usage();
	REQUIRE(all.size() == 3 + httpd::compat::kMaxUnknownLegacyNames);

	// The one past the bound left no trace; the first is still counted.
	std::snprintf(name, sizeof(name), "made-up-%zu", past - 1);
	for (size_t i = 0; i < all.size(); ++i)
		CHECK(all[i].name != name);
	CHECK(all[3].name == "made-up-0");
	CHECK(all[3].calls == 1);
}
