/*
 * timerdannounce.h - the announce time a timer counts with
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

#ifndef __timerdannounce__
#define __timerdannounce__

#include <time.h>

#include <timerdclient/timerdtypes.h>

// A stored announce time of 0, below 0 or after the alarm stands for none.
time_t timerAnnounceTime(CTimerd::CTimerEventTypes type, time_t announce, time_t alarm);

#endif
