/*
 * eventbus.h - the bus that carries box events to whoever listens
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

#ifndef __coreapi_eventbus_h__
#define __coreapi_eventbus_h__

#include <stdint.h>
#include <string>
#include <vector>

#include <OpenThreads/Mutex>

namespace coreapi
{

enum class EventType
{
	Zap, Mode, Standby, Volume, Mute,
	RecordStart, RecordStop,
	TimerChanged, EpgUpdated,
	BouquetsChanged
};

// A value type by construction: the message payloads this is fed from are
// freed by the main loop as soon as the publish call returns.
struct Event
{
	EventType   type;
	/* Not the same width on every type: a Zap carries the whole channel id,
	   an EpgUpdated only its low 48 bits, because the schedule is kept under
	   the service key alone. Comparing the two needs the mask. */
	uint64_t    channel_id;
	int         value;
	std::string text;

	Event() : type(EventType::Zap), channel_id(0), value(0) {}
};

class Subscriber
{
	public:
		/* Leaves the bus, so a registration cannot outlive the subscriber that
		   owns it, but it cannot reach a delivery that is already walking its
		   copy of the list: see publish for when destroying one is safe.
		   Virtual because owners hold subscribers by this type. */
		virtual ~Subscriber();

		/* Runs on the publishing thread, usually the one that also serves the
		   remote control and the picture but not only: the movie player feeds
		   the same message path from a hint thread it starts for every file it
		   plays, so this has to be thread-safe.

		   It arrives before the message is acted on, so a Mode event is ahead
		   of the mode changing. React to what the event carries; going to read
		   the state gets the old one.

		   It has to return at once: no socket, no disk, no nested message loop,
		   no throw. A blocking dialog is a nested message loop and holds this
		   call open for as long as it stands, and every event that arrives in
		   that window is dropped. An exception that escapes is swallowed and
		   costs the subscribers behind this one their copy of the event. */
		virtual void onEvent(const Event &e) = 0;
};

class EventBus
{
	public:
		static EventBus &instance();

		void subscribe(Subscriber *s);
		void unsubscribe(Subscriber *s);

		/* Delivers before it returns, so it is only ever as quick as the
		   slowest onEvent. It holds no lock while delivering, so a subscriber
		   may subscribe or unsubscribe from inside its own onEvent. Three rules
		   the destructor above cannot cover, all because the list is copied
		   before delivery starts: a subscriber must not be destroyed from
		   inside an onEvent, nor on any other thread while a delivery may be
		   running, and one that another subscriber removes during a delivery
		   still receives that event. */
		void publish(const Event &e);

	private:
		EventBus() {}

		OpenThreads::Mutex        mutex;
		std::vector<Subscriber *> subs;
};

} // namespace coreapi

#endif
