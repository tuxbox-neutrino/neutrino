/*
 * ep_epg.cpp - routes for programme data
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

#include "httpd/endpoints.h"

#include "httpd/endpoint.h"
#include "httpd/http.h"
#include "httpd/json.h"
#include "httpd/schema.h"
#include "httpd/status.h"

#include "coreapi/channels.h"
#include "coreapi/epg.h"
#include "coreapi/base/errors.h"
#include "coreapi/base/result.h"
#include "coreapi/base/types.h"

#include <cstddef>
#include <cstdio>
#include <string>
#include <utility>
#include <vector>

#include <stdint.h>

#include <fcntl.h>
#include <unistd.h>

namespace httpd
{

namespace
{

const unsigned long kDefaultSearchLimit = 100;

const FieldDesc kEventFields[] = {
	HTTPD_MEMBER("id", FieldType::ChannelId,
		"what the guide names this event by, hexadecimal, carrying the channel in its upper bits"),
	HTTPD_MEMBER("channel_id", FieldType::ChannelId,
		"the channel the event belongs to, hexadecimal, named the way the channel routes name one, and the half the guide keeps where this box carries no such channel"),
	HTTPD_MEMBER("title", FieldType::String, "what the event is called"),
	/* What the guide calls the short text: where an event has none the event manager fills
	   this field with the beginning of the long one, cut at a hundred and twenty bytes. Said
	   here rather than undone, undoing it taking a second read of the guide per event. */
	HTTPD_MEMBER("description", FieldType::String,
		"the short text beside the name, empty where there is none, and the beginning of the long text where the event carries no short one"),
	HTTPD_MEMBER("start", FieldType::Time, "when the event begins, seconds since the epoch"),
	HTTPD_MEMBER("duration", FieldType::UInt, "how long it runs, in seconds"),
};

const Schema kEventSchema = { "event", HTTPD_FIELDS(kEventFields) };

const FieldDesc kEventListFields[] = {
	HTTPD_LIST_OF("items", &kEventSchema, "the events of the window, in start time order"),
};

const Schema kEventListSchema = { "event-list", HTTPD_FIELDS(kEventListFields) };

const FieldDesc kSearchFields[] = {
	HTTPD_LIST_OF("items", &kEventSchema, "the events that matched, in the order the guide holds them"),
	HTTPD_MEMBER("truncated", FieldType::Bool,
		"whether the guide held more than the limit allowed, which a list exactly as long as the limit cannot say on its own"),
};

const Schema kSearchSchema = { "event-search", HTTPD_FIELDS(kSearchFields) };

const FieldDesc kScheduleFields[] = {
	HTTPD_MEMBER("channel_id", FieldType::ChannelId,
		"the channel this row is for, hexadecimal, as it was named in the request"),
	HTTPD_LIST_OF("events", &kEventSchema, "what it is showing inside the window, in start time order"),
	HTTPD_MEMBER("truncated", FieldType::Bool,
		"whether the guide held more of this channel inside the window than one row carries, which a row exactly as long as the ceiling cannot say on its own"),
};

const Schema kScheduleSchema = { "channel-schedule", HTTPD_FIELDS(kScheduleFields) };

const FieldDesc kGridFields[] = {
	HTTPD_LIST_OF("items", &kScheduleSchema,
		"one row per channel, in the order the channels were named"),
	/* Absent on the last page, which is the one thing that says a walk is over. The number
	   of rows does not say it: a page ends at the limit or at the ceiling on events,
	   whichever comes first, so a client that stops on a short page stops in the middle. */
	HTTPD_MEMBER_OPTIONAL("next_cursor", FieldType::ChannelId,
		"hand this back as cursor for the page after this one, and absent when this page is the last, which is the only thing that says there is no more"),
};

const Schema kGridSchema = { "epg-grid", HTTPD_FIELDS(kGridFields) };

const FieldDesc kEventDetailFields[] = {
	HTTPD_MEMBER("id", FieldType::ChannelId,
		"what the guide names this event by, hexadecimal, carrying the channel in its upper bits"),
	HTTPD_MEMBER("channel_id", FieldType::ChannelId,
		"the channel the event belongs to, hexadecimal, named the way the channel routes name one, and the half the guide keeps where this box carries no such channel"),
	HTTPD_MEMBER("title", FieldType::String, "what the event is called"),
	HTTPD_MEMBER("description", FieldType::String,
		"the short text beside the name, empty where there is none"),
	HTTPD_MEMBER("long_description", FieldType::String,
		"the long text under the short one, empty where there is none, and never a copy of the short one"),
	HTTPD_MEMBER("start", FieldType::Time, "when the event begins, seconds since the epoch"),
	HTTPD_MEMBER("duration", FieldType::UInt, "how long it runs, in seconds"),
	HTTPD_MEMBER("rating", FieldType::UInt,
		"the least age in years the event is broadcast for, and nought where it carries no rating, which is not the same as an event for every age"),
	HTTPD_MEMBER("genre", FieldType::UInt,
		"what the event is about, as the broadcast's own classification: the broad class in the upper four bits and the narrower one in the lower four, and nought for an event that carries none"),
};

const Schema kEventDetailSchema = { "event-detail", HTTPD_FIELDS(kEventDetailFields) };

void appendEvent(Json &j, const coreapi::EventInfo &e)
{
	char id[24];

	j.beginObject();
	j.key("id");
	j.value(hexId(e.event_id, id));
	j.key("channel_id");
	j.value(hexId(e.channel_id, id));
	j.key("title");
	j.value(e.title);
	j.key("description");
	j.value(e.description);
	j.key("start");
	j.value((long long) e.start);
	j.key("duration");
	j.value((unsigned long) e.duration);
	j.endObject();
}

// The key is the caller's, because the same array of events is the whole of one
// answer and one member of a row of another, and a writer that named it would
// name it wrongly in one of the two.
void appendEvents(Json &j, const char *key, const coreapi::EventList &events)
{
	j.key(key);
	j.beginArray();
	for (size_t i = 0; i < events.size(); ++i)
		appendEvent(j, events[i]);
	j.endArray();
}

Response listEvents(const Request &r)
{
	/* Both ends of the window are required and are declared so, rather than one being
	   filled in here from the clock. A guide read with no bound is the whole schedule of a
	   channel, and a caller that meant a day would get it without having said anything
	   wrong. */
	coreapi::Result<coreapi::EventList> got =
		coreapi::epg::forChannel(r.asChannelId("channel"), r.asTime("from"), r.asTime("to"));
	if (!got.ok())
		return problemFor(got.error());

	const coreapi::EventList events = std::move(got).value();

	Response out = okJson();
	Json j(out.body, 32 + 200 * events.size());
	j.beginObject();
	appendEvents(j, "items", events);
	j.endObject();
	return out;
}

Response currentEvent(const Request &r)
{
	coreapi::Result<coreapi::EventInfo> got = coreapi::epg::current(r.asChannelId("channel"));
	if (!got.ok())
		return problemFor(got.error());

	Response out = okJson();
	Json j(out.body, 256);
	appendEvent(j, got.value());
	return out;
}

Response searchEvents(const Request &r)
{
	/* No cursor here, unlike the channel list. The guide is rebuilt under a reader as the
	   box receives it, so a position in it does not survive to a second call however it is
	   written, and a caller that wants the rest narrows the window. What the answer carries
	   instead is whether it was cut, which a list exactly as long as the limit cannot say. */
	const size_t limit = r.has("limit") ? (size_t) r.asUInt("limit") : (size_t) kDefaultSearchLimit;

	coreapi::Result<coreapi::SearchResult> got =
		coreapi::epg::search(r.asString("q"), r.asTime("from"), r.asTime("to"), limit);
	if (!got.ok())
		return problemFor(got.error());

	const coreapi::SearchResult found = std::move(got).value();

	Response out = okJson();
	Json j(out.body, 48 + 200 * found.events.size());
	j.beginObject();
	appendEvents(j, "items", found.events);
	j.key("truncated");
	j.value(found.truncated);
	j.endObject();
	return out;
}

/* The most channels one request may name by hand.

   Counted in channels and not in bytes, for the reason the bouquet write beside this
   counts its own list that way: a caller told its value was too long would have to work
   back from bytes to channels. Above the widest page, so naming more than one page holds
   is a thing a caller may do and then walk; far below what a bouquet holds, a selection
   written out by hand being a selection somebody made. */
const size_t kMaxNamedChannels = 200;

/* The widest bouquet number a row may bound. Written here as well as beside the channel
   listing rather than shared, the two being two tables that each state their own bounds:
   a row whose bound came from another module changes when that module's does. */
const long kMaxBouquetId = 2147483647L;

/* The identifiers out of the one value a caller named its channels in.

   One value holding a list, and not the same name written several times: a name
   written twice is refused where values are read, so a list has to be written as one
   value to arrive at all.

   Each is spelt the way a channel is named everywhere else here. An element spelling
   nothing names no channel, and sixteen digits are the whole of one: read without
   those two an empty element would become the channel numbered nought.

   The order is the caller's and is kept, because it is the order of the rows it is
   about to draw. Nothing is dropped here, an identifier named twice included: what
   that costs is the walk's business and is settled where the walk is. */
bool readNamedChannels(const std::string &text, coreapi::ChannelIdList &out)
{
	out.clear();
	size_t at = 0;
	for (;;)
	{
		size_t comma = text.find(',', at);
		if (comma == std::string::npos)
			comma = text.size();

		const size_t len = comma - at;
		if (len == 0 || len > 16)
			return false;

		uint64_t id = 0;
		for (size_t i = at; i < comma; ++i)
		{
			const char c = text[i];
			int digit = -1;
			if (c >= '0' && c <= '9')
				digit = c - '0';
			else if (c >= 'a' && c <= 'f')
				digit = 10 + (c - 'a');
			else if (c >= 'A' && c <= 'F')
				digit = 10 + (c - 'A');
			if (digit < 0)
				return false;
			id = (id << 4) | (uint64_t) digit;
		}
		out.push_back((coreapi::ChannelId) id);

		if (comma >= text.size())
			return true;
		at = comma + 1;
	}
}

/* Which channels the grid is for, out of the two ways a request may say it.

   Two ways and not one, because neither answers for the other. A bouquet is what a
   caller drawing the box's own list means, and asking it to fetch that list first
   only to hand it straight back would be a second request for something this end
   already holds. A list written out is what a caller with a selection of its own
   means, and there is no bouquet that stands for the channels somebody picked.

   Exactly one of them. Neither is a request for every channel the box carries, which
   is the unbounded answer this route exists to not give; both is a request with two
   answers, and preferring one would leave the caller never learning the other half
   was dropped. */
bool namedChannels(const Request &r, coreapi::ChannelIdList &ids, Response &refusal)
{
	const bool by_bouquet = r.has("bouquet");
	const bool by_hand = r.has("channels");

	if (by_bouquet && by_hand)
	{
		refusal = problemResponse(StatusBadRequest, coreapi::ErrorCode::ConflictingParameters,
					  "a grid is for a bouquet or for the channels named, and not for both");
		return false;
	}
	if (!by_bouquet && !by_hand)
	{
		refusal = problemResponse(StatusBadRequest, coreapi::ErrorCode::MissingParameter,
					  "say which channels the grid is for, by bouquet or by naming them");
		return false;
	}

	if (by_hand)
	{
		if (!readNamedChannels(r.asString("channels"), ids))
		{
			refusal = problemResponse(StatusBadRequest, coreapi::ErrorCode::BadInt,
						  "the channels are not hexadecimal identifiers separated by commas");
			return false;
		}
		if (ids.size() > kMaxNamedChannels)
		{
			// The number out of the constant rather than written again here, a
			// sentence carrying a ceiling somebody kept in step by hand being
			// one that is right until the ceiling moves.
			char most[32];
			std::snprintf(most, sizeof(most), "%lu", (unsigned long) kMaxNamedChannels);
			refusal = problemResponse(StatusBadRequest, coreapi::ErrorCode::TooManyChannels,
						  std::string("one request names at most ") + most + " channels");
			return false;
		}
		return true;
	}

	/* Both halves of the bouquet, television and radio together, and in the order the
	   bouquet holds them. The listing route narrows a bouquet to one of its halves and
	   defaults to television; a grid that did the same would leave the half it did not give
	   missing with nothing saying so. */
	coreapi::Result<coreapi::ChannelList> got =
		coreapi::channels::bouquetChannels((uint32_t) r.asUInt("bouquet"));
	if (!got.ok())
	{
		refusal = problemFor(got.error());
		return false;
	}

	const coreapi::ChannelList members = std::move(got).value();
	ids.reserve(members.size());
	for (size_t i = 0; i < members.size(); ++i)
		ids.push_back(members[i].id);
	return true;
}

Response gridEvents(const Request &r)
{
	coreapi::ChannelIdList ids;
	Response refusal;
	if (!namedChannels(r, ids, refusal))
		return refusal;

	const size_t limit = r.has("limit")
		? (size_t) r.asUInt("limit")
		: coreapi::epg::DEFAULT_GRID_CHANNELS;

	coreapi::Result<coreapi::epg::Grid> got =
		coreapi::epg::grid(ids, r.asTime("from"), r.asTime("to"),
				   r.has("cursor"), r.asChannelId("cursor"), limit);
	if (!got.ok())
		return problemFor(got.error());

	const coreapi::epg::Grid page = std::move(got).value();

	size_t events = 0;
	for (size_t i = 0; i < page.channels.size(); ++i)
		events += page.channels[i].events.size();

	Response out = okJson();
	Json j(out.body, 64 + 64 * page.channels.size() + 200 * events);
	j.beginObject();
	j.key("items");
	j.beginArray();
	for (size_t i = 0; i < page.channels.size(); ++i)
	{
		const coreapi::epg::ChannelSchedule &row = page.channels[i];
		char id[24];

		j.beginObject();
		j.key("channel_id");
		j.value(hexId(row.id, id));
		appendEvents(j, "events", row.events);
		j.key("truncated");
		j.value(row.truncated);
		j.endObject();
	}
	j.endArray();

	/* Left out on the last page rather than answered empty: an empty cursor is an absent
	   one and would be answered with the first page again. */
	if (page.more)
	{
		char cursor[24];
		j.key("next_cursor");
		j.value(hexId(page.next_after, cursor));
	}
	j.endObject();
	return out;
}

Response eventDetail(const Request &r)
{
	coreapi::Result<coreapi::EventDetail> got =
		coreapi::epg::event(r.asChannelId("id"), r.asTime("start"));
	if (!got.ok())
		return problemFor(got.error());

	const coreapi::EventDetail e = std::move(got).value();
	char id[24];

	Response out = okJson();
	Json j(out.body, 512 + e.long_description.size());
	j.beginObject();
	j.key("id");
	j.value(hexId(e.event_id, id));
	j.key("channel_id");
	j.value(hexId(e.channel_id, id));
	j.key("title");
	j.value(e.title);
	j.key("description");
	j.value(e.description);
	j.key("long_description");
	j.value(e.long_description);
	j.key("start");
	j.value((long long) e.start);
	j.key("duration");
	j.value((unsigned long) e.duration);
	j.key("rating");
	j.value((unsigned long) e.rating);
	j.key("genre");
	j.value((unsigned long) e.genre);
	j.endObject();
	return out;
}

coreapi::epg::Kind xmltvKindFor(const Request &r)
{
	if (!r.has("mode"))
		return coreapi::epg::Kind::All;
	const std::string &m = r.asString("mode");
	if (m == "tv")
		return coreapi::epg::Kind::Tv;
	if (m == "radio")
		return coreapi::epg::Kind::Radio;
	return coreapi::epg::Kind::All;
}

/* The file coreapi wrote is opened here and removed under its name in the same breath,
   whether or not the open worked: the name was this request's alone, and a request that
   fails between here and the transport sending the answer must not be the reason one is
   left in /tmp. What is left is the descriptor answerFromDescriptor takes over. */
Response xmltvResponse(const std::string &path)
{
	const int fd = ::open(path.c_str(), O_RDONLY | O_CLOEXEC);
	::unlink(path.c_str());

	Response out;
	out.code = StatusOk;
	if (!answerFromDescriptor(out, fd))
		return problemResponse(StatusInternalServerError, coreapi::ErrorCode::ChangeRefused,
				       "the guide could not be read back for sending");
	out.content_type = "application/xml";
	return out;
}

Response xmltvDocument(const Request &r)
{
	coreapi::Result<std::string> got = coreapi::epg::xmltv(xmltvKindFor(r));
	if (!got.ok())
		return problemFor(got.error());
	return xmltvResponse(got.value());
}

const Param kXmltvParams[] = {
	HTTPD_QUERY_FROM_SET("mode", "which channels to include, television and radio both when it is left out",
		"tv,radio,all"),
};

const Param kWindowParams[] = {
	HTTPD_QUERY_REQUIRED("channel", ParamType::ChannelId, "the channel, hexadecimal"),
	HTTPD_QUERY_REQUIRED("from", ParamType::Time, "when the window opens, seconds since the epoch"),
	HTTPD_QUERY_REQUIRED("to", ParamType::Time, "when it closes, which has to be after it opens"),
};

const Param kCurrentParams[] = {
	HTTPD_QUERY_REQUIRED("channel", ParamType::ChannelId, "the channel, hexadecimal"),
};

/* Both ends of the window are required here for the reason they are required of the one
   channel read: a guide read with no bound is a whole schedule, and over a bouquet it is
   every whole schedule the box holds.

   Neither of the two ways of naming the channels is required on its own, because either
   will do and neither will do beside the other. What holds that is the handler: a table
   declares one value at a time and says nothing about two of them together. */
const Param kGridParams[] = {
	HTTPD_QUERY_IN("bouquet", ParamType::UInt, "the bouquet whose channels the grid is for, counted from one, both of its halves", 1,
		kMaxBouquetId),
	HTTPD_QUERY("channels", ParamType::String,
		"the channels the grid is for, hexadecimal, separated by commas, in the order the rows are wanted"),
	HTTPD_QUERY_REQUIRED("from", ParamType::Time, "when the window opens, seconds since the epoch"),
	HTTPD_QUERY_REQUIRED("to", ParamType::Time, "when it closes, which has to be after it opens"),
	HTTPD_QUERY("cursor", ParamType::ChannelId,
		"the channel of the last row of the page before this one, as that page answered it"),
	/* The ceiling is the layer's own and is written from it rather than typed
	   here, so a page that holds more one day does not need this row edited to
	   allow it. */
	HTTPD_QUERY_IN("limit", ParamType::UInt, "how many channels at most, twenty when it is left out, and fewer than this are answered where the events of the page reach their own ceiling first", 1,
		(long) coreapi::epg::MAX_GRID_CHANNELS),
};

/* Both halves of the name, because the guide files one identifier under every
   showing of the event and the moment is which of them is meant. Every listing
   here answers the two beside each other, so a caller always holds both. */
const Param kEventParams[] = {
	HTTPD_QUERY_REQUIRED("id", ParamType::ChannelId, "the event, hexadecimal, as a listing names it"),
	HTTPD_QUERY_REQUIRED("start", ParamType::Time,
		"when that showing begins, seconds since the epoch, as the listing says beside the identifier"),
};

const Param kSearchParams[] = {
	/* No floor on the length here. A row bounding a string below is refused where the
	   tables are checked, so the two character floor a search has stays where it is enforced,
	   one layer down, and a query below it comes back saying which rule it broke. */
	HTTPD_QUERY_REQUIRED("q", ParamType::String,
		"what to look for in the name and both texts of an event"),
	HTTPD_QUERY_REQUIRED("from", ParamType::Time, "when the window opens, seconds since the epoch"),
	HTTPD_QUERY_REQUIRED("to", ParamType::Time, "when it closes, which has to be after it opens"),
	/* The ceiling is the layer's own and is written from it rather than typed
	   here, so a search that returns more one day does not need this row
	   edited to allow it. */
	HTTPD_QUERY_IN("limit", ParamType::UInt, "how many at most, one hundred when it is left out", 1,
		(long) coreapi::epg::MAX_SEARCH_RESULTS),
};

const Endpoint kEpgEndpoints[] = {
	{ Method::Get, "/api/v1/epg", AuthLevel::Read,
	  "what a channel is showing over a window of time",
	  HTTPD_PARAMS(kWindowParams), &kEventListSchema, &listEvents, false },
	{ Method::Get, "/api/v1/epg/current", AuthLevel::Read,
	  "what a channel is showing now",
	  HTTPD_PARAMS(kCurrentParams), &kEventSchema, &currentEvent, false },
	{ Method::Get, "/api/v1/epg/search", AuthLevel::Read,
	  "the events of any channel whose text matches, inside a window",
	  HTTPD_PARAMS(kSearchParams), &kSearchSchema, &searchEvents, false },
	{ Method::Get, "/api/v1/epg/xmltv", AuthLevel::Read,
	  "every user bouquet's schedule as one XMLTV document",
	  HTTPD_PARAMS(kXmltvParams), NULL, &xmltvDocument, false },
	{ Method::Get, "/api/v1/epg/grid", AuthLevel::Read,
	  "what several channels are showing over a window of time, a page of channels at a time, where a page holding fewer rows than were asked for is not on that account the last",
	  HTTPD_PARAMS(kGridParams), &kGridSchema, &gridEvents, false },
	{ Method::Get, "/api/v1/epg/event", AuthLevel::Read,
	  "one event with the age it is broadcast for, what it is about, and the long text a listing does not carry",
	  HTTPD_PARAMS(kEventParams), &kEventDetailSchema, &eventDetail, false },
};

} // namespace

extern const RouteTable epgTable = {
	HTTPD_TABLE("epg", kEpgEndpoints)
};

} // namespace httpd
