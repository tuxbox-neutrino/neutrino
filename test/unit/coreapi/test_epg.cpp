/*
 * test_epg.cpp - tests for programme data
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

#include "support/catch.hpp"
#include "support/fakes.h"
#include "coreapi/epg.h"

#include <limits>
#include <string>

using namespace coreapi;

namespace
{
const ChannelId ERSTE = 0xb9b0040200016dcbULL;
const ChannelId ZDF   = 0xbdf8040300017a01ULL;

// The whole schedule, which is the window the converted endpoints ask for
// because none of them has ever taken one. Both ends of the range, so that the
// window holds every event a time_t can express.
const time_t WHOLE_FROM = std::numeric_limits<time_t>::min();
const time_t WHOLE_TO   = std::numeric_limits<time_t>::max();

EventInfo mk(ChannelId channel, uint64_t id, const char *title, time_t start, unsigned duration)
{
	EventInfo e;
	e.event_id   = id;
	e.channel_id = channel;
	e.title      = title;
	e.start      = start;
	e.duration   = duration;
	return e;
}
}

TEST_CASE("current event is the one covering now", "[epg]")
{
	FakeEpgSource fake;
	EventInfo a;
	a.event_id = 1; a.channel_id = 0x2b66; a.title = "Tagesschau";
	a.start = 1000; a.duration = 900;
	EventInfo b;
	b.event_id = 2; b.channel_id = 0x2b66; b.title = "Tatort";
	b.start = 1900; b.duration = 5400;
	fake.events.push_back(a);
	fake.events.push_back(b);
	InstalledGuideSources installed(&fake);

	fake.now = 1500;
	Result<EventInfo> r = epg::current(0x2b66);
	REQUIRE(r.ok());
	REQUIRE(r.value().title == "Tagesschau");

	// The moment the first ends is the moment the second begins, and both
	// rules take that moment, so the earlier one still answers for it.
	fake.now = 1900;
	Result<EventInfo> edge = epg::current(0x2b66);
	REQUIRE(edge.ok());
	REQUIRE(edge.value().title == "Tagesschau");

	fake.now = 1901;
	Result<EventInfo> later = epg::current(0x2b66);
	REQUIRE(later.ok());
	REQUIRE(later.value().title == "Tatort");
}

TEST_CASE("an empty EPG reports NotFound rather than an empty event", "[epg]")
{
	FakeEpgSource fake;
	InstalledGuideSources installed(&fake);

	Result<EventInfo> r = epg::current(0x2b66);
	REQUIRE_FALSE(r.ok());
	REQUIRE(r.error().status == Status::NotFound);
}

TEST_CASE("search rejects a query that is too short", "[epg]")
{
	FakeEpgSource fake;
	InstalledGuideSources installed(&fake);

	Result<SearchResult> r = epg::search("a", WHOLE_FROM, WHOLE_TO, 10);
	REQUIRE_FALSE(r.ok());
	REQUIRE(r.error().status == Status::InvalidArgument);
	REQUIRE(r.error().code == ErrorCode::QueryTooShort);

	// Refused before the source, or a one character query over every channel
	// would already have been run by the time it was turned down.
	REQUIRE(fake.last_query.empty());

	Result<SearchResult> empty = epg::search("", WHOLE_FROM, WHOLE_TO, 10);
	REQUIRE_FALSE(empty.ok());
	REQUIRE(empty.error().status == Status::InvalidArgument);
	REQUIRE(fake.last_query.empty());
}

/* The one read here that nothing else narrows: every channel's whole schedule,
   matched in three texts per event. Each of these is refused before the guide
   is touched, because a search that was going to be turned down must not have
   been run first. */
TEST_CASE("search will not run without a window and a bound on what it returns", "[epg]")
{
	FakeEpgSource fake;
	fake.events.push_back(mk(ERSTE, 1, "Tatort", 1000, 5400));
	InstalledGuideSources installed(&fake);

	Result<SearchResult> backwards = epg::search("Ta", 2000, 1000, 10);
	REQUIRE_FALSE(backwards.ok());
	REQUIRE(backwards.error().status == Status::InvalidArgument);
	REQUIRE(backwards.error().code == ErrorCode::EmptyWindow);
	REQUIRE(fake.last_query.empty());

	Result<SearchResult> instant = epg::search("Ta", 1000, 1000, 10);
	REQUIRE_FALSE(instant.ok());
	REQUIRE(instant.error().code == ErrorCode::EmptyWindow);
	REQUIRE(fake.last_query.empty());

	Result<SearchResult> nothing = epg::search("Ta", WHOLE_FROM, WHOLE_TO, 0);
	REQUIRE_FALSE(nothing.ok());
	REQUIRE(nothing.error().status == Status::InvalidArgument);
	REQUIRE(nothing.error().code == ErrorCode::NoRoomForAResult);
	REQUIRE(fake.last_query.empty());

	// Asking for more than one search builds is refused rather than answered
	// with fewer, which would be indistinguishable from a guide that held no
	// more.
	Result<SearchResult> greedy =
		epg::search("Ta", WHOLE_FROM, WHOLE_TO, epg::MAX_SEARCH_RESULTS + 1);
	REQUIRE_FALSE(greedy.ok());
	REQUIRE(greedy.error().status == Status::InvalidArgument);
	REQUIRE(greedy.error().code == ErrorCode::LimitTooLarge);
	REQUIRE(fake.last_query.empty());

	REQUIRE(epg::search("Ta", WHOLE_FROM, WHOLE_TO, epg::MAX_SEARCH_RESULTS).ok());
	REQUIRE(fake.last_query == "Ta");
}

TEST_CASE("two characters is the shortest query that reaches the source", "[epg]")
{
	FakeEpgSource fake;
	fake.events.push_back(mk(ERSTE, 1, "Tatort", 1000, 5400));
	fake.events.push_back(mk(ZDF,   2, "Tagesschau", 1000, 900));
	InstalledGuideSources installed(&fake);

	Result<SearchResult> r = epg::search("Ta", WHOLE_FROM, WHOLE_TO, 10);
	REQUIRE(r.ok());
	REQUIRE(fake.last_query == "Ta");
	REQUIRE(r.value().events.size() == 2);
	REQUIRE_FALSE(r.value().truncated);

	Result<SearchResult> none = epg::search("Sportschau", WHOLE_FROM, WHOLE_TO, 10);
	REQUIRE(none.ok());
	// Nothing matched is an answer, not a fault: there is a list to return.
	REQUIRE(none.value().events.empty());
	REQUIRE_FALSE(none.value().truncated);
}

/* The source on the box stops at the ceiling it is handed, so a caller asking
   for exactly what it wants back can never learn there was more. The layer above
   therefore asks for one more than it means to keep, and this holds for a source
   that honours the ceiling as much as for one that ignores it. A fake that could
   only ignore it is how this went unnoticed. */
TEST_CASE("a source that stops at the ceiling is still told apart from a guide that held no more", "[epg]")
{
	FakeEpgSource fake;
	fake.honours_bounds = true;
	fake.events.push_back(mk(ERSTE, 1, "Tatort eins", 3000, 600));
	fake.events.push_back(mk(ERSTE, 2, "Tatort zwei", 4000, 600));
	fake.events.push_back(mk(ZDF,   3, "Tatort drei", 5000, 600));
	fake.events.push_back(mk(ZDF,   4, "Tatort vier", 6000, 600));
	fake.events.push_back(mk(ERSTE, 5, "Tatort fünf", 7000, 600));
	InstalledGuideSources installed(&fake);

	// Two wanted, five there.
	Result<SearchResult> cut = epg::search("Tatort", 2000, 8000, 2);
	REQUIRE(cut.ok());
	REQUIRE(cut.value().events.size() == 2);
	REQUIRE(cut.value().truncated);
	// One more than was wanted is what makes that answerable at all, and the
	// source stopped there, which is the shape the box has and the one that
	// used to leave truncated false.
	REQUIRE(fake.last_limit == 3);
	REQUIRE(fake.last_returned == 3);

	// Five wanted, five there: full, and not cut.
	Result<SearchResult> whole = epg::search("Tatort", 2000, 8000, 5);
	REQUIRE(whole.ok());
	REQUIRE(whole.value().events.size() == 5);
	REQUIRE_FALSE(whole.value().truncated);

	// More wanted than there are.
	Result<SearchResult> roomy = epg::search("Tatort", 2000, 8000, 10);
	REQUIRE(roomy.ok());
	REQUIRE(roomy.value().events.size() == 5);
	REQUIRE_FALSE(roomy.value().truncated);

	// A source that stops at the ceiling still leaves the window to the layer
	// above for anything it did not filter: an event outside the window is not
	// counted towards the answer whichever of the two dropped it.
	// The first two have ended by 4700 and the third has not begun before
	// 5500, so exactly one is on air across this window.
	Result<SearchResult> narrow = epg::search("Tatort", 4700, 5500, 10);
	REQUIRE(narrow.ok());
	REQUIRE(narrow.value().events.size() == 1);
	REQUIRE(narrow.value().events[0].title == "Tatort drei");
	REQUIRE_FALSE(narrow.value().truncated);
}

/* A window and a count, applied the same way the window read applies its own,
   so that an event lying across an edge is in or out of both by one measure. */
TEST_CASE("search answers only inside the window and stops at the bound", "[epg]")
{
	FakeEpgSource fake;
	// Two inside the window, one that ended before it opened, one that starts
	// after it closes.
	fake.events.push_back(mk(ERSTE, 1, "Tatort früh",  1000, 500));
	fake.events.push_back(mk(ERSTE, 2, "Tatort eins",  3000, 600));
	fake.events.push_back(mk(ZDF,   3, "Tatort zwei",  4000, 600));
	fake.events.push_back(mk(ZDF,   4, "Tatort spät",  9000, 600));
	InstalledGuideSources installed(&fake);

	Result<SearchResult> inside = epg::search("Tatort", 2000, 8000, 10);
	REQUIRE(inside.ok());
	REQUIRE(inside.value().events.size() == 2);
	REQUIRE(inside.value().events[0].title == "Tatort eins");
	REQUIRE(inside.value().events[1].title == "Tatort zwei");
	REQUIRE_FALSE(inside.value().truncated);

	// Exactly as many as were asked for, with more behind them, is the one
	// case a list on its own cannot report.
	Result<SearchResult> cut = epg::search("Tatort", 2000, 8000, 1);
	REQUIRE(cut.ok());
	REQUIRE(cut.value().events.size() == 1);
	REQUIRE(cut.value().events[0].title == "Tatort eins");
	REQUIRE(cut.value().truncated);

	// And a bound the guide never reaches does not claim it was reached.
	Result<SearchResult> roomy = epg::search("Tatort", 2000, 8000, 2);
	REQUIRE(roomy.ok());
	REQUIRE(roomy.value().events.size() == 2);
	REQUIRE_FALSE(roomy.value().truncated);
}

TEST_CASE("a window keeps every event that is on air inside it", "[epg]")
{
	FakeEpgSource fake;
	// Started before the window opens and still running when it does.
	fake.events.push_back(mk(ERSTE, 1, "Nachtmagazin", 500, 600));
	// Ends exactly where the window opens, so it is on air at that moment.
	fake.events.push_back(mk(ERSTE, 2, "Wetter", 400, 600));
	// Over before the window opens.
	fake.events.push_back(mk(ERSTE, 3, "Sandmann", 300, 600));
	// Inside.
	fake.events.push_back(mk(ERSTE, 4, "Tagesschau", 1500, 900));
	// Begins exactly where the window ends, which the window does not hold.
	fake.events.push_back(mk(ERSTE, 5, "Tatort", 2000, 5400));
	// Another channel entirely.
	fake.events.push_back(mk(ZDF,   6, "Heute", 1500, 900));
	InstalledGuideSources installed(&fake);

	Result<EventList> r = epg::forChannel(ERSTE, 1000, 2000);
	REQUIRE(r.ok());
	REQUIRE(r.value().size() == 3);
	REQUIRE(r.value()[0].title == "Nachtmagazin");
	REQUIRE(r.value()[1].title == "Wetter");
	REQUIRE(r.value()[2].title == "Tagesschau");
}

TEST_CASE("the two rules place an event the same way at every boundary", "[epg]")
{
	// The relation stated above both of them: an event covers a moment exactly
	// when the window holding that moment alone holds the event. Asserted
	// against the rules themselves rather than through a source, because they
	// are the production code and no source is linked into this binary.
	const time_t start = 1000;
	const unsigned lengths[] = { 0, 1, 900 };

	for (size_t l = 0; l < sizeof(lengths) / sizeof(lengths[0]); l++)
	{
		const time_t duration = (time_t) lengths[l];
		// Both ends and one step either side of each, which is where two rules
		// that mean the same thing can still differ.
		const time_t probes[] = {
			start - 2, start - 1, start, start + 1,
			start + duration - 1, start + duration,
			start + duration + 1, start + duration + 2
		};
		for (size_t i = 0; i < sizeof(probes) / sizeof(probes[0]); i++)
		{
			const time_t when = probes[i];
			bool covers = eventCoversTime(start, lengths[l], when);
			bool inside = eventInWindow(start, lengths[l], when, when + 1);
			REQUIRE(covers == inside);
			// Spelled out once, so the two cannot agree by being wrong alike.
			REQUIRE(covers == (when >= start && when <= start + duration));
		}
	}
}

TEST_CASE("the window rule is closed on the event and half open on itself", "[epg]")
{
	// Directly on the rule, for the same reason as the case above.

	// An event still on air when the window opens is in, including the one
	// whose last moment is that moment.
	REQUIRE(eventInWindow(400, 600, 1000, 2000));
	REQUIRE_FALSE(eventInWindow(300, 600, 1000, 2000));
	// One that begins where the window ends is out, because the window does
	// not hold its own end and windows laid end to end must not overlap.
	REQUIRE_FALSE(eventInWindow(2000, 5400, 1000, 2000));
	REQUIRE(eventInWindow(1999, 5400, 1000, 2000));
	// An event of no length is placed by its start alone.
	REQUIRE(eventInWindow(1000, 0, 1000, 2000));
	REQUIRE_FALSE(eventInWindow(999, 0, 1000, 2000));
	REQUIRE_FALSE(eventInWindow(2000, 0, 1000, 2000));
}

TEST_CASE("the whole schedule window keeps every event of the channel", "[epg]")
{
	FakeEpgSource fake;
	fake.events.push_back(mk(ERSTE, 1, "Tagesschau", 1000, 900));
	fake.events.push_back(mk(ERSTE, 2, "Tatort", 1900, 5400));
	fake.events.push_back(mk(ZDF,   3, "Heute", 1000, 900));
	InstalledGuideSources installed(&fake);

	Result<EventList> r = epg::forChannel(ERSTE, WHOLE_FROM, WHOLE_TO);
	REQUIRE(r.ok());
	REQUIRE(r.value().size() == 2);
	REQUIRE(r.value()[0].event_id == 1);
	REQUIRE(r.value()[1].event_id == 2);
	REQUIRE(r.value()[0].channel_id == ERSTE);
	REQUIRE((const void *) &r.value()[0] == fake.last_buffer);
}

TEST_CASE("an event that outlasts the range is still inside the window", "[epg]")
{
	// The end of a programme is a start plus a duration, and near the top of
	// the range that sum is not representable. Computed the wrong way it wraps
	// below the window and the event disappears from the whole schedule read.
	const time_t top = std::numeric_limits<time_t>::max();

	REQUIRE(eventInWindow(top - 10, 100, top - 5, top));
	REQUIRE(eventCoversTime(top - 10, 100, top - 5));

	FakeEpgSource fake;
	fake.events.push_back(mk(ERSTE, 1, "Bis ans Ende der Zeit", top - 10, 100));
	InstalledGuideSources installed(&fake);

	Result<EventList> r = epg::forChannel(ERSTE, top - 5, top);
	REQUIRE(r.ok());
	REQUIRE(r.value().size() == 1);
}

TEST_CASE("the whole schedule window holds an event at either end of the range", "[epg]")
{
	// It is what every converted endpoint asks for, so anything it drops is an
	// event the reader it replaced would have printed.
	const time_t top = std::numeric_limits<time_t>::max();
	const time_t bottom = std::numeric_limits<time_t>::min();

	FakeEpgSource fake;
	fake.events.push_back(mk(ERSTE, 1, "Ganz frueh", bottom, 0));
	fake.events.push_back(mk(ERSTE, 2, "Punkt Null", 0, 0));
	fake.events.push_back(mk(ERSTE, 3, "Ganz spaet", top - 1, 0));
	InstalledGuideSources installed(&fake);

	Result<EventList> r = epg::forChannel(ERSTE, WHOLE_FROM, WHOLE_TO);
	REQUIRE(r.ok());
	REQUIRE(r.value().size() == 3);
}

TEST_CASE("a channel with nothing scheduled is an empty list and not an error", "[epg]")
{
	FakeEpgSource fake;
	fake.events.push_back(mk(ZDF, 1, "Heute", 1000, 900));
	InstalledGuideSources installed(&fake);

	Result<EventList> r = epg::forChannel(ERSTE, WHOLE_FROM, WHOLE_TO);
	REQUIRE(r.ok());
	REQUIRE(r.value().empty());
}

TEST_CASE("a window that cannot hold anything is refused before the source", "[epg]")
{
	FakeEpgSource fake;
	fake.events.push_back(mk(ERSTE, 1, "Tagesschau", 1000, 900));
	InstalledGuideSources installed(&fake);

	Result<EventList> same = epg::forChannel(ERSTE, 1000, 1000);
	REQUIRE_FALSE(same.ok());
	REQUIRE(same.error().status == Status::InvalidArgument);

	Result<EventList> backwards = epg::forChannel(ERSTE, 2000, 1000);
	REQUIRE_FALSE(backwards.ok());
	REQUIRE(backwards.error().status == Status::InvalidArgument);

	// Nothing was read, so the refusal is the domain's and not an empty
	// schedule that happened to look like one.
	REQUIRE(fake.event_reads == 0);
}

TEST_CASE("nothing on now is named apart from a guide that cannot be read", "[epg]")
{
	FakeEpgSource fake;
	fake.events.push_back(mk(ERSTE, 1, "Tagesschau", 1000, 900));
	fake.now = 5000;
	InstalledGuideSources installed(&fake);

	// Both are failures and both can carry NotFound, so the status alone does
	// not separate them. The code is what a caller reads to tell a channel
	// with nothing on from a guide it should try again later.
	Result<EventInfo> nothing_on = epg::current(ERSTE);
	REQUIRE_FALSE(nothing_on.ok());
	REQUIRE(nothing_on.error().status == Status::NotFound);
	REQUIRE(nothing_on.error().code == ErrorCode::NoCurrentEvent);

	fake.status = Status::Internal;
	Result<EventInfo> broken = epg::current(ERSTE);
	REQUIRE_FALSE(broken.ok());
	REQUIRE(broken.error().code == ErrorCode::EpgUnavailable);
}

TEST_CASE("every read hands the source's own status on", "[epg]")
{
	FakeEpgSource fake;
	fake.events.push_back(mk(ERSTE, 1, "Tagesschau", 1000, 900));
	fake.now = 1200;
	InstalledGuideSources installed(&fake);

	// An event manager that has not started answers Internal, and all three
	// reads have to report that rather than a channel with nothing on it.
	fake.status = Status::Internal;

	Result<EventList> list = epg::forChannel(ERSTE, WHOLE_FROM, WHOLE_TO);
	REQUIRE_FALSE(list.ok());
	REQUIRE(list.error().status == Status::Internal);

	Result<EventInfo> now = epg::current(ERSTE);
	REQUIRE_FALSE(now.ok());
	REQUIRE(now.error().status == Status::Internal);

	Result<SearchResult> found = epg::search("Tagesschau", WHOLE_FROM, WHOLE_TO, 10);
	REQUIRE_FALSE(found.ok());
	REQUIRE(found.error().status == Status::Internal);

	fake.status = Status::Ok;
	REQUIRE(epg::current(ERSTE).ok());
}

TEST_CASE("the two names an event carries are kept apart", "[epg]")
{
	// The layers below call the event's name a description and its short text
	// a text, so a straight carry of those names would leave the title in the
	// field the endpoints print as a description.
	FakeEpgSource fake;
	EventInfo e = mk(ERSTE, 1, "Tagesschau", 1000, 900);
	e.description = "Nachrichten und Wetter";
	fake.events.push_back(e);
	fake.now = 1200;
	InstalledGuideSources installed(&fake);

	Result<EventInfo> r = epg::current(ERSTE);
	REQUIRE(r.ok());
	REQUIRE(r.value().title == "Tagesschau");
	REQUIRE(r.value().description == "Nachrichten und Wetter");
	REQUIRE(r.value().event_id == 1);
	REQUIRE(r.value().channel_id == ERSTE);
	REQUIRE(r.value().start == 1000);
	REQUIRE(r.value().duration == 900);
}

/* The grid, which is the read that answers for several channels at once, and
   the three things it promises about an answer too big to send whole: a row cut
   at its ceiling says so, a page cut at its own ends early and still says there
   is more, and the walk always advances. */
namespace
{
const ChannelId DRITTE = 0xc0de040400018000ULL;

ChannelIdList named(ChannelId a)
{
	ChannelIdList out;
	out.push_back(a);
	return out;
}

ChannelIdList named(ChannelId a, ChannelId b)
{
	ChannelIdList out = named(a);
	out.push_back(b);
	return out;
}

ChannelIdList named(ChannelId a, ChannelId b, ChannelId c)
{
	ChannelIdList out = named(a, b);
	out.push_back(c);
	return out;
}

// One channel's worth of back to back half hour events, starting at the epoch,
// so that a case can say how wide a row it wants and read the ceilings against
// it rather than against a number written twice.
void fill(FakeEpgSource &fake, ChannelId channel, size_t howmany, uint64_t first_id)
{
	for (size_t i = 0; i < howmany; i++)
		fake.events.push_back(mk(channel, first_id + i, "Sendung",
					 (time_t)(i * 1800), 1800));
}

// What the whole of a filled channel's schedule lies inside, so that a case
// asking for a row is never also asking about the window.
const time_t DAYS_FROM = 0;
const time_t DAYS_TO   = 400 * 1800;
}

TEST_CASE("a grid will not run without a window and a bound on the page", "[epg]")
{
	FakeEpgSource fake;
	fill(fake, ERSTE, 2, 1);
	InstalledGuideSources installed(&fake);

	Result<epg::Grid> backwards = epg::grid(named(ERSTE), 2000, 1000, false, 0, 10);
	REQUIRE_FALSE(backwards.ok());
	REQUIRE(backwards.error().status == Status::InvalidArgument);
	REQUIRE(backwards.error().code == ErrorCode::EmptyWindow);
	// Refused before the guide, or a grid that was going to be turned down
	// would have been read first.
	REQUIRE(fake.event_reads == 0);

	Result<epg::Grid> instant = epg::grid(named(ERSTE), 1000, 1000, false, 0, 10);
	REQUIRE_FALSE(instant.ok());
	REQUIRE(instant.error().code == ErrorCode::EmptyWindow);
	REQUIRE(fake.event_reads == 0);

	Result<epg::Grid> nothing = epg::grid(named(ERSTE), DAYS_FROM, DAYS_TO, false, 0, 0);
	REQUIRE_FALSE(nothing.ok());
	REQUIRE(nothing.error().code == ErrorCode::NoRoomForAResult);
	REQUIRE(fake.event_reads == 0);

	// Asking for more channels than one page answers for is refused rather
	// than answered with fewer, which is the reason a search's own ceiling is
	// refused: a page shorter than the one asked for cannot be told from a
	// list that held no more.
	Result<epg::Grid> greedy =
		epg::grid(named(ERSTE), DAYS_FROM, DAYS_TO, false, 0, epg::MAX_GRID_CHANNELS + 1);
	REQUIRE_FALSE(greedy.ok());
	REQUIRE(greedy.error().code == ErrorCode::LimitTooLarge);
	REQUIRE(fake.event_reads == 0);

	REQUIRE(epg::grid(named(ERSTE), DAYS_FROM, DAYS_TO, false, 0,
			  epg::MAX_GRID_CHANNELS).ok());
	REQUIRE(fake.event_reads == 1);
}

TEST_CASE("a grid answers one row per channel, in the order they were named", "[epg]")
{
	FakeEpgSource fake;
	fill(fake, ERSTE, 2, 1);
	fill(fake, ZDF, 3, 100);
	InstalledGuideSources installed(&fake);

	// Named the other way round from the order they were filled in, so that
	// the order of the rows is the caller's and not the guide's.
	Result<epg::Grid> r = epg::grid(named(ZDF, ERSTE), DAYS_FROM, DAYS_TO, false, 0, 10);
	REQUIRE(r.ok());
	const epg::Grid page = std::move(r).value();
	REQUIRE(page.channels.size() == 2);
	REQUIRE(page.channels[0].id == ZDF);
	REQUIRE(page.channels[0].events.size() == 3);
	REQUIRE(page.channels[1].id == ERSTE);
	REQUIRE(page.channels[1].events.size() == 2);
	REQUIRE_FALSE(page.more);
}

/* A channel nothing is known about is a row with nothing in it and not a
   refusal, which is the same answer the one channel read gives: the event
   manager cannot tell a channel it has never seen from one that is off air, so
   a grid that refused the whole page for one such channel would refuse it for
   a channel that is merely quiet. */
TEST_CASE("a channel the guide knows nothing about is an empty row", "[epg]")
{
	FakeEpgSource fake;
	fill(fake, ERSTE, 2, 1);
	InstalledGuideSources installed(&fake);

	Result<epg::Grid> r = epg::grid(named(ERSTE, DRITTE), DAYS_FROM, DAYS_TO, false, 0, 10);
	REQUIRE(r.ok());
	const epg::Grid page = std::move(r).value();
	REQUIRE(page.channels.size() == 2);
	REQUIRE(page.channels[1].id == DRITTE);
	REQUIRE(page.channels[1].events.empty());
	REQUIRE_FALSE(page.channels[1].truncated);
}

/* The rule the whole walk rests on. A cursor names an identifier, so one
   identifier standing in two places in the list is two places one cursor names,
   and the page beginning at the first of them can be the page whose last row is
   that identifier again: the cursor comes back unchanged and the walk never
   advances. */
TEST_CASE("a channel named twice is one row, at the place it was first named", "[epg]")
{
	FakeEpgSource fake;
	fill(fake, ERSTE, 1, 1);
	fill(fake, ZDF, 1, 100);
	InstalledGuideSources installed(&fake);

	Result<epg::Grid> r = epg::grid(named(ERSTE, ZDF, ERSTE), DAYS_FROM, DAYS_TO,
					false, 0, 10);
	REQUIRE(r.ok());
	const epg::Grid page = std::move(r).value();
	REQUIRE(page.channels.size() == 2);
	REQUIRE(page.channels[0].id == ERSTE);
	REQUIRE(page.channels[1].id == ZDF);
	REQUIRE_FALSE(page.more);

	// And the walk still ends where the list does: a page of one begun after
	// the first row is the second row and nothing behind it, rather than the
	// repeat coming back as a third place.
	Result<epg::Grid> second = epg::grid(named(ERSTE, ZDF, ERSTE), DAYS_FROM, DAYS_TO,
					     true, ERSTE, 1);
	REQUIRE(second.ok());
	const epg::Grid after = std::move(second).value();
	REQUIRE(after.channels.size() == 1);
	REQUIRE(after.channels[0].id == ZDF);
	REQUIRE_FALSE(after.more);
}

TEST_CASE("a grid walks its channels a page at a time and the walk ends", "[epg]")
{
	FakeEpgSource fake;
	fill(fake, ERSTE, 1, 1);
	fill(fake, ZDF, 1, 100);
	fill(fake, DRITTE, 1, 200);
	InstalledGuideSources installed(&fake);

	const ChannelIdList all = named(ERSTE, ZDF, DRITTE);

	Result<epg::Grid> first = epg::grid(all, DAYS_FROM, DAYS_TO, false, 0, 2);
	REQUIRE(first.ok());
	const epg::Grid one = std::move(first).value();
	REQUIRE(one.channels.size() == 2);
	REQUIRE(one.more);
	// Where the next page begins is the last row of this one and not the last
	// channel of the list.
	REQUIRE(one.next_after == ZDF);

	Result<epg::Grid> second = epg::grid(all, DAYS_FROM, DAYS_TO, true, one.next_after, 2);
	REQUIRE(second.ok());
	const epg::Grid two = std::move(second).value();
	REQUIRE(two.channels.size() == 1);
	REQUIRE(two.channels[0].id == DRITTE);
	// Nothing behind it, so the walk is over and the caller has each channel
	// once.
	REQUIRE_FALSE(two.more);
}

TEST_CASE("a cursor naming a channel the request did not ask about is refused", "[epg]")
{
	FakeEpgSource fake;
	fill(fake, ERSTE, 1, 1);
	InstalledGuideSources installed(&fake);

	/* Refused rather than answered with the first page, which is what starting
	   again would give: a caller walking pages until the cursor comes back
	   empty would then walk them for ever with nothing to see that it was. */
	Result<epg::Grid> r = epg::grid(named(ERSTE), DAYS_FROM, DAYS_TO, true, ZDF, 10);
	REQUIRE_FALSE(r.ok());
	REQUIRE(r.error().status == Status::InvalidArgument);
	REQUIRE(r.error().code == ErrorCode::NoSuchChannel);
	REQUIRE(fake.event_reads == 0);
}

TEST_CASE("a row cut at its ceiling says so and one that was not does not", "[epg]")
{
	FakeEpgSource fake;
	fill(fake, ERSTE, epg::MAX_EVENTS_PER_CHANNEL + 1, 1);
	fill(fake, ZDF, epg::MAX_EVENTS_PER_CHANNEL, 10000);
	InstalledGuideSources installed(&fake);

	Result<epg::Grid> r = epg::grid(named(ERSTE, ZDF), DAYS_FROM, DAYS_TO, false, 0, 10);
	REQUIRE(r.ok());
	const epg::Grid page = std::move(r).value();
	REQUIRE(page.channels.size() == 2);

	REQUIRE(page.channels[0].events.size() == epg::MAX_EVENTS_PER_CHANNEL);
	REQUIRE(page.channels[0].truncated);
	// What a cut row loses is its tail, so the earliest events are the ones
	// that survive and a caller can ask for the rest by opening its window
	// after the last one it was given.
	REQUIRE(page.channels[0].events[0].event_id == 1);

	/* The row that is exactly as long as the ceiling with nothing behind it.
	   It is the one case a length cannot speak for, which is the whole reason
	   the row carries a word about itself. */
	REQUIRE(page.channels[1].events.size() == epg::MAX_EVENTS_PER_CHANNEL);
	REQUIRE_FALSE(page.channels[1].truncated);
}

/* The failure this answer is shaped against: a page that ended at its own
   ceiling holds fewer rows than were asked for and is not the last page, so
   nothing but the flag can say the walk goes on. A caller reading the number of
   rows instead stops here, and what it loses is every channel behind this
   point. */
TEST_CASE("a page cut short by its event ceiling still says there is more", "[epg]")
{
	FakeEpgSource fake;
	// Each row is filled to its own ceiling, so the page ceiling is what ends
	// the page rather than the channel limit, which is set above the number of
	// channels there are.
	const size_t rows = (epg::MAX_EVENTS_PER_PAGE / epg::MAX_EVENTS_PER_CHANNEL) + 2;
	REQUIRE(rows <= epg::MAX_GRID_CHANNELS);

	ChannelIdList all;
	for (size_t i = 0; i < rows; i++)
	{
		const ChannelId channel = ERSTE + i;
		all.push_back(channel);
		fill(fake, channel, epg::MAX_EVENTS_PER_CHANNEL, 1 + i * 1000);
	}
	InstalledGuideSources installed(&fake);

	Result<epg::Grid> r = epg::grid(all, DAYS_FROM, DAYS_TO, false, 0, rows);
	REQUIRE(r.ok());
	const epg::Grid page = std::move(r).value();

	REQUIRE(page.channels.size() < rows);
	REQUIRE(page.more);
	REQUIRE(page.next_after == page.channels.back().id);
	// No row of it was cut: what ran out was the room on the page and not the
	// room in a row, and the two say different things to a caller.
	for (size_t i = 0; i < page.channels.size(); i++)
		REQUIRE_FALSE(page.channels[i].truncated);

	// And the page after it reaches what this one did not, so the walk that
	// was cut short still gets there.
	Result<epg::Grid> next = epg::grid(all, DAYS_FROM, DAYS_TO, true, page.next_after, rows);
	REQUIRE(next.ok());
	REQUIRE(std::move(next).value().channels.size() == rows - page.channels.size());
}

/* Whatever the ceilings are set to, a page carries at least one row. A page
   that came back empty naming the channel it began at would be a cursor that
   never moves, and a caller walking on it would ask the same question for
   ever. */
TEST_CASE("a page always carries a row, so the walk always advances", "[epg]")
{
	FakeEpgSource fake;
	const size_t rows = 40;
	ChannelIdList all;
	for (size_t i = 0; i < rows; i++)
	{
		const ChannelId channel = ERSTE + i;
		all.push_back(channel);
		fill(fake, channel, epg::MAX_EVENTS_PER_CHANNEL, 1 + i * 1000);
	}
	InstalledGuideSources installed(&fake);

	size_t seen = 0;
	bool have_cursor = false;
	ChannelId cursor = 0;
	// Bounded, because the thing being held is that the walk ends: a walk that
	// stopped advancing would run here until this ran out rather than hanging.
	for (size_t round = 0; round < rows + 2; round++)
	{
		Result<epg::Grid> r = epg::grid(all, DAYS_FROM, DAYS_TO, have_cursor, cursor,
						epg::MAX_GRID_CHANNELS);
		REQUIRE(r.ok());
		const epg::Grid page = std::move(r).value();
		REQUIRE_FALSE(page.channels.empty());
		seen += page.channels.size();
		if (!page.more)
			break;
		have_cursor = true;
		cursor = page.next_after;
	}
	// Each channel once and none passed over.
	REQUIRE(seen == rows);
}

TEST_CASE("a guide that cannot be read fails the whole page", "[epg]")
{
	FakeEpgSource fake;
	fill(fake, ERSTE, 1, 1);
	fill(fake, ZDF, 1, 100);
	InstalledGuideSources installed(&fake);

	/* A page short one row would read as a channel with nothing on it, which
	   is the same lie a silently cut list tells and is worse here: it is a row
	   the caller asked for by name. */
	fake.status = Status::Internal;
	Result<epg::Grid> r = epg::grid(named(ERSTE, ZDF), DAYS_FROM, DAYS_TO, false, 0, 10);
	REQUIRE_FALSE(r.ok());
	REQUIRE(r.error().status == Status::Internal);
	REQUIRE(r.error().code == ErrorCode::EpgUnavailable);
}

TEST_CASE("one event answers the three things a listing does not carry", "[epg]")
{
	FakeEpgSource fake;
	EventDetail d;
	d.event_id = 0x2b660000abcdULL;
	d.channel_id = ERSTE;
	d.title = "Tatort";
	d.description = "Krimireihe";
	d.long_description = "Ein Fall fuer zwei Kommissare in Muenster.";
	d.start = 1000;
	d.duration = 5400;
	d.rating = 12;
	d.genre = 0x11;
	fake.details.push_back(d);
	InstalledGuideSources installed(&fake);

	Result<EventDetail> r = epg::event(d.event_id, 1000);
	REQUIRE(r.ok());
	REQUIRE(r.value().title == "Tatort");
	// The two texts apart, which is what a listing cannot answer: the event
	// manager fills a listing's short text with the beginning of the long one
	// where an event carries no short text.
	REQUIRE(r.value().description == "Krimireihe");
	REQUIRE(r.value().long_description == "Ein Fall fuer zwei Kommissare in Muenster.");
	REQUIRE(r.value().rating == 12);
	REQUIRE(r.value().genre == 0x11);
}

TEST_CASE("an event is named by its identifier and the moment together", "[epg]")
{
	FakeEpgSource fake;
	EventDetail d;
	d.event_id = 0x2b660000abcdULL;
	d.channel_id = ERSTE;
	d.title = "Tatort";
	d.start = 1000;
	d.duration = 5400;
	fake.details.push_back(d);
	InstalledGuideSources installed(&fake);

	// One identifier carries a showing per broadcast, so the moment is what
	// says which of them is meant and an identifier on its own names several.
	Result<EventDetail> wrong_moment = epg::event(d.event_id, 2000);
	REQUIRE_FALSE(wrong_moment.ok());
	REQUIRE(wrong_moment.error().status == Status::NotFound);
	REQUIRE(wrong_moment.error().code == ErrorCode::NoSuchEvent);

	Result<EventDetail> unknown = epg::event(0x99, 1000);
	REQUIRE_FALSE(unknown.ok());
	REQUIRE(unknown.error().code == ErrorCode::NoSuchEvent);

	// The guide not answering at all is a different thing from its not holding
	// the event, and a caller retries one and not the other.
	fake.status = Status::Internal;
	Result<EventDetail> broken = epg::event(d.event_id, 1000);
	REQUIRE_FALSE(broken.ok());
	REQUIRE(broken.error().status == Status::Internal);
	REQUIRE(broken.error().code == ErrorCode::EpgUnavailable);
}

/* What every read below is about: the guide keeps the lower forty eight bits of
   an identifier, and a channel is named by all sixty four everywhere else here.
   The cases were written against a box that answered the half, which is not a
   channel any route of this API answers on, and a timer built out of one named
   a channel the box does not have. */

namespace
{
// The half the guide keeps of the two channels above, which is what an event
// off the guide's own walk carries.
const ChannelId ERSTE_KEY = ERSTE & GUIDE_KEY_MASK;

ChannelInfo channelOf(ChannelId id, ChannelId epg_id, const char *name)
{
	ChannelInfo c;
	c.id = id;
	c.epg_id = epg_id;
	c.name = name;
	c.kind = ServiceKind::Tv;
	return c;
}
}

TEST_CASE("a hit off the guide's own walk is named the way a channel route names one", "[epg]")
{
	FakeEpgSource fake;
	fake.events.push_back(mk(ERSTE_KEY, 1, "Tatort", 1000, 5400));
	InstalledGuideSources installed(&fake);
	installed.channels.channels.push_back(channelOf(ERSTE, ERSTE, "Das Erste HD"));

	Result<SearchResult> r = epg::search("Tatort", WHOLE_FROM, WHOLE_TO, 10);
	REQUIRE(r.ok());
	REQUIRE(r.value().events.size() == 1);
	REQUIRE(r.value().events[0].channel_id == ERSTE);

	// And the same for the read that answers one event, which takes its channel
	// out of the same half identifier.
	EventDetail d;
	d.event_id = 1;
	d.channel_id = ERSTE_KEY;
	d.start = 1000;
	fake.details.push_back(d);
	Result<EventDetail> one = epg::event(1, 1000);
	REQUIRE(one.ok());
	REQUIRE(one.value().channel_id == ERSTE);
}

TEST_CASE("two channels sharing the half the guide keeps answer with one of them, and always the same one", "[epg]")
{
	/* This is the case the box cannot resolve and has never been able to: the
	   guide holds one schedule under those bits and does not know which of the
	   two services it came from. What matters is that whatever comes back is a
	   channel this box has and that it does not move between two reads, because
	   a caller reads it and then asks the channel route about it. */
	const ChannelId lower  = 0x0001040200016dcbULL;
	const ChannelId higher = 0xb9b0040200016dcbULL;
	REQUIRE((lower & GUIDE_KEY_MASK) == (higher & GUIDE_KEY_MASK));

	FakeEpgSource fake;
	fake.events.push_back(mk(lower & GUIDE_KEY_MASK, 1, "Tatort", 1000, 5400));
	InstalledGuideSources installed(&fake);

	// Named in the order that would answer wrongly if the first held won.
	installed.channels.channels.push_back(channelOf(higher, higher, "Das Erste HD"));
	installed.channels.channels.push_back(channelOf(lower, lower, "Das Erste"));

	Result<SearchResult> first = epg::search("Tatort", WHOLE_FROM, WHOLE_TO, 10);
	REQUIRE(first.ok());
	REQUIRE(first.value().events.size() == 1);
	REQUIRE(first.value().events[0].channel_id == lower);

	Result<SearchResult> again = epg::search("Tatort", WHOLE_FROM, WHOLE_TO, 10);
	REQUIRE(again.ok());
	REQUIRE(again.value().events.size() == 1);
	REQUIRE(again.value().events[0].channel_id == lower);

	// Asked about either of them by name, each answers for itself: the half is
	// ambiguous and a whole identifier is not, so the widening never overrides
	// a caller who already said which channel it meant.
	Result<EventList> mine = epg::forChannel(higher, WHOLE_FROM, WHOLE_TO);
	REQUIRE(mine.ok());
	REQUIRE(mine.value().size() == 1);
	REQUIRE(mine.value()[0].channel_id == higher);
}

TEST_CASE("a hit for a channel the box does not carry keeps the half the guide holds", "[epg]")
{
	FakeEpgSource fake;
	fake.events.push_back(mk(ERSTE_KEY, 1, "Tatort", 1000, 5400));
	InstalledGuideSources installed(&fake);

	/* Nothing to widen it from, and the entry is one the guide really holds, so
	   it is answered as it stands rather than dropped. A screen can tell the
	   two apart because it finds no channel of that name. */
	Result<SearchResult> r = epg::search("Tatort", WHOLE_FROM, WHOLE_TO, 10);
	REQUIRE(r.ok());
	REQUIRE(r.value().events.size() == 1);
	REQUIRE(r.value().events[0].channel_id == ERSTE_KEY);
}

TEST_CASE("a channel whose schedule is filed under another's is asked for under that one and answered under its own", "[epg]")
{
	/* A standard and a high definition service sharing one schedule is the
	   ordinary shape of this on a box. Asked with the channel's own identifier,
	   the guide holds nothing under it; asked with the other's, everything that
	   comes back would name the other channel, and a timer made out of a row
	   would record the wrong one. */
	FakeEpgSource fake;
	fake.events.push_back(mk(ZDF, 1, "heute", 1000, 900));
	fake.now = 1200;
	InstalledGuideSources installed(&fake);
	installed.channels.channels.push_back(channelOf(ZDF, ZDF, "ZDF HD"));
	installed.channels.channels.push_back(channelOf(ERSTE, ZDF, "ZDF"));

	Result<EventList> r = epg::forChannel(ERSTE, WHOLE_FROM, WHOLE_TO);
	REQUIRE(r.ok());
	REQUIRE(r.value().size() == 1);
	REQUIRE(r.value()[0].title == "heute");
	REQUIRE(r.value()[0].channel_id == ERSTE);

	Result<EventInfo> now = epg::current(ERSTE);
	REQUIRE(now.ok());
	REQUIRE(now.value().title == "heute");
	REQUIRE(now.value().channel_id == ERSTE);

	// And the channel the schedule really belongs to still answers for itself.
	Result<EventList> host = epg::forChannel(ZDF, WHOLE_FROM, WHOLE_TO);
	REQUIRE(host.ok());
	REQUIRE(host.value().size() == 1);
	REQUIRE(host.value()[0].channel_id == ZDF);
}

TEST_CASE("a grid row keeps the name the request gave it and its events carry the whole one", "[epg]")
{
	/* The row is what a cursor names, so it has to come back under the name it
	   was asked for or a walk cannot be followed. The events inside it are what
	   a timer is made out of, so they carry the identifier the channel routes
	   answer on. */
	FakeEpgSource fake;
	fake.events.push_back(mk(ERSTE_KEY, 1, "Tatort", 1000, 5400));
	InstalledGuideSources installed(&fake);
	installed.channels.channels.push_back(channelOf(ERSTE, ERSTE, "Das Erste HD"));

	ChannelIdList ids;
	ids.push_back(ERSTE_KEY);

	Result<epg::Grid> r = epg::grid(ids, WHOLE_FROM, WHOLE_TO, false, 0, 10);
	REQUIRE(r.ok());
	REQUIRE(r.value().channels.size() == 1);
	REQUIRE(r.value().channels[0].id == ERSTE_KEY);
	REQUIRE(r.value().channels[0].events.size() == 1);
	REQUIRE(r.value().channels[0].events[0].channel_id == ERSTE);
}
