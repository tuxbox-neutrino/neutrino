/*
 * ep_timers.cpp - routes for timers
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
#include "coreapi/base/result.h"
#include "coreapi/timers.h"
#include "coreapi/base/types.h"

#include <cstddef>
#include <cstdio>
#include <string>
#include <utility>
#include <vector>

#include <stdint.h>

namespace httpd
{

namespace
{

/* The kinds a caller may name, written as words rather than as the numbers the daemon
   files them under. Two of those numbers are not what they look like: the slot
   between standby and record is kept for a kind the daemon no longer has, and the
   label a timer read off another box wears is in the same field without being a kind
   this box makes. A caller naming a number would be naming a file format.

   Remotebox is not offered at all, for the reason the layer below refuses it.

   Immediate record is offered and is not stored under that number: the layer below
   builds it as a recording whose start has come, that being the only shape the daemon
   runs, so a read of the timer afterwards answers record. Kept as a name of its own
   because a caller that means now should not have to know what the daemon's clock is
   doing to say so. */
struct TimerKind
{
	const char        *name;
	coreapi::TimerType type;
};

const TimerKind kKinds[] = {
	{ "shutdown",         coreapi::TimerType::Shutdown },
	{ "zapto",            coreapi::TimerType::Zapto },
	{ "standby",          coreapi::TimerType::Standby },
	{ "record",           coreapi::TimerType::Record },
	{ "remind",           coreapi::TimerType::Remind },
	{ "sleeptimer",       coreapi::TimerType::Sleeptimer },
	{ "exec-plugin",      coreapi::TimerType::ExecPlugin },
	{ "immediate-record", coreapi::TimerType::ImmediateRecord },
};

const size_t kKindCount = sizeof(kKinds) / sizeof(kKinds[0]);

// The same list as one string, so the row that declares the parameter and the
// two functions below cannot come to name different sets.
const char kKindValues[] =
	"shutdown,zapto,standby,record,remind,sleeptimer,exec-plugin,immediate-record";

/* The number a timer read off the box carries, as the word for it, and the number
   itself for a kind this build has no word for. A timer file written by another image
   can name a kind nothing here knows, and the read of it deliberately keeps such a
   timer rather than dropping it. Written as a decimal string under the same member,
   which is a value no word here collides with. */
std::string kindName(int type)
{
	for (size_t i = 0; i < kKindCount; ++i)
	{
		if ((int) kKinds[i].type == type)
			return kKinds[i].name;
	}
	char buf[32];
	std::snprintf(buf, sizeof(buf), "%d", type);
	return std::string(buf);
}

bool kindFromName(const std::string &name, int &out)
{
	for (size_t i = 0; i < kKindCount; ++i)
	{
		if (name != kKinds[i].name)
			continue;
		out = (int) kKinds[i].type;
		return true;
	}
	return false;
}

const FieldDesc kTimerFields[] = {
	HTTPD_MEMBER("id", FieldType::UInt, "what the daemon names this timer by, which is never nought"),
	HTTPD_MEMBER("kind", FieldType::String,
		"what the timer does, and the number itself for a kind written by an image this one has no word for"),
	HTTPD_MEMBER("channel_id", FieldType::ChannelId,
		"the channel it acts on, hexadecimal, and nought for a kind that acts on none"),
	HTTPD_MEMBER("start", FieldType::Time, "when it fires, seconds since the epoch"),
	HTTPD_MEMBER("stop", FieldType::Time,
		"when it stops, and nought for a timer that has nothing to stop"),
	HTTPD_MEMBER("title", FieldType::String,
		"the programme of a recording or a zap, the words of a reminder, or the plugin of an exec timer"),
	HTTPD_MEMBER("repeat", FieldType::Int, "how it repeats, in the daemon's own numbering"),
	HTTPD_MEMBER("repeat_count", FieldType::UInt,
		"how many times it still runs, nought for without end"),
	HTTPD_MEMBER("state", FieldType::Int,
		"what the daemon is doing with it, in the daemon's own numbering"),
	HTTPD_MEMBER("announce", FieldType::Time,
		"when the box says it is coming, which the daemon keeps separately"),
	HTTPD_MEMBER("epg_id", FieldType::ChannelId,
		"the guide entry it was made from, hexadecimal, nought for none"),
	HTTPD_MEMBER("epg_start", FieldType::Time, "when that entry begins"),
	HTTPD_MEMBER("standby_on", FieldType::Bool, "whether the box goes to standby with it"),
	HTTPD_MEMBER("recording_dir", FieldType::String,
		"where a recording is written, empty for wherever the box records"),
};

/* The two flags the creation takes are not among them. The daemon's answer for a
   timer carries neither, so a member for either would read false on every timer
   whatever was asked for, which is a member that states a thing nobody knows. */
const Schema kTimerSchema = { "timer", HTTPD_FIELDS(kTimerFields) };

const FieldDesc kTimerListFields[] = {
	HTTPD_LIST_OF("items", &kTimerSchema, "every timer the daemon holds, in the order it keeps them"),
};

const Schema kTimerListSchema = { "timer-list", HTTPD_FIELDS(kTimerListFields) };

const FieldDesc kCreatedFields[] = {
	HTTPD_MEMBER("id", FieldType::String,
		"the timer the daemon made, as the text the route that addresses it takes"),
};

const Schema kCreatedSchema = { "timer-created", HTTPD_FIELDS(kCreatedFields) };

void appendTimer(Json &j, const coreapi::TimerInfo &t)
{
	char id[24];

	j.beginObject();
	j.key("id");
	j.value((unsigned long) t.id);
	j.key("kind");
	j.value(kindName(t.type));
	j.key("channel_id");
	j.value(hexId(t.channel_id, id));
	j.key("start");
	j.value((long long) t.start);
	j.key("stop");
	j.value((long long) t.stop);
	j.key("title");
	j.value(t.title);
	j.key("repeat");
	j.value(t.repeat);
	j.key("repeat_count");
	j.value((unsigned long) t.repeat_count);
	j.key("state");
	j.value(t.state);
	j.key("announce");
	j.value((long long) t.announce);
	j.key("epg_id");
	j.value(hexId(t.epg_id, id));
	j.key("epg_start");
	j.value((long long) t.epg_start);
	j.key("standby_on");
	j.value(t.standby_on);
	j.key("recording_dir");
	j.value(t.recording_dir);
	j.endObject();
}

Response listTimers(const Request &)
{
	/* Not paged, unlike the channel list. A box holds tens of these and not
	   thousands, and a cursor over a list the daemon reorders as timers fire
	   would name a place that had moved under the caller. */
	coreapi::Result<coreapi::TimerList> got = coreapi::timers::list();
	if (!got.ok())
		return problemFor(got.error());

	const coreapi::TimerList all = std::move(got).value();

	Response out = okJson();
	Json j(out.body, 32 + 240 * all.size());
	j.beginObject();
	j.key("items");
	j.beginArray();
	for (size_t i = 0; i < all.size(); ++i)
		appendTimer(j, all[i]);
	j.endArray();
	j.endObject();
	return out;
}

/* The one timer of that id out of the list the daemon holds, because the layer below
   offers no read of a single one and a change has to be built on what is there rather
   than on what a caller sent. found says whether it was there, and the refusal carries
   the answer for everything else: a list that could not be read at all is not a timer
   that is not there. */
bool oneTimer(uint32_t id, coreapi::TimerInfo &out, bool &found, Response &refusal)
{
	found = false;
	coreapi::Result<coreapi::TimerList> got = coreapi::timers::list();
	if (!got.ok())
	{
		refusal = problemFor(got.error());
		return false;
	}

	const coreapi::TimerList all = std::move(got).value();
	for (size_t i = 0; i < all.size(); ++i)
	{
		if (all[i].id != id)
			continue;
		out = all[i];
		found = true;
		return true;
	}
	return true;
}

Response notThere()
{
	return problemResponse(StatusNotFound, coreapi::ErrorCode::NoSuchTimer,
	                       "the daemon holds no timer of that id");
}

Response createTimer(const Request &r)
{
	coreapi::TimerInfo t;
	// Held to the set above by the row that declares it, so a name that is not
	// one of them never reaches here and this cannot fail.
	if (!kindFromName(r.asString("kind"), t.type))
		return problemResponse(StatusBadRequest, coreapi::ErrorCode::NoSuchTimerType,
		                       "the box does not make timers of that kind");

	t.channel_id = r.asChannelId("channel_id");
	t.start = r.asTime("start");
	t.stop = r.asTime("stop");
	t.title = r.asString("title");
	t.repeat = (int) r.asInt("repeat");
	t.repeat_count = (uint32_t) r.asUInt("repeat_count");
	t.announce = r.asTime("announce");
	t.epg_id = r.asChannelId("epg_id");
	t.epg_start = r.asTime("epg_start");
	t.standby_on = r.asBool("standby_on");
	t.recording_dir = r.asString("recording_dir");
	/* Both off unless asked for, because a caller that named a start and a stop
	   named the window it wants: either of these makes the daemon file the timer
	   at times other than the ones that were sent, and a later read answers those
	   and not these. The kinds that carry no recording ignore them. */
	t.recording_safety = r.asBool("recording_safety");
	t.auto_adjust = r.asBool("auto_adjust");

	/* Every rule about what a timer may be is the layer below's: a kind it does
	   not build, a recording with no channel, a recording that ends no later
	   than it begins, and a one-off already behind us. A second copy of any of
	   them here would be a rule that can disagree with the one that decides. */
	coreapi::Result<uint32_t> made = coreapi::timers::create(t);
	if (!made.ok())
		return problemFor(made.error());

	const uint32_t id = made.value();

	char text[24];
	std::snprintf(text, sizeof(text), "%lu", (unsigned long) id);

	Response out = createdJson();
	/* Where the timer that was made can be reached, which is the one thing a
	   caller cannot work out from the answer alone without knowing how these
	   paths are spelt. */
	out.headers.push_back(std::make_pair(std::string("Location"),
	                                     std::string("/api/v1/timers/") + text));

	Json j(out.body, 48);
	j.beginObject();
	j.key("id");
	j.value(std::string(text));
	j.endObject();
	return out;
}

Response changeTimer(const Request &r)
{
	const uint32_t id = (uint32_t) r.asUInt("id");

	/* The timer as it is, then what the request names written over it. A change
	   built out of the request alone would clear every field the request left
	   out, which is a replacement and not a correction, and the caller asked
	   for a correction. */
	coreapi::TimerInfo t;
	bool found = false;
	Response refusal;
	if (!oneTimer(id, t, found, refusal))
		return refusal;
	if (!found)
		return notThere();

	if (r.has("start"))
		t.start = r.asTime("start");
	if (r.has("stop"))
		t.stop = r.asTime("stop");
	if (r.has("title"))
		t.title = r.asString("title");
	if (r.has("repeat"))
		t.repeat = (int) r.asInt("repeat");
	if (r.has("repeat_count"))
		t.repeat_count = (uint32_t) r.asUInt("repeat_count");
	if (r.has("announce"))
		t.announce = r.asTime("announce");
	if (r.has("standby_on"))
		t.standby_on = r.asBool("standby_on");

	/* The kind and the channel are not offered and are not written over. The
	   daemon's protocol carries no way to move either, so a route that took
	   them would be one that answered ok and changed neither. The two flags the
	   creation takes are left out for that same reason: the change command
	   carries times, repeat and a directory and nothing else, and the daemon
	   applies both flags when the timer is built. */
	coreapi::Result<void> done = coreapi::timers::modify(t);
	if (!done.ok())
		return problemFor(done.error());

	/* Read back rather than answered out of what was sent. The daemon keeps
	   what a change is allowed to touch, and a body written from the request
	   would state the parts it did not take as though it had. */
	coreapi::TimerInfo now;
	if (!oneTimer(id, now, found, refusal))
		return refusal;
	if (!found)
		return notThere();

	Response out = okJson();
	Json j(out.body, 256);
	appendTimer(j, now);
	return out;
}

Response removeTimer(const Request &r)
{
	// NotFound for an id the daemon does not hold, which the layer below asks
	// before the removal, so a second removal of one timer is answered as the
	// removal of a timer that is not there and not as a second success.
	coreapi::Result<void> done = coreapi::timers::remove((uint32_t) r.asUInt("id"));
	if (!done.ok())
		return problemFor(done.error());
	return noContent();
}

/* The widest id the daemon has to hand out, which is what the row is bounded by rather
   than what the accessor holds. The daemon keeps its counter as an int and counts up
   from nought, so this is its own ceiling and not a compromise.

   Bounded and not left to the cast, which is the mistake this replaces. The accessor
   answers an unsigned long, the value below is a thirty two bit id, and those are the
   same width on the box and not on the machine the suite is built on. So a row that
   trusted the cast was checked by nothing wherever the two differ, and an id of two to
   the thirty two plus one arrived as one: it answered as the removal of a timer nobody
   asked about, and removed it. */
const long kMaxTimerId = 2147483647L;

/* And the widest each of the two numbers a timer carries can be, for the same
   reason and read the same way.

   The repeat is the daemon's own numbering: nought to six are the plain
   repeats, and above them the weekday flag carries one bit per day from the
   ninth up, so the widest it reaches is that flag with every day set.

   The count is how many times a repeating timer still runs, and the daemon
   keeps it in a thirty two bit field. This bound is below that field and it is
   the widest a bound can be written where this runs; what it refuses above the
   line is counts nobody can mean, and what it buys is that the value below
   cannot arrive as a different number from the one that was sent. */
const long kMaxRepeat = 0xff00L;
const long kMaxRepeatCount = 2147483647L;

const Param kOneParams[] = {
	HTTPD_SEGMENT_IN("id", ParamType::UInt, "the timer, as the daemon numbers them", 1, kMaxTimerId),
};

const Param kCreateParams[] = {
	HTTPD_BODY_REQUIRED_FROM_SET("kind", "what the timer is to do", kKindValues),
	HTTPD_BODY("channel_id", ParamType::ChannelId,
		"the channel, hexadecimal, for the kinds that act on one"),
	HTTPD_BODY_REQUIRED("start", ParamType::Time, "when it fires, seconds since the epoch"),
	HTTPD_BODY("stop", ParamType::Time, "when it stops, for a recording"),
	HTTPD_BODY_TEXT("title", "the programme, the words or the plugin, by kind", 512),
	HTTPD_BODY_IN("repeat", ParamType::Int, "how it repeats, in the daemon's own numbering", 0,
		kMaxRepeat),
	HTTPD_BODY_IN("repeat_count", ParamType::UInt, "how many times it runs, nought for without end", 0,
		kMaxRepeatCount),
	HTTPD_BODY("announce", ParamType::Time, "when the box says it is coming"),
	HTTPD_BODY("epg_id", ParamType::ChannelId, "the guide entry it was made from, hexadecimal"),
	HTTPD_BODY("epg_start", ParamType::Time, "when that entry begins"),
	HTTPD_BODY("standby_on", ParamType::Bool, "whether the box goes to standby with it"),
	HTTPD_BODY_TEXT("recording_dir", "where a recording is written", 1024),
	HTTPD_BODY("recording_safety", ParamType::Bool,
		"whether the box widens a recording by the margins it is set to, starting it earlier and stopping it later"),
	HTTPD_BODY("auto_adjust", ParamType::Bool,
		"whether the box moves a recording onto the guide's own times for the programme it covers, which it does only where the box is set up for it"),
};

/* The id is in the path and everything else is in the body, and every one of
   the latter is optional: what a correction leaves out is what it does not
   change, and a required member here would make every correction a
   replacement. */
const Param kChangeParams[] = {
	HTTPD_SEGMENT_IN("id", ParamType::UInt, "the timer, as the daemon numbers them", 1, kMaxTimerId),
	HTTPD_BODY("start", ParamType::Time, "when it fires, seconds since the epoch"),
	HTTPD_BODY("stop", ParamType::Time, "when it stops"),
	HTTPD_BODY_TEXT("title", "the programme, the words or the plugin, by kind", 512),
	HTTPD_BODY_IN("repeat", ParamType::Int, "how it repeats, in the daemon's own numbering", 0,
		kMaxRepeat),
	HTTPD_BODY_IN("repeat_count", ParamType::UInt, "how many times it runs, nought for without end", 0,
		kMaxRepeatCount),
	HTTPD_BODY("announce", ParamType::Time, "when the box says it is coming"),
	HTTPD_BODY("standby_on", ParamType::Bool, "whether the box goes to standby with it"),
};

const Endpoint kTimerEndpoints[] = {
	{ Method::Get, "/api/v1/timers", AuthLevel::Read,
	  "every timer the box holds",
	  NULL, 0, &kTimerListSchema, &listTimers, false },
	{ Method::Post, "/api/v1/timers", AuthLevel::Write,
	  "makes a timer and answers where it can be reached",
	  HTTPD_PARAMS(kCreateParams), &kCreatedSchema, &createTimer, false },
	{ Method::Patch, "/api/v1/timers/{id}", AuthLevel::Write,
	  "changes what a timer the daemon holds may be changed about",
	  HTTPD_PARAMS(kChangeParams), &kTimerSchema, &changeTimer, false },
	{ Method::Delete, "/api/v1/timers/{id}", AuthLevel::Write,
	  "removes one timer",
	  HTTPD_PARAMS(kOneParams), NULL, &removeTimer, false },
};

} // namespace

extern const RouteTable timersTable = {
	HTTPD_TABLE("timers", kTimerEndpoints)
};

} // namespace httpd
