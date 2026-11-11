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
 * A stream never changes what the box shows. It gets one of three things:
 *  - Live: the channel the picture shows, on the picture's frontend, which
 *    is not retuned;
 *  - Shared: another channel on the transponder the picture's frontend is
 *    tuned to, on that frontend, which is not retuned either; its PAT and
 *    PMT are read from the transponder as it is;
 *  - Idle: a frontend nobody uses, whose demux (its number + 1) no PiP
 *    reads.
 * Anything else is refused, and a refusal touches neither the picture, nor
 * a PiP, nor a recording. While a recording starts, from before it looks for
 * a frontend until its start has locked one, every stream is refused: the
 * recording may have ended streams for that frontend, and a stream let in
 * meanwhile could take it again.
 *
 * Without dynamic demux, PiP slot i reads demux i + 1 from frontend
 * pip_fe[i], and a stream on frontend f reads demux f + 1 with its source
 * set to f. A stream on frontend i would therefore move the source of the
 * demux PiP slot i reads. The AV-input PiP reads demux 1 from source 2.
 *
 * The facts are read in the same hold of the frontend manager's lock as the
 * frontend is then bound, so the decision and the binding see one state.
 *
 * A stream the CI module would descramble from the frontend of its way is
 * refused, and not sent another way, where its service would be one the
 * module does not descramble yet and the setting allows no other beside
 * those it descrambles (the picture's, a PiP's, a recording's, a stream's,
 * also one a stream was granted and has not asked the CA manager for yet):
 * with ci_mode 2 none, with ci_mode 1 only one on the transponder all of
 * them lie on, and that one only through the live frontend, the one input
 * they come through.
 */

#ifndef __zapit_streamadmission_h__
#define __zapit_streamadmission_h__

#include <stdint.h>
#include <vector>

namespace streamadmission
{

const int kMaxSlots = 3;
// Services the CI module serves that the facts list; past that, ci_more.
const int kMaxCi = 16;

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
	bool     recording_starting;
	/* The box is in standby, while zapit may still be awake: for the EPG
	   scan, for a stream or a recording that ran when the box went into
	   standby, or woken again by the scan. Nothing is shown then, so no
	   frontend is the picture's. */
	bool     box_standby;
	/* The transponder the live frontend is tuned to (0 when there is none or
	   it was never tuned), and the one the channel asked for lies on. */
	uint64_t live_tp;
	uint64_t channel_tp;
	/* A busy frontend is shared only with the service it carries. */
	bool     no_same_fe;
	/* A channel scan may tune the live frontend, which keeps its transponder
	   id meanwhile: from the scan's start, which clears zapit's current
	   channel, until a zap sets one again. A reload of the channels clears
	   it as well. */
	bool     scanning;
	/* Descrambled by the CI module: the channel the picture shows, and the
	   one asked for, where the stream has the box descramble it. A channel
	   that is not scrambled, or one an e2 stream passes on scrambled, is
	   not. ci_mode is the setting for another such service beside one the
	   module descrambles: 0 allowed, 1 on the same transponder only,
	   anything else not allowed. */
	bool     live_ci;
	bool     channel_ci;
	int      ci_mode;
	/* channel_ci holds from ci_tuner, the tuner the module is bound to
	   (-1: none, every tuner reaches it); from another tuner the module
	   does not descramble the channel. Each way weighs it on its own
	   frontend: Live and Shared on the live one, Idle on stream_fe, the
	   frontend the stream was given, counted as ci_tuner while it has
	   none (-1). */
	int      ci_tuner;
	int      stream_fe;
	/* The services the module descrambles now, by channel and transponder:
	   what the CA manager has it descramble (the picture, a PiP, a
	   recording, a stream), and what a stream's hold promises it before the
	   stream asks the CA manager; ci_streamed: a stream's hold carries it.
	   ci_more: there were more than kMaxCi. Filled through addCiService. */
	int      ci_count;
	uint64_t ci_channel[kMaxCi];
	uint64_t ci_tp[kMaxCi];
	bool     ci_streamed[kMaxCi];
	bool     ci_more;

	Facts();
};

/* Adds a service the CI module descrambles to f, once per channel; one a
   stream's hold carries is marked so, also where the CA manager lists it. */
void addCiService(Facts &f, uint64_t channel, uint64_t tp, bool streamed);

enum Way
{
	Refuse,
	Live,
	Shared,
	Idle
};

Way decide(const Facts &f, uint64_t channel);

// The picture runs from a broadcast channel on a tuned frontend, and the box
// is not in standby.
bool liveRuns(const Facts &f);

/* Whether switching the picture to a channel may be weighed for a stream that
   found no frontend: the picture runs and nothing is under way that a zap would
   disturb or race, a recording starting or a channel scan. Not with PiPs the GUI
   starts past zapit, and not on a Cst box while a PiP runs. */
bool switchable(const Facts &f);

/* What the admission decides for a stream of channel once the picture shows it
   from frontend number zap_fe and no PiP shows it; the live channel
   itself stays on its frontend. Refuse without a frontend. */
Way afterSwitch(const Facts &f, uint64_t channel, int zap_fe);

/* The frontends an idle frontend must not be: the picture's while it runs,
   every PiP slot's frontend, frontend number i for every active slot i, and
   frontend 0 while the AV-input PiP reads demux 1. Frontends that streams
   hold come on top of these. */
void held(const Facts &f, std::vector<int> &out);

} // namespace streamadmission

#endif
