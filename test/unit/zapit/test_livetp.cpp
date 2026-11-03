/*
 * test_livetp.cpp - whether the live channel's tuner stands on its transponder
 *
 * Copyright (C) 2026 Tuxbox-Project (live tuner taken, tests)
 *
 * License: GPL-2.0-or-later
 *
 * livetp decides from values read off the box whether a stream holds the
 * live frontend on another transponder than the live channel's, and what
 * the end of the streams does about it. These cases drive the shipped
 * object; where a case stands for a start of a stream, a completed zap or
 * standby it calls Rezap::reset() itself, and check-live-displaced.sh holds
 * the program to calling it at those points.
 */

#include "support/catch.hpp"

#include <zapit/livetp.h>

namespace
{

// Two transponders with the same lower 32 bits, as NVOD services carry.
const uint64_t kTpA = 0x0130044d00010001ULL;
const uint64_t kTpB = 0x0130044e00020001ULL;
const uint64_t kTpA2 = 0x0d00044d00010001ULL;

} // namespace

TEST_CASE("L1: a tuned live frontend on another transponder is displaced", "[zapit][livetp]")
{
	REQUIRE(livetp::displaced(false, true, true, kTpB, kTpA, false));
}

TEST_CASE("L2: a live frontend on the channel's own transponder is not", "[zapit][livetp]")
{
	REQUIRE_FALSE(livetp::displaced(false, true, true, kTpA, kTpA, false));
}

TEST_CASE("L3: a frontend without signal or no frontend at all is not", "[zapit][livetp]")
{
	// No signal: a zap to the same channel does nothing.
	REQUIRE_FALSE(livetp::displaced(false, true, false, kTpB, kTpA, false));
	// No tuner, as on a box without one.
	REQUIRE_FALSE(livetp::displaced(false, false, false, 0, kTpA, false));
	REQUIRE_FALSE(livetp::displaced(false, false, true, kTpB, kTpA, false));
}

TEST_CASE("L4: a web channel is never displaced", "[zapit][livetp]")
{
	REQUIRE_FALSE(livetp::displaced(true, true, true, kTpB, kTpA, false));
	REQUIRE_FALSE(livetp::displaced(true, true, true, kTpB, kTpA, true));
}

TEST_CASE("L5: an NVOD channel compares the lower 32 bits of the transponders", "[zapit][livetp]")
{
	// Same lower half: the frontend would not retune, so not displaced.
	REQUIRE_FALSE(livetp::displaced(false, true, true, kTpA2, kTpA, true));
	// The same pair counts as displaced for a channel that is not NVOD.
	REQUIRE(livetp::displaced(false, true, true, kTpA2, kTpA, false));
	// Another lower half is displaced for NVOD as well.
	REQUIRE(livetp::displaced(false, true, true, kTpB, kTpA, true));
}

namespace
{

const uint64_t kLive = 0x0001000100010001ULL;   // the live channel
const uint64_t kOther = 0x0002000100020001ULL;  // another one

/* The box as the program reads it at an end of streams: awake in TV mode,
   the live channel displaced from its tuner to kTpB, no stream left. */
struct Box
{
	bool awake, broadcast, displaced, streaming, pending;
	uint64_t live, fe_tp;
	Box() : awake(true), broadcast(true), displaced(true), streaming(false), pending(false), live(kLive), fe_tp(kTpB) {}
	livetp::Rezap::Step step(livetp::Rezap &r) const
	{
		return r.next(awake, broadcast, displaced, streaming, pending, live, fe_tp);
	}
	// Whether this end of the streams zaps.
	bool stop(livetp::Rezap &r) const
	{
		return step(r) == livetp::Rezap::Zap;
	}
};

} // namespace

TEST_CASE("R1: the end of streams looks again only for an awake box showing a displaced broadcast channel", "[zapit][livetp][rezap]")
{
	REQUIRE(livetp::rezapWanted(true, true, true));
	REQUIRE_FALSE(livetp::rezapWanted(false, true, true));   // standby
	REQUIRE_FALSE(livetp::rezapWanted(true, false, true));   // a file, a web channel
	REQUIRE_FALSE(livetp::rezapWanted(true, true, false));   // the picture is there
}

TEST_CASE("R2: a displacement is zapped once, and not while a stream runs or is set up", "[zapit][livetp][rezap]")
{
	livetp::Rezap r;
	Box b;
	b.streaming = true;
	REQUIRE_FALSE(b.stop(r));
	// Not marked by the stream that still ran: its own end zaps.
	b.streaming = false;
	REQUIRE(b.stop(r));
	// The same displacement again, as a refused client's end announces it.
	REQUIRE_FALSE(b.stop(r));
	REQUIRE_FALSE(b.stop(r));
	// Each condition on its own keeps the zap away.
	livetp::Rezap fresh;
	Box s = b; s.awake = false; REQUIRE_FALSE(s.stop(fresh));
	Box m = b; m.broadcast = false; REQUIRE_FALSE(m.stop(fresh));
	Box d = b; d.displaced = false; REQUIRE_FALSE(d.stop(fresh));
	REQUIRE(b.stop(fresh));
}

TEST_CASE("R3: another displacement, of another channel or onto another transponder, is zapped again", "[zapit][livetp][rezap]")
{
	livetp::Rezap r;
	Box b;
	REQUIRE(b.stop(r));
	Box t = b; t.fe_tp = kTpA2;
	REQUIRE(t.stop(r));
	Box c = t; c.live = kOther;
	REQUIRE(c.stop(r));
}

TEST_CASE("R4: a zap that failed during a hold is tried again after the next stream", "[zapit][livetp][rezap]")
{
	livetp::Rezap r;
	Box b;
	// The streams end, the zap is tried and finds no frontend once more:
	// nothing resets the mark.
	REQUIRE(b.stop(r));
	REQUIRE_FALSE(b.stop(r));
	// A stream starts and ends; the live channel is still displaced the
	// same way.
	r.reset();      // EVT_STREAM_START
	REQUIRE(b.stop(r));
}

TEST_CASE("R5: after a zap that worked, standby and a wake that failed, the end of streams zaps again", "[zapit][livetp][rezap]")
{
	livetp::Rezap r;
	Box b;
	REQUIRE(b.stop(r));
	r.reset();      // EVT_ZAP_COMPLETE of the live channel
	r.reset();      // into standby
	// In standby a stream takes the live tuner to the same transponder.
	Box sleeping = b; sleeping.awake = false;
	REQUIRE_FALSE(sleeping.stop(r));
	r.reset();      // out of standby: the wake's zap finds no frontend
	REQUIRE(b.stop(r));
}

TEST_CASE("R6: ends of refused requests do not repeat the zap, a mark from before standby does not hold", "[zapit][livetp][rezap]")
{
	livetp::Rezap r;
	Box b;
	REQUIRE(b.stop(r));
	// Refused requests on the raw port: an end of streams with no start.
	for (int i = 0; i < 5; i++)
		REQUIRE_FALSE(b.stop(r));
	// Without the reset at standby the same pair would stay blocked.
	r.reset();
	REQUIRE(b.stop(r));
}

TEST_CASE("R7: a zap still pending does not use the try, which is made once it is over", "[zapit][livetp][rezap]")
{
	livetp::Rezap r;
	Box b;
	b.pending = true;
	REQUIRE(b.step(r) == livetp::Rezap::Later);
	REQUIRE(b.step(r) == livetp::Rezap::Later);
	b.pending = false;
	REQUIRE(b.step(r) == livetp::Rezap::Zap);
	REQUIRE(b.step(r) == livetp::Rezap::Nothing);
	// The zap just sent is pending itself: no second try, no later one.
	b.pending = true;
	REQUIRE(b.step(r) == livetp::Rezap::Nothing);
	// Nothing to do is nothing, pending or not.
	livetp::Rezap fresh;
	Box s = b; s.streaming = true; REQUIRE(s.step(fresh) == livetp::Rezap::Nothing);
	Box d = b; d.displaced = false; REQUIRE(d.step(fresh) == livetp::Rezap::Nothing);
}

TEST_CASE("R8: the wake zap pending when the stream ends, its failure, then the zap back", "[zapit][livetp][rezap]")
{
	livetp::Rezap r;
	Box b;
	r.reset();      // out of standby; the wake zap goes out and finds no frontend
	b.pending = true;
	// The stream's end is read before the wake zap's failure.
	REQUIRE(b.step(r) == livetp::Rezap::Later);
	// The failure ends the guard; the check made again zaps.
	b.pending = false;
	REQUIRE(b.step(r) == livetp::Rezap::Zap);
	// Ends of refused requests after that do nothing, and arm nothing.
	for (int i = 0; i < 3; i++)
		REQUIRE(b.step(r) == livetp::Rezap::Nothing);
	// A same-channel zap by the user right before the end: the same.
	livetp::Rezap user;
	b.pending = true;
	REQUIRE(b.step(user) == livetp::Rezap::Later);
	b.pending = false;
	REQUIRE(b.step(user) == livetp::Rezap::Zap);
}

TEST_CASE("R9: the zap goes back to the channel last asked for when the list holds it", "[zapit][livetp][rezap]")
{
	// A wake onto C, or a zap to C, that found no frontend left zapit at A.
	REQUIRE(livetp::rezapTarget(kOther, false, true, kLive) == kOther);
	// A web channel, or one the list does not hold: zapit's live channel.
	REQUIRE(livetp::rezapTarget(kOther, true, true, kLive) == kLive);
	REQUIRE(livetp::rezapTarget(kOther, false, false, kLive) == kLive);
	// Nothing asked for yet.
	REQUIRE(livetp::rezapTarget(0, false, true, kLive) == kLive);
	// The try is marked for the channel zapped to.
	livetp::Rezap r;
	Box b;
	b.live = livetp::rezapTarget(kOther, false, true, kLive);
	REQUIRE(b.stop(r));
	REQUIRE_FALSE(b.stop(r));
	Box a = b; a.live = kLive;
	REQUIRE(a.stop(r));
}
