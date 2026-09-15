/*
 * test_postpayload.cpp - tests for who owns a posted payload
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
#include "support/fakes.h"

#include <neutrinoMessages.h>

#include <cstring>

using namespace coreapi;

TEST_CASE("a payload arrives as a copy the sink owns", "[deps]")
{
	FakeCommandSink sink;
	InstalledSink installed_sink(&sink);

	unsigned char source[8] = { 1, 2, 3, 4, 5, 6, 7, 8 };
	Result<void> r = postPayload(NeutrinoMessages::ZAPTO, source, sizeof(source));
	REQUIRE(r.ok());
	REQUIRE(sink.posted.size() == 1);

	unsigned char *block = (unsigned char *) sink.posted[0].second;
	REQUIRE(block != 0);
	// A block of its own, or the loop would free the caller's bytes.
	REQUIRE((const void *) block != (const void *) source);
	REQUIRE(std::memcmp(block, source, sizeof(source)) == 0);
	delete[] block;
}

TEST_CASE("a refused payload is released on the side of the line that needs it", "[deps]")
{
	FakeCommandSink sink;
	sink.answer = Status::Busy;
	InstalledSink installed_sink(&sink);

	unsigned char source[8] = { 1, 2, 3, 4, 5, 6, 7, 8 };

	// Outside the range the queue releases for, so nothing else will.
	long before = test_array_deletes;
	Result<void> outside = postPayload(NeutrinoMessages::ZAPTO, source, sizeof(source));
	const long released_outside = test_array_deletes - before;

	REQUIRE_FALSE(outside.ok());
	REQUIRE(released_outside == 1);

	// Inside it the queue has already released the block by the time it
	// answers, so a release here would be the second one.
	before = test_array_deletes;
	Result<void> inside = postPayload(NeutrinoMessages::EVT_ZAP_FAILED, source, sizeof(source));
	const long released_inside = test_array_deletes - before;

	REQUIRE_FALSE(inside.ok());
	// Checked before the release below, so that a block already gone is caught
	// here rather than by the abort a second release would raise.
	REQUIRE(released_inside == 0);

	// A fake queue keeps what a real one would have released.
	delete[] (unsigned char *) sink.posted[1].second;
}

TEST_CASE("only a busy answer leaves the release to the queue", "[deps]")
{
	/* Internal is what the real sink answers before the remote control is
	   built, without reaching the queue. Conflict stands for a sink that is
	   written later and answers something of its own: the contract requires
	   busy and nothing else from one that did reach the queue, because
	   anything else is read here as a block the queue never released. Two of
	   them, or a decision that merely tested for a failure would pass. */
	const Status answers[] = { Status::Internal, Status::Conflict };

	for (size_t i = 0; i < sizeof(answers) / sizeof(answers[0]); i++)
	{
		FakeCommandSink sink;
		sink.answer = answers[i];
		InstalledSink installed_sink(&sink);

		unsigned char source[8] = { 1, 2, 3, 4, 5, 6, 7, 8 };

		// The message the queue would have released for, had it seen this.
		long before = test_array_deletes;
		Result<void> inside = postPayload(NeutrinoMessages::EVT_ZAP_FAILED, source, sizeof(source));
		const long released_inside = test_array_deletes - before;

		REQUIRE_FALSE(inside.ok());
		REQUIRE(inside.error().status == answers[i]);
		REQUIRE(released_inside == 1);

		// And the same for one it would not have released for.
		before = test_array_deletes;
		Result<void> outside = postPayload(NeutrinoMessages::ZAPTO, source, sizeof(source));
		const long released_outside = test_array_deletes - before;

		REQUIRE_FALSE(outside.ok());
		REQUIRE(released_outside == 1);
	}
}

TEST_CASE("the release rule keeps both edges of the queue's own", "[deps]")
{
	FakeCommandSink sink;
	sink.answer = Status::Busy;
	InstalledSink installed_sink(&sink);

	/* The rule is a copy of the one the queue applies to a refused payload,
	   and both apply it to a number rather than to a name, so a copy that
	   loses an edge stops matching there. No message sits on either edge
	   today, which is why nothing else would notice. */
	const neutrino_msg_t first_inside = CRCInput::RC_WithData;
	const neutrino_msg_t first_outside = CRCInput::RC_WithDataEnd;
	const unsigned char one = 'x';

	long before = test_array_deletes;
	Result<void> low = postPayload(first_inside, &one, sizeof(one));
	const long released_low = test_array_deletes - before;

	REQUIRE_FALSE(low.ok());
	REQUIRE(released_low == 0);

	// A fake queue keeps what a real one would have released.
	delete[] (unsigned char *) sink.posted[0].second;

	before = test_array_deletes;
	Result<void> high = postPayload(first_outside, &one, sizeof(one));
	const long released_high = test_array_deletes - before;

	REQUIRE_FALSE(high.ok());
	REQUIRE(released_high == 1);
}
