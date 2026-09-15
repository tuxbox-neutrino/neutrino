/*
 * test_storageroots.cpp - tests for the directories the file routes are confined to
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

#include "httpd/endpoints.h"
#include "httpd/http.h"
#include "httpd/router.h"

#include "coreapi/base/deps.h"
#include "coreapi/storage.h"
#include "coreapi/box/storage_internal.h"

#include <system/settings.h>

#include <config.h>

#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

#include <fcntl.h>
#include <limits.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

/* Where a box records is a setting and can name anywhere, and the list the file
   routes are confined to has to say so. A box whose recordings sit outside the
   media this build knows about shows an empty file browser otherwise, and shows
   it without a word: the refusal is the same one a name that is really out of
   bounds gets.

   Every case here drives the routes and nothing else. The list used to be put
   together by a handler of the older server, so a box answering only the new
   one never built it at all, and so did a box with the older server switched
   off. What these say is that no such call is needed. */

using namespace coreapi;

namespace
{

using namespace httpd;

struct ShippedRoutes
{
	ShippedRoutes() { setRoutesForTest(NULL); }
	~ShippedRoutes() { setRoutesForTest(NULL); }

	private:
		ShippedRoutes(const ShippedRoutes &);
		ShippedRoutes &operator=(const ShippedRoutes &);
};

// A name as it travels in a query. The separator is left as itself, so what a
// case made and what it asks for read the same.
std::string escaped(const std::string &v)
{
	static const char kHex[] = "0123456789ABCDEF";
	std::string out;
	for (size_t i = 0; i < v.size(); ++i)
	{
		const unsigned char c = (unsigned char) v[i];
		const bool plain = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
				   (c >= '0' && c <= '9') || c == '.' || c == '-' ||
				   c == '_' || c == '~' || c == '/';
		if (plain)
		{
			out += (char) c;
			continue;
		}
		out += '%';
		out += kHex[c >> 4];
		out += kHex[c & 0x0f];
	}
	return out;
}

/* The same as send below, as a caller whose credential stands for one part of the box.
   System, because that is the level such a token is minted at: the narrowing is the
   scope and never the level. */
Response sendScoped(const std::string &target, const std::string &scope)
{
	std::string path = target;
	std::string query;
	const size_t q = target.find('?');
	if (q != std::string::npos)
	{
		path = target.substr(0, q);
		query = target.substr(q + 1);
	}
	return dispatch(Get, path, query, std::string(), "127.0.0.1", AuthLevel::System,
	                "", "", "", scope);
}

Response send(Method m, const std::string &target, AuthLevel granted)
{
	std::string path = target;
	std::string query;
	const size_t q = target.find('?');
	if (q != std::string::npos)
	{
		path = target.substr(0, q);
		query = target.substr(q + 1);
	}
	return dispatch(m, path, query, std::string(), "127.0.0.1", granted);
}

bool put(const std::string &path, const std::string &what)
{
	const int fd = open(path.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0600);
	if (fd < 0)
		return false;
	const ssize_t wrote = write(fd, what.data(), what.size());
	if (close(fd) != 0)
		return false;
	return wrote == (ssize_t) what.size();
}

// The same comparison the layer under test makes, so that a case asking for a
// directory outside the default list is holding itself to the list the build
// really has rather than to a copy of it written here.
bool insideDefaults(const std::string &path)
{
	char mine[PATH_MAX];
	if (realpath(path.c_str(), mine) == NULL)
		return false;
	const std::string me(mine);

	const std::string all = storage::defaultRoots();
	size_t i = 0;
	while (i <= all.size())
	{
		size_t e = all.find(':', i);
		if (e == std::string::npos)
			e = all.size();
		const std::string one = all.substr(i, e - i);
		i = e + 1;
		if (one.empty())
			continue;
		char root[PATH_MAX];
		if (realpath(one.c_str(), root) == NULL)
			continue;
		const std::string r(root);
		if (r == "/")
			continue;
		if (me == r)
			return true;
		if (me.size() > r.size() && me.compare(0, r.size(), r) == 0 && me[r.size()] == '/')
			return true;
	}
	return false;
}

/* A directory of its own outside every default root, which is the whole point:
   /tmp is one of the media this build lists, so the sandbox every other case
   here makes would be reachable whether the list was ever put together or not,
   and a case built on one would pass against a layer that does nothing.

   Several places are tried because which of them a machine has and lets this
   write is the machine's business. An empty answer is a case that cannot
   measure anything, and the case says so rather than passing. */
struct OutsideBox
{
	std::string dir;

	OutsideBox()
	{
		static const char *const where[] =
			{ "/dev/shm", "/run", "/opt", "/srv", "/usr/local", "/root", "/home" };

		for (size_t i = 0; i < sizeof(where) / sizeof(where[0]); ++i)
		{
			if (access(where[i], W_OK) != 0)
				continue;
			std::string t = std::string(where[i]) + "/coreapi_roots_XXXXXX";
			std::vector<char> buf(t.begin(), t.end());
			buf.push_back('\0');
			if (mkdtemp(&buf[0]) == NULL)
				continue;
			const std::string made(&buf[0]);
			if (!insideDefaults(made))
			{
				dir = made;
				return;
			}
			(void) system(("rm -rf " + made).c_str());
		}
	}

	~OutsideBox()
	{
		if (!dir.empty())
			(void) system(("rm -rf " + dir).c_str());
	}

	std::string at(const std::string &name) const { return dir + "/" + name; }

	private:
		OutsideBox(const OutsideBox &);
		OutsideBox &operator=(const OutsideBox &);
};

// A box that has never had its list put together, which is the state the
// defect is about, and the list put back whichever way a case leaves.
struct PlainRoots
{
	std::string before;

	PlainRoots() : before(storage::roots()) { storage::setRoots(storage::defaultRoots()); }
	~PlainRoots() { storage::setRoots(before); }

	private:
		PlainRoots(const PlainRoots &);
		PlainRoots &operator=(const PlainRoots &);
};

/* The store the box runs on rather than a fake, because the whole of what is
   being measured is that the setting is read through the declaration the
   program already carries, off the member the box's own loop writes.

   Nothing is saved: a read never reaches the loop, and a case that asked for
   one would be waiting on a thread that is not running here. */
struct RecordingDir
{
	SNeutrinoSettings     values;
	ClearedSettingsSource cleared;

	explicit RecordingDir(const std::string &dir) : values(SNeutrinoSettings())
	{
		setSettingsText(values.network_nfs_recordingdir, dir);
		coreapi::installRealSettingsSource(&values, NULL);
	}

	~RecordingDir() { coreapi::installRealSettingsSource(NULL, NULL); }

	void moveTo(const std::string &dir)
	{
		setSettingsText(values.network_nfs_recordingdir, dir);
	}

	private:
		RecordingDir(const RecordingDir &);
		RecordingDir &operator=(const RecordingDir &);
};

/* The movie browser's own file, pointed at one a case wrote. The shipped name sits in
   the directory a box on this machine keeps its configuration in, and a case reading
   that one would answer differently on every machine. */
struct MovieBrowserConfig
{
	std::string path;
	const char *before;

	explicit MovieBrowserConfig(const std::string &p)
		: path(p), before(storage::internal::moviebrowser_config_path)
	{
		storage::internal::moviebrowser_config_path = path.c_str();
	}

	~MovieBrowserConfig() { storage::internal::moviebrowser_config_path = before; }

	private:
		MovieBrowserConfig(const MovieBrowserConfig &);
		MovieBrowserConfig &operator=(const MovieBrowserConfig &);
};

} // namespace

TEST_CASE("the directory the box records into is listed without a call to the older server", "[storageroots][endpoints]")
{
	ShippedRoutes shipped;
	PlainRoots plain;

	OutsideBox box;
	{
		INFO("no directory outside the default roots could be made, so this case measures nothing");
		REQUIRE_FALSE(box.dir.empty());
	}

	RecordingDir recording(box.dir);
	REQUIRE(put(box.at("film.ts"), "recorded"));

	// Nothing of the older server has been called, and nothing of it is
	// reachable from here: this is the route the new pages ask.
	Response r = send(Get, "/api/v1/storage/files?path=" + escaped(box.dir), AuthLevel::System);
	REQUIRE(r.code == 200);
	REQUIRE(r.body.find("film.ts") != std::string::npos);
}

TEST_CASE("a directory entered after the last request is reached by the next one", "[storageroots][endpoints]")
{
	/* The reason the list is put together per request rather than once when the box
	   starts. An operator who moves the recording directory in the menu gets a file
	   browser that follows, and gets it without restarting anything. */
	ShippedRoutes shipped;
	PlainRoots plain;

	OutsideBox first;
	OutsideBox second;
	{
		INFO("no directory outside the default roots could be made, so this case measures nothing");
		REQUIRE_FALSE(first.dir.empty());
		REQUIRE_FALSE(second.dir.empty());
	}

	RecordingDir recording(first.dir);
	REQUIRE(put(second.at("later.ts"), "recorded"));

	// Not yet: the second directory is nothing this box has been told about.
	REQUIRE(send(Get, "/api/v1/storage/files?path=" + escaped(second.dir), AuthLevel::System).code == 400);

	recording.moveTo(second.dir);

	Response now = send(Get, "/api/v1/storage/files?path=" + escaped(second.dir), AuthLevel::System);
	REQUIRE(now.code == 200);
	REQUIRE(now.body.find("later.ts") != std::string::npos);

	// And the one it was moved away from is not reachable any more, so the list
	// is put together again rather than grown.
	REQUIRE(send(Get, "/api/v1/storage/files?path=" + escaped(first.dir), AuthLevel::System).code == 400);
}

TEST_CASE("the directories the movie browser was given are listed as well", "[storageroots][endpoints]")
{
	/* The movie browser keeps its own directories in a file of its own rather than in
	   the settings, so they are read where it writes them. Up to eight, and a line that
	   does not name an absolute directory is left out rather than ending the list. */
	ShippedRoutes shipped;
	PlainRoots plain;

	OutsideBox box;
	{
		INFO("no directory outside the default roots could be made, so this case measures nothing");
		REQUIRE_FALSE(box.dir.empty());
	}

	const std::string listed = box.at("watched");
	const std::string unlisted = box.at("unwatched");
	REQUIRE(mkdir(listed.c_str(), 0700) == 0);
	REQUIRE(mkdir(unlisted.c_str(), 0700) == 0);
	REQUIRE(put(listed + "/film.ts", "recorded"));

	const std::string conf = box.at("moviebrowser.conf");
	REQUIRE(put(conf, "mb_dir_0=\nmb_dir_1=" + listed + "\nmb_dir_2=relative/name\n"));

	MovieBrowserConfig pointed(conf);

	Response r = send(Get, "/api/v1/storage/files?path=" + escaped(listed), AuthLevel::System);
	REQUIRE(r.code == 200);
	REQUIRE(r.body.find("film.ts") != std::string::npos);

	// The directory beside it is not in the file, so the list took what it was
	// given and not the directory the two of them sit in.
	REQUIRE(send(Get, "/api/v1/storage/files?path=" + escaped(unlisted), AuthLevel::System).code == 400);
}

TEST_CASE("a directory this box was never told about stays out of reach", "[storageroots][endpoints]")
{
	/* The other half of all of the above. Putting the list together again widens it by
	   what the configuration names and by nothing else, so a name outside it is refused
	   as it was before any of this ran. */
	ShippedRoutes shipped;
	PlainRoots plain;

	OutsideBox recorded;
	OutsideBox elsewhere;
	{
		INFO("no directory outside the default roots could be made, so this case measures nothing");
		REQUIRE_FALSE(recorded.dir.empty());
		REQUIRE_FALSE(elsewhere.dir.empty());
	}

	RecordingDir recording(recorded.dir);
	REQUIRE(put(elsewhere.at("private.txt"), "not yours"));

	Response listed = send(Get, "/api/v1/storage/files?path=" + escaped(elsewhere.dir), AuthLevel::System);
	REQUIRE(listed.code == 400);
	REQUIRE(listed.body.find("outside-roots") != std::string::npos);

	Response read = send(Get, "/api/v1/storage/file?path=" + escaped(elsewhere.at("private.txt")),
			     AuthLevel::System);
	REQUIRE(read.code == 400);
	REQUIRE(read.fd < 0);
}

TEST_CASE("a credential for the media reaches a directory this box was told to record into",
          "[storageroots][endpoints]")
{
	/* The other half of what the scope admits. The media roots it admits by name are
	   covered where the scope itself is; this is the half that only exists once the roots
	   are put together again: a directory outside the shipped list is one somebody entered
	   in a menu, and a recording there is what the address beside it is for. */
	ShippedRoutes shipped;
	PlainRoots plain;

	OutsideBox box;
	{
		INFO("no directory outside the default roots could be made, so this case measures nothing");
		REQUIRE_FALSE(box.dir.empty());
	}

	RecordingDir recording(box.dir);
	REQUIRE(put(box.at("film.ts"), "recorded"));

	REQUIRE(sendScoped("/api/v1/storage/file?path=" + escaped(box.at("film.ts")), "media").code == 200);
}

TEST_CASE("the roots are put together before a scope is held to them", "[storageroots][endpoints]")
{
	/* WHAT THIS GUARDS IS AN ORDER, and the order is otherwise written down nowhere. The
	   scope resolves against the roots, so the call that puts them together has to run
	   first; turned around, a scope would be held to whatever the last request left behind.
	   The whole suite stays green when the two are swapped, which is why this case exists
	   and why it sets the roots to something deliberately useless first. */
	ShippedRoutes shipped;
	PlainRoots plain;

	OutsideBox box;
	{
		INFO("no directory outside the default roots could be made, so this case measures nothing");
		REQUIRE_FALSE(box.dir.empty());
	}

	RecordingDir recording(box.dir);
	REQUIRE(put(box.at("film.ts"), "recorded"));

	// A list this file is not in, left standing as a stale answer would be.
	coreapi::storage::setRoots("/nonexistent-root-for-this-case");

	REQUIRE(sendScoped("/api/v1/storage/file?path=" + escaped(box.at("film.ts")), "media").code == 200);
}
