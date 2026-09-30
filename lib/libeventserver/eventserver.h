/*

        $Header: /cvs/tuxbox/apps/misc/libs/libeventserver/eventserver.h,v 1.14 2004/05/06 15:06:30 thegoodguy Exp $

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

#ifndef __libevent__
#define __libevent__

#include <stdint.h>

#include <string>
#include <map>
#include <set>
#include <deque>

#include <pthread.h>

/* The longest one send holds its sender, connection and both writes together.
   The sender keeps its thread; what a client has not taken by then waits in the
   outbox (see sendEvent). Declared here because a caller cannot otherwise know
   what sending an event can cost it. */
#define EVENT_SEND_TIMEOUT_MS 300

/* The largest body a receiver reads. Every event this tree sends is a few
   hundred bytes at most; the bound is for a header whose size no event could
   have, which is then not read at all. */
#define EVENT_BODY_MAX (1024 * 1024)


class CEventServer
{
	public:

		enum initiators
		{
			INITID_CONTROLD,
			INITID_SECTIONSD,
			INITID_ZAPIT,
			INITID_TIMERD,
			INITID_HTTPD,
			INITID_NEUTRINO,
			INITID_GENERIC_INPUT_EVENT_PROVIDER
		};


		struct commandRegisterEvent
		{
			unsigned int eventID;
			unsigned int clientID;
			char udsName[50];
		};

		struct commandUnRegisterEvent
		{
			unsigned int eventID;
			unsigned int clientID;
		};

		struct eventHead
		{
			unsigned int eventID;
			unsigned int initiatorID;
			unsigned int dataSize;
		};

		CEventServer();
		~CEventServer();

		void registerEvent2(const unsigned int eventID, const unsigned int ClientID, const std::string &udsName);
		void registerEvent(const int fd);
		void unRegisterEvent2(const unsigned int eventID, const unsigned int ClientID);
		void unRegisterEvent(const int fd);
		/* Hands the event to every client registered for it. A client that
		   cannot take it inside EVENT_SEND_TIMEOUT_MS, a busy loop behind a
		   full backlog, gets it later: the event waits in this server's
		   outbox and a thread of its own delivers it, in the order sent,
		   once the client takes connections again. The sender waits at most
		   one bounded send per registered client. An event is lost only
		   when its client is gone or the outbox is full; one that waits goes
		   with its registration when the client unregisters or moves to
		   another socket, unless a try to deliver it is already under way. */
		void sendEvent(const unsigned int eventID, const initiators initiatorID, const void *eventbody = NULL, const unsigned int eventbodysize = 0);
		// How many events wait in the outbox.
		size_t pendingEvents();

		/* Called right after a direct send came back, before its answer is
		   acted on; for the tests, which need a second sender in exactly
		   that gap. NULL everywhere else. */
		static void (*deliverProbeForTest)(const char *udsName);
		/* Called by the outbox thread with an event in hand, before it asks
		   whether the registration the event waits for still holds; for the
		   tests, which need an unregister or a move in exactly that gap.
		   NULL everywhere else. */
		static void (*outboxProbeForTest)(const char *udsName);

	protected:

		struct eventClient
		{
			char udsName[50];
			// tells this registration from a later one of the same client,
			// which an event queued for this one must not reach
			unsigned int generation;
		};

		//key: ClientID                                              // Map is a Sorted Associative Container
		typedef std::map<unsigned int, eventClient> eventClientMap;  // -> clients with lower ClientID receive events first

		//key: eventID
		std::map<unsigned int, eventClientMap> eventData;

		bool sendEvent2Client(const unsigned int eventID, const initiators initiatorID, const eventClient *ClientData, const void *eventbody = NULL, const unsigned int eventbodysize = 0);

	private:
		enum sendResult { SENT, BUSY, GONE };
		sendResult deliver(const unsigned int eventID, const initiators initiatorID, const char *udsName, const void *eventbody, const unsigned int eventbodysize);

		struct pendingEvent
		{
			unsigned int eventID;
			initiators initiatorID;
			std::string body;
			// the registration it waits for: one that is gone or has moved
			// to another socket does not get it
			unsigned int clientID;
			unsigned int generation;
			// which one it is, so taking it out takes out no other
			unsigned long seq;
		};
		// per client socket, in the order the events were sent
		typedef std::map<std::string, std::deque<pendingEvent> > outboxMap;
		outboxMap outbox;
		unsigned long outbox_seq;
		// clients a sender is delivering to directly right now: an event for
		// one of them queues behind, so a newer one cannot overtake an older
		// one that is about to be found busy
		std::set<std::string> outbox_inflight;
		size_t outbox_count;
		bool outbox_running;
		bool outbox_stopping;
		pthread_mutex_t outbox_lock;
		// eventData is written by the thread that serves registrations and
		// read by every thread that sends
		pthread_mutex_t data_lock;
		// numbers the registrations, under data_lock
		unsigned int registrations;

		// with outbox_lock held; false when the event could not be kept
		bool enqueueLocked(const std::string &udsName, const unsigned int eventID, const initiators initiatorID, const void *eventbody, const unsigned int eventbodysize, const unsigned int clientID, const unsigned int generation, bool in_front);
		// takes what waits on udsName for a registration that has gone
		void dropQueued(const std::string &udsName, const unsigned int eventID, const unsigned int clientID);
		// takes data_lock; whether the registration the event waits for is
		// still the one there
		bool stillRegistered(const pendingEvent &e);
		// with outbox_lock held; takes out the one event of that number
		void removeQueuedLocked(const std::string &udsName, unsigned long seq);
		void startOutboxLocked();
		static void *outboxThread(void *arg);

		CEventServer(const CEventServer &);
		CEventServer &operator=(const CEventServer &);
};


#endif
