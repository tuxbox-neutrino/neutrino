/*
 * test_streaming.cpp - tests for streams and playlists
 *
 * Copyright (C) 2026 NI-Team
 * Copyright (C) 2026 Tuxbox-Project (tests)
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

#include "support/catch.hpp"
#include "support/fakes.h"
#include "support/wireserver.h"

#include "httpd/auth.h"
#include "httpd/carry.h"
#include "httpd/credentials.h"
#include "httpd/endpoint.h"
#include "httpd/endpoints.h"
#include "httpd/events.h"
#include "httpd/livestream.h"
#include "httpd/netmatch.h"
#include "httpd/webconfig.h"
#include "httpd/http.h"
#include "httpd/doc/openapi.h"
#include "httpd/router.h"
#include "httpd/status.h"

#include "coreapi/base/errors.h"
#include "coreapi/base/result.h"
#include "coreapi/epg.h"
#include "coreapi/settings/settings.h"
#include "coreapi/streaming.h"
#include "coreapi/base/types.h"

#include <atomic>
#include <cerrno>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <utility>
#include <vector>

#include <stdint.h>

#include <neutrinoMessages.h>

#include <arpa/inet.h>
#include <signal.h>
#include <dirent.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

using namespace httpd;

namespace
{

// Every byte a call left in the file it named, read back, and the name taken
// away afterwards: removing it is what the call hands to whoever asked, so a
// case reading the content does that part itself.
std::string readAndRemove(const std::string &path)
{
	std::string out;
	FILE *f = std::fopen(path.c_str(), "rb");
	REQUIRE(f != NULL);
	char buf[4096];
	size_t n;
	while ((n = std::fread(buf, 1, sizeof(buf), f)) > 0)
		out.append(buf, n);
	std::fclose(f);
	::unlink(path.c_str());
	return out;
}

size_t countOf(const std::string &haystack, const std::string &needle)
{
	size_t n = 0;
	for (std::string::size_type at = haystack.find(needle); at != std::string::npos;
	     at = haystack.find(needle, at + 1))
		++n;
	return n;
}

// The pid in the name narrows a look in /tmp to this process: a part running
// beside this one makes playlists of the same scheme.
std::string ownPlaylistPrefix()
{
	char mine[64];
	std::snprintf(mine, sizeof(mine), "coreapi-playlist.%d.", (int) ::getpid());
	return mine;
}

// How many of the files this call's own naming scheme could have made are
// sitting in /tmp right now. Matched by prefix and not by a fixed name, because
// the path carries this process's id and a counter of its own.
size_t playlistFilesInTmp()
{
	DIR *d = ::opendir("/tmp");
	if (d == NULL)
		return 0;
	const std::string prefix = ownPlaylistPrefix();
	size_t n = 0;
	for (struct dirent *e = ::readdir(d); e != NULL; e = ::readdir(d))
		if (std::strncmp(e->d_name, prefix.c_str(), prefix.size()) == 0)
			++n;
	::closedir(d);
	return n;
}

/* The head one half of the list is written under. Built here from the pieces a
   case names rather than transcribed, because what every case below is about is
   which authority and which half went into it. */
std::string headOf(const char *authority, const char *half)
{
	return std::string("#EXTM3U tvg-url=\"http://") + authority +
	       "/api/v1/epg/xmltv?mode=" + half + "\"\n";
}

/* What this machine is called, read the same way the playlist reads it. A case
   comparing the two is about the field carrying this box's name and not about
   the name itself, which is whatever the machine running the case is called. */
std::string thisHostName()
{
	char name[256];
	if (::gethostname(name, sizeof(name)) != 0)
		return std::string();
	name[sizeof(name) - 1] = '\0';
	return std::string(name);
}

coreapi::ChannelInfo makeChannel(uint64_t id, const char *name, coreapi::ServiceKind kind)
{
	coreapi::ChannelInfo c;
	c.id = id;
	c.name = name;
	c.kind = kind;
	return c;
}

/* One bouquet a viewer put together and can see, one it hid, and one the provider sent:
   the three states a bouquet this fixture holds can be in, and the two whose channels
   have no place in a list meant to be what the viewer sees. The port is set rather than
   left to the row's own default, so a case reading an address back is reading a number
   this fixture put there. */
struct Fixture
{
	InstalledDependencies deps;
	FakeSettingsSource settings;
	InstalledSettingsSource in_settings;

	static const uint64_t kShown1 = 0x1001;
	static const uint64_t kShown2 = 0x1002;
	static const uint64_t kShownRadio = 0x2001;
	// In a bouquet, and called something carrying the byte that tells the
	// fields of an entry from the title behind them.
	static const uint64_t kComma = 0x1003;
	static const uint64_t kHidden = 0x3001;
	static const uint64_t kProvider = 0x4001;
	// Wider than thirty two bits, and in no bouquet: what a request asks for by
	// identifier is a channel the box has, and holding one no list here carries
	// keeps the playlists above reading the same.
	static const uint64_t kWide = 0xb544000800850088ULL;
	// In no bouquet either, for the same reason, and called something a file
	// name cannot be called.
	static const uint64_t kOdd = 0x5001;

	Fixture() : in_settings(&settings)
	{
		settings.ints["streaming_port"] = 31339;
		/* Off, so a case that switches the picture says so; what a box with
		   no line for it does is a case of its own. */
		settings.ints["streaming_switch_picture"] = 0;

		coreapi::BouquetInfo shown;
		shown.id = 1;
		shown.name = "Meine";
		shown.user_bouquet = true;
		deps.channels.bouquets.push_back(shown);

		coreapi::BouquetInfo hidden;
		hidden.id = 2;
		hidden.name = "Versteckt";
		hidden.hidden = true;
		hidden.user_bouquet = true;
		deps.channels.bouquets.push_back(hidden);

		coreapi::BouquetInfo provider;
		provider.id = 3;
		provider.name = "Provider";
		provider.user_bouquet = false;
		deps.channels.bouquets.push_back(provider);

		coreapi::ChannelList shown_members;
		shown_members.push_back(makeChannel(kShown1, "First", coreapi::ServiceKind::Tv));
		shown_members.push_back(makeChannel(kShown2, "Second", coreapi::ServiceKind::Tv));
		shown_members.push_back(makeChannel(kComma, "Eins, Zwei", coreapi::ServiceKind::Tv));
		shown_members.push_back(makeChannel(kShownRadio, "Tunes", coreapi::ServiceKind::Radio));
		deps.channels.bouquet_members[1] = shown_members;

		coreapi::ChannelList hidden_members;
		hidden_members.push_back(makeChannel(kHidden, "HiddenChannel", coreapi::ServiceKind::Tv));
		deps.channels.bouquet_members[2] = hidden_members;

		coreapi::ChannelList provider_members;
		provider_members.push_back(makeChannel(kProvider, "ProviderChannel", coreapi::ServiceKind::Tv));
		deps.channels.bouquet_members[3] = provider_members;

		deps.channels.channels = shown_members;
		deps.channels.channels.push_back(makeChannel(kWide, "Wide", coreapi::ServiceKind::Tv));
		/* A name carrying what neither a file name nor a header line can hold:
		   a separator, quotation marks, a byte above the seventh bit and the
		   two that end a line. Every one of those is something a channel really
		   can be called, the name coming out of the stream and not out of
		   anything on this box, and in no bouquet so that the lists above read
		   the same. */
		deps.channels.channels.push_back(
			makeChannel(kOdd, "A/B \"C\" \xc3\xb6\r\nX: y", coreapi::ServiceKind::Tv));
		// The picture shows Second, which needs no PIN: a switch away from it may be weighed.
		deps.channels.current = deps.channels.channels[1];
		deps.channels.current_status = coreapi::Status::Ok;
	}

	private:
		Fixture(const Fixture &);
		Fixture &operator=(const Fixture &);
};

} // namespace

TEST_CASE("an address names the authority the caller gave", "[streaming]")
{
	Fixture fx;

	const std::string url =
		coreapi::streaming::urlFor("box.example:8080", 0xb544000800850088ULL).value();
	REQUIRE(url == "http://box.example:31339/id=b544000800850088");
}

TEST_CASE("an address without an authority is refused, not built", "[streaming]")
{
	/* The whole reason this parameter exists: http://:31339/id=... is what a caller gets
	   from a server that filled nothing in, and there is no way for that caller to tell.
	   No settings source here on purpose: a refusal that came out of reading the port
	   would carry the status of a store that could not be read, so the status asked for
	   below is what says the authority was looked at first. */
	InstalledDependencies deps;
	const coreapi::Result<std::string> r =
		coreapi::streaming::urlFor("", 0xb544000800850088ULL);
	REQUIRE_FALSE(r.ok());
	REQUIRE(r.error().status == coreapi::Status::InvalidArgument);
	REQUIRE(r.error().code == coreapi::ErrorCode::NoAuthority);
}

TEST_CASE("the port the box carries is the port in the address", "[streaming]")
{
	// Not the constant: a box whose owner moved the stream to another port
	// would otherwise be handed an address nothing listens on.
	Fixture fx;
	fx.settings.ints["streaming_port"] = 8899;
	REQUIRE(coreapi::streaming::urlFor("box", 1).value() == "http://box:8899/id=1");
}

TEST_CASE("an authority that already carries a port loses it", "[streaming]")
{
	Fixture fx;

	REQUIRE(coreapi::streaming::urlFor("box.example:8080", 1).value()
	        == "http://box.example:31339/id=1");
	// The bracketed form, whose colons belong to the address and not to a port
	REQUIRE(coreapi::streaming::urlFor("[fd00::1]:8080", 1).value()
	        == "http://[fd00::1]:31339/id=1");
	REQUIRE(coreapi::streaming::urlFor("[fd00::1]", 1).value()
	        == "http://[fd00::1]:31339/id=1");
	// A name carrying no port at all keeps every byte of itself
	REQUIRE(coreapi::streaming::urlFor("box.example", 1).value()
	        == "http://box.example:31339/id=1");
}

TEST_CASE("the playlist holds every visible user bouquet and nothing else", "[streaming]")
{
	Fixture fx;

	const std::string path =
		coreapi::streaming::playlist("box", coreapi::streaming::Scope::Tv).value();
	const std::string doc = readAndRemove(path);

	REQUIRE(doc.compare(0, headOf("box", "tv").size(), headOf("box", "tv")) == 0);
	REQUIRE(countOf(doc, "#EXTINF:-1 ") == 3);
	// By name and not by count: a filter dropped on either of these would leave
	// a list of the same shape holding channels nobody asked to see.
	REQUIRE(doc.find("HiddenChannel") == std::string::npos);
	REQUIRE(doc.find("ProviderChannel") == std::string::npos);
	REQUIRE(doc.find("First") != std::string::npos);
	REQUIRE(doc.find("Second") != std::string::npos);
}

TEST_CASE("every line of the playlist carries the address a player fetches", "[streaming]")
{
	Fixture fx;

	const std::string path =
		coreapi::streaming::playlist("box.example:8080", coreapi::streaming::Scope::Tv).value();
	const std::string doc = readAndRemove(path);

	REQUIRE(doc.find("http://box.example:31339/id=1001\n") != std::string::npos);
	REQUIRE(doc.find("http://box.example:31339/id=1002\n") != std::string::npos);
}

TEST_CASE("which half the playlist covers is the half that was asked for", "[streaming]")
{
	Fixture fx;

	{
		const std::string doc = readAndRemove(
			coreapi::streaming::playlist("box", coreapi::streaming::Scope::Radio).value());
		REQUIRE(doc.find("Tunes") != std::string::npos);
		REQUIRE(doc.find("First") == std::string::npos);
	}
	{
		const std::string doc = readAndRemove(
			coreapi::streaming::playlist("box", coreapi::streaming::Scope::Tv).value());
		REQUIRE(doc.find("First") != std::string::npos);
		REQUIRE(doc.find("Tunes") == std::string::npos);
	}
}

TEST_CASE("a playlist of the current mode follows the box and not a guess", "[streaming]")
{
	Fixture fx;

	fx.deps.channels.mode = NeutrinoModes::mode_radio;
	{
		const std::string doc = readAndRemove(
			coreapi::streaming::playlist("box", coreapi::streaming::Scope::CurrentMode).value());
		REQUIRE(doc.find("Tunes") != std::string::npos);
		REQUIRE(doc.find("First") == std::string::npos);
	}

	fx.deps.channels.mode = NeutrinoModes::mode_tv;
	{
		const std::string doc = readAndRemove(
			coreapi::streaming::playlist("box", coreapi::streaming::Scope::CurrentMode).value());
		REQUIRE(doc.find("First") != std::string::npos);
		REQUIRE(doc.find("Tunes") == std::string::npos);
	}
}

TEST_CASE("a playlist of one channel names that channel and nothing else", "[streaming]")
{
	Fixture fx;

	const std::string doc = readAndRemove(
		coreapi::streaming::playlistFor("box", Fixture::kShown1).value());

	REQUIRE(doc.compare(0, headOf("box", "tv").size(), headOf("box", "tv")) == 0);
	REQUIRE(countOf(doc, "#EXTINF:-1 ") == 1);
	REQUIRE(doc.find(",First\n") != std::string::npos);
	REQUIRE(doc.find("http://box:31339/id=1001\n") != std::string::npos);
	REQUIRE(doc.find("Second") == std::string::npos);
}

TEST_CASE("the head of a playlist names the guide of this server", "[streaming]")
{
	/* What a box reading this list in keeps is the address in this line, and it
	   fetches every schedule from it from then on. A line naming the route the
	   copied endpoint answers would send it to the other server, which names a
	   channel by another spelling than the entries below do. */
	Fixture fx;

	/* The guide is answered here, so its address keeps the port the request
	   arrived on; the stream is answered by the box's own server, so that
	   address takes the port the box streams on. Both in one document. */
	const std::string doc = readAndRemove(
		coreapi::streaming::playlist("box.example:8080", coreapi::streaming::Scope::Tv).value());
	const std::string head = headOf("box.example:8080", "tv");
	REQUIRE(doc.compare(0, head.size(), head) == 0);
	REQUIRE(doc.find("http://box.example:31339/id=1001\n") != std::string::npos);
	REQUIRE(doc.find("/control/xmltv") == std::string::npos);
}

TEST_CASE("the half the head names is the half that was written", "[streaming]")
{
	Fixture fx;

	{
		const std::string doc = readAndRemove(
			coreapi::streaming::playlist("box", coreapi::streaming::Scope::Radio).value());
		const std::string head = headOf("box", "radio");
		REQUIRE(doc.compare(0, head.size(), head) == 0);
	}

	/* The mode the box happens to be in is written out as the half it stands
	   for. A guide of both halves would offer schedules for channels this list
	   does not hold. */
	fx.deps.channels.mode = NeutrinoModes::mode_radio;
	{
		const std::string doc = readAndRemove(
			coreapi::streaming::playlist("box", coreapi::streaming::Scope::CurrentMode).value());
		const std::string head = headOf("box", "radio");
		REQUIRE(doc.compare(0, head.size(), head) == 0);
	}
	fx.deps.channels.mode = NeutrinoModes::mode_tv;
	{
		const std::string doc = readAndRemove(
			coreapi::streaming::playlist("box", coreapi::streaming::Scope::CurrentMode).value());
		const std::string head = headOf("box", "tv");
		REQUIRE(doc.compare(0, head.size(), head) == 0);
	}
}

TEST_CASE("an entry carries every field a box reading it back in needs", "[streaming]")
{
	/* The whole line, byte for byte, and not the fields one at a time: what a
	   reader takes off it is the order and the spelling together, and a case
	   asking after each field on its own passes on a line no reader can
	   read. */
	Fixture fx;
	fx.deps.logos.put(Fixture::kShown1, "/share/tuxbox/neutrino/icons/logo/1001.png");

	const std::string doc = readAndRemove(
		coreapi::streaming::playlist("box.example:8080", coreapi::streaming::Scope::Tv).value());

	const std::string entry =
		"#EXTINF:-1 tvg-id=\"1001\" tvg-name=\"First\""
		" tvg-logo=\"http://box.example:8080/api/v1/channels/1001/logo\""
		" radio=\"\" group-prefix=\"" + thisHostName() + "\""
		" group-title=\"Meine\",First\n"
		"http://box.example:31339/id=1001\n";
	REQUIRE(doc.find(entry) != std::string::npos);
}

TEST_CASE("a channel with no picture is handed no address for one", "[streaming]")
{
	Fixture fx;

	const unsigned looked_before = fx.deps.logos.searches;
	const std::string doc = readAndRemove(
		coreapi::streaming::playlist("box", coreapi::streaming::Scope::Tv).value());

	// The field is written and empty, and no address for a picture is in the
	// document at all
	REQUIRE(countOf(doc, "tvg-logo=\"\"") == 3);
	REQUIRE(doc.find("/logo") == std::string::npos);
	// And the search really ran: without this the case above is green whether
	// or not anything ever looked
	REQUIRE(fx.deps.logos.searches > looked_before);
}

TEST_CASE("the address of a picture is the route that hands one over", "[streaming]")
{
	/* Not the path the picture has on this box's disc, which is what the copied
	   endpoint writes: this server hands over the web directory and one name
	   beside it, so that path is a picture nothing answers with. */
	Fixture fx;
	fx.deps.logos.put(Fixture::kShown2, "/share/tuxbox/neutrino/icons/logo/1002.png");

	const std::string doc = readAndRemove(
		coreapi::streaming::playlist("box", coreapi::streaming::Scope::Tv).value());

	REQUIRE(doc.find("tvg-logo=\"http://box/api/v1/channels/1002/logo\"") != std::string::npos);
	REQUIRE(doc.find("/share/tuxbox") == std::string::npos);
}

TEST_CASE("the separator is kept out of the fields of an entry", "[streaming]")
{
	/* A comma is what tells the fields from the title behind them, so one
	   inside a field would leave a reader taking the rest of the fields for the
	   title. The title itself is the name as it is: it is the rest of the
	   line. */
	Fixture fx;

	const std::string doc = readAndRemove(
		coreapi::streaming::playlist("box", coreapi::streaming::Scope::Tv).value());

	REQUIRE(doc.find("tvg-name=\"Eins. Zwei\"") != std::string::npos);
	REQUIRE(doc.find(",Eins, Zwei\n") != std::string::npos);
}

TEST_CASE("which half a channel is in is said on the line", "[streaming]")
{
	Fixture fx;

	{
		const std::string doc = readAndRemove(
			coreapi::streaming::playlist("box", coreapi::streaming::Scope::Radio).value());
		REQUIRE(doc.find(" radio=\"true\"") != std::string::npos);
		REQUIRE(doc.find(" radio=\"\"") == std::string::npos);
	}
	{
		const std::string doc = readAndRemove(
			coreapi::streaming::playlist("box", coreapi::streaming::Scope::Tv).value());
		REQUIRE(doc.find(" radio=\"\"") != std::string::npos);
		REQUIRE(doc.find(" radio=\"true\"") == std::string::npos);
	}
}

TEST_CASE("the identifier an entry is keyed by is the one the guide writes", "[streaming]")
{
	/* THE ONE THING THAT MAKES THE TWO DOCUMENTS ONE. A box reading the list in
	   keeps tvg-id and matches it against the channel names in the guide the
	   head points at. The copied endpoint wrote the lower forty eight bits in
	   both of its documents; this server writes the whole identifier in its
	   guide, so a line carrying the short one would leave every channel taken
	   over without a schedule. Read out of both documents rather than
	   transcribed from either. */
	Fixture fx;

	const std::string list = readAndRemove(
		coreapi::streaming::playlist("box", coreapi::streaming::Scope::Tv).value());
	const std::string guide = readAndRemove(
		coreapi::epg::xmltv(coreapi::epg::Kind::Tv).value());

	REQUIRE(list.find("tvg-id=\"1001\"") != std::string::npos);
	REQUIRE(guide.find("<channel id=\"1001\">") != std::string::npos);
}

TEST_CASE("a playlist for a channel the box does not have is refused", "[streaming]")
{
	Fixture fx;

	const size_t before = playlistFilesInTmp();
	const coreapi::Result<std::string> r =
		coreapi::streaming::playlistFor("box", 0xdeadbeefULL);
	REQUIRE_FALSE(r.ok());
	REQUIRE(r.error().status == coreapi::Status::NotFound);
	REQUIRE(r.error().code == coreapi::ErrorCode::NoSuchChannel);
	// And nothing was written before the lookup said so
	REQUIRE(playlistFilesInTmp() == before);
}

TEST_CASE("a playlist without an authority is refused before anything is written", "[streaming]")
{
	Fixture fx;

	const size_t before = playlistFilesInTmp();
	const coreapi::Result<std::string> r =
		coreapi::streaming::playlist("", coreapi::streaming::Scope::Tv);
	REQUIRE_FALSE(r.ok());
	REQUIRE(r.error().code == coreapi::ErrorCode::NoAuthority);
	REQUIRE(playlistFilesInTmp() == before);
}

namespace
{

/* A fixture of many small bouquets rather than one large one, so that what a
   call holds at once is one bouquet's channels however many there are
   altogether: the list a source hands over is copied per bouquet. */
void buildManyChannels(FakeChannelSource &channels, size_t bouquets, size_t per_bouquet)
{
	channels.bouquets.clear();
	channels.bouquet_members.clear();

	uint64_t id = 0x9000;
	for (size_t b = 0; b < bouquets; ++b)
	{
		coreapi::BouquetInfo one;
		one.id = (uint32_t)(b + 1);
		one.user_bouquet = true;
		channels.bouquets.push_back(one);

		coreapi::ChannelList members;
		members.reserve(per_bouquet);
		for (size_t c = 0; c < per_bouquet; ++c)
			members.push_back(makeChannel(id++, "chan", coreapi::ServiceKind::Tv));
		channels.bouquet_members[one.id] = members;
	}
}

// How many bytes the file this call is writing holds right now. Its name is not
// known here, and the prefix is, which is the same reason the counter of them
// above matches by prefix.
off_t playlistBytesSoFar()
{
	DIR *d = ::opendir("/tmp");
	if (d == NULL)
		return 0;

	const std::string prefix = ownPlaylistPrefix();
	off_t largest = 0;
	for (struct dirent *e = ::readdir(d); e != NULL; e = ::readdir(d))
	{
		if (std::strncmp(e->d_name, prefix.c_str(), prefix.size()) != 0)
			continue;
		struct stat st;
		const std::string path = std::string("/tmp/") + e->d_name;
		if (::stat(path.c_str(), &st) == 0 && st.st_size > largest)
			largest = st.st_size;
	}
	::closedir(d);
	return largest;
}

/* Looks at the file while the call that is writing it is still asking for
   bouquets. A generator that writes each line out as it is built has most of
   the document on disc by the time it asks for the last one; a generator that
   holds the whole of it in memory first has nothing there until it is done. */
struct PeekingChannelSource : public FakeChannelSource
{
	mutable off_t most_seen;

	PeekingChannelSource() : most_seen(0) {}

	coreapi::Status bouquetChannels(uint32_t id, coreapi::ChannelList &out) const
	{
		const off_t now = playlistBytesSoFar();
		if (now > most_seen)
			most_seen = now;
		return FakeChannelSource::bouquetChannels(id, out);
	}
};

} // namespace

TEST_CASE("the playlist is written as it is built and not held in one piece", "[streaming]")
{
	/* The reason it is written to a file at all, measured on the file rather than on
	   this process's memory: a peak reading only moves when what is live passes where
	   the process has already been, so a reading taken late in a run says nothing. What
	   the file holds part way through says it outright.

	   Two thousand bouquets of fifty channels, which is a document of several megabytes:
	   the buffer between this layer and the disc is four kilobytes, so a generator
	   writing as it goes has all but the last few kilobytes on disc long before it stops
	   asking for bouquets. */
	PeekingChannelSource channels;
	InstalledChannelSource in_channels(&channels);
	// Every entry asks after a picture, and this fixture names none, so what is
	// measured below is the writing and not a search of the filesystem.
	FakeLogoSource logos;
	InstalledLogoSource in_logos(&logos);
	FakeSettingsSource settings;
	InstalledSettingsSource in_settings(&settings);
	settings.ints["streaming_port"] = 31339;

	buildManyChannels(channels, 2000, 50);

	const coreapi::Result<std::string> got =
		coreapi::streaming::playlist("box", coreapi::streaming::Scope::Tv);
	REQUIRE(got.ok());

	struct stat st;
	REQUIRE(::stat(got.value().c_str(), &st) == 0);
	::unlink(got.value().c_str());

	// The document really is the size this case rests on
	REQUIRE(st.st_size > 4000000);
	// And most of it was on disc while the source was still being asked
	REQUIRE(channels.most_seen > st.st_size / 2);
}

// ================================================================ who may ask

namespace
{

const char kWriteToken[] = "3333333333333333cccccccccccccccc";

/* The shipped routes in front of nhttpd and the bridge, on a port the kernel
   picks, over the channels and the port of the fixture above.

   A program a case names runs in place of the converter; one the case does
   not name is the suite's own, which is not there. The streaming port of the
   fixture is one nothing listens on, so not even a program that ran as
   named could reach a box on this machine. */
struct StreamBox
{
	Fixture     fx;
	WireServer  server;
	const char *converter_before;
	std::vector<std::string> files;

	StreamBox()
		: server(4, 100, WireServer::Threaded),
		  converter_before(livestream::converterForTest())
	{
		fx.settings.ints["streaming_port"] = 9;
		addApiToken(tokenLookupPrefix(kWriteToken), hashSecret(kWriteToken), AuthLevel::Write, "", 0);
	}

	~StreamBox()
	{
		carry::waitIdle(5000);
		livestream::setConverterForTest(converter_before);
		livestream::setFirstByteMsForTest(0);
		carry::setCarrier(carry::Program, NULL);
		setSessionClockForTest(0);
		setConfigForTest(defaultWebConfig());
		forgetApiTokens();
		for (size_t i = 0; i < files.size(); ++i)
			::unlink(files[i].c_str());
	}

	int port() const { return server.port; }

	// A file of this case's own in /tmp, removed with the box.
	std::string tempFile(const char *what)
	{
		char path[96];
		std::snprintf(path, sizeof(path), "/tmp/coreapi-stream.%d.%u.%s", (int) ::getpid(),
		              (unsigned) files.size(), what);
		files.push_back(path);
		::unlink(path);
		return path;
	}

	/* A shell script run in place of the converter. PIDFILE in it is
	   replaced by a file the script can write its process id to. */
	void useConverter(const std::string &script, std::string *pidfile = NULL)
	{
		const std::string pf = tempFile("pid");
		std::string body = script;
		for (size_t at = body.find("PIDFILE"); at != std::string::npos; at = body.find("PIDFILE"))
			body.replace(at, 7, pf);
		const std::string path = tempFile("sh");
		FILE *f = std::fopen(path.c_str(), "w");
		REQUIRE(f != NULL);
		std::fputs(("#!/bin/sh\n" + body).c_str(), f);
		std::fclose(f);
		REQUIRE(::chmod(path.c_str(), 0755) == 0);
		scripts.push_back(path);
		livestream::setConverterForTest(scripts.back().c_str());
		if (pidfile != NULL)
			*pidfile = pf;
	}

	std::vector<std::string> scripts;

	private:
		StreamBox(const StreamBox &);
		StreamBox &operator=(const StreamBox &);
};

std::string askAs(const char *method, const std::string &path, const std::string &headers)
{
	return std::string(method) + " " + path + " HTTP/1.0\r\nHost: box\r\n" + headers + "\r\n";
}

std::string bearer(const std::string &token)
{
	return "Authorization: Bearer " + token + "\r\n";
}

std::string problemCodeOf(const std::string &body)
{
	const std::string mark = "\"type\":\"/errors/";
	const size_t at = body.find(mark);
	if (at == std::string::npos)
		return std::string();
	const size_t from = at + mark.size();
	return body.substr(from, body.find('"', from) - from);
}

const char kBrowser[] = "/api/v1/stream/browser/1001?video=h264&audio=mp2";

std::string asWriter(const char *method, const std::string &path)
{
	return askAs(method, path, bearer(kWriteToken));
}

// The process id a script wrote, waiting up to ms for it; 0 for none.
pid_t pidFrom(const std::string &file, int ms)
{
	for (int waited = 0; waited <= ms; waited += 20)
	{
		FILE *f = std::fopen(file.c_str(), "r");
		if (f != NULL)
		{
			long v = 0;
			const bool got = std::fscanf(f, "%ld", &v) == 1;
			std::fclose(f);
			if (got && v > 0)
				return (pid_t) v;
		}
		napMs(20);
	}
	return 0;
}

// Whether the process is gone and collected within ms: a zombie still answers.
bool goneWithin(pid_t pid, int ms)
{
	for (int waited = 0; waited <= ms; waited += 20)
	{
		if (::kill(pid, 0) != 0 && errno == ESRCH)
			return true;
		napMs(20);
	}
	return false;
}

size_t runningAfter(int ms)
{
	for (int waited = 0; waited < ms && carry::running() != 0; waited += 20)
		napMs(20);
	return carry::running();
}

// A channel source that counts what the routes and the jobs ask of it.
struct CountingChannels : public FakeChannelSource
{
	mutable std::atomic<int> finds;
	CountingChannels() : finds(0) {}
	coreapi::Status findChannel(coreapi::ChannelId id, coreapi::ChannelInfo &out) const
	{
		++finds;
		return FakeChannelSource::findChannel(id, out);
	}
};

} // namespace

TEST_CASE("a browser stream answers a session or a token that may write, and no reader",
          "[streaming][wire][auth]")
{
	StreamBox box;

	// this machine reads as the home network does, without presenting anything
	WebConfig c = defaultWebConfig();
	NetPrefix here;
	REQUIRE(parsePrefix("127.0.0.0/8", &here));
	c.lan_read.push_back(here);
	setConfigForTest(c);

	const std::string reader = "1111111111111111aaaaaaaaaaaaaaaa";
	const std::string owner = "2222222222222222bbbbbbbbbbbbbbbb";
	addApiToken(tokenLookupPrefix(reader), hashSecret(reader), AuthLevel::Read, "", 0);
	addApiToken(tokenLookupPrefix(owner), hashSecret(owner), AuthLevel::System, "", 0);
	const std::string media = openScopedToken(mediaScopeName(), AuthLevel::System);
	REQUIRE(!media.empty());

	const time_t t0 = 1800000000;
	setSessionClockForTest(t0);
	const std::string session = openSession("root");
	REQUIRE(!session.empty());
	const std::string cookie = "Cookie: " + std::string(sessionCookieName()) + "=" + session + "\r\n";

	struct Who
	{
		const char *name;
		std::string headers;
		std::string query;
		bool through;
	};
	const Who who[] = {
		{ "the home network alone", "", "", false },
		{ "a token that may read", bearer(reader), "", false },
		{ "a token that may write", bearer(kWriteToken), "", true },
		{ "a token of the system", bearer(owner), "", true },
		{ "a token scoped to media", bearer(media), "", false },
		{ "a session", cookie, "", true },
		{ "a scoped token in the address", "", "&token=" + media, false },
		{ "a standing token in the address", "", "&token=" + std::string(kWriteToken), false },
	};
	const std::string browser = "/api/v1/stream/browser/1001?video=h264&audio=mp2";
	const char *const methods[] = { "GET", "HEAD" };
	for (size_t w = 0; w < sizeof(who) / sizeof(who[0]); ++w)
	{
		for (size_t m = 0; m < 2; ++m)
		{
			INFO(who[w].name << ": " << methods[m]);
			WireStream r(box.port(), askAs(methods[m], browser + who[w].query, who[w].headers).c_str());
			REQUIRE(r.ok());
			const std::string body = r.readToEof(5000);
			if (who[w].through)
			{
				CHECK(r.code() != 403);
				CHECK(r.code() != 0);
			}
			else
			{
				CHECK(r.code() == 403);
				if (methods[m][0] == 'G')
					CHECK(problemCodeOf(body) == "not-permitted");
			}
		}
	}

	// The playlists and a channel's address stay a read: the home network
	// alone gets them.
	const char *const reads[] = {
		"/api/v1/stream/playlist", "/api/v1/stream/playlist.m3u",
		"/api/v1/stream/playlist/1001", "/api/v1/stream/1001"
	};
	for (size_t i = 0; i < sizeof(reads) / sizeof(reads[0]); ++i)
	{
		INFO("read " << reads[i]);
		WireStream r(box.port(), askAs("GET", reads[i], "").c_str());
		REQUIRE(r.ok());
		r.readToEof(5000);
		CHECK(r.code() == 200);
	}

	// a session that has run out is nothing
	setSessionClockForTest(t0 + (time_t) c.session_lifetime_s + 60);
	for (size_t m = 0; m < 2; ++m)
	{
		INFO("a session that ran out: " << methods[m]);
		WireStream r(box.port(), askAs(methods[m], browser, cookie).c_str());
		REQUIRE(r.ok());
		r.readToEof(5000);
		CHECK(r.code() == 403);
	}
}

// ================================================================ the program

TEST_CASE("a program's output is the body, and the program ends with its reader",
          "[streaming][wire][program]")
{
	StreamBox box;
	CountingChannels channels;
	channels.channels = box.fx.deps.channels.channels;
	InstalledChannelSource counted(&channels);
	std::string pidfile;
	box.useConverter("echo $$ > PIDFILE\nwhile :; do printf 'G0123456789abcdef'; sleep 0.02; done\n",
	                 &pidfile);

	pid_t pid = 0;
	{
		WireStream r(box.port(), asWriter("GET", kBrowser).c_str());
		REQUIRE(r.ok());
		CHECK(r.code() == 200);
		CHECK(r.headerOf("Content-Type").find("video/mp2t") != std::string::npos);
		CHECK(r.headerOf("Cache-Control").find("no-store") != std::string::npos);
		CHECK_FALSE(r.hasHeader("Content-Length"));
		pid = pidFrom(pidfile, 3000);
		REQUIRE(pid > 0);

		// What the job asks of the box once the program runs: nothing.
		const int asked = channels.finds.load();
		CHECK_FALSE(r.closedWithin(600));
		CHECK(r.body.compare(0, 6, "G01234") == 0);
		CHECK(r.body.size() > 100u);
		CHECK(channels.finds.load() == asked);
		CHECK(carry::seatsTaken(carry::Program) == 1u);
	}

	// The reader went: the program and its seat are gone within a second.
	CHECK(goneWithin(pid, 1000));
	CHECK(runningAfter(1000) == 0);
	CHECK(carry::seatsTaken(carry::Program) == 0u);
}

TEST_CASE("a reader that leaves a quiet program frees its seat within a second",
          "[streaming][wire][program]")
{
	/* The program wrote once and says nothing more: no write fails that
	   could tell the job its reader went, so the job has to ask. */
	StreamBox box;
	std::string pidfile;
	box.useConverter("echo $$ > PIDFILE\nprintf G\nexec sleep 30\n", &pidfile);

	pid_t pid = 0;
	{
		WireStream r(box.port(), asWriter("GET", kBrowser).c_str());
		REQUIRE(r.ok());
		CHECK(r.code() == 200);
		pid = pidFrom(pidfile, 3000);
		REQUIRE(pid > 0);
		CHECK_FALSE(r.closedWithin(300));
		CHECK(r.body == "G");
	}
	CHECK(goneWithin(pid, 1000));
	CHECK(runningAfter(1000) == 0);
}

TEST_CASE("a HEAD of a browser stream says what the GET would meet and starts nothing",
          "[streaming][wire][program]")
{
	StreamBox box;
	std::string pidfile;
	box.useConverter("echo $$ > PIDFILE\nexec sleep 30\n", &pidfile);

	WireStream r(box.port(), asWriter("HEAD", kBrowser).c_str());
	REQUIRE(r.ok());
	CHECK(r.code() == 200);
	CHECK(r.headerOf("Content-Type").find("video/mp2t") != std::string::npos);
	CHECK(r.closedWithin(2000));
	CHECK(r.body.empty());
	CHECK(pidFrom(pidfile, 300) == 0);
	CHECK(carry::running() == 0u);
}

TEST_CASE("a third program is refused while two run", "[streaming][wire][program]")
{
	StreamBox box;
	std::string first_pid;
	box.useConverter("echo $$ >> PIDFILE\nwhile :; do printf 'G'; sleep 0.05; done\n", &first_pid);

	WireStream a(box.port(), asWriter("GET", kBrowser).c_str());
	WireStream b(box.port(), asWriter("GET", kBrowser).c_str());
	REQUIRE(a.ok());
	REQUIRE(b.ok());
	CHECK(a.code() == 200);
	CHECK(b.code() == 200);
	CHECK(carry::seatsTaken(carry::Program) == 2u);

	for (int m = 0; m < 2; ++m)
	{
		const char *method = m == 0 ? "GET" : "HEAD";
		INFO(method);
		WireStream c(box.port(), asWriter(method, kBrowser).c_str());
		REQUIRE(c.ok());
		const std::string body = c.readToEof(3000);
		CHECK(c.code() == 503);
		CHECK(c.headerOf("Retry-After").find("15") != std::string::npos);
		if (m == 0)
			CHECK(problemCodeOf(body) == "too-many-conversions");
	}
}

TEST_CASE("a box without the program refuses a browser stream with 501", "[streaming][wire][program]")
{
	StreamBox box;
	livestream::setConverterForTest("/nonexistent/converter");

	for (int m = 0; m < 2; ++m)
	{
		const char *method = m == 0 ? "GET" : "HEAD";
		INFO(method);
		WireStream r(box.port(), asWriter(method, kBrowser).c_str());
		REQUIRE(r.ok());
		const std::string body = r.readToEof(3000);
		CHECK(r.code() == 501);
		if (m == 0)
			CHECK(problemCodeOf(body) == "not-playable-in-browser");
	}
	CHECK(carry::running() == 0u);
}

TEST_CASE("a program that ends before its first byte is refused, and collected",
          "[streaming][wire][program]")
{
	StreamBox box;
	std::string pidfile;
	box.useConverter("echo $$ > PIDFILE\nexit 3\n", &pidfile);

	WireStream r(box.port(), asWriter("GET", kBrowser).c_str());
	REQUIRE(r.ok());
	const std::string body = r.readToEof(5000);
	CHECK(r.code() == 503);
	CHECK(problemCodeOf(body) == "upstream-unreachable");
	const pid_t pid = pidFrom(pidfile, 1000);
	REQUIRE(pid > 0);
	CHECK(goneWithin(pid, 1000));
	CHECK(runningAfter(1000) == 0);
}

TEST_CASE("a program that stays silent is refused when its time is up, and ended",
          "[streaming][wire][program]")
{
	StreamBox box;
	livestream::setFirstByteMsForTest(400);
	std::string pidfile;
	box.useConverter("echo $$ > PIDFILE\nexec sleep 30\n", &pidfile);

	const int64_t t0 = wireNowMs();
	WireStream r(box.port(), asWriter("GET", kBrowser).c_str());
	REQUIRE(r.ok());
	const std::string body = r.readToEof(5000);
	CHECK(r.code() == 503);
	CHECK(problemCodeOf(body) == "upstream-unreachable");
	CHECK(wireNowMs() - t0 < 3000);
	const pid_t pid = pidFrom(pidfile, 1000);
	REQUIRE(pid > 0);
	CHECK(goneWithin(pid, 1000));
}

namespace
{

// Whether bit n (signal n) is set in a mask as /proc writes it.
bool maskHas(const std::string &hex, int sig)
{
	const unsigned long long v = std::strtoull(hex.c_str(), NULL, 16);
	return (v >> (sig - 1)) & 1ull;
}

std::string lineValue(const std::string &text, const char *key)
{
	const size_t at = text.find(key);
	if (at == std::string::npos)
		return std::string();
	size_t from = at + std::strlen(key);
	while (from < text.size() && (text[from] == ' ' || text[from] == '\t'))
		++from;
	return text.substr(from, text.find('\n', from) - from);
}

} // namespace

TEST_CASE("the program starts with every signal at its default and nothing of the box's open",
          "[streaming][wire][program]")
{
	/* This program ignores SIGPIPE and SIGHUP, and a thread may block what it
	   likes; the server's threads, and the job's, are made by a thread that
	   blocks SIGTERM here. None of it may reach the converter. */
	struct sigaction ign, old_pipe, old_hup;
	std::memset(&ign, 0, sizeof(ign));
	ign.sa_handler = SIG_IGN;
	sigemptyset(&ign.sa_mask);
	REQUIRE(sigaction(SIGPIPE, &ign, &old_pipe) == 0);
	REQUIRE(sigaction(SIGHUP, &ign, &old_hup) == 0);
	sigset_t term, old_mask;
	sigemptyset(&term);
	sigaddset(&term, SIGTERM);
	REQUIRE(pthread_sigmask(SIG_BLOCK, &term, &old_mask) == 0);

	std::string body;
	std::string script;
	{
		StreamBox box;
		/* ls lists the shell's descriptors from a process of its own, so the
		   listing adds none to them. */
		box.useConverter("grep -E '^Sig(Blk|Ign):' /proc/$$/status\n"
		                 "ls -l /proc/$$/fd\n"
		                 "echo end\nexec sleep 30\n");
		script = box.scripts.back();
		WireStream r(box.port(), asWriter("GET", kBrowser).c_str());
		REQUIRE(r.ok());
		CHECK(r.code() == 200);
		for (int i = 0; i < 50 && r.body.find("end\n") == std::string::npos; ++i)
			r.closedWithin(100);
		body = r.body;
	}
	pthread_sigmask(SIG_SETMASK, &old_mask, NULL);
	sigaction(SIGPIPE, &old_pipe, NULL);
	sigaction(SIGHUP, &old_hup, NULL);

	INFO(body);
	REQUIRE(body.find("end\n") != std::string::npos);
	CHECK(lineValue(body, "SigBlk:") == "0000000000000000");
	const std::string ignored = lineValue(body, "SigIgn:");
	REQUIRE(!ignored.empty());
	CHECK_FALSE(maskHas(ignored, SIGPIPE));
	CHECK_FALSE(maskHas(ignored, SIGHUP));
	CHECK_FALSE(maskHas(ignored, SIGTERM));
	CHECK_FALSE(maskHas(ignored, SIGCHLD));

	// Beyond the three standard ones, only the script the shell reads.
	size_t at = 0;
	int listed = 0;
	int others = 0;
	while ((at = body.find(" -> ", at)) != std::string::npos)
	{
		const size_t start = body.rfind(' ', at - 1) + 1;
		const size_t end = body.find('\n', at);
		const int n = std::atoi(body.substr(start, at - start).c_str());
		const std::string target = body.substr(at + 4, end - at - 4);
		at = end;
		++listed;
		if (n <= 2)
			continue;
		CHECK(target == script);
		++others;
	}
	CHECK(listed >= 3);
	CHECK(others <= 1);
}

TEST_CASE("a process that does not supervise its children starts no program",
          "[streaming][wire][program]")
{
	StreamBox box;
	std::string pidfile;
	box.useConverter("echo $$ > PIDFILE\nwhile :; do printf 'G'; sleep 0.05; done\n", &pidfile);

	struct sigaction old, ign, nowait;
	std::memset(&ign, 0, sizeof(ign));
	ign.sa_handler = SIG_IGN;
	sigemptyset(&ign.sa_mask);
	std::memset(&nowait, 0, sizeof(nowait));
	nowait.sa_handler = SIG_DFL;
	nowait.sa_flags = SA_NOCLDWAIT;
	sigemptyset(&nowait.sa_mask);
	REQUIRE(sigaction(SIGCHLD, NULL, &old) == 0);

	const struct sigaction *ways[] = { &ign, &nowait };
	for (int w = 0; w < 2; ++w)
	{
		REQUIRE(sigaction(SIGCHLD, ways[w], NULL) == 0);
		for (int m = 0; m < 2; ++m)
		{
			const char *method = m == 0 ? "GET" : "HEAD";
			INFO((w == 0 ? "ignored " : "no child wait ") << method);
			WireStream r(box.port(), asWriter(method, kBrowser).c_str());
			REQUIRE(r.ok());
			const std::string body = r.readToEof(3000);
			CHECK(r.code() == 500);
			if (m == 0)
				CHECK(problemCodeOf(body) == "no-supervision");
		}
	}
	REQUIRE(sigaction(SIGCHLD, &old, NULL) == 0);
	CHECK(pidFrom(pidfile, 300) == 0);

	// Supervised again, the same request runs.
	WireStream r(box.port(), asWriter("GET", kBrowser).c_str());
	REQUIRE(r.ok());
	CHECK(r.code() == 200);
}

TEST_CASE("a server that stops ends a running program", "[streaming][wire][program]")
{
	StreamBox box;
	std::string pidfile;
	box.useConverter("echo $$ > PIDFILE\nwhile :; do printf 'G'; sleep 0.05; done\n", &pidfile);

	WireStream r(box.port(), asWriter("GET", kBrowser).c_str());
	REQUIRE(r.ok());
	CHECK(r.code() == 200);
	const pid_t pid = pidFrom(pidfile, 3000);
	REQUIRE(pid > 0);

	carry::beginStop();
	CHECK(carry::waitIdle(3000));
	CHECK(goneWithin(pid, 1000));
	CHECK(r.closedWithin(2000));
	carry::endStop();
}

TEST_CASE("the stream routes answer their refusals", "[streaming][wire]")
{
	StreamBox box;
	struct Case
	{
		const char *request;
		int code;
		const char *problem;
	};
	const Case cases[] = {
		{ "GET /api/v1/stream/browser/9999?video=h264&audio=mp2 HTTP/1.0\r\nHost: box\r\n", 404, "no-such-channel" },
		{ "GET /api/v1/stream/browser/1001?video=mpeg2&audio=mp2 HTTP/1.0\r\nHost: box\r\n", 501, "not-playable-in-browser" },
		{ "GET /api/v1/stream/9999 HTTP/1.0\r\nHost: box\r\n", 404, "no-such-channel" },
		{ "GET /api/v1/stream/1001 HTTP/1.0\r\n", 400, "no-authority" },
		{ "GET /api/v1/stream/playlist HTTP/1.0\r\n", 400, "no-authority" },
		{ "GET /api/v1/stream/playlist.m3u HTTP/1.0\r\n", 400, "no-authority" },
		{ "GET /api/v1/stream/playlist/1001 HTTP/1.0\r\n", 400, "no-authority" },
		{ "GET /api/v1/stream/playlist/9999 HTTP/1.0\r\nHost: box\r\n", 404, "no-such-channel" },
	};
	for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); ++i)
	{
		INFO(cases[i].request);
		const std::string req = std::string(cases[i].request) + bearer(kWriteToken) + "\r\n";
		WireStream r(box.port(), req.c_str());
		REQUIRE(r.ok());
		const std::string body = r.readToEof(3000);
		CHECK(r.code() == cases[i].code);
		CHECK(problemCodeOf(body) == cases[i].problem);
	}
}

// ================================================================ a tuner first

TEST_CASE("whether a stream gets a tuner is asked of the box for a tuner channel only",
          "[streaming][tuner]")
{
	Fixture fx;
	fx.deps.channels.channels.push_back(makeChannel(0x7001, "Web", coreapi::ServiceKind::WebTv));

	REQUIRE(coreapi::streaming::tunerFor(0x1001).value() == coreapi::StreamTuner::Free);
	REQUIRE(fx.deps.channels.tuner_asked.size() == 1);
	REQUIRE(fx.deps.channels.tuner_asked[0] == 0x1001u);

	fx.deps.channels.stream_tuner = coreapi::StreamTuner::NoneFree;
	REQUIRE(coreapi::streaming::tunerFor(0x2001).value() == coreapi::StreamTuner::NoneFree);
	fx.deps.channels.stream_tuner = coreapi::StreamTuner::NoAnswer;
	REQUIRE(coreapi::streaming::tunerFor(0x1001).value() == coreapi::StreamTuner::NoAnswer);

	// A web channel needs no tuner and is not asked about.
	fx.deps.channels.tuner_asked.clear();
	fx.deps.channels.carried_asked.clear();
	fx.deps.channels.recorded_asked.clear();
	REQUIRE(coreapi::streaming::tunerFor(0x7001).value() == coreapi::StreamTuner::Free);
	REQUIRE(fx.deps.channels.tuner_asked.empty());
	REQUIRE(fx.deps.channels.carried_asked.empty());
	REQUIRE(fx.deps.channels.recorded_asked.empty());

	/* A channel the stream server carries already is handed to another reader
	   without a tuner of its own: admitted, and zapit is not asked, whatever
	   it would say. */
	fx.deps.channels.stream_tuner = coreapi::StreamTuner::NoneFree;
	fx.deps.channels.stream_carried = true;
	REQUIRE(coreapi::streaming::tunerFor(0x1001).value() == coreapi::StreamTuner::Free);
	REQUIRE(fx.deps.channels.carried_asked.size() == 1);
	REQUIRE(fx.deps.channels.carried_asked[0] == 0x1001u);
	REQUIRE(fx.deps.channels.tuner_asked.empty());
	// Not carried: zapit decides, as for any other channel.
	fx.deps.channels.stream_carried = false;
	REQUIRE(coreapi::streaming::tunerFor(0x1001).value() == coreapi::StreamTuner::NoneFree);
	REQUIRE(fx.deps.channels.tuner_asked.size() == 1);
	fx.deps.channels.tuner_asked.clear();

	/* A channel being recorded gets no tuner, as at the stream server's port,
	   and zapit is not asked, whatever it would grant; carried as well, it is
	   shared like any carried channel. */
	fx.deps.channels.stream_tuner = coreapi::StreamTuner::Free;
	fx.deps.channels.stream_recorded = true;
	fx.deps.channels.recorded_asked.clear();
	REQUIRE(coreapi::streaming::tunerFor(0x1001).value() == coreapi::StreamTuner::NoneFree);
	REQUIRE(fx.deps.channels.recorded_asked.size() == 1);
	REQUIRE(fx.deps.channels.recorded_asked[0] == 0x1001u);
	REQUIRE(fx.deps.channels.tuner_asked.empty());
	fx.deps.channels.stream_carried = true;
	REQUIRE(coreapi::streaming::tunerFor(0x1001).value() == coreapi::StreamTuner::Free);
	REQUIRE(fx.deps.channels.tuner_asked.empty());
	fx.deps.channels.stream_carried = false;
	fx.deps.channels.stream_recorded = false;
	fx.deps.channels.stream_tuner = coreapi::StreamTuner::NoneFree;

	const coreapi::Result<coreapi::StreamTuner> nobody = coreapi::streaming::tunerFor(0xdeadbeefULL);
	REQUIRE_FALSE(nobody.ok());
	REQUIRE(nobody.error().code == coreapi::ErrorCode::NoSuchChannel);
	REQUIRE(fx.deps.channels.tuner_asked.empty());

	fx.deps.channels.channels_status = coreapi::Status::Internal;
	const coreapi::Result<coreapi::StreamTuner> unread = coreapi::streaming::tunerFor(0x1001);
	REQUIRE_FALSE(unread.ok());
	REQUIRE(unread.error().code == coreapi::ErrorCode::ChannelListUnavailable);
}

TEST_CASE("a browser stream with no tuner free beside the picture is refused before anything starts",
          "[streaming][wire][tuner]")
{
	StreamBox box;
	box.fx.deps.channels.channels.push_back(makeChannel(0x7001, "Web", coreapi::ServiceKind::WebTv));
	std::string pidfile;
	box.useConverter("echo $$ >> PIDFILE\nwhile :; do printf 'G'; sleep 0.05; done\n", &pidfile);

	struct Case
	{
		coreapi::StreamTuner tuner;
		int code;
		const char *problem;
	};
	const Case cases[] = {
		{ coreapi::StreamTuner::NoneFree, 409, "no-free-tuner" },
		{ coreapi::StreamTuner::NoAnswer, 503, "upstream-unreachable" },
	};
	for (size_t c = 0; c < 2; ++c)
	{
		box.fx.deps.channels.stream_tuner = cases[c].tuner;
		for (int m = 0; m < 2; ++m)
		{
			const char *method = m == 0 ? "GET" : "HEAD";
			INFO(cases[c].problem << ": " << method);
			box.fx.deps.channels.tuner_asked.clear();
			WireStream r(box.port(), asWriter(method, kBrowser).c_str());
			REQUIRE(r.ok());
			const std::string body = r.readToEof(3000);
			CHECK(r.code() == cases[c].code);
			if (m == 0)
				CHECK(problemCodeOf(body) == cases[c].problem);
			CHECK(box.fx.deps.channels.tuner_asked.size() == 1);
		}
	}
	CHECK(pidFrom(pidfile, 300) == 0);
	CHECK(carry::running() == 0u);

	// A tuner free: the same request starts the program, for HEAD the check only.
	box.fx.deps.channels.stream_tuner = coreapi::StreamTuner::Free;
	{
		WireStream head(box.port(), asWriter("HEAD", kBrowser).c_str());
		REQUIRE(head.ok());
		CHECK(head.code() == 200);
	}
	{
		WireStream get(box.port(), asWriter("GET", kBrowser).c_str());
		REQUIRE(get.ok());
		CHECK(get.code() == 200);
		CHECK(pidFrom(pidfile, 3000) > 0);
	}
	CHECK(runningAfter(2000) == 0);

	// A web channel is not asked about, whatever the tuners say.
	box.fx.deps.channels.stream_tuner = coreapi::StreamTuner::NoneFree;
	box.fx.deps.channels.tuner_asked.clear();
	{
		WireStream web(box.port(), asWriter("GET", "/api/v1/stream/browser/7001?video=unknown&audio=unknown").c_str());
		REQUIRE(web.ok());
		CHECK(web.code() == 200);
	}
	CHECK(box.fx.deps.channels.tuner_asked.empty());
	CHECK(runningAfter(2000) == 0);

	/* Nor is a channel the stream server carries already, which its port hands
	   to the converter as another reader: HEAD and GET are answered as for a
	   free tuner, with no tuner none would be free. */
	box.fx.deps.channels.stream_carried = true;
	box.fx.deps.channels.tuner_asked.clear();
	{
		WireStream head(box.port(), asWriter("HEAD", kBrowser).c_str());
		REQUIRE(head.ok());
		CHECK(head.code() == 200);
	}
	{
		WireStream get(box.port(), asWriter("GET", kBrowser).c_str());
		REQUIRE(get.ok());
		CHECK(get.code() == 200);
	}
	CHECK(box.fx.deps.channels.tuner_asked.empty());
	CHECK(box.fx.deps.channels.carried_asked.size() >= 2);
	CHECK(runningAfter(2000) == 0);

	/* A channel being recorded and not carried is refused before anything
	   starts, for GET and HEAD, with zapit not asked though it would grant a
	   tuner: the stream server's port would refuse the converter. */
	box.fx.deps.channels.stream_carried = false;
	box.fx.deps.channels.stream_recorded = true;
	box.fx.deps.channels.stream_tuner = coreapi::StreamTuner::Free;
	box.fx.deps.channels.tuner_asked.clear();
	for (int m = 0; m < 2; ++m)
	{
		const char *method = m == 0 ? "GET" : "HEAD";
		INFO("recorded: " << method);
		WireStream r(box.port(), asWriter(method, kBrowser).c_str());
		REQUIRE(r.ok());
		const std::string body = r.readToEof(3000);
		CHECK(r.code() == 409);
		if (m == 0)
			CHECK(problemCodeOf(body) == "no-free-tuner");
	}
	CHECK(box.fx.deps.channels.tuner_asked.empty());
	CHECK(carry::running() == 0u);
}

// ================================================================ switching the picture

TEST_CASE("a switch of the picture is weighed only for live television of the channel's kind with nothing recording",
          "[streaming][tuner][switch]")
{
	Fixture fx;
	fx.deps.channels.stream_tuner = coreapi::StreamTuner::NoneFreeUnlessSwitched;

	// Mode left at nothing the box reports: not asked, and no switch offered.
	REQUIRE(coreapi::streaming::tunerFor(0x1001).value() == coreapi::StreamTuner::NoneFree);
	REQUIRE(fx.deps.channels.tuner_asked_switch.back() == false);

	fx.deps.channels.mode = NeutrinoModes::mode_tv;
	REQUIRE(coreapi::streaming::tunerFor(0x1001).value() == coreapi::StreamTuner::NoneFreeUnlessSwitched);
	REQUIRE(fx.deps.channels.tuner_asked_switch.back() == true);
	// A radio channel while the box shows television.
	REQUIRE(coreapi::streaming::tunerFor(0x2001).value() == coreapi::StreamTuner::NoneFree);
	REQUIRE(fx.deps.channels.tuner_asked_switch.back() == false);

	fx.deps.channels.mode = NeutrinoModes::mode_radio;
	REQUIRE(coreapi::streaming::tunerFor(0x2001).value() == coreapi::StreamTuner::NoneFreeUnlessSwitched);
	REQUIRE(coreapi::streaming::tunerFor(0x1001).value() == coreapi::StreamTuner::NoneFree);

	// The movie player, standby: neither.
	fx.deps.channels.mode = NeutrinoModes::mode_ts;
	REQUIRE(coreapi::streaming::tunerFor(0x1001).value() == coreapi::StreamTuner::NoneFree);
	fx.deps.channels.mode = NeutrinoModes::mode_standby;
	REQUIRE(coreapi::streaming::tunerFor(0x1001).value() == coreapi::StreamTuner::NoneFree);

	// Anything recording, of any channel, a timeshift included.
	fx.deps.channels.mode = NeutrinoModes::mode_tv;
	coreapi::RecordingInfo shift;
	shift.id = 7;
	shift.channel_id = 0x1002;
	shift.timeshift = true;
	fx.deps.recordings.recordings.push_back(shift);
	REQUIRE(coreapi::streaming::tunerFor(0x1001).value() == coreapi::StreamTuner::NoneFree);
	REQUIRE(fx.deps.channels.tuner_asked_switch.back() == false);
	fx.deps.recordings.recordings.clear();
	REQUIRE(coreapi::streaming::tunerFor(0x1001).value() == coreapi::StreamTuner::NoneFreeUnlessSwitched);
}

TEST_CASE("the setting turns a switch that would free a tuner into a stream", "[streaming][tuner][switch]")
{
	Fixture fx;
	fx.deps.channels.mode = NeutrinoModes::mode_tv;
	fx.deps.channels.stream_tuner = coreapi::StreamTuner::NoneFreeUnlessSwitched;

	REQUIRE(coreapi::streaming::tunerFor(0x1001).value() == coreapi::StreamTuner::NoneFreeUnlessSwitched);
	fx.settings.ints["streaming_switch_picture"] = 1;
	REQUIRE(coreapi::streaming::tunerFor(0x1001).value() == coreapi::StreamTuner::FreeBySwitch);
	fx.settings.ints["streaming_switch_picture"] = 0;
	REQUIRE(coreapi::streaming::tunerFor(0x1001).value() == coreapi::StreamTuner::NoneFreeUnlessSwitched);

	// Where no switch would help, the setting changes nothing.
	fx.settings.ints["streaming_switch_picture"] = 1;
	fx.deps.channels.stream_tuner = coreapi::StreamTuner::NoneFree;
	REQUIRE(coreapi::streaming::tunerFor(0x1001).value() == coreapi::StreamTuner::NoneFree);
	fx.deps.channels.stream_tuner = coreapi::StreamTuner::Free;
	REQUIRE(coreapi::streaming::tunerFor(0x1001).value() == coreapi::StreamTuner::Free);
}

TEST_CASE("a box with no line for the setting lets a stream switch the picture", "[streaming][tuner][switch]")
{
	Fixture fx;
	fx.deps.channels.mode = NeutrinoModes::mode_tv;
	fx.deps.channels.stream_tuner = coreapi::StreamTuner::NoneFreeUnlessSwitched;
	fx.settings.ints.erase("streaming_switch_picture");

	REQUIRE(coreapi::streaming::tunerFor(0x1001).value() == coreapi::StreamTuner::FreeBySwitch);
	REQUIRE(coreapi::settings::get("streaming_switch_picture").value() == "1");
	// A line that says off keeps it off.
	fx.settings.ints["streaming_switch_picture"] = 0;
	REQUIRE(coreapi::streaming::tunerFor(0x1001).value() == coreapi::StreamTuner::NoneFreeUnlessSwitched);
}

TEST_CASE("a browser stream a switch would serve is refused naming the setting, or streamed where it is on",
          "[streaming][wire][tuner][switch]")
{
	StreamBox box;
	std::string pidfile;
	box.useConverter("echo $$ >> PIDFILE\nwhile :; do printf 'G'; sleep 0.05; done\n", &pidfile);
	box.fx.deps.channels.mode = NeutrinoModes::mode_tv;
	box.fx.deps.channels.stream_tuner = coreapi::StreamTuner::NoneFreeUnlessSwitched;

	{
		WireStream r(box.port(), asWriter("GET", kBrowser).c_str());
		REQUIRE(r.ok());
		const std::string body = r.readToEof(3000);
		CHECK(r.code() == 409);
		CHECK(problemCodeOf(body) == "no-free-tuner");
		CHECK(body.find("\"depends_on\":[\"streaming_switch_picture\"]") != std::string::npos);
		CHECK(r.headerOf("X-Depends-On") == "streaming_switch_picture");
	}
	// A HEAD, which has no document, names the setting in the header.
	{
		WireStream r(box.port(), asWriter("HEAD", kBrowser).c_str());
		REQUIRE(r.ok());
		const std::string body = r.readToEof(3000);
		CHECK(r.code() == 409);
		CHECK(body.empty());
		CHECK(r.headerOf("X-Depends-On") == "streaming_switch_picture");
	}
	CHECK(pidFrom(pidfile, 300) == 0);

	// Refused for another reason, the setting is not named.
	box.fx.deps.channels.stream_tuner = coreapi::StreamTuner::NoneFree;
	{
		WireStream r(box.port(), asWriter("GET", kBrowser).c_str());
		REQUIRE(r.ok());
		const std::string body = r.readToEof(3000);
		CHECK(r.code() == 409);
		CHECK(body.find("depends_on") == std::string::npos);
		CHECK_FALSE(r.hasHeader("X-Depends-On"));
	}

	// Switched on, the converter starts and the stream server does the switch.
	box.fx.settings.ints["streaming_switch_picture"] = 1;
	box.fx.deps.channels.stream_tuner = coreapi::StreamTuner::NoneFreeUnlessSwitched;
	{
		WireStream get(box.port(), asWriter("GET", kBrowser).c_str());
		REQUIRE(get.ok());
		CHECK(get.code() == 200);
		CHECK(pidFrom(pidfile, 3000) > 0);
	}
	CHECK(runningAfter(2000) == 0);
}

TEST_CASE("no switch is weighed for a channel the GUI would ask the PIN for", "[streaming][tuner][switch]")
{
	Fixture fx;
	fx.deps.channels.mode = NeutrinoModes::mode_tv;
	fx.deps.channels.stream_tuner = coreapi::StreamTuner::NoneFreeUnlessSwitched;
	for (size_t i = 0; i < fx.deps.channels.channels.size(); ++i)
		if (fx.deps.channels.channels[i].id == 0x1001)
			fx.deps.channels.channels[i].locked = true;

	// The prompt off: the lock is not asked for, and a switch is weighed.
	REQUIRE(coreapi::streaming::tunerFor(0x1001).value() == coreapi::StreamTuner::NoneFreeUnlessSwitched);
	// On: a locked channel where bouquets start unlocked is not switched to.
	fx.settings.ints["parentallock_prompt"] = 2;
	REQUIRE(coreapi::streaming::tunerFor(0x1001).value() == coreapi::StreamTuner::NoneFree);
	REQUIRE(fx.deps.channels.tuner_asked_switch.back() == false);
	fx.settings.ints["streaming_switch_picture"] = 1;
	REQUIRE(coreapi::streaming::tunerFor(0x1001).value() == coreapi::StreamTuner::NoneFree);
	// Where bouquets start locked, a locked channel is the ordinary case and an unlocked one asks.
	fx.settings.ints["parentallock_defaultlocked"] = 1;
	fx.deps.channels.current.locked = true;
	REQUIRE(coreapi::streaming::tunerFor(0x1001).value() == coreapi::StreamTuner::FreeBySwitch);
	REQUIRE(coreapi::streaming::tunerFor(0x1002).value() == coreapi::StreamTuner::NoneFree);
	// A rule that cannot be read counts as asking for the PIN.
	fx.settings.ints["parentallock_prompt"] = 0;
	REQUIRE(coreapi::streaming::tunerFor(0x1002).value() == coreapi::StreamTuner::FreeBySwitch);
	fx.settings.fail_next = true;
	REQUIRE(coreapi::streaming::tunerFor(0x1002).value() == coreapi::StreamTuner::NoneFree);
	REQUIRE(fx.deps.channels.tuner_asked_switch.back() == false);
}

TEST_CASE("no switch is weighed away from a picture the GUI would ask the PIN for", "[streaming][tuner][switch]")
{
	Fixture fx;
	fx.deps.channels.mode = NeutrinoModes::mode_tv;
	fx.deps.channels.stream_tuner = coreapi::StreamTuner::NoneFreeUnlessSwitched;
	fx.settings.ints["streaming_switch_picture"] = 1;
	fx.settings.ints["parentallock_prompt"] = 2;

	REQUIRE(coreapi::streaming::tunerFor(0x1001).value() == coreapi::StreamTuner::FreeBySwitch);
	// The picture shows a locked channel where bouquets start unlocked: a zap back would ask again.
	fx.deps.channels.current.locked = true;
	REQUIRE(coreapi::streaming::tunerFor(0x1001).value() == coreapi::StreamTuner::NoneFree);
	REQUIRE(fx.deps.channels.tuner_asked_switch.back() == false);
	// With the prompt off it is shown without one.
	fx.settings.ints["parentallock_prompt"] = 0;
	REQUIRE(coreapi::streaming::tunerFor(0x1001).value() == coreapi::StreamTuner::FreeBySwitch);
	// A picture whose channel cannot be told counts as one that asks.
	fx.deps.channels.current_status = coreapi::Status::NotFound;
	REQUIRE(coreapi::streaming::tunerFor(0x1001).value() == coreapi::StreamTuner::NoneFree);
	REQUIRE(fx.deps.channels.tuner_asked_switch.back() == false);
}
