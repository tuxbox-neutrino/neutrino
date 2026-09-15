/*
 * messagebridge.h - passing GUI messages in and out of the core API
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

#ifndef __coreapi_messagebridge_h__
#define __coreapi_messagebridge_h__

#include <stdint.h>

#include <driver/neutrino_msg_t.h>

namespace coreapi
{

/* Runs ahead of the handler that frees the payload, so whatever data points at
   has to be read out here. Only the typedefs are named, because the header that
   names the messages drags the whole remote control in. */
void publishFromMessage(const neutrino_msg_t msg, const neutrino_msg_data_t data);

/* One recording began, and one ended. Not taken from the message loop the way
   everything above is, because the message the loop carries says whether the box
   is recording at all and not which recording: it is sent when the first one
   begins and when the last one ends, so a reader watching the stream would see a
   second recording start and a first of two end without a word.

   So these are called where the box's own list of recordings gains and loses
   one. Each carries the number that names the recording in the listing and the
   channel it is of, so a reader holding a list can act on the one that moved. */
void publishRecordingStarted(uint64_t channel_id, uint32_t recording_id);
void publishRecordingStopped(uint64_t channel_id, uint32_t recording_id);

namespace detail
{
/* Reachable so a test can stand on the boundaries instead of waiting them out.
   A negative reading is one the clock could not give, and opens the window. */
bool epgWindowOpenAt(int64_t now_ms);

/* Negative if the clock could not be read. Reachable so the unit is pinned by
   something other than a sleep that has to be long enough. */
int64_t monotonicMs();
}

} // namespace coreapi

#endif
