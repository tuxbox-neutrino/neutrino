/*
 * epg.cpp - programme data, searches, and one channel schedule
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

#include "epg.h"
#include "coreapi/base/errors.h"
#include "coreapi/base/deps.h"

#include <cstdio>
#include <limits>
#include <map>
#include <utility>

#include <fcntl.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

namespace coreapi
{
namespace epg
{

namespace
{

// The same five entities and the same drop of bytes below 32 the control API's
// own XML text form applies, so a title reads the same whichever generator
// wrote it.
std::string xmlText(const std::string &s)
{
	std::string out;
	out.reserve(s.size());
	for (size_t i = 0; i < s.size(); i++)
	{
		const unsigned char c = (unsigned char) s[i];
		switch (c)
		{
			case '<': out += "&lt;"; break;
			case '>': out += "&gt;"; break;
			case '&': out += "&amp;"; break;
			case '"': out += "&quot;"; break;
			case '\'': out += "&apos;"; break;
			default:
				if (c >= 32)
					out += (char) c;
		}
	}
	return out;
}

std::string hexChannelId(ChannelId id)
{
	char buf[24];
	std::snprintf(buf, sizeof(buf), "%llx", (unsigned long long) id);
	return std::string(buf);
}

// gmtime_r rather than gmtime: several requests answer at once here, and
// gmtime keeps its result in memory every caller on every thread shares.
std::string xmltvTimestamp(time_t t)
{
	struct tm tmv;
	gmtime_r(&t, &tmv);
	char buf[32];
	// Unqualified: <time.h> here is the C header and puts this name in the
	// global namespace rather than in std, unlike <cstdio>'s snprintf above.
	strftime(buf, sizeof(buf), "%Y%m%d%H%M%S %z", &tmv);
	return std::string(buf);
}

bool isRadioKind(ServiceKind k)
{
	return k == ServiceKind::Radio || k == ServiceKind::WebRadio;
}

// What the control API caps one channel's programmes at. Kept at the same
// number: this call streams instead of holding the document in memory, but the
// cap also keeps one channel with years of buffered schedule from crowding out
// every other channel's few days.
//
// Named for the document it bounds, so it is not read as the ceiling a grid row
// carries. The two are separate numbers because this one is what a copied API
// already wrote out and cannot move.
const size_t MAX_XMLTV_EVENTS_PER_CHANNEL = 50;

// Wide enough to hold everything a source has, so eventsForChannel is asked for
// one channel's whole known schedule rather than for a magic count. time_t is 32
// bits on the box, so its own maximum stands for the far end.
const time_t FAR_FUTURE = std::numeric_limits<time_t>::max();

bool writeAll(std::FILE *f, const std::string &s)
{
	return s.empty() || std::fwrite(s.data(), 1, s.size(), f) == s.size();
}

/* What the guide is asked about a channel, and what the answer may name that
   channel by. Two numbers, neither reliably the one a caller handed in.

   The guide is keyed by epg_id, which is another channel's wherever a box maps
   one schedule onto two services, and it keeps only its lower forty eight bits.
   A read that passed a caller's identifier straight through answered nothing for
   a mapped channel; one that stamped back what the guide holds named a channel
   by something no channel route answers on. Between them they made a timer built
   out of a guide entry point at a channel this box does not have.

   The channel list is the only place holding both halves, so it is asked once. A
   channel the box does not have leaves both numbers as they arrived: refusing
   the read would lose an entry the box really holds. */
struct GuideChannel
{
	ChannelId whole;
	ChannelId key;
};

/* The key a channel's schedule is filed under, and never nought. Nought is what
   the event manager reads as every channel at once, so a read meaning one
   channel and handing it nought comes back with the whole guide under that
   channel's name. Nothing on the box leaves the field empty. */
ChannelId guideKeyOf(ChannelId key, ChannelId whole)
{
	return key != 0 ? key : whole;
}

GuideChannel guideChannelOf(ChannelId asked)
{
	GuideChannel named;
	named.whole = asked;
	named.key = asked;

	ChannelId whole = 0;
	ChannelId key = 0;
	if (channelSource().guideChannel(asked, whole, key) == Status::Ok)
	{
		named.whole = whole;
		named.key = guideKeyOf(key, whole);
	}
	return named;
}

void stampChannel(EventList &events, ChannelId whole)
{
	for (size_t i = 0; i < events.size(); i++)
		events[i].channel_id = whole;
}

} // namespace

// As in the channel reads, the status always comes from the source, so one cause
// cannot become two answers on the way out.

Result<EventList> forChannel(ChannelId id, time_t from, time_t to)
{
	if (to <= from)
		return fail(Status::InvalidArgument, ErrorCode::EmptyWindow,
			    "the window ends before it begins");

	const GuideChannel named = guideChannelOf(id);

	EventList out;
	Status s = epgSource().eventsForChannel(named.key, from, to, out);
	if (s != Status::Ok)
		return fail(s, ErrorCode::EpgUnavailable,
			    "the programme guide could not be read");
	stampChannel(out, named.whole);
	return ok(std::move(out));
}

Result<EventInfo> current(ChannelId channel_id)
{
	const GuideChannel named = guideChannelOf(channel_id);

	EventInfo out;
	Status s = epgSource().currentEvent(named.key, out);
	if (s == Status::NotFound)
		return fail(s, ErrorCode::NoCurrentEvent,
			    "nothing is scheduled for this channel now");
	if (s != Status::Ok)
		return fail(s, ErrorCode::EpgUnavailable,
			    "the programme guide could not be read");
	out.channel_id = named.whole;
	return ok(std::move(out));
}

Result<EventDetail> event(uint64_t event_id, time_t start)
{
	EventDetail out;
	Status s = epgSource().eventDetail(event_id, start, out);
	if (s == Status::NotFound)
		return fail(s, ErrorCode::NoSuchEvent,
			    "the guide holds no event beginning then under that identifier");
	if (s != Status::Ok)
		return fail(s, ErrorCode::EpgUnavailable,
			    "the programme guide could not be read");
	/* The identifier an event carries holds the key the guide filed it under
	   and nothing above it, so this read names its channel by the same half
	   identifier a search does and is widened the same way. */
	out.channel_id = guideChannelOf(out.channel_id).whole;
	return ok(std::move(out));
}

Result<Grid> grid(const ChannelIdList &ids, time_t from, time_t to,
		  bool have_cursor, ChannelId cursor, size_t limit)
{
	if (to <= from)
		return fail(Status::InvalidArgument, ErrorCode::EmptyWindow,
			    "the window ends before it begins");
	if (limit == 0)
		return fail(Status::InvalidArgument, ErrorCode::NoRoomForAResult,
			    "a page that may hold nothing is not a page");
	if (limit > MAX_GRID_CHANNELS)
		return fail(Status::InvalidArgument, ErrorCode::LimitTooLarge,
			    "one page answers for fewer channels than that");

	// Before the cursor is looked for, because everything the walk promises
	// rests on one identifier naming one place in the list.
	ChannelIdList named;
	named.reserve(ids.size());
	for (size_t i = 0; i < ids.size(); i++)
	{
		bool already = false;
		for (size_t j = 0; j < named.size() && !already; j++)
			already = named[j] == ids[i];
		if (!already)
			named.push_back(ids[i]);
	}

	size_t begin = 0;
	if (have_cursor)
	{
		bool found = false;
		for (size_t i = 0; i < named.size() && !found; i++)
		{
			if (named[i] != cursor)
				continue;
			begin = i + 1;
			found = true;
		}
		// The request is wrong and not the box: the cursor and the channels it
		// is read against arrived in the same request.
		if (!found)
			return fail(Status::InvalidArgument, ErrorCode::NoSuchChannel,
				    "the cursor names a channel this request did not ask about");
	}

	Grid out;
	size_t events_on_page = 0;
	size_t next = begin;
	while (next < named.size() && out.channels.size() < limit)
	{
		/* The row is named by what the request named it and never by what the
		   channel list widened that to: the cursor a caller hands back is one of
		   these. The events inside carry the whole identifier. */
		const GuideChannel row_channel = guideChannelOf(named[next]);

		ChannelSchedule row;
		row.id = named[next];

		EventList held;
		Status s = epgSource().eventsForChannel(row_channel.key, from, to, held);
		if (s != Status::Ok)
			return fail(s, ErrorCode::EpgUnavailable,
				    "the programme guide could not be read");
		stampChannel(held, row_channel.whole);

		/* The window is applied again because a source may ignore what it was
		   handed, and the ceiling is counted against what is kept: a source
		   answering events outside the window would otherwise spend the row's
		   room on events this drops. */
		for (size_t k = 0; k < held.size(); k++)
		{
			if (!eventInWindow(held[k].start, held[k].duration, from, to))
				continue;
			if (row.events.size() == MAX_EVENTS_PER_CHANNEL)
			{
				row.truncated = true;
				break;
			}
			row.events.push_back(held[k]);
		}

		events_on_page += row.events.size();
		out.channels.push_back(row);
		next++;

		/* Asked after the row has been taken, so a page always carries at least
		   one row and the walk always advances. Asked first, a row the ceiling
		   had no room for would come back as an empty page naming the channel
		   the page began at, and a caller walking on that cursor would ask the
		   same question for ever. */
		if (events_on_page >= MAX_EVENTS_PER_PAGE)
			break;
	}

	/* Read off where the walk stopped and not off the number of rows: the two
	   part company on a page the event ceiling ended early. */
	out.more = next < named.size();
	if (out.more)
		out.next_after = out.channels.back().id;
	return ok(std::move(out));
}

Result<SearchResult> search(const std::string &q, time_t from, time_t to, size_t limit)
{
	if (q.size() < 2)
		return fail(Status::InvalidArgument, ErrorCode::QueryTooShort,
			    "the search needs at least two characters");
	if (to <= from)
		return fail(Status::InvalidArgument, ErrorCode::EmptyWindow,
			    "the window ends before it begins");
	if (limit == 0)
		return fail(Status::InvalidArgument, ErrorCode::NoRoomForAResult,
			    "a search that may return nothing is not a search");
	if (limit > MAX_SEARCH_RESULTS)
		return fail(Status::InvalidArgument, ErrorCode::LimitTooLarge,
			    "one search returns fewer than that");

	// One more than is wanted, so a source that stops at what it was asked for
	// still leaves this something to see. Without the extra one a source that
	// honours the ceiling exactly can never be told from a guide that held no
	// more, and truncated would never be set on the path that most needs it.
	EventList matched;
	Status s = epgSource().search(q, from, to, limit + 1, matched);
	if (s != Status::Ok)
		return fail(s, ErrorCode::EpgUnavailable,
			    "the programme guide could not be read");

	/* One lookup per channel and not one per hit: a search over a week comes back
	   with the same channel over and over, and the lookup behind this walks the
	   whole channel list. Kept for this call alone, the channel list being
	   rebuilt underneath a reader. */
	std::map<ChannelId, ChannelId> widened;

	SearchResult out;
	out.events.reserve(matched.size() < limit ? matched.size() : limit);
	// The window is applied again because a source may ignore what it was
	// handed; the count is what tells a full answer from a cut one.
	for (size_t i = 0; i < matched.size(); i++)
	{
		// The same rule the window read uses, so an event lying across the edge
		// is in or out of both by the same measure.
		if (!eventInWindow(matched[i].start, matched[i].duration, from, to))
			continue;
		if (out.events.size() == limit)
		{
			out.truncated = true;
			break;
		}

		/* This is the read the guide answers whole, so the hits carry the key
		   the guide filed them under. Widened here and not left to whoever draws
		   them: a hit is what a timer is made out of, and a timer made on a key
		   names no channel of this box. */
		const ChannelId key = matched[i].channel_id;
		std::map<ChannelId, ChannelId>::iterator seen = widened.find(key);
		if (seen == widened.end())
			seen = widened.insert(std::make_pair(key, guideChannelOf(key).whole)).first;
		matched[i].channel_id = seen->second;

		out.events.push_back(matched[i]);
	}
	return ok(std::move(out));
}

Result<std::string> xmltv(Kind kind)
{
	BouquetList bouquets;
	Status s = channelSource().allBouquets(bouquets);
	if (s != Status::Ok)
		return fail(s, ErrorCode::BouquetListUnavailable,
			    "the bouquet list could not be read");

	// One name per call, drawn from this process and a counter unique to it, so
	// one file being read out is never the one another request has just reopened
	// and is still writing.
	static long counter = 0;
	char path[64];
	std::snprintf(path, sizeof(path), "/tmp/coreapi-xmltv.%d.%ld.xml",
		      (int) getpid(), __sync_add_and_fetch(&counter, 1));

	const int fd = ::open(path, O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC, 0600);
	if (fd < 0)
		return fail(Status::Internal, ErrorCode::ChangeRefused,
			    "the guide could not be written to a file");

	std::FILE *f = fdopen(fd, "w");
	if (f == NULL)
	{
		::close(fd);
		::unlink(path);
		return fail(Status::Internal, ErrorCode::ChangeRefused,
			    "the guide could not be written to a file");
	}

	bool written = writeAll(f, "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
				    "<!DOCTYPE tv SYSTEM \"xmltv.dtd\">\n"
				    "<tv generator-info-name=\"Neutrino XMLTV Generator v1.0\">\n");

	for (size_t i = 0; written && i < bouquets.size(); i++)
	{
		const BouquetInfo &b = bouquets[i];
		// The same two conditions the control API filters a bouquet by
		// before it lists any of its channels.
		if (b.hidden || !b.user_bouquet)
			continue;

		ChannelList channels;
		if (channelSource().bouquetChannels(b.id, channels) != Status::Ok)
			continue;

		for (size_t j = 0; written && j < channels.size(); j++)
		{
			const ChannelInfo &c = channels[j];
			const bool radio = isRadioKind(c.kind);
			if (kind == Kind::Tv && radio)
				continue;
			if (kind == Kind::Radio && !radio)
				continue;

			const std::string cid = hexChannelId(c.id);
			written = writeAll(f, "<channel id=\"" + cid + "\">\n"
						"<display-name>" + xmlText(c.name) + "</display-name>\n"
						"</channel>\n");
			if (!written)
				break;

			/* The guide is keyed by epg_id and the listing already carries it,
			   so this is the one read here that needs no lookup. Asking with the
			   identifier instead left every channel a box maps another's
			   schedule onto empty in the document. */
			EventList events;
			if (epgSource().eventsForChannel(guideKeyOf(c.epg_id, c.id), 0, FAR_FUTURE,
							 events) != Status::Ok)
				continue;
			if (events.size() > MAX_XMLTV_EVENTS_PER_CHANNEL)
				events.resize(MAX_XMLTV_EVENTS_PER_CHANNEL);

			for (size_t k = 0; written && k < events.size(); k++)
			{
				const EventInfo &e = events[k];
				const time_t stop = e.start + (time_t) e.duration;
				written = writeAll(f, "<programme channel=\"" + cid +
							"\" start=\"" + xmltvTimestamp(e.start) +
							"\" stop=\"" + xmltvTimestamp(stop) + "\" >\n"
							"<title lang=\"de\">" + xmlText(e.title) + "</title>\n"
							"<desc lang=\"de\">" + xmlText(e.description) + "</desc>\n"
							"</programme>\n");
			}
		}
	}

	if (written)
		written = writeAll(f, "</tv>\n\r\n");

	// fclose flushes what fwrite buffered; a written flag that missed an earlier
	// short write must not be overridden by a close that goes on to succeed on
	// what is left.
	const bool closed = (std::fclose(f) == 0);
	if (!written || !closed)
	{
		::unlink(path);
		return fail(Status::Internal, ErrorCode::ChangeRefused,
			    "the guide could not be written to a file");
	}

	return ok(std::string(path));
}

} // namespace epg
} // namespace coreapi
