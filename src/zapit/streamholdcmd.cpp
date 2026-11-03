/*
 * streamholdcmd.cpp - zapit's command that holds a frontend for a stream
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

#include "streamholdcmd.h"

namespace streamholdcmd
{

namespace
{

Reply refusal(int32_t verdict)
{
	Reply r;
	r.verdict = verdict;
	r.hold_id = 0;
	r.fenumber = -1;
	return r;
}

Reply send(Ops &ops, const Reply &r)
{
	ops.answer(r);
	return r;
}

bool late(Ops &ops, int64_t deadline_ms)
{
	return deadline_ms != 0 && ops.nowMs() > deadline_ms;
}

} // namespace

Reply run(Ops &ops, uint64_t channel, int64_t deadline_ms, bool probe, bool lock_frontend)
{
	// Holds dropped since the last command are given back before this one
	// looks at what is free.
	ops.collect();

	if (!ops.serving())
		return send(ops, refusal(Failed));
	if (!probe && late(ops, deadline_ms))
		return send(ops, refusal(Failed));

	if (ops.channel(channel) != Broadcast)
		return send(ops, refusal(NotFound));
	/* A probe answers what a hold would get, and a hold needs a place. */
	if (!ops.roomForHold())
		return send(ops, refusal(Busy));

	const bool bind = !probe;
	const bool lock = lock_frontend && bind;

	void *fe = 0;
	int fenumber = -1;
	streamadmission::Way way;

	ops.lockFrontends();
	{
		streamadmission::Facts f;
		ops.facts(f);
		way = streamadmission::decide(f, channel);

		if (way == streamadmission::Live || way == streamadmission::Shared)
		{
			fenumber = f.live_fe;
			if (bind)
				fe = ops.holdLive(lock);
		}
		else if (way == streamadmission::Idle)
		{
			std::vector<int> held;
			streamadmission::held(f, held);
			ops.heldByStreams(held);
			/* zapit closes the frontends in standby; one is opened for a
			   hold, never for a probe. */
			if (bind && f.standby)
				ops.openFrontends();
			fe = ops.pickIdle(held, bind, lock);
			if (fe)
				fenumber = ops.number(fe);
		}
	}
	ops.unlockFrontends();

	if (way == streamadmission::Refuse || fenumber < 0)
		return send(ops, refusal(Busy));

	if (probe)
	{
		Reply r = refusal(Granted);
		r.fenumber = fenumber;
		return send(ops, r);
	}
	if (!fe)
		return send(ops, refusal(Busy));

	/* An idle frontend is tuned; on a shared one only PAT and PMT are read.
	   zapit may have begun to stop meanwhile: no hold for a stream nobody
	   will serve. */
	bool ready = true;
	if (way == streamadmission::Idle)
		ready = ops.tune(fe);
	else if (way == streamadmission::Shared)
		ready = ops.readPids(fe);
	if (way != streamadmission::Live && (!ready || late(ops, deadline_ms) || !ops.serving()))
	{
		if (lock)
			ops.unlockFrontend(fe);
		return send(ops, refusal(Failed));
	}

	const uint32_t id = ops.offer(fe);
	if (!id)
	{
		if (lock)
			ops.unlockFrontend(fe);
		return send(ops, refusal(Busy));
	}

	Reply r;
	r.verdict = Granted;
	r.hold_id = id;
	r.fenumber = fenumber;
	/* An answer that did not go out leaves a hold nobody knows the number
	   of: it is ended here rather than left to its expiry. */
	if (!ops.answer(r))
	{
		ops.drop(id);
		ops.collect();
	}
	return r;
}

} // namespace streamholdcmd
