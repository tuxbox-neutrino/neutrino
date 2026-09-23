/*
 * livestream.h - an answer whose body is a running program's output
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

#ifndef __httpd_livestream_h__
#define __httpd_livestream_h__

#include "endpoint.h"

#include <cstddef>

/* Declared rather than included, for the reason the event stream beside this
   declares it: the only thing wanted here is a connection to hand back to the
   transport, and the file that hands it back includes the library anyway. */
struct MHD_Connection;

namespace httpd
{

namespace livestream
{

/* Starts the thread that watches the pipes of the programs this is running, and stops
   it again. Called where the daemon is started and stopped, because a session is a
   connection of that daemon.

   Idempotent, both of them. stop waits until every session is gone, for the same
   reason the event stream does: this library says that stopping a daemon while a
   connection of it is suspended leaks the connection or worse. */
void start();
void stop();

// How many programs are running for browsers right now. The number the ceiling
// is spent against, and the one thing that says a session whose reader walked
// away was let go.
size_t openSessions();

// The most that may run at once, so that a refusal can say what the limit was.
size_t maxSessions();
void   setMaxSessionsForTest(size_t n);

/* Whether this answer is one of these rather than a document. The handler puts
   the command in the answer and returns, and this is what the transport asks
   before it sends what a handler usually returns. */
bool isStream(const Response &r);

enum Opened
{
	StreamOpened,
	// The box is already running as many as it will. refusal carries the
	// document that says so.
	StreamRefused,
	// Something this server needed did not work. Nothing was started and the
	// connection is still the caller's to answer on.
	StreamFailed
};

/* Runs the command the answer carries and makes its output the body.

   The connection is taken over on StreamOpened and on nothing else, which is the same
   bargain the event stream makes: nothing else may queue an answer on it afterwards.

   The program is stopped and reaped when the answer ends, however it ends. A browser
   that closes the tab is the ordinary case and not an error. */
Opened open(struct MHD_Connection *connection, const Response &r, Response &refusal);

} // namespace livestream
} // namespace httpd

#endif
