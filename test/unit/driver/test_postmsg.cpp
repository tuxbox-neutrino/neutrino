/*
 * test_postmsg.cpp - tests for posting a message to the GUI
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
#include "support/rcfixture.h"

#include <config.h>
#include <global.h>
#include <driver/shutdown_count.h>
#include <timerd/timermanager.h>

#include <cstdlib>
#include <new>
#include <string>
#include <unistd.h>

/* rcinput.cpp reaches these from paths this test never runs. Linking the
 * translation units that define them would pull in the whole application. */
SNeutrinoSettings g_settings;
SglobalInfo g_info;
bool checkLongPress(uint32_t) { return false; }
CTimerManager *CTimerManager::getInstance() { return NULL; }
void CTimerManager::cancelShutdownOnWakeup() {}
SHTDCNT::SHTDCNT() {}
SHTDCNT::~SHTDCNT() {}
SHTDCNT *SHTDCNT::getInstance() { static SHTDCNT one; return &one; }
void SHTDCNT::resetSleepTimer() {}

// Reachable from the other test files, see fakes.h.
long test_array_deletes = 0;

/* Array new is the only allocation form a message payload uses, so replacing
 * it here counts the release without disturbing the rest of the suite. */
void *operator new[](size_t n)
{
	void *p = malloc(n ? n : 1);
	if (p == NULL)
		throw std::bad_alloc();
	return p;
}

void operator delete[](void *p) throw()
{
	if (p != NULL)
		test_array_deletes++;
	free(p);
}

namespace
{

/* Moves the socket every CRCInput in this binary binds off the path a second
 * run of it, or a running box, would already hold. Set once for the whole
 * process rather than per case: every case here shares one and the same
 * number to tell it apart with, and Catch runs them all after every static in
 * the binary has already been constructed, this one included. Restored on
 * the way out so nothing is left in /tmp for the run after. */
struct EventSocketPathGuard
{
	std::string path;

	EventSocketPathGuard() : path(std::string(NEUTRINO_UDS_NAME) + "." + std::to_string(getpid()))
	{
		setEventSocketPathForTest(path.c_str());
	}

	~EventSocketPathGuard()
	{
		setEventSocketPathForTest(NEUTRINO_UDS_NAME);
		unlink(path.c_str());
	}
};

EventSocketPathGuard event_socket_path_guard;

} // namespace

TEST_CASE("two suites do not share one socket", "[harness]")
{
	REQUIRE(std::string(currentEventSocketPath()) != std::string(NEUTRINO_UDS_NAME));
	REQUIRE(std::string(currentEventSocketPath()).find(std::to_string(getpid()))
	        != std::string::npos);
}

TEST_CASE("postMsg hands the message and its payload to the named queue", "[rcinput]")
{
	requireFreeEventSocket();

	CRCInput rc;

	unsigned char *low = new unsigned char[8];
	unsigned char *high = new unsigned char[8];
	REQUIRE(rc.postMsg(NeutrinoMessages::EVT_ZAP_SUB_FAILED, (neutrino_msg_data_t) low, false));
	REQUIRE(rc.postMsg(NeutrinoMessages::EVT_ZAP_FAILED, (neutrino_msg_data_t) high, true));

	// the high queue is served first, so the order of arrival names the queue each post took
	neutrino_msg_t got = 0;
	neutrino_msg_data_t gotdata = 0;
	rc.getMsg_ms(&got, &gotdata, 0);
	REQUIRE(got == NeutrinoMessages::EVT_ZAP_FAILED);
	REQUIRE(gotdata == (neutrino_msg_data_t) high);

	rc.getMsg_ms(&got, &gotdata, 0);
	REQUIRE(got == NeutrinoMessages::EVT_ZAP_SUB_FAILED);
	REQUIRE(gotdata == (neutrino_msg_data_t) low);

	delete[] high;
	delete[] low;
}

TEST_CASE("postMsg reports a refused message and frees its payload", "[rcinput]")
{
	requireFreeEventSocket();

	CRCInput rc;

	const long filled = fillQueue(rc, true);
	REQUIRE(filled > 0);
	REQUIRE(filled < 1000000);

	// the low queue is untouched, so a post that names it still fits
	unsigned char *kept = new unsigned char[8];
	long before = test_array_deletes;
	bool taken = rc.postMsg(NeutrinoMessages::EVT_ZAP_FAILED, (neutrino_msg_data_t) kept, false);
	long delta = test_array_deletes - before;
	REQUIRE(taken);
	REQUIRE(delta == 0);

	unsigned char *dropped = new unsigned char[8];
	before = test_array_deletes;
	taken = rc.postMsg(NeutrinoMessages::EVT_ZAP_FAILED, (neutrino_msg_data_t) dropped);
	delta = test_array_deletes - before;
	REQUIRE_FALSE(taken);
	REQUIRE(delta == 1);

	unsigned char *foreign = new unsigned char[8];
	before = test_array_deletes;
	taken = rc.postMsg(NeutrinoMessages::SHOW_INFOBAR, (neutrino_msg_data_t) foreign);
	delta = test_array_deletes - before;
	REQUIRE_FALSE(taken);
	REQUIRE(delta == 0);

	delete[] foreign;
	delete[] kept;
}

TEST_CASE("a full low queue does not refuse a high priority message", "[rcinput]")
{
	requireFreeEventSocket();

	CRCInput rc;

	const long filled = fillQueue(rc, false);
	REQUIRE(filled > 0);
	REQUIRE(filled < 1000000);
	REQUIRE(rc.postMsg(NeutrinoMessages::SHOW_INFOBAR, 0, true));
}

TEST_CASE("the range the queue frees for is the number the tree writes out", "[rcinput]")
{
	/* Six places still write this interval as CRCInput::RC_WithData + 0x10000000 rather
	   than reading the name below, so moving the name would split the tree rather than
	   move it, in a rule whose two halves disagreeing means a payload freed twice or by
	   nobody. Stated here once, against the numbers themselves. */
	/* Read into locals first. The class declares its constants in the class body and
	   defines none of them out of line, so binding one to a reference does not link. */
	const neutrino_msg_t first = CRCInput::RC_WithData;
	const neutrino_msg_t past_last = CRCInput::RC_WithDataEnd;

	REQUIRE(first == 0xA0000000UL);
	REQUIRE(past_last == 0xB0000000UL);
	REQUIRE(past_last == first + 0x10000000UL);
}

TEST_CASE("the queue releases a refused payload at the edges of its own range", "[rcinput]")
{
	requireFreeEventSocket();

	CRCInput rc;

	// A refusal is the only answer that releases anything, so the queue has to
	// be turning everything away before any of this can be seen.
	const long filled = fillQueue(rc, true);
	REQUIRE(filled > 0);
	REQUIRE(filled < 1000000);

	/* Both numbers come from the two names the queue's own condition is
	   written with, so nothing about the range is stated a second time here.
	   What is checked is that rule at both of its edges: the layer above asks
	   the same question to decide whether to release a refused payload, and a
	   rule nobody measures at its edges is one that can be narrowed without
	   anything noticing. */
	struct edge { neutrino_msg_t msg; long released; };
	const edge edges[] = {
		{ CRCInput::RC_WithData - 1,     0 },
		{ CRCInput::RC_WithData,         1 },
		{ CRCInput::RC_WithDataEnd - 1,  1 },
		{ CRCInput::RC_WithDataEnd,      0 }
	};

	for (size_t i = 0; i < sizeof(edges) / sizeof(edges[0]); i++)
	{
		unsigned char *block = new unsigned char[8];
		const long before = test_array_deletes;
		const bool taken = rc.postMsg(edges[i].msg, (neutrino_msg_data_t) block);
		const long released = test_array_deletes - before;

		// Measured first, so that whatever the queue left behind is ours to
		// release whether the checks below hold or not.
		if (released == 0)
			delete[] block;

		INFO("edge " << i << ", message " << edges[i].msg);
		REQUIRE_FALSE(taken);
		REQUIRE(released == edges[i].released);
	}
}
