/*
 * test_archive.cpp - tests for the archive listing
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
#include "support/answers.h"

#include "coreapi/archive.h"
#include "coreapi/base/deps.h"
#include "coreapi/base/errors.h"

#include "httpd/auth.h"
#include "httpd/credentials.h"
#include "httpd/endpoint.h"
#include "httpd/endpoints.h"
#include "httpd/http.h"
#include "httpd/router.h"

#include "jsoncpp/json/json.h"

#include <neutrinoMessages.h>
#include <OpenThreads/Block>
#include <OpenThreads/Thread>

#include <cstdio>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include <arpa/inet.h>
#include <fcntl.h>
#include <ifaddrs.h>
#include <net/if.h>
#include <openssl/sha.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>

using namespace coreapi;

namespace
{

// A record directory of its own, with real files, removed whole afterwards.
struct Disk
{
	std::string base;
	std::vector<std::string> made;
	std::vector<std::string> dirs;

	Disk()
	{
		char tmpl[] = "/tmp/coreapi_archive_XXXXXX";
		// Without it every file below would land under /.
		REQUIRE(mkdtemp(tmpl) != NULL);
		base = tmpl;
	}
	~Disk()
	{
		for (size_t i = made.size(); i > 0; --i)
			unlink(made[i - 1].c_str());
		for (size_t i = dirs.size(); i > 0; --i)
			rmdir(dirs[i - 1].c_str());
		rmdir(base.c_str());
	}
	std::string file(const std::string &rel, const std::string &body, time_t mtime = 0)
	{
		const std::string p = base + "/" + rel;
		FILE *f = std::fopen(p.c_str(), "wb");
		if (f == NULL)
			return std::string();
		std::fwrite(body.data(), 1, body.size(), f);
		std::fclose(f);
		if (mtime != 0)
		{
			struct timeval tv[2] = { { mtime, 0 }, { mtime, 0 } };
			utimes(p.c_str(), tv);
		}
		made.push_back(p);
		return p;
	}
	void dir(const std::string &rel)
	{
		const std::string p = base + "/" + rel;
		mkdir(p.c_str(), 0700);
		dirs.push_back(p);
	}
	void link(const std::string &rel, const std::string &to)
	{
		const std::string p = base + "/" + rel;
		if (symlink(to.c_str(), p.c_str()) == 0)
			made.push_back(p);
	}
	// A recording as the box writes one: the stream and its metadata.
	void recording(const std::string &stem, const std::string &title, const std::string &channel,
	               int minutes, time_t mtime, size_t bytes = 188)
	{
		file(stem + ".ts", std::string(bytes, 'G'), mtime);
		file(stem + ".xml",
		     "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<neutrino commandversion=\"1\">\n"
		     "\t<record command=\"record\">\n\t\t<channelname>" + channel + "</channelname>\n"
		     "\t\t<epgtitle>" + title + "</epgtitle>\n\t\t<id>12345678901</id>\n"
		     "\t\t<length>" + std::to_string(minutes) + "</length>\n\t</record>\n</neutrino>\n", mtime);
	}
};

struct Archive
{
	Disk                    disk;
	FakeSettingsSource      settings;
	InstalledSettingsSource in_settings;
	FakeRecordingSource     recordings;
	InstalledRecordingSource in_recordings;

	Archive() : in_settings(&settings), in_recordings(&recordings)
	{
		settings.strings["network_nfs_recordingdir"] = disk.base;
		archive::notePlaying(std::string());
	}
	~Archive() { archive::notePlaying(std::string()); }
};

} // namespace

TEST_CASE("the archive lists finished recordings newest first with their metadata", "[archive]")
{
	Archive a;
	a.disk.recording("Tatort_20261001", "Tatort: Murot", "Das Erste HD", 90, 1790000000);
	a.disk.recording("Heute_20261002", "heute journal", "ZDF HD", 30, 1790100000);
	a.disk.file("stray.ts", "no metadata", 1790200000);

	Result<archive::Page> got = archive::list("", 0, archive::kDefaultLimit);
	REQUIRE(got.ok());
	const archive::Page p = got.value();
	REQUIRE(p.total == 2);
	REQUIRE(p.items.size() == 2);
	REQUIRE(p.items[0].title == "heute journal");
	REQUIRE(p.items[0].channel == "ZDF HD");
	REQUIRE(p.items[0].duration == 30 * 60);
	REQUIRE(p.items[0].start == 1790100000 - 30 * 60);
	REQUIRE(p.items[0].size == 188);
	REQUIRE(p.items[0].channel_id == 12345678901ULL);
	REQUIRE(p.items[0].id.size() == 16);
	REQUIRE(p.items[0].id.find_first_not_of("0123456789abcdef") == std::string::npos);
	REQUIRE(p.items[1].title == "Tatort: Murot");
	REQUIRE_FALSE(p.items[0].playing);
}

TEST_CASE("an archive id is the same on every scan and differs per file", "[archive]")
{
	Archive a;
	a.disk.recording("a", "A", "X", 1, 1790000000);
	a.disk.recording("b", "B", "X", 1, 1790000001);
	const archive::Page one = archive::list("", 0, 10).value();
	const archive::Page two = archive::list("", 0, 10).value();
	REQUIRE(one.items[0].id == two.items[0].id);
	REQUIRE(one.items[0].id != one.items[1].id);
}

TEST_CASE("the archive pages and filters by words of the title", "[archive]")
{
	Archive a;
	for (int i = 0; i < 20; ++i)
		a.disk.recording("r" + std::to_string(i), i % 2 ? "Krimi " + std::to_string(i) : "Doku", "X", 1,
		                 1790000000 + i);
	const archive::Page first = archive::list("", 0, 15).value();
	REQUIRE(first.total == 20);
	REQUIRE(first.items.size() == 15);
	const archive::Page rest = archive::list("", 15, 15).value();
	REQUIRE(rest.items.size() == 5);
	const archive::Page krimi = archive::list("kRiMi", 0, 50).value();
	REQUIRE(krimi.total == 10);
	REQUIRE(archive::list("", 0, 0).value().items.size() == 0);
	REQUIRE(archive::list("", 0, 500).value().items.size() == 20);
}

namespace
{

// Three recordings whose order differs by every key; title and channel differ in case.
void threeOrders(Archive &a)
{
	a.disk.recording("a", "alpha", "Zdf", 30, 1790010000, 300);
	a.disk.recording("b", "Beta", "arte", 90, 1790020000, 100);
	a.disk.recording("c", "gamma", "Mdr", 60, 1790005000, 200);
}

std::string titlesOf(const archive::Page &p)
{
	std::string out;
	for (size_t i = 0; i < p.items.size(); ++i)
		out += (i ? " " : "") + p.items[i].title;
	return out;
}

std::string sortedBy(archive::SortKey key, bool descending)
{
	return titlesOf(archive::list("", 0, 10, archive::Sort(key, descending)).value());
}

} // namespace

TEST_CASE("the archive sorts by every key in both orders", "[archive]")
{
	Archive a;
	threeOrders(a);
	REQUIRE(sortedBy(archive::SortKey::Start, false) == "gamma alpha Beta");
	REQUIRE(sortedBy(archive::SortKey::Start, true) == "Beta alpha gamma");
	REQUIRE(sortedBy(archive::SortKey::Title, false) == "alpha Beta gamma");
	REQUIRE(sortedBy(archive::SortKey::Title, true) == "gamma Beta alpha");
	REQUIRE(sortedBy(archive::SortKey::Channel, false) == "Beta gamma alpha");
	REQUIRE(sortedBy(archive::SortKey::Channel, true) == "alpha gamma Beta");
	REQUIRE(sortedBy(archive::SortKey::Duration, false) == "alpha gamma Beta");
	REQUIRE(sortedBy(archive::SortKey::Duration, true) == "Beta gamma alpha");
	REQUIRE(sortedBy(archive::SortKey::Size, false) == "Beta gamma alpha");
	REQUIRE(sortedBy(archive::SortKey::Size, true) == "alpha gamma Beta");
}

TEST_CASE("the archive sorts newest first unless asked and each key has its own first order", "[archive]")
{
	Archive a;
	threeOrders(a);
	REQUIRE(titlesOf(archive::list("", 0, 10).value()) == "Beta alpha gamma");
	REQUIRE(archive::descendingByDefault(archive::SortKey::Start));
	REQUIRE(archive::descendingByDefault(archive::SortKey::Duration));
	REQUIRE(archive::descendingByDefault(archive::SortKey::Size));
	REQUIRE_FALSE(archive::descendingByDefault(archive::SortKey::Title));
	REQUIRE_FALSE(archive::descendingByDefault(archive::SortKey::Channel));
}

TEST_CASE("equal keys fall back to the id so pages neither overlap nor skip", "[archive]")
{
	Archive a;
	for (int i = 0; i < 7; ++i)
		a.disk.recording("same" + std::to_string(i), "Same", "X", 10, 1790000000, 188);
	const archive::SortKey keys[] = { archive::SortKey::Start, archive::SortKey::Title, archive::SortKey::Channel,
	                                  archive::SortKey::Duration, archive::SortKey::Size };
	for (size_t k = 0; k < sizeof(keys) / sizeof(keys[0]); ++k)
	{
		for (int d = 0; d < 2; ++d)
		{
			const archive::Sort by(keys[k], d == 1);
			std::vector<std::string> ids;
			for (size_t offset = 0; offset < 7; offset += 3)
			{
				const archive::Page p = archive::list("", offset, 3, by).value();
				for (size_t i = 0; i < p.items.size(); ++i)
					ids.push_back(p.items[i].id);
			}
			INFO("key " << k << " descending " << d);
			REQUIRE(ids.size() == 7);
			for (size_t i = 1; i < ids.size(); ++i)
				REQUIRE(ids[i - 1] < ids[i]);
		}
	}
}

TEST_CASE("sorting follows the title filter and comes before the offset", "[archive]")
{
	Archive a;
	a.disk.recording("k1", "Krimi delta", "X", 10, 1790000001);
	a.disk.recording("d1", "Doku alpha", "X", 10, 1790000002);
	a.disk.recording("k2", "krimi Alpha", "X", 10, 1790000003);
	a.disk.recording("k3", "KRIMI charlie", "X", 10, 1790000004);
	a.disk.recording("k4", "Krimi bravo", "X", 10, 1790000005);
	const archive::Sort by(archive::SortKey::Title, false);
	const archive::Page p = archive::list("krimi", 1, 2, by).value();
	REQUIRE(p.total == 4);
	REQUIRE(titlesOf(p) == "Krimi bravo KRIMI charlie");
	REQUIRE(titlesOf(archive::list("krimi", 3, 2, by).value()) == "Krimi delta");
}

TEST_CASE("the archive looks one folder deep and follows no link", "[archive]")
{
	Archive a;
	a.disk.dir("Das Erste HD");
	a.disk.recording("Das Erste HD/Tatort", "Tatort", "Das Erste HD", 90, 1790000000);
	a.disk.dir("Das Erste HD/deeper");
	a.disk.recording("Das Erste HD/deeper/hidden", "Hidden", "X", 1, 1790000001);
	a.disk.file("evil.xml", "<epgtitle>evil</epgtitle>");
	a.disk.link("evil.ts", "/etc/passwd");
	a.disk.link("out", "/");
	const archive::Page p = archive::list("", 0, 50).value();
	REQUIRE(p.total == 1);
	REQUIRE(p.items[0].title == "Tatort");
}

TEST_CASE("the timeshift buffer under its dot folder is never listed", "[archive]")
{
	Archive a;
	a.disk.recording("Tatort", "Tatort", "Das Erste HD", 90, 1790000000);
	a.disk.dir(".timeshift");
	a.disk.recording(".timeshift/live", "Live", "X", 1, 1790000001);
	const archive::Page p = archive::list("", 0, 50).value();
	REQUIRE(p.total == 1);
	REQUIRE(p.items[0].title == "Tatort");
}

TEST_CASE("metadata is read as text the box may not have written", "[archive]")
{
	Archive a;
	a.disk.file("empty.ts", "x", 1790000000);
	a.disk.file("empty.xml", "", 1790000000);
	a.disk.file("ent.ts", "x", 1790000001);
	a.disk.file("ent.xml", "<epgtitle>Tom &amp; Jerry &#228; &#x263A; &lt;b&gt;</epgtitle><length>x</length>",
	            1790000001);
	a.disk.file("big.ts", "x", 1790000002);
	a.disk.file("big.xml", std::string(3 * 1024 * 1024, 'a'), 1790000002);
	const archive::Page p = archive::list("", 0, 50).value();
	REQUIRE(p.total == 3);
	REQUIRE(p.items[0].title.empty());
	REQUIRE(p.items[1].title == "Tom & Jerry \xc3\xa4 \xe2\x98\xba <b>");
	REQUIRE(p.items[1].duration == 0);
	REQUIRE(p.items[1].start == 1790000001);
	REQUIRE(p.items[2].title.empty());
}

TEST_CASE("a list entry's title and channel are cut to the same bound as every other text", "[archive]")
{
	Archive a;
	a.disk.file("one.ts", "x", 1790000000);
	const std::string title(archive::kMaxShortText + 2000, 'a');
	const std::string channel(archive::kMaxShortText + 200, 'c');
	a.disk.file("one.xml", "<epgtitle>" + title + "</epgtitle><channelname>" + channel + "</channelname>",
	            1790000000);
	const archive::Entry e = archive::list("", 0, 10).value().items[0];
	REQUIRE(e.title.size() == archive::kMaxShortText);
	REQUIRE(e.channel.size() == archive::kMaxShortText);
	REQUIRE(e.title == title.substr(0, archive::kMaxShortText));
	REQUIRE(e.channel == channel.substr(0, archive::kMaxShortText));
}

TEST_CASE("a length past a sane bound is unknown rather than a number that overflows its multiply", "[archive]")
{
	Archive a;
	a.disk.file("one.ts", "x", 1790000000);
	a.disk.file("one.xml", "<epgtitle>One</epgtitle><length>153722867280912931</length>", 1790000000);
	const archive::Entry e = archive::list("", 0, 10).value().items[0];
	REQUIRE(e.duration == 0);
	REQUIRE(e.start == 1790000000);

	Archive b;
	b.disk.file("two.ts", "x", 1790000000);
	b.disk.file("two.xml", "<epgtitle>Two</epgtitle><length>" +
	            std::to_string(archive::kMaxDurationMinutes) + "</length>", 1790000000);
	const archive::Entry f = archive::list("", 0, 10).value().items[0];
	REQUIRE(f.duration == (long) (archive::kMaxDurationMinutes * 60));

	Archive c;
	c.disk.file("three.ts", "x", 1790000000);
	c.disk.file("three.xml", "<epgtitle>Three</epgtitle><length>" +
	            std::to_string(archive::kMaxDurationMinutes + 1) + "</length>", 1790000000);
	const archive::Entry g = archive::list("", 0, 10).value().items[0];
	REQUIRE(g.duration == 0);
}

TEST_CASE("a cover with a second link beside it is never served", "[archive]")
{
	Archive a;
	a.disk.recording("Tatort", "Tatort", "Das Erste HD", 1, 1790000000);
	const std::string ts = a.disk.base + "/Tatort.ts";
	const std::string cover = a.disk.base + "/Tatort.jpg";
	REQUIRE(::link(ts.c_str(), cover.c_str()) == 0);
	a.disk.made.push_back(cover);

	const std::string id = archive::list("", 0, 10).value().items[0].id;
	REQUIRE_FALSE(archive::details(id).value().cover);
	REQUIRE_FALSE(archive::coverPath(id).ok());
}

TEST_CASE("an entity with no valid code point is kept rather than turned into a NUL or bad UTF-8", "[archive]")
{
	Archive a;
	a.disk.file("ent.ts", "x", 1790000000);
	a.disk.file("ent.xml",
	            "<epgtitle>&#0;&#x;&#xD800;&#x110000;ok</epgtitle><length>x</length>",
	            1790000000);
	const archive::Page p = archive::list("", 0, 10).value();
	REQUIRE(p.total == 1);
	REQUIRE(p.items[0].title == "&#0;&#x;&#xD800;&#x110000;ok");
}

TEST_CASE("a box with no record directory has an empty archive", "[archive]")
{
	Archive a;
	a.settings.strings["network_nfs_recordingdir"] = "";
	Result<archive::Page> got = archive::list("", 0, 10);
	REQUIRE(got.ok());
	REQUIRE(got.value().total == 0);
	a.settings.strings["network_nfs_recordingdir"] = "relative/dir";
	REQUIRE(archive::list("", 0, 10).value().total == 0);
}

TEST_CASE("the archive says which recording is playing", "[archive]")
{
	Archive a;
	a.disk.recording("one", "One", "X", 1, 1790000000);
	archive::notePlaying(a.disk.base + "/one.ts");
	REQUIRE(archive::list("", 0, 10).value().items[0].playing);
	REQUIRE(archive::playingPath() == a.disk.base + "/one.ts");
}

TEST_CASE("an archive id names one recording and nothing else", "[archive]")
{
	Archive a;
	a.disk.recording("one", "One", "X", 1, 1790000000);
	const std::string id = archive::list("", 0, 1).value().items[0].id;
	Result<archive::Entry> got = archive::find(id);
	REQUIRE(got.ok());
	REQUIRE(got.value().path == a.disk.base + "/one.ts");
	REQUIRE(archive::find("0000000000000000").error().code == ErrorCode::NoSuchRecording);
	REQUIRE(archive::find("../../etc/passwd").error().code == ErrorCode::NoSuchRecording);
	REQUIRE(archive::find(id.substr(0, 15)).error().code == ErrorCode::NoSuchRecording);
	REQUIRE(archive::find("ABCDEF0123456789").error().code == ErrorCode::NoSuchRecording);
}

TEST_CASE("removing a recording takes its metadata and cover with it", "[archive]")
{
	Archive a;
	a.disk.recording("one", "One", "X", 1, 1790000000);
	a.disk.file("one.jpg", "cover");
	a.disk.file("one.gif", "cover");
	a.disk.file("one.jpeg", "cover");
	a.disk.file("one.bmp", "cover");
	a.disk.recording("two", "Two", "X", 1, 1790000001);
	const std::string id = archive::find(archive::list("One", 0, 1).value().items[0].id).value().id;
	REQUIRE(archive::remove(id).ok());
	struct stat st;
	REQUIRE(lstat((a.disk.base + "/one.ts").c_str(), &st) != 0);
	REQUIRE(lstat((a.disk.base + "/one.xml").c_str(), &st) != 0);
	REQUIRE(lstat((a.disk.base + "/one.jpg").c_str(), &st) != 0);
	REQUIRE(lstat((a.disk.base + "/one.gif").c_str(), &st) != 0);
	REQUIRE(lstat((a.disk.base + "/one.jpeg").c_str(), &st) != 0);
	REQUIRE(lstat((a.disk.base + "/one.bmp").c_str(), &st) != 0);
	REQUIRE(lstat((a.disk.base + "/two.ts").c_str(), &st) == 0);
	REQUIRE(archive::remove(id).error().code == ErrorCode::NoSuchRecording);
}

TEST_CASE("a recording being written or played is not removed", "[archive]")
{
	Archive a;
	a.disk.recording("live", "Live", "X", 1, 1790000000);
	a.disk.recording("shown", "Shown", "X", 1, 1790000001);
	RecordingInfo r;
	r.id = 7;
	r.path = a.disk.base + "/live.ts";
	a.recordings.recordings.push_back(r);
	archive::notePlaying(a.disk.base + "/shown.ts");

	const archive::Page p = archive::list("", 0, 10).value();
	const std::string shown = p.items[0].id;
	const std::string live = p.items[1].id;
	REQUIRE(archive::remove(live).error().code == ErrorCode::RecordingRunning);
	REQUIRE(archive::remove(shown).error().code == ErrorCode::RecordingPlaying);
	struct stat st;
	REQUIRE(lstat((a.disk.base + "/live.ts").c_str(), &st) == 0);
	REQUIRE(lstat((a.disk.base + "/shown.ts").c_str(), &st) == 0);

	a.recordings.list_status = Status::Internal;
	REQUIRE_FALSE(archive::remove(shown).ok());
}

TEST_CASE("removing never reaches out of the record directory", "[archive]")
{
	Archive a;
	Disk outside;
	outside.recording("victim", "Victim", "X", 1, 1790000000);
	a.disk.file("evil.xml", "<epgtitle>evil</epgtitle>");
	a.disk.link("evil.ts", outside.base + "/victim.ts");
	REQUIRE(archive::list("", 0, 10).value().total == 0);

	unsigned char d[SHA256_DIGEST_LENGTH];
	SHA256((const unsigned char *) "evil.ts", 7, d);
	static const char kHex[] = "0123456789abcdef";
	std::string id;
	for (size_t i = 0; i < 8; ++i)
	{
		id += kHex[d[i] >> 4];
		id += kHex[d[i] & 15];
	}
	REQUIRE(archive::find(id).error().code == ErrorCode::NoSuchRecording);
	REQUIRE(archive::remove(id).error().code == ErrorCode::NoSuchRecording);

	struct stat st;
	REQUIRE(lstat((outside.base + "/victim.ts").c_str(), &st) == 0);
}

TEST_CASE("a running or playing recording is matched through a symlinked record directory", "[archive]")
{
	Archive a;
	Disk real;
	real.recording("live", "Live", "X", 1, 1790000000);
	real.recording("shown", "Shown", "X", 1, 1790000001);
	a.disk.link("real", real.base);
	const std::string link = a.disk.base + "/real";
	a.settings.strings["network_nfs_recordingdir"] = link;

	RecordingInfo r;
	r.id = 7;
	r.path = link + "/live.ts";
	a.recordings.recordings.push_back(r);
	archive::notePlaying(link + "/shown.ts");

	const archive::Page p = archive::list("", 0, 10).value();
	REQUIRE(p.items.size() == 2);
	const std::string live = p.items[0].title == "Live" ? p.items[0].id : p.items[1].id;
	const std::string shown = p.items[0].title == "Shown" ? p.items[0].id : p.items[1].id;
	for (size_t i = 0; i < p.items.size(); ++i)
	{
		if (p.items[i].title == "Shown")
			REQUIRE(p.items[i].playing);
		else
			REQUIRE_FALSE(p.items[i].playing);
	}

	REQUIRE(archive::remove(live).error().code == ErrorCode::RecordingRunning);
	REQUIRE(archive::remove(shown).error().code == ErrorCode::RecordingPlaying);
	struct stat st;
	REQUIRE(lstat((real.base + "/live.ts").c_str(), &st) == 0);
	REQUIRE(lstat((real.base + "/shown.ts").c_str(), &st) == 0);
}

namespace
{

// A loop whose player never comes back: the post hands the message over and returns.
// The player blocks until the case lets it go, so the case can join it before it ends.
struct NeverReturns : public OpenThreads::Thread
{
	OpenThreads::Block *held;
	void run() { held->block(); }
};

struct LoopThatNeverReturns : public CommandSink
{
	// Declared before the thread that holds it, so it is never destroyed first.
	OpenThreads::Block held;
	NeverReturns player;
	std::vector<std::string> paths;
	LoopThatNeverReturns() { player.held = &held; }
	// Also run on a failed REQUIRE, which unwinds past an explicit letGo().
	~LoopThatNeverReturns() { letGo(); }
	Status post(neutrino_msg_t msg, neutrino_msg_data_t data)
	{
		if (msg == NeutrinoMessages::EVT_PLAY_RECORDING)
			paths.push_back(archive::playAsked((const char *) data).path);
		delete[] (unsigned char *) data;
		if (!player.isRunning())
			player.start();
		return Status::Ok;
	}
	void letGo()
	{
		if (player.isRunning())
		{
			held.release();
			player.join();
		}
	}
};

} // namespace

TEST_CASE("playing a recording hands its path to the loop and returns at once", "[archive]")
{
	Archive a;
	a.disk.recording("one", "One", "X", 1, 1790000000);
	FakeChannelSource channels;
	InstalledChannelSource in_channels(&channels);
	LoopThatNeverReturns loop;
	InstalledSink in_sink(&loop);

	const std::string id = archive::list("", 0, 1).value().items[0].id;
	struct timeval before, after;
	gettimeofday(&before, NULL);
	REQUIRE(archive::play(id, false, false).ok());
	gettimeofday(&after, NULL);
	REQUIRE(after.tv_sec - before.tv_sec < 2);
	REQUIRE(loop.paths.size() == 1);
	REQUIRE(loop.paths[0] == a.disk.base + "/one.ts");
	REQUIRE(loop.player.isRunning());
	loop.letGo();
}

TEST_CASE("playing is refused while something plays or the box sleeps", "[archive]")
{
	Archive a;
	a.disk.recording("one", "One", "X", 1, 1790000000);
	FakeChannelSource channels;
	InstalledChannelSource in_channels(&channels);
	FakeCommandSink sink;
	InstalledSink in_sink(&sink);
	const std::string id = archive::list("", 0, 1).value().items[0].id;

	archive::notePlaying("/somewhere/else.ts");
	REQUIRE(archive::play(id, false, false).error().code == ErrorCode::PlaybackRunning);
	REQUIRE(archive::play(id, true, false).error().code == ErrorCode::PlaybackRunning);
	REQUIRE(archive::play(id, false, false).error().message == std::string("something is playing in the movie player"));
	// A timeshift is not ended for a play, asked or not.
	archive::notePlaying("/somewhere/live_temp.ts", true);
	REQUIRE(archive::play(id, false, false).error().code == ErrorCode::RecordingPlaying);
	REQUIRE(archive::play(id, true, true).error().code == ErrorCode::RecordingPlaying);
	REQUIRE(archive::play(id, false, false).error().message == std::string("the box is playing back its timeshift"));
	archive::notePlaying(std::string());
	REQUIRE(sink.posted.empty());

	channels.mode = NeutrinoModes::mode_standby;
	REQUIRE(archive::play(id, false, false).error().code == ErrorCode::BoxInStandby);
	REQUIRE(sink.posted.empty());

	channels.mode_status = Status::Internal;
	REQUIRE(archive::play(id, true, false).error().code == ErrorCode::ModeUnavailable);
	REQUIRE(sink.posted.empty());

	REQUIRE(archive::play("0000000000000000", true, false).error().code == ErrorCode::NoSuchRecording);

	for (size_t i = 0; i < sink.posted.size(); ++i)
		delete[] (unsigned char *) sink.posted[i].second;
}

TEST_CASE("a sleeping box plays a recording only when asked to wake", "[archive]")
{
	Archive a;
	a.disk.recording("one", "One", "X", 1, 1790000000);
	FakeChannelSource channels;
	InstalledChannelSource in_channels(&channels);
	FakeCommandSink sink;
	InstalledSink in_sink(&sink);
	const std::string id = archive::list("", 0, 1).value().items[0].id;
	const std::string path = a.disk.base + "/one.ts";

	channels.mode = NeutrinoModes::mode_standby;
	REQUIRE(archive::play(id, false, false).error().code == ErrorCode::BoxInStandby);
	REQUIRE(sink.posted.empty());
	// One message: the loop wakes the box on it before it plays, as it does for a zap.
	REQUIRE(archive::play(id, true, false).ok());
	REQUIRE(sink.posted.size() == 1);
	REQUIRE(sink.posted[0].first == NeutrinoMessages::EVT_PLAY_RECORDING);
	REQUIRE(archive::playAsked((const char *) sink.posted[0].second).path == path);

	channels.mode = NeutrinoModes::mode_tv;
	REQUIRE(archive::play(id, false, false).ok());
	REQUIRE(archive::play(id, true, false).ok());
	REQUIRE(sink.posted.size() == 3);
	REQUIRE(sink.posted[1].first == NeutrinoMessages::EVT_PLAY_RECORDING);
	REQUIRE(sink.posted[2].first == NeutrinoMessages::EVT_PLAY_RECORDING);
	REQUIRE(archive::playAsked((const char *) sink.posted[2].second).path == path);

	for (size_t i = 0; i < sink.posted.size(); ++i)
		delete[] (unsigned char *) sink.posted[i].second;
}

TEST_CASE("a box still starting refuses a play and posts nothing", "[archive]")
{
	Archive a;
	a.disk.recording("one", "One", "X", 1, 1790000000);
	FakeChannelSource channels;
	channels.mode_status = Status::NotFound;
	InstalledChannelSource in_channels(&channels);
	FakeCommandSink sink;
	InstalledSink in_sink(&sink);
	const std::string id = archive::list("", 0, 1).value().items[0].id;

	// The player draws on screens the box has not built until it settles on a mode.
	const bool leave[] = { false, true };
	for (size_t i = 0; i < 2; ++i)
	{
		Result<void> r = archive::play(id, leave[i], leave[i]);
		REQUIRE_FALSE(r.ok());
		REQUIRE(r.error().status == Status::Conflict);
		REQUIRE(r.error().code == ErrorCode::ModeUnavailable);
		REQUIRE(r.error().message == std::string("the box has not finished starting"));
	}
	REQUIRE(sink.posted.empty());

	const std::string play = "/api/v1/recordings/archive/" + id + "/play";
	httpd::Response early = httpd::dispatch(httpd::Post, play, "", "{}", "192.168.1.9", httpd::AuthLevel::Write);
	REQUIRE(early.code == 409);
	REQUIRE(early.body.find("/errors/mode-unavailable") != std::string::npos);
	REQUIRE(sink.posted.empty());
}

TEST_CASE("a file playing is ended for a play only when asked", "[archive]")
{
	Archive a;
	a.disk.recording("one", "One", "X", 1, 1790000000);
	FakeChannelSource channels;
	InstalledChannelSource in_channels(&channels);
	FakeCommandSink sink;
	InstalledSink in_sink(&sink);
	const std::string id = archive::list("", 0, 1).value().items[0].id;
	const std::string path = a.disk.base + "/one.ts";

	// The same recording as well as any other file.
	const std::string playing[] = { "/somewhere/else.ts", path };
	for (size_t i = 0; i < 2; ++i)
	{
		archive::notePlaying(playing[i]);
		REQUIRE(archive::play(id, false, false).error().code == ErrorCode::PlaybackRunning);
		REQUIRE(sink.posted.empty());
	}
	// One message: the player ends on it and hands it to the loop, which plays it.
	REQUIRE(archive::play(id, false, true).ok());
	REQUIRE(sink.posted.size() == 1);
	REQUIRE(sink.posted[0].first == NeutrinoMessages::EVT_PLAY_RECORDING);
	REQUIRE(archive::playAsked((const char *) sink.posted[0].second).path == path);
	REQUIRE(archive::playAsked((const char *) sink.posted[0].second).stop_playback);

	// Ahead of standby: a no to ending the playback asks nothing about the sleep.
	channels.mode = NeutrinoModes::mode_standby;
	REQUIRE(archive::play(id, false, false).error().code == ErrorCode::PlaybackRunning);
	REQUIRE(archive::play(id, false, true).error().code == ErrorCode::BoxInStandby);
	REQUIRE(archive::play(id, true, true).ok());
	REQUIRE(sink.posted.size() == 2);

	// Nothing playing, the leave changes nothing.
	archive::notePlaying(std::string());
	channels.mode = NeutrinoModes::mode_tv;
	REQUIRE(archive::play(id, false, true).ok());
	REQUIRE(sink.posted.size() == 3);

	// Without leave the message says so, and a file that starts before the loop takes it plays on.
	REQUIRE(archive::play(id, false, false).ok());
	REQUIRE(sink.posted.size() == 4);
	REQUIRE(archive::playAsked((const char *) sink.posted[3].second).path == path);
	REQUIRE_FALSE(archive::playAsked((const char *) sink.posted[3].second).stop_playback);

	for (size_t i = 0; i < sink.posted.size(); ++i)
		delete[] (unsigned char *) sink.posted[i].second;
}

namespace
{

::Json::Value parsed(const std::string &text)
{
	::Json::Value v;
	::Json::CharReaderBuilder b;
	std::string errs;
	std::unique_ptr< ::Json::CharReader> r(b.newCharReader());
	r->parse(text.data(), text.data() + text.size(), &v, &errs);
	return v;
}

httpd::Response ask(httpd::Method m, const std::string &path, const std::string &query,
                    httpd::AuthLevel level = httpd::AuthLevel::Write)
{
	return httpd::dispatch(m, path, query, std::string(), "192.168.1.9", level);
}

} // namespace

TEST_CASE("the archive route answers a page newest first", "[archive][routes]")
{
	Archive a;
	for (int i = 0; i < 17; ++i)
		a.disk.recording("r" + std::to_string(i), "Film " + std::to_string(i), "ZDF", 90, 1790000000 + i);
	httpd::Response r = ask(httpd::Get, "/api/v1/recordings/archive", "", httpd::AuthLevel::Read);
	REQUIRE(r.code == 200);
	const ::Json::Value v = parsed(r.body);
	REQUIRE(v["total"].asUInt() == 17);
	REQUIRE(v["items"].size() == 15);
	REQUIRE(v["next_offset"].asUInt() == 15);
	REQUIRE(v["items"][0]["title"].asString() == "Film 16");
	REQUIRE_FALSE(v["items"][0].isMember("path"));
	httpd::Response last = ask(httpd::Get, "/api/v1/recordings/archive", "offset=15", httpd::AuthLevel::Read);
	REQUIRE_FALSE(parsed(last.body).isMember("next_offset"));
}

TEST_CASE("the archive route sorts by a named key and refuses one it does not know", "[archive][routes]")
{
	Archive a;
	threeOrders(a);
	const std::string path = "/api/v1/recordings/archive";
	httpd::Response r = ask(httpd::Get, path, "sort=title", httpd::AuthLevel::Read);
	REQUIRE(r.code == 200);
	::Json::Value v = parsed(r.body);
	REQUIRE(v["items"][0]["title"].asString() == "alpha");
	REQUIRE(v["items"][2]["title"].asString() == "gamma");
	v = parsed(ask(httpd::Get, path, "sort=channel&order=desc", httpd::AuthLevel::Read).body);
	REQUIRE(v["items"][0]["title"].asString() == "alpha");
	v = parsed(ask(httpd::Get, path, "sort=size", httpd::AuthLevel::Read).body);
	REQUIRE(v["items"][0]["title"].asString() == "alpha");
	v = parsed(ask(httpd::Get, path, "sort=duration&order=asc&offset=1&limit=1", httpd::AuthLevel::Read).body);
	REQUIRE(v["items"].size() == 1);
	REQUIRE(v["items"][0]["title"].asString() == "gamma");
	REQUIRE(v["next_offset"].asUInt() == 2);
	v = parsed(ask(httpd::Get, path, "order=asc", httpd::AuthLevel::Read).body);
	REQUIRE(v["items"][0]["title"].asString() == "gamma");

	httpd::Response bad = ask(httpd::Get, path, "sort=rating", httpd::AuthLevel::Read);
	REQUIRE(bad.code == 400);
	REQUIRE(bad.body.find("sort") != std::string::npos);
	REQUIRE(ask(httpd::Get, path, "order=up", httpd::AuthLevel::Read).code == 400);
	REQUIRE(ask(httpd::Get, path, "sort=", httpd::AuthLevel::Read).code == 400);
	REQUIRE(ask(httpd::Get, path, "order=", httpd::AuthLevel::Read).code == 400);
}

TEST_CASE("the first archive page fits in eight kilobytes", "[archive][routes]")
{
	Archive a;
	for (int i = 0; i < 20; ++i)
		a.disk.recording("Ein_sehr_langer_Dateiname_einer_Aufnahme_mit_Sender_und_Datum_" + std::to_string(i),
		                 std::string(80, 'T'), std::string(40, 'C'), 120, 1790000000 + i, 4096);
	httpd::Response r = ask(httpd::Get, "/api/v1/recordings/archive", "", httpd::AuthLevel::Read);
	REQUIRE(r.code == 200);
	REQUIRE(r.body.size() <= 8192);
}

TEST_CASE("the archive play and delete routes answer like the module", "[archive][routes]")
{
	Archive a;
	a.disk.recording("one", "One", "X", 1, 1790000000);
	FakeChannelSource channels;
	InstalledChannelSource in_channels(&channels);
	FakeCommandSink sink;
	InstalledSink in_sink(&sink);
	const std::string id = archive::list("", 0, 1).value().items[0].id;

	REQUIRE(ask(httpd::Post, "/api/v1/recordings/archive/" + id + "/play", "").code == 202);
	std::map<std::string, std::string> fills;
	fills["id"] = id;
	REQUIRE(sendBodyExample("POST", "/api/v1/recordings/archive/{id}/play", fills) == 202);
	REQUIRE(ask(httpd::Post, "/api/v1/recordings/archive/0000000000000000/play", "").code == 404);
	channels.mode = NeutrinoModes::mode_standby;
	REQUIRE(ask(httpd::Post, "/api/v1/recordings/archive/" + id + "/play", "").code == 409);
	const std::string play = "/api/v1/recordings/archive/" + id + "/play";
	httpd::Response asleep = httpd::dispatch(httpd::Post, play, "", "{\"wake\":false}", "192.168.1.9",
	                                         httpd::AuthLevel::Write);
	REQUIRE(asleep.code == 409);
	REQUIRE(asleep.body.find("box-in-standby") != std::string::npos);
	const size_t before = sink.posted.size();
	REQUIRE(httpd::dispatch(httpd::Post, play, "", "{\"wake\":true}", "192.168.1.9",
	                        httpd::AuthLevel::Write).code == 202);
	REQUIRE(sink.posted.size() == before + 1);
	channels.mode = 0;
	archive::notePlaying(a.disk.base + "/one.ts");
	httpd::Response busy = httpd::dispatch(httpd::Post, play, "", "{\"stop_playback\":false}", "192.168.1.9",
	                                       httpd::AuthLevel::Write);
	REQUIRE(busy.code == 409);
	REQUIRE(busy.body.find("/errors/playback-running") != std::string::npos);
	REQUIRE(ask(httpd::Post, "/api/v1/recordings/archive/" + id + "/play", "").code == 409);
	const size_t ended = sink.posted.size();
	REQUIRE(httpd::dispatch(httpd::Post, play, "", "{\"stop_playback\":true}", "192.168.1.9",
	                        httpd::AuthLevel::Write).code == 202);
	REQUIRE(sink.posted.size() == ended + 1);
	REQUIRE(ask(httpd::Delete, "/api/v1/recordings/archive/" + id, "").code == 409);
	archive::notePlaying(a.disk.base + "/live_temp.ts", true);
	httpd::Response shift = httpd::dispatch(httpd::Post, play, "", "{\"stop_playback\":true}", "192.168.1.9",
	                                        httpd::AuthLevel::Write);
	REQUIRE(shift.code == 409);
	REQUIRE(shift.body.find("/errors/recording-playing") != std::string::npos);
	archive::notePlaying(std::string());
	REQUIRE(ask(httpd::Delete, "/api/v1/recordings/archive/" + id, "", httpd::AuthLevel::Read).code == 403);
	REQUIRE(ask(httpd::Delete, "/api/v1/recordings/archive/" + id, "").code == 204);
	REQUIRE(ask(httpd::Delete, "/api/v1/recordings/archive/" + id, "").code == 404);
	for (size_t i = 0; i < sink.posted.size(); ++i)
		delete[] (unsigned char *) sink.posted[i].second;
}

TEST_CASE("a running recording keeps its archive file", "[archive][routes]")
{
	Archive a;
	a.disk.recording("live", "Live", "X", 1, 1790000000);
	RecordingInfo r;
	r.id = 3;
	r.path = a.disk.base + "/live.ts";
	a.recordings.recordings.push_back(r);
	const std::string id = archive::list("", 0, 1).value().items[0].id;
	httpd::Response got = ask(httpd::Delete, "/api/v1/recordings/archive/" + id, "");
	REQUIRE(got.code == 409);
	REQUIRE(got.body.find("recording-running") != std::string::npos);
}

TEST_CASE("the archive file is answered from the disk and the playlist names it with the same token",
         "[archive][routes]")
{
	Archive a;
	a.disk.recording("one", "Tatort: Murot\nund das Paradies", "X", 90, 1790000000, 1880);
	const std::string id = archive::list("", 0, 1).value().items[0].id;
	const std::string token = httpd::openScopedToken(httpd::mediaScopeName(), httpd::AuthLevel::System);
	REQUIRE_FALSE(token.empty());
	const std::string q = std::string(httpd::queryTokenName()) + "=" + token;

	httpd::Response f = httpd::dispatch(httpd::Get, "/api/v1/recordings/archive/" + id + "/file", q,
	                                    std::string(), "192.168.1.9", httpd::AuthLevel::System, std::string(),
	                                    std::string(), "box:8081", httpd::mediaScopeName());
	REQUIRE(f.code == 200);
	REQUIRE(f.fd >= 0);
	REQUIRE(f.content_type == "video/mp2t");
	::close(f.fd);

	httpd::Response p = httpd::dispatch(httpd::Get, "/api/v1/recordings/archive/" + id + "/playlist.m3u", q,
	                                    std::string(), "192.168.1.9", httpd::AuthLevel::System, std::string(),
	                                    std::string(), "localhost:8081", httpd::mediaScopeName());
	REQUIRE(p.code == 200);
	REQUIRE(p.content_type == "audio/x-mpegurl");
	REQUIRE(p.body == "#EXTM3U\n#EXTINF:5400,Tatort: Murot und das Paradies\n"
	                  "http://localhost:8081/api/v1/recordings/archive/" + id + "/file?" + q + "\n");
}

TEST_CASE("a reader on the home network gets the file and a playlist without a token", "[archive][routes]")
{
	Archive a;
	a.disk.recording("one", "One", "X", 90, 1790000000);
	const std::string id = archive::list("", 0, 1).value().items[0].id;

	httpd::Response f = httpd::dispatch(httpd::Get, "/api/v1/recordings/archive/" + id + "/file", "",
	                                    std::string(), "192.168.1.9", httpd::AuthLevel::Read, std::string(),
	                                    std::string(), "box:8081");
	REQUIRE(f.code == 200);
	REQUIRE(f.fd >= 0);
	::close(f.fd);

	httpd::Response p = httpd::dispatch(httpd::Get, "/api/v1/recordings/archive/" + id + "/playlist.m3u", "",
	                                    std::string(), "192.168.1.9", httpd::AuthLevel::Read, std::string(),
	                                    std::string(), "localhost:8081");
	REQUIRE(p.code == 200);
	REQUIRE(p.body == "#EXTM3U\n#EXTINF:5400,One\n"
	                  "http://localhost:8081/api/v1/recordings/archive/" + id + "/file\n");
}

TEST_CASE("the archive refuses a foreign scope and answers an unknown id as not found", "[archive][routes]")
{
	Archive a;
	a.disk.recording("one", "One", "X", 1, 1790000000);
	const std::string id = archive::list("", 0, 1).value().items[0].id;

	httpd::Response f = httpd::dispatch(httpd::Get, "/api/v1/recordings/archive/" + id + "/file", "",
	                                    std::string(), "192.168.1.9", httpd::AuthLevel::Read, std::string(),
	                                    std::string(), "box:8081", "someother");
	REQUIRE(f.code == 403);
	REQUIRE(httpd::dispatch(httpd::Get, "/api/v1/recordings/archive/0000000000000000/file", "", std::string(),
	                        "192.168.1.9", httpd::AuthLevel::Read).code == 404);

	const std::string token = httpd::openScopedToken(httpd::mediaScopeName(), httpd::AuthLevel::System);
	const std::string q = std::string(httpd::queryTokenName()) + "=" + token;
	httpd::Response pf = httpd::dispatch(httpd::Get, "/api/v1/recordings/archive/0000000000000000/playlist.m3u",
	                                     q, std::string(), "192.168.1.9", httpd::AuthLevel::System,
	                                     std::string(), std::string(), "localhost:8081", httpd::mediaScopeName());
	REQUIRE(pf.code == 404);

	httpd::Response pforeign = httpd::dispatch(httpd::Get, "/api/v1/recordings/archive/" + id + "/playlist.m3u",
	                                           q, std::string(), "192.168.1.9", httpd::AuthLevel::Read,
	                                           std::string(), std::string(), "box:8081", "someother");
	REQUIRE(pforeign.code == 403);
	REQUIRE(pforeign.body.find(token) == std::string::npos);
}

TEST_CASE("a host that is no plain authority writes no playlist", "[archive][routes]")
{
	Archive a;
	a.disk.recording("one", "One", "X", 1, 1790000000);
	const std::string id = archive::list("", 0, 1).value().items[0].id;
	const std::string token = httpd::openScopedToken(httpd::mediaScopeName(), httpd::AuthLevel::System);
	const std::string q = std::string(httpd::queryTokenName()) + "=" + token;
	httpd::Response p = httpd::dispatch(httpd::Get, "/api/v1/recordings/archive/" + id + "/playlist.m3u", q,
	                                    std::string(), "192.168.1.9", httpd::AuthLevel::System, std::string(),
	                                    std::string(), "evil\r\nX: y", httpd::mediaScopeName());
	REQUIRE(p.code == 400);
}

TEST_CASE("a forged host never reaches the playlist, the box's own names do", "[archive][routes]")
{
	Archive a;
	a.disk.recording("one", "One", "X", 1, 1790000000);
	const std::string id = archive::list("", 0, 1).value().items[0].id;
	const std::string path = "/api/v1/recordings/archive/" + id + "/playlist.m3u";

	// No local address to fall back to: refused, same as a host that is no authority at all.
	httpd::Response evil = httpd::dispatch(httpd::Get, path, "", std::string(), "192.168.1.9",
	                                       httpd::AuthLevel::Read, std::string(), std::string(), "evil.com");
	REQUIRE(evil.code == 400);
	REQUIRE(evil.body.find("evil.com") == std::string::npos);

	// With one: the box's own address stands in, and the forged name never appears.
	httpd::Response rebuilt = httpd::dispatch(httpd::Get, path, "", std::string(), "192.168.1.9",
	                                          httpd::AuthLevel::Read, std::string(), std::string(),
	                                          "evil.com", std::string(), "10.0.0.5:8081");
	REQUIRE(rebuilt.code == 200);
	REQUIRE(rebuilt.body.find("evil.com") == std::string::npos);
	REQUIRE(rebuilt.body.find("http://10.0.0.5:8081/") != std::string::npos);

	httpd::Response loopback = httpd::dispatch(httpd::Get, path, "", std::string(), "192.168.1.9",
	                                           httpd::AuthLevel::Read, std::string(), std::string(),
	                                           "localhost:8081");
	REQUIRE(loopback.code == 200);
	REQUIRE(loopback.body.find("http://localhost:8081/") != std::string::npos);

	// A trailing dot, the way an absolute DNS name is sometimes written, is still this box.
	httpd::Response dotted = httpd::dispatch(httpd::Get, path, "", std::string(), "192.168.1.9",
	                                         httpd::AuthLevel::Read, std::string(), std::string(),
	                                         "localhost.:8081");
	REQUIRE(dotted.code == 200);
	REQUIRE(dotted.body.find("http://localhost.:8081/") != std::string::npos);

	char hostname[256];
	REQUIRE(gethostname(hostname, sizeof(hostname)) == 0);
	hostname[sizeof(hostname) - 1] = 0;
	httpd::Response named = httpd::dispatch(httpd::Get, path, "", std::string(), "192.168.1.9",
	                                        httpd::AuthLevel::Read, std::string(), std::string(),
	                                        std::string(hostname) + ":8081");
	REQUIRE(named.code == 200);
	REQUIRE(named.body.find(std::string("http://") + hostname + ":8081/") != std::string::npos);

	// Under a domain this box never configured, the way a router's own DNS or mDNS names it.
	httpd::Response aliased = httpd::dispatch(httpd::Get, path, "", std::string(), "192.168.1.9",
	                                          httpd::AuthLevel::Read, std::string(), std::string(),
	                                          std::string(hostname) + ".fritz.box:8081");
	REQUIRE(aliased.code == 200);
	REQUIRE(aliased.body.find(std::string("http://") + hostname + ".fritz.box:8081/") != std::string::npos);

	// An interface address, and not the loopback one the box always accepts anyway.
	struct ifaddrs *list = NULL;
	REQUIRE(getifaddrs(&list) == 0);
	std::string address;
	for (struct ifaddrs *i = list; i != NULL && address.empty(); i = i->ifa_next)
	{
		if (i->ifa_addr == NULL || i->ifa_addr->sa_family != AF_INET || (i->ifa_flags & IFF_LOOPBACK))
			continue;
		char text[INET6_ADDRSTRLEN];
		const struct sockaddr_in *v4 = (const struct sockaddr_in *) i->ifa_addr;
		if (inet_ntop(AF_INET, &v4->sin_addr, text, sizeof(text)) != NULL)
			address = text;
	}
	freeifaddrs(list);
	if (address.empty())
	{
		WARN("no IPv4 interface beside the loopback one, so the address check is left out");
		return;
	}
	httpd::Response byAddress = httpd::dispatch(httpd::Get, path, "", std::string(), "192.168.1.9",
	                                            httpd::AuthLevel::Read, std::string(), std::string(), address);
	REQUIRE(byAddress.code == 200);
	REQUIRE(byAddress.body.find(std::string("http://") + address + "/") != std::string::npos);
}

TEST_CASE("a second token in the query never reaches the playlist it would have named",
         "[archive][routes]")
{
	Archive a;
	a.disk.recording("one", "One", "X", 1, 1790000000);
	const std::string id = archive::list("", 0, 1).value().items[0].id;
	const std::string token = httpd::openScopedToken(httpd::mediaScopeName(), httpd::AuthLevel::System);
	const std::string name = httpd::queryTokenName();

	// The second value must never reach the body, whatever the router does with the pair.
	const std::string duplicated = name + "=" + token + "&" + name + "=%0D%0Aevil";
	httpd::Response p = httpd::dispatch(httpd::Get, "/api/v1/recordings/archive/" + id + "/playlist.m3u",
	                                    duplicated, std::string(), "192.168.1.9", httpd::AuthLevel::System,
	                                    std::string(), std::string(), "box:8081", httpd::mediaScopeName());
	REQUIRE(p.code != 200);
	REQUIRE(p.body.find("evil") == std::string::npos);
}

TEST_CASE("an address token that answers for no scope writes no playlist", "[archive][routes]")
{
	Archive a;
	a.disk.recording("one", "One", "X", 1, 1790000000);
	const std::string id = archive::list("", 0, 1).value().items[0].id;
	const std::string q = std::string(httpd::queryTokenName()) + "=not-a-real-token";
	const std::string path = "/api/v1/recordings/archive/" + id + "/playlist.m3u";

	// The header token granted the scope, not the query one this dispatch carries.
	httpd::Response header = httpd::dispatch(httpd::Get, path, q, std::string(), "192.168.1.9",
	                                         httpd::AuthLevel::System, std::string(), std::string(),
	                                         "box:8081", httpd::mediaScopeName());
	REQUIRE(header.code == 400);
	REQUIRE(header.body.find("not-a-real-token") == std::string::npos);

	// A reader on the home network whose token resolved to nothing.
	httpd::Response lan = httpd::dispatch(httpd::Get, path, q, std::string(), "192.168.1.9",
	                                      httpd::AuthLevel::Read, std::string(), std::string(), "box:8081");
	REQUIRE(lan.code == 400);
	REQUIRE(lan.body.find("not-a-real-token") == std::string::npos);
}

TEST_CASE("a media token the gate did not use is not written into the playlist", "[archive][routes]")
{
	Archive a;
	a.disk.recording("one", "One", "X", 1, 1790000000);
	const std::string id = archive::list("", 0, 1).value().items[0].id;
	const std::string token = httpd::openScopedToken(httpd::mediaScopeName(), httpd::AuthLevel::System);
	const std::string q = std::string(httpd::queryTokenName()) + "=" + token;

	// A header token without a scope decided this request; the query one was never the credential.
	httpd::Response p = httpd::dispatch(httpd::Get, "/api/v1/recordings/archive/" + id + "/playlist.m3u", q,
	                                    std::string(), "192.168.1.9", httpd::AuthLevel::System, std::string(),
	                                    std::string(), "box:8081");
	REQUIRE(p.code == 400);
	REQUIRE(p.body.find(token) == std::string::npos);
}

TEST_CASE("only a login draws a media token", "[archive][token]")
{
	httpd::Response read = httpd::dispatch(httpd::Post, "/api/v1/token/media", "", std::string(), "192.168.1.9",
	                                       httpd::AuthLevel::Read);
	REQUIRE(read.code == 403);
	REQUIRE(read.body.find("\"token\"") == std::string::npos);

	httpd::Response sys = httpd::dispatch(httpd::Post, "/api/v1/token/media", "", std::string(), "192.168.1.9",
	                                      httpd::AuthLevel::System);
	REQUIRE(sys.code == 200);
	const ::Json::Value v = parsed(sys.body);
	REQUIRE(v["scope"].asString() == "media");

	httpd::Credentials c;
	c.query_token = v["token"].asString();
	std::string scope;
	REQUIRE(httpd::granted(c, true, &scope) == httpd::AuthLevel::System);
	REQUIRE(scope == "media");
}

TEST_CASE("a public caller draws no token", "[archive][token]")
{
	httpd::Response r = httpd::dispatch(httpd::Post, "/api/v1/token/media", "", std::string(), "192.168.1.9",
	                                    httpd::AuthLevel::Public);
	REQUIRE(r.code == 403);
}

TEST_CASE("a media scope reaches the archive file route", "[archive][token]")
{
	Archive a;
	a.disk.recording("one", "One", "X", 1, 1790000000);
	const std::string id = archive::list("", 0, 1).value().items[0].id;
	httpd::Response f = httpd::dispatch(httpd::Get, "/api/v1/recordings/archive/" + id + "/file", "",
	                                    std::string(), "192.168.1.9", httpd::AuthLevel::System, std::string(),
	                                    std::string(), "box:8081", httpd::mediaScopeName());
	REQUIRE(f.code == 200);
	REQUIRE(f.fd >= 0);
	::close(f.fd);
}

TEST_CASE("scoped tokens are held to a ceiling and the newest works", "[archive][token]")
{
	httpd::forgetApiTokens();
	std::string last;
	for (int i = 0; i < 300; ++i)
		last = httpd::openScopedToken(httpd::mediaScopeName(), httpd::AuthLevel::System);
	REQUIRE(httpd::scopedTokenCountForTest() <= httpd::kMaxScopedTokens);
	httpd::Credentials c;
	c.query_token = last;
	std::string scope;
	REQUIRE(httpd::granted(c, true, &scope) == httpd::AuthLevel::System);
	REQUIRE(scope == "media");
	httpd::forgetApiTokens();
}

TEST_CASE("the soonest to expire is the scoped token the ceiling drops", "[archive][token]")
{
	httpd::forgetApiTokens();
	const time_t now = time(NULL);

	const std::string old_token = httpd::randomToken();
	httpd::addApiToken(httpd::tokenLookupPrefix(old_token), httpd::hashSecret(old_token),
	                   httpd::AuthLevel::System, httpd::mediaScopeName(), now + 100000);

	for (size_t i = 0; i < httpd::kMaxScopedTokens - 2; ++i)
	{
		const std::string filler = httpd::randomToken();
		httpd::addApiToken(httpd::tokenLookupPrefix(filler), httpd::hashSecret(filler),
		                   httpd::AuthLevel::System, httpd::mediaScopeName(), now + 100000);
	}

	// Inserted last but with the smallest expiry: a ceiling that dropped by age or by
	// position rather than by expiry would keep this one and drop one of the fillers.
	const std::string soon_token = httpd::randomToken();
	httpd::addApiToken(httpd::tokenLookupPrefix(soon_token), httpd::hashSecret(soon_token),
	                   httpd::AuthLevel::System, httpd::mediaScopeName(), now + 50);

	REQUIRE(httpd::scopedTokenCountForTest() == httpd::kMaxScopedTokens);

	REQUIRE_FALSE(httpd::openScopedToken(httpd::mediaScopeName(), httpd::AuthLevel::System).empty());
	REQUIRE(httpd::scopedTokenCountForTest() == httpd::kMaxScopedTokens);

	httpd::Credentials soon_c;
	soon_c.query_token = soon_token;
	std::string soon_scope;
	REQUIRE(httpd::granted(soon_c, true, &soon_scope) != httpd::AuthLevel::System);

	httpd::Credentials old_c;
	old_c.query_token = old_token;
	std::string old_scope;
	REQUIRE(httpd::granted(old_c, true, &old_scope) == httpd::AuthLevel::System);
	REQUIRE(old_scope == "media");
	httpd::forgetApiTokens();
}

TEST_CASE("the ceiling drops the soonest to expire whatever its scope", "[archive][token]")
{
	httpd::forgetApiTokens();
	const time_t now = time(NULL);

	const std::string soon_token = httpd::randomToken();
	httpd::addApiToken(httpd::tokenLookupPrefix(soon_token), httpd::hashSecret(soon_token),
	                   httpd::AuthLevel::System, httpd::mediaScopeName(), now + 50);

	for (size_t i = 0; i < httpd::kMaxScopedTokens - 1; ++i)
	{
		const std::string filler = httpd::randomToken();
		httpd::addApiToken(httpd::tokenLookupPrefix(filler), httpd::hashSecret(filler),
		                   httpd::AuthLevel::Read, "recordings", now + 100000);
	}
	REQUIRE(httpd::scopedTokenCountForTest() == httpd::kMaxScopedTokens);

	const std::string fresh = httpd::randomToken();
	httpd::addApiToken(httpd::tokenLookupPrefix(fresh), httpd::hashSecret(fresh),
	                   httpd::AuthLevel::Read, "recordings", now + 100000);
	REQUIRE(httpd::scopedTokenCountForTest() == httpd::kMaxScopedTokens);

	httpd::Credentials soon_c;
	soon_c.query_token = soon_token;
	std::string soon_scope;
	REQUIRE(httpd::granted(soon_c, true, &soon_scope) != httpd::AuthLevel::System);
	httpd::forgetApiTokens();
}

namespace
{

// Metadata as the box writes it, every detail stated.
const char kFullMetadata[] =
	"<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n\n<neutrino commandversion=\"1\">\n"
	"\t<record command=\"record\">\n"
	"\t\t<channelname>Das Erste HD</channelname>\n"
	"\t\t<epgtitle>Tatort: Murot</epgtitle>\n"
	"\t\t<id>12345678901</id>\n"
	"\t\t<info1>Krimi &amp; mehr</info1>\n"
	"\t\t<info2>Murot ermittelt.\nIn Wiesbaden &#228;ndert sich alles.</info2>\n"
	"\t\t<epgid>0</epgid>\n"
	"\t\t<mode>1</mode>\n"
	"\t\t<videopid>101</videopid>\n"
	"\t\t<audiopids>\n"
	"\t\t\t<audio pid=\"102\" audiotype=\"0\" selected=\"1\" name=\"Deutsch\"/>\n"
	"\t\t\t<audio pid=\"103\" audiotype=\"0\" selected=\"0\" name=\"Audiodeskription &amp; mehr\"/>\n"
	"\t\t\t<audio pid=\"106\" audiotype=\"6\" selected=\"0\" name=\"\"/>\n"
	"\t\t</audiopids>\n"
	"\t\t<vtxtpid>104</vtxtpid>\n"
	"\t\t<genremajor>16</genremajor>\n"
	"\t\t<genreminor>1</genreminor>\n"
	"\t\t<seriename>Tatort</seriename>\n"
	"\t\t<length>90</length>\n"
	"\t\t<productioncountry>DE</productioncountry>\n"
	"\t\t<productiondate>2025</productiondate>\n"
	"\t\t<rating>81</rating>\n"
	"\t\t<qualitiy>2</qualitiy>\n"
	"\t\t<parentallockage>12</parentallockage>\n"
	"\t\t<dateoflastplay>0</dateoflastplay>\n"
	"\t</record>\n</neutrino>\n";

// Every byte a whole UTF-8 sequence.
bool wholeUtf8(const std::string &s)
{
	for (size_t i = 0; i < s.size();)
	{
		const unsigned char c = (unsigned char) s[i];
		const size_t n = c < 0x80 ? 1 : (c >> 5) == 6 ? 2 : (c >> 4) == 14 ? 3 : (c >> 3) == 30 ? 4 : 0;
		if (n == 0 || i + n > s.size())
			return false;
		for (size_t k = 1; k < n; ++k)
			if (((unsigned char) s[i + k] >> 6) != 2)
				return false;
		i += n;
	}
	return true;
}

std::string onlyId()
{
	return archive::list("", 0, 1).value().items[0].id;
}

} // namespace

TEST_CASE("the details of a recording are what its metadata states", "[archive][details]")
{
	Archive a;
	a.disk.file("Tatort.ts", std::string(376, 'G'), 1790000000);
	a.disk.file("Tatort.xml", kFullMetadata, 1790000000);

	Result<archive::Details> got = archive::details(onlyId());
	REQUIRE(got.ok());
	const archive::Details d = got.value();
	REQUIRE(d.entry.title == "Tatort: Murot");
	REQUIRE(d.entry.channel == "Das Erste HD");
	REQUIRE(d.entry.duration == 90 * 60);
	REQUIRE(d.entry.size == 376);
	REQUIRE(d.description == "Krimi & mehr");
	REQUIRE(d.long_description == "Murot ermittelt.\nIn Wiesbaden \xc3\xa4ndert sich alles.");
	REQUIRE(d.genre == 16);
	REQUIRE(d.genre_minor == 1);
	REQUIRE(d.series == "Tatort");
	REQUIRE(d.country == "DE");
	REQUIRE(d.year == 2025);
	REQUIRE(d.rating == 81);
	REQUIRE(d.quality == 2);
	REQUIRE(d.age == 12);
	REQUIRE(d.audio.size() == 2);
	REQUIRE(d.audio[0] == "Deutsch");
	REQUIRE(d.audio[1] == "Audiodeskription & mehr");
	REQUIRE_FALSE(d.cover);
}

TEST_CASE("details the metadata leaves out or states as nought stay empty", "[archive][details]")
{
	Archive a;
	a.disk.file("bare.ts", "x", 1790000000);
	a.disk.file("bare.xml", "<epgtitle>Bare</epgtitle><info1></info1><genremajor>0</genremajor>"
	                        "<quality>3</quality><audiopids>\n</audiopids><rating>x</rating>", 1790000000);
	const archive::Details d = archive::details(onlyId()).value();
	REQUIRE(d.entry.title == "Bare");
	REQUIRE(d.description.empty());
	REQUIRE(d.long_description.empty());
	REQUIRE(d.genre == 0);
	REQUIRE(d.rating == 0);
	REQUIRE(d.quality == 3);
	REQUIRE(d.audio.empty());
	REQUIRE(d.series.empty());
	REQUIRE(archive::details("0000000000000000").error().code == ErrorCode::NoSuchRecording);
	REQUIRE(archive::details("../../etc/passw").error().code == ErrorCode::NoSuchRecording);
}

namespace
{

archive::Details numbersOf(const std::string &numbers)
{
	Archive a;
	a.disk.file("n.ts", "x", 1790000000);
	a.disk.file("n.xml", "<epgtitle>N</epgtitle>" + numbers, 1790000000);
	return archive::details(onlyId()).value();
}

} // namespace

TEST_CASE("detail numbers out of their range are left out", "[archive][details]")
{
	archive::Details d = numbersOf("<parentallockage>99</parentallockage><quality>3</quality><rating>100</rating>"
	                               "<genremajor>255</genremajor><genreminor>255</genreminor>"
	                               "<productiondate>9999</productiondate>");
	REQUIRE(d.age == 99);
	REQUIRE(d.quality == 3);
	REQUIRE(d.rating == 100);
	REQUIRE(d.genre == 255);
	REQUIRE(d.genre_minor == 255);
	REQUIRE(d.year == 9999);
	REQUIRE(numbersOf("<parentallockage>18</parentallockage>").age == 18);

	d = numbersOf("<parentallockage>21</parentallockage><quality>4</quality><rating>101</rating>"
	              "<genremajor>256</genremajor><genreminor>300</genreminor>"
	              "<productiondate>20250</productiondate>");
	REQUIRE(d.age == 0);
	REQUIRE(d.quality == 0);
	REQUIRE(d.rating == 0);
	REQUIRE(d.genre == 0);
	REQUIRE(d.genre_minor == 0);
	REQUIRE(d.year == 0);

	d = numbersOf("<parentallockage>-1</parentallockage><qualitiy>-2</qualitiy><rating>-81</rating>"
	              "<genremajor>-16</genremajor><productiondate>99999999999999999999999</productiondate>");
	REQUIRE(d.age == 0);
	REQUIRE(d.quality == 0);
	REQUIRE(d.rating == 0);
	REQUIRE(d.genre == 0);
	REQUIRE(d.year == 0);
}

TEST_CASE("detail texts are cut to their bound at a character boundary", "[archive][details]")
{
	Archive a;
	// The whole file stays under the metadata ceiling, so the bound and not the ceiling cuts.
	std::string short_text;
	for (size_t i = 0; i < archive::kMaxShortText; ++i)
		short_text += "\xc3\xa4";
	std::string long_text;
	for (size_t i = 0; i < archive::kMaxLongText / 2 + 100; ++i)
		long_text += "\xc3\xa4";
	std::string audio = "<audiopids>";
	for (size_t i = 0; i < archive::kMaxAudioTracks + 5; ++i)
		audio += "<audio pid=\"1\" name=\"" + std::string(archive::kMaxShortText + 7, 'n') + "\"/>";
	audio += "</audiopids>";
	a.disk.file("long.ts", "x", 1790000000);
	a.disk.file("long.xml", "<epgtitle>L</epgtitle><info1>x" + short_text + "</info1><info2>" + long_text +
	                        "</info2><seriename>" + std::string(archive::kMaxShortText * 2, 's') +
	                        "</seriename>" + audio, 1790000000);
	const archive::Details d = archive::details(onlyId()).value();
	REQUIRE(d.description.size() <= archive::kMaxShortText);
	REQUIRE(d.description.size() >= archive::kMaxShortText - 1);
	REQUIRE(wholeUtf8(d.description));
	REQUIRE(d.long_description.size() == archive::kMaxLongText);
	REQUIRE(wholeUtf8(d.long_description));
	REQUIRE(d.series.size() == archive::kMaxShortText);
	REQUIRE(d.audio.size() == archive::kMaxAudioTracks);
	REQUIRE(d.audio[0].size() == archive::kMaxShortText);
}

TEST_CASE("a cover is a plain picture of the same name beside the stream", "[archive][details]")
{
	Archive a;
	a.disk.recording("none", "None", "X", 1, 1790000000);
	a.disk.recording("png", "Png", "X", 1, 1790000001);
	a.disk.file("png.png", "PNG");
	a.disk.recording("both", "Both", "X", 1, 1790000002);
	a.disk.file("both.png", "PNG");
	a.disk.file("both.jpg", "JPG");
	a.disk.recording("linked", "Linked", "X", 1, 1790000003);
	a.disk.link("linked.jpg", "/etc/hostname");
	a.disk.recording("folder", "Folder", "X", 1, 1790000004);
	a.disk.dir("folder.jpg");
	a.disk.dir("sub");
	a.disk.recording("sub/deep", "Deep", "X", 1, 1790000005);
	a.disk.file("sub/deep.jpeg", "JPEG");

	const archive::Page p = archive::list("", 0, 10, archive::Sort(archive::SortKey::Title, false)).value();
	REQUIRE(p.items.size() == 6);
	// Both Deep Folder Linked None Png
	REQUIRE(archive::details(p.items[0].id).value().cover);
	REQUIRE(archive::coverPath(p.items[0].id).value() == a.disk.base + "/both.jpg");
	REQUIRE(archive::coverPath(p.items[1].id).value() == a.disk.base + "/sub/deep.jpeg");
	REQUIRE_FALSE(archive::details(p.items[2].id).value().cover);
	REQUIRE(archive::coverPath(p.items[2].id).error().code == ErrorCode::NoSuchRecording);
	REQUIRE_FALSE(archive::details(p.items[3].id).value().cover);
	REQUIRE(archive::coverPath(p.items[3].id).error().code == ErrorCode::NoSuchRecording);
	REQUIRE_FALSE(archive::details(p.items[4].id).value().cover);
	REQUIRE(archive::coverPath(p.items[4].id).error().code == ErrorCode::NoSuchRecording);
	REQUIRE(archive::details(p.items[5].id).value().cover);
	REQUIRE(archive::coverPath(p.items[5].id).value() == a.disk.base + "/png.png");
	REQUIRE(archive::coverPath("0000000000000000").error().code == ErrorCode::NoSuchRecording);
}

TEST_CASE("the details route answers the members the metadata states and no others", "[archive][routes]")
{
	Archive a;
	a.disk.file("Tatort.ts", std::string(376, 'G'), 1790000000);
	a.disk.file("Tatort.xml", kFullMetadata, 1790000000);
	a.disk.file("Tatort.png", "PNG");
	a.disk.recording("bare", "Bare", "ZDF", 30, 1790000001);
	const archive::Page p = archive::list("", 0, 10, archive::Sort(archive::SortKey::Title, false)).value();
	const std::string bare = p.items[0].id;
	const std::string full = p.items[1].id;

	httpd::Response r = ask(httpd::Get, "/api/v1/recordings/archive/" + full, "", httpd::AuthLevel::Read);
	REQUIRE(r.code == 200);
	::Json::Value v = parsed(r.body);
	REQUIRE(v["id"].asString() == full);
	REQUIRE(v["title"].asString() == "Tatort: Murot");
	REQUIRE(v["channel"].asString() == "Das Erste HD");
	REQUIRE(v["channel_id"].asString() == "2dfdc1c35");
	REQUIRE(v["start"].asInt64() == 1790000000 - 90 * 60);
	REQUIRE(v["duration"].asUInt() == 90 * 60);
	REQUIRE(v["size"].asUInt() == 376);
	REQUIRE(v["playing"].asBool() == false);
	REQUIRE(v["description"].asString() == "Krimi & mehr");
	REQUIRE(v["long_description"].asString() == "Murot ermittelt.\nIn Wiesbaden \xc3\xa4ndert sich alles.");
	REQUIRE(v["genre"].asUInt() == 16);
	REQUIRE(v["genre_minor"].asUInt() == 1);
	REQUIRE(v["series"].asString() == "Tatort");
	REQUIRE(v["country"].asString() == "DE");
	REQUIRE(v["year"].asUInt() == 2025);
	REQUIRE(v["rating"].asUInt() == 81);
	REQUIRE(v["quality"].asUInt() == 2);
	REQUIRE(v["age"].asUInt() == 12);
	REQUIRE(v["audio"].size() == 2);
	REQUIRE(v["audio"][1].asString() == "Audiodeskription & mehr");
	REQUIRE(v["cover"].asBool() == true);
	REQUIRE_FALSE(v.isMember("path"));

	v = parsed(ask(httpd::Get, "/api/v1/recordings/archive/" + bare, "", httpd::AuthLevel::Read).body);
	REQUIRE(v["title"].asString() == "Bare");
	REQUIRE(v["cover"].asBool() == false);
	const char *const absent[] = { "description", "long_description", "genre", "genre_minor", "series",
	                               "country", "year", "rating", "quality", "age", "audio" };
	for (size_t i = 0; i < sizeof(absent) / sizeof(absent[0]); ++i)
	{
		INFO(absent[i]);
		REQUIRE_FALSE(v.isMember(absent[i]));
	}

	httpd::Response gone = ask(httpd::Get, "/api/v1/recordings/archive/0000000000000000", "",
	                           httpd::AuthLevel::Read);
	REQUIRE(gone.code == 404);
	REQUIRE(gone.body.find("no-such-recording") != std::string::npos);
}

TEST_CASE("the cover route sends the picture and refuses a recording without one", "[archive][routes]")
{
	Archive a;
	a.disk.recording("one", "One", "X", 1, 1790000000);
	a.disk.file("one.png", "PNG");
	a.disk.recording("two", "Two", "X", 1, 1790000001);
	a.disk.file("two.jpg", "JPG");
	a.disk.recording("three", "Three", "X", 1, 1790000002);
	const archive::Page p = archive::list("", 0, 10, archive::Sort(archive::SortKey::Title, false)).value();

	httpd::Response png = ask(httpd::Get, "/api/v1/recordings/archive/" + p.items[0].id + "/cover", "",
	                          httpd::AuthLevel::Read);
	REQUIRE(png.code == 200);
	REQUIRE(png.content_type == "image/png");
	REQUIRE(png.fd >= 0);
	REQUIRE(png.length == 3);
	::close(png.fd);

	httpd::Response jpg = ask(httpd::Get, "/api/v1/recordings/archive/" + p.items[2].id + "/cover", "",
	                          httpd::AuthLevel::Read);
	REQUIRE(jpg.code == 200);
	REQUIRE(jpg.content_type == "image/jpeg");
	::close(jpg.fd);

	httpd::Response none = ask(httpd::Get, "/api/v1/recordings/archive/" + p.items[1].id + "/cover", "",
	                           httpd::AuthLevel::Read);
	REQUIRE(none.code == 404);
	REQUIRE(none.body.find("no-such-recording") != std::string::npos);
	REQUIRE(ask(httpd::Get, "/api/v1/recordings/archive/0000000000000000/cover", "",
	            httpd::AuthLevel::Read).code == 404);
}

TEST_CASE("a path names its archive entry by the archive's own rules without a scan", "[archive]")
{
	Archive a;
	a.disk.recording("Tatort", "Tatort", "Das Erste HD", 90, 1790000000);
	a.disk.dir("Serien");
	a.disk.recording("Serien/Lanz", "Lanz", "ZDF HD", 75, 1790000001);
	a.disk.dir("Serien/Tief");
	a.disk.recording("Serien/Tief/Zu", "Zu tief", "X", 1, 1790000002);
	a.disk.dir(".hidden");
	a.disk.recording(".hidden/Shift", "Shift", "X", 1, 1790000003);
	a.disk.file("stray.ts", "no metadata", 1790000004);
	a.disk.link("Linked.ts", a.disk.base + "/Tatort.ts");
	const archive::Page all = archive::list("", 0, 10).value();
	REQUIRE(all.total == 2);
	for (size_t i = 0; i < all.items.size(); ++i)
	{
		Result<archive::Entry> e = archive::entryAt(all.items[i].path);
		INFO(all.items[i].path);
		REQUIRE(e.ok());
		REQUIRE(e.value().id == all.items[i].id);
		REQUIRE(e.value().title == all.items[i].title);
		REQUIRE(e.value().channel == all.items[i].channel);
	}
	REQUIRE(archive::entryAt(a.disk.base + "/Serien/Tief/Zu.ts").error().code == ErrorCode::NoSuchRecording);
	REQUIRE(archive::entryAt(a.disk.base + "/.hidden/Shift.ts").error().code == ErrorCode::NoSuchRecording);
	REQUIRE(archive::entryAt(a.disk.base + "/stray.ts").error().code == ErrorCode::NoSuchRecording);
	REQUIRE(archive::entryAt(a.disk.base + "/Tatort.xml").error().code == ErrorCode::NoSuchRecording);
	REQUIRE(archive::entryAt("/tmp/elsewhere.ts").error().code == ErrorCode::NoSuchRecording);
	REQUIRE_FALSE(archive::entryAt("").ok());
	// Links the list skips are refused too, at the leaf and at the folder level.
	REQUIRE(archive::entryAt(a.disk.base + "/Linked.ts").error().code == ErrorCode::NoSuchRecording);
	a.disk.link("Alias", a.disk.base + "/Serien");
	REQUIRE(archive::entryAt(a.disk.base + "/Alias/Lanz.ts").error().code == ErrorCode::NoSuchRecording);

	// The record directory reached through a linked spelling is still the archive.
	char tmpl[] = "/tmp/coreapi_archive_root_XXXXXX";
	REQUIRE(mkdtemp(tmpl) != NULL);
	const std::string spelled = std::string(tmpl) + "/movie";
	REQUIRE(symlink(a.disk.base.c_str(), spelled.c_str()) == 0);
	Result<archive::Entry> top = archive::entryAt(spelled + "/Tatort.ts");
	Result<archive::Entry> deep = archive::entryAt(spelled + "/Serien/Lanz.ts");
	unlink(spelled.c_str());
	rmdir(tmpl);
	REQUIRE(top.ok());
	REQUIRE(deep.ok());
	REQUIRE(top.value().title == "Tatort");
	REQUIRE(deep.value().title == "Lanz");
}
