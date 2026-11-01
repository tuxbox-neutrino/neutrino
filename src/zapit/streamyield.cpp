/*
 * streamyield.cpp - zapit's command that ends stream holds for a recording
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

#include "streamyield.h"

#include <cstddef>

namespace streamyield
{

namespace
{

Reply send(Ops &ops, int32_t stage, uint32_t fe_mask)
{
	Reply r;
	r.stage = stage;
	r.fe_mask = fe_mask;
	ops.answer(r);
	return r;
}

} // namespace

Reply run(Ops &ops, uint64_t channel, int64_t deadline_ms, bool holds_lock)
{
	// Holds dropped since the last command are given back before anything
	// is looked at, and before the answer.
	ops.collect();

	if (!ops.serving())
		return send(ops, kRefused, 0);
	if (channel == 0 || !holds_lock)
		return send(ops, 0, 0);
	if (deadline_ms != 0 && deadline_ms - ops.nowMs() < kMarginMs)
		return send(ops, kRefused, 0);

	std::vector<int> end;
	uint32_t mask = 0;
	ops.lockFrontends();
	const int stage = ops.search(channel, end);
	for (size_t i = 0; i < end.size(); ++i)
	{
		if (end[i] < 0 || end[i] >= 32)
			continue;
		ops.revoke(end[i]);
		mask |= 1u << end[i];
	}
	ops.unlockFrontends();

	return send(ops, stage, mask);
}

} // namespace streamyield
