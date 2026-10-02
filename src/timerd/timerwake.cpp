/*
 * timerwake.cpp - the moment a timer needs the box up for
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

#include "timerwake.h"

void CTimerWake::add(CTimerd::CTimerEventTypes type, time_t announce, time_t alarm, bool is_rec)
{
	const time_t at = timerAnnounceTime(type, announce, alarm);
	if (at <= 0)
		return;

	if (!any || at < first)
	{
		any = true;
		first = at;
		rec = is_rec;
	}
	// On a tie a box left on is the safer miss.
	else if (at == first && !is_rec)
		rec = false;
}

time_t CTimerWake::wakeMinutes() const
{
	if (!any)
		return 0;
	return (first - 3 * 60) / 60;
}

bool CTimerWake::dueWithin(time_t now, time_t span) const
{
	return any && first - now <= span;
}
