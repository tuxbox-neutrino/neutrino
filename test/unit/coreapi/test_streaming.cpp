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
#include "httpd/credentials.h"
#include "httpd/endpoint.h"
#include "httpd/endpoints.h"
#include "httpd/events.h"
#include "httpd/netmatch.h"
#include "httpd/webconfig.h"
#include "httpd/http.h"
#include "httpd/doc/openapi.h"
#include "httpd/router.h"
#include "httpd/status.h"

#include "coreapi/base/errors.h"
#include "coreapi/base/result.h"
#include "coreapi/epg.h"
#include "coreapi/streaming.h"
#include "coreapi/base/types.h"

#include <cstdio>
#include <cstring>
#include <string>
#include <utility>
#include <vector>

#include <stdint.h>

#include <neutrinoMessages.h>

#include <arpa/inet.h>
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

/* The stream routes in front of nhttpd and the bridge, on a port the kernel
   picks, over the channels and the port of the fixture above. */
struct StreamBox
{
	Fixture    fx;
	WireServer server;

	StreamBox() : server(4, 100, WireServer::Threaded)
	{
		setRoutesForTest(&streamTable);
		addApiToken(tokenLookupPrefix(kWriteToken), hashSecret(kWriteToken), AuthLevel::Write, "", 0);
	}

	~StreamBox()
	{
		setRoutesForTest(NULL);
		setSessionClockForTest(0);
		setConfigForTest(defaultWebConfig());
		forgetApiTokens();
	}

	int port() const { return server.port; }

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
