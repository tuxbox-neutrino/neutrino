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
	  live_ci(false), channel_ci(false), ci_mode(0), ci_tuner(-1), stream_fe(-1), ci_count(0),
	  ci_more(false)
{
	for (int i = 0; i < kMaxSlots; ++i)
	{
		slot[i].active = false;
		slot[i].channel = 0;
		slot[i].fe = -1;
	}
	for (int i = 0; i < kMaxCi; ++i)
	{
		ci_channel[i] = 0;
		ci_tp[i] = 0;
		ci_streamed[i] = false;
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

int ciCountOf(const Facts &f)
{
	if (f.ci_count < 0)
		return 0;
	return f.ci_count > kMaxCi ? kMaxCi : f.ci_count;
}

bool ciListed(const Facts &f, uint64_t channel)
{
	for (int i = 0; i < ciCountOf(f); ++i)
		if (f.ci_channel[i] == channel)
			return true;
	return false;
}

// Takes channel out of the services the module serves, unless a stream's
// hold carries it.
void ciLeave(Facts &f, uint64_t channel)
{
	int kept = 0;
	for (int i = 0; i < ciCountOf(f); ++i)
	{
		if (f.ci_channel[i] == channel && !f.ci_streamed[i])
			continue;
		f.ci_channel[kept] = f.ci_channel[i];
		f.ci_tp[kept] = f.ci_tp[i];
		f.ci_streamed[kept] = f.ci_streamed[i];
		kept++;
	}
	f.ci_count = kept;
}

bool pipShows(const Facts &f, uint64_t channel)
{
	for (int i = 0; i < slotsOf(f); ++i)
		if (f.slot[i].active && f.slot[i].channel == channel)
			return true;
	return false;
}

enum CiJoin
{
	CiFree,       // nothing the module serves stands in the way
	CiShareOnly,  // only on the live frontend, on the transponder of all it serves
	CiNever
};

/* Whether the module would descramble the channel asked for from frontend
   fe: one the module is bound to, or any where it is bound to none; a
   frontend not picked yet (-1) counts as the bound one. */
bool ciFrom(const Facts &f, int fe)
{
	return f.channel_ci && (f.ci_tuner < 0 || fe < 0 || fe == f.ci_tuner);
}

/* Whether channel may come to the CI module beside the services it serves,
   from frontend fe. picture: the live channel counts as served while the
   picture runs; not where the picture is about to show channel itself. */
CiJoin ciJoin(const Facts &f, uint64_t channel, bool picture, int fe)
{
	if (!ciFrom(f, fe) || f.ci_mode == 0)
		return CiFree;
	const bool shown = picture && liveRuns(f) && f.live_ci;
	// A service the module serves already adds none.
	if ((shown && channel == f.live_channel) || ciListed(f, channel))
		return CiFree;
	const bool other = shown || f.ci_more || ciCountOf(f) > 0;
	bool elsewhere = f.ci_more || (shown && f.live_tp != f.channel_tp);
	for (int i = 0; i < ciCountOf(f); ++i)
		if (f.ci_tp[i] != f.channel_tp)
			elsewhere = true;
	if (!other)
		return CiFree;
	if (f.ci_mode == 1 && !elsewhere)
		return CiShareOnly;
	return CiNever;
}

} // namespace

void addCiService(Facts &f, uint64_t channel, uint64_t tp, bool streamed)
{
	for (int i = 0; i < ciCountOf(f); ++i)
	{
		if (f.ci_channel[i] == channel)
		{
			f.ci_streamed[i] = f.ci_streamed[i] || streamed;
			return;
		}
	}
	if (ciCountOf(f) == kMaxCi)
	{
		f.ci_more = true;
		return;
	}
	const int i = ciCountOf(f);
	f.ci_channel[i] = channel;
	f.ci_tp[i] = tp;
	f.ci_streamed[i] = streamed;
	f.ci_count = i + 1;
}

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

	/* A CI channel the module may not serve beside what it serves takes
	   no frontend at all, the live one, a shared one or an idle one. Each
	   way asks it for the frontend it gives the stream, which need not be
	   the tuner the module is bound to. */
	const CiJoin on_live = ciJoin(f, channel, true, f.live_fe);
	const CiJoin on_own = ciJoin(f, channel, true, f.stream_fe);

	if (liveRuns(f) && channel == f.live_channel)
	{
		if (on_live == CiNever)
			return Refuse;
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
	   refuses until the next zap). Where it may not share, the channel is
	   any other channel and goes on below, as it binds only its own
	   demuxes. */
	if (liveRuns(f) && f.live_tp != 0 && f.channel_tp == f.live_tp && !f.scanning &&
	    f.variant != DynamicDemux && !f.no_same_fe && liveDemuxFree(f))
		return on_live == CiNever ? Refuse : Shared;

	/* An idle frontend takes it only where nothing the module serves stands
	   in the way: where only one on the same transponder may join the
	   module, it joins on the live frontend, as another frontend tuned there
	   would be another input for the module. With dynamic demux no frontend
	   is idle. */
	if (on_own != CiFree || f.variant == DynamicDemux)
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
	if (channel != f.live_channel)
	{
		/* The module stops serving the channel the picture leaves, unless a
		   PiP still shows it or a stream's hold carries it. The zap brings
		   the channel asked for to it as the picture's, from the frontend
		   the zap takes, which the setting has to allow beside the services
		   that stay. */
		if (!pipShows(g, f.live_channel))
			ciLeave(g, f.live_channel);
		if (ciJoin(g, channel, false, zap_fe) != CiFree)
			return Refuse;
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
