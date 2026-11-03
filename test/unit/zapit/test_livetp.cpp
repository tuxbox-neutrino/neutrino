/*
 * test_livetp.cpp - whether the live channel's tuner stands on its transponder
 *
 * Copyright (C) 2026 Tuxbox-Project (live tuner taken, tests)
 *
 * License: GPL-2.0-or-later
 *
 * livetp decides from values read off the box whether a stream holds the
 * live frontend on another transponder than the live channel's. These
 * cases drive the shipped object.
 */

#include "support/catch.hpp"

#include <zapit/livetp.h>

namespace
{

// Two transponders with the same lower 32 bits, as NVOD services carry.
const uint64_t kTpA = 0x0130044d00010001ULL;
const uint64_t kTpB = 0x0130044e00020001ULL;
const uint64_t kTpA2 = 0x0d00044d00010001ULL;

} // namespace

TEST_CASE("L1: a tuned live frontend on another transponder is displaced", "[zapit][livetp]")
{
	REQUIRE(livetp::displaced(false, true, true, kTpB, kTpA, false));
}

TEST_CASE("L2: a live frontend on the channel's own transponder is not", "[zapit][livetp]")
{
	REQUIRE_FALSE(livetp::displaced(false, true, true, kTpA, kTpA, false));
}

TEST_CASE("L3: a frontend without signal or no frontend at all is not", "[zapit][livetp]")
{
	// No signal: a zap to the same channel does nothing.
	REQUIRE_FALSE(livetp::displaced(false, true, false, kTpB, kTpA, false));
	// No tuner, as on a box without one.
	REQUIRE_FALSE(livetp::displaced(false, false, false, 0, kTpA, false));
	REQUIRE_FALSE(livetp::displaced(false, false, true, kTpB, kTpA, false));
}

TEST_CASE("L4: a web channel is never displaced", "[zapit][livetp]")
{
	REQUIRE_FALSE(livetp::displaced(true, true, true, kTpB, kTpA, false));
	REQUIRE_FALSE(livetp::displaced(true, true, true, kTpB, kTpA, true));
}

TEST_CASE("L5: an NVOD channel compares the lower 32 bits of the transponders", "[zapit][livetp]")
{
	// Same lower half: the frontend would not retune, so not displaced.
	REQUIRE_FALSE(livetp::displaced(false, true, true, kTpA2, kTpA, true));
	// The same pair counts as displaced for a channel that is not NVOD.
	REQUIRE(livetp::displaced(false, true, true, kTpA2, kTpA, false));
	// Another lower half is displaced for NVOD as well.
	REQUIRE(livetp::displaced(false, true, true, kTpB, kTpA, true));
}
