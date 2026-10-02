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

const char kKindDocs[] =
	"shutdown: switches the box off at `start`\n"
	"zapto: switches to `channel_id` at `start`, waking the box from deep standby for it; needs `channel_id`\n"
	"standby: at `start` puts the box into standby when `standby_on` is `true`, and brings it out of standby when it is `false`\n"
	"record: records `channel_id` from `start` to `stop`, waking the box from deep standby for it; needs `channel_id` and a `stop` after `start`\n"
	"remind: shows `title` on the screen at `start`\n"
	"sleeptimer: at `start` switches the box off or puts it into standby, whichever the box's own shutdown setting chooses\n"
	"exec-plugin: starts the plugin named in `title` at `start`\n"
	"immediate-record: a recording of `channel_id` that begins at once: send the current time as `start` and the end as `stop`; it is filed, and read back, as `record`";

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
	HTTPD_MEMBER("id", FieldType::UInt,
		"the number the timer daemon gave this timer, 1 or more; the `{id}` of `PATCH` and `DELETE /api/v1/timers/{id}`"),
	HTTPD_MEMBER("kind", FieldType::String,
		"what the timer does, one of the kinds `POST /api/v1/timers` takes (`shutdown`, `zapto`, `standby`, `record`, "
		"`remind`, `sleeptimer`, `exec-plugin`, `immediate-record`); a timer made with `immediate-record` reads back as "
		"`record`, and a kind this build has no name for is answered as its decimal type number, e.g. `10`"),
	HTTPD_MEMBER("channel_id", FieldType::ChannelId,
		"the channel it switches to or records, hexadecimal as the channel routes name it, and `0` for a kind that acts on no channel"),
	HTTPD_MEMBER("start", FieldType::Time, "when the timer fires next, Unix time in seconds"),
	HTTPD_MEMBER("stop", FieldType::Time,
		"when a recording ends, Unix time in seconds, and `0` for a timer that has nothing to stop"),
	HTTPD_MEMBER("title", FieldType::String,
		"the programme of a recording or a zap, the words of a reminder, or the plugin of an exec timer"),
	HTTPD_MEMBER("repeat", FieldType::Int,
		"how it repeats, coded as the `repeat` member of `POST /api/v1/timers` (0 once, 1 daily, 2 weekly, "
		"3 every 2 weeks, 4 every 4 weeks, 5 monthly, 256 plus weekday bits); a timer made outside this API may "
		"carry a value that API refuses, such as 6 (by event description)"),
	HTTPD_MEMBER("repeat_count", FieldType::UInt,
		"how many more times a repeating timer runs, and `0` for without end"),
	HTTPD_MEMBER("state", FieldType::Int,
		"where the timer is in its life: 0 scheduled, 1 announced (the `announce` moment has passed), "
		"2 running; 3 finished and 4 terminated last only until the daemon reschedules or drops the timer"),
	HTTPD_MEMBER("announce", FieldType::Time,
		"when the box announces the timer and wakes for it, Unix time in seconds; unless it was set, "
		"60 seconds before `start`, and 180 seconds before it for a recording"),
	HTTPD_MEMBER("epg_id", FieldType::ChannelId,
		"the guide event the timer was made from, hexadecimal as `GET /api/v1/epg` names events, and `0` for none"),
	HTTPD_MEMBER("epg_start", FieldType::Time,
		"when that guide event begins, Unix time in seconds, and `0` for none"),
	HTTPD_MEMBER("standby_on", FieldType::Bool,
		"for a `standby` timer, `true` when it puts the box into standby and `false` when it brings the box out; "
		"`false` for every other kind"),
	HTTPD_MEMBER("recording_dir", FieldType::String,
		"where a recording is written, empty for wherever the box records"),
};

/* The two flags the creation takes are not among them. The daemon's answer for a
   timer carries neither, so a member for either would read false on every timer
   whatever was asked for, which is a member that states a thing nobody knows. */
const Schema kTimerSchema = { "timer", HTTPD_FIELDS(kTimerFields) };

const FieldDesc kTimerListFields[] = {
	HTTPD_LIST_OF("items", &kTimerSchema,
		"every timer the daemon holds, in the order it keeps them, which is not necessarily the order of `start`"),
};

const Schema kTimerListSchema = { "timer-list", HTTPD_FIELDS(kTimerListFields) };

const FieldDesc kCreatedFields[] = {
	HTTPD_MEMBER("id", FieldType::String,
		"the number the daemon gave the new timer, as decimal text, which is the `{id}` of "
		"`PATCH` and `DELETE /api/v1/timers/{id}` and the end of the `Location` header"),
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

// Only a recording keeps an end; the daemon would drop any other's silently.
bool endWithoutRecording(int type, time_t stop)
{
	return stop > 0 &&
	       type != (int) coreapi::TimerType::Record &&
	       type != (int) coreapi::TimerType::ImmediateRecord;
}

Response noEnd()
{
	return problemResponse(StatusBadRequest, coreapi::ErrorCode::NoSuchParameter,
	                       "only a recording has an end");
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

	if (endWithoutRecording(t.type, t.stop))
		return noEnd();

	/* Every other rule about what a timer may be is the layer below's: a kind
	   it does not build, a recording with no channel, a recording that ends no
	   later than it begins, a repeat the box does not make, and a one-off
	   already behind us. A second copy of any of them here would be a rule that
	   can disagree with the one that decides. */
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

	if (r.has("stop") && endWithoutRecording(t.type, r.asTime("stop")))
		return noEnd();

	if (r.has("start"))
	{
		const time_t start = r.asTime("start");
		// Keeps its lead; an announce the request names overrides it below.
		if (t.announce > 0)
			t.announce += start - t.start;
		t.start = start;
	}
	if (r.has("stop"))
		t.stop = r.asTime("stop");
	if (r.has("repeat"))
		t.repeat = (int) r.asInt("repeat");
	if (r.has("repeat_count"))
		t.repeat_count = (uint32_t) r.asUInt("repeat_count");
	if (r.has("announce"))
		t.announce = r.asTime("announce");

	/* The kind, the channel, the title and the standby flag are not offered:
	   the daemon's change command carries only times, repeat and a directory,
	   so taking them would answer ok and change nothing. The two flags the
	   creation takes are left out for the same reason. */
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
   repeats, of which the layer below takes nought to five, and above them the
   weekday flag carries one bit per day from the ninth up, so the widest it
   reaches is that flag with every day set.

   The count is how many times a repeating timer still runs, and the daemon
   keeps it in a thirty two bit field. This bound is below that field and it is
   the widest a bound can be written where this runs; what it refuses above the
   line is counts nobody can mean, and what it buys is that the value below
   cannot arrive as a different number from the one that was sent. */
const long kMaxRepeat = 0xff00L;
const long kMaxRepeatCount = 2147483647L;

const char kRepeatDoc[] =
	"how the timer repeats, as a number:\n\n"
	"| value | repeats |\n"
	"|---|---|\n"
	"| 0 | never, it runs once (the default) |\n"
	"| 1 | daily |\n"
	"| 2 | weekly |\n"
	"| 3 | every 2 weeks |\n"
	"| 4 | every 4 weeks |\n"
	"| 5 | monthly |\n"
	"| 256 + day bits | on the chosen weekdays |\n\n"
	"For weekdays, add 256 and the bit of every day: Monday 512, Tuesday 1024, Wednesday 2048, "
	"Thursday 4096, Friday 8192, Saturday 16384, Sunday 32768. At least 1 day is needed, so "
	"Monday to Friday is 256 + 512 + 1024 + 2048 + 4096 + 8192 = 16128 and every day is 65280. "
	"Any other value is refused with `400 not-a-listed-value`.";

const Param kOneParams[] = {
	HTTPD_SEGMENT_IN("id", ParamType::UInt,
		"the timer, by the decimal `id` `GET /api/v1/timers` answers for it", 1, kMaxTimerId),
};

const Param kCreateParams[] = {
	HTTPD_BODY_REQUIRED_FROM_SET("kind",
		"what the timer does when it fires; the kind decides which other members it reads", kKindValues, kKindDocs),
	HTTPD_BODY("channel_id", ParamType::ChannelId,
		"the channel to switch to or record, hexadecimal as `GET /api/v1/channels` answers it; "
		"required for `zapto`, `record` and `immediate-record`, ignored by the other kinds"),
	HTTPD_BODY_REQUIRED("start", ParamType::Time,
		"when the timer fires, Unix time in seconds; for a timer that runs once it may not lie before the "
		"current minute, while a repeating timer may start in the past and then fires at once"),
	HTTPD_BODY("stop", ParamType::Time,
		"when a recording ends, Unix time in seconds, after `start`; any value but `0` is refused for the "
		"kinds that do not record"),
	HTTPD_BODY_TEXT("title",
		"the programme title stored with a `record` timer, the text a `remind` timer shows, or the name of the "
		"plugin an `exec-plugin` timer starts; ignored by the other kinds, and cut to what the daemon stores", 512),
	HTTPD_BODY_IN("repeat", ParamType::Int, kRepeatDoc, 0, kMaxRepeat),
	HTTPD_BODY_IN("repeat_count", ParamType::UInt,
		"how many times a repeating timer runs, and `0` (the default) for without end", 0,
		kMaxRepeatCount),
	HTTPD_BODY("announce", ParamType::Time,
		"when the box announces the timer and wakes for it, Unix time in seconds; left out, `0` or less, or "
		"later than `start`, it becomes 60 seconds before `start`, or 180 seconds before it for `record`; "
		"an `immediate-record` timer has none"),
	HTTPD_BODY("epg_id", ParamType::ChannelId,
		"the guide event the timer is made from, hexadecimal as `GET /api/v1/epg` answers it, for `record` and "
		"`zapto`; with `auto_adjust` the box follows that event"),
	HTTPD_BODY("epg_start", ParamType::Time,
		"when that guide event begins, Unix time in seconds, sent together with `epg_id`"),
	HTTPD_BODY("standby_on", ParamType::Bool,
		"for a `standby` timer only: `true` puts the box into standby at `start`, `false` (the default) "
		"brings it out of standby"),
	HTTPD_BODY_TEXT("recording_dir",
		"the directory a `record` timer writes to, as a path on the box; empty or left out for the box's "
		"own recording directory", 1024),
	HTTPD_BODY("recording_safety", ParamType::Bool,
		"whether the box widens a recording by the margins it is set to, starting it earlier and stopping it "
		"later; `false` when left out, so the timer keeps exactly the `start` and `stop` sent"),
	HTTPD_BODY("auto_adjust", ParamType::Bool,
		"whether the box moves a recording onto the guide's own times for the programme `epg_id` names, which it "
		"does only where the box is set up for it; `false` when left out"),
};

/* The id is in the path and everything else is in the body, and every one of
   the latter is optional: what a correction leaves out is what it does not
   change, and a required member here would make every correction a
   replacement. */
const Param kChangeParams[] = {
	HTTPD_SEGMENT_IN("id", ParamType::UInt,
		"the timer, by the decimal `id` `GET /api/v1/timers` answers for it", 1, kMaxTimerId),
	HTTPD_BODY("start", ParamType::Time,
		"the new moment the timer fires, Unix time in seconds; a timer that runs once may not be moved before "
		"the current minute, and a recording that runs once keeps its start once it has begun"),
	HTTPD_BODY("stop", ParamType::Time,
		"the new end of a recording, Unix time in seconds, after `start` (and, for a recording that runs once "
		"and has begun, after now); any value but `0` is refused for a timer that does not record"),
	HTTPD_BODY_IN("repeat", ParamType::Int, kRepeatDoc, 0, kMaxRepeat),
	HTTPD_BODY_IN("repeat_count", ParamType::UInt,
		"how many times a repeating timer runs, and `0` for without end", 0,
		kMaxRepeatCount),
	HTTPD_BODY("announce", ParamType::Time,
		"when the box announces the timer, Unix time in seconds; left out, it keeps its distance to `start` "
		"when `start` moves, and `0` or less, or later than `start`, makes it 60 seconds before `start`, or "
		"180 seconds for a recording; a running recording keeps the one it has"),
};

const RouteRefusal kCreateTimerRefusals[] = {
	HTTPD_REFUSES(InvalidArgument, NotAListedValue,
		"the box does not repeat timers that way"),
	HTTPD_REFUSES(InvalidArgument, TimerWithoutChannel,
		"this kind of timer needs a channel"),
	HTTPD_REFUSES(InvalidArgument, RecordingWithoutDuration,
		"a recording has to end after it begins"),
	HTTPD_REFUSES(InvalidArgument, TimerInThePast,
		"a timer that runs once cannot begin before now"),
	HTTPD_REFUSES(Conflict, TimerExists,
		"the box already has a timer like this one"),
};

const RouteRefusal kChangeTimerRefusals[] = {
	HTTPD_REFUSES(InvalidArgument, NotAListedValue,
		"the box does not repeat timers that way"),
	HTTPD_REFUSES(InvalidArgument, RecordingWithoutDuration,
		"a recording has to end after it begins"),
	HTTPD_REFUSES(InvalidArgument, TimerInThePast,
		"a timer that runs once cannot begin before now"),
	HTTPD_REFUSES(NotFound, NoSuchTimer,
		"the daemon holds no timer of that id"),
	HTTPD_REFUSES(Conflict, RecordingRunning,
		"a recording that is running keeps the start it began at"),
};

const RouteRefusal kRemoveTimerRefusals[] = {
	HTTPD_REFUSES(NotFound, NoSuchTimer,
		"no timer with that id"),
	HTTPD_REFUSES(Internal, TimerStillThere,
		"the box still has the timer"),
};

const Endpoint kTimerEndpoints[] = {
	{ Method::Get, "/api/v1/timers", AuthLevel::Read,
	  "every timer the box holds",
	  "Lists every timer the timer daemon holds, with what each one does and when. Not paged: a box holds "
	  "tens of timers, not thousands. A timer leaves the list once it has run for the last time; a repeating "
	  "one is rescheduled and stays.\n\n"
	  "Use the `id` of an entry to change it with `PATCH /api/v1/timers/{id}` or remove it with "
	  "`DELETE /api/v1/timers/{id}`.\n\n"
	  "**Refusals:**\n"
	  "- `timer-list-unavailable` (a 5xx status): the timer daemon could not be read; try again later.\n\n"
	  "**Related:** `POST /api/v1/timers`, the `timer-changed` event on `GET /api/v1/events`.",
	  NULL, 0, &kTimerListSchema, &listTimers, false,
	  Answers200, HTTPD_NO_REFUSALS },
	{ Method::Post, "/api/v1/timers", AuthLevel::Write,
	  "makes a timer and answers where it can be reached",
	  "Makes a timer in the box's timer daemon, the same daemon the box's own timer list shows. `kind` decides "
	  "what it does and which other members matter (see the list under `kind`); `start` is always required. "
	  "The answer is `201` with the new timer's decimal `id` and a `Location` header `/api/v1/timers/{id}`.\n\n"
	  "For a recording of a programme from the guide, send `kind` `record`, the `channel_id`, the event's "
	  "`start` and `start + duration` as `stop`, and optionally `epg_id`, `epg_start` and `title` from "
	  "`GET /api/v1/epg/event`. `recording_safety` and `auto_adjust` are off unless sent, so the timer keeps "
	  "exactly the times given.\n\n"
	  "**Preconditions:** none on the box state: a timer can be made in standby, and a `record` or `zapto` "
	  "timer wakes the box from deep standby before it is due.\n\n"
	  "**Side effects:** the daemon stores the timer in its file and every client sees a `timer-changed` "
	  "event on `GET /api/v1/events`.\n\n"
	  "**Refusals:**\n"
	  "- `400 timer-without-channel`: `zapto`, `record` or `immediate-record` without `channel_id`.\n"
	  "- `400 recording-without-duration`: a recording whose `stop` is not after `start`.\n"
	  "- `400 no-such-parameter`: a `stop` other than `0` for a kind that does not record.\n"
	  "- `400 not-a-listed-value`: a `repeat` value outside the table under `repeat`.\n"
	  "- `400 timer-in-the-past`: a timer that runs once (`repeat` 0) with a `start` before the current "
	  "minute. Send a later `start`.\n"
	  "- `409 timer-exists`: the daemon already holds a timer like this one; find it with `GET /api/v1/timers`.\n"
	  "- `timer-not-created`, `clock-unavailable`: the daemon declined or the box could not read its clock; "
	  "try again.\n\n"
	  "**Related:** `GET /api/v1/timers`, `PATCH /api/v1/timers/{id}`, `DELETE /api/v1/timers/{id}`, "
	  "`GET /api/v1/epg/event`.",
	  HTTPD_PARAMS(kCreateParams), &kCreatedSchema, &createTimer, false,
	  Answers201, HTTPD_REFUSALS_AND_BODY(kCreateTimerRefusals,
		"{\"kind\":\"record\",\"channel_id\":\"283d000103f2\",\"start\":2000000000,"
		"\"stop\":2000003600,\"title\":\"Tagesschau\",\"recording_safety\":true}") },
	{ Method::Patch, "/api/v1/timers/{id}", AuthLevel::Write,
	  "changes what a timer the daemon holds may be changed about",
	  "Changes the times and the repetition of a timer the daemon holds and answers the timer as the daemon "
	  "reads it afterwards. Send only the members to change: everything left out keeps its value. Only `start`, `stop`, `repeat`, `repeat_count` and `announce` can be "
	  "changed; to change the kind, the channel, the title or the standby flag, remove the timer with "
	  "`DELETE /api/v1/timers/{id}` and make a new one with `POST /api/v1/timers`.\n\n"
	  "Moving `start` moves an announcement that was set by the same amount, unless `announce` is sent too.\n\n"
	  "**Preconditions:** a recording that runs once and has already begun can only have its `stop` changed, "
	  "and its new `stop` must lie after now.\n\n"
	  "**Side effects:** the daemon stores the change and every client sees a `timer-changed` event on "
	  "`GET /api/v1/events`.\n\n"
	  "**Refusals:**\n"
	  "- `404 no-such-timer`: the daemon holds no timer with that id; read the ids again from "
	  "`GET /api/v1/timers`.\n"
	  "- `409 recording-running`: a `start` other than the current one for a recording that runs once and "
	  "has begun.\n"
	  "- `400 timer-in-the-past`: a timer that runs once moved before the current minute, or a running "
	  "recording given a `stop` before now.\n"
	  "- `400 recording-without-duration`: a recording whose `stop` would not lie after its `start`.\n"
	  "- `400 no-such-parameter`: a `stop` other than `0` for a timer that does not record.\n"
	  "- `400 not-a-listed-value`: a `repeat` outside the table under `repeat`, including a timer that "
	  "already carries such a value and is changed without sending a new `repeat`.\n"
	  "- `timer-not-changed`: the daemon did not take the change; read the timer again.\n\n"
	  "**Related:** `GET /api/v1/timers`, `DELETE /api/v1/timers/{id}`.",
	  HTTPD_PARAMS(kChangeParams), &kTimerSchema, &changeTimer, false,
	  Answers200, HTTPD_REFUSALS(kChangeTimerRefusals) },
	{ Method::Delete, "/api/v1/timers/{id}", AuthLevel::Write,
	  "removes one timer",
	  "Removes one timer from the timer daemon and answers `204` once a fresh read of the daemon's list no "
	  "longer holds it. Removing the timer of a recording that is running stops that recording, and a "
	  "repeating timer is removed with all its future runs.\n\n"
	  "**Side effects:** the daemon drops the timer from its file and every client sees a `timer-changed` "
	  "event on `GET /api/v1/events`.\n\n"
	  "**Refusals:**\n"
	  "- `404 no-such-timer`: the daemon holds no timer with that id, which is also the answer to a second "
	  "removal of the same timer.\n"
	  "- `500 timer-still-there`: the daemon took the removal and still holds the timer; read "
	  "`GET /api/v1/timers` and try again.\n"
	  "- `timer-not-removed` (a 5xx status): the daemon did not take the removal; try again.\n\n"
	  "**Related:** `GET /api/v1/timers`, `POST /api/v1/timers`.",
	  HTTPD_PARAMS(kOneParams), NULL, &removeTimer, false,
	  Answers204, HTTPD_REFUSALS(kRemoveTimerRefusals) },
};

} // namespace

extern const RouteTable timersTable = {
	HTTPD_TABLE("timers", kTimerEndpoints)
};

} // namespace httpd
