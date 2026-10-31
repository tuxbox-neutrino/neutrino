/*
 * streamsetup.cpp - the steps from an accepted client to a running stream
 *
 * Copyright (C) 2026 Thilo Graf
 *
 * License: GPL-2.0-or-later
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
 */

#include "streamsetup.h"

#include <OpenThreads/ScopedLock>

namespace streamsetup
{

StartGate::StartGate(OpenThreads::Mutex &mx, std::atomic<bool> &r)
	: m(mx), running(r), pending(0)
{
}

bool StartGate::enter()
{
	OpenThreads::ScopedLock<OpenThreads::Mutex> l(m);
	if (!running.load())
		return false;
	pending++;
	return true;
}

void StartGate::leaveLocked()
{
	pending--;
}

bool StartGate::mayPublishLocked() const
{
	return running.load();
}

int StartGate::pendingLocked() const
{
	return pending;
}

namespace
{

/* One setup's place in the count: left exactly once, in the publishing hold
   of the mutex or, on any other way out, here. */
class Pending
{
public:
	explicit Pending(StartGate &g) : gate(g), counted(true) {}
	~Pending()
	{
		if (!counted)
			return;
		OpenThreads::ScopedLock<OpenThreads::Mutex> l(gate.mutex());
		gate.leaveLocked();
	}
	void leaveLocked()
	{
		counted = false;
		gate.leaveLocked();
	}

private:
	Pending(const Pending &);
	Pending &operator=(const Pending &);
	StartGate &gate;
	bool counted;
};

/* The hold zapit answered, until a stream takes it: dropped on every way out
   that does not hand it over. */
class HoldRef
{
public:
	HoldRef(Ops &o, uint32_t i) : ops(o), held(i) {}
	~HoldRef()
	{
		if (held)
			ops.drop(held);
	}
	uint32_t id() const { return held; }
	uint32_t release()
	{
		const uint32_t i = held;
		held = 0;
		return i;
	}

private:
	HoldRef(const HoldRef &);
	HoldRef &operator=(const HoldRef &);
	Ops &ops;
	uint32_t held;
};

Outcome outcome(Result r, bool published)
{
	Outcome o;
	o.result = r;
	o.published = published;
	return o;
}

/* Under the mutex: builds the stream, which takes fd and the hold together,
   and publishes it if it starts. Consumed either way: a stream that did not
   start is discarded and has closed fd. */
Outcome publish(Ops &ops, Pending &pending, int fd, const Target &t, void *fe, uint32_t hold_id)
{
	void *stream = ops.buildLocked(fd, t, fe, hold_id);
	if (ops.startLocked(stream))
	{
		ops.publishLocked(stream);
		pending.leaveLocked();
		return outcome(Consumed, true);
	}
	ops.discardLocked(stream);
	return outcome(Consumed, false);
}

} // namespace

Outcome run(int fd, StartGate &gate, Ops &ops)
{
	if (!gate.enter())
		return outcome(Refused, false);
	Pending pending(gate);

	Target t;
	if (!ops.readTarget(fd, t))
		return outcome(Refused, false);

	{
		OpenThreads::ScopedLock<OpenThreads::Mutex> l(gate.mutex());
		if (ops.runningLocked(t.channel))
		{
			ops.joinLocked(t.channel, fd);
			return outcome(Consumed, false);
		}
	}

	if (t.kind == Unknown)
		return outcome(Refused, false);

	if (t.kind == Web)
	{
		OpenThreads::ScopedLock<OpenThreads::Mutex> l(gate.mutex());
		if (!gate.mayPublishLocked())
			return outcome(Refused, false);
		return publish(ops, pending, fd, t, 0, 0);
	}

	if (ops.recording(t.channel))
		return outcome(Refused, false);

	HoldRef hold(ops, ops.hold(t.channel));
	if (!hold.id())
		return outcome(Refused, false);
	void *fe = ops.adopt(hold.id());
	if (!fe)
		return outcome(Refused, false);
	if (!ops.preparePids(t))
		return outcome(Refused, false);

	OpenThreads::ScopedLock<OpenThreads::Mutex> l(gate.mutex());
	if (!gate.mayPublishLocked())
		return outcome(Refused, false);
	return publish(ops, pending, fd, t, fe, hold.release());
}

} // namespace streamsetup
