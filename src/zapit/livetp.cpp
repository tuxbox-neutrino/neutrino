/*
 * livetp.cpp - whether the live channel's tuner stands on its transponder
 *
 * Copyright (C) 2026 Tuxbox-Project (live tuner taken)
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

#include "livetp.h"

namespace livetp
{

bool displaced(bool web, bool present, bool tuned, uint64_t fe_tp, uint64_t channel_tp, bool nvod)
{
	if (web || !present || !tuned)
		return false;
	if (nvod)
		return (fe_tp & 0xFFFFFFFFULL) != (channel_tp & 0xFFFFFFFFULL);
	return fe_tp != channel_tp;
}

} // namespace livetp
