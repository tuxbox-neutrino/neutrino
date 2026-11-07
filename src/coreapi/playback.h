/*
 * playback.h - what the movie player is playing, as the box shows it
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

#ifndef __coreapi_playback_h__
#define __coreapi_playback_h__

#include "coreapi/base/result.h"
#include "coreapi/base/types.h"

#include <stdint.h>

#include <string>

namespace coreapi
{
namespace playback
{

enum class State
{
	Stopped,
	Playing,
	Paused,
	Forward,
	Rewind
};

enum class Source
{
	None,
	Channel,
	Recording,
	File
};

// What the movie player knows as a file starts.
struct Started
{
	std::string path;
	std::string title;
	std::string channel;
	ChannelId   channel_id;
	bool        timeshift;

	Started() : channel_id(0), timeshift(false) {}
};

// One turn of the play loop.
struct Sample
{
	int   position_ms;
	int   duration_ms;
	State state;
	int   speed;

	Sample() : position_ms(0), duration_ms(0), state(State::Playing), speed(1) {}
};

// The movie player's own copy; a reader takes it whole under a short lock.
struct Snapshot
{
	bool        active;
	bool        timeshift;
	std::string archive_id;  // empty unless the file is a recording of the archive
	std::string name;        // the file name without its directories
	std::string title;
	std::string channel;
	ChannelId   channel_id;
	// Counts the starts, never sent: what a stop names to the loop, which asks no disk about it.
	uint64_t    generation;
	long        position;    // seconds
	long        duration;    // seconds
	State       state;
	int         speed;

	Snapshot() : active(false), timeshift(false), channel_id(0), generation(0), position(0), duration(0),
		state(State::Stopped), speed(0) {}
};

// Called by the play loop's thread. begin publishes nothing; the first sample
// does, then every change of state, every jump, and end.
void begin(const Started &s);
void observe(const Sample &s);
void observeAt(const Sample &s, int64_t now_ms);
void end();

Snapshot snapshot();

const char *stateName(State s);
const char *sourceName(Source s);

// What the television shows. Channel carries the live channel, Recording and
// File the snapshot; None in standby or with nothing to show.
struct Now
{
	Source      source;
	ChannelInfo channel;
	Snapshot    playing;

	Now() : source(Source::None) {}
};

Result<Now> now();

enum class Stop
{
	Asked,          // the loop was asked to end the file
	NothingToStop   // no file plays, a timeshift plays, or not the recording named
};

/* Asks the loop to end the file the movie player plays, as its stop key does:
   any file where recording_id is empty, else only the recording of that id.
   Live television and a timeshift are never ended. The message names the
   playback by its generation. */
Result<Stop> stop(const std::string &recording_id);

/* For the loop: whether the playback of that generation still runs, and is a
   file and no timeshift. Reads the snapshot only. */
bool playsGeneration(uint64_t generation);

/* What the picture on the television is. Read from the decoder only in the
   modes that show a moving picture: live television, a web channel and the
   movie player, as the box's own stream information reads it; anything else,
   standby and radio among them, and a box that cannot say, answer shown
   false. Never refused. */
Result<PictureInfo> picture();

} // namespace playback
} // namespace coreapi

#endif
