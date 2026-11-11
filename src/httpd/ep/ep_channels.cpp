/*
 * ep_channels.cpp - routes for channels and bouquets
 *
 * Copyright (C) 2026 NI-Team
 * Copyright (C) 2026 Tuxbox-Project (additions, fixes)
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
#include "coreapi/websources.h"
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

const char kVideoCodecDocs[] =
	"unknown: the box has not read the channel's streams yet, or found a coding it has no name for\n"
	"none: the service carries no picture, as a radio service does\n"
	"mpeg2: MPEG-2 video\n"
	"h264: H.264 (AVC)\n"
	"hevc: H.265 (HEVC)\n"
	"cavs: Chinese AVS video";

const char kHintSourceValues[] = "none,stream,service_type";

const char kHintSourceDocs[] =
	"none: no hint, and video_codec_hint is unknown\n"
	"stream: read off the channel, which the box does when it plays it, or written into its service list by a scan with the box's pid option\n"
	"service_type: named by the DVB service type the broadcaster signals (ETSI EN 300 468): 0x01 and 0x11 mpeg2, "
	"0x16, 0x19 and 0x1C h264, 0x1F and 0x20 hevc, 0x02 and 0x0A none; 0x01 is the generic television type, "
	"used for MPEG-2 above all but allowed for other codings, so mpeg2 from it is a guess";

const char kAudioCodecDocs[] =
	"unknown: a track the box found and could not classify\n"
	"mp2: MPEG-1 audio layer II\n"
	"ac3: Dolby Digital (AC-3)\n"
	"eac3: Dolby Digital Plus (E-AC-3)\n"
	"aac: AAC (Advanced Audio Coding)\n"
	"aacplus: HE-AAC (AAC+)\n"
	"dts: DTS (Digital Theater Systems)\n"
	"dtshd: DTS-HD (high resolution DTS)\n"
	"lpcm: uncompressed linear PCM";

const char kListModeDocs[] =
	"tv: the television list, which television and WebTV channels are in (the default)\n"
	"radio: the radio list, which radio and web radio channels are in";

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

const char *hintSourceName(coreapi::CodecHintSource s)
{
	switch (s)
	{
		case coreapi::CodecHintSource::None:        return "none";
		case coreapi::CodecHintSource::Stream:      return "stream";
		case coreapi::CodecHintSource::ServiceType: return "service_type";
	}
	return "none";
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
		"the coding of the picture stream, as the box read it from the channel", kVideoCodecDocs),
	HTTPD_MEMBER("pid", FieldType::UInt,
		"the packet id (PID, decimal) of the picture stream in the transport stream, and 0 where the box knows none"),
};

const Schema kVideoSchema = { "video-stream", HTTPD_FIELDS(kVideoFields) };

const FieldDesc kStreamPidsFields[] = {
	HTTPD_MEMBER("pmt", FieldType::UInt,
		"the packet id (PID, decimal) of the programme map table, which lists the channel's streams, and 0 where the box knows none"),
	HTTPD_MEMBER("pcr", FieldType::UInt,
		"the packet id (PID, decimal) carrying the clock reference the decoders keep time by, often the picture's own, and 0 where the box knows none"),
	HTTPD_MEMBER("teletext", FieldType::UInt,
		"the packet id (PID, decimal) of the teletext stream, and 0 where the box knows none or the channel carries none"),
};

const Schema kStreamPidsSchema = { "stream-pids", HTTPD_FIELDS(kStreamPidsFields) };

const FieldDesc kAudioTrackFields[] = {
	HTTPD_MEMBER("pid", FieldType::UInt,
		"the packet id (PID, decimal) of this sound track in the transport stream"),
	HTTPD_MEMBER_OF_SET("codec", kAudioCodecValues,
		"the coding of this sound track, as the box read it from the channel", kAudioCodecDocs),
	HTTPD_MEMBER("description", FieldType::String,
		"what the stream says the track is, usually a 3 letter ISO 639 language code such as `deu`, not translated here"),
	HTTPD_MEMBER("selected", FieldType::Bool,
		"whether this is the channel's selected track, the one the box plays while the channel is playing, which no track is while the box has not read the channel's streams"),
};

const Schema kAudioTrackSchema = { "audio-track", HTTPD_FIELDS(kAudioTrackFields) };

/* The members every channel answer carries. Written once and shared, because two
   shapes are answered here: the listing carries these, and the two routes that
   answer one channel carry these and what its streams are. */
#define HTTPD_CHANNEL_MEMBERS \
	HTTPD_MEMBER("id", FieldType::ChannelId, \
		"the channel id every route here names this channel by, hexadecimal, up to 16 digits"), \
	HTTPD_MEMBER("epg_id", FieldType::ChannelId, \
		"the id the guide keeps this channel's schedule under, hexadecimal; another channel's id wherever 2 channels share 1 schedule. The guide routes look it up themselves, so callers pass `id` to them, never this"), \
	HTTPD_MEMBER("number", FieldType::Int, \
		"the number the box shows beside the channel in its list, and 0 or less where it shows none"), \
	HTTPD_MEMBER("name", FieldType::String, "the channel's name as the box shows it, which is not unique"), \
	HTTPD_MEMBER("url", FieldType::String, \
		"where a channel played without a tuner is fetched from, empty for a channel that is tuned"), \
	HTTPD_MEMBER("service_id", FieldType::UInt, \
		"the DVB service id of the channel inside its transport stream, decimal"), \
	HTTPD_MEMBER("transport_stream_id", FieldType::UInt, \
		"the DVB transport stream id of the transponder that carries it, decimal"), \
	HTTPD_MEMBER("original_network_id", FieldType::UInt, \
		"the DVB original network id of that transport stream, decimal"), \
	HTTPD_MEMBER("satellite_position", FieldType::Int, \
		"the orbital position in tenths of a degree, east positive and west negative (192 is 19.2 degrees east); cable sources are numbered from 3840 (0xF00) and terrestrial ones from 3584 (0xE00), and 0 is a channel received without a tuner"), \
	HTTPD_MEMBER("freq_id", FieldType::UInt, \
		"the transponder key the box files it under, which is not a frequency"), \
	HTTPD_MEMBER_OF_SET("kind", "unknown,tv,radio,webtv,webradio", \
		"what sort of service the channel is and how the box receives it", \
		"unknown: a service the box has not classified\n" \
		"tv: a television service received by a tuner\n" \
		"radio: a radio service received by a tuner\n" \
		"webtv: a television channel the box plays from the address in `url`\n" \
		"webradio: a radio channel the box plays from the address in `url`"), \
	HTTPD_MEMBER("scrambled", FieldType::Bool, \
		"whether the channel is marked as scrambled, so the box needs a descrambler or softcam to show it"), \
	HTTPD_MEMBER("locked", FieldType::Bool, \
		"whether the box asks for the parental code before it plays"), \
	HTTPD_MEMBER_OF_SET("video_codec_hint", kVideoCodecValues, \
		"what the picture is most likely coded in, known before the channel is played: as read off the channel where the box has done so, else as its DVB service type names it; a hint, which `video` of the routes answering one channel settles once `streams_known` is true", \
		kVideoCodecDocs), \
	HTTPD_MEMBER_OF_SET("video_codec_hint_source", kHintSourceValues, \
		"where `video_codec_hint` comes from, so a caller can tell a coding the box read from one it guessed", kHintSourceDocs)

const FieldDesc kChannelFields[] = { HTTPD_CHANNEL_MEMBERS };

const Schema kChannelSchema = { "channel", HTTPD_FIELDS(kChannelFields) };

/* The same channel with what the box knows about its streams, which is what the
   two routes that answer a single channel carry and the listing does not.

   Not in the listing because it cannot be: the track list is read off the channel
   itself and a page of five hundred would read five hundred of them, and on a box
   nobody has switched around on it would answer nothing known for every one. */
const FieldDesc kChannelDetailFields[] = {
	HTTPD_CHANNEL_MEMBERS,
	HTTPD_OBJECT("video", &kVideoSchema,
		"the picture stream: its coding and its packet id, both unknown while `streams_known` is false unless the box's service list carries them"),
	HTTPD_LIST_OF("audio", &kAudioTrackSchema,
		"the sound tracks, in the order the stream lists them, and empty while streams_known is false"),
	HTTPD_OBJECT("pids", &kStreamPidsSchema,
		"the packet ids beside the picture and the sound; like the picture's, known for a channel the box has played or one a scan with the box's pid option wrote down"),
	/* The honest member. The box reads a channel's stream layout when it plays
	   the channel, so a channel nobody has switched to has none, and a caller
	   told nothing about that would read an empty track list as a channel with
	   no sound. What a channel scan writes down beforehand is one pid with
	   neither a codec nor a language. */
	HTTPD_MEMBER("streams_known", FieldType::Bool,
		"whether the box has read this channel's streams, which it does when it plays the channel; false leaves audio empty, and video unknown or none unless the box's service list carries it"),
};

const Schema kChannelDetailSchema = { "channel-detail", HTTPD_FIELDS(kChannelDetailFields) };

#undef HTTPD_CHANNEL_MEMBERS

const FieldDesc kChannelPageFields[] = {
	HTTPD_LIST_OF("items", &kChannelSchema,
		"the channels of this page, by the number the box shows beside each, or in the bouquet's own order where `bouquet` was given"),
	/* Absent on the last page rather than empty, and that is why it is the one
	   member here a reader has to test for. An empty value is an absent one
	   everywhere a parameter is read, so a client handing an empty cursor back
	   would be answered with the first page again. */
	HTTPD_MEMBER_OPTIONAL("next_cursor", FieldType::ChannelId,
		"the id of the last channel on this page, hexadecimal; send it as `cursor` to get the next page. Absent on the last page"),
};

const Schema kChannelPageSchema = { "channel-page", HTTPD_FIELDS(kChannelPageFields) };

const FieldDesc kBouquetFields[] = {
	HTTPD_MEMBER("id", FieldType::UInt,
		"the bouquet's position in the box's bouquet list, counted from 1; the `bouquet` of `GET /api/v1/channels` and `GET /api/v1/epg/grid`. It changes when a bouquet before it is moved or removed"),
	HTTPD_MEMBER("name", FieldType::String,
		"the bouquet's name, which the `/api/v1/bouquets/{bouquet}` routes address it by as long as no other bouquet carries it too"),
	HTTPD_MEMBER("hidden", FieldType::Bool,
		"whether the box leaves the bouquet out of the channel lists it shows on screen"),
	HTTPD_MEMBER("locked", FieldType::Bool,
		"whether the box asks for the parental code before it opens it"),
	HTTPD_MEMBER("user_bouquet", FieldType::Bool,
		"whether somebody made it here rather than the provider sending it"),
	HTTPD_MEMBER("editable", FieldType::Bool,
		"whether a change to it lasts: false for a bouquet the box builds itself, the one of the channels no other bouquet holds and those of WebTV and web radio, which the routes that change a bouquet refuse with `bouquet-not-editable`"),
	HTTPD_MEMBER("tv_count", FieldType::UInt,
		"how many television channels (tuned and WebTV) the bouquet holds"),
	HTTPD_MEMBER("radio_count", FieldType::UInt,
		"how many radio channels (tuned and web radio) the bouquet holds"),
};

const Schema kBouquetSchema = { "bouquet", HTTPD_FIELDS(kBouquetFields) };

const FieldDesc kBouquetListFields[] = {
	HTTPD_LIST_OF("items", &kBouquetSchema,
		"every bouquet the box holds, hidden ones included, in the box's own order"),
};

const Schema kBouquetListSchema = { "bouquet-list", HTTPD_FIELDS(kBouquetListFields) };

const FieldDesc kLogoFields[] = {
	HTTPD_MEMBER("id", FieldType::ChannelId,
		"the channel id every route here names this channel by, hexadecimal, up to 16 digits"),
	HTTPD_MEMBER("short_id", FieldType::ChannelId,
		"the lower 48 bits of `id`, hexadecimal, which is what the box names a picture file after"),
	HTTPD_MEMBER("name", FieldType::String,
		"the channel's name as the box shows it, which the box also tries as a picture file name"),
	HTTPD_MEMBER("path", FieldType::String,
		"where the picture is, empty both for a channel that has none and for a listing that was asked not to look"),
	HTTPD_MEMBER("resolved", FieldType::String,
		"the file `path` leads to when it is a symbolic link, and empty when it is not a link"),
};

const Schema kLogoSchema = { "logo", HTTPD_FIELDS(kLogoFields) };

const FieldDesc kLogoListFields[] = {
	HTTPD_LIST_OF("items", &kLogoSchema,
		"every channel of the list `mode` names, in the order the box holds them"),
};

const Schema kLogoListSchema = { "logo-list", HTTPD_FIELDS(kLogoListFields) };

/* The number apart from the name, which the copied endpoint did not do: that one
   printed the two into one string. The number is what the stream said and the
   name is what this box calls it. */
const FieldDesc kCaidFields[] = {
	HTTPD_MEMBER("caid", FieldType::UInt,
		"the conditional access system id the stream announces, as a decimal number (commonly written in hexadecimal, e.g. 0x1702 is 5890)"),
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
	j.key("video_codec_hint");
	j.value(videoCodecName(c.video_hint));
	j.key("video_codec_hint_source");
	j.value(hintSourceName(c.video_hint_source));
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

	j.key("pids");
	j.beginObject();
	j.key("pmt");
	j.value((unsigned) s.pmt_pid);
	j.key("pcr");
	j.value((unsigned) s.pcr_pid);
	j.key("teletext");
	j.value((unsigned) s.teletext_pid);
	j.endObject();

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
	   without asking whether there was one. In standby nothing plays either. */
	coreapi::Result<coreapi::ChannelInfo> got = coreapi::channels::playing();
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
		j.key("editable");
		j.value(b.editable);
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

	/* 202 although the reading is done: the box has read the files again when
	   this answers, and only the channel lists on its screen are rebuilt
	   afterwards, on its own loop. */
	return accepted();
}

/* A channel file uploaded beforehand put in place of the box's own, which the
   layer below checks and has the box read in. Done, and not only accepted:
   the box has read its lists again by the time this answers. */
Response putChannelFile(const Request &r)
{
	const std::string name = r.asString("name");
	const coreapi::channels::ChannelFile which =
		(name == "myservices") ? coreapi::channels::MyServicesFile :
		(name == "bouquets") ? coreapi::channels::BouquetsFile :
		(name == "ubouquets") ? coreapi::channels::UserBouquetsFile : coreapi::channels::ServicesFile;
	const coreapi::Result<void> done = coreapi::channels::putChannelFile(which, r.asString("file"));
	if (!done.ok())
		return problemFor(done.error());
	return noContent();
}

/* The lists the box reads web channels and the XMLTV guide from, with the
   directories its own channel lists are kept in, which is where a page puts a
   file it uploads before naming it in a list. */
const FieldDesc kWebSourcesFields[] = {
	HTTPD_LIST_OF_VALUES("webtv", ElementType::String,
		"the web TV channel lists the box reads, XML or M3U, as addresses or files on the box, in the order it reads them"),
	HTTPD_LIST_OF_VALUES("webradio", ElementType::String,
		"the web radio channel lists, in the same form"),
	HTTPD_LIST_OF_VALUES("xmltv", ElementType::String,
		"the XMLTV guides the box reads programme data from, as addresses or files on the box"),
	HTTPD_MEMBER("webtv_dir", FieldType::String,
		"the directory the box keeps web TV channel lists of its own in, where an uploaded list belongs"),
	HTTPD_MEMBER("webradio_dir", FieldType::String,
		"the directory the box keeps web radio channel lists of its own in"),
};

const Schema kWebSourcesSchema = { "web-sources", HTTPD_FIELDS(kWebSourcesFields) };

void writeList(Json &j, const char *name, const std::vector<std::string> &all)
{
	j.key(name);
	j.beginArray();
	for (size_t i = 0; i < all.size(); ++i)
		j.value(all[i]);
	j.endArray();
}

Response webSourcesAnswer()
{
	const coreapi::Result<coreapi::websources::Lists> lists = coreapi::websources::read();
	if (!lists.ok())
		return problemFor(lists.error());

	Response out = okJson();
	Json j(out.body, 2048);
	j.beginObject();
	writeList(j, "webtv", lists.value().webtv);
	writeList(j, "webradio", lists.value().webradio);
	writeList(j, "xmltv", lists.value().xmltv);
	j.key("webtv_dir");
	j.value(coreapi::websources::webtvDir());
	j.key("webradio_dir");
	j.value(coreapi::websources::webradioDir());
	j.endObject();
	return out;
}

Response readWebSources(const Request &)
{
	return webSourcesAnswer();
}

/* One list at a time and as the whole body, for the reason the bouquet's own
   list is: a list is not a value the router binds. Answered with all three as
   the box will read them, the write being carried by the program's loop a
   moment later and a read meanwhile answering what it named. */
Response writeWebSources(const Request &r)
{
	const std::string kind = r.asString("kind");
	const coreapi::websources::Kind which =
		(kind == "webradio") ? coreapi::websources::WebRadio :
		(kind == "xmltv") ? coreapi::websources::XmlTv : coreapi::websources::WebTv;

	std::vector<std::string> entries;
	const ArrayRead read = readStringArray(r.body(), coreapi::websources::kMaxSources, entries);
	if (read == ArrayRead::TooMany)
	{
		char most[32];
		std::snprintf(most, sizeof(most), "%lu", (unsigned long) coreapi::websources::kMaxSources);
		return problemResponse(StatusBadRequest, coreapi::ErrorCode::BadString,
		                       std::string("a list holds at most ") + most + " entries");
	}
	if (read != ArrayRead::Ok)
		return problemResponse(StatusBadRequest, coreapi::ErrorCode::BadString,
		                       "the body is not one array of strings");

	const coreapi::Result<void> written =
		coreapi::websources::write(which, entries, r.has("reload") && r.asBool("reload"));
	if (!written.ok())
		return problemFor(written.error());
	return webSourcesAnswer();
}

Response zap(const Request &r)
{
	/* The channel is looked up before anything is posted, which the layer below
	   does and this does not repeat: a command is posted and forgotten, so a zap
	   to an id nobody has would otherwise be accepted and change nothing. */
	coreapi::Result<void> done = coreapi::channels::zap(r.asChannelId("channel_id"),
							     r.has("wake") && r.asBool("wake"),
							     r.has("stop_playback") && r.asBool("stop_playback"));
	if (!done.ok())
		return problemFor(done.error());

	/* Accepted and not done. The message is on the box's own queue and what takes
	   it off reports nothing back, so this server has no way to have learnt that
	   the channel changed. */
	return accepted();
}

Response setMode(const Request &r)
{
	const coreapi::channels::Mode mode = (r.asString("mode") == "radio")
		? coreapi::channels::Mode::Radio
		: coreapi::channels::Mode::Tv;

	coreapi::Result<void> done = coreapi::channels::setMode(mode,
								r.has("wake") && r.asBool("wake"));
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
	   way. So the layer below refuses such a list unless the caller said to take
	   them off the box, and a page asks its user before it says so. */

	coreapi::ChannelIdList ids;
	if (!readChannelIds(items, ids))
		return problemResponse(StatusBadRequest, coreapi::ErrorCode::BadInt,
		                       "the body is not one array of hexadecimal channel identifiers");

	const coreapi::channels::Mode kind = (r.asString("mode") == "radio")
		? coreapi::channels::Mode::Radio
		: coreapi::channels::Mode::Tv;

	const bool remove_from_box = r.has("remove_from_box") && r.asBool("remove_from_box");
	coreapi::Result<void> done =
		coreapi::channels::setBouquetChannels(r.asString("bouquet"), ids, kind, remove_from_box);
	if (!done.ok())
		return problemFor(done.error());
	return noContent();
}

const Param kListParams[] = {
	HTTPD_QUERY_FROM_SET("mode", "which channel list to page through; with `bouquet`, which of the bouquet's channels",
		"tv,radio", kListModeDocs),
	/* Bounded, and the bound is what makes the narrowing below it safe rather than
	   the cast. The value is handed on as a thirty two bit number, the accessor
	   answers an unsigned long, and those are the same width on the box and not on
	   the machine this is built on, so a row leaving both ends at nought was
	   checked by nothing wherever they differ: two to the thirty two plus three
	   arrived as three and answered with that bouquet's channels. */
	HTTPD_QUERY_IN("bouquet", ParamType::UInt,
		"answer only this bouquet's channels, by the `id` `GET /api/v1/bouquets` answers (counted from 1), instead of the whole list", 1,
		kMaxBouquetId),
	HTTPD_QUERY("cursor", ParamType::ChannelId,
		"the `next_cursor` of the page before, hexadecimal; left out for the first page"),
	HTTPD_QUERY_IN("limit", ParamType::UInt, "how many channels at most this page holds, 100 when left out", 1,
		kMaxPage),
};

const Param kBouquetListParams[] = {
	HTTPD_QUERY("holds", ParamType::ChannelId,
		"answer only the bouquets that hold this channel id, hexadecimal; it costs a read of every bouquet's members"),
};

const Param kOneParams[] = {
	HTTPD_SEGMENT("id", ParamType::ChannelId,
		"the channel id, hexadecimal, up to 16 digits, as `GET /api/v1/channels` answers it"),
};

const Param kLogoParams[] = {
	HTTPD_SEGMENT("id", ParamType::ChannelId,
		"the channel id, hexadecimal, up to 16 digits, as `GET /api/v1/channels` answers it"),
};

const Param kLogoListParams[] = {
	HTTPD_QUERY_FROM_SET("mode", "which channel list to answer, television when left out",
		"tv,radio", kListModeDocs),
	HTTPD_QUERY("files", ParamType::Bool,
		"whether to look for the file as well, which costs a walk of the picture directories for every channel"),
};

const Param kReloadParams[] = {
	HTTPD_BODY("hard", ParamType::Bool,
		"`false` (the default) first writes the channels the box holds in memory to disc if it has changed them since it last read or wrote them, and then reads them back, so such a change wins; `true` skips the write, so the files on disc win over memory"),
};

const Param kChannelFileParams[] = {
	HTTPD_SEGMENT_FROM_SET("name", "which of the box's 4 channel files the upload replaces",
		"services,myservices,bouquets,ubouquets",
		"services: services.xml, the transponders and services a scan found\n"
		"myservices: myservices.xml, transponders and services added by hand\n"
		"bouquets: bouquets.xml, the providers' bouquets\n"
		"ubouquets: ubouquets.xml, the user's own bouquets"),
	HTTPD_BODY_REQUIRED_TEXT("file",
		"the upload's name in /tmp, where the caller put it (PUT and PATCH /api/v1/storage/file), "
		"of letters, digits, dots, dashes and underscores, ending in .xml", 128),
};

const RouteRefusal kChannelFileRefusals[] = {
	HTTPD_REFUSES(InvalidArgument, BadName,
		"the upload is a plain name in the temporary directory, of letters, digits, dots, dashes and underscores, ending in .xml"),
	HTTPD_REFUSES(NotFound, NoSuchName, "there is no uploaded file of that name"),
	HTTPD_REFUSES(InvalidArgument, ChannelFileRefused,
		"the file is no list of bouquets: it holds a sat element where only Bouquet belongs"),
	HTTPD_REFUSES(Internal, ChannelFileRefused,
		"the file could not be put in place of the box's ubouquets.xml, which is as it was"),
	HTTPD_REFUSES(Internal, ChannelListUnavailable,
		"the file is in place, and the box did not read its channel lists again"),
};

const Param kWebSourceParams[] = {
	HTTPD_SEGMENT_FROM_SET("kind", "which of the 3 lists the body replaces", "webtv,webradio,xmltv",
		"webtv: the web TV channel lists\n"
		"webradio: the web radio channel lists\n"
		"xmltv: the XMLTV guides"),
	HTTPD_BODY_IS_LIST_OF("sources", ParamType::String,
		"a JSON array of the entries in the order the box is to read them, each an http or https address or an absolute path on the box of up to 1024 bytes, 100 for a guide, which is the longest the guide daemon takes, and without a `#`, which the settings file cannot keep; `[]` empties the list",
		0, (long) coreapi::websources::kMaxSources),
	HTTPD_QUERY("reload", ParamType::Bool,
		"`true` has the box read its channel lists again once the list is in place, and the guides as well where the XMLTV list was written; left out, the list is read at the next reload or start"),
};

const RouteRefusal kWebSourceRefusals[] = {
	HTTPD_REFUSES(InvalidArgument, BadString,
		"entry 1 of the webtv list is not an http or https address or an absolute path of up to 1024 bytes without control characters or a number sign"),
};

const Param kZapParams[] = {
	HTTPD_BODY_REQUIRED("channel_id", ParamType::ChannelId,
		"the channel to play, hexadecimal, up to 16 digits, as `GET /api/v1/channels` answers it"),
	HTTPD_BODY("wake", ParamType::Bool,
		"`true` switches a box in standby on and then plays the channel; `false` (the default) leaves a box in standby alone and the request is refused with `409 box-in-standby`"),
	HTTPD_BODY("stop_playback", ParamType::Bool,
		"`true` ends a file the movie player is playing and then plays the channel; `false` (the default) leaves the playback alone and the request is refused with `409 playback-running`"),
};

const Param kModeParams[] = {
	HTTPD_BODY_REQUIRED_FROM_SET("mode", "the mode the box is to switch to, and with it the list it plays from", "tv,radio",
		"tv: television mode, playing from the television list\n"
		"radio: radio mode, playing from the radio list"),
	HTTPD_BODY("wake", ParamType::Bool,
		"`true` switches a box in standby on and then changes the mode; `false` (the default) leaves a box in standby alone and the request is refused with `409 box-in-standby`"),
};

/* A bouquet's name, bounded because it is a name and not a document. The ceiling
   is the same wherever the name is written, in the path and in a body, so a name
   this server accepts is one it can be asked about again. */
const long kMaxBouquetName = 255;

const Param kNewBouquetParams[] = {
	HTTPD_BODY_REQUIRED_TEXT("name",
		"the name of the new bouquet, which no other bouquet may carry; it is what the bouquet routes address it by",
		kMaxBouquetName),
};

const Param kBouquetParams[] = {
	HTTPD_SEGMENT_TEXT("bouquet",
		"the bouquet's `name` as `GET /api/v1/bouquets` answers it, percent encoded in the path", kMaxBouquetName),
};

const Param kRenameParams[] = {
	HTTPD_SEGMENT_TEXT("bouquet",
		"the bouquet's current `name` as `GET /api/v1/bouquets` answers it, percent encoded in the path", kMaxBouquetName),
	HTTPD_BODY_REQUIRED_TEXT("name",
		"the new name, which no bouquet may carry yet, the renamed one included", kMaxBouquetName),
};

const Param kMoveParams[] = {
	HTTPD_SEGMENT_TEXT("bouquet",
		"the bouquet's `name` as `GET /api/v1/bouquets` answers it, percent encoded in the path", kMaxBouquetName),
	HTTPD_BODY_REQUIRED_FROM_SET("direction", "which way the bouquet moves, by exactly 1 place", "up,down",
		"up: 1 place towards the start of the list, swapping with the bouquet before it\n"
		"down: 1 place towards the end of the list, swapping with the bouquet after it"),
};

const Param kHiddenParams[] = {
	HTTPD_SEGMENT_TEXT("bouquet",
		"the bouquet's `name` as `GET /api/v1/bouquets` answers it, percent encoded in the path", kMaxBouquetName),
	HTTPD_BODY_REQUIRED("on", ParamType::Bool,
		"`true` leaves the bouquet out of the channel lists the box shows on screen, `false` shows it again"),
};

const Param kLockedParams[] = {
	HTTPD_SEGMENT_TEXT("bouquet",
		"the bouquet's `name` as `GET /api/v1/bouquets` answers it, percent encoded in the path", kMaxBouquetName),
	HTTPD_BODY_REQUIRED("on", ParamType::Bool,
		"`true` makes the box ask for the parental PIN before it opens the bouquet, `false` removes that lock"),
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
	HTTPD_SEGMENT_TEXT("bouquet",
		"the bouquet's `name` as `GET /api/v1/bouquets` answers it, percent encoded in the path", kMaxBouquetName),
	HTTPD_QUERY_REQUIRED_FROM_SET("mode", "which of the bouquet's 2 channel lists the body replaces", "tv,radio",
		"tv: the television channels of the bouquet (tuned and WebTV)\n"
		"radio: the radio channels of the bouquet (tuned and web radio)"),
	HTTPD_BODY_IS_LIST_OF("channels", ParamType::ChannelId,
		"a JSON array of channel ids, hexadecimal strings as `GET /api/v1/channels` answers them, in the order the bouquet is to hold them; an id named twice is kept once, at its first place; `[]` empties that list of the bouquet",
		0, (long) kMaxChannelsInBouquet),
	HTTPD_QUERY("remove_from_box", ParamType::Bool,
		"`true` takes a channel the list leaves out off the box when no other bouquet holds it, which the box does on its own when a channel leaves its last bouquet; left out or `false`, such a list is refused with `409 channel-would-be-removed` and nothing changes"),
};

const RouteRefusal kListChannelsRefusals[] = {
	HTTPD_REFUSES(InvalidArgument, NoSuchChannel,
		"the cursor names a channel this list does not hold"),
	HTTPD_REFUSES(NotFound, NoSuchBouquet,
		"no bouquet with that id"),
};

const RouteRefusal kCurrentChannelRefusals[] = {
	HTTPD_REFUSES(NotFound, NoRunningChannel,
		"no channel is playing"),
};

const RouteRefusal kCurrentCryptRefusals[] = {
	HTTPD_REFUSES(NotFound, NoRunningChannel,
		"nothing is playing, the box is in standby"),
};

const RouteRefusal kGetChannelRefusals[] = {
	HTTPD_REFUSES(NotFound, NoSuchChannel,
		"no channel with that id"),
};

const RouteRefusal kGetLogoRefusals[] = {
	HTTPD_REFUSES(NotFound, LogosNotOffered,
		"this server is set not to hand channel pictures over"),
	HTTPD_REFUSES(NotFound, NoSuchLogo,
		"no picture for that channel"),
};

const RouteRefusal kZapRefusals[] = {
	HTTPD_REFUSES(NotFound, NoSuchChannel,
		"no channel with that id"),
	HTTPD_REFUSES(Conflict, BoxInStandby,
		"the box is in standby"),
	HTTPD_REFUSES(Conflict, RecordingHoldsTuner,
		"a recording holds the tuner this channel needs"),
	HTTPD_REFUSES(Conflict, PlaybackRunning,
		"something is playing in the movie player"),
	HTTPD_REFUSES(Conflict, ModeUnavailable,
		"the box has not finished starting"),
};

const RouteRefusal kSetModeRefusals[] = {
	HTTPD_REFUSES(Conflict, BoxInStandby,
		"the box is in standby"),
	HTTPD_REFUSES(Conflict, ModeUnavailable,
		"the box has not finished starting"),
};

const RouteRefusal kMakeBouquetRefusals[] = {
	HTTPD_REFUSES(Conflict, NameTaken,
		"a bouquet of that name is already there"),
	HTTPD_REFUSES(Internal, BouquetNotChanged,
		"the box did not change the bouquet"),
	HTTPD_REFUSES(Internal, BouquetNotSaved,
		"saving the bouquet files failed: the list the box runs carries the change, the files may not"),
};

const RouteRefusal kRemoveBouquetRefusals[] = {
	HTTPD_REFUSES(NotFound, NoSuchBouquet,
		"no bouquet of that name"),
	HTTPD_REFUSES(Conflict, NameShared,
		"more than one bouquet carries that name"),
	HTTPD_REFUSES(Conflict, BouquetNotEditable,
		"the box builds this bouquet itself, so a change to it would not last"),
	HTTPD_REFUSES(Internal, BouquetNotChanged,
		"the box did not change the bouquet"),
	HTTPD_REFUSES(Internal, BouquetNotSaved,
		"saving the bouquet files failed: the list the box runs carries the change, the files may not"),
};

const RouteRefusal kRenameBouquetRefusals[] = {
	HTTPD_REFUSES(NotFound, NoSuchBouquet,
		"no bouquet of that name"),
	HTTPD_REFUSES(Conflict, NameShared,
		"more than one bouquet carries that name"),
	HTTPD_REFUSES(Conflict, BouquetNotEditable,
		"the box builds this bouquet itself, so a change to it would not last"),
	HTTPD_REFUSES(Conflict, NameTaken,
		"a bouquet of that name is already there"),
	HTTPD_REFUSES(Internal, BouquetNotChanged,
		"the box did not change the bouquet"),
	HTTPD_REFUSES(Internal, BouquetNotSaved,
		"saving the bouquet files failed: the list the box runs carries the change, the files may not"),
};

const RouteRefusal kMoveBouquetRefusals[] = {
	HTTPD_REFUSES(InvalidArgument, AlreadyAtTheEnd,
		"the bouquet is already at that end of the list"),
	HTTPD_REFUSES(NotFound, NoSuchBouquet,
		"no bouquet of that name"),
	HTTPD_REFUSES(Conflict, NameShared,
		"more than one bouquet carries that name"),
	HTTPD_REFUSES(Conflict, BouquetNotEditable,
		"the box builds this bouquet itself, so a change to it would not last"),
	HTTPD_REFUSES(Internal, BouquetNotChanged,
		"the box did not change the bouquet"),
	HTTPD_REFUSES(Internal, BouquetNotSaved,
		"saving the bouquet files failed: the list the box runs carries the change, the files may not"),
};

const RouteRefusal kHideBouquetRefusals[] = {
	HTTPD_REFUSES(NotFound, NoSuchBouquet,
		"no bouquet of that name"),
	HTTPD_REFUSES(Conflict, NameShared,
		"more than one bouquet carries that name"),
	HTTPD_REFUSES(Conflict, BouquetNotEditable,
		"the box builds this bouquet itself, so a change to it would not last"),
	HTTPD_REFUSES(Internal, BouquetNotChanged,
		"the box did not change the bouquet"),
	HTTPD_REFUSES(Internal, BouquetNotSaved,
		"saving the bouquet files failed: the list the box runs carries the change, the files may not"),
};

const RouteRefusal kLockBouquetRefusals[] = {
	HTTPD_REFUSES(NotFound, NoSuchBouquet,
		"no bouquet of that name"),
	HTTPD_REFUSES(Conflict, NameShared,
		"more than one bouquet carries that name"),
	HTTPD_REFUSES(Conflict, BouquetNotEditable,
		"the box builds this bouquet itself, so a change to it would not last"),
	HTTPD_REFUSES(Internal, BouquetNotChanged,
		"the box did not change the bouquet"),
	HTTPD_REFUSES(Internal, BouquetNotSaved,
		"saving the bouquet files failed: the list the box runs carries the change, the files may not"),
};

const RouteRefusal kFillBouquetRefusals[] = {
	HTTPD_REFUSES(InvalidArgument, BadInt,
		"the body is not one array of hexadecimal channel identifiers"),
	HTTPD_REFUSES(InvalidArgument, TooManyChannels,
		"one half of a bouquet takes at most 4096 channels"),
	HTTPD_REFUSES(InvalidArgument, WrongKindOfChannel,
		"a channel of the other kind for this half"),
	HTTPD_REFUSES(NotFound, NoSuchChannel,
		"no channel with that id"),
	HTTPD_REFUSES(NotFound, NoSuchBouquet,
		"no bouquet of that name"),
	HTTPD_REFUSES(Conflict, NameShared,
		"more than one bouquet carries that name"),
	HTTPD_REFUSES(Conflict, BouquetNotEditable,
		"the box builds this bouquet itself, so a change to it would not last"),
	HTTPD_REFUSES(Conflict, ChannelWouldBeRemoved,
		"no other bouquet holds a channel the list leaves out, so the box would forget it"),
	HTTPD_REFUSES(Internal, BouquetNotChanged,
		"the box did not change the bouquet"),
	HTTPD_REFUSES(Internal, BouquetNotSaved,
		"saving the bouquet files failed: the list the box runs carries the change, the files may not"),
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
	  "Lists the channels of the television or the radio list, or of 1 bouquet, a page at a time, a whole list by the number the box shows beside each channel and a bouquet in its own order. Each entry carries the channel `id` that every other route takes.\n\n"
	  "To read the whole list, send the first request without `cursor`, then repeat with `cursor` set to the `next_cursor` of the answer until an answer has no `next_cursor`. The cursor is a channel id, not an offset, so the pages keep their place while the list keeps its order; a reload that reorders the list between 2 pages can repeat or leave out channels, so start again without `cursor` once the list changed. A channel listed twice in a bouquet is answered once.\n\n"
	  "**Refusals:**\n"
	  "- `400 no-such-channel`: `cursor` names a channel this list does not hold, for example after the list changed; start again without `cursor`.\n"
	  "- `404 no-such-bouquet`: no bouquet has that `bouquet` number; read the numbers from `GET /api/v1/bouquets`.\n\n"
	  "**Related:** `GET /api/v1/channels/{id}` for 1 channel with its streams, `GET /api/v1/bouquets`, `POST /api/v1/zap`.",
	  HTTPD_PARAMS(kListParams), &kChannelPageSchema, &listChannels, false,
	  Answers200, HTTPD_REFUSALS(kListChannelsRefusals) },
	{ Method::Get, "/api/v1/channels/current", AuthLevel::Read,
	  "the channel the box is playing, with what its streams are",
	  "Answers the channel the box is playing live, with its picture and sound streams as the box has read them, which sound track is selected, and the packet ids of the programme map, the clock reference and the teletext.\n\n"
	  "**Preconditions:** the box is not in standby and plays a channel, not a recording or a file.\n\n"
	  "**Refusals:**\n"
	  "- `404 no-running-channel`: no channel is playing, for example because the box is in standby or the movie player shows a recording or a file; `GET /api/v1/playback` says which. Wake it with `POST /api/v1/system/standby` or play a channel with `POST /api/v1/zap` and `wake: true`.\n\n"
	  "**Related:** the `zap` event on `GET /api/v1/events` says when this changes; `GET /api/v1/playback`, `GET /api/v1/channels/current/crypt`, `GET /api/v1/epg/current`.",
	  NULL, 0, &kChannelDetailSchema, &currentChannel, false,
	  Answers200, HTTPD_REFUSALS(kCurrentChannelRefusals) },
	{ Method::Get, "/api/v1/channels/logos", AuthLevel::Read,
	  "every channel of one list with what is known about the picture it is shown with",
	  "Lists every channel of the television or the radio list with the ids the box names its channel picture (logo) files after. Not paged.\n\n"
	  "By default the box does not look for the files, and `path` and `resolved` are empty. With `files=true` it searches its logo directories for every channel, which is slow on a large list, and fills in where the picture is.\n\n"
	  "This list is answered whether or not the server hands pictures out; to fetch one picture, use `GET /api/v1/channels/{id}/logo`.",
	  HTTPD_PARAMS(kLogoListParams), &kLogoListSchema, &listLogos, false,
	  Answers200, HTTPD_NO_REFUSALS },
	{ Method::Get, "/api/v1/channels/current/crypt", AuthLevel::Read,
	  "which conditional access systems the running channel is scrambled under",
	  "Lists the conditional access systems (CA system ids, CAIDs) the channel playing live announces, with the box's name for each system. An empty list means the channel is not scrambled.\n\n"
	  "**Preconditions:** the box is not in standby and plays a channel.\n\n"
	  "**Refusals:**\n"
	  "- `404 no-running-channel`: nothing is playing, for example in standby.\n\n"
	  "**Related:** `GET /api/v1/system/decryption` for what is descrambling it, `GET /api/v1/channels/current`.",
	  NULL, 0, &kCryptListSchema, &currentCrypt, false,
	  Answers200, HTTPD_REFUSALS(kCurrentCryptRefusals) },
	{ Method::Get, "/api/v1/channels/{id}", AuthLevel::Read,
	  "one channel by its identifier, with what its streams are where the box has read them",
	  "Answers 1 channel by its id, with its picture and sound streams and the packet ids beside them. The box reads a channel's streams when it plays it, so a channel nobody has played since the box started can answer `streams_known: false` and an empty `audio` list, with a `video` codec of `unknown` or `none` unless the box's service list carries the picture's pid and coding.\n\n"
	  "**Refusals:**\n"
	  "- `404 no-such-channel`: no channel has that id; take ids from `GET /api/v1/channels`.\n\n"
	  "**Related:** `GET /api/v1/channels/current`, `POST /api/v1/zap`.",
	  HTTPD_PARAMS(kOneParams), &kChannelDetailSchema, &getChannel, false,
	  Answers200, HTTPD_REFUSALS(kGetChannelRefusals) },
	{ Method::Get, "/api/v1/channels/{id}/logo", AuthLevel::Read,
	  "the picture the box shows that channel with, as the file it is",
	  "Answers the channel's picture (logo) file itself, with the media type its name ends in: `image/svg+xml`, `image/png`, `image/jpeg` or `image/gif`, otherwise `application/octet-stream`. Byte ranges (`Range` header) are answered with `206`.\n\n"
	  "**Preconditions:** this server is set to hand channel pictures out (`channel_logos` of `GET /api/v1/system/webserver`).\n\n"
	  "**Refusals:**\n"
	  "- `404 logos-not-offered`: the server is set not to hand channel pictures out; treat it like a channel without a picture.\n"
	  "- `404 no-such-logo`: the box has no picture for that channel, or the file could not be read; show the channel name instead.\n\n"
	  "**Related:** `GET /api/v1/channels/logos`.",
	  HTTPD_PARAMS(kLogoParams), NULL, &getLogo, false,
	  Answers200 | Answers206, HTTPD_REFUSALS(kGetLogoRefusals) },
	{ Method::Get, "/api/v1/bouquets", AuthLevel::Read,
	  "the bouquets the box holds, or only those holding one channel",
	  "Lists the bouquets (channel groups) the box holds, in its own order, with their number, name, flags and how many television and radio channels each holds. Hidden bouquets are included.\n\n"
	  "The `name` addresses a bouquet in the `/api/v1/bouquets/{bouquet}` routes; the `id` is its current position and is what `GET /api/v1/channels` and `GET /api/v1/epg/grid` take as `bouquet`. With `holds`, only the bouquets holding that channel are answered, for example to find the bouquet to open on the channel playing.\n\n"
	  "**Related:** `GET /api/v1/channels?bouquet=`, the `bouquets-changed` event on `GET /api/v1/events`.",
	  HTTPD_PARAMS(kBouquetListParams), &kBouquetListSchema, &listBouquets, false,
	  Answers200, HTTPD_NO_REFUSALS },
	{ Method::Post, "/api/v1/zap", AuthLevel::Write,
	  "asks the box to play one channel; refused with recording-holds-tuner while a recording holds the tuner the channel needs, with playback-running while the movie player plays a file unless stop_playback is set, and with box-in-standby in standby unless wake is set",
	  "Switches live playback to the channel `channel_id` names, as choosing it on the remote control would. `202` means the box has queued the switch, not that it has happened: watch for the `zap` event on `GET /api/v1/events`, or read `GET /api/v1/channels/current`.\n\n"
	  "**Preconditions:** the box is not in standby, or `wake` is `true`. The movie player plays no file, or `stop_playback` is `true`. No running recording holds the tuner the channel needs.\n\n"
	  "**Refusals:**\n"
	  "- `404 no-such-channel`: no channel has that id. Take ids from `GET /api/v1/channels`.\n"
	  "- `409 recording-holds-tuner`: a recording occupies the tuner this channel needs. This is checked first, so neither `stop_playback` nor `wake` helps. Pick a channel the free tuner receives, or end the recording with `DELETE /api/v1/recordings/{id}`.\n"
	  "- `409 playback-running`: the movie player is playing a file, such as a finished recording, and `stop_playback` was not `true`. A web channel or the timeshift playing is not refused. Send again with `stop_playback: true` to end the playback and play the channel.\n"
	  "- `409 mode-unavailable`: the box is still starting and has not settled on a mode. Send again once it has.\n"
	  "- `409 box-in-standby`: the box is in standby and `wake` was not `true`. Send again with `wake: true` to switch the box on and play the channel.\n\n"
	  "**Related:** `POST /api/v1/mode`, `GET /api/v1/system/standby`, `POST /api/v1/system/standby`.",
	  HTTPD_PARAMS(kZapParams), NULL, &zap, false,
	  Answers202, HTTPD_REFUSALS(kZapRefusals) },
	{ Method::Post, "/api/v1/mode", AuthLevel::Write,
	  "asks the box to change between television and radio; refused with box-in-standby in standby unless wake is set",
	  "Switches the box between television and radio mode, as the TV and radio keys of the remote control do; the box then plays from that list. `202` means the box has queued the change: watch for the `mode` event on `GET /api/v1/events`.\n\n"
	  "**Preconditions:** the box is not in standby, or `wake` is `true`.\n\n"
	  "**Refusals:**\n"
	  "- `409 mode-unavailable`: the box is still starting and has not settled on a mode. Send again once it has.\n"
	  "- `409 box-in-standby`: the box is in standby and `wake` was not `true`. Send again with `wake: true` to switch the box on in that mode.\n\n"
	  "**Related:** `POST /api/v1/zap`, `GET /api/v1/system/standby`.",
	  HTTPD_PARAMS(kModeParams), NULL, &setMode, false,
	  Answers202, HTTPD_REFUSALS(kSetModeRefusals) },
	{ Method::Post, "/api/v1/bouquets", AuthLevel::Write,
	  "makes a bouquet with nothing in it",
	  "Makes a new, empty user bouquet. Fill it with `PUT /api/v1/bouquets/{bouquet}/channels`. The answer is `201` once the box has written its bouquet file and the new name reads back from its list; no `Location` header, the bouquet is addressed by the name that was sent.\n\n"
	  "**Side effects:** the bouquet files are written, and every client sees a `bouquets-changed` event on `GET /api/v1/events`.\n\n"
	  "**Refusals:**\n"
	  "- `409 name-taken`: a bouquet of that name exists; choose another name.\n"
	  "- `500 bouquet-not-changed`: the box did not take the change or it did not read back; read `GET /api/v1/bouquets` and try again.\n"
	  "- `500 bouquet-not-saved`: the change is in the list the box runs, but its bouquet files may not hold it; do not send it again, read `GET /api/v1/bouquets`.\n\n"
	  "**Related:** `GET /api/v1/bouquets`, `DELETE /api/v1/bouquets/{bouquet}`.",
	  HTTPD_PARAMS(kNewBouquetParams), NULL, &makeBouquet, false,
	  Answers201, HTTPD_REFUSALS_AND_BODY(kMakeBouquetRefusals, "{\"name\":\"Sport\"}") },
	{ Method::Delete, "/api/v1/bouquets/{bouquet}", AuthLevel::Write,
	  "takes a bouquet away, with whatever it holds",
	  "Removes a bouquet together with the channel entries it holds. Every bouquet after it moves up 1 place, so the `id` numbers of `GET /api/v1/bouquets` change.\n\n"
	  "**Side effects:** the bouquet files are written, and every client sees a `bouquets-changed` event on `GET /api/v1/events`.\n\n"
	  "**Refusals:**\n"
	  "- `404 no-such-bouquet`: no bouquet has that name; read the names from `GET /api/v1/bouquets`.\n"
	  "- `409 name-shared`: more than one bouquet carries that name, so it names none of them; rename one of them on the box first.\n"
	  "- `409 bouquet-not-editable`: the box builds this bouquet itself (`editable` is false in `GET /api/v1/bouquets`) and makes it again, so a change to it would not last.\n"
	  "- `500 bouquet-not-changed`: the box did not take the change or it did not read back.\n"
	  "- `500 bouquet-not-saved`: the change is in the list the box runs, but its bouquet files may not hold it; do not send it again, read `GET /api/v1/bouquets`.\n\n"
	  "**Related:** `POST /api/v1/bouquets`.",
	  HTTPD_PARAMS(kBouquetParams), NULL, &removeBouquet, false,
	  Answers204, HTTPD_REFUSALS(kRemoveBouquetRefusals) },
	{ Method::Put, "/api/v1/bouquets/{bouquet}/name", AuthLevel::Write,
	  "renames it, which moves the name every other route here addresses it by",
	  "Gives a bouquet a new name. Afterwards the bouquet is addressed by the new name only: the old path no longer names anything.\n\n"
	  "**Side effects:** the bouquet files are written, and every client sees a `bouquets-changed` event on `GET /api/v1/events`.\n\n"
	  "**Refusals:**\n"
	  "- `404 no-such-bouquet`: no bouquet has the name in the path.\n"
	  "- `409 name-shared`: more than one bouquet carries that name, so it names none of them; rename one of them on the box first.\n"
	  "- `409 bouquet-not-editable`: the box builds this bouquet itself (`editable` is false in `GET /api/v1/bouquets`) and makes it again, so a change to it would not last.\n"
	  "- `409 name-taken`: a bouquet already carries the new name, which includes renaming a bouquet to its own name.\n"
	  "- `500 bouquet-not-changed`: the box did not take the change, or the old name is still there or the new one is missing afterwards.\n"
	  "- `500 bouquet-not-saved`: the change is in the list the box runs, but its bouquet files may not hold it; do not send it again, read `GET /api/v1/bouquets`.\n\n"
	  "**Related:** `GET /api/v1/bouquets`.",
	  HTTPD_PARAMS(kRenameParams), NULL, &renameBouquet, false,
	  Answers204, HTTPD_REFUSALS_AND_BODY(kRenameBouquetRefusals, "{\"name\":\"Favourites\"}") },
	{ Method::Put, "/api/v1/bouquets/{bouquet}/position", AuthLevel::Write,
	  "moves it one place up or down the list, and refuses at either end",
	  "Moves a bouquet 1 place up or down the bouquet list, swapping it with its neighbour; this also swaps their `id` numbers. To move a bouquet further, send the request again.\n\n"
	  "**Side effects:** the bouquet files are written, and every client sees a `bouquets-changed` event on `GET /api/v1/events`.\n\n"
	  "**Refusals:**\n"
	  "- `400 already-at-the-end`: the bouquet is first and `direction` is `up`, or last and `direction` is `down`.\n"
	  "- `404 no-such-bouquet`: no bouquet has that name.\n"
	  "- `409 name-shared`: more than one bouquet carries that name, so it names none of them; rename one of them on the box first.\n"
	  "- `409 bouquet-not-editable`: the box builds this bouquet itself (`editable` is false in `GET /api/v1/bouquets`) and makes it again, so a change to it would not last.\n"
	  "- `500 bouquet-not-changed`: the box did not take the move or it did not read back.\n"
	  "- `500 bouquet-not-saved`: the change is in the list the box runs, but its bouquet files may not hold it; do not send it again, read `GET /api/v1/bouquets`.\n\n"
	  "**Related:** `GET /api/v1/bouquets`.",
	  HTTPD_PARAMS(kMoveParams), NULL, &moveBouquet, false,
	  Answers204, HTTPD_REFUSALS(kMoveBouquetRefusals) },
	{ Method::Put, "/api/v1/bouquets/{bouquet}/hidden", AuthLevel::Write,
	  "says whether the box leaves it out of the lists it draws",
	  "Hides a bouquet from the channel lists the box shows on screen, or shows it again. A hidden bouquet is still answered by `GET /api/v1/bouquets` with `hidden: true`.\n\n"
	  "**Side effects:** the bouquet files are written, and every client sees a `bouquets-changed` event on `GET /api/v1/events`.\n\n"
	  "**Refusals:**\n"
	  "- `404 no-such-bouquet`: no bouquet has that name.\n"
	  "- `409 name-shared`: more than one bouquet carries that name, so it names none of them; rename one of them on the box first.\n"
	  "- `409 bouquet-not-editable`: the box builds this bouquet itself (`editable` is false in `GET /api/v1/bouquets`) and makes it again, so a change to it would not last.\n"
	  "- `500 bouquet-not-changed`: the box did not take the change or it did not read back.\n"
	  "- `500 bouquet-not-saved`: the change is in the list the box runs, but its bouquet files may not hold it; do not send it again, read `GET /api/v1/bouquets`.",
	  HTTPD_PARAMS(kHiddenParams), NULL, &hideBouquet, false,
	  Answers204, HTTPD_REFUSALS(kHideBouquetRefusals) },
	{ Method::Put, "/api/v1/bouquets/{bouquet}/locked", AuthLevel::Write,
	  "says whether the box asks for the parental code before it opens it",
	  "Locks a bouquet behind the box's parental PIN, or unlocks it. The lock applies on the box's own screen; this API does not ask for the PIN.\n\n"
	  "**Side effects:** the bouquet files are written, and every client sees a `bouquets-changed` event on `GET /api/v1/events`.\n\n"
	  "**Refusals:**\n"
	  "- `404 no-such-bouquet`: no bouquet has that name.\n"
	  "- `409 name-shared`: more than one bouquet carries that name, so it names none of them; rename one of them on the box first.\n"
	  "- `409 bouquet-not-editable`: the box builds this bouquet itself (`editable` is false in `GET /api/v1/bouquets`) and makes it again, so a change to it would not last.\n"
	  "- `500 bouquet-not-changed`: the box did not take the change or it did not read back.\n"
	  "- `500 bouquet-not-saved`: the change is in the list the box runs, but its bouquet files may not hold it; do not send it again, read `GET /api/v1/bouquets`.",
	  HTTPD_PARAMS(kLockedParams), NULL, &lockBouquet, false,
	  Answers204, HTTPD_REFUSALS(kLockBouquetRefusals) },
	{ Method::Post, "/api/v1/channels/reload", AuthLevel::Write,
	  "asks the box to read its channel lists again",
	  "Makes the box read its channel and bouquet files again, for example after they were changed on disc. By default it first writes the channels it holds in memory to disc if it has changed them since it last read or wrote them, so such a change is not lost; with `hard: true` it skips that write and the files on disc win. `202` means the box has read the files again; the channel lists on its screen are rebuilt afterwards, on its own loop.\n\n"
	  "**Refusals:**\n"
	  "- `channel-list-unavailable` (a 5xx status): the box's channel manager did not say it had read the files again; try again later.\n\n"
	  "**Related:** `GET /api/v1/channels`, `GET /api/v1/bouquets`.",
	  HTTPD_PARAMS(kReloadParams), NULL, &reloadChannelLists, false,
	  Answers202, HTTPD_NO_REFUSALS },
	{ Method::Post, "/api/v1/channels/files/{name}", AuthLevel::System,
	  "puts an uploaded channel or bouquet file in place of the box's own and has the box read its channel lists again",
	  "Replaces one of the 4 files the box reads its channels from with a file uploaded to /tmp beforehand, and has the box read its channel lists again from the files, so the upload wins over the channels the box holds in memory, as `POST /api/v1/channels/reload` with `hard: true` does. `204` says the file is in place and the box has read its lists again.\n\n"
	  "The box first takes the upload over: it copies it into a directory of its own and removes the upload, so what it checks is what it puts in place. The file holds no more than 16 MiB, the box's own XML reader takes it, and its root element is `zapit`, holding only `sat`, `cable` and `terrestrial` elements for `services` and `myservices`, each with its `name`, and only `Bouquet` elements for `bouquets` and `ubouquets`, which keeps a file sent as the wrong one of the 4 out. It is written beside the box's file and renamed over it, so the box reads the old file or the new one and never part of either.\n\n"
	  "**Preconditions:** the file was uploaded to /tmp, as `PUT` and `PATCH /api/v1/storage/file` do in pieces for a file larger than one request.\n\n"
	  "**Side effects:** the channel lists are read again, which drops changes the box held only in memory and which every client sees as a `bouquets-changed` event on `GET /api/v1/events`.\n\n"
	  "**Refusals:**\n"
	  "- `400 bad-name`: `file` is not a plain name in /tmp ending in .xml.\n"
	  "- `404 no-such-name`: no file of that name was uploaded.\n"
	  "- `400 channel-file-refused`: the file is larger than 16 MiB, is no XML the box can read, or is not a list of the kind `name` says; the detail says which, and the box's file is untouched.\n"
	  "- `500 channel-file-refused`: the box could not take the upload over or put the file in place, or its channel manager did not answer before, and its own file is as it was; or the file is in place and read, and the disc did not confirm the change, which a power cut may undo; the detail says which.\n"
	  "- `500 channel-list-unavailable`: the file is in place, and the box's channel manager did not say it had read it; `POST /api/v1/channels/reload` with `hard: true` does that later.\n\n"
	  "**Related:** `POST /api/v1/channels/reload`, `PUT /api/v1/storage/file`.",
	  HTTPD_PARAMS(kChannelFileParams), NULL, &putChannelFile, false,
	  Answers204, HTTPD_REFUSALS_AND_BODY(kChannelFileRefusals, "{\"file\":\"ubouquets.xml\"}") },
	{ Method::Get, "/api/v1/channels/sources", AuthLevel::Write,
	  "the lists the box reads web channels and XMLTV guides from",
	  "Reads the 3 lists the box takes web content from: `webtv` and `webradio`, the channel lists (XML or M3U) of web TV and web radio, and `xmltv`, the XMLTV guides it reads programme data from, each an address or a file on the box in the order the box reads them; with them the directories the box keeps channel lists of its own in, where a file uploaded with `PUT /api/v1/storage/file` belongs before it is named in a list. Asks for the level a write does, an address in these lists often carrying an access key.\n\n"
	  "**Related:** `PUT /api/v1/channels/sources/{kind}`, `POST /api/v1/channels/reload`.",
	  NULL, 0, &kWebSourcesSchema, &readWebSources, false,
	  Answers200, HTTPD_NO_REFUSALS },
	{ Method::Put, "/api/v1/channels/sources/{kind}", AuthLevel::Write,
	  "replaces one of the lists the box reads web channels or XMLTV guides from, and reads them in again on request",
	  "Replaces the list `kind` names with the entries of the body, a JSON array of addresses and absolute paths in the order the box is to read them; `[]` empties it, and the other 2 lists stay as they are. The box takes the list over on its own loop and saves its settings with it; the answer carries all 3 lists as the box will read them. With `reload=true` the box then reads its channel lists again, and the guides as well when the XMLTV list was written.\n\n"
	  "**Side effects:** the box's settings are saved; with `reload=true` the channel lists are read again, which every client sees as a `bouquets-changed` event on `GET /api/v1/events`.\n\n"
	  "**Refusals:**\n"
	  "- `400 bad-string`: the body is not 1 array of strings, holds more than 32 entries, or an entry is not an http or https address or an absolute path of up to 1024 bytes (100 for a guide) without control characters or `#`; the detail names the entry, and nothing is written.\n\n"
	  "**Related:** `GET /api/v1/channels/sources`.",
	  HTTPD_PARAMS(kWebSourceParams), &kWebSourcesSchema, &writeWebSources, false,
	  Answers200, HTTPD_REFUSALS_AND_BODY(kWebSourceRefusals, "[\"http://example.com/webtv.xml\"]") },
	{ Method::Put, "/api/v1/bouquets/{bouquet}/channels", AuthLevel::Write,
	  "replaces the television or the radio members of it with the channels the body names, in the order it names them, and empties that half for a body naming none; the body is a JSON array of hexadecimal channel identifiers",
	  "Replaces the television or the radio channels of a bouquet with the channels the body lists, in that order; the other list of the bouquet stays as it is. `[]` empties the list. The body is a JSON array of hexadecimal channel id strings, at most 4096; every id is checked before anything changes, so a refused request leaves the bouquet untouched.\n\n"
	  "To add or remove 1 channel, read the current list with `GET /api/v1/channels?bouquet={id}&mode=...`, change it and send the whole list back.\n\n"
	  "**Side effects:** the bouquet files are written, the box renumbers the channels it shows, and every client sees a `bouquets-changed` event on `GET /api/v1/events`. A channel that no other bouquet holds is taken off the box when it leaves this list, so a list leaving one out is refused unless `remove_from_box` is `true`. A channel the body names twice goes in once, where it is named first. A channel this half already held twice (a bouquet file can name one twice) keeps its second entry behind the list, and the listing answers it once.\n\n"
	  "**Refusals:**\n"
	  "- `400 bad-int` or `400 bad-string`: the body is not 1 JSON array of hexadecimal ids of up to 16 digits.\n"
	  "- `400 too-many-channels`: more than 4096 ids.\n"
	  "- `400 wrong-kind-of-channel`: an id names a radio channel for the television half, or the other way round; nothing was changed.\n"
	  "- `404 no-such-channel`: an id names no channel; nothing was changed.\n"
	  "- `404 no-such-bouquet`: no bouquet has that name.\n"
	  "- `409 name-shared`: more than one bouquet carries that name, so it names none of them; rename one of them on the box first.\n"
	  "- `409 bouquet-not-editable`: the box builds this bouquet itself (`editable` is false in `GET /api/v1/bouquets`) and makes it again, so a change to it would not last.\n"
	  "- `409 channel-would-be-removed`: no other bouquet holds a channel the list leaves out, as the bouquets stand when the list is checked, so the box would forget it; the detail names up to 8 of them by id. Ask the user, then send the same list again with `remove_from_box=true`.\n"
	  "- `500 bouquet-not-changed`: the box did not take the change, or the list did not read back in the order sent.\n"
	  "- `500 bouquet-not-saved`: the change is in the list the box runs, but its bouquet files may not hold it; do not send it again, read `GET /api/v1/bouquets`.\n\n"
	  "**Related:** `GET /api/v1/channels`, `POST /api/v1/bouquets`.",
	  HTTPD_PARAMS(kFillParams), NULL, &fillBouquet, false,
	  Answers204, HTTPD_REFUSALS(kFillBouquetRefusals) },
};

} // namespace

// The pair, written where the array is, so the length beside it is the length of
// the array and not a number somebody kept in step by hand.
extern const RouteTable channelsTable = {
	HTTPD_TABLE("channels", kChannelEndpoints)
};

} // namespace httpd
