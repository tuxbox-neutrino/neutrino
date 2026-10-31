/*
 * streamholdcmd.h - zapit's command that holds a frontend for a stream
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
 * The order of one hold, apart from what it acts on. zapit runs it on its
 * own thread, so a hold, a zap and a PiP start never overlap; the facts,
 * the decision, the search and the binding happen in one hold of the
 * frontend manager's lock, so nothing another thread changes under that
 * lock can come between them. Tuning and reading PAT and PMT happen after
 * the lock is given up: nobody may wait for a tuner while holding it.
 *
 * A probe asks the same question and changes nothing: it binds no
 * frontend, locks none, opens none in standby and enters no hold.
 */

#ifndef __zapit_streamholdcmd_h__
#define __zapit_streamholdcmd_h__

#include <stdint.h>
#include <vector>

#include "streamadmission.h"

namespace streamholdcmd
{

/* What a hold or a probe answers. The values travel in zapit's reply. */
enum Verdict
{
	Granted = 0,
	// No frontend may be had for it without changing what the box shows.
	Busy = 1,
	// No such channel, or a web channel, which needs no frontend.
	NotFound = 2,
	// zapit is stopping, the time ran out, or the frontend did not tune.
	Failed = 3
};

struct Reply
{
	int32_t  verdict;
	uint32_t hold_id;   // 0 for a probe and for a refusal
	int32_t  fenumber;  // the frontend, -1 for a refusal
};

enum Kind
{
	NoChannel,
	WebChannel,
	Broadcast
};

/* What a hold acts on. zapit implements it over itself, the frontend
   manager and the register; a case implements it over a model. A frontend
   is the manager's object, passed through as it is. */
class Ops
{
public:
	virtual ~Ops() {}

	virtual void collect() = 0;          // the register's collect
	virtual bool serving() = 0;          // zapit has not begun to stop
	virtual int64_t nowMs() = 0;         // the clock a deadline is on
	virtual Kind channel(uint64_t id) = 0;
	virtual bool roomForHold() = 0;      // the register can take one more

	virtual void lockFrontends() = 0;
	virtual void unlockFrontends() = 0;

	/* Under the frontend manager's lock: */
	virtual void facts(streamadmission::Facts &f) = 0;
	virtual void heldByStreams(std::vector<int> &out) = 0;
	virtual void *holdLive(bool lock) = 0;
	virtual void openFrontends() = 0;
	virtual void *pickIdle(const std::vector<int> &held, bool bind, bool lock) = 0;

	virtual int number(void *fe) = 0;
	virtual bool tune(void *fe) = 0;     // on the bound frontend, PAT and PMT read
	virtual void unlockFrontend(void *fe) = 0;
	virtual uint32_t offer(void *fe) = 0;
	virtual void drop(uint32_t id) = 0;
	virtual bool answer(const Reply &r) = 0;
};

/* One hold or probe of a channel, answered through ops. deadline_ms is on
   ops.nowMs(); 0 means none. With lock_frontend the frontend's use count
   carries the hold as well, which is what keeps a recording off it. Answers
   what it sent. */
Reply run(Ops &ops, uint64_t channel, int64_t deadline_ms, bool probe, bool lock_frontend);

} // namespace streamholdcmd

#endif
