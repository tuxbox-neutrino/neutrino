/*
 * test_recordtuner.cpp - the frontend a recording takes
 *
 * Copyright (C) 2026 Thilo Graf
 *
 * License: GPL-2.0-or-later
 *
 * recordtuner holds what CutBackNeutrino decides about the live frontend,
 * as plain functions. These cases drive the shipped object with the values
 * the recording reads off the box: whether the live frontend carries the
 * recording's transponder, and whether the recording then needs a full zap.
 */

#include "support/catch.hpp"

#include <driver/recordtuner.h>

namespace
{

const uint64_t kTpArd = 0x0001044100000000ULL;
const uint64_t kTpZdf = 0x000103f300000000ULL;

} // namespace

TEST_CASE("L1: an untuned live frontend whose transponder id is still 0 carries nothing", "[driver][recordtuner]")
{
	// What opening leaves: id 0, not tuned. sameTsidOnid() would say yes.
	REQUIRE_FALSE(recordtuner::liveServes(true, false, 0, kTpArd));
	// Tuned, but the id was never set: still not the channel's transponder.
	REQUIRE_FALSE(recordtuner::liveServes(true, true, 0, kTpArd));
	// No live frontend at all.
	REQUIRE_FALSE(recordtuner::liveServes(false, true, kTpArd, kTpArd));
}

TEST_CASE("L2: a live frontend tuned to another transponder does not carry the channel", "[driver][recordtuner]")
{
	REQUIRE_FALSE(recordtuner::liveServes(true, true, kTpZdf, kTpArd));
	// The transponder it was on before a failed tune counts for nothing.
	REQUIRE_FALSE(recordtuner::liveServes(true, false, kTpArd, kTpArd));
}

TEST_CASE("L3: a live frontend tuned to the channel's transponder carries it", "[driver][recordtuner]")
{
	REQUIRE(recordtuner::liveServes(true, true, kTpArd, kTpArd));
}

TEST_CASE("L4: the live frontend that does not carry the recording needs a full zap", "[driver][recordtuner]")
{
	/* The live channel id and the recorded one name the same transponder,
	   but the live frontend sits on another: the ids say nothing here. */
	const bool serves = recordtuner::liveServes(true, true, kTpZdf, kTpArd);
	REQUIRE(recordtuner::fullZap(true, false, serves));
	// Carrying it, the recording is only tuned.
	REQUIRE_FALSE(recordtuner::fullZap(true, false, recordtuner::liveServes(true, true, kTpArd, kTpArd)));
	// On another frontend the picture is never touched.
	REQUIRE_FALSE(recordtuner::fullZap(false, false, serves));
}

TEST_CASE("L5: beside a web channel the recording's frontend is only tuned", "[driver][recordtuner]")
{
	REQUIRE_FALSE(recordtuner::fullZap(true, true, false));
	REQUIRE_FALSE(recordtuner::fullZap(true, true, true));
}
