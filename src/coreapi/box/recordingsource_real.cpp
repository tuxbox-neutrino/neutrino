/*
 * recordingsource_real.cpp - recording control against the running timer daemon
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

#include <connection/basicclient.h>
#include <timerdclient/timerdclient.h>

extern CTimerdClient *g_Timerd;

namespace coreapi
{

namespace
{

/* Its own translation unit for the reason the plugin source beside it has one:
   half of it is defined by the application and by nothing a build that only
   wants this layer links.

   The two halves go to two different places on purpose. What the box is
   recording is the program's own record of what it is doing. Ending one is the
   timer daemon's, because the daemon holds the timer that carries a recording's
   end; asking the loop directly would end the writing and leave that timer
   running against a recording that had gone. */
class RealRecordingSource : public RecordingSource
{
	public:
		Status list(RecordingList &out) const
		{
			return boxRecordings(out);
		}

		Status stop(uint32_t id)
		{
			if (!g_Timerd)
				return Status::Internal;

			/* The daemon acknowledges this with nothing at all, so an Ok says
			   the command went out and no more. What it sets in motion is a
			   message to the loop, and nothing here can wait for that. */
			CBasicClient::Deadline bound(TIMER_CALL_TIMEOUT_MS);
			g_Timerd->stopTimerEvent((int) id);
			return Status::Ok;
		}
};

RealRecordingSource g_real_recording_source;

} // anonymous namespace

void installRealRecordingSource() { setRecordingSource(&g_real_recording_source); }

} // namespace coreapi
