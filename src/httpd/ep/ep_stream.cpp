/*
 * ep_stream.cpp - routes for streams and playlists
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
#include "httpd/livestream.h"
#include "httpd/schema.h"
#include "httpd/status.h"

#include "coreapi/browserplay.h"
#include "coreapi/channels.h"
#include "coreapi/base/errors.h"
#include "coreapi/base/result.h"
#include "coreapi/settings/settings.h"
#include "coreapi/streaming.h"
#include "coreapi/base/types.h"

#include <cstdio>
#include <cstdlib>
#include <string>
#include <utility>
#include <vector>

#include <fcntl.h>
#include <unistd.h>

namespace httpd
{

namespace
{

// The type a player is handed a list of channels under. One place, so the two
// routes that answer such a list cannot come to name it differently.
const char *playlistContentType()
{
	return "audio/x-mpegurl";
}

const FieldDesc kAddressFields[] = {
	HTTPD_MEMBER("url", FieldType::String,
		"where a player fetches this channel, under the name this request reached the box by"),
	HTTPD_MEMBER("name", FieldType::String, "what the box calls the channel, as the channel list names it"),
	HTTPD_MEMBER("port", FieldType::UInt,
		"the port the box streams on, which is the port the address above carries"),
};

const Schema kAddressSchema = { "stream-address", HTTPD_FIELDS(kAddressFields) };

/* The authority is the request's own and is never taken from a caller's parameters.
   It is the name that caller reached this box by, so it is the one name an address
   handed back to it is reachable under. A request that named none is refused a floor
   down rather than handed an address with nothing where the box should be. */
Response streamAddress(const Request &r)
{
	const coreapi::ChannelId id = r.asChannelId("id");

	coreapi::Result<std::string> url = coreapi::streaming::urlFor(r.host(), id);
	if (!url.ok())
		return problemFor(url.error());

	coreapi::Result<coreapi::ChannelInfo> channel = coreapi::channels::get(id);
	if (!channel.ok())
		return problemFor(channel.error());

	/* The number is read beside the address rather than picked back out of it.
	   What the address is for is playing, and what the number is for is a
	   caller building an address of its own; taking the first apart to find the
	   second would be this layer reading back what it has just written. */
	coreapi::Result<std::string> port = coreapi::settings::get("streaming_port");
	if (!port.ok())
		return problemFor(port.error());

	Response out = okJson();
	Json j(out.body, 192);
	j.beginObject();
	j.key("url");
	j.value(url.value());
	j.key("name");
	j.value(channel.value().name);
	j.key("port");
	j.value((unsigned long) std::strtoul(port.value().c_str(), NULL, 10));
	j.endObject();
	return out;
}

coreapi::streaming::Scope scopeFor(const Request &r)
{
	if (!r.has("mode"))
		return coreapi::streaming::Scope::CurrentMode;
	const std::string &m = r.asString("mode");
	if (m == "tv")
		return coreapi::streaming::Scope::Tv;
	if (m == "radio")
		return coreapi::streaming::Scope::Radio;
	return coreapi::streaming::Scope::CurrentMode;
}

/* The file the layer below wrote is opened here and removed under its name in the same
   breath, whether or not the open worked: the name was this request's alone, and a
   request that fails between here and the transport sending the answer must not be
   the reason one is left in /tmp. What is left afterwards is the descriptor
   answerFromDescriptor takes over, which is what frees the space once it is done. */
Response playlistAt(const std::string &path)
{
	const int fd = ::open(path.c_str(), O_RDONLY | O_CLOEXEC);
	::unlink(path.c_str());

	Response out;
	out.code = StatusOk;
	if (!answerFromDescriptor(out, fd))
		return problemResponse(StatusInternalServerError, coreapi::ErrorCode::ChangeRefused,
				       "the playlist could not be read back for sending");
	out.content_type = playlistContentType();
	return out;
}

/* What the file a caller saves is called, which without this is the last
   segment of the route: a browser handed a playlist under /playlist/1001 writes
   1001, and a caller with four of them has four files named after numbers.

   The channel's own name and a suffix, through the one writer of this header
   there is (endpoints.h). What a channel may be called is whatever the stream
   said, which is any bytes at all, so the name is never written into the header
   as itself; that is that writer's whole subject and the reason there is not a
   second one here.

   The identifier stands in where the channel cannot be read. A playlist that
   was written names a channel the box had a moment ago, so this is a lookup
   that lost a race rather than a request for something absent, and answering
   the file with no name at all over it would take a working playlist away for
   the sake of a header. */
void nameThePlaylistAfter(Response &out, coreapi::ChannelId id)
{
	char hex[24];
	std::string name = hexId(id, hex);

	coreapi::Result<coreapi::ChannelInfo> channel = coreapi::channels::get(id);
	if (channel.ok() && !channel.value().name.empty())
		name = channel.value().name;

	out.headers.push_back(std::make_pair(std::string("Content-Disposition"),
					     dispositionFor(name + ".m3u")));
}

Response streamPlaylist(const Request &r)
{
	coreapi::Result<std::string> written =
		coreapi::streaming::playlist(r.host(), scopeFor(r));
	if (!written.ok())
		return problemFor(written.error());
	return playlistAt(written.value());
}

Response streamPlaylistFor(const Request &r)
{
	const coreapi::ChannelId id = r.asChannelId("id");

	coreapi::Result<std::string> written =
		coreapi::streaming::playlistFor(r.host(), id);
	if (!written.ok())
		return problemFor(written.error());

	Response out = playlistAt(written.value());
	// Only over an answer that is the playlist. A refusal carries a document
	// saying what went wrong, and a header telling a browser to save that as a
	// playlist would hand somebody a file holding the reason they have none.
	if (out.code == StatusOk)
		nameThePlaylistAfter(out, id);
	return out;
}

/* THE SPELLINGS THE CHANNEL ROUTE WRITES, and this route takes them back
   rather than looking them up. The box learns a channel's streams when it
   tunes it, so a caller that has just read the channel knows them and this
   route would otherwise be asking the same question a second time and getting
   a different answer for a channel nobody has been on.

   What that leaves is a caller naming codecs a channel does not carry, which
   costs a stream that does not play and nothing else: the picture is never
   re-encoded whatever is asked for, and the sound is decoded by what is in it
   rather than by what was said about it. */
const char kVideoCodecs[] = "mpeg2,h264,hevc,cavs,none,unknown";
const char kAudioCodecs[] = "mp2,ac3,eac3,aac,aacplus,dts,dtshd,lpcm,unknown";

// The widest a pid can be. Zero is the programme association table and never a
// stream, which is why it doubles as the value that means none was named.
const long kMaxPid = 8191;

/* The address the converter reads from, which is this box talking to itself.
   Loopback rather than the name the request arrived under: the reader is a
   program on this box, the answer must not depend on which of its addresses
   somebody typed, and a box whose own name does not resolve still has this
   one. */
std::string localSource(const std::string &port, coreapi::ChannelId id)
{
	char tail[32];
	std::snprintf(tail, sizeof(tail), "/id=%llx", (unsigned long long) id);
	return "http://127.0.0.1:" + port + tail;
}

/* One channel as something a browser can play, which for most of German
   satellite television means the picture untouched and the sound converted.

   The answer is a stream and not a document: this returns the command, and the
   transport runs it and keeps the connection (src/httpd/livestream.h). What
   comes back is a transport stream for a channel with a picture and an ADTS
   stream for radio, so that radio needs nothing in the page but an audio
   element. */
Response streamForBrowser(const Request &r)
{
	const coreapi::ChannelId id = r.asChannelId("id");

	coreapi::Result<coreapi::ChannelInfo> channel = coreapi::channels::get(id);
	if (!channel.ok())
		return problemFor(channel.error());

	const std::string &video = r.asString("video");
	const std::string &audio = r.asString("audio");

	const coreapi::browserplay::Path path = coreapi::browserplay::pathFor(video, audio);
	if (path == coreapi::browserplay::Path::NotOffered)
	{
		/* Said rather than answered with a stream nothing decodes. A caller
		   told this offers the raw address to a player outside the browser,
		   which is the honest end of this road and the whole reason the
		   channel route says what the streams are. */
		return problemResponse(StatusNotImplemented,
				       coreapi::ErrorCode::NotPlayableInBrowser,
				       "no browser plays what this channel carries, and this box will not re-encode a picture");
	}

	coreapi::Result<std::string> port = coreapi::settings::get("streaming_port");
	if (!port.ok())
		return problemFor(port.error());

	const bool radio = channel.value().kind == coreapi::ServiceKind::Radio ||
			   channel.value().kind == coreapi::ServiceKind::WebRadio;
	unsigned pid = 0;
	if (r.has("apid"))
		pid = (unsigned) r.asUInt("apid");

	Response out;
	out.code = StatusOk;
	out.content_type = coreapi::browserplay::typeFor(path, radio);
	out.stream_argv = coreapi::browserplay::commandFor(path, radio,
							   localSource(port.value(), id), pid);
	// Live and never the same twice, so nothing between here and the player
	// keeps a minute of it and hands it to the next caller.
	out.headers.push_back(std::make_pair(std::string("Cache-Control"),
					     std::string("no-store")));
	return out;
}

const Param kChannelParams[] = {
	HTTPD_SEGMENT("id", ParamType::ChannelId,
		"the channel, hexadecimal, up to 16 digits, as GET /api/v1/channels answers it"),
};

const Param kBrowserParams[] = {
	HTTPD_SEGMENT("id", ParamType::ChannelId,
		"the channel, hexadecimal, up to 16 digits, as GET /api/v1/channels answers it"),
	HTTPD_QUERY_REQUIRED_FROM_SET("video",
		"what the channel's picture is, as the channel route spells it",
		kVideoCodecs,
		"mpeg2: MPEG-2 video\n"
		"h264: H.264/AVC video\n"
		"hevc: H.265/HEVC video\n"
		"cavs: Chinese AVS video\n"
		"none: no picture, a radio channel\n"
		"unknown: the box has not tuned this channel yet and has not read its codecs"),
	HTTPD_QUERY_REQUIRED_FROM_SET("audio",
		"what the channel's sound is, as the channel route spells it",
		kAudioCodecs,
		"mp2: MPEG-1 layer 2 audio\n"
		"ac3: Dolby Digital audio\n"
		"eac3: Dolby Digital Plus audio\n"
		"aac: Advanced Audio Coding\n"
		"aacplus: High Efficiency AAC\n"
		"dts: DTS (Digital Theater Systems) audio\n"
		"dtshd: DTS-HD audio\n"
		"lpcm: uncompressed linear PCM audio\n"
		"unknown: the box has not tuned this channel yet and has not read its codecs"),
	HTTPD_QUERY_IN("apid", ParamType::UInt,
		"which sound to take, decimal pid; 0, the default, takes the first the channel carries",
		0, kMaxPid),
};

const Param kPlaylistParams[] = {
	// Carried in the query, because the method that fetches a list is written
	// with no body.
	HTTPD_QUERY_FROM_SET("mode", "which list to return, the one the box is showing when it is left out",
		"tv,radio",
		"tv: the television channel list\n"
		"radio: the radio channel list"),
};

const RouteRefusal kStreamAddressRefusals[] = {
	HTTPD_REFUSES(InvalidArgument, NoAuthority,
		"the request named no authority to build an address under"),
};

const RouteRefusal kStreamForBrowserRefusals[] = {
	HTTPD_REFUSES(NotFound, NoSuchChannel,
		"no channel with that id"),
	HTTPD_REFUSES(NotSupported, NotPlayableInBrowser,
		"no browser plays what this channel carries, and this box will not re-encode a picture"),
	HTTPD_REFUSES_AS(503, TooManyConversions,
		"this box is already converting as many streams as it will"),
};

const Endpoint kStreamEndpoints[] = {
	{ Method::Get, "/api/v1/stream/playlist", AuthLevel::Read,
	  "every visible user bouquet as one playlist a player opens",
	  "Builds and returns an M3U playlist (`audio/x-mpegurl`) naming every channel "
	  "of the user's visible bouquets, from the television or the radio list as "
	  "`mode` says, and from the list the box is showing when `mode` is left out. Each "
	  "entry's address points at this box's own streaming port, under the channel "
	  "id in hexadecimal, and the entry's title carries the channel's name and "
	  "bouquet. A tuner is only taken when a player actually opens an entry's "
	  "address, not by fetching this playlist.\n\n"
	  "**Related:** `GET /api/v1/stream/{id}`, `GET /api/v1/stream/playlist/{id}`.",
	  HTTPD_PARAMS(kPlaylistParams), NULL, &streamPlaylist, false,
	  Answers200 | Answers206, HTTPD_NO_REFUSALS },
	/* THE SAME LIST UNDER THE NAME ITS FILE HAS. A box reading a playlist back
	   in as a channel list decides what it was handed by the extension of the
	   address it was given and reads nothing it cannot name that way
	   (CBouquetManager::loadWebchannels), so the address above, which ends in
	   no extension and whose last point is the one in an address like
	   192.168.1.5, is turned away there before a byte of it is read. The webtv
	   route names its file the same way and for readers of the same kind. */
	{ Method::Get, "/api/v1/stream/playlist.m3u", AuthLevel::Read,
	  "the same list, under a name a reader that goes by file names accepts",
	  "Returns exactly the playlist `GET /api/v1/stream/playlist` builds, under an "
	  "address ending in `.m3u`. A box reading a playlist back in as a channel "
	  "list decides what it was handed by the extension of the address it was "
	  "given, and the plain address above ends in no extension it recognizes, so "
	  "this route exists for readers of that kind.\n\n"
	  "**Related:** `GET /api/v1/stream/playlist`.",
	  HTTPD_PARAMS(kPlaylistParams), NULL, &streamPlaylist, false,
	  Answers200 | Answers206, HTTPD_NO_REFUSALS },
	{ Method::Get, "/api/v1/stream/playlist/{id}", AuthLevel::Read,
	  "one channel as a playlist a player opens",
	  "Builds and returns an M3U playlist (`audio/x-mpegurl`) naming the single "
	  "channel `id`, for a player that only opens playlists rather than stream "
	  "addresses directly. The answer carries a `Content-Disposition` header "
	  "naming the file after the channel, so a browser that saves it rather than "
	  "opening it gets a sensible name.\n\n"
	  "**Related:** `GET /api/v1/stream/{id}`.",
	  HTTPD_PARAMS(kChannelParams), NULL, &streamPlaylistFor, false,
	  Answers200 | Answers206, HTTPD_NO_REFUSALS },
	{ Method::Get, "/api/v1/stream/{id}", AuthLevel::Read,
	  "where a player fetches one channel",
	  "Returns the address a player can open to receive the raw stream of channel "
	  "`id`, built under the same host or address this request itself reached the "
	  "box by, together with the channel's name and the port the box streams on. "
	  "Nothing is started by this call: the tuner is taken only once a player "
	  "actually opens the address.\n\n"
	  "**Refusals:**\n"
	  "- `400 no-authority`: the request named no host to build the address under.\n\n"
	  "**Related:** `GET /api/v1/stream/playlist/{id}`, `GET /api/v1/stream/browser/{id}`.",
	  HTTPD_PARAMS(kChannelParams), &kAddressSchema, &streamAddress, false,
	  Answers200, HTTPD_REFUSALS(kStreamAddressRefusals) },
	{ Method::Get, "/api/v1/stream/browser/{id}", AuthLevel::Read,
	  "one channel as a browser can play it, with the sound converted where it has to be",
	  "Opens channel `id` and streams it straight into the response: an MPEG "
	  "transport stream for a channel with a picture, or a raw ADTS stream for "
	  "radio, so radio needs only an audio element and no demuxer in the page. "
	  "The picture is never re-encoded, only copied; the box has no spare cycles "
	  "to encode one. Depending on `video` and `audio`, the sound is either "
	  "copied as well or converted to AAC so a browser's own media stack can "
	  "decode it; `video` and `audio` are the codecs the channel route reports "
	  "for this channel, and naming codecs the channel does not actually carry "
	  "costs a stream that will not play and nothing else. `apid` picks which "
	  "sound track to take where the channel carries several. The answer is sent "
	  "`Cache-Control: no-store`, since it is a live stream and never the same "
	  "stream twice.\n\n"
	  "**Preconditions:** the box carries a converter able to produce the "
	  "requested result; otherwise the raw stream address is the only option "
	  "(`GET /api/v1/stream/{id}`).\n\n"
	  "**Refusals:**\n"
	  "- `404 no-such-channel`: no channel has that id.\n"
	  "- `501 not-playable-in-browser`: no browser plays what this channel "
	  "carries, and this box will not re-encode a picture; use `GET "
	  "/api/v1/stream/{id}` for the raw address instead.\n"
	  "- `503 too-many-conversions`: the box is already converting as many "
	  "streams as it will, at most 2 at once; retry after the time named in the "
	  "`Retry-After` header.\n\n"
	  "**Related:** `GET /api/v1/stream/{id}`.",
	  HTTPD_PARAMS(kBrowserParams), NULL, &streamForBrowser, false,
	  Answers200, HTTPD_REFUSALS(kStreamForBrowserRefusals) },
};

} // namespace

extern const RouteTable streamTable = {
	HTTPD_TABLE("stream", kStreamEndpoints)
};

} // namespace httpd
