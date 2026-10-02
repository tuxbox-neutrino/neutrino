/*
 * timerwake.h - the moment a timer needs the box up for
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

#ifndef __neutrino_timerwake__
#define __neutrino_timerwake__

#include <time.h>

#include <timerdclient/timerdannounce.h>

class CTimerWake
{
	bool   any;
	time_t first;
	bool   rec;

 public:
	CTimerWake() : any(false), first(0), rec(false) {}

	void add(CTimerd::CTimerEventTypes type, time_t announce, time_t alarm, bool is_rec = false);

	bool found() const { return any; }
	time_t earliest() const { return first; }
	bool isRec() const { return rec; }

	// 0 is no wake.
	time_t wakeMinutes() const;
	bool dueWithin(time_t now, time_t span) const;
};

#endif
