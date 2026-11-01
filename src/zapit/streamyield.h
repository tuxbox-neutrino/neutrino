/*
 * streamyield.h - zapit's command that ends stream holds for a recording
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
 * A recording that finds no frontend asks zapit which streams stand in its
 * way (fesearch::streamsToEnd) and has zapit take their holds back. It runs
 * on zapit's own thread, after whatever command came before it: a hold that
 * is still being tuned there is done by then and found in the register like
 * any other. The search and the taking back happen in one hold of the
 * frontend manager's lock, and the answer goes out after it; the recording
 * then stops the streams themselves and asks for a frontend again.
 *
 * Nothing is taken back that the answer could not report: a request with
 * less than kMarginMs left before its deadline is refused before anything
 * is done.
 */

#ifndef __zapit_streamyield_h__
#define __zapit_streamyield_h__

#include <stdint.h>
#include <vector>

namespace streamyield
{

// The stage answered when zapit did nothing: it stops, or time is short.
const int32_t kRefused = -1;
// What a request must have left for its answer to reach the recording.
const int64_t kMarginMs = 1000;

/* The values travel in zapit's reply. */
struct Reply
{
	int32_t  stage;     // as fesearch::StreamsToEnd, or kRefused
	uint32_t fe_mask;   // bit n: the holds on frontend number n were taken back
};

/* What a yield acts on. zapit implements it over itself, the frontend
   manager and the register; a case implements it over a model. */
class Ops
{
public:
	virtual ~Ops() {}

	virtual void collect() = 0;          // the register's collect
	virtual bool serving() = 0;          // zapit has not begun to stop
	virtual int64_t nowMs() = 0;         // the clock a deadline is on

	virtual void lockFrontends() = 0;
	virtual void unlockFrontends() = 0;

	/* Under the frontend manager's lock: */
	/* The stage for a recording of channel and the numbers of the
	   frontends whose streams have to end. */
	virtual int search(uint64_t channel, std::vector<int> &end) = 0;
	/* Takes back every stream hold on frontend number fenumber and gives
	   the frontend back. */
	virtual void revoke(int fenumber) = 0;

	virtual bool answer(const Reply &r) = 0;
};

/* One yield for a recording of channel, answered through ops. Channel 0
   only collects the holds dropped since the last command. Without
   holds_lock a stream hold does not lock its frontend, a recording takes
   such a frontend as it is, and nothing is taken back either. deadline_ms
   is on ops.nowMs(); 0 means none. Answers what it sent. */
Reply run(Ops &ops, uint64_t channel, int64_t deadline_ms, bool holds_lock);

} // namespace streamyield

#endif
