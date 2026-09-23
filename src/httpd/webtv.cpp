/*
 * webtv.cpp - fetching a channel's stream off a far server and passing it on
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

#include "webtv.h"

#include "http.h"
#include "randomsource.h"
#include "status.h"

#include "coreapi/base/errors.h"

#include <cstddef>
#include <cstdio>
#include <cstring>
#include <vector>

#include <stdint.h>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <pthread.h>
#include <strings.h>
#include <sys/socket.h>
#include <time.h>

#include <curl/curl.h>

#include <OpenThreads/Condition>
#include <OpenThreads/Mutex>
#include <OpenThreads/ScopedLock>


namespace httpd
{

namespace webtv
{

namespace
{

/* The networks nothing here connects to, written as text and read once.

   Loopback and the two private families are the ones an attack aims at: every service
   on this box that answers without a credential is on the first, and the printer, the
   router and the other box are on the second.

   The rest are here because leaving a hole in a refusal list is how a refusal list
   stops being one: the carrier range, which is a private range on a large network;
   link local, which on this box is whatever a machine on the wire claims; the ranges
   reserved for documentation and for benchmarking; multicast, which is not a thing to
   open a connection to; and the two blocks that carry a v4 address inside a v6 one,
   which would otherwise be a spelling of the private ranges. */
const char *const kRefused[] =
{
	"0.0.0.0/8",
	"10.0.0.0/8",
	"100.64.0.0/10",
	"127.0.0.0/8",
	"169.254.0.0/16",
	"172.16.0.0/12",
	"192.0.0.0/24",
	"192.0.2.0/24",
	"192.168.0.0/16",
	"198.18.0.0/15",
	"198.51.100.0/24",
	"203.0.113.0/24",
	"224.0.0.0/4",
	"240.0.0.0/4",
	"::/128",
	"::1/128",
	"64:ff9b::/96",
	"2001:db8::/32",
	"fc00::/7",
	"fe80::/10",
	"ff00::/8"
};

/* What one write to the library carries. A segment arrives in tens of
   kilobytes at a time, so this is the buffer the library keeps per pass
   through and not a rate. */
const size_t kBlockSize = 32u * 1024u;

/* The most one pass through holds for a browser that is not reading it yet.

   This is what makes the fetch and the sending one speed rather than two: past it the
   thread doing the fetching waits, the far server's window closes, and the box stops
   pulling bytes it has nowhere to put. Without it a browser that paused would leave
   this box downloading a live stream into memory for as long as the stream runs. */
const size_t kPendingCeiling = 256u * 1024u;

// How long to wait for a name and a connection. Past this the far server is
// not answering, and a channel that takes longer than this to start is a
// channel that has not started.
const long kConnectSeconds = 8;

/* How long a fetch may stall before it is given up. Not a ceiling on the whole
   transfer, because a live stream runs for hours by design; a ceiling on
   silence, which is what a far server that has gone away looks like from here.
   It is also what keeps a thread from being held by one: nothing else would
   ever end that fetch. */
const long kStallSeconds = 20;

// The whole of a document that is a document, which unlike a stream has an end
// and is expected to arrive in one go.
const long kDocumentSeconds = 15;

const size_t kMaxRedirects = 5;

/* What a channel of these lists is worth as a token, and how many are kept.

   The far servers sign their own addresses and expire them in minutes, so an
   address held longer than this is one that has stopped working anyway. The
   count is what a box holds while several players each read a playlist of a
   few dozen addresses every few seconds. */
const unsigned kPartLifetimeMs = 300u * 1000u;
const size_t   kPartCeiling    = 2048;

// Two bytes of text each, and long enough that nothing guesses one.
const size_t kTokenBytes = 16;

const size_t   kRelayCeilingDefault  = 6;
const unsigned kHeaderWaitDefaultMs  = 2000;

/* Built on first use rather than at namespace scope, so nothing here needs a
   constructor run before main: the linker drops an object out of a convenience archive
   when nothing on the line asked for a symbol it defines, and a unit whose only effect
   is a constructor defines none.

   What is done under it is appending to a string and walking a list as long as the
   ceiling on pass throughs. Two things besides: the suspend inside the reader, which
   the library states may be called from there and which has to happen while the
   emptiness that led to it still holds, and the wait inside the writer, which gives
   the lock up while it waits. */
OpenThreads::Mutex &lock()
{
	static OpenThreads::Mutex m;
	return m;
}

struct Part
{
	std::string   token;
	std::string   url;
	coreapi::ChannelId id;
	hls::UriKind  kind;
	int64_t       minted;

	Part() : id(0), kind(hls::UriKind::Segment), minted(0) {}
};

std::vector<Part> &parts()
{
	static std::vector<Part> *p = new std::vector<Part>;
	return *p;
}

const std::vector<NetPrefix> *refused_override_ = NULL;
size_t   relay_ceiling_   = kRelayCeilingDefault;
unsigned header_wait_ms_  = kHeaderWaitDefaultMs;

std::vector<NetPrefix> *buildRefused()
{
	std::vector<NetPrefix> *made = new std::vector<NetPrefix>;
	const size_t count = sizeof(kRefused) / sizeof(kRefused[0]);
	made->reserve(count);
	for (size_t i = 0; i < count; ++i)
	{
		NetPrefix p;
		if (!parsePrefix(kRefused[i], &p))
		{
			made->clear();
			break;
		}
		made->push_back(p);
	}
	return made;
}

int64_t monotonicMs()
{
	struct timespec t;
	if (clock_gettime(CLOCK_MONOTONIC, &t) != 0)
		return 0;
	return (int64_t) t.tv_sec * 1000 + t.tv_nsec / 1000000;
}

/* The library's own one time set up, done here and not where the server starts
   because this is the only part of this program that reaches out over the
   network from a thread of its own. Doing it lazily inside curl_easy_init is
   what the library says not to do when more than one thread may be the first. */
pthread_once_t curl_once_ = PTHREAD_ONCE_INIT;

void initCurl()
{
	curl_global_init(CURL_GLOBAL_DEFAULT);
}

/* The address a connection is about to be made to, as text.

   Written from the resolved address and never from the name, which is the
   whole of the point: a name is whatever the far server's nameserver answers
   with, and the answer can be an address on this network. */
bool addressText(const struct curl_sockaddr *a, std::string &out)
{
	char buf[INET6_ADDRSTRLEN];
	std::memset(buf, 0, sizeof(buf));

	if (a->family == AF_INET)
	{
		const struct sockaddr_in *v4 = (const struct sockaddr_in *) &a->addr;
		if (inet_ntop(AF_INET, &v4->sin_addr, buf, sizeof(buf)) == NULL)
			return false;
	}
	else if (a->family == AF_INET6)
	{
		const struct sockaddr_in6 *v6 = (const struct sockaddr_in6 *) &a->addr;
		if (inet_ntop(AF_INET6, &v6->sin6_addr, buf, sizeof(buf)) == NULL)
			return false;
	}
	else
	{
		return false;
	}

	out = buf;
	return true;
}

/* Where every connection this file makes is decided, including the ones a redirection
   asks for, because the library asks this again for each of them. Reached from C and
   may not throw.

   Refusing here rather than by looking at the address written in the answer is what
   closes the two ways round such a check: a name that resolves to an address on this
   network, and a name that resolves to one address when it is checked and another when
   it is used. There is nothing between this answer and the connect. */
curl_socket_t openSocket(void *cls, curlsocktype purpose, struct curl_sockaddr *address)
{
	(void) purpose;

	bool *refused = (bool *) cls;
	std::string text;

	try
	{
		if (!addressText(address, text) || addressRefused(text))
		{
			if (refused != NULL)
				*refused = true;
			return CURL_SOCKET_BAD;
		}
	}
	catch (...)
	{
		if (refused != NULL)
			*refused = true;
		return CURL_SOCKET_BAD;
	}

	const int fd = ::socket(address->family, address->socktype, address->protocol);
	return (fd < 0) ? CURL_SOCKET_BAD : (curl_socket_t) fd;
}

/* Everything every fetch here is set up with, in one place so a rule written for one
   of them cannot be missing from the other.

   The protocol list is the second half of the refusal, beside the address check:
   without it a redirection could name a scheme this library knows how to open on the
   filesystem, and the address check would never be asked because no connection would
   be made.

   The certificate is not checked, which is what everything else in this tree that
   fetches over TLS does and for the same reason: the box carries no store of
   authorities, so checking would refuse every address rather than the wrong ones. It
   does not open the hole this file is about: whoever sat in the middle would still
   only be answered on an address that passed the check above, because that check is
   made on the address and not on the name or the certificate. What it costs is that
   the bytes of a channel are not proof against being changed on the way, which is what
   they already were on the box's own player. */
void configure(CURL *h, const std::string &url, bool *refused)
{
	curl_easy_setopt(h, CURLOPT_URL, url.c_str());
	curl_easy_setopt(h, CURLOPT_NOSIGNAL, 1L);
	curl_easy_setopt(h, CURLOPT_FOLLOWLOCATION, 1L);
	curl_easy_setopt(h, CURLOPT_MAXREDIRS, (long) kMaxRedirects);
	curl_easy_setopt(h, CURLOPT_CONNECTTIMEOUT, kConnectSeconds);
	curl_easy_setopt(h, CURLOPT_SSL_VERIFYPEER, 0L);
	curl_easy_setopt(h, CURLOPT_SSL_VERIFYHOST, 0L);
	curl_easy_setopt(h, CURLOPT_USERAGENT, "neutrino-ni-web");
	curl_easy_setopt(h, CURLOPT_OPENSOCKETFUNCTION, &openSocket);
	curl_easy_setopt(h, CURLOPT_OPENSOCKETDATA, refused);

#if defined(CURL_AT_LEAST_VERSION) && CURL_AT_LEAST_VERSION(7, 85, 0)
	curl_easy_setopt(h, CURLOPT_PROTOCOLS_STR, "http,https");
	curl_easy_setopt(h, CURLOPT_REDIR_PROTOCOLS_STR, "http,https");
#else
	curl_easy_setopt(h, CURLOPT_PROTOCOLS, (long) (CURLPROTO_HTTP | CURLPROTO_HTTPS));
	curl_easy_setopt(h, CURLOPT_REDIR_PROTOCOLS, (long) (CURLPROTO_HTTP | CURLPROTO_HTTPS));
#endif
}

// -------------------------------------------------------------- the document

struct Sink
{
	std::string *body;
	size_t       cap;
	bool         too_large;

	Sink() : body(NULL), cap(0), too_large(false) {}
};

size_t intoSink(char *ptr, size_t size, size_t nmemb, void *cls)
{
	Sink *s = (Sink *) cls;
	const size_t n = size * nmemb;
	if (s == NULL || s->body == NULL)
		return 0;

	try
	{
		if (s->body->size() + n > s->cap)
		{
			// Answering short is what tells the library to stop, and the flag
			// is what tells the difference between this and a far server that
			// hung up: both come back as one error code.
			s->too_large = true;
			return 0;
		}
		s->body->append(ptr, n);
	}
	catch (...)
	{
		return 0;
	}
	return n;
}

} // namespace

// --------------------------------------------------------------- the policy

const std::vector<NetPrefix> &refusedNetworks()
{
	if (refused_override_ != NULL)
		return *refused_override_;

	/* Read once and kept, because it is read for every connection this box makes and
	   the reading is a parse of twenty one lines. Built on first use and never
	   destroyed, so nothing has to run before main; the compiler's own guard is what
	   makes the first use safe when two fetching threads reach it together.

	   Empty rather than short if any of them fails to read: a list missing one of its
	   entries lets a whole network through while looking like a list somebody
	   configured on purpose, and an empty one is refused by the caller below. */
	static const std::vector<NetPrefix> *held = buildRefused();
	return *held;
}

bool addressRefused(const std::string &addr)
{
	const std::vector<NetPrefix> &list = refusedNetworks();

	/* A list that could not be built refuses everything rather than nothing.
	   The one way this file can be wrong that matters is being wrong towards
	   letting a connection through, and a box that cannot read its own refusal
	   list is a box that has to stop rather than open up. A case may install
	   an empty list on purpose, which is what the override is for and is why
	   the two are told apart. */
	if (list.empty())
		return refused_override_ == NULL;

	// An address this cannot read is refused. Nothing here has a rule about a
	// form it does not understand, and no rule means no.
	NetPrefix probe;
	std::memset(&probe, 0, sizeof(probe));
	probe.family = AF_INET;
	probe.len = 0;
	const bool readable_v4 = addressInPrefix(addr, probe);
	probe.family = AF_INET6;
	const bool readable_v6 = addressInPrefix(addr, probe);
	if (!readable_v4 && !readable_v6)
		return true;

	return addressInAnyPrefix(addr, list);
}

bool urlAcceptable(const std::string &url)
{
	if (url.empty() || url.size() > 4096)
		return false;
	if (url.find('\0') != std::string::npos)
		return false;
	// A control character in an address is a header written into one, and curl
	// is not the layer that should be deciding what to do about it.
	for (size_t i = 0; i < url.size(); ++i)
	{
		const unsigned char c = (unsigned char) url[i];
		if (c < 0x20 || c == 0x7F)
			return false;
	}
	return strncasecmp(url.c_str(), "http://", 7) == 0 ||
	       strncasecmp(url.c_str(), "https://", 8) == 0;
}

void setRefusedNetworksForTest(const std::vector<NetPrefix> *list)
{
	refused_override_ = list;
}

// -------------------------------------------------------------- the document

Reached fetchBounded(const std::string &url, size_t cap, Document &out)
{
	out = Document();
	if (!urlAcceptable(url))
		return Reached::Refused;

	pthread_once(&curl_once_, &initCurl);

	CURL *h = curl_easy_init();
	if (h == NULL)
		return Reached::Unreachable;

	bool refused = false;
	Sink sink;
	sink.body = &out.body;
	sink.cap = cap;

	configure(h, url, &refused);
	curl_easy_setopt(h, CURLOPT_TIMEOUT, kDocumentSeconds);
	// A document is small and every one of these servers offers it packed.
	curl_easy_setopt(h, CURLOPT_ACCEPT_ENCODING, "");
	curl_easy_setopt(h, CURLOPT_WRITEFUNCTION, &intoSink);
	curl_easy_setopt(h, CURLOPT_WRITEDATA, &sink);

	const CURLcode rc = curl_easy_perform(h);

	long code = 0;
	curl_easy_getinfo(h, CURLINFO_RESPONSE_CODE, &code);
	out.code = code;

	const char *effective = NULL;
	curl_easy_getinfo(h, CURLINFO_EFFECTIVE_URL, &effective);
	out.url = (effective != NULL) ? effective : url;

	const char *type = NULL;
	curl_easy_getinfo(h, CURLINFO_CONTENT_TYPE, &type);
	out.content_type = (type != NULL) ? type : "";

	curl_easy_cleanup(h);

	if (refused)
		return Reached::Refused;
	if (sink.too_large)
		return Reached::TooLarge;
	if (rc != CURLE_OK)
		return Reached::Unreachable;
	return Reached::Ok;
}

// ----------------------------------------------------------------- the parts

/* ONE ADDRESS PER SEGMENT AND NOT ONE PER READING OF THE PLAYLIST.

   A live playlist is read again every few seconds, and the segments in the middle of
   it are the same segments they were the time before. A player merges the new reading
   into the old one by matching the address a media sequence number carries, so an
   address that is fresh every time makes every segment look like a different segment:
   hls.js answers "media sequence mismatch" and gives up, measured against Pluto TV. It
   also grew the table by one entry per segment per reading, so a channel left playing
   walked into the ceiling within the hour.

   So the table is asked first. What is handed back for a segment already in it is the
   address that segment already has, moved to the end so it ages from the last time it
   was named. Nothing is revealed by that: the two addresses stood for the same bytes
   anyway. */
std::string mintPart(coreapi::ChannelId id, const std::string &url, hls::UriKind kind)
{
	if (!urlAcceptable(url))
		return std::string();

	const int64_t now = monotonicMs();

	OpenThreads::ScopedLock<OpenThreads::Mutex> held(lock());
	std::vector<Part> &list = parts();

	/* The old ones go before the new one is added, so that a box left playing
	   for a week holds what a player is using and nothing else. Oldest first,
	   which is the order they were added in, so the walk stops at the first
	   that is still good. */
	size_t stale = 0;
	while (stale < list.size() && (now - list[stale].minted) > (int64_t) kPartLifetimeMs)
		++stale;
	if (stale > 0)
		list.erase(list.begin(), list.begin() + (std::ptrdiff_t) stale);

	for (size_t i = 0; i < list.size(); ++i)
	{
		if (list[i].id != id || list[i].kind != kind || list[i].url != url)
			continue;
		/* Taken out and put back rather than touched where it stands: the
		   pruning above reads the list as oldest first and stops at the first
		   entry that is still good, which a timestamp raised in place would
		   make untrue. */
		Part again = list[i];
		again.minted = now;
		list.erase(list.begin() + (std::ptrdiff_t) i);
		list.push_back(again);
		return again.token;
	}

	unsigned char raw[kTokenBytes];
	if (!randomBytes(raw, sizeof(raw)))
		return std::string();

	char text[kTokenBytes * 2 + 1];
	for (size_t i = 0; i < sizeof(raw); ++i)
		std::snprintf(text + i * 2, 3, "%02x", raw[i]);

	Part p;
	p.token = text;
	p.url = url;
	p.id = id;
	p.kind = kind;
	p.minted = now;

	// And past the ceiling the oldest goes whether it has aged out or not,
	// because a player reading a very long playlist is still a player whose
	// addresses have to fit somewhere.
	while (list.size() >= kPartCeiling)
		list.erase(list.begin());

	list.push_back(p);
	return p.token;
}

bool findPart(const std::string &token, coreapi::ChannelId &id, std::string &url,
              hls::UriKind &kind)
{
	if (token.empty())
		return false;

	OpenThreads::ScopedLock<OpenThreads::Mutex> held(lock());
	const std::vector<Part> &list = parts();
	const int64_t now = monotonicMs();

	for (size_t i = 0; i < list.size(); ++i)
	{
		if (list[i].token != token)
			continue;
		// An address that has aged out is one nothing has, rather than one
		// that is fetched because it is still written down.
		if ((now - list[i].minted) > (int64_t) kPartLifetimeMs)
			return false;
		id = list[i].id;
		url = list[i].url;
		kind = list[i].kind;
		return true;
	}
	return false;
}

size_t partsHeld()
{
	OpenThreads::ScopedLock<OpenThreads::Mutex> held(lock());
	return parts().size();
}

void forgetParts()
{
	OpenThreads::ScopedLock<OpenThreads::Mutex> held(lock());
	parts().clear();
}

unsigned partLifetimeMs()
{
	return kPartLifetimeMs;
}

size_t partCeiling()
{
	return kPartCeiling;
}

// ----------------------------------------------------------- the pass through

bool isRelay(const Response &r)
{
	return r.code == StatusOk && !r.relay_url.empty();
}

Response relayOf(const std::string &url, const char *fallback_type)
{
	Response out;
	out.code = StatusOk;
	out.relay_url = url;
	out.content_type = (fallback_type != NULL) ? fallback_type : "application/octet-stream";
	/* Both of these are about what sits between this box and the browser. A
	   cache that stored a live segment would replay it, and a proxy that
	   buffered a stream would hold it until it ended, which it does not. */
	out.headers.push_back(std::make_pair(std::string("Cache-Control"),
	                                     std::string("no-store, no-transform")));
	out.headers.push_back(std::make_pair(std::string("X-Accel-Buffering"),
	                                     std::string("no")));
	return out;
}

size_t relayCeiling()
{
	return relay_ceiling_;
}

void setRelayCeilingForTest(size_t n)
{
	relay_ceiling_ = n;
}

unsigned headerWaitMs()
{
	return header_wait_ms_;
}

void setHeaderWaitMsForTest(unsigned ms)
{
	header_wait_ms_ = ms;
}

} // namespace webtv

} // namespace httpd
