/*
 * test_timerdannounce.cpp - tests for the announce time a timer counts with
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

#include <timerdclient/timerdannounce.h>

namespace
{

const time_t kAlarm = 1790954280;

} // anonymous namespace

TEST_CASE("an announce time of none becomes the lead the timer list gives", "[timerdannounce]")
{
	CHECK(timerAnnounceTime(CTimerd::TIMER_ZAPTO, 0, kAlarm) == kAlarm - 60);
	CHECK(timerAnnounceTime(CTimerd::TIMER_RECORD, 0, kAlarm) == kAlarm - 180);
	CHECK(timerAnnounceTime(CTimerd::TIMER_EXEC_PLUGIN, 0, kAlarm) == kAlarm - 60);
	CHECK(timerAnnounceTime(CTimerd::TIMER_STANDBY, 0, kAlarm) == kAlarm - 60);

	// What a recording with a margin and no announce time is filed with.
	CHECK(timerAnnounceTime(CTimerd::TIMER_RECORD, -300, kAlarm) == kAlarm - 180);

	// Moved past by a change to the alarm alone.
	CHECK(timerAnnounceTime(CTimerd::TIMER_ZAPTO, kAlarm + 300, kAlarm) == kAlarm - 60);
}

TEST_CASE("an announce time of its own is kept, up to the alarm itself", "[timerdannounce]")
{
	CHECK(timerAnnounceTime(CTimerd::TIMER_RECORD, kAlarm - 600, kAlarm) == kAlarm - 600);
	CHECK(timerAnnounceTime(CTimerd::TIMER_ZAPTO, kAlarm, kAlarm) == kAlarm);
}
