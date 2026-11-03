/*
 * timerepg.cpp - the moment of a timer the guide event is looked up at
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

#include "timerepg.h"

namespace timerepg
{

time_t checkTime(time_t alarm, time_t stop, bool safety, int pre, int post)
{
	return checkTimeFor<time_t>(alarm, stop, safety, pre, post);
}

} // namespace timerepg
