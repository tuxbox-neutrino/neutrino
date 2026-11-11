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
 *
 * Asked, a refused hold or probe also says whether switching the picture to
 * the channel would free a frontend for it. That is a prediction under the
 * same lock and changes nothing either. The switch itself is a command of its
 * own (runSwitch), which only the GUI sends, and only where a setting allows
 * it: the GUI thread, where every recording starts, waits for it.
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
	/* 1 for a refusal, asked with ask_switch, that switching the picture to
	   the channel would turn into a hold of the picture's frontend; else 0. */
	int32_t  switch_frees;
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
	/* The number of the frontend a zap of the channel would take, found the
	   way the zap finds it with the frontends in keep counted as busy, or -1;
	   keep receives those frontends, every stream hold's and every PiP's.
	   turns_dish says that zap may drive a motor. Leaves every frontend as it
	   found it. */
	virtual int zapFrontend(std::vector<void *> &keep, bool &turns_dish) = 0;

	virtual int number(void *fe) = 0;
	/* On the bound frontend, PAT and PMT read: what the PMT says of the
	   channel, scrambled or not, is in the facts that follow. */
	virtual bool tune(void *fe) = 0;
	/* PAT and PMT read on the live frontend as it stands, for another
	   channel on its transponder; never tunes. */
	virtual bool readPids(void *fe) = 0;
	virtual void unlockFrontend(void *fe) = 0;
	virtual uint32_t offer(void *fe) = 0;
	virtual void drop(uint32_t id) = 0;
	virtual bool answer(const Reply &r) = 0;
};

/* One hold or probe of a channel, answered through ops. deadline_ms is on
   ops.nowMs(); 0 means none. With lock_frontend the frontend's use count
   carries the hold as well, which is what keeps a recording off it. With
   ask_switch a refusal says whether switching the picture would free a
   frontend. Answers what it sent. */
Reply run(Ops &ops, uint64_t channel, int64_t deadline_ms, bool probe, bool lock_frontend,
	  bool ask_switch = false);

/* Under the frontend manager's lock, after a refusal: whether switching the
   picture to the channel would let a stream of it have the picture's
   frontend. The picture runs and nothing is under way (switchable); a channel
   other than the live one needs a frontend the zap finds with every stream
   and PiP kept, that carries no stream hold and turns no dish; and once the
   picture shows the channel from it and no PiP does, the admission says Live. */
bool switchFrees(Ops &ops, const streamadmission::Facts &f, uint64_t channel, std::vector<void *> *keep = 0);

/* The switch itself: what the GUI asks for a stream that found no frontend. */
class SwitchOps : public Ops
{
public:
	/* Not under the frontend manager's lock: */
	virtual void stopPipsShowing(uint64_t channel) = 0;   // every PiP slot that shows it, no other
	/* Zaps the picture to the channel the ordinary way, with the frontends
	   in keep, the ones the prediction counted busy, kept busy for as long,
	   and without stopping a PiP to free a frontend. Whether it zapped. */
	virtual bool zapKept(uint64_t channel, const std::vector<void *> &keep) = 0;
	virtual bool answerSwitch(int32_t switched) = 0;
};

/* The ordinary admission and the prediction again, on fresh facts: where a
   hold would get a frontend as things are, nothing is switched and the answer
   is 2; where the prediction holds, the picture is zapped to a channel other
   than the live one, the PiP slots showing the channel stop once it shows it,
   and the answer is 1. A zap that fails zaps back to the channel shown before
   and stops no PiP. Otherwise 0, and nothing changed. */
int32_t runSwitch(SwitchOps &ops, uint64_t channel);

/* A reply as it goes on the wire, member for member; Wire is zapit's reply
   message, which this header does not know. */
template <typename Wire>
Wire toWire(const Reply &r)
{
	Wire w;
	w.verdict = r.verdict;
	w.hold_id = r.hold_id;
	w.fenumber = r.fenumber;
	w.switch_frees = r.switch_frees;
	return w;
}

} // namespace streamholdcmd

#endif
