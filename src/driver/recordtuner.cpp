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

} // namespace recordtuner
