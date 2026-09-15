/*
 * commandsink_real.cpp - command sink that reaches the running GUI
 *
 * Copyright (C) 2026 NI-Team
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
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include "coreapi/base/deps.h"

#include <driver/rcinput.h>

extern CRCInput *g_RCInput;

namespace coreapi
{

namespace
{

// Its own translation unit, because the remote control is all it needs and the
// channel stack beside it would keep it out of a test binary.
class RealCommandSink : public CommandSink
{
	public:
		Status post(neutrino_msg_t msg, neutrino_msg_data_t data)
		{
			// A remote control that was never built has no queue to refuse
			// anything, which is not the answer a full one gives.
			if (!g_RCInput)
				return Status::Internal;
			if (!g_RCInput->postMsg(msg, data))
				return Status::Busy;
			return Status::Ok;
		}
};

RealCommandSink g_real_sink;

} // anonymous namespace

void installRealCommandSink() { setCommandSink(&g_real_sink); }

} // namespace coreapi
