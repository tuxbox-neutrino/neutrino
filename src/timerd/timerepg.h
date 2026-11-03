/*
 * timerepg.h - the moment of a timer the guide event is looked up at
 *
 * Copyright (C) 2026 Tuxbox-Project (guide event within the margin)
 *
 * License: GPL-2.0-or-later
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
 */

#ifndef __neutrino_timerepg__
#define __neutrino_timerepg__

#include <limits>

#include <stdint.h>
#include <time.h>

namespace timerepg
{

/* checkTime() for times of type T. The margins are taken off in 64 bits
   and only where T holds the result: with a 32-bit T, a time near its end
   plus a margin would not fit, and the time asked for falls back to the
   middle of the whole time instead. */
template <typename T>
T checkTimeFor(T alarm, T stop, bool safety, int pre, int post)
{
	const T whole = alarm / 2 + stop / 2;
	if (!safety)
		return whole;
	const int64_t lo = (int64_t) std::numeric_limits<T>::min();
	const int64_t hi = (int64_t) std::numeric_limits<T>::max();
	const int64_t a = (int64_t) alarm;
	const int64_t s = (int64_t) stop;
	if (pre > 0 ? a > hi - pre : a < lo - pre)
		return whole;
	if (post > 0 ? s < lo + post : s > hi + post)
		return whole;
	const int64_t asked_alarm = a + pre;
	const int64_t asked_stop = s - post;
	if (asked_stop <= asked_alarm)
		return whole;
	return (T) (asked_alarm / 2 + asked_stop / 2);
}

/* The moment the guide event of a recording is looked up at: the middle of
   the time it records. A recording with the safety margin (safety) starts
   pre seconds before and ends post seconds after the time asked for, so
   the middle of the time asked for is taken; with post larger than pre the
   middle of the whole time can lie in the next event. pre and post come
   from the box's setting, as a timer keeps no record of the margins it was
   made with. A time asked for that is empty, reversed or out of the range
   of time_t falls back to the middle of the whole time. */
time_t checkTime(time_t alarm, time_t stop, bool safety, int pre, int post);

} // namespace timerepg

#endif
