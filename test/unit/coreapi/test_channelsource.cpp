/*
 * test_channelsource.cpp - tests for the channel source
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

using namespace coreapi;

TEST_CASE("the service kind covers every combination of radio and url", "[types]")
{
	REQUIRE(serviceKind(false, false) == ServiceKind::Tv);
	REQUIRE(serviceKind(false, true)  == ServiceKind::WebTv);
	REQUIRE(serviceKind(true,  false) == ServiceKind::Radio);
	REQUIRE(serviceKind(true,  true)  == ServiceKind::WebRadio);

	// Nothing classifies as Unknown; it is what an unfilled record keeps.
	REQUIRE(ChannelInfo().kind == ServiceKind::Unknown);
}

TEST_CASE("the channel source is injectable", "[deps]")
{
	FakeChannelSource fake;
	ChannelInfo c;
	c.id = 0xb9b0040200016dcbULL;
	c.name = "Das Erste HD";
	c.kind = ServiceKind::Tv;
	fake.channels.push_back(c);

	InstalledChannelSource installed_source(&fake);

	ChannelInfo found;
	REQUIRE(channelSource().findChannel(0xb9b0040200016dcbULL, found) == Status::Ok);
	REQUIRE(found.name == "Das Erste HD");
	REQUIRE(channelSource().findChannel(0xbdf8040300017a01ULL, found) == Status::NotFound);
}

TEST_CASE("the channel source separates television from radio", "[deps]")
{
	FakeChannelSource fake;

	ChannelInfo tv;
	tv.id = 1;
	tv.kind = ServiceKind::Tv;
	fake.channels.push_back(tv);

	ChannelInfo webtv;
	webtv.id = 2;
	webtv.kind = ServiceKind::WebTv;
	webtv.url = "http://example.invalid/stream";
	fake.channels.push_back(webtv);

	ChannelInfo radio;
	radio.id = 3;
	radio.kind = ServiceKind::Radio;
	fake.channels.push_back(radio);

	InstalledChannelSource installed_source(&fake);

	ChannelList out;
	REQUIRE(channelSource().allChannels(true, out) == Status::Ok);
	REQUIRE(out.size() == 2);
	REQUIRE(out[0].id == 1);
	REQUIRE(out[1].id == 2);

	REQUIRE(channelSource().allChannels(false, out) == Status::Ok);
	REQUIRE(out.size() == 1);
	REQUIRE(out[0].id == 3);
}
