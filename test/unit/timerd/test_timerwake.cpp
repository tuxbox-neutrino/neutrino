/*
 * test_timerwake.cpp - tests for the moment a timer needs the box up for
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

#include <time.h>

#include <timerd/timerwake.h>

namespace
{

const CTimerd::CTimerEventTypes kRecord = CTimerd::TIMER_RECORD;
const CTimerd::CTimerEventTypes kZapto = CTimerd::TIMER_ZAPTO;

// A recording made the way the timer list makes one: announced three minutes ahead.
const time_t kRecAlarm = 1790954280;
const time_t kRecAnnounce = kRecAlarm - 180;

// A zap timer with no announce time, a day after the recording.
const time_t kLateZapAlarm = kRecAlarm + 86400;

// A zap timer announced a minute ahead, an hour after the recording.
const time_t kZapAlarm = kRecAlarm + 3600;
const time_t kZapAnnounce = kZapAlarm - 60;

// A zap timer with no announce time, ahead of both.
const time_t kEarlyZapAlarm = kRecAlarm - 4000;

} // anonymous namespace

TEST_CASE("a timer with no announce time wakes the box wherever the list holds it", "[timerwake]")
{
	const time_t expected = kEarlyZapAlarm - 60;

	CTimerWake first;
	first.add(kZapto, 0, kEarlyZapAlarm);
	first.add(kRecord, kRecAnnounce, kRecAlarm);
	first.add(kZapto, kZapAnnounce, kZapAlarm);
	CHECK(first.earliest() == expected);

	CTimerWake middle;
	middle.add(kRecord, kRecAnnounce, kRecAlarm);
	middle.add(kZapto, 0, kEarlyZapAlarm);
	middle.add(kZapto, kZapAnnounce, kZapAlarm);
	CHECK(middle.earliest() == expected);

	CTimerWake last;
	last.add(kRecord, kRecAnnounce, kRecAlarm);
	last.add(kZapto, kZapAnnounce, kZapAlarm);
	last.add(kZapto, 0, kEarlyZapAlarm);
	CHECK(last.earliest() == expected);
}

TEST_CASE("a later timer with no announce time takes no other timer's wake", "[timerwake]")
{
	CTimerWake after_one;
	after_one.add(kRecord, kRecAnnounce, kRecAlarm);
	after_one.add(kZapto, 0, kLateZapAlarm);
	CHECK(after_one.earliest() == kRecAnnounce);
	// What the box writes as its wake time: the announce time less three minutes.
	CHECK(after_one.wakeMinutes() * 60 == 1790953920);

	CTimerWake between_two;
	between_two.add(kRecord, kRecAnnounce, kRecAlarm);
	between_two.add(kZapto, 0, kLateZapAlarm);
	between_two.add(kZapto, kZapAnnounce, kZapAlarm);
	CHECK(between_two.earliest() == kRecAnnounce);
}

TEST_CASE("a negative announce time is no moment in 1969", "[timerwake]")
{
	// A zap announced five minutes ahead, then a recording asked for with a
	// margin of five minutes and no announce time.
	const time_t zap_alarm = 1790954400;
	const time_t guide_alarm = 1790961600 - 300;

	CTimerWake later;
	later.add(kZapto, zap_alarm - 300, zap_alarm);
	later.add(kRecord, -300, guide_alarm);
	CHECK(later.earliest() == zap_alarm - 300);
	CHECK(later.wakeMinutes() * 60 == 1790953920);

	CTimerWake earlier;
	earlier.add(kZapto, kZapAnnounce, kZapAlarm);
	earlier.add(kRecord, -300, kRecAlarm);
	CHECK(earlier.earliest() == kRecAlarm - 180);
}

TEST_CASE("a valid announce time among broken ones is the wake", "[timerwake]")
{
	CTimerWake wake;
	wake.add(kRecord, 0, 0);
	wake.add(kZapto, 0, 60);
	wake.add(kZapto, kZapAnnounce, kZapAlarm);
	wake.add(kRecord, -300, kLateZapAlarm);
	wake.add(kZapto, kLateZapAlarm + 600, kLateZapAlarm);
	REQUIRE(wake.found());
	CHECK(wake.earliest() == kZapAnnounce);
}

TEST_CASE("nothing to wake for is no wake", "[timerwake]")
{
	const time_t now = kRecAlarm;

	CTimerWake none;
	CHECK_FALSE(none.found());
	CHECK(none.wakeMinutes() == 0);
	CHECK_FALSE(none.dueWithin(now, 600));

	// Timers whose times are no moment at all.
	CTimerWake broken;
	broken.add(kRecord, 0, 0);
	broken.add(kZapto, -300, 60);
	CHECK_FALSE(broken.found());
}

TEST_CASE("the box comes up quietly only for a recording, and a tie keeps it on", "[timerwake]")
{
	CTimerWake rec_earlier;
	rec_earlier.add(kRecord, kRecAnnounce, kRecAlarm, true);
	rec_earlier.add(kZapto, kZapAnnounce, kZapAlarm);
	CHECK(rec_earlier.isRec());

	CTimerWake zap_earlier;
	zap_earlier.add(kRecord, kRecAnnounce, kRecAlarm, true);
	zap_earlier.add(kZapto, 0, kEarlyZapAlarm);
	CHECK_FALSE(zap_earlier.isRec());

	// The zap is due at the same moment the recording is.
	const time_t tied_zap_alarm = kRecAnnounce + 60;

	CTimerWake tie_rec_listed_first;
	tie_rec_listed_first.add(kRecord, kRecAnnounce, kRecAlarm, true);
	tie_rec_listed_first.add(kZapto, 0, tied_zap_alarm);
	CHECK(tie_rec_listed_first.earliest() == kRecAnnounce);
	CHECK_FALSE(tie_rec_listed_first.isRec());

	CTimerWake tie_zap_listed_first;
	tie_zap_listed_first.add(kZapto, 0, tied_zap_alarm);
	tie_zap_listed_first.add(kRecord, kRecAnnounce, kRecAlarm, true);
	CHECK(tie_zap_listed_first.earliest() == kRecAnnounce);
	CHECK_FALSE(tie_zap_listed_first.isRec());
}

TEST_CASE("after a timer wake a zap with no announce time holds the power-off back", "[timerwake]")
{
	const time_t now = kRecAlarm;

	CTimerWake soon;
	soon.add(kZapto, 0, now + 480);
	CHECK(soon.dueWithin(now, 600));

	// Announced exactly ten minutes from now, and a second later.
	CTimerWake edge;
	edge.add(kZapto, 0, now + 660);
	CHECK(edge.dueWithin(now, 600));
	CTimerWake past_edge;
	past_edge.add(kZapto, 0, now + 661);
	CHECK_FALSE(past_edge.dueWithin(now, 600));

	// A recording begun by hand is filed with no announce time and is running.
	CTimerWake running;
	running.add(kRecord, 0, now - 1800);
	CHECK(running.dueWithin(now, 600));
}
