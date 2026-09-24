/*
 * test_sse.cpp - tests for the event stream
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
 *
 * DELIBERATE DRIFT from the source's test_sse.cpp: their wire cases drive the
 * libmicrohttpd server this tree does not carry. The transport-free cases
 * are here as written; the wire cases follow with nhttpd's own harness in
 * the bridge slice.
 */

#include "support/catch.hpp"

#include "httpd/endpoint.h"
#include "httpd/events.h"
#include "httpd/http.h"
#include "httpd/router.h"

#include "coreapi/base/eventbus.h"

#include <set>
#include <string>

using namespace httpd;

namespace
{

// What the header beside the code says these are, so that a case restoring
// them puts back what ships and a case reading them fails when the shipped
// value moves without anybody saying so.
const unsigned kShippedHeartbeatMs = 20000;
const size_t   kShippedCeiling     = 256u * 1024u;

} // namespace

TEST_CASE("every event type has a name and no two share one", "[sse]")
{
	std::set<std::string> seen;
	for (int i = 0; i <= (int) coreapi::EventType::SettingsChanged; ++i)
	{
		const char *n = events::typeName((coreapi::EventType) i);
		INFO("type value " << i);
		REQUIRE(n != NULL);
		REQUIRE(std::string(n).size() > 0);
		REQUIRE(seen.insert(n).second);
	}
	REQUIRE(seen.size() == 11);
}

TEST_CASE("the shipped beat and the shipped ceiling are what the header says", "[sse]")
{
	/* Both are settable, and both are set by the cases below to something a
	   run can afford to wait for. What ships is the pair a box runs on, and
	   nothing else in this file would notice it moving. */
	REQUIRE(events::heartbeatMs() == kShippedHeartbeatMs);
	REQUIRE(events::pendingCeiling() == kShippedCeiling);
}

TEST_CASE("the route is a read and answers with a request for a stream", "[sse]")
{
	// The gate is asked before the handler, so a caller with less than the
	// route declares never reaches the answer that would open one.
	Response denied = dispatchIn(eventsTable, Method::Get, "/api/v1/events",
	                             "", "", "127.0.0.1", AuthLevel::Public);
	REQUIRE(denied.code == 403);
	REQUIRE_FALSE(events::isStream(denied));

	Response allowed = dispatchIn(eventsTable, Method::Get, "/api/v1/events",
	                              "", "", "127.0.0.1", AuthLevel::Read);
	REQUIRE(allowed.code == 200);
	REQUIRE(events::isStream(allowed));
	REQUIRE(allowed.body.empty());
}
