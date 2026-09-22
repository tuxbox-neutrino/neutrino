/*
 * streaming.cpp - live and recorded streams, and the playlists naming them
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

#include "streaming.h"
#include "coreapi/base/errors.h"
#include "coreapi/base/deps.h"
#include "coreapi/settings/settings.h"

#include <cstdio>
#include <string>

#include <neutrinoMessages.h>

#include <fcntl.h>
#include <sys/types.h>
#include <unistd.h>

namespace coreapi
{
namespace streaming
{

namespace
{

// The spelling the streaming server reads an identifier back in, which is the
// one the copied endpoint writes: hexadecimal and without leading zeroes.
std::string hexChannelId(ChannelId id)
{
	char buf[24];
	std::snprintf(buf, sizeof(buf), "%llx", (unsigned long long) id);
	return std::string(buf);
}

bool isRadioKind(ServiceKind k)
{
	return k == ServiceKind::Radio || k == ServiceKind::WebRadio;
}

bool writeAll(std::FILE *f, const std::string &s)
{
	return s.empty() || std::fwrite(s.data(), 1, s.size(), f) == s.size();
}

/* The host of an authority, with a port dropped if one is there.

   Bracketed first, because an address of the second family is written with
   colons of its own and only the brackets say where it ends; cutting at the last
   colon instead would name a host nothing answers on. Everything after the
   closing bracket is the port and goes.

   Otherwise the last colon, and only when what follows it is a port: a name may
   carry a colon and a number may not. Nothing after the colon counts as a port
   as well, since an authority may state an empty one. */
std::string hostOf(const std::string &authority)
{
	if (!authority.empty() && authority[0] == '[')
	{
		const std::string::size_type close = authority.find(']');
		if (close == std::string::npos)
			return authority;
		return authority.substr(0, close + 1);
	}

	const std::string::size_type colon = authority.rfind(':');
	if (colon == std::string::npos)
		return authority;
	for (std::string::size_type i = colon + 1; i < authority.size(); i++)
	{
		if (authority[i] < '0' || authority[i] > '9')
			return authority;
	}
	return authority.substr(0, colon);
}

/* Everything of an address but the channel, so the refusal of an empty authority
   and the reading of the port are written once. The port is read off the box
   rather than written here, because an owner who moved the stream to another
   port would otherwise be handed an address nothing listens on. */
Result<std::string> addressPrefix(const std::string &authority)
{
	if (authority.empty())
		return fail(Status::InvalidArgument, ErrorCode::NoAuthority,
			    "the request named no authority to build an address under");

	Result<std::string> port = settings::get("streaming_port");
	if (!port.ok())
		return fail(port.error());

	return ok("http://" + hostOf(authority) + ":" + port.value() + "/id=");
}

/* A comma is what tells the fields of an entry from the title behind them, so
   one inside a field goes the way the copied endpoint sends it. */
std::string withoutCommas(const std::string &s)
{
	std::string out(s);
	for (std::string::size_type i = 0; i < out.size(); i++)
	{
		if (out[i] == ',')
			out[i] = '.';
	}
	return out;
}

/* The name this box answers under, which a box reading the list back in puts in
   front of every bouquet it takes over: two boxes' lists read into one stay
   apart that way. Empty where the name cannot be read, and a reader meeting an
   empty one names the bouquet after the kind of list it is instead.

   gethostname is not said to terminate what it truncates, so the last byte is
   written rather than trusted. */
std::string boxHostName()
{
	char name[256];
	if (gethostname(name, sizeof(name)) != 0)
		return std::string();
	name[sizeof(name) - 1] = '\0';
	return std::string(name);
}

/* What every entry of one playlist shares, read once rather than per entry: a
   satellite list writes thousands of them and neither of these changes between
   two lines. */
struct Shared
{
	// Where a player fetches a channel. It carries the port the box streams on,
	// which is not the port this request arrived on.
	std::string stream_prefix;
	/* Where this server answers, which is the authority as it arrived, port and
	   all: what is built off it below are routes of this server, and no other
	   port answers them. */
	std::string api_base;
	std::string group_prefix;
};

Shared sharedFor(const std::string &authority, const std::string &stream_prefix)
{
	Shared s;
	s.stream_prefix = stream_prefix;
	s.api_base = "http://" + authority;
	s.group_prefix = boxHostName();
	return s;
}

/* Where a reader fetches that channel's picture, and empty for a channel that
   has none.

   THE ROUTE AND NOT THE FILE. The copied endpoint writes the path the picture
   has on this box's disc, which the server it was written for answered; this
   one serves the web directory and one name beside it, so that path would be a
   broken picture on every line.

   The picture source is asked here and not channels::logo, which looks the
   channel up a second time for the name this loop already holds, nor
   channels::logos, which walks every channel the box knows rather than the ones
   the bouquets name and holds the answer for all of them at once. Either way
   the search is one walk of the picture directories per entry, and it is what
   says whether the field is empty: a channel with no picture must not be handed
   an address that answers with nothing. */
std::string logoUrlFor(const Shared &sh, const ChannelInfo &c)
{
	std::string path;
	if (logoSource().logoFor(c.id, c.name, path) != Status::Ok)
		return std::string();
	return sh.api_base + "/api/v1/channels/" + hexChannelId(c.id) + "/logo";
}

/* One entry of a playlist: what a player shows, what a reader keys a guide by,
   what it draws beside the name, and what is fetched.

   THE FIELDS ARE FOR THE READ BACK. A box takes an m3u in as a channel list of
   its own (CBouquetManager::loadWebchannels) and reads tvg-id, tvg-name,
   tvg-logo, group-prefix and group-title off the line. A line carrying the
   address alone comes back as a channel with no picture, no schedule and the
   bouquet of last resort, which is why they are written although no player
   asks for any of them.

   tvg-id is the whole identifier and not the lower forty eight bits the copied
   endpoint writes. What a reader does with the field is match it against the
   channel names in the document tvg-url points at, and the guide this list
   points at names a channel by the whole of it. */
std::string entryFor(const Shared &sh, const ChannelInfo &c, const std::string &bouquet)
{
	const std::string id = hexChannelId(c.id);

	std::string out = "#EXTINF:-1 tvg-id=\"" + id + "\"";
	out += " tvg-name=\"" + withoutCommas(c.name) + "\"";
	out += " tvg-logo=\"" + logoUrlFor(sh, c) + "\"";
	out += std::string(" radio=\"") + (isRadioKind(c.kind) ? "true" : "") + "\"";
	out += " group-prefix=\"" + sh.group_prefix + "\"";
	out += " group-title=\"" + bouquet + "\",";
	out += c.name + "\n";
	out += sh.stream_prefix + id + "\n";
	return out;
}

/* The head of a playlist, naming where the schedule for what follows is read.

   A ROUTE OF THIS SERVER and never the one the copied endpoint names: a box
   reading this list in keeps the address as a guide source it fetches from then
   on, so one naming a route this server does not answer would leave every
   channel taken over without a schedule. The old address still answers, but it
   answers out of the other server and names channels by another spelling than
   the field above uses.

   The half is always named, a list of the mode the box happens to be in
   included. What was written is one half, and a guide holding both would offer
   the reader schedules for channels its list does not hold. */
std::string headerFor(const Shared &sh, bool radio)
{
	return "#EXTM3U tvg-url=\"" + sh.api_base + "/api/v1/epg/xmltv?mode=" +
	       (radio ? "radio" : "tv") + "\"\n";
}

/* One name per call, drawn from this process and a counter unique to it, so two
   requests answered at once never share a file: one being read out must never be
   the one another has just reopened and is still writing. */
std::FILE *openPlaylist(std::string &path)
{
	static long counter = 0;
	char name[64];
	std::snprintf(name, sizeof(name), "/tmp/coreapi-playlist.%d.%ld.m3u",
		      (int) getpid(), __sync_add_and_fetch(&counter, 1));

	const int fd = ::open(name, O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC, 0600);
	if (fd < 0)
		return NULL;

	std::FILE *f = fdopen(fd, "w");
	if (f == NULL)
	{
		::close(fd);
		::unlink(name);
		return NULL;
	}

	path = name;
	return f;
}

Result<std::string> notWritten()
{
	return fail<std::string>(Status::Internal, ErrorCode::ChangeRefused,
				 "the playlist could not be written to a file");
}

Result<std::string> finish(std::FILE *f, const std::string &path, bool written)
{
	// fclose flushes what fwrite buffered; a written flag that missed an earlier
	// short write must not be overridden by a close that goes on to succeed on
	// what is left.
	const bool closed = (std::fclose(f) == 0);
	if (!written || !closed)
	{
		::unlink(path.c_str());
		return notWritten();
	}
	return ok(std::string(path));
}

/* Whether the half a playlist covers is the radio half. Two of the modes the box
   counts are radio, the one played off a tuner and the one played off a url. */
Result<bool> radioWanted(Scope s)
{
	if (s == Scope::Radio)
		return ok(true);
	if (s == Scope::Tv)
		return ok(false);

	int box_mode = 0;
	Status m = channelSource().currentMode(box_mode);
	if (m == Status::NotFound)
		return fail(m, ErrorCode::NoMode,
			    "the box has not picked a mode yet");
	if (m != Status::Ok)
		return fail(m, ErrorCode::ModeUnavailable,
			    "the box mode could not be read");

	return ok(box_mode == NeutrinoModes::mode_radio || box_mode == NeutrinoModes::mode_webradio);
}

} // namespace

Result<std::string> urlFor(const std::string &authority, ChannelId id)
{
	Result<std::string> prefix = addressPrefix(authority);
	if (!prefix.ok())
		return fail(prefix.error());
	return ok(prefix.value() + hexChannelId(id));
}

Result<std::string> playlist(const std::string &authority, Scope s)
{
	Result<std::string> prefix = addressPrefix(authority);
	if (!prefix.ok())
		return fail(prefix.error());

	Result<bool> radio = radioWanted(s);
	if (!radio.ok())
		return fail(radio.error());

	BouquetList bouquets;
	Status b = channelSource().allBouquets(bouquets);
	if (b != Status::Ok)
		return fail(b, ErrorCode::BouquetListUnavailable,
			    "the bouquet list could not be read");

	std::string path;
	std::FILE *f = openPlaylist(path);
	if (f == NULL)
		return notWritten();

	const Shared sh = sharedFor(authority, prefix.value());

	bool written = writeAll(f, headerFor(sh, radio.value()));
	for (size_t i = 0; written && i < bouquets.size(); i++)
	{
		const BouquetInfo &one = bouquets[i];
		/* The two conditions the copied endpoint filters a bouquet by. A hidden
		   bouquet is one the viewer asked not to be shown and a provider
		   bouquet is one the viewer never put together. */
		if (one.hidden || !one.user_bouquet)
			continue;

		ChannelList channels;
		if (channelSource().bouquetChannels(one.id, channels) != Status::Ok)
			continue;

		for (size_t j = 0; written && j < channels.size(); j++)
		{
			// A bouquet keeps both halves and this answers one of them, which
			// is what the copied endpoint asks the bouquet for by name.
			if (isRadioKind(channels[j].kind) != radio.value())
				continue;
			written = writeAll(f, entryFor(sh, channels[j], one.name));
		}
	}

	return finish(f, path, written);
}

Result<std::string> playlistFor(const std::string &authority, ChannelId id)
{
	Result<std::string> prefix = addressPrefix(authority);
	if (!prefix.ok())
		return fail(prefix.error());

	// Read rather than taken from the caller, because what a player shows beside
	// the address is the name the box has for that channel, and a channel the
	// box does not have has no address worth handing out.
	ChannelInfo channel;
	Status s = channelSource().findChannel(id, channel);
	if (s == Status::NotFound)
		return fail(s, ErrorCode::NoSuchChannel,
			    "no channel with that id");
	if (s != Status::Ok)
		return fail(s, ErrorCode::ChannelListUnavailable,
			    "the channel list could not be read");

	std::string path;
	std::FILE *f = openPlaylist(path);
	if (f == NULL)
		return notWritten();

	const Shared sh = sharedFor(authority, prefix.value());

	/* No bouquet on a list of one. The channel was asked for by identifier and
	   which bouquets hold it was never read, and a reader meeting an entry with
	   no group puts it in the bouquet it keeps for that. */
	bool written = writeAll(f, headerFor(sh, isRadioKind(channel.kind)));
	if (written)
		written = writeAll(f, entryFor(sh, channel, std::string()));

	return finish(f, path, written);
}

} // namespace streaming
} // namespace coreapi
