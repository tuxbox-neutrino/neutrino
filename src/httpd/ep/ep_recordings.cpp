/*
 * ep_recordings.cpp - routes for recordings
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

#include "coreapi/base/errors.h"
#include "coreapi/recordings.h"
#include "coreapi/base/result.h"
#include "coreapi/base/types.h"

#include <cstddef>
#include <string>
#include <utility>

#include <stdint.h>

namespace httpd
{

namespace
{

/* What the box is writing now, which is not what the timer routes beside this answer.
   A timer is a row in a daemon's file saying what the box will do; a recording is a
   file growing on a disc and a tuner being held for it. The two share a set of
   numbers, because the box names a running recording by the timer it carries, and
   they are not the same list: a timer that has not fired is not a recording, and the
   shift the box keeps of what it is showing is a recording nobody made a timer for.

   Every route here is a list or acts on one member of it. There is no route that
   starts an ordinary recording, because making a timer that fires now is what starts
   one. */

const char *startedBy(bool from_timer)
{
	// Two words and not a flag named after one of them, so that a third way of
	// starting a recording can be named rather than folded into "not a timer".
	return from_timer ? "timer" : "immediate";
}

const FieldDesc kRecordingFields[] = {
	HTTPD_MEMBER("id", FieldType::UInt,
		"what names this recording, which is the timer it carries and is what the route that ends it takes"),
	HTTPD_MEMBER("channel_id", FieldType::ChannelId,
		"the channel being recorded, hexadecimal; what it is called is the channel list's answer"),
	HTTPD_MEMBER("title", FieldType::String,
		"what the guide called the programme when the recording began, empty for one begun with nothing to read"),
	HTTPD_MEMBER("start", FieldType::Time,
		"when the box began writing, seconds since the epoch, which is not when a timer was due"),
	HTTPD_MEMBER("path", FieldType::String, "the whole name of the file being written"),
	/* The one member absent from some answers, and absent for the one reason
	   the shape allows: nought is a real size for a recording that has just
	   begun, so a file that could not be measured at all has to be told apart
	   from one that is empty. */
	HTTPD_MEMBER_OPTIONAL("size", FieldType::UInt,
		"what that file weighed when it was looked at, absent for a file that could not be measured"),
	HTTPD_MEMBER("timeshift", FieldType::Bool,
		"whether this is the shift the box keeps of what it is showing rather than a recording somebody keeps"),
	/* Not a stated set, although two words are all this writes today. A third way of
	   starting a recording is meant to be nameable, and a set declared here would
	   make a client generated from this document turn down the first answer that used
	   it. */
	HTTPD_MEMBER("started_by", FieldType::String,
		"timer for one a timer the daemon already held started, immediate for one somebody asked for at the time"),
};

const Schema kRecordingSchema = { "recording", HTTPD_FIELDS(kRecordingFields) };

const FieldDesc kRecordingListFields[] = {
	HTTPD_LIST_OF("items", &kRecordingSchema,
		"every recording the box is taking, and empty for a box recording nothing"),
};

const Schema kRecordingListSchema = { "recording-list", HTTPD_FIELDS(kRecordingListFields) };

Response listRecordings(const Request &)
{
	/* Not paged, for the reason the timer list is not: a box takes a handful
	   of these at once and never more than its own ceiling, and a cursor over
	   a list that is rebuilt whenever one starts or stops would name a place
	   that had moved under the caller. */
	coreapi::Result<coreapi::RecordingList> got = coreapi::recordings::list();
	if (!got.ok())
		return problemFor(got.error());

	const coreapi::RecordingList all = std::move(got).value();

	Response out = okJson();
	Json j(out.body, 32 + 240 * all.size());
	j.beginObject();
	j.key("items");
	j.beginArray();
	for (size_t i = 0; i < all.size(); ++i)
	{
		const coreapi::RecordingInfo &r = all[i];
		char id[24];

		j.beginObject();
		j.key("id");
		j.value((unsigned long) r.id);
		j.key("channel_id");
		j.value(hexId(r.channel_id, id));
		j.key("title");
		j.value(r.title);
		j.key("start");
		j.value((long long) r.start);
		j.key("path");
		j.value(r.path);
		if (r.size_known)
		{
			j.key("size");
			j.value((unsigned long long) r.size);
		}
		j.key("timeshift");
		j.value(r.timeshift);
		j.key("started_by");
		j.value(std::string(startedBy(r.from_timer)));
		j.endObject();
	}
	j.endArray();
	j.endObject();
	return out;
}

Response stopRecording(const Request &r)
{
	/* NotFound for a number nothing is being recorded under, which the layer
	   below asks before the request goes out, so ending one recording twice is
	   answered as ending one that is not there and not as a second success. */
	coreapi::Result<void> done = coreapi::recordings::stop((uint32_t) r.asUInt("id"));
	if (!done.ok())
		return problemFor(done.error());
	/* Accepted and not done. What stops the writing is the loop that does it,
	   reached by way of the timer daemon, and neither of them answers back, so
	   an answer saying the recording has ended would be this server stating
	   something it has no way of having learnt. */
	return accepted();
}

Response startTimeshift(const Request &)
{
	coreapi::Result<void> done = coreapi::recordings::startTimeshift();
	if (!done.ok())
		return problemFor(done.error());
	return accepted();
}

Response stopTimeshift(const Request &)
{
	coreapi::Result<void> done = coreapi::recordings::stopTimeshift();
	if (!done.ok())
		return problemFor(done.error());
	return accepted();
}

/* The widest number the timer daemon hands out, read the same way the timer routes
   read it: the daemon keeps its counter as an int and counts up from one. A bound is
   read by the same check on the box and on the machine the suite runs on, where the
   cast alone is not: the accessor is an unsigned long and the value a thirty two bit
   number.

   The floor is one and not nought: nought is what a recording carries while the daemon
   has not given it a number, and there is nothing to ask the daemon to stop under a
   number it never gave. */
const long kMaxRecordingId = 2147483647L;

const Param kOneParams[] = {
	HTTPD_SEGMENT_IN("id", ParamType::UInt, "the recording, as the recording list names it", 1,
		kMaxRecordingId),
};

const Endpoint kRecordingEndpoints[] = {
	{ Method::Get, "/api/v1/recordings", AuthLevel::Read,
	  "every recording the box is taking at this moment",
	  NULL, 0, &kRecordingListSchema, &listRecordings, false },
	/* Written out and therefore answered ahead of the route below it, which
	   binds anything in that position: which of two matching routes answers is
	   settled by how many of its segments are written out. */
	{ Method::Post, "/api/v1/recordings/timeshift", AuthLevel::Write,
	  "asks the box to begin shifting the channel it is showing",
	  NULL, 0, NULL, &startTimeshift, false },
	{ Method::Delete, "/api/v1/recordings/timeshift", AuthLevel::Write,
	  "asks the box to end the shift it is keeping",
	  NULL, 0, NULL, &stopTimeshift, false },
	{ Method::Delete, "/api/v1/recordings/{id}", AuthLevel::Write,
	  "asks the box to end one recording",
	  HTTPD_PARAMS(kOneParams), NULL, &stopRecording, false },
};

} // namespace

extern const RouteTable recordingsTable = {
	HTTPD_TABLE("recordings", kRecordingEndpoints)
};

} // namespace httpd
