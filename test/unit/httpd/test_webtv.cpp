/*
 * test_webtv.cpp - tests for the pass through, the playlists and what may be fetched
 *
 * Copyright (C) 2026 NI-Team
 * Copyright (C) 2026 Tuxbox-Project (carried fetches over the wire)
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
#include "httpd/hlsrewrite.h"
#include "httpd/netmatch.h"
#include "httpd/nhttpd_bridge.h"
#include "httpd/router.h"
#include "httpd/webconfig.h"
#include "httpd/webtv.h"

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <poll.h>
#include <pthread.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>

using namespace httpd;

namespace
{

int64_t nowMs()
{
	struct timespec t;
	if (clock_gettime(CLOCK_MONOTONIC, &t) != 0)
		return 0;
	return (int64_t) t.tv_sec * 1000 + t.tv_nsec / 1000000;
}

// ------------------------------------------------------------- a far server
//
// The thing on the other side of the pass through. It has to be a real socket
// and not a seam, because what is under test is what curl does with a
// redirection, with an address, and with a body that arrives in pieces, and a
// seam in front of curl would test the seam.

struct FarServer
{
	int         fd;
	int         port;
	pthread_t   thread;
	bool        running;

	// What goes back, in two pieces with a wait between them, so that a case
	// can tell bytes being passed on as they arrive from bytes being collected
	// and sent at the end.
	std::string first;
	unsigned    pause_ms;
	std::string rest;

	// What arrived, so that a case can say the far server was never asked at
	// all, which is what a refusal before the connection means.
	unsigned    asked;
	std::string last_path;
	// The whole head of the last request, for what this box asked with.
	std::string last_head;
	// When the last reply was finished, against which a reader says it had
	// bytes before then.
	int64_t     finished_at;

	pthread_mutex_t said;

	FarServer() : fd(-1), port(0), running(false), pause_ms(0), asked(0), finished_at(0)
	{
		pthread_mutex_init(&said, NULL);
	}

	~FarServer()
	{
		stop();
		pthread_mutex_destroy(&said);
	}

	unsigned timesAsked()
	{
		pthread_mutex_lock(&said);
		const unsigned n = asked;
		pthread_mutex_unlock(&said);
		return n;
	}

	std::string lastHead()
	{
		pthread_mutex_lock(&said);
		const std::string h = last_head;
		pthread_mutex_unlock(&said);
		return h;
	}

	int64_t finishedAt()
	{
		pthread_mutex_lock(&said);
		const int64_t at = finished_at;
		pthread_mutex_unlock(&said);
		return at;
	}

	std::string url(const std::string &path) const
	{
		char buf[64];
		std::snprintf(buf, sizeof(buf), "http://127.0.0.1:%d", port);
		return std::string(buf) + path;
	}

	bool start()
	{
		fd = ::socket(AF_INET, SOCK_STREAM, 0);
		if (fd < 0)
			return false;
		int on = 1;
		::setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));

		struct sockaddr_in a;
		std::memset(&a, 0, sizeof(a));
		a.sin_family = AF_INET;
		a.sin_port = 0;
		a.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
		if (::bind(fd, (struct sockaddr *) &a, sizeof(a)) != 0)
			return false;
		if (::listen(fd, 8) != 0)
			return false;

		socklen_t n = sizeof(a);
		if (::getsockname(fd, (struct sockaddr *) &a, &n) != 0)
			return false;
		port = ntohs(a.sin_port);

		running = true;
		if (pthread_create(&thread, NULL, &serve, this) != 0)
		{
			running = false;
			return false;
		}
		return true;
	}

	void stop()
	{
		if (!running)
		{
			if (fd >= 0)
				::close(fd);
			fd = -1;
			return;
		}
		running = false;
		::shutdown(fd, SHUT_RDWR);
		::close(fd);
		fd = -1;
		pthread_join(thread, NULL);
	}

	static void *serve(void *cls)
	{
		FarServer *o = (FarServer *) cls;
		for (;;)
		{
			const int c = ::accept(o->fd, NULL, NULL);
			if (c < 0)
				return NULL;

			std::string head;
			char buf[1024];
			for (;;)
			{
				const ssize_t got = ::recv(c, buf, sizeof(buf), 0);
				if (got <= 0)
					break;
				head.append(buf, (size_t) got);
				if (head.find("\r\n\r\n") != std::string::npos)
					break;
				if (head.size() > 64u * 1024u)
					break;
			}

			std::string path;
			const size_t sp = head.find(' ');
			if (sp != std::string::npos)
			{
				const size_t sp2 = head.find(' ', sp + 1);
				if (sp2 != std::string::npos)
					path = head.substr(sp + 1, sp2 - sp - 1);
			}

			pthread_mutex_lock(&o->said);
			++o->asked;
			o->last_path = path;
			o->last_head = head;
			const std::string one = o->first;
			const unsigned wait = o->pause_ms;
			const std::string two = o->rest;
			pthread_mutex_unlock(&o->said);

			if (!one.empty())
				(void) ::send(c, one.data(), one.size(), MSG_NOSIGNAL);
			if (wait != 0)
				napMs((int) wait);
			if (!two.empty())
				(void) ::send(c, two.data(), two.size(), MSG_NOSIGNAL);

			pthread_mutex_lock(&o->said);
			o->finished_at = nowMs();
			pthread_mutex_unlock(&o->said);

			::close(c);
		}
	}

private:
	FarServer(const FarServer &);
	FarServer &operator=(const FarServer &);
};

std::string httpReply(const std::string &type, const std::string &body)
{
	char head[256];
	std::snprintf(head, sizeof(head),
	              "HTTP/1.1 200 OK\r\nContent-Type: %s\r\nContent-Length: %u\r\n"
	              "Connection: close\r\n\r\n",
	              type.c_str(), (unsigned) body.size());
	return std::string(head) + body;
}

// Only the documentation range, so that a case can run a far server on this
// machine and still drive the refusal on a redirection.
std::vector<NetPrefix> onlyDocumentationRange()
{
	std::vector<NetPrefix> out;
	NetPrefix p;
	REQUIRE(parsePrefix("192.0.2.0/24", &p));
	out.push_back(p);
	return out;
}

// A minter a case can read back, so that what the rewrite asked for is what is
// compared rather than what came out of it.
struct RecordingMinter : public hls::Minter
{
	std::vector<std::string> urls;
	std::vector<hls::UriKind> kinds;
	bool refuse;
	size_t refuse_after;

	RecordingMinter() : refuse(false), refuse_after(0) {}

	bool mint(const std::string &absolute, hls::UriKind kind, std::string &out)
	{
		if (refuse && urls.size() >= refuse_after)
			return false;
		urls.push_back(absolute);
		kinds.push_back(kind);
		char buf[32];
		std::snprintf(buf, sizeof(buf), "/p/%u", (unsigned) urls.size());
		out = buf;
		return true;
	}
};

std::string resolved(const char *base, const char *ref)
{
	std::string out;
	if (!hls::resolveUri(base, ref, out))
		return std::string("<refused>");
	return out;
}

} // namespace

// =========================================================== the addresses

TEST_CASE("a reference in a playlist resolves the way a browser resolves it", "[webtv]")
{
	const char *base = "https://cdn.example/v1/master/a/b/master.m3u8?k=1";

	// The shape a real master playlist reaches its variants with, which is the
	// one a resolver written for the easy half gets wrong.
	CHECK(resolved(base, "../../../../../manifest/x/0.m3u8") ==
	      "https://cdn.example/manifest/x/0.m3u8");
	CHECK(resolved(base, "2.m3u8") == "https://cdn.example/v1/master/a/b/2.m3u8");
	CHECK(resolved(base, "/abs/2.m3u8") == "https://cdn.example/abs/2.m3u8");
	CHECK(resolved(base, "//other.example/x.m3u8") == "https://other.example/x.m3u8");
	CHECK(resolved(base, "http://plain.example/x.ts") == "http://plain.example/x.ts");
	CHECK(resolved(base, "./seg.ts?sid=9") == "https://cdn.example/v1/master/a/b/seg.ts?sid=9");
	CHECK(resolved(base, "?only=query") == "https://cdn.example/v1/master/a/b/master.m3u8?only=query");
	CHECK(resolved(base, "a/./b/../c.ts") == "https://cdn.example/v1/master/a/b/a/c.ts");

	// A colon after the first characters of a reference is a scheme by the
	// rules a browser follows, so this is a scheme nobody has and not a file
	// called odd:name.ts. Refused rather than fetched, which is the same
	// answer a browser would give it.
	CHECK(resolved(base, "odd:name.ts") == "<refused>");
	// A colon after a separator is not one, and that name is fetched.
	CHECK(resolved(base, "a/odd:name.ts") == "https://cdn.example/v1/master/a/b/a/odd:name.ts");
}

TEST_CASE("a reference this box would have to decide about is refused", "[webtv]")
{
	const char *base = "https://cdn.example/a/b.m3u8";

	CHECK(resolved(base, "file:///etc/passwd") == "<refused>");
	CHECK(resolved(base, "ftp://cdn.example/x.ts") == "<refused>");
	CHECK(resolved(base, "data:text/plain,x") == "<refused>");
	// A fragment names the playlist itself and not anything to fetch.
	CHECK(resolved(base, "#top") == "<refused>");
	CHECK(resolved(base, "") == "<refused>");
	// Above the root of the far server is the far server's business and never
	// an address this builds.
	CHECK(resolved(base, "../../../../x.ts") == "https://cdn.example/x.ts");
	// And a base that is not one of the two schemes resolves nothing at all.
	CHECK(resolved("ftp://cdn.example/a/b.m3u8", "x.ts") == "<refused>");
}

TEST_CASE("a playlist is told from anything else by its first line", "[webtv]")
{
	CHECK(hls::looksLikePlaylist("#EXTM3U\n#EXTINF:4,\nx.ts\n"));
	CHECK(hls::looksLikePlaylist("\xEF\xBB\xBF#EXTM3U\n"));
	CHECK_FALSE(hls::looksLikePlaylist("#EXTINF:4,\n"));
	CHECK_FALSE(hls::looksLikePlaylist(std::string("\x47\x40\x00\x10", 4)));
	CHECK_FALSE(hls::looksLikePlaylist(""));
}

// ============================================================ the rewrite

TEST_CASE("every address in a master playlist is replaced and each is named for what it is",
          "[webtv]")
{
	const std::string body =
		"#EXTM3U\r\n"
		"#EXT-X-VERSION:3\r\n"
		"#EXT-X-MEDIA:TYPE=SUBTITLES,GROUP-ID=\"subs\",NAME=\"en\",URI=\"subtitle/en.m3u8?t=1\"\r\n"
		"#EXT-X-STREAM-INF:BANDWIDTH=1200000\r\n"
		"../../manifest/1.m3u8\r\n"
		"#EXT-X-STREAM-INF:BANDWIDTH=500000\r\n"
		"https://other.example/2.m3u8\r\n";

	RecordingMinter m;
	std::string out;
	size_t uris = 0;
	REQUIRE(hls::rewrite(body, "https://cdn.example/a/b/c/master.m3u8", m, out, uris) ==
	        hls::Rewritten::Ok);

	REQUIRE(uris == 3);
	REQUIRE(m.urls.size() == 3);
	CHECK(m.urls[0] == "https://cdn.example/a/b/c/subtitle/en.m3u8?t=1");
	CHECK(m.urls[1] == "https://cdn.example/a/manifest/1.m3u8");
	CHECK(m.urls[2] == "https://other.example/2.m3u8");

	// The kind is what decides whether the box reads what comes back or hands
	// it on, so it is the half of the rewrite that has to be right.
	CHECK(m.kinds[0] == hls::UriKind::Playlist);
	CHECK(m.kinds[1] == hls::UriKind::Playlist);
	CHECK(m.kinds[2] == hls::UriKind::Playlist);

	// Nothing of the far server's addresses is left for a browser to fetch.
	CHECK(out.find("cdn.example") == std::string::npos);
	CHECK(out.find("other.example") == std::string::npos);
	CHECK(out.find("URI=\"/p/1\"") != std::string::npos);
	CHECK(out.find("\r\n/p/2\r\n") != std::string::npos);
	// And the tags themselves come through as they were.
	CHECK(out.find("#EXT-X-STREAM-INF:BANDWIDTH=1200000\r\n") != std::string::npos);
	CHECK(out.compare(0, 9, "#EXTM3U\r\n") == 0);
}

TEST_CASE("a media playlist names its segments, its key and its map apart", "[webtv]")
{
	const std::string body =
		"#EXTM3U\n"
		"#EXT-X-TARGETDURATION:4\n"
		"#EXT-X-KEY:METHOD=AES-128,URI=\"https://keys.example/k1\",IV=0x00\n"
		"#EXT-X-MAP:URI=\"init.mp4\"\n"
		"\n"
		"#EXTINF:4.0,\n"
		"seg1.ts?sid=7\n"
		"#EXTINF:4.0,\n"
		"seg2.ts\n";

	RecordingMinter m;
	std::string out;
	size_t uris = 0;
	REQUIRE(hls::rewrite(body, "https://cdn.example/live/x.m3u8", m, out, uris) ==
	        hls::Rewritten::Ok);

	REQUIRE(uris == 4);
	CHECK(m.kinds[0] == hls::UriKind::Key);
	CHECK(m.kinds[1] == hls::UriKind::Map);
	CHECK(m.kinds[2] == hls::UriKind::Segment);
	CHECK(m.kinds[3] == hls::UriKind::Segment);
	CHECK(m.urls[0] == "https://keys.example/k1");
	CHECK(m.urls[1] == "https://cdn.example/live/init.mp4");
	CHECK(m.urls[2] == "https://cdn.example/live/seg1.ts?sid=7");

	// The blank line is a line of the document and stays one.
	CHECK(out.find("\n\n#EXTINF") != std::string::npos);
}

TEST_CASE("a playlist this box cannot carry across is refused rather than half rewritten",
          "[webtv]")
{
	RecordingMinter m;
	std::string out;
	size_t uris = 0;

	// A stretch of a file. The page would ask this box for the stretch, this
	// box would answer the whole file, and what the player decoded would be
	// wrong with nothing saying so.
	const std::string ranged =
		"#EXTM3U\n#EXT-X-BYTERANGE:1000@0\n#EXTINF:4.0,\nall.ts\n";
	CHECK(hls::rewrite(ranged, "https://cdn.example/x.m3u8", m, out, uris) ==
	      hls::Rewritten::HasByteRange);

	const std::string mapped =
		"#EXTM3U\n#EXT-X-MAP:URI=\"init.mp4\",BYTERANGE=\"700@0\"\n";
	CHECK(hls::rewrite(mapped, "https://cdn.example/x.m3u8", m, out, uris) ==
	      hls::Rewritten::HasByteRange);

	// An address this cannot resolve stops the rewrite, because one address
	// left alone is one the browser fetches from the far server itself, which
	// is the request this whole file exists to make unnecessary.
	const std::string odd = "#EXTM3U\n#EXTINF:4.0,\nfile:///etc/passwd\n";
	CHECK(hls::rewrite(odd, "https://cdn.example/x.m3u8", m, out, uris) ==
	      hls::Rewritten::BadReference);

	// And so does a minter that has run out of room.
	RecordingMinter tight;
	tight.refuse = true;
	tight.refuse_after = 1;
	const std::string two = "#EXTM3U\n#EXTINF:4,\na.ts\n#EXTINF:4,\nb.ts\n";
	CHECK(hls::rewrite(two, "https://cdn.example/x.m3u8", tight, out, uris) ==
	      hls::Rewritten::BadReference);

	CHECK(hls::rewrite("not a playlist", "https://cdn.example/x.m3u8", m, out, uris) ==
	      hls::Rewritten::NotAPlaylist);
}

TEST_CASE("a playlist longer than this box holds is refused", "[webtv]")
{
	std::string body = "#EXTM3U\n";
	for (size_t i = 0; i <= hls::kMaxUris; ++i)
		body += "#EXTINF:4,\nseg.ts\n";

	RecordingMinter m;
	std::string out;
	size_t uris = 0;
	CHECK(hls::rewrite(body, "https://cdn.example/x.m3u8", m, out, uris) ==
	      hls::Rewritten::TooManyUris);
}

// ============================================== which addresses may be fetched

TEST_CASE("the networks this box will not fetch from", "[webtv]")
{
	webtv::setRefusedNetworksForTest(NULL);

	// The one that matters: everything on this box that answers without a
	// credential is on it.
	CHECK(webtv::addressRefused("127.0.0.1"));
	CHECK(webtv::addressRefused("127.1.2.3"));
	CHECK(webtv::addressRefused("::1"));
	// The same address written the way a dual stack socket reports it.
	CHECK(webtv::addressRefused("::ffff:127.0.0.1"));

	// The home network behind the box.
	CHECK(webtv::addressRefused("10.0.0.1"));
	CHECK(webtv::addressRefused("172.16.0.1"));
	CHECK(webtv::addressRefused("172.31.255.254"));
	CHECK(webtv::addressRefused("192.168.1.1"));
	CHECK(webtv::addressRefused("fc00::1"));
	CHECK(webtv::addressRefused("fd12:3456::1"));

	// And the rest of what is not an address to fetch television from.
	CHECK(webtv::addressRefused("0.0.0.0"));
	CHECK(webtv::addressRefused("169.254.1.1"));
	CHECK(webtv::addressRefused("100.64.0.1"));
	CHECK(webtv::addressRefused("224.0.0.1"));
	CHECK(webtv::addressRefused("240.0.0.1"));
	CHECK(webtv::addressRefused("192.0.0.1"));
	CHECK(webtv::addressRefused("198.18.0.1"));
	CHECK(webtv::addressRefused("fe80::1"));
	CHECK(webtv::addressRefused("ff02::1"));
	CHECK(webtv::addressRefused("::"));

	// A form this has no rule about is refused, because no rule means no.
	CHECK(webtv::addressRefused(""));
	CHECK(webtv::addressRefused("not an address"));
	CHECK(webtv::addressRefused("999.1.1.1"));

	// What is left is the public internet, which is where these channels are.
	CHECK_FALSE(webtv::addressRefused("8.8.8.8"));
	CHECK_FALSE(webtv::addressRefused("1.1.1.1"));
	CHECK_FALSE(webtv::addressRefused("52.29.1.1"));
	// Just outside the two ranges whose edges are the easy ones to get wrong.
	CHECK_FALSE(webtv::addressRefused("172.32.0.1"));
	CHECK_FALSE(webtv::addressRefused("100.128.0.1"));
	CHECK_FALSE(webtv::addressRefused("2606:4700:4700::1111"));
}

TEST_CASE("only the two schemes a browser would have fetched itself", "[webtv]")
{
	CHECK(webtv::urlAcceptable("http://cdn.example/x.m3u8"));
	CHECK(webtv::urlAcceptable("HTTPS://cdn.example/x.m3u8"));

	CHECK_FALSE(webtv::urlAcceptable("file:///etc/passwd"));
	CHECK_FALSE(webtv::urlAcceptable("ftp://cdn.example/x"));
	CHECK_FALSE(webtv::urlAcceptable("gopher://cdn.example/x"));
	CHECK_FALSE(webtv::urlAcceptable("scp://cdn.example/x"));
	CHECK_FALSE(webtv::urlAcceptable(""));
	CHECK_FALSE(webtv::urlAcceptable(std::string("http://a\0b", 10)));
	CHECK_FALSE(webtv::urlAcceptable("http://a\r\nHost: b"));
	CHECK_FALSE(webtv::urlAcceptable(std::string("http://a/") + std::string(5000, 'x')));
}

TEST_CASE("a minted address is found once and only under its own name", "[webtv]")
{
	webtv::forgetParts();

	const std::string token =
		webtv::mintPart(0x4211, "https://cdn.example/seg1.ts", hls::UriKind::Segment);
	REQUIRE(!token.empty());
	// Long enough that nothing guesses one.
	CHECK(token.size() == 32);

	coreapi::ChannelId id = 0;
	std::string url;
	hls::UriKind kind = hls::UriKind::Playlist;
	REQUIRE(webtv::findPart(token, id, url, kind));
	CHECK(id == 0x4211u);
	CHECK(url == "https://cdn.example/seg1.ts");
	CHECK(kind == hls::UriKind::Segment);

	CHECK_FALSE(webtv::findPart("", id, url, kind));
	CHECK_FALSE(webtv::findPart(std::string(32, '0'), id, url, kind));

	// An address this box would not fetch is never minted, so no token can
	// ever stand for one.
	CHECK(webtv::mintPart(0x4211, "file:///etc/passwd", hls::UriKind::Segment).empty());

	webtv::forgetParts();
	CHECK(webtv::partsHeld() == 0);
}

TEST_CASE("the same segment keeps the address it was given", "[webtv]")
{
	webtv::forgetParts();

	/* A live playlist is read again every few seconds and most of what is in
	   it was in the reading before. A player merges the two by the address a
	   media sequence carries, so a fresh address for the same segment reads as
	   a different segment and the merge is refused. */
	const std::string first =
		webtv::mintPart(0x4211, "https://cdn.example/seg7.ts", hls::UriKind::Segment);
	const std::string again =
		webtv::mintPart(0x4211, "https://cdn.example/seg7.ts", hls::UriKind::Segment);
	REQUIRE(!first.empty());
	CHECK(first == again);
	CHECK(webtv::partsHeld() == 1);

	// Everything that makes it a different thing gets a different address.
	CHECK(webtv::mintPart(0x4211, "https://cdn.example/seg8.ts", hls::UriKind::Segment) != first);
	CHECK(webtv::mintPart(0x4212, "https://cdn.example/seg7.ts", hls::UriKind::Segment) != first);
	CHECK(webtv::mintPart(0x4211, "https://cdn.example/seg7.ts", hls::UriKind::Key) != first);

	// And the one that was reused is still the one it was.
	coreapi::ChannelId id = 0;
	std::string url;
	hls::UriKind kind = hls::UriKind::Playlist;
	REQUIRE(webtv::findPart(first, id, url, kind));
	CHECK(id == 0x4211u);
	CHECK(url == "https://cdn.example/seg7.ts");
	CHECK(kind == hls::UriKind::Segment);

	webtv::forgetParts();
	CHECK(webtv::partsHeld() == 0);
}

// ============================================================ over the wire
//
// The routes as the box serves them: nhttpd with the bridge on a loopback port,
// every answer that fetches carried on a job of its own (carry.h), against far
// servers on this machine. The shipped refusal list refuses this machine, so a
// case that wants to reach one puts the documentation range in its place.

namespace
{

const char kToken[] = "0123456789abcdef0123456789abcdef";

/* The box. Every case leaves the transport, the refusal list, the minted parts
   and the tokens as they ship, whichever line it left through. */
struct WebBox
{
	InstalledDependencies wired;
	WireServer            server;

	explicit WebBox(WireServer::Mode m = WireServer::Threaded)
		: server(4, 100, m)
	{
		addApiToken(tokenLookupPrefix(kToken), hashSecret(kToken), AuthLevel::Write, "", 0);
	}

	~WebBox()
	{
		carry::waitIdle(5000);
		carry::setCarrier(carry::Fetch, NULL);
		carry::setStopBoundMsForTest(0);
		webtv::setRefusedNetworksForTest(NULL);
		webtv::setOpenDelayMsForTest(0);
		webtv::forgetParts();
		forgetApiTokens();
	}

	int port() const { return server.port; }

	std::string addWebChannel(coreapi::ChannelId id, const std::string &url)
	{
		coreapi::ChannelInfo ch;
		ch.id = id;
		ch.name = "Web";
		ch.url = url;
		ch.kind = coreapi::ServiceKind::WebTv;
		wired.channels.channels.push_back(ch);

		char buf[24];
		std::snprintf(buf, sizeof(buf), "%llx", (unsigned long long) id);
		return buf;
	}

	void addTunedChannel(coreapi::ChannelId id)
	{
		coreapi::ChannelInfo ch;
		ch.id = id;
		ch.name = "Aerial";
		ch.kind = coreapi::ServiceKind::Tv;
		wired.channels.channels.push_back(ch);
	}

private:
	WebBox(const WebBox &);
	WebBox &operator=(const WebBox &);
};

std::string ask(const char *method, const std::string &path, const std::string &extra = "")
{
	return std::string(method) + " " + path + " HTTP/1.0\r\nHost: box\r\n"
		"Authorization: Bearer " + kToken + "\r\n" + extra + "\r\n";
}

std::string playlistPath(const std::string &id)
{
	return "/api/v1/webtv/" + id + "/playlist.m3u8";
}

std::string streamPath(const std::string &id)
{
	return "/api/v1/webtv/" + id + "/stream";
}

std::string partPath(coreapi::ChannelId id, const std::string &url, hls::UriKind kind)
{
	char buf[24];
	std::snprintf(buf, sizeof(buf), "%llx", (unsigned long long) id);
	return "/api/v1/webtv/" + std::string(buf) + "/part/" + webtv::mintPart(id, url, kind);
}

// The first minted address in a rewritten playlist, as the path a browser asks.
std::string firstPart(const std::string &playlist, const std::string &id)
{
	const std::string mark = "/api/v1/webtv/" + id + "/part/";
	const size_t at = playlist.find(mark);
	if (at == std::string::npos)
		return std::string();
	const size_t end = playlist.find('\n', at);
	return playlist.substr(at, (end == std::string::npos ? playlist.size() : end) - at);
}

/* A far server that takes connections and never answers: the kernel completes
   each one into the backlog, the request goes out and nothing comes back, which
   from this box looks like a far server that has stopped in the middle of
   answering. */
struct SilentServer
{
	int fd;
	int port;

	SilentServer() : fd(-1), port(0)
	{
		fd = ::socket(AF_INET, SOCK_STREAM, 0);
		REQUIRE(fd >= 0);
		struct sockaddr_in a;
		std::memset(&a, 0, sizeof(a));
		a.sin_family = AF_INET;
		a.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
		REQUIRE(::bind(fd, (struct sockaddr *) &a, sizeof(a)) == 0);
		REQUIRE(::listen(fd, 32) == 0);
		socklen_t n = sizeof(a);
		REQUIRE(::getsockname(fd, (struct sockaddr *) &a, &n) == 0);
		port = ntohs(a.sin_port);
	}

	~SilentServer()
	{
		if (fd >= 0)
			::close(fd);
	}

	std::string url(const std::string &path) const
	{
		char buf[64];
		std::snprintf(buf, sizeof(buf), "http://127.0.0.1:%d", port);
		return std::string(buf) + path;
	}

private:
	SilentServer(const SilentServer &);
	SilentServer &operator=(const SilentServer &);
};

// A request sent and nothing read, for a case that holds several open at once.
int sendAsk(int port, const std::string &request)
{
	const int fd = ::socket(AF_INET, SOCK_STREAM, 0);
	if (fd < 0)
		return -1;
	struct sockaddr_in a;
	std::memset(&a, 0, sizeof(a));
	a.sin_family = AF_INET;
	a.sin_port = htons((uint16_t) port);
	a.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
	if (::connect(fd, (struct sockaddr *) &a, sizeof(a)) != 0 ||
	    ::send(fd, request.data(), request.size(), MSG_NOSIGNAL) != (ssize_t) request.size())
	{
		::close(fd);
		return -1;
	}
	return fd;
}

// Whatever arrives on fd until the far end closes it or the time is up.
std::string readAll(int fd, int wait_ms)
{
	std::string got;
	const int64_t until = nowMs() + wait_ms;
	for (;;)
	{
		const int64_t left = until - nowMs();
		if (left <= 0)
			return got;
		struct pollfd p;
		p.fd = fd;
		p.events = POLLIN;
		p.revents = 0;
		if (poll(&p, 1, (int) left) <= 0)
			continue;
		char buf[4096];
		const ssize_t n = ::recv(fd, buf, sizeof(buf), 0);
		if (n <= 0)
			return got;
		got.append(buf, (size_t) n);
	}
}

size_t seatsAfter(size_t want, int budget_ms)
{
	for (int waited = 0; waited < budget_ms; waited += 10)
	{
		if (carry::seatsTaken(carry::Fetch) == want)
			return want;
		napMs(10);
	}
	return carry::seatsTaken(carry::Fetch);
}

size_t runningAfter(size_t want, int budget_ms)
{
	for (int waited = 0; waited < budget_ms; waited += 10)
	{
		if (carry::running() == want)
			return want;
		napMs(10);
	}
	return carry::running();
}

std::string problemCode(const std::string &body)
{
	const std::string mark = "\"type\":\"/errors/";
	const size_t at = body.find(mark);
	if (at == std::string::npos)
		return std::string();
	const size_t from = at + mark.size();
	return body.substr(from, body.find('"', from) - from);
}

} // namespace

TEST_CASE("a channel's playlist is fetched on a job of its own and comes back rewritten",
          "[webtv][wire]")
{
	WebBox box;
	FarServer far;
	REQUIRE(far.start());
	const std::vector<NetPrefix> only_docs = onlyDocumentationRange();
	webtv::setRefusedNetworksForTest(&only_docs);

	far.first = httpReply("application/vnd.apple.mpegurl",
	                      "#EXTM3U\n#EXT-X-STREAM-INF:BANDWIDTH=500000\nlow/index.m3u8\n");
	const std::string id = box.addWebChannel(0x1234, far.url("/master.m3u8"));

	WireStream r(box.port(), ask("GET", playlistPath(id)).c_str());
	REQUIRE(r.ok());
	const std::string body = r.readToEof(5000);
	CHECK(r.code() == 200);
	CHECK(r.headerOf("Content-Type") == "application/vnd.apple.mpegurl");
	// read whole before it is answered, so it says how long it is
	CHECK(r.headerOf("Content-Length") == std::to_string(body.size()));
	CHECK(r.headerOf("Cache-Control").find("no-store") != std::string::npos);
	CHECK(body.compare(0, 8, "#EXTM3U\n") == 0);
	CHECK(body.find("127.0.0.1") == std::string::npos);
	CHECK(body.find("low/index.m3u8") == std::string::npos);
	CHECK(!firstPart(body, id).empty());
	CHECK(webtv::partsHeld() == 1);
	CHECK(runningAfter(0, 2000) == 0);

	// the nested playlist the first one named is read and rewritten the same way
	far.first = httpReply("application/vnd.apple.mpegurl",
	                      "#EXTM3U\n#EXT-X-TARGETDURATION:4\n#EXTINF:4,\nseg1.ts\n");
	WireStream nested(box.port(), ask("GET", firstPart(body, id)).c_str());
	REQUIRE(nested.ok());
	const std::string inner = nested.readToEof(5000);
	CHECK(nested.code() == 200);
	CHECK(far.lastHead().compare(0, 20, "GET /low/index.m3u8 ") == 0);
	CHECK(inner.find("seg1.ts") == std::string::npos);
	CHECK(!firstPart(inner, id).empty());
	CHECK(runningAfter(0, 2000) == 0);
}

TEST_CASE("a part is passed on as the far server sent it, with its length", "[webtv][wire]")
{
	WebBox box;
	FarServer far;
	REQUIRE(far.start());
	const std::vector<NetPrefix> only_docs = onlyDocumentationRange();
	webtv::setRefusedNetworksForTest(&only_docs);
	box.addWebChannel(0x1234, far.url("/master.m3u8"));

	// bytes that are not a document: what comes back is what the far server
	// sent, not something read and written out again
	const std::string payload(3000, '\x47');
	far.first = httpReply("video/MP2T", payload);
	WireStream seg(box.port(),
	               ask("GET", partPath(0x1234, far.url("/seg1.ts"), hls::UriKind::Segment)).c_str());
	REQUIRE(seg.ok());
	CHECK(seg.readToEof(5000) == payload);
	CHECK(seg.code() == 200);
	CHECK(seg.headerOf("Content-Length") == "3000");
	CHECK(seg.headerOf("Content-Type") == "video/MP2T");
	// asked for the bytes as they are held, since they are passed on unread
	CHECK(far.lastHead().find("Accept-Encoding: identity") != std::string::npos);
	CHECK(runningAfter(0, 2000) == 0);
	CHECK(carry::seatsTaken(carry::Fetch) == 0);
}

TEST_CASE("of the far server's head only the type and an unencoded length go on", "[webtv][wire]")
{
	WebBox box;
	FarServer far;
	REQUIRE(far.start());
	const std::vector<NetPrefix> only_docs = onlyDocumentationRange();
	webtv::setRefusedNetworksForTest(&only_docs);
	box.addWebChannel(0x1234, far.url("/master.m3u8"));

	far.first = "HTTP/1.1 200 OK\r\nContent-Type: video/MP2T\r\nContent-Length: 10\r\n"
		"Content-Encoding: gzip\r\nContent-Disposition: attachment; filename=x.html\r\n"
		"Set-Cookie: far=1; Path=/\r\nConnection: close\r\n\r\n0123456789";
	WireStream seg(box.port(),
	               ask("GET", partPath(0x1234, far.url("/seg1.ts"), hls::UriKind::Segment)).c_str());
	REQUIRE(seg.ok());
	CHECK(seg.readToEof(5000) == "0123456789");
	CHECK(seg.code() == 200);
	CHECK(seg.headerOf("Content-Type") == "video/MP2T");
	// the length counts encoded bytes, and the browser is told no encoding
	CHECK_FALSE(seg.hasHeader("Content-Length"));
	CHECK_FALSE(seg.hasHeader("Content-Encoding"));
	CHECK_FALSE(seg.hasHeader("Content-Disposition"));
	CHECK_FALSE(seg.hasHeader("Set-Cookie"));
	CHECK(seg.headerOf("X-Content-Type-Options") == "nosniff");
}

TEST_CASE("a stream's bytes are passed on as they arrive and not collected first",
          "[webtv][wire]")
{
	WebBox box;
	FarServer far;
	REQUIRE(far.start());
	const std::vector<NetPrefix> only_docs = onlyDocumentationRange();
	webtv::setRefusedNetworksForTest(&only_docs);
	const std::string id = box.addWebChannel(0x99, far.url("/live.ts"));

	/* A head and a first piece, a wait, then the rest. A box that collected the
	   whole body could not have a byte on the wire before the wait is over. */
	const std::string half(4000, 'a');
	char head[160];
	std::snprintf(head, sizeof(head),
	              "HTTP/1.1 200 OK\r\nContent-Type: video/MP2T\r\nContent-Length: %u\r\n"
	              "Connection: close\r\n\r\n", (unsigned) (half.size() * 2));
	far.first = std::string(head) + half;
	far.pause_ms = 1500;
	far.rest = half;

	const int64_t asked_at = nowMs();
	const int fd = sendAsk(box.port(), ask("GET", streamPath(id)));
	REQUIRE(fd >= 0);
	std::string got;
	while (got.size() < 3000 && nowMs() - asked_at < 1200)
	{
		struct pollfd p;
		p.fd = fd;
		p.events = POLLIN;
		p.revents = 0;
		if (poll(&p, 1, 100) <= 0)
			continue;
		char buf[2048];
		const ssize_t n = ::recv(fd, buf, sizeof(buf), 0);
		if (n <= 0)
			break;
		got.append(buf, (size_t) n);
	}
	CHECK(got.size() >= 3000);
	CHECK(got.find("200 OK") != std::string::npos);
	CHECK(nowMs() - asked_at < 1200);
	CHECK(far.finishedAt() == 0);
	got += readAll(fd, 4000);
	::close(fd);
	CHECK(got.size() > half.size() * 2);
	CHECK(runningAfter(0, 3000) == 0);
}

TEST_CASE("the shipped list refuses an address on this machine before anything is asked",
          "[webtv][wire]")
{
	WebBox box;
	FarServer far;
	REQUIRE(far.start());
	webtv::setRefusedNetworksForTest(NULL);
	far.first = httpReply("application/vnd.apple.mpegurl", "#EXTM3U\n");
	const std::string id = box.addWebChannel(0x1234, far.url("/master.m3u8"));

	const std::string paths[] = {
		playlistPath(id), streamPath(id),
		partPath(0x1234, far.url("/seg1.ts"), hls::UriKind::Segment),
	};
	for (size_t i = 0; i < sizeof(paths) / sizeof(paths[0]); ++i)
	{
		INFO(paths[i]);
		WireStream r(box.port(), ask("GET", paths[i]).c_str());
		REQUIRE(r.ok());
		CHECK(r.code() == 403);
		CHECK(problemCode(r.readToEof(5000)) == "address-refused");
	}
	// refused before a connection was made, which is the whole difference
	// between a refusal and a request that happened to fail
	CHECK(far.timesAsked() == 0);
}

TEST_CASE("a redirection into a refused network is refused, read or passed on", "[webtv][wire]")
{
	WebBox box;
	FarServer far;
	REQUIRE(far.start());
	const std::vector<NetPrefix> only_docs = onlyDocumentationRange();
	webtv::setRefusedNetworksForTest(&only_docs);
	far.first = "HTTP/1.1 302 Found\r\nLocation: http://192.0.2.7:80/inside\r\n"
		"Content-Length: 0\r\nConnection: close\r\n\r\n";
	const std::string id = box.addWebChannel(0x1234, far.url("/master.m3u8"));

	const std::string paths[] = {
		playlistPath(id), streamPath(id),
		partPath(0x1234, far.url("/seg1.ts"), hls::UriKind::Segment),
	};
	for (size_t i = 0; i < sizeof(paths) / sizeof(paths[0]); ++i)
	{
		INFO(paths[i]);
		WireStream r(box.port(), ask("GET", paths[i]).c_str());
		REQUIRE(r.ok());
		CHECK(r.code() == 403);
		CHECK(problemCode(r.readToEof(5000)) == "address-refused");
	}
	CHECK(far.timesAsked() == 3);
	CHECK(runningAfter(0, 2000) == 0);
}

TEST_CASE("an address nobody minted, or minted for another channel, is not fetched",
          "[webtv][wire]")
{
	WebBox box;
	FarServer far;
	REQUIRE(far.start());
	const std::vector<NetPrefix> only_docs = onlyDocumentationRange();
	webtv::setRefusedNetworksForTest(&only_docs);
	box.addWebChannel(0x1234, far.url("/one.m3u8"));
	const std::string two = box.addWebChannel(0x5678, far.url("/two.m3u8"));

	WireStream unknown(box.port(),
	                   ask("GET", "/api/v1/webtv/1234/part/" + std::string(32, 'a')).c_str());
	REQUIRE(unknown.ok());
	CHECK(unknown.code() == 404);
	CHECK(problemCode(unknown.readToEof(5000)) == "no-such-part");

	const std::string token =
		webtv::mintPart(0x1234, far.url("/seg1.ts"), hls::UriKind::Segment);
	WireStream crossed(box.port(), ask("GET", "/api/v1/webtv/" + two + "/part/" + token).c_str());
	REQUIRE(crossed.ok());
	CHECK(crossed.code() == 404);
	CHECK(problemCode(crossed.readToEof(5000)) == "no-such-part");
	CHECK(far.timesAsked() == 0);
}

TEST_CASE("a channel the box tunes to, or none at all, has nothing here to fetch",
          "[webtv][wire]")
{
	WebBox box;
	box.addTunedChannel(0x2222);

	const std::string paths[] = { playlistPath("2222"), streamPath("2222") };
	for (size_t i = 0; i < 2; ++i)
	{
		INFO(paths[i]);
		WireStream tuned(box.port(), ask("GET", paths[i]).c_str());
		REQUIRE(tuned.ok());
		CHECK(tuned.code() == 404);
		CHECK(problemCode(tuned.readToEof(5000)) == "not-a-web-channel");
	}
	const std::string none[] = { playlistPath("9999"), streamPath("9999") };
	for (size_t i = 0; i < 2; ++i)
	{
		INFO(none[i]);
		WireStream nobody(box.port(), ask("GET", none[i]).c_str());
		REQUIRE(nobody.ok());
		CHECK(nobody.code() == 404);
		CHECK(problemCode(nobody.readToEof(5000)) == "no-such-channel");
	}
	CHECK(carry::running() == 0);
}

TEST_CASE("what goes wrong at the far server is said as such, read or passed on",
          "[webtv][wire]")
{
	WebBox box;
	FarServer far;
	REQUIRE(far.start());
	const std::vector<NetPrefix> only_docs = onlyDocumentationRange();
	webtv::setRefusedNetworksForTest(&only_docs);
	const std::string id = box.addWebChannel(0x1234, far.url("/gone.m3u8"));

	far.first = "HTTP/1.1 404 Not Found\r\nContent-Length: 9\r\nConnection: close\r\n\r\nnot here.";
	const std::string paths[] = {
		playlistPath(id), streamPath(id),
		partPath(0x1234, far.url("/seg1.ts"), hls::UriKind::Segment),
	};
	for (size_t i = 0; i < sizeof(paths) / sizeof(paths[0]); ++i)
	{
		INFO(paths[i]);
		WireStream r(box.port(), ask("GET", paths[i]).c_str());
		REQUIRE(r.ok());
		CHECK(r.code() == 503);
		const std::string body = r.readToEof(5000);
		CHECK(problemCode(body) == "upstream-unreachable");
		CHECK(body.find("answered 404") != std::string::npos);
		// nothing of what the far server said comes along
		CHECK(body.find("not here") == std::string::npos);
	}

	// and a far server that is not there at all
	const int port = far.port;
	far.stop();
	const std::string gone = box.addWebChannel(0x4321, "http://127.0.0.1:" + std::to_string(port) + "/x.ts");
	WireStream nobody(box.port(), ask("GET", streamPath(gone)).c_str());
	REQUIRE(nobody.ok());
	CHECK(nobody.code() == 503);
	CHECK(problemCode(nobody.readToEof(5000)) == "upstream-unreachable");
	CHECK(runningAfter(0, 2000) == 0);
}

TEST_CASE("an answer that is no playlist, or one this box cannot read, is said as such",
          "[webtv][wire]")
{
	WebBox box;
	FarServer far;
	REQUIRE(far.start());
	const std::vector<NetPrefix> only_docs = onlyDocumentationRange();
	webtv::setRefusedNetworksForTest(&only_docs);
	const std::string id = box.addWebChannel(0x1234, far.url("/live.ts"));
	const std::string nested = partPath(0x1234, far.url("/low.m3u8"), hls::UriKind::Playlist);

	far.first = httpReply("video/MP2T", std::string(2000, '\x47'));
	const std::string lists[] = { playlistPath(id), nested };
	for (size_t i = 0; i < 2; ++i)
	{
		INFO(lists[i]);
		WireStream r(box.port(), ask("GET", lists[i]).c_str());
		REQUIRE(r.ok());
		CHECK(r.code() == 415);
		CHECK(problemCode(r.readToEof(5000)) == "not-a-playlist");
	}

	far.first = httpReply("application/vnd.apple.mpegurl",
	                      "#EXTM3U\n#EXT-X-BYTERANGE:1000@0\n#EXTINF:4.0,\nall.ts\n");
	for (size_t i = 0; i < 2; ++i)
	{
		INFO(lists[i]);
		WireStream r(box.port(), ask("GET", lists[i]).c_str());
		REQUIRE(r.ok());
		CHECK(r.code() == 415);
		CHECK(problemCode(r.readToEof(5000)) == "playlist-unreadable");
	}
	CHECK(runningAfter(0, 2000) == 0);
}

TEST_CASE("every fetch takes one of six seats, playlists included, and the seventh is refused",
          "[webtv][wire]")
{
	WebBox box;
	SilentServer silent;
	const std::vector<NetPrefix> only_docs = onlyDocumentationRange();
	webtv::setRefusedNetworksForTest(&only_docs);
	const std::string id = box.addWebChannel(0x1234, silent.url("/master.m3u8"));
	const std::string part = partPath(0x1234, silent.url("/seg1.ts"), hls::UriKind::Segment);

	// two playlists, two streams and two parts, each waiting for a far server
	// that never answers
	const std::string held[] = {
		playlistPath(id), playlistPath(id), streamPath(id), streamPath(id), part, part,
	};
	std::vector<int> open;
	for (size_t i = 0; i < 6; ++i)
	{
		const int fd = sendAsk(box.port(), ask("GET", held[i]));
		REQUIRE(fd >= 0);
		open.push_back(fd);
		REQUIRE(seatsAfter(i + 1, 3000) == i + 1);
	}
	CHECK(carry::seatsOf(carry::Fetch) == 6);

	const std::string asked[] = { playlistPath(id), streamPath(id), part };
	for (size_t i = 0; i < 3; ++i)
	{
		INFO(asked[i]);
		WireStream full(box.port(), ask("GET", asked[i]).c_str());
		REQUIRE(full.ok());
		CHECK(full.code() == 503);
		CHECK(full.headerOf("Retry-After") == "5");
		CHECK(problemCode(full.readToEof(5000)) == "too-many-streams");

		// a HEAD says what the GET would meet
		WireStream head(box.port(), ask("HEAD", asked[i]).c_str());
		REQUIRE(head.ok());
		CHECK(head.code() == 503);
	}

	// the rest of the box answers as before
	const int64_t t0 = nowMs();
	WireStream other(box.port(), "GET /api/v1/session HTTP/1.0\r\nHost: box\r\n\r\n");
	REQUIRE(other.ok());
	CHECK(other.code() == 200);
	CHECK(nowMs() - t0 < 200);

	// a browser that leaves ends its fetch, and its seat is free again
	const int64_t left_at = nowMs();
	for (size_t i = 0; i < open.size(); ++i)
		::close(open[i]);
	CHECK(seatsAfter(0, 3000) == 0);
	CHECK(nowMs() - left_at < 2000);
	CHECK(runningAfter(0, 2000) == 0);
}

TEST_CASE("without connection threads a fetch holds no request behind it", "[webtv][wire]")
{
	WebBox box(WireServer::Serial);
	SilentServer silent;
	const std::vector<NetPrefix> only_docs = onlyDocumentationRange();
	webtv::setRefusedNetworksForTest(&only_docs);
	const std::string id = box.addWebChannel(0x1234, silent.url("/live.ts"));

	const int fd = sendAsk(box.port(), ask("GET", streamPath(id)));
	REQUIRE(fd >= 0);
	REQUIRE(seatsAfter(1, 3000) == 1);

	const int64_t t0 = nowMs();
	WireStream other(box.port(), "GET /api/v1/session HTTP/1.0\r\nHost: box\r\n\r\n");
	REQUIRE(other.ok());
	CHECK(other.code() == 200);
	CHECK(nowMs() - t0 < 500);

	::close(fd);
	CHECK(seatsAfter(0, 3000) == 0);
}

TEST_CASE("a HEAD tells what the GET would meet and fetches nothing", "[webtv][wire]")
{
	WebBox box;
	FarServer far;
	REQUIRE(far.start());
	const std::vector<NetPrefix> only_docs = onlyDocumentationRange();
	webtv::setRefusedNetworksForTest(&only_docs);
	far.first = httpReply("application/vnd.apple.mpegurl", "#EXTM3U\n");
	const std::string id = box.addWebChannel(0x1234, far.url("/master.m3u8"));

	const std::string paths[] = {
		playlistPath(id), streamPath(id),
		partPath(0x1234, far.url("/seg1.ts"), hls::UriKind::Segment),
	};
	for (size_t i = 0; i < sizeof(paths) / sizeof(paths[0]); ++i)
	{
		INFO(paths[i]);
		WireStream head(box.port(), ask("HEAD", paths[i]).c_str());
		REQUIRE(head.ok());
		CHECK(head.code() == 200);
		CHECK(head.readToEof(3000).empty());
	}
	CHECK(far.timesAsked() == 0);
	CHECK(carry::running() == 0);
}

TEST_CASE("a server that stops ends the fetches and refuses new ones until it starts again",
          "[webtv][wire]")
{
	WebBox box;
	SilentServer silent;
	FarServer far;
	REQUIRE(far.start());
	const std::vector<NetPrefix> only_docs = onlyDocumentationRange();
	webtv::setRefusedNetworksForTest(&only_docs);
	const std::string stuck = box.addWebChannel(0x1234, silent.url("/master.m3u8"));
	far.first = httpReply("application/vnd.apple.mpegurl", "#EXTM3U\n#EXTINF:4,\nseg1.ts\n");
	const std::string fine = box.addWebChannel(0x5678, far.url("/master.m3u8"));

	const int a = sendAsk(box.port(), ask("GET", playlistPath(stuck)));
	const int b = sendAsk(box.port(), ask("GET", streamPath(stuck)));
	REQUIRE(a >= 0);
	REQUIRE(b >= 0);
	REQUIRE(seatsAfter(2, 3000) == 2);

	// a reload: the fetches end within the stop and say nothing more
	const int64_t t0 = nowMs();
	CApiBridge::stopStreams();
	CHECK(nowMs() - t0 < 3000);
	CHECK(carry::running() == 0);
	CHECK(readAll(a, 1000).empty());
	CHECK(readAll(b, 1000).empty());
	::close(a);
	::close(b);

	// stopped, a new fetch is refused before anything is asked
	WireStream refused(box.port(), ask("GET", playlistPath(fine)).c_str());
	REQUIRE(refused.ok());
	CHECK(refused.code() == 503);
	CHECK(problemCode(refused.readToEof(5000)) == "server-stopping");
	CHECK(far.timesAsked() == 0);

	CApiBridge::startStreams();
	WireStream again(box.port(), ask("GET", playlistPath(fine)).c_str());
	REQUIRE(again.ok());
	CHECK(again.code() == 200);
	CHECK(again.readToEof(5000).compare(0, 7, "#EXTM3U") == 0);
	CHECK(runningAfter(0, 2000) == 0);
}

TEST_CASE("a fetch stuck where nothing reaches it outlives the stop and ends on its own",
          "[webtv][wire]")
{
	WebBox box;
	FarServer far;
	REQUIRE(far.start());
	const std::vector<NetPrefix> only_docs = onlyDocumentationRange();
	webtv::setRefusedNetworksForTest(&only_docs);
	far.first = httpReply("video/MP2T", std::string(2000, '\x47'));
	const std::string id = box.addWebChannel(0x1234, far.url("/live.ts"));

	/* Held before its connection is opened, as a slow name lookup holds one:
	   neither the stop nor the progress callback reach it there. */
	webtv::setOpenDelayMsForTest(1500);
	carry::setStopBoundMsForTest(300);
	const int fd = sendAsk(box.port(), ask("GET", streamPath(id)));
	REQUIRE(fd >= 0);
	REQUIRE(seatsAfter(1, 3000) == 1);

	const int64_t t0 = nowMs();
	CApiBridge::stopStreams();
	CHECK(nowMs() - t0 < 1000);
	CHECK(carry::running() == 1);
	CApiBridge::startStreams();

	// it ends once the hold is over, writes nothing, and gives its seat back
	CHECK(runningAfter(0, 4000) == 0);
	CHECK(carry::seatsTaken(carry::Fetch) == 0);
	CHECK(readAll(fd, 500).empty());
	::close(fd);
	webtv::setOpenDelayMsForTest(0);
}

TEST_CASE("a far server that stops halfway is given up on once the browser leaves",
          "[webtv][wire]")
{
	WebBox box;
	FarServer far;
	REQUIRE(far.start());
	const std::vector<NetPrefix> only_docs = onlyDocumentationRange();
	webtv::setRefusedNetworksForTest(&only_docs);
	const std::string id = box.addWebChannel(0x1234, far.url("/live.ts"));

	far.first = "HTTP/1.1 200 OK\r\nContent-Type: video/MP2T\r\nConnection: close\r\n\r\n" +
		std::string(1000, '\x47');
	far.pause_ms = 3000;
	const int fd = sendAsk(box.port(), ask("GET", streamPath(id)));
	REQUIRE(fd >= 0);
	const std::string got = readAll(fd, 800);
	CHECK(got.find("200 OK") != std::string::npos);
	REQUIRE(seatsAfter(1, 1000) == 1);

	const int64_t left_at = nowMs();
	::close(fd);
	CHECK(seatsAfter(0, 2500) == 0);
	CHECK(nowMs() - left_at < 2000);
}

TEST_CASE("a playlist named by a playlist names no further playlist", "[webtv][wire]")
{
	WebBox box;
	FarServer far;
	REQUIRE(far.start());
	const std::vector<NetPrefix> only_docs = onlyDocumentationRange();
	webtv::setRefusedNetworksForTest(&only_docs);
	box.addWebChannel(0x1234, far.url("/master.m3u8"));
	const std::string nested = partPath(0x1234, far.url("/low/index.m3u8"), hls::UriKind::Playlist);

	// a media playlist one level down is read as usual
	far.first = httpReply("application/vnd.apple.mpegurl", "#EXTM3U\n#EXTINF:4,\nseg1.ts\n");
	WireStream media(box.port(), ask("GET", nested).c_str());
	REQUIRE(media.ok());
	media.readToEof(5000);
	CHECK(media.code() == 200);

	// one that goes on to more playlists is not, whichever tag names them
	const char *const deeper[] = {
		"#EXTM3U\n#EXT-X-STREAM-INF:BANDWIDTH=1\nnext.m3u8\n",
		"#EXTM3U\n#EXT-X-MEDIA:TYPE=AUDIO,GROUP-ID=\"a\",NAME=\"x\",URI=\"audio.m3u8\"\n",
		"#EXTM3U\n#EXT-X-I-FRAME-STREAM-INF:BANDWIDTH=1,URI=\"iframes.m3u8\"\n",
	};
	for (size_t i = 0; i < sizeof(deeper) / sizeof(deeper[0]); ++i)
	{
		INFO(deeper[i]);
		far.first = httpReply("application/vnd.apple.mpegurl", deeper[i]);
		WireStream r(box.port(), ask("GET", nested).c_str());
		REQUIRE(r.ok());
		const std::string body = r.readToEof(5000);
		CHECK(r.code() == 415);
		CHECK(problemCode(body) == "playlist-unreadable");
		CHECK(body.find("deeper") != std::string::npos);
	}
	CHECK(runningAfter(0, 2000) == 0);
}

TEST_CASE("a part larger than its kind may be is refused or cut off; a stream is not", "[webtv][wire]")
{
	CHECK(webtv::byteCeilingOf(webtv::RelaySegment) == 64u * 1024u * 1024u);
	CHECK(webtv::byteCeilingOf(webtv::RelayMap) == 4u * 1024u * 1024u);
	CHECK(webtv::byteCeilingOf(webtv::RelayKey) == 4096u);
	CHECK(webtv::byteCeilingOf(webtv::RelayStream) == 0u);

	WebBox box;
	FarServer far;
	REQUIRE(far.start());
	const std::vector<NetPrefix> only_docs = onlyDocumentationRange();
	webtv::setRefusedNetworksForTest(&only_docs);
	const std::string id = box.addWebChannel(0x1234, far.url("/live.ts"));
	const std::string key = partPath(0x1234, far.url("/key.bin"), hls::UriKind::Key);
	const std::string big(5000, 'k');

	// announced longer than a key may be: refused before anything goes out
	far.first = httpReply("application/octet-stream", big);
	WireStream announced(box.port(), ask("GET", key).c_str());
	REQUIRE(announced.ok());
	const std::string said = announced.readToEof(5000);
	CHECK(announced.code() == 503);
	CHECK(problemCode(said) == "upstream-unreachable");
	CHECK(said.find("kkkk") == std::string::npos);

	// sent without a length and going on past it: cut off at the ceiling
	far.first = "HTTP/1.1 200 OK\r\nContent-Type: application/octet-stream\r\nConnection: close\r\n\r\n" + big;
	WireStream sent(box.port(), ask("GET", key).c_str());
	REQUIRE(sent.ok());
	const std::string cut = sent.readToEof(5000);
	CHECK(sent.code() == 200);
	CHECK(cut.size() <= 4096u);

	// a channel's stream goes on for as long as it is sent
	const std::string long_stream(300 * 1024, 's');
	far.first = "HTTP/1.1 200 OK\r\nContent-Type: video/MP2T\r\nConnection: close\r\n\r\n" + long_stream;
	WireStream stream(box.port(), ask("GET", streamPath(id)).c_str());
	REQUIRE(stream.ok());
	CHECK(stream.readToEof(5000) == long_stream);
	CHECK(runningAfter(0, 2000) == 0);
}
