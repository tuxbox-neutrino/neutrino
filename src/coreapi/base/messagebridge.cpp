/*
 * messagebridge.cpp - passing GUI messages in and out of the core API
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

#include "messagebridge.h"

#include "eventbus.h"

#include <time.h>

#include <OpenThreads/Mutex>
#include <OpenThreads/ScopedLock>

#include <neutrinoMessages.h>
#include <zapit/types.h>

namespace coreapi
{

namespace
{
const int64_t epg_window_ms = 500;
}

namespace detail
{

/* One current/next event arrives per table update, and a transponder with many
   services delivers them in bursts a subscriber has no use for. */
bool epgWindowOpenAt(int64_t now_ms)
{
	/* No reading of a monotonic clock is negative, so this is one that could
	   not be taken. It costs the rate limit, not every update behind it. */
	if (now_ms < 0)
		return true;

	/* Locked because the ordering that keeps the callers apart is not this
	   function's to rely on: the movie player runs a hint thread that feeds the
	   same message path and is ordered against the main loop only by the join
	   that ends it. On the 32 bit targets a torn read of these 64 bits leaves
	   the window stuck open or stuck shut. */
	static OpenThreads::Mutex mutex;
	// a window behind zero, so the first update is never inside one
	static int64_t last_ms = -epg_window_ms;

	OpenThreads::ScopedLock<OpenThreads::Mutex> lock(mutex);
	if (now_ms - last_ms < epg_window_ms)
		return false;
	last_ms = now_ms;
	return true;
}

/* Monotonic because the system clock is set from the transponder time, and a
   step would otherwise hold the window open or shut for the size of the step.
   Signed and 64 bit because the seconds alone overflow a 32 bit millisecond
   count after 24 days of uptime. */
int64_t monotonicMs()
{
	struct timespec t;
	if (clock_gettime(CLOCK_MONOTONIC, &t))
		return -1;

	return (int64_t) t.tv_sec * 1000 + t.tv_nsec / 1000000;
}

} // namespace detail

void publishFromMessage(const neutrino_msg_t msg, const neutrino_msg_data_t data)
{
	EventType type;
	uint64_t channel_id = 0;
	int value = 0;

	switch (msg)
	{
		/* Both carry a heap block the caller frees a few branches down, so the
		   id is copied out rather than the pointer kept. */
		case NeutrinoMessages::EVT_ZAP_COMPLETE:
			type = EventType::Zap;
			if (data)
				channel_id = *(const t_channel_id *) data;
			break;

		// this id is the shortened one, see Event::channel_id
		case NeutrinoMessages::EVT_CURRENTNEXT_EPG:
			if (!detail::epgWindowOpenAt(detail::monotonicMs()))
				return;
			type = EventType::EpgUpdated;
			if (data)
				channel_id = *(const t_channel_id *) data;
			break;

		// the rezap flag rides on top of the mode
		case NeutrinoMessages::CHANGEMODE:
			type = EventType::Mode;
			value = (int)(data & NeutrinoModes::mode_mask);
			break;

		/* STANDBY_ON and STANDBY_OFF say nothing here: they are the wish, and
		   the box may refuse it or drop it half way. It says what happened
		   instead, once it has (system::announceStandby).

		   EVT_SET_VOLUME and EVT_SET_MUTE are wishes as well, and the handset
		   never sends them (osd::announceVolume, osd::announceMute). */

		/* EVT_RECORDMODE is deliberately not here. It says whether the box is
		   recording at all and not which recording, and the zapit side sends it
		   only where that answer changes, so a second recording starting and a
		   first of two ending carry no message. What the stream says about
		   recordings comes from the two calls below instead. */

		// Carries no id: a reader rereads the whole list.
		case NeutrinoMessages::EVT_TIMERLIST_CHANGED:
			type = EventType::TimerChanged;
			break;

		default:
			return;
	}

	/* Built past the switch: an event carries a string, and all but a handful of
	   the messages the loop dispatches have left through the branch above. */
	Event e;
	e.type = type;
	e.channel_id = channel_id;
	e.value = value;

	EventBus::instance().publish(e);
}

namespace
{
void publishRecording(EventType type, uint64_t channel_id, uint32_t recording_id)
{
	Event e;
	e.type = type;
	e.channel_id = channel_id;
	// The number the listing names this recording by. Widened by the caller, and
	// the daemon hands these out counting up from one, so nothing here can reach
	// the sign bit.
	e.value = (int) recording_id;
	EventBus::instance().publish(e);
}
} // anonymous namespace

void publishRecordingStarted(uint64_t channel_id, uint32_t recording_id)
{
	publishRecording(EventType::RecordStart, channel_id, recording_id);
}

void publishRecordingStopped(uint64_t channel_id, uint32_t recording_id)
{
	publishRecording(EventType::RecordStop, channel_id, recording_id);
}

} // namespace coreapi
