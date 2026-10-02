/*
 * timerdannounce.cpp - the announce time a timer counts with
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

#include <timerdclient/timerdannounce.h>

time_t timerAnnounceTime(CTimerd::CTimerEventTypes type, time_t announce, time_t alarm)
{
	if (announce > 0 && announce <= alarm)
		return announce;
	// The leads the timer list uses.
	return alarm - ((type == CTimerd::TIMER_RECORD) ? 3 * 60 : 60);
}
