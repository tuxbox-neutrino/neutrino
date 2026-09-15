/*
 * test_sse.cpp - tests for the event stream
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

#include "httpd/webconfig.h"
#include "httpd/endpoint.h"
#include "httpd/events.h"
#include "httpd/http.h"
#include "httpd/router.h"
#include "httpd/server.h"

#include "coreapi/base/eventbus.h"

#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <set>
#include <string>
#include <vector>

#include <pthread.h>

#include <stdint.h>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <poll.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

using namespace httpd;

namespace
{

// What the header beside the code says these are, so that a case restoring
// them puts back what ships and a case reading them fails when the shipped
// value moves without anybody saying so.
const unsigned kShippedHeartbeatMs = 20000;
const size_t   kShippedCeiling     = 256u * 1024u;

// Short enough that a case does not wait out the shipped interval, and long
// enough that a beat is not what every read comes back with.
const unsigned kFastHeartbeatMs = 100;

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

/* Waits for the number of open streams to reach what is wanted and answers
   what it reached, so that a case asserts the number rather than how long the
   library took to get there. A number read once would be read before the
   daemon's own thread had finished with the connection. */
size_t waitForStreams(size_t want, int budget_ms)
{
	for (int waited = 0; waited < budget_ms; waited += 10)
	{
		if (events::openStreams() == want)
			return want;
		napMs(10);
	}
	return events::openStreams();
}

ServerConfig loopback()
{
	ServerConfig c = defaultConfig();
	c.port = 0;                       // let the kernel choose
	c.bind_address = "127.0.0.1";
	return c;
}

/* A server carrying streams, and every case leaves it stopped and the shipped values
   back in place whichever line it left through. A failed check unwinds past everything
   after it, and a case that left a daemon bound would take the port, the ceiling and
   the beat with it for the rest of the run.

   The ceiling on open streams is installed as configuration and not through a seam of
   its own, because the file this server reads already carries that key. */
struct Streaming
{
	// Declared first, so the daemon starts after the seams are filled and stops
	// before they are cleared.
	InstalledDependencies wired_;

	explicit Streaming(unsigned max_streams = 4, unsigned beat_ms = kFastHeartbeatMs)
	{
		WebConfig c = defaultWebConfig();
		c.server = loopback();
		c.sse_max_streams = max_streams;
		setConfigForTest(c);

		REQUIRE(start(c.server));
		port_ = boundPort();
		events::setHeartbeatMs(beat_ms);
	}

	~Streaming()
	{
		stop();
		setRoutesForTest(NULL);
		setConfigForTest(defaultWebConfig());
		events::setHeartbeatMs(kShippedHeartbeatMs);
		events::setPendingCeiling(kShippedCeiling);
	}

	int port() const { return port_; }

private:
	Streaming(const Streaming &);
	Streaming &operator=(const Streaming &);

	int port_;
};

/* One event stream, read off the socket rather than through the client beside this file.

   That client is not used for the stream itself. It reads one whole reply and answers it,
   which a stream never becomes, and it does not decode the framing the library wraps a
   reply of unknown length in. Handed a stream it would wait out its deadline and then
   report a reply that never finished, and made to answer early it would answer with the
   framing bytes in the middle of the content.

   Teaching it the framing was the other way, and it is the wrong one here: its contract is
   a whole reply and every other wire case in the suite rests on that, while what a stream
   needs is bytes as they arrive, a deadline per read, and a reader that can be told to
   stop reading and leave the socket open.

   The 503 in this file does go through the shared client: that answer is a document with a
   length like any other refusal. */
class StreamReader
{
	public:
		explicit StreamReader(int fd)
			: fd_(fd), need_(0), consumed_(0), chunked_(false), reading_(true),
			  ok_(false), code_(0)
		{
			if (fd_ >= 0)
				ok_ = readHead();
		}

		~StreamReader()
		{
			if (fd_ >= 0)
				::close(fd_);
		}

		bool ok() const { return ok_; }
		int  code() const { return code_; }

		std::string headerOf(const std::string &name) const
		{
			const std::string want = lowered(name);
			for (size_t i = 0; i < headers_.size(); ++i)
			{
				if (lowered(headers_[i].first) == want)
					return headers_[i].second;
			}
			return std::string();
		}

		// The next frame nobody has taken yet, terminator and all, and empty
		// when none arrived inside the budget.
		std::string readFrame(int budget_ms)
		{
			const int64_t deadline = nowMs() + budget_ms;
			for (;;)
			{
				const size_t end = decoded_.find("\n\n", consumed_);
				if (end != std::string::npos)
				{
					const std::string frame = decoded_.substr(consumed_, end + 2 - consumed_);
					consumed_ = end + 2;
					return frame;
				}
				if (!pull(deadline))
					return std::string();
			}
		}

		/* Reads until every one of these has been seen or the budget runs out,
		   and answers everything that arrived. Written this way rather than as
		   a fixed wait so that a case which got what it was waiting for does
		   not go on paying for the wait. */
		std::string readUntilAll(const std::vector<std::string> &wanted, int budget_ms)
		{
			const int64_t deadline = nowMs() + budget_ms;
			const size_t from = consumed_;
			for (;;)
			{
				bool all = true;
				for (size_t i = 0; i < wanted.size() && all; ++i)
					all = decoded_.find(wanted[i], from) != std::string::npos;
				if (all)
					break;
				if (!pull(deadline))
					break;
			}
			consumed_ = decoded_.size();
			return decoded_.substr(from);
		}

		// Stops taking anything off the socket while leaving it open, which is
		// what a peer that has gone quiet without going away looks like.
		void stopReading() { reading_ = false; }

	private:
		StreamReader(const StreamReader &);
		StreamReader &operator=(const StreamReader &);

		static std::string lowered(const std::string &s)
		{
			std::string out(s);
			for (size_t i = 0; i < out.size(); ++i)
			{
				if (out[i] >= 'A' && out[i] <= 'Z')
					out[i] = (char) (out[i] - 'A' + 'a');
			}
			return out;
		}

		static std::string trimmed(const std::string &s)
		{
			size_t b = 0;
			size_t e = s.size();
			while (b < e && (s[b] == ' ' || s[b] == '\t'))
				++b;
			while (e > b && (s[e - 1] == ' ' || s[e - 1] == '\t' || s[e - 1] == '\r'))
				--e;
			return s.substr(b, e - b);
		}

		// Answers false when nothing more is going to arrive, which is the
		// deadline, the peer closing, or a reader that was told to stop.
		bool pull(int64_t deadline)
		{
			if (!reading_)
				return false;

			const int64_t left = deadline - nowMs();
			if (left <= 0)
				return false;

			struct pollfd p;
			std::memset(&p, 0, sizeof(p));
			p.fd = fd_;
			p.events = POLLIN;
			const int ready = ::poll(&p, 1, (int) left);
			if (ready <= 0)
				return false;

			char buf[4096];
			const ssize_t n = ::recv(fd_, buf, sizeof(buf), 0);
			if (n <= 0)
				return false;
			raw_.append(buf, (size_t) n);
			decode();
			return true;
		}

		/* The framing off the wire. A reply of unknown length is sent as a
		   length in hexadecimal, the bytes, and two line ends, over and over,
		   and a case reading those bytes as content would be asserting against
		   the framing. */
		void decode()
		{
			if (!chunked_)
			{
				decoded_ += raw_;
				raw_.clear();
				return;
			}

			for (;;)
			{
				if (need_ == 0)
				{
					const size_t eol = raw_.find("\r\n");
					if (eol == std::string::npos)
						return;
					std::string line = raw_.substr(0, eol);
					const size_t semi = line.find(';');
					if (semi != std::string::npos)
						line = line.substr(0, semi);
					const unsigned long n = std::strtoul(line.c_str(), NULL, 16);
					raw_.erase(0, eol + 2);
					if (n == 0)
						return;              // the end of the body
					need_ = (size_t) n;
				}
				// The whole piece and the two line ends after it, so nothing
				// half a piece long reaches a case.
				if (raw_.size() < need_ + 2)
					return;
				decoded_.append(raw_, 0, need_);
				raw_.erase(0, need_ + 2);
				need_ = 0;
			}
		}

		bool readHead()
		{
			const int64_t deadline = nowMs() + 5000;
			std::string head;
			for (;;)
			{
				const size_t end = raw_.find("\r\n\r\n");
				if (end != std::string::npos)
				{
					head = raw_.substr(0, end);
					raw_.erase(0, end + 4);
					break;
				}
				const int64_t left = deadline - nowMs();
				if (left <= 0)
					return false;

				struct pollfd p;
				std::memset(&p, 0, sizeof(p));
				p.fd = fd_;
				p.events = POLLIN;
				if (::poll(&p, 1, (int) left) <= 0)
					return false;
				char buf[4096];
				const ssize_t n = ::recv(fd_, buf, sizeof(buf), 0);
				if (n <= 0)
					return false;
				raw_.append(buf, (size_t) n);
			}

			size_t pos = 0;
			bool first = true;
			while (pos < head.size())
			{
				size_t eol = head.find('\n', pos);
				if (eol == std::string::npos)
					eol = head.size();
				const std::string line = trimmed(head.substr(pos, eol - pos));
				pos = eol + 1;

				if (first)
				{
					first = false;
					const size_t sp = line.find(' ');
					if (line.compare(0, 5, "HTTP/") != 0 || sp == std::string::npos)
						return false;
					code_ = (int) std::strtol(line.c_str() + sp + 1, NULL, 10);
					continue;
				}
				const size_t colon = line.find(':');
				if (colon != std::string::npos)
					headers_.push_back(std::make_pair(trimmed(line.substr(0, colon)),
					                                  trimmed(line.substr(colon + 1))));
			}

			chunked_ = lowered(headerOf("Transfer-Encoding")) == "chunked";
			decode();
			return true;
		}

		int         fd_;
		std::string raw_;        // off the wire, framing and all
		std::string decoded_;    // what the framing carried
		size_t      need_;       // bytes still owed on the piece being read
		size_t      consumed_;   // how much of decoded_ a case has taken
		bool        chunked_;
		bool        reading_;
		bool        ok_;
		int         code_;
		std::vector<std::pair<std::string, std::string> > headers_;
};

/* rcvbuf, when it is not zero, is asked for before the connect and not after,
   which is the only moment it changes what this end tells the other end it can
   hold. A small one is how a case gets a peer that is wedged rather than one
   the two kernels between them can buffer several megabytes for. */
int connectLoopback(int port, int rcvbuf)
{
	struct sockaddr_in a;
	std::memset(&a, 0, sizeof(a));
	a.sin_family = AF_INET;
	a.sin_port = htons((uint16_t) port);
	if (::inet_pton(AF_INET, "127.0.0.1", &a.sin_addr) != 1)
		return -1;

	const int fd = ::socket(AF_INET, SOCK_STREAM, 0);
	if (fd < 0)
		return -1;

	if (rcvbuf > 0)
		::setsockopt(fd, SOL_SOCKET, SO_RCVBUF, &rcvbuf, sizeof(rcvbuf));

	struct timeval tv;
	std::memset(&tv, 0, sizeof(tv));
	tv.tv_sec = 5;
	::setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
	::setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));

	if (::connect(fd, (const struct sockaddr *) &a, sizeof(a)) != 0)
	{
		::close(fd);
		return -1;
	}
	return fd;
}

/* Opens the stream and answers once its head has been read. Reading the head is what
   makes every case below deterministic rather than a race: the head is queued after the
   stream is registered, so a case holding the head knows the subscription is in place
   and that whatever it publishes next cannot be lost. */
int openStream(int port, int rcvbuf = 0)
{
	const int fd = connectLoopback(port, rcvbuf);
	if (fd < 0)
		return -1;

	char req[160];
	std::snprintf(req, sizeof(req),
	              "GET /api/v1/events HTTP/1.1\r\nHost: 127.0.0.1:%d\r\n\r\n", port);
	const size_t len = std::strlen(req);
	size_t sent = 0;
	while (sent < len)
	{
		const ssize_t n = ::send(fd, req + sent, len - sent, MSG_NOSIGNAL);
		if (n <= 0)
		{
			::close(fd);
			return -1;
		}
		sent += (size_t) n;
	}
	return fd;
}

/* Whether this server's event lock is held at the moment the reaper names a descriptor.

   Asked from a thread of its own because the thread holding the lock cannot ask for it,
   and asked by taking it: the call below reads the number of open streams, which every
   other caller reaches through that same lock, so a call that has not come back is a lock
   that is held. This is the only observation from outside that tells the safe arrangement
   from the unsafe one.

   Two flags and not one, because there are two ways for the asker not to have answered and
   only one of them is the answer: it may have been made to wait, which is the property, or
   it may never have reached the lock at all. */
struct LockWatch
{
	// The reaper is at the point where it names a descriptor.
	std::atomic<bool> go;
	// The asker is about to reach for the lock, which is the statement before
	// the one that blocks.
	std::atomic<bool> asking;
	// The asker got the lock.
	std::atomic<bool> answered;
	// The reaper reached that point at all.
	std::atomic<bool> reached;
	// The asker was at the door to be waited for, so what follows means
	// something.
	std::atomic<bool> door;
	std::atomic<bool> held;
	pthread_t         asker;
	bool              started;

	LockWatch()
		: go(false), asking(false), answered(false), reached(false), door(false),
		  held(false), started(false)
	{
	}

	void start()
	{
		started = pthread_create(&asker, NULL, &ask, this) == 0;
	}

	void join()
	{
		if (!started)
			return;
		go = true;                 // in case the reaper never reached the point
		pthread_join(asker, NULL);
		started = false;
	}

	// Runs on the reaping thread with the lock held, and answers nothing: what
	// it leaves behind is whether the asker got through while it waited.
	static void probe()
	{
		LockWatch *w = watched;
		if (w == NULL || w->reached.exchange(true))
			return;                // once, however many streams are reaped

		w->go = true;

		// Waited for and not assumed. An asker that never ran leaves the same
		// silence as one that is being kept out.
		for (int waited = 0; waited < 5000 && !w->asking; waited += 2)
			napMs(2);
		if (!w->asking)
			return;
		w->door = true;

		napMs(200);
		w->held = !w->answered;
	}

	static LockWatch *watched;

private:
	LockWatch(const LockWatch &);
	LockWatch &operator=(const LockWatch &);

	static void *ask(void *cls)
	{
		LockWatch *w = (LockWatch *) cls;
		while (!w->go)
			napMs(2);
		// Set immediately before the call that blocks, and by nothing else.
		w->asking = true;
		events::openStreams();
		w->answered = true;
		return NULL;
	}
};

LockWatch *LockWatch::watched = NULL;

void publish(coreapi::EventType t, uint64_t channel_id, int value)
{
	coreapi::Event e;
	e.type = t;
	e.channel_id = channel_id;
	e.value = value;
	coreapi::EventBus::instance().publish(e);
}

} // namespace

TEST_CASE("every event type has a name and no two share one", "[sse]")
{
	std::set<std::string> seen;
	for (int i = 0; i <= (int) coreapi::EventType::BouquetsChanged; ++i)
	{
		const char *n = events::typeName((coreapi::EventType) i);
		INFO("type value " << i);
		REQUIRE(n != NULL);
		REQUIRE(std::string(n).size() > 0);
		REQUIRE(seen.insert(n).second);
	}
	REQUIRE(seen.size() == 10);
}

TEST_CASE("the shipped beat and the shipped ceiling are what the header says", "[sse]")
{
	/* Both are settable, and both are set by the cases below to something a
	   run can afford to wait for. What ships is the pair a box runs on, and
	   nothing else in this file would notice it moving. */
	REQUIRE(events::heartbeatMs() == kShippedHeartbeatMs);
	REQUIRE(events::pendingCeiling() == kShippedCeiling);
}

TEST_CASE("the route is a read and answers with a request for a stream", "[sse]")
{
	// The gate is asked before the handler, so a caller with less than the
	// route declares never reaches the answer that would open one.
	Response denied = dispatchIn(eventsTable, Method::Get, "/api/v1/events",
	                             "", "", "127.0.0.1", AuthLevel::Public);
	REQUIRE(denied.code == 403);
	REQUIRE_FALSE(events::isStream(denied));

	Response allowed = dispatchIn(eventsTable, Method::Get, "/api/v1/events",
	                              "", "", "127.0.0.1", AuthLevel::Read);
	REQUIRE(allowed.code == 200);
	REQUIRE(events::isStream(allowed));
	REQUIRE(allowed.body.empty());
}

TEST_CASE("a published event reaches a connected stream", "[sse]")
{
	Streaming serving;
	StreamReader s(openStream(serving.port()));
	REQUIRE(s.ok());
	REQUIRE(s.code() == 200);
	REQUIRE(s.headerOf("Content-Type") == "text/event-stream");
	REQUIRE(s.headerOf("Cache-Control") == "no-cache, no-transform");
	REQUIRE(s.headerOf("X-Accel-Buffering") == "no");
	// The framing this file decodes, named here so that a library which
	// stopped using it fails a check rather than silently leaving the decoder
	// with nothing to do.
	REQUIRE(s.headerOf("Transfer-Encoding") == "chunked");

	// The stream opens with the field that says how long to wait before coming
	// back, so the first frame is that and the event is the one after it.
	REQUIRE(s.readFrame(2000).find("retry:") != std::string::npos);

	publish(coreapi::EventType::Zap, 0x2b66, 0);

	const std::string frame = s.readFrame(2000);
	INFO(frame);
	REQUIRE(frame.find("event: zap") != std::string::npos);
	REQUIRE(frame.find("\"channel_id\"") != std::string::npos);
	// Read as hexadecimal where it is read, so written as hexadecimal here.
	REQUIRE(frame.find("\"channel_id\":\"2b66\"") != std::string::npos);
	// The frame ends right after the document, which is what a reader watching
	// for a blank line takes as the end of the event.
	REQUIRE(frame.size() > 3);
	REQUIRE(frame.compare(frame.size() - 3, 3, "}\n\n") == 0);
}

TEST_CASE("the stream opens with a retry field", "[sse]")
{
	// The browser default is a fixed few seconds, which turns a restart into a
	// stampede from every open tab at once.
	Streaming serving;
	StreamReader s(openStream(serving.port()));
	REQUIRE(s.ok());

	const std::string first = s.readFrame(2000);
	INFO(first);
	REQUIRE(first.find("retry:") != std::string::npos);

	// Longer than the default it replaces, or it would be a stampede sooner
	// rather than no stampede.
	const size_t at = first.find("retry:");
	const long ms = std::strtol(first.c_str() + at + 6, NULL, 10);
	REQUIRE(ms >= 10000);
}

TEST_CASE("two streams are told to come back at different moments", "[sse]")
{
	/* Every tab watching a box that restarted is disconnected at the same
	   moment, so one number for all of them brings them all back at the same
	   moment too, however large it is. */
	Streaming serving;
	StreamReader a(openStream(serving.port()));
	StreamReader b(openStream(serving.port()));
	REQUIRE(a.ok());
	REQUIRE(b.ok());
	REQUIRE(a.readFrame(2000) != b.readFrame(2000));
}

TEST_CASE("a comment frame arrives without waiting for anything to happen", "[sse]")
{
	/* A proxy or a NAT box drops a stream that says nothing for between thirty
	   and sixty seconds, and a suspended connection is one the library does not
	   poll, so this is also the only thing that ever notices a peer that went
	   away. Driven at a tenth of a second, because a case that waited out the
	   shipped interval would add twenty seconds to every run. */
	Streaming serving(4, kFastHeartbeatMs);
	StreamReader s(openStream(serving.port()));
	REQUIRE(s.ok());
	REQUIRE(s.readFrame(2000).find("retry:") != std::string::npos);

	const std::string beat = s.readFrame(2000);
	INFO(beat);
	REQUIRE(beat == ":\n\n");
}

TEST_CASE("an event wakes the stream itself and not the next beat", "[sse]")
{
	/* Driven with a beat five seconds away and read for two, so that what
	   carries the event is the delivery and cannot be the thread that beats.
	   Without this the two are not told apart: at a beat every tenth of a
	   second the frame arrives either way, and a delivery that appended
	   without waking anything would read as working. */
	Streaming serving(4, 5000);
	StreamReader s(openStream(serving.port()));
	REQUIRE(s.ok());
	REQUIRE(s.readFrame(2000).find("retry:") != std::string::npos);

	publish(coreapi::EventType::Volume, 0, 11);

	const std::string frame = s.readFrame(2000);
	INFO(frame);
	REQUIRE(frame.find("event: volume") != std::string::npos);
	REQUIRE(frame.find("\"value\":11") != std::string::npos);
}

TEST_CASE("over the ceiling the answer is 503 and not a silent queue", "[sse]")
{
	Streaming serving(2);
	StreamReader a(openStream(serving.port()));
	StreamReader b(openStream(serving.port()));
	REQUIRE(a.ok());
	REQUIRE(b.ok());
	REQUIRE(events::openStreams() == 2);

	/* Read off the socket rather than through the client beside this file, and for the
	   reason that client is not used for a stream at all: a refusal that was not a
	   refusal is a stream, and a caller waiting for the whole of a stream waits for as
	   long as anybody lets it. This answers on the head. */
	StreamReader third(openStream(serving.port()));
	REQUIRE(third.ok());
	REQUIRE(third.code() == 503);
	REQUIRE_FALSE(third.headerOf("Retry-After").empty());
	REQUIRE(third.headerOf("Content-Type") == "application/problem+json");
	// A document with a length, which is what says it ended: a queue nobody was
	// told about would be a body that never does.
	REQUIRE_FALSE(third.headerOf("Content-Length").empty());
	REQUIRE(third.headerOf("Transfer-Encoding").empty());

	std::vector<std::string> named;
	named.push_back("too-many-streams");
	REQUIRE(third.readUntilAll(named, 1000).find("too-many-streams") != std::string::npos);

	// The refusal is the whole of it: nothing was queued behind the two that
	// are open, and the two are still the two.
	REQUIRE(events::openStreams() == 2);
}

TEST_CASE("a ceiling of none means no stream is opened at all", "[sse]")
{
	// The file this server reads takes that key down to nothing, and what it
	// means there has to be what it means here.
	Streaming serving(0);
	StreamReader r(openStream(serving.port()));
	REQUIRE(r.ok());
	REQUIRE(r.code() == 503);
	REQUIRE(r.headerOf("Transfer-Encoding").empty());
	REQUIRE(events::openStreams() == 0);
}

TEST_CASE("a closed stream is not counted and its buffer is gone", "[sse]")
{
	Streaming serving(2);
	const size_t before = events::openStreams();
	REQUIRE(before == 0);
	{
		StreamReader s(openStream(serving.port()));
		REQUIRE(s.ok());
		REQUIRE(events::openStreams() == before + 1);
	}
	REQUIRE(waitForStreams(before, 3000) == before);
	REQUIRE(events::openStreams() == before);

	// And the place it held is free again, which is the half of it that a
	// count alone would not show.
	StreamReader again(openStream(serving.port()));
	REQUIRE(again.ok());
	REQUIRE(events::openStreams() == 1);
}

/* The place a stream holds comes back when its peer goes, and not when something is next
   written down it.

   A stream with nothing to say is held out of the library's event loop, so the library does
   not look at that socket again until it is put back. What used to find a peer that had
   gone was the beat: the stream was woken, written to, and the write failed, on the second
   beat, because the first goes into the kernel's buffer. With the twenty seconds this ships
   with, eight streams closed in a hurry held every place on the box for over a minute.

   The beat here is a minute, which is far longer than this case waits, so a place that comes
   back inside it cannot have come back because anything was written down the stream. */
TEST_CASE("a stream whose peer went away gives its place back at once", "[sse]")
{
	Streaming serving(1, 60000);
	{
		StreamReader s(openStream(serving.port()));
		REQUIRE(s.ok());
		REQUIRE(s.code() == 200);
		REQUIRE(events::openStreams() == 1);
	}

	REQUIRE(waitForStreams(0, 3000) == 0);

	// And the place is one another caller can have, which a count of none on
	// its own would not show.
	StreamReader again(openStream(serving.port()));
	REQUIRE(again.ok());
	REQUIRE(again.code() == 200);
	REQUIRE(events::openStreams() == 1);
}

TEST_CASE("publishing with no stream open costs nothing and does not crash", "[sse]")
{
	Streaming serving;
	events::stop();
	publish(coreapi::EventType::Standby, 0, 1);
	REQUIRE(events::openStreams() == 0);
	events::start();
	publish(coreapi::EventType::Standby, 0, 0);
	REQUIRE(events::openStreams() == 0);
}

TEST_CASE("events published while a stream is being set up are not lost", "[sse]")
{
	/* A snapshot fetched before the subscription would miss whatever happened
	   in between, so the subscription comes first and the buffer is drained
	   after. This is the server half of that contract: the head of the answer
	   is queued after the stream is registered, so a client that has read the
	   head is a client nothing can be lost from. */
	Streaming serving;
	StreamReader s(openStream(serving.port()));
	REQUIRE(s.ok());

	for (int i = 0; i < 5; ++i)
		publish(coreapi::EventType::Volume, 0, i);

	std::vector<std::string> wanted;
	for (int i = 0; i < 5; ++i)
	{
		char want[32];
		std::snprintf(want, sizeof(want), "\"value\":%d", i);
		wanted.push_back(want);
	}

	const std::string all = s.readUntilAll(wanted, 3000);
	for (size_t i = 0; i < wanted.size(); ++i)
	{
		INFO(wanted[i] << " in " << all);
		REQUIRE(all.find(wanted[i]) != std::string::npos);
	}
}

TEST_CASE("a value a caller could not have written cannot break a frame", "[sse]")
{
	/* The frames are separated by a blank line, so a member carrying one would
	   be a frame ending where the body says so and the rest of it read as the
	   next frame. Nothing fills the text member yet, which is exactly why this
	   is worth pinning before something does. */
	Streaming serving;
	StreamReader s(openStream(serving.port()));
	REQUIRE(s.ok());
	REQUIRE(s.readFrame(2000).find("retry:") != std::string::npos);

	coreapi::Event e;
	e.type = coreapi::EventType::Mode;
	e.text = "one\n\ntwo";
	coreapi::EventBus::instance().publish(e);

	const std::string frame = s.readFrame(2000);
	INFO(frame);
	REQUIRE(frame.find("event: mode") != std::string::npos);
	REQUIRE(frame.find("one\\n\\ntwo") != std::string::npos);
	/* The frame ends where the document ends. Asking only that it holds a
	   blank line would be asking nothing: a frame is what stops at the first
	   one, so every frame there is holds one at its end. */
	REQUIRE(frame.size() > 3);
	REQUIRE(frame.compare(frame.size() - 3, 3, "}\n\n") == 0);

	// And what follows is not the rest of the value read as a frame of its own.
	const std::string next = s.readFrame(2000);
	INFO(next);
	REQUIRE(next.find("two") == std::string::npos);
}

TEST_CASE("a stream that stops reading is dropped rather than buffered forever", "[sse]")
{
	// A NAT box that vanishes leaves a socket that never reports an error, and
	// a per-stream buffer with no ceiling is a memory leak with a network
	// trigger.
	Streaming serving;
	StreamReader s(openStream(serving.port()));
	REQUIRE(s.ok());
	s.stopReading();

	for (int i = 0; i < 100000 && events::openStreams() > 0; ++i)
		publish(coreapi::EventType::Volume, 0, i);

	REQUIRE(waitForStreams(0, 5000) == 0);
	REQUIRE(events::openStreams() == 0);
}

TEST_CASE("every event type reaches a stream under its own name", "[sse]")
{
	/* The names are walked elsewhere; this is the wire. A beat five seconds
	   away, so that what comes back between two publishes is the event and
	   never a comment. */
	Streaming serving(4, 5000);
	StreamReader s(openStream(serving.port()));
	REQUIRE(s.ok());
	REQUIRE(s.readFrame(2000).find("retry:") != std::string::npos);

	static const coreapi::EventType kAll[] = {
		coreapi::EventType::Zap,
		coreapi::EventType::Mode,
		coreapi::EventType::Standby,
		coreapi::EventType::Volume,
		coreapi::EventType::Mute,
		coreapi::EventType::RecordStart,
		coreapi::EventType::RecordStop,
		coreapi::EventType::TimerChanged,
		coreapi::EventType::EpgUpdated,
		coreapi::EventType::BouquetsChanged
	};
	const size_t count = sizeof(kAll) / sizeof(kAll[0]);
	REQUIRE(count == 10);

	for (size_t i = 0; i < count; ++i)
	{
		coreapi::Event e;
		e.type = kAll[i];
		// Every bit set, which is the half of the identifier a number on the
		// wire would round away.
		e.channel_id = 0xFFFFFFFFFFFFFFFFull;
		e.value = (int) i;
		e.text = "a\"b\\c\td";
		coreapi::EventBus::instance().publish(e);

		const std::string frame = s.readFrame(2000);
		INFO(events::typeName(kAll[i]) << " frame: " << frame);
		REQUIRE(frame.find(std::string("event: ") + events::typeName(kAll[i]) + "\n")
		        != std::string::npos);
		REQUIRE(frame.find("\"channel_id\":\"ffffffffffffffff\"") != std::string::npos);
		REQUIRE(frame.find("a\\\"b\\\\c\\td") != std::string::npos);
		char want[32];
		std::snprintf(want, sizeof(want), "\"value\":%d", (int) i);
		REQUIRE(frame.find(want) != std::string::npos);
		REQUIRE(frame.size() > 3);
		REQUIRE(frame.compare(frame.size() - 3, 3, "}\n\n") == 0);
	}
}

TEST_CASE("a stream that is carrying events is not sent comments as well", "[sse]")
{
	/* The interval is how long ONE stream may be silent. Measured off a clock
	   the whole server shared, a stream that had carried an event a moment ago
	   would still be sent a comment when the beat fell due, which is a beat
	   measuring nothing. Four intervals pass here, and events arrive twenty
	   times faster than that. */
	Streaming serving(4, 400);
	StreamReader s(openStream(serving.port()));
	REQUIRE(s.ok());
	REQUIRE(s.readFrame(2000).find("retry:") != std::string::npos);

	size_t comments = 0;
	size_t frames = 0;
	const int64_t until = nowMs() + 1600;
	while (nowMs() < until)
	{
		publish(coreapi::EventType::Volume, 0, 1);
		napMs(20);
		for (;;)
		{
			const std::string f = s.readFrame(1);
			if (f.empty())
				break;
			++frames;
			if (f == ":\n\n")
				++comments;
		}
	}
	/* What is still on its way, so a comment already sent is counted rather
	   than left on the socket. Bounded by the clock as well as by the silence:
	   a server that beat every stream on a clock of its own would keep sending
	   comments for as long as anything read them, and a drain that waited for
	   quiet would wait for ever. */
	const int64_t drained = nowMs() + 600;
	while (nowMs() < drained)
	{
		const std::string f = s.readFrame(100);
		if (f.empty())
			break;
		++frames;
		if (f == ":\n\n")
			++comments;
	}

	INFO("frames " << frames << " of which comments " << comments);
	// Enough traffic that a beat would have fallen due several times.
	REQUIRE(frames > 20);
	REQUIRE(comments == 0);
}

TEST_CASE("a wedged stream's descriptor is named while the lock is held", "[sse]")
{
	/* The descriptor belongs to the library, which closes it just after the call that gives
	   the stream back, and that call waits on this server's event lock. So the number is
	   good while the lock is held and is anybody's the moment it is not: read under the
	   lock and used after it, it can be closed, handed to the next connection the box
	   accepts, and shut down under that one.

	   Nothing about the shutdown itself tells the two arrangements apart. What does is
	   whether anybody else can take the lock at that moment. */
	LockWatch watch;
	LockWatch::watched = &watch;
	watch.start();
	REQUIRE(watch.started);
	events::setReapProbeForTest(&LockWatch::probe);

	/* The reader outlives the server on purpose. A peer that closes its socket
	   is a stream the library gives back by itself, so a reader that went out
	   of scope with the server would leave nothing for the reaper to reach. */
	struct Keep
	{
		StreamReader *reader;
		Keep() : reader(NULL) {}
		~Keep() { delete reader; }
	private:
		Keep(const Keep &);
		Keep &operator=(const Keep &);
	} keep;

	bool wedged = false;
	size_t held_back = 0;
	{
		// A beat far enough away that nothing but this case writes into the
		// stream while it is being wedged, so what is pending only moves when
		// this case or the library moves it.
		Streaming serving(4, 5000);
		/* Above anything published below, so that the peer is wedged rather
		   than dropped: a stream over the buffer ceiling is ended by the reader
		   on its way through and never reaches the descriptor at all. What is
		   wanted here is the other state, a stream the library is trying to
		   write to and cannot. */
		events::setPendingCeiling(64u * 1024u * 1024u);

		// A small window, so the far end asks for little and the near end has
		// less room to hide what it has been handed.
		keep.reader = new StreamReader(openStream(serving.port(), 4096));
		REQUIRE(keep.reader->ok());
		keep.reader->stopReading();

		/* Published until what is pending stops going down while nothing is being
		   published. That, and nothing else, is what says the socket will take no more: a
		   quantity cannot say it, because how much the two kernels absorb between them is
		   theirs to decide and the buffer that absorbs it grows while it is being filled.
		   An earlier form sent a megabyte and called that wedged; the sending buffer here
		   grows to four, so the megabyte was still draining when the reaper looked, and the
		   case failed about one run in three. Bounded, so a shape that stopped wedging
		   fails the check below rather than running until somebody kills it. */
		for (int round = 0; round < 12 && !wedged; ++round)
		{
			for (int i = 0; i < 50000; ++i)
				publish(coreapi::EventType::Volume, 0, i);

			held_back = events::pendingBytes();
			napMs(250);
			wedged = held_back > 0 && events::pendingBytes() == held_back;
		}

		/* Pending is not empty, so the stream is not suspended, and none of it
		   moved while nothing was written, so the library is blocked on the
		   socket. That is the one state the reaper has to reach for a
		   descriptor to answer, and it cannot come undone while this case still
		   holds the far end unread. */
		INFO("held back " << held_back << " bytes");
		REQUIRE(wedged);
		REQUIRE(events::openStreams() == 1);

		// Leaving here stops the server, which takes every stream down, and a
		// stream in that state is taken down by its descriptor.
	}

	events::setReapProbeForTest(NULL);
	watch.join();
	LockWatch::watched = NULL;

	// Three checks and not one, so that a run which observed nothing says so
	// instead of reading like a run that observed the property.
	REQUIRE(watch.reached);      // the reaper named a descriptor
	REQUIRE(watch.door);         // somebody was there to be kept out
	REQUIRE(watch.held);         // and was kept out
	REQUIRE(events::openStreams() == 0);
}

TEST_CASE("a head of the stream is answered without opening one", "[sse]")
{
	/* A HEAD is answered by the route that answers the GET, and a HEAD that
	   opened a stream would hold a connection for a caller that asked for a
	   head and is not going to read a body. */
	Streaming serving;
	testhttp::Reply r = testhttp::request(serving.port(), "HEAD", "/api/v1/events");
	REQUIRE(r.transport_ok);
	REQUIRE(r.code == 200);
	REQUIRE(r.header("Content-Type") == "text/event-stream");
	REQUIRE(r.header("X-Accel-Buffering") == "no");
	// The head of a document of no length and not the head of a stream, which
	// is what a length beside it says and what a framing beside it would deny.
	REQUIRE(r.header("Transfer-Encoding").empty());
	REQUIRE(r.header("Content-Length") == "0");
	REQUIRE(events::openStreams() == 0);
}

TEST_CASE("a stream still open when the server stops is closed with it", "[sse]")
{
	/* The library says that stopping a daemon while a connection of it is
	   suspended leaks the connection and the socket or does something worse,
	   and an idle stream is a suspended connection. So stopping the server has
	   to have finished with every one of them before the daemon goes. */
	{
		Streaming serving;
		StreamReader s(openStream(serving.port()));
		REQUIRE(s.ok());
		REQUIRE(events::openStreams() == 1);
	}
	REQUIRE(events::openStreams() == 0);
}

TEST_CASE("a server started a second time is subscribed a second time", "[sse]")
{
	/* The subscription is made where the daemon is started and given up where
	   it is stopped, so a box that stops and starts its server again has to
	   end up subscribed. Nothing here checks that it is not subscribed twice:
	   the bus refuses a subscriber it already holds, so a case saying so would
	   be a case about the bus. */
	{
		Streaming first;
		StreamReader s(openStream(first.port()));
		REQUIRE(s.ok());
	}

	Streaming second;
	StreamReader s(openStream(second.port()));
	REQUIRE(s.ok());
	REQUIRE(s.readFrame(2000).find("retry:") != std::string::npos);

	publish(coreapi::EventType::Volume, 0, 7);

	const std::string one = s.readFrame(2000);
	INFO(one);
	REQUIRE(one.find("\"value\":7") != std::string::npos);
}
