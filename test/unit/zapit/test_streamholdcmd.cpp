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
#include <string>
#include <vector>

using namespace streamholdcmd;

namespace
{

const uint64_t kLive = 0x1001;
const uint64_t kOther = 0x2002;
const uint64_t kPip = 0x3003;
const uint64_t kWeb = 0x4004;
const uint64_t kShared = 0x5005;   // another channel on the live transponder

const uint64_t kLiveTp = 0x2cec00c0044d0001ULL;
const uint64_t kOtherTp = 0x2ed400c0043e0001ULL;

struct Fe
{
	int number;
	int use;   // the use count lockFrontend raises
};

struct Box : public SwitchOps
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
	bool pids_ok;
	bool stop_while_reading;   // zapit begins to stop while PAT and PMT are read
	bool scrambled_in_pmt;     // the PMT read shows the channel scrambled, for the module
	bool answer_ok;
	uint32_t next_id;

	int lock_depth;
	int collects, opens, binds, live_holds, picks, tunes, pid_reads, unlocks, offers, drops, answers;
	bool facts_locked, bind_locked, tune_unlocked, read_unlocked;
	int zap_fe;            // what zapFrontend answers
	bool zap_turns_dish;
	int zap_asks;
	bool zap_locked;       // every zapFrontend came under the lock
	bool zap_ok;           // what a zap to any channel but the live one answers
	bool zap_back_ok;      // what a zap to the live one answers
	int zaps, pip_stops, switch_answers;
	uint64_t pip_stopped, zapped;
	std::vector<uint64_t> zapped_all;
	std::string order;     // z for each zap, p for each PiP stop, in the order they came
	bool switch_unlocked;  // the PiP stop and the zap came outside the lock
	int32_t switched;
	std::vector<void *> predicted_keep, zapped_keep;
	uint64_t asked;
	Reply sent;

	Box()
		: serving_now(true), stop_while_tuning(false), clock(1000), clock_after_tune(-1), room(true), tune_ok(true),
		  pids_ok(true), stop_while_reading(false), scrambled_in_pmt(false), answer_ok(true), next_id(9),
		  lock_depth(0), collects(0),
		  opens(0), binds(0), live_holds(0), picks(0), tunes(0), pid_reads(0), unlocks(0), offers(0),
		  drops(0), answers(0), facts_locked(true), bind_locked(true), tune_unlocked(true),
		  read_unlocked(true), zap_fe(-1), zap_turns_dish(false), zap_asks(0), zap_locked(true),
		  zap_ok(true), zap_back_ok(true), zaps(0), pip_stops(0), switch_answers(0), pip_stopped(0), zapped(0),
		  switch_unlocked(true), switched(-1), asked(0)
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
		f.live_tp = kLiveTp;
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
		asked = id;
		if (id == kWeb)
			return WebChannel;
		return (id == kLive || id == kOther || id == kPip || id == kShared) ? Broadcast : NoChannel;
	}
	bool roomForHold() { return room; }
	void lockFrontends() { lock_depth++; }
	void unlockFrontends() { lock_depth--; }

	void facts(streamadmission::Facts &out)
	{
		facts_locked = facts_locked && lock_depth > 0;
		out = f;
		// As zapit sets it: the transponder of the channel asked for.
		out.channel_tp = (asked == kLive || asked == kShared) ? kLiveTp : kOtherTp;
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
	int zapFrontend(std::vector<void *> &keep, bool &turns_dish)
	{
		zap_asks++;
		zap_locked = zap_locked && lock_depth > 0;
		turns_dish = zap_turns_dish;
		// As zapit counts them: every stream hold's frontend and every PiP's.
		keep.clear();
		for (size_t i = 0; i < stream_holds.size(); ++i)
			keep.push_back(&fe[stream_holds[i]]);
		for (int i = 0; i < f.slots; ++i)
			if (f.slot[i].active && f.slot[i].fe >= 0)
				keep.push_back(&fe[f.slot[i].fe]);
		predicted_keep = keep;
		return zap_fe;
	}
	void stopPipsShowing(uint64_t channel)
	{
		pip_stops++;
		pip_stopped = channel;
		order += 'p';
		switch_unlocked = switch_unlocked && lock_depth == 0;
		for (int i = 0; i < f.slots; ++i)
			if (f.slot[i].active && f.slot[i].channel == channel)
				f.slot[i].active = false;
	}
	bool zapKept(uint64_t channel, const std::vector<void *> &keep)
	{
		zaps++;
		zapped = channel;
		zapped_all.push_back(channel);
		zapped_keep = keep;
		order += 'z';
		switch_unlocked = switch_unlocked && lock_depth == 0;
		return channel == kLive ? zap_back_ok : zap_ok;
	}
	bool answerSwitch(int32_t s)
	{
		switch_answers++;
		switched = s;
		return true;
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
		if (scrambled_in_pmt)
			f.channel_ci = true;
		return tune_ok;
	}
	bool readPids(void *)
	{
		pid_reads++;
		read_unlocked = read_unlocked && lock_depth == 0;
		if (stop_while_reading)
			serving_now = false;
		if (clock_after_tune >= 0)
			clock = clock_after_tune;
		if (scrambled_in_pmt)
			f.channel_ci = true;
		return pids_ok;
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

TEST_CASE("a hold of another channel on the live transponder shares the picture's frontend untuned", "[zapit][streamholdcmd][shared]")
{
	Box box;
	const Reply r = run(box, kShared, 5000, false, true);
	REQUIRE(r.verdict == Granted);
	REQUIRE(r.hold_id == 9);
	REQUIRE(r.fenumber == 0);
	REQUIRE(box.live_holds == 1);
	REQUIRE(box.picks == 0);
	REQUIRE(box.fe[0].use == 1);      // locked once, released by the register
	REQUIRE(box.uses() == 1);
	REQUIRE(box.source[1] == 0);
	REQUIRE(box.tunes == 0);          // the picture's frontend is not retuned
	REQUIRE(box.pid_reads == 1);      // but the channel's PAT and PMT are read
	REQUIRE(box.read_unlocked);
	REQUIRE(box.bind_locked);
	REQUIRE(box.facts_locked);
	REQUIRE(box.offers == 1);
	REQUIRE(box.unlocks == 0);
	REQUIRE(box.lock_depth == 0);
}

TEST_CASE("the live channel itself still reads no PAT and PMT", "[zapit][streamholdcmd][shared]")
{
	Box box;
	REQUIRE(run(box, kLive, 5000, false, true).verdict == Granted);
	REQUIRE(box.pid_reads == 0);
	REQUIRE(box.tunes == 0);
}

TEST_CASE("a channel on another transponder still takes an idle frontend", "[zapit][streamholdcmd][shared]")
{
	Box box;
	const Reply r = run(box, kOther, 5000, false, true);
	REQUIRE(r.verdict == Granted);
	REQUIRE(r.fenumber == 1);
	REQUIRE(box.live_holds == 0);
	REQUIRE(box.pid_reads == 0);
	REQUIRE(box.tunes == 1);
}

TEST_CASE("a shared hold whose PAT and PMT cannot be read gives the frontend back once", "[zapit][streamholdcmd][shared]")
{
	Box box;
	box.pids_ok = false;
	const Reply r = run(box, kShared, 5000, false, true);
	REQUIRE(r.verdict == Failed);
	REQUIRE(r.hold_id == 0);
	REQUIRE(r.fenumber == -1);
	REQUIRE(box.pid_reads == 1);
	REQUIRE(box.unlocks == 1);
	REQUIRE(box.uses() == 0);
	REQUIRE(box.offers == 0);
	REQUIRE(box.tunes == 0);
}

TEST_CASE("a shared hold fails when zapit stops or the time runs out while reading", "[zapit][streamholdcmd][shared]")
{
	Box box;
	box.stop_while_reading = true;
	REQUIRE(run(box, kShared, 5000, false, true).verdict == Failed);
	REQUIRE(box.unlocks == 1);
	REQUIRE(box.uses() == 0);
	REQUIRE(box.offers == 0);

	Box late;
	late.clock_after_tune = 7000;
	REQUIRE(run(late, kShared, 5000, false, true).verdict == Failed);
	REQUIRE(late.unlocks == 1);
	REQUIRE(late.uses() == 0);
	REQUIRE(late.offers == 0);
}

TEST_CASE("without locking the frontend a shared hold leaves its use count alone", "[zapit][streamholdcmd][shared]")
{
	Box box;
	REQUIRE(run(box, kShared, 5000, false, false).verdict == Granted);
	REQUIRE(box.uses() == 0);
	REQUIRE(box.live_holds == 1);

	Box failing;
	failing.pids_ok = false;
	REQUIRE(run(failing, kShared, 5000, false, false).verdict == Failed);
	REQUIRE(failing.unlocks == 0);
	REQUIRE(failing.uses() == 0);
}

TEST_CASE("while a channel scan runs a channel on the live transponder takes an idle frontend", "[zapit][streamholdcmd][shared]")
{
	Box box;
	box.f.scanning = true;
	const Reply r = run(box, kShared, 5000, false, true);
	REQUIRE(r.verdict == Granted);
	REQUIRE(r.fenumber == 1);
	REQUIRE(box.live_holds == 0);
	REQUIRE(box.pid_reads == 0);
	REQUIRE(box.tunes == 1);
	REQUIRE(box.fe[0].use == 0);
}

TEST_CASE("a probe of a shared channel binds, locks and reads nothing", "[zapit][streamholdcmd][shared]")
{
	Box box;
	const std::map<int, int> sources_before = box.source;
	const Reply r = run(box, kShared, 0, true, true);
	REQUIRE(r.verdict == Granted);
	REQUIRE(r.fenumber == 0);
	REQUIRE(r.hold_id == 0);
	REQUIRE(box.binds == 0);
	REQUIRE(box.live_holds == 0);
	REQUIRE(box.pid_reads == 0);
	REQUIRE(box.uses() == 0);
	REQUIRE(box.source == sources_before);
}

TEST_CASE("a register that cannot take a shared hold gives the frontend back", "[zapit][streamholdcmd][shared]")
{
	Box box;
	box.next_id = 0;
	REQUIRE(run(box, kShared, 5000, false, true).verdict == Busy);
	REQUIRE(box.unlocks == 1);
	REQUIRE(box.uses() == 0);
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

TEST_CASE("P1: a channel the PMT shows scrambled is asked again, and refused where the module may serve no other", "[zapit][streamholdcmd][ci]")
{
	// The module serves a recording; no other CI service is allowed.
	const uint64_t kRec = 0x6006;
	for (int shared = 0; shared < 2; ++shared)
	{
		Box box;
		box.f.ci_mode = 2;
		streamadmission::addCiService(box.f, kRec, kLiveTp, false);
		box.scrambled_in_pmt = true;
		const uint64_t channel = shared ? kShared : kOther;
		const Reply r = run(box, channel, 5000, false, true);
		REQUIRE(r.verdict == Busy);
		REQUIRE(r.hold_id == 0);
		REQUIRE(box.offers == 0);
		REQUIRE(box.uses() == 0);        // the frontend went back
		REQUIRE(box.unlocks == 1);
		REQUIRE(box.lock_depth == 0);
		REQUIRE(box.facts_locked);       // asked again under the lock
		REQUIRE((shared ? box.pid_reads : box.tunes) == 1);
	}
}

TEST_CASE("P2: a channel the PMT leaves as it was, or one the setting allows beside the others, is held as decided", "[zapit][streamholdcmd][ci]")
{
	const uint64_t kRec = 0x6006;
	{
		Box box;
		box.f.ci_mode = 2;
		streamadmission::addCiService(box.f, kRec, kLiveTp, false);
		const Reply r = run(box, kOther, 5000, false, true);
		REQUIRE(r.verdict == Granted);
		REQUIRE(box.offers == 1);
	}
	{
		Box box;
		box.f.ci_mode = 0;
		streamadmission::addCiService(box.f, kRec, kLiveTp, false);
		box.scrambled_in_pmt = true;
		const Reply r = run(box, kOther, 5000, false, true);
		REQUIRE(r.verdict == Granted);
		REQUIRE(box.offers == 1);
	}
	{
		// The live channel is not read again: its PMT is the picture's.
		Box box;
		box.f.ci_mode = 2;
		box.f.live_ci = true;
		box.scrambled_in_pmt = true;
		const Reply r = run(box, kLive, 5000, false, true);
		REQUIRE(r.verdict == Granted);
	}
}

TEST_CASE("P3: a channel from a tuner the module is not bound to is held, also once the PMT shows it scrambled", "[zapit][streamholdcmd][ci]")
{
	// The module serves a recording on the tuner it is bound to; no other CI service is allowed.
	const uint64_t kRec = 0x6006;
	{
		// Scrambled kLive through the software CA on frontend 0, the module bound to tuner 1.
		Box box;
		box.f.ci_mode = 2;
		box.f.ci_tuner = 1;
		box.f.channel_ci = true;
		streamadmission::addCiService(box.f, kRec, kOtherTp, false);
		const Reply r = run(box, kLive, 5000, false, true);
		REQUIRE(r.verdict == Granted);
		REQUIRE(r.fenumber == 0);
		REQUIRE(box.offers == 1);
	}
	/* Shared reads on frontend 0, Idle tunes frontend 1: held with the
	   module bound to tuner 2, refused with it bound to that frontend. */
	for (int shared = 0; shared < 2; ++shared)
	{
		const int given = shared ? 0 : 1;
		for (int bound = 0; bound < 2; ++bound)
		{
			Box box;
			box.f.ci_mode = 2;
			box.f.ci_tuner = bound ? given : 2;
			streamadmission::addCiService(box.f, kRec, kOtherTp, false);
			box.scrambled_in_pmt = true;
			const Reply r = run(box, shared ? kShared : kOther, 5000, false, true);
			REQUIRE(r.verdict == (bound ? Busy : Granted));
			REQUIRE(r.fenumber == (bound ? -1 : given));
			REQUIRE(box.offers == (bound ? 0 : 1));
			REQUIRE(box.uses() == (bound ? 0 : 1));
			REQUIRE((shared ? box.pid_reads : box.tunes) == 1);
			REQUIRE(box.lock_depth == 0);
		}
	}
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

namespace
{

// Frontends 1 and 2 used by recordings: a channel on another transponder finds none idle.
void fill(Box &box)
{
	box.fe[1].use = 1;
	box.fe[2].use = 1;
}

} // namespace

TEST_CASE("a refused hold asked nothing of a switch says nothing of one", "[zapit][streamholdcmd][switch]")
{
	Box box;
	fill(box);
	const Reply r = run(box, kOther, 5000, false, true);
	REQUIRE(r.verdict == Busy);
	REQUIRE(r.switch_frees == 0);
	REQUIRE(box.zap_asks == 0);
}

TEST_CASE("a refused hold says whether switching the picture would free a tuner, and changes nothing", "[zapit][streamholdcmd][switch]")
{
	Box box;
	fill(box);
	box.zap_fe = 0;   // the zap would take the picture's own frontend
	const Reply r = run(box, kOther, 5000, false, true, true);
	REQUIRE(r.verdict == Busy);
	REQUIRE(r.switch_frees == 1);
	REQUIRE(r.hold_id == 0);
	REQUIRE(r.fenumber == -1);
	REQUIRE(box.sent.switch_frees == 1);
	REQUIRE(box.zap_asks == 1);
	REQUIRE(box.zap_locked);
	REQUIRE(box.binds == 0);
	REQUIRE(box.live_holds == 0);
	REQUIRE(box.tunes == 0);
	REQUIRE(box.offers == 0);
	REQUIRE(box.uses() == 2);
	REQUIRE(box.lock_depth == 0);

	// A probe answers the same.
	const Reply p = run(box, kOther, 0, true, true, true);
	REQUIRE(p.verdict == Busy);
	REQUIRE(p.switch_frees == 1);
	REQUIRE(box.binds == 0);
	REQUIRE(box.uses() == 2);
}

TEST_CASE("no switch frees a tuner where the zap finds none or would take a stream's", "[zapit][streamholdcmd][switch]")
{
	Box box;
	fill(box);
	box.zap_fe = -1;
	REQUIRE(run(box, kOther, 5000, false, true, true).switch_frees == 0);

	// A stream on the picture's frontend would lose its transponder.
	box.zap_fe = 0;
	box.stream_holds.push_back(0);
	REQUIRE(run(box, kOther, 5000, false, true, true).switch_frees == 0);
	REQUIRE(box.binds == 0);
}

TEST_CASE("no switch is offered while a recording starts, in standby, or where the demux would not be free", "[zapit][streamholdcmd][switch]")
{
	Box box;
	fill(box);
	box.zap_fe = 0;
	box.f.recording_starting = true;
	REQUIRE(run(box, kOther, 5000, false, true, true).switch_frees == 0);
	box.f.recording_starting = false;

	box.f.avinput_pip = true;   // reads demux 1, which a stream on frontend 0 would read
	REQUIRE(run(box, kOther, 5000, false, true, true).switch_frees == 0);
	box.f.avinput_pip = false;

	box.f.standby = true;
	fill(box);
	box.fe[0].use = 1;   // in standby nothing is the picture's: all three used
	REQUIRE(run(box, kOther, 5000, false, true, true).switch_frees == 0);
}

TEST_CASE("a granted hold asks nothing about a switch", "[zapit][streamholdcmd][switch]")
{
	Box box;
	const Reply r = run(box, kOther, 5000, false, true, true);
	REQUIRE(r.verdict == Granted);
	REQUIRE(r.switch_frees == 0);
	REQUIRE(box.zap_asks == 0);
	REQUIRE(run(box, kLive, 5000, false, true, true).switch_frees == 0);
	REQUIRE(box.zap_asks == 0);
}

TEST_CASE("the live channel shown by a PiP as well is freed by the switch without a zap", "[zapit][streamholdcmd][switch]")
{
	Box box;
	box.f.slot[0].active = true;
	box.f.slot[0].fe = 1;
	box.f.slot[0].channel = kLive;
	const Reply r = run(box, kLive, 5000, false, true, true);
	REQUIRE(r.verdict == Busy);
	REQUIRE(r.switch_frees == 1);
	REQUIRE(box.zap_asks == 0);
	REQUIRE(box.binds == 0);
}

TEST_CASE("the switch predicts on fresh facts under the lock, then zaps outside it and stops the PiP showing the channel", "[zapit][streamholdcmd][switch]")
{
	Box box;
	fill(box);
	box.zap_fe = 0;
	box.f.slots = 2;
	// A second PiP slot shows the channel from frontend 2.
	box.f.slot[1].active = true;
	box.f.slot[1].channel = kOther;
	box.f.slot[1].fe = 2;
	REQUIRE(runSwitch(box, kOther) == 1);
	REQUIRE(box.switched == 1);
	REQUIRE(box.switch_answers == 1);
	REQUIRE(box.zap_asks == 1);
	REQUIRE(box.zap_locked);
	REQUIRE(box.pip_stops == 1);
	REQUIRE(box.pip_stopped == kOther);
	REQUIRE(box.zaps == 1);
	REQUIRE(box.zapped == kOther);
	REQUIRE(box.switch_unlocked);
	REQUIRE(box.binds == 0);
	REQUIRE(box.offers == 0);
	REQUIRE(box.lock_depth == 0);
	// The zap keeps what the prediction kept, the stopped PiP's frontend among them.
	REQUIRE(box.predicted_keep.size() == 1);
	REQUIRE(box.predicted_keep[0] == &box.fe[2]);
	REQUIRE(box.zapped_keep == box.predicted_keep);
	REQUIRE_FALSE(box.f.slot[1].active);
	// The PiP stops once the picture shows the channel, not before the zap.
	REQUIRE(box.order == "zp");
}

TEST_CASE("a switch the prediction does not hold up stops no PiP and zaps nothing", "[zapit][streamholdcmd][switch]")
{
	Box box;
	fill(box);
	box.zap_fe = -1;
	REQUIRE(runSwitch(box, kOther) == 0);
	box.zap_fe = 0;
	box.zap_turns_dish = true;
	REQUIRE(runSwitch(box, kOther) == 0);
	box.zap_turns_dish = false;
	box.stream_holds.push_back(0);
	REQUIRE(runSwitch(box, kOther) == 0);
	box.stream_holds.clear();
	box.f.recording_starting = true;
	REQUIRE(runSwitch(box, kOther) == 0);
	box.f.recording_starting = false;
	box.serving_now = false;
	REQUIRE(runSwitch(box, kOther) == 0);
	box.serving_now = true;
	REQUIRE(runSwitch(box, kWeb) == 0);
	REQUIRE(runSwitch(box, 0xdead) == 0);
	REQUIRE(box.pip_stops == 0);
	REQUIRE(box.zaps == 0);
	REQUIRE(box.switch_answers == 7);
	REQUIRE(box.switched == 0);
	REQUIRE(box.lock_depth == 0);

	// In standby nothing is the picture's: the stream takes its frontend, nothing is switched.
	box.f.standby = true;
	REQUIRE(runSwitch(box, kOther) == 2);
	REQUIRE(box.pip_stops == 0);
	REQUIRE(box.zaps == 0);
}

TEST_CASE("the live channel a PiP shows as well is switched by stopping that PiP alone", "[zapit][streamholdcmd][switch]")
{
	Box box;
	box.f.slot[0].active = true;
	box.f.slot[0].fe = 1;
	box.f.slot[0].channel = kLive;
	REQUIRE(runSwitch(box, kLive) == 1);
	REQUIRE(box.pip_stops == 1);
	REQUIRE(box.pip_stopped == kLive);
	REQUIRE(box.zaps == 0);
	REQUIRE(box.zap_asks == 0);
	REQUIRE(box.order == "p");
	// The stream then takes the picture's frontend.
	REQUIRE(run(box, kLive, 5000, false, true).verdict == Granted);
}

TEST_CASE("a zap that fails goes back to the channel shown before, stops no PiP and answers that nothing was switched", "[zapit][streamholdcmd][switch]")
{
	Box box;
	fill(box);
	box.zap_fe = 0;
	box.f.slots = 2;
	box.f.slot[1].active = true;
	box.f.slot[1].channel = kOther;
	box.f.slot[1].fe = 2;
	box.zap_ok = false;
	REQUIRE(runSwitch(box, kOther) == 0);
	REQUIRE(box.switched == 0);
	REQUIRE(box.switch_answers == 1);
	REQUIRE(box.zaps == 2);
	REQUIRE(box.zapped_all.size() == 2);
	REQUIRE(box.zapped_all[0] == kOther);
	REQUIRE(box.zapped_all[1] == kLive);
	// The way back keeps the same set, the PiP's frontend among them.
	REQUIRE(box.zapped_keep == box.predicted_keep);
	REQUIRE(box.pip_stops == 0);
	REQUIRE(box.f.slot[1].active);
	REQUIRE(box.switch_unlocked);
	REQUIRE(box.lock_depth == 0);

	// A way back that fails as well still answers 0, and tries no third zap.
	box.zapped_all.clear();
	box.zap_back_ok = false;
	REQUIRE(runSwitch(box, kOther) == 0);
	REQUIRE(box.zapped_all.size() == 2);
	REQUIRE(box.pip_stops == 0);
}

TEST_CASE("a reply goes on the wire member for member", "[zapit][streamholdcmd][switch]")
{
	struct Wire
	{
		int32_t  verdict;
		uint32_t hold_id;
		int32_t  fenumber;
		int32_t  switch_frees;
		Wire() : verdict(3), hold_id(0), fenumber(-1), switch_frees(0) {}
	};
	Reply r;
	r.verdict = Busy;
	r.hold_id = 0x1234;
	r.fenumber = 2;
	r.switch_frees = 1;
	const Wire w = toWire<Wire>(r);
	REQUIRE(w.verdict == Busy);
	REQUIRE(w.hold_id == 0x1234u);
	REQUIRE(w.fenumber == 2);
	REQUIRE(w.switch_frees == 1);

	// The refusal a hold sends carries the prediction to the wire as well.
	Box box;
	fill(box);
	box.zap_fe = 0;
	const Wire sent = toWire<Wire>(run(box, kOther, 5000, false, true, true));
	REQUIRE(sent.verdict == Busy);
	REQUIRE(sent.switch_frees == 1);
}

TEST_CASE("a frontend that came free since the refusal is taken without a switch", "[zapit][streamholdcmd][switch]")
{
	Box box;   // frontends 1 and 2 free: a hold of the channel gets one as things are
	box.zap_fe = 0;
	REQUIRE(runSwitch(box, kOther) == 2);
	REQUIRE(box.switched == 2);
	REQUIRE(box.pip_stops == 0);
	REQUIRE(box.zaps == 0);
	REQUIRE(box.binds == 0);
	REQUIRE(box.uses() == 0);
	REQUIRE(box.lock_depth == 0);
	// And the live channel itself, once nothing keeps its stream off.
	REQUIRE(runSwitch(box, kLive) == 2);
	REQUIRE(box.zaps == 0);

	// No room for one more hold: switching would not help either.
	fill(box);
	box.room = false;
	REQUIRE(runSwitch(box, kOther) == 0);
	REQUIRE(box.zaps == 0);
	REQUIRE(box.pip_stops == 0);
}
