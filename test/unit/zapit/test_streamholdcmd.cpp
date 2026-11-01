/*
 * test_streamholdcmd.cpp - the command that holds a frontend for a stream
 *
 * Copyright (C) 2026 Thilo Graf
 *
 * License: GPL-2.0-or-later
 *
 * streamholdcmd::run is the order zapit follows for a hold and a probe.
 * These cases drive the shipped object over a model of the box: three
 * frontends with use counts, a demux source table and a lock that knows
 * whether it is held, so a case can say what happened under it and what
 * after it.
 */

#include "support/catch.hpp"

#include <zapit/streamadmission.h>
#include <zapit/streamholdcmd.h>

#include <map>
#include <vector>

using namespace streamholdcmd;

namespace
{

const uint64_t kLive = 0x1001;
const uint64_t kOther = 0x2002;
const uint64_t kPip = 0x3003;
const uint64_t kWeb = 0x4004;

struct Fe
{
	int number;
	int use;   // the use count lockFrontend raises
};

struct Box : public Ops
{
	Fe fe[3];
	streamadmission::Facts f;
	std::vector<int> stream_holds;
	std::map<int, int> source;   // demux -> the frontend it reads

	bool serving_now;
	bool stop_while_tuning;   // zapit begins to stop during the tune
	int64_t clock;
	int64_t clock_after_tune;
	bool room;
	bool tune_ok;
	bool answer_ok;
	uint32_t next_id;

	int lock_depth;
	int collects, opens, binds, live_holds, picks, tunes, unlocks, offers, drops, answers;
	bool facts_locked, bind_locked, tune_unlocked;
	Reply sent;

	Box()
		: serving_now(true), stop_while_tuning(false), clock(1000), clock_after_tune(-1), room(true), tune_ok(true),
		  answer_ok(true), next_id(9), lock_depth(0), collects(0), opens(0), binds(0),
		  live_holds(0), picks(0), tunes(0), unlocks(0), offers(0), drops(0), answers(0),
		  facts_locked(true), bind_locked(true), tune_unlocked(true)
	{
		for (int i = 0; i < 3; ++i)
		{
			fe[i].number = i;
			fe[i].use = 0;
		}
		// The picture on frontend 0, one PiP slot, nothing in it.
		f.live_dvb = true;
		f.live_tuned = true;
		f.live_channel = kLive;
		f.live_fe = 0;
		f.slots = 1;
		source[0] = 0;
	}

	int uses() const
	{
		return fe[0].use + fe[1].use + fe[2].use;
	}

	void collect() { collects++; }
	bool serving() { return serving_now; }
	int64_t nowMs() { return clock; }
	Kind channel(uint64_t id)
	{
		if (id == kWeb)
			return WebChannel;
		return (id == kLive || id == kOther || id == kPip) ? Broadcast : NoChannel;
	}
	bool roomForHold() { return room; }
	void lockFrontends() { lock_depth++; }
	void unlockFrontends() { lock_depth--; }

	void facts(streamadmission::Facts &out)
	{
		facts_locked = facts_locked && lock_depth > 0;
		out = f;
	}
	void heldByStreams(std::vector<int> &out)
	{
		out.insert(out.end(), stream_holds.begin(), stream_holds.end());
	}
	void *holdLive(bool lock)
	{
		live_holds++;
		binds++;
		bind_locked = bind_locked && lock_depth > 0;
		source[f.live_fe + 1] = f.live_fe;
		if (lock)
			fe[f.live_fe].use++;
		return &fe[f.live_fe];
	}
	void openFrontends() { opens++; }
	void *pickIdle(const std::vector<int> &held, bool bind, bool lock)
	{
		picks++;
		for (int i = 0; i < 3; ++i)
		{
			bool is_held = fe[i].use > 0;
			for (size_t k = 0; k < held.size(); ++k)
				is_held = is_held || held[k] == i;
			if (is_held)
				continue;
			if (bind)
			{
				binds++;
				bind_locked = bind_locked && lock_depth > 0;
				source[i + 1] = i;
			}
			if (lock)
				fe[i].use++;
			return &fe[i];
		}
		return 0;
	}
	int number(void *p) { return static_cast<Fe *>(p)->number; }
	bool tune(void *)
	{
		tunes++;
		tune_unlocked = tune_unlocked && lock_depth == 0;
		if (stop_while_tuning)
			serving_now = false;
		if (clock_after_tune >= 0)
			clock = clock_after_tune;
		return tune_ok;
	}
	void unlockFrontend(void *p)
	{
		unlocks++;
		static_cast<Fe *>(p)->use--;
	}
	uint32_t offer(void *)
	{
		offers++;
		return next_id;
	}
	void drop(uint32_t) { drops++; }
	bool answer(const Reply &r)
	{
		answers++;
		sent = r;
		return answer_ok;
	}
};

} // namespace

TEST_CASE("a hold of the live channel binds the picture's frontend and enters a hold", "[zapit][streamholdcmd]")
{
	Box box;
	const Reply r = run(box, kLive, 5000, false, true);
	REQUIRE(r.verdict == Granted);
	REQUIRE(r.hold_id == 9);
	REQUIRE(r.fenumber == 0);
	REQUIRE(box.sent.hold_id == 9);
	REQUIRE(box.live_holds == 1);
	REQUIRE(box.fe[0].use == 1);
	REQUIRE(box.tunes == 0);      // the picture's frontend is not retuned
	REQUIRE(box.offers == 1);
	REQUIRE(box.facts_locked);
	REQUIRE(box.bind_locked);
	REQUIRE(box.lock_depth == 0);
}

TEST_CASE("a hold of another channel binds an idle frontend and tunes it after the lock", "[zapit][streamholdcmd]")
{
	Box box;
	const Reply r = run(box, kOther, 5000, false, true);
	REQUIRE(r.verdict == Granted);
	REQUIRE(r.fenumber == 1);
	REQUIRE(box.fe[1].use == 1);
	REQUIRE(box.source[2] == 1);
	REQUIRE(box.source.count(1) == 0);   // demux 1 was not set
	REQUIRE(box.tunes == 1);
	REQUIRE(box.tune_unlocked);
	REQUIRE(box.bind_locked);
}

TEST_CASE("without locking the frontend a hold leaves its use count alone", "[zapit][streamholdcmd]")
{
	Box box;
	REQUIRE(run(box, kOther, 5000, false, false).verdict == Granted);
	REQUIRE(box.uses() == 0);
	REQUIRE(box.binds == 1);
	REQUIRE(box.offers == 1);
}

TEST_CASE("repeated probes of the live channel bind, lock and open nothing", "[zapit][streamholdcmd]")
{
	Box box;
	const std::map<int, int> sources_before = box.source;
	for (int i = 0; i < 5; ++i)
	{
		const Reply r = run(box, kLive, 0, true, true);
		REQUIRE(r.verdict == Granted);
		REQUIRE(r.fenumber == 0);
		REQUIRE(r.hold_id == 0);
	}
	REQUIRE(box.uses() == 0);
	REQUIRE(box.binds == 0);
	REQUIRE(box.live_holds == 0);
	REQUIRE(box.opens == 0);
	REQUIRE(box.offers == 0);
	REQUIRE(box.tunes == 0);
	REQUIRE(box.source == sources_before);
	REQUIRE(box.answers == 5);
}

TEST_CASE("repeated probes in standby bind, lock and open nothing", "[zapit][streamholdcmd]")
{
	Box box;
	box.f.standby = true;
	const std::map<int, int> sources_before = box.source;
	for (int i = 0; i < 5; ++i)
	{
		const Reply r = run(box, kOther, 0, true, true);
		REQUIRE(r.verdict == Granted);
		REQUIRE(r.fenumber == 0);   // in standby the picture holds nothing
	}
	REQUIRE(box.uses() == 0);
	REQUIRE(box.binds == 0);
	REQUIRE(box.opens == 0);
	REQUIRE(box.offers == 0);
	REQUIRE(box.source == sources_before);

	// A hold in standby does open them, which is what the probes left out.
	REQUIRE(run(box, kOther, 5000, false, true).verdict == Granted);
	REQUIRE(box.opens == 1);
	REQUIRE(box.uses() == 1);
}

TEST_CASE("a probe answers a refusal the way a hold would", "[zapit][streamholdcmd]")
{
	Box box;
	box.f.slot[0].active = true;
	box.f.slot[0].fe = 1;
	box.f.slot[0].channel = kPip;
	REQUIRE(run(box, kPip, 0, true, true).verdict == Busy);
	REQUIRE(run(box, kPip, 5000, false, true).verdict == Busy);
	// And the live channel, whose demux the PiP reads from frontend 1
	REQUIRE(run(box, kLive, 5000, false, true).verdict == Busy);
	REQUIRE(box.binds == 0);
	REQUIRE(box.uses() == 0);
	REQUIRE(box.offers == 0);
	REQUIRE(box.sent.hold_id == 0);
	REQUIRE(box.lock_depth == 0);
}

TEST_CASE("while a recording starts a hold and a probe are refused and bind nothing", "[zapit][streamholdcmd]")
{
	Box box;
	box.f.recording_starting = true;
	REQUIRE(run(box, kOther, 5000, false, true).verdict == Busy);
	REQUIRE(run(box, kLive, 5000, false, true).verdict == Busy);
	REQUIRE(run(box, kOther, 0, true, true).verdict == Busy);
	REQUIRE(run(box, kLive, 0, true, true).verdict == Busy);
	REQUIRE(box.binds == 0);
	REQUIRE(box.live_holds == 0);
	REQUIRE(box.uses() == 0);
	REQUIRE(box.tunes == 0);
	REQUIRE(box.offers == 0);
	REQUIRE(box.sent.hold_id == 0);
	REQUIRE(box.lock_depth == 0);
}

TEST_CASE("frontends the PiP and other streams hold are passed over", "[zapit][streamholdcmd]")
{
	Box box;
	box.f.slot[0].active = true;
	box.f.slot[0].fe = 1;
	box.f.slot[0].channel = kPip;
	// Live 0, PiP slot 0 on 1: frontend 2 is the one left.
	Reply r = run(box, kOther, 5000, false, false);
	REQUIRE(r.verdict == Granted);
	REQUIRE(r.fenumber == 2);
	REQUIRE(box.source[3] == 2);
	REQUIRE(box.source.count(1) == 0);   // the PiP's demux was not set

	// A stream holds frontend 2 now: nothing is left.
	box.stream_holds.push_back(2);
	r = run(box, kOther, 5000, false, false);
	REQUIRE(r.verdict == Busy);
	REQUIRE(r.hold_id == 0);
}

TEST_CASE("an unknown channel and a web channel are not found", "[zapit][streamholdcmd]")
{
	Box box;
	REQUIRE(run(box, 0xdead, 5000, false, true).verdict == NotFound);
	REQUIRE(run(box, kWeb, 5000, false, true).verdict == NotFound);
	REQUIRE(run(box, kWeb, 0, true, true).verdict == NotFound);
	REQUIRE(box.lock_depth == 0);
	REQUIRE(box.picks == 0);
}

TEST_CASE("a zapit that stops and a request past its time are failed before the lock", "[zapit][streamholdcmd]")
{
	Box box;
	box.serving_now = false;
	REQUIRE(run(box, kOther, 5000, false, true).verdict == Failed);
	REQUIRE(run(box, kOther, 0, true, true).verdict == Failed);

	box.serving_now = true;
	box.clock = 6000;
	REQUIRE(run(box, kOther, 5000, false, true).verdict == Failed);
	REQUIRE(box.picks == 0);
	// A probe is not held to a deadline.
	REQUIRE(run(box, kOther, 5000, true, true).verdict == Granted);
}

TEST_CASE("a frontend that does not tune is given back and the hold failed", "[zapit][streamholdcmd]")
{
	Box box;
	box.tune_ok = false;
	const Reply r = run(box, kOther, 5000, false, true);
	REQUIRE(r.verdict == Failed);
	REQUIRE(r.hold_id == 0);
	REQUIRE(box.unlocks == 1);
	REQUIRE(box.uses() == 0);
	REQUIRE(box.offers == 0);
}

TEST_CASE("H1: zapit that begins to stop while the frontend tunes enters no hold", "[zapit][streamholdcmd]")
{
	Box box;
	box.stop_while_tuning = true;
	const Reply r = run(box, kOther, 5000, false, true);
	REQUIRE(r.verdict == Failed);
	REQUIRE(r.hold_id == 0);
	REQUIRE(box.tunes == 1);
	REQUIRE(box.unlocks == 1);
	REQUIRE(box.uses() == 0);
	REQUIRE(box.offers == 0);
	REQUIRE(box.answers == 1);
}

TEST_CASE("a deadline that passes while tuning fails the hold and gives the frontend back", "[zapit][streamholdcmd]")
{
	Box box;
	box.clock_after_tune = 7000;
	REQUIRE(run(box, kOther, 5000, false, true).verdict == Failed);
	REQUIRE(box.uses() == 0);
	REQUIRE(box.offers == 0);
}

TEST_CASE("a full register refuses before anything is bound", "[zapit][streamholdcmd]")
{
	Box box;
	box.room = false;
	REQUIRE(run(box, kLive, 5000, false, true).verdict == Busy);
	REQUIRE(box.binds == 0);
	REQUIRE(box.picks == 0);
	REQUIRE(box.uses() == 0);
}

TEST_CASE("a probe with a full register answers what a hold would get", "[zapit][streamholdcmd]")
{
	/* Eight holds entered: a hold now would be refused, so a probe says so,
	   and changes nothing on the way. */
	Box box;
	box.room = false;
	const std::map<int, int> sources_before = box.source;
	REQUIRE(run(box, kLive, 0, true, true).verdict == Busy);
	box.f.standby = true;
	REQUIRE(run(box, kOther, 0, true, true).verdict == Busy);
	REQUIRE(box.binds == 0);
	REQUIRE(box.live_holds == 0);
	REQUIRE(box.opens == 0);
	REQUIRE(box.picks == 0);
	REQUIRE(box.offers == 0);
	REQUIRE(box.uses() == 0);
	REQUIRE(box.source == sources_before);
	REQUIRE(box.lock_depth == 0);
	REQUIRE(box.sent.hold_id == 0);
}

TEST_CASE("a register that cannot take the hold gives the frontend back", "[zapit][streamholdcmd]")
{
	Box box;
	box.next_id = 0;
	REQUIRE(run(box, kLive, 5000, false, true).verdict == Busy);
	REQUIRE(box.uses() == 0);
	REQUIRE(run(box, kOther, 5000, false, true).verdict == Busy);
	REQUIRE(box.uses() == 0);
}

TEST_CASE("a hold whose answer did not go out is dropped and collected at once", "[zapit][streamholdcmd]")
{
	Box box;
	box.answer_ok = false;
	const Reply r = run(box, kOther, 5000, false, true);
	REQUIRE(r.verdict == Granted);
	REQUIRE(box.drops == 1);
	REQUIRE(box.collects == 2);   // the one every command starts with, and this one
}

TEST_CASE("every command first collects what was dropped since the last", "[zapit][streamholdcmd]")
{
	Box box;
	run(box, kOther, 0, true, true);
	run(box, 0xdead, 0, true, true);
	box.serving_now = false;
	run(box, kOther, 0, true, true);
	REQUIRE(box.collects == 3);
	REQUIRE(box.answers == 3);
}
