/*
 * livetp.h - whether the live channel's tuner stands on its transponder
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
 *
 * A stream can hold the live frontend on another transponder, for
 * instance one set up while the box was in standby. A zap to the live
 * channel then finds no frontend, the live channel stays what it was, and
 * the box shows nothing. A zap to the channel shown is otherwise taken for
 * one that changes nothing; this decides, from values read off the box,
 * when it is not, and when the end of the streams brings the picture back
 * by itself.
 */

#ifndef __livetp_h__
#define __livetp_h__

#include <stdint.h>

namespace livetp
{

/* Whether the live channel is displaced from its tuner: it is a broadcast
   channel (web: false), its frontend is there (present) and tuned, but to
   another transponder than the channel's. NVOD channels compare the lower
   32 bits of both transponder ids, as the frontend does when it decides
   whether a zap changes the transponder. A frontend without signal, or no
   frontend at all, is not displaced. */
bool displaced(bool web, bool present, bool tuned, uint64_t fe_tp, uint64_t channel_tp, bool nvod);

/* Whether the end of the streams asks zapit to give their holds back and
   look again: the box is awake and shows a broadcast channel (TV or radio
   mode, not a file, not a web channel), and the live channel is
   displaced. Read before zapit is asked, so an end of streams with the
   picture in place costs nothing. */
bool rezapWanted(bool awake, bool broadcast_mode, bool displaced);

/* The channel to zap back to: the one the box was last asked to show
   (asked), when it is a broadcast channel (not asked_web) the channel list
   holds (asked_listed), since a zap that found no frontend leaves zapit's
   live channel at the one before it; otherwise zapit's live channel. */
uint64_t rezapTarget(uint64_t asked, bool asked_web, bool asked_listed, uint64_t live);

/* The zap back once the stream that took the live tuner ended, tried once
   per displacement: the channel zapped to and the transponder the live
   frontend stands on. A stream server that refuses a client announces the
   end of streams without a start, and a zap that fails again would
   otherwise be tried on each of those. The mark is forgotten when a stream
   starts, when a zap to the live channel completes, and when the box goes
   into or out of standby; the same displacement later, after any of
   those, is tried again. */
class Rezap
{
public:
	enum Step
	{
		Nothing,  // no zap
		Zap,      // zap now; the try is marked
		Later     // a zap of the program is pending: ask again once it is over
	};

	Rezap() : marked(false), mark_channel(0), mark_tp(0) {}

	/* What to do at this end of the streams, read after zapit gave the
	   ended streams' holds back. Nothing unless rezapWanted() holds, no
	   stream runs or is being set up (streaming; its end announces itself
	   again) and this displacement was not tried. Later while the zap the
	   program sent last is pending (zap_pending): a zap sent then is
	   dropped, so the try is not marked. Zap otherwise, which marks the
	   try, also when the zap then fails. */
	Step next(bool awake, bool broadcast_mode, bool displaced, bool streaming,
		  bool zap_pending, uint64_t channel, uint64_t fe_tp);
	void reset() { marked = false; }

private:
	bool     marked;
	uint64_t mark_channel;
	uint64_t mark_tp;
};

} // namespace livetp

#endif
