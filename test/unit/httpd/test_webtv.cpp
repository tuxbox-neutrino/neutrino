/*
 * test_webtv.cpp - tests for the pass through, the playlists and what may be fetched
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

#include "httpd/hlsrewrite.h"
#include "httpd/netmatch.h"
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

void napMs(int ms)
{
	struct timespec t;
	t.tv_sec = ms / 1000;
	t.tv_nsec = (long) (ms % 1000) * 1000 * 1000;
	nanosleep(&t, NULL);
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
