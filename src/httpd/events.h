/*
 * events.h - the event stream, and the seats that listen on it
 *
 * Copyright (C) 2026 NI-Team
 * Copyright (C) 2026 Tuxbox-Project (adaptation)
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

#ifndef __httpd_events_h__
#define __httpd_events_h__

#include "endpoint.h"

#include "coreapi/base/eventbus.h"

#include <cstddef>
#include <string>

/* DELIBERATE DRIFT from the source's events.h (see events.cpp's head): nhttpd
   carries a stream on a thread of its own (carry.h), so what a caller hands
   over is the socket's descriptor and a write function, not a library
   connection. Everything else in this header is as written. */

namespace httpd
{

namespace events
{

/* Subscribes the one subscriber this server has and starts the thread that beats and
   reaps, and installs what the transport carries a stream with (carry::Events).
   Idempotent, and called where the daemon is started, a stream being a connection of
   that daemon.

   The ceiling on how many streams may be open at once is read here, out of the
   configuration in effect, and not per request: a ceiling read on a request thread
   would be a second reader of it. */
void start();

/* Unsubscribes and then closes every stream, in that order and not the other: a
   delivery already walking its copy of the bus's subscriber list cannot be reached by
   unsubscribing, so the streams it may still be appending to have to outlive it.

   Waits for the streams to be gone before it answers, the library saying that
   stopping a daemon while a connection of it is suspended leaks the connection or
   worse. Idempotent. */
void stop();

// How many streams are open. The one number that says a stream nobody is
// reading was let go, which nothing else about the process shows.
size_t openStreams();

/* How much this server is holding for peers that have not taken it yet, over every
   stream. The companion to the count above: what a stream costs while nothing is
   wrong is a connection, and what it costs while a peer has gone quiet is memory,
   and neither is visible in a process that looks healthy until it runs out. */
size_t pendingBytes();

/* The name one event type is carried under, which is the SSE event field a browser
   dispatches on. NULL for a value that is not one of the declared types, which the
   switch behind this cannot be reached with while the compiler holds it to the
   enumeration. */
const char *typeName(coreapi::EventType t);

/* One event as the JSON body of its frame, appended to what the caller already holds.

   Every member every time and never a shape that depends on the type: what each of
   them means does depend on the type, but a client reading a member that is absent
   for the type it arrived with would have to know which types carry which before it
   can read any of them.

   The identifier is written as hexadecimal text and not as a number, being sixty four
   bits wide where the numbers a browser parses out of JSON carry fifty three. */
void appendEventJson(std::string &out, const coreapi::Event &e);

/* The media type a route asks for a stream with, and the test for an answer that is
   such a request: a 200 marked as the stream (Response::event_stream). A handler
   answers with a Response like any other, which keeps the route in the table where it
   is declared and checked. What it cannot do from there is write to a socket that
   stays open, so the transport carries the stream on a thread of its own (carry.h). */
const char *streamContentType();
bool isStream(const Response &r);

/* Whether this route is one that answers with a stream, asked by the address of the
   route rather than by its path or by anything it declares.

   The tables have no field that says it: a route states what it answers with as a
   Schema, and a stream is not a document with members. Asking by address keeps the
   path and the media type in this file, so a route added below needs nothing kept in
   step. */
bool isStreamRoute(const Endpoint &ep);

const char *streamsFullDetail();

// What became of an attempt to turn a connection into a stream.
enum Opened
{
	// Queued. The connection belongs to this file until it closes.
	StreamOpened,
	/* The server is already carrying as many streams as it will, and refusal carries the
	   answer that says so. Never a queue: a caller told to wait is a caller that can come
	   back, and a caller silently held is a connection this box pays for. */
	StreamRefused,
	// The library would not take the response, which is what every other
	// failure to queue one is.
	StreamFailed
};

/* The registration a caller holds between opening a stream and pumping it.
   Opaque: what is behind it belongs to this file's lock. */
typedef void *StreamHandle;

/* One block of the stream, handed to the caller's socket. Answers false when
   the peer did not take it, which ends the stream. Called with no lock of
   this file's held. */
typedef bool (*StreamWrite)(void *ctx, const char *bytes, size_t n);

/* Registers a stream on this descriptor: the seat is counted and the opening
   frames are seeded before this answers, so a caller that writes its header
   next cannot lose an event published in between. On StreamOpened the handle
   is the caller's to spend, on exactly one of two calls: pumpStream(), or
   closeStream() for a stream whose header never made it out. The descriptor
   itself stays the caller's; it is named here only so a reaped stream can be
   shut down under a blocked send.

   placed says the caller holds a place the transport reserved for this stream
   before its job was started (carry.h, Carrier::reserve), which the stream then
   takes on or, refused or failed, gives back; on a throw it is still the
   caller's. A caller without one is held to the ceiling here, counting such
   places.

   session is the cookie token of the session that let the request in
   (Response::event_session), and empty where a token or the address did: the
   stream ends with that session, signed out, run out, or ended or given new
   tokens by a change of the login, and carries nothing more once it stands no
   longer. A stream without one keeps its own rules. */
Opened openStream(int fd, StreamHandle &out, Response &refusal, bool placed = false,
                  const std::string &session = std::string());

/* Runs the stream on the calling thread until it ends: waits beside the
   lock, draws what is pending in blocks, and hands each block to write. Ends
   on a refused write, over the ceiling, when the stream is closed, or when the
   session it was opened under stands no longer, asked before every block and
   at least once a second while there is none; gives the registration back on
   every way out. */
void pumpStream(StreamHandle h, StreamWrite write, void *ctx);

// Gives back a registration that was never pumped.
void closeStream(StreamHandle h);

/* How long one stream may be silent before a comment frame is sent down it, measured
   per stream from whatever last went into it and not from a clock the whole server
   shares.

   Two jobs and not one. A proxy or a NAT box drops a stream that says nothing for
   somewhere between thirty and sixty seconds, and this keeps it alive; and a
   suspended connection is one the library does not poll, so a peer that went away is
   not noticed until something is written to it. So the interval is also the longest
   a stream nobody is reading is counted for.

   Settable because a case that waited out the shipped interval would add that many
   seconds to every run. */
void     setHeartbeatMs(unsigned ms);
unsigned heartbeatMs();

/* Run on the thread that reaps a stream, at the moment that stream's socket is about
   to be shut down under it, and installed by nothing in the product.

   What a case observes from in here is the one property that makes naming that
   descriptor safe at all: this file's lock is held. The call that gives a stream back
   waits on that lock and the library closes the socket behind it, so a descriptor
   named while it is held cannot have been reissued to the next connection the box
   accepts and shut down under that one. Cleared by passing NULL. */
typedef void (*ReapProbe)(void);
void setReapProbeForTest(ReapProbe p);

/* The most one stream may be holding for a peer that is not reading it.

   A peer behind a NAT box that vanishes leaves a socket that reports no error and
   takes no data, and the events keep arriving: without a ceiling that is a leak with
   a network trigger. Over it the stream is dropped rather than trimmed, because what
   a client would have been handed after a gap is a picture of the box that is wrong
   in a way it cannot see. */
void   setPendingCeiling(size_t bytes);
size_t pendingCeiling();

} // namespace events

/* The table this module states, as the pair rather than as an array and a count, so
   the file which owns the list of tables can name it without anything having to run
   before main: a count declared in one unit and read in another is not a constant the
   linker can write. */
extern const RouteTable eventsTable;

} // namespace httpd

#endif
