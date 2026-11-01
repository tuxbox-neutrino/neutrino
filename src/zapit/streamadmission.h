/*
 * streamadmission.h - whether a stream may have a tuner, and which way
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
 *
 * A stream never changes what the box shows. It gets one of two things:
 *  - Live: the channel the picture shows, on the picture's frontend, which
 *    is not retuned;
 *  - Idle: a frontend nobody uses, whose demux (its number + 1) no PiP
 *    reads.
 * Anything else is refused, and a refusal touches neither the picture, nor
 * a PiP, nor a recording.
 *
 * Without dynamic demux, PiP slot i reads demux i + 1 from frontend
 * pip_fe[i], and a stream on frontend f reads demux f + 1 with its source
 * set to f. A stream on frontend i would therefore move the source of the
 * demux PiP slot i reads. The AV-input PiP reads demux 1 from source 2.
 *
 * The facts are read in the same hold of the frontend manager's lock as the
 * frontend is then bound, so the decision and the binding see one state.
 */

#ifndef __zapit_streamadmission_h__
#define __zapit_streamadmission_h__

#include <stdint.h>
#include <vector>

namespace streamadmission
{

const int kMaxSlots = 3;

enum Variant
{
	Plain,         // fixed demux, PiP through zapit only
	QuadPip,       // the GUI starts PiPs past zapit's thread: no streams
	DynamicDemux,  // demux numbers are handed out: no idle frontend
	Cst            // the PiP demux is set up apart from the frontend number
};

struct Slot
{
	bool     active;   // its frontend is set
	uint64_t channel;
	int      fe;       // frontend number, -1 when none
};

struct Facts
{
	Variant  variant;
	bool     standby;
	bool     live_dvb;       // a live channel is set and it is not a web channel
	bool     live_tuned;
	uint64_t live_channel;
	int      live_fe;        // frontend number, -1 when none
	int      slots;          // PiP slots this box has, at most kMaxSlots
	Slot     slot[kMaxSlots];
	bool     avinput_pip;

	Facts();
};

enum Way
{
	Refuse,
	Live,
	Idle
};

Way decide(const Facts &f, uint64_t channel);

// The picture runs from a broadcast channel on a tuned frontend.
bool liveRuns(const Facts &f);

/* The frontends an idle frontend must not be: the picture's while it runs,
   every PiP slot's frontend, frontend number i for every active slot i, and
   frontend 0 while the AV-input PiP reads demux 1. Frontends that streams
   hold come on top of these. */
void held(const Facts &f, std::vector<int> &out);

} // namespace streamadmission

#endif
