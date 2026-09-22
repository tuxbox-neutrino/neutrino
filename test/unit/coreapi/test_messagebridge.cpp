/*
 * test_messagebridge.cpp - tests for the GUI message bridge
 *
 * Copyright (C) 2026 NI-Team
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
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include "support/catch.hpp"

#include <stdint.h>
#include <unistd.h>

#include <vector>

#include <neutrinoMessages.h>
#include <zapit/types.h>

#include "coreapi/base/eventbus.h"
#include "coreapi/base/messagebridge.h"
#include "coreapi/system.h"

using namespace coreapi;

namespace
{

struct Collector : public Subscriber
{
	std::vector<Event> seen;
	void onEvent(const Event &e) { seen.push_back(e); }
};

/* Holds the payload the way the message path does: a heap block the handler
   frees once the bridge has returned. */
struct Payload
{
	unsigned char *p;

	explicit Payload(t_channel_id id) : p(new unsigned char[sizeof(id) + 1])
	{
		*(t_channel_id *) p = id;
	}
	~Payload() { delete[] p; }

	neutrino_msg_data_t operator()() const { return (neutrino_msg_data_t) p; }
};

// What zapit puts on a zap, satellite position and transponder included.
const t_channel_id full_id = 0x4711000200030004ULL;
// What sectionsd puts on an epg update, the service key it files events under.
const t_channel_id service_key = full_id & 0xFFFFFFFFFFFFULL;

} // namespace

/* Long enough that a clock reading assembled in the wrong unit lands outside
   the bounds whichever second the sleep starts in. */
TEST_CASE("the window clock counts milliseconds", "[messagebridge]")
{
	int64_t before = detail::monotonicMs();
	usleep(1200000);
	int64_t after = detail::monotonicMs();

	REQUIRE(after - before >= 1100);
	/* Loose on purpose: what this separates is milliseconds from microseconds,
	   and a tighter bound would double as an assertion about scheduling. */
	REQUIRE(after - before < 60000);
}

TEST_CASE("a zap carries the whole channel id out of the payload", "[messagebridge]")
{
	Collector c;
	EventBus::instance().subscribe(&c);

	Payload id(full_id);
	publishFromMessage(NeutrinoMessages::EVT_ZAP_COMPLETE, id());

	EventBus::instance().unsubscribe(&c);

	REQUIRE(c.seen.size() == 1);
	REQUIRE(c.seen[0].type == EventType::Zap);
	REQUIRE(c.seen[0].channel_id == full_id);
	// the bits above the service key are what a lookup needs, so they stay
	REQUIRE(c.seen[0].channel_id != service_key);
	REQUIRE(c.seen[0].value == 0);
}

// The pointer is read, never kept, so a zap without one is an event with no id.
TEST_CASE("a zap without a payload still reports", "[messagebridge]")
{
	Collector c;
	EventBus::instance().subscribe(&c);

	publishFromMessage(NeutrinoMessages::EVT_ZAP_COMPLETE, 0);

	EventBus::instance().unsubscribe(&c);

	REQUIRE(c.seen.size() == 1);
	REQUIRE(c.seen[0].type == EventType::Zap);
	REQUIRE(c.seen[0].channel_id == 0);
}

TEST_CASE("a mode change reports the mode without the rezap flag", "[messagebridge]")
{
	Collector c;
	EventBus::instance().subscribe(&c);

	publishFromMessage(NeutrinoMessages::CHANGEMODE,
			   NeutrinoModes::mode_radio | NeutrinoModes::norezap);

	EventBus::instance().unsubscribe(&c);

	REQUIRE(c.seen.size() == 1);
	REQUIRE(c.seen[0].type == EventType::Mode);
	REQUIRE(c.seen[0].value == NeutrinoModes::mode_radio);
}

/* Asking for standby is not going into it: a request can be refused and one
   can be dropped half way. The box says what happened instead. */
TEST_CASE("asking for standby is not an event", "[messagebridge]")
{
	Collector c;
	EventBus::instance().subscribe(&c);

	publishFromMessage(NeutrinoMessages::STANDBY_ON, (neutrino_msg_data_t) "cec");
	publishFromMessage(NeutrinoMessages::STANDBY_OFF, 0);

	EventBus::instance().unsubscribe(&c);

	REQUIRE(c.seen.empty());
}

TEST_CASE("the box says which way it went once it has", "[messagebridge]")
{
	Collector c;
	EventBus::instance().subscribe(&c);

	coreapi::system::announceStandby(true);
	coreapi::system::announceStandby(false);

	EventBus::instance().unsubscribe(&c);

	REQUIRE(c.seen.size() == 2);
	REQUIRE(c.seen[0].type == EventType::Standby);
	REQUIRE(c.seen[0].value == 1);
	REQUIRE(c.seen[1].type == EventType::Standby);
	REQUIRE(c.seen[1].value == 0);
}

TEST_CASE("a volume change reports the level", "[messagebridge]")
{
	Collector c;
	EventBus::instance().subscribe(&c);

	publishFromMessage(NeutrinoMessages::EVT_SET_VOLUME, 42);

	EventBus::instance().unsubscribe(&c);

	REQUIRE(c.seen.size() == 1);
	REQUIRE(c.seen[0].type == EventType::Volume);
	REQUIRE(c.seen[0].value == 42);
}

/* The one the redesign found missing. A reader that is told the level and not
   the silence draws a box at fifty seven per cent that is making no sound, and
   a second browser turning the sound off is a change nothing on the first one
   reports. */
TEST_CASE("a mute reports the state that was asked for", "[messagebridge]")
{
	Collector c;
	EventBus::instance().subscribe(&c);

	publishFromMessage(NeutrinoMessages::EVT_SET_MUTE, 1);
	publishFromMessage(NeutrinoMessages::EVT_SET_MUTE, 0);

	EventBus::instance().unsubscribe(&c);

	REQUIRE(c.seen.size() == 2);
	REQUIRE(c.seen[0].type == EventType::Mute);
	REQUIRE(c.seen[0].value == 1);
	REQUIRE(c.seen[1].type == EventType::Mute);
	REQUIRE(c.seen[1].value == 0);
}

/* The message the loop carries about recording says whether the box is
   recording at all, and zapit sends it only where that answer changes: the
   first recording turns it on and the last one turns it off, and every start
   and every stop in between sends nothing. Read as a recording starting and
   stopping it therefore told a reader that a second recording had not begun
   and that one of two had not ended, which is worse than telling it nothing.
   So nothing is what it now says, and the stream is fed by the two calls
   below, which are made once for each recording. */
TEST_CASE("the record mode message says nothing about a recording", "[messagebridge]")
{
	Collector c;
	EventBus::instance().subscribe(&c);

	publishFromMessage(NeutrinoMessages::EVT_RECORDMODE, 1);
	publishFromMessage(NeutrinoMessages::EVT_RECORDMODE, 0);

	EventBus::instance().unsubscribe(&c);

	REQUIRE(c.seen.empty());
}

/* Each recording, and which one: a reader holding a list has to be able to act
   on the row that moved. The channel is carried too, so that a screen showing
   one channel can tell whether this concerns it without asking. */
TEST_CASE("each recording that begins and ends says which one it was", "[messagebridge]")
{
	Collector c;
	EventBus::instance().subscribe(&c);

	publishRecordingStarted(0x2b66, 7);
	publishRecordingStarted(0x2b67, 8);
	publishRecordingStopped(0x2b66, 7);

	EventBus::instance().unsubscribe(&c);

	REQUIRE(c.seen.size() == 3);

	REQUIRE(c.seen[0].type == EventType::RecordStart);
	REQUIRE(c.seen[0].channel_id == 0x2b66);
	REQUIRE(c.seen[0].value == 7);

	/* The one the mode message could not carry: a second recording while one
	   is already running moves nothing about whether the box is recording. */
	REQUIRE(c.seen[1].type == EventType::RecordStart);
	REQUIRE(c.seen[1].channel_id == 0x2b67);
	REQUIRE(c.seen[1].value == 8);

	// And the other one: a first of two ending, with one still running.
	REQUIRE(c.seen[2].type == EventType::RecordStop);
	REQUIRE(c.seen[2].channel_id == 0x2b66);
	REQUIRE(c.seen[2].value == 7);
}

/* EVT_TIMER is the expiry of a timer the remote control layer holds, one of
   them behind every fading window, so translating it would put a stream on the
   bus that means nothing. The key is here for the same reason: an idle box
   still moves through this function. */
TEST_CASE("messages with no counterpart publish nothing", "[messagebridge]")
{
	Collector c;
	EventBus::instance().subscribe(&c);

	publishFromMessage(NeutrinoMessages::EVT_TIMER, 7);
	publishFromMessage(CRCInput::RC_ok, 0);
	publishFromMessage(NeutrinoMessages::EVT_SI_FINISHED, 0);

	EventBus::instance().unsubscribe(&c);

	REQUIRE(c.seen.empty());
}

/* The window is process wide and this is the only case in the suite that
   reaches it, boundaries and updates in one because the boundaries have to be
   walked from the state no update has moved yet. One case rather than two, so
   the order is the order of these statements and not the runner's. */
TEST_CASE("the epg window holds updates back at its edges", "[messagebridge]")
{
	// a first update is not inside a window, whatever the clock reads
	REQUIRE(detail::epgWindowOpenAt(0));

	// and the next one is, right up to the last millisecond of it
	REQUIRE_FALSE(detail::epgWindowOpenAt(1));
	REQUIRE_FALSE(detail::epgWindowOpenAt(499));

	// the window is 500ms wide and closes on its own edge
	REQUIRE(detail::epgWindowOpenAt(500));

	/* A clock that could not be read turns the limit off rather than shutting
	   the window for good, and does not move it either. */
	REQUIRE(detail::epgWindowOpenAt(-1));
	REQUIRE_FALSE(detail::epgWindowOpenAt(999));

	/* From here on the real clock, which has to be past the 500 the window was
	   left at. Stated rather than assumed: a box a second out of its boot would
	   otherwise fail below with nothing saying why. */
	REQUIRE(detail::monotonicMs() > 1000);

	Collector c;
	EventBus::instance().subscribe(&c);

	Payload id(service_key);

	publishFromMessage(NeutrinoMessages::EVT_CURRENTNEXT_EPG, id());
	publishFromMessage(NeutrinoMessages::EVT_CURRENTNEXT_EPG, id());
	CHECK(c.seen.size() == 1);
	CHECK(c.seen[0].type == EventType::EpgUpdated);
	CHECK(c.seen[0].value == 0);

	/* Shortened where a zap is not, so a subscriber that keeps a zapped id and
	   matches epg updates against it has to mask before comparing. */
	CHECK(c.seen[0].channel_id == service_key);
	CHECK(c.seen[0].channel_id == (full_id & 0xFFFFFFFFFFFFULL));
	CHECK(c.seen[0].channel_id != full_id);

	EventBus::instance().unsubscribe(&c);

	REQUIRE(c.seen.size() == 1);
}
