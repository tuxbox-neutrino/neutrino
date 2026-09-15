/*
 * $Header: /cvs/tuxbox/apps/misc/libs/libconnection/basicsocket.h,v 1.2 2003/02/24 21:14:15 thegoodguy Exp $
 *
 * Basic Socket Class - The Tuxbox Project
 *
 * (C) 2003 by thegoodguy <thegoodguy@berlios.de>
 *
 * License: GPL
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
 *
 */

#ifndef __basicsocket__
#define __basicsocket__

#include <sys/time.h>
#include <time.h>
#include <unistd.h>

/* A deadline is an absolute moment on CLOCK_MONOTONIC past which no wait may go
   on. It exists because the timeout beside it bounds one wait and not one
   exchange: both calls below wait once per chunk, so a peer that delivers a
   byte just inside every timeout holds the caller for as long as it likes.
   A null deadline is what every caller passed before this existed and leaves
   the old behaviour exactly as it was. */

/* What is left of the deadline, as a span a select may wait for. False when
   nothing is left, which is the caller's cue to give up rather than wait once
   more. A clock that cannot be read answers true with the span untouched, so a
   failing clock costs the ceiling rather than the exchange. */
bool deadline_left(const struct timespec * deadline, timeval & left);

bool send_data(int fd, const void * data, const size_t size, const timeval timeout, const struct timespec * deadline = 0);
bool receive_data(int fd, void * data, const size_t size, const timeval timeout, const struct timespec * deadline = 0);

#endif
