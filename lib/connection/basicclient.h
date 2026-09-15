/*
 * $Header: /cvs/tuxbox/apps/misc/libs/libconnection/basicclient.h,v 1.8 2009/02/24 19:09:06 seife Exp $
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

#ifndef __basicclient__
#define __basicclient__

#include <malloc.h>
#include <pthread.h>
#include <sys/types.h>
#include <time.h>

#include <atomic>

#include <OpenThreads/Mutex>

class CBasicClient
{
 private:
	int sock_fd;

 protected:
	virtual unsigned char   getVersion   () const = 0;
	virtual const    char * getSocketName() const = 0;

	/* The moment every wait below is cut off at, or null while this thread has
	   set none. */
	static const struct timespec * deadline();

	bool open_connection();
	bool send_data(const char * data, const size_t size);
	bool send_string(const char * data);
	bool receive_data(char* data, const size_t size, bool use_max_timeout = false);
	bool send(const unsigned char command, const char* data = NULL, const unsigned int size = 0);
	void close_connection();
	
	CBasicClient();

	/* There is one descriptor per instance and send() closes it before it
	   reopens, so two callers would trade descriptors mid exchange. */
	OpenThreads::Mutex request_mutex;

	/* Only ever written by the thread holding request_mutex, and read by any
	   thread before it takes it, to tell a second guard on its own thread.
	   Atomic, because a plain read of a word another thread writes is a race
	   however the word is stored. Zero means free, no thread id is zero. */
	std::atomic<pthread_t> request_owner;

 public:
	/* Bounds every wait this thread makes through any client of this
	   transport, the turn in front of it and the connect included, until it
	   goes out of scope. Without one a wait keeps the fixed timeout it has
	   always had, which bounds one wait and not one exchange: a list read is
	   one wait per timer, and a peer that answers just inside every wait is
	   never given up on. Nothing changes for a caller that sets none, which is
	   every caller in the GUI.

	   The moment is held per thread and not per client, because one client is
	   shared between the message loop and the web threads and a deadline one of
	   them set must not end an exchange the other started. An inner one may
	   only shorten: a call inside a bounded call cannot outlive its bound. */
	class Deadline
	{
	 public:
		explicit Deadline(unsigned ms);
		~Deadline();

	 private:
		Deadline(const Deadline &);
		Deadline & operator=(const Deadline &);

		bool            had_outer;
		struct timespec outer;
	};

	/* Held across one request and its response. Not recursive: a guarded
	   method must not call another guarded method.

	   A guard whose thread has a deadline gives the turn up when the moment
	   passes rather than queueing behind an exchange with no ceiling of its
	   own. Every guarded method then answers the way it answers a peer that
	   cannot be reached, and touches no descriptor, because the one this
	   client holds belongs to whoever did get the turn. */
	class RequestGuard
	{
	 public:
		RequestGuard(CBasicClient & client);
		~RequestGuard();

	 private:
		RequestGuard(const RequestGuard &);
		RequestGuard & operator=(const RequestGuard &);

		CBasicClient & client;
		bool           held;
	};
};

#endif
