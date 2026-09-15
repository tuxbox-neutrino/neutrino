/*
 * $Header: /cvs/tuxbox/apps/misc/libs/libconnection/basicclient.cpp,v 1.17 2004/04/08 07:19:00 thegoodguy Exp $
 *
 * Basic Client Class - The Tuxbox Project
 *
 * (C) 2002-2003 by thegoodguy <thegoodguy@berlios.de>
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

#include "basicclient.h"
#include "basicmessage.h"
#include "basicsocket.h"

#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <time.h>
#include <unistd.h>

#define TIMEOUT_SEC  60
#define TIMEOUT_USEC 0
#define MAX_TIMEOUT_SEC  300
#define MAX_TIMEOUT_USEC 0

/* Per thread rather than per client, for the reason on the Deadline class. Two
   plain words so that no registration is needed when a thread ends, which the
   oldest toolchain in the target set would otherwise have to provide. */
static __thread bool            t_deadline_set = false;
static __thread struct timespec t_deadline_at;

/* Raised for as long as a guard on this thread has given its turn up. The
   descriptor then belongs to the exchange that did get the turn, so every
   method below leaves it alone. */
static __thread int t_turn_denied = 0;

static bool turn_denied()
{
	return t_turn_denied != 0;
}

CBasicClient::CBasicClient()
{
	sock_fd = -1;
	request_owner = 0;
}

const struct timespec * CBasicClient::deadline()
{
	return t_deadline_set ? &t_deadline_at : 0;
}

CBasicClient::Deadline::Deadline(unsigned ms)
{
	had_outer = t_deadline_set;
	if (had_outer)
		outer = t_deadline_at;

	struct timespec at;
	/* A clock that cannot be read leaves whatever was already set. Arming a
	   moment that cannot be compared against would end every wait at once. */
	if (clock_gettime(CLOCK_MONOTONIC, &at) != 0)
		return;

	at.tv_sec  += (time_t) (ms / 1000);
	at.tv_nsec += (long) (ms % 1000) * 1000000L;
	if (at.tv_nsec >= 1000000000L)
	{
		at.tv_nsec -= 1000000000L;
		at.tv_sec  += 1;
	}

	if (had_outer && (outer.tv_sec < at.tv_sec ||
			  (outer.tv_sec == at.tv_sec && outer.tv_nsec < at.tv_nsec)))
		return;

	t_deadline_at  = at;
	t_deadline_set = true;
}

CBasicClient::Deadline::~Deadline()
{
	if (had_outer)
	{
		t_deadline_at  = outer;
		t_deadline_set = true;
	}
	else
		t_deadline_set = false;
}

/* Neither a connect on a unix socket nor a lock offers a bounded wait: a connect
   that cannot be made at once fails outright rather than reporting that it is
   under way, and the mutex has no timed form. So the only bound available to
   either is to come back until the moment runs out. Short enough not to matter
   beside a wait, long enough not to spin. */
static const long RETRY_NS = 20 * 1000 * 1000;

/* Without a deadline this is the blocking connect it has always been, which is
   what every caller in the GUI gets. */
static bool connect_until(int fd, const struct sockaddr * addr, socklen_t len,
			  const struct timespec * deadline)
{
	if (!deadline)
		return connect(fd, addr, len) == 0;

	int flags = fcntl(fd, F_GETFL, 0);
	if (flags == -1 || fcntl(fd, F_SETFL, flags | O_NONBLOCK) == -1)
		return connect(fd, addr, len) == 0;

	bool done = false;
	for (;;)
	{
		if (connect(fd, addr, len) == 0)
		{
			done = true;
			break;
		}
		int err = errno;
		if (err == EISCONN)
		{
			done = true;
			break;
		}
		if (err != EAGAIN && err != EINPROGRESS && err != EALREADY && err != EINTR)
			break;

		/* Seeded with the wait every other leg falls back on, because a clock
		   that cannot be read leaves this untouched and a zero here would turn
		   the wait below into no wait at all. */
		timeval left;
		left.tv_sec = TIMEOUT_SEC;
		left.tv_usec = TIMEOUT_USEC;
		if (!deadline_left(deadline, left))
			break;

		if (err == EINPROGRESS || err == EALREADY)
		{
			fd_set writefds;
			FD_ZERO(&writefds);
			FD_SET(fd, &writefds);
			if (select(fd + 1, NULL, &writefds, NULL, &left) <= 0)
				break;
			int pending = 0;
			socklen_t plen = sizeof(pending);
			if (getsockopt(fd, SOL_SOCKET, SO_ERROR, &pending, &plen) != 0 || pending != 0)
				break;
			done = true;
			break;
		}

		struct timespec pause;
		pause.tv_sec = 0;
		pause.tv_nsec = RETRY_NS;
		if (left.tv_sec == 0 && (long) left.tv_usec * 1000L < RETRY_NS)
			pause.tv_nsec = (long) left.tv_usec * 1000L;
		nanosleep(&pause, 0);
	}

	fcntl(fd, F_SETFL, flags);
	return done;
}

/* Without a deadline this is the blocking lock it has always been, which is what
   every caller in the GUI gets. */
static bool lock_until(OpenThreads::Mutex & m, const struct timespec * deadline)
{
	if (!deadline)
	{
		m.lock();
		return true;
	}

	for (;;)
	{
		if (m.trylock() == 0)
			return true;

		/* Seeded with the wait every other leg falls back on, because a clock
		   that cannot be read leaves this untouched and the turn is then waited
		   for as it always was. */
		timeval left;
		left.tv_sec = TIMEOUT_SEC;
		left.tv_usec = TIMEOUT_USEC;
		if (!deadline_left(deadline, left))
			return false;

		struct timespec pause;
		pause.tv_sec = 0;
		pause.tv_nsec = RETRY_NS;
		if (left.tv_sec == 0 && (long) left.tv_usec * 1000L < RETRY_NS)
			pause.tv_nsec = (long) left.tv_usec * 1000L;
		nanosleep(&pause, 0);
	}
}

CBasicClient::RequestGuard::RequestGuard(CBasicClient & c) : client(c), held(false)
{
	/* Taking the guard twice on one thread hangs the process with no output.
	   Reported unconditionally rather than through assert, because a release
	   build that defines NDEBUG is exactly where the hang is hardest to find. */
	const pthread_t owner = client.request_owner.load();
	if (owner && pthread_equal(owner, pthread_self()))
	{
		printf("[CBasicClient] re-entrant request guard: %s\n", client.getSocketName());
		fflush(stdout);
		abort();
	}

	held = lock_until(client.request_mutex, deadline());
	if (!held)
	{
		t_turn_denied++;
		return;
	}
	client.request_owner.store(pthread_self());
}

CBasicClient::RequestGuard::~RequestGuard()
{
	if (!held)
	{
		t_turn_denied--;
		return;
	}

	client.request_owner.store(0);
	client.request_mutex.unlock();
}

bool CBasicClient::open_connection()
{
	if (turn_denied())
		return false;

	close_connection();

	struct sockaddr_un servaddr;
	int clilen;

	memset(&servaddr, 0, sizeof(struct sockaddr_un));
	servaddr.sun_family = AF_UNIX;
	strcpy(servaddr.sun_path, getSocketName());              // no length check !!!
	clilen = sizeof(servaddr.sun_family) + strlen(servaddr.sun_path);

	if ((sock_fd = socket(AF_UNIX, SOCK_STREAM, 0)) < 0)
	{
		printf("[CBasicClient] socket failed.\n");
		perror(getSocketName());
		sock_fd = -1;
		return false;
	}

	if (!connect_until(sock_fd, (struct sockaddr*) &servaddr, clilen, deadline()))
	{
		printf("[CBasicClient] connect failed.\n");
		perror(getSocketName());
		close_connection();
		return false;
	}
	return true;
}

void CBasicClient::close_connection()
{
	if (turn_denied())
		return;

	if (sock_fd != -1)
	{
		close(sock_fd);
		sock_fd = -1;
	}
}

bool CBasicClient::send_data(const char* data, const size_t size)
{
	timeval timeout;

	if (turn_denied())
		return false;

	if (sock_fd == -1)
		return false;
	
	timeout.tv_sec  = TIMEOUT_SEC;
	timeout.tv_usec = TIMEOUT_USEC;
	
	if (::send_data(sock_fd, data, size, timeout, deadline()) == false)
	{
		printf("[CBasicClient] send failed: %s\n", getSocketName());
		close_connection();
		return false;
	}
	return true;
}

bool CBasicClient::send_string(const char* data)
{
	uint8_t send_length;
	size_t length = strlen(data);
	if (length > 255)
	{
		printf("[CBasicClient] string too long - sending only first 255 characters: %s\n", data);
		send_length = 255;
	}
	else
	{
		send_length = static_cast<uint8_t>(length);
	}
	return (send_data((char *)&send_length, sizeof(send_length)) &&
		send_data(data, send_length));
}

bool CBasicClient::receive_data(char* data, const size_t size, bool use_max_timeout)
{
	timeval timeout;

	if (turn_denied())
		return false;

	if (sock_fd == -1)
		return false;

	if (use_max_timeout)
	{
		timeout.tv_sec  = MAX_TIMEOUT_SEC;
		timeout.tv_usec = MAX_TIMEOUT_USEC;
	}
	else
	{
		timeout.tv_sec  = TIMEOUT_SEC;
		timeout.tv_usec = TIMEOUT_USEC;
	}

	if (::receive_data(sock_fd, data, size, timeout, deadline()) == false)
	{
		printf("[CBasicClient] receive failed: %s\n", getSocketName());
		close_connection();
		return false;
	}
	return true;
}

bool CBasicClient::send(const unsigned char command, const char* data, const unsigned int size)
{
	CBasicMessage::Header msgHead;
	msgHead.version = getVersion();
	msgHead.cmd     = command;

	open_connection(); // if the return value is false, the next send_data call will return false, too

	if (!send_data((char*)&msgHead, sizeof(msgHead)))
	    return false;
	
	if (size != 0)
	    return send_data(data, size);

	return true;
}

