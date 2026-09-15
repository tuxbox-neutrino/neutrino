/*
 * epg.h - programme data, searches, and one channel schedule
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

#ifndef __coreapi_epg_h__
#define __coreapi_epg_h__

#include "coreapi/base/result.h"
#include "coreapi/base/types.h"

#include <stddef.h>
#include <stdint.h>
#include <string>
#include <time.h>
#include <vector>

namespace coreapi
{
namespace epg
{

/* Every event of the channel that overlaps [from, to), in start time order.

   The channel and the events come back named the way every channel route names
   one, which is not what the guide is asked: a schedule is filed under an
   epg_id, which is another channel's wherever a box maps one schedule onto two
   services, and the guide keeps only its lower half. Both steps happen below
   this.

   A channel with nothing scheduled answers an empty list and not an error, the
   event manager not being able to tell a channel it has never seen from one that
   is off air. InvalidArgument for a window that ends where it begins or earlier. */
Result<EventList> forChannel(ChannelId id, time_t from, time_t to);

// The event the channel is showing. NotFound when nothing covers the moment,
// because there is no empty event worth handing back.
Result<EventInfo> current(ChannelId channel_id);

// The three things only the guide's own read of a single event carries: the age
// it is broadcast for, what it is about, and the long text under the short one.
// NotFound for a pair the guide holds no showing under.
Result<EventDetail> event(uint64_t event_id, time_t start);

/* One channel of a grid. truncated is there for the reason a search has one: a
   row exactly as long as the ceiling is the one case where a whole answer and a
   cut one look the same. What a caller does about it is ask again with the
   window opening after the last event it was given. */
struct ChannelSchedule
{
	ChannelId id;
	EventList events;
	bool      truncated;

	ChannelSchedule() : id(0), truncated(false) {}
};

typedef std::vector<ChannelSchedule> ScheduleList;

/* One page of a grid. more says there are channels this page did not reach and
   next_after names the last it did. The pair rather than a count, because a
   position in a list moves when the box reloads under a caller walking it and an
   identifier does not.

   more is the only thing that says whether a walk is over: this page ends at the
   caller's limit or at the ceiling on events below, whichever comes first, so a
   caller that stops at a short page stops in the middle of the guide. */
struct Grid
{
	ScheduleList channels;
	bool         more;
	ChannelId    next_after;

	Grid() : more(false), next_after(0) {}
};

// Above a day of half hour programmes, which is more than any grid draws at
// once, and low enough that a channel carrying a fortnight of schedule cannot
// fill a page on its own.
const size_t MAX_EVENTS_PER_CHANNEL = 64;

/* What actually bounds the size of the answer: the row ceiling alone leaves a
   page worth the channel ceiling times that, which runs to megabytes on a box
   with tens of them. Reaching this ends the page early rather than dropping
   anything.

   Set from the request this route exists for, a hundred channels over six hours
   being about a dozen events each. A page always carries at least one row, so a
   walk cannot stop advancing. */
const size_t MAX_EVENTS_PER_PAGE = 1200;

// The most channels one page answers for, and what a caller that named no number
// gets. The default is a screenful rather than the ceiling, because a grid is
// drawn a screen at a time.
const size_t MAX_GRID_CHANNELS = 100;
const size_t DEFAULT_GRID_CHANNELS = 20;

/* Every named channel's events inside [from, to), one row per channel, a page at
   a time, in the order the caller named them.

   An identifier named twice is kept once at the place it was first named: a
   cursor names an identifier, so one standing in two places is two places the
   next page could begin from, and the walk then stops advancing while the rest
   is never reached.

   have_cursor and cursor together are where the page begins, the page being the
   channels after the one the cursor names. NoSuchChannel for a cursor naming a
   channel this list does not hold, rather than starting again from the front,
   which a caller walking until the cursor comes back empty would never see.

   InvalidArgument for a window that ends where it begins or earlier, for a limit
   of none, and for a limit above the ceiling: refused rather than narrowed,
   because an answer shorter than the one asked for cannot be told from a guide
   that held no more. A guide that cannot be read for one channel fails the whole
   page, a page short one row for that reason saying the channel had nothing on
   it.

   This costs one read of the guide per channel of the page, the event manager
   offering no read that answers several channels over a window. */
Result<Grid> grid(const ChannelIdList &ids, time_t from, time_t to,
		  bool have_cursor, ChannelId cursor, size_t limit);

// A caller that asks for more is refused rather than quietly given this many,
// because a list shorter than the one asked for cannot be told from a schedule
// that held no more.
const size_t MAX_SEARCH_RESULTS = 200;

// Every event matching q that overlaps [from, to), in the order the guide holds
// them, at most limit of them.
//
// The window and the limit are the caller's and both are required. This is the
// one read here that nothing else narrows: the guide holds every channel's whole
// schedule, a match is looked for in the name and both texts of every event in
// it, and the shortest useful queries select the most. A query below two
// characters is refused for that reason and not because it is meaningless.
//
// What is not this layer's to fix: the guide is walked whole, under its own read
// lock, because the layer below offers no narrower read. The window and the
// limit bound what a caller receives, not that walk.
//
// A hit list as long as limit with more behind it comes back with truncated set,
// which works whatever the source does with the ceiling because one more than
// the caller asked for is what is asked of it. There is no cursor: the guide is
// rebuilt under a reader as the box receives it.
//
// The events come back unfiltered by any channel list. What is not as the event
// manager holds them is the channel each names: the guide keeps the lower forty
// eight bits and every read here answers with the whole identifier. A hit for a
// channel the box no longer carries keeps the half the guide holds.
Result<SearchResult> search(const std::string &q, time_t from, time_t to, size_t limit);

// All is what an absent request parameter means, and it is not the same as
// asking twice: a caller that asks for Tv gets television alone.
enum class Kind
{
	Tv,
	Radio,
	All
};

// The document every user bouquet's schedule makes, written out as it is built
// rather than held together in one string first: a satellite list runs to
// hundreds of channels and tens of thousands of events.
//
// The path is this call's own and unique to it, so two requests in flight each
// get their own file. It names a file already written and closed, and removing
// it once it has been read is the caller's part of the bargain.
Result<std::string> xmltv(Kind kind);

} // namespace epg
} // namespace coreapi

#endif
