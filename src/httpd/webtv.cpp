/*
 * webtv.cpp - fetching a channel's stream off a far server and passing it on
 *
 * Copyright (C) 2026 NI-Team
 * Copyright (C) 2026 Tuxbox-Project (carried fetches)
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

#include <atomic>
#include <cerrno>
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
   address held longer than this is one that has stopped working anyway. The time
   runs from the last time an address was minted or asked for: a playlist a player
   reads again every few seconds, a variant named once by the channel's own one,
   stays for as long as it is read, and the table gives up the address used least
   recently first.

   One playlist mints at most kPlaylistParts addresses, and the table holds more
   than the fetch seats times that: a playlist is only read on a seat, so the
   playlists being read at any one time cannot push out an address one of them
   has just handed out. A playlist that would need more is refused rather than
   answered with addresses already forgotten. */
const unsigned kPartLifetimeMs = 300u * 1000u;
const size_t   kPlaylistParts  = 512;
const size_t   kPartCeiling    = 4096;

// Two bytes of text each, and long enough that nothing guesses one.
const size_t kTokenBytes = 16;

/* How often a fetch made for a browser looks whether that browser is still
   there. Each look is a poll of its connection; a fetch is given up within
   this, and the library's own pace of asking, after its browser has left,
   which is what frees its seat for the next. */
const int64_t kPeerLookMs = 250;

/* Built on first use rather than at namespace scope, so nothing here needs a
   constructor run before main: the linker drops an object out of a convenience archive
   when nothing on the line asked for a symbol it defines, and a unit whose only effect
   is a constructor defines none.

   What is done under it is walking and changing the list of minted parts, and nothing
   that waits. */
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
std::atomic<unsigned> open_delay_ms_(0);
std::atomic<unsigned> part_lifetime_ms_(kPartLifetimeMs);

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

void napMs(unsigned ms)
{
	struct timespec t;
	t.tv_sec = (time_t) (ms / 1000u);
	t.tv_nsec = (long) (ms % 1000u) * 1000L * 1000L;
	while (nanosleep(&t, &t) != 0 && errno == EINTR)
	{
	}
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

	const unsigned delay = open_delay_ms_.load();
	if (delay != 0)
		napMs(delay);

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

// ------------------------------------------------------------- the watch

/* Whether a fetch made for a carried answer goes on: not once a stop has
   cancelled its job, and not once the browser it is for has gone. */
struct Watch
{
	carry::Job *job;
	int64_t     looked;
	bool        given_up;

	explicit Watch(carry::Job *j) : job(j), looked(0), given_up(false) {}
};

/* The library's progress callback, asked at least once a second while a fetch
   waits and with every block that arrives; answering anything but zero ends the
   transfer. The browser's connection is looked at no more often than
   kPeerLookMs. Reached from C on the job's thread and may not throw; nothing in
   it allocates. */
int keepGoing(void *cls, curl_off_t dt, curl_off_t dn, curl_off_t ut, curl_off_t un)
{
	(void) dt;
	(void) dn;
	(void) ut;
	(void) un;

	Watch *w = (Watch *) cls;
	if (w == NULL || w->job == NULL)
		return 0;
	if (w->given_up)
		return 1;
	if (carry::cancelled(*w->job))
	{
		w->given_up = true;
		return 1;
	}
	const int64_t now = monotonicMs();
	if (now - w->looked >= kPeerLookMs)
	{
		w->looked = now;
		if (!carry::waitPeer(*w->job, 0))
		{
			w->given_up = true;
			return 1;
		}
	}
	return 0;
}

// A handle and the header list it was given, cleaned up on every way out.
struct Handle
{
	CURL              *h;
	struct curl_slist *ask;

	Handle() : h(curl_easy_init()), ask(NULL) {}
	~Handle()
	{
		if (h != NULL)
			curl_easy_cleanup(h);
		if (ask != NULL)
			curl_slist_free_all(ask);
	}

private:
	Handle(const Handle &);
	Handle &operator=(const Handle &);
};

// ------------------------------------------------------- the pass through

/* One pass through, on its job's thread and nowhere else, so nothing here
   needs a lock: the callbacks below run inside curl_easy_perform on that
   thread. */
struct Passing
{
	carry::Job *job;
	CURL       *handle;
	Watch       watch;

	// What the head that counts said: the last one, and never a redirection's.
	bool        head_seen;
	long        code;
	std::string content_type;
	bool        has_length;
	uint64_t    length;
	/* A body in an encoding. The length the far server states is then the
	   length of the encoded bytes, and the browser is told no encoding, so the
	   length is not passed on. */
	bool        encoded;

	// The header went out; from here every failure only ends the connection.
	bool        sent;
	// The far server answered outside 200 to 299, and the transfer was ended.
	bool        turned_away;

	// The kind's ceiling (byteCeilingOf), 0 for none, and what went out so far.
	uint64_t    ceiling;
	uint64_t    passed;
	// More than the ceiling was announced or sent, and the transfer was ended.
	bool        too_large;

	explicit Passing(carry::Job &j)
		: job(&j), handle(NULL), watch(&j), head_seen(false), code(0),
		  has_length(false), length(0), encoded(false), sent(false), turned_away(false),
		  ceiling(0), passed(0), too_large(false)
	{
	}
};

// The value of a header line when the line is that header, the name compared
// without case and the value without the space around it.
bool headerValue(const std::string &line, const char *name, std::string &value)
{
	const size_t n = std::strlen(name);
	if (line.size() <= n || line[n] != ':' || strncasecmp(line.c_str(), name, n) != 0)
		return false;
	size_t from = n + 1;
	size_t to = line.size();
	while (from < to && (line[from] == ' ' || line[from] == '\t'))
		++from;
	while (to > from && (line[to - 1] == '\r' || line[to - 1] == '\n' ||
	                     line[to - 1] == ' ' || line[to - 1] == '\t'))
		--to;
	value = line.substr(from, to - from);
	return true;
}

// A length as the far server wrote it: digits and nothing else.
bool lengthOf(const std::string &value, uint64_t &out)
{
	if (value.empty() || value.size() > 19)
		return false;
	uint64_t got = 0;
	for (size_t i = 0; i < value.size(); ++i)
	{
		if (value[i] < '0' || value[i] > '9')
			return false;
		got = got * 10u + (uint64_t) (value[i] - '0');
	}
	out = got;
	return true;
}

// The far server's head, a line at a time. Reached from C and may not throw.
size_t headLine(char *ptr, size_t size, size_t nmemb, void *cls)
{
	Passing *p = (Passing *) cls;
	const size_t n = size * nmemb;
	if (p == NULL)
		return 0;

	try
	{
		const std::string line(ptr, n);
		std::string value;
		if (line.compare(0, 5, "HTTP/") == 0)
		{
			// A redirection brings a head of its own after this one, and the
			// last of them is the answer's.
			p->head_seen = false;
			p->content_type.clear();
			p->has_length = false;
			p->length = 0;
			p->encoded = false;
		}
		else if (headerValue(line, "Content-Type", value))
		{
			p->content_type = value;
		}
		else if (headerValue(line, "Content-Length", value))
		{
			p->has_length = lengthOf(value, p->length);
		}
		else if (headerValue(line, "Content-Encoding", value))
		{
			p->encoded = !value.empty() && strcasecmp(value.c_str(), "identity") != 0;
		}
		else if (line == "\r\n" || line == "\n")
		{
			long code = 0;
			curl_easy_getinfo(p->handle, CURLINFO_RESPONSE_CODE, &code);
			// A head that only says where to look instead, or that the answer
			// is still to come, is not the answer.
			if (code >= 200 && (code < 300 || code >= 400))
			{
				p->code = code;
				p->head_seen = true;
			}
		}
	}
	catch (...)
	{
		return 0;
	}
	return n;
}

/* The far server's body, a block at a time: the header goes out with the first
   block, and every block is written before the next is taken, so a browser
   that reads slowly slows the fetch down rather than filling the box's memory.
   Answering short ends the transfer. Reached from C and may not throw. */
size_t bodyBlock(char *ptr, size_t size, size_t nmemb, void *cls)
{
	Passing *p = (Passing *) cls;
	const size_t n = size * nmemb;
	if (p == NULL)
		return 0;

	try
	{
		if (!p->sent)
		{
			if (!p->head_seen || p->code < 200 || p->code >= 300)
			{
				// what comes with a refusal is not the channel
				p->turned_away = true;
				return 0;
			}
			// a length past the ceiling is refused before anything goes out
			if (p->ceiling != 0 && p->has_length && p->length > p->ceiling)
			{
				p->too_large = true;
				return 0;
			}
			Response &a = carry::answerOf(*p->job);
			a.content_type = safeType((RelayKind) a.relay_kind, p->content_type);
			p->sent = true;
			if (!carry::sendHead(*p->job, (p->has_length && !p->encoded) ? (int64_t) p->length : -1))
				return 0;
		}
		// a body that goes on past the ceiling is cut off where it passes it
		if (p->ceiling != 0 && p->passed + n > p->ceiling)
		{
			p->too_large = true;
			return 0;
		}
		p->passed += n;
		if (n > 0 && !carry::sendBody(*p->job, ptr, n))
			return 0;
	}
	catch (...)
	{
		return 0;
	}
	return n;
}

Response upstreamProblem(const char *why)
{
	Response r = problemResponse(StatusServiceUnavailable, coreapi::ErrorCode::UpstreamUnreachable, why);
	sandbox(r);
	return r;
}

/* The types each kind may go out with, and the one it goes out with otherwise.
   A segment is a piece of a transport stream or of a fragmented mp4, alone or
   sound only; a map is the head of the latter; a stream is what radio and
   television stations send without a playlist. */
const char *const kSegmentTypes[] = {
	"video/mp2t", "video/mp4", "audio/mp4", "audio/aac", "audio/mpeg",
	"video/iso.segment", "application/octet-stream", "binary/octet-stream", NULL
};
const char *const kMapTypes[] = {
	"video/mp4", "audio/mp4", "application/octet-stream", NULL
};
const char *const kStreamTypes[] = {
	"audio/mpeg", "audio/aac", "audio/aacp", "audio/x-aac", "audio/ogg", "audio/opus",
	"video/mp2t", "video/mp4", "application/octet-stream", NULL
};

// The type alone, in lower case: the parameters cut off, the space around it gone.
std::string bareType(const std::string &named)
{
	std::string t = named.substr(0, named.find(';'));
	size_t from = 0;
	size_t to = t.size();
	while (from < to && (t[from] == ' ' || t[from] == '\t'))
		++from;
	while (to > from && (t[to - 1] == ' ' || t[to - 1] == '\t'))
		--to;
	t = t.substr(from, to - from);
	for (size_t i = 0; i < t.size(); ++i)
	{
		if (t[i] >= 'A' && t[i] <= 'Z')
			t[i] = (char) (t[i] - 'A' + 'a');
	}
	return t;
}

const char *oneOf(const char *const *list, const std::string &type, const char *otherwise)
{
	for (size_t i = 0; list[i] != NULL; ++i)
	{
		if (type == list[i])
			return list[i];
	}
	return otherwise;
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

Reached fetchBounded(const std::string &url, size_t cap, Document &out, carry::Job *job)
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

	Watch watch(job);
	if (job != NULL)
	{
		curl_easy_setopt(h, CURLOPT_NOPROGRESS, 0L);
		curl_easy_setopt(h, CURLOPT_XFERINFOFUNCTION, &keepGoing);
		curl_easy_setopt(h, CURLOPT_XFERINFODATA, &watch);
	}

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

	if (watch.given_up)
		return Reached::Cancelled;
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
	while (stale < list.size() && (now - list[stale].minted) > (int64_t) part_lifetime_ms_.load())
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

bool findPart(const std::string &token, coreapi::ChannelId id, std::string &url,
              hls::UriKind &kind)
{
	if (token.empty())
		return false;

	OpenThreads::ScopedLock<OpenThreads::Mutex> held(lock());
	std::vector<Part> &list = parts();
	const int64_t now = monotonicMs();

	for (size_t i = 0; i < list.size(); ++i)
	{
		if (list[i].token != token)
			continue;
		/* A token stands for an address of one channel. Under another channel it
		   names nothing, and asking for it so does not keep it either. */
		if (list[i].id != id)
			return false;
		// An address that has aged out is one nothing has, rather than one
		// that is fetched because it is still written down.
		if ((now - list[i].minted) > (int64_t) part_lifetime_ms_.load())
			return false;
		url = list[i].url;
		kind = list[i].kind;
		/* Asked for, it is in use: its time starts again, and it moves to the
		   end the way mintPart moves one, so the list stays oldest first. */
		Part again = list[i];
		again.minted = now;
		list.erase(list.begin() + (std::ptrdiff_t) i);
		list.push_back(again);
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
	return part_lifetime_ms_.load();
}

void setPartLifetimeMsForTest(unsigned ms)
{
	part_lifetime_ms_ = (ms == 0) ? kPartLifetimeMs : ms;
}

size_t partCeiling()
{
	return kPartCeiling;
}

size_t partsPerPlaylist()
{
	return kPlaylistParts;
}

// ----------------------------------------------------------- the pass through

Response relayOf(const std::string &url, RelayKind kind, const char *fallback_type)
{
	Response out;
	out.code = StatusOk;
	out.relay_url = url;
	out.relay_kind = (int) kind;
	const std::string fallback = (fallback_type != NULL) ? fallback_type : "";
	out.content_type = (kind == RelayPlaylist || kind == RelayNestedPlaylist)
		? fallback : safeType(kind, fallback);
	/* Both of these are about what sits between this box and the browser. A
	   cache that stored a live segment would replay it, and a proxy that
	   buffered a stream would hold it until it ended, which it does not. */
	out.headers.push_back(std::make_pair(std::string("Cache-Control"),
	                                     std::string("no-store, no-transform")));
	out.headers.push_back(std::make_pair(std::string("X-Accel-Buffering"),
	                                     std::string("no")));
	sandbox(out);
	return out;
}

std::string safeType(RelayKind kind, const std::string &named)
{
	const std::string type = bareType(named);
	switch (kind)
	{
		case RelaySegment:  return oneOf(kSegmentTypes, type, "video/mp2t");
		case RelayMap:      return oneOf(kMapTypes, type, "video/mp4");
		case RelayKey:      return "application/octet-stream";
		case RelayStream:   return oneOf(kStreamTypes, type, "application/octet-stream");
		case RelayPlaylist:
		case RelayNestedPlaylist:
			break;
	}
	return "application/octet-stream";
}

void sandbox(Response &r)
{
	for (size_t i = 0; i < r.headers.size(); ++i)
	{
		if (strcasecmp(r.headers[i].first.c_str(), "Content-Security-Policy") == 0)
		{
			r.headers[i].second = "sandbox; default-src 'none'";
			return;
		}
	}
	r.headers.push_back(std::make_pair(std::string("Content-Security-Policy"),
	                                   std::string("sandbox; default-src 'none'")));
}

void relay(carry::Job &job)
{
	const std::string url = carry::answerOf(job).relay_url;

	pthread_once(&curl_once_, &initCurl);

	Handle h;
	if (h.h == NULL)
	{
		carry::refuse(job, upstreamProblem("the server this channel is played from could not be reached"));
		return;
	}

	Passing p(job);
	p.handle = h.h;
	p.ceiling = byteCeilingOf((RelayKind) carry::answerOf(job).relay_kind);
	bool refused = false;

	configure(h.h, url, &refused);
	/* Bytes as the far server holds them. They are passed on and not read, and
	   a browser told no encoding would take packed bytes for the media. */
	h.ask = curl_slist_append(NULL, "Accept-Encoding: identity");
	if (h.ask != NULL)
		curl_easy_setopt(h.h, CURLOPT_HTTPHEADER, h.ask);
	curl_easy_setopt(h.h, CURLOPT_LOW_SPEED_LIMIT, 1L);
	curl_easy_setopt(h.h, CURLOPT_LOW_SPEED_TIME, kStallSeconds);
	curl_easy_setopt(h.h, CURLOPT_HEADERFUNCTION, &headLine);
	curl_easy_setopt(h.h, CURLOPT_HEADERDATA, &p);
	curl_easy_setopt(h.h, CURLOPT_WRITEFUNCTION, &bodyBlock);
	curl_easy_setopt(h.h, CURLOPT_WRITEDATA, &p);
	curl_easy_setopt(h.h, CURLOPT_NOPROGRESS, 0L);
	curl_easy_setopt(h.h, CURLOPT_XFERINFOFUNCTION, &keepGoing);
	curl_easy_setopt(h.h, CURLOPT_XFERINFODATA, &p.watch);

	const CURLcode rc = curl_easy_perform(h.h);

	/* After the header only the close says the rest; a fetch given up has
	   nobody left to tell. */
	if (p.sent || p.watch.given_up || carry::cancelled(job))
		return;

	/* A refused address ends the fetch before a socket exists. It is the one
	   of these a caller cannot do anything about by asking again. */
	if (refused)
	{
		Response no = problemResponse(StatusForbidden, coreapi::ErrorCode::AddressRefused,
		                              "this channel leads to an address on a network this box does not fetch from");
		sandbox(no);
		carry::refuse(job, no);
		return;
	}

	if (p.too_large)
	{
		carry::refuse(job, upstreamProblem("the server this channel is played from sends a part larger than this box passes on"));
		return;
	}

	/* A far server that answered a refusal is answered on with one, rather
	   than with an empty body under a code that says all is well: a player
	   handed nothing under a 200 reports a broken stream and says nothing
	   about which of the two ends broke it. */
	if (p.turned_away || (p.head_seen && (p.code < 200 || p.code >= 300)))
	{
		char why[128];
		std::snprintf(why, sizeof(why),
		              "the server this channel is played from answered %ld", p.code);
		carry::refuse(job, upstreamProblem(why));
		return;
	}

	// An answer with nothing in it, which is still an answer.
	if (rc == CURLE_OK && p.head_seen)
	{
		carry::sendHead(job, (p.has_length && !p.encoded) ? (int64_t) p.length : 0);
		return;
	}

	carry::refuse(job, upstreamProblem("the server this channel is played from could not be reached"));
}

uint64_t byteCeilingOf(RelayKind kind)
{
	switch (kind)
	{
		case RelaySegment: return 64u * 1024u * 1024u;
		case RelayMap:     return 4u * 1024u * 1024u;
		case RelayKey:     return 4u * 1024u;
		case RelayStream:
		case RelayPlaylist:
		case RelayNestedPlaylist:
			break;
	}
	return 0;
}

void setOpenDelayMsForTest(unsigned ms)
{
	open_delay_ms_ = ms;
}

} // namespace webtv

} // namespace httpd
