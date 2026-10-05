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

#include "httpd/auth.h"
#include "httpd/endpoint.h"
#include "httpd/http.h"
#include "httpd/json.h"
#include "httpd/schema.h"
#include "httpd/status.h"
#include "httpd/webconfig.h"

#include "coreapi/archive.h"
#include "coreapi/base/errors.h"
#include "coreapi/recordings.h"
#include "coreapi/base/result.h"
#include "coreapi/base/types.h"

#include <cstddef>
#include <cstdio>
#include <string>
#include <utility>

#include <stdint.h>

#include <arpa/inet.h>
#include <fcntl.h>
#include <ifaddrs.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <unistd.h>

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
		"when the box began writing, Unix time in seconds, which is not when a timer was due"),
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
	HTTPD_SEGMENT_IN("id", ParamType::UInt, "the recording, decimal, as GET /api/v1/recordings names it", 1,
		kMaxRecordingId),
};

const RouteRefusal kStartTimeshiftRefusals[] = {
	HTTPD_REFUSES(Conflict, TimeshiftRunning,
		"the box is already keeping a shift"),
};

const RouteRefusal kStopRecordingRefusals[] = {
	HTTPD_REFUSES(NotFound, NoSuchRecording,
		"the box is recording nothing under that number"),
};

const FieldDesc kArchiveFields[] = {
	HTTPD_MEMBER("id", FieldType::String,
		"what names this recording, 16 hexadecimal digits that stay the same while the file does"),
	HTTPD_MEMBER("title", FieldType::String, "what the guide called the programme, empty when the box noted none"),
	HTTPD_MEMBER("channel", FieldType::String, "the channel's name as the box noted it"),
	HTTPD_MEMBER("channel_id", FieldType::ChannelId, "the channel, hexadecimal, 0 when the box noted none"),
	HTTPD_MEMBER("start", FieldType::Time,
		"when the recording began, Unix time in seconds, reckoned from when the file was last written and the length the box noted"),
	HTTPD_MEMBER("duration", FieldType::UInt, "how long it is in seconds, 0 when the box noted no length"),
	HTTPD_MEMBER("size", FieldType::UInt, "how many bytes the stream file holds"),
	HTTPD_MEMBER("playing", FieldType::Bool, "whether the box is playing this recording on the television now"),
};
const Schema kArchiveSchema = { "archived-recording", HTTPD_FIELDS(kArchiveFields) };

const FieldDesc kArchivePageFields[] = {
	HTTPD_LIST_OF("items", &kArchiveSchema, "one page of finished recordings, in the order sort and order ask for"),
	HTTPD_MEMBER("total", FieldType::UInt, "how many recordings match, on every page"),
	HTTPD_MEMBER_OPTIONAL("next_offset", FieldType::UInt, "the offset of the next page, absent on the last"),
};
const Schema kArchivePageSchema = { "archive-page", HTTPD_FIELDS(kArchivePageFields) };

const FieldDesc kArchiveDetailsFields[] = {
	HTTPD_MEMBER("id", FieldType::String, "the recording, as GET /api/v1/recordings/archive names it"),
	HTTPD_MEMBER("title", FieldType::String, "what the guide called the programme, empty when the box noted none"),
	HTTPD_MEMBER("channel", FieldType::String, "the channel's name as the box noted it"),
	HTTPD_MEMBER("channel_id", FieldType::ChannelId, "the channel, hexadecimal, 0 when the box noted none"),
	HTTPD_MEMBER("start", FieldType::Time,
		"when the recording began, Unix time in seconds, reckoned as the list reckons it"),
	HTTPD_MEMBER("duration", FieldType::UInt, "how long it is in seconds, 0 when the box noted no length"),
	HTTPD_MEMBER("size", FieldType::UInt, "how many bytes the stream file holds"),
	HTTPD_MEMBER("playing", FieldType::Bool, "whether the box is playing this recording on the television now"),
	HTTPD_MEMBER_OPTIONAL("description", FieldType::String,
		"the guide's short text, at most 1024 bytes; absent when the box noted none"),
	HTTPD_MEMBER_OPTIONAL("long_description", FieldType::String,
		"the guide's long text with its line breaks, at most 16384 bytes; absent when the box noted none"),
	HTTPD_MEMBER_OPTIONAL("genre", FieldType::UInt,
		"what it is about as the guide's content byte, the upper four bits the broad class from 1 for a film "
		"to 10 for leisure; absent when the box noted none"),
	HTTPD_MEMBER_OPTIONAL("genre_minor", FieldType::UInt, "a narrower kind the box noted apart; absent when none"),
	HTTPD_MEMBER_OPTIONAL("series", FieldType::String, "the series it belongs to, at most 1024 bytes; absent when none"),
	HTTPD_MEMBER_OPTIONAL("country", FieldType::String, "where it was made, at most 1024 bytes; absent when none"),
	HTTPD_MEMBER_OPTIONAL("year", FieldType::UInt, "the year it was made; absent when none"),
	HTTPD_MEMBER_OPTIONAL("rating", FieldType::UInt,
		"a film database's rating in tenths, 81 for 8.1, at most 100; absent when none"),
	HTTPD_MEMBER_OPTIONAL("quality", FieldType::UInt, "stars from 1 to 3 somebody gave it; absent when none"),
	HTTPD_MEMBER_OPTIONAL("age", FieldType::UInt, "the age in years it is approved from, up to 18, or 99 for a recording that always asks for the PIN; absent when none"),
	HTTPD_MEMBER_AS_WRITTEN("audio", FieldType::Array, true,
		"the names of its sound tracks in recorded order, at most 16 of at most 1024 bytes; absent when none is named",
		NULL, NULL, ElementType::String),
	HTTPD_MEMBER("cover", FieldType::Bool,
		"whether a cover picture lies beside it, which GET /api/v1/recordings/archive/{id}/cover sends"),
};
const Schema kArchiveDetailsSchema = { "archived-recording-details", HTTPD_FIELDS(kArchiveDetailsFields) };

coreapi::archive::SortKey sortKeyOf(const std::string &name)
{
	if (name == "title")
		return coreapi::archive::SortKey::Title;
	if (name == "channel")
		return coreapi::archive::SortKey::Channel;
	if (name == "duration")
		return coreapi::archive::SortKey::Duration;
	if (name == "size")
		return coreapi::archive::SortKey::Size;
	return coreapi::archive::SortKey::Start;
}

Response listArchive(const Request &r)
{
	const size_t limit = r.has("limit") ? (size_t) r.asUInt("limit") : coreapi::archive::kDefaultLimit;
	const size_t offset = r.has("offset") ? (size_t) r.asUInt("offset") : 0;
	const coreapi::archive::SortKey key = sortKeyOf(r.has("sort") ? r.asString("sort") : std::string());
	const bool descending = r.has("order") ? r.asString("order") == "desc"
	                                       : coreapi::archive::descendingByDefault(key);
	coreapi::Result<coreapi::archive::Page> got =
		coreapi::archive::list(r.has("title") ? r.asString("title") : std::string(), offset, limit,
		                       coreapi::archive::Sort(key, descending));
	if (!got.ok())
		return problemFor(got.error());
	const coreapi::archive::Page p = got.value();

	Response out = okJson();
	Json j(out.body, 64 + 300 * p.items.size());
	j.beginObject();
	j.key("items");
	j.beginArray();
	for (size_t i = 0; i < p.items.size(); ++i)
	{
		const coreapi::archive::Entry &e = p.items[i];
		char id[24];
		j.beginObject();
		j.key("id");
		j.value(e.id);
		j.key("title");
		j.value(e.title);
		j.key("channel");
		j.value(e.channel);
		j.key("channel_id");
		j.value(hexId(e.channel_id, id));
		j.key("start");
		j.value((long long) e.start);
		j.key("duration");
		j.value((unsigned long) e.duration);
		j.key("size");
		j.value((unsigned long long) e.size);
		j.key("playing");
		j.value(e.playing);
		j.endObject();
	}
	j.endArray();
	j.key("total");
	j.value((unsigned long) p.total);
	if (offset + p.items.size() < p.total)
	{
		j.key("next_offset");
		j.value((unsigned long) (offset + p.items.size()));
	}
	j.endObject();
	return out;
}

void textIfAny(Json &j, const char *name, const std::string &value)
{
	if (value.empty())
		return;
	j.key(name);
	j.value(value);
}

void numberIfAny(Json &j, const char *name, unsigned long value)
{
	if (value == 0)
		return;
	j.key(name);
	j.value(value);
}

Response archivedDetails(const Request &r)
{
	coreapi::Result<coreapi::archive::Details> got = coreapi::archive::details(r.asString("id"));
	if (!got.ok())
		return problemFor(got.error());
	const coreapi::archive::Details d = got.value();
	const coreapi::archive::Entry &e = d.entry;
	char id[24];

	Response out = okJson();
	Json j(out.body, 512 + d.description.size() + d.long_description.size());
	j.beginObject();
	j.key("id");
	j.value(e.id);
	j.key("title");
	j.value(e.title);
	j.key("channel");
	j.value(e.channel);
	j.key("channel_id");
	j.value(hexId(e.channel_id, id));
	j.key("start");
	j.value((long long) e.start);
	j.key("duration");
	j.value((unsigned long) e.duration);
	j.key("size");
	j.value((unsigned long long) e.size);
	j.key("playing");
	j.value(e.playing);
	textIfAny(j, "description", d.description);
	textIfAny(j, "long_description", d.long_description);
	numberIfAny(j, "genre", d.genre);
	numberIfAny(j, "genre_minor", d.genre_minor);
	textIfAny(j, "series", d.series);
	textIfAny(j, "country", d.country);
	numberIfAny(j, "year", d.year);
	numberIfAny(j, "rating", d.rating);
	numberIfAny(j, "quality", d.quality);
	numberIfAny(j, "age", d.age);
	if (!d.audio.empty())
	{
		j.key("audio");
		j.beginArray();
		for (size_t i = 0; i < d.audio.size(); ++i)
			j.value(d.audio[i]);
		j.endArray();
	}
	j.key("cover");
	j.value(d.cover);
	j.endObject();
	return out;
}

const char *pictureType(const std::string &path)
{
	const size_t dot = path.rfind('.');
	const std::string ext = dot == std::string::npos ? std::string() : path.substr(dot);
	if (ext == ".png")
		return "image/png";
	if (ext == ".gif")
		return "image/gif";
	if (ext == ".bmp")
		return "image/bmp";
	return "image/jpeg";
}

Response archivedCover(const Request &r)
{
	coreapi::Result<std::string> path = coreapi::archive::coverPath(r.asString("id"));
	if (!path.ok())
		return problemFor(path.error());
	const int fd = ::open(path.value().c_str(), O_RDONLY | O_CLOEXEC | O_NOFOLLOW);
	if (fd < 0)
		return problemResponse(StatusNotFound, coreapi::ErrorCode::NoSuchRecording, "this recording has no cover");
	Response out;
	out.code = StatusOk;
	out.content_type = pictureType(path.value());
	if (!answerFromDescriptor(out, fd))
		return problemResponse(StatusNotFound, coreapi::ErrorCode::NoSuchRecording, "this recording has no cover");
	return out;
}

Response playArchived(const Request &r)
{
	coreapi::Result<void> done = coreapi::archive::play(r.asString("id"), r.has("wake") && r.asBool("wake"),
							    r.has("stop_playback") && r.asBool("stop_playback"));
	if (!done.ok())
		return problemFor(done.error());
	return accepted();
}

Response removeArchived(const Request &r)
{
	coreapi::Result<void> done = coreapi::archive::remove(r.asString("id"));
	if (!done.ok())
		return problemFor(done.error());
	return noContent();
}

// A media token reaches the archive; no scope is the whole box.
bool reachesArchive(const std::string &scope)
{
	return scope.empty() || scope == mediaScopeName();
}

bool plainAuthority(const std::string &host)
{
	if (host.empty() || host.size() > 255)
		return false;
	for (size_t i = 0; i < host.size(); ++i)
	{
		const char c = host[i];
		if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') ||
		      c == '.' || c == '-' || c == ':' || c == '[' || c == ']'))
			return false;
	}
	return true;
}

std::string loweredAscii(const std::string &s)
{
	std::string out(s);
	for (size_t i = 0; i < out.size(); ++i)
	{
		if (out[i] >= 'A' && out[i] <= 'Z')
			out[i] = (char) (out[i] - 'A' + 'a');
	}
	return out;
}

// The name without a trailing ":port"; unchanged where that is not unambiguous, which
// for a bracketed or plain spelling alike is never a name this box answers to.
std::string withoutPort(const std::string &host)
{
	if (!host.empty() && host[0] == '[')
	{
		const size_t close = host.find(']');
		if (close == std::string::npos)
			return host;
		if (close + 1 < host.size())
		{
			if (host[close + 1] != ':' || host.find_first_not_of("0123456789", close + 2) != std::string::npos)
				return host;
		}
		return host.substr(1, close - 1);
	}
	const size_t first = host.find(':');
	if (first == std::string::npos)
		return host;
	if (first != host.rfind(':'))
		return host;
	const std::string port = host.substr(first + 1);
	if (port.empty() || port.find_first_not_of("0123456789") != std::string::npos)
		return host;
	return host.substr(0, first);
}

// Every address this box answers an interface by, in whatever family it has one.
void boxAddresses(std::vector<std::string> &out)
{
	struct ifaddrs *list = NULL;
	if (getifaddrs(&list) != 0)
		return;
	for (struct ifaddrs *i = list; i != NULL; i = i->ifa_next)
	{
		if (i->ifa_addr == NULL)
			continue;
		char text[INET6_ADDRSTRLEN];
		if (i->ifa_addr->sa_family == AF_INET)
		{
			const struct sockaddr_in *a = (const struct sockaddr_in *) i->ifa_addr;
			if (inet_ntop(AF_INET, &a->sin_addr, text, sizeof(text)) != NULL)
				out.push_back(text);
		}
		else if (i->ifa_addr->sa_family == AF_INET6)
		{
			const struct sockaddr_in6 *a = (const struct sockaddr_in6 *) i->ifa_addr;
			if (inet_ntop(AF_INET6, &a->sin6_addr, text, sizeof(text)) != NULL)
				out.push_back(loweredAscii(text));
		}
	}
	freeifaddrs(list);
}

// Whether a name is this box: an interface address, a name whose first label is this
// box's own (any domain after it, a router or mDNS alias included), localhost or a
// loopback address, or the configured public address. A port or a trailing dot is read
// and ignored either way.
bool hostNamesThisBox(const std::string &host)
{
	std::string stripped = withoutPort(host);
	if (!stripped.empty() && stripped[stripped.size() - 1] == '.')
		stripped.resize(stripped.size() - 1);
	const std::string name = loweredAscii(stripped);
	if (name.empty())
		return false;
	if (name == "localhost" || name == "127.0.0.1" || name == "::1")
		return true;

	char hostname[256];
	if (gethostname(hostname, sizeof(hostname)) == 0)
	{
		hostname[sizeof(hostname) - 1] = 0;
		const std::string box_host = loweredAscii(hostname);
		const std::string box_label = box_host.substr(0, box_host.find('.'));
		if (!box_label.empty() && name.substr(0, name.find('.')) == box_label)
			return true;
	}

	std::vector<std::string> addresses;
	boxAddresses(addresses);
	for (size_t i = 0; i < addresses.size(); ++i)
	{
		if (name == addresses[i])
			return true;
	}
	return false;
}

std::string oneLine(const std::string &s)
{
	std::string out;
	for (size_t i = 0; i < s.size(); ++i)
	{
		const unsigned char c = (unsigned char) s[i];
		if (c < 0x20 || c == 0x7f)
		{
			if (!out.empty() && out[out.size() - 1] != ' ')
				out += ' ';
			continue;
		}
		out += (char) c;
	}
	return out;
}

Response archivedFile(const Request &r)
{
	if (!reachesArchive(r.scope()))
		return problemResponse(StatusForbidden, coreapi::ErrorCode::NotPermitted, notPermittedDetail());
	coreapi::Result<coreapi::archive::Entry> e = coreapi::archive::find(r.asString("id"));
	if (!e.ok())
		return problemFor(e.error());
	const int fd = ::open(e.value().path.c_str(), O_RDONLY | O_CLOEXEC | O_NOFOLLOW);
	if (fd < 0)
		return problemResponse(StatusNotFound, coreapi::ErrorCode::NoSuchRecording, "no recording has that id");
	Response out;
	out.code = StatusOk;
	out.content_type = "video/mp2t";
	if (!answerFromDescriptor(out, fd))
		return problemResponse(StatusNotFound, coreapi::ErrorCode::NoSuchRecording, "no recording has that id");
	return out;
}

Response archivedPlaylist(const Request &r)
{
	if (!reachesArchive(r.scope()))
		return problemResponse(StatusForbidden, coreapi::ErrorCode::NotPermitted, notPermittedDetail());
	/* The gate may have read another credential; write out only an address token that is
	   a media token and the one the gate used. */
	const std::string &token = r.addressToken();
	if (!token.empty())
	{
		Credentials presented;
		presented.query_token = token;
		std::string resolved_scope;
		granted(presented, true, &resolved_scope);
		if (resolved_scope != mediaScopeName() || r.scope() != mediaScopeName())
			return problemResponse(StatusBadRequest, coreapi::ErrorCode::MissingParameter,
			                       "the token in this address is not a media token this box drew");
	}
	if (!plainAuthority(r.host()))
		return problemResponse(StatusBadRequest, coreapi::ErrorCode::NoAuthority,
		                       "the request named no host this box can be reached under");
	// A Host that is not this box is never echoed; the box's own address stands in for it.
	const std::string authority = hostNamesThisBox(r.host()) ? r.host() : r.localAddress();
	if (authority.empty())
		return problemResponse(StatusBadRequest, coreapi::ErrorCode::NoAuthority,
		                       "the request named no host this box can be reached under");
	coreapi::Result<coreapi::archive::Entry> e = coreapi::archive::find(r.asString("id"));
	if (!e.ok())
		return problemFor(e.error());
	const coreapi::archive::Entry &one = e.value();
	char length[24];
	std::snprintf(length, sizeof(length), "%ld", one.duration > 0 ? one.duration : -1L);
	Response out;
	out.code = StatusOk;
	out.content_type = "audio/x-mpegurl";
	out.body = std::string("#EXTM3U\n#EXTINF:") + length + "," + oneLine(one.title) + "\n" +
	           "http://" + authority + "/api/v1/recordings/archive/" + one.id + "/file" +
	           (token.empty() ? std::string() : std::string("?") + queryTokenName() + "=" + token) + "\n";
	return out;
}

const Param kArchiveListParams[] = {
	HTTPD_QUERY_TEXT("title", "only recordings whose title holds these words, any case", 255),
	HTTPD_QUERY_IN("offset", ParamType::UInt, "how many matching recordings to skip, 0 when left out", 0, 1000000),
	HTTPD_QUERY_IN("limit", ParamType::UInt, "how many at most, 15 when left out", 1,
		(long) coreapi::archive::kMaxLimit),
	HTTPD_QUERY_FROM_CLOSED_SET("sort", "what the list is ordered by, start when left out; see the values below",
		"title,start,channel,duration,size",
		"title: the title, any case\n"
		"start: when the recording began\n"
		"channel: the channel's name, any case\n"
		"duration: how long it is\n"
		"size: how many bytes the stream file holds"),
	HTTPD_QUERY_FROM_CLOSED_SET("order", "which way; left out, desc for start, duration and size and asc for title and channel",
		"asc,desc",
		"asc: smallest, earliest or A first\n"
		"desc: largest, latest or Z first"),
};

const Param kArchiveOneParams[] = {
	HTTPD_SEGMENT_TEXT("id", "the recording, as GET /api/v1/recordings/archive names it", 16),
};

const Param kArchivePlayParams[] = {
	HTTPD_SEGMENT_TEXT("id", "the recording, as GET /api/v1/recordings/archive names it", 16),
	HTTPD_BODY("wake", ParamType::Bool,
		"`true` switches a box in standby on and then plays the recording; `false` (the default) leaves a box in standby alone and the request is refused with `409 box-in-standby`"),
	HTTPD_BODY("stop_playback", ParamType::Bool,
		"`true` ends a file the movie player is playing and then plays the recording; `false` (the default) leaves the playback alone and the request is refused with `409 playback-running`"),
};

const RouteRefusal kArchiveDetailsRefusals[] = {
	HTTPD_REFUSES(NotFound, NoSuchRecording, "no recording has that id"),
};

const RouteRefusal kArchiveCoverRefusals[] = {
	HTTPD_REFUSES(NotFound, NoSuchRecording, "no recording has that id, or it has no cover"),
};

const RouteRefusal kArchivePlayRefusals[] = {
	HTTPD_REFUSES(NotFound, NoSuchRecording, "no recording has that id"),
	HTTPD_REFUSES(Conflict, PlaybackRunning, "something is playing in the movie player"),
	HTTPD_REFUSES(Conflict, RecordingPlaying, "the box is playing back its timeshift"),
	HTTPD_REFUSES(Conflict, ModeUnavailable, "the box has not finished starting"),
	HTTPD_REFUSES(Conflict, BoxInStandby, "the box is in standby"),
};

const RouteRefusal kArchiveRemoveRefusals[] = {
	HTTPD_REFUSES(NotFound, NoSuchRecording, "no recording has that id"),
	HTTPD_REFUSES(Conflict, RecordingRunning, "the box is still writing this recording"),
	HTTPD_REFUSES(Conflict, RecordingPlaying, "the box is playing this recording"),
};

const RouteRefusal kArchiveFileRefusals[] = {
	HTTPD_REFUSES(NotFound, NoSuchRecording, "no recording has that id"),
	HTTPD_REFUSES(Denied, NotPermitted, "the credential reaches another part of the box"),
};

const RouteRefusal kArchivePlaylistRefusals[] = {
	HTTPD_REFUSES(NotFound, NoSuchRecording, "no recording has that id"),
	HTTPD_REFUSES(Denied, NotPermitted, "the credential reaches another part of the box"),
	HTTPD_REFUSES(InvalidArgument, MissingParameter, "the token in the address is not a media token"),
	HTTPD_REFUSES(InvalidArgument, NoAuthority, "the request named no host"),
};

const Endpoint kRecordingEndpoints[] = {
	{ Method::Get, "/api/v1/recordings", AuthLevel::Read,
	  "every recording the box is taking at this moment",
	  "Returns every recording the box is writing right now, including the shift it "
	  "keeps of what it is showing if one is running, as a flat list. There is no "
	  "paging: a box takes as many recordings at once as its tuners and its own "
	  "ceiling allow, which is always a handful, and a cursor over a list that is "
	  "rebuilt whenever one starts or stops would name a place that had already "
	  "moved. A box recording nothing answers an empty list.\n\n"
	  "**Related:** `POST /api/v1/recordings/timeshift`, `DELETE /api/v1/recordings/{id}`.",
	  NULL, 0, &kRecordingListSchema, &listRecordings, false,
	  Answers200, HTTPD_NO_REFUSALS },
	/* Written out and therefore answered ahead of the route below it, which
	   binds anything in that position: which of two matching routes answers is
	   settled by how many of its segments are written out. */
	{ Method::Post, "/api/v1/recordings/timeshift", AuthLevel::Write,
	  "asks the box to begin shifting the channel it is showing",
	  "Asks the box to start keeping a time shift of whatever channel it is "
	  "currently showing. `202` means the command reached the loop that writes "
	  "recordings, not that the shift has begun: watch `GET /api/v1/recordings` for "
	  "a row with `timeshift` set to true to see it running.\n\n"
	  "**Preconditions:** the box is not already keeping a shift.\n\n"
	  "**Side effects:** a new recording of the current channel starts under the "
	  "box's own tuner, listed like any other recording.\n\n"
	  "**Refusals:**\n"
	  "- `409 timeshift-running`: the box is already keeping a shift.\n\n"
	  "**Related:** `GET /api/v1/recordings`, `DELETE /api/v1/recordings/timeshift`.",
	  NULL, 0, NULL, &startTimeshift, false,
	  Answers202, HTTPD_REFUSALS(kStartTimeshiftRefusals) },
	{ Method::Delete, "/api/v1/recordings/timeshift", AuthLevel::Write,
	  "asks the box to end the shift it is keeping",
	  "Asks the box to stop the time shift it is keeping of the channel it is "
	  "showing, which also stops the box from starting the next one on its own. "
	  "`202` means the command reached the loop, not that the shift has already "
	  "ended: watch `GET /api/v1/recordings` for the row to disappear.\n\n"
	  "**Side effects:** the file the shift was writing stops growing and is no "
	  "longer listed under `GET /api/v1/recordings`.\n\n"
	  "**Related:** `POST /api/v1/recordings/timeshift`, `GET /api/v1/recordings`.",
	  NULL, 0, NULL, &stopTimeshift, false,
	  Answers202, HTTPD_NO_REFUSALS },
	{ Method::Delete, "/api/v1/recordings/{id}", AuthLevel::Write,
	  "asks the box to end one recording",
	  "Asks the box to stop the recording named by `id`, which is the same number "
	  "`GET /api/v1/recordings` lists it under and is in fact the timer the "
	  "recording carries. `202` means the command reached the timer daemon, not "
	  "that the file has stopped growing: watch `GET /api/v1/recordings` for the "
	  "row to disappear. Ending the shift of what is being shown this way works "
	  "the same as ending any other recording.\n\n"
	  "**Refusals:**\n"
	  "- `404 no-such-recording`: the box is recording nothing under that id; this "
	  "also covers asking twice, since the id does not come back once the recording "
	  "has ended.\n\n"
	  "**Related:** `GET /api/v1/recordings`, `DELETE /api/v1/recordings/timeshift`.",
	  HTTPD_PARAMS(kOneParams), NULL, &stopRecording, false,
	  Answers202, HTTPD_REFUSALS(kStopRecordingRefusals) },
	{ Method::Get, "/api/v1/recordings/archive", AuthLevel::Read,
	  "the finished recordings on the record disk, newest first unless sorted otherwise, a page at a time",
	  "Lists the finished recordings in the box's record directory and the folders directly below it, newest "
	  "first, 15 to a page unless `limit` says otherwise. A recording is a transport stream the box wrote with "
	  "its metadata file beside it; title, channel and length come from that file, `start` is reckoned from "
	  "when the stream was last written and that length. `title` narrows the list to titles holding those "
	  "words. `sort` orders the matching recordings by `title`, `start`, `channel`, `duration` or `size` "
	  "before the page is cut; `order` left out is `desc` for `start`, `duration` and `size` and `asc` for "
	  "`title` and `channel`. Recordings that compare equal follow their `id`, ascending, so pages never "
	  "overlap. Pass `next_offset` back as `offset`, with the same `title`, `sort` and `order`, for the next "
	  "page; the last page carries none.\n\n"
	  "**Related:** `GET /api/v1/recordings/archive/{id}`, `GET /api/v1/recordings/archive/{id}/file`, "
	  "`POST /api/v1/recordings/archive/{id}/play`, "
	  "`DELETE /api/v1/recordings/archive/{id}`, `GET /api/v1/recordings`.",
	  HTTPD_PARAMS(kArchiveListParams), &kArchivePageSchema, &listArchive, false,
	  Answers200, HTTPD_NO_REFUSALS },
	{ Method::Get, "/api/v1/recordings/archive/{id}", AuthLevel::Read,
	  "one finished recording with everything its metadata file says about it",
	  "Answers one finished recording with the members the list gives it and the details its metadata file "
	  "states beside them: the guide's short and long text, genre, series, country, year, rating, stars, "
	  "the age it is approved from and the names of its sound tracks. A detail the file leaves empty, "
	  "states as 0 or states past its range is left out of the answer. Texts are cut at a character boundary, the long text at 16384 "
	  "bytes and every other at 1024. `cover` says whether `GET /api/v1/recordings/archive/{id}/cover` has a "
	  "picture to send.\n\n"
	  "**Refusals:**\n"
	  "- `404 no-such-recording`: no recording has that `id`.\n\n"
	  "**Related:** `GET /api/v1/recordings/archive`, `GET /api/v1/recordings/archive/{id}/cover`.",
	  HTTPD_PARAMS(kArchiveOneParams), &kArchiveDetailsSchema, &archivedDetails, false,
	  Answers200, HTTPD_REFUSALS(kArchiveDetailsRefusals) },
	{ Method::Get, "/api/v1/recordings/archive/{id}/cover", AuthLevel::Read,
	  "the cover picture of one finished recording",
	  "Sends the picture that lies beside the recording's stream under the same name, looked for as the "
	  "box's movie browser looks: `.jpg`, `.png`, `.gif`, `.jpeg`, then `.bmp`. Only a plain file inside the "
	  "record directory counts; a link is never followed.\n\n"
	  "**Refusals:**\n"
	  "- `404 no-such-recording`: no recording has that `id`, or it has no cover.\n\n"
	  "**Related:** `GET /api/v1/recordings/archive/{id}`.",
	  HTTPD_PARAMS(kArchiveOneParams), NULL, &archivedCover, false,
	  Answers200, HTTPD_REFUSALS(kArchiveCoverRefusals) },
	{ Method::Post, "/api/v1/recordings/archive/{id}/play", AuthLevel::Write,
	  "asks the box to play one finished recording on the television; refused with playback-running while "
	  "the movie player plays a file unless stop_playback is set, and with box-in-standby in standby unless "
	  "wake is set",
	  "Hands one finished recording to the box's movie player, which plays it on the television in place of "
	  "live television and returns to it when the recording ends or the viewer stops it; with the player set "
	  "to repeat, it plays until the viewer stops it. `202` means the "
	  "request reached the box's loop, not that the picture has changed; this route never waits for the "
	  "player.\n\n"
	  "**Preconditions:** the box is not in standby, or `wake` is `true`. The movie player plays no file, or "
	  "`stop_playback` is `true`. The box is not playing back its timeshift.\n\n"
	  "**Refusals:**\n"
	  "- `404 no-such-recording`: no recording has that `id`.\n"
	  "- `409 playback-running`: the movie player is playing a file, this recording or any other, and "
	  "`stop_playback` was not `true`. Send again with `stop_playback: true` to end that playback and play "
	  "this recording.\n"
	  "- `409 recording-playing`: the box is playing back the shift it keeps of live television. Neither "
	  "`stop_playback` nor `wake` helps; end the timeshift first.\n"
	  "- `409 mode-unavailable`: the box is still starting and has not settled on a mode. Send again once "
	  "it has.\n"
	  "- `409 box-in-standby`: the box is in standby and `wake` was not `true`. Send again with `wake: true` "
	  "to switch the box on and play the recording.\n\n"
	  "**Related:** `GET /api/v1/recordings/archive`, `GET /api/v1/system/standby`, `POST /api/v1/zap`.",
	  HTTPD_PARAMS(kArchivePlayParams), NULL, &playArchived, false,
	  Answers202, HTTPD_REFUSALS(kArchivePlayRefusals) },
	{ Method::Get, "/api/v1/recordings/archive/{id}/file", AuthLevel::Read,
	  "the bytes of one finished recording, for a player",
	  "Sends the transport stream of one finished recording straight from the disk. A `Range` header asks "
	  "for part of it and is answered `206`, which is how a player jumps. A reader on the home network is "
	  "answered by address alone, with no token. Anyone else, or a player that can carry nothing else, puts "
	  "the media token of `POST /api/v1/token/media`, which only a login draws, in its query.\n\n"
	  "**Refusals:**\n"
	  "- `404 no-such-recording`: no recording has that `id`.\n"
	  "- `403 not-permitted`: the token in the address stands for another part of the box.\n\n"
	  "**Related:** `GET /api/v1/recordings/archive/{id}/playlist.m3u`, `POST /api/v1/token/media`.",
	  HTTPD_PARAMS(kArchiveOneParams), NULL, &archivedFile, true,
	  Answers200 | Answers206, HTTPD_REFUSALS(kArchiveFileRefusals) },
	{ Method::Get, "/api/v1/recordings/archive/{id}/playlist.m3u", AuthLevel::Read,
	  "a one line playlist for a player such as VLC, carrying the address of the recording",
	  "Answers an M3U playlist with one entry: the address of the recording's file. A reader on the home "
	  "network fetches it without a token and the entry carries none. Fetched with the media token of "
	  "`POST /api/v1/token/media` in the query, the entry carries that very token, so the playlist lasts "
	  "exactly as long as it does; no token is ever drawn here. Meant for the home network: the public "
	  "address does not carry these routes. The entry's address is the `Host` only when it names this box "
	  "itself (an interface address, its hostname under any domain, localhost or the configured public "
	  "address, a port or a trailing dot either carried or not); anything else is replaced by the address "
	  "the box itself answered the request on, never echoed.\n\n"
	  "**Refusals:**\n"
	  "- `400 missing-parameter`: the token in the address is not a media token this box drew, or is not "
	  "the credential this request was granted on; it is never written out.\n"
	  "- `400 no-authority`: the request named no host this box answers to, and the connection could not "
	  "say its own address either.\n"
	  "- `404 no-such-recording`: no recording has that `id`.\n"
	  "- `403 not-permitted`: the token stands for another part of the box.\n\n"
	  "**Related:** `GET /api/v1/recordings/archive/{id}/file`.",
	  HTTPD_PARAMS(kArchiveOneParams), NULL, &archivedPlaylist, true,
	  Answers200, HTTPD_REFUSALS(kArchivePlaylistRefusals) },
	{ Method::Delete, "/api/v1/recordings/archive/{id}", AuthLevel::Write,
	  "removes one finished recording from the disk for good",
	  "Removes one finished recording: the stream file, its metadata file and a cover picture of the same "
	  "name. Nothing outside the record directory is ever reached.\n\n"
	  "**Refusals:**\n"
	  "- `404 no-such-recording`: no recording has that `id`, also when it was removed already.\n"
	  "- `409 recording-running`: the box is still writing this recording; stop it first.\n"
	  "- `409 recording-playing`: the box is playing this recording.\n\n"
	  "**Related:** `GET /api/v1/recordings/archive`, `DELETE /api/v1/recordings/{id}`.",
	  HTTPD_PARAMS(kArchiveOneParams), NULL, &removeArchived, false,
	  Answers204, HTTPD_REFUSALS(kArchiveRemoveRefusals) },
};

} // namespace

extern const RouteTable recordingsTable = {
	HTTPD_TABLE("recordings", kRecordingEndpoints)
};

} // namespace httpd
