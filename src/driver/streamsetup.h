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
 * A client is answered with its status before any picture byte: 200 when
 * the stream is about to start, 404 for a channel the box does not have, 503
 * when no frontend may be had for it. A request that is not one - not a GET
 * of a path, or not complete in time - is closed without an answer.
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
#include <string>

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

/* Reading the request.

   The request line, at most 511 bytes up to and including its line end, all
   of it within budget_ms. A peer that closes before the line end leaves what
   came. Then whatever else is already there - the header a player sends - is
   taken without waiting, at most 8 KiB, and let go; a client that sends the
   line alone is not waited for. False when nothing came, or the time ran out
   before the line end. */
bool readRequest(int fd, unsigned budget_ms, std::string &line);

enum Form
{
	Garbage,    // not a GET of a path: closed without an answer
	NoTarget,   // "/" or "/?...": the channel the box shows
	ById,       // ".../id=<hex>..."
	E2,         // the service reference form another kind of receiver uses
	BadTarget   // a path that names no channel: 404
};

struct Request
{
	Form     form;
	uint64_t id;       // ById: the channel; E2: the 48 bits of it the form carries
	uint32_t satpos;   // E2: the position the form carries

	Request() : form(Garbage), id(0), satpos(0) {}
};

Request parseRequestLine(const std::string &line);

/* Answering it. */

// The whole head, and for a refusal its one-line reason, as it goes out.
std::string statusBytes(int status, const char *reason);

/* Writes the head of a stream about to start, within 2 seconds. False when
   it did not go out whole; the client then gets nothing more. */
bool writeOk(int fd);

/* Writes a refusal within 2 seconds, says nothing more, and reads what the
   client still sends for at most half a second, so the refusal is not lost
   to a reset. The caller closes fd. */
void refuse(int fd, int status, const char *reason);

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

	enum Read
	{
		ReadOk,
		ReadGarbage,   // no answer
		ReadUnknown,   // 404 no such channel
		ReadNoLive     // 404 no live channel: none asked for, and none shown
	};
	// Reads the request and names the channel it asks for.
	virtual Read readTarget(int fd, Target &t) = 0;
	virtual bool writeOk(int fd) = 0;
	virtual void refuse(int fd, int status, const char *reason) = 0;
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
