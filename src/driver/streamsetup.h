/*
 * streamsetup.h - the steps from an accepted client to a running stream
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
 *
 * The stream server takes one client at a time on its own thread. A client
 * that asks for a broadcast channel gets a frontend zapit holds for it
 * (zapit/streamhold.h) or nothing; the picture, a PiP and a recording are
 * never changed for it.
 *
 * Who owns what, on every way out:
 *  - Refused: the descriptor is still the caller's, which closes it; a hold
 *    taken on the way is dropped here.
 *  - Consumed: the descriptor went to a running stream, or to a stream
 *    object that closed it when it was discarded; the hold went with it, at
 *    the same step.
 *
 * A setup counts as under way from the moment it is let in until it is
 * published or refused, so a box that checks for streams in the meantime
 * sees one. Publishing and the end of that count happen in one hold of the
 * mutex, and so does the check that the server still runs: a stop that
 * comes first is seen and the stream not published; a stop that comes after
 * finds the stream among those it stops.
 */

#ifndef __streamsetup_h__
#define __streamsetup_h__

#include <atomic>
#include <stdint.h>

#include <OpenThreads/Mutex>

namespace streamsetup
{

class StartGate
{
public:
	StartGate(OpenThreads::Mutex &m, std::atomic<bool> &running);

	// Takes the mutex. Refuses once the server stops; otherwise one more
	// setup is under way.
	bool enter();
	// The caller holds the mutex.
	void leaveLocked();
	bool mayPublishLocked() const;
	int pendingLocked() const;

	OpenThreads::Mutex &mutex() { return m; }

private:
	StartGate(const StartGate &);
	StartGate &operator=(const StartGate &);

	OpenThreads::Mutex &m;
	std::atomic<bool> &running;
	int pending;
};

enum Kind
{
	Unknown,
	Web,
	Broadcast
};

struct Target
{
	uint64_t channel;
	Kind     kind;
	bool     e2;     // asked for in the form another kind of receiver uses

	Target() : channel(0), kind(Unknown), e2(false) {}
};

/* What a setup acts on. The stream manager implements it over itself and
   zapit; a case implements it over a model. A stream is the manager's
   object, passed through as it is. */
class Ops
{
public:
	virtual ~Ops() {}

	// Reads the request and names the channel it asks for.
	virtual bool readTarget(int fd, Target &t) = 0;
	// Under the mutex: whether a stream of the channel runs, and handing fd
	// to it.
	virtual bool runningLocked(uint64_t channel) = 0;
	virtual void joinLocked(uint64_t channel, int fd) = 0;

	virtual bool recording(uint64_t channel) = 0;
	// zapit's hold, asked with no lock held; 0 when refused.
	virtual uint32_t hold(uint64_t channel) = 0;
	virtual void *adopt(uint32_t id) = 0;
	virtual void drop(uint32_t id) = 0;
	// The pids to stream; false when there is nothing to stream.
	virtual bool preparePids(const Target &t) = 0;

	/* Under the mutex. build takes fd and the hold; discard deletes the
	   stream, which closes fd and drops the hold. */
	virtual void *buildLocked(int fd, const Target &t, void *fe, uint32_t hold_id) = 0;
	virtual bool startLocked(void *stream) = 0;
	virtual void publishLocked(void *stream) = 0;
	virtual void discardLocked(void *stream) = 0;
};

enum Result
{
	Refused,
	Consumed
};

struct Outcome
{
	Result result;
	bool   published;   // a new stream runs, not a client added to one
};

Outcome run(int fd, StartGate &gate, Ops &ops);

} // namespace streamsetup

#endif
