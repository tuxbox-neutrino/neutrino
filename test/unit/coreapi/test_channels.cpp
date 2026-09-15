/*
 * test_channels.cpp - tests for channels and zapping
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
#include "coreapi/channels.h"

#include <neutrinoMessages.h>
#include <timerdclient/timerdtypes.h>

#include <cstring>
#include <string>

using namespace coreapi;

namespace
{
// Ids as the box hands them out. The satellite position and the transport id
// live above bit 31, so a 32 bit lookup would name a different channel.
const ChannelId ERSTE  = 0xb9b0040200016dcbULL;
const ChannelId ZDF    = 0xbdf8040300017a01ULL;
const ChannelId DLF    = 0xb9b004020001780fULL;
const ChannelId BYTEFM = 0xbdf804030001788aULL;
// Shares its low half with ERSTE and differs only above bit 31.
const ChannelId ABSENT = 0x1e24040200016dcbULL;

// Every name is long enough to be held on the heap, or a channel's name would
// live inside the channel and its address would move with any copy.
const char *ERSTE_NAME = "Das Erste HD Regionalfenster";

ChannelInfo mk(ChannelId id, const char *name, ServiceKind kind)
{
	ChannelInfo c;
	c.id = id;
	c.name = name;
	c.kind = kind;
	if (kind == ServiceKind::WebTv || kind == ServiceKind::WebRadio)
		c.url = "http://example.invalid/stream";
	return c;
}

// Reads a zap payload back and frees it the way the message loop frees it.
CTimerd::EventInfo takePayload(neutrino_msg_data_t data)
{
	unsigned char *payload = (unsigned char *) data;
	CTimerd::EventInfo info;
	std::memcpy(&info, payload, sizeof(info));
	delete[] payload;
	return info;
}
}

TEST_CASE("the payload address checks need a string that is not copy on write", "[channels]")
{
	// A copy on write string keeps its buffer through a copy, which would
	// leave the two channel payload checks below unable to see a copy.
	std::string a(200, 'x');
	std::string b(a);
	REQUIRE(a.data() != b.data());
}

TEST_CASE("list splits on radio rather than on a single service kind", "[channels]")
{
	FakeChannelSource fake;
	fake.channels.push_back(mk(ERSTE, ERSTE_NAME, ServiceKind::Tv));
	fake.channels.push_back(mk(ZDF, "Tagesschau24 Livestream", ServiceKind::WebTv));
	fake.channels.push_back(mk(DLF, "Deutschlandfunk Kultur", ServiceKind::Radio));
	fake.channels.push_back(mk(BYTEFM, "ByteFM Hamburg Livestream", ServiceKind::WebRadio));
	InstalledChannelSource installed_source(&fake);

	Result<ChannelList> tv = channels::list(true);
	REQUIRE(tv.ok());
	REQUIRE(tv.value().size() == 2);
	REQUIRE(tv.value()[0].name == ERSTE_NAME);
	REQUIRE(tv.value()[1].name == "Tagesschau24 Livestream");
	// The list the source filled, not a copy of it.
	REQUIRE((const void *) &tv.value()[0] == fake.last_buffer);

	Result<ChannelList> radio = channels::list(false);
	REQUIRE(radio.ok());
	REQUIRE(radio.value().size() == 2);
	REQUIRE(radio.value()[0].name == "Deutschlandfunk Kultur");
	REQUIRE(radio.value()[1].name == "ByteFM Hamburg Livestream");
	REQUIRE((const void *) &radio.value()[0] == fake.last_buffer);
}

TEST_CASE("an unreadable channel list names itself", "[channels]")
{
	FakeChannelSource fake;
	fake.channels_status = Status::Busy;
	InstalledChannelSource installed_source(&fake);

	Result<ChannelList> r = channels::list(true);
	REQUIRE_FALSE(r.ok());
	REQUIRE(r.error().status == Status::Busy);
	REQUIRE(r.error().code == ErrorCode::ChannelListUnavailable);
	REQUIRE(r.error().message == std::string("the channel list could not be read"));
}

TEST_CASE("get matches the whole id rather than its low half", "[channels]")
{
	FakeChannelSource fake;
	fake.channels.push_back(mk(ERSTE, ERSTE_NAME, ServiceKind::Tv));
	InstalledChannelSource installed_source(&fake);

	Result<ChannelInfo> good = channels::get(ERSTE);
	REQUIRE(good.ok());
	REQUIRE(good.value().name == ERSTE_NAME);
	REQUIRE(good.value().id == ERSTE);
	REQUIRE((const void *) good.value().name.data() == fake.last_buffer);

	// Only the high half differs, so a truncated lookup would answer this
	// with the channel above.
	Result<ChannelInfo> bad = channels::get(ABSENT);
	REQUIRE_FALSE(bad.ok());
	REQUIRE(bad.error().status == Status::NotFound);
	REQUIRE(bad.error().code == ErrorCode::NoSuchChannel);
	REQUIRE(bad.error().message == std::string("no channel with that id"));
}

TEST_CASE("get separates a missing channel from an unreadable map", "[channels]")
{
	FakeChannelSource fake;
	fake.channels_status = Status::Internal;
	InstalledChannelSource installed_source(&fake);

	Result<ChannelInfo> r = channels::get(ERSTE);
	REQUIRE_FALSE(r.ok());
	REQUIRE(r.error().status == Status::Internal);
	REQUIRE(r.error().code == ErrorCode::ChannelListUnavailable);
	REQUIRE(r.error().message == std::string("the channel list could not be read"));
}

TEST_CASE("current reports NotFound when nothing is running", "[channels]")
{
	FakeChannelSource fake;
	InstalledChannelSource installed_source(&fake);

	Result<ChannelInfo> r = channels::current();
	REQUIRE_FALSE(r.ok());
	REQUIRE(r.error().status == Status::NotFound);
	REQUIRE(r.error().code == ErrorCode::NoRunningChannel);
	REQUIRE(r.error().message == std::string("nothing is playing"));

	fake.current = mk(ERSTE, ERSTE_NAME, ServiceKind::Tv);
	fake.current_status = Status::Ok;

	Result<ChannelInfo> running = channels::current();
	REQUIRE(running.ok());
	REQUIRE(running.value().name == ERSTE_NAME);
	REQUIRE((const void *) running.value().name.data() == fake.last_buffer);
}

TEST_CASE("a running channel the map does not hold is not an idle box", "[channels]")
{
	FakeChannelSource fake;
	fake.current_status = Status::Internal;
	InstalledChannelSource installed_source(&fake);

	Result<ChannelInfo> r = channels::current();
	REQUIRE_FALSE(r.ok());
	REQUIRE(r.error().status == Status::Internal);
	REQUIRE(r.error().code == ErrorCode::CurrentChannelUnresolved);
	REQUIRE(r.error().message == std::string("the running channel is not in the channel list"));
}

TEST_CASE("bouquets are returned as values", "[channels]")
{
	FakeChannelSource fake;
	BouquetInfo b;
	b.id = 1;
	b.name = "Favoriten";
	b.tv_count = 3;
	fake.bouquets.push_back(b);
	InstalledChannelSource installed_source(&fake);

	Result<BouquetList> good = channels::bouquets();
	REQUIRE(good.ok());
	REQUIRE(good.value().size() == 1);
	REQUIRE(good.value()[0].name == "Favoriten");
	REQUIRE(good.value()[0].tv_count == 3);
	REQUIRE((const void *) &good.value()[0] == fake.last_buffer);
}

TEST_CASE("bouquetChannels resolves the id it was given", "[channels]")
{
	FakeChannelSource fake;
	fake.bouquet_members[1].push_back(mk(ERSTE, ERSTE_NAME, ServiceKind::Tv));
	fake.bouquet_members[2].push_back(mk(ZDF, "ZDF HD Hauptprogramm", ServiceKind::Tv));
	fake.bouquet_members[2].push_back(mk(DLF, "Deutschlandfunk Kultur", ServiceKind::Radio));
	InstalledChannelSource installed_source(&fake);

	Result<ChannelList> good = channels::bouquetChannels(2);
	REQUIRE(good.ok());
	REQUIRE(good.value().size() == 2);
	REQUIRE(good.value()[0].name == "ZDF HD Hauptprogramm");
	REQUIRE(good.value()[1].name == "Deutschlandfunk Kultur");
	REQUIRE((const void *) &good.value()[0] == fake.last_buffer);

	Result<ChannelList> first = channels::bouquetChannels(1);
	REQUIRE(first.ok());
	REQUIRE(first.value().size() == 1);
	REQUIRE(first.value()[0].name == ERSTE_NAME);

	Result<ChannelList> bad = channels::bouquetChannels(7);
	REQUIRE_FALSE(bad.ok());
	REQUIRE(bad.error().status == Status::NotFound);
	REQUIRE(bad.error().code == ErrorCode::NoSuchBouquet);
	REQUIRE(bad.error().message == std::string("no bouquet with that id"));
}

TEST_CASE("a missing bouquet manager fails both bouquet reads the same way", "[channels]")
{
	FakeChannelSource fake;
	fake.bouquet_members[1].push_back(mk(ERSTE, ERSTE_NAME, ServiceKind::Tv));
	// The one cause behind both reads on a box that started without a
	// bouquet manager.
	fake.bouquets_status = Status::Internal;
	InstalledChannelSource installed_source(&fake);

	Result<BouquetList> all = channels::bouquets();
	REQUIRE_FALSE(all.ok());
	REQUIRE(all.error().status == Status::Internal);
	REQUIRE(all.error().code == ErrorCode::BouquetListUnavailable);
	REQUIRE(all.error().message == std::string("the bouquet list could not be read"));

	// A bouquet that exists must not come back as absent just because the
	// manager is gone.
	Result<ChannelList> members = channels::bouquetChannels(1);
	REQUIRE_FALSE(members.ok());
	REQUIRE(members.error().status == Status::Internal);
	REQUIRE(members.error().code == ErrorCode::BouquetListUnavailable);
	REQUIRE(members.error().message == std::string("the bouquet list could not be read"));
}

TEST_CASE("every read hands the source's status on unchanged", "[channels]")
{
	// Two of them, or a hard coded status would match the only one a read
	// ever produced and pass.
	const Status reported[] = { Status::Busy, Status::Internal };

	for (size_t i = 0; i < sizeof(reported) / sizeof(reported[0]); i++)
	{
		FakeChannelSource fake;
		fake.channels_status = reported[i];
		fake.bouquets_status = reported[i];
		fake.current_status  = reported[i];
		fake.mode_status     = reported[i];
		InstalledChannelSource installed_source(&fake);

		Result<ChannelList> tv = channels::list(true);
		REQUIRE_FALSE(tv.ok());
		REQUIRE(tv.error().status == reported[i]);

		Result<ChannelInfo> one = channels::get(ERSTE);
		REQUIRE_FALSE(one.ok());
		REQUIRE(one.error().status == reported[i]);

		Result<ChannelInfo> now = channels::current();
		REQUIRE_FALSE(now.ok());
		REQUIRE(now.error().status == reported[i]);

		Result<BouquetList> all = channels::bouquets();
		REQUIRE_FALSE(all.ok());
		REQUIRE(all.error().status == reported[i]);

		Result<ChannelList> members = channels::bouquetChannels(1);
		REQUIRE_FALSE(members.ok());
		REQUIRE(members.error().status == reported[i]);

		Result<int> box = channels::mode();
		REQUIRE_FALSE(box.ok());
		REQUIRE(box.error().status == reported[i]);
	}
}

TEST_CASE("zap posts the loop's zap command carrying the whole id", "[channels]")
{
	FakeChannelSource fake;
	fake.channels.push_back(mk(ERSTE, ERSTE_NAME, ServiceKind::Tv));
	InstalledChannelSource installed_source(&fake);

	FakeCommandSink sink;
	InstalledSink installed_sink(&sink);

	const long before = test_array_deletes;
	Result<void> r = channels::zap(ERSTE);
	const long released = test_array_deletes - before;

	REQUIRE(r.ok());
	REQUIRE(sink.posted.size() == 1);
	REQUIRE(sink.posted[0].first == NeutrinoMessages::ZAPTO);
	REQUIRE(sink.posted[0].second != 0);

	// Handed over rather than released: the loop frees what it was given.
	REQUIRE(released == 0);

	CTimerd::EventInfo info = takePayload(sink.posted[0].second);
	// Only the half above bit 31 tells this id from ABSENT, so a narrowed
	// payload would zap somewhere else.
	REQUIRE(info.channel_id == ERSTE);
	// The loop reads one field out of the block, the rest travels with it.
	REQUIRE(info.epg_id == 0);
	REQUIRE(info.epg_starttime == 0);
	REQUIRE(info.apids == 0);
	REQUIRE_FALSE(info.recordingSafety);
}

TEST_CASE("a zap to an id nobody has is not posted", "[channels]")
{
	FakeChannelSource fake;
	fake.channels.push_back(mk(ERSTE, ERSTE_NAME, ServiceKind::Tv));
	InstalledChannelSource installed_source(&fake);

	FakeCommandSink sink;
	InstalledSink installed_sink(&sink);

	// Differs from the channel above only over bit 31.
	Result<void> r = channels::zap(ABSENT);
	REQUIRE_FALSE(r.ok());
	REQUIRE(r.error().status == Status::NotFound);
	REQUIRE(r.error().code == ErrorCode::NoSuchChannel);
	REQUIRE(r.error().message == std::string("no channel with that id"));
	// A posted command answers nothing, so this one would be lost unseen.
	REQUIRE(sink.posted.empty());
}

TEST_CASE("an unreadable channel list stops a zap before it is posted", "[channels]")
{
	FakeChannelSource fake;
	fake.channels_status = Status::Internal;
	InstalledChannelSource installed_source(&fake);

	FakeCommandSink sink;
	InstalledSink installed_sink(&sink);

	Result<void> r = channels::zap(ERSTE);
	REQUIRE_FALSE(r.ok());
	REQUIRE(r.error().status == Status::Internal);
	REQUIRE(r.error().code == ErrorCode::ChannelListUnavailable);
	REQUIRE(r.error().message == std::string("the channel list could not be read"));
	REQUIRE(sink.posted.empty());
}

TEST_CASE("setMode posts the mode the loop switches on", "[channels]")
{
	FakeCommandSink sink;
	InstalledSink installed_sink(&sink);

	REQUIRE(channels::setMode(channels::Mode::Radio).ok());
	REQUIRE(channels::setMode(channels::Mode::Tv).ok());

	REQUIRE(sink.posted.size() == 2);
	REQUIRE(sink.posted[0].first == NeutrinoMessages::CHANGEMODE);
	REQUIRE(sink.posted[0].second == (neutrino_msg_data_t) NeutrinoModes::mode_radio);
	REQUIRE(sink.posted[1].first == NeutrinoMessages::CHANGEMODE);
	REQUIRE(sink.posted[1].second == (neutrino_msg_data_t) NeutrinoModes::mode_tv);
	// The mode alone: the flag that rides beside it would keep the old channel.
	REQUIRE((sink.posted[0].second & (neutrino_msg_data_t) NeutrinoModes::norezap) == 0);
}

TEST_CASE("a refused command surfaces as the queue's own answer", "[channels]")
{
	// Two of them, or a hard coded status would match the only one a refusal
	// ever produced and pass.
	const Status refused[] = { Status::Busy, Status::Internal };

	for (size_t i = 0; i < sizeof(refused) / sizeof(refused[0]); i++)
	{
		FakeChannelSource fake;
		fake.channels.push_back(mk(ERSTE, ERSTE_NAME, ServiceKind::Tv));
		InstalledChannelSource installed_source(&fake);

		FakeCommandSink sink;
		sink.answer = refused[i];
		InstalledSink installed_sink(&sink);

		Result<void> z = channels::zap(ERSTE);
		REQUIRE_FALSE(z.ok());
		REQUIRE(z.error().status == refused[i]);
		REQUIRE(z.error().code == ErrorCode::CommandNotPosted);
		REQUIRE(z.error().message == std::string("the box did not take the command"));

		Result<void> m = channels::setMode(channels::Mode::Radio);
		REQUIRE_FALSE(m.ok());
		REQUIRE(m.error().status == refused[i]);
		REQUIRE(m.error().code == ErrorCode::CommandNotPosted);
		REQUIRE(m.error().message == std::string("the box did not take the command"));
	}
}

TEST_CASE("mode answers with the box mode rather than the two a command takes", "[channels]")
{
	FakeChannelSource fake;
	// Neither television nor radio, and a caller has to be able to see it.
	fake.mode = NeutrinoModes::mode_standby;
	InstalledChannelSource installed_source(&fake);

	Result<int> r = channels::mode();
	REQUIRE(r.ok());
	REQUIRE(r.value() == NeutrinoModes::mode_standby);

	// The control API compares this word whole against the mode constants, so
	// a masked answer would rename a mode that carried anything above the mask
	// instead of leaving it unrecognised.
	fake.mode = NeutrinoModes::mode_tv | NeutrinoModes::norezap;
	Result<int> flagged = channels::mode();
	REQUIRE(flagged.ok());
	REQUIRE(flagged.value() == (NeutrinoModes::mode_tv | NeutrinoModes::norezap));

	fake.mode_status = Status::NotFound;
	Result<int> none = channels::mode();
	REQUIRE_FALSE(none.ok());
	REQUIRE(none.error().status == Status::NotFound);
	REQUIRE(none.error().code == ErrorCode::NoMode);
	REQUIRE(none.error().message == std::string("the box has not picked a mode yet"));

	fake.mode_status = Status::Internal;
	Result<int> broken = channels::mode();
	REQUIRE_FALSE(broken.ok());
	REQUIRE(broken.error().status == Status::Internal);
	REQUIRE(broken.error().code == ErrorCode::ModeUnavailable);
	REQUIRE(broken.error().message == std::string("the box mode could not be read"));
}

TEST_CASE("a refused zap leaves no payload behind", "[channels]")
{
	FakeChannelSource fake;
	fake.channels.push_back(mk(ERSTE, ERSTE_NAME, ServiceKind::Tv));
	InstalledChannelSource installed_source(&fake);

	FakeCommandSink sink;
	sink.answer = Status::Busy;
	InstalledSink installed_sink(&sink);

	/* The queue frees a refused payload only for the messages whose number
	   says they carry one, and this one's does not, so the release has to
	   happen on the way out of the command. */
	const long before = test_array_deletes;
	Result<void> r = channels::zap(ERSTE);
	const long released = test_array_deletes - before;

	REQUIRE_FALSE(r.ok());
	REQUIRE(sink.posted.size() == 1);
	REQUIRE(sink.posted[0].second != 0);
	REQUIRE(released == 1);
}
