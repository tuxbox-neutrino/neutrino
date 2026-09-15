/*
 * $Header: /cvs/tuxbox/apps/misc/libs/libconnection/basicsocket.cpp,v 1.2 2003/02/24 21:14:15 thegoodguy Exp $
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

#include "basicsocket.h"

#include <errno.h>
#include <stdio.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

bool deadline_left(const struct timespec * deadline, timeval & left)
{
	if (!deadline)
		return true;

	struct timespec now;
	if (clock_gettime(CLOCK_MONOTONIC, &now) != 0)
		return true;

	long long us = ((long long) deadline->tv_sec - (long long) now.tv_sec) * 1000000LL
		     + ((long long) deadline->tv_nsec - (long long) now.tv_nsec) / 1000LL;
	if (us <= 0)
		return false;

	left.tv_sec  = (time_t) (us / 1000000LL);
	left.tv_usec = (suseconds_t) (us % 1000000LL);
	return true;
}

/* The wait the caller asked for, cut down to what is left of the deadline.
   False when the moment has passed. */
static bool wait_budget(const timeval & timeout, const struct timespec * deadline, timeval & tv)
{
	tv = timeout;
	if (!deadline)
		return true;

	timeval left = timeout;
	if (!deadline_left(deadline, left))
		return false;

	if (left.tv_sec < tv.tv_sec ||
	    (left.tv_sec == tv.tv_sec && left.tv_usec < tv.tv_usec))
		tv = left;
	return true;
}

bool send_data(int fd, const void * data, const size_t size, const timeval timeout, const struct timespec * deadline)
{
	fd_set       writefds;
	timeval      tv;
	const void * buffer;
	size_t       n;
	int          rc;

	n = size;

	while (n > 0)
	{
		/* Asked once per chunk and not only before a select, because a peer
		   that keeps taking a byte at a time never makes this block and would
		   otherwise never be given up on. */
		if (!wait_budget(timeout, deadline, tv))
		{
			printf("[basicsocket] send deadline expired.\n");
			return false;
		}

		buffer = (void *)((char *)data + (size - n));
		rc = ::send(fd, buffer, n, MSG_DONTWAIT | MSG_NOSIGNAL);
		
		if (rc == -1)
		{
			perror("[basicsocket] send_data");
char * buf = (char *) data;
printf("send_data: errno %d data %X\n", errno, buf[0]);
			//if (errno == EPIPE)
			if (errno == EPIPE || errno == ESPIPE)
				return false;
			
			FD_ZERO(&writefds);
			FD_SET(fd, &writefds);
			
			rc = select(fd + 1, NULL, &writefds, NULL, &tv);
			
			if (rc == 0)
			{
				printf("[basicsocket] send timed out.\n");
				return false;
			}
			if (rc == -1)
			{
				perror("[basicsocket] send_data select");
				return false;
			}
		}
		else
			n -= rc;
	}
	return true;
}


bool receive_data(int fd, void * data, const size_t size, const timeval timeout, const struct timespec * deadline)
{
	fd_set    readfds;
	timeval   tv;
	void    * buffer;
	size_t    n;
	int       rc;

	n = size;

	while (n > 0)
	{
		if (!wait_budget(timeout, deadline, tv))
		{
			printf("[basicsocket] receive deadline expired.\n");
			return false;
		}

		FD_ZERO(&readfds);
		FD_SET(fd, &readfds);
		
		rc = select(fd + 1, &readfds, NULL, NULL, &tv);
			
		if (rc == 0)
		{
			printf("[basicsocket] receive timed out.\n");
			return false;
		}
		if (rc == -1)
		{
			perror("[basicsocket] receive_data select");
			return false;
		}
		buffer = (void *)((char *)data + (size - n));
		rc = ::recv(fd, buffer, n, MSG_DONTWAIT | MSG_NOSIGNAL);
		
		if ((rc == 0) || (rc == -1))
		{
			if (rc == -1)
			{
				perror("[basicsocket] receive_data");

				//if (errno == EPIPE)
				if (errno == EPIPE || errno == ESPIPE)
					return false;
			}
			else
			{
				/*
				 * silently return false
				 *
				 * printf("[basicsocket] no more data\n");
				 */
				return false;
			}

		}
		else
			n -= rc;
	}
	return true;
}
