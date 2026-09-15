/*
 * server.h - the HTTP server and its lifetime
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

#ifndef __httpd_server_h__
#define __httpd_server_h__

#include <cstddef>
#include <string>

namespace httpd
{

struct ServerConfig
{
	int         port;
	// An address literal of either family. A v6 literal binds v6 only, so one
	// of these cannot serve both families at once and the default is the v4
	// wildcard.
	std::string bind_address;   // "0.0.0.0", or "::" for every v6 address
	unsigned    thread_pool;    // 4
	unsigned    connection_limit;
	unsigned    connection_timeout_s;
	/* The most of a request body this server keeps, and past it the request is
	   refused.

	   What this costs is paid only for a caller the answer is being built for. A body
	   is kept only where the route that will answer declares a value carried in one
	   and the caller may reach that route; every other request has its body read off
	   the connection and dropped, counted and never stored.

	   What the box can be holding for bodies at once is therefore this times the
	   number of connections held by callers whose requests are being answered, rather
	   than times connection_limit. The timeout below does not bound the wider figure:
	   it is an idle timeout, and a caller writing one byte a minute never reaches
	   it. */
	size_t      max_body_bytes; // 1u << 20
};

ServerConfig defaultConfig();

// False when the port cannot be bound, because the box may already be running
// something there and a server that cannot listen is not a reason to end the program
// that was going to keep working without it. Also false for a port outside the range
// the protocol has and for an address that is not a literal of either family, so a
// number or a name out of a settings file cannot quietly produce a server somewhere
// nobody asked for.
//
// And false while the program has not installed what the reads behind these routes go
// through: those accessors end the process instead of answering, so a server that
// started without them would take the program down on its first request.
bool start(const ServerConfig &c);

// Idempotent, and safe to call while another thread is in start or stop: the
// two serialise, so shutdown paths that overlap do not have to agree on which
// of them owns the daemon.
void stop();

bool running();

// The port actually bound, which is not the port asked for when the request was
// zero.
int boundPort();

/* How many requests the server is holding state for. The daemon may abandon a
   connection without calling the handler a last time, so the state one request needs
   is freed from the notification the daemon makes when it is done and never on the way
   out of a handler. That is a mechanism which fails silently: nothing about a request
   that leaked its state looks any different, until the box runs out of memory after a
   long enough day. Read after the daemon is stopped. */
size_t openRequestsForTest();

/* How many bytes of request bodies this server has taken into memory since it was
   started, which is the difference between reading a body and paying for one.

   Here because that difference is invisible from a socket: a caller refused a route it
   may not reach is answered the same words whether the megabyte it sent was kept until
   the answer or dropped as it arrived. What a case can watch on its own is memory, and
   a case that watches memory is one that passes on a machine with enough of it.

   Counted and never uncounted, so it is a total and not a level. */
size_t bodyBytesKeptForTest();

} // namespace httpd

#endif
