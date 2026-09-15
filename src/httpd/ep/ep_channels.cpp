/*
 * ep_channels.cpp - routes for channels and bouquets
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
#include "httpd/webconfig.h"

#include "coreapi/channels.h"
#include "coreapi/base/errors.h"
#include "coreapi/base/result.h"
#include "coreapi/base/types.h"

#include <algorithm>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <string>
#include <utility>
#include <vector>

#include <fcntl.h>
#include <stdint.h>

namespace httpd
{

/* Everything below is in the unnamed namespace, the one table this module states
   excepted: a handler named the obvious thing here cannot then collide with a
   handler named the obvious thing in the module beside it. */
namespace
{

/* The default page and the widest one a caller may ask for. Declared in the table
   below rather than checked in a handler, so the router refuses the rest before a
   handler is entered.

   Paged at all because the buildsystem ships channel lists of 1238 and 2664
   services and a box that imports its own holds more. */
const long          kMaxPage     = 500;
const unsigned long kDefaultPage = 100;
const long          kMaxBouquetId = 2147483647L;

const char *kindName(coreapi::ServiceKind k)
{
	switch (k)
	{
		case coreapi::ServiceKind::Unknown:  return "unknown";
		case coreapi::ServiceKind::Tv:       return "tv";
		case coreapi::ServiceKind::Radio:    return "radio";
		case coreapi::ServiceKind::WebTv:    return "webtv";
		case coreapi::ServiceKind::WebRadio: return "webradio";
	}
	/* No default above, and an unhandled enumerator is an error in this
	   directory, so a kind added without a name stops the build. What is left
	   is a value cast into the enum from outside it. */
	return "unknown";
}

// The channel stack's own split, radio against everything else, so a kind it has
// not filled in is listed with television rather than nowhere.
bool isRadio(coreapi::ServiceKind k)
{
	return k == coreapi::ServiceKind::Radio || k == coreapi::ServiceKind::WebRadio;
}

/* What a channel's picture and its sound tracks are coded in, as the words a
   player is written against.

   unknown beside none: none is a channel that carries no picture, which is a
   radio service, and unknown is a channel whose streams this box has never read.
   A player told none plays the sound; a player told unknown has to try or ask. */
const char kVideoCodecValues[] = "unknown,none,mpeg2,h264,hevc,cavs";
const char kAudioCodecValues[] = "unknown,mp2,ac3,eac3,aac,aacplus,dts,dtshd,lpcm";

const char *videoCodecName(coreapi::VideoCodec c)
{
	switch (c)
	{
		case coreapi::VideoCodec::Unknown: return "unknown";
		case coreapi::VideoCodec::None:    return "none";
		case coreapi::VideoCodec::Mpeg2:   return "mpeg2";
		case coreapi::VideoCodec::H264:    return "h264";
		case coreapi::VideoCodec::Hevc:    return "hevc";
		case coreapi::VideoCodec::Cavs:    return "cavs";
	}
	// No default above, so a codec added without a word stops the build. What
	// is left is a value cast into the enum from outside it.
	return "unknown";
}

const char *audioCodecName(coreapi::AudioCodec c)
{
	switch (c)
	{
		case coreapi::AudioCodec::Unknown: return "unknown";
		case coreapi::AudioCodec::Mp2:     return "mp2";
		case coreapi::AudioCodec::Ac3:     return "ac3";
		case coreapi::AudioCodec::Eac3:    return "eac3";
		case coreapi::AudioCodec::Aac:     return "aac";
		case coreapi::AudioCodec::AacPlus: return "aacplus";
		case coreapi::AudioCodec::Dts:     return "dts";
		case coreapi::AudioCodec::DtsHd:   return "dtshd";
		case coreapi::AudioCodec::Lpcm:    return "lpcm";
	}
	return "unknown";
}

const FieldDesc kVideoFields[] = {
	HTTPD_MEMBER_OF_SET("codec", kVideoCodecValues,
		"what the picture is coded in, none for a service that carries no picture and unknown for one whose streams this box has not read"),
	HTTPD_MEMBER("pid", FieldType::UInt,
		"the stream the picture is in, and nought where the box holds none"),
};

const Schema kVideoSchema = { "video-stream", HTTPD_FIELDS(kVideoFields) };

const FieldDesc kAudioTrackFields[] = {
	HTTPD_MEMBER("pid", FieldType::UInt, "the stream this track is in"),
	HTTPD_MEMBER_OF_SET("codec", kAudioCodecValues,
		"what this track is coded in, and unknown for one the box found and could not classify"),
	HTTPD_MEMBER("description", FieldType::String,
		"what the stream says the track is, which is usually a three letter language code and is not translated here"),
	HTTPD_MEMBER("selected", FieldType::Bool,
		"whether this is the track the box is playing, which no track is while the box has not read the channel's streams"),
};

const Schema kAudioTrackSchema = { "audio-track", HTTPD_FIELDS(kAudioTrackFields) };

/* The members every channel answer carries. Written once and shared, because two
   shapes are answered here: the listing carries these, and the two routes that
   answer one channel carry these and what its streams are. */
#define HTTPD_CHANNEL_MEMBERS \
	HTTPD_MEMBER("id", FieldType::ChannelId, "what every route here names this channel by, hexadecimal"), \
	HTTPD_MEMBER("epg_id", FieldType::ChannelId, \
		"the identifier the guide keeps this channel's schedule under, which is another channel's wherever two share one, and which the guide routes here look up for themselves, so nothing has to ask for a schedule under it"), \
	HTTPD_MEMBER("number", FieldType::Int, \
		"the position the box lists the channel at, negative or nought where it has none"), \
	HTTPD_MEMBER("name", FieldType::String, "what the box calls the channel"), \
	HTTPD_MEMBER("url", FieldType::String, \
		"where a channel played without a tuner is fetched from, empty for a channel that is tuned"), \
	HTTPD_MEMBER("service_id", FieldType::UInt, "the service this channel is in its transport stream"), \
	HTTPD_MEMBER("transport_stream_id", FieldType::UInt, "the transport stream that carries it"), \
	HTTPD_MEMBER("original_network_id", FieldType::UInt, "the network that stream belongs to"), \
	HTTPD_MEMBER("satellite_position", FieldType::Int, \
		"where the box points for it, nought for a channel that needs no dish"), \
	HTTPD_MEMBER("freq_id", FieldType::UInt, \
		"the transponder key the box files it under, which is not a frequency"), \
	HTTPD_MEMBER_OF_SET("kind", "unknown,tv,radio,webtv,webradio", \
		"what sort of service it is, and unknown for one the box has not classified"), \
	HTTPD_MEMBER("scrambled", FieldType::Bool, "whether the box expects to have to descramble it"), \
	HTTPD_MEMBER("locked", FieldType::Bool, \
		"whether the box asks for the parental code before it plays")

const FieldDesc kChannelFields[] = { HTTPD_CHANNEL_MEMBERS };

const Schema kChannelSchema = { "channel", HTTPD_FIELDS(kChannelFields) };

/* The same channel with what the box knows about its streams, which is what the
   two routes that answer a single channel carry and the listing does not.

   Not in the listing because it cannot be: the track list is read off the channel
   itself and a page of five hundred would read five hundred of them, and on a box
   nobody has switched around on it would answer nothing known for every one. */
const FieldDesc kChannelDetailFields[] = {
	HTTPD_CHANNEL_MEMBERS,
	HTTPD_OBJECT("video", &kVideoSchema, "what the picture is and where it is"),
	HTTPD_LIST_OF("audio", &kAudioTrackSchema,
		"the sound tracks, in the order the stream lists them, and empty while streams_known is false"),
	/* The honest member. The box reads a channel's stream layout when it plays
	   the channel, so a channel nobody has switched to has none, and a caller
	   told nothing about that would read an empty track list as a channel with
	   no sound. What a channel scan writes down beforehand is one pid with
	   neither a codec nor a language. */
	HTTPD_MEMBER("streams_known", FieldType::Bool,
		"whether the box has read this channel's streams, which it does when it plays the channel; false leaves audio empty and video unknown or none"),
};

const Schema kChannelDetailSchema = { "channel-detail", HTTPD_FIELDS(kChannelDetailFields) };

#undef HTTPD_CHANNEL_MEMBERS

const FieldDesc kChannelPageFields[] = {
	HTTPD_LIST_OF("items", &kChannelSchema,
		"the channels of this page, in the order the box holds them"),
	/* Absent on the last page rather than empty, and that is why it is the one
	   member here a reader has to test for. An empty value is an absent one
	   everywhere a parameter is read, so a client handing an empty cursor back
	   would be answered with the first page again. */
	HTTPD_MEMBER_OPTIONAL("next_cursor", FieldType::ChannelId,
		"hand this back as cursor for the page after this one, and absent when this page is the last"),
};

const Schema kChannelPageSchema = { "channel-page", HTTPD_FIELDS(kChannelPageFields) };

const FieldDesc kBouquetFields[] = {
	HTTPD_MEMBER("id", FieldType::UInt,
		"what the channel list names this bouquet by, counted from one"),
	HTTPD_MEMBER("name", FieldType::String, "what the box calls the bouquet"),
	HTTPD_MEMBER("hidden", FieldType::Bool, "whether the box leaves it out of the lists it draws"),
	HTTPD_MEMBER("locked", FieldType::Bool,
		"whether the box asks for the parental code before it opens it"),
	HTTPD_MEMBER("user_bouquet", FieldType::Bool,
		"whether somebody made it here rather than the provider sending it"),
	HTTPD_MEMBER("tv_count", FieldType::UInt, "how many television services it holds"),
	HTTPD_MEMBER("radio_count", FieldType::UInt, "how many radio services it holds"),
};

const Schema kBouquetSchema = { "bouquet", HTTPD_FIELDS(kBouquetFields) };

const FieldDesc kBouquetListFields[] = {
	HTTPD_LIST_OF("items", &kBouquetSchema, "every bouquet the box holds, in its own order"),
};

const Schema kBouquetListSchema = { "bouquet-list", HTTPD_FIELDS(kBouquetListFields) };

const FieldDesc kLogoFields[] = {
	HTTPD_MEMBER("id", FieldType::ChannelId, "what every route here names this channel by, hexadecimal"),
	HTTPD_MEMBER("short_id", FieldType::ChannelId,
		"the lower forty eight bits of that, hexadecimal, which is what the box names a picture file after"),
	HTTPD_MEMBER("name", FieldType::String, "what the box calls the channel"),
	HTTPD_MEMBER("path", FieldType::String,
		"where the picture is, empty both for a channel that has none and for a listing that was asked not to look"),
	HTTPD_MEMBER("resolved", FieldType::String,
		"where that name leads when it is a link, empty when it is not one"),
};

const Schema kLogoSchema = { "logo", HTTPD_FIELDS(kLogoFields) };

const FieldDesc kLogoListFields[] = {
	HTTPD_LIST_OF("items", &kLogoSchema,
		"every channel of the half that was asked for, in the order the box holds them"),
};

const Schema kLogoListSchema = { "logo-list", HTTPD_FIELDS(kLogoListFields) };

/* The number apart from the name, which the copied endpoint did not do: that one
   printed the two into one string. The number is what the stream said and the
   name is what this box calls it. */
const FieldDesc kCaidFields[] = {
	HTTPD_MEMBER("caid", FieldType::UInt, "the identifier the stream carries"),
	HTTPD_MEMBER("system", FieldType::String,
		"what this box calls the system that identifier belongs to, empty for one it has no name for"),
};

const Schema kCaidSchema = { "ca-system", HTTPD_FIELDS(kCaidFields) };

const FieldDesc kCryptListFields[] = {
	HTTPD_LIST_OF("items", &kCaidSchema,
		"every system the running channel is scrambled under, and empty for one that is not scrambled"),
};

const Schema kCryptListSchema = { "crypt", HTTPD_FIELDS(kCryptListFields) };

/* One channel, every member every time.

   The shape does not follow the kind. A member there for a webtv channel and
   absent for a tuned one would make a reader learn which kinds carry which before
   it could read any of them, and an empty url is a true answer for a channel that
   has none. */
void appendChannelMembers(Json &j, const coreapi::ChannelInfo &c)
{
	char id[24];

	j.key("id");
	j.value(hexId(c.id, id));
	j.key("epg_id");
	j.value(hexId(c.epg_id, id));
	j.key("number");
	j.value((int) c.number);
	j.key("name");
	j.value(c.name);
	j.key("url");
	j.value(c.url);
	j.key("service_id");
	j.value((unsigned) c.service_id);
	j.key("transport_stream_id");
	j.value((unsigned) c.transport_stream_id);
	j.key("original_network_id");
	j.value((unsigned) c.original_network_id);
	j.key("satellite_position");
	j.value((int) c.satellite_position);
	j.key("freq_id");
	j.value((unsigned) c.freq_id);
	j.key("kind");
	j.value(kindName(c.kind));
	j.key("scrambled");
	j.value(c.scrambled);
	j.key("locked");
	j.value(c.locked);
}

void appendChannel(Json &j, const coreapi::ChannelInfo &c)
{
	j.beginObject();
	appendChannelMembers(j, c);
	j.endObject();
}

/* The same channel and what the box knows about its streams.

   The two are written together and never one without the other, because
   streams_known is what says how to read the two members above it: a track list
   arriving without it cannot be told apart from a channel that carries no sound. */
void appendChannelDetail(Json &j, const coreapi::ChannelInfo &c,
                         const coreapi::ChannelStreams &s)
{
	j.beginObject();
	appendChannelMembers(j, c);

	j.key("video");
	j.beginObject();
	j.key("codec");
	j.value(videoCodecName(s.video_codec));
	j.key("pid");
	j.value((unsigned) s.video_pid);
	j.endObject();

	j.key("audio");
	j.beginArray();
	for (size_t i = 0; i < s.audio.size(); ++i)
	{
		j.beginObject();
		j.key("pid");
		j.value((unsigned) s.audio[i].pid);
		j.key("codec");
		j.value(audioCodecName(s.audio[i].codec));
		j.key("description");
		j.value(s.audio[i].description);
		j.key("selected");
		j.value(s.audio[i].selected);
		j.endObject();
	}
	j.endArray();

	j.key("streams_known");
	j.value(s.known);
	j.endObject();
}

/* What the box knows about one channel's streams, and nothing known where it
   could not be asked.

   A refusal is not passed on. The channel has been read already, so what is left
   to fail here is the channel going away between the two reads, and nothing known
   is what that is: true whether the reason is a reload or a channel nobody has
   ever switched to. */
coreapi::ChannelStreams streamsOf(coreapi::ChannelId id)
{
	coreapi::Result<coreapi::ChannelStreams> got = coreapi::channels::streams(id);
	if (!got.ok())
		return coreapi::ChannelStreams();
	return std::move(got).value();
}

/* The list with any id it carries twice reduced to the first item carrying it.

   Not tidying. A cursor names an id, so an id in the list twice names two places
   the next page could begin, and the page beginning at the first of them can be
   the page whose last item is that same id again: the cursor then comes back
   unchanged and the walk runs for ever without advancing. Measured on a list of
   twelve where one page ended on a repeated id: fourteen pages answered, the same
   two items each time, and two thirds of the list never seen.

   Reducing rather than refusing, because an id is the whole of what this route
   lets a caller address: one route answers one channel for an id and it answers
   the first that carries it. Keeping the first is what makes the two routes
   agree.

   The box does not hand such a list over: the service manager files its channels
   in a map under the id. A bouquet is two vectors, and one written by hand can.
   The scan is a sorted copy of the identifiers, eight bytes an entry and one
   sort, and it ends there for every list that is already what it should be.

   Every guarantee below rests on this having run. */
void dropRepeatedIds(coreapi::ChannelList &all)
{
	if (all.size() < 2)
		return;

	std::vector<coreapi::ChannelId> sorted;
	sorted.reserve(all.size());
	for (size_t i = 0; i < all.size(); ++i)
		sorted.push_back(all[i].id);
	std::sort(sorted.begin(), sorted.end());

	bool repeated = false;
	for (size_t i = 1; i < sorted.size() && !repeated; ++i)
		repeated = sorted[i] == sorted[i - 1];
	// Which is every list the box itself builds, and the walk below is not
	// entered for one.
	if (!repeated)
		return;

	// Only the identifiers that are there more than once are looked for here,
	// and there are few of them wherever there are any at all.
	std::vector<coreapi::ChannelId> taken;
	coreapi::ChannelList kept;
	kept.reserve(all.size());
	for (size_t i = 0; i < all.size(); ++i)
	{
		const bool once =
			(std::upper_bound(sorted.begin(), sorted.end(), all[i].id) -
			 std::lower_bound(sorted.begin(), sorted.end(), all[i].id)) == 1;
		if (!once)
		{
			if (std::find(taken.begin(), taken.end(), all[i].id) != taken.end())
				continue;
			taken.push_back(all[i].id);
		}
		kept.push_back(all[i]);
	}
	all.swap(kept);
}

/* Where this page begins and ends in the list that was read.

   The page is the items after the one the cursor names, in list order. The cursor
   is that item's id and not a position in the list, because a position moves when
   the box reloads its channels under a caller walking them: a page after an offset
   would skip or repeat whatever the reload shifted past it, and neither the caller
   nor this server could see that it had.

   A cursor naming an id the list does not hold is refused. Starting again from the
   beginning is what the list would otherwise do, and a caller that walks pages
   until the cursor comes back empty would then walk them forever.

   What the walk promises: no item is answered on two pages, none between two pages
   is passed over, and the walk ends. It rests on the list handed here holding each
   id once, which is what the reduction above is for. */
bool pageBounds(const coreapi::ChannelList &all, bool have_cursor, coreapi::ChannelId cursor,
                size_t limit, size_t &begin, size_t &end, Response &refusal)
{
	begin = 0;
	if (have_cursor)
	{
		bool found = false;
		for (size_t i = 0; i < all.size() && !found; ++i)
		{
			if (all[i].id != cursor)
				continue;
			begin = i + 1;
			found = true;
		}
		if (!found)
		{
			refusal = problemResponse(StatusBadRequest, coreapi::ErrorCode::NoSuchChannel,
			                          "the cursor names a channel this list does not hold");
			return false;
		}
	}

	end = begin + limit;
	if (end > all.size())
		end = all.size();
	return true;
}

Response listChannels(const Request &r)
{
	// Absent reads as television, which is the list a caller that says nothing
	// means: the one the box opens on.
	const bool radio = r.has("mode") && r.asString("mode") == "radio";
	const size_t limit = r.has("limit") ? (size_t) r.asUInt("limit") : (size_t) kDefaultPage;
	const bool by_bouquet = r.has("bouquet");

	coreapi::Result<coreapi::ChannelList> got =
		by_bouquet ? coreapi::channels::bouquetChannels((uint32_t) r.asUInt("bouquet"))
		           : coreapi::channels::list(!radio);
	if (!got.ok())
		return problemFor(got.error());

	coreapi::ChannelList all = std::move(got).value();

	/* A bouquet answers its television and its radio members as one list, so the
	   mode narrows it here. The whole list read above is narrowed by the layer
	   below instead, and both narrow it the same way. */
	if (by_bouquet)
	{
		coreapi::ChannelList kept;
		kept.reserve(all.size());
		for (size_t i = 0; i < all.size(); ++i)
		{
			if (isRadio(all[i].kind) == radio)
				kept.push_back(all[i]);
		}
		all.swap(kept);
	}

	// Before anything is paged, because everything the paging promises rests on
	// one id naming one item.
	dropRepeatedIds(all);

	size_t begin = 0;
	size_t end = 0;
	Response refusal;
	if (!pageBounds(all, r.has("cursor"), r.asChannelId("cursor"), limit, begin, end, refusal))
		return refusal;

	Response out = okJson();
	Json j(out.body, 64 + 240 * (end - begin));
	j.beginObject();
	j.key("items");
	j.beginArray();
	for (size_t i = begin; i < end; ++i)
		appendChannel(j, all[i]);
	j.endArray();

	/* Left out on the last page rather than answered empty. An empty value is an
	   absent one to every parameter this server reads, so an empty cursor handed
	   back is no cursor at all and would be answered with the first page again. */
	if (end < all.size())
	{
		char cursor[24];
		j.key("next_cursor");
		j.value(hexId(all[end - 1].id, cursor));
	}
	j.endObject();
	return out;
}

Response getChannel(const Request &r)
{
	coreapi::Result<coreapi::ChannelInfo> got = coreapi::channels::get(r.asChannelId("id"));
	if (!got.ok())
		return problemFor(got.error());

	Response out = okJson();
	Json j(out.body, 512);
	appendChannelDetail(j, got.value(), streamsOf(got.value().id));
	return out;
}

Response currentChannel(const Request &)
{
	/* An idle box answers that there is nothing playing, and it is an answer
	   rather than a fault: the layer below has already turned it into a refusal
	   that carries a code. The old server dereferenced the running channel
	   without asking whether there was one. */
	coreapi::Result<coreapi::ChannelInfo> got = coreapi::channels::current();
	if (!got.ok())
		return problemFor(got.error());

	Response out = okJson();
	Json j(out.body, 512);
	appendChannelDetail(j, got.value(), streamsOf(got.value().id));
	return out;
}

/* WHETHER ONE BOUQUET HOLDS ONE CHANNEL.

   Both halves of it, because a bouquet is one thing to whoever made it and what
   the channel stack answers for a bouquet is its television and its radio members
   together.

   A bouquet the box refuses to answer for is a bouquet that does not hold the
   channel as far as this is concerned. The alternative is to refuse the whole
   listing over one bouquet the caller did not ask about. */
bool bouquetHolds(uint32_t bouquet, coreapi::ChannelId id)
{
	coreapi::Result<coreapi::ChannelList> got = coreapi::channels::bouquetChannels(bouquet);
	if (!got.ok())
		return false;

	const coreapi::ChannelList &members = got.value();
	for (size_t i = 0; i < members.size(); ++i)
		if (members[i].id == id)
			return true;
	return false;
}

Response listBouquets(const Request &r)
{
	coreapi::Result<coreapi::BouquetList> got = coreapi::channels::bouquets();
	if (!got.ok())
		return problemFor(got.error());

	const coreapi::BouquetList all = std::move(got).value();

	/* Narrowed to the bouquets that hold one channel, for the caller that has a
	   channel and wants the bouquet to open on. It costs a read of every
	   bouquet's members, which is why it is asked for by name. The other way
	   round, a caller working this out for itself would be one request per
	   bouquet, and a box carries tens of them. */
	const bool narrowed = r.has("holds");
	const coreapi::ChannelId wanted = narrowed ? r.asChannelId("holds") : 0;

	Response out = okJson();
	Json j(out.body, 32 + 128 * all.size());
	j.beginObject();
	j.key("items");
	j.beginArray();
	for (size_t i = 0; i < all.size(); ++i)
	{
		const coreapi::BouquetInfo &b = all[i];
		if (narrowed && !bouquetHolds(b.id, wanted))
			continue;
		j.beginObject();
		/* The bouquet's own number and never where it sits in this answer. An
		   index would name a different bouquet the moment one before it is
		   hidden or emptied, which is what made an empty bouquet consume a
		   position in the old server. */
		j.key("id");
		j.value((unsigned long) b.id);
		j.key("name");
		j.value(b.name);
		j.key("hidden");
		j.value(b.hidden);
		j.key("locked");
		j.value(b.locked);
		j.key("user_bouquet");
		j.value(b.user_bouquet);
		j.key("tv_count");
		j.value((unsigned long) b.tv_count);
		j.key("radio_count");
		j.value((unsigned long) b.radio_count);
		j.endObject();
	}
	j.endArray();
	j.endObject();
	return out;
}

/* What a picture goes out as, taken from the end of the name the box's own search
   picked. Four of them, because those are the four that search looks for and in
   this order; anything else is handed over as bytes rather than called a picture.

   Compared as written, without folding case, because every name the search builds
   carries one of these four spellings. */
struct PictureType
{
	const char *suffix;
	const char *media;
};

const PictureType kPictureTypes[] = {
	{ ".svg", "image/svg+xml" },
	{ ".png", "image/png" },
	{ ".jpg", "image/jpeg" },
	{ ".gif", "image/gif" },
};

const char *pictureTypeFor(const std::string &path)
{
	for (size_t i = 0; i < sizeof(kPictureTypes) / sizeof(kPictureTypes[0]); ++i)
	{
		const size_t n = std::strlen(kPictureTypes[i].suffix);
		if (path.size() >= n &&
		    path.compare(path.size() - n, n, kPictureTypes[i].suffix) == 0)
			return kPictureTypes[i].media;
	}
	return "application/octet-stream";
}

/* The picture itself, sent out of the open file. The transport sends its bytes
   from the kernel to the socket and gives the descriptor back when the answer is
   done with.

   Three things are not what the picture of the box's own screen does. The name is
   not taken away after the open: that one is a file this request made, and this
   one belongs to the box and has to still be there for the next request. The type
   is the one the name ends in. And the open does not refuse a link, which every
   other file this server hands out does refuse: here a link is the ordinary case.

   Opened once and measured off that descriptor, never opened a second time by
   name. The directory a picture sits in is writable on a box, so between the
   search that found this name and this open somebody can put another file there.

   A name that will not open is answered as a channel with no picture and
   deliberately not as a fault of its own: the search found it a moment ago. */
Response logoAt(const std::string &path)
{
	const int fd = ::open(path.c_str(), O_RDONLY | O_CLOEXEC);
	if (fd < 0)
		return problemResponse(StatusNotFound, coreapi::ErrorCode::NoSuchLogo,
		                       "the picture the box named could not be read");

	Response out;
	out.code = StatusOk;
	out.content_type = pictureTypeFor(path);
	// The descriptor is taken over whichever way this answers, so there is
	// nothing left here to give back.
	if (!answerFromDescriptor(out, fd))
		return problemResponse(StatusNotFound, coreapi::ErrorCode::NoSuchLogo,
		                       "the picture the box named could not be read");
	return out;
}

/* Refused before the box is asked, when this server is set not to hand channel
   pictures over (webconfig.h).

   HERE AND NOT IN THE PAGE. The switch is answered by the route that says how this
   server is set up, and that route is System, so the caller the switch was written
   for, a reader on the home network who presented nothing, cannot learn it and
   draws the pictures anyway. Refusing here holds for every caller.

   Not found and not forbidden. A page meets this where it already meets a channel
   that has no picture, and it does the same thing about either. The reason is
   still readable: the code beside the status is this one's own.

   Before the channel is looked up, because the switch is about this server and not
   about the channel. */
Response getLogo(const Request &r)
{
	if (!config().channel_logos)
		return problemResponse(StatusNotFound, coreapi::ErrorCode::LogosNotOffered,
				       "this server is set not to hand channel pictures over");

	coreapi::Result<std::string> found = coreapi::channels::logo(r.asChannelId("id"));
	if (!found.ok())
		return problemFor(found.error());
	return logoAt(found.value());
}

/* THE SWITCH IS NOT READ HERE, and that is a decision rather than an omission.

   What this hands over is where a picture is on this box's disc and never a
   picture, and it says nothing about one unless a caller asks it to look, which
   the pages never do.

   What it could do with the switch off is empty the two members that say where a
   picture is, and that would read as a box with no pictures, where the truth is a
   server that will not hand them over. This document has no member that could say
   the second. */
Response listLogos(const Request &r)
{
	// Absent reads as television, the same way the channel listing reads it.
	const bool radio = r.has("mode") && r.asString("mode") == "radio";
	// Absent reads as not looking, because looking is the expensive half and a
	// caller that wants it pays for it on purpose.
	const bool files = r.has("files") && r.asBool("files");

	coreapi::Result<coreapi::LogoList> got = coreapi::channels::logos(!radio, files);
	if (!got.ok())
		return problemFor(got.error());

	const coreapi::LogoList all = std::move(got).value();

	Response out = okJson();
	Json j(out.body, 32 + 160 * all.size());
	j.beginObject();
	j.key("items");
	j.beginArray();
	for (size_t i = 0; i < all.size(); ++i)
	{
		char id[24];
		j.beginObject();
		j.key("id");
		j.value(hexId(all[i].id, id));
		j.key("short_id");
		j.value(hexId(all[i].short_id, id));
		j.key("name");
		j.value(all[i].name);
		j.key("path");
		j.value(all[i].path);
		j.key("resolved");
		j.value(all[i].resolved);
		j.endObject();
	}
	j.endArray();
	j.endObject();
	return out;
}

Response currentCrypt(const Request &)
{
	coreapi::Result<coreapi::CaidList> got = coreapi::channels::currentCaids();
	if (!got.ok())
		return problemFor(got.error());

	const coreapi::CaidList all = std::move(got).value();

	Response out = okJson();
	Json j(out.body, 32 + 64 * all.size());
	j.beginObject();
	j.key("items");
	j.beginArray();
	for (size_t i = 0; i < all.size(); ++i)
	{
		j.beginObject();
		j.key("caid");
		j.value((unsigned) all[i].caid);
		j.key("system");
		j.value(all[i].system);
		j.endObject();
	}
	j.endArray();
	j.endObject();
	return out;
}

Response reloadChannelLists(const Request &r)
{
	coreapi::Result<void> done =
		coreapi::channels::reloadChannels(r.has("hard") && r.asBool("hard"));
	if (!done.ok())
		return problemFor(done.error());

	/* Accepted and not done. What the box does with it runs on its own loop,
	   and what a caller sees of it is the next listing it asks for. */
	return accepted();
}

Response zap(const Request &r)
{
	/* The channel is looked up before anything is posted, which the layer below
	   does and this does not repeat: a command is posted and forgotten, so a zap
	   to an id nobody has would otherwise be accepted and change nothing. */
	coreapi::Result<void> done = coreapi::channels::zap(r.asChannelId("channel_id"));
	if (!done.ok())
		return problemFor(done.error());

	/* Accepted and not done. The message is on the box's own queue and what takes
	   it off reports nothing back, so this server has no way to have learnt that
	   the channel changed, and in standby it will not. */
	return accepted();
}

Response setMode(const Request &r)
{
	const coreapi::channels::Mode mode = (r.asString("mode") == "radio")
		? coreapi::channels::Mode::Radio
		: coreapi::channels::Mode::Tv;

	coreapi::Result<void> done = coreapi::channels::setMode(mode);
	if (!done.ok())
		return problemFor(done.error());
	return accepted();
}

/* The identifiers a caller means, out of the elements of the list it sent, spelled
   the way a channel is named everywhere else.

   The order is the caller's and is kept: it is what the bouquet holds them in
   afterwards, so a list read into a set here would throw away half of what the
   request said.

   An identifier written twice names one channel and is kept once, at the place it
   was first named: a bouquet holding a channel twice holds two entries a caller
   cannot address apart.

   False for anything that is not such a list, answered before the box is asked. */
bool readChannelIds(const std::vector<std::string> &items, coreapi::ChannelIdList &out)
{
	out.clear();
	out.reserve(items.size());
	for (size_t n = 0; n < items.size(); ++n)
	{
		const std::string &one = items[n];
		/* An element spelling nothing names no channel, and sixteen digits are
		   the whole of one: a list that let either through would turn an empty
		   string into the channel numbered nought and a longer run of digits
		   into whatever its last sixteen happened to be. */
		if (one.empty() || one.size() > 16)
			return false;

		uint64_t id = 0;
		for (size_t i = 0; i < one.size(); ++i)
		{
			const char c = one[i];
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

		const coreapi::ChannelId named = (coreapi::ChannelId) id;
		if (std::find(out.begin(), out.end(), named) == out.end())
			out.push_back(named);
	}
	return true;
}

/* The seven below change the bouquet list. Each names its bouquet by the name the
   listing answers with, because a position is what the box renumbers the moment
   anything before it is taken away or moved, and each writes the list out and
   reads it back through the layer under this one.

   The name arrives as one path segment and is decoded after the path has been
   split, so a name carrying a separator or a percent reaches here whole.

   None of them answers a document: what a bouquet is now is what the listing
   says. */

Response makeBouquet(const Request &r)
{
	coreapi::Result<void> done = coreapi::channels::addBouquet(r.asString("name"));
	if (!done.ok())
		return problemFor(done.error());

	/* It exists, and there is nothing to name that the caller does not have: the
	   name is the caller's own and is the whole of what addresses it. No Location,
	   because the only one that could go there is that name escaped again. */
	return created();
}

Response removeBouquet(const Request &r)
{
	coreapi::Result<void> done = coreapi::channels::deleteBouquet(r.asString("bouquet"));
	if (!done.ok())
		return problemFor(done.error());
	return noContent();
}

Response renameBouquet(const Request &r)
{
	// The key moves with the name, so the path this arrived on names nothing
	// afterwards. That is stated in the summary of the route rather than
	// softened here.
	coreapi::Result<void> done =
		coreapi::channels::renameBouquet(r.asString("bouquet"), r.asString("name"));
	if (!done.ok())
		return problemFor(done.error());
	return noContent();
}

Response moveBouquet(const Request &r)
{
	const coreapi::channels::Direction where = (r.asString("direction") == "up")
		? coreapi::channels::Direction::Up
		: coreapi::channels::Direction::Down;

	coreapi::Result<void> done = coreapi::channels::moveBouquet(r.asString("bouquet"), where);
	if (!done.ok())
		return problemFor(done.error());
	return noContent();
}

Response hideBouquet(const Request &r)
{
	coreapi::Result<void> done =
		coreapi::channels::setBouquetHidden(r.asString("bouquet"), r.asBool("on"));
	if (!done.ok())
		return problemFor(done.error());
	return noContent();
}

Response lockBouquet(const Request &r)
{
	coreapi::Result<void> done =
		coreapi::channels::setBouquetLock(r.asString("bouquet"), r.asBool("on"));
	if (!done.ok())
		return problemFor(done.error());
	return noContent();
}

/* The most channels one half of one bouquet takes in one request.

   Counted in channels and not in bytes, because the ceiling bounds the work: the
   call under this one walks the list against what the half holds and then puts
   each channel where the list says, both of which are the length of the list
   squared in the worst case. Four thousand squared is work the slowest box in the
   set finishes in a fraction of a second, and ten times that is not.

   Above every bouquet a box carries: a satellite's whole provider list is under
   two thousand services.

   The bytes are bounded elsewhere and far above this: four thousand identifiers
   written as an array is about eighty kilobytes of the megabyte the transport
   allows. This is the number that answers first. */
const size_t kMaxChannelsInBouquet = 4096;

Response fillBouquet(const Request &r)
{
	/* The list is the body and not a value inside one. A body member is held to
	   the ceiling every value of this server is held to, and four kilobytes is
	   about two hundred and forty identifiers, which is a limit on how big a
	   bouquet may be that nobody chose and nothing states.

	   Read here rather than bound by the router, because the router binds values
	   and a list is not one; the row beside the bouquet declares that the body is
	   a list of identifiers. */
	std::vector<std::string> items;
	const ArrayRead read = readStringArray(r.body(), kMaxChannelsInBouquet, items);
	if (read == ArrayRead::TooMany)
	{
		// The number out of the constant and not written again here, a sentence
		// carrying a ceiling somebody kept in step by hand being one that is
		// right until the ceiling moves.
		char most[32];
		std::snprintf(most, sizeof(most), "%lu", (unsigned long) kMaxChannelsInBouquet);
		return problemResponse(StatusBadRequest, coreapi::ErrorCode::TooManyChannels,
		                       std::string("one half of a bouquet takes at most ") + most +
		                       " channels");
	}
	if (read != ArrayRead::Ok)
		return problemResponse(StatusBadRequest, coreapi::ErrorCode::BadString,
		                       "the body is not one array of hexadecimal channel identifiers");

	/* A list of nothing empties that half, and is the one thing a caller could not
	   say here before. It cannot be confused with a request built wrong: the body
	   is the list, so a request that forgot it carries no array at all, and which
	   half the list is for is a parameter the route requires.

	   Worth saying what it costs, because the box forgets a channel no bouquet
	   holds any more: emptying the last half that held a channel takes the channel
	   off the box. A list going from five names to one takes four out the same
	   way. */

	coreapi::ChannelIdList ids;
	if (!readChannelIds(items, ids))
		return problemResponse(StatusBadRequest, coreapi::ErrorCode::BadInt,
		                       "the body is not one array of hexadecimal channel identifiers");

	const coreapi::channels::Mode kind = (r.asString("mode") == "radio")
		? coreapi::channels::Mode::Radio
		: coreapi::channels::Mode::Tv;

	coreapi::Result<void> done =
		coreapi::channels::setBouquetChannels(r.asString("bouquet"), ids, kind);
	if (!done.ok())
		return problemFor(done.error());
	return noContent();
}

const Param kListParams[] = {
	HTTPD_QUERY_FROM_SET("mode", "which of the two lists to answer, television when it is left out",
		"tv,radio"),
	/* Bounded, and the bound is what makes the narrowing below it safe rather than
	   the cast. The value is handed on as a thirty two bit number, the accessor
	   answers an unsigned long, and those are the same width on the box and not on
	   the machine this is built on, so a row leaving both ends at nought was
	   checked by nothing wherever they differ: two to the thirty two plus three
	   arrived as three and answered with that bouquet's channels. */
	HTTPD_QUERY_IN("bouquet", ParamType::UInt, "answer this bouquet's members rather than the whole list, counted from one", 1,
		kMaxBouquetId),
	HTTPD_QUERY("cursor", ParamType::ChannelId, "the id of the last item of the page before this one"),
	HTTPD_QUERY_IN("limit", ParamType::UInt, "how many at most, one hundred when it is left out", 1,
		kMaxPage),
};

const Param kBouquetListParams[] = {
	HTTPD_QUERY("holds", ParamType::ChannelId,
		"answer only the bouquets that hold this channel, in the box's own order, hexadecimal; it costs a read of every bouquet's members"),
};

const Param kOneParams[] = {
	HTTPD_SEGMENT("id", ParamType::ChannelId, "the channel, hexadecimal"),
};

const Param kLogoParams[] = {
	HTTPD_SEGMENT("id", ParamType::ChannelId, "the channel, hexadecimal"),
};

const Param kLogoListParams[] = {
	HTTPD_QUERY_FROM_SET("mode", "which of the two lists to answer, television when it is left out",
		"tv,radio"),
	HTTPD_QUERY("files", ParamType::Bool,
		"whether to look for the file as well, which costs a walk of the picture directories for every channel"),
};

const Param kReloadParams[] = {
	HTTPD_BODY("hard", ParamType::Bool,
		"whether to leave the services the box is holding unwritten, so that what is on the disc wins over what is in memory"),
};

const Param kZapParams[] = {
	HTTPD_BODY_REQUIRED("channel_id", ParamType::ChannelId, "the channel to play, hexadecimal"),
};

const Param kModeParams[] = {
	HTTPD_BODY_REQUIRED_FROM_SET("mode", "which of the two lists the box is to be in", "tv,radio"),
};

/* A bouquet's name, bounded because it is a name and not a document. The ceiling
   is the same wherever the name is written, in the path and in a body, so a name
   this server accepts is one it can be asked about again. */
const long kMaxBouquetName = 255;

const Param kNewBouquetParams[] = {
	HTTPD_BODY_REQUIRED_TEXT("name", "what to call it", kMaxBouquetName),
};

const Param kBouquetParams[] = {
	HTTPD_SEGMENT_TEXT("bouquet", "the bouquet, by its name", kMaxBouquetName),
};

const Param kRenameParams[] = {
	HTTPD_SEGMENT_TEXT("bouquet", "the bouquet, by the name it carries now", kMaxBouquetName),
	HTTPD_BODY_REQUIRED_TEXT("name", "what to call it instead", kMaxBouquetName),
};

const Param kMoveParams[] = {
	HTTPD_SEGMENT_TEXT("bouquet", "the bouquet, by its name", kMaxBouquetName),
	HTTPD_BODY_REQUIRED_FROM_SET("direction", "which way, one place at a time", "up,down"),
};

const Param kHiddenParams[] = {
	HTTPD_SEGMENT_TEXT("bouquet", "the bouquet, by its name", kMaxBouquetName),
	HTTPD_BODY_REQUIRED("on", ParamType::Bool, "whether the box leaves it out of the lists it draws"),
};

const Param kLockedParams[] = {
	HTTPD_SEGMENT_TEXT("bouquet", "the bouquet, by its name", kMaxBouquetName),
	HTTPD_BODY_REQUIRED("on", ParamType::Bool,
		"whether the box asks for the parental code before it opens it"),
};

/* The body is the list, and the third row is the one that says so. Which half of
   the bouquet the list is for goes in the query and not in the body because it
   names what the request is addressed to rather than what it carries.

   Required, and not defaulted to television: a client that left it out would be
   told it wrote half a request, rather than having the television half of a
   bouquet replaced while it meant the radio one.

   The list is declared and not left to the summary. What the document tells a
   caller to send is written out of the rows and out of nothing else, and a page
   drawn from a document that describes no body shows no field to type one into.

   ChannelId because that is what one element is, and what one looks like on the
   wire comes out of the same writer that describes an identifier anywhere else
   here.

   The two numbers count channels and not characters, which is what a row naming
   the whole of a body counts. The floor is nought because an empty list means the
   half holds nothing. */
const Param kFillParams[] = {
	HTTPD_SEGMENT_TEXT("bouquet", "the bouquet, by its name", kMaxBouquetName),
	HTTPD_QUERY_REQUIRED_FROM_SET("mode", "which half of the bouquet the body is", "tv,radio"),
	HTTPD_BODY_IS_LIST_OF("channels", ParamType::ChannelId,
		"the channels this half is to hold, hexadecimal, in the order they are to be in, and none of them to empty it",
		0, (long) kMaxChannelsInBouquet),
};

/* Every route that reads asks for the least a route may ask for and is reached by
   a caller the network exemption already grants that much. The two that change
   something ask for a write, which is what the check over these tables holds them
   to: a caller on the box's own network is granted a read without presenting
   anything, so a route that changed the channel at that level would be one any
   page a browser on that network visits could reach in that browser's name. */
const Endpoint kChannelEndpoints[] = {
	{ Method::Get, "/api/v1/channels", AuthLevel::Read,
	  "the channels the box holds, a page at a time",
	  HTTPD_PARAMS(kListParams), &kChannelPageSchema, &listChannels, false },
	{ Method::Get, "/api/v1/channels/current", AuthLevel::Read,
	  "the channel the box is playing, with what its streams are",
	  NULL, 0, &kChannelDetailSchema, &currentChannel, false },
	{ Method::Get, "/api/v1/channels/logos", AuthLevel::Read,
	  "every channel of one list with what is known about the picture it is shown with",
	  HTTPD_PARAMS(kLogoListParams), &kLogoListSchema, &listLogos, false },
	{ Method::Get, "/api/v1/channels/current/crypt", AuthLevel::Read,
	  "which conditional access systems the running channel is scrambled under",
	  NULL, 0, &kCryptListSchema, &currentCrypt, false },
	{ Method::Get, "/api/v1/channels/{id}", AuthLevel::Read,
	  "one channel by its identifier, with what its streams are where the box has read them",
	  HTTPD_PARAMS(kOneParams), &kChannelDetailSchema, &getChannel, false },
	{ Method::Get, "/api/v1/channels/{id}/logo", AuthLevel::Read,
	  "the picture the box shows that channel with, as the file it is",
	  HTTPD_PARAMS(kLogoParams), NULL, &getLogo, false },
	{ Method::Get, "/api/v1/bouquets", AuthLevel::Read,
	  "the bouquets the box holds, or only those holding one channel",
	  HTTPD_PARAMS(kBouquetListParams), &kBouquetListSchema, &listBouquets, false },
	{ Method::Post, "/api/v1/zap", AuthLevel::Write,
	  "asks the box to play one channel",
	  HTTPD_PARAMS(kZapParams), NULL, &zap, false },
	{ Method::Post, "/api/v1/mode", AuthLevel::Write,
	  "asks the box to change between television and radio",
	  HTTPD_PARAMS(kModeParams), NULL, &setMode, false },
	{ Method::Post, "/api/v1/bouquets", AuthLevel::Write,
	  "makes a bouquet with nothing in it",
	  HTTPD_PARAMS(kNewBouquetParams), NULL, &makeBouquet, false },
	{ Method::Delete, "/api/v1/bouquets/{bouquet}", AuthLevel::Write,
	  "takes a bouquet away, with whatever it holds",
	  HTTPD_PARAMS(kBouquetParams), NULL, &removeBouquet, false },
	{ Method::Put, "/api/v1/bouquets/{bouquet}/name", AuthLevel::Write,
	  "renames it, which moves the name every other route here addresses it by",
	  HTTPD_PARAMS(kRenameParams), NULL, &renameBouquet, false },
	{ Method::Put, "/api/v1/bouquets/{bouquet}/position", AuthLevel::Write,
	  "moves it one place up or down the list, and refuses at either end",
	  HTTPD_PARAMS(kMoveParams), NULL, &moveBouquet, false },
	{ Method::Put, "/api/v1/bouquets/{bouquet}/hidden", AuthLevel::Write,
	  "says whether the box leaves it out of the lists it draws",
	  HTTPD_PARAMS(kHiddenParams), NULL, &hideBouquet, false },
	{ Method::Put, "/api/v1/bouquets/{bouquet}/locked", AuthLevel::Write,
	  "says whether the box asks for the parental code before it opens it",
	  HTTPD_PARAMS(kLockedParams), NULL, &lockBouquet, false },
	{ Method::Post, "/api/v1/channels/reload", AuthLevel::Write,
	  "asks the box to read its channel lists again",
	  HTTPD_PARAMS(kReloadParams), NULL, &reloadChannelLists, false },
	{ Method::Put, "/api/v1/bouquets/{bouquet}/channels", AuthLevel::Write,
	  "replaces the television or the radio members of it with the channels the body names, in the order it names them, and empties that half for a body naming none; the body is a JSON array of hexadecimal channel identifiers",
	  HTTPD_PARAMS(kFillParams), NULL, &fillBouquet, false },
};

} // namespace

// The pair, written where the array is, so the length beside it is the length of
// the array and not a number somebody kept in step by hand.
extern const RouteTable channelsTable = {
	HTTPD_TABLE("channels", kChannelEndpoints)
};

} // namespace httpd
