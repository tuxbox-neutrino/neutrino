/*
 * eventbus.cpp - the bus that carries box events to whoever listens
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

#include "eventbus.h"

#include <algorithm>
#include <cstdio>

#include <OpenThreads/ScopedLock>

namespace coreapi
{

namespace
{
/* The publish site is reached from many nesting levels of the main loop, so a
   subscriber that publishes must not re-enter delivery. Per thread, because a
   publish from another thread is not a nested one and has to get through. */
thread_local bool in_delivery = false;

// An object, so the flag is lowered on every way out of delivery.
struct DeliveryGuard
{
	DeliveryGuard() { in_delivery = true; }
	~DeliveryGuard() { in_delivery = false; }
};
} // namespace

Subscriber::~Subscriber()
{
	EventBus::instance().unsubscribe(this);
}

EventBus &EventBus::instance()
{
	/* Never destroyed: a subscriber or another static may still publish while
	   the exit handlers run. */
	static EventBus *bus = new EventBus;
	return *bus;
}

void EventBus::subscribe(Subscriber *s)
{
	OpenThreads::ScopedLock<OpenThreads::Mutex> lock(mutex);
	if (std::find(subs.begin(), subs.end(), s) == subs.end())
		subs.push_back(s);
}

void EventBus::unsubscribe(Subscriber *s)
{
	OpenThreads::ScopedLock<OpenThreads::Mutex> lock(mutex);
	subs.erase(std::remove(subs.begin(), subs.end(), s), subs.end());
}

void EventBus::publish(const Event &e)
{
	if (in_delivery)
		return;

	std::vector<Subscriber *> copy;
	{
		OpenThreads::ScopedLock<OpenThreads::Mutex> lock(mutex);
		copy = subs;
	}

	DeliveryGuard guard;

	/* Walked from a copy and with the lock given up, so a subscriber may leave
	   the list from inside its own onEvent. */
	for (size_t i = 0; i < copy.size(); i++)
	{
		/* The main loop this is published from is built without exceptions and
		   carries no handler above it, so an escaping one would end the
		   process rather than reach anybody. */
		try { copy[i]->onEvent(e); }
		catch (...)
		{
			/* the event is gone either way, and without a line here the symptom
			   is one subscriber falling silent now and then */
			std::fputs("coreapi: subscriber threw, event dropped\n", stderr);
		}
	}
}

} // namespace coreapi
