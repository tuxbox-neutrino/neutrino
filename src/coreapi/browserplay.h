/*
 * browserplay.h - which channels a browser can be handed, and how
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

#ifndef __coreapi_browserplay_h__
#define __coreapi_browserplay_h__

#include <string>
#include <vector>

namespace coreapi
{
namespace browserplay
{

/* THE PICTURE IS NEVER RE-ENCODED. The weakest box this runs on decodes video in
   hardware and has no cycles left to encode any. Everything below follows: the
   picture is copied or the channel is not offered at all, and the only thing
   ever converted is the sound. */
enum class Path
{
	// Both streams go out as they came in. The box pays a demux and a mux.
	Copy,
	// The picture is copied and the sound is encoded to AAC.
	ConvertSound,
	/* The box does not know yet what this channel carries, so it does the one
	   thing right for every channel it could be: copies the picture and encodes
	   the sound. Whether the result plays is then the browser's answer.

	   THIS IS THE ORDINARY CASE FOR A CHANNEL NOBODY HAS BEEN ON. A box learns
	   what a channel carries when it tunes it, and the scan does not record it,
	   so every cold channel arrives here. */
	Blind,
	/* Nothing this box can do makes this channel playable in a browser, so the
	   address of the raw stream is offered instead. Saying so is the point: a
	   player handed a picture no browser decodes shows a black rectangle and no
	   reason for it. */
	NotOffered
};

/* Which of the three a channel falls into, from the codec names the channel
   route writes. Nothing but that route's own spelling is accepted, so a value
   this does not know lands on NotOffered rather than on a guess.

   video is "none" for radio: no picture means no synchronising and no container
   work beyond the sound itself. It is "unknown" for a channel the box has never
   tuned, and so is the sound, which is Blind above and not a refusal.

   WHAT THIS DOES NOT ANSWER is whether the browser asking can play the result.
   That is a question about the browser, and a table of browsers written into
   this program is wrong by the time it ships. The page asks its own media stack.
   So Copy on an HEVC channel is this box saying it can hand the picture over
   untouched, not a promise that it will be decoded. */
Path pathFor(const std::string &video_codec, const std::string &audio_codec);

// The name this path is called by wherever it crosses the wire. Never empty.
const char *pathName(Path p);

/* The command that carries one channel to a browser, argument by argument and
   never as a line for a shell: the source carries an identifier from a request
   and a line assembled around it is a line a caller can put its own commands
   into.

   source is the address the raw transport stream is read from, which is this
   box's own streaming server. audio_pid is the stream the sound is taken from
   and zero means the first one the source carries.

   Empty for NotOffered. */
std::vector<std::string> commandFor(Path p, bool radio, const std::string &source,
                                    unsigned audio_pid);

/* What the answer to that command is, as a media type. mpegts for a picture and
   adts for sound alone, which lets radio be played by an element the browser
   already has instead of by a demuxer the page has to fetch.

   NULL for NotOffered. */
const char *typeFor(Path p, bool radio);

} // namespace browserplay
} // namespace coreapi

#endif
