/*
 * playback.cpp - what the movie player is playing, as the box shows it
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

#include "coreapi/playback.h"

#include "coreapi/archive.h"
#include "coreapi/channels.h"
#include "coreapi/base/deps.h"
#include "coreapi/base/errors.h"
#include "coreapi/base/eventbus.h"
#include "coreapi/base/messagebridge.h"

#include <neutrinoMessages.h>

#include <OpenThreads/Mutex>
#include <OpenThreads/ScopedLock>

#include <cstdlib>

#include <limits.h>
#include <stdlib.h>

namespace coreapi
{
namespace playback
{

namespace
{

// Further off than this from where the clock says the loop should be is a jump.
const int kJumpMs = 5000;

struct Held
{
	Snapshot snap;
	bool     observed;
	int      position_ms;
	int64_t  at_ms;
	long     listed_duration;  // seconds, from the archive

	Held() : observed(false), position_ms(0), at_ms(0), listed_duration(0) {}
};

OpenThreads::Mutex &lock()
{
	static OpenThreads::Mutex m;
	return m;
}

Held &held()
{
	static Held h;
	return h;
}

// Cut at a character boundary, as the archive cuts its texts.
std::string bounded(const std::string &s)
{
	size_t n = archive::kMaxShortText;
	if (s.size() <= n)
		return s;
	while (n > 0 && ((unsigned char) s[n] & 0xc0) == 0x80)
		--n;
	return s.substr(0, n);
}

// The last part of a path or an address, without what a query would carry.
std::string nameOf(const std::string &path)
{
	std::string s = path.substr(0, path.find_first_of("?#"));
	const size_t slash = s.find_last_of('/');
	return slash == std::string::npos ? s : s.substr(slash + 1);
}

Source sourceOf(const Snapshot &s)
{
	if (!s.active)
		return Source::None;
	if (s.timeshift)
		return Source::Channel;
	return s.archive_id.empty() ? Source::File : Source::Recording;
}

Event eventOf(const Snapshot &s, State state)
{
	Event e;
	e.type = EventType::Playback;
	e.channel_id = s.channel_id;
	e.value = (int) s.position;
	e.text = std::string(stateName(state)) + " " + sourceName(sourceOf(s));
	if (!s.archive_id.empty())
		e.text += " " + s.archive_id;
	return e;
}

bool jumped(const Held &h, const Sample &s, int64_t now_ms)
{
	if (s.state == State::Playing)
	{
		const int64_t expected = h.position_ms + (now_ms - h.at_ms);
		return std::llabs((long long) (s.position_ms - expected)) > kJumpMs;
	}
	if (s.state == State::Paused)
		return std::abs(s.position_ms - h.position_ms) > kJumpMs;
	return false;
}

} // namespace

void begin(const Started &s)
{
	Snapshot snap;
	snap.active = true;
	snap.timeshift = s.timeshift;
	snap.name = nameOf(s.path);
	snap.title = s.title;
	snap.channel = s.channel;
	snap.channel_id = s.channel_id;
	snap.state = State::Playing;
	snap.speed = 1;
	// A timeshift file may lie in the record directory and is still the channel.
	Result<archive::Entry> e = archive::entryAt(s.timeshift ? std::string() : s.path);
	if (e.ok())
	{
		snap.archive_id = e.value().id;
		if (snap.title.empty())
			snap.title = e.value().title;
		if (snap.channel.empty())
			snap.channel = e.value().channel;
		snap.duration = e.value().duration;
	}
	snap.title = bounded(snap.title);
	snap.channel = bounded(snap.channel);

	OpenThreads::ScopedLock<OpenThreads::Mutex> hold(lock());
	held() = Held();
	held().snap = snap;
	held().listed_duration = snap.duration;
}

void observe(const Sample &s)
{
	observeAt(s, detail::monotonicMs());
}

void observeAt(const Sample &s, int64_t now_ms)
{
	Event e;
	bool send = false;
	{
		OpenThreads::ScopedLock<OpenThreads::Mutex> hold(lock());
		Held &h = held();
		if (!h.snap.active)
			return;
		const bool winding = s.state == State::Forward || s.state == State::Rewind;
		send = !h.observed || s.state != h.snap.state || (winding && s.speed != h.snap.speed) ||
		       jumped(h, s, now_ms);
		h.observed = true;
		h.position_ms = s.position_ms;
		h.at_ms = now_ms;
		h.snap.position = s.position_ms / 1000;
		// A player without a length of its own, as the PC build's, reports 0.
		h.snap.duration = s.duration_ms > 0 ? s.duration_ms / 1000 : h.listed_duration;
		h.snap.state = s.state;
		h.snap.speed = s.speed;
		if (send)
			e = eventOf(h.snap, s.state);
	}
	if (send)
		EventBus::instance().publish(e);
}

void end()
{
	Event e;
	{
		OpenThreads::ScopedLock<OpenThreads::Mutex> hold(lock());
		if (!held().snap.active)
			return;
		e = eventOf(held().snap, State::Stopped);
		held() = Held();
	}
	EventBus::instance().publish(e);
}

Snapshot snapshot()
{
	OpenThreads::ScopedLock<OpenThreads::Mutex> hold(lock());
	return held().snap;
}

const char *stateName(State s)
{
	switch (s)
	{
		case State::Playing: return "playing";
		case State::Paused:  return "paused";
		case State::Forward: return "forward";
		case State::Rewind:  return "rewind";
		case State::Stopped: break;
	}
	return "stopped";
}

const char *sourceName(Source s)
{
	switch (s)
	{
		case Source::Channel:   return "channel";
		case Source::Recording: return "recording";
		case Source::File:      return "file";
		case Source::None:      break;
	}
	return "none";
}

Result<Now> now()
{
	Now out;
	int mode = 0;
	if (channelSource().currentMode(mode) == Status::Ok && mode == NeutrinoModes::mode_standby)
		return ok(out);
	out.playing = snapshot();
	out.source = sourceOf(out.playing);
	if (out.source == Source::Recording || out.source == Source::File)
		return ok(out);
	Result<ChannelInfo> c = channels::current();
	if (!c.ok())
	{
		if (c.error().code != ErrorCode::NoRunningChannel)
			return fail(c.error());
		out.source = Source::None;
		return ok(out);
	}
	out.source = Source::Channel;
	out.channel = c.value();
	return ok(out);
}

} // namespace playback
} // namespace coreapi
