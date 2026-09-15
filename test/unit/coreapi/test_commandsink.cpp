/*
 * test_commandsink.cpp - tests for the command sink
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
#include "support/rcfixture.h"

#include "coreapi/base/deps.h"

/* The application owns this pointer, and no other translation unit in the test
 * binary defines it, so the sink under test can be shown both states of it. */
CRCInput *g_RCInput = 0;

using namespace coreapi;

namespace
{
/* A failed assertion unwinds out of the case, so the two globals a case moves
 * are put back by a destructor rather than by its last lines. Declared after
 * any CRCInput it refers to, so the pointer is cleared before that is gone. */
struct SinkFixture
{
	~SinkFixture()
	{
		g_RCInput = 0;
		setCommandSink(0);
	}
};
} // anonymous namespace

TEST_CASE("the real sink answers for a remote control that was never built", "[commandsink]")
{
	SinkFixture guard;
	installRealCommandSink();

	// No queue at all, which the caller must not read as a queue that turned
	// this one message away.
	g_RCInput = 0;
	Result<void> unbuilt = postCommand(NeutrinoMessages::SHOW_INFOBAR, 0);
	REQUIRE_FALSE(unbuilt.ok());
	REQUIRE(unbuilt.error().status == Status::Internal);
}

TEST_CASE("the real sink hands a message over and names a full queue", "[commandsink]")
{
	requireFreeEventSocket();

	CRCInput rc;
	SinkFixture guard;

	installRealCommandSink();
	g_RCInput = &rc;

	// A message whose number says it carries no block, so a refusal of it can
	// never be read as a pointer.
	Result<void> taken = postCommand(NeutrinoMessages::SHOW_INFOBAR, 0x1234);
	REQUIRE(taken.ok());

	// The message the sink handed over, read back out of the queue it named.
	neutrino_msg_t got = 0;
	neutrino_msg_data_t gotdata = 0;
	rc.getMsg_ms(&got, &gotdata, 0);
	REQUIRE(got == NeutrinoMessages::SHOW_INFOBAR);
	REQUIRE(gotdata == (neutrino_msg_data_t) 0x1234);

	const long filled = fillQueue(rc, true);
	REQUIRE(filled > 0);
	REQUIRE(filled < 1000000);

	// Retryable, and the box is there to retry against, which is the whole
	// difference from the answer above.
	Result<void> refused = postCommand(NeutrinoMessages::SHOW_INFOBAR, 0);
	REQUIRE_FALSE(refused.ok());
	REQUIRE(refused.error().status == Status::Busy);
}

TEST_CASE("a payload the queue released on refusal is not released twice", "[commandsink]")
{
	requireFreeEventSocket();

	CRCInput rc;
	SinkFixture guard;

	installRealCommandSink();
	g_RCInput = &rc;

	const long filled = fillQueue(rc, true);
	REQUIRE(filled > 0);
	REQUIRE(filled < 1000000);

	/* This message's number tells the queue it carries a block, so the queue
	   frees it before it answers. Against the real queue rather than a fake,
	   because a second release is not something an assertion can catch. */
	unsigned char source[8] = { 1, 2, 3, 4, 5, 6, 7, 8 };
	Result<void> r = postPayload(NeutrinoMessages::EVT_ZAP_FAILED, source, sizeof(source));
	REQUIRE_FALSE(r.ok());
	REQUIRE(r.error().status == Status::Busy);
}

TEST_CASE("the fixture puts the remote control back on every way out", "[commandsink]")
{
	// Never dereferenced, only looked at: it stands for a control that a case
	// leaving through a failed assertion would have left behind.
	CRCInput *const sentinel = (CRCInput *) 0x1;

	{
		SinkFixture guard;
		g_RCInput = sentinel;
	}

	// The destructor runs whether a case returns or an assertion unwinds it,
	// which is the whole reason the restore is not written as a last line.
	REQUIRE(g_RCInput == 0);
}

TEST_CASE("the two rules meet at the edges of the range they share", "[commandsink]")
{
	requireFreeEventSocket();

	CRCInput rc;
	SinkFixture guard;

	installRealCommandSink();
	g_RCInput = &rc;

	const long filled = fillQueue(rc, true);
	REQUIRE(filled > 0);
	REQUIRE(filled < 1000000);

	/* One rule lives in the queue and the other in the layer above it, and the
	   only thing that keeps them from releasing the block twice or not at all
	   is that they answer the same at every number. Their own cases each pin
	   one of them; this one crosses both, at the edges rather than in the
	   middle where a narrowed range would still agree. */
	const neutrino_msg_t edges[] = {
		CRCInput::RC_WithData,
		CRCInput::RC_WithDataEnd - 1,
		CRCInput::RC_WithDataEnd
	};

	unsigned char source[8] = { 1, 2, 3, 4, 5, 6, 7, 8 };

	for (size_t i = 0; i < sizeof(edges) / sizeof(edges[0]); i++)
	{
		const long before = test_array_deletes;
		Result<void> r = postPayload(edges[i], source, sizeof(source));
		const long released = test_array_deletes - before;

		INFO("edge " << i << ", message " << edges[i]);
		REQUIRE_FALSE(r.ok());
		REQUIRE(r.error().status == Status::Busy);
		// Once, by whichever of the two rules owns this number. Twice ends the
		// run before this line, none of them leaves the count at zero.
		REQUIRE(released == 1);
	}
}
