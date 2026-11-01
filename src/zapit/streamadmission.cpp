/*
 * streamadmission.cpp - whether a stream may have a tuner, and which way
 *
 * Copyright (C) 2026 Thilo Graf
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

#include "streamadmission.h"

namespace streamadmission
{

Facts::Facts()
	: variant(Plain), standby(false), live_dvb(false), live_tuned(false),
	  live_channel(0), live_fe(-1), slots(0), avinput_pip(false), recording_starting(false)
{
	for (int i = 0; i < kMaxSlots; ++i)
	{
		slot[i].active = false;
		slot[i].channel = 0;
		slot[i].fe = -1;
	}
}

namespace
{

int slotsOf(const Facts &f)
{
	if (f.slots < 0)
		return 0;
	return f.slots > kMaxSlots ? kMaxSlots : f.slots;
}

bool anyPip(const Facts &f)
{
	if (f.avinput_pip)
		return true;
	for (int i = 0; i < slotsOf(f); ++i)
		if (f.slot[i].active)
			return true;
	return false;
}

} // namespace

bool liveRuns(const Facts &f)
{
	return f.live_dvb && !f.standby && f.live_tuned && f.live_fe >= 0;
}

Way decide(const Facts &f, uint64_t channel)
{
	if (f.variant == QuadPip || f.recording_starting)
		return Refuse;

	// The PiP shows it: a stream on its frontend could only share or retune.
	for (int i = 0; i < slotsOf(f); ++i)
		if (f.slot[i].active && f.slot[i].channel == channel)
			return Refuse;

	if (f.variant == Cst && anyPip(f))
		return Refuse;

	if (liveRuns(f) && channel == f.live_channel)
	{
		if (f.variant == DynamicDemux && anyPip(f))
			return Refuse;
		/* The stream reads demux live + 1 and sets its source to the live
		   frontend. A PiP slot reading that demux from another frontend, or
		   the AV-input PiP on demux 1, would lose its picture. Refused
		   outright: an idle frontend for the live channel would be a second
		   tuner on the transponder the picture already has. */
		for (int i = 0; i < slotsOf(f); ++i)
			if (f.slot[i].active && i == f.live_fe && f.slot[i].fe != f.live_fe)
				return Refuse;
		if (f.avinput_pip && f.live_fe == 0)
			return Refuse;
		return Live;
	}

	if (f.variant == DynamicDemux)
		return Refuse;
	return Idle;
}

void held(const Facts &f, std::vector<int> &out)
{
	if (f.live_dvb && !f.standby && f.live_fe >= 0)
		out.push_back(f.live_fe);
	for (int i = 0; i < slotsOf(f); ++i)
	{
		if (!f.slot[i].active)
			continue;
		if (f.slot[i].fe >= 0)
			out.push_back(f.slot[i].fe);
		out.push_back(i);
	}
	if (f.avinput_pip)
		out.push_back(0);
}

} // namespace streamadmission
