/*
 * rcfixture.h - fixture for the remote key tests
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

#ifndef __test_rcfixture_h__
#define __test_rcfixture_h__

#include "catch.hpp"

#include <neutrinoMessages.h>
#include <driver/rcinput.h>

#include <errno.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#include <cstring>

/* CRCInput's constructor unlinks and rebinds whatever currentEventSocketPath() names,
 * which would cut a running instance off from the clients that reach it through that
 * path if this checked, and later bound, the shipped constant itself.
 *
 * Asked without blocking, because a listener whose backlog is full leaves a blocking
 * connect waiting for as long as it takes somebody to accept. A full backlog is still
 * an answer: somebody is there. */
inline bool eventSocketInUse()
{
	int s = socket(AF_UNIX, SOCK_STREAM, 0);
	if (s < 0)
		return true;

	// Added to the flags rather than written over them, and checked: a socket
	// left blocking here is the wait this whole function exists to avoid, and
	// it would not say so.
	const int flags = fcntl(s, F_GETFL, 0);
	if (flags < 0 || fcntl(s, F_SETFL, flags | O_NONBLOCK) < 0)
	{
		close(s);
		return true;
	}

	struct sockaddr_un addr;
	memset(&addr, 0, sizeof(addr));
	addr.sun_family = AF_UNIX;
	strncpy(addr.sun_path, currentEventSocketPath(), sizeof(addr.sun_path) - 1);

	errno = 0;
	const bool answered = connect(s, (struct sockaddr *) &addr, sizeof(addr)) == 0 ||
			      errno == EAGAIN || errno == EINPROGRESS;
	close(s);
	return answered;
}

/* Waited for and then required, never skipped. A case that returns early still
   counts as passed, so a held socket would take a fifth of these assertions out
   of a run that says nothing about it. Waited for first, because whatever holds
   the socket is usually on its way out. */
inline void requireFreeEventSocket()
{
	for (int i = 0; i < 50 && eventSocketInUse(); i++)
		usleep(100 * 1000);

	INFO("another process holds " << currentEventSocketPath()
	     << ", and taking it away from it is what this waits to avoid");
	REQUIRE_FALSE(eventSocketInUse());
}

// Nothing drains either queue here, so posting until it refuses fills it.
inline long fillQueue(CRCInput &rc, bool priority)
{
	long posted = 0;
	while (rc.postMsg(NeutrinoMessages::SHOW_INFOBAR, 0, priority) && ++posted < 1000000)
		;
	return posted;
}

#endif
