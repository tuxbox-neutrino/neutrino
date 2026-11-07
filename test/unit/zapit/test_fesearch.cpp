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

/* FBC: one input feeds the tuners of a set. A root tunes the input, and a
   link tied to it with the link flag tunes from what the root tuned. An
   automatic link tunes only from a root that already carries the channel's
   input, and a root does not move its input from under a locked link that
   could not follow. The views say so through root_carries and fbc_links; a
   box without FBC leaves both at their defaults. */

namespace
{

// The views at the indices given hang on the root at index r with the link flag.
void tie(std::vector<FeView> &fes, int r, int a, int b = -1, int c = -1)
{
	fes[r].fbc_links.clear();
	fes[r].fbc_links.push_back(a);
	if (b >= 0)
		fes[r].fbc_links.push_back(b);
	if (c >= 0)
		fes[r].fbc_links.push_back(c);
}

// An automatic link no root of its set carries the channel's input for.
FeView uncarried(int number)
{
	FeView v = independent(number);
	v.root_carries = false;
	return v;
}

std::vector<int> numbersFrom(int first, int last)
{
	std::vector<int> v;
	for (int n = first; n <= last; ++n)
		v.push_back(n);
	return v;
}

} // namespace

TEST_CASE("F1: a link whose input no root carries is not chosen", "[zapit][fesearch][fbc]")
{
	/* Roots 0 and 1 are busy and tuned to nothing link 2 could share: tied
	   to either, it would take that root's input away. */
	std::vector<FeView> fes;
	fes.push_back(independent(0));
	fes.push_back(independent(1));
	fes.push_back(uncarried(2));
	fes[0].locked = true;
	fes[1].locked = true;
	REQUIRE(fesearch::pickIdle(fes) == -1);

	// A root that carries the channel's input lets the link be taken.
	fes[2].root_carries = true;
	REQUIRE(fesearch::pickIdle(fes) == 2);
}

TEST_CASE("F2: a free root comes before the links of its set", "[zapit][fesearch][fbc]")
{
	SECTION("the free root first in the map, not the free link after it")
	{
		std::vector<FeView> fes;
		fes.push_back(independent(0));
		fes.push_back(independent(1));
		fes.push_back(independent(2));
		fes[1].locked = true;
		REQUIRE(fesearch::pickIdle(fes) == 0);
	}
	SECTION("a link no root carries for gives way to the free root after it")
	{
		std::vector<FeView> fes;
		fes.push_back(uncarried(0));
		fes.push_back(independent(1));
		REQUIRE(fesearch::pickIdle(fes) == 1);
	}
}

TEST_CASE("F3: a free root with a locked link on another band is passed over", "[zapit][fesearch][fbc]")
{
	std::vector<FeView> fes;
	fes.push_back(independent(0));  // root, free
	fes.push_back(independent(1));  // root, busy
	fes.push_back(independent(2));  // link tied to root 0
	fes.push_back(independent(3));  // link, free
	tie(fes, 0, 2);
	fes[1].locked = true;
	fes[2].locked = true;
	fes[2].loop_can_tune = false;
	REQUIRE(fesearch::pickIdle(fes) == 3);

	// The link holds a band the channel is on: the root may tune.
	fes[2].loop_can_tune = true;
	REQUIRE(fesearch::pickIdle(fes) == 0);

	// A free link holds nothing.
	fes[2].loop_can_tune = false;
	fes[2].locked = false;
	REQUIRE(fesearch::pickIdle(fes) == 0);
}

TEST_CASE("F4: a free master a link blocks leaves only its twins", "[zapit][fesearch][fbc]")
{
	std::vector<FeView> fes;
	fes.push_back(master(0, true, false));
	fes.push_back(member(1, false));  // loop
	fes.push_back(member(2, true));   // twin
	fes.push_back(independent(3));    // link tied to the master
	link(fes, 0, 1, 2);
	tie(fes, 0, 3);
	fes[3].locked = true;
	fes[3].loop_can_tune = false;
	// Not the free loop member: the master is its first free member and out.
	REQUIRE(fesearch::pickIdle(fes) == 2);

	fes[2].locked = true;
	REQUIRE(fesearch::pickIdle(fes) == -1);

	fes[2].locked = false;
	fes[3].loop_can_tune = true;
	REQUIRE(fesearch::pickIdle(fes) == 0);
}

TEST_CASE("F5: a link blocks the master itself, not a loop member of a locked master", "[zapit][fesearch][fbc]")
{
	std::vector<FeView> fes;
	fes.push_back(master(0, true, false));
	fes.push_back(member(1, false));  // loop
	fes.push_back(member(2, true));   // twin
	fes.push_back(independent(3));    // link tied to the master
	link(fes, 0, 1, 2);
	tie(fes, 0, 3);
	fes[0].locked = true;             // on a band the channel is on
	fes[3].locked = true;
	fes[3].loop_can_tune = false;
	REQUIRE(fesearch::pickIdle(fes) == 1);
}

TEST_CASE("F6: without FBC the walk is the one it was", "[zapit][fesearch][fbc]")
{
	const FeView none;
	REQUIRE(none.root_carries);
	REQUIRE(none.fbc_links.empty());

	// Two satellite tuners, the second unused, and a terrestrial one.
	std::vector<FeView> fes;
	fes.push_back(independent(0));
	FeView unused;
	unused.number = 1;
	fes.push_back(unused);
	fes.push_back(independent(2));
	fes[2].usable = false;            // a satellite channel
	fes[0].locked = true;             // held
	REQUIRE(fesearch::pickIdle(fes) == -1);

	fes[0].locked = false;
	REQUIRE(fesearch::pickIdle(fes) == 0);
}

TEST_CASE("S1: a free link no root carries for is not found as it is", "[zapit][fesearch][streamstoend][fbc]")
{
	std::vector<FeView> fes;
	fes.push_back(independent(0));    // root, streams on another transponder
	fes.push_back(uncarried(1));
	std::vector<Use> use(2);
	streamsOnly(fes, use, 0);

	const StreamsToEnd r = fesearch::streamsToEnd(fes, use, false, false);
	REQUIRE(r.stage == 2);
	REQUIRE(r.end == numbers(0));
}

TEST_CASE("S2: a busy link on the channel's transponder is shared whatever its roots carry", "[zapit][fesearch][streamstoend][fbc]")
{
	std::vector<FeView> fes;
	fes.push_back(uncarried(0));
	std::vector<Use> use(1);
	streamsOnly(fes, use, 0);
	use[0].on_tp = true;

	const StreamsToEnd r = fesearch::streamsToEnd(fes, use, false, false);
	REQUIRE(r.stage == 1);
	REQUIRE(r.end.empty());
}

TEST_CASE("S3: the streams on a link that cannot serve the recording are not ended", "[zapit][fesearch][streamstoend][fbc]")
{
	std::vector<FeView> fes;
	fes.push_back(independent(0));
	fes.push_back(uncarried(1));
	std::vector<Use> use(2);
	kept(fes, use, 0);
	streamsOnly(fes, use, 1);

	const StreamsToEnd r = fesearch::streamsToEnd(fes, use, false, false);
	REQUIRE(r.stage == 0);
	REQUIRE(r.end.empty());
}

TEST_CASE("S4: a root is free once the streams on its link on another band end", "[zapit][fesearch][streamstoend][fbc]")
{
	std::vector<FeView> fes;
	fes.push_back(independent(0));    // root, free
	fes.push_back(uncarried(1));      // link tied to it
	tie(fes, 0, 1);
	fes[1].loop_can_tune = false;
	std::vector<Use> use(2);

	SECTION("streams alone hold the link: they end")
	{
		streamsOnly(fes, use, 1);
		const StreamsToEnd r = fesearch::streamsToEnd(fes, use, false, false);
		REQUIRE(r.stage == 2);
		REQUIRE(r.end == numbers(1));
	}
	SECTION("the link is kept: no stream frees the root")
	{
		kept(fes, use, 1);
		const StreamsToEnd r = fesearch::streamsToEnd(fes, use, false, false);
		REQUIRE(r.stage == 0);
		REQUIRE(r.end.empty());
	}
}

TEST_CASE("S5: with the picture on a link, stage 3 ends the streams of its root", "[zapit][fesearch][streamstoend][fbc]")
{
	std::vector<FeView> fes;
	fes.push_back(independent(0));    // root, streams
	fes.push_back(uncarried(1));      // link tied to it, the picture
	tie(fes, 0, 1);
	fes[1].loop_can_tune = false;
	std::vector<Use> use(2);
	streamsOnly(fes, use, 0);
	use[1].live = true;               // the picture locks nothing

	const StreamsToEnd r = fesearch::streamsToEnd(fes, use, true, false);
	REQUIRE(r.stage == 3);
	REQUIRE(r.end == numbers(0));
}

TEST_CASE("S6: a stream on a link of the picture's root gives way to the recording", "[zapit][fesearch][streamstoend][fbc]")
{
	std::vector<FeView> fes;
	fes.push_back(independent(0));    // root, the picture
	fes.push_back(independent(1));    // root, a recording
	fes.push_back(uncarried(2));      // link tied to root 0, a stream
	fes.push_back(uncarried(3));      // link, free
	tie(fes, 0, 2);
	fes[2].loop_can_tune = false;
	std::vector<Use> use(4);
	use[0].live = true;
	kept(fes, use, 1);
	streamsOnly(fes, use, 2);

	const StreamsToEnd r = fesearch::streamsToEnd(fes, use, true, false);
	REQUIRE(r.stage == 3);
	REQUIRE(r.end == numbers(2));
}

TEST_CASE("S7: a free master a kept link blocks offers no member as it is, a free twin does", "[zapit][fesearch][streamstoend][fbc]")
{
	std::vector<FeView> fes;
	fes.push_back(master(0, true, false));
	fes.push_back(member(1, false));  // loop, free
	fes.push_back(uncarried(2));      // link tied to the master
	link(fes, 0, 1);
	tie(fes, 0, 2);
	fes[2].loop_can_tune = false;
	std::vector<Use> use(3);
	kept(fes, use, 2);

	SECTION("no twin: nothing, whatever streams end")
	{
		const StreamsToEnd r = fesearch::streamsToEnd(fes, use, false, false);
		REQUIRE(r.stage == 0);
		REQUIRE(r.end.empty());
	}
	SECTION("a free twin is found as it is")
	{
		fes.push_back(member(3, true));
		use.push_back(Use());
		link(fes, 0, 1, 3);
		const StreamsToEnd r = fesearch::streamsToEnd(fes, use, false, false);
		REQUIRE(r.stage == 1);
		REQUIRE(r.end.empty());
	}
	SECTION("the master held on a band the channel is on: its free loop member is found as it is")
	{
		kept(fes, use, 0);
		const StreamsToEnd r = fesearch::streamsToEnd(fes, use, false, false);
		REQUIRE(r.stage == 1);
		REQUIRE(r.end.empty());
	}
}

TEST_CASE("S8: a loop member needs the links of its master out of the way", "[zapit][fesearch][streamstoend][fbc]")
{
	std::vector<FeView> fes;
	fes.push_back(master(0, true, false));
	fes.push_back(member(1, false));  // loop, streams on another band
	fes.push_back(uncarried(2));      // link tied to the master, another band
	link(fes, 0, 1);
	tie(fes, 0, 2);
	fes[1].loop_can_tune = false;
	fes[2].loop_can_tune = false;
	std::vector<Use> use(3);
	streamsOnly(fes, use, 1);

	SECTION("the link is kept: no way")
	{
		kept(fes, use, 2);
		const StreamsToEnd r = fesearch::streamsToEnd(fes, use, false, false);
		REQUIRE(r.stage == 0);
		REQUIRE(r.end.empty());
	}
	SECTION("streams alone hold the link: both end")
	{
		streamsOnly(fes, use, 2);
		const StreamsToEnd r = fesearch::streamsToEnd(fes, use, false, false);
		REQUIRE(r.stage == 2);
		REQUIRE(r.end == numbers(1, 2));
	}
}

TEST_CASE("S9: a PiP on a link holds its root against a stream, not against a recording", "[zapit][fesearch][streamstoend][fbc]")
{
	std::vector<FeView> fes;
	fes.push_back(independent(0));    // root, free
	fes.push_back(uncarried(1));      // link tied to it, the PiP
	tie(fes, 0, 1);
	fes[1].loop_can_tune = false;

	SECTION("a stream: the PiP's frontend is held, so its root is out")
	{
		fes[1].locked = true;
		REQUIRE(fesearch::pickIdle(fes) == -1);
	}
	SECTION("a recording: the PiP locks nothing, the root is found as it is")
	{
		std::vector<Use> use(2);
		use[1].kept = true;
		liveElsewhere(fes, use);
		const StreamsToEnd r = fesearch::streamsToEnd(fes, use, true, false);
		REQUIRE(r.stage == 1);
		REQUIRE(r.end.empty());
	}
}

TEST_CASE("S10: a loop member that is tied to its master as well is ended once", "[zapit][fesearch][streamstoend][fbc]")
{
	/* Loop members 1 to 3 are tied to master 0 as well, so each is both one
	   of its links and one of its FBC links. Counted twice, the master would
	   end five or six frontends, and root 4 with its three links would win
	   with four. */
	std::vector<FeView> fes;
	fes.push_back(master(0, true, false));
	fes.push_back(member(1, false));
	fes.push_back(member(2, false));
	fes.push_back(member(3, false));
	fes[0].links = numbersFrom(0, 3);
	tie(fes, 0, 1, 2, 3);
	fes.push_back(independent(4));
	fes.push_back(uncarried(5));
	fes.push_back(uncarried(6));
	fes.push_back(uncarried(7));
	tie(fes, 4, 5, 6, 7);
	std::vector<Use> use(8);
	for (int i = 1; i < 8; ++i)
	{
		if (i != 4)
			fes[i].loop_can_tune = false;
		streamsOnly(fes, use, i);
	}

	const StreamsToEnd r = fesearch::streamsToEnd(fes, use, false, false);
	REQUIRE(r.stage == 2);
	REQUIRE(r.end == numbersFrom(1, 3));
}

TEST_CASE("S11: a loop member of a master that stays held is free once its own streams end", "[zapit][fesearch][streamstoend][fbc]")
{
	/* The walk weighs the links tied to a master only when it takes the
	   master itself. Master 0 stays held on a band the channel is on, so
	   once the stream on loop member 1 ends, member 1 is taken whatever
	   link 2 holds. */
	std::vector<FeView> fes;
	fes.push_back(master(0, true, false));
	fes.push_back(member(1, false));  // loop, a stream on another band
	fes.push_back(uncarried(2));      // link tied to the master, another band
	link(fes, 0, 1);
	tie(fes, 0, 2);
	fes[1].loop_can_tune = false;
	fes[2].loop_can_tune = false;
	std::vector<Use> use(3);
	kept(fes, use, 0);
	streamsOnly(fes, use, 1);

	SECTION("the link is kept: the member's stream ends")
	{
		kept(fes, use, 2);
		const StreamsToEnd r = fesearch::streamsToEnd(fes, use, false, false);
		REQUIRE(r.stage == 2);
		REQUIRE(r.end == numbers(1));
	}
	SECTION("streams hold the link: they stay")
	{
		streamsOnly(fes, use, 2);
		const StreamsToEnd r = fesearch::streamsToEnd(fes, use, false, false);
		REQUIRE(r.stage == 2);
		REQUIRE(r.end == numbers(1));
	}
}

TEST_CASE("S12: a master whose own streams end is free, so the links tied to it count", "[zapit][fesearch][streamstoend][fbc]")
{
	/* Master 0 carries streams on another band. Once they end, the walk takes
	   the master first, and link 2 on another band holds its input. */
	std::vector<FeView> fes;
	fes.push_back(master(0, true, false));
	fes.push_back(member(1, false));  // loop, free
	fes.push_back(uncarried(2));      // link tied to the master, another band
	link(fes, 0, 1);
	tie(fes, 0, 2);
	fes[0].loop_can_tune = false;
	fes[2].loop_can_tune = false;
	std::vector<Use> use(3);
	streamsOnly(fes, use, 0);

	SECTION("the link is kept: no way")
	{
		kept(fes, use, 2);
		const StreamsToEnd r = fesearch::streamsToEnd(fes, use, false, false);
		REQUIRE(r.stage == 0);
		REQUIRE(r.end.empty());
	}
	SECTION("streams hold the link: they end with the master's")
	{
		streamsOnly(fes, use, 2);
		const StreamsToEnd r = fesearch::streamsToEnd(fes, use, false, false);
		REQUIRE(r.stage == 2);
		REQUIRE(r.end == numbers(0, 2));
	}
}

TEST_CASE("S13: frontends that share a number are told apart", "[zapit][fesearch][streamstoend][fbc]")
{
	/* A number is only unique within its adapter: frontend 0 of a second
	   adapter has number 0 as well. */
	SECTION("a member with the master's number does not free the master")
	{
		std::vector<FeView> fes;
		fes.push_back(master(0, true, false));
		fes.push_back(member(0, false));  // loop, a stream on another band
		fes.push_back(uncarried(2));      // link tied to the master, another band
		link(fes, 0, 1);
		tie(fes, 0, 2);
		fes[1].loop_can_tune = false;
		fes[2].loop_can_tune = false;
		std::vector<Use> use(3);
		kept(fes, use, 0);
		streamsOnly(fes, use, 1);
		kept(fes, use, 2);

		const StreamsToEnd r = fesearch::streamsToEnd(fes, use, false, false);
		REQUIRE(r.stage == 2);
		REQUIRE(r.end == numbers(0));
	}
	SECTION("two members with one number are two frontends to end")
	{
		std::vector<FeView> fes;
		fes.push_back(master(0, true, false));
		fes.push_back(member(1, false));
		fes.push_back(member(1, false));
		link(fes, 0, 1, 2);
		fes[1].loop_can_tune = false;
		fes[2].loop_can_tune = false;
		fes.push_back(independent(3));
		std::vector<Use> use(4);
		streamsOnly(fes, use, 1);
		streamsOnly(fes, use, 2);
		streamsOnly(fes, use, 3);

		// The master ends two frontends, frontend 3 one.
		const StreamsToEnd r = fesearch::streamsToEnd(fes, use, false, false);
		REQUIRE(r.stage == 2);
		REQUIRE(r.end == numbers(3));
	}
}

namespace
{

/* What the kept zap reads and sets on a frontend: its use count, the
   channel it carries, its number and its master's. */
struct KeptFe
{
	int usecount;
	uint64_t channel;
	int number;
	int master;

	explicit KeptFe(int n) : usecount(0), channel(0), number(n), master(-1)
	{
	}
	uint64_t getChannelID() { return channel; }
	void setChannelID(uint64_t id) { channel = id; }
	int getNumber() const { return number; }
	int getMaster() { return master; }
};

/* A search the way getFrontend makes one: it names the channel on each
   frontend it weighs, and takes the first one nobody uses. */
struct FirstUnused
{
	std::vector<KeptFe *> fes;
	std::vector<int> counts_seen;   // the use counts as the search found them
	int searches;

	FirstUnused() : searches(0)
	{
	}
	KeptFe *operator()()
	{
		++searches;
		counts_seen.clear();
		for (size_t i = 0; i < fes.size(); ++i)
			counts_seen.push_back(fes[i]->usecount);
		for (size_t i = 0; i < fes.size(); ++i)
		{
			fes[i]->setChannelID(0x77);
			if (fes[i]->usecount == 0)
				return fes[i];
		}
		return 0;
	}
};

std::vector<int *> countsOf(const std::vector<KeptFe *> &fes)
{
	std::vector<int *> c;
	for (size_t i = 0; i < fes.size(); ++i)
		c.push_back(&fes[i]->usecount);
	return c;
}

std::vector<const int *> allOf(const std::vector<KeptFe *> &fes)
{
	std::vector<const int *> c;
	for (size_t i = 0; i < fes.size(); ++i)
		c.push_back(&fes[i]->usecount);
	return c;
}

} // namespace

TEST_CASE("the zap a switch predicts counts the kept frontends busy and leaves every frontend as it found it", "[zapit][fesearch][switch]")
{
	KeptFe a(0), b(1), c(2);
	a.channel = 0x11;
	b.channel = 0x22;
	std::vector<KeptFe *> fes;
	fes.push_back(&a);
	fes.push_back(&b);
	fes.push_back(&c);
	FirstUnused walk;
	walk.fes = fes;

	// Nothing kept: the search takes the first frontend.
	std::vector<KeptFe *> keep;
	REQUIRE(fesearch::pickKept(fes, countsOf(keep), walk) == &a);

	// The stream's frontend and the PiP's kept: the search sees them busy and takes the third.
	keep.push_back(&a);
	keep.push_back(&b);
	REQUIRE(fesearch::pickKept(fes, countsOf(keep), walk) == &c);
	REQUIRE(walk.counts_seen == std::vector<int>({ 1, 1, 0 }));
	// Afterwards the counts are back, and so are the channels the search named.
	REQUIRE(a.usecount == 0);
	REQUIRE(b.usecount == 0);
	REQUIRE(c.usecount == 0);
	REQUIRE(a.channel == 0x11);
	REQUIRE(b.channel == 0x22);
	REQUIRE(c.channel == 0);

	// A frontend locked by a recording stays locked, one higher only for the search.
	a.usecount = 1;
	keep.clear();
	keep.push_back(&b);
	keep.push_back(&c);
	REQUIRE(fesearch::pickKept(fes, countsOf(keep), walk) == 0);
	REQUIRE(walk.counts_seen == std::vector<int>({ 1, 1, 1 }));
	REQUIRE(a.usecount == 1);
	REQUIRE(b.usecount == 0);
	REQUIRE(c.usecount == 0);
	REQUIRE(walk.searches == 3);
}

TEST_CASE("the kept zap raises the kept counts and its release works out what is still locked", "[zapit][fesearch][switch]")
{
	KeptFe a(0), b(1), c(2);
	std::vector<KeptFe *> fes;
	fes.push_back(&a);
	fes.push_back(&b);
	fes.push_back(&c);

	std::vector<KeptFe *> keep;
	REQUIRE_FALSE(fesearch::raiseKept(countsOf(keep)));
	keep.push_back(&b);
	REQUIRE(fesearch::raiseKept(countsOf(keep)));
	REQUIRE(b.usecount == 1);
	// Nothing else locked: once released no frontend is.
	REQUIRE_FALSE(fesearch::releaseKept(countsOf(keep), allOf(fes)));
	REQUIRE(b.usecount == 0);

	// A recording on another frontend: still locked after the release.
	c.usecount = 1;
	REQUIRE(fesearch::raiseKept(countsOf(keep)));
	REQUIRE(fesearch::releaseKept(countsOf(keep), allOf(fes)));
	REQUIRE(b.usecount == 0);
	REQUIRE(c.usecount == 1);

	// A stream's lock on the kept frontend itself outlasts the zap.
	c.usecount = 0;
	b.usecount = 1;
	REQUIRE(fesearch::raiseKept(countsOf(keep)));
	REQUIRE(b.usecount == 2);
	REQUIRE(fesearch::releaseKept(countsOf(keep), allOf(fes)));
	REQUIRE(b.usecount == 1);

	// A count at nothing is not lowered below it.
	b.usecount = 0;
	REQUIRE_FALSE(fesearch::releaseKept(countsOf(keep), allOf(fes)));
	REQUIRE(b.usecount == 0);
}

TEST_CASE("a tune turns the dish where its frontend or the channel's satellite says a motor or USALS", "[zapit][fesearch][switch]")
{
	REQUIRE_FALSE(fesearch::turnsDish(false, false, false, 0, false));
	REQUIRE_FALSE(fesearch::turnsDish(false, false, true, 0, false));
	REQUIRE(fesearch::turnsDish(true, false, false, 0, false));
	REQUIRE(fesearch::turnsDish(false, true, false, 0, false));
	REQUIRE(fesearch::turnsDish(false, false, true, 3, false));
	REQUIRE(fesearch::turnsDish(false, false, true, 0, true));
	// A satellite the frontend does not list says nothing.
	REQUIRE_FALSE(fesearch::turnsDish(false, false, false, 3, true));

	// The dish is the master's for a linked frontend, the frontend's own otherwise.
	KeptFe m(0), link(1), other(2);
	link.master = 0;
	std::vector<KeptFe *> fes;
	fes.push_back(&other);
	fes.push_back(&m);
	fes.push_back(&link);
	REQUIRE(fesearch::dishOf(&link, fes, true) == &m);
	REQUIRE(fesearch::dishOf(&link, fes, false) == &link);
	REQUIRE(fesearch::dishOf(&other, fes, false) == &other);
}
