/*
 * livestream.h - an answer whose body is a running program's output
 *
 * Copyright (C) 2026 NI-Team
 * Copyright (C) 2026 Tuxbox-Project (carrier, child setup, supervision)
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

#include "carry.h"
#include "endpoint.h"

#include <cstddef>

namespace httpd
{

namespace livestream
{

/* What carries a program's output (carry::Program): an answer that names a
   command in Response::stream_argv runs it on a job of its own and passes
   what it writes on as the body.

   check, which a HEAD is answered by too, refuses what the job would refuse
   before it starts anything: a process that cannot supervise a child (500
   no-supervision) and a box without the program (501). The seats and a
   server that is stopping are the transport's.

   run starts the program with a clean signal state and nothing open but
   its output, waits up to 15 seconds for its first bytes, and refuses with
   503 upstream-unreachable when it ends or stays silent before them. From
   the first byte on it passes on what the program writes, in blocks of up
   to 32 KiB, until the program ends, the reader leaves or the server stops;
   then the program and everything it started are ended and collected. */
const carry::Carrier *carrier();

/* Whether ep is the route whose answers this carries. Those can meet the
   refusals the transport gives itself, and the document states them for it
   by this. */
bool carriedRoute(const Endpoint &ep);

/* Whether this process can supervise a child now: one that ignores SIGCHLD
   or has the kernel discard what its children exit with is never told when
   one ends, and the number of an ended child is free for the system to hand
   to somebody else at once. False fills refusal. */
bool supervisable(Response &refusal);

// ---- seams for the suite; nothing in the product sets them

/* Runs path in place of the program an answer names, with the arguments it
   names. NULL runs what the answer names. */
void setConverterForTest(const char *path);
const char *converterForTest();

// How long the first byte may take; 0 puts the shipped 15 seconds back.
void setFirstByteMsForTest(unsigned ms);

} // namespace livestream
} // namespace httpd

#endif
