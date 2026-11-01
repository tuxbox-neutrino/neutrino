/*
 * recordtuner.cpp - the frontend a recording takes
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

#include "recordtuner.h"

namespace recordtuner
{

bool liveServes(bool present, bool tuned, uint64_t tsid_onid, uint64_t channel_tp)
{
	return present && tuned && tsid_onid == channel_tp;
}

bool fullZap(bool live_is_frontend, bool live_web, bool live_serves)
{
	return live_is_frontend && !live_web && !live_serves;
}

namespace
{

/* Has zapit end the streams in the way and stops them; stage is 0 when no
   answer came. Whether any were stopped. */
bool endStreams(Ops &ops, int &stage)
{
	uint32_t mask = 0;
	if (!ops.yield(stage, mask))
	{
		stage = 0;
		return false;
	}
	if (mask == 0)
		return false;
	ops.stopStreamsOn(mask);
	return true;
}

} // namespace

Choice choose(Ops &ops, bool standby, bool wake)
{
	Choice c;
	c.way = NoFrontend;
	c.live = 0;
	c.live_locked = false;
	c.streams_ended = false;
	int stage = 0;

	if (standby)
	{
		c.streams_ended = endStreams(ops, stage);
		if (wake)
			ops.wake();
	}

	ops.lockFrontends();
	c.live = ops.liveFrontend();
	if (ops.liveRecords(c.live))
	{
		c.way = LiveFrontend;
		ops.unlockFrontends();
		return c;
	}

	/* First with the live frontend locked, so the picture stays. */
	ops.lockFrontend(c.live);
	bool got = ops.allocate();
	if (got)
		c.live_locked = true;
	else
	{
		ops.unlockFrontend(c.live);
		if (!standby)
		{
			ops.unlockFrontends();
			c.streams_ended = endStreams(ops, stage);
			ops.lockFrontends();
			if (stage == 1 || stage == 2)
			{
				c.live = ops.liveFrontend();
				ops.lockFrontend(c.live);
				got = ops.allocate();
				if (got)
					c.live_locked = true;
				else
					ops.unlockFrontend(c.live);
			}
		}
		/* Then with it free: the recording may take the live frontend,
		   and the picture follows the recording. */
		if (!got)
		{
			c.live = ops.liveFrontend();
			got = ops.allocate();
		}
	}
	ops.unlockFrontends();

	if (got)
		c.way = Allocated;
	return c;
}

} // namespace recordtuner
