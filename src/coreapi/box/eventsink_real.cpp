/*
 * eventsink_real.cpp - event sink fed by the running event server
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

#include "coreapi/base/deps.h"

#include <map>

#include <driver/neutrino_msg_t.h>
#include <eventserver.h>

namespace coreapi
{

namespace
{

// Its own translation unit for the reason the command sink has one: the event
// server is all it needs.
//
// The server is derived from privately rather than held, because two of its
// members are needed and neither is public: the table the registration fills,
// which is the only thing that tells an event this sink carries from one it does
// not, and the per client send, which is the only one of the two sends that
// answers whether it got through.
class RealEventSink : public EventSink, private CEventServer
{
	public:
		void install()
		{
			// Walked rather than listed, so that what is registered is what
			// can be sent and the two cannot come apart.
			for (size_t i = 0; i < (size_t) BoxEvent::Count; i++)
				registerEvent2(eventNumber((BoxEvent) i),
					       CEventServer::INITID_HTTPD,
					       currentEventSocketPath());
		}

		Status send(unsigned event_id, const void *body, size_t size)
		{
			std::map<unsigned int, eventClientMap>::const_iterator it =
				eventData.find(event_id);
			if (it == eventData.end() || it->second.empty())
				return Status::NotSupported;

			/* The initiator picks which table the loop reads the number
			   through, so it is part of the command.

			   The clients are walked here rather than through sendEvent, which
			   drops what each send answered: a loop that cannot be reached is
			   the difference between a command that went out and one that did
			   not. Ok means the whole command reached every registered loop's
			   socket inside the transport's ceiling, not that any of them has
			   read it. Walking the table directly rather than copying it first
			   is safe because install writes it once. */
			bool sent = true;
			for (eventClientMap::const_iterator c = it->second.begin();
			     c != it->second.end(); ++c)
			{
				if (!sendEvent2Client(event_id, CEventServer::INITID_HTTPD,
						      &c->second, body, (unsigned int) size))
					sent = false;
			}
			return sent ? Status::Ok : Status::Internal;
		}
};

RealEventSink g_real_event_sink;

} // anonymous namespace

void installRealEventSink()
{
	g_real_event_sink.install();
	setEventSink(&g_real_event_sink);
}

} // namespace coreapi
