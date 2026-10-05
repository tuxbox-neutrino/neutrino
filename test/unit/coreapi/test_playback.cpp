/*
 * test_playback.cpp - tests for what the television shows
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

#include "coreapi/archive.h"
#include "coreapi/channels.h"
#include "coreapi/playback.h"
#include "coreapi/base/deps.h"
#include "coreapi/base/errors.h"
#include "coreapi/base/eventbus.h"

#include "httpd/endpoint.h"
#include "httpd/http.h"
#include "httpd/router.h"

#include "jsoncpp/json/json.h"

#include <neutrinoMessages.h>

#include <cstdio>
#include <memory>
#include <string>
#include <vector>

#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>

using namespace coreapi;

namespace
{

// Subscribed for its own lifetime: the bus outlives the case, and a failed REQUIRE
// must not leave it holding this.
struct Collector : public Subscriber
{
	std::vector<Event> seen;
	Collector() { EventBus::instance().subscribe(this); }
	~Collector() { EventBus::instance().unsubscribe(this); }
	void onEvent(const Event &e)
	{
		if (e.type == EventType::Playback)
			seen.push_back(e);
	}
};

// A record directory with real files, removed whole afterwards.
struct Shelf
{
	std::string base;
	std::vector<std::string> made;
	std::vector<std::string> dirs;
	FakeSettingsSource      settings;
	InstalledSettingsSource in_settings;

	Shelf() : in_settings(&settings)
	{
		char tmpl[] = "/tmp/coreapi_playback_XXXXXX";
		// Without it every file below would land under /.
		REQUIRE(mkdtemp(tmpl) != NULL);
		base = tmpl;
		settings.strings["network_nfs_recordingdir"] = base;
		playback::end();
	}
	~Shelf()
	{
		playback::end();
		for (size_t i = made.size(); i > 0; --i)
			unlink(made[i - 1].c_str());
		for (size_t i = dirs.size(); i > 0; --i)
			rmdir(dirs[i - 1].c_str());
		rmdir(base.c_str());
	}
	std::string file(const std::string &rel, const std::string &body)
	{
		const std::string p = base + "/" + rel;
		FILE *f = std::fopen(p.c_str(), "wb");
		if (f == NULL)
			return std::string();
		std::fwrite(body.data(), 1, body.size(), f);
		std::fclose(f);
		made.push_back(p);
		return p;
	}
	void dir(const std::string &rel)
	{
		mkdir((base + "/" + rel).c_str(), 0700);
		dirs.push_back(base + "/" + rel);
	}
	std::string recording(const std::string &stem, const std::string &title)
	{
		file(stem + ".xml", "<neutrino><record><channelname>ZDF HD</channelname><epgtitle>" + title +
		     "</epgtitle><length>30</length></record></neutrino>");
		return file(stem + ".ts", std::string(188, 'G'));
	}
};

playback::Sample at(int position_s, playback::State state = playback::State::Playing, int speed = 1)
{
	playback::Sample s;
	s.position_ms = position_s * 1000;
	s.duration_ms = 1800 * 1000;
	s.state = state;
	s.speed = speed;
	return s;
}

playback::Started started(const std::string &path, const std::string &title = std::string())
{
	playback::Started s;
	s.path = path;
	s.title = title;
	return s;
}

::Json::Value parsed(const std::string &text)
{
	::Json::Value v;
	::Json::CharReaderBuilder b;
	std::string errs;
	std::unique_ptr< ::Json::CharReader> r(b.newCharReader());
	r->parse(text.data(), text.data() + text.size(), &v, &errs);
	return v;
}

httpd::Response askPlayback()
{
	return httpd::dispatch(httpd::Get, "/api/v1/playback", "", std::string(), "192.168.1.9", httpd::AuthLevel::Read);
}

coreapi::ChannelInfo channel(coreapi::ChannelId id, const std::string &name)
{
	coreapi::ChannelInfo c;
	c.id = id;
	c.name = name;
	return c;
}

} // namespace

TEST_CASE("the player's snapshot is empty until a file starts and again once it ends", "[playback]")
{
	Shelf shelf;
	REQUIRE_FALSE(playback::snapshot().active);
	playback::begin(started("/media/usb/Filme/Urlaub 2026.mkv", "Urlaub"));
	playback::Snapshot s = playback::snapshot();
	REQUIRE(s.active);
	REQUIRE(s.name == "Urlaub 2026.mkv");
	REQUIRE(s.title == "Urlaub");
	REQUIRE(s.archive_id.empty());
	REQUIRE(s.state == playback::State::Playing);
	playback::observeAt(at(90, playback::State::Paused, 0), 1000);
	s = playback::snapshot();
	REQUIRE(s.position == 90);
	REQUIRE(s.duration == 1800);
	REQUIRE(s.state == playback::State::Paused);
	playback::end();
	REQUIRE_FALSE(playback::snapshot().active);
}

TEST_CASE("a recording of the archive is named by the archive's own id", "[playback]")
{
	Shelf shelf;
	const std::string top = shelf.recording("Tatort_20261001", "Tatort");
	shelf.dir("Serien");
	const std::string deep = shelf.recording("Serien/Lanz_20261002", "Lanz");
	const archive::Page page = archive::list("", 0, 10).value();
	REQUIRE(page.items.size() == 2);

	for (size_t i = 0; i < page.items.size(); ++i)
	{
		const archive::Entry &e = page.items[i];
		playback::begin(started(e.path));
		INFO(e.path);
		REQUIRE(playback::snapshot().archive_id == e.id);
		// The metadata stands in where the player knew no title.
		REQUIRE(playback::snapshot().title == e.title);
		REQUIRE(playback::snapshot().channel == "ZDF HD");
	}
	REQUIRE(top != deep);
}

TEST_CASE("a recording the player knows no length of runs as long as the archive says", "[playback]")
{
	Shelf shelf;
	FakeChannelSource channels;
	InstalledChannelSource in_channels(&channels);
	channels.mode = NeutrinoModes::mode_tv;
	const std::string path = shelf.recording("Tatort_20261001", "Tatort");
	playback::Sample unknown = at(90);
	unknown.duration_ms = 0;

	playback::begin(started(path));
	REQUIRE(playback::snapshot().duration == 1800);
	playback::observeAt(unknown, 1000);
	REQUIRE(playback::snapshot().duration == 1800);
	REQUIRE(playback::snapshot().position == 90);
	REQUIRE(parsed(askPlayback().body)["duration"].asInt() == 1800);
	// What the player measures wins over the metadata.
	playback::Sample measured = unknown;
	measured.duration_ms = 1750 * 1000;
	playback::observeAt(measured, 2000);
	REQUIRE(playback::snapshot().duration == 1750);

	playback::begin(started(shelf.file("stray.ts", std::string(188, 'G'))));
	playback::observeAt(unknown, 3000);
	REQUIRE(playback::snapshot().duration == 0);
}

TEST_CASE("a file outside the archive or without metadata is a file and keeps no directory", "[playback]")
{
	Shelf shelf;
	const std::string stray = shelf.file("stray.ts", std::string(188, 'G'));
	playback::begin(started(stray));
	REQUIRE(playback::snapshot().archive_id.empty());
	REQUIRE(playback::snapshot().name == "stray.ts");
	playback::begin(started("http://example.org/live/stream.m3u8?token=secret#x"));
	REQUIRE(playback::snapshot().name == "stream.m3u8");
	// A timeshift is the channel, even where its file lies in the record directory.
	playback::Started shift = started(shelf.recording("Shift", "Live"));
	shift.timeshift = true;
	playback::begin(shift);
	REQUIRE(playback::snapshot().archive_id.empty());
	REQUIRE(playback::snapshot().timeshift);
}

TEST_CASE("the player publishes its start, every change of state, every jump and its end", "[playback]")
{
	Shelf shelf;
	const std::string path = shelf.recording("Tatort", "Tatort");
	const std::string id = archive::list("", 0, 1).value().items[0].id;
	Collector c;

	playback::Started s = started(path);
	s.channel_id = 0x2b66;
	playback::begin(s);
	REQUIRE(c.seen.empty());
	playback::observeAt(at(600), 10000);
	REQUIRE(c.seen.size() == 1);
	REQUIRE(c.seen[0].text == "playing recording " + id);
	REQUIRE(c.seen[0].value == 600);
	REQUIRE(c.seen[0].channel_id == 0x2b66);

	// Steady ticks say nothing, and neither does a loop held up by a menu.
	playback::observeAt(at(601), 11000);
	playback::observeAt(at(602), 12000);
	playback::observeAt(at(642), 52000);
	REQUIRE(c.seen.size() == 1);

	playback::observeAt(at(642, playback::State::Paused, 0), 53000);
	playback::observeAt(at(642, playback::State::Paused, 0), 90000);
	REQUIRE(c.seen.size() == 2);
	REQUIRE(c.seen[1].text == "paused recording " + id);

	playback::observeAt(at(642), 91000);
	REQUIRE(c.seen.size() == 3);
	REQUIRE(c.seen[2].text == "playing recording " + id);

	// A jump ahead and one back, each once.
	playback::observeAt(at(943), 92000);
	playback::observeAt(at(944), 93000);
	playback::observeAt(at(884), 94000);
	REQUIRE(c.seen.size() == 5);
	REQUIRE(c.seen[3].value == 943);
	REQUIRE(c.seen[4].value == 884);
	REQUIRE(c.seen[4].text == "playing recording " + id);

	playback::observeAt(at(900, playback::State::Forward, 4), 95000);
	playback::observeAt(at(1000, playback::State::Forward, 4), 96000);
	playback::observeAt(at(980, playback::State::Rewind, -2), 97000);
	REQUIRE(c.seen.size() == 7);
	REQUIRE(c.seen[5].text == "forward recording " + id);
	REQUIRE(c.seen[6].text == "rewind recording " + id);

	playback::end();
	REQUIRE(c.seen.size() == 8);
	REQUIRE(c.seen[7].text == "stopped recording " + id);
	REQUIRE(c.seen[7].value == 980);
	playback::end();
	playback::observeAt(at(5), 98000);
	REQUIRE(c.seen.size() == 8);
}

TEST_CASE("a file and a timeshift name their source and no id", "[playback]")
{
	Shelf shelf;
	Collector c;
	playback::begin(started("/media/usb/a.mkv"));
	playback::observeAt(at(0), 1000);
	playback::Started shift = started("/media/hdd/.timeshift/live.ts");
	shift.timeshift = true;
	shift.channel_id = 0x77;
	playback::begin(shift);
	playback::observeAt(at(3), 2000);
	playback::end();
	REQUIRE(c.seen.size() == 3);
	REQUIRE(c.seen[0].text == "playing file");
	REQUIRE(c.seen[1].text == "playing channel");
	REQUIRE(c.seen[1].channel_id == 0x77);
	REQUIRE(c.seen[2].text == "stopped channel");
}

TEST_CASE("the playback route says nothing plays in standby or with no channel", "[playback][routes]")
{
	Shelf shelf;
	FakeChannelSource channels;
	InstalledChannelSource in_channels(&channels);
	channels.mode = NeutrinoModes::mode_tv;
	httpd::Response r = askPlayback();
	REQUIRE(r.code == 200);
	REQUIRE(parsed(r.body)["source"].asString() == "none");

	channels.current = channel(0x2b66, "Das Erste HD");
	channels.current_status = Status::Ok;
	channels.mode = NeutrinoModes::mode_standby;
	playback::begin(started("/media/usb/a.mkv"));
	r = askPlayback();
	REQUIRE(parsed(r.body)["source"].asString() == "none");
	REQUIRE(parsed(r.body).size() == 1);
}

TEST_CASE("the playback route answers the live channel and its timeshift", "[playback][routes]")
{
	Shelf shelf;
	FakeChannelSource channels;
	InstalledChannelSource in_channels(&channels);
	channels.mode = NeutrinoModes::mode_tv;
	channels.current = channel(0x2b66, "Das Erste HD");
	channels.current_status = Status::Ok;

	::Json::Value v = parsed(askPlayback().body);
	REQUIRE(v["source"].asString() == "channel");
	REQUIRE(v["channel"]["id"].asString() == "2b66");
	REQUIRE(v["channel"]["name"].asString() == "Das Erste HD");
	REQUIRE_FALSE(v["timeshift"].asBool());
	REQUIRE_FALSE(v.isMember("position"));

	playback::Started shift = started("/media/hdd/.timeshift/live.ts");
	shift.timeshift = true;
	playback::begin(shift);
	v = parsed(askPlayback().body);
	REQUIRE(v["source"].asString() == "channel");
	REQUIRE(v["timeshift"].asBool());

	channels.current_status = Status::Internal;
	httpd::Response r = askPlayback();
	REQUIRE(r.code == 500);
	REQUIRE(r.body.find("current-channel-unresolved") != std::string::npos);
}

TEST_CASE("the playback route answers a recording and a file with where the player stands", "[playback][routes]")
{
	Shelf shelf;
	FakeChannelSource channels;
	InstalledChannelSource in_channels(&channels);
	channels.mode = NeutrinoModes::mode_tv;
	channels.current = channel(0x2b66, "Das Erste HD");
	channels.current_status = Status::Ok;
	const std::string path = shelf.recording("Tatort", "Tatort: Murot");
	const std::string id = archive::list("", 0, 1).value().items[0].id;

	playback::begin(started(path));
	playback::observeAt(at(754, playback::State::Paused, 0), 1000);
	::Json::Value v = parsed(askPlayback().body);
	REQUIRE(v["source"].asString() == "recording");
	REQUIRE(v["recording"]["id"].asString() == id);
	REQUIRE(v["recording"]["title"].asString() == "Tatort: Murot");
	REQUIRE(v["recording"]["channel"].asString() == "ZDF HD");
	REQUIRE(v["position"].asInt() == 754);
	REQUIRE(v["duration"].asInt() == 1800);
	REQUIRE(v["paused"].asBool());
	REQUIRE(v["state"].asString() == "paused");
	REQUIRE(v["returns_to"]["id"].asString() == "2b66");
	REQUIRE_FALSE(v.isMember("channel"));
	REQUIRE_FALSE(v.isMember("file"));
	REQUIRE(askPlayback().body.find(shelf.base) == std::string::npos);

	playback::begin(started("/media/usb/Filme/Urlaub.mkv", "Urlaub"));
	playback::observeAt(at(5), 2000);
	channels.current_status = Status::NotFound;
	v = parsed(askPlayback().body);
	REQUIRE(v["source"].asString() == "file");
	REQUIRE(v["file"]["name"].asString() == "Urlaub.mkv");
	REQUIRE(v["file"]["title"].asString() == "Urlaub");
	REQUIRE_FALSE(v["paused"].asBool());
	REQUIRE(v["state"].asString() == "playing");
	REQUIRE_FALSE(v.isMember("returns_to"));
	REQUIRE(askPlayback().body.find("/media/usb") == std::string::npos);
}

TEST_CASE("the route and the event name every state alike", "[playback][routes]")
{
	Shelf shelf;
	FakeChannelSource channels;
	InstalledChannelSource in_channels(&channels);
	channels.mode = NeutrinoModes::mode_tv;
	const playback::State states[] = { playback::State::Playing, playback::State::Paused,
		playback::State::Forward, playback::State::Rewind };
	playback::begin(started("/media/usb/a.mkv"));
	for (size_t i = 0; i < sizeof(states) / sizeof(states[0]); ++i)
	{
		playback::observeAt(at(10, states[i]), 1000 + (int64_t) i);
		REQUIRE(parsed(askPlayback().body)["state"].asString() == playback::stateName(states[i]));
	}
	REQUIRE(parsed(askPlayback().body)["source"].asString() == playback::sourceName(playback::Source::File));
}

TEST_CASE("the current channel is no channel while a recording or a file plays", "[playback][routes]")
{
	Shelf shelf;
	FakeChannelSource channels;
	InstalledChannelSource in_channels(&channels);
	channels.mode = NeutrinoModes::mode_tv;
	channels.current = channel(0x2b66, "Das Erste HD");
	channels.current_status = Status::Ok;
	const std::string path = shelf.recording("Tatort", "Tatort");

	playback::begin(started(path));
	httpd::Response r = httpd::dispatch(httpd::Get, "/api/v1/channels/current", "", std::string(), "192.168.1.9",
	                                    httpd::AuthLevel::Read);
	REQUIRE(r.code == 404);
	const ::Json::Value v = parsed(r.body);
	REQUIRE(v["type"].asString() == "/errors/no-running-channel");
	REQUIRE(v["detail"].asString().find("GET /api/v1/playback") != std::string::npos);
	REQUIRE(channels::playing().error().code == ErrorCode::NoRunningChannel);

	playback::Started shift = started("/media/hdd/.timeshift/live.ts");
	shift.timeshift = true;
	playback::begin(shift);
	REQUIRE(channels::playing().ok());

	playback::end();
	REQUIRE(channels::playing().value().id == 0x2b66);
}

TEST_CASE("a change of winding speed is told and the route says the speed", "[playback][routes]")
{
	Shelf shelf;
	FakeChannelSource channels;
	InstalledChannelSource in_channels(&channels);
	channels.mode = NeutrinoModes::mode_tv;
	Collector c;
	playback::begin(started("/media/usb/a.mkv"));
	playback::observeAt(at(10), 1000);
	playback::observeAt(at(20, playback::State::Forward, 2), 2000);
	playback::observeAt(at(40, playback::State::Forward, 2), 3000);
	playback::observeAt(at(80, playback::State::Forward, 4), 4000);
	playback::observeAt(at(160, playback::State::Forward, 4), 5000);
	REQUIRE(c.seen.size() == 3);
	REQUIRE(c.seen[2].text == "forward file");
	REQUIRE(c.seen[2].value == 80);
	REQUIRE(parsed(askPlayback().body)["speed"].asInt() == 4);
}

TEST_CASE("a title and a channel are cut to what the archive answers", "[playback]")
{
	Shelf shelf;
	playback::Started s = started("/media/usb/a.mkv", std::string(archive::kMaxShortText + 40, 't'));
	s.channel = std::string(archive::kMaxShortText - 1, 'c') + "\xc3\xa4";
	playback::begin(s);
	REQUIRE(playback::snapshot().title.size() == archive::kMaxShortText);
	REQUIRE(playback::snapshot().channel == std::string(archive::kMaxShortText - 1, 'c'));
}
