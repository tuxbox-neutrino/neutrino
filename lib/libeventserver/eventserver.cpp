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

// How many events the outbox holds for clients that were busy, all clients
// together, and how long its thread rests after a turn that delivered
// nothing.
const size_t kOutboxMax = 256;
const useconds_t kOutboxRestUs = 100 * 1000;

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

void (*CEventServer::deliverProbeForTest)(const char *udsName) = NULL;
void (*CEventServer::outboxProbeForTest)(const char *udsName) = NULL;

CEventServer::CEventServer()
	: outbox_seq(0), outbox_count(0), outbox_running(false), outbox_stopping(false),
	  registrations(0)
{
	pthread_mutex_init(&outbox_lock, NULL);
	pthread_mutex_init(&data_lock, NULL);
}

/* The daemons keep their server for the life of the process; a server that
   goes away first stops its outbox thread and waits until it is gone. What is
   still in the outbox then is lost with the server. The wait ends: the thread
   looks for the stop before every client, and one send is bounded by
   EVENT_SEND_TIMEOUT_MS. It has no bound of its own, because the thread uses
   the server's maps and locks, and nothing of the server may go while it runs. */
CEventServer::~CEventServer()
{
	pthread_mutex_lock(&outbox_lock);
	outbox_stopping = true;
	pthread_mutex_unlock(&outbox_lock);

	bool running = true;
	while (running)
	{
		pthread_mutex_lock(&outbox_lock);
		running = outbox_running;
		pthread_mutex_unlock(&outbox_lock);
		if (running)
			usleep(10 * 1000);
	}
	pthread_mutex_destroy(&outbox_lock);
	pthread_mutex_destroy(&data_lock);
}

/* Registering again under the same socket changes nothing, and what waits for
   that socket stays. Under another socket it is a new registration: what
   waited for the old one goes, to reach neither the client, which listens
   elsewhere now, nor whatever listens on the old socket next. */
void CEventServer::registerEvent2(const unsigned int eventID, const unsigned int ClientID, const std::string &udsName)
{
	char name[sizeof(eventClient::udsName)];
	snprintf(name, sizeof(name), "%s", udsName.c_str());

	std::string left;
	pthread_mutex_lock(&data_lock);
	eventClientMap &clients = eventData[eventID];
	eventClientMap::iterator it = clients.find(ClientID);
	if (it == clients.end() || strcmp(it->second.udsName, name) != 0)
	{
		if (it != clients.end())
			left = it->second.udsName;
		eventClient &client = clients[ClientID];
		snprintf(client.udsName, sizeof(client.udsName), "%s", name);
		client.generation = ++registrations;
	}
	pthread_mutex_unlock(&data_lock);

	if (!left.empty())
		dropQueued(left, eventID, ClientID);
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
	std::string left;
	pthread_mutex_lock(&data_lock);
	std::map<unsigned int, eventClientMap>::iterator it = eventData.find(eventID);
	if (it != eventData.end())
	{
		eventClientMap::iterator c = it->second.find(ClientID);
		if (c != it->second.end())
		{
			left = c->second.udsName;
			it->second.erase(c);
		}
	}
	pthread_mutex_unlock(&data_lock);

	/* What waited for it goes with it and frees its place in the outbox.
	   An attempt the outbox thread has already started is not called back:
	   that one event may still arrive, as a direct send that was under way
	   does. */
	if (!left.empty())
		dropQueued(left, eventID, ClientID);
}

void CEventServer::unRegisterEvent(const int fd)
{
	commandUnRegisterEvent msg;
	ssize_t ignored __attribute__((unused)) = read(fd, &msg, sizeof(msg));
	unRegisterEvent2(msg.eventID, msg.clientID);
}

void CEventServer::sendEvent(const unsigned int eventID, const initiators initiatorID, const void *eventbody, const unsigned int eventbodysize)
{
	// a copy, taken under the lock, and nothing inserted for an event
	// nobody registered for
	eventClientMap notifyClients;
	pthread_mutex_lock(&data_lock);
	std::map<unsigned int, eventClientMap>::const_iterator it = eventData.find(eventID);
	if (it != eventData.end())
		notifyClients = it->second;
	pthread_mutex_unlock(&data_lock);

	for (eventClientMap::iterator pos = notifyClients.begin(); pos != notifyClients.end(); ++pos)
	{
		//allen clients ein event schicken
		const std::string name(pos->second.udsName);

		/* Behind what already waits for this client, and behind a send to it
		   another thread has started, so it keeps the order: that send may
		   yet come back busy and its event go into the outbox, in front. */
		pthread_mutex_lock(&outbox_lock);
		const bool behind = outbox.find(name) != outbox.end() || outbox_inflight.count(name) != 0;
		if (behind)
		{
			// queued in the same step that found the queue: let go in
			// between, the thread could empty it and a later sender find
			// the client free
			const bool kept = enqueueLocked(name, eventID, initiatorID, eventbody, eventbodysize,
							pos->first, pos->second.generation, false);
			pthread_mutex_unlock(&outbox_lock);
			if (!kept)
				reportDrop(name.c_str());
			continue;
		}
		outbox_inflight.insert(name);
		pthread_mutex_unlock(&outbox_lock);

		const sendResult r = deliver(eventID, initiatorID, name.c_str(), eventbody, eventbodysize);
		if (deliverProbeForTest != NULL)
			deliverProbeForTest(name.c_str());
		// The mark goes and a busy event goes in front in one step, before
		// any later sender can find the client free.
		pthread_mutex_lock(&outbox_lock);
		outbox_inflight.erase(name);
		const bool kept = (r != BUSY) || enqueueLocked(name, eventID, initiatorID, eventbody, eventbodysize,
								pos->first, pos->second.generation, true);
		pthread_mutex_unlock(&outbox_lock);
		if (r == GONE || !kept)
			reportDrop(name.c_str());
	}
}

size_t CEventServer::pendingEvents()
{
	pthread_mutex_lock(&outbox_lock);
	const size_t n = outbox_count;
	pthread_mutex_unlock(&outbox_lock);
	return n;
}

bool CEventServer::enqueueLocked(const std::string &udsName, const unsigned int eventID, const initiators initiatorID, const void *eventbody, const unsigned int eventbodysize, const unsigned int clientID, const unsigned int generation, bool in_front)
{
	bool kept = false;
	if (outbox_count < kOutboxMax && !outbox_stopping)
	{
		pendingEvent e;
		e.eventID = eventID;
		e.initiatorID = initiatorID;
		e.clientID = clientID;
		e.generation = generation;
		e.seq = ++outbox_seq;
		if (eventbody != NULL && eventbodysize != 0)
			e.body.assign((const char *) eventbody, eventbodysize);
		if (in_front)
			outbox[udsName].push_front(e);
		else
			outbox[udsName].push_back(e);
		outbox_count++;
		kept = true;
	}
	// Asked on every event, kept or not: a thread that could not be started
	// before is tried again, a full outbox included, which only it can empty.
	startOutboxLocked();
	return kept;
}

void CEventServer::startOutboxLocked()
{
	if (outbox_running || outbox_stopping || outbox_count == 0)
		return;
	pthread_t t;
	pthread_attr_t attr;
	pthread_attr_init(&attr);
	pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
	// without a thread the events wait for the next send to try again
	if (pthread_create(&t, &attr, &CEventServer::outboxThread, this) == 0)
		outbox_running = true;
	pthread_attr_destroy(&attr);
}

/* Walks the clients that have something waiting, one event each per turn,
   so a client that is still busy holds up no other. An event leaves its
   queue when it was delivered, when its client is gone or when the
   registration it was queued for is; a busy client keeps it for the next
   turn. The thread ends when the outbox is empty and
   starts again with the next event queued. */
void *CEventServer::outboxThread(void *arg)
{
	CEventServer *self = (CEventServer *) arg;

	for (;;)
	{
		std::deque<std::string> names;
		pthread_mutex_lock(&self->outbox_lock);
		if (self->outbox_stopping || self->outbox.empty())
		{
			self->outbox_running = false;
			pthread_mutex_unlock(&self->outbox_lock);
			return NULL;
		}
		for (outboxMap::const_iterator q = self->outbox.begin(); q != self->outbox.end(); ++q)
			names.push_back(q->first);
		pthread_mutex_unlock(&self->outbox_lock);

		bool delivered = false;
		for (size_t i = 0; i < names.size(); i++)
		{
			// the front it copies is taken out by its number afterwards: a
			// registration that went meanwhile may have taken it already,
			// and then nothing else goes in its place
			pendingEvent e;
			pthread_mutex_lock(&self->outbox_lock);
			/* Asked before every client and not only between turns: one
			   turn past enough busy clients outlasts the destructor's wait,
			   and the server must not go away under a send. */
			if (self->outbox_stopping)
			{
				self->outbox_running = false;
				pthread_mutex_unlock(&self->outbox_lock);
				return NULL;
			}
			outboxMap::iterator q = self->outbox.find(names[i]);
			// not while a sender is delivering to the same client directly:
			// its event is older than everything queued behind it
			const bool have = (q != self->outbox.end() && !q->second.empty())
				&& self->outbox_inflight.count(names[i]) == 0;
			if (have)
				e = q->second.front();
			pthread_mutex_unlock(&self->outbox_lock);
			if (!have)
				continue;

			/* Asked before every try. dropQueued takes what waits when a
			   registration goes, but a sender that read the registration
			   just before can still queue an event for it after that. */
			if (outboxProbeForTest != NULL)
				outboxProbeForTest(names[i].c_str());
			if (!self->stillRegistered(e))
			{
				pthread_mutex_lock(&self->outbox_lock);
				self->removeQueuedLocked(names[i], e.seq);
				pthread_mutex_unlock(&self->outbox_lock);
				continue;
			}

			const sendResult r = self->deliver(e.eventID, e.initiatorID, names[i].c_str(),
							   e.body.empty() ? NULL : e.body.data(),
							   (unsigned int) e.body.size());
			if (r == BUSY)
				continue;

			pthread_mutex_lock(&self->outbox_lock);
			self->removeQueuedLocked(names[i], e.seq);
			pthread_mutex_unlock(&self->outbox_lock);

			if (r == GONE)
				reportDrop(names[i].c_str());
			else
				delivered = true;
		}
		if (!delivered)
			usleep(kOutboxRestUs);
	}
}

void CEventServer::dropQueued(const std::string &udsName, const unsigned int eventID, const unsigned int clientID)
{
	pthread_mutex_lock(&outbox_lock);
	outboxMap::iterator q = outbox.find(udsName);
	if (q != outbox.end())
	{
		std::deque<pendingEvent> &events = q->second;
		for (std::deque<pendingEvent>::iterator e = events.begin(); e != events.end(); )
		{
			if (e->eventID == eventID && e->clientID == clientID)
			{
				e = events.erase(e);
				outbox_count--;
			}
			else
				++e;
		}
		if (events.empty())
			outbox.erase(q);
	}
	pthread_mutex_unlock(&outbox_lock);
}

bool CEventServer::stillRegistered(const pendingEvent &e)
{
	bool registered = false;
	pthread_mutex_lock(&data_lock);
	std::map<unsigned int, eventClientMap>::const_iterator it = eventData.find(e.eventID);
	if (it != eventData.end())
	{
		eventClientMap::const_iterator c = it->second.find(e.clientID);
		registered = c != it->second.end() && c->second.generation == e.generation;
	}
	pthread_mutex_unlock(&data_lock);
	return registered;
}

void CEventServer::removeQueuedLocked(const std::string &udsName, unsigned long seq)
{
	outboxMap::iterator q = outbox.find(udsName);
	if (q == outbox.end())
		return;
	std::deque<pendingEvent> &events = q->second;
	for (std::deque<pendingEvent>::iterator e = events.begin(); e != events.end(); ++e)
	{
		if (e->seq == seq)
		{
			events.erase(e);
			outbox_count--;
			break;
		}
	}
	if (events.empty())
		outbox.erase(q);
}


/* An event is a notification, and the client at the other end reads it on the
   thread it does everything else on. So one send waits for that client only as
   long as EVENT_SEND_TIMEOUT_MS, and the sender keeps its thread. sendEvent puts
   an event the client could not take in time into the outbox, from where it is
   delivered later; a caller of this one gets the answer and decides itself.

   Every wait is against one deadline taken at the top, so a client cannot spend
   the budget once for the connection and again for each write.

   What the answer means: true when the whole header and the whole body reached
   the client's socket, false for anything else. It does not mean the client has
   read them. */
bool CEventServer::sendEvent2Client(const unsigned int eventID, const initiators initiatorID, const eventClient *ClientData, const void *eventbody, const unsigned int eventbodysize)
{
	const sendResult r = deliver(eventID, initiatorID, ClientData->udsName, eventbody, eventbodysize);
	if (r != SENT)
		reportDrop(ClientData->udsName);
	return r == SENT;
}

/* One try inside the send's deadline. BUSY is a client that did not take the
   event in time, its backlog full or its reading slow; GONE is one that is not
   there at all, whose socket file is missing or refuses. */
CEventServer::sendResult CEventServer::deliver(const unsigned int eventID, const initiators initiatorID, const char *udsName, const void *eventbody, const unsigned int eventbodysize)
{
	struct sockaddr_un servaddr;
	int clilen, sock_fd;

	memset(&servaddr, 0, sizeof(struct sockaddr_un));
	servaddr.sun_family = AF_UNIX;
	snprintf(servaddr.sun_path, sizeof(servaddr.sun_path), "%s", udsName);
	clilen = sizeof(servaddr.sun_family) + strlen(servaddr.sun_path);

	if ((sock_fd = socket(AF_UNIX, SOCK_STREAM, 0)) < 0)
	{
		perror("[eventserver]: socket");
		return BUSY;
	}

	// Set before the connect, because a client whose backlog is full is the
	// case the loop below has to be able to come back to: a blocking connect
	// would park in the kernel with no deadline and nothing to come back for.
	const int flags = fcntl(sock_fd, F_GETFL, 0);
	if (flags < 0 || fcntl(sock_fd, F_SETFL, flags | O_NONBLOCK) < 0)
	{
		perror("[eventserver]: nonblock");
		close(sock_fd);
		return BUSY;
	}

	const int64_t start = nowMs();
	if (start < 0)
	{
		close(sock_fd);
		return BUSY;
	}
	const int64_t deadline = start + EVENT_SEND_TIMEOUT_MS;

	if (!connectBy(sock_fd, (struct sockaddr *) &servaddr, clilen, deadline))
	{
		const int err = errno;
		close(sock_fd);
		errno = err;
		if (err == ENOENT || err == ECONNREFUSED || err == ENOTDIR || err == EACCES)
			return GONE;
		return BUSY;
	}

	eventHead head;
	head.eventID = eventID;
	head.initiatorID = initiatorID;
	head.dataSize = eventbodysize;

	bool sent = writeAll(sock_fd, &head, sizeof(head), deadline);
	if (sent && eventbodysize != 0)
		sent = writeAll(sock_fd, eventbody, eventbodysize, deadline);

	close(sock_fd);
	return sent ? SENT : BUSY;
}
