/*
 * streaming.h - live and recorded streams, and the playlists naming them
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

#ifndef __coreapi_streaming_h__
#define __coreapi_streaming_h__

#include "coreapi/base/result.h"
#include "coreapi/base/types.h"

#include <string>

namespace coreapi
{
namespace streaming
{

/* Where a player fetches one channel. authority is the caller's: nothing here
   can know under which name this box is reachable from wherever the answer is
   going, a box answering under as many names as there are ways to reach it.

   A port the authority already carries is dropped and the port the box streams
   on is put there instead. Host and port are told apart the way an authority is
   written: a bracketed group at the front is taken whole, and otherwise the last
   colon separates only when nothing but digits follows it, so a name carrying
   colons of its own keeps them.

   InvalidArgument for an empty authority, because "http://:31339/id=..." is not
   an address and a caller handed one has no way of noticing. */
Result<std::string> urlFor(const std::string &authority, ChannelId id);

/* Which channels the playlist below covers. CurrentMode is what the box is
   showing, which is what a caller that named nothing means. */
enum class Scope
{
	CurrentMode,
	Tv,
	Radio
};

/* The playlist every visible user bouquet makes, written out as it is built
   rather than held together in one string first: a satellite list runs to
   hundreds of channels, and this is what keeps a request for the whole of it
   from costing the size of the answer in memory.

   The path is this call's own and unique to it, so two requests in flight each
   get their own file. It names a file already written and closed, and removing
   it once it has been read is the caller's part of the bargain. */
Result<std::string> playlist(const std::string &authority, Scope s);

// The same for one channel, in the same file and with the same bargain about
// it. NotFound for a channel the box does not have.
Result<std::string> playlistFor(const std::string &authority, ChannelId id);

} // namespace streaming
} // namespace coreapi

#endif
