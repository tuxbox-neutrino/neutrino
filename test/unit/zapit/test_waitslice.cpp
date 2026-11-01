/*
 * test_waitslice.cpp - a bounded wait taken in slices
 *
 * Copyright (C) 2026 Thilo Graf
 *
 * License: GPL-2.0-or-later
 *
 * waitslice::nextSliceMs is how long zapit waits at a time for a tuner or
 * in a test delay, so that a stop is seen within a slice. These cases walk
 * the shipped function over whole waits.
 */

#include "support/catch.hpp"

#include <zapit/waitslice.h>

TEST_CASE("S1: a slice is never longer than the slice or than what is left", "[zapit][waitslice]")
{
	for (int64_t total = 0; total <= 450; total += 50)
	{
		for (int64_t elapsed = 0; elapsed <= total; elapsed += 7)
		{
			const int s = waitslice::nextSliceMs(total, elapsed, 100);
			REQUIRE(s >= 0);
			REQUIRE(s <= 100);
			REQUIRE(s <= total - elapsed);
		}
	}
	REQUIRE(waitslice::nextSliceMs(4000, 0, 100) == 100);
	REQUIRE(waitslice::nextSliceMs(4000, 3950, 100) == 50);
}

TEST_CASE("S2: a wait ends when nothing is left, and the slices add up to it", "[zapit][waitslice]")
{
	REQUIRE(waitslice::nextSliceMs(4000, 4000, 100) == 0);
	REQUIRE(waitslice::nextSliceMs(4000, 4001, 100) == 0);
	REQUIRE(waitslice::nextSliceMs(0, 0, 100) == 0);

	int64_t elapsed = 0;
	int slices = 0;
	for (;;)
	{
		const int s = waitslice::nextSliceMs(1234, elapsed, waitslice::kSliceMs);
		if (s == 0)
			break;
		elapsed += s;
		slices++;
		REQUIRE(slices < 100);
	}
	REQUIRE(elapsed == 1234);
	REQUIRE(slices == 13);
}

TEST_CASE("S3: a clock that went backwards counts as no time passed", "[zapit][waitslice]")
{
	REQUIRE(waitslice::nextSliceMs(4000, -500, 100) == 100);
	REQUIRE(waitslice::nextSliceMs(50, -500, 100) == 50);
}
