/*

        $Header: /cvs/tuxbox/apps/misc/libs/libeventserver/eventserver.cpp,v 1.12 2003/03/14 06:25:49 obi Exp $

	Event-Server  -   DBoxII-Project

	Copyright (C) 2001 Steffen Hehn 'McClean'
	Homepage: http://dbox.cyberphoria.org/

	License: GPL

	This program is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation; either version 2 of the License, or
	(at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program; if not, write to the Free Software
	Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/

#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <poll.h>
#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <time.h>
#include <unistd.h>

#include "eventserver.h"

namespace
{

// How long a connect that the kernel refused to queue waits before asking
// again, and how often a drop is reported however many there were.
const int64_t kRetryMs = 5;
const int64_t kDropReportMs = 5000;

// Negative when the clock cannot be read, which every caller below treats as
// time already spent, so an unreadable clock cuts a send short instead of
// removing its bound.
int64_t nowMs()
{
	struct timespec t;
	if (clock_gettime(CLOCK_MONOTONIC, &t) != 0)
		return -1;
	return (int64_t) t.tv_sec * 1000 + (int64_t) (t.tv_nsec / 1000000);
}

// Waits until the socket can be used for what is about to be done with it, or
// says the deadline has gone. One deadline covers the whole send, so a client
// cannot spend the budget once per step.
bool waitReady(int fd, short events, int64_t deadline)
{
	for (;;)
	{
		const int64_t now = nowMs();
		if (now < 0)
			return false;
		const int64_t left = deadline - now;
		if (left <= 0)
			return false;

		struct pollfd p;
		p.fd = fd;
		p.events = events;
		p.revents = 0;
		const int r = poll(&p, 1, (int) left);
		if (r > 0)
			return (p.revents & (POLLERR | POLLHUP | POLLNVAL)) == 0;
		if (r == 0)
			return false;
		if (errno != EINTR)
			return false;
	}
}

// True only when every byte went out. A stream that stops half way through a
// header or a body cannot be taken back: the reader is left with less than the
// header says and drops it, and false is what has the sender keep the event and
// send it again whole.
bool writeAll(int fd, const void *data, size_t size, int64_t deadline)
{
	const char *p = (const char *) data;
	size_t done = 0;
	while (done < size)
	{
		// before every write and not only once the buffer is full: a run of
		// short writes must not carry a send past its bound either
		const int64_t now = nowMs();
		if (now < 0 || now >= deadline)
			return false;
		const ssize_t n = write(fd, p + done, size - done);
		if (n > 0)
		{
			done += (size_t) n;
			continue;
		}
		if (n < 0 && errno == EINTR)
			continue;
		if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK))
		{
			if (!waitReady(fd, POLLOUT, deadline))
				return false;
			continue;
		}
		return false;
	}
	return true;
}

// A unix socket whose listener has a full backlog answers a non-blocking connect
// with EAGAIN and queues nothing, so there is no connection to wait on and the
// call itself is what has to be made again. A local socket that is merely slow
// to accept answers EINPROGRESS, and that one is waited on. Both are the state
// this exists to survive; anything else is the client not being there.
bool connectBy(int fd, const struct sockaddr *addr, socklen_t len, int64_t deadline)
{
	for (;;)
	{
		if (connect(fd, addr, len) == 0)
			return true;
		if (errno == EISCONN)
			return true;
		// EINTR falls through to the deadline below with everything else, so
		// that a stream of signals cannot keep this here past the budget.
		if (errno == EINPROGRESS || errno == EALREADY)
		{
			int err = 0;
			socklen_t errlen = sizeof(err);
			if (!waitReady(fd, POLLOUT, deadline))
				return false;
			if (getsockopt(fd, SOL_SOCKET, SO_ERROR, &err, &errlen) != 0)
				return false;
			if (err == 0)
				return true;
			if (err != EAGAIN)
				return false;
			// fall through to the wait below and ask again
		}
		else if (errno != EAGAIN && errno != EINTR)
		{
			return false;
		}

		// Nothing to poll on: the kernel took no connection, so this waits out
		// a slice of the budget and asks again rather than sleeping on a
		// descriptor that will never become ready.
		const int64_t now = nowMs();
		if (now < 0 || deadline - now <= 0)
			return false;
		const int64_t left = deadline - now;
		struct timespec ts;
		ts.tv_sec = 0;
		ts.tv_nsec = (left < kRetryMs ? left : kRetryMs) * 1000000;
		nanosleep(&ts, NULL);
	}
}

/* A drop is one line, and the condition that produces them produces them in
   bursts, so the line is rate limited.

   One slot per client rather than one for the server, because a client dropping
   steadily would otherwise silence the single drop of a client beside it, and
   which client is dropping is the whole of what the line is for. The table is
   fixed and small: the clients are the registered initiators, and a name that
   finds no slot shares the last one rather than allocating, which costs that
   name a cadence and never a line.

   The count is the running total and is not reset, so a line always carries
   what came before it and nothing is lost by a burst ending between two of
   them. What is not reported is the drops after the last line until the next
   one arrives; there is no timer here to flush them.

   Every sending thread and the outbox thread count here, so the table is only
   touched under drop_lock: a count two threads raised at once lost one of
   them, and a slot a name was being written into could be read half written.
   The line itself is written after letting go. The seconds are enough because
   the interval is measured in them. */
struct DropCount
{
	// The same width the client name is registered with.
	char     name[50];
	unsigned last_reported;   // monotonic seconds, 0 for never
	unsigned dropped;
};

pthread_mutex_t drop_lock = PTHREAD_MUTEX_INITIALIZER;

// with drop_lock held
DropCount &slotFor(const char *udsName)
{
	static DropCount slots[8];

	size_t free_slot = 0;
	for (size_t i = 0; i < sizeof(slots) / sizeof(slots[0]); i++)
	{
		if (slots[i].name[0] == '\0')
		{
			free_slot = i;
			break;
		}
		if (strncmp(slots[i].name, udsName, sizeof(slots[i].name) - 1) == 0)
			return slots[i];
		free_slot = i;
	}
	// snprintf both bounds the copy and guarantees the terminator in one call,
	// so there is no separate write whose omission would leave the buffer
	// unterminated if this were ever refactored.
	snprintf(slots[free_slot].name, sizeof(slots[free_slot].name), "%s", udsName);
	return slots[free_slot];
}

void reportDrop(const char *udsName)
{
	const int saved = errno;
	const int64_t ms = nowMs();
	const unsigned now = (ms < 0) ? 0 : (unsigned)(ms / 1000);

	pthread_mutex_lock(&drop_lock);
	DropCount &slot = slotFor(udsName);
	slot.dropped++;
	if (now != 0 && slot.last_reported != 0 &&
	    now - slot.last_reported < (unsigned)(kDropReportMs / 1000))
	{
		pthread_mutex_unlock(&drop_lock);
		errno = saved;
		return;
	}
	slot.last_reported = (now == 0) ? 1 : now;
	const unsigned dropped = slot.dropped;
	pthread_mutex_unlock(&drop_lock);

	fprintf(stderr, "[eventserver]: dropped %u event(s) so far for %s: %s\n",
		dropped, udsName, strerror(saved));
	errno = saved;
}

} // anonymous namespace

void CEventServer::registerEvent2(const unsigned int eventID, const unsigned int ClientID, const std::string &udsName)
{
	strcpy(eventData[eventID][ClientID].udsName, udsName.c_str());
}

void CEventServer::registerEvent(const int fd)
{
	commandRegisterEvent msg;

	int readresult = read(fd, &msg, sizeof(msg));
	if (readresult <= 0)
		perror("[eventserver]: read");
//	printf("[eventserver]: read from %d %x bytes  %d/%d\n", fd, errno, readresult,  sizeof(msg));
//	printf("[eventserver]: registered event (%d) to: %d - %s\n", msg.eventID, msg.clientID, msg.udsName);
	registerEvent2(msg.eventID, msg.clientID, msg.udsName);
}

void CEventServer::unRegisterEvent2(const unsigned int eventID, const unsigned int ClientID)
{
	eventData[eventID].erase(ClientID);
}

void CEventServer::unRegisterEvent(const int fd)
{
	commandUnRegisterEvent msg;
	ssize_t ignored __attribute__((unused)) = read(fd, &msg, sizeof(msg));
	unRegisterEvent2(msg.eventID, msg.clientID);
}

void CEventServer::sendEvent(const unsigned int eventID, const initiators initiatorID, const void *eventbody, const unsigned int eventbodysize)
{
	eventClientMap notifyClients = eventData[eventID];

	for (eventClientMap::iterator pos = notifyClients.begin(); pos != notifyClients.end(); ++pos)
	{
		//allen clients ein event schicken
		eventClient client = pos->second;
		sendEvent2Client(eventID, initiatorID, &client, eventbody, eventbodysize);
	}
}


/* An event is a notification, and the client at the other end reads it on the
   thread it does everything else on. So this waits for that client only as long
   as EVENT_SEND_TIMEOUT_MS, and a client that cannot take the event inside that
   loses the event. The sender keeps its thread, which is the trade: one missed
   notification against a daemon stopped for as long as a client is busy.

   Every wait is against one deadline taken at the top, so a client cannot spend
   the budget once for the connection and again for each write.

   What the answer means: true when the whole header and the whole body reached
   the client's socket, false for anything else. It does not mean the client has
   read them. */
bool CEventServer::sendEvent2Client(const unsigned int eventID, const initiators initiatorID, const eventClient *ClientData, const void *eventbody, const unsigned int eventbodysize)
{
	struct sockaddr_un servaddr;
	int clilen, sock_fd;

	memset(&servaddr, 0, sizeof(struct sockaddr_un));
	servaddr.sun_family = AF_UNIX;
	strcpy(servaddr.sun_path, ClientData->udsName);
	clilen = sizeof(servaddr.sun_family) + strlen(servaddr.sun_path);

	if ((sock_fd = socket(AF_UNIX, SOCK_STREAM, 0)) < 0)
	{
		perror("[eventserver]: socket");
		return false;
	}

	// Set before the connect, because a client whose backlog is full is the
	// case the loop below has to be able to come back to: a blocking connect
	// would park in the kernel with no deadline and nothing to come back for.
	const int flags = fcntl(sock_fd, F_GETFL, 0);
	if (flags < 0 || fcntl(sock_fd, F_SETFL, flags | O_NONBLOCK) < 0)
	{
		perror("[eventserver]: nonblock");
		close(sock_fd);
		return false;
	}

	const int64_t start = nowMs();
	if (start < 0)
	{
		close(sock_fd);
		return false;
	}
	const int64_t deadline = start + EVENT_SEND_TIMEOUT_MS;

	if (!connectBy(sock_fd, (struct sockaddr *) &servaddr, clilen, deadline))
	{
		reportDrop(ClientData->udsName);
		close(sock_fd);
		return false;
	}

	eventHead head;
	head.eventID = eventID;
	head.initiatorID = initiatorID;
	head.dataSize = eventbodysize;

	bool sent = writeAll(sock_fd, &head, sizeof(head), deadline);
	if (sent && eventbodysize != 0)
		sent = writeAll(sock_fd, eventbody, eventbodysize, deadline);

	close(sock_fd);
	return sent;
}
