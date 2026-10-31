/*
 * test_streamadmission.cpp - whether a stream may have a tuner, and which way
 *
 * Copyright (C) 2026 Thilo Graf
 *
 * License: GPL-2.0-or-later
 *
 * The rule a stream is admitted by, on the facts zapit reads under the
 * frontend manager's lock. The box this is written for has the picture on
 * one frontend, PiP slot i reading demux i + 1, and a stream on frontend f
 * reading demux f + 1.
 */

#include "support/catch.hpp"

#include <zapit/streamadmission.h>

#include <algorithm>
#include <vector>

using namespace streamadmission;

namespace
{

const uint64_t kLive = 0x1001;
const uint64_t kOther = 0x2002;
const uint64_t kPip = 0x3003;

// The picture on frontend fe, tuned, one PiP slot, nothing in it.
Facts liveOn(int fe)
{
	Facts f;
	f.live_dvb = true;
	f.live_tuned = true;
	f.live_channel = kLive;
	f.live_fe = fe;
	f.slots = 1;
	return f;
}

void pipOn(Facts &f, int slot, int fe, uint64_t channel)
{
	f.slot[slot].active = true;
	f.slot[slot].fe = fe;
	f.slot[slot].channel = channel;
}

bool has(const std::vector<int> &v, int fe)
{
	return std::find(v.begin(), v.end(), fe) != v.end();
}

} // namespace

TEST_CASE("the live channel streams from the picture's frontend", "[zapit][admission]")
{
	Facts f = liveOn(0);
	REQUIRE(decide(f, kLive) == Live);
}

TEST_CASE("another channel needs an idle frontend", "[zapit][admission]")
{
	Facts f = liveOn(0);
	REQUIRE(decide(f, kOther) == Idle);
}

TEST_CASE("the channel a PiP shows is refused", "[zapit][admission]")
{
	Facts f = liveOn(0);
	pipOn(f, 0, 1, kPip);
	REQUIRE(decide(f, kPip) == Refuse);
	// Even when it is the live channel as well
	pipOn(f, 0, 1, kLive);
	REQUIRE(decide(f, kLive) == Refuse);
}

TEST_CASE("a slot that is not active is not looked at", "[zapit][admission]")
{
	Facts f = liveOn(0);
	f.slot[0].channel = kOther;  // left behind, its frontend gone
	REQUIRE(decide(f, kOther) == Idle);
}

TEST_CASE("the live channel is refused when a PiP reads the live demux from elsewhere", "[zapit][admission]")
{
	/* Live on frontend 0 reads demux 0; a stream on it reads demux 1 and sets
	   that demux's source to frontend 0. PiP slot 0 reads demux 1 from
	   frontend 1. */
	Facts f = liveOn(0);
	pipOn(f, 0, 1, kPip);
	REQUIRE(decide(f, kLive) == Refuse);
}

TEST_CASE("the live channel is admitted when the PiP on its demux has the same source", "[zapit][admission]")
{
	Facts f = liveOn(0);
	pipOn(f, 0, 0, kPip);
	REQUIRE(decide(f, kLive) == Live);
}

TEST_CASE("the live channel is admitted when the PiP reads another demux", "[zapit][admission]")
{
	// Live on frontend 1 reads demux 2 for a stream; slot 0 reads demux 1.
	Facts f = liveOn(1);
	pipOn(f, 0, 0, kPip);
	REQUIRE(decide(f, kLive) == Live);
}

TEST_CASE("the live channel on frontend 0 is refused while the AV-input PiP reads demux 1", "[zapit][admission]")
{
	Facts f = liveOn(0);
	f.avinput_pip = true;
	REQUIRE(decide(f, kLive) == Refuse);
	// On another frontend its demux is not demux 1.
	f = liveOn(1);
	f.avinput_pip = true;
	REQUIRE(decide(f, kLive) == Live);
}

TEST_CASE("the live channel is not live in standby, untuned or from the web", "[zapit][admission]")
{
	Facts f = liveOn(0);
	f.standby = true;
	REQUIRE(decide(f, kLive) == Idle);

	f = liveOn(0);
	f.live_tuned = false;
	REQUIRE(decide(f, kLive) == Idle);

	f = liveOn(0);
	f.live_dvb = false;
	REQUIRE(decide(f, kLive) == Idle);

	f = liveOn(-1);
	REQUIRE(decide(f, kLive) == Idle);
}

TEST_CASE("a refused live channel is not offered an idle frontend instead", "[zapit][admission]")
{
	Facts f = liveOn(0);
	pipOn(f, 0, 1, kPip);
	REQUIRE(decide(f, kLive) == Refuse);
	REQUIRE(decide(f, kOther) == Idle);
}

TEST_CASE("a build whose GUI starts PiPs itself refuses every stream", "[zapit][admission]")
{
	Facts f = liveOn(0);
	f.variant = QuadPip;
	REQUIRE(decide(f, kLive) == Refuse);
	REQUIRE(decide(f, kOther) == Refuse);
}

TEST_CASE("with dynamic demux only the live channel streams, and only without PiP", "[zapit][admission]")
{
	Facts f = liveOn(0);
	f.variant = DynamicDemux;
	REQUIRE(decide(f, kLive) == Live);
	REQUIRE(decide(f, kOther) == Refuse);

	pipOn(f, 0, 0, kPip);
	REQUIRE(decide(f, kLive) == Refuse);

	f = liveOn(0);
	f.variant = DynamicDemux;
	f.avinput_pip = true;
	f.live_fe = 1;
	REQUIRE(decide(f, kLive) == Refuse);
}

TEST_CASE("on the PiP demux variant both ways need the PiP off", "[zapit][admission]")
{
	Facts f = liveOn(0);
	f.variant = Cst;
	REQUIRE(decide(f, kLive) == Live);
	REQUIRE(decide(f, kOther) == Idle);

	pipOn(f, 0, 0, kPip);
	REQUIRE(decide(f, kLive) == Refuse);
	REQUIRE(decide(f, kOther) == Refuse);
}

TEST_CASE("slots beyond the box's own are not read", "[zapit][admission]")
{
	Facts f = liveOn(0);
	f.slots = 1;
	pipOn(f, 2, 1, kOther);   // garbage in a slot this box does not have
	REQUIRE(decide(f, kOther) == Idle);

	f.slots = 3;
	REQUIRE(decide(f, kOther) == Refuse);

	f.slots = 9;   // more than there can be: read as the most there can be
	REQUIRE(decide(f, kOther) == Refuse);
}

TEST_CASE("held: the picture's frontend while live runs, also untuned", "[zapit][admission]")
{
	Facts f = liveOn(1);
	f.live_tuned = false;
	std::vector<int> h;
	held(f, h);
	REQUIRE(has(h, 1));

	f.standby = true;
	h.clear();
	held(f, h);
	REQUIRE_FALSE(has(h, 1));

	f = liveOn(1);
	f.live_dvb = false;   // a web channel holds no frontend
	h.clear();
	held(f, h);
	REQUIRE(h.empty());
}

TEST_CASE("held: a PiP slot's frontend and the frontend with the slot's number", "[zapit][admission]")
{
	Facts f = liveOn(0);
	f.slots = 2;
	pipOn(f, 1, 2, kPip);   // slot 1 reads demux 2 from frontend 2
	std::vector<int> h;
	held(f, h);
	REQUIRE(has(h, 0));     // the picture
	REQUIRE(has(h, 2));     // the slot's frontend
	REQUIRE(has(h, 1));     // a stream on frontend 1 would read demux 2
	REQUIRE_FALSE(has(h, 3));
}

TEST_CASE("held: frontend 0 while the AV-input PiP reads demux 1", "[zapit][admission]")
{
	Facts f = liveOn(1);
	f.avinput_pip = true;
	std::vector<int> h;
	held(f, h);
	REQUIRE(has(h, 0));
	REQUIRE(has(h, 1));
}

TEST_CASE("held: live on 0 and PiP slot 0 on 1 hold both, leaving frontend 2", "[zapit][admission]")
{
	Facts f = liveOn(0);
	pipOn(f, 0, 1, kPip);
	std::vector<int> h;
	held(f, h);
	REQUIRE(has(h, 0));
	REQUIRE(has(h, 1));
	REQUIRE_FALSE(has(h, 2));
	REQUIRE(decide(f, kOther) == Idle);
}
