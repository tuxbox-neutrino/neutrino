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

// One entry of a playlist: what a player shows and what it fetches.
std::string entryFor(const std::string &prefix, const ChannelInfo &c)
{
	return "#EXTINF:-1," + c.name + "\n" + prefix + hexChannelId(c.id) + "\n";
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

	bool written = writeAll(f, "#EXTM3U\n");
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
			written = writeAll(f, entryFor(prefix.value(), channels[j]));
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

	bool written = writeAll(f, "#EXTM3U\n");
	if (written)
		written = writeAll(f, entryFor(prefix.value(), channel));

	return finish(f, path, written);
}

} // namespace streaming
} // namespace coreapi
