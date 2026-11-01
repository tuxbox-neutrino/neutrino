/*
 * recordtuner.h - the frontend a recording takes
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
 * The decisions CRecordManager::CutBackNeutrino takes about the live
 * frontend, as plain functions over what it reads off the box, so that
 * they can be run where no tuner is.
 *
 * The live channel id does not say where the live frontend stands. A wake
 * from standby whose zap found no frontend leaves both as they were, and an
 * EPG scan in standby sets the live channel id to the channel it reads
 * without moving the live frontend. So whether the live frontend carries a
 * channel is read off the frontend: tuned, and on the channel's transponder.
 * The frontend's own sameTsidOnid() does not answer that: it is true for a
 * frontend whose transponder id is still 0, which it is after opening and
 * until it is tuned.
 *
 * A recording comes before streams. When it finds no frontend, zapit ends
 * the streams in its way (CMD_STREAM_YIELD) and the recording stops them:
 * first those on frontends only streams use, the picture kept; only then is
 * the live frontend taken, as it always was, with the streams on it. In
 * standby there is no picture to keep, and the streams end before zapit
 * wakes - the wake's zap would find no frontend otherwise - and never after.
 * Nobody waits for zapit with the frontend manager's lock or the live
 * frontend's lock held: zapit's thread takes the first and reads the use
 * count the second raises. Whether the live frontend carries the recorded
 * live channel is asked under the frontend manager's lock; it reads fields
 * of zapit and of the frontend and takes no lock. The live frontend is read
 * once for that question and the first try beside it, so that a zap in
 * between cannot make the two name different frontends, and read again
 * under the lock each time it is locked or allocated beside after that: a
 * zap while zapit ended the streams may have moved the picture to another
 * frontend.
 */

#ifndef __recordtuner_h__
#define __recordtuner_h__

#include <stdint.h>

namespace recordtuner
{

/* Whether the live frontend carries the transponder channel_tp: there is
   one, it is tuned, and to that transponder. */
bool liveServes(bool present, bool tuned, uint64_t tsid_onid, uint64_t channel_tp);

/* Whether the recording's frontend needs a full zap, which changes what the
   box shows: it is the live frontend, the live channel is a broadcast one,
   and the live frontend does not carry the recording's transponder. A
   recording on another frontend, or beside a web channel, is only tuned. */
bool fullZap(bool live_is_frontend, bool live_web, bool live_serves);

/* What the choice of a recording's frontend acts on. The recording manager
   implements it over the box, a case over a model. */
class Ops
{
public:
	virtual ~Ops() {}

	/* zapit leaves standby, which zaps to the live channel, and is waited
	   for. */
	virtual void wake() = 0;
	/* Under the frontend manager's lock: whether the live channel is the
	   recorded one and fe, the live frontend read just before, carries it. */
	virtual bool liveRecords(void *fe) = 0;

	virtual void lockFrontends() = 0;    // the frontend manager's lock
	virtual void unlockFrontends() = 0;
	// Under the frontend manager's lock: the live frontend at the time of the call.
	virtual void *liveFrontend() = 0;
	// A frontend's use count, for the live frontend read just before.
	virtual void lockFrontend(void *fe) = 0;
	virtual void unlockFrontend(void *fe) = 0;
	/* Under the frontend manager's lock: a frontend for the recording, as
	   allocateFE finds one; false when none is free. */
	virtual bool allocate() = 0;

	/* zapit ends the stream holds the recording needs; false when no whole
	   answer came. stage as fesearch::StreamsToEnd, mask the frontend
	   numbers whose holds were taken back. */
	virtual bool yield(int &stage, uint32_t &mask) = 0;
	// Stops the streams on the frontends in mask.
	virtual void stopStreamsOn(uint32_t mask) = 0;
};

enum Way
{
	NoFrontend,
	LiveFrontend,   // the live frontend, which carries the recorded live channel
	Allocated       // the frontend allocate found
};

struct Choice
{
	Way  way;
	/* The live frontend as last read: the one the recording's frontend was
	   found beside, and the one live_locked refers to. */
	void *live;
	/* That live frontend is still locked, as the try that found the
	   recording's frontend left it; the caller unlocks it once the
	   recording's zap is done. */
	bool live_locked;
	// Streams were stopped for the recording, whether or not it got a frontend.
	bool streams_ended;
};

/* The frontend for a recording, in the order above. standby: the box is in
   standby; wake: zapit is to be woken for it. */
Choice choose(Ops &ops, bool standby, bool wake);

} // namespace recordtuner

#endif
