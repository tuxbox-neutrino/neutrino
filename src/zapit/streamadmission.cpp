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
	  live_channel(0), live_fe(-1), slots(0), avinput_pip(false), recording_starting(false),
	  box_standby(false), live_tp(0), channel_tp(0), no_same_fe(false), scanning(false),
	  live_ci(false), channel_ci(false), ci_one_service(false)
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

/* The stream reads demux live + 1 and sets its source to the live frontend.
   A PiP slot reading that demux from another frontend, or the AV-input PiP
   on demux 1, would lose its picture. */
bool liveDemuxFree(const Facts &f)
{
	for (int i = 0; i < slotsOf(f); ++i)
		if (f.slot[i].active && i == f.live_fe && f.slot[i].fe != f.live_fe)
			return false;
	return !(f.avinput_pip && f.live_fe == 0);
}

} // namespace

bool liveRuns(const Facts &f)
{
	return f.live_dvb && !(f.standby || f.box_standby) && f.live_tuned && f.live_fe >= 0;
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
		/* Refused outright when the live demux is not free: an idle
		   frontend for the live channel would be a second tuner on the
		   transponder the picture already has, and binding it would move
		   the live channel's own demuxes to that frontend. */
		if (!liveDemuxFree(f))
			return Refuse;
		return Live;
	}

	/* Another channel on the live frontend's transponder. With dynamic
	   demux the live channel reads the demux its zap was given, and
	   nothing binds one for another channel; with no_same_fe the live
	   frontend carries only its own service; while a scan runs, the
	   transponder id need not be the one it is tuned to (a tune outside
	   a zap, as the signal test and motor control send, does not wait
	   for the lock and leaves the frontend untuned, which liveRuns
	   refuses until the next zap); and where the CI
	   module may serve one channel marked for it only, a second such channel
	   would take it from the picture. Where it may not share, the channel is
	   any other channel and goes on below, as it binds only its own
	   demuxes. */
	if (liveRuns(f) && f.live_tp != 0 && f.channel_tp == f.live_tp && !f.scanning &&
	    f.variant != DynamicDemux && !f.no_same_fe && liveDemuxFree(f) &&
	    !(f.ci_one_service && f.live_ci && f.channel_ci))
		return Shared;

	if (f.variant == DynamicDemux)
		return Refuse;
	return Idle;
}

bool switchable(const Facts &f)
{
	if (f.variant == QuadPip || f.recording_starting || f.scanning)
		return false;
	if (f.variant == Cst && anyPip(f))
		return false;
	return liveRuns(f);
}

Way afterSwitch(const Facts &f, uint64_t channel, int zap_fe)
{
	Facts g = f;
	if (channel != f.live_channel)
	{
		if (zap_fe < 0)
			return Refuse;
		g.live_channel = channel;
		g.live_fe = zap_fe;
		g.live_tp = f.channel_tp;
		g.live_tuned = true;
		g.live_ci = f.channel_ci;
	}
	for (int i = 0; i < slotsOf(g); ++i)
	{
		if (g.slot[i].active && g.slot[i].channel == channel)
		{
			g.slot[i].active = false;
			g.slot[i].channel = 0;
			g.slot[i].fe = -1;
		}
	}
	return decide(g, channel);
}

void held(const Facts &f, std::vector<int> &out)
{
	if (f.live_dvb && !(f.standby || f.box_standby) && f.live_fe >= 0)
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
