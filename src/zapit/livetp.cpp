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

bool rezapWanted(bool awake, bool broadcast_mode, bool displaced)
{
	return awake && broadcast_mode && displaced;
}

uint64_t rezapTarget(uint64_t asked, bool asked_web, bool asked_listed, uint64_t live)
{
	return asked && !asked_web && asked_listed ? asked : live;
}

Rezap::Step Rezap::next(bool awake, bool broadcast_mode, bool displaced, bool streaming,
			bool zap_pending, uint64_t channel, uint64_t fe_tp)
{
	if (!rezapWanted(awake, broadcast_mode, displaced) || streaming)
		return Nothing;
	if (marked && mark_channel == channel && mark_tp == fe_tp)
		return Nothing;
	if (zap_pending)
		return Later;
	marked = true;
	mark_channel = channel;
	mark_tp = fe_tp;
	return Zap;
}

} // namespace livetp
