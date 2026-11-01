/*
 * test_recordtuner.cpp - the frontend a recording takes
 *
 * Copyright (C) 2026 Thilo Graf
 *
 * License: GPL-2.0-or-later
 *
 * recordtuner holds what CutBackNeutrino decides about the live frontend,
 * as plain functions. These cases drive the shipped object with the values
 * the recording reads off the box: whether the live frontend carries the
 * recording's transponder, and whether the recording then needs a full zap;
 * and the order in which it gets its frontend, ending streams on the way.
 */

#include "support/catch.hpp"

#include <driver/recordtuner.h>

#include <cstdio>
#include <string>
#include <vector>

namespace
{

const uint64_t kTpArd = 0x0001044100000000ULL;
const uint64_t kTpZdf = 0x000103f300000000ULL;

} // namespace

TEST_CASE("L1: an untuned live frontend whose transponder id is still 0 carries nothing", "[driver][recordtuner]")
{
	// What opening leaves: id 0, not tuned. sameTsidOnid() would say yes.
	REQUIRE_FALSE(recordtuner::liveServes(true, false, 0, kTpArd));
	// Tuned, but the id was never set: still not the channel's transponder.
	REQUIRE_FALSE(recordtuner::liveServes(true, true, 0, kTpArd));
	// No live frontend at all.
	REQUIRE_FALSE(recordtuner::liveServes(false, true, kTpArd, kTpArd));
}

TEST_CASE("L2: a live frontend tuned to another transponder does not carry the channel", "[driver][recordtuner]")
{
	REQUIRE_FALSE(recordtuner::liveServes(true, true, kTpZdf, kTpArd));
	// The transponder it was on before a failed tune counts for nothing.
	REQUIRE_FALSE(recordtuner::liveServes(true, false, kTpArd, kTpArd));
}

TEST_CASE("L3: a live frontend tuned to the channel's transponder carries it", "[driver][recordtuner]")
{
	REQUIRE(recordtuner::liveServes(true, true, kTpArd, kTpArd));
}

TEST_CASE("L4: the live frontend that does not carry the recording needs a full zap", "[driver][recordtuner]")
{
	/* The live channel id and the recorded one name the same transponder,
	   but the live frontend sits on another: the ids say nothing here. */
	const bool serves = recordtuner::liveServes(true, true, kTpZdf, kTpArd);
	REQUIRE(recordtuner::fullZap(true, false, serves));
	// Carrying it, the recording is only tuned.
	REQUIRE_FALSE(recordtuner::fullZap(true, false, recordtuner::liveServes(true, true, kTpArd, kTpArd)));
	// On another frontend the picture is never touched.
	REQUIRE_FALSE(recordtuner::fullZap(false, false, serves));
}

TEST_CASE("L5: beside a web channel the recording's frontend is only tuned", "[driver][recordtuner]")
{
	REQUIRE_FALSE(recordtuner::fullZap(true, true, false));
	REQUIRE_FALSE(recordtuner::fullZap(true, true, true));
}

/* recordtuner::choose: the order in which a recording gets its frontend,
   over a model that writes down every step and the locks held at it. */

namespace
{

struct Box : public recordtuner::Ops
{
	std::vector<std::string> steps;
	int lock_depth;
	int fe[3];               // the frontends; their addresses are what the choice sees
	int locks[3];            // the use count the choice raised on each
	int live;                // which of them carries the picture
	int live_after_yield;    // a zap while streams end moves the picture there; -1 none
	int live_after_read;     // a zap right after the next read of the live frontend; -1 none
	bool live_records;       // the live channel is the recorded one, carried by fe[carrier]
	int carrier;
	void *judged;            // the frontend liveRecords was asked about
	bool judged_locked;      // and whether the frontend manager's lock was held then
	int reads;               // reads of the live frontend
	std::vector<bool> allocations;   // what each allocation finds, in turn
	size_t allocated;
	bool yield_ok;
	int yield_stage;
	uint32_t yield_mask;
	bool yield_under_lock;

	Box()
		: lock_depth(0), live(0), live_after_yield(-1), live_after_read(-1), live_records(false), carrier(0),
		  judged(0), judged_locked(false), reads(0), allocated(0), yield_ok(true), yield_stage(2),
		  yield_mask(0x2), yield_under_lock(false)
	{
		for (int i = 0; i < 3; ++i)
		{
			fe[i] = i;
			locks[i] = 0;
		}
	}

	int locked() const { return locks[0] + locks[1] + locks[2]; }
	bool liveLocked() const { return locks[live] > 0; }

	void wake() { steps.push_back("wake"); }
	bool liveRecords(void *p)
	{
		steps.push_back("live");
		judged = p;
		judged_locked = lock_depth > 0;
		return live_records && p == &fe[carrier];
	}
	void lockFrontends() { lock_depth++; }
	void unlockFrontends() { lock_depth--; }
	void *liveFrontend()
	{
		reads++;
		void *p = &fe[live];
		if (live_after_read >= 0)
		{
			live = live_after_read;
			live_after_read = -1;
		}
		return p;
	}
	void lockFrontend(void *p) { locks[*static_cast<int *>(p)]++; }
	void unlockFrontend(void *p) { locks[*static_cast<int *>(p)]--; }
	bool allocate()
	{
		const bool got = allocated < allocations.size() && allocations[allocated];
		allocated++;
		std::string step = liveLocked() ? "allocate live locked" : "allocate live free";
		if (lock_depth <= 0)
			step += " without the lock";
		steps.push_back(got ? step : step + ": none");
		return got;
	}
	bool yield(int &stage, uint32_t &mask)
	{
		yield_under_lock = yield_under_lock || lock_depth > 0 || locked() > 0;
		steps.push_back("yield");
		if (live_after_yield >= 0)
			live = live_after_yield;
		stage = yield_stage;
		mask = yield_mask;
		return yield_ok;
	}
	void stopStreamsOn(uint32_t mask)
	{
		char buf[32];
		snprintf(buf, sizeof(buf), "stop 0x%x", mask);
		steps.push_back(buf);
	}

	int count(const std::string &step) const
	{
		int n = 0;
		for (size_t i = 0; i < steps.size(); ++i)
			if (steps[i] == step)
				n++;
		return n;
	}
};

std::vector<std::string> list(const char *a, const char *b = 0, const char *c = 0, const char *d = 0,
			      const char *e = 0, const char *f = 0)
{
	const char *all[] = { a, b, c, d, e, f };
	std::vector<std::string> v;
	for (size_t i = 0; i < 6 && all[i]; ++i)
		v.push_back(all[i]);
	return v;
}

} // namespace

TEST_CASE("F1: a free frontend is taken with the picture's kept, and no stream is asked to end", "[driver][recordtuner]")
{
	Box box;
	box.allocations.push_back(true);
	const recordtuner::Choice c = recordtuner::choose(box, false, false);
	REQUIRE(c.way == recordtuner::Allocated);
	REQUIRE(c.live_locked);
	REQUIRE_FALSE(c.streams_ended);
	REQUIRE(box.steps == list("live", "allocate live locked"));
	REQUIRE(box.lock_depth == 0);
}

TEST_CASE("F2: stage 2 ends the streams in the way and allocates with the picture kept", "[driver][recordtuner]")
{
	Box box;
	box.allocations.push_back(false);
	box.allocations.push_back(true);
	const recordtuner::Choice c = recordtuner::choose(box, false, false);
	REQUIRE(box.steps == list("live", "allocate live locked: none", "yield", "stop 0x2", "allocate live locked"));
	REQUIRE(c.way == recordtuner::Allocated);
	REQUIRE(c.live_locked);
	REQUIRE(c.live == &box.fe[0]);
	REQUIRE(c.streams_ended);
	REQUIRE(box.liveLocked());
	REQUIRE(box.locked() == 1);
	REQUIRE(box.lock_depth == 0);
}

TEST_CASE("F3: stage 3 allocates with the picture's frontend free", "[driver][recordtuner]")
{
	Box box;
	box.yield_stage = 3;
	box.yield_mask = 0x1;
	box.allocations.push_back(false);
	box.allocations.push_back(true);
	const recordtuner::Choice c = recordtuner::choose(box, false, false);
	REQUIRE(box.steps == list("live", "allocate live locked: none", "yield", "stop 0x1", "allocate live free"));
	REQUIRE(c.way == recordtuner::Allocated);
	REQUIRE_FALSE(c.live_locked);
	REQUIRE(c.streams_ended);
	REQUIRE(box.locked() == 0);
	// On the live frontend it does not carry, the recording zaps the picture.
	REQUIRE(recordtuner::fullZap(true, false, false));
}

TEST_CASE("F4: no answer from zapit stops nothing and allocates as before", "[driver][recordtuner]")
{
	Box box;
	box.yield_ok = false;
	box.allocations.push_back(false);
	box.allocations.push_back(true);
	const recordtuner::Choice c = recordtuner::choose(box, false, false);
	REQUIRE(box.steps == list("live", "allocate live locked: none", "yield", "allocate live free"));
	REQUIRE(c.way == recordtuner::Allocated);
	REQUIRE_FALSE(c.streams_ended);

	// Stage 0 and a refusal do the same.
	Box none;
	none.yield_stage = 0;
	none.yield_mask = 0;
	none.allocations.push_back(false);
	none.allocations.push_back(false);
	REQUIRE(recordtuner::choose(none, false, false).way == recordtuner::NoFrontend);
	REQUIRE(none.steps == list("live", "allocate live locked: none", "yield", "allocate live free: none"));
}

TEST_CASE("F5: in standby the streams end before zapit wakes, then the frontend is allocated", "[driver][recordtuner]")
{
	Box box;
	box.yield_mask = 0x1;
	box.allocations.push_back(true);
	const recordtuner::Choice c = recordtuner::choose(box, true, true);
	REQUIRE(box.steps == list("yield", "stop 0x1", "wake", "live", "allocate live locked"));
	REQUIRE(c.way == recordtuner::Allocated);
	REQUIRE(c.streams_ended);
}

TEST_CASE("F6: in standby nothing ends a second time after the wake", "[driver][recordtuner]")
{
	Box box;
	box.yield_mask = 0x1;
	box.allocations.push_back(false);
	box.allocations.push_back(false);
	const recordtuner::Choice c = recordtuner::choose(box, true, true);
	REQUIRE(box.steps == list("yield", "stop 0x1", "wake", "live", "allocate live locked: none", "allocate live free: none"));
	REQUIRE(box.count("yield") == 1);
	REQUIRE(c.way == recordtuner::NoFrontend);
	// The caller announces the streams' end: nothing was started for them.
	REQUIRE(c.streams_ended);
	REQUIRE(box.lock_depth == 0);
	REQUIRE(box.locked() == 0);
}

TEST_CASE("F6b: a recording that got its frontend after streams ended says so and announces nothing", "[driver][recordtuner]")
{
	Box box;
	box.yield_mask = 0x1;
	box.allocations.push_back(false);
	box.allocations.push_back(true);
	const recordtuner::Choice c = recordtuner::choose(box, true, true);
	REQUIRE(c.way == recordtuner::Allocated);
	REQUIRE(c.streams_ended);
	REQUIRE(box.steps == list("yield", "stop 0x1", "wake", "live", "allocate live locked: none", "allocate live free"));
}

TEST_CASE("F7: the live channel on a live frontend that carries it is recorded from it", "[driver][recordtuner]")
{
	Box box;
	box.live_records = true;
	const recordtuner::Choice c = recordtuner::choose(box, false, false);
	REQUIRE(c.way == recordtuner::LiveFrontend);
	REQUIRE(c.live == &box.fe[0]);
	REQUIRE_FALSE(c.live_locked);
	REQUIRE(box.steps == list("live"));
	REQUIRE(box.judged_locked);
	REQUIRE(box.lock_depth == 0);
}

TEST_CASE("F8: the live channel on a live frontend that does not carry it is allocated for", "[driver][recordtuner]")
{
	Box box;
	box.live_records = false;
	box.allocations.push_back(true);
	const recordtuner::Choice c = recordtuner::choose(box, false, false);
	REQUIRE(c.way == recordtuner::Allocated);
	REQUIRE(box.steps == list("live", "allocate live locked"));
}

TEST_CASE("F9: zapit is never asked while the frontend lock or the live frontend's lock is held", "[driver][recordtuner]")
{
	for (int standby = 0; standby < 2; ++standby)
	{
		for (int stage = 0; stage <= 3; ++stage)
		{
			Box box;
			box.yield_stage = stage;
			box.allocations.push_back(false);
			box.allocations.push_back(false);
			box.allocations.push_back(false);
			recordtuner::choose(box, standby != 0, standby != 0);
			REQUIRE(box.count("yield") == 1);
			REQUIRE_FALSE(box.yield_under_lock);
			for (size_t i = 0; i < box.steps.size(); ++i)
				REQUIRE(box.steps[i].find("without the lock") == std::string::npos);
			REQUIRE(box.lock_depth == 0);
		}
	}
}

TEST_CASE("F10: a zap while the streams end moves the picture: the choice follows it", "[driver][recordtuner]")
{
	Box box;
	box.live_after_yield = 1;
	box.allocations.push_back(false);
	box.allocations.push_back(true);

	SECTION("stage 2: the new live frontend is the one locked, and the one reported")
	{
		const recordtuner::Choice c = recordtuner::choose(box, false, false);
		REQUIRE(box.steps == list("live", "allocate live locked: none", "yield", "stop 0x2", "allocate live locked"));
		REQUIRE(c.way == recordtuner::Allocated);
		REQUIRE(c.live_locked);
		REQUIRE(c.live == &box.fe[1]);
		REQUIRE(box.locks[0] == 0);
		REQUIRE(box.locks[1] == 1);
	}
	SECTION("stage 3: the frontend found beside is the new live one")
	{
		box.yield_stage = 3;
		box.yield_mask = 0x1;
		const recordtuner::Choice c = recordtuner::choose(box, false, false);
		REQUIRE(box.steps == list("live", "allocate live locked: none", "yield", "stop 0x1", "allocate live free"));
		REQUIRE_FALSE(c.live_locked);
		REQUIRE(c.live == &box.fe[1]);
		REQUIRE(box.locked() == 0);
	}
}

TEST_CASE("F11: the live frontend asked about is the one the choice reports, though a zap follows the read", "[driver][recordtuner]")
{
	Box box;
	box.live_after_read = 1;

	SECTION("it carries the recorded live channel: recorded off that one")
	{
		box.live_records = true;
		const recordtuner::Choice c = recordtuner::choose(box, false, false);
		REQUIRE(c.way == recordtuner::LiveFrontend);
		REQUIRE(box.judged == &box.fe[0]);
		REQUIRE(box.judged_locked);
		REQUIRE(c.live == box.judged);
		REQUIRE(box.reads == 1);
		REQUIRE(box.lock_depth == 0);
	}
	SECTION("it does not carry it: the first try locks that one")
	{
		box.live_records = true;
		box.carrier = 1;    // the picture moves to the frontend that carries it
		box.allocations.push_back(true);
		const recordtuner::Choice c = recordtuner::choose(box, false, false);
		REQUIRE(c.way == recordtuner::Allocated);
		REQUIRE(box.judged == &box.fe[0]);
		REQUIRE(box.judged_locked);
		REQUIRE(c.live == box.judged);
		REQUIRE(c.live_locked);
		REQUIRE(box.locks[0] == 1);
		REQUIRE(box.locks[1] == 0);
		REQUIRE(box.reads == 1);
	}
}
