/*
 * events.h - the event stream, and the seats that listen on it
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

#ifndef __httpd_events_h__
#define __httpd_events_h__

#include "endpoint.h"

#include "coreapi/base/eventbus.h"

#include <cstddef>
#include <string>

/* Declared rather than included, so this header costs the library's headers nothing:
   the only thing wanted from it here is a connection to hand back to the transport,
   and the file that does hand it back includes the library anyway. */
struct MHD_Connection;

namespace httpd
{

namespace events
{

/* Subscribes the one subscriber this server has and starts the thread that beats and
   reaps. Idempotent, and called where the daemon is started, a stream being a
   connection of that daemon.

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
   such a request. A handler answers with a Response like any other, which keeps the
   route in the table where it is declared and checked. What it cannot do from there
   is write to a socket that stays open, so the transport turns the connection. */
const char *streamContentType();
bool isStream(const Response &r);

/* Whether this route is one that answers with a stream, asked by the address of the
   route rather than by its path or by anything it declares.

   The tables have no field that says it: a route states what it answers with as a
   Schema, and a stream is not a document with members. Asking by address keeps the
   path and the media type in this file, so a route added below needs nothing kept in
   step. */
bool isStreamRoute(const Endpoint &ep);

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

/* Attaches a stream to this connection, carrying over the media type and the headers
   the handler wrote. Called from the access handler and from nowhere else, the library
   allowing a response to be queued for a live connection only from there. */
Opened openStream(struct MHD_Connection *connection, const Response &r, Response &refusal);

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
