/*
 * events.cpp - the event stream, and the seats that listen on it
 *
 * Copyright (C) 2026 NI-Team
 * Copyright (C) 2026 Tuxbox-Project (fixes)
 *
 * DELIBERATE DRIFT from the source's events.cpp, declared here so the next
 * upstream diff reads mechanically. nhttpd carries a stream on the thread
 * of the connection it arrived on, where the source's libmicrohttpd holds
 * suspended connections in an event loop. The file is otherwise taken as
 * written; the translation is one pattern in eight places:
 *
 *   Stream.conn                      -> gone; fd (already there) is enough
 *   suspended + MHD_suspend          -> suspended = "the pump waits on the
 *                                       one condition beside the lock"
 *   resume vectors + MHD_resume      -> one bool, one broadcast after the
 *                                       lock is given up
 *   readStream (content reader)      -> the pump's draw: <= kBlockSize per
 *                                       turn, taken under the lock, written
 *                                       outside it (pumpStream below)
 *   streamGone (free callback)       -> called by the pump on its way out,
 *                                       and by closeStream for a stream
 *                                       that was opened and never pumped
 *   openStream(MHD_Connection*,r,..) -> openStream(int fd, StreamHandle&,
 *                                       refusal): registration, count and
 *                                       preamble seeding as written; the
 *                                       response object build is gone, the
 *                                       bridge writes the header
 *   reapUnderLock                    -> shutdown branch as written; the
 *                                       suspended branch asks for the one
 *                                       broadcast instead of a resume
 *   stop()/closeEvery()              -> as written, over the same wake bool
 *
 * One condition for every pump, beside the one lock: a per-stream condition
 * would have a lifetime question the broadcast does not. A woken pump
 * re-checks its own stream under the lock; a wake that finds nothing is a
 * loop turn. The draw stays at kBlockSize so pendingBytes() keeps meaning
 * what it says while a peer is not reading.
 *
 * One ceiling besides, in start(): every open stream holds one of nhttpd's
 * HTTPD_MAX_CONNECTIONS connection threads, and the configuration allows more
 * streams than there are threads, for a server that spends none on a stream.
 * At most a third of the threads go to streams, so ordinary requests always
 * find one.
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

#include "events.h"

#include "webconfig.h"
#include "http.h"
#include "json.h"
#include "status.h"

#include "coreapi/base/errors.h"

#include "yconfig.h"

#include <cstddef>
#include <cstdio>
#include <cstring>
#include <vector>

#include <poll.h>
#include <pthread.h>
#include <sys/socket.h>
#include <time.h>

#include <OpenThreads/Condition>
#include <OpenThreads/Mutex>
#include <OpenThreads/ScopedLock>

namespace httpd
{

namespace events
{

namespace
{

/* The comment frame. A colon opens a comment in this format, so what a client
   does with it is drop it, and what it does for this server is put a byte on a
   socket that has been quiet. */
const char kComment[] = ":\n\n";

/* The most one write to the library carries. A frame is a few dozen bytes, so
   this is the buffer the library keeps per stream rather than a rate: what is
   pending goes out in one call whenever it fits. */
const size_t kBlockSize = 4096;

const unsigned kHeartbeatDefaultMs = 20000;

// A quarter of a megabyte per stream, on a box whose whole memory is measured
// in tens of them.
const size_t kPendingCeilingDefault = 256u * 1024u;

/* How often the thread below looks. Fifty milliseconds while there is anything to
   look at, a stream whose peer stopped reading being dropped by this thread and
   nothing else, and a quarter of a second while there is not. Clamped to the
   heartbeat when a case has driven that shorter than the tick. */
const unsigned kTendBusyMs = 50;
const unsigned kTendIdleMs = 250;

// How long a shutdown waits before it starts saying what it is waiting for.
// Not a deadline: what stopping does when the streams are not back yet is
// stated where it waits.
const int kSayWaitingMs = 5000;

/* What a client is told to wait before it opens the stream again. Longer than the
   browser's own default, and spread across the streams this server has handed out:
   every tab that was watching a box that restarted comes back at once otherwise, and
   comes back together however long the wait is if they are all told the same. */
const unsigned kRetryBaseMs   = 10000;
const unsigned kRetrySpreadMs = 1000;
const unsigned kRetrySpread   = 10;

struct Stream
{
	/* The socket, kept for the thread that reaps a wedged stream. The pump
	   that owns the connection closes it only after this stream is gone from
	   the list, so a descriptor read under the lock is good while it is held. */
	int         fd;
	std::string pending;
	// Set once and never cleared: a stream past the ceiling is gone, and
	// clearing it would restart a peer that is not reading with a gap in the
	// middle of what it did read.
	bool        overflowed;
	// The stream is being taken down because this server is stopping serving
	// them, which is a clean end and not a fault.
	bool        closing;
	/* The pump is waiting on the condition beside the lock, with nothing to
	   send. Set and cleared only by the pump itself, around its wait; what
	   everything else does with it is decide whether a broadcast is owed or
	   the socket has to be shut down under a blocked send. */
	bool        suspended;
	// The socket has been shut down already, so that a stream being reaped is
	// reaped once however many times the thread comes round.
	bool        shut;
	/* When something last went into this stream, so the interval is what the header
	   beside this says it is: how long this stream may be silent. One clock for the
	   server would send a comment down a stream that had just carried an event. */
	int64_t     last_written;

	Stream() : fd(-1), overflowed(false), closing(false),
	           suspended(false), shut(false), last_written(0)
	{
	}
};

/* One stream, owned until the list that keeps them owns it. The two calls
   between the allocation and the push can both fail, and a raw pointer dropped
   there is memory nothing ever gives back. */
struct Holder
{
	Stream *held;

	explicit Holder(Stream *s) : held(s) {}
	~Holder() { delete held; }
	Stream *release()
	{
		Stream *out = held;
		held = NULL;
		return out;
	}

private:
	Holder(const Holder &);
	Holder &operator=(const Holder &);
};

/* Held by everything that reads or writes the list below and the streams in it.
   Built on first use, so nothing here needs a constructor run before main.

   Short by construction: what is done under it is appending to a string and walking a
   list as long as the ceiling on open streams. Two things besides, both held here on
   purpose:

   the suspend inside the reader, which the library states may be called from there and
   which has to happen while the emptiness that led to it still holds;

   and the shutdown of a wedged stream's socket, which is a syscall that takes no lock
   of the library's and is the whole of what keeps that descriptor from being somebody
   else's. The call that gives a stream back waits here, and the library closes the
   socket behind that call, so a descriptor read under this lock cannot have been closed
   until it is given up. Read it here and use it outside and the number can be closed,
   reissued to another connection, and shut down under that one.

   Nothing else that could take a lock of the library's is called from under it. */
OpenThreads::Mutex &lock()
{
	static OpenThreads::Mutex m;
	return m;
}

/* The one condition every pump waits on, beside the one lock. Never
   destroyed, for the reason the subscriber is not: there is no moment in a
   process being torn down at which no pump can be waiting. A broadcast wakes
   every pump and each re-checks its own stream under the lock, so a wake
   that finds nothing costs a loop turn and a per-stream lifetime question
   costs nothing because there is none. */
OpenThreads::Condition &cond()
{
	static OpenThreads::Condition *c = new OpenThreads::Condition;
	return *c;
}

std::vector<Stream *> &streams()
{
	static std::vector<Stream *> *s = new std::vector<Stream *>;
	return *s;
}

size_t   max_streams_      = 0;
unsigned heartbeat_ms_     = kHeartbeatDefaultMs;
size_t   pending_ceiling_  = kPendingCeilingDefault;
unsigned next_serial_      = 0;
bool     serving_          = false;
bool     tending_          = false;
pthread_t tender_;

// Nothing in the product installs one. What a case does through it is stated
// where it is declared.
ReapProbe reap_probe_      = NULL;

int64_t monotonicMs()
{
	struct timespec t;
	if (clock_gettime(CLOCK_MONOTONIC, &t) != 0)
		return 0;
	return (int64_t) t.tv_sec * 1000 + t.tv_nsec / 1000000;
}

void sleepMs(unsigned ms)
{
	struct timespec t;
	t.tv_sec = (time_t) (ms / 1000);
	t.tv_nsec = (long) (ms % 1000) * 1000 * 1000;
	nanosleep(&t, NULL);
}

/* Appends to a stream and holds it to the ceiling. The whole of what was pending is
   given back when the ceiling is reached: what is being kept is for a peer that has
   stopped taking it, and half of it is worth nothing to either side.

   Answers whether the stream has something to be woken for. now is passed in rather
   than read here, so one delivery to every stream reads the clock once. */
bool appendTo(Stream *s, const char *bytes, size_t n, int64_t now)
{
	if (s->overflowed || s->closing)
		return false;

	if (s->pending.size() + n > pending_ceiling_)
	{
		s->overflowed = true;
		std::string().swap(s->pending);
		return true;
	}

	s->pending.append(bytes, n);
	s->last_written = now;
	return true;
}

/* The one subscriber this server has, holding the list of streams rather than one
   subscriber per stream. The bus copies its subscriber list on every publish, so a
   list that grew with connections would make every event cost what the box has
   open. */
class Fanout : public coreapi::Subscriber
{
	public:
		void onEvent(const coreapi::Event &e);
};

Fanout &fanout()
{
	/* Never destroyed. A subscriber may not be destroyed while a delivery may
	   be running, and there is no moment in a process being torn down at which
	   that can be known. */
	static Fanout *f = new Fanout;
	return *f;
}

void Fanout::onEvent(const coreapi::Event &e)
{
	/* Nothing may leave here by being thrown. What the bus does with a throw is swallow
	   it and cost every subscriber behind this one their copy of the event, so the one
	   allocation in here answers for itself. Built before the lock is taken, so what is
	   held while the message path waits is a list walk and a few appends. */
	std::string frame;
	try
	{
		const char *name = typeName(e.type);
		if (name == NULL)
			return;

		frame.reserve(128);
		frame += "event: ";
		frame += name;
		frame += "\ndata: ";
		appendEventJson(frame, e);
		frame += "\n\n";
	}
	catch (...)
	{
		return;
	}

	const int64_t now = monotonicMs();

	bool wake = false;
	try
	{
		OpenThreads::ScopedLock<OpenThreads::Mutex> held(lock());

		std::vector<Stream *> &list = streams();
		for (size_t i = 0; i < list.size(); ++i)
		{
			Stream *s = list[i];
			if (!appendTo(s, frame.data(), frame.size(), now))
				continue;
			if (s->suspended)
				wake = true;
		}
	}
	catch (...)
	{
		// An append that could not grow leaves that stream short and the rest
		// of them untouched, which is better than the whole delivery ending.
	}

	/* Outside the lock, and this is the whole of what the bus thread does to a socket:
	   nothing. The pumps wake, and each writes what was appended to its own
	   stream on its own connection thread. */
	if (wake)
		cond().broadcast();
}

/* The stream is over, one way or the other. Where a stream stops being
   counted, and the only place: run by the pump on its way out, and by
   closeStream for a stream that was opened and never pumped. May not throw -
   the pump's caller is built without exceptions. */
void streamGone(void *cls)
{
	Stream *s = (Stream *) cls;
	if (s == NULL)
		return;

	/* The line above this function says this may not throw, and nothing in it does:
	   erasing a pointer out of a vector moves pointers, and a destructor does not throw
	   either. What was missing was the difference between saying so and holding to it,
	   which is what every later edit here decides. */
	try
	{
		{
			OpenThreads::ScopedLock<OpenThreads::Mutex> held(lock());
			std::vector<Stream *> &list = streams();
			for (size_t i = 0; i < list.size(); ++i)
			{
				if (list[i] != s)
					continue;
				list.erase(list.begin() + (std::ptrdiff_t) i);
				break;
			}
		}

		// Outside the lock, so that whatever the string gives back is given
		// back without the rest of the streams waiting on it.
		delete s;
	}
	catch (...)
	{
	}
}

/* Whether the far end of a stream has gone.

   Asked because nothing else here would notice. A stream with nothing to say is held out
   of the library's event loop, so the library does not look at that socket again until
   it is put back. What used to find one was the heartbeat: the stream was woken every
   twenty seconds, written to, and the write failed, but only on the second of them,
   because the first goes into the kernel's buffer. Two beats and the gap before the
   first is over a minute, and for that minute the stream still counted against the
   ceiling, so eight tabs closed in a hurry left every reader on the network refused.

   This is the same question asked without writing anything. A peer that closes only the
   half it writes on and goes on reading is answered yes as well; nothing that reads this
   server's streams does that.

   Called with the lock held, for the reason the reap below is: the descriptor belongs to
   the library, and it is good only for as long as this lock is. */
bool peerGone(int fd)
{
	if (fd < 0)
		return false;

	struct pollfd p;
	p.fd = fd;
	/* The half close. The three conditions below it are reported whether they
	   were asked for or not, and are asked for by writing none of them. */
#ifdef POLLRDHUP
	p.events = POLLRDHUP;
#else
	p.events = 0;
#endif
	p.revents = 0;

	// No wait at all: this runs down the whole list every turn of a thread
	// that has other streams to get to.
	if (poll(&p, 1, 0) <= 0)
		return false;

#ifdef POLLRDHUP
	const short gone = POLLRDHUP | POLLHUP | POLLERR | POLLNVAL;
#else
	const short gone = POLLHUP | POLLERR | POLLNVAL;
#endif
	return (p.revents & gone) != 0;
}

/* Takes a stream out of the pump's reach for good.

   Called with the lock held, and the shutdown below is why that is not merely a
   convention. The descriptor belongs to the connection thread, which closes it
   only after the pump has given this stream back, and giving it back waits on
   this lock. So the number is good while it is held and is anybody's the moment
   it is not: read here and used outside, it can be closed, handed to the next
   connection the box accepts, and shut down under that one. */
void reapUnderLock(Stream *s, bool &wake)
{
	/* The clean way out first: a pump waiting on the condition finds the
	   closing flag its caller set and ends the body on the way through, which
	   touches no descriptor at all.

	   A pump that is not waiting is one blocked in a send the peer is not
	   taking, which is what a peer that stopped reading looks like, and no
	   flag reaches a blocked send. That one alone gets the socket shut down
	   under it, which fails the send and hands the pump its way out. */
	if (s->suspended)
	{
		wake = true;
		return;
	}
	if (s->shut || s->fd < 0)
		return;

	s->shut = true;
	if (reap_probe_ != NULL)
		reap_probe_();
	::shutdown(s->fd, SHUT_RDWR);
}

/* One turn of the thread below. Answers false when it has been asked to stop, and
   leaves nap holding how long to wait before the next turn.

   The boundary is in here and not around the loop, because what it has to protect is
   the pair of writes that mark a stream awake and then wake it: a throw between them,
   or before the resumes below, is a stream nothing ever wakes again. */
bool tendOnce(unsigned &nap)
{
	bool go_on = true;
	bool wake = false;

	try
	{
		OpenThreads::ScopedLock<OpenThreads::Mutex> held(lock());
		if (!tending_)
		{
			go_on = false;
		}
		else
		{
			const int64_t now = monotonicMs();
			std::vector<Stream *> &list = streams();

			for (size_t i = 0; i < list.size(); ++i)
			{
				Stream *s = list[i];
				/* A peer that has gone is marked as one that is being closed before it is reaped,
				   because the reader is what ends a suspended stream and the reader asks that flag.
				   Without it the stream would be woken, find nothing to say and suspend again. */
				if (!s->closing && peerGone(s->fd))
					s->closing = true;
				if (s->overflowed || s->closing)
				{
					reapUnderLock(s, wake);
					continue;
				}
				// This stream's own silence and not the server's. A stream that
				// carried an event a moment ago is not silent, and a comment
				// sent down it would be a beat measuring nothing.
				if ((now - s->last_written) < (int64_t) heartbeat_ms_)
					continue;
				if (!appendTo(s, kComment, sizeof(kComment) - 1, now))
					continue;
				if (s->suspended)
					wake = true;
			}

			nap = kTendIdleMs;
			if (!list.empty())
				nap = (heartbeat_ms_ < kTendBusyMs) ? heartbeat_ms_ : kTendBusyMs;
			if (nap == 0)
				nap = 1;
		}
	}
	catch (...)
	{
		/* A turn that could not grow a buffer is a turn that did part of its work, and what
		   it collected is still woken below. Nothing is said about it: the next turn is a
		   few tens of milliseconds away and a line per turn would be the whole log. */
	}

	if (wake)
		cond().broadcast();

	return go_on;
}

/* Beats and reaps. A thread of this file's own and not the bus's: writing to a socket
   is exactly what the bus thread may not do, and a stream nobody is reading is only
   found by writing to it. A thread start routine, so nothing may leave it by being
   thrown: an exception out of here does not end a stream, it ends the process. */
void *tend(void *)
{
	for (;;)
	{
		unsigned nap = kTendIdleMs;
		bool go_on = true;
		try
		{
			go_on = tendOnce(nap);
		}
		catch (...)
		{
		}

		if (!go_on)
			return NULL;
		sleepMs(nap);
	}
}

/* Marks every stream as one that is going and does what that takes. Answers how many
   there were, so the caller can wait for the library to give them back.

   Nothing may leave here by being thrown either: this is reached from the call that
   stops the server, and the rest of the tree is built without exceptions. */
size_t closeEvery()
{
	bool wake = false;
	size_t left = 0;

	try
	{
		OpenThreads::ScopedLock<OpenThreads::Mutex> held(lock());
		std::vector<Stream *> &list = streams();
		left = list.size();
		for (size_t i = 0; i < list.size(); ++i)
		{
			list[i]->closing = true;
			reapUnderLock(list[i], wake);
		}
	}
	catch (...)
	{
	}

	if (wake)
		cond().broadcast();

	return left;
}

// The retry field a stream opens with, spread over the streams this server has
// handed out so that the tabs watching a box that restarted do not all come
// back at the same moment.
void appendPreamble(std::string &out, unsigned serial)
{
	char line[48];
	std::snprintf(line, sizeof(line), "retry: %u\n\n",
	              kRetryBaseMs + (serial % kRetrySpread) * kRetrySpreadMs);
	out += line;
}

} // namespace

const char *typeName(coreapi::EventType t)
{
	/* No default, so that a type added to the enumeration stops this build
	   rather than reaching a wire under a name somebody guessed. This
	   directory compiles an unhandled enumerator as an error. */
	switch (t)
	{
		case coreapi::EventType::Zap:          return "zap";
		case coreapi::EventType::Mode:         return "mode";
		case coreapi::EventType::Standby:      return "standby";
		case coreapi::EventType::Volume:       return "volume";
		case coreapi::EventType::Mute:         return "mute";
		case coreapi::EventType::RecordStart:  return "record-start";
		case coreapi::EventType::RecordStop:   return "record-stop";
		case coreapi::EventType::TimerChanged: return "timer-changed";
		case coreapi::EventType::EpgUpdated:   return "epg-updated";
		case coreapi::EventType::BouquetsChanged: return "bouquets-changed";
		case coreapi::EventType::SettingsChanged: return "settings-changed";
	}

	// Only a value that is not one of the enumerators reaches here, which is
	// not something the program can produce and is not something to name.
	return NULL;
}

void appendEventJson(std::string &out, const coreapi::Event &e)
{
	char id[24];
	std::snprintf(id, sizeof(id), "%llx", (unsigned long long) e.channel_id);

	Json j(out, 96);
	j.beginObject();
	j.key("channel_id");
	j.value(id);
	j.key("value");
	j.value(e.value);
	j.key("text");
	j.value(e.text);
	j.endObject();
}

const char *streamContentType()
{
	return "text/event-stream";
}

bool isStream(const Response &r)
{
	return r.code == StatusOk && r.content_type == streamContentType();
}

bool isStreamRoute(const Endpoint &ep)
{
	for (size_t i = 0; i < eventsTable.count; ++i)
	{
		if (&eventsTable.endpoints[i] == &ep)
			return true;
	}
	return false;
}

Opened openStream(int fd, StreamHandle &out, Response &refusal)
{
	out = NULL;

	/* A connection that names no descriptor is not made into a stream at all:
	   a stream that cannot be shut down is one a peer that stops reading
	   holds until the server is stopped. */
	if (fd < 0)
		return StreamFailed;

	Stream *s = NULL;
	unsigned beat_s = 0;
	{
		OpenThreads::ScopedLock<OpenThreads::Mutex> held(lock());

		/* Counted and registered under one lock, so that two requests arriving
		   together cannot both find room for the last place. */
		if (!serving_ || streams().size() >= max_streams_)
		{
			// Only the number is taken from in here. The document that carries
			// it is written below, once this is given up.
			beat_s = heartbeat_ms_ / 1000u;
		}
		else
		{
			/* Owned until the list owns it. Between the allocation and the
			   push there are two calls that can fail, and a raw pointer
			   dropped there is a stream nobody ever frees. */
			Holder made(new Stream);
			made.held->fd = fd;
			made.held->last_written = monotonicMs();

			/* Seeded before the stream is registered, so the first thing the reader finds is
			   bytes rather than nothing. That is what puts the head of the answer on the wire
			   without waiting for the box, and a client that has read the head knows what is
			   published next cannot be lost. */
			appendPreamble(made.held->pending, next_serial_);

			streams().push_back(made.held);
			++next_serial_;
			s = made.release();
		}
	}

	if (s == NULL)
	{
		/* Written with the lock given up. It builds a document, an error and a header, which
		   is three allocations and a format, and a delivery from the message path would queue
		   behind all of it for an answer that is only ever a refusal. */
		char after[16];
		// Longer than the interval after which a stream whose peer has gone is
		// noticed, because that is when a place frees up.
		std::snprintf(after, sizeof(after), "%u", beat_s + 10u);

		refusal = problemResponse(StatusServiceUnavailable,
		                          coreapi::ErrorCode::TooManyStreams,
		                          "this server is already carrying as many event streams as it will");
		refusal.headers.push_back(std::make_pair(std::string("Retry-After"),
		                                         std::string(after)));
		return StreamRefused;
	}

	/* Owned by the caller from here: it either pumps, which gives the stream
	   back on its way out, or it closes the handle without pumping. There is
	   no response object to build - the caller writes the header itself, and
	   what it fails on is its own to clean up through closeStream(). */
	out = (StreamHandle) s;
	return StreamOpened;
}

void closeStream(StreamHandle h)
{
	streamGone((Stream *) h);
}

void pumpStream(StreamHandle h, StreamWrite write, void *ctx)
{
	Stream *s = (Stream *) h;
	if (s == NULL || write == NULL)
	{
		streamGone(s);
		return;
	}

	/* The boundary: the caller is built without exceptions, and a throw out
	   of a string here may not end the process. A pump that cannot allocate
	   its block ends its stream and nothing else. */
	try
	{
		std::string block;
		block.reserve(kBlockSize);

		for (;;)
		{
			bool end = false;
			bool error = false;
			block.clear();

			{
				OpenThreads::ScopedLock<OpenThreads::Mutex> held(lock());
				for (;;)
				{
					/* Asked before the pending bytes are looked at: a stream
					   being closed on the way out has nothing left to say and
					   a stream over the ceiling has had what it was holding
					   taken away. */
					if (s->closing)
					{
						end = true;
						break;
					}
					if (s->overflowed)
					{
						error = true;
						break;
					}
					if (!s->pending.empty())
					{
						/* One draw of at most kBlockSize, not the whole of
						   what is pending: pendingBytes() keeps meaning what
						   it says while the send below blocks on a peer that
						   is not reading, which is what the wedged-reader
						   check measures. */
						const size_t n = (s->pending.size() < kBlockSize)
							? s->pending.size() : kBlockSize;
						block.assign(s->pending.data(), n);
						s->pending.erase(0, n);
						break;
					}

					/* Under the same lock as the emptiness that led to it,
					   and in a loop: a broadcast wakes every pump, and one
					   woken for another's bytes waits again. The timeout is
					   a belt over the broadcasts, not a schedule. */
					s->suspended = true;
					cond().wait(&lock(), 1000);
					s->suspended = false;
				}
			}

			if (end || error)
				break;

			/* Outside the lock, which is the point of the block: a peer that
			   takes its time costs this stream's thread, not the delivery. */
			if (!write(ctx, block.data(), block.size()))
			{
				OpenThreads::ScopedLock<OpenThreads::Mutex> held(lock());
				s->closing = true;
				break;
			}
		}
	}
	catch (...)
	{
	}

	streamGone(s);
}

void start()
{
	bool serving = false;
	{
		OpenThreads::ScopedLock<OpenThreads::Mutex> held(lock());

		/* Read here and not per request. The configuration is written before
		   the server starts and read by the threads it runs, and a ceiling
		   read on a request thread would be a second reader of it. */
		max_streams_ = config().sse_max_streams;
		const size_t carried = HTTPD_MAX_CONNECTIONS / 3;
		if (max_streams_ > carried)
		{
			std::fprintf(stderr, "httpd: %u event streams configured, this server "
			             "carries %u beside its other connections\n",
			             (unsigned) max_streams_, (unsigned) carried);
			max_streams_ = carried;
		}
		serving_ = true;

		if (!tending_)
		{
			tending_ = true;
			if (pthread_create(&tender_, NULL, &tend, NULL) != 0)
			{
				/* Without it a stream is never beaten and a peer that went
				   away is never noticed, so the streams are refused rather
				   than served silently unattended. */
				tending_ = false;
				serving_ = false;
			}
		}
		serving = serving_;
	}

	/* Only when there is something for a delivery to feed. The bus copies its
	   subscriber list on every publish, so one that can never reach a stream is
	   a copy every event pays for and nobody reads. */
	if (!serving)
		return;

	try
	{
		coreapi::EventBus::instance().subscribe(&fanout());
	}
	catch (...)
	{
		/* The list the bus keeps could not grow. Nothing would be delivered,
		   so nothing is served either: a stream that is open and silent looks
		   to a client exactly like a box where nothing is happening. */
		OpenThreads::ScopedLock<OpenThreads::Mutex> held(lock());
		serving_ = false;
	}
}

void stop()
{
	/* First, and before a single stream is touched. A delivery already walking its copy
	   of the bus's subscriber list cannot be reached by unsubscribing, so this only stops
	   the next one; everything below is safe against one already running, every stream
	   being taken down under the lock that delivery also takes. */
	coreapi::EventBus::instance().unsubscribe(&fanout());

	pthread_t t;
	bool joinable = false;
	{
		OpenThreads::ScopedLock<OpenThreads::Mutex> held(lock());
		serving_ = false;
		if (tending_)
		{
			tending_ = false;
			t = tender_;
			joinable = true;
		}
	}
	if (joinable)
		pthread_join(t, NULL);

	/* Waited for, and not up to some number of seconds. The library says that stopping a
	   daemon while a connection of it is suspended leaks the connection and the socket or
	   does something worse, and every idle stream is a suspended connection, so a wait that
	   gave up would hand the caller the one state the call it is about to make ends the
	   process on.

	   Asked again on every turn because closing a stream is not one act. A turn that finds
	   the same streams as the one before says so. */
	for (int waited = 0; ; waited += 10)
	{
		const size_t left = closeEvery();
		if (left == 0)
			return;
		if (waited > 0 && (waited % kSayWaitingMs) == 0)
			std::fprintf(stderr, "httpd: still waiting for %u event stream(s) to close\n",
			             (unsigned) left);
		sleepMs(10);
	}
}

size_t openStreams()
{
	OpenThreads::ScopedLock<OpenThreads::Mutex> held(lock());
	return streams().size();
}

size_t pendingBytes()
{
	OpenThreads::ScopedLock<OpenThreads::Mutex> held(lock());

	size_t total = 0;
	std::vector<Stream *> &list = streams();
	for (size_t i = 0; i < list.size(); ++i)
		total += list[i]->pending.size();
	return total;
}

void setHeartbeatMs(unsigned ms)
{
	OpenThreads::ScopedLock<OpenThreads::Mutex> held(lock());
	heartbeat_ms_ = (ms > 0) ? ms : 1;
}

unsigned heartbeatMs()
{
	OpenThreads::ScopedLock<OpenThreads::Mutex> held(lock());
	return heartbeat_ms_;
}

void setReapProbeForTest(ReapProbe p)
{
	OpenThreads::ScopedLock<OpenThreads::Mutex> held(lock());
	reap_probe_ = p;
}

void setPendingCeiling(size_t bytes)
{
	OpenThreads::ScopedLock<OpenThreads::Mutex> held(lock());
	pending_ceiling_ = bytes;
}

size_t pendingCeiling()
{
	OpenThreads::ScopedLock<OpenThreads::Mutex> held(lock());
	return pending_ceiling_;
}

/* Named in the namespace and not in the unnamed one beside it, because the
   table below carries its address and that table is declared where the list of
   tables is read. */
Response streamHandler(const Request &)
{
	/* Not the answer itself. What goes back from here says which media type the answer
	   is and what has to be said around it, and the transport turns the connection into
	   the stream: a handler answers with bytes and returns, and a stream is what happens
	   after a handler has returned. */
	Response out;
	out.code = StatusOk;
	out.content_type = streamContentType();

	// Both of these are about what sits between this server and the browser.
	// A cache that stored a stream would replay it, and a proxy that buffered
	// one would hold every frame until the stream ended, which it does not.
	out.headers.push_back(std::make_pair(std::string("Cache-Control"),
	                                     std::string("no-cache, no-transform")));
	out.headers.push_back(std::make_pair(std::string("X-Accel-Buffering"),
	                                     std::string("no")));
	return out;
}

} // namespace events

/* Read rather than written to, and a stream rather than a document, so it asks
   for the least any route can ask for and is reached by a caller the network
   exemption already grants that much. */
const Endpoint kEventsEndpoints[] = {
	{ Method::Get, "/api/v1/events", AuthLevel::Read,
	  "the box's own events as they happen", NULL, 0, NULL, &events::streamHandler, false },
};

/* The pair, written where the array is, so that the length beside it is the
   length of the array and not a number somebody kept in step by hand. */
extern const RouteTable eventsTable = {
	HTTPD_TABLE("events", kEventsEndpoints)
};

} // namespace httpd
