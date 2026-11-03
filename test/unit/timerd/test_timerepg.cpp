/*
 * test_timerepg.cpp - the moment of a timer the guide event is looked up at
 *
 * Copyright (C) 2026 Tuxbox-Project (guide event within the margin, tests)
 *
 * License: GPL-2.0-or-later
 *
 * A recording timer names the guide event that runs in the middle of the
 * time it records. These cases drive the shipped object with the margins
 * a box sets and check which of two events in a row that moment falls in.
 */

#include "support/catch.hpp"

#include <stdint.h>
#include <time.h>

#include <timerd/timerepg.h>

namespace
{

const time_t kMin = 60;
const time_t kT0 = 1762200000;	// any moment on the minute

// An event from start to stop (exclusive of the next one's start).
bool within(time_t t, time_t start, time_t stop)
{
	return t >= start && t < stop;
}

} // namespace

TEST_CASE("E1: a short event recorded with the margins is found, not the one after it", "[timerd][timerepg]")
{
	// A 10-minute event, pre 2 and post 15 minutes: the whole time runs
	// from 2 minutes before to 15 minutes after, its middle in the next
	// event.
	const time_t start = kT0, stop = kT0 + 10 * kMin;
	const int pre = 2 * kMin, post = 15 * kMin;
	const time_t alarm = start - pre, end = stop + post;
	REQUIRE_FALSE(within(alarm / 2 + end / 2, start, stop));
	REQUIRE(within(timerepg::checkTime(alarm, end, true, pre, post), start, stop));
}

TEST_CASE("E2: a quick recording near the end of an event finds the event it was made in", "[timerd][timerepg]")
{
	// Made 3 minutes before the end with pre 2 and post 10: it starts now,
	// the margin before already past.
	const time_t start = kT0 - 60 * kMin, stop = kT0 + 3 * kMin;
	const int pre = 2 * kMin, post = 10 * kMin;
	const time_t alarm = kT0 - pre, end = stop + post;
	REQUIRE_FALSE(within(alarm / 2 + end / 2, start, stop));
	REQUIRE(within(timerepg::checkTime(alarm, end, true, pre, post), start, stop));
}

TEST_CASE("E3: a timer without the margin keeps the middle of its time", "[timerd][timerepg]")
{
	const time_t alarm = kT0, end = kT0 + 90 * kMin;
	REQUIRE(timerepg::checkTime(alarm, end, false, 2 * kMin, 15 * kMin) == alarm / 2 + end / 2);
	// Margins of nothing change nothing either.
	REQUIRE(timerepg::checkTime(alarm, end, true, 0, 0) == alarm / 2 + end / 2);
}

TEST_CASE("E4: a time that the margins leave empty or reversed falls back to the middle", "[timerd][timerepg]")
{
	// Shorter than the margins, for instance after the margins were raised.
	const time_t alarm = kT0, end = kT0 + 10 * kMin;
	REQUIRE(timerepg::checkTime(alarm, end, true, 5 * kMin, 10 * kMin) == alarm / 2 + end / 2);
	REQUIRE(timerepg::checkTime(alarm, end, true, 6 * kMin, 6 * kMin) == alarm / 2 + end / 2);
	// What is left of the time is still taken while it is not empty.
	REQUIRE(timerepg::checkTime(alarm, end, true, 5 * kMin, 4 * kMin) == (alarm + 5 * kMin) / 2 + (alarm + 6 * kMin) / 2);
}

TEST_CASE("E5: near the end of a 32-bit time the margin is not taken off past it", "[timerd][timerepg]")
{
	/* Stored with no margin, then the margin before raised to 10 min: the
	   time asked for would start past the end of a 32-bit time, so it is
	   empty. Wrapped round instead, it would start in 1901 and the middle
	   would be decades away. */
	const int32_t alarm = INT32_MAX - 100, stop = INT32_MAX - 40;
	const int32_t whole = alarm / 2 + stop / 2;
	REQUIRE(timerepg::checkTimeFor<int32_t>(alarm, stop, true, 600, 0) == whole);
	REQUIRE(timerepg::checkTimeFor<int64_t>(alarm, stop, true, 600, 0) == whole);
	// The box's own time_t, whatever its width.
	REQUIRE(timerepg::checkTime((time_t) alarm, (time_t) stop, true, 600, 0) == (time_t) whole);
}

TEST_CASE("E6: a time asked for that reaches the end of a 32-bit time is taken", "[timerd][timerepg]")
{
	const int32_t asked_alarm = INT32_MAX - 1000, asked_stop = INT32_MAX;
	const int32_t middle = asked_alarm / 2 + asked_stop / 2;
	REQUIRE(timerepg::checkTimeFor<int32_t>(asked_alarm - 120, asked_stop, true, 120, 0) == middle);
	REQUIRE(timerepg::checkTimeFor<int32_t>(asked_alarm - 120, INT32_MAX, true, 120, 0) >= asked_alarm);
}

TEST_CASE("E7: a margin that would carry the time past what the type holds falls back", "[timerd][timerepg]")
{
	// A margin below zero moves the end out past INT32_MAX.
	const int32_t alarm = INT32_MAX - 100, stop = INT32_MAX - 10;
	REQUIRE(timerepg::checkTimeFor<int32_t>(alarm, stop, true, -50, -50) == alarm / 2 + stop / 2);
	// And one past the start of the range.
	const int32_t early = INT32_MIN + 10, later = INT32_MIN + 500;
	REQUIRE(timerepg::checkTimeFor<int32_t>(early, later, true, -100, 0) == early / 2 + later / 2);
	REQUIRE(timerepg::checkTimeFor<int32_t>(early, later, true, 0, 600) == early / 2 + later / 2);
	// The same times in 64 bits hold the result and take the margins off.
	REQUIRE(timerepg::checkTimeFor<int64_t>(alarm, stop, true, -50, -50) == ((int64_t) alarm - 50) / 2 + ((int64_t) stop + 50) / 2);
}
