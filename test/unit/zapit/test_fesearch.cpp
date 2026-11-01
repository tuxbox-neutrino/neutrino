/*
 * test_fesearch.cpp - the frontend a stream may take
 *
 * Copyright (C) 2026 Thilo Graf
 *
 * License: GPL-2.0-or-later
 *
 * fesearch::pickIdle is the walk the frontend manager makes for a stream,
 * on plain data. These cases drive the shipped object with frontends laid
 * out the way the adapter in CFEManager lays them out: in the order of the
 * frontend map, a master listing itself first among its links.
 */

#include "support/catch.hpp"

#include <zapit/fesearch.h>

#include <vector>

using fesearch::FeView;

namespace
{

FeView independent(int number)
{
	FeView v;
	v.number = number;
	v.mode = FeView::Independent;
	v.usable = true;
	v.same_pos = true;
	return v;
}

FeView master(int number, bool loop, bool rotor)
{
	FeView v = independent(number);
	v.mode = FeView::Master;
	v.have_loop = loop;
	v.have_rotor = rotor;
	return v;
}

// A member reached through its master only, as the adapter marks every
// linked frontend.
FeView member(int number, bool twin)
{
	FeView v;
	v.number = number;
	v.mode = FeView::Other;
	v.usable = false;
	v.same_pos = true;
	v.is_twin = twin;
	return v;
}

// Links a master at index m to itself and to the members at the indices given.
void link(std::vector<FeView> &fes, int m, int a, int b = -1)
{
	fes[m].links.clear();
	fes[m].links.push_back(m);
	fes[m].links.push_back(a);
	if (b >= 0)
		fes[m].links.push_back(b);
}

} // namespace

TEST_CASE("an idle independent frontend is taken, the first in the map order", "[zapit][fesearch]")
{
	std::vector<FeView> fes;
	fes.push_back(independent(0));
	fes.push_back(independent(1));
	REQUIRE(fesearch::pickIdle(fes) == 0);

	fes[0].locked = true;
	REQUIRE(fesearch::pickIdle(fes) == 1);
}

TEST_CASE("a locked frontend is never chosen, even on the channel's transponder", "[zapit][fesearch]")
{
	/* The walk this mirrors hands a busy frontend back when it already sits on
	   the channel's transponder. Nothing in a view says which transponder a
	   frontend sits on, so there is nothing such a share could be decided
	   from: a locked frontend is out, whatever it is tuned to. */
	std::vector<FeView> fes;
	fes.push_back(independent(0));
	fes[0].locked = true;
	REQUIRE(fesearch::pickIdle(fes) == -1);

	fes.push_back(independent(1));
	fes[1].locked = true;
	REQUIRE(fesearch::pickIdle(fes) == -1);
}

TEST_CASE("a frontend the channel cannot be received on is skipped", "[zapit][fesearch]")
{
	std::vector<FeView> fes;
	fes.push_back(independent(0));
	fes.push_back(independent(1));
	// Another delivery system, or the position not configured on it
	fes[0].usable = false;
	REQUIRE(fesearch::pickIdle(fes) == 1);

	fes[1].usable = false;
	REQUIRE(fesearch::pickIdle(fes) == -1);
}

TEST_CASE("an unused or linked frontend is only reached through its master", "[zapit][fesearch]")
{
	std::vector<FeView> fes;
	fes.push_back(member(0, false));
	fes.push_back(member(1, true));
	// Not linked to anything: nothing reaches them.
	REQUIRE(fesearch::pickIdle(fes) == -1);
}

TEST_CASE("a free member comes before a free twin on a master", "[zapit][fesearch]")
{
	std::vector<FeView> fes;
	fes.push_back(master(0, false, false));
	fes.push_back(member(1, true));
	fes.push_back(member(2, false));
	link(fes, 0, 1, 2);

	fes[0].locked = true;
	// The twin is visited first and still loses to the plain member.
	REQUIRE(fesearch::pickIdle(fes) == 2);

	fes[2].locked = true;
	REQUIRE(fesearch::pickIdle(fes) == 1);

	fes[1].locked = true;
	REQUIRE(fesearch::pickIdle(fes) == -1);
}

TEST_CASE("the master is its own first member", "[zapit][fesearch]")
{
	std::vector<FeView> fes;
	fes.push_back(master(0, false, false));
	fes.push_back(member(1, false));
	link(fes, 0, 1);
	REQUIRE(fesearch::pickIdle(fes) == 0);
}

TEST_CASE("a locked loop member on another band or polarisation leaves only the twins", "[zapit][fesearch]")
{
	std::vector<FeView> fes;
	fes.push_back(master(0, true, false));
	fes.push_back(member(1, false));
	fes.push_back(member(2, true));
	link(fes, 0, 1, 2);

	fes[0].locked = true;
	fes[0].loop_can_tune = false;
	// The free loop member would have to change the band the master holds.
	REQUIRE(fesearch::pickIdle(fes) == 2);

	// Holding a band that suits the channel, the loop member is free.
	fes[0].loop_can_tune = true;
	REQUIRE(fesearch::pickIdle(fes) == 1);

	// Without a loop, the band does not matter.
	fes[0].loop_can_tune = false;
	fes[0].have_loop = false;
	REQUIRE(fesearch::pickIdle(fes) == 1);
}

TEST_CASE("a loop member visited before the conflict is let go again", "[zapit][fesearch]")
{
	// The free member is seen first; the locked one after it still rules it
	// out, because both share the master's cable.
	std::vector<FeView> fes;
	fes.push_back(master(0, true, false));
	fes.push_back(member(1, false));
	fes.push_back(member(2, false));
	link(fes, 0, 1, 2);

	fes[2].locked = true;
	fes[2].loop_can_tune = false;
	REQUIRE(fesearch::pickIdle(fes) == -1);
}

TEST_CASE("a rotor a locked member keeps elsewhere frees no member of that master", "[zapit][fesearch]")
{
	std::vector<FeView> fes;
	fes.push_back(master(0, false, true));
	fes.push_back(member(1, true));
	fes.push_back(independent(2));
	link(fes, 0, 1);

	fes[0].locked = true;
	fes[0].same_pos = false;
	// Not the twin of the rotor master, the next frontend in the map
	REQUIRE(fesearch::pickIdle(fes) == 2);

	// The dish already points where the channel is: the twin may take it.
	fes[0].same_pos = true;
	REQUIRE(fesearch::pickIdle(fes) == 1);
}

TEST_CASE("an idle live frontend nobody locked is passed over when it is held", "[zapit][fesearch]")
{
	/* The picture runs on frontend 0 without a use count: live TV does not
	   lock its frontend. Held by the adapter, it is out, and the free spare
	   is taken rather than the frontend the picture needs. */
	std::vector<FeView> fes;
	fes.push_back(independent(0));
	fes.push_back(independent(1));
	REQUIRE(fesearch::pickIdle(fes) == 0);

	fes[0].locked = true; // held: it carries the picture
	REQUIRE(fesearch::pickIdle(fes) == 1);
}

TEST_CASE("live on frontend 0 and PiP on frontend 1 send a stream to frontend 2", "[zapit][fesearch]")
{
	/* The picture on frontend 0, the PiP slot 0 on frontend 1 and reading
	   demux 1 from it. Frontend 0 is held twice over: for the picture, and
	   because a stream on it would read demux 1 and set that demux's source
	   back to frontend 0 under the PiP. Frontend 1 is held for the PiP. */
	std::vector<FeView> fes;
	fes.push_back(independent(0));
	fes.push_back(independent(1));
	fes.push_back(independent(2));
	fes[0].locked = true;
	fes[1].locked = true;

	const int got = fesearch::pickIdle(fes);
	REQUIRE(got == 2);

	const fesearch::Binding b = fesearch::binding(fes[got].number);
	REQUIRE(b.demux == 3);
	REQUIRE(b.source == 2);
	// The demux the PiP reads is not the one the stream binds.
	REQUIRE(b.demux != 1);
}

TEST_CASE("a stream on frontend n reads demux n + 1 with n as its source", "[zapit][fesearch]")
{
	for (int n = 0; n < 4; ++n)
	{
		const fesearch::Binding b = fesearch::binding(n);
		REQUIRE(b.demux == n + 1);
		REQUIRE(b.source == n);
	}
}

TEST_CASE("an index outside the list in a master's links is ignored", "[zapit][fesearch]")
{
	std::vector<FeView> fes;
	fes.push_back(master(0, false, false));
	fes[0].links.push_back(7);
	fes[0].links.push_back(0);
	REQUIRE(fesearch::pickIdle(fes) == 0);
}

/* streamsToEnd: the streams a recording that found no frontend has to end.
   The views are laid out as above; the uses say what holds each frontend. */

namespace
{

using fesearch::Use;
using fesearch::StreamsToEnd;

// Held by stream holds alone, each of them one of its use count.
void streamsOnly(std::vector<FeView> &fes, std::vector<Use> &use, int i, int streams = 1)
{
	fes[i].locked = true;
	use[i].streams = streams;
}

// Held by something a recording does not end, with or without streams.
void kept(std::vector<FeView> &fes, std::vector<Use> &use, int i, int streams = 0)
{
	fes[i].locked = true;
	use[i].streams = streams;
	use[i].kept = true;
}

std::vector<int> numbers(int a = -1, int b = -1)
{
	std::vector<int> v;
	if (a >= 0)
		v.push_back(a);
	if (b >= 0)
		v.push_back(b);
	return v;
}

// The picture on a frontend the channel is not received on.
void liveElsewhere(std::vector<FeView> &fes, std::vector<Use> &use)
{
	FeView v = independent((int) fes.size());
	v.usable = false;
	fes.push_back(v);
	Use u;
	u.live = true;
	use.push_back(u);
}

} // namespace

TEST_CASE("O1: a frontend only streams hold is ended before the picture's", "[zapit][fesearch][streamstoend]")
{
	std::vector<FeView> fes;
	fes.push_back(independent(0));
	fes.push_back(independent(1));
	std::vector<Use> use(2);
	use[0].live = true;
	streamsOnly(fes, use, 1);

	const StreamsToEnd r = fesearch::streamsToEnd(fes, use, true, false);
	REQUIRE(r.stage == 2);
	REQUIRE(r.end == numbers(1));
}

TEST_CASE("O2: a free frontend ends nothing", "[zapit][fesearch][streamstoend]")
{
	std::vector<FeView> fes;
	fes.push_back(independent(0));
	fes.push_back(independent(1));
	std::vector<Use> use(2);
	use[0].live = true;

	const StreamsToEnd r = fesearch::streamsToEnd(fes, use, true, false);
	REQUIRE(r.stage == 1);
	REQUIRE(r.end.empty());
}

TEST_CASE("O3: one tuner with the picture and a stream of it gives up the picture", "[zapit][fesearch][streamstoend]")
{
	std::vector<FeView> fes;
	fes.push_back(independent(0));
	std::vector<Use> use(1);
	streamsOnly(fes, use, 0);   // the stream of the live channel locks it

	SECTION("the picture runs: stage 3, its stream ends")
	{
		use[0].live = true;
		const StreamsToEnd r = fesearch::streamsToEnd(fes, use, true, false);
		REQUIRE(r.stage == 3);
		REQUIRE(r.end == numbers(0));
	}
	SECTION("in standby there is no picture to keep: stage 2")
	{
		const StreamsToEnd r = fesearch::streamsToEnd(fes, use, false, false);
		REQUIRE(r.stage == 2);
		REQUIRE(r.end == numbers(0));
	}
}

TEST_CASE("O4: a stream's frontend on the channel's transponder is shared", "[zapit][fesearch][streamstoend]")
{
	std::vector<FeView> fes;
	fes.push_back(independent(0));
	fes.push_back(independent(1));
	std::vector<Use> use(2);
	use[0].live = true;
	streamsOnly(fes, use, 1);
	use[1].on_tp = true;

	const StreamsToEnd r = fesearch::streamsToEnd(fes, use, true, false);
	REQUIRE(r.stage == 1);
	REQUIRE(r.end.empty());
}

TEST_CASE("O4b: without sharing frontends only the same service is shared", "[zapit][fesearch][streamstoend]")
{
	std::vector<FeView> fes;
	fes.push_back(independent(0));
	fes.push_back(independent(1));
	std::vector<Use> use(2);
	use[0].live = true;
	streamsOnly(fes, use, 1);
	use[1].on_tp = true;

	SECTION("another service on the transponder: its stream ends")
	{
		const StreamsToEnd r = fesearch::streamsToEnd(fes, use, true, true);
		REQUIRE(r.stage == 2);
		REQUIRE(r.end == numbers(1));
	}
	SECTION("the same service: shared")
	{
		use[1].same_service = true;
		const StreamsToEnd r = fesearch::streamsToEnd(fes, use, true, true);
		REQUIRE(r.stage == 1);
		REQUIRE(r.end.empty());
	}
}

TEST_CASE("O5: a frontend a recording and a stream share is not one whose streams end", "[zapit][fesearch][streamstoend]")
{
	std::vector<FeView> fes;
	fes.push_back(independent(0));
	fes.push_back(independent(1));
	std::vector<Use> use(2);
	use[0].live = true;
	kept(fes, use, 1, 1);

	const StreamsToEnd r = fesearch::streamsToEnd(fes, use, true, false);
	// No stage 2: only the picture's own frontend is left.
	REQUIRE(r.stage == 3);
	REQUIRE(r.end.empty());
}

TEST_CASE("O6: a PiP's frontend is passed over for one only streams hold", "[zapit][fesearch][streamstoend]")
{
	std::vector<FeView> fes;
	fes.push_back(independent(0));
	fes.push_back(independent(1));
	fes.push_back(independent(2));
	std::vector<Use> use(3);
	use[0].live = true;
	kept(fes, use, 1, 1);       // the PiP shows from it; a stream holds it too
	streamsOnly(fes, use, 2);

	const StreamsToEnd r = fesearch::streamsToEnd(fes, use, true, false);
	REQUIRE(r.stage == 2);
	REQUIRE(r.end == numbers(2));
}

TEST_CASE("O7: a loop member on the picture's band blocks: the picture is given up", "[zapit][fesearch][streamstoend]")
{
	/* Master 0 carries the picture on band A, its loop member 1 a stream on
	   band A, the channel is on band B. With the picture kept, the member's
	   stream would not help: the picture's frontend keeps the cable on A. */
	std::vector<FeView> fes;
	fes.push_back(master(0, true, false));
	fes.push_back(member(1, false));
	link(fes, 0, 1);
	fes[0].loop_can_tune = false;
	fes[1].loop_can_tune = false;
	std::vector<Use> use(2);
	use[0].live = true;
	streamsOnly(fes, use, 1);

	const StreamsToEnd r = fesearch::streamsToEnd(fes, use, true, false);
	REQUIRE(r.stage == 3);
	REQUIRE(r.end == numbers(1));
}

TEST_CASE("O8: the same with a twin member ends only the twin's stream", "[zapit][fesearch][streamstoend]")
{
	std::vector<FeView> fes;
	fes.push_back(master(0, true, false));
	fes.push_back(member(1, true));
	link(fes, 0, 1);
	fes[0].loop_can_tune = false;
	fes[1].loop_can_tune = false;
	std::vector<Use> use(2);
	use[0].live = true;
	streamsOnly(fes, use, 1);

	const StreamsToEnd r = fesearch::streamsToEnd(fes, use, true, false);
	REQUIRE(r.stage == 2);
	REQUIRE(r.end == numbers(1));
}

TEST_CASE("O9: a free loop master needs every member on the other band ended", "[zapit][fesearch][streamstoend]")
{
	std::vector<FeView> fes;
	fes.push_back(master(0, true, false));
	fes.push_back(member(1, false));
	fes.push_back(member(2, false));
	link(fes, 0, 1, 2);
	fes[1].loop_can_tune = false;
	fes[2].loop_can_tune = false;
	std::vector<Use> use(3);
	streamsOnly(fes, use, 1);
	streamsOnly(fes, use, 2);
	liveElsewhere(fes, use);

	const StreamsToEnd r = fesearch::streamsToEnd(fes, use, true, false);
	REQUIRE(r.stage == 2);
	REQUIRE(r.end == numbers(1, 2));
}

TEST_CASE("O10: a stream that keeps a rotor elsewhere is ended for a free member", "[zapit][fesearch][streamstoend]")
{
	std::vector<FeView> fes;
	fes.push_back(master(0, false, true));
	fes.push_back(member(1, false));
	link(fes, 0, 1);
	fes[0].same_pos = false;    // the dish points at X, the channel is on Y
	std::vector<Use> use(2);
	streamsOnly(fes, use, 0);
	liveElsewhere(fes, use);

	const StreamsToEnd r = fesearch::streamsToEnd(fes, use, true, false);
	REQUIRE(r.stage == 2);
	REQUIRE(r.end == numbers(0));
}

TEST_CASE("O11: a recording that keeps a rotor elsewhere leaves nothing to end", "[zapit][fesearch][streamstoend]")
{
	std::vector<FeView> fes;
	fes.push_back(master(0, false, true));
	fes.push_back(member(1, false));
	link(fes, 0, 1);
	fes[0].same_pos = false;
	std::vector<Use> use(2);
	kept(fes, use, 0);
	liveElsewhere(fes, use);

	const StreamsToEnd r = fesearch::streamsToEnd(fes, use, true, false);
	REQUIRE(r.stage == 0);
	REQUIRE(r.end.empty());
}

TEST_CASE("O12: frontends the channel cannot be received on end nothing", "[zapit][fesearch][streamstoend]")
{
	std::vector<FeView> fes;
	fes.push_back(independent(0));
	fes.push_back(member(1, false));    // reached through a master only
	std::vector<Use> use(2);
	streamsOnly(fes, use, 0);
	streamsOnly(fes, use, 1);
	fes[0].usable = false;

	const StreamsToEnd r = fesearch::streamsToEnd(fes, use, false, false);
	REQUIRE(r.stage == 0);
	REQUIRE(r.end.empty());
}

TEST_CASE("O13: the way that ends the fewest frontends wins, then the walk's order", "[zapit][fesearch][streamstoend]")
{
	// A loop master that needs two members ended, then two frontends that
	// need one each.
	std::vector<FeView> fes;
	fes.push_back(master(0, true, false));
	fes.push_back(member(1, false));
	fes.push_back(member(2, false));
	link(fes, 0, 1, 2);
	fes[1].loop_can_tune = false;
	fes[2].loop_can_tune = false;
	fes.push_back(independent(3));
	fes.push_back(independent(4));
	std::vector<Use> use(5);
	streamsOnly(fes, use, 1);
	streamsOnly(fes, use, 2);
	streamsOnly(fes, use, 3);
	streamsOnly(fes, use, 4);
	liveElsewhere(fes, use);

	const StreamsToEnd r = fesearch::streamsToEnd(fes, use, true, false);
	REQUIRE(r.stage == 2);
	REQUIRE(r.end == numbers(3));
}

TEST_CASE("O14: in standby the frontend the picture had is one whose streams end", "[zapit][fesearch][streamstoend]")
{
	std::vector<FeView> fes;
	fes.push_back(independent(0));
	fes.push_back(independent(1));
	std::vector<Use> use(2);
	streamsOnly(fes, use, 0);   // the old live frontend, a stream on it
	streamsOnly(fes, use, 1);

	const StreamsToEnd r = fesearch::streamsToEnd(fes, use, false, false);
	REQUIRE(r.stage == 2);
	REQUIRE(r.end == numbers(0));
}

TEST_CASE("O15: a frontend is kept by any use beyond the holds that lock it", "[zapit][fesearch][streamstoend]")
{
	// A recording and a hold on one frontend.
	SECTION("holds that do not lock: the use count is the recording's")
	{
		REQUIRE(fesearch::keptBy(1, 1, false));
		REQUIRE_FALSE(fesearch::keptBy(0, 1, false));
	}
	SECTION("holds that lock: kept only past the holds")
	{
		REQUIRE_FALSE(fesearch::keptBy(1, 1, true));
		REQUIRE(fesearch::keptBy(2, 1, true));
		REQUIRE_FALSE(fesearch::keptBy(2, 2, true));
	}
}
