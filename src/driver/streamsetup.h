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
 *
 * Stopping the streams on a frontend, or of a channel, works the same way
 * for a setup under way: the stop marks the frontend or the channel as
 * taken back in the hold of the mutex in which it takes the streams out. A
 * setup let in before that mark is not published and answered 503, as when
 * no frontend was free; one let in after it is not affected, and a stream
 * published before it is among those stopped. A stop does not wait for a
 * setup: one may be waiting for the recording that stops it.
 */

#ifndef __streamsetup_h__
#define __streamsetup_h__

#include <atomic>
#include <map>
#include <pthread.h>
#include <stdint.h>
#include <string>

#include <OpenThreads/Mutex>

namespace streamsetup
{

class StartGate
{
public:
	StartGate(OpenThreads::Mutex &m, std::atomic<bool> &running);

	enum Publish
	{
		Ok,
		Stopping,    // the server stops: closed without an answer
		Cancelled    // its frontend or its channel was taken back since
	};

	// Takes the mutex. Refuses once the server stops; otherwise one more
	// setup is under way, and since is the moment it was let in.
	bool enter(uint64_t &since);
	// The caller holds the mutex.
	void leaveLocked();
	/* Whether a setup let in at since may publish its stream of channel on
	   frontend number fe (-1 for none). */
	Publish mayPublishLocked(uint64_t since, int fe, uint64_t channel) const;
	/* Takes back frontend number fe, or channel, for every setup let in
	   before this. */
	void cancelFrontendLocked(int fe);
	void cancelChannelLocked(uint64_t channel);
	/* Whether streams on any frontend were taken back since a setup was let in
	   at since; a setup that has no frontend yet cannot say which it would get. */
	bool frontendCancelledSinceLocked(uint64_t since) const;
	int pendingLocked() const;

	OpenThreads::Mutex &mutex() { return m; }

private:
	StartGate(const StartGate &);
	StartGate &operator=(const StartGate &);

	OpenThreads::Mutex &m;
	std::atomic<bool> &running;
	int pending;
	/* Counts the take-backs; each key remembers the last that named it.
	   A setup let in at since is affected by a later one only, so the
	   keys are forgotten whenever no setup is under way. */
	uint64_t epoch;
	std::map<int, uint64_t> fe_cancelled;
	std::map<uint64_t, uint64_t> channel_cancelled;
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

/* Asks for a send buffer of bytes on fd. Every client of a stream gets it,
   the one that starts the stream and each one that joins it: the reader
   does not wait for a client, and what does not fit in the buffer is not
   sent to that client. */
void setSendBuffer(int fd, int bytes);

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
	/* zapit's hold, asked with no lock held: its number, 0 when refused,
	   and the frontend's number. since is when the setup was let in, which
	   a switch of the picture for it is held to. */
	struct Hold
	{
		uint32_t id;
		int      fenumber;
	};
	virtual Hold hold(int fd, uint64_t channel, uint64_t since) = 0;
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
	/* zapit was asked to hold a frontend, and may have tuned it, whether
	   or not the hold was granted. */
	bool   asked_hold;

	Outcome() : result(Refused), published(false), asked_hold(false) {}
};

Outcome run(int fd, StartGate &gate, Ops &ops);

/* Whether the server announces that no stream runs any more after a setup:
   it published nothing, and no stream runs and no other setup is under
   way. A stream refused or not started is announced like one that ended,
   so the EPG scan that paused for its setup goes on. The announcement
   carries asked_hold: a frontend the setup had tuned may be the one the
   scan reads from, and no stream start was announced for it. */
bool idleAfter(const Outcome &o, bool streams_empty, int pending);

/* Switching the picture for a stream that found no frontend, where the
   setting allows it.

   What the GUI and the setup know of the box for it: the GUI decides on its own
   thread just before it asks zapit, the setup decides the same without
   zap_pending before it asks the GUI at all. */
struct SwitchFacts
{
	bool tv_mode;        // the box shows live television from a tuner
	bool radio_mode;     // or live radio from a tuner
	bool radio_channel;  // the stream's channel is a radio channel
	bool zap_pending;    // a zap the GUI sent has not been answered yet
	bool recording;      // a recording or a timeshift runs, of any channel
	/* The GUI would ask for the parental PIN before it showed the channel:
	   the prompt is on and the channel's lock differs from the default. */
	bool pin_needed;
	/* The same for the channel the picture shows, or that channel is not
	   known: a zap back from a failed switch would ask for the PIN again. */
	bool shown_pin_needed;
	// The GUI holds the picture back, for a PIN refused or being asked for.
	bool picture_held;

	SwitchFacts() : tv_mode(false), radio_mode(false), radio_channel(false), zap_pending(false), recording(false),
			pin_needed(false), shown_pin_needed(false), picture_held(false) {}
};

/* Whether the GUI asks for the PIN before it shows a channel, by the rule the
   channel list zaps by: never with the prompt off, else where the channel's
   lock differs from the default. */
bool pinNeeded(bool prompt_never, bool channel_locked, bool default_locked);

/* Live television for a television channel or live radio for a radio
   channel, no zap under way, nothing recording, and no PIN to ask for,
   neither for the channel nor for the picture it would replace, which
   plays: a recording of any channel could be starting or need its tuner
   while the picture moves, an automatic timeshift is left to the GUI's own
   zap to end, a stream never puts a locked channel on the screen, and a
   picture held back for the PIN stays dark on the next channel too. */
bool switchEligible(const SwitchFacts &s);

/* Whether the client on fd has hung up or the descriptor failed, asked
   without waiting. A client that sends more is not counted as gone. */
bool peerGone(int fd);

/* A setup's hold where the picture may be switched for it, as the stream
   manager runs it over zapit, the GUI and the client. */
class HoldOps
{
public:
	virtual ~HoldOps() {}

	struct Answer
	{
		int      verdict;       // zapit's: 0 granted, 1 busy, 2 not found, 3 failed
		uint32_t hold_id;
		int      fenumber;
		bool     switch_frees;
		Answer() : verdict(3), hold_id(0), fenumber(-1), switch_frees(false) {}
	};
	virtual int64_t nowMs() = 0;
	// The box would let a stream of the channel switch the picture (switchEligible).
	virtual bool eligible() = 0;
	virtual bool settingOn() = 0;
	// zapit's hold until the deadline; false when no answer came.
	virtual bool ask(int64_t until, bool ask_switch, Answer &a) = 0;
	virtual bool clientGone() = 0;
	// One switch at a time.
	virtual void lockSwitch() = 0;
	virtual void unlockSwitch() = 0;
	// Asks the GUI to switch the picture and waits for its answer (SwitchOutcome).
	virtual int requestSwitch() = 0;
	virtual void pause(unsigned ms) = 0;
};

/* The hold; where none is free, a switch would free one, the setting allows
   it and the client is still there: once more under the switch lock, then
   the switch, then the hold again, up to holds_after times retry_ms apart.
   Each stage has budget_ms. Answers the hold, id 0 for a refusal. */
Ops::Hold holdWithSwitch(HoldOps &ops, unsigned budget_ms, int holds_after, unsigned retry_ms);

/* What the GUI's answer to a switch request says. */
enum SwitchOutcome
{
	NotSwitched = 0,
	Switched = 1,     // the picture shows the channel
	NotNeeded = 2     // a frontend is free for the stream as things are
};

/* Where a setup waits for the GUI to switch the picture, one request at a
   time. begin numbers a request; the GUI claims it before it acts and says
   done after. Without a claim in time the setup cancels it, and so does a stop
   of the streams of its channel or on a frontend; a later claim fails, so a
   request nobody waits for, or one for a setup that is taken back, is never
   carried out. A claimed request runs to its end. */
class SwitchWait
{
public:
	SwitchWait();
	~SwitchWait();

	uint32_t begin(uint64_t channel);
	// The GUI: true while seq is still waited for; it is then carried out.
	bool claim(uint32_t seq);
	void done(uint32_t seq, int outcome);
	/* The setup: up to claim_ms for the claim, then up to done_ms for the
	   outcome; NotSwitched when the time ran out or the request was cancelled. */
	int wait(uint32_t seq, unsigned claim_ms, unsigned done_ms);
	// A stop of the streams of channel, or of any setup: what is pending is not claimed.
	void cancelFor(uint64_t channel);
	void cancelPending();
	// As the server stops: every wait ends, and nothing pending is claimed after.
	void cancelAll();

private:
	SwitchWait(const SwitchWait &);
	SwitchWait &operator=(const SwitchWait &);

	enum State
	{
		Idle,
		Pending,
		Claimed,
		Done,
		Cancelled
	};

	pthread_mutex_t m;
	pthread_cond_t  changed;
	uint32_t        seq_;
	uint64_t        channel_;
	State           state_;
	int             outcome_;
};

/* Under the gate's mutex: begins a switch request for a setup let in at since,
   only while that setup may still publish its stream of channel and no
   frontend's streams were taken back since it was let in. A stop that takes
   the setup back before this is seen here; one after it cancels the request
   (cancelFor, cancelPending) under the same mutex. */
bool beginSwitchLocked(const StartGate &gate, SwitchWait &w, uint64_t since, uint64_t channel, uint32_t &seq);

/* What the GUI does for a switch request it claimed: switches the picture
   to the channel, and answers Switched, NotSwitched or NotNeeded. */
class SwitchPicture
{
public:
	virtual ~SwitchPicture() {}
	virtual int switchPicture(uint64_t channel) = 0;
};

/* The GUI's part of a switch request: the picture is switched only while
   the setup still waits for seq, and the outcome goes back to it. Whether
   it was carried out. */
bool serveSwitch(SwitchWait &wait, uint32_t seq, uint64_t channel, SwitchPicture &picture);

} // namespace streamsetup

#endif
