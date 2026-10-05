/*
 * ep_playback.cpp - what the television shows
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
#include "coreapi/channels.h"
#include "coreapi/playback.h"

namespace httpd
{

namespace
{

const char kSourceDocs[] =
	"channel: a live channel, from a tuner or the web, or its timeshift\n"
	"recording: a finished recording of the archive\n"
	"file: any other file or stream the movie player plays\n"
	"none: nothing, the box is in standby or has no channel";

const FieldDesc kChannelRefFields[] = {
	HTTPD_MEMBER("id", FieldType::ChannelId, "the channel, hexadecimal"),
	HTTPD_MEMBER("name", FieldType::String, "what the channel list calls it"),
};
const Schema kChannelRefSchema = { "playback-channel", HTTPD_FIELDS(kChannelRefFields) };

const FieldDesc kRecordingRefFields[] = {
	HTTPD_MEMBER("id", FieldType::String, "the recording, as GET /api/v1/recordings/archive names it"),
	HTTPD_MEMBER("title", FieldType::String, "the programme's title, empty when its metadata names none"),
	HTTPD_MEMBER("channel", FieldType::String, "the channel it was recorded from, empty when unknown"),
};
const Schema kRecordingRefSchema = { "playback-recording", HTTPD_FIELDS(kRecordingRefFields) };

const FieldDesc kFileRefFields[] = {
	HTTPD_MEMBER("name", FieldType::String, "the file's name without its directories, or the last part of a stream's address"),
	HTTPD_MEMBER("title", FieldType::String, "the title the player shows, empty when it has none"),
};
const Schema kFileRefSchema = { "playback-file", HTTPD_FIELDS(kFileRefFields) };

const FieldDesc kPlaybackFields[] = {
	HTTPD_MEMBER_OF_SET("source", "channel,recording,file,none", "what the television shows", kSourceDocs),
	HTTPD_OBJECT_OPTIONAL("channel", &kChannelRefSchema, "the live channel, only for source channel"),
	HTTPD_MEMBER_OPTIONAL("timeshift", FieldType::Bool, "whether the channel is shown from its timeshift, only for source channel"),
	HTTPD_OBJECT_OPTIONAL("recording", &kRecordingRefSchema, "the recording, only for source recording"),
	HTTPD_OBJECT_OPTIONAL("file", &kFileRefSchema, "the file, only for source file"),
	HTTPD_MEMBER_OPTIONAL("position", FieldType::Int, "seconds played, only for a recording or a file"),
	HTTPD_MEMBER_OPTIONAL("duration", FieldType::Int, "seconds in all, 0 when the player does not know, only for a recording or a file"),
	HTTPD_MEMBER_OPTIONAL("paused", FieldType::Bool, "whether the player stands still, only for a recording or a file"),
	HTTPD_MEMBER_AS_WRITTEN("state", FieldType::String, true,
		"how the player moves, only for a recording or a file: playing at normal speed, paused, winding forward "
		"or rewind, and stopped in the moment the player has ended it", NULL, "playing,paused,forward,rewind,stopped",
		ElementType::None),
	HTTPD_MEMBER_OPTIONAL("speed", FieldType::Int,
		"the player's speed, 1 normal, 0 paused, above 1 winding forward, below 0 winding back, only for a recording or a file"),
	HTTPD_OBJECT_OPTIONAL("returns_to", &kChannelRefSchema,
		"the live channel the box shows once the playback ends, only for a recording or a file and only when the box has one"),
};
const Schema kPlaybackSchema = { "playback", HTTPD_FIELDS(kPlaybackFields) };

const RouteRefusal kPlaybackRefusals[] = {
	HTTPD_REFUSES(Internal, CurrentChannelUnresolved, "the running channel is not in the channel list"),
};

// The route's own words, which the check over answer sets reads beside its rows.
const char *sourceWord(coreapi::playback::Source s)
{
	switch (s)
	{
		case coreapi::playback::Source::Channel:   return "channel";
		case coreapi::playback::Source::Recording: return "recording";
		case coreapi::playback::Source::File:      return "file";
		case coreapi::playback::Source::None:      break;
	}
	return "none";
}

const char *stateWord(coreapi::playback::State s)
{
	switch (s)
	{
		case coreapi::playback::State::Paused:  return "paused";
		case coreapi::playback::State::Forward: return "forward";
		case coreapi::playback::State::Rewind:  return "rewind";
		case coreapi::playback::State::Stopped: return "stopped";
		case coreapi::playback::State::Playing: break;
	}
	return "playing";
}

void appendChannelRef(Json &j, const coreapi::ChannelInfo &c)
{
	char id[24];
	j.beginObject();
	j.key("id");
	j.value(hexId(c.id, id));
	j.key("name");
	j.value(c.name);
	j.endObject();
}

Response playbackNow(const Request &)
{
	coreapi::Result<coreapi::playback::Now> got = coreapi::playback::now();
	if (!got.ok())
		return problemFor(got.error());
	const coreapi::playback::Now &now = got.value();
	const coreapi::playback::Snapshot &p = now.playing;

	Response out = okJson();
	Json j(out.body, 256);
	j.beginObject();
	j.key("source");
	j.value(sourceWord(now.source));
	switch (now.source)
	{
		case coreapi::playback::Source::Channel:
			j.key("channel");
			appendChannelRef(j, now.channel);
			j.key("timeshift");
			j.value(p.active && p.timeshift);
			break;
		case coreapi::playback::Source::Recording:
		case coreapi::playback::Source::File:
		{
			if (now.source == coreapi::playback::Source::Recording)
			{
				j.key("recording");
				j.beginObject();
				j.key("id");
				j.value(p.archive_id);
				j.key("title");
				j.value(p.title);
				j.key("channel");
				j.value(p.channel);
				j.endObject();
			}
			else
			{
				j.key("file");
				j.beginObject();
				j.key("name");
				j.value(p.name);
				j.key("title");
				j.value(p.title);
				j.endObject();
			}
			j.key("position");
			j.value(p.position);
			j.key("duration");
			j.value(p.duration);
			j.key("paused");
			j.value(p.state == coreapi::playback::State::Paused);
			j.key("state");
			j.value(stateWord(p.state));
			j.key("speed");
			j.value(p.speed);
			coreapi::Result<coreapi::ChannelInfo> live = coreapi::channels::current();
			if (live.ok())
			{
				j.key("returns_to");
				appendChannelRef(j, live.value());
			}
			break;
		}
		case coreapi::playback::Source::None:
			break;
	}
	j.endObject();
	return out;
}

const Endpoint kPlaybackEndpoints[] = {
	{ Method::Get, "/api/v1/playback", AuthLevel::Read,
	  "what the television shows: a live channel, a recording, another file or nothing",
	  "Answers what the box shows on the television, the one place that says so. `source` is `channel` "
	  "for live television from a tuner or the web, also while it is shown from its timeshift; "
	  "`recording` for a finished recording of the archive the movie player plays; `file` for any other "
	  "file or stream it plays; `none` in standby or with no channel at all. A recording or file carries "
	  "where the player stands: `position` and `duration` in seconds as the player last read them, about "
	  "once a second, `paused` and `state`; `returns_to` names the channel the box goes back to when the "
	  "playback ends. While a recording or file plays, `GET /api/v1/channels/current` answers "
	  "`404 no-running-channel`.\n\n"
	  "**Refusals:**\n"
	  "- `500 current-channel-unresolved`: the box names a running channel its channel list does not hold.\n\n"
	  "**Related:** the `playback` event on `GET /api/v1/events` says when this changes; "
	  "`GET /api/v1/channels/current`, `GET /api/v1/recordings/archive/{id}`.",
	  NULL, 0, &kPlaybackSchema, &playbackNow, false,
	  Answers200, HTTPD_REFUSALS(kPlaybackRefusals) },
};

} // namespace

extern const RouteTable playbackTable = {
	HTTPD_TABLE("playback", kPlaybackEndpoints)
};

} // namespace httpd
