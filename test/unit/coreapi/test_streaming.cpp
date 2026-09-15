/*
 * test_streaming.cpp - tests for streams and playlists
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

#include "support/catch.hpp"
#include "support/fakes.h"
#include "support/httpclient.h"

#include "httpd/endpoint.h"
#include "httpd/events.h"
#include "httpd/http.h"
#include "httpd/doc/openapi.h"
#include "httpd/router.h"
#include "httpd/server.h"
#include "httpd/status.h"

#include "coreapi/base/errors.h"
#include "coreapi/base/result.h"
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

// How many of the files this call's own naming scheme could have made are
// sitting in /tmp right now. Matched by prefix and not by a fixed name, because
// the path carries this process's id and a counter of its own.
size_t playlistFilesInTmp()
{
	DIR *d = ::opendir("/tmp");
	if (d == NULL)
		return 0;
	const std::string prefix = "coreapi-playlist.";
	size_t n = 0;
	for (struct dirent *e = ::readdir(d); e != NULL; e = ::readdir(d))
		if (std::strncmp(e->d_name, prefix.c_str(), prefix.size()) == 0)
			++n;
	::closedir(d);
	return n;
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

	REQUIRE(doc.compare(0, 8, "#EXTM3U\n") == 0);
	REQUIRE(countOf(doc, "#EXTINF:-1,") == 2);
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

	REQUIRE(doc.compare(0, 8, "#EXTM3U\n") == 0);
	REQUIRE(countOf(doc, "#EXTINF:-1,") == 1);
	REQUIRE(doc.find("#EXTINF:-1,First\n") != std::string::npos);
	REQUIRE(doc.find("http://box:31339/id=1001\n") != std::string::npos);
	REQUIRE(doc.find("Second") == std::string::npos);
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

	const std::string prefix = "coreapi-playlist.";
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

namespace
{

/* The routes the server ships, in front of nothing, on a port the kernel picks.
   Everything it did is undone from the destructor, whichever line a case left through: a
   case that left a daemon bound would take it with it for the rest of the run. The
   fixture is declared first, because the server refuses to start while the seams its
   routes read through are empty. */
struct Serving
{
	Fixture fx;
	int port;

	Serving() : port(0)
	{
		setRoutesForTest(NULL);

		ServerConfig c = defaultConfig();
		c.port = 0;
		c.bind_address = "127.0.0.1";
		if (start(c))
			port = boundPort();
	}

	~Serving()
	{
		stop();
		setRoutesForTest(NULL);
	}

	private:
		Serving(const Serving &);
		Serving &operator=(const Serving &);
};

std::vector<std::pair<std::string, std::string> > headers(const char *name, const char *value)
{
	std::vector<std::pair<std::string, std::string> > out;
	out.push_back(std::make_pair(std::string(name), std::string(value)));
	return out;
}

/* The whole answer to a request with no Host line at all, written to the socket by hand.

   The client beside this cannot send one: it writes the line itself unless a case names
   the header, and naming it is how a case sets the value rather than how a case leaves
   it out. The two are different requests on the wire and the library tells them apart,
   which was measured rather than assumed: the one naming the header with nothing after
   it is passed on with an empty value, and the one with no such line at all is refused
   where it arrives.

   The whole answer and not the body alone, because what a case here asks about is the
   status. */
std::string replyWithoutHost(int port, const char *path)
{
	const int fd = ::socket(AF_INET, SOCK_STREAM, 0);
	if (fd < 0)
		return std::string("this case could not open a socket");

	struct sockaddr_in to;
	std::memset(&to, 0, sizeof(to));
	to.sin_family = AF_INET;
	to.sin_port = htons((uint16_t) port);
	to.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
	if (::connect(fd, (const struct sockaddr *) &to, sizeof(to)) != 0)
	{
		::close(fd);
		return std::string("this case could not reach the server");
	}

	std::string req = "GET ";
	req += path;
	req += " HTTP/1.1\r\nConnection: close\r\n\r\n";

	size_t written = 0;
	while (written < req.size())
	{
		const ssize_t n = ::write(fd, req.data() + written, req.size() - written);
		if (n <= 0)
		{
			::close(fd);
			return std::string("this case could not write its request");
		}
		written += (size_t) n;
	}

	std::string got;
	char buf[4096];
	for (;;)
	{
		const ssize_t n = ::read(fd, buf, sizeof(buf));
		if (n <= 0)
			break;
		got.append(buf, (size_t) n);
	}
	::close(fd);
	return got;
}

std::string jsonString(const std::string &body, const char *key)
{
	const std::string looking = std::string("\"") + key + "\":\"";
	const std::string::size_type at = body.find(looking);
	if (at == std::string::npos)
		return std::string();
	const std::string::size_type from = at + looking.size();
	const std::string::size_type end = body.find('"', from);
	if (end == std::string::npos)
		return std::string();
	return body.substr(from, end - from);
}

/* How many of this process's descriptors are on a file this endpoint's own naming scheme
   made, found by asking what each of them is on rather than by counting them. A number
   on its own answers a different question: connections opening and closing move it while
   a case runs. A name that has been taken away reads back with a word after it and is
   the same file, so the prefix and not the whole name is what is compared. */
size_t openPlaylistDescriptors()
{
	DIR *d = ::opendir("/proc/self/fd");
	if (d == NULL)
		return 0;

	const std::string prefix = "/tmp/coreapi-playlist.";
	size_t n = 0;
	for (struct dirent *e = ::readdir(d); e != NULL; e = ::readdir(d))
	{
		if (e->d_name[0] == '.')
			continue;

		std::string link = "/proc/self/fd/";
		link += e->d_name;

		char target[4096];
		const ssize_t got = ::readlink(link.c_str(), target, sizeof(target) - 1);
		if (got <= 0)
			continue;

		const std::string what(target, (size_t) got);
		if (what.size() >= prefix.size() && what.compare(0, prefix.size(), prefix) == 0)
			++n;
	}
	::closedir(d);
	return n;
}

/* The last answer's descriptor is given back when the library is done with that
   answer, which is not always before the reply has been read out here. So the
   count is waited on rather than taken once, and what it was when the wait gave
   up is what a case reads. */
size_t waitForNoPlaylistDescriptors(int timeout_ms)
{
	for (int waited = 0; waited < timeout_ms; waited += 10)
	{
		const size_t now = openPlaylistDescriptors();
		if (now == 0)
			return now;
		::usleep(10 * 1000);
	}
	return openPlaylistDescriptors();
}

} // namespace

TEST_CASE("the address a request gets names the host that request used", "[streaming]")
{
	/* Over the wire and with the header driven by hand, because an address
	   built from a host a case passed in itself says nothing about whether the
	   server ever reads one. */
	Serving serving;
	REQUIRE(serving.port > 0);

	const testhttp::Reply r =
		testhttp::request(serving.port, "GET", "/api/v1/stream/b544000800850088",
		                  headers("Host", "box.example:8080"));
	REQUIRE(r.transport_ok);
	REQUIRE(r.code == 200);
	REQUIRE(jsonString(r.body, "url") == "http://box.example:31339/id=b544000800850088");
	REQUIRE(jsonString(r.body, "name") == "Wide");
	REQUIRE(r.body.find("\"port\":31339") != std::string::npos);
}

/* Both shapes are refused and both with 400, but by different layers, and which layer
   is what the checks below are about. A request naming the header with nothing after it
   carries an empty authority and reaches this route, which has no address to hand out
   and says so in this server's own document. A request leaving the line out carries
   none and never arrives: libmicrohttpd refuses it in parse_connection_headers
   (src/microhttpd/connection.c) as of 1.0.0, where the check is gated on the daemon's
   client_discipline and runs at its default of 0; through 0.9.75 the same check hung on
   strict_for_client, also 0, and did not run. RFC 9112 asks for that refusal, so it is
   the library's to make.

   The second half therefore asks whether this server's document is there, and not what
   the library puts in its place, which differs between versions and is empty in a build
   without messages. Only the one answer is stated, because configure.ac will not
   configure against a library older than 1.0.10 and there is no longer a build in which
   the other one happens.

   It is also the guard against the change being given back: a library that stops
   refusing sends the request to the route, the route writes no-authority again, and the
   check turns over instead of the old answer quietly returning. */
TEST_CASE("a request with no host is refused by the library and one with an empty host by this server",
          "[streaming]")
{
	Serving serving;
	REQUIRE(serving.port > 0);

	const testhttp::Reply named_empty =
		testhttp::request(serving.port, "GET", "/api/v1/stream/b544000800850088",
		                  headers("Host", ""));
	REQUIRE(named_empty.transport_ok);
	REQUIRE(named_empty.code == 400);
	REQUIRE(named_empty.body.find("no-authority") != std::string::npos);
	// And above all: no address at all went out
	REQUIRE(named_empty.body.find("31339") == std::string::npos);

	const std::string raw =
		replyWithoutHost(serving.port, "/api/v1/stream/b544000800850088");
	REQUIRE(raw.compare(0, 12, "HTTP/1.1 400") == 0);
	REQUIRE(raw.find("no-authority") == std::string::npos);
	REQUIRE(raw.find("31339") == std::string::npos);
}

TEST_CASE("the playlist arrives whole and not chunked", "[streaming]")
{
	Serving serving;
	REQUIRE(serving.port > 0);

	const testhttp::Reply r =
		testhttp::request(serving.port, "GET", "/api/v1/stream/playlist",
		                  headers("Host", "box.example"));
	REQUIRE(r.transport_ok);
	REQUIRE(r.code == 200);
	REQUIRE(r.header("Content-Type") == "audio/x-mpegurl");
	REQUIRE_FALSE(r.header("Content-Length").empty());
	// An answer of unknown length goes out chunked, and the client above
	// decodes none, so a case reading a body at all rests on this
	REQUIRE(r.header("Transfer-Encoding").empty());
	REQUIRE(r.body.find("http://box.example:31339/id=1001") != std::string::npos);
}

TEST_CASE("one channel's playlist arrives the same way", "[streaming]")
{
	Serving serving;
	REQUIRE(serving.port > 0);

	const testhttp::Reply r =
		testhttp::request(serving.port, "GET", "/api/v1/stream/playlist/1001",
		                  headers("Host", "box.example"));
	REQUIRE(r.transport_ok);
	REQUIRE(r.code == 200);
	REQUIRE(r.header("Content-Type") == "audio/x-mpegurl");
	REQUIRE(r.body.find("#EXTINF:-1,First\n") != std::string::npos);
	REQUIRE(r.body.find("Second") == std::string::npos);
}

TEST_CASE("a playlist is named after the channel and not after the route", "[streaming]")
{
	/* Without this a browser writes the last segment of the route, so a caller
	   that saved four of them has four files named after numbers. */
	Serving serving;
	REQUIRE(serving.port > 0);

	const testhttp::Reply r =
		testhttp::request(serving.port, "GET", "/api/v1/stream/playlist/1001",
		                  headers("Host", "box.example"));
	REQUIRE(r.transport_ok);
	REQUIRE(r.code == 200);
	REQUIRE(r.header("Content-Disposition") == "attachment; filename*=UTF-8''First.m3u");
}

TEST_CASE("nothing a channel is called reaches the header as itself", "[streaming]")
{
	/* A name is bytes and a header is a line. What a channel is called comes out
	   of the stream, so it may hold a quotation mark, a separator, a byte of any
	   value and the two that end a line, and every one of those written into a
	   header straight would let whoever named the channel write headers of their
	   own into an answer this box sends.

	   One spelling and not two: the encoded form alone, which is the same
	   decision the route that hands a file over made, and for the reason written
	   down there. */
	Serving serving;
	REQUIRE(serving.port > 0);

	const testhttp::Reply r =
		testhttp::request(serving.port, "GET", "/api/v1/stream/playlist/5001",
		                  headers("Host", "box.example"));
	REQUIRE(r.transport_ok);
	REQUIRE(r.code == 200);

	const std::string said = r.header("Content-Disposition");
	REQUIRE(said == "attachment; filename*=UTF-8''A%2FB%20%22C%22%20%C3%B6%0D%0AX%3A%20y.m3u");
	// Said the other way round as well, because what the case above compares is
	// one string and this is the rule that string exists to keep.
	REQUIRE(said.find('"') == std::string::npos);
	REQUIRE(said.find('\r') == std::string::npos);
	REQUIRE(said.find('\n') == std::string::npos);
	REQUIRE(said.find('/') == std::string::npos);
}

TEST_CASE("which half the playlist covers can be named in the query", "[streaming]")
{
	Serving serving;
	REQUIRE(serving.port > 0);

	const testhttp::Reply r =
		testhttp::request(serving.port, "GET", "/api/v1/stream/playlist?mode=radio",
		                  headers("Host", "box.example"));
	REQUIRE(r.transport_ok);
	REQUIRE(r.code == 200);
	REQUIRE(r.body.find("Tunes") != std::string::npos);
	REQUIRE(r.body.find("First") == std::string::npos);
}

TEST_CASE("nothing on the way out builds a second answer from a playlist", "[streaming]")
{
	/* The answer carries a descriptor and hands it over, so a second response
	   built out of this one would be a second owner of it. Three places on the
	   way out build one response from another, and that each of them passes
	   this by is measured here rather than believed. */
	Fixture fx;
	setRoutesForTest(NULL);

	const Response r = dispatch(Get, "/api/v1/stream/playlist", "", "", "127.0.0.1",
	                            AuthLevel::Read, "", "", "box.example");
	REQUIRE(r.code == StatusOk);
	REQUIRE(r.fd >= 0);
	REQUIRE(r.body.empty());
	REQUIRE(r.length > 0);

	// A stream is told by its media type, and this one is not that type
	REQUIRE_FALSE(events::isStream(r));
	// The document is told by its bytes, and this answer carries none in its
	// body at all
	REQUIRE_FALSE(openapi::isDocument(r));
	// The pages are offered only what the router did not have, and this is an
	// answer the router had
	REQUIRE(r.code != StatusNotFound);

	::close(r.fd);
}

TEST_CASE("a thousand playlists leak no descriptors and leave no files", "[streaming]")
{
	Serving serving;
	REQUIRE(serving.port > 0);

	/* The counter has to find something before finding nothing means anything.
	   One descriptor is held on a file of exactly the shape this endpoint makes
	   and with its name taken away, which is the state a leaked one would be
	   in, and a counter that cannot see that one would report every leak as
	   none. */
	{
		const coreapi::Result<std::string> made =
			coreapi::streaming::playlist("box", coreapi::streaming::Scope::Tv);
		REQUIRE(made.ok());
		const int held = ::open(made.value().c_str(), O_RDONLY);
		REQUIRE(held >= 0);
		::unlink(made.value().c_str());
		REQUIRE(openPlaylistDescriptors() == 1u);
		::close(held);
	}
	REQUIRE(openPlaylistDescriptors() == 0u);

	const size_t files_before = playlistFilesInTmp();

	/* Counted rather than checked a thousand times over, and counted at all
	   because a run where none of them arrived leaves nothing behind either:
	   the count is what keeps this from being green over a server that answered
	   nothing. */
	size_t answered = 0;
	for (int i = 0; i < 1000; ++i)
	{
		const testhttp::Reply r =
			testhttp::request(serving.port, "GET", "/api/v1/stream/playlist",
			                  headers("Host", "b"));
		if (r.transport_ok && r.code == 200)
			++answered;
	}
	REQUIRE(answered == 1000u);

	REQUIRE(waitForNoPlaylistDescriptors(5000) == 0u);
	REQUIRE(playlistFilesInTmp() == files_before);
}
