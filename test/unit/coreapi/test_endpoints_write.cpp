/*
 * test_endpoints_write.cpp - tests for every writing route
 *
 * Copyright (C) 2026 NI-Team
 * Copyright (C) 2026 Tuxbox-Project (fixes, tests)
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
#include "support/answers.h"
#include "support/fakes.h"

#include "httpd/auth.h"
#include "httpd/webconfig.h"
#include "httpd/credentials.h"
#include "httpd/endpoint.h"
#include "httpd/endpoints.h"
#include "httpd/http.h"
#include "httpd/json.h"
#include "httpd/doc/openapi.h"
#include "httpd/router.h"
#include "httpd/login_bridge.h"
#include "httpd/schema.h"
#include "httpd/server.h"
#include "httpd/status.h"

#include "coreapi/archive.h"
#include "coreapi/daemons.h"
#include "coreapi/base/errors.h"
#include "coreapi/base/settingswrites.h"
#include "configfile.h"
#include "coreapi/netfs.h"
#include "coreapi/shares.h"
#include "coreapi/extensionindex.h"
#include "coreapi/extensions.h"
#include "coreapi/osd.h"
#include "coreapi/plugins.h"
#include "coreapi/system.h"
#include "coreapi/base/schema.h"
#include "coreapi/settings/settings.h"
#include "coreapi/storage.h"
#include "coreapi/websources.h"
#include "coreapi/box/storage_internal.h"
#include "coreapi/base/types.h"
#include "coreapi/backup.h"
#include "coreapi/channels.h"
#include "coreapi/network.h"
#include "coreapi/webui.h"

#include <neutrinoMessages.h>

#include "jsoncpp/json/json.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <memory>
#include <limits>
#include <string>
#include <thread>
#include <vector>

#include <openssl/evp.h>
#include <openssl/pem.h>

#include <fcntl.h>
#include <limits.h>
#include <dirent.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#include <timerdclient/timerdtypes.h>

using namespace httpd;

namespace
{

/* Cleared going in as well as coming out, so a case elsewhere that failed and
   unwound past its own restore cannot decide what these read. */
struct ShippedRoutes
{
	ShippedRoutes() { setRoutesForTest(NULL); }
	~ShippedRoutes() { setRoutesForTest(NULL); }

	private:
		ShippedRoutes(const ShippedRoutes &);
		ShippedRoutes &operator=(const ShippedRoutes &);
};

struct Reply
{
	int                                              code;
	std::string                                      content_type;
	std::string                                      body;
	std::vector<std::pair<std::string, std::string> > headers;

	Reply() : code(0) {}
};

// A file answer's descriptor is closed here: the reply keeps what the router
// said, not the file.
Reply replyOf(const Response &r)
{
	Reply out;
	out.code = r.code;
	out.content_type = r.content_type;
	out.body = r.body;
	out.headers = r.headers;
	if (r.fd >= 0)
		close(r.fd);
	return out;
}

// The value of a header the answer carries, and empty for one it does not, so
// that a header absent and a header empty cannot be told apart by accident: no
// header this server writes is ever empty.
std::string headerOf(const Reply &r, const char *name)
{
	for (size_t i = 0; i < r.headers.size(); ++i)
	{
		if (r.headers[i].first == name)
			return r.headers[i].second;
	}
	return std::string();
}

/* One request, split where a transport splits it and with the level already
   resolved, which is what the router is handed. The level is a parameter of every
   one of these because that is the whole of what half of these cases are about: the
   same request from a caller the network exemption granted a read and from a caller
   holding the box's credential must not come back the same. */
Reply send(Method m, const std::string &target, const std::string &body,
           AuthLevel granted, const std::string &peer)
{
	std::string path = target;
	std::string query;
	const size_t q = target.find('?');
	if (q != std::string::npos)
	{
		path = target.substr(0, q);
		query = target.substr(q + 1);
	}
	return replyOf(dispatch(m, path, query, body, peer, granted));
}

Reply authedPost(const std::string &t, const std::string &b)
{
	return send(Post, t, b, AuthLevel::System, "127.0.0.1");
}

Reply authedPut(const std::string &t, const std::string &b)
{
	return send(Put, t, b, AuthLevel::System, "127.0.0.1");
}

Reply authedPatch(const std::string &t, const std::string &b)
{
	return send(Patch, t, b, AuthLevel::System, "127.0.0.1");
}

Reply authedDelete(const std::string &t)
{
	return send(Delete, t, "", AuthLevel::System, "127.0.0.1");
}

Reply authedGet(const std::string &t)
{
	return send(Get, t, "", AuthLevel::System, "127.0.0.1");
}

// What a caller on the box's own network is granted without presenting
// anything, which is a read and nothing above it.
Reply lanPost(const std::string &t, const std::string &b)
{
	return send(Post, t, b, AuthLevel::Read, "192.168.1.9");
}

Reply lanPatch(const std::string &t, const std::string &b)
{
	return send(Patch, t, b, AuthLevel::Read, "192.168.1.9");
}

Reply lanPut(const std::string &t, const std::string &b)
{
	return send(Put, t, b, AuthLevel::Read, "192.168.1.9");
}

Reply lanDelete(const std::string &t)
{
	return send(Delete, t, "", AuthLevel::Read, "192.168.1.9");
}

// The tree's own parser as a second opinion: a body only this server can read
// is not a document anybody else can use.
bool parseDocument(const std::string &doc, ::Json::Value &root)
{
	::Json::CharReaderBuilder builder;
	std::unique_ptr< ::Json::CharReader> reader(builder.newCharReader());
	std::string errs;
	return reader->parse(doc.data(), doc.data() + doc.size(), &root, &errs);
}

::Json::Value parsed(const std::string &body)
{
	::Json::Value root;
	REQUIRE(parseDocument(body, root));
	return root;
}

// What the item of this id is set to, and a sentinel that is not the empty
// string for one the answer does not carry, so that a value withheld and an
// item missing altogether cannot read the same.
std::string valueOf(const std::string &body, const std::string &id)
{
	const ::Json::Value items = parsed(body)["items"];
	for (::Json::ArrayIndex i = 0; i < items.size(); ++i)
	{
		if (items[i]["id"].asString() == id)
			return items[i]["value"].asString();
	}
	return "<no such item>";
}

std::string stringField(const std::string &body, const char *name)
{
	const ::Json::Value root = parsed(body);
	REQUIRE(root.isMember(name));
	return root[name].asString();
}

/* The configuration is process wide and is put back from a destructor, because a
   check that fails unwinds past a last line. The password is hashed rather than
   written down, so what the login route goes through here is the derivation it goes
   through on the box. */
const char kUser[] = "root";
// none of the passwords these boxes have shipped with, which the first-login gate would hold to a change
const char kPassword[] = "own-pass-1";

// The address the login cases are answered from, in the range set aside for
// documentation so it is nobody's network.
const char kPeer[] = "203.0.113.7";

struct ConfigFixture
{
	WebConfig before;

	ConfigFixture() : before(config())
	{
		/* The gate in front of the login route is process wide and counts wrong
		   answers per address, so a case that guessed wrong would otherwise
		   decide what the case after it is answered with. Cleared going in as
		   well as coming out, because a case that failed unwinds past its own
		   restore. */
		forgetLoginAttemptsForTest();

		WebConfig c = defaultWebConfig();
		c.username = kUser;
		/* Well under what the box is configured with, because every case here pays
		   for it and one of them pays a hundred times: what is being exercised is
		   that both answers pay the same. High enough that the three intervals the
		   timing case measures are milliseconds rather than microseconds, so the
		   scheduler is not what the comparison is reading. */
		c.password_hash = hashSecret(kPassword, 2000);
		setConfigForTest(c);
	}

	~ConfigFixture()
	{
		forgetLoginAttemptsForTest();
		setLoginClockForTest(0);
		setConfigForTest(before);
	}

	private:
		ConfigFixture(const ConfigFixture &);
		ConfigFixture &operator=(const ConfigFixture &);
};

std::string loginBody(const char *user, const char *password)
{
	std::string out = "{\"user\":\"";
	out += user;
	out += "\",\"password\":\"";
	out += password;
	out += "\"}";
	return out;
}

Reply login(const char *user, const char *password)
{
	// Public, which is what a caller presenting nothing is, so this is the one
	// route these cases reach without a level.
	return send(Post, "/api/v1/login", loginBody(user, password), AuthLevel::Public, kPeer);
}

// The session an answer handed out, read off the cookie it set, because that is
// where a session travels and nothing in the body names it.
std::string tokenOf(const Reply &r)
{
	const std::string c = headerOf(r, "Set-Cookie");
	const size_t eq = c.find('=');
	const size_t end = c.find(';');
	REQUIRE(eq != std::string::npos);
	REQUIRE(end > eq);
	return c.substr(eq + 1, end - eq - 1);
}

/* Seconds this thread spent on a processor, not seconds of wall clock. Both cases below
   compare one answer's cost against another's, and what makes one cost more is work the
   box did: a derivation run or not run. Wall clock also carries the time this thread was
   ready and not running, which on a loaded machine is larger than the difference being
   measured. A request here is dispatched in this thread and returns in it.

   Sleeping is the one thing this cannot see, and no answer here sleeps. */
double cpuSecondsOf(void (*run)())
{
	struct timespec a;
	struct timespec b;
	REQUIRE(clock_gettime(CLOCK_THREAD_CPUTIME_ID, &a) == 0);
	run();
	REQUIRE(clock_gettime(CLOCK_THREAD_CPUTIME_ID, &b) == 0);
	return (double)(b.tv_sec - a.tv_sec) + (double)(b.tv_nsec - a.tv_nsec) / 1e9;
}

/* The quickest of five runs. Processor time still moves with the clock the processor runs
   at, which a busy host changes from one run to the next by more than the factor of two the
   cases below allow; the quickest run is the one least slowed by that, and the cost of the
   work itself is what remains in it. */
double bestCpuSecondsOf(void (*run)())
{
	double best = cpuSecondsOf(run);
	for (int i = 1; i < 5; ++i)
	{
		const double t = cpuSecondsOf(run);
		if (t < best)
			best = t;
	}
	return best;
}

const int kTimingRounds = 32;

/* Every round starts with the gate cleared, in all three of these and not only
   in the two that guess wrong, so that what the three measure differs by the
   answer being measured and by nothing else. Without it the wrong ones would be
   refused without deriving anything after the free attempts ran out, which is
   the gate working and would read here as an answer that costs nothing. */
void manyWrongLogins()
{
	for (int i = 0; i < kTimingRounds; ++i)
	{
		forgetLoginAttemptsForTest();
		(void) login(kUser, "not the password");
	}
}

void manyRightLogins()
{
	for (int i = 0; i < kTimingRounds; ++i)
	{
		forgetLoginAttemptsForTest();
		const Reply r = login(kUser, kPassword);
		// Every one of them opens a session, and a session left in the table
		// outlives this case. Closed by the token out of the cookie, which is
		// what the route that closes one is given.
		const std::string cookie = headerOf(r, "Set-Cookie");
		const size_t eq = cookie.find('=');
		const size_t end = cookie.find(';');
		if (eq != std::string::npos && end != std::string::npos && end > eq)
			(void) closeSession(cookie.substr(eq + 1, end - eq - 1));
	}
}

void manyWrongNames()
{
	for (int i = 0; i < kTimingRounds; ++i)
	{
		forgetLoginAttemptsForTest();
		(void) login("nobody at all", kPassword);
	}
}

// Eight attempts that each reach the derivation because the record in front of
// them is cleared first, against eight that the gate turns down because it is
// not. Written out here rather than inside the case so that both are measured
// through the same reading as everything else timed in this file.
void eightDerivedRefusals()
{
	for (int i = 0; i < 8; ++i)
	{
		forgetLoginAttemptsForTest();
		(void) login(kUser, "nope");
	}
}

void eightGatedRefusals()
{
	for (int i = 0; i < 8; ++i)
		REQUIRE(login(kUser, "nope").code == 429);
}

const uint64_t kChannel = 0x2b66ULL;

// An identifier the way every answer here writes one, so a case builds a
// request out of the same spelling it reads back rather than out of a second
// one written by hand.
std::string hexOf(uint64_t v)
{
	char buf[24];
	std::snprintf(buf, sizeof(buf), "%llx", (unsigned long long) v);
	return std::string(buf);
}

coreapi::ChannelInfo makeChannel(uint64_t id, const char *name)
{
	coreapi::ChannelInfo c;
	c.id = id;
	c.epg_id = id;
	c.name = name;
	c.kind = coreapi::ServiceKind::Tv;
	return c;
}

// Two bouquets on the channel fake, numbered as the box numbers them, because
// the fixture below ships none and every bouquet route names one.
void giveBouquets(FakeChannelSource &f)
{
	coreapi::BouquetInfo first;
	first.id = 1;
	first.name = "Erstes";
	first.user_bouquet = true;
	f.bouquets.push_back(first);

	coreapi::BouquetInfo second;
	second.id = 2;
	second.name = "Zweites";
	second.user_bouquet = true;
	f.bouquets.push_back(second);
}

// The bouquet of that name out of a listing, and a null value for a name the
// listing does not carry, so a bouquet that is gone and one that is unchanged
// cannot read alike.
::Json::Value bouquetNamed(const std::string &body, const std::string &name)
{
	const ::Json::Value items = parsed(body)["items"];
	for (::Json::ArrayIndex i = 0; i < items.size(); ++i)
	{
		if (items[i]["name"].asString() == name)
			return items[i];
	}
	return ::Json::Value();
}

// For a table written here rather than shipped: what the check over tables
// says about it is the subject, and nothing calls it.
Response unreachedHandler(const Request &)
{
	return noContent();
}

/* The number the document states for this route's body, and nought for a member it
   does not carry. Read out of the document rather than written down here, which is
   the whole point of the cases below: one carrying its own copy of the number would
   go on passing after the document and the server had parted company, and a page
   drawn from a document that offers what this box turns down looks like it worked. */
long documentSays(const char *path, const char *verb, const char *member)
{
	const ::Json::Value schema =
		parsed(openapi::document())["paths"][path][verb]["requestBody"]
			["content"]["application/json"]["schema"];
	return schema.isMember(member) ? (long) schema[member].asInt64() : 0L;
}

// A body of that many identifiers, written the way every answer here writes
// one, with the last of them spoiled where a case wants the refusal to be about
// something other than how many there are.
std::string channelList(uint64_t base, long how_many, bool last_is_junk)
{
	std::string body = "[";
	for (long i = 0; i < how_many; ++i)
	{
		if (i > 0)
			body += ',';
		body += '"';
		body += (last_is_junk && i + 1 == how_many) ? std::string("not-hex") : hexOf(base + (uint64_t) i);
		body += '"';
	}
	body += ']';
	return body;
}

// The same for the other body, whose members are the caller's own names.
std::string settingsObject(long how_many)
{
	std::string body = "{";
	for (long i = 0; i < how_many; ++i)
	{
		char one[64];
		std::snprintf(one, sizeof(one), "%s\"k%ld\":\"1\"", i > 0 ? "," : "", i);
		body += one;
	}
	body += '}';
	return body;
}

/* Everything a mutating route reaches, installed at once. One fixture and not one per
   case, because most of these routes touch two of them at a time: a zap is looked up
   in the channel list and posted to the command queue, and a case installing only one
   would be measuring whichever it left out. */
struct BoxFixture
{
	FakeChannelSource        channels;
	FakeCommandSink          commands;
	FakeEventSink            events;
	FakeSystemSource         box;
	FakeTimerSource          timers;
	FakeSettingsSource       store;

	InstalledChannelSource   installed_channels;
	InstalledSink            installed_commands;
	InstalledEventSink       installed_events;
	InstalledSystemSource    installed_box;
	InstalledTimerSource     installed_timers;
	InstalledSettingsSource  installed_store;

	BoxFixture()
		: installed_channels(&channels),
		  installed_commands(&commands),
		  installed_events(&events),
		  installed_box(&box),
		  installed_timers(&timers),
		  installed_store(&store)
	{
		channels.channels.push_back(makeChannel(kChannel, "the one"));
		channels.current = channels.channels[0];
		channels.current_status = coreapi::Status::Ok;
		box.volume_level = 40;
		// Well clear of the epoch, so a timer made an hour from now is not a
		// timer in the past on a clock the fixture never set.
		timers.clock = 1735689600;
	}

	private:
		BoxFixture(const BoxFixture &);
		BoxFixture &operator=(const BoxFixture &);
};

// A directory of its own per case, so nothing a case leaves behind is what the
// next one reads.
struct Sandbox
{
	std::string dir;

	Sandbox()
	{
		char tmpl[] = "/tmp/httpd_write_XXXXXX";
		if (mkdtemp(tmpl) != NULL)
			dir = tmpl;
	}

	~Sandbox()
	{
		if (!dir.empty())
			(void) system(("rm -rf " + dir).c_str());
	}

	std::string at(const std::string &name) const { return dir + "/" + name; }

	private:
		Sandbox(const Sandbox &);
		Sandbox &operator=(const Sandbox &);
};

/* A box of the case's own for the backup routes: a configuration directory with
   a list of what is backed up, backup.sh, the box's own /var/tuxbox in the
   sandbox, and the restart after a restore caught before it starts, so no case
   restarts the machine it runs on. */
unsigned &rebootsStarted()
{
	static unsigned started = 0;
	return started;
}

bool rebootStartsOk = true;

// Stands in for the restart, and ends what a restart that started would end.
bool catchReboot()
{
	rebootsStarted()++;
	if (rebootStartsOk)
		coreapi::backup::internal::restartedForTest();
	return rebootStartsOk;
}

// A restart that has not come yet: the restore waits for it.
bool restartLater()
{
	return true;
}

struct BackupFixture
{
	Sandbox     box;
	std::string config;
	std::string list;
	std::string bin;
	const char *before_tmp;
	const char *before_config;
	const char *before_list;
	const char *before_bin;
	const char *before_tuxbox;
	const char *before_reboot;
	bool        before_in_place;

	BackupFixture()
		: config(box.at("config")), list(box.at("config/tobackup.conf")), bin(box.at("bin")),
		  before_tmp(coreapi::backup::internal::tmp_dir), before_config(coreapi::backup::internal::config_dir),
		  before_list(coreapi::backup::internal::backup_list), before_bin(coreapi::backup::internal::script_dir),
		  before_tuxbox(coreapi::backup::internal::tuxbox_dir),
		  before_reboot(coreapi::backup::internal::restart_command),
		  before_in_place(coreapi::backup::internal::restart_in_place)
	{
		mkdir(config.c_str(), 0700);
		mkdir(bin.c_str(), 0700);
		mkdir(box.at("extra").c_str(), 0700);
		mkdir(box.at("other").c_str(), 0700);
		write(config + "/neutrino.conf", "volume=50\n");
		write(box.at("extra/a.conf"), "a=1\n");
		write(box.at("other/x.conf"), "x=1\n");
		write(list, "# what is backed up\n" + box.at("extra") + "/   # a directory\n-" + box.at("other") + "\n" +
		      box.at("cfg") + "/*.cfg\n" + box.at("wild") + "/*/s.conf\n");
		// backup.sh as the box's: an archive named after the box and the day, in $1
		script("backup.sh", "#!/bin/sh\ntar -czf \"$1/settings_testbox_20261013_1200.tar.gz\" -C / \"" +
		       config.substr(1) + "\" 2>/dev/null\necho Backup to $1\n");
		coreapi::backup::internal::tmp_dir = box.dir.c_str();
		coreapi::backup::internal::config_dir = config.c_str();
		coreapi::backup::internal::backup_list = list.c_str();
		coreapi::backup::internal::script_dir = bin.c_str();
		// an archive of the box's own layout, config/..., lands in the sandbox
		coreapi::backup::internal::tuxbox_dir = box.dir.c_str();
		rebootsStarted() = 0;
		rebootStartsOk = true;
		coreapi::backup::internal::setRestarterForTest(&catchReboot);
	}

	~BackupFixture()
	{
		coreapi::backup::internal::max_members = 20000;
		coreapi::backup::internal::max_unpacked = 256ull * 1024 * 1024;
		coreapi::backup::internal::setRestarterForTest(NULL);
		coreapi::backup::internal::restartedForTest();
		coreapi::backup::internal::tuxbox_dir = before_tuxbox;
		coreapi::backup::internal::restart_command = before_reboot;
		coreapi::backup::internal::restart_in_place = before_in_place;
		coreapi::backup::internal::tmp_dir = before_tmp;
		coreapi::backup::internal::config_dir = before_config;
		coreapi::backup::internal::backup_list = before_list;
		coreapi::backup::internal::script_dir = before_bin;
	}

	void write(const std::string &path, const std::string &text)
	{
		FILE *f = std::fopen(path.c_str(), "w");
		if (f != NULL)
		{
			std::fputs(text.c_str(), f);
			std::fclose(f);
		}
	}

	void script(const char *name, const std::string &text)
	{
		write(bin + "/" + name, text);
		chmod((bin + "/" + name).c_str(), 0700);
	}

	static std::string read(const std::string &path)
	{
		std::string out;
		FILE *f = std::fopen(path.c_str(), "r");
		if (f == NULL)
			return "(none)";
		char buf[4096];
		size_t n;
		while ((n = std::fread(buf, 1, sizeof(buf), f)) > 0)
			out.append(buf, n);
		std::fclose(f);
		return out;
	}

	/* What the unpacking may leave behind and must not: its own directory in the
	   upload directory, and a file written beside its place that never took it. */
	std::vector<std::string> leftovers() const
	{
		std::vector<std::string> out;
		const std::string dirs[] = { box.dir, config, box.at("extra"), box.at("cfg") };
		for (size_t i = 0; i < sizeof(dirs) / sizeof(dirs[0]); ++i)
		{
			DIR *d = opendir(dirs[i].c_str());
			if (d == NULL)
				continue;
			struct dirent *e;
			while ((e = readdir(d)) != NULL)
				if (std::strncmp(e->d_name, "restore.", 8) == 0 || std::strncmp(e->d_name, ".restore-", 9) == 0)
					out.push_back(dirs[i] + "/" + e->d_name);
			closedir(d);
		}
		return out;
	}

	// An archive in the upload directory holding the named paths as tar stores them.
	bool archive(const std::string &name, const std::string &paths, const std::string &dir = "/")
	{
		const std::string cmd = "tar -czf '" + box.at(name) + "' -C '" + dir + "' " + paths + " 2>/dev/null";
		return std::system(cmd.c_str()) == 0;
	}

	/* A plain tar built entry by entry, the way an archive built by hand can
	   carry what tar itself would never store: a name that climbs, a newline in
	   a name, a long name or a pax record naming another place than the header. */
	struct RawEntry
	{
		std::string name;
		char        type;
		std::string data;
	};

	static std::string rawHeader(const std::string &name, char type, size_t size,
				     const std::string &prefix = std::string())
	{
		char h[512];
		std::memset(h, 0, sizeof(h));
		std::memcpy(h, name.data(), name.size() < 100 ? name.size() : 99);
		std::memcpy(h + 345, prefix.data(), prefix.size() < 155 ? prefix.size() : 154);
		std::memcpy(h + 100, "0000644", 7);
		std::memcpy(h + 108, "0000000", 7);
		std::memcpy(h + 116, "0000000", 7);
		std::snprintf(h + 124, 12, "%011lo", (unsigned long) size);
		std::memcpy(h + 136, "00000000000", 11);
		h[156] = type;
		std::memcpy(h + 257, "ustar", 6);
		std::memcpy(h + 263, "00", 2);
		std::memset(h + 148, ' ', 8);
		unsigned sum = 0;
		for (size_t i = 0; i < sizeof(h); ++i)
			sum += (unsigned char) h[i];
		std::snprintf(h + 148, 8, "%06o", sum);
		h[155] = ' ';
		return std::string(h, sizeof(h));
	}

	void rawTarOf(const std::string &name, const std::vector<RawEntry> &entries,
		      const std::string &prefix = std::string())
	{
		std::string out;
		for (size_t i = 0; i < entries.size(); ++i)
		{
			out += rawHeader(entries[i].name, entries[i].type, entries[i].data.size(), prefix);
			std::string data = entries[i].data;
			data.resize(((data.size() + 511) / 512) * 512, '\0');
			out += data;
		}
		out += std::string(1024, '\0');
		FILE *f = std::fopen(box.at(name).c_str(), "wb");
		if (f != NULL)
		{
			std::fwrite(out.data(), 1, out.size(), f);
			std::fclose(f);
		}
	}

	void rawTar(const std::string &name, const std::string &member)
	{
		std::vector<RawEntry> one;
		RawEntry e = { member, '0', "x\n" };
		one.push_back(e);
		rawTarOf(name, one);
	}

	// A pax record as tar writes one: its own length first, the newline counted.
	static std::string paxRecord(const std::string &key, const std::string &value)
	{
		const std::string body = " " + key + "=" + value + "\n";
		size_t len = body.size() + 1;
		while (std::to_string(len).size() + body.size() != len)
			++len;
		return std::to_string(len) + body;
	}

	private:
		BackupFixture(const BackupFixture &);
		BackupFixture &operator=(const BackupFixture &);
};

/* The packet a wake would send, caught before it reaches a socket, so no case
   broadcasts on the network of the machine it runs on. */
std::vector<unsigned char> &wakePackets()
{
	static std::vector<unsigned char> sent;
	return sent;
}

bool wakeSendsOk = true;

bool catchWake(const unsigned char *packet, size_t length)
{
	wakePackets().assign(packet, packet + length);
	return wakeSendsOk;
}

struct WakeFixture
{
	WakeFixture()
	{
		wakePackets().clear();
		wakeSendsOk = true;
		coreapi::network::internal::setWakeSenderForTest(&catchWake);
	}
	~WakeFixture() { coreapi::network::internal::setWakeSenderForTest(NULL); }
};

/* The web interface's own settings file, one of the case's own, with what a box
   ships in it and a key that is not read back. */
struct WebuiFixture
{
	Sandbox     box;
	std::string path;
	std::string former;
	const char *before;
	const char *before_former;

	WebuiFixture() : path(box.at("webui.conf")), former(box.at("yWeb.conf")),
		before(coreapi::webui::internal::config_path),
		before_former(coreapi::webui::internal::former_path)
	{
		coreapi::webui::internal::config_path = path.c_str();
		// the file taken over is the case's own, and there is none unless it writes one
		coreapi::webui::internal::former_path = former.c_str();
		FILE *f = std::fopen(path.c_str(), "w");
		if (f != NULL)
		{
			std::fputs("start_page=none\nyweb_box_name=${HOSTNAME}\ntmdb_api_key=SECRETVALUE\n", f);
			std::fclose(f);
		}
	}

	~WebuiFixture()
	{
		coreapi::webui::internal::config_path = before;
		coreapi::webui::internal::former_path = before_former;
	}

	std::string file() const
	{
		std::string out;
		FILE *f = std::fopen(path.c_str(), "r");
		if (f == NULL)
			return out;
		char buf[512];
		size_t n;
		while ((n = std::fread(buf, 1, sizeof(buf), f)) > 0)
			out.append(buf, n);
		std::fclose(f);
		return out;
	}

	private:
		WebuiFixture(const WebuiFixture &);
		WebuiFixture &operator=(const WebuiFixture &);
};

/* The default list and the movie browser's file go with the roots, because the routes
   below put the roots together again out of both before they look at a name. Left as
   they ship, what the sandbox admits would be the media of whatever machine this runs
   on rather than the one directory the case made. */
struct RootsFixture
{
	Sandbox     box;
	std::string browser;
	std::string before;
	const char *before_defaults;
	const char *before_browser;

	RootsFixture()
		: browser(box.at("moviebrowser.conf")),
		  before(coreapi::storage::roots()),
		  before_defaults(coreapi::storage::internal::default_roots),
		  before_browser(coreapi::storage::internal::moviebrowser_config_path)
	{
		coreapi::storage::internal::default_roots = box.dir.c_str();
		coreapi::storage::internal::moviebrowser_config_path = browser.c_str();
		coreapi::storage::setRoots(box.dir);
	}

	~RootsFixture()
	{
		coreapi::storage::setRoots(before);
		coreapi::storage::internal::default_roots = before_defaults;
		coreapi::storage::internal::moviebrowser_config_path = before_browser;
	}

	private:
		RootsFixture(const RootsFixture &);
		RootsFixture &operator=(const RootsFixture &);
};

// An empty directory where the process table is read, so a daemon answers the
// same whatever else is running on the machine this is built on.
struct ProcFixture
{
	Sandbox     box;
	std::string before;

	ProcFixture() : before(coreapi::daemons::procRoot())
	{
		coreapi::daemons::setProcRoot(box.dir);
	}

	~ProcFixture() { coreapi::daemons::setProcRoot(before); }

	private:
		ProcFixture(const ProcFixture &);
		ProcFixture &operator=(const ProcFixture &);
};

/* A key the program declares, found in the schema rather than written out here,
   so that a row renamed in the tables fails this as the missing row it is
   instead of as a write that was refused. */
std::string secretKeyIn(const char *section)
{
	coreapi::Result<std::vector<coreapi::Descriptor> > got = coreapi::settings::schema();
	REQUIRE(got.ok());
	const std::vector<coreapi::Descriptor> rows = std::move(got).value();
	for (size_t i = 0; i < rows.size(); ++i)
	{
		if (rows[i].section == NULL || std::string(section) != rows[i].section)
			continue;
		if (!rows[i].secret || rows[i].type != coreapi::ValueType::String)
			continue;
		return rows[i].key;
	}
	return std::string();
}

std::string plainKeyIn(const char *section)
{
	coreapi::Result<std::vector<coreapi::Descriptor> > got = coreapi::settings::schema();
	REQUIRE(got.ok());
	const std::vector<coreapi::Descriptor> rows = std::move(got).value();
	for (size_t i = 0; i < rows.size(); ++i)
	{
		if (rows[i].section == NULL || std::string(section) != rows[i].section)
			continue;
		if (rows[i].secret || rows[i].type != coreapi::ValueType::String)
			continue;
		// a text the box offers a list for takes nothing but its list
		if (rows[i].choices_from != NULL)
			continue;
		return rows[i].key;
	}
	return std::string();
}

std::string messageOfBytes(size_t n)
{
	std::string out = "{\"text\":\"";
	out.append(n, 'x');
	out += "\"}";
	return out;
}

std::string timerBody(time_t start)
{
	char buf[64];
	std::snprintf(buf, sizeof(buf), "%lld", (long long) start);
	std::string out = "{\"kind\":\"record\",\"channel_id\":\"2b66\",\"title\":\"the news\",\"start\":";
	out += buf;
	std::snprintf(buf, sizeof(buf), "%lld", (long long) (start + 3600));
	out += ",\"stop\":";
	out += buf;
	out += "}";
	return out;
}

// The route of that path and method, out of the tables the server ships, so a
// case holding an answer to a shape holds it to the shape the server declares.
const Endpoint *routeFor(Method m, const char *path)
{
	size_t tables = 0;
	const RouteTable *const *t = allRoutes(&tables);
	for (size_t i = 0; i < tables; ++i)
	{
		for (size_t j = 0; j < t[i]->count; ++j)
		{
			if (t[i]->endpoints[j].method == m && std::string(path) == t[i]->endpoints[j].path)
				return &t[i]->endpoints[j];
		}
	}
	return NULL;
}

} // namespace

TEST_CASE("login answers a cookie and a wrong password does not", "[write]")
{
	ShippedRoutes shipped;
	ConfigFixture cfg;

	const Reply bad = login(kUser, "nope");
	REQUIRE(bad.code == 401);
	REQUIRE(headerOf(bad, "Set-Cookie").empty());
	// Says what a yes is made of without saying which half of this one was
	// wrong, and names nothing this server has.
	REQUIRE(headerOf(bad, "WWW-Authenticate") == "Bearer");

	const Reply ok = login(kUser, kPassword);
	REQUIRE(ok.code == 200);

	const std::string c = headerOf(ok, "Set-Cookie");
	REQUIRE(c.find("HttpOnly") != std::string::npos);
	REQUIRE(c.find("SameSite=Lax") != std::string::npos);
	REQUIRE(c.find("Path=/") != std::string::npos);
	// A cookie marked Secure is never sent over a plain connection, and this
	// server has no TLS to offer, so one carrying it would be a session nothing
	// on this box could use.
	REQUIRE(c.find("Secure") == std::string::npos);

	/* The second token comes back in the body and not in the cookie. A token
	   the browser attaches by itself is attached to another site's requests as
	   well, which is the whole of what a second one is for. */
	REQUIRE(ok.body.find("\"csrf\"") != std::string::npos);
	const std::string csrf = stringField(ok.body, "csrf");
	REQUIRE_FALSE(csrf.empty());
	REQUIRE(c.find(csrf) == std::string::npos);
	// And the answer says which header to send it back in, so a page does not
	// carry the name written out in a second place.
	REQUIRE(stringField(ok.body, "csrf_header") == std::string(csrfHeaderName()));

	const size_t eq = c.find('=');
	const size_t end = c.find(';');
	REQUIRE(eq != std::string::npos);
	REQUIRE(end > eq);
	const std::string token = c.substr(eq + 1, end - eq - 1);
	REQUIRE(csrfFor(token) == csrf);
	REQUIRE(closeSession(token));
}

/* The route that opens a session and the route that describes one answer one
   document, and these three are what holds them to it.

   What they are worth is what they replace: opening a session used to answer three
   members, so a caller that had just logged in still had to ask a second route what
   it was allowed to do, and a page showed a session that may write as one that may
   only read for as long as that second answer took to arrive. */
TEST_CASE("opening a session answers the shape the session route answers", "[write]")
{
	ShippedRoutes shipped;
	ConfigFixture cfg;

	const Endpoint *opened = routeFor(Post, "/api/v1/login");
	const Endpoint *described = routeFor(Get, "/api/v1/session");
	REQUIRE(opened != NULL);
	REQUIRE(described != NULL);
	REQUIRE(opened->schema != NULL);
	/* One shape and not two that agree today. Two rows a person compared by
	   reading them are two rows that come apart the next time one of them is
	   edited, and a caller generating itself from the document would carry two
	   types for one thing. */
	REQUIRE(opened->schema == described->schema);

	const Reply ok = login(kUser, kPassword);
	REQUIRE(ok.code == 200);
	const std::string token = tokenOf(ok);

	const ::Json::Value root = parsed(ok.body);
	REQUIRE(root.isObject());

	/* Both directions, which is the whole of what a shape is worth: every
	   member the document promises has to be there, and a member the answer
	   carries that the document does not name would be one a caller is never
	   told about. */
	const Schema &shape = *opened->schema;
	for (size_t i = 0; i < shape.count; ++i)
	{
		INFO(shape.fields[i].name);
		REQUIRE(root.isMember(shape.fields[i].name));
	}
	REQUIRE(root.getMemberNames().size() == shape.count);

	REQUIRE(root["authenticated"].asBool() == true);
	REQUIRE(root["user"].asString() == std::string(kUser));
	REQUIRE(root["csrf"].asString() == csrfFor(token));
	REQUIRE(root["csrf_header"].asString() == std::string(csrfHeaderName()));

	/* Counted down and not handed out as a moment, and inside what the
	   configuration allows, so a member answering nought or answering the
	   moment the session started would fail here rather than read as a
	   plausible number. It is in this answer because knowing when to renew is a
	   question a caller has the moment it logs in. */
	const long left = (long) root["expires_in"].asInt64();
	REQUIRE(left > 0);
	REQUIRE(left <= (long) config().session_lifetime_s);

	REQUIRE(closeSession(token));
}

TEST_CASE("the level a login answers is what the session it opened is worth", "[write]")
{
	/* And never what the request that asked was granted. That request arrived
	   carrying nothing and was Public, and a caller told public a moment after it
	   logged in would go and ask for a credential it is already holding.

	   The member is constant today and is in the answer anyway: a caller has no
	   business holding the rule that a live session is granted the highest level. */
	ShippedRoutes shipped;
	ConfigFixture cfg;

	const Reply ok = login(kUser, kPassword);
	REQUIRE(ok.code == 200);
	const std::string token = tokenOf(ok);

	const std::string said = stringField(ok.body, "level");
	REQUIRE(said == std::string(authLevelName(AuthLevel::System)));
	// Written out a second time rather than read off the server alone, because
	// the name in the answer is the name the document uses and a page holds one
	// against the other.
	REQUIRE(said == "system");
	// What the asking request was granted, which is what an answer describing
	// the request instead of the session it made would have said.
	REQUIRE(said != std::string(authLevelName(AuthLevel::Public)));

	REQUIRE(closeSession(token));
}

TEST_CASE("a login and the session route describe one session the same way", "[write]")
{
	/* Member for member, which is what one writer means and what two writers
	   could not be held to: a member answered by one of them and not the other
	   is exactly the fault these cases exist for, and comparing the two answers
	   catches it whichever of them is the one that changed. */
	ShippedRoutes shipped;
	ConfigFixture cfg;

	const Reply ok = login(kUser, kPassword);
	REQUIRE(ok.code == 200);
	const std::string token = tokenOf(ok);

	// The same session, asked about through the other route, at the level a
	// request carrying it is granted.
	const Reply asked = replyOf(dispatch(Get, "/api/v1/session", "", "", kPeer,
	                                     sessionLevel(token), std::string(), token));
	REQUIRE(asked.code == 200);

	const ::Json::Value a = parsed(ok.body);
	const ::Json::Value b = parsed(asked.body);

	const Endpoint *opened = routeFor(Post, "/api/v1/login");
	REQUIRE(opened != NULL);
	REQUIRE(opened->schema != NULL);
	const Schema &shape = *opened->schema;
	REQUIRE(shape.count > 0);

	for (size_t i = 0; i < shape.count; ++i)
	{
		const std::string name = shape.fields[i].name;
		INFO(name);
		REQUIRE(a.isMember(name));
		REQUIRE(b.isMember(name));
		if (name == "expires_in")
		{
			/* The one member that is a countdown and not a fact: a second can
			   pass between the two answers, so the later one is the smaller and
			   both are still a session that has time left. */
			REQUIRE(b[name].asInt64() > 0);
			REQUIRE(b[name].asInt64() <= a[name].asInt64());
			continue;
		}
		REQUIRE(a[name] == b[name]);
	}

	REQUIRE(closeSession(token));
}

TEST_CASE("a login says nothing about which half was wrong", "[write]")
{
	ShippedRoutes shipped;
	ConfigFixture cfg;

	const Reply wrong_name = login("nobody at all", kPassword);
	const Reply wrong_secret = login(kUser, "nope");

	REQUIRE(wrong_name.code == wrong_secret.code);
	// Word for word, because a sentence that differed would answer the question
	// the code above refuses to answer.
	REQUIRE(wrong_name.body == wrong_secret.body);
	REQUIRE(wrong_name.body.find("nobody at all") == std::string::npos);
}

TEST_CASE("a wrong password takes as long as a right one", "[write]")
{
	/* A comparison that returns early tells an attacker how much of the secret
	   they have, and a name checked before the derivation tells them which
	   names exist. This is a coarse check and it says so: it fails only on a
	   difference large enough to be usable, which is the only kind that can be
	   measured across a network in any case. */
	ShippedRoutes shipped;
	ConfigFixture cfg;

	const double right = bestCpuSecondsOf(&manyRightLogins);
	const double wrong = bestCpuSecondsOf(&manyWrongLogins);
	const double name = bestCpuSecondsOf(&manyWrongNames);

	INFO("right " << right << " wrong " << wrong << " unknown name " << name);
	REQUIRE(right > 0.0);
	REQUIRE(wrong > right * 0.5);
	REQUIRE(wrong < right * 2.0);
	// The half that is easy to get wrong: a name nobody has must cost what a
	// name somebody has costs, or the box answers whether an account exists.
	REQUIRE(name > right * 0.5);
	REQUIRE(name < right * 2.0);
}

TEST_CASE("logging out drops the session the request arrived with", "[write]")
{
	ShippedRoutes shipped;
	ConfigFixture cfg;

	const Reply in = login(kUser, kPassword);
	REQUIRE(in.code == 200);
	const std::string c = headerOf(in, "Set-Cookie");
	const std::string token = c.substr(c.find('=') + 1, c.find(';') - c.find('=') - 1);
	REQUIRE_FALSE(csrfFor(token).empty());

	/* The token travels beside the request and is never named in what was sent:
	   a route taking the session to close out of a body would close anybody's
	   session for whoever can guess a token. */
	const Response out = dispatch(Post, "/api/v1/logout", "", "", "127.0.0.1",
	                              AuthLevel::System, std::string(), token);
	REQUIRE(out.code == 204);
	REQUIRE(csrfFor(token).empty());

	// And the browser is told to forget it, so a caller that logged out does
	// not go on presenting a token that names nothing.
	const Reply gone = replyOf(out);
	REQUIRE(headerOf(gone, "Set-Cookie").find("Max-Age=0") != std::string::npos);
}

/* The session clock is moved a year ahead of the real one, so the gate's own walk of
   the table, which reads the real clock, never takes a session these cases hold. */
struct SessionClock
{
	time_t at;
	SessionClock() : at(time(NULL) + 365 * 24 * 60 * 60) { setSessionClockForTest(at); }
	~SessionClock() { setSessionClockForTest(0); }
	void move(time_t by) { at += by; setSessionClockForTest(at); }
};

TEST_CASE("a request a session carries moves its end on, and never past thirty days", "[write]")
{
	ConfigFixture cfg;
	WebConfig c = config();
	c.session_lifetime_s = 10 * 24 * 60 * 60;
	setConfigForTest(c);
	SessionClock clock;

	const std::string token = openSession(kUser);
	REQUIRE_FALSE(token.empty());
	REQUIRE(sessionExpiresIn(token) == 10 * 24 * 60 * 60);

	Credentials cred;
	cred.peer = "203.0.113.5";
	cred.cookie_token = token;

	// Five days in, a request: ten days again from here.
	clock.move(5 * 24 * 60 * 60);
	REQUIRE(granted(cred) == AuthLevel::System);
	REQUIRE(sessionExpiresIn(token) == 10 * 24 * 60 * 60);

	// Asking how long it has left is not a request it carried.
	clock.move(24 * 60 * 60);
	REQUIRE(sessionExpiresIn(token) == 9 * 24 * 60 * 60);

	// Busy every few days, it still ends thirty days after it was opened.
	for (int day = 6; day < 25; day += 4)
	{
		clock.move(4 * 24 * 60 * 60);
		REQUIRE(granted(cred) == AuthLevel::System);
	}
	// Opened at day 0, now day 26: four days left however busy it is.
	REQUIRE(sessionExpiresIn(token) == 4 * 24 * 60 * 60);
	clock.move(4 * 24 * 60 * 60);
	REQUIRE(granted(cred) == AuthLevel::Public);
	REQUIRE_FALSE(sessionIsLive(token));

	// Left alone for its lifetime, a session is gone without a request.
	const std::string idle = openSession(kUser);
	clock.move(10 * 24 * 60 * 60);
	REQUIRE_FALSE(sessionIsLive(idle));
}

TEST_CASE("the session table holds sixty four and gives up the one running out soonest", "[write]")
{
	ConfigFixture cfg;
	SessionClock clock;
	// Whatever earlier cases left open goes first, so the count below is this case's.
	expireSessions(std::numeric_limits<time_t>::max());
	REQUIRE(sessionCount() == 0);

	std::vector<std::string> tokens;
	for (int i = 0; i < 64; ++i)
	{
		tokens.push_back(openSession(kUser));
		REQUIRE_FALSE(tokens.back().empty());
		clock.move(1);
	}
	REQUIRE(sessionCount() == 64);

	const std::string newest = openSession(kUser);
	REQUIRE_FALSE(newest.empty());
	REQUIRE(sessionCount() == 64);
	REQUIRE_FALSE(sessionIsLive(tokens[0]));
	REQUIRE(sessionIsLive(tokens[1]));
	REQUIRE(sessionIsLive(newest));

	expireSessions(std::numeric_limits<time_t>::max());
}

TEST_CASE("a media token drawn through a session ends with that session", "[write]")
{
	ConfigFixture cfg;
	SessionClock clock;

	Credentials player;
	player.peer = "203.0.113.5";

	// Signing out ends it.
	const std::string signed_out = openSession(kUser);
	player.query_token = openScopedToken(mediaScopeName(), AuthLevel::System, signed_out);
	REQUIRE_FALSE(player.query_token.empty());
	REQUIRE(granted(player, true) == AuthLevel::System);
	REQUIRE(closeSession(signed_out));
	REQUIRE(granted(player, true) == AuthLevel::Public);

	// So does running out, whether or not the table was walked since.
	const std::string ran_out = openSession(kUser);
	player.query_token = openScopedToken(mediaScopeName(), AuthLevel::System, ran_out);
	REQUIRE(granted(player, true) == AuthLevel::System);
	clock.move(config().session_lifetime_s);
	REQUIRE(granted(player, true) == AuthLevel::Public);

	// A session that is gone draws nothing.
	REQUIRE(openScopedToken(mediaScopeName(), AuthLevel::System, signed_out).empty());

	// Drawn without a session, it lives the few hours every one does.
	const std::string bare = openScopedToken(mediaScopeName(), AuthLevel::System);
	REQUIRE_FALSE(bare.empty());
	player.query_token = bare;
	REQUIRE(granted(player, true) == AuthLevel::System);
}

TEST_CASE("a media token for a caller a bearer let in is not tied to a dead cookie", "[write]")
{
	/* The cookie arrives beside the bearer whether or not it is still a session.
	   The token is tied to the session only when that session is what let the
	   caller in; a dead one beside a bearer would otherwise refuse the token. */
	ShippedRoutes shipped;
	ConfigFixture cfg;

	const std::string dead = openSession(kUser);
	REQUIRE(closeSession(dead));

	const Response out = dispatch(Post, "/api/v1/token/media", "", "", "127.0.0.1",
	                              AuthLevel::System, std::string(), dead);
	REQUIRE(out.code == 200);
	REQUIRE_FALSE(stringField(replyOf(out).body, "token").empty());
}

TEST_CASE("a cookie alone does not carry a request that changes something", "[write]")
{
	/* The gate, driven with what a browser would send. A cookie is attached by
	   the browser to whatever asks for it, another site's page included, so the
	   cookie says who the caller is and the second token says who asked. */
	ShippedRoutes shipped;
	ConfigFixture cfg;

	const Reply in = login(kUser, kPassword);
	REQUIRE(in.code == 200);
	const std::string c = headerOf(in, "Set-Cookie");
	const std::string token = c.substr(c.find('=') + 1, c.find(';') - c.find('=') - 1);
	const std::string csrf = stringField(in.body, "csrf");

	Credentials cred;
	cred.peer = "192.168.1.9";
	cred.cookie_token = token;

	REQUIRE(granted(cred) == AuthLevel::System);

	Response refusal;
	// A read goes through on the cookie alone, which is what makes the page
	// work at all.
	REQUIRE(allowed(AuthLevel::Read, AuthLevel::System, Get, cred, &refusal));
	// A change does not.
	REQUIRE_FALSE(allowed(AuthLevel::Write, AuthLevel::System, Post, cred, &refusal));
	REQUIRE(refusal.code == 403);

	cred.csrf_header = csrf;
	REQUIRE(allowed(AuthLevel::Write, AuthLevel::System, Post, cred, &refusal));

	// A token that is not the one this session was given is no token at all.
	cred.csrf_header = std::string(csrf.size(), 'a');
	REQUIRE_FALSE(allowed(AuthLevel::Write, AuthLevel::System, Post, cred, &refusal));

	REQUIRE(closeSession(token));
}

TEST_CASE("every route that changes something asks for more than a read", "[write]")
{
	/* The one thing that must never regress, walked over the real tables rather than
	   over a list written here. A caller on the box's own network is granted a read
	   and is asked for no credential, so a route that changed something at that level
	   would be one any page a browser on that network visits could reach in that
	   browser's name. */
	ShippedRoutes shipped;

	size_t tables = 0;
	const RouteTable *const *t = allRoutes(&tables);
	size_t changing = 0;
	for (size_t i = 0; i < tables; ++i)
	{
		for (size_t j = 0; j < t[i]->count; ++j)
		{
			const Endpoint &e = t[i]->endpoints[j];
			if (e.method == Get || e.method == Head || e.method == Options)
				continue;
			INFO(e.path);
			if (std::string(e.path) == "/api/v1/login")
			{
				// The single exception, and the same one the check over the
				// tables makes: being reached grants nothing.
				REQUIRE(e.auth == AuthLevel::Public);
				continue;
			}
			REQUIRE((e.auth == AuthLevel::Write || e.auth == AuthLevel::System));
			++changing;
		}
	}
	// A walk that found nothing would pass this whatever the tables held, and a
	// route added at a level below a write has to move this number by hand.
	REQUIRE(changing == 68);
}

TEST_CASE("a caller on the local network cannot zap, reboot or write a setting", "[write]")
{
	ShippedRoutes shipped;
	BoxFixture box;

	REQUIRE(lanPost("/api/v1/zap", "{\"channel_id\":\"2b66\"}").code == 403);
	REQUIRE(lanPost("/api/v1/system/reboot", "{}").code == 403);
	REQUIRE(lanPatch("/api/v1/settings/audio", "{\"audio_volume_percent_ac3\":\"3\"}").code == 403);
	REQUIRE(lanPost("/api/v1/timers", timerBody(box.timers.clock + 3600)).code == 403);
	REQUIRE(lanPost("/api/v1/osd/message", "{\"text\":\"hello\"}").code == 403);

	// Nothing reached the box: a refusal that posted the command first would
	// answer 403 and change the channel anyway.
	REQUIRE(box.commands.posted.empty());
	REQUIRE(box.events.sent.empty());
	REQUIRE(box.timers.timers.empty());
}

TEST_CASE("a write asking for the box's own credential is refused a write", "[write]")
{
	// The other half of the rule above: a caller granted a write is still not
	// one that may take the box off the air or empty a credential.
	ShippedRoutes shipped;
	BoxFixture box;

	REQUIRE(send(Post, "/api/v1/system/reboot", "{}", AuthLevel::Write, "127.0.0.1").code == 403);
	REQUIRE(send(Post, "/api/v1/daemons/samba/start", "{}", AuthLevel::Write, "127.0.0.1").code == 403);
	REQUIRE(send(Delete, "/api/v1/storage/path?path=/tmp/x", "", AuthLevel::Write, "127.0.0.1").code == 403);
	REQUIRE(send(Patch, "/api/v1/settings/audio", "{\"audio_volume_percent_ac3\":\"3\"}", AuthLevel::Write, "127.0.0.1").code == 403);
	REQUIRE(send(Post, "/api/v1/plugins/tuxtxt/start", "{}", AuthLevel::Write, "127.0.0.1").code == 403);
	REQUIRE(box.events.sent.empty());

	// The level these ask for still reaches them.
	REQUIRE(authedPost("/api/v1/plugins/tuxtxt/start", "{}").code == 202);
	REQUIRE(box.events.sent.size() == 1u);

	// And a write is enough for the routes that ask for one.
	REQUIRE(send(Post, "/api/v1/zap", "{\"channel_id\":\"2b66\"}", AuthLevel::Write, "127.0.0.1").code == 202);
}

TEST_CASE("a state change answers accepted and not done", "[write]")
{
	/* The box is asked on its own loop and nothing comes back from what does
	   it, so the answer says the box was asked. Standby off while not in
	   standby is accepted and changes nothing, which is exactly the thing a 200
	   here would be claiming otherwise. */
	ShippedRoutes shipped;
	BoxFixture box;

	REQUIRE(authedPost("/api/v1/zap", "{\"channel_id\":\"2b66\"}").code == 202);
	REQUIRE(authedPost("/api/v1/system/standby", "{\"on\":true}").code == 202);
	REQUIRE(authedPost("/api/v1/system/reboot", "{}").code == 202);
	REQUIRE(authedPost("/api/v1/mode", "{\"mode\":\"radio\"}").code == 202);
	REQUIRE(authedPut("/api/v1/osd/volume", "{\"percent\":30}").code == 202);
	REQUIRE(authedPut("/api/v1/osd/mute", "{\"on\":true}").code == 202);

	// Each of them reached the box, so the code above is not what a route that
	// did nothing would answer either.
	REQUIRE(box.commands.posted.size() == 2);
	REQUIRE(box.events.sent.size() == 4);
}

TEST_CASE("a zap to a channel the box does not have posts nothing", "[write]")
{
	ShippedRoutes shipped;
	BoxFixture box;

	const Reply r = authedPost("/api/v1/zap", "{\"channel_id\":\"dead\"}");
	REQUIRE(r.code == 404);
	REQUIRE(r.body.find("no-such-channel") != std::string::npos);
	// A command is posted and forgotten, so a zap to an id nobody has would
	// otherwise be accepted and change nothing with nothing to say it.
	REQUIRE(box.commands.posted.empty());
}

TEST_CASE("a zap or a mode change in standby is refused unless it may switch the box on", "[write]")
{
	ShippedRoutes shipped;
	BoxFixture box;
	box.channels.mode = NeutrinoModes::mode_standby;

	const Reply zap = authedPost("/api/v1/zap", "{\"channel_id\":\"2b66\"}");
	REQUIRE(zap.code == 409);
	REQUIRE(zap.body.find("/errors/box-in-standby") != std::string::npos);
	REQUIRE(authedPost("/api/v1/zap", "{\"channel_id\":\"2b66\",\"wake\":false}").code == 409);

	const Reply mode = authedPost("/api/v1/mode", "{\"mode\":\"radio\"}");
	REQUIRE(mode.code == 409);
	REQUIRE(mode.body.find("/errors/box-in-standby") != std::string::npos);
	REQUIRE(authedPost("/api/v1/mode", "{\"mode\":\"radio\",\"wake\":false}").code == 409);

	REQUIRE(box.commands.posted.empty());

	// One auth level per route, flag or not.
	REQUIRE(send(Post, "/api/v1/zap", "{\"channel_id\":\"2b66\",\"wake\":true}",
		     AuthLevel::Write, "127.0.0.1").code == 202);
	REQUIRE(send(Post, "/api/v1/mode", "{\"mode\":\"radio\",\"wake\":true}",
		     AuthLevel::Write, "127.0.0.1").code == 202);
	REQUIRE(box.commands.posted.size() == 2);
	REQUIRE(box.commands.posted[0].first == NeutrinoMessages::ZAPTO);
	REQUIRE(box.commands.posted[1].first == NeutrinoMessages::CHANGEMODE);
	REQUIRE((box.commands.posted[1].second & NeutrinoModes::mode_mask) == (neutrino_msg_data_t) NeutrinoModes::mode_radio);
	delete[] (unsigned char *) box.commands.posted[0].second;
}

TEST_CASE("wake is read as a yes or a no", "[write]")
{
	ShippedRoutes shipped;
	BoxFixture box;
	box.channels.mode = NeutrinoModes::mode_standby;

	const Reply zap = authedPost("/api/v1/zap", "{\"channel_id\":\"2b66\",\"wake\":\"please\"}");
	REQUIRE(zap.code == 400);
	REQUIRE(zap.body.find("/errors/bad-bool") != std::string::npos);
	REQUIRE(authedPost("/api/v1/mode", "{\"mode\":\"tv\",\"wake\":\"please\"}").code == 400);
	REQUIRE(box.commands.posted.empty());
}

TEST_CASE("a zap a recording holds the tuner for answers a conflict and posts nothing", "[write]")
{
	ShippedRoutes shipped;
	BoxFixture box;
	box.channels.mode = NeutrinoModes::mode_tv;
	box.channels.zap_possible = false;

	const Reply r = authedPost("/api/v1/zap", "{\"channel_id\":\"2b66\"}");
	REQUIRE(r.code == 409);
	REQUIRE(r.body.find("/errors/recording-holds-tuner") != std::string::npos);
	REQUIRE(box.commands.posted.empty());
}

TEST_CASE("a zap while a file plays is refused unless it may end the playback", "[write]")
{
	ShippedRoutes shipped;
	BoxFixture box;
	box.channels.mode = NeutrinoModes::mode_ts;
	coreapi::archive::notePlaying("/media/hdd/movie/one.ts");

	const Reply r = authedPost("/api/v1/zap", "{\"channel_id\":\"2b66\"}");
	const Reply no = authedPost("/api/v1/zap", "{\"channel_id\":\"2b66\",\"stop_playback\":false}");
	const Reply bad = authedPost("/api/v1/zap", "{\"channel_id\":\"2b66\",\"stop_playback\":\"please\"}");
	const size_t before = box.commands.posted.size();
	const Reply yes = authedPost("/api/v1/zap", "{\"channel_id\":\"2b66\",\"stop_playback\":true}");
	// A mode change leaves the movie player playing, so it asks nothing.
	const Reply mode = authedPost("/api/v1/mode", "{\"mode\":\"tv\"}");
	coreapi::archive::notePlaying(std::string());

	REQUIRE(r.code == 409);
	REQUIRE(r.body.find("/errors/playback-running") != std::string::npos);
	REQUIRE(no.code == 409);
	REQUIRE(bad.code == 400);
	REQUIRE(before == 0);
	REQUIRE(yes.code == 202);
	REQUIRE(mode.code == 202);
	REQUIRE(box.commands.posted.size() == 2);
	REQUIRE(box.commands.posted[0].first == NeutrinoMessages::ZAPTO);
	delete[] (unsigned char *) box.commands.posted[0].second;
}

TEST_CASE("a settings write reaches the layer and a refused one says which rule", "[write]")
{
	ShippedRoutes shipped;
	BoxFixture box;

	const Reply ok = authedPatch("/api/v1/settings/audio", "{\"audio_volume_percent_ac3\":\"50\"}");
	REQUIRE(ok.code == 200);
	// It reached the store rather than being answered here.
	REQUIRE(box.store.ints["audio_volume_percent_ac3"] == 50);
	REQUIRE(box.store.persisted == 1);
	// And the answer is the section as it reads now, which is what a caller
	// would get by asking.
	REQUIRE(authedGet("/api/v1/settings/audio").body == ok.body);

	const Reply bad = authedPatch("/api/v1/settings/audio", "{\"audio_volume_percent_ac3\":\"5000\"}");
	REQUIRE(bad.code == 400);
	REQUIRE(bad.body.find("out-of-range") != std::string::npos);
	// Refused before the store was asked, so the value the box was running on
	// is what it is still running on.
	REQUIRE(box.store.ints["audio_volume_percent_ac3"] == 50);

	const Reply none = authedPatch("/api/v1/settings/audio", "{\"there_is_no_such_key\":\"1\"}");
	REQUIRE(none.code == 404);
	REQUIRE(none.body.find("no-such-setting") != std::string::npos);

	// A section nobody declared is that, and not a page whose every setting is
	// unknown.
	const Reply nowhere = authedPatch("/api/v1/settings/no-such-page", "{\"audio_volume_percent_ac3\":\"50\"}");
	REQUIRE(nowhere.code == 404);
	REQUIRE(nowhere.body.find("no-such-name") != std::string::npos);

	/* A key declared on another page is refused as one this page does not
	   declare. A second code for it would tell a caller which keys exist
	   elsewhere on the box, one guess at a time. */
	const Reply elsewhere = authedPatch("/api/v1/settings/audio", "{\"network_ntprefresh\":\"30\"}");
	REQUIRE(elsewhere.code == 404);
	REQUIRE(elsewhere.body.find("no-such-setting") != std::string::npos);
}

TEST_CASE("a settings write of several keys reports each key's outcome", "[write]")
{
	/* One bad key must not silently discard the good ones, and must not hide
	   that it was discarded either. A single code cannot say both. */
	ShippedRoutes shipped;
	BoxFixture box;

	const Reply r = authedPatch("/api/v1/settings/audio",
	                            "{\"audio_volume_percent_ac3\":\"50\",\"nope\":\"1\"}");
	REQUIRE(r.code == 207);
	REQUIRE(r.body.find("\"audio_volume_percent_ac3\":{\"status\":200}") != std::string::npos);
	REQUIRE(r.body.find("no-such-setting") != std::string::npos);

	// The good one landed rather than being dropped with the bad one.
	REQUIRE(box.store.ints["audio_volume_percent_ac3"] == 50);

	const ::Json::Value results = parsed(r.body)["results"];
	REQUIRE(results.isObject());
	REQUIRE(results.size() == 2);
	REQUIRE(results["audio_volume_percent_ac3"]["status"].asInt() == 200);
	REQUIRE(results["nope"]["status"].asInt() == 404);
	REQUIRE(results["nope"]["code"].asString() == "no-such-setting");

	// Every one of them landing is answered as one thing and not as a document
	// a caller has to walk to find out that nothing went wrong.
	const Reply all = authedPatch("/api/v1/settings/audio",
	                              "{\"audio_volume_percent_ac3\":\"40\",\"audio_volume_percent_pcm\":\"60\"}");
	REQUIRE(all.code == 200);
	REQUIRE(box.store.ints["audio_volume_percent_ac3"] == 40);
	REQUIRE(box.store.ints["audio_volume_percent_pcm"] == 60);
}

TEST_CASE("a settings write of nothing at all is refused rather than answered ok", "[write]")
{
	ShippedRoutes shipped;
	BoxFixture box;

	const Reply empty = authedPatch("/api/v1/settings/audio", "{}");
	REQUIRE(empty.code == 400);
	REQUIRE(empty.body.find("missing-parameter") != std::string::npos);

	const Reply nonsense = authedPatch("/api/v1/settings/audio", "[1,2]");
	REQUIRE(nonsense.code == 400);
	REQUIRE(nonsense.body.find("bad-string") != std::string::npos);

	const Reply nested = authedPatch("/api/v1/settings/audio", "{\"a\":{\"b\":1}}");
	REQUIRE(nested.code == 400);
	REQUIRE(nested.body.find("bad-string") != std::string::npos);

	REQUIRE(box.store.persisted == 0);
}

TEST_CASE("a credential cannot be emptied by writing nothing to it", "[write]")
{
	/* A form redrawn from a read sends an empty field back, because the read of
	   a credential answers nothing, and taking that would wipe the value the
	   read protected. */
	ShippedRoutes shipped;
	BoxFixture box;

	const std::string secret = secretKeyIn("network");
	REQUIRE_FALSE(secret.empty());
	box.store.strings[secret] = "the password";

	const Reply r = authedPatch("/api/v1/settings/network", "{\"" + secret + "\":\"\"}");
	REQUIRE(r.code == 400);
	REQUIRE(r.body.find("empty-credential") != std::string::npos);
	REQUIRE(box.store.strings[secret] == "the password");
}

TEST_CASE("clearing a credential is its own act", "[write]")
{
	/* The verb writing to it could not express: an explicit clear, so emptying
	   one is deliberate and a round trip still cannot do it by accident. */
	ShippedRoutes shipped;
	BoxFixture box;

	const std::string secret = secretKeyIn("network");
	REQUIRE_FALSE(secret.empty());
	box.store.strings[secret] = "the password";

	const Reply r = authedPost("/api/v1/settings/secret/clear", "{\"key\":\"" + secret + "\"}");
	REQUIRE(r.code == 200);
	REQUIRE(box.store.strings[secret].empty());
	REQUIRE(box.store.persisted == 1);
	// Answered in the shape one setting has in a section listing, which for a
	// credential is nothing: clearing one is not what makes it unreadable.
	REQUIRE(stringField(r.body, "id") == secret);
	REQUIRE(stringField(r.body, "value").empty());

	// And the rule it exists beside is still there afterwards.
	REQUIRE(authedPatch("/api/v1/settings/network", "{\"" + secret + "\":\"\"}").code == 400);
}

TEST_CASE("clearing refuses a row that is not a credential", "[write]")
{
	ShippedRoutes shipped;
	BoxFixture box;

	const std::string plain = plainKeyIn("network");
	REQUIRE_FALSE(plain.empty());
	box.store.strings[plain] = "keep me";

	const Reply r = authedPost("/api/v1/settings/secret/clear", "{\"key\":\"" + plain + "\"}");
	REQUIRE(r.code == 400);
	/* Its own answer and not the one for a key nothing declares: the key is
	   right, and telling a caller there is no such setting would send it
	   looking for a name it already has. */
	REQUIRE(r.body.find("not-a-credential") != std::string::npos);
	REQUIRE(box.store.strings[plain] == "keep me");

	const Reply unknown = authedPost("/api/v1/settings/secret/clear", "{\"key\":\"there_is_no_such_key\"}");
	REQUIRE(unknown.code == 404);
	REQUIRE(unknown.body.find("no-such-setting") != std::string::npos);

	// It is the most a route can ask for, because it writes a value the write
	// beside it refuses to write.
	const Endpoint *ep = routeFor(Post, "/api/v1/settings/secret/clear");
	REQUIRE(ep != NULL);
	REQUIRE(ep->auth == AuthLevel::System);
}

TEST_CASE("a timer round trip creates, changes and removes", "[write]")
{
	ShippedRoutes shipped;
	BoxFixture box;

	const Reply created = authedPost("/api/v1/timers", timerBody(box.timers.clock + 3600));
	REQUIRE(created.code == 201);
	const std::string id = stringField(created.body, "id");
	REQUIRE_FALSE(id.empty());
	REQUIRE(headerOf(created, "Location") == "/api/v1/timers/" + id);
	REQUIRE(box.timers.timers.size() == 1);

	char later[64];
	std::snprintf(later, sizeof(later), "{\"stop\":%lld}", (long long) (box.timers.clock + 9000));
	const Reply changed = authedPatch("/api/v1/timers/" + id, later);
	REQUIRE(changed.code == 200);
	// The change reached the daemon and the answer is read back from it rather
	// than written out of what was sent.
	REQUIRE(parsed(changed.body)["stop"].asInt64() == box.timers.clock + 9000);
	REQUIRE(box.timers.timers[0].stop == box.timers.clock + 9000);
	// What a correction left out is what it did not change, which is the whole
	// of the difference between a correction and a replacement.
	REQUIRE(box.timers.timers[0].channel_id == kChannel);
	REQUIRE(box.timers.timers[0].start == box.timers.clock + 3600);
	REQUIRE(box.timers.timers[0].title == "the news");

	REQUIRE(authedDelete("/api/v1/timers/" + id).code == 204);
	REQUIRE(box.timers.timers.empty());
	// The daemon is asked whether it holds the timer before the removal, so a
	// second one is the removal of a timer that is not there.
	const Reply again = authedDelete("/api/v1/timers/" + id);
	REQUIRE(again.code == 404);
	REQUIRE(again.body.find("no-such-timer") != std::string::npos);

	REQUIRE(authedPatch("/api/v1/timers/" + id, later).code == 404);
}

TEST_CASE("the two flags a recording carries are taken on the way in and nowhere else", "[write]")
{
	ShippedRoutes shipped;
	BoxFixture box;

	/* Left out, both are off. A caller that named a start and a stop named the
	   window it wants, and either flag has the daemon file the timer at other
	   times than the ones that were sent. */
	REQUIRE(authedPost("/api/v1/timers", timerBody(box.timers.clock + 3600)).code == 201);
	REQUIRE(box.timers.timers.size() == 1);
	REQUIRE_FALSE(box.timers.timers[0].recording_safety);
	REQUIRE_FALSE(box.timers.timers[0].auto_adjust);

	// Asked for, both reach the daemon, which applies them itself and is the
	// whole of what this route does with them.
	std::string asked = timerBody(box.timers.clock + 7200);
	asked.erase(asked.size() - 1);
	asked += ",\"recording_safety\":true,\"auto_adjust\":true}";
	REQUIRE(authedPost("/api/v1/timers", asked).code == 201);
	REQUIRE(box.timers.timers.size() == 2);
	REQUIRE(box.timers.timers[1].recording_safety);
	REQUIRE(box.timers.timers[1].auto_adjust);

	/* A change takes neither, and says so rather than taking one and dropping
	   it: the daemon's change command carries times, repeat and a directory, so
	   a route that took either would be one that answered ok and applied
	   neither. */
	const Reply changed = authedPatch("/api/v1/timers/1", "{\"recording_safety\":true}");
	REQUIRE(changed.code == 400);
	REQUIRE(changed.body.find("no-such-parameter") != std::string::npos);
	REQUIRE_FALSE(box.timers.timers[0].recording_safety);

	/* Nor does a read state either. The daemon's answer for a timer carries
	   neither, so a member for one would read false on every timer whatever was
	   asked for, the second of these two included. */
	const ::Json::Value items = parsed(authedGet("/api/v1/timers").body)["items"];
	REQUIRE(items.size() == 2);
	REQUIRE_FALSE(items[1].isMember("recording_safety"));
	REQUIRE_FALSE(items[1].isMember("auto_adjust"));
}

TEST_CASE("a timer in the past is refused with the reason", "[write]")
{
	ShippedRoutes shipped;
	BoxFixture box;

	const Reply r = authedPost("/api/v1/timers", timerBody(box.timers.clock - 3600));
	REQUIRE(r.code == 400);
	REQUIRE(r.body.find("timer-in-the-past") != std::string::npos);
	REQUIRE(box.timers.timers.empty());
}

TEST_CASE("a timer the daemon cannot make, change or let go of is refused with the reason", "[write]")
{
	ShippedRoutes shipped;
	BoxFixture box;

	char body[160];
	std::snprintf(body, sizeof(body), "{\"kind\":\"record\",\"start\":%lld,\"stop\":%lld}",
	              (long long) (box.timers.clock + 3600), (long long) (box.timers.clock + 7200));
	const Reply unchannelled = authedPost("/api/v1/timers", body);
	REQUIRE(unchannelled.code == 400);
	REQUIRE(unchannelled.body.find("timer-without-channel") != std::string::npos);

	box.timers.add_status = coreapi::Status::Conflict;
	const Reply twice = authedPost("/api/v1/timers", timerBody(box.timers.clock + 3600));
	REQUIRE(twice.code == 409);
	REQUIRE(twice.body.find("timer-exists") != std::string::npos);
	REQUIRE(box.timers.timers.empty());

	box.timers.add_status = coreapi::Status::Ok;
	REQUIRE(authedPost("/api/v1/timers", timerBody(box.timers.clock + 3600)).code == 201);
	std::snprintf(body, sizeof(body), "{\"stop\":%lld}", (long long) (box.timers.clock + 3600));
	const Reply empty = authedPatch("/api/v1/timers/1", body);
	REQUIRE(empty.code == 400);
	REQUIRE(empty.body.find("recording-without-duration") != std::string::npos);
	REQUIRE(box.timers.modifications == 0u);

	box.timers.ignore_removals = true;
	const Reply kept = authedDelete("/api/v1/timers/1");
	REQUIRE(kept.code == 500);
	REQUIRE(kept.body.find("timer-still-there") != std::string::npos);
	REQUIRE(box.timers.removals == 1u);
}

TEST_CASE("a change names nothing the daemon would drop", "[write]")
{
	ShippedRoutes shipped;
	BoxFixture box;

	REQUIRE(authedPost("/api/v1/timers", timerBody(box.timers.clock + 3600)).code == 201);
	char zap[160];
	std::snprintf(zap, sizeof(zap), "{\"kind\":\"zapto\",\"channel_id\":\"2b66\",\"start\":%lld}",
	              (long long) (box.timers.clock + 3600));
	REQUIRE(authedPost("/api/v1/timers", zap).code == 201);
	REQUIRE(box.timers.timers.size() == 2);

	const Reply titled = authedPatch("/api/v1/timers/1", "{\"title\":\"other\"}");
	REQUIRE(titled.code == 400);
	REQUIRE(titled.body.find("no-such-parameter") != std::string::npos);
	REQUIRE(box.timers.timers[0].title == "the news");

	const Reply standby = authedPatch("/api/v1/timers/1", "{\"standby_on\":true}");
	REQUIRE(standby.code == 400);
	REQUIRE(standby.body.find("no-such-parameter") != std::string::npos);
	REQUIRE_FALSE(box.timers.timers[0].standby_on);

	char later[64];
	std::snprintf(later, sizeof(later), "{\"stop\":%lld}", (long long) (box.timers.clock + 9000));
	const Reply ended = authedPatch("/api/v1/timers/2", later);
	REQUIRE(ended.code == 400);
	REQUIRE(ended.body.find("no-such-parameter") != std::string::npos);
	REQUIRE(box.timers.timers[1].stop == 0);
	REQUIRE(box.timers.modifications == 0);

	// Nought is what a zap holds, so it is taken.
	REQUIRE(authedPatch("/api/v1/timers/2", "{\"stop\":0}").code == 200);
	REQUIRE(box.timers.modifications == 1);

	REQUIRE(authedPatch("/api/v1/timers/1", later).code == 200);
	REQUIRE(box.timers.timers[0].stop == box.timers.clock + 9000);
}

TEST_CASE("a timer that keeps no end is refused one when it is made", "[write]")
{
	ShippedRoutes shipped;
	BoxFixture box;

	const char *const kinds[] = { "shutdown", "zapto", "standby", "remind", "sleeptimer", "exec-plugin" };
	const long long start = (long long) box.timers.clock + 3600;
	for (size_t i = 0; i < sizeof(kinds) / sizeof(kinds[0]); ++i)
	{
		INFO(kinds[i]);
		char body[200];
		std::snprintf(body, sizeof(body),
		              "{\"kind\":\"%s\",\"channel_id\":\"2b66\",\"start\":%lld,\"stop\":%lld}",
		              kinds[i], start, start + 3600);
		const Reply ended = authedPost("/api/v1/timers", body);
		REQUIRE(ended.code == 400);
		REQUIRE(ended.body.find("no-such-parameter") != std::string::npos);
		REQUIRE(box.timers.timers.empty());

		std::snprintf(body, sizeof(body),
		              "{\"kind\":\"%s\",\"channel_id\":\"2b66\",\"start\":%lld,\"stop\":0}",
		              kinds[i], start);
		REQUIRE(authedPost("/api/v1/timers", body).code == 201);
		REQUIRE(box.timers.timers.size() == 1);
		box.timers.timers.clear();
	}

	char now_one[200];
	std::snprintf(now_one, sizeof(now_one),
	              "{\"kind\":\"immediate-record\",\"channel_id\":\"2b66\",\"start\":%lld,\"stop\":%lld}",
	              (long long) box.timers.clock, (long long) box.timers.clock + 3600);
	REQUIRE(authedPost("/api/v1/timers", now_one).code == 201);
	REQUIRE(box.timers.timers.size() == 1);
	REQUIRE(box.timers.timers[0].stop == box.timers.clock + 3600);
	REQUIRE(authedPost("/api/v1/timers", timerBody(box.timers.clock + 7200)).code == 201);
	REQUIRE(box.timers.timers.size() == 2);
	REQUIRE(box.timers.timers[1].stop == box.timers.clock + 10800);
}

TEST_CASE("a one-off moved into the past is refused with the reason", "[write]")
{
	ShippedRoutes shipped;
	BoxFixture box;

	const time_t start = box.timers.clock + 3600;
	REQUIRE(authedPost("/api/v1/timers", timerBody(start)).code == 201);

	char body[64];
	std::snprintf(body, sizeof(body), "{\"start\":%lld}", (long long) (box.timers.clock - 3600));
	const Reply moved = authedPatch("/api/v1/timers/1", body);
	REQUIRE(moved.code == 400);
	REQUIRE(moved.body.find("timer-in-the-past") != std::string::npos);
	REQUIRE(box.timers.timers[0].start == start);

	std::snprintf(body, sizeof(body), "{\"start\":%lld}", (long long) (start + 60));
	REQUIRE(authedPatch("/api/v1/timers/1", body).code == 200);
	REQUIRE(box.timers.timers[0].start == start + 60);
}

// The page sends no start for a running recording.
TEST_CASE("a running recording is lengthened by a change that names only its stop", "[write]")
{
	ShippedRoutes shipped;
	BoxFixture box;

	const time_t began = box.timers.clock + 60;
	REQUIRE(authedPost("/api/v1/timers", timerBody(began)).code == 201);
	REQUIRE(box.timers.timers.size() == 1);
	box.timers.timers[0].state = (int) CTimerd::TIMERSTATE_ISRUNNING;
	box.timers.clock = began + 600;

	char body[64];
	std::snprintf(body, sizeof(body), "{\"stop\":%lld}", (long long) (began + 7200));
	const Reply changed = authedPatch("/api/v1/timers/1", body);
	REQUIRE(changed.code == 200);
	REQUIRE(box.timers.timers[0].start == began);
	REQUIRE(box.timers.timers[0].stop == began + 7200);

	std::snprintf(body, sizeof(body), "{\"start\":%lld}", (long long) (began + 60));
	const Reply moved = authedPatch("/api/v1/timers/1", body);
	REQUIRE(moved.code == 409);
	REQUIRE(moved.body.find("recording-running") != std::string::npos);
	REQUIRE(box.timers.timers[0].start == began);

	std::snprintf(body, sizeof(body), "{\"stop\":%lld}", (long long) box.timers.clock);
	const Reply ended = authedPatch("/api/v1/timers/1", body);
	REQUIRE(ended.code == 400);
	REQUIRE(ended.body.find("timer-in-the-past") != std::string::npos);
	REQUIRE(box.timers.timers[0].stop == began + 7200);
}

// The i-th timer's announce time, and -1 for one the daemon never got.
static time_t filedAnnounce(const BoxFixture &box, size_t i)
{
	return i < box.timers.timers.size() ? box.timers.timers[i].announce : -1;
}

// The page sends an empty field as no member at all.
TEST_CASE("a timer made without an announce time is filed with the box's lead", "[write]")
{
	ShippedRoutes shipped;
	BoxFixture box;

	const time_t start = box.timers.clock + 3600;
	authedPost("/api/v1/timers", timerBody(start));
	CHECK(filedAnnounce(box, 0) == start - 180);
	const ::Json::Value items = parsed(authedGet("/api/v1/timers").body)["items"];
	CHECK(items[0]["announce"].asInt64() == start - 180);

	std::string asked = timerBody(start + 7200);
	asked.erase(asked.size() - 1);
	char member[48];
	std::snprintf(member, sizeof(member), ",\"announce\":%lld}", (long long) (start + 7200 - 600));
	asked += member;
	authedPost("/api/v1/timers", asked);
	CHECK(filedAnnounce(box, 1) == start + 7200 - 600);
}

/* Left where it was, the announce time would end up after the start. Each step
   lands on a value the one before did not leave behind. */
TEST_CASE("a change of the start alone moves the announce time with it", "[write]")
{
	ShippedRoutes shipped;
	BoxFixture box;

	const time_t start = box.timers.clock + 3600;
	char body[160];
	std::snprintf(body, sizeof(body),
		"{\"kind\":\"zapto\",\"channel_id\":\"2b66\",\"start\":%lld,\"announce\":%lld}",
		(long long) start, (long long) (start - 300));
	authedPost("/api/v1/timers", body);
	CHECK(filedAnnounce(box, 0) == start - 300);

	std::snprintf(body, sizeof(body), "{\"start\":%lld}", (long long) (start - 600));
	const Reply moved = authedPatch("/api/v1/timers/1", body);
	CHECK(filedAnnounce(box, 0) == start - 900);
	CHECK(parsed(moved.body)["announce"].asInt64() == start - 900);

	std::snprintf(body, sizeof(body), "{\"start\":%lld,\"announce\":%lld}",
		(long long) start, (long long) (start - 1200));
	authedPatch("/api/v1/timers/1", body);
	CHECK(filedAnnounce(box, 0) == start - 1200);

	// A timer filed with none gets the lead at its new start, not the distance moved.
	if (!box.timers.timers.empty())
		box.timers.timers[0].announce = 0;
	std::snprintf(body, sizeof(body), "{\"start\":%lld}", (long long) (start + 600));
	authedPatch("/api/v1/timers/1", body);
	CHECK(filedAnnounce(box, 0) == start + 600 - 60);
}

TEST_CASE("an announce time the daemon holds that cannot move with the start gives way to the lead", "[write]")
{
	/* The route bounds the start it is sent, so the far ends of a time_t can
	   only be in what the daemon holds, which the older interfaces fill from
	   anywhere. Moving the start then shifts an announce time that does not
	   fit, and the move is checked rather than left to wrap: it gives the
	   announce time up and the layer below fills in the default lead. */
	ShippedRoutes shipped;
	BoxFixture box;

	const time_t start = box.timers.clock + 3600;
	char body[200];
	std::snprintf(body, sizeof(body),
		"{\"kind\":\"zapto\",\"channel_id\":\"2b66\",\"start\":%lld,\"announce\":%lld,\"repeat\":1}",
		(long long) start, (long long) (start - 300));
	REQUIRE(authedPost("/api/v1/timers", body).code == 201);
	REQUIRE(filedAnnounce(box, 0) == start - 300);

	// An announce time at the top of the range, moved later.
	box.timers.timers[0].announce = std::numeric_limits<time_t>::max() - 10;
	std::snprintf(body, sizeof(body), "{\"start\":%lld}", (long long) (start + 60));
	REQUIRE(authedPatch("/api/v1/timers/1", body).code == 200);
	CHECK(filedAnnounce(box, 0) == start + 60 - 60);

	/* A start at the bottom of it and an announce time at the top, which is
	   where wrapping gives an answer that looks like one: the two moves wrap
	   in opposite directions and leave an announce time six seconds before
	   the new start, a lead nobody set. */
	box.timers.timers[0].start = std::numeric_limits<time_t>::min();
	box.timers.timers[0].announce = std::numeric_limits<time_t>::max() - 5;
	std::snprintf(body, sizeof(body), "{\"start\":%lld}", (long long) (start + 120));
	REQUIRE(authedPatch("/api/v1/timers/1", body).code == 200);
	CHECK(filedAnnounce(box, 0) == start + 120 - 60);
}

TEST_CASE("a timer time outside the clock of the box is refused and nothing is stored", "[write]")
{
	/* The box's clock ends at 2038-01-19 03:14:07 UTC and the far ends of a
	   wider time_t are what its own timer list cannot write out, so a start,
	   an end and a guide start are taken from nought to that second. */
	ShippedRoutes shipped;
	BoxFixture box;

	const Reply late = authedPost("/api/v1/timers",
		"{\"kind\":\"zapto\",\"channel_id\":\"2b66\",\"start\":2147483648}");
	REQUIRE(late.code == 400);
	REQUIRE(late.body.find("out-of-range") != std::string::npos);
	REQUIRE(late.body.find("start") != std::string::npos);
	REQUIRE(authedPost("/api/v1/timers",
		"{\"kind\":\"zapto\",\"channel_id\":\"2b66\",\"start\":-1,\"repeat\":1}").code == 400);
	REQUIRE(authedPost("/api/v1/timers",
		"{\"kind\":\"record\",\"channel_id\":\"2b66\",\"start\":2147483000,\"stop\":2147483648}").code == 400);
	const Reply guide = authedPost("/api/v1/timers",
		"{\"kind\":\"zapto\",\"channel_id\":\"2b66\",\"start\":2147483000,\"epg_start\":-1}");
	REQUIRE(guide.code == 400);
	REQUIRE(guide.body.find("epg_start") != std::string::npos);
	REQUIRE(box.timers.timers.empty());

	// The last second is still in, so the bound is the clock's and not one
	// below it.
	REQUIRE(authedPost("/api/v1/timers",
		"{\"kind\":\"zapto\",\"channel_id\":\"2b66\",\"start\":2147483647}").code == 201);
	REQUIRE(box.timers.timers.size() == 1);

	// A correction is held to the same bounds and changes nothing.
	const time_t filed = box.timers.timers[0].start;
	REQUIRE(authedPatch("/api/v1/timers/1", "{\"start\":2147483648}").code == 400);
	REQUIRE(authedPatch("/api/v1/timers/1", "{\"stop\":-1}").code == 400);
	REQUIRE(box.timers.timers[0].start == filed);
	REQUIRE(box.timers.modifications == 0);
}

TEST_CASE("a timer of a kind the box does not make is refused by name", "[write]")
{
	ShippedRoutes shipped;
	BoxFixture box;

	/* The kinds are words and not the numbers the daemon files them under: one
	   of those numbers is kept for a kind the daemon no longer has and one is
	   the label a timer read off another box wears, so a caller naming a number
	   would be naming a file format. */
	const Reply r = authedPost("/api/v1/timers",
	                           "{\"kind\":\"remotebox\",\"start\":1735693200}");
	REQUIRE(r.code == 400);
	REQUIRE(r.body.find("bad-enum") != std::string::npos);
	REQUIRE(box.timers.timers.empty());

	// And a recording that ends no later than it begins, which is the layer
	// below's rule and not a second copy of it here.
	const Reply short_one = authedPost("/api/v1/timers",
	                                   "{\"kind\":\"record\",\"channel_id\":\"2b66\","
	                                   "\"start\":1735693200,\"stop\":1735693200}");
	REQUIRE(short_one.code == 400);
	REQUIRE(short_one.body.find("recording-without-duration") != std::string::npos);
}

TEST_CASE("a repeat the box does not make is refused over both routes", "[write]")
{
	ShippedRoutes shipped;
	BoxFixture box;

	std::string six = timerBody(box.timers.clock + 3600);
	six.erase(six.size() - 1);
	six += ",\"repeat\":6}";
	const Reply made = authedPost("/api/v1/timers", six);
	REQUIRE(made.code == 400);
	REQUIRE(made.body.find("not-a-listed-value") != std::string::npos);
	REQUIRE(box.timers.timers.empty());

	REQUIRE(authedPost("/api/v1/timers", timerBody(box.timers.clock + 3600)).code == 201);
	const Reply changed = authedPatch("/api/v1/timers/1", "{\"repeat\":6}");
	REQUIRE(changed.code == 400);
	REQUIRE(changed.body.find("not-a-listed-value") != std::string::npos);
	REQUIRE(box.timers.timers[0].repeat == 0);
}

TEST_CASE("the timer list is answered and carries what its shape says", "[write]")
{
	ShippedRoutes shipped;
	BoxFixture box;

	REQUIRE(authedPost("/api/v1/timers", timerBody(box.timers.clock + 3600)).code == 201);

	const Reply r = authedGet("/api/v1/timers");
	REQUIRE(r.code == 200);
	REQUIRE(r.content_type == "application/json");

	const ::Json::Value items = parsed(r.body)["items"];
	REQUIRE(items.size() == 1);
	REQUIRE(items[0]["kind"].asString() == "record");
	REQUIRE(items[0]["channel_id"].asString() == "2b66");

	const Endpoint *ep = routeFor(Get, "/api/v1/timers");
	REQUIRE(ep != NULL);
	REQUIRE(ep->auth == AuthLevel::Read);
	const char *why = "";
	INFO(why);
	REQUIRE(schemaIsSane(*ep->schema, &why));
}

TEST_CASE("a daemon nobody declared is not found and nothing is started", "[write]")
{
	ShippedRoutes shipped;
	ProcFixture proc;

	const Reply r = authedPost("/api/v1/daemons/rm-rf/start", "{}");
	REQUIRE(r.code == 404);
	REQUIRE(r.body.find("no-such-daemon") != std::string::npos);

	// The verb is a segment of the path and not a value in the body, so a verb
	// this server does not have is a path it does not have.
	REQUIRE(authedPost("/api/v1/daemons/samba/detonate", "{}").code == 404);
	REQUIRE(authedPost("/api/v1/daemons/samba/detonate", "{}").body.find("no-such-route") != std::string::npos);

	// A daemon every image knows and this box does not carry: said, and nothing runs.
	Sandbox none;
	const std::string before = coreapi::daemons::systemRoot();
	coreapi::daemons::setSystemRoot(none.dir);
	const Reply missing = authedPost("/api/v1/daemons/samba/start", "{}");
	coreapi::daemons::setSystemRoot(before);
	REQUIRE(missing.code == 404);
	REQUIRE(missing.body.find("script-missing") != std::string::npos);
}

TEST_CASE("an OSD message over the ceiling is refused before the box sees it", "[write]")
{
	ShippedRoutes shipped;
	BoxFixture box;

	const Reply over = authedPost("/api/v1/osd/message", messageOfBytes(5000));
	REQUIRE(over.code == 400);
	// Refused where the values are checked, which is before a handler is
	// entered, so the length never becomes an allocation on the thread the box
	// draws on.
	REQUIRE(box.events.sent.empty());

	REQUIRE(authedPost("/api/v1/osd/message", messageOfBytes(coreapi::osd::MAX_MESSAGE_BYTES)).code == 202);
	REQUIRE(box.events.sent.size() == 1);

	// Nothing to show is not a message, and it is the layer below that says so.
	const Reply nothing = authedPost("/api/v1/osd/message", "{\"text\":\"\"}");
	REQUIRE(nothing.code == 400);
	REQUIRE(nothing.body.find("missing-parameter") != std::string::npos);
}

TEST_CASE("the volume is answered and refused by the same numbers", "[write]")
{
	ShippedRoutes shipped;
	BoxFixture box;
	box.box.volume_level = 42;
	box.box.is_muted = true;

	const Reply r = authedGet("/api/v1/osd/volume");
	REQUIRE(r.code == 200);
	REQUIRE(parsed(r.body)["percent"].asInt() == 42);
	REQUIRE(parsed(r.body)["muted"].asBool());

	// Both readings or neither: a level with the mute state missing would be
	// drawn as sound that is on.
	box.box.mute_status = coreapi::Status::Internal;
	REQUIRE(authedGet("/api/v1/osd/volume").code == 500);
	box.box.mute_status = coreapi::Status::Ok;

	REQUIRE(authedPut("/api/v1/osd/volume", "{\"percent\":101}").code == 400);
	REQUIRE(authedPut("/api/v1/osd/volume", "{\"percent\":-1}").code == 400);
	REQUIRE(authedPut("/api/v1/osd/volume", "{\"percent\":100}").code == 202);
	REQUIRE(authedPut("/api/v1/osd/volume", "{\"percent\":0}").code == 202);
	// The event carries the number in a single byte, so a value that got past
	// the row would arrive as a different volume rather than as the loudest.
	REQUIRE(box.events.sent.size() == 2);
}

TEST_CASE("a directory is made and a path removed inside the roots and nowhere else", "[write]")
{
	ShippedRoutes shipped;
	RootsFixture roots;

	const std::string inside = roots.box.at("made");
	const Reply made = authedPost("/api/v1/storage/directory", "{\"path\":\"" + inside + "\"}");
	// Made rather than asked for: this one is a call to the filesystem that
	// answered before the response was built.
	REQUIRE(made.code == 201);
	struct stat st;
	REQUIRE(stat(inside.c_str(), &st) == 0);
	REQUIRE(S_ISDIR(st.st_mode));

	// The name is taken, which is a conflict and not a second creation.
	REQUIRE(authedPost("/api/v1/storage/directory", "{\"path\":\"" + inside + "\"}").code == 409);

	// Outside the roots is refused before the filesystem is asked, so what
	// exists elsewhere on the box cannot be read off which refusal comes back.
	const Reply outside = authedPost("/api/v1/storage/directory", "{\"path\":\"/etc/made\"}");
	REQUIRE(outside.code == 400);
	REQUIRE(outside.body.find("outside-roots") != std::string::npos);
	REQUIRE(stat("/etc/made", &st) != 0);

	REQUIRE(authedDelete("/api/v1/storage/path?path=" + inside).code == 204);
	REQUIRE(stat(inside.c_str(), &st) != 0);
	REQUIRE(authedDelete("/api/v1/storage/path?path=" + inside).code == 404);
	REQUIRE(authedDelete("/api/v1/storage/path?path=/etc/passwd").code == 400);
}

TEST_CASE("a directory that still holds something is not removed", "[write]")
{
	ShippedRoutes shipped;
	RootsFixture roots;

	const std::string dir = roots.box.at("full");
	REQUIRE(mkdir(dir.c_str(), 0700) == 0);
	const std::string inner = dir + "/inner";
	REQUIRE(mkdir(inner.c_str(), 0700) == 0);

	const Reply full = authedDelete("/api/v1/storage/path?path=" + dir);
	REQUIRE(full.code == 409);
	REQUIRE(full.body.find("not-empty") != std::string::npos);
	struct stat st;
	REQUIRE(stat(inner.c_str(), &st) == 0);

	REQUIRE(authedDelete("/api/v1/storage/path?path=" + inner).code == 204);
	REQUIRE(authedDelete("/api/v1/storage/path?path=" + dir).code == 204);
}

TEST_CASE("an id wider than the daemon's own is refused and removes nothing", "[write]")
{
	/* Driven rather than reasoned about, because the reasoning was wrong. The
	   accessor answers an unsigned long and the value below is a thirty two bit id,
	   and those are the same width on the box and not here, so a row that left the
	   bound to the cast was checked by nothing on this build: two to the thirty two
	   plus one arrived as one, and the removal of a timer nobody asked about answered
	   204 and took it. */
	ShippedRoutes shipped;
	BoxFixture box;

	REQUIRE(authedPost("/api/v1/timers", timerBody(box.timers.clock + 3600)).code == 201);
	REQUIRE(box.timers.timers.size() == 1);
	REQUIRE(box.timers.timers[0].id == 1);

	const Reply gone = authedDelete("/api/v1/timers/4294967297");
	REQUIRE(gone.code == 400);
	REQUIRE(gone.body.find("out-of-range") != std::string::npos);
	// The one that was there is still there, which is the half a status alone
	// does not say.
	REQUIRE(box.timers.timers.size() == 1);
	REQUIRE(box.timers.timers[0].id == 1);

	const time_t filed = box.timers.timers[0].announce;
	const Reply wide = authedPatch("/api/v1/timers/4294967297", "{\"announce\":1735689000}");
	REQUIRE(wide.code == 400);
	REQUIRE(wide.body.find("out-of-range") != std::string::npos);
	REQUIRE(box.timers.timers[0].announce == filed);

	// The daemon numbers from one, so nought names no timer and the row says so
	// rather than leaving it to be answered further down.
	REQUIRE(authedDelete("/api/v1/timers/0").code == 400);

	// And the widest id it can hand out is one the row still takes, so the
	// bound is the daemon's ceiling and not one below it.
	REQUIRE(authedDelete("/api/v1/timers/2147483647").code == 404);
}

TEST_CASE("a timer number wider than its field is refused and nothing is stored", "[write]")
{
	ShippedRoutes shipped;
	BoxFixture box;

	const time_t start = box.timers.clock + 3600;
	char at[64];
	std::snprintf(at, sizeof(at), "%lld", (long long) start);

	const std::string wide_repeat =
		std::string("{\"kind\":\"record\",\"channel_id\":\"2b66\",\"start\":") + at +
		",\"stop\":" + at + ",\"repeat\":4294967296}";
	const Reply repeat = authedPost("/api/v1/timers", wide_repeat);
	REQUIRE(repeat.code == 400);
	REQUIRE(repeat.body.find("out-of-range") != std::string::npos);
	// It was stored as nought before, which is a timer that runs once rather
	// than the one that was asked for.
	REQUIRE(box.timers.timers.empty());

	const std::string wide_count =
		std::string("{\"kind\":\"record\",\"channel_id\":\"2b66\",\"start\":") + at +
		",\"stop\":" + at + ",\"repeat_count\":4294967296}";
	REQUIRE(authedPost("/api/v1/timers", wide_count).code == 400);
	REQUIRE(box.timers.timers.empty());

	// The widest each of them means is still taken, so neither bound refuses
	// anything a caller can mean. 0xff00 is the weekday flag with every day set.
	char ends[64];
	std::snprintf(ends, sizeof(ends), "%lld", (long long) (start + 3600));
	const std::string widest =
		std::string("{\"kind\":\"record\",\"channel_id\":\"2b66\",\"start\":") + at +
		",\"stop\":" + ends + ",\"repeat\":65280,\"repeat_count\":2147483647}";
	const Reply made = authedPost("/api/v1/timers", widest);
	REQUIRE(made.code == 201);
	REQUIRE(box.timers.timers.size() == 1);
	REQUIRE(box.timers.timers[0].repeat == 65280);
	REQUIRE(box.timers.timers[0].repeat_count == 2147483647u);
}

TEST_CASE("a settings write naming one setting twice is refused whole", "[write]")
{
	/* This handler reads its own body, so the router's check of a name written twice
	   never ran over these. Without this the two halves of one server disagreed: the
	   route beside it answered duplicate-parameter while a repeat here was taken,
	   both writes ran, the first was discarded with nothing said, and the answer named
	   the key twice. */
	ShippedRoutes shipped;
	BoxFixture box;

	const Reply r = authedPatch("/api/v1/settings/audio",
	                            "{\"audio_volume_percent_ac3\":\"30\",\"audio_volume_percent_ac3\":\"70\"}");
	REQUIRE(r.code == 400);
	REQUIRE(r.body.find("duplicate-parameter") != std::string::npos);
	// Refused whole and before anything was written, so neither of the two
	// values is what the box is running on.
	REQUIRE(box.store.ints.find("audio_volume_percent_ac3") == box.store.ints.end());
	REQUIRE(box.store.persisted == 0);

	// The one that was refused, and the one that would have been misreported:
	// two values of one key where the first passes and the second does not.
	const Reply mixed = authedPatch("/api/v1/settings/audio",
	                                "{\"audio_volume_percent_ac3\":\"3\",\"audio_volume_percent_ac3\":\"5000\"}");
	REQUIRE(mixed.code == 400);
	REQUIRE(box.store.ints.find("audio_volume_percent_ac3") == box.store.ints.end());

	// And the route beside it answers a repeat the same way, through the
	// router, which is what the two halves now agree about.
	const Reply twice = authedPost("/api/v1/settings/secret/clear",
	                               "{\"key\":\"a\",\"key\":\"b\"}");
	REQUIRE(twice.code == 400);
	REQUIRE(twice.body.find("duplicate-parameter") != std::string::npos);
}

TEST_CASE("every key of an answer with a result per key is named once", "[write]")
{
	/* One member per result is a document only while the keys are distinct, and
	   what makes them distinct is the refusal above. Read back through a parser
	   rather than by looking at the bytes, because a document carrying one name
	   twice is one a parser silently collapses. */
	ShippedRoutes shipped;
	BoxFixture box;

	const Reply r = authedPatch("/api/v1/settings/audio",
	                            "{\"audio_volume_percent_ac3\":\"50\",\"nope\":\"1\",\"also_nope\":\"2\"}");
	REQUIRE(r.code == 207);

	const ::Json::Value results = parsed(r.body)["results"];
	REQUIRE(results.size() == 3);
	const ::Json::Value::Members names = results.getMemberNames();
	REQUIRE(names.size() == 3);
	for (size_t i = 0; i < names.size(); ++i)
	{
		size_t seen = 0;
		for (size_t j = 0; j < names.size(); ++j)
			seen += (names[i] == names[j]) ? 1 : 0;
		INFO(names[i]);
		REQUIRE(seen == 1);
	}
}

/* The settings that are one fact in two parts, through the route: the id of a start
   channel alone brings the name of its channel, the name alone is refused, and the
   weather place is written whole, emptying the postal code a search had entered. */
TEST_CASE("a write keeps the start channel and the weather place together", "[write]")
{
	ShippedRoutes shipped;
	BoxFixture box;
	// The start channels are changeable only while the box does not start on the last one.
	box.store.ints["uselastchannel"] = 0;
	coreapi::ChannelInfo tv;
	tv.id = 0x1e2404020001a001ULL;
	tv.name = "First Channel Television HD";
	tv.kind = coreapi::ServiceKind::Tv;
	box.channels.channels.push_back(tv);

	const Reply r = authedPatch("/api/v1/settings/channel", "{\"startchanneltv_id\":\"1e2404020001a001\"}");
	REQUIRE(r.code == 200);
	CHECK(box.store.strings["startchanneltv_id"] == "1e2404020001a001");
	CHECK(box.store.strings["startchanneltv"] == "First Channel Television HD");

	const Reply alone = authedPatch("/api/v1/settings/channel", "{\"startchanneltv\":\"Another Name\"}");
	REQUIRE(alone.code == 409);
	CHECK(alone.body.find("setting-condition-not-met") != std::string::npos);
	CHECK(alone.body.find("startchanneltv_id") != std::string::npos);
	CHECK(box.store.strings["startchanneltv"] == "First Channel Television HD");

	box.store.strings["weather_postalcode"] = "10178";
	const Reply place = authedPatch("/api/v1/settings/weather",
	                                "{\"weather_city\":\"Hamburg\",\"weather_location\":\"53.55,9.99\"}");
	REQUIRE(place.code == 200);
	CHECK(box.store.strings["weather_city"] == "Hamburg");
	CHECK(box.store.strings["weather_location"] == "53.55,9.99");
	CHECK(box.store.strings["weather_postalcode"].empty());
}

TEST_CASE("a write the parental lock holds is refused as setting-locked", "[write]")
{
	ShippedRoutes shipped;
	BoxFixture box;
	box.box.parental_locked = true;

	const Reply r = authedPatch("/api/v1/settings/parental", "{\"parentallock_lockage\":\"16\"}");
	REQUIRE(r.code == 409);
	REQUIRE(r.body.find("setting-locked") != std::string::npos);
	REQUIRE(box.store.ints.count("parentallock_lockage") == 0);
}

TEST_CASE("a write of a setting the box lacks is refused as setting-not-on-this-box", "[write]")
{
	ShippedRoutes shipped;
	BoxFixture box;

	box.box.caps.has_fan = 0;
	const Reply r = authedPatch("/api/v1/settings/misc", "{\"fan_speed\":\"3\"}");
	REQUIRE(r.code == 409);
	REQUIRE(r.body.find("setting-not-on-this-box") != std::string::npos);
	REQUIRE(box.store.ints.count("fan_speed") == 0);

	box.box.caps.has_fan = 1;
	const Reply taken = authedPatch("/api/v1/settings/misc", "{\"fan_speed\":\"3\"}");
	REQUIRE(taken.code == 200);
	REQUIRE(box.store.ints["fan_speed"] == 3);
}

/* A setting whose values the box decides, while nothing says what they are: the route
   takes nothing new and says why, so a client can tell it from a value the box does not
   offer. */
TEST_CASE("a setting whose values nobody can state is refused as such through the route", "[write]")
{
	ShippedRoutes shipped;
	BoxFixture box;

	const Reply r = authedPatch("/api/v1/settings/video", "{\"video_Mode\":\"13\"}");
	REQUIRE(r.code == 400);
	CHECK(r.body.find("choices-unavailable") != std::string::npos);
	CHECK(box.store.ints.count("video_Mode") == 0);
}

/* An id written alone has to bring a name, so the route answers for the channel it
   names: none the box holds, one of the other list, or a list that cannot be read.
   Nothing of the pair is stored in any of the three. */
TEST_CASE("a start channel id that brings no name is refused for the channel it names", "[write]")
{
	ShippedRoutes shipped;
	BoxFixture box;
	coreapi::ChannelInfo radio;
	radio.id = 0x1e2404020001b002ULL;
	radio.name = "First Channel Radio Programme";
	radio.kind = coreapi::ServiceKind::Radio;
	box.channels.channels.push_back(radio);

	const Reply none = authedPatch("/api/v1/settings/channel", "{\"startchanneltv_id\":\"dead\"}");
	REQUIRE(none.code == 404);
	CHECK(none.body.find("no-such-channel") != std::string::npos);

	const Reply other = authedPatch("/api/v1/settings/channel", "{\"startchanneltv_id\":\"1e2404020001b002\"}");
	REQUIRE(other.code == 400);
	CHECK(other.body.find("not-a-listed-value") != std::string::npos);

	box.channels.channels_status = coreapi::Status::Internal;
	const Reply unread = authedPatch("/api/v1/settings/channel", "{\"startchanneltv_id\":\"1e2404020001b002\"}");
	REQUIRE(unread.code == 500);
	CHECK(unread.body.find("channel-list-unavailable") != std::string::npos);

	CHECK(box.store.strings.find("startchanneltv_id") == box.store.strings.end());
	CHECK(box.store.strings.find("startchanneltv") == box.store.strings.end());
}

/* A row's conditions through the route: a setting the screen greys out is refused with the
   settings that grey it out, written together with the one that allows it in either order
   it is taken, and a value it already holds is taken back unasked. */
TEST_CASE("a write is held to the conditions of the rows it writes", "[write][conditions]")
{
	ShippedRoutes shipped;
	BoxFixture box;
	box.store.ints["srs_enable"] = 0;

	const Reply alone = authedPatch("/api/v1/settings/audio", "{\"srs_algo\":\"0\"}");
	REQUIRE(alone.code == 409);
	const ::Json::Value refused = parsed(alone.body);
	CHECK(refused["type"].asString() == "/errors/setting-condition-not-met");
	REQUIRE(refused["depends_on"].size() == 1);
	CHECK(refused["depends_on"][0].asString() == "srs_enable");
	CHECK(box.store.ints.count("srs_algo") == 0);

	// What the row holds goes back unasked, as when a page sends a whole section.
	const Reply held = authedPatch("/api/v1/settings/audio", "{\"srs_algo\":\"1\"}");
	CHECK(held.code == 200);

	const Reply both = authedPatch("/api/v1/settings/audio", "{\"srs_algo\":\"0\",\"srs_enable\":\"1\"}");
	REQUIRE(both.code == 200);
	CHECK(box.store.ints["srs_algo"] == 0);
	CHECK(box.store.ints["srs_enable"] == 1);

	box.store.ints["srs_enable"] = 0;
	box.store.ints.erase("srs_algo");
	const Reply reversed = authedPatch("/api/v1/settings/audio", "{\"srs_enable\":\"1\",\"srs_algo\":\"0\"}");
	CHECK(reversed.code == 200);
	CHECK(box.store.ints["srs_algo"] == 0);

	// Mixed: the independent key lands, the refused one names what refused it.
	box.store.ints["srs_enable"] = 0;
	box.store.ints.erase("srs_algo");
	box.store.ints["audio_DolbyDigital"] = 0;
	const Reply mixed = authedPatch("/api/v1/settings/audio", "{\"srs_algo\":\"0\",\"audio_DolbyDigital\":\"1\"}");
	REQUIRE(mixed.code == 207);
	const ::Json::Value results = parsed(mixed.body)["results"];
	CHECK(results["srs_algo"]["status"].asInt() == 409);
	CHECK(results["srs_algo"]["code"].asString() == "setting-condition-not-met");
	REQUIRE(results["srs_algo"]["depends_on"].size() == 1);
	CHECK(results["srs_algo"]["depends_on"][0].asString() == "srs_enable");
	CHECK(results["audio_DolbyDigital"]["status"].asInt() == 200);
	CHECK(box.store.ints.count("srs_algo") == 0);
	CHECK(box.store.ints["audio_DolbyDigital"] == 1);
}

/* What the write brought is answered under its own key, and a half taken down by the
   other names it, in an answer a parser reads back with every key once. */
/* A start channel id sent with a name its row refuses: the name keeps its refusal, and the id
   lands as one sent alone, with the name of its channel written beside it, so the store never
   holds the new id beside the old name. */
TEST_CASE("a refused start channel name leaves the id to write its channel's name", "[write][conditions]")
{
	ShippedRoutes shipped;
	BoxFixture box;
	box.store.ints["uselastchannel"] = 0;
	box.store.strings["startchanneltv_id"] = "1e2404020001b002";
	box.store.strings["startchanneltv"] = "Old Name";
	coreapi::ChannelInfo tv;
	tv.id = 0x1e2404020001a001ULL;
	tv.name = "First Channel Television HD";
	tv.kind = coreapi::ServiceKind::Tv;
	box.channels.channels.push_back(tv);

	const Reply r = authedPatch("/api/v1/settings/channel",
	                            "{\"startchanneltv_id\":\"1e2404020001a001\",\"startchanneltv\":\"Name #3\"}");
	REQUIRE(r.code == 207);
	const ::Json::Value results = parsed(r.body)["results"];
	CHECK(results["startchanneltv_id"]["status"].asInt() == 200);
	CHECK(results["startchanneltv"]["status"].asInt() == 400);
	CHECK(box.store.strings["startchanneltv_id"] == "1e2404020001a001");
	CHECK(box.store.strings["startchanneltv"] == "First Channel Television HD");
}

/* A start channel id written alone while the box starts on the last channel: the name it would
   have brought falls with it, and the one key sent is answered as itself, naming what greys it
   out. */
TEST_CASE("a start channel written alone while it is greyed out is answered as itself", "[write][conditions]")
{
	ShippedRoutes shipped;
	BoxFixture box;
	box.store.ints["uselastchannel"] = 1;
	coreapi::ChannelInfo tv;
	tv.id = 0x1e2404020001a001ULL;
	tv.name = "First Channel Television HD";
	tv.kind = coreapi::ServiceKind::Tv;
	box.channels.channels.push_back(tv);

	const Reply r = authedPatch("/api/v1/settings/channel", "{\"startchanneltv_id\":\"1e2404020001a001\"}");
	REQUIRE(r.code == 409);
	const ::Json::Value refused = parsed(r.body);
	CHECK(refused["type"].asString() == "/errors/setting-condition-not-met");
	REQUIRE(refused["depends_on"].size() == 1);
	CHECK(refused["depends_on"][0].asString() == "uselastchannel");
	CHECK(box.store.strings.count("startchanneltv_id") == 0);
	CHECK(box.store.strings.count("startchanneltv") == 0);
}

TEST_CASE("an answer with a result per key names what a write brought and what took a half down", "[write]")
{
	ShippedRoutes shipped;
	BoxFixture box;
	// The start channels are changeable only while the box does not start on the last one.
	box.store.ints["uselastchannel"] = 0;
	coreapi::ChannelInfo tv;
	tv.id = 0x1e2404020001a001ULL;
	tv.name = "First Channel Television HD";
	tv.kind = coreapi::ServiceKind::Tv;
	box.channels.channels.push_back(tv);

	const Reply brought = authedPatch("/api/v1/settings/channel",
	                                  "{\"startchanneltv_id\":\"1e2404020001a001\",\"nope\":\"1\"}");
	REQUIRE(brought.code == 207);
	const ::Json::Value results = parsed(brought.body)["results"];
	REQUIRE(results.size() == 3);
	CHECK(results["startchanneltv_id"]["status"].asInt() == 200);
	CHECK(results["startchanneltv"]["status"].asInt() == 200);
	CHECK(results["nope"]["status"].asInt() == 404);
	CHECK_FALSE(results["nope"].isMember("depends_on"));

	box.store.strings["weather_city"] = "Berlin";
	const Reply down = authedPatch("/api/v1/settings/weather",
	                               "{\"weather_city\":\"Ham\\nburg\",\"weather_location\":\"53.55,9.99\"}");
	REQUIRE(down.code == 207);
	const ::Json::Value halves = parsed(down.body)["results"];
	REQUIRE(halves.size() == 2);
	CHECK(halves["weather_city"]["code"].asString() == "bad-string");
	CHECK_FALSE(halves["weather_city"].isMember("depends_on"));
	CHECK(halves["weather_location"]["status"].asInt() == 409);
	CHECK(halves["weather_location"]["code"].asString() == "setting-condition-not-met");
	REQUIRE(halves["weather_location"]["depends_on"].size() == 1);
	CHECK(halves["weather_location"]["depends_on"][0].asString() == "weather_city");
	// Neither half, and not the postal code the place would have emptied.
	CHECK(box.store.strings["weather_city"] == "Berlin");
	CHECK(box.store.strings.find("weather_location") == box.store.strings.end());
	CHECK(box.store.strings.find("weather_postalcode") == box.store.strings.end());
}

TEST_CASE("a settings value that cannot be answered back is not stored", "[write]")
{
	/* The writer replaces a byte it cannot read with the character that says
	   so, which is right for a name off a broadcast stream and wrong for a
	   value this server stores: written and read back it came out different, so
	   a client writing and reading never converged on what the box holds.
	   Refused on the way in, where substituting was silent. */
	ShippedRoutes shipped;
	BoxFixture box;

	const std::string plain = plainKeyIn("network");
	REQUIRE_FALSE(plain.empty());
	box.store.strings[plain] = "before";

	// Two bytes that begin no sequence there is, between two that are text.
	const std::string bad = "{\"" + plain + "\":\"a\xff\xfe b\"}";
	const Reply r = authedPatch("/api/v1/settings/network", bad);
	REQUIRE(r.code == 400);
	REQUIRE(r.body.find("bad-string") != std::string::npos);
	REQUIRE(box.store.strings[plain] == "before");

	// Text above the first hundred and twenty eight characters is still text
	// and is still stored, so the refusal is about what cannot be read and not
	// about what is not plain.
	const Reply fine = authedPatch("/api/v1/settings/network",
	                               "{\"" + plain + "\":\"\xc3\xa4\xe2\x82\xac\"}");
	REQUIRE(fine.code == 200);
	REQUIRE(box.store.strings[plain] == "\xc3\xa4\xe2\x82\xac");

	// And what is stored reads back as what was sent, which is the round trip
	// that did not settle before.
	REQUIRE(valueOf(authedGet("/api/v1/settings/network").body, plain) == "\xc3\xa4\xe2\x82\xac");
}

TEST_CASE("the wait after a wrong guess grows and stops growing", "[write]")
{
	/* The policy on its own, walked rather than slept through. The ceiling is
	   the whole of the promise that this cannot lock the only account out:
	   there is one account on this box and nobody to unlock it. */
	REQUIRE(loginDelayFor(0) == 0);
	REQUIRE(loginDelayFor(1) == 0);
	REQUIRE(loginDelayFor(2) == 0);
	REQUIRE(loginDelayFor(3) == 1);
	REQUIRE(loginDelayFor(4) == 2);
	REQUIRE(loginDelayFor(5) == 4);
	REQUIRE(loginDelayFor(6) == 8);

	// It rises, and it stops. Both halves, because a delay that stopped rising
	// at the third guess and one that never stopped are each wrong.
	unsigned last = 0;
	unsigned ceiling = 0;
	for (unsigned f = 3; f < 200; ++f)
	{
		const unsigned d = loginDelayFor(f);
		INFO("after " << f << " wrong answers");
		REQUIRE(d >= last);
		REQUIRE(d <= 60);
		if (d == 60)
			++ceiling;
		last = d;
	}
	// A ceiling nothing reaches would pass the bound above while leaving the
	// wait to grow for ever.
	REQUIRE(ceiling > 0);
	REQUIRE(loginDelayFor(4000000000u) == 60);
}

TEST_CASE("guessing is slowed down and the only account is never locked out", "[write]")
{
	ShippedRoutes shipped;
	ConfigFixture cfg;

	// The gate's clock, so the wait can be walked through without waiting it
	// out. Put back by the fixture.
	time_t now = 1735689600;
	setLoginClockForTest(now);

	// The free ones, because the ordinary reason a password is wrong is that
	// somebody mistyped it.
	REQUIRE(login(kUser, "nope").code == 401);
	REQUIRE(login(kUser, "nope").code == 401);

	const Reply third = login(kUser, "nope");
	REQUIRE(third.code == 401);

	// And now the address waits.
	const Reply refused = login(kUser, "nope");
	REQUIRE(refused.code == 429);
	REQUIRE(refused.body.find("too-many-attempts") != std::string::npos);
	REQUIRE(headerOf(refused, "Retry-After") == "1");
	// It says that too much is being asked and nothing about which of the two
	// things closed the gate, and it names neither the account nor the address.
	REQUIRE(refused.body.find(kUser) == std::string::npos);
	REQUIRE(refused.body.find(kPeer) == std::string::npos);

	// The right password is refused too while the wait stands, which is what
	// makes this a wait and not a filter on wrong guesses.
	REQUIRE(login(kUser, kPassword).code == 429);

	// It runs out on its own. Nobody has to unlock anything, because there is
	// nobody to do it.
	now += 2;
	setLoginClockForTest(now);
	const Reply in = login(kUser, kPassword);
	REQUIRE(in.code == 200);

	// And a right answer clears the record outright, so the next wrong one
	// starts from the free attempts again.
	REQUIRE(login(kUser, "nope").code == 401);
	REQUIRE(login(kUser, "nope").code == 401);
	REQUIRE(login(kUser, "nope").code == 401);

	const std::string c = headerOf(in, "Set-Cookie");
	REQUIRE(closeSession(c.substr(c.find('=') + 1, c.find(';') - c.find('=') - 1)));
}

TEST_CASE("the gate turns an attempt down before it derives anything", "[write]")
{
	/* The point of the gate is the processor and not only the guessing: login
	   is the one route reachable without a credential and it derives a key on
	   every attempt, so an answer that turned a caller down after deriving
	   would have paid for the attempt it was refusing. Coarse, and it says so:
	   it fails only on a difference large enough to matter. */
	ShippedRoutes shipped;
	ConfigFixture cfg;

	setLoginClockForTest(1735689600);

	const double derived = cpuSecondsOf(&eightDerivedRefusals);

	// Three wrong answers, and then eight refusals that cost nothing.
	for (int i = 0; i < 3; ++i)
		(void) login(kUser, "nope");
	const double turned = cpuSecondsOf(&eightGatedRefusals);

	INFO("derived " << derived << " turned down " << turned);
	REQUIRE(derived > 0.0);
	REQUIRE(turned < derived / 4.0);
}

TEST_CASE("one address guessing does not turn another one away", "[write]")
{
	ShippedRoutes shipped;
	ConfigFixture cfg;

	setLoginClockForTest(1735689600);

	for (int i = 0; i < 6; ++i)
		(void) send(Post, "/api/v1/login", loginBody(kUser, "nope"), AuthLevel::Public, "203.0.113.9");
	REQUIRE(send(Post, "/api/v1/login", loginBody(kUser, kPassword), AuthLevel::Public, "203.0.113.9").code == 429);

	// The record is per address, so somebody else's guessing is not what stands
	// between this caller and the box.
	const Reply in = send(Post, "/api/v1/login", loginBody(kUser, kPassword), AuthLevel::Public, "198.51.100.4");
	REQUIRE(in.code == 200);

	const std::string c = headerOf(in, "Set-Cookie");
	REQUIRE(closeSession(c.substr(c.find('=') + 1, c.find(';') - c.find('=') - 1)));
}

TEST_CASE("only so many attempts are answered at once", "[write]")
{
	/* The half a per address delay does not buy: an attacker with a thousand addresses
	   defeats that and does not defeat this, because every one of these is a
	   derivation on a thread of this daemon. Driven through the gate itself rather
	   than through the route, because what it bounds is how many are in flight and a
	   case that answered them one after another would never have two. */
	ShippedRoutes shipped;
	ConfigFixture cfg;

	unsigned retry = 0;
	REQUIRE(beginLoginAttempt("198.51.100.1", &retry) == LoginAttempt::Open);
	REQUIRE(beginLoginAttempt("198.51.100.2", &retry) == LoginAttempt::Open);
	REQUIRE(beginLoginAttempt("198.51.100.3", &retry) == LoginAttempt::Open);
	REQUIRE(beginLoginAttempt("198.51.100.4", &retry) == LoginAttempt::Open);

	// A fifth, from an address that has never guessed wrong at all.
	REQUIRE(beginLoginAttempt("198.51.100.5", &retry) == LoginAttempt::TooBusy);
	REQUIRE(retry > 0);

	// And the room comes back, so a burst is a wait and not a door that shuts.
	endLoginAttempt("198.51.100.1", false);
	REQUIRE(beginLoginAttempt("198.51.100.5", &retry) == LoginAttempt::Open);

	endLoginAttempt("198.51.100.2", false);
	endLoginAttempt("198.51.100.3", false);
	endLoginAttempt("198.51.100.4", false);
	endLoginAttempt("198.51.100.5", false);
}

TEST_CASE("an address whose delay ran out gets one attempt at a time", "[write]")
{
	ShippedRoutes shipped;
	ConfigFixture cfg;

	time_t now = 1735689600;
	setLoginClockForTest(now);
	for (int i = 0; i < 3; ++i)
		endLoginAttempt("203.0.113.7", false);

	now += 1;
	setLoginClockForTest(now);
	unsigned retry = 0;
	REQUIRE(beginLoginAttempt("203.0.113.7", &retry) == LoginAttempt::Open);
	REQUIRE(beginLoginAttempt("203.0.113.7", &retry) == LoginAttempt::TooSoon);
	REQUIRE(retry > 0);
	REQUIRE(beginLoginAttempt("198.51.100.7", &retry) == LoginAttempt::Open);

	endLoginAttempt("203.0.113.7", false);
	endLoginAttempt("198.51.100.7", false);
}

TEST_CASE("the table of who has been guessing does not grow without bound", "[write]")
{
	/* Otherwise the thing an anonymous caller exhausts is this rather than the
	   processor. What goes when it is full is the address that has been quiet
	   longest, which is the direction that cannot lock anybody out. */
	ShippedRoutes shipped;
	ConfigFixture cfg;

	setLoginClockForTest(1735689600);

	char peer[32];
	for (int i = 0; i < 400; ++i)
	{
		std::snprintf(peer, sizeof(peer), "198.51.%d.%d", i / 200, i % 200);
		endLoginAttempt(peer, false);
	}
	REQUIRE(loginAttemptCountForTest() > 0);
	REQUIRE(loginAttemptCountForTest() <= 400u);

	// The sweep runs where the gate is asked, so this is what a request does.
	unsigned retry = 0;
	(void) beginLoginAttempt("198.51.100.250", &retry);
	endLoginAttempt("198.51.100.250", false);
	REQUIRE(loginAttemptCountForTest() <= 256u);
}

TEST_CASE("a bouquet round trip makes, renames, moves and takes away", "[write]")
{
	ShippedRoutes shipped;
	BoxFixture box;
	giveBouquets(box.channels);

	REQUIRE(authedPost("/api/v1/bouquets", "{\"name\":\"Drittes\"}").code == 201);

	/* Read back over the wire and not out of the answer. An answer of 201 says
	   what this server decided to send and nothing about the list the box
	   holds, and every check below is about the list. */
	const ::Json::Value made = bouquetNamed(authedGet("/api/v1/bouquets").body, "Drittes");
	REQUIRE(made["id"].asUInt() == 3u);
	REQUIRE(made["user_bouquet"].asBool());

	REQUIRE(authedPut("/api/v1/bouquets/Drittes/name", "{\"name\":\"Viertes\"}").code == 204);
	const std::string renamed = authedGet("/api/v1/bouquets").body;
	REQUIRE(bouquetNamed(renamed, "Viertes")["id"].asUInt() == 3u);
	// The key moved with it, so the old name names nothing.
	REQUIRE(bouquetNamed(renamed, "Drittes").isNull());

	REQUIRE(authedPut("/api/v1/bouquets/Viertes/position", "{\"direction\":\"up\"}").code == 204);
	const ::Json::Value moved = bouquetNamed(authedGet("/api/v1/bouquets").body, "Viertes");
	REQUIRE(moved["id"].asUInt() == 2u);

	REQUIRE(authedPut("/api/v1/bouquets/Viertes/hidden", "{\"on\":true}").code == 204);
	REQUIRE(authedPut("/api/v1/bouquets/Viertes/locked", "{\"on\":true}").code == 204);
	const std::string flagged = authedGet("/api/v1/bouquets").body;
	REQUIRE(bouquetNamed(flagged, "Viertes")["hidden"].asBool());
	REQUIRE(bouquetNamed(flagged, "Viertes")["locked"].asBool());
	// And the bouquets beside it are not what was meant.
	REQUIRE_FALSE(bouquetNamed(flagged, "Erstes")["hidden"].asBool());
	REQUIRE_FALSE(bouquetNamed(flagged, "Zweites")["locked"].asBool());

	REQUIRE(authedDelete("/api/v1/bouquets/Viertes").code == 204);
	REQUIRE(bouquetNamed(authedGet("/api/v1/bouquets").body, "Viertes").isNull());

	// A second removal is the removal of a bouquet that is not there.
	const Reply again = authedDelete("/api/v1/bouquets/Viertes");
	REQUIRE(again.code == 404);
	REQUIRE(again.body.find("no-such-bouquet") != std::string::npos);

	// A name something else carries is a conflict and moves nothing.
	const Reply taken = authedPost("/api/v1/bouquets", "{\"name\":\"Erstes\"}");
	REQUIRE(taken.code == 409);
	REQUIRE(taken.body.find("name-taken") != std::string::npos);
	REQUIRE(parsed(authedGet("/api/v1/bouquets").body)["items"].size() == 2u);
}

TEST_CASE("every bouquet route refuses a name no bouquet carries and changes nothing", "[write]")
{
	ShippedRoutes shipped;
	BoxFixture box;
	giveBouquets(box.channels);

	const Reply answers[] = {
		authedPut("/api/v1/bouquets/Keines/name", "{\"name\":\"Anders\"}"),
		authedPut("/api/v1/bouquets/Keines/position", "{\"direction\":\"up\"}"),
		authedPut("/api/v1/bouquets/Keines/hidden", "{\"on\":true}"),
		authedPut("/api/v1/bouquets/Keines/locked", "{\"on\":true}"),
		authedPut("/api/v1/bouquets/Keines/channels?mode=tv", "[\"2b66\"]"),
	};
	for (size_t i = 0; i < sizeof(answers) / sizeof(answers[0]); ++i)
	{
		INFO(i);
		REQUIRE(answers[i].code == 404);
		REQUIRE(answers[i].body.find("no-such-bouquet") != std::string::npos);
	}

	const Reply taken = authedPut("/api/v1/bouquets/Zweites/name", "{\"name\":\"Erstes\"}");
	REQUIRE(taken.code == 409);
	REQUIRE(taken.body.find("name-taken") != std::string::npos);

	REQUIRE(box.channels.bouquets[0].name == "Erstes");
	REQUIRE(box.channels.bouquets[1].name == "Zweites");
	REQUIRE(box.channels.saves == 0u);
}

TEST_CASE("a bouquet name two bouquets carry is answered as shared", "[write]")
{
	ShippedRoutes shipped;
	BoxFixture box;
	giveBouquets(box.channels);
	coreapi::BouquetInfo twin;
	twin.id = 3;
	twin.name = "Erstes";
	twin.user_bouquet = true;
	box.channels.bouquets.push_back(twin);

	// Every write that names a bouquet, and none of them reaches either one.
	const Reply hide = authedPut("/api/v1/bouquets/Erstes/hidden", "{\"on\":true}");
	REQUIRE(hide.code == 409);
	REQUIRE(hide.body.find("name-shared") != std::string::npos);
	REQUIRE(authedPut("/api/v1/bouquets/Erstes/locked", "{\"on\":true}").code == 409);
	REQUIRE(authedPut("/api/v1/bouquets/Erstes/name", "{\"name\":\"Anders\"}").code == 409);
	REQUIRE(authedPut("/api/v1/bouquets/Erstes/position", "{\"direction\":\"down\"}").code == 409);
	REQUIRE(authedPut("/api/v1/bouquets/Erstes/channels?mode=tv", "[]").code == 409);
	REQUIRE(authedDelete("/api/v1/bouquets/Erstes").code == 409);

	const ::Json::Value all = parsed(authedGet("/api/v1/bouquets").body)["items"];
	REQUIRE(all.size() == 3u);
	for (::Json::ArrayIndex i = 0; i < all.size(); i++)
	{
		REQUIRE_FALSE(all[i]["hidden"].asBool());
		REQUIRE_FALSE(all[i]["locked"].asBool());
	}
	REQUIRE(box.channels.saves == 0);
}

TEST_CASE("a bouquet name carrying a separator and a percent is one name", "[write]")
{
	ShippedRoutes shipped;
	BoxFixture box;
	giveBouquets(box.channels);

	/* The name is one path segment, and a segment is decoded after the path has
	   been split, so an encoded separator stays a byte of the name rather than
	   making a second segment out of it. */
	REQUIRE(authedPost("/api/v1/bouquets", "{\"name\":\"Sport/Film 50%\"}").code == 201);
	REQUIRE(authedPut("/api/v1/bouquets/Sport%2FFilm%2050%25/hidden", "{\"on\":true}").code == 204);

	const std::string all = authedGet("/api/v1/bouquets").body;
	// The bouquet that name means and no other, which is what a name split at
	// the separator would have reached instead.
	REQUIRE(bouquetNamed(all, "Sport/Film 50%")["hidden"].asBool());
	REQUIRE_FALSE(bouquetNamed(all, "Erstes")["hidden"].asBool());
	REQUIRE_FALSE(bouquetNamed(all, "Zweites")["hidden"].asBool());

	// And the same name written with the separator as a separator reaches no
	// route at all, which is the shape the encoding exists to avoid.
	REQUIRE(authedPut("/api/v1/bouquets/Sport/Film/hidden", "{\"on\":false}").code == 404);
}

TEST_CASE("a caller on the local network can change no bouquet", "[write]")
{
	ShippedRoutes shipped;
	BoxFixture box;
	giveBouquets(box.channels);

	REQUIRE(lanPost("/api/v1/bouquets", "{\"name\":\"Drittes\"}").code == 403);
	REQUIRE(lanPut("/api/v1/bouquets/Erstes/name", "{\"name\":\"Anders\"}").code == 403);
	REQUIRE(lanPut("/api/v1/bouquets/Zweites/position", "{\"direction\":\"up\"}").code == 403);
	REQUIRE(lanPut("/api/v1/bouquets/Erstes/hidden", "{\"on\":true}").code == 403);
	REQUIRE(lanPut("/api/v1/bouquets/Erstes/locked", "{\"on\":true}").code == 403);
	REQUIRE(lanPut("/api/v1/bouquets/Erstes/channels?mode=tv", "[\"2b66\"]").code == 403);
	REQUIRE(lanDelete("/api/v1/bouquets/Erstes").code == 403);

	// Nothing reached the box. A refusal written after the act would answer 403
	// and change the list anyway, and the code alone cannot tell the two apart.
	REQUIRE(box.channels.bouquets.size() == 2u);
	REQUIRE(box.channels.bouquets[0].name == "Erstes");
	REQUIRE(box.channels.bouquets[1].name == "Zweites");
	REQUIRE_FALSE(box.channels.bouquets[0].hidden);
	REQUIRE_FALSE(box.channels.bouquets[0].locked);
	REQUIRE(box.channels.saves == 0u);
	REQUIRE(box.channels.reloads == 0u);
}

TEST_CASE("a bouquet route written below a write is a table the server will not start on",
          "[write]")
{
	/* The other half of the rule, and the half the refusal above cannot show:
	   a route written at a read is refused where tables are checked, so the
	   server does not start and the route never answers anything at all. */
	static const Param params[] = {
		HTTPD_SEGMENT_TEXT("bouquet", "the bouquet", 255),
	};
	static const Endpoint too_low[] = {
		{ Delete, "/api/v1/bouquets/{bouquet}", AuthLevel::Read,
		  "takes a bouquet away", NULL, HTTPD_PARAMS(params), NULL, &unreachedHandler, false, Answers200, HTTPD_NO_REFUSALS },
	};
	const RouteTable low = { HTTPD_TABLE_N("low", too_low, 1) };
	std::string why;
	REQUIRE_FALSE(tableIsSane(low, &why));
	INFO(why);
	REQUIRE(why.find("no more than a read") != std::string::npos);

	// The same table at a write is one it would start on, so what the check
	// turned down is the level and not anything else about the route.
	static const Endpoint high[] = {
		{ Delete, "/api/v1/bouquets/{bouquet}", AuthLevel::Write,
		  "takes a bouquet away", NULL, HTTPD_PARAMS(params), NULL, &unreachedHandler, false, Answers200, HTTPD_NO_REFUSALS },
	};
	const RouteTable fine = { HTTPD_TABLE_N("fine", high, 1) };
	std::string nothing_wrong;
	REQUIRE(tableIsSane(fine, &nothing_wrong));
	REQUIRE(nothing_wrong.empty());
}

TEST_CASE("filling one half of a bouquet leaves the other half alone", "[write]")
{
	ShippedRoutes shipped;
	BoxFixture box;
	giveBouquets(box.channels);

	// One of each kind, and both of them in the first bouquet, so that a call
	// naming one half can be seen to leave the other where it was.
	coreapi::ChannelInfo radio = makeChannel(0x4c21ULL, "the radio one");
	radio.kind = coreapi::ServiceKind::Radio;
	box.channels.channels.push_back(radio);
	box.channels.bouquet_members[1].push_back(box.channels.channels[0]);
	box.channels.bouquet_members[1].push_back(radio);

	/* A channel the box does not have is refused, and the bouquet still holds
	   what it held. A handler that applied the list as it read it would have
	   emptied the television half and answered that the request failed. */
	const Reply absent = authedPut("/api/v1/bouquets/Erstes/channels?mode=tv",
	                               "[\"2b66\",\"dead\"]");
	REQUIRE(absent.code == 404);
	REQUIRE(absent.body.find("no-such-channel") != std::string::npos);
	REQUIRE(box.channels.bouquet_members[1].size() == 2u);
	REQUIRE(box.channels.saves == 0u);

	// A list that is not identifiers is refused before the box is asked at all.
	const Reply junk = authedPut("/api/v1/bouquets/Erstes/channels?mode=tv",
	                             "[\"2b66\",\"\",\"4c21\"]");
	REQUIRE(junk.code == 400);
	REQUIRE(box.channels.saves == 0u);

	// And so is a body that is not a list at all, which is what the body of
	// every other route here is.
	const Reply object = authedPut("/api/v1/bouquets/Erstes/channels?mode=tv",
	                               "{\"channel_ids\":\"2b66\"}");
	REQUIRE(object.code == 400);
	REQUIRE(box.channels.saves == 0u);

	// Which half the list is belongs to the request and not to the body, and a
	// caller that leaves it out is told it wrote half a request rather than
	// having the other half replaced.
	const Reply nohalf = authedPut("/api/v1/bouquets/Erstes/channels", "[\"2b66\"]");
	REQUIRE(nohalf.code == 400);
	REQUIRE(nohalf.body.find("missing-parameter") != std::string::npos);
	REQUIRE(box.channels.saves == 0u);

	REQUIRE(authedPut("/api/v1/bouquets/Erstes/channels?mode=tv",
	                  "[\"2b66\"]").code == 204);
	// Two, because the radio member stayed: the television half is what was
	// replaced and it now holds the one channel that was named.
	REQUIRE(box.channels.bouquet_members[1].size() == 2u);
	REQUIRE(box.channels.saves == 1u);
	REQUIRE(box.channels.reloads == 1u);

	const ::Json::Value now = bouquetNamed(authedGet("/api/v1/bouquets").body, "Erstes");
	REQUIRE(now["tv_count"].asUInt() == 1u);
	REQUIRE(now["radio_count"].asUInt() == 1u);
}

TEST_CASE("a half takes no channel of the other kind", "[write]")
{
	ShippedRoutes shipped;
	BoxFixture box;
	giveBouquets(box.channels);

	coreapi::ChannelInfo radio = makeChannel(0x4c21ULL, "the radio one");
	radio.kind = coreapi::ServiceKind::Radio;
	box.channels.channels.push_back(radio);
	box.channels.bouquet_members[1].push_back(box.channels.channels[0]);
	box.channels.bouquet_members[1].push_back(radio);

	/* The box files a channel by its own kind, so the television channel sent
	   for the radio half would have gone into the television half a second
	   time while the radio half was emptied, and the read back would only
	   have said so afterwards. Refused before the box is asked, both ways
	   round, with a code a page can tell from a channel that is not there. */
	const Reply tvForRadio = authedPut("/api/v1/bouquets/Erstes/channels?mode=radio",
	                                   "[\"2b66\"]");
	REQUIRE(tvForRadio.code == 400);
	REQUIRE(tvForRadio.body.find("wrong-kind-of-channel") != std::string::npos);
	const Reply radioForTv = authedPut("/api/v1/bouquets/Erstes/channels?mode=tv",
	                                   "[\"2b66\",\"4c21\"]");
	REQUIRE(radioForTv.code == 400);
	REQUIRE(radioForTv.body.find("wrong-kind-of-channel") != std::string::npos);
	REQUIRE(box.channels.bouquet_members[1].size() == 2u);
	REQUIRE(box.channels.saves == 0u);

	REQUIRE(authedPut("/api/v1/bouquets/Erstes/channels?mode=radio",
	                  "[\"4c21\"]").code == 204);
	REQUIRE(box.channels.saves == 1u);
}

TEST_CASE("a bouquet holds its channels in the order the list was written in", "[write]")
{
	/* What a caller sends is what it can read back. The half held whatever
	   order the box happened to put things in before, so a page that let
	   somebody sort a bouquet had nothing to send, and the numbers the box
	   draws beside a channel are counted off that order. */
	ShippedRoutes shipped;
	BoxFixture box;
	giveBouquets(box.channels);

	const uint64_t first = 0x2b66ULL;
	const uint64_t second = 0x4c21ULL;
	const uint64_t third = 0x9f07ULL;
	box.channels.channels.push_back(makeChannel(second, "the second"));
	box.channels.channels.push_back(makeChannel(third, "the third"));

	REQUIRE(authedPut("/api/v1/bouquets/Erstes/channels?mode=tv",
	                  "[\"9f07\",\"2b66\",\"4c21\"]").code == 204);

	const ::Json::Value written = parsed(authedGet("/api/v1/channels?bouquet=1").body)["items"];
	REQUIRE(written.size() == 3u);
	REQUIRE(written[0]["id"].asString() == hexOf(third));
	REQUIRE(written[1]["id"].asString() == hexOf(first));
	REQUIRE(written[2]["id"].asString() == hexOf(second));

	// The other way round, because a list that arrived in the order the half
	// already held would say nothing about whether the order was read at all.
	REQUIRE(authedPut("/api/v1/bouquets/Erstes/channels?mode=tv",
	                  "[\"4c21\",\"2b66\",\"9f07\"]").code == 204);

	const ::Json::Value again = parsed(authedGet("/api/v1/channels?bouquet=1").body)["items"];
	REQUIRE(again.size() == 3u);
	REQUIRE(again[0]["id"].asString() == hexOf(second));
	REQUIRE(again[1]["id"].asString() == hexOf(first));
	REQUIRE(again[2]["id"].asString() == hexOf(third));

	/* A channel named twice is kept where it was first named. Dropping the
	   second mention is what keeps a bouquet from holding one channel twice,
	   and keeping the first place is what keeps the rest of the order the
	   caller's. */
	REQUIRE(authedPut("/api/v1/bouquets/Erstes/channels?mode=tv",
	                  "[\"9f07\",\"2b66\",\"9f07\",\"4c21\"]").code == 204);

	const ::Json::Value twice = parsed(authedGet("/api/v1/channels?bouquet=1").body)["items"];
	REQUIRE(twice.size() == 3u);
	REQUIRE(twice[0]["id"].asString() == hexOf(third));
	REQUIRE(twice[1]["id"].asString() == hexOf(first));
	REQUIRE(twice[2]["id"].asString() == hexOf(second));
}

TEST_CASE("a list no single value could carry is one this route takes", "[write]")
{
	/* Three hundred channels, which as one value was more than this server
	   lets a value be: an identifier and its separator are seventeen bytes and
	   a value stops at four thousand and ninety six, so a provider's bouquet
	   could not be written at all. The body carries the list now, and what
	   bounds it is the number of channels the route states. */
	ShippedRoutes shipped;
	BoxFixture box;
	giveBouquets(box.channels);

	const size_t many = 300;
	const uint64_t base = 0x2b66000000010000ULL;
	std::string body = "[";
	for (size_t i = 0; i < many; ++i)
	{
		box.channels.channels.push_back(makeChannel(base + i, "one of many"));
		if (i > 0)
			body += ',';
		body += '"';
		body += hexOf(base + i);
		body += '"';
	}
	body += ']';

	// The ceiling that used to stop this, measured rather than asserted from
	// memory: a case that sent a short list would pass whatever the ceiling is.
	REQUIRE(body.size() > 4096u);

	REQUIRE(authedPut("/api/v1/bouquets/Erstes/channels?mode=tv", body).code == 204);

	const ::Json::Value now = bouquetNamed(authedGet("/api/v1/bouquets").body, "Erstes");
	REQUIRE(now["tv_count"].asUInt() == (unsigned) many);

	// And in the order they were written in, which is the other half of the
	// same request: a page of the listing reads the beginning of that order.
	const ::Json::Value page = parsed(authedGet("/api/v1/channels?bouquet=1&limit=3").body)["items"];
	REQUIRE(page.size() == 3u);
	REQUIRE(page[0]["id"].asString() == hexOf(base));
	REQUIRE(page[1]["id"].asString() == hexOf(base + 1));
	REQUIRE(page[2]["id"].asString() == hexOf(base + 2));
}

TEST_CASE("a list longer than a bouquet takes is refused by name and changes nothing", "[write]")
{
	/* A ceiling there has to be, and this is the one the route chose rather
	   than the one that fell out of how a value happens to travel. Named, so a
	   caller can tell it from a body that is wrong, and refused rather than
	   met by taking the first four thousand of them. */
	ShippedRoutes shipped;
	BoxFixture box;
	giveBouquets(box.channels);
	box.channels.bouquet_members[1].push_back(box.channels.channels[0]);

	const uint64_t base = 0x2b66000000010000ULL;
	std::string body = "[";
	for (size_t i = 0; i <= 4096; ++i)
	{
		if (i > 0)
			body += ',';
		body += '"';
		body += hexOf(base + i);
		body += '"';
	}
	body += ']';

	const Reply over = authedPut("/api/v1/bouquets/Erstes/channels?mode=tv", body);
	REQUIRE(over.code == 400);
	REQUIRE(over.body.find("too-many-channels") != std::string::npos);
	// The number the route states, out of the answer, so the sentence and the
	// ceiling cannot drift apart with nothing said.
	REQUIRE(over.body.find("4096") != std::string::npos);

	// Nothing of it was applied, and none of those channels exists on this
	// box: a route that read the list first would have answered no-such-channel
	// and told a caller to go looking for a channel rather than to send less.
	REQUIRE(box.channels.bouquet_members[1].size() == 1u);
	REQUIRE(box.channels.saves == 0u);

	// One below the ceiling is a request, so the refusal is the ceiling and not
	// everything above some smaller number.
	std::string under = body.substr(0, body.rfind(','));
	under += ']';
	const Reply at = authedPut("/api/v1/bouquets/Erstes/channels?mode=tv", under);
	REQUIRE(at.code == 404);
	REQUIRE(at.body.find("no-such-channel") != std::string::npos);
}

TEST_CASE("a bouquet write the box never received is not answered as done", "[write]")
{
	/* The one answer none of the seven may give. With the channel daemon's socket gone
	   the box goes on running and every command written to it is written into nothing,
	   and each of these answered that it had done what it was asked while the bouquet
	   list stood still. */
	ShippedRoutes shipped;
	BoxFixture box;
	giveBouquets(box.channels);
	box.channels.bouquet_members[1].push_back(box.channels.channels[0]);
	box.channels.write_status = coreapi::Status::Internal;

	Reply answers[7];
	answers[0] = authedPost("/api/v1/bouquets", "{\"name\":\"Drittes\"}");
	answers[1] = authedDelete("/api/v1/bouquets/Zweites");
	answers[2] = authedPut("/api/v1/bouquets/Erstes/name", "{\"name\":\"Anders\"}");
	answers[3] = authedPut("/api/v1/bouquets/Erstes/position", "{\"direction\":\"down\"}");
	answers[4] = authedPut("/api/v1/bouquets/Erstes/hidden", "{\"on\":true}");
	answers[5] = authedPut("/api/v1/bouquets/Erstes/locked", "{\"on\":true}");
	answers[6] = authedPut("/api/v1/bouquets/Erstes/channels?mode=tv",
	                       "[\"" + hexOf(kChannel) + "\"]");

	for (size_t i = 0; i < sizeof(answers) / sizeof(answers[0]); ++i)
	{
		INFO("answer " << i);
		// Named rather than left to the code alone, because those two are what
		// a caller reads as done and this server has no third way of saying it.
		REQUIRE(answers[i].code != 201);
		REQUIRE(answers[i].code != 204);
		REQUIRE(answers[i].code == 500);
		REQUIRE(answers[i].body.find("bouquet-not-changed") != std::string::npos);
	}

	// And the list is where it was, which is the half the code alone cannot
	// say: an answer that refused and changed something would read the same.
	const std::string all = authedGet("/api/v1/bouquets").body;
	REQUIRE(bouquetNamed(all, "Drittes").isNull());
	REQUIRE_FALSE(bouquetNamed(all, "Zweites").isNull());
	REQUIRE(bouquetNamed(all, "Anders").isNull());
	REQUIRE(bouquetNamed(all, "Erstes")["id"].asUInt() == 1u);
	REQUIRE_FALSE(bouquetNamed(all, "Erstes")["hidden"].asBool());
	REQUIRE_FALSE(bouquetNamed(all, "Erstes")["locked"].asBool());
	REQUIRE(box.channels.bouquet_members[1].size() == 1u);

	// Nothing was written out or read back either, because a change that did
	// not happen has nothing to save.
	REQUIRE(box.channels.saves == 0u);
	REQUIRE(box.channels.reloads == 0u);
}

TEST_CASE("a bouquet write whose save failed answers that and not a refusal", "[write]")
{
	/* The bouquet is made and in the list the box runs, and only the files may
	   not hold it. Answered as not changed, a client would make it again and be
	   told the name is taken. */
	ShippedRoutes shipped;
	BoxFixture box;
	giveBouquets(box.channels);
	box.channels.save_status = coreapi::Status::Internal;

	const Reply made = authedPost("/api/v1/bouquets", "{\"name\":\"Drittes\"}");
	REQUIRE(made.code == 500);
	REQUIRE(made.body.find("bouquet-not-saved") != std::string::npos);
	REQUIRE(made.body.find("bouquet-not-changed") == std::string::npos);

	const std::string all = authedGet("/api/v1/bouquets").body;
	REQUIRE_FALSE(bouquetNamed(all, "Drittes").isNull());
}

TEST_CASE("every bouquet write states a failed save the same way", "[write]")
{
	/* The six that change a bouquet that is there end in the writing out the
	   make above ends in, so a save that failed is the same answer from each:
	   the change is in the list the box runs and the files may not hold it.
	   Each is a route of its own in the document and states that answer, so
	   each is sent one here. In this order the rename's new name is the one
	   the removal finds, because the list carries the rename all the same. */
	ShippedRoutes shipped;
	BoxFixture box;
	giveBouquets(box.channels);
	box.channels.save_status = coreapi::Status::Internal;

	const Reply replies[] = {
		authedPut("/api/v1/bouquets/Zweites/hidden", "{\"on\":true}"),
		authedPut("/api/v1/bouquets/Zweites/locked", "{\"on\":true}"),
		authedPut("/api/v1/bouquets/Zweites/position", "{\"direction\":\"up\"}"),
		authedPut("/api/v1/bouquets/Zweites/channels?mode=tv", "[\"2b66\"]"),
		authedPut("/api/v1/bouquets/Zweites/name", "{\"name\":\"Anders\"}"),
		authedDelete("/api/v1/bouquets/Anders"),
	};
	for (size_t i = 0; i < sizeof(replies) / sizeof(replies[0]); ++i)
	{
		INFO("write " << i);
		REQUIRE(replies[i].code == 500);
		REQUIRE(replies[i].body.find("bouquet-not-saved") != std::string::npos);
		REQUIRE(replies[i].body.find("bouquet-not-changed") == std::string::npos);
	}
}

TEST_CASE("a list that would take a channel off the box asks first", "[write]")
{
	/* The box forgets a channel that leaves the last bouquet holding it. Only
	   Erstes holds the channel here, so a list without it is refused with the
	   bouquet as it was and nothing written, and the same list said to take it
	   off the box goes through. */
	ShippedRoutes shipped;
	BoxFixture box;
	giveBouquets(box.channels);
	box.channels.bouquet_members[1].push_back(box.channels.channels[0]);

	const Reply asked = authedPut("/api/v1/bouquets/Erstes/channels?mode=tv", "[]");
	REQUIRE(asked.code == 409);
	REQUIRE(asked.body.find("channel-would-be-removed") != std::string::npos);
	REQUIRE(box.channels.bouquet_members[1].size() == 1u);
	REQUIRE(box.channels.saves == 0u);

	// Said not to, in so many words, it is asked all the same.
	REQUIRE(authedPut("/api/v1/bouquets/Erstes/channels?mode=tv&remove_from_box=false", "[]").code == 409);
	REQUIRE(box.channels.bouquet_members[1].size() == 1u);

	const Reply told = authedPut("/api/v1/bouquets/Erstes/channels?mode=tv&remove_from_box=true", "[]");
	REQUIRE(told.code == 204);
	REQUIRE(box.channels.bouquet_members[1].empty());
}

TEST_CASE("an empty list empties that half and a body carrying no list does not", "[write]")
{
	/* The two belong in one case, because what separates them is the whole of the
	   rule. A body that was never written and a body written as an empty array were
	   answered alike, the reason being that a half nobody meant to empty must not be
	   emptied. That holds for the first and not for the second: the body of this
	   route is the list, so a request that forgot it carries no array at all. */
	ShippedRoutes shipped;
	BoxFixture box;
	giveBouquets(box.channels);

	coreapi::ChannelInfo radio = makeChannel(0x4c21ULL, "the radio one");
	radio.kind = coreapi::ServiceKind::Radio;
	box.channels.channels.push_back(radio);
	box.channels.bouquet_members[1].push_back(box.channels.channels[0]);
	box.channels.bouquet_members[1].push_back(radio);

	// No list at all, which is what a client that built its body wrong sends.
	const Reply nothing = authedPut("/api/v1/bouquets/Erstes/channels?mode=tv", "");
	REQUIRE(nothing.code == 400);
	REQUIRE(nothing.body.find("bad-string") != std::string::npos);
	REQUIRE(box.channels.bouquet_members[1].size() == 2u);
	REQUIRE(box.channels.saves == 0u);

	// Nor is the half the request is addressed to something that may be left
	// out: that is a parameter the route requires, and a request without it is
	// turned away before a handler is entered, whatever its body says.
	const Reply nohalf = authedPut("/api/v1/bouquets/Erstes/channels", "[]");
	REQUIRE(nohalf.code == 400);
	REQUIRE(nohalf.body.find("missing-parameter") != std::string::npos);
	REQUIRE(box.channels.bouquet_members[1].size() == 2u);
	REQUIRE(box.channels.saves == 0u);

	// The same route and the same bouquet as the refusal above, one written
	// pair of brackets apart. The channel leaves its only bouquet, which the
	// request says to do: the box would forget it, and that is asked first.
	const Reply emptied = authedPut("/api/v1/bouquets/Erstes/channels?mode=tv&remove_from_box=true", "[]");
	REQUIRE(emptied.code == 204);
	REQUIRE(box.channels.saves == 1u);
	REQUIRE(box.channels.reloads == 1u);

	/* The television half holds nothing and the radio half is where it was,
	   because a list is one half of a bouquet and never both. A route that
	   read an empty list as both would take a bouquet apart. */
	const ::Json::Value now = bouquetNamed(authedGet("/api/v1/bouquets").body, "Erstes");
	REQUIRE(now["tv_count"].asUInt() == 0u);
	REQUIRE(now["radio_count"].asUInt() == 1u);
	REQUIRE(box.channels.bouquet_members[1].size() == 1u);

	// And the channel it held is still on the box, because taking a channel
	// out of a bouquet is not taking it off the box.
	REQUIRE(authedGet("/api/v1/channels/" + hexOf(kChannel)).code == 200);
}

TEST_CASE("a bouquet at the end of the list is not moved past it", "[write]")
{
	ShippedRoutes shipped;
	BoxFixture box;
	giveBouquets(box.channels);

	const Reply top = authedPut("/api/v1/bouquets/Erstes/position", "{\"direction\":\"up\"}");
	REQUIRE(top.code == 400);
	REQUIRE(top.body.find("already-at-the-end") != std::string::npos);

	const Reply bottom = authedPut("/api/v1/bouquets/Zweites/position",
	                               "{\"direction\":\"down\"}");
	REQUIRE(bottom.code == 400);
	REQUIRE(bottom.body.find("already-at-the-end") != std::string::npos);

	// And the list is where it was, which an answer that shifted nothing and
	// said ok would look exactly like from the outside.
	const std::string all = authedGet("/api/v1/bouquets").body;
	REQUIRE(bouquetNamed(all, "Erstes")["id"].asUInt() == 1u);
	REQUIRE(bouquetNamed(all, "Zweites")["id"].asUInt() == 2u);
	REQUIRE(box.channels.saves == 0u);
}


namespace
{

// nhttpd.conf as a case starts from: where the server answers, and a line of
// nhttpd's own a save leaves alone.
const char kServerFileHead[] = "WebsiteMain.host=0.0.0.0\nWebsiteMain.port=8081\nconfigfile.version=5\n";

/* A configuration of this case's own, in files of its own, installed the way the
   program installs one: through the read that also records where the files are,
   which is what the route writes back into, the access file for the account and
   nhttpd.conf for where the server answers. What is in effect is put back on the
   way out, because it is a global of this program. */
struct WebFile
{
	std::string path;
	std::string server;
	WebConfig   before;

	static void put(const std::string &where, const std::string &text)
	{
		std::FILE *f = std::fopen(where.c_str(), "w");
		REQUIRE(f != NULL);
		REQUIRE(std::fwrite(text.data(), 1, text.size(), f) == text.size());
		REQUIRE(std::fclose(f) == 0);
	}

	WebFile(const char *tag, const std::string &text,
	        const std::string &server_text = kServerFileHead) : before(config())
	{
		char name[160];
		std::snprintf(name, sizeof(name), "/tmp/ni-webroute-%s-%d", tag, (int) getpid());
		path = name;
		server = path + ".nhttpd";

		put(path, text);
		put(server, server_text);

		REQUIRE(load(path, server));
		REQUIRE(configPath() == path);
		REQUIRE(serverPath() == server);
	}

	~WebFile()
	{
		::unlink(path.c_str());
		::unlink(server.c_str());
		setConfigForTest(before);
	}

	private:
		WebFile(const WebFile &);
		WebFile &operator=(const WebFile &);
};

std::string wholeOf(const std::string &path)
{
	std::string out;
	std::FILE *f = std::fopen(path.c_str(), "r");
	if (f == NULL)
		return out;
	char buf[1024];
	size_t got = 0;
	while ((got = std::fread(buf, 1, sizeof(buf), f)) > 0)
		out.append(buf, got);
	std::fclose(f);
	return out;
}

/* The whole answer and not the part a Reply keeps, because what half these
   cases are about is the field of it that never reaches a wire: what the
   transport is to put the server on once the answer has gone. */
Response putWebserver(const std::string &body,
                      const std::string &host = "box.example:8081",
                      AuthLevel as = AuthLevel::System)
{
	return dispatch(Put, "/api/v1/system/webserver", "", body, "192.0.2.7", as,
	                std::string(), std::string(), host);
}

const char kWebFileHead[] = "username=root\n";

/* Every member the shape names is in the answer and nothing else is, one level down as
   well.

   A walk of its own and not the one the read cases keep, which is private to that file:
   what the two share is a rule rather than a function, and a third file for a dozen lines
   would be a place for the rule to go stale in. */
void answerFitsShape(const ::Json::Value &v, const Schema &sch, const std::string &where)
{
	INFO(where << " against " << sch.name);
	REQUIRE(v.isObject());

	for (size_t i = 0; i < sch.count; ++i)
	{
		const FieldDesc &f = sch.fields[i];
		INFO("member " << f.name);
		if (!f.optional)
			REQUIRE(v.isMember(f.name));
		if (v.isMember(f.name) && f.type == FieldType::Object && f.nested != NULL)
			answerFitsShape(v[f.name], *f.nested, where + "." + f.name);
	}

	const std::vector<std::string> named = v.getMemberNames();
	for (size_t i = 0; i < named.size(); ++i)
	{
		INFO("member " << named[i] << " is in the answer and in no row of the shape");
		bool declared = false;
		for (size_t k = 0; k < sch.count; ++k)
			declared = declared || named[i] == sch.fields[k].name;
		REQUIRE(declared);
	}
}

} // namespace

TEST_CASE("moving the port says where the box will be, and moves it afterwards", "[write]")
{
	/* The first of the four ways this route can lock its caller out. The
	   connection this answer arrives on is the last one at the old address, and
	   nothing else in this program would tell the caller where to go next. */
	ShippedRoutes shipped;
	WebFile file("port", kWebFileHead);

	const Response r = putWebserver("{\"port\":8099}");
	REQUIRE(r.code == 200);

	const ::Json::Value root = parsed(r.body);
	REQUIRE(root["webserver"]["port"].asInt() == 8099);
	REQUIRE(root["caller"]["address"].asString() == "http://box.example:8099/");
	REQUIRE(root["caller"]["port_moved"].asBool());
	REQUIRE(root["caller"]["restarting"].asBool());

	// nhttpd.conf says so before the answer does anything about it, and the
	// access file does not say it at all.
	REQUIRE(wholeOf(file.server).find("WebsiteMain.port=8099\n") != std::string::npos);
	REQUIRE(wholeOf(file.path).find("port=") == std::string::npos);

	/* And the server is not on it yet. The answer names the file the transport
	   is to read once these bytes have gone out, and that is the whole of the
	   order this route exists to keep. */
	REQUIRE(r.reload_after == file.path);
	REQUIRE(config().server.port == 8081);
}

TEST_CASE("a second write before the restart still says what the restart will do", "[write]")
{
	/* The first write waits for a restart that runs once its answer is gone. A
	   caller writing again in that window is answered about the server as it
	   runs, and the first write's part of the coming restart is part of that. */
	ShippedRoutes shipped;
	WebFile file("second", kWebFileHead);

	const Response first = putWebserver("{\"port\":8099,\"password\":\"another-one\"}");
	REQUIRE(first.code == 200);
	REQUIRE(first.reload_after == file.path);

	const Response second = putWebserver("{\"port\":8099}");
	REQUIRE(second.code == 200);
	const ::Json::Value root = parsed(second.body);
	REQUIRE(root["caller"]["port_moved"].asBool());
	REQUIRE(root["caller"]["password_changed"].asBool());
	REQUIRE(root["caller"]["restarting"].asBool());

	// A write naming only the logo switch starts from what the first one wrote.
	const Response third = putWebserver("{\"channel_logos\":true}");
	REQUIRE(third.code == 200);
	const ::Json::Value after = parsed(third.body);
	REQUIRE(after["webserver"]["port"].asInt() == 8099);
	REQUIRE(after["caller"]["restarting"].asBool());
	REQUIRE(wholeOf(file.server).find("WebsiteMain.port=8099\n") != std::string::npos);

	// Nothing of it is in effect before the restart reads the file.
	REQUIRE(config().server.port == 8081);
}

TEST_CASE("narrowing what the server listens on is said before it happens", "[write]")
{
	/* The second way out. A caller that is not on the loopback will not reach
	   this box again after this, and the answer is the last thing it hears. */
	ShippedRoutes shipped;
	WebFile file("bind", kWebFileHead);

	const Response r = putWebserver("{\"bind\":\"127.0.0.1\"}");
	REQUIRE(r.code == 200);

	const ::Json::Value root = parsed(r.body);
	REQUIRE(root["webserver"]["bind"].asString() == "127.0.0.1");
	// The port did not move, so the address a caller holds is still an address.
	REQUIRE_FALSE(root["caller"]["port_moved"].asBool());
	REQUIRE(root["caller"]["restarting"].asBool());
	REQUIRE(r.reload_after == file.path);
	REQUIRE(wholeOf(file.server).find("WebsiteMain.host=127.0.0.1\n") != std::string::npos);
}

TEST_CASE("an address nhttpd would not listen on as written is turned down", "[write]")
{
	/* nhttpd listens on addresses of the first family only, and what it cannot
	   use there it answers by listening on every interface: a caller asking for
	   one would get all of them. */
	ShippedRoutes shipped;
	WebFile file("bind6", kWebFileHead);

	const std::string before = wholeOf(file.server);
	const Response r = putWebserver("{\"bind\":\"::1\"}");
	REQUIRE(r.code == 400);
	REQUIRE(r.reload_after.empty());
	REQUIRE(r.body.find("WebsiteMain.host") != std::string::npos);
	REQUIRE(wholeOf(file.server) == before);
}

TEST_CASE("a port moved in the menu at the box is not moved back by a write", "[write]")
{
	/* The menu writes nhttpd.conf without passing through this route. A write
	   naming something else starts from what that file says now, and the coming
	   restart is what puts the server on the port the menu chose: the caller is
	   told so, the port it reached the box on not being the one it will. */
	ShippedRoutes shipped;
	WebFile file("menu", kWebFileHead);

	WebFile::put(file.server, "WebsiteMain.host=0.0.0.0\nWebsiteMain.port=8090\nconfigfile.version=5\n");

	const Response r = putWebserver("{\"user\":\"admin\"}");
	REQUIRE(r.code == 200);
	const ::Json::Value root = parsed(r.body);
	REQUIRE(root["webserver"]["port"].asInt() == 8090);
	REQUIRE(root["caller"]["port_moved"].asBool());
	REQUIRE(root["caller"]["restarting"].asBool());
	REQUIRE(wholeOf(file.server).find("WebsiteMain.port=8090\n") != std::string::npos);

	/* And a caller that writes back the document it read: the read answers what
	   nhttpd.conf says, the same reading the write starts from, so restating it
	   restates the menu's port and not the one still bound. */
	const Reply read = authedGet("/api/v1/system/webserver");
	REQUIRE(read.code == 200);
	REQUIRE(parsed(read.body)["port"].asInt() == 8090);
	const Response restated = putWebserver("{\"port\":8090,\"user\":\"root\"}");
	REQUIRE(restated.code == 200);
	REQUIRE(wholeOf(file.server).find("WebsiteMain.port=8090\n") != std::string::npos);
}

TEST_CASE("a new password is taken as a password and answered as neither form", "[write]")
{
	/* The third way out, and the one with something to hide. What goes in is a
	   password; what the file holds is the stored form; what the answer says is
	   that the next sign in needs a different one. */
	ShippedRoutes shipped;
	WebFile file("password", kWebFileHead);

	// held to the rules every writer of the password is held to
	const std::string untouched = wholeOf(file.path);
	const char *const refused[] = { "{\"password\":\"short\"}", "{\"password\":\"coolstream\"}" };
	for (size_t i = 0; i < sizeof(refused) / sizeof(refused[0]); ++i)
	{
		INFO(refused[i]);
		const Response no = putWebserver(refused[i]);
		REQUIRE(no.code == 400);
		REQUIRE(no.body.find("bad-string") != std::string::npos);
		REQUIRE(wholeOf(file.path) == untouched);
	}

	const Response r = putWebserver("{\"password\":\"a password nobody guesses\"}");
	REQUIRE(r.code == 200);

	const ::Json::Value root = parsed(r.body);
	REQUIRE(root["caller"]["password_changed"].asBool());
	REQUIRE(root["webserver"]["has_password"].asBool());
	REQUIRE(r.body.find("a password nobody guesses") == std::string::npos);
	REQUIRE(r.body.find("pbkdf2") == std::string::npos);
	REQUIRE(r.reload_after == file.path);

	/* The file carries the stored form and never the value, which is what
	   verifies against it. */
	const std::string written = wholeOf(file.path);
	REQUIRE(written.find("a password nobody guesses") == std::string::npos);
	const size_t at = written.find("password_hash=");
	REQUIRE(at != std::string::npos);
	const size_t end = written.find('\n', at);
	REQUIRE(end != std::string::npos);
	const std::string stored = written.substr(at + 14, end - at - 14);
	REQUIRE(verifySecret("a password nobody guesses", stored));
}

TEST_CASE("the networks that read without a password are not narrowed from here", "[write]")
{
	/* The fourth way out, and the one this route does not offer. Narrowing that
	   list locks out every caller on the network at once rather than the one
	   asking, and the line belongs to the save that owns it. A body naming it is
	   a body naming something this route does not take, and nothing is written
	   at all. */
	ShippedRoutes shipped;
	WebFile file("lanread", kWebFileHead);

	const std::string before = wholeOf(file.path);
	const Response r = putWebserver("{\"lan_read\":\"127.0.0.1/32\"}");
	REQUIRE(r.code == 400);
	REQUIRE(r.reload_after.empty());
	REQUIRE(wholeOf(file.path) == before);

	// And the list is still the one a read answers with.
	const Response read = dispatch(Get, "/api/v1/system/webserver", "", "", "192.0.2.7",
	                               AuthLevel::System);
	REQUIRE(read.code == 200);
	REQUIRE(parsed(read.body)["lan_read"].size() > 0u);
}

TEST_CASE("the lines of nhttpd.conf the settings page offers are read and written here", "[write]")
{
	/* Whether the pages ask for the login and of whom not, and how nhttpd serves
	   them: the page, the menu at the box and nhttpd read one file, and nhttpd
	   reads it when it starts, so a change restarts it as a new port does. */
	ShippedRoutes shipped;
	WebFile file("options", kWebFileHead);

	const Reply read = authedGet("/api/v1/system/webserver");
	REQUIRE(read.code == 200);
	const ::Json::Value was = parsed(read.body);
	REQUIRE_FALSE(was["page_login"].asBool());
	REQUIRE(was["no_login_prefix"].asString().empty());
	REQUIRE(was["threading"].asBool());
	REQUIRE(was["send_all_types"].asBool());
	REQUIRE(was["hosted_directory"].asString() == ServerOptions().hosted_directory);
	REQUIRE(was.isMember("password_shipped"));

	const Response r = putWebserver("{\"page_login\":true,\"no_login_prefix\":\"10.0.\","
	                                "\"hosted_directory\":\"/media/hdd/web\"}");
	REQUIRE(r.code == 200);
	const ::Json::Value root = parsed(r.body);
	REQUIRE(root["webserver"]["page_login"].asBool());
	REQUIRE(root["webserver"]["no_login_prefix"].asString() == "10.0.");
	REQUIRE(root["webserver"]["hosted_directory"].asString() == "/media/hdd/web");
	REQUIRE_FALSE(root["caller"]["port_moved"].asBool());
	REQUIRE(root["caller"]["restarting"].asBool());
	REQUIRE(r.reload_after == file.path);

	const std::string server = wholeOf(file.server);
	REQUIRE(server.find("mod_auth.authenticate=true\n") != std::string::npos);
	REQUIRE(server.find("mod_auth.no_auth_client=10.0.\n") != std::string::npos);
	REQUIRE(server.find("WebsiteMain.hosted_directory=/media/hdd/web\n") != std::string::npos);
	REQUIRE(wholeOf(file.path).find("authenticate") == std::string::npos);

	// What the file already says arms no restart of its own.
	const Response same = putWebserver("{\"page_login\":true,\"threading\":true}");
	REQUIRE(same.code == 200);
	REQUIRE(same.reload_after.empty());

	// An empty prefix is a value, the one that exempts nobody: it takes the
	// prefix away again.
	const Response cleared = putWebserver("{\"no_login_prefix\":\"\"}");
	REQUIRE(cleared.code == 200);
	REQUIRE(parsed(cleared.body)["webserver"]["no_login_prefix"].asString().empty());
	REQUIRE(wholeOf(file.server).find("mod_auth.no_auth_client=\n") != std::string::npos);
	REQUIRE(cleared.reload_after == file.path);
}

TEST_CASE("an option nhttpd could not use as written is turned down", "[write]")
{
	ShippedRoutes shipped;
	WebFile file("badoptions", kWebFileHead);

	const std::string before = wholeOf(file.server);
	const char *const bodies[] = {
		"{\"no_login_prefix\":\"192.168.1.0/24\"}",
		"{\"override_directory\":\"web\"}",
		"{\"hosted_directory\":\"/media/../etc\"}",
	};
	for (size_t i = 0; i < sizeof(bodies) / sizeof(bodies[0]); ++i)
	{
		INFO(bodies[i]);
		const Response r = putWebserver(bodies[i]);
		REQUIRE(r.code == 400);
		REQUIRE(r.reload_after.empty());
		REQUIRE(wholeOf(file.server) == before);
	}
}

TEST_CASE("a change that could not be saved does not move the server", "[write]")
{
	/* Otherwise the box runs until its next start on something its file does not
	   say, which is a state nobody can see from outside and which the next start
	   silently takes back. */
	ShippedRoutes shipped;
	WebFile file("refused", kWebFileHead);

	const std::string before = wholeOf(file.path);
	const std::string before_server = wholeOf(file.server);
	const Response r = putWebserver("{\"bind\":\"not an address\"}");
	REQUIRE(r.code == 400);
	REQUIRE(r.content_type == "application/problem+json");
	REQUIRE(r.reload_after.empty());
	REQUIRE(wholeOf(file.path) == before);
	REQUIRE(wholeOf(file.server) == before_server);

	// The box says which value it was and not only that one was.
	REQUIRE(r.body.find("WebsiteMain.host") != std::string::npos);
}

TEST_CASE("asking for what is already in effect takes the box off nothing", "[write]")
{
	/* A restart for a change that is not one would be a way to take the box off
	   the network for a moment by asking it politely for what it already has. */
	ShippedRoutes shipped;
	WebFile file("same", kWebFileHead);

	const Response r = putWebserver("{\"port\":8081,\"user\":\"root\"}");
	REQUIRE(r.code == 200);

	const ::Json::Value root = parsed(r.body);
	REQUIRE_FALSE(root["caller"]["port_moved"].asBool());
	REQUIRE_FALSE(root["caller"]["restarting"].asBool());
	REQUIRE(r.reload_after.empty());
}

TEST_CASE("a body naming nothing is not a way to restart the server", "[write]")
{
	ShippedRoutes shipped;
	WebFile file("empty", kWebFileHead);

	const Response r = putWebserver("{}");
	REQUIRE(r.code == 400);
	REQUIRE(r.reload_after.empty());
}

TEST_CASE("what a request does not name keeps the value it had", "[write]")
{
	/* A caller changing a port has no business restating a user name it was not
	   asked about, and one that had to would write back whatever it last read.
	   An empty member is a different thing: that is a value the caller wrote,
	   and the save turns an empty one down. */
	ShippedRoutes shipped;
	WebFile file("kept", "username=someone\n");

	const Response r = putWebserver("{\"port\":8099}");
	REQUIRE(r.code == 200);
	REQUIRE(parsed(r.body)["webserver"]["user"].asString() == "someone");
	REQUIRE(wholeOf(file.path).find("username=someone") != std::string::npos);

	const Response emptied = putWebserver("{\"user\":\"\"}");
	REQUIRE(emptied.code == 400);
	REQUIRE(emptied.reload_after.empty());
}

TEST_CASE("setting the server up asks for what the calls that stop the box ask for", "[write]")
{
	ShippedRoutes shipped;
	WebFile file("level", kWebFileHead);

	const std::string before = wholeOf(file.path);
	const Response r = putWebserver("{\"port\":8099}", "box.example:8081", AuthLevel::Write);
	REQUIRE(r.code == 403);
	REQUIRE(r.reload_after.empty());
	REQUIRE(wholeOf(file.path) == before);
}

TEST_CASE("the answer to a write is the shape the document states for it", "[write]")
{
	ShippedRoutes shipped;
	WebFile file("shape", kWebFileHead);

	const Response r = putWebserver("{\"port\":8099}");
	REQUIRE(r.code == 200);

	const Endpoint *ep = routeFor(Put, "/api/v1/system/webserver");
	REQUIRE(ep != NULL);
	REQUIRE(ep->schema != NULL);
	const char *why = "";
	INFO(why);
	REQUIRE(schemaIsSane(*ep->schema, &why));
	answerFitsShape(parsed(r.body), *ep->schema, "PUT /api/v1/system/webserver");
}

TEST_CASE("a request that named no authority is handed no address it made up", "[write]")
{
	/* A name this side invented reads to a caller exactly like the one it gave,
	   so a handler with no authority to build on says it has none. */
	ShippedRoutes shipped;
	WebFile file("noauthority", kWebFileHead);

	const Response r = putWebserver("{\"port\":8099}", std::string());
	REQUIRE(r.code == 200);
	REQUIRE(parsed(r.body)["caller"]["address"].asString().empty());
}

TEST_CASE("every table this server ships is still one it can start on", "[write]")
{
	/* Two tables can carry one answer between them and neither of them is wrong
	   read alone, so the routes added here are checked against every route
	   there was as well as against each other. */
	ShippedRoutes shipped;

	size_t tables = 0;
	const RouteTable *const *t = allRoutes(&tables);
	REQUIRE(tables >= 10);

	std::string why;
	const bool sane = tablesAreSane(t, tables, &why);
	INFO(why);
	REQUIRE(sane);
}

TEST_CASE("the routes that drive the plugins ask for what their acts are worth", "[write]")
{
	/* Read off the shipped tables rather than written out here. Starting a
	   plugin and running a script are the whole of the box; reloading the
	   list is a write. The walk above counts them; this says which is which. */
	ShippedRoutes shipped;

	size_t tables = 0;
	const RouteTable *const *t = allRoutes(&tables);
	size_t seen = 0;
	for (size_t i = 0; i < tables; ++i)
	{
		for (size_t j = 0; j < t[i]->count; ++j)
		{
			const Endpoint &e = t[i]->endpoints[j];
			const std::string path = e.path;
			INFO(path);
			if (path == "/api/v1/scripts/{name}" || path == "/api/v1/plugins/{name}/start")
			{
				REQUIRE(e.auth == AuthLevel::System);
				++seen;
			}
			else if (path == "/api/v1/plugins/reload")
			{
				REQUIRE(e.auth == AuthLevel::Write);
				++seen;
			}
			else if (path == "/api/v1/plugins")
			{
				REQUIRE(e.auth == AuthLevel::Read);
				++seen;
			}
		}
	}
	// A walk that found none of them would pass every line above.
	REQUIRE(seen == 4);
}

TEST_CASE("the ceiling the document states for a bouquet's list is the one the server keeps", "[write]")
{
	/* Not that a number is there, but that it is the right one. Both sides are
	   read rather than written: the document says how many, and the server is
	   asked with exactly that many and with one more. A document saying seven
	   or forty thousand fails here, and a case comparing the document against a
	   constant typed beside it would pass with either. */
	ShippedRoutes shipped;
	BoxFixture box;
	giveBouquets(box.channels);
	box.channels.bouquet_members[1].push_back(box.channels.channels[0]);

	const long most = documentSays("/api/v1/bouquets/{bouquet}/channels", "put", "maxItems");
	REQUIRE(most > 0);

	const uint64_t base = 0x2b66000000010000ULL;
	const Reply over = authedPut("/api/v1/bouquets/Erstes/channels?mode=tv",
	                             channelList(base, most + 1, false));
	REQUIRE(over.code == 400);
	REQUIRE(over.body.find("too-many-channels") != std::string::npos);

	/* And exactly that many is not too many. The last of them is not
	   hexadecimal, so the answer names that instead of walking four thousand
	   identifiers against a source that holds none of them: what is asked here
	   is that the refusal is no longer about how many there are. */
	const Reply at = authedPut("/api/v1/bouquets/Erstes/channels?mode=tv",
	                           channelList(base, most, true));
	REQUIRE(at.code == 400);
	REQUIRE(at.body.find("too-many-channels") == std::string::npos);

	REQUIRE(box.channels.bouquet_members[1].size() == 1u);
	REQUIRE(box.channels.saves == 0u);
}

TEST_CASE("the floor the document states for a bouquet's list is the one the server keeps", "[write]")
{
	/* Asked of both sides rather than written down, because this is the number
	   that has moved once already: the branch that lets an empty list empty a
	   half took the floor from one to none. A tree where one of the two moved
	   and the other did not fails here, rather than shipping a page that offers
	   what the box turns down or hides what it takes. */
	ShippedRoutes shipped;
	BoxFixture box;
	giveBouquets(box.channels);
	box.channels.bouquet_members[1].push_back(box.channels.channels[0]);

	// Said to take the channel off the box, so the answer turns on the floor alone.
	const long least = documentSays("/api/v1/bouquets/{bouquet}/channels", "put", "minItems");
	const Reply empty = authedPut("/api/v1/bouquets/Erstes/channels?mode=tv&remove_from_box=true", "[]");
	REQUIRE((empty.code >= 400) == (least > 0));
}

TEST_CASE("the document describes the settings body as the server reads it", "[write]")
{
	// Three numbers and one kind, each asked of both sides.
	ShippedRoutes shipped;
	BoxFixture box;

	const long most  = documentSays("/api/v1/settings/{section}", "patch", "maxProperties");
	const long least = documentSays("/api/v1/settings/{section}", "patch", "minProperties");
	REQUIRE(most > 0);

	/* A section nobody declares, so the answer turns on the body alone: the
	   body reader runs first and a body it refuses never reaches the section
	   check. One over the ceiling is a body this server will not read; exactly
	   the ceiling gets as far as a section that is not there. */
	const Reply over = authedPatch("/api/v1/settings/no-such-page", settingsObject(most + 1));
	REQUIRE(over.code == 400);

	const Reply at = authedPatch("/api/v1/settings/no-such-page", settingsObject(most));
	REQUIRE(at.code == 404);

	// The floor, the same way.
	const Reply none = authedPatch("/api/v1/settings/audio", "{}");
	REQUIRE((none.code >= 400) == (least > 0));

	/* And the kind: what the document says a value is, is what the route takes.
	   A setting read as text and written as text is what lets a page show a
	   value and send it again. */
	const ::Json::Value value =
		parsed(openapi::document())["paths"]["/api/v1/settings/{section}"]["patch"]
			["requestBody"]["content"]["application/json"]["schema"]["additionalProperties"];
	REQUIRE(value["type"].asString() == "string");
	REQUIRE(authedPatch("/api/v1/settings/audio",
	                    "{\"audio_volume_percent_ac3\":\"50\"}").code == 200);
}

/* The tuner put down and up again, which is what somebody reaches for when the
   picture has gone and the box has carried on as though it had not.

   A fixture of its own, because the one the rest of this file uses installs the
   five seams a body driven route needs and the tuner is not one of them. What
   is installed is put back from a destructor, because a check that fails
   unwinds past a last line. */
namespace
{

struct TunerFixture
{
	FakeTunerSource      tuner;
	InstalledTunerSource installed;

	TunerFixture() : installed(&tuner) {}

	private:
		TunerFixture(const TunerFixture &);
		TunerFixture &operator=(const TunerFixture &);
};

} // namespace

TEST_CASE("a tuner reset is asked of the box and answers nothing of its own", "[write]")
{
	ShippedRoutes shipped;
	TunerFixture fx;

	REQUIRE(fx.tuner.resets == 0);

	const Reply r = authedPost("/api/v1/tuner/reset", "");
	// No content and not accepted: the channel daemon is told and answers, so
	// by the time this is written the reset has been taken.
	REQUIRE(r.code == 204);
	REQUIRE(r.body.empty());
	REQUIRE(fx.tuner.resets == 1);
}

TEST_CASE("a box that would not take the reset says so", "[write]")
{
	ShippedRoutes shipped;
	TunerFixture fx;

	fx.tuner.reset_status = coreapi::Status::Internal;

	const Reply r = authedPost("/api/v1/tuner/reset", "");
	REQUIRE(r.code == 500);
	// Its own code, because what a caller does about it is not what it does
	// about a signal it cannot read: the box did not even take the request.
	REQUIRE(r.body.find("tuner-not-reset") != std::string::npos);
	REQUIRE(fx.tuner.resets == 1);
}

TEST_CASE("a tuner reset is not something a write level reaches", "[write]")
{
	/* It drives the channel daemon's own standby, which is what the route that
	   puts the box in standby is System for, and every live viewer loses the
	   picture while it runs. The reload of the channel lists beside it is Write
	   because it rewrites what the box holds and leaves the picture alone. */
	ShippedRoutes shipped;
	TunerFixture fx;

	REQUIRE(lanPost("/api/v1/tuner/reset", "").code == 403);
	REQUIRE(send(Post, "/api/v1/tuner/reset", "", AuthLevel::Write, "127.0.0.1").code == 403);
	REQUIRE(fx.tuner.resets == 0);
}

/* The two files the box keeps its network filesystems in, written as entries.

   Both paths are process wide and are put back from a destructor, because a
   check that fails unwinds past a last line and the case after this one would
   otherwise be writing whatever this one pointed at. */
namespace
{

struct NetfsFixture
{
	Sandbox     box;
	std::string before_fstab;
	std::string before_auto;
	std::string before_init;

	NetfsFixture()
		: before_fstab(coreapi::netfs::pathOf(coreapi::netfs::Table::Fstab)),
		  before_auto(coreapi::netfs::pathOf(coreapi::netfs::Table::Automount)),
		  before_init(coreapi::netfs::initRoot())
	{
		coreapi::netfs::setPath(coreapi::netfs::Table::Fstab, box.at("fstab"));
		coreapi::netfs::setPath(coreapi::netfs::Table::Automount, box.at("auto.net"));
		/* A directory of its own for the scripts, because one of them is called
		   fstab and so is one of the files: on the box they sit in different
		   directories, and a fixture that put them in one would have the script
		   and the file at one name. */
		REQUIRE(::mkdir(box.at("init.d").c_str(), 0700) == 0);
		coreapi::netfs::setInitRoot(box.at("init.d"));
	}

	~NetfsFixture()
	{
		coreapi::netfs::setPath(coreapi::netfs::Table::Fstab, before_fstab);
		coreapi::netfs::setPath(coreapi::netfs::Table::Automount, before_auto);
		coreapi::netfs::setInitRoot(before_init);
	}

	void put(const char *name, const std::string &text) const
	{
		std::FILE *f = std::fopen(box.at(name).c_str(), "w");
		REQUIRE(f != NULL);
		REQUIRE(std::fwrite(text.data(), 1, text.size(), f) == text.size());
		REQUIRE(std::fclose(f) == 0);
	}

	void script(const char *name, const std::string &text) const
	{
		const std::string at = box.at("init.d") + "/" + name;
		std::FILE *f = std::fopen(at.c_str(), "w");
		REQUIRE(f != NULL);
		REQUIRE(std::fwrite(text.data(), 1, text.size(), f) == text.size());
		REQUIRE(std::fclose(f) == 0);
		REQUIRE(::chmod(at.c_str(), 0700) == 0);
	}

	std::string fstab() const { return wholeOf(box.at("fstab")); }

	private:
		NetfsFixture(const NetfsFixture &);
		NetfsFixture &operator=(const NetfsFixture &);
};

// One entry out of a table the route answered with, found by its slot rather
// than by where it sits, although the two agree: what a write addresses is the
// slot, and a case reading by position would pass over a table that had moved.
::Json::Value slotOf(const std::string &body, unsigned slot)
{
	const ::Json::Value items = parsed(body)["items"];
	for (::Json::ArrayIndex i = 0; i < items.size(); ++i)
	{
		if (items[i]["slot"].asUInt() == slot)
			return items[i];
	}
	return ::Json::Value();
}

const char kNfsEntry[] =
	"{\"active\":true,\"type\":\"nfs\",\"host\":\"192.168.0.10\","
	"\"remote_dir\":\"/nas/video\",\"local_dir\":\"/srv\",\"options\":\"soft,nolock\"}";

} // namespace

TEST_CASE("an entry written into a slot is what the file then holds", "[write][netfs]")
{
	ShippedRoutes shipped;
	NetfsFixture netfs;

	const Reply r = authedPut("/api/v1/storage/netfs/fstab/0", kNfsEntry);
	REQUIRE(r.code == 200);

	// The answer is the whole table, because the write rewrote the whole file
	// and what a caller has to be able to read afterwards is the file.
	REQUIRE(parsed(r.body)["items"].size() == 8);
	const ::Json::Value put = slotOf(r.body, 0);
	REQUIRE(put["host"].asString() == "192.168.0.10");
	REQUIRE(put["remote_dir"].asString() == "/nas/video");
	REQUIRE(put["local_dir"].asString() == "/srv");
	REQUIRE(put["active"].asBool());

	/* And the file reads as a filesystem table and not only as something this
	   server can read back: the columns are the six such a file has, in the
	   order a reader of one splits them. */
	const std::string written = netfs.fstab();
	REQUIRE(written.find("192.168.0.10:/nas/video\t/srv\t\tnfs\tsoft,nolock\t\t0\t0\n")
		!= std::string::npos);
	// The head the screen at the box writes, so that the file is the same file
	// whichever of the two wrote it last.
	REQUIRE(written.compare(0, 2, "# ") == 0);
}

TEST_CASE("a write leaves the password where a read could not show it", "[write][netfs]")
{
	/* A read answers nothing for the password, so a form that redraws itself
	   from what it read sends nothing back, and taking that nothing would wipe
	   the value the read protected. */
	ShippedRoutes shipped;
	NetfsFixture netfs;

	netfs.put("fstab",
		  "//192.168.1.99/folder\t/mnt/folder\t\tcifs\tusername=uli,password=geheim\t\t0\t0\n");

	const Reply r = authedPut("/api/v1/storage/netfs/fstab/0",
				  "{\"active\":true,\"type\":\"cifs\",\"host\":\"192.168.1.99\","
				  "\"remote_dir\":\"/folder\",\"local_dir\":\"/mnt/anders\","
				  "\"user\":\"uli\"}");
	REQUIRE(r.code == 200);
	REQUIRE(r.body.find("geheim") == std::string::npos);
	REQUIRE(slotOf(r.body, 0)["has_password"].asBool());
	REQUIRE(slotOf(r.body, 0)["local_dir"].asString() == "/mnt/anders");

	// Still in the file, which is where the kernel reads it from.
	REQUIRE(netfs.fstab().find("password=geheim") != std::string::npos);
}

TEST_CASE("a password that was typed replaces the one that was there", "[write][netfs]")
{
	ShippedRoutes shipped;
	NetfsFixture netfs;

	netfs.put("fstab",
		  "//192.168.1.99/folder\t/mnt/folder\t\tcifs\tusername=uli,password=alt\t\t0\t0\n");

	const Reply r = authedPut("/api/v1/storage/netfs/fstab/0",
				  "{\"active\":true,\"type\":\"cifs\",\"host\":\"192.168.1.99\","
				  "\"remote_dir\":\"/folder\",\"local_dir\":\"/mnt/folder\","
				  "\"user\":\"uli\",\"password\":\"neu\"}");
	REQUIRE(r.code == 200);
	// Neither form of it is in the answer: the one that went in is not read
	// back, and the one that was there is gone.
	REQUIRE(r.body.find("neu") == std::string::npos);
	REQUIRE(r.body.find("alt") == std::string::npos);

	const std::string written = netfs.fstab();
	REQUIRE(written.find("password=neu") != std::string::npos);
	REQUIRE(written.find("password=alt") == std::string::npos);
}

TEST_CASE("writing one slot leaves every other slot where it stands", "[write][netfs]")
{
	ShippedRoutes shipped;
	NetfsFixture netfs;

	netfs.put("fstab",
		  "192.168.0.10:/first\t/srv\t\tnfs\tsoft\t\t0\t0\n"
		  "192.168.0.10:/second\t/mnt/two\t\tnfs\tsoft\t\t0\t0\n");

	const Reply r = authedPut("/api/v1/storage/netfs/fstab/1", kNfsEntry);
	REQUIRE(r.code == 200);

	REQUIRE(slotOf(r.body, 0)["remote_dir"].asString() == "/first");
	REQUIRE(slotOf(r.body, 1)["remote_dir"].asString() == "/nas/video");
	REQUIRE(slotOf(r.body, 2)["host"].asString() == "");
}

TEST_CASE("an emptied slot is still a slot and carries nothing", "[write][netfs]")
{
	/* Emptying the slot is also the one way a password comes off a mount, there
	   being no other reading of a member left out. */
	ShippedRoutes shipped;
	NetfsFixture netfs;

	netfs.put("fstab",
		  "//192.168.1.99/folder\t/mnt/folder\t\tcifs\tusername=uli,password=geheim\t\t0\t0\n"
		  "192.168.0.10:/second\t/mnt/two\t\tnfs\tsoft\t\t0\t0\n");

	const Reply r = authedDelete("/api/v1/storage/netfs/fstab/0");
	REQUIRE(r.code == 200);

	REQUIRE(parsed(r.body)["items"].size() == 8);
	REQUIRE(slotOf(r.body, 0)["host"].asString() == "");
	REQUIRE_FALSE(slotOf(r.body, 0)["has_password"].asBool());
	REQUIRE_FALSE(slotOf(r.body, 0)["active"].asBool());
	// The slot behind it did not move up into the one that was emptied.
	REQUIRE(slotOf(r.body, 1)["remote_dir"].asString() == "/second");

	REQUIRE(netfs.fstab().find("geheim") == std::string::npos);
	// An empty slot is written as a line that mounts nothing, so the start up
	// script passes over it rather than trying to reach a server nobody named.
	REQUIRE(netfs.fstab().find("noauto") != std::string::npos);
}

TEST_CASE("a value that would not survive the file is refused and nothing is written",
	  "[write][netfs]")
{
	ShippedRoutes shipped;
	NetfsFixture netfs;

	netfs.put("fstab", "192.168.0.10:/first\t/srv\t\tnfs\tsoft\t\t0\t0\n");
	const std::string before = netfs.fstab();

	struct Bad
	{
		const char *what;
		const char *body;
	};
	const Bad bad[] = {
		// The columns are split on whitespace, so a value carrying any would be
		// read back as two columns and take the ones behind it with it.
		{ "a space in the mount point",
		  "{\"active\":true,\"type\":\"nfs\",\"host\":\"192.168.0.10\",\"remote_dir\":\"/x\","
		  "\"local_dir\":\"/mnt/two words\"}" },
		// A number sign ends a line in the second file, and in the first it is
		// what a comment begins with.
		{ "a number sign in the server",
		  "{\"active\":true,\"type\":\"nfs\",\"host\":\"one#two\",\"remote_dir\":\"/x\","
		  "\"local_dir\":\"/mnt/two\"}" },
		// The colon is what separates the server from what it exports.
		{ "a colon in an export server",
		  "{\"active\":true,\"type\":\"nfs\",\"host\":\"a:b\",\"remote_dir\":\"/x\","
		  "\"local_dir\":\"/mnt/two\"}" },
		// Three options this layer writes itself out of members of their own.
		{ "an option this layer writes itself",
		  "{\"active\":true,\"type\":\"nfs\",\"host\":\"192.168.0.10\",\"remote_dir\":\"/x\","
		  "\"local_dir\":\"/mnt/two\",\"options\":\"soft,noauto\"}" },
		{ "a password written as an option",
		  "{\"active\":true,\"type\":\"cifs\",\"host\":\"192.168.0.10\",\"remote_dir\":\"/x\","
		  "\"local_dir\":\"/mnt/two\",\"options\":\"password=geheim\"}" }
	};

	for (size_t i = 0; i < sizeof(bad) / sizeof(bad[0]); ++i)
	{
		INFO(bad[i].what);
		const Reply r = authedPut("/api/v1/storage/netfs/fstab/1", bad[i].body);
		REQUIRE(r.code == 400);
		REQUIRE(r.body.find("bad-string") != std::string::npos);
		REQUIRE(netfs.fstab() == before);
	}
}

TEST_CASE("an entry written to the automounter goes in that file's own shape", "[write][netfs]")
{
	ShippedRoutes shipped;
	NetfsFixture netfs;

	const Reply r = authedPut("/api/v1/storage/netfs/automount/0",
				  "{\"active\":false,\"type\":\"cifs\",\"host\":\"192.168.0.11\","
				  "\"remote_dir\":\"/pictures\",\"local_dir\":\"pictures\","
				  "\"user\":\"uli\",\"password\":\"geheim\"}");
	REQUIRE(r.code == 200);

	const std::string written = wholeOf(netfs.box.at("auto.net"));
	// Switched off is a line commented out there and never the word the other
	// file uses, and the server is named without the stray pair of characters
	// the screen at the box writes in front of it.
	REQUIRE(written.find("#pictures\t-fstype=cifs,username=uli,password=geheim"
			     "\t\t//192.168.0.11/pictures\n") != std::string::npos);
	REQUIRE(written.find("noauto") == std::string::npos);
	REQUIRE(written.find("://") == std::string::npos);

	// And it reads back as what went in.
	const ::Json::Value put = slotOf(r.body, 0);
	REQUIRE_FALSE(put["active"].asBool());
	REQUIRE(put["local_dir"].asString() == "pictures");
	REQUIRE(put["host"].asString() == "192.168.0.11");
	REQUIRE(put["user"].asString() == "uli");
	REQUIRE(put["has_password"].asBool());
	REQUIRE(put["options"].asString() == "");
}

TEST_CASE("mounting and unmounting are the box's own script and not a mount from here",
	  "[write][netfs]")
{
	/* What is mounted at start up and what a caller asks for afterwards have to
	   be one piece of work, and they are only one if one script does both. */
	ShippedRoutes shipped;
	NetfsFixture netfs;

	netfs.script("fstab", "#!/bin/sh\nprintf 'ran %s\\n' \"$1\"\n");

	const Reply mounted = authedPost("/api/v1/storage/netfs/fstab/mount", "");
	REQUIRE(mounted.code == 200);
	REQUIRE(stringField(mounted.body, "output") == "ran start_netfs\n");

	const Reply unmounted = authedPost("/api/v1/storage/netfs/fstab/unmount", "");
	REQUIRE(unmounted.code == 200);
	REQUIRE(stringField(unmounted.body, "output") == "ran stop_netfs\n");
}

TEST_CASE("the automounter is started again so that it reads its file afresh", "[write][netfs]")
{
	ShippedRoutes shipped;
	NetfsFixture netfs;

	netfs.script("autofs", "#!/bin/sh\nprintf 'ran %s\\n' \"$1\"\n");

	const Reply r = authedPost("/api/v1/storage/netfs/automount/reload", "");
	REQUIRE(r.code == 200);
	REQUIRE(stringField(r.body, "output") == "ran restart\n");
}

TEST_CASE("none of the network filesystem routes is reached below system", "[write][netfs]")
{
	ShippedRoutes shipped;
	NetfsFixture netfs;

	REQUIRE(lanPut("/api/v1/storage/netfs/fstab/0", kNfsEntry).code == 403);
	REQUIRE(send(Put, "/api/v1/storage/netfs/fstab/0", kNfsEntry,
		     AuthLevel::Write, "127.0.0.1").code == 403);
	REQUIRE(lanDelete("/api/v1/storage/netfs/fstab/0").code == 403);
	REQUIRE(lanPost("/api/v1/storage/netfs/fstab/mount", "").code == 403);
	REQUIRE(lanPost("/api/v1/storage/netfs/automount/reload", "").code == 403);
}

namespace
{

/* The box's own shares as a case holds them: eight slots, what is mounted on
   each, and what the next mount and unmount are to come to. */
struct FakeShares : public coreapi::shares::Source
{
	std::vector<coreapi::shares::Share> slots;
	std::vector<coreapi::shares::State> states;
	coreapi::shares::Mounted next_mount;
	coreapi::shares::Unmounted next_unmount;
	coreapi::Status next_write;
	std::vector<std::string> mounted, unmounted;

	FakeShares()
		: slots(coreapi::shares::kSlots), states(coreapi::shares::kSlots, coreapi::shares::State::Unmounted),
		  next_mount(coreapi::shares::Mounted::Ok), next_unmount(coreapi::shares::Unmounted::Ok),
		  next_write(coreapi::Status::Ok)
	{
		for (size_t i = 0; i < slots.size(); ++i)
			slots[i].local_dir = "/mnt/mnt" + std::to_string((unsigned long) i);
		coreapi::shares::setSource(this);
	}
	~FakeShares() { coreapi::shares::setSource(NULL); }

	coreapi::Status read(std::vector<coreapi::shares::Share> &out, std::vector<coreapi::shares::State> &st) const
	{
		out = slots;
		st = states;
		return coreapi::Status::Ok;
	}
	coreapi::Status write(size_t slot, const coreapi::shares::Share &share)
	{
		if (next_write != coreapi::Status::Ok)
			return next_write;
		const std::string kept = slots[slot].password;
		slots[slot] = share;
		if (!share.change_password)
			slots[slot].password = kept;
		return coreapi::Status::Ok;
	}
	coreapi::shares::Mounted mount(const coreapi::shares::Share &share)
	{
		mounted.push_back(share.local_dir);
		if (next_mount == coreapi::shares::Mounted::Ok)
			for (size_t i = 0; i < slots.size(); ++i)
				if (slots[i].local_dir == share.local_dir)
					states[i] = coreapi::shares::State::Mounted;
		return next_mount;
	}
	coreapi::shares::Unmounted unmount(const coreapi::shares::Share &share)
	{
		unmounted.push_back(share.local_dir);
		if (next_unmount == coreapi::shares::Unmounted::Ok)
			for (size_t i = 0; i < slots.size(); ++i)
				if (slots[i].local_dir == share.local_dir)
					states[i] = coreapi::shares::State::Unmounted;
		return next_unmount;
	}
};

const char kShareBody[] =
	"{\"automount\":true,\"type\":\"cifs\",\"local_dir\":\"/mnt/nas\",\"host\":\"nas.lan\","
	"\"remote_dir\":\"media\",\"user\":\"uli\",\"password\":\"p$(reboot)'x\",\"options1\":\"rw\","
	"\"options2\":\"vers=3.0\",\"mac\":\"00:11:22:aa:bb:cc\"}";

} // namespace

TEST_CASE("the box's own shares are read without the password and written whole", "[write][shares]")
{
	ShippedRoutes shipped;
	FakeShares fake;
	fake.slots[2].host = "10.0.0.2";
	fake.slots[2].remote_dir = "/export";
	fake.slots[2].password = "kept";

	const Reply list = authedGet("/api/v1/storage/shares");
	REQUIRE(list.code == 200);
	const ::Json::Value got = parsed(list.body);
	REQUIRE(got["items"].size() == 8);
	REQUIRE(got["items"][2]["host"].asString() == "10.0.0.2");
	REQUIRE(got["items"][2]["has_password"].asBool());
	REQUIRE(got["items"][2]["state"].asString() == "unmounted");
	REQUIRE(list.body.find("kept") == std::string::npos);

	// Written whole; a password that is no word of the shell's is still just a password.
	const Reply put = authedPut("/api/v1/storage/shares/5", kShareBody);
	REQUIRE(put.code == 200);
	REQUIRE(fake.slots[5].kind == coreapi::shares::Kind::Cifs);
	REQUIRE(fake.slots[5].automount);
	REQUIRE(fake.slots[5].host == "nas.lan");
	REQUIRE(fake.slots[5].username == "uli");
	REQUIRE(fake.slots[5].password == "p$(reboot)'x");
	REQUIRE(fake.slots[5].mac == "00:11:22:aa:bb:cc");
	REQUIRE(put.body.find("reboot") == std::string::npos);
	REQUIRE(parsed(put.body)["items"][5]["has_password"].asBool());

	// Left out, the stored password stays; sent empty, it goes.
	REQUIRE(authedPut("/api/v1/storage/shares/2",
			  "{\"automount\":false,\"type\":\"nfs\",\"local_dir\":\"/mnt/two\",\"host\":\"10.0.0.2\","
			  "\"remote_dir\":\"/export\"}").code == 200);
	REQUIRE(fake.slots[2].password == "kept");
	REQUIRE(fake.slots[2].local_dir == "/mnt/two");
	REQUIRE(authedPut("/api/v1/storage/shares/2",
			  "{\"automount\":false,\"type\":\"nfs\",\"local_dir\":\"/mnt/two\",\"password\":\"\"}").code == 200);
	REQUIRE(fake.slots[2].password.empty());
	REQUIRE(fake.slots[2].host.empty());

	// What the box could not keep or mount is refused, and nothing is written.
	const char *const refused[] = {
		"{\"automount\":false,\"type\":\"nfs\",\"local_dir\":\"mnt/rel\"}",
		"{\"automount\":false,\"type\":\"nfs\",\"local_dir\":\"/mnt/../etc\"}",
		"{\"automount\":false,\"type\":\"nfs\",\"local_dir\":\"/mnt/with space\"}",
		"{\"automount\":false,\"type\":\"nfs\",\"local_dir\":\"/mnt/x\",\"remote_dir\":\"/a b\"}",
		"{\"automount\":false,\"type\":\"nfs\",\"local_dir\":\"/mnt/x\",\"host\":\"nas;reboot\"}",
		// a host the mount would read as an option of its own
		"{\"automount\":false,\"type\":\"nfs\",\"local_dir\":\"/mnt/x\",\"host\":\"-oro\"}",
		"{\"automount\":false,\"type\":\"cifs\",\"local_dir\":\"/mnt/x\",\"password\":\"a,b\"}",
		"{\"automount\":false,\"type\":\"cifs\",\"local_dir\":\"/mnt/x\",\"user\":\"a#b\"}",
		"{\"automount\":false,\"type\":\"nfs\",\"local_dir\":\"/mnt/x\",\"options1\":\"rw soft\"}",
		"{\"automount\":false,\"type\":\"nfs\",\"local_dir\":\"/mnt/x\",\"mac\":\"00:11:22\"}",
		"{\"automount\":false,\"type\":\"cifs\",\"local_dir\":\"/mnt/x\",\"options1\":\"rw,password=s\"}",
		"{\"automount\":false,\"type\":\"cifs\",\"local_dir\":\"/mnt/x\",\"options2\":\"Credentials=/etc/c\"}",
		"{\"automount\":false,\"type\":\"lufs\",\"local_dir\":\"/mnt/x\"}",
	};
	for (size_t i = 0; i < sizeof(refused) / sizeof(refused[0]); ++i)
	{
		INFO(refused[i]);
		const Reply no = authedPut("/api/v1/storage/shares/6", refused[i]);
		REQUIRE(no.code == 400);
		REQUIRE((no.body.find("bad-string") != std::string::npos || no.body.find("bad-enum") != std::string::npos ||
			 no.body.find("not-in-set") != std::string::npos));
		REQUIRE(fake.slots[6].local_dir == "/mnt/mnt6");
	}

	// A box that could not be asked to take it over says so, and one too busy to.
	fake.next_write = coreapi::Status::Internal;
	const Reply unposted = authedPut("/api/v1/storage/shares/6", "{\"automount\":false,\"type\":\"nfs\",\"local_dir\":\"/mnt/x\"}");
	REQUIRE(unposted.code == 500);
	REQUIRE(unposted.body.find("command-not-posted") != std::string::npos);
	fake.next_write = coreapi::Status::Busy;
	const Reply busy = authedPut("/api/v1/storage/shares/6", "{\"automount\":false,\"type\":\"nfs\",\"local_dir\":\"/mnt/x\"}");
	REQUIRE(busy.code == 409);
	REQUIRE(busy.body.find("command-not-posted") != std::string::npos);
	REQUIRE(fake.slots[6].local_dir == "/mnt/mnt6");
}

TEST_CASE("a slot's share is mounted and unmounted, and nothing else on its directory", "[write][shares]")
{
	ShippedRoutes shipped;
	FakeShares fake;
	fake.slots[1].host = "10.0.0.1";
	fake.slots[1].remote_dir = "/export";

	// A slot naming nothing to mount.
	const Reply empty = authedPost("/api/v1/storage/shares/0/mount", "");
	REQUIRE(empty.code == 409);
	REQUIRE(empty.body.find("share-not-configured") != std::string::npos);
	REQUIRE(fake.mounted.empty());

	// What the mount came to, as the box's mounter says it.
	const struct
	{
		coreapi::shares::Mounted said;
		int code;
		const char *wire;
	} outcomes[] = {
		{ coreapi::shares::Mounted::Unsupported, 501, "filesystem-unsupported" },
		{ coreapi::shares::Mounted::Timeout, 500, "timeout" },
		{ coreapi::shares::Mounted::Failed, 500, "mount-failed" },
		{ coreapi::shares::Mounted::Busy, 409, "mount-point-busy" },
	};
	for (size_t i = 0; i < sizeof(outcomes) / sizeof(outcomes[0]); ++i)
	{
		INFO(outcomes[i].wire);
		fake.next_mount = outcomes[i].said;
		const Reply r = authedPost("/api/v1/storage/shares/1/mount", "");
		REQUIRE(r.code == outcomes[i].code);
		REQUIRE(r.body.find(outcomes[i].wire) != std::string::npos);
	}
	fake.next_mount = coreapi::shares::Mounted::Ok;
	const Reply mounted = authedPost("/api/v1/storage/shares/1/mount", "");
	REQUIRE(mounted.code == 200);
	REQUIRE(parsed(mounted.body)["items"][1]["state"].asString() == "mounted");
	// mounted already
	REQUIRE(authedPost("/api/v1/storage/shares/1/mount", "").code == 409);

	// Something else on a slot's directory is neither mounted over nor taken away.
	fake.slots[3].host = "10.0.0.3";
	fake.slots[3].remote_dir = "/other";
	fake.states[3] = coreapi::shares::State::Foreign;
	const size_t mounts = fake.mounted.size();
	REQUIRE(authedPost("/api/v1/storage/shares/3/mount", "").code == 409);
	const Reply foreign = authedPost("/api/v1/storage/shares/3/unmount", "");
	REQUIRE(foreign.code == 409);
	REQUIRE(foreign.body.find("share-not-mounted") != std::string::npos);
	REQUIRE(fake.mounted.size() == mounts);
	REQUIRE(fake.unmounted.empty());
	REQUIRE(authedPost("/api/v1/storage/shares/4/unmount", "").code == 409);

	// The unmount of the slot's own share, decided again where it is done.
	fake.next_unmount = coreapi::shares::Unmounted::NotOurs;
	const Reply overmounted = authedPost("/api/v1/storage/shares/1/unmount", "");
	REQUIRE(overmounted.code == 409);
	REQUIRE(overmounted.body.find("share-not-mounted") != std::string::npos);
	fake.next_unmount = coreapi::shares::Unmounted::Failed;
	const Reply stuck = authedPost("/api/v1/storage/shares/1/unmount", "");
	REQUIRE(stuck.code == 500);
	REQUIRE(stuck.body.find("unmount-failed") != std::string::npos);
	fake.next_unmount = coreapi::shares::Unmounted::Ok;
	const Reply gone = authedPost("/api/v1/storage/shares/1/unmount", "");
	REQUIRE(gone.code == 200);
	REQUIRE(parsed(gone.body)["items"][1]["state"].asString() == "unmounted");
	REQUIRE(fake.unmounted.back() == "/mnt/mnt1");

	// A slot of the userspace FTP filesystem is read, and mounted and unmounted at the box only.
	fake.slots[5].kind = coreapi::shares::Kind::Lufs;
	fake.slots[5].host = "ftp.lan";
	fake.slots[5].remote_dir = "pub";
	REQUIRE(parsed(authedGet("/api/v1/storage/shares").body)["items"][5]["type"].asString() == "lufs");
	const size_t before = fake.mounted.size();
	const Reply lufs_mount = authedPost("/api/v1/storage/shares/5/mount", "");
	REQUIRE(lufs_mount.code == 501);
	REQUIRE(lufs_mount.body.find("filesystem-unsupported") != std::string::npos);
	fake.states[5] = coreapi::shares::State::Foreign;
	const Reply lufs_unmount = authedPost("/api/v1/storage/shares/5/unmount", "");
	REQUIRE(lufs_unmount.code == 501);
	REQUIRE(lufs_unmount.body.find("filesystem-unsupported") != std::string::npos);
	REQUIRE(fake.mounted.size() == before);
}

TEST_CASE("every slot set to mount automatically is mounted, and each says what came of it", "[write][shares]")
{
	ShippedRoutes shipped;
	FakeShares fake;
	fake.slots[0].automount = true;
	fake.slots[0].host = "10.0.0.1";
	fake.slots[0].remote_dir = "/a";
	fake.slots[2].automount = true;   // names nothing to mount
	fake.slots[4].automount = true;
	fake.slots[4].host = "10.0.0.4";
	fake.slots[4].remote_dir = "/b";
	fake.states[4] = coreapi::shares::State::Mounted;   // mounted already: left out
	fake.slots[5].host = "10.0.0.5";                     // not automatic: left out
	fake.slots[5].remote_dir = "/c";

	const Reply r = authedPost("/api/v1/storage/shares/mount", "");
	REQUIRE(r.code == 200);
	const ::Json::Value got = parsed(r.body);
	REQUIRE(got["items"].size() == 2);
	REQUIRE(got["items"][0]["slot"].asUInt() == 0);
	REQUIRE(got["items"][0]["mounted"].asBool());
	REQUIRE(got["items"][1]["slot"].asUInt() == 2);
	REQUIRE_FALSE(got["items"][1]["mounted"].asBool());
	REQUIRE(got["items"][1]["code"].asString() == "share-not-configured");
	REQUIRE(fake.mounted.size() == 1);
}

TEST_CASE("a box without shares of its own says so, and none of the routes is reached below system", "[write][shares]")
{
	ShippedRoutes shipped;
	{
		const Reply none = authedGet("/api/v1/storage/shares");
		REQUIRE(none.code == 501);
		REQUIRE(none.body.find("command-not-sent") != std::string::npos);
		REQUIRE(authedPut("/api/v1/storage/shares/0", "{\"automount\":false,\"type\":\"nfs\",\"local_dir\":\"/mnt/x\"}").code == 501);
		REQUIRE(authedPost("/api/v1/storage/shares/mount", "").code == 501);
		REQUIRE(authedPost("/api/v1/storage/shares/0/mount", "").code == 501);
		REQUIRE(authedPost("/api/v1/storage/shares/0/unmount", "").code == 501);
	}
	FakeShares fake;
	REQUIRE(send(Get, "/api/v1/storage/shares", "", AuthLevel::Write, "127.0.0.1").code == 403);
	REQUIRE(send(Put, "/api/v1/storage/shares/0", kShareBody, AuthLevel::Write, "127.0.0.1").code == 403);
	REQUIRE(lanPost("/api/v1/storage/shares/0/mount", "").code == 403);
	REQUIRE(lanPost("/api/v1/storage/shares/0/unmount", "").code == 403);
	REQUIRE(lanPost("/api/v1/storage/shares/mount", "").code == 403);
}

namespace
{

/* The pages' directory and the temporary directory of a case's own for the
   extension routes, with one script of the pages' own already there. */
struct ExtFixture
{
	Sandbox     box;
	std::string web, tmp;
	const char *before_web;
	const char *before_tmp;

	ExtFixture()
		: web(box.at("web")), tmp(box.at("tmp")), before_web(coreapi::extensions::internal::web_root),
		  before_tmp(coreapi::extensions::internal::tmp_dir)
	{
		REQUIRE(::mkdir(web.c_str(), 0755) == 0);
		REQUIRE(::mkdir((web + "/scripts").c_str(), 0755) == 0);
		REQUIRE(::mkdir(tmp.c_str(), 0700) == 0);
		REQUIRE(::mkdir(box.at("src").c_str(), 0700) == 0);
		write(web + "/scripts/Y_Tools.sh", "#!/bin/sh\necho the pages' own\n");
		coreapi::extensions::internal::web_root = web.c_str();
		coreapi::extensions::internal::tmp_dir = tmp.c_str();
	}

	~ExtFixture()
	{
		coreapi::extensions::internal::web_root = before_web;
		coreapi::extensions::internal::tmp_dir = before_tmp;
	}

	static void write(const std::string &path, const std::string &text)
	{
		std::FILE *f = std::fopen(path.c_str(), "w");
		REQUIRE(f != NULL);
		REQUIRE(std::fwrite(text.data(), 1, text.size(), f) == text.size());
		REQUIRE(std::fclose(f) == 0);
	}

	static std::string read(const std::string &path)
	{
		std::string out;
		std::FILE *f = std::fopen(path.c_str(), "r");
		if (f == NULL)
			return "(none)";
		char buf[512];
		size_t got;
		while ((got = std::fread(buf, 1, sizeof(buf), f)) > 0)
			out.append(buf, got);
		std::fclose(f);
		return out;
	}

	// An extension's tree under src/, files given as path and text.
	void tree(const std::vector<std::pair<std::string, std::string> > &files)
	{
		(void) std::system(("rm -rf '" + box.at("src") + "' && mkdir -p '" + box.at("src") + "'").c_str());
		for (size_t i = 0; i < files.size(); ++i)
		{
			const std::string path = box.at("src") + "/" + files[i].first;
			(void) std::system(("mkdir -p \"$(dirname '" + path + "')\"").c_str());
			write(path, files[i].second);
		}
	}

	// tar.gz of what src/ holds, uploaded to tmp/ as name.
	bool pack(const std::string &name, const std::string &what)
	{
		const std::string cmd = "tar -czf '" + tmp + "/" + name + "' -C '" + box.at("src") + "' " + what + " 2>/dev/null";
		return std::system(cmd.c_str()) == 0;
	}

	// A plain tar built entry by entry, as an archive built by hand can be.
	void raw(const std::string &name, const std::vector<BackupFixture::RawEntry> &entries)
	{
		std::string out;
		for (size_t i = 0; i < entries.size(); ++i)
		{
			out += BackupFixture::rawHeader(entries[i].name, entries[i].type, entries[i].data.size());
			std::string data = entries[i].data;
			data.resize(((data.size() + 511) / 512) * 512, '\0');
			out += data;
		}
		out += std::string(1024, '\0');
		write(tmp + "/" + name, out);
	}

	// What an install or a removal leaves behind besides what it is for.
	std::vector<std::string> leftovers() const
	{
		std::vector<std::string> out;
		const std::string dirs[] = { tmp, web + "/extensions", web + "/scripts" };
		for (size_t i = 0; i < sizeof(dirs) / sizeof(dirs[0]); ++i)
		{
			DIR *d = opendir(dirs[i].c_str());
			if (d == NULL)
				continue;
			struct dirent *e;
			while ((e = readdir(d)) != NULL)
				if (std::strncmp(e->d_name, "extension.", 10) == 0 || std::strncmp(e->d_name, ".install-", 9) == 0 ||
				    std::strncmp(e->d_name, ".old-", 5) == 0 || std::strncmp(e->d_name, ".remove-", 8) == 0 ||
				    std::strncmp(e->d_name, ".ext-", 5) == 0)
					out.push_back(dirs[i] + "/" + e->d_name);
			closedir(d);
		}
		return out;
	}

	private:
		ExtFixture(const ExtFixture &);
		ExtFixture &operator=(const ExtFixture &);
};

std::vector<std::pair<std::string, std::string> > clockFiles(const std::string &version, const std::string &tool)
{
	std::vector<std::pair<std::string, std::string> > f;
	f.push_back(std::make_pair("clock/manifest.json", "{\"name\":\"Clock\",\"version\":\"" + version +
		"\",\"slot\":\"tools\",\"entry\":\"index.html\",\"scripts\":[\"clock_tool\"]}"));
	f.push_back(std::make_pair("clock/index.html", "<p>clock " + version + "</p>"));
	f.push_back(std::make_pair("clock/scripts/clock_tool.sh", tool));
	return f;
}

// A script that says which version it is and every argument it was handed.
std::string sayingTool(const std::string &version)
{
	return "#!/bin/sh\nprintf '" + version + ":%s|' \"$@\"\n";
}

Reply runClock(const std::string &script, const std::string &body)
{
	return authedPost("/api/v1/webui/extensions/clock/scripts/" + script, body);
}

/* A key pair of a case's own, its public half where the box looks for the
   image's, put back after; sign() answers the base64 signature an index
   publishes beside it. */
struct IndexSigner
{
	EVP_PKEY   *key;
	std::string pub;
	const char *before;

	explicit IndexSigner(const std::string &dir)
		: key(NULL), pub(dir + "/webui-index.pub"), before(coreapi::extensions::internal::index_key)
	{
		REQUIRE(::mkdir(dir.c_str(), 0700) == 0);
		EVP_PKEY_CTX *ctx = EVP_PKEY_CTX_new_id(EVP_PKEY_ED25519, NULL);
		REQUIRE(ctx != NULL);
		REQUIRE(EVP_PKEY_keygen_init(ctx) == 1);
		REQUIRE(EVP_PKEY_keygen(ctx, &key) == 1);
		EVP_PKEY_CTX_free(ctx);
		std::FILE *f = std::fopen(pub.c_str(), "w");
		REQUIRE(f != NULL);
		REQUIRE(PEM_write_PUBKEY(f, key) == 1);
		REQUIRE(std::fclose(f) == 0);
		coreapi::extensions::internal::index_key = pub.c_str();
	}

	~IndexSigner()
	{
		coreapi::extensions::internal::index_key = before;
		EVP_PKEY_free(key);
	}

	std::string sign(const std::string &data) const
	{
		EVP_MD_CTX *ctx = EVP_MD_CTX_new();
		size_t len = 0;
		REQUIRE(EVP_DigestSignInit(ctx, NULL, NULL, NULL, key) == 1);
		REQUIRE(EVP_DigestSign(ctx, NULL, &len, (const unsigned char *) data.data(), data.size()) == 1);
		std::vector<unsigned char> sig(len);
		REQUIRE(EVP_DigestSign(ctx, &sig[0], &len, (const unsigned char *) data.data(), data.size()) == 1);
		EVP_MD_CTX_free(ctx);
		std::vector<unsigned char> b64(((len + 2) / 3) * 4 + 1);
		const int n = EVP_EncodeBlock(&b64[0], &sig[0], (int) len);
		return std::string((const char *) &b64[0], (size_t) n);
	}

	private:
		IndexSigner(const IndexSigner &);
		IndexSigner &operator=(const IndexSigner &);
};

// An index offering clock alone, at that SHA-256.
std::string clockIndex(const std::string &sha)
{
	return "{\"format\":1,\"extensions\":[{\"id\":\"clock\",\"name\":\"Clock\",\"version\":\"1.0\","
	       "\"url\":\"https://ext.example/clock.tar.gz\",\"sha256\":\"" + sha + "\"}]}";
}

// clock.tar.gz installed from index.json, both uploaded beside each other.
Reply installFromIndex(ExtFixture &x, const std::string &dir, const std::string &index, const std::string &sig)
{
	ExtFixture::write(x.tmp + "/index.json", index);
	return authedPost("/api/v1/webui/extensions", "{\"file\":\"clock.tar.gz\",\"dir\":\"" + dir +
		"\",\"index\":\"index.json\",\"signature\":\"" + sig + "\"}");
}

std::string indexBody(const std::string &file, const std::string &sig)
{
	return "{\"index\":\"" + file + "\",\"signature\":\"" + sig + "\"}";
}

} // namespace

TEST_CASE("an extension uploaded as an archive is installed, listed, replaced and removed with its scripts", "[write][extensions]")
{
	ShippedRoutes shipped;
	ExtFixture x;

	// none yet, and the list says so to a reader on the home network
	REQUIRE(parsed(send(Get, "/api/v1/webui/extensions", "", AuthLevel::Read, "192.168.1.20").body)["items"].size() == 0);

	// a script its manifest does not name is a file of it like any other
	std::vector<std::pair<std::string, std::string> > files = clockFiles("1.0", sayingTool("one"));
	files.push_back(std::make_pair("clock/scripts/old_tool.sh", "#!/bin/sh\necho old\n"));
	x.tree(files);
	REQUIRE(x.pack("clock.tar.gz", "clock"));
	const Reply in = authedPost("/api/v1/webui/extensions", "{\"file\":\"clock.tar.gz\",\"unsigned\":true}");
	REQUIRE(in.code == 201);
	const ::Json::Value got = parsed(in.body);
	REQUIRE(got["dir"].asString() == "clock");
	REQUIRE(got["scripts"].size() == 1);
	REQUIRE(got["scripts"][0].asString() == "clock_tool");
	REQUIRE(x.read(x.web + "/extensions/clock/index.html") == "<p>clock 1.0</p>");
	struct stat st;
	REQUIRE(stat((x.web + "/extensions/clock/scripts/clock_tool.sh").c_str(), &st) == 0);
	REQUIRE((st.st_mode & 0777) == 0755);
	REQUIRE(stat((x.web + "/extensions/clock/scripts/old_tool.sh").c_str(), &st) == 0);
	REQUIRE((st.st_mode & 0777) == 0644);
	REQUIRE(stat((x.web + "/extensions/clock/index.html").c_str(), &st) == 0);
	REQUIRE((st.st_mode & 0777) == 0644);
	// its scripts stay in its directory: nothing beside the pages' own
	REQUIRE(access((x.web + "/scripts/clock_tool.sh").c_str(), F_OK) != 0);
	REQUIRE(access((x.web + "/scripts/old_tool.sh").c_str(), F_OK) != 0);
	REQUIRE(access((x.tmp + "/clock.tar.gz").c_str(), F_OK) != 0);
	REQUIRE(x.leftovers().empty());

	const ::Json::Value listed = parsed(send(Get, "/api/v1/webui/extensions", "", AuthLevel::Read, "192.168.1.20").body);
	REQUIRE(listed["items"].size() == 1);
	REQUIRE(listed["items"][0]["dir"].asString() == "clock");
	REQUIRE(listed["items"][0]["scripts"].size() == 1);
	REQUIRE(listed["items"][0]["scripts"][0].asString() == "clock_tool");

	// run from there, each element one argument, a space and a quote in it kept
	const Reply ran = runClock("clock_tool", "[\"a b\",\"it's\"]");
	REQUIRE(ran.code == 200);
	REQUIRE(parsed(ran.body)["output"].asString() == "one:a b|one:it's|");
	REQUIRE(parsed(runClock("clock_tool", "[]").body)["output"].asString() == "one:|");

	// only what the manifest names, of an extension that is there, by a plain name
	const Reply unnamed = runClock("old_tool", "[]");
	REQUIRE(unnamed.code == 404);
	REQUIRE(unnamed.body.find("script-missing") != std::string::npos);
	const Reply nobody = authedPost("/api/v1/webui/extensions/other/scripts/clock_tool", "[]");
	REQUIRE(nobody.code == 404);
	REQUIRE(nobody.body.find("no-such-name") != std::string::npos);
	REQUIRE(runClock("-clock_tool", "[]").code == 400);
	REQUIRE(runClock(".clock_tool", "[]").code == 400);
	REQUIRE(authedPost("/api/v1/webui/extensions/bad.name/scripts/clock_tool", "[]").code == 400);

	// what the script is handed: strings, a list of them, none read as an option
	const Reply dash = runClock("clock_tool", "[\"-n\"]");
	REQUIRE(dash.code == 400);
	REQUIRE(dash.body.find("leading-dash") != std::string::npos);
	REQUIRE(runClock("clock_tool", "{\"args\":[]}").code == 400);
	REQUIRE(runClock("clock_tool", "[1]").code == 400);
	std::string many = "[";
	for (size_t i = 0; i <= coreapi::extensions::kMaxScriptArgs; ++i)
		many += std::string(i ? "," : "") + "\"a\"";
	many += "]";
	const Reply too_many = runClock("clock_tool", many);
	REQUIRE(too_many.code == 400);
	REQUIRE(too_many.body.find("bad-string") != std::string::npos);
	const Reply too_long = runClock("clock_tool", "[\"" + std::string(coreapi::extensions::kMaxScriptArgument + 1, 'a') + "\"]");
	REQUIRE(too_long.code == 400);
	REQUIRE(too_long.body.find("bad-string") != std::string::npos);
	// and only for a caller at the system level
	REQUIRE(send(Post, "/api/v1/webui/extensions/clock/scripts/clock_tool", "[]", AuthLevel::Write, "127.0.0.1").code == 403);

	/* A newer version takes its place. The copies an earlier install left
	   beside the pages' own scripts go where they are still as it left them;
	   one changed at the box stays. */
	ExtFixture::write(x.web + "/scripts/clock_tool.sh", sayingTool("one"));
	ExtFixture::write(x.web + "/scripts/old_tool.sh", "#!/bin/sh\necho changed at the box\n");
	x.tree(clockFiles("2.0", sayingTool("two")));
	// an update that names another extension, or none an extension carries, changes nothing
	REQUIRE(x.pack("clock2.tar.gz", "clock"));
	const Reply other = authedPost("/api/v1/webui/extensions", "{\"file\":\"clock2.tar.gz\",\"dir\":\"radio\",\"unsigned\":true}");
	REQUIRE(other.code == 400);
	REQUIRE(other.body.find("extension-refused") != std::string::npos);
	REQUIRE(other.body.find("holds the extension clock, not radio") != std::string::npos);
	REQUIRE(access((x.tmp + "/clock2.tar.gz").c_str(), F_OK) != 0);
	REQUIRE(x.pack("clock2.tar.gz", "clock"));
	const Reply unnamable = authedPost("/api/v1/webui/extensions", "{\"file\":\"clock2.tar.gz\",\"dir\":\"bad.name\",\"unsigned\":true}");
	REQUIRE(unnamable.code == 400);
	REQUIRE(unnamable.body.find("bad-name") != std::string::npos);
	REQUIRE(access((x.tmp + "/clock2.tar.gz").c_str(), F_OK) != 0);
	REQUIRE(x.read(x.web + "/extensions/clock/index.html") == "<p>clock 1.0</p>");
	REQUIRE(x.leftovers().empty());
	REQUIRE(x.pack("clock2.tar.gz", "clock"));
	REQUIRE(authedPost("/api/v1/webui/extensions", "{\"file\":\"clock2.tar.gz\",\"dir\":\"clock\",\"unsigned\":true}").code == 201);
	REQUIRE(x.read(x.web + "/extensions/clock/index.html") == "<p>clock 2.0</p>");
	REQUIRE(access((x.web + "/extensions/clock/scripts/old_tool.sh").c_str(), F_OK) != 0);
	REQUIRE(access((x.web + "/scripts/clock_tool.sh").c_str(), F_OK) != 0);
	REQUIRE(x.read(x.web + "/scripts/old_tool.sh") == "#!/bin/sh\necho changed at the box\n");
	REQUIRE(parsed(runClock("clock_tool", "[\"x\"]").body)["output"].asString() == "two:x|");
	REQUIRE(x.leftovers().empty());

	// Removed with its scripts and the copy an earlier install left, the pages' own left.
	ExtFixture::write(x.web + "/scripts/clock_tool.sh", sayingTool("two"));
	REQUIRE(authedDelete("/api/v1/webui/extensions/clock").code == 204);
	REQUIRE(access((x.web + "/extensions/clock").c_str(), F_OK) != 0);
	REQUIRE(access((x.web + "/scripts/clock_tool.sh").c_str(), F_OK) != 0);
	REQUIRE(x.read(x.web + "/scripts/old_tool.sh") == "#!/bin/sh\necho changed at the box\n");
	REQUIRE(x.read(x.web + "/scripts/Y_Tools.sh") == "#!/bin/sh\necho the pages' own\n");
	const Reply gone = runClock("clock_tool", "[]");
	REQUIRE(gone.code == 404);
	REQUIRE(gone.body.find("no-such-name") != std::string::npos);
	const Reply again = authedDelete("/api/v1/webui/extensions/clock");
	REQUIRE(again.code == 404);
	REQUIRE(again.body.find("no-such-name") != std::string::npos);
	REQUIRE(authedDelete("/api/v1/webui/extensions/bad.name").code == 400);
	REQUIRE(x.leftovers().empty());
}

TEST_CASE("an extension from a signed index is held to it, and a file without one to the caller's word", "[write][extensions]")
{
	ShippedRoutes shipped;
	ExtFixture x;
	IndexSigner signer(x.box.at("key"));
	x.tree(clockFiles("1.0", sayingTool("one")));
	REQUIRE(x.pack("clock.tar.gz", "clock"));
	const std::string sha = coreapi::extensions::fileSha256(x.tmp + "/clock.tar.gz");
	REQUIRE(sha.size() == 64);
	const std::string index = clockIndex(sha);
	const std::string sig = signer.sign(index);

	// what a signed index offers, read by the box; the upload goes
	ExtFixture::write(x.tmp + "/index.json", index);
	const Reply listed = authedPost("/api/v1/webui/extension-index", indexBody("index.json", sig));
	REQUIRE(listed.code == 200);
	const ::Json::Value offers = parsed(listed.body)["items"];
	REQUIRE(offers.size() == 1);
	REQUIRE(offers[0]["id"].asString() == "clock");
	REQUIRE(offers[0]["name"].asString() == "Clock");
	REQUIRE(offers[0]["version"].asString() == "1.0");
	REQUIRE(offers[0]["url"].asString() == "https://ext.example/clock.tar.gz");
	REQUIRE(offers[0]["sha256"].asString() == sha);
	REQUIRE(access((x.tmp + "/index.json").c_str(), F_OK) != 0);
	// the example the document gives, with the signature this index carries in place of its stand-in
	ExtFixture::write(x.tmp + "/index.json", index);
	REQUIRE(sendBodyExample("POST", "/api/v1/webui/extension-index", std::map<std::string, std::string>(),
				std::string(86, 'A') + "==", sig) == 200);

	/* A changed index, an archive the index does not name, one it does not
	   offer, a signature of another length: nothing installed, both uploads gone. */
	struct Refused { std::string dir; std::string index; std::string sig; const char *says; };
	const std::string zeros(64, '0');
	const Refused refused[] = {
		{ "clock", index + " ", sig, "does not hold to its signature" },
		{ "clock", clockIndex(zeros), signer.sign(clockIndex(zeros)), "its SHA-256 differs" },
		{ "radio", index, sig, "offers no extension radio" },
		{ "clock", index, "c2hvcnQ=", "not 64 bytes in base64" },
	};
	for (size_t i = 0; i < sizeof(refused) / sizeof(refused[0]); ++i)
	{
		INFO(refused[i].says);
		REQUIRE(x.pack("clock.tar.gz", "clock"));
		const Reply r = installFromIndex(x, refused[i].dir, refused[i].index, refused[i].sig);
		REQUIRE(r.code == 400);
		REQUIRE(r.body.find("extension-refused") != std::string::npos);
		REQUIRE(r.body.find(refused[i].says) != std::string::npos);
		REQUIRE(access((x.tmp + "/clock.tar.gz").c_str(), F_OK) != 0);
		REQUIRE(access((x.tmp + "/index.json").c_str(), F_OK) != 0);
		REQUIRE(access((x.web + "/extensions/clock").c_str(), F_OK) != 0);
		REQUIRE(x.leftovers().empty());
	}

	// neither an index nor the caller's word
	REQUIRE(x.pack("clock.tar.gz", "clock"));
	const Reply neither = authedPost("/api/v1/webui/extensions", "{\"file\":\"clock.tar.gz\"}");
	REQUIRE(neither.code == 400);
	REQUIRE(neither.body.find("from a signed index, or unsigned") != std::string::npos);
	REQUIRE(access((x.tmp + "/clock.tar.gz").c_str(), F_OK) != 0);
	// an index without the extension it installs named
	REQUIRE(x.pack("clock.tar.gz", "clock"));
	ExtFixture::write(x.tmp + "/index.json", index);
	const Reply unnamed = authedPost("/api/v1/webui/extensions",
		"{\"file\":\"clock.tar.gz\",\"index\":\"index.json\",\"signature\":\"" + sig + "\"}");
	REQUIRE(unnamed.code == 400);
	REQUIRE(unnamed.body.find("bad-name") != std::string::npos);
	REQUIRE(access((x.tmp + "/index.json").c_str(), F_OK) != 0);
	REQUIRE(access((x.tmp + "/clock.tar.gz").c_str(), F_OK) != 0);

	// an archive that is not there, or not named as one: the index goes all the same
	ExtFixture::write(x.tmp + "/index.json", index);
	REQUIRE(authedPost("/api/v1/webui/extensions", "{\"file\":\"gone.tar.gz\",\"dir\":\"clock\",\"index\":\"index.json\","
		"\"signature\":\"" + sig + "\"}").code == 404);
	REQUIRE(access((x.tmp + "/index.json").c_str(), F_OK) != 0);
	ExtFixture::write(x.tmp + "/index.json", index);
	REQUIRE(authedPost("/api/v1/webui/extensions", "{\"file\":\"clock.zip\",\"dir\":\"clock\",\"index\":\"index.json\","
		"\"signature\":\"" + sig + "\"}").code == 400);
	REQUIRE(access((x.tmp + "/index.json").c_str(), F_OK) != 0);

	// the archive the index offers, by its id and its SHA-256
	REQUIRE(x.pack("clock.tar.gz", "clock"));
	const Reply in = installFromIndex(x, "clock", index, sig);
	REQUIRE(in.code == 201);
	REQUIRE(x.read(x.web + "/extensions/clock/index.html") == "<p>clock 1.0</p>");
	REQUIRE(access((x.tmp + "/index.json").c_str(), F_OK) != 0);
	REQUIRE(x.leftovers().empty());

	// an index of another shape is refused whole, however well signed
	const std::string good = "{\"id\":\"clock\",\"version\":\"1.0\",\"url\":\"https://ext.example/c.tar.gz\",\"sha256\":\"" + sha + "\"}";
	const std::string entry_head = "{\"format\":1,\"extensions\":[{\"id\":";
	struct Shape { std::string text; const char *says; };
	const Shape shapes[] = {
		{ "[]", "not a JSON object" },
		{ "{\"format\":2,\"extensions\":[]}", "not of format 1" },
		{ "{\"format\":1,\"extensions\":{}}", "something other than a list" },
		{ "{\"format\":1,\"extensions\":[" + good + "," + good + "]}", "lists clock twice" },
		{ entry_head + "\"bad.name\",\"version\":\"1\",\"url\":\"https://e/x\",\"sha256\":\"" + sha + "\"}]}", "no id an extension can carry" },
		{ entry_head + "\"clock\",\"version\":\"1 0\",\"url\":\"https://e/x\",\"sha256\":\"" + sha + "\"}]}", "no version" },
		{ entry_head + "\"clock\",\"version\":\"1\",\"url\":\"http://e/x\",\"sha256\":\"" + sha + "\"}]}", "no https address" },
		{ entry_head + "\"clock\",\"version\":\"1\",\"url\":\"https://e/x\",\"sha256\":\"abc\"}]}", "no SHA-256" },
	};
	for (size_t i = 0; i < sizeof(shapes) / sizeof(shapes[0]); ++i)
	{
		INFO(shapes[i].says);
		ExtFixture::write(x.tmp + "/index.json", shapes[i].text);
		const Reply r = authedPost("/api/v1/webui/extension-index", indexBody("index.json", signer.sign(shapes[i].text)));
		REQUIRE(r.code == 400);
		REQUIRE(r.body.find(shapes[i].says) != std::string::npos);
		REQUIRE(access((x.tmp + "/index.json").c_str(), F_OK) != 0);
	}
	REQUIRE(authedPost("/api/v1/webui/extension-index", indexBody("../index.json", sig)).code == 400);
	const Reply none = authedPost("/api/v1/webui/extension-index", indexBody("none.json", sig));
	REQUIRE(none.code == 404);
	REQUIRE(none.body.find("no-such-name") != std::string::npos);
	REQUIRE(send(Post, "/api/v1/webui/extension-index", indexBody("index.json", sig), AuthLevel::Write, "127.0.0.1").code == 403);

	// an image without a key checks no index, and installs nothing from one
	const std::string missing = x.box.at("no-key.pub");
	coreapi::extensions::internal::index_key = missing.c_str();
	ExtFixture::write(x.tmp + "/index.json", index);
	const Reply keyless = authedPost("/api/v1/webui/extension-index", indexBody("index.json", sig));
	REQUIRE(x.pack("clock.tar.gz", "clock"));
	const Reply keyless_install = installFromIndex(x, "clock", index, sig);
	coreapi::extensions::internal::index_key = signer.pub.c_str();
	REQUIRE(keyless.code == 501);
	REQUIRE(keyless.body.find("no Ed25519 key") != std::string::npos);
	REQUIRE(keyless_install.code == 501);
	REQUIRE(access((x.tmp + "/clock.tar.gz").c_str(), F_OK) != 0);
	REQUIRE(access((x.tmp + "/index.json").c_str(), F_OK) != 0);
}

TEST_CASE("an archive that is not one extension's files and directories is refused, and nothing changes", "[write][extensions]")
{
	ShippedRoutes shipped;
	ExtFixture x;

	REQUIRE(authedPost("/api/v1/webui/extensions", "{\"file\":\"../clock.tar.gz\",\"unsigned\":true}").code == 400);
	const Reply none = authedPost("/api/v1/webui/extensions", "{\"file\":\"none.tar.gz\",\"unsigned\":true}");
	REQUIRE(none.code == 404);
	REQUIRE(none.body.find("no-such-name") != std::string::npos);

	struct Refused
	{
		const char *file;
		const char *says;
	};
	std::vector<BackupFixture::RawEntry> raw;
	BackupFixture::RawEntry manifest = { "clock/manifest.json", '0', "{}" };
	BackupFixture::RawEntry other = { "other/manifest.json", '0', "{}" };
	raw.push_back(manifest);
	raw.push_back(other);
	x.raw("two.tar", raw);
	raw.clear();
	BackupFixture::RawEntry page = { "clock/index.html", '0', "<p/>" };
	raw.push_back(page);
	x.raw("nomanifest.tar", raw);
	raw.clear();
	BackupFixture::RawEntry link = { "clock/evil", '2', "" };
	raw.push_back(manifest);
	raw.push_back(link);
	x.raw("link.tar", raw);
	raw.clear();
	BackupFixture::RawEntry climb = { "clock/../../etc/x", '0', "x" };
	raw.push_back(manifest);
	raw.push_back(climb);
	x.raw("climb.tar", raw);
	raw.clear();
	BackupFixture::RawEntry absolute = { "/clock/manifest.json", '0', "{}" };
	raw.push_back(absolute);
	x.raw("absolute.tar", raw);
	raw.clear();
	// a manifest with no directory around it
	BackupFixture::RawEntry loose = { "manifest.json", '0', "{}" };
	raw.push_back(loose);
	x.raw("loose.tar", raw);
	raw.clear();
	BackupFixture::RawEntry badname = { "bad name/manifest.json", '0', "{}" };
	raw.push_back(badname);
	x.raw("badname.tar", raw);
	raw.clear();
	// what the manifest says of its scripts
	struct Manifest
	{
		const char *file;
		const char *text;
	};
	const Manifest manifests[] = {
		{ "notobject.tar",  "[]" },
		{ "extra.tar",      "{} {}" },
		{ "comment.tar",    "{\"name\":\"Clock\" /* a note */}" },
		{ "linecomment.tar", "{\"scripts\":[] // a note\n}" },
		{ "notlist.tar",    "{\"scripts\":\"tool\"}" },
		{ "badscript.tar",  "{\"scripts\":[\"bad$name\"]}" },
		{ "dashscript.tar", "{\"scripts\":[\"-tool\"]}" },
		{ "notext.tar",     "{\"scripts\":[1]}" },
		{ "missing.tar",    "{\"scripts\":[\"tool\"]}" },
	};
	for (size_t i = 0; i < sizeof(manifests) / sizeof(manifests[0]); ++i)
	{
		BackupFixture::RawEntry m = { "clock/manifest.json", '0', manifests[i].text };
		raw.push_back(m);
		x.raw(manifests[i].file, raw);
		raw.clear();
	}
	BackupFixture::RawEntry tool_manifest = { "clock/manifest.json", '0', "{\"scripts\":[\"tool\"]}" };
	BackupFixture::RawEntry tool_dir = { "clock/scripts/tool.sh/", '5', "" };
	raw.push_back(tool_manifest);
	raw.push_back(tool_dir);
	x.raw("scriptdir.tar", raw);
	raw.clear();
	BackupFixture::RawEntry twice = { "clock/manifest.json", '0', "{\"again\":1}" };
	raw.push_back(manifest);
	raw.push_back(twice);
	x.raw("twice.tar", raw);
	raw.clear();
	BackupFixture::RawEntry asfile = { "clock/data", '0', "x" };
	BackupFixture::RawEntry asdir = { "clock/data/inner.txt", '0', "y" };
	raw.push_back(manifest);
	raw.push_back(asfile);
	raw.push_back(asdir);
	x.raw("fileanddir.tar", raw);
	ExtFixture::write(x.tmp + "/junk.tar.gz", "this is no archive\n");
	const Refused refused[] = {
		{ "two.tar",        "more than one top-level directory" },
		{ "nomanifest.tar", "manifest.json" },
		{ "link.tar",       "a link or a device" },
		{ "climb.tar",      "climbs" },
		{ "absolute.tar",   "absolute" },
		{ "loose.tar",      "a file beside its directory" },
		{ "badname.tar",    "letters, digits, dashes and underscores" },
		{ "notobject.tar",  "is not a JSON object" },
		{ "extra.tar",      "is not a JSON object" },
		{ "comment.tar",    "is not a JSON object" },
		{ "linecomment.tar", "is not a JSON object" },
		{ "notlist.tar",    "other than a list" },
		{ "badscript.tar",  "names a script other than" },
		{ "dashscript.tar", "names a script other than" },
		{ "notext.tar",     "names a script other than" },
		{ "missing.tar",    "the archive holds no scripts" },
		{ "scriptdir.tar",  "the archive holds no scripts" },
		{ "twice.tar",      "twice" },
		{ "fileanddir.tar", "as a file and as a directory" },
		{ "junk.tar.gz",    "cannot be read" },
	};
	for (size_t i = 0; i < sizeof(refused) / sizeof(refused[0]); ++i)
	{
		INFO(refused[i].file);
		const Reply r = authedPost("/api/v1/webui/extensions", std::string("{\"file\":\"") + refused[i].file + "\",\"unsigned\":true}");
		REQUIRE(r.code == 400);
		REQUIRE(r.body.find("extension-refused") != std::string::npos);
		REQUIRE(r.body.find(refused[i].says) != std::string::npos);
		REQUIRE(access((x.tmp + "/" + refused[i].file).c_str(), F_OK) != 0);
		REQUIRE(access((x.web + "/extensions/clock").c_str(), F_OK) != 0);
		REQUIRE(x.leftovers().empty());
	}
}

TEST_CASE("an extension's script that fails, runs too long or writes too much is said", "[write][extensions]")
{
	ShippedRoutes shipped;
	ExtFixture x;
	std::vector<std::pair<std::string, std::string> > files;
	files.push_back(std::make_pair("clock/manifest.json", "{\"name\":\"Clock\",\"scripts\":[\"fails\",\"slow\",\"loud\",\"clock_tool\"]}"));
	files.push_back(std::make_pair("clock/scripts/fails.sh", "#!/bin/sh\nexit 3\n"));
	files.push_back(std::make_pair("clock/scripts/slow.sh", "#!/bin/sh\nsleep 5\n"));
	files.push_back(std::make_pair("clock/scripts/loud.sh", "#!/bin/sh\nhead -c 70000 /dev/zero | tr '\\0' a\n"));
	files.push_back(std::make_pair("clock/scripts/clock_tool.sh", sayingTool("one")));
	x.tree(files);
	REQUIRE(x.pack("clock.tar.gz", "clock"));
	REQUIRE(authedPost("/api/v1/webui/extensions", "{\"file\":\"clock.tar.gz\",\"unsigned\":true}").code == 201);

	const Reply failed = runClock("fails", "[]");
	REQUIRE(failed.code == 500);
	REQUIRE(failed.body.find("exit-status") != std::string::npos);
	const Reply loud = runClock("loud", "[]");
	REQUIRE(loud.code == 500);
	REQUIRE(loud.body.find("output-too-large") != std::string::npos);
	// a time of its own, so the case does not wait the ten seconds a box gives a script
	coreapi::extensions::internal::script_timeout_ms = 300;
	const Reply slow = runClock("slow", "[]");
	coreapi::extensions::internal::script_timeout_ms = coreapi::extensions::kScriptTimeoutMs;
	REQUIRE(slow.code == 409);
	REQUIRE(slow.body.find("timeout") != std::string::npos);
	const Reply zero = runClock("clock_tool", "[\"a\\u0000b\"]");
	REQUIRE(zero.code == 400);
	REQUIRE(zero.body.find("value-has-zero-byte") != std::string::npos);
	REQUIRE(authedDelete("/api/v1/webui/extensions/clock").code == 204);
}

TEST_CASE("one extension change at a time, and a disc that refuses is said", "[write][extensions]")
{
	ShippedRoutes shipped;
	ExtFixture x;
	x.tree(clockFiles("1.0", "#!/bin/sh\necho one\n"));
	REQUIRE(x.pack("clock.tar.gz", "clock"));

	coreapi::extensions::internal::holdTurnForTest(true);
	const Reply busy = authedPost("/api/v1/webui/extensions", "{\"file\":\"clock.tar.gz\",\"unsigned\":true}");
	const Reply busy_remove = authedDelete("/api/v1/webui/extensions/clock");
	coreapi::extensions::internal::holdTurnForTest(false);
	REQUIRE(busy.code == 409);
	REQUIRE(busy.body.find("extension-refused") != std::string::npos);
	REQUIRE(busy_remove.code == 409);

	if (geteuid() != 0)
	{
		// the extensions directory not writable: nothing is put in place or taken away
		REQUIRE(::mkdir((x.web + "/extensions").c_str(), 0755) == 0);
		REQUIRE(x.pack("clock.tar.gz", "clock"));
		REQUIRE(authedPost("/api/v1/webui/extensions", "{\"file\":\"clock.tar.gz\",\"unsigned\":true}").code == 201);
		chmod((x.web + "/extensions").c_str(), 0555);
		REQUIRE(x.pack("clock.tar.gz", "clock"));
		const Reply refused = authedPost("/api/v1/webui/extensions", "{\"file\":\"clock.tar.gz\",\"unsigned\":true}");
		const Reply kept = authedDelete("/api/v1/webui/extensions/clock");
		chmod((x.web + "/extensions").c_str(), 0755);
		REQUIRE(refused.code == 500);
		REQUIRE(refused.body.find("extension-refused") != std::string::npos);
		REQUIRE(kept.code == 500);
		REQUIRE(x.read(x.web + "/extensions/clock/index.html") == "<p>clock 1.0</p>");
	}
}

TEST_CASE("the pages' own scripts and what is not a plain file are never an extension's", "[write][extensions]")
{
	ShippedRoutes shipped;
	ExtFixture x;

	/* Y_Tools.sh as an extension installed by the old manager left it: its
	   copy in the pages' place. Never the extension's to take away. */
	std::vector<std::pair<std::string, std::string> > files = clockFiles("1.0", sayingTool("one"));
	REQUIRE(::mkdir((x.web + "/extensions").c_str(), 0755) == 0);
	REQUIRE(std::system(("mkdir -p '" + x.web + "/extensions/clock/scripts'").c_str()) == 0);
	ExtFixture::write(x.web + "/extensions/clock/manifest.json", "{}");
	ExtFixture::write(x.web + "/extensions/clock/scripts/Y_Tools.sh", "#!/bin/sh\necho the pages' own\n");
	files.push_back(std::make_pair("clock/scripts/Y_Tools.sh", "#!/bin/sh\necho the pages' own\n"));
	x.tree(files);
	REQUIRE(x.pack("clock.tar.gz", "clock"));
	REQUIRE(authedPost("/api/v1/webui/extensions", "{\"file\":\"clock.tar.gz\",\"unsigned\":true}").code == 201);
	REQUIRE(x.read(x.web + "/scripts/Y_Tools.sh") == "#!/bin/sh\necho the pages' own\n");
	REQUIRE(authedDelete("/api/v1/webui/extensions/clock").code == 204);
	REQUIRE(x.read(x.web + "/scripts/Y_Tools.sh") == "#!/bin/sh\necho the pages' own\n");

	// A link in the pages' scripts under the name of one of its scripts is neither its copy nor followed.
	ExtFixture::write(x.box.at("elsewhere.sh"), sayingTool("one"));
	REQUIRE(symlink(x.box.at("elsewhere.sh").c_str(), (x.web + "/scripts/clock_tool.sh").c_str()) == 0);
	x.tree(clockFiles("1.0", sayingTool("one")));
	REQUIRE(x.pack("clock.tar.gz", "clock"));
	REQUIRE(authedPost("/api/v1/webui/extensions", "{\"file\":\"clock.tar.gz\",\"unsigned\":true}").code == 201);
	REQUIRE(x.pack("clock.tar.gz", "clock"));
	REQUIRE(authedPost("/api/v1/webui/extensions", "{\"file\":\"clock.tar.gz\",\"unsigned\":true}").code == 201);
	REQUIRE(authedDelete("/api/v1/webui/extensions/clock").code == 204);
	struct stat link_st;
	REQUIRE(lstat((x.web + "/scripts/clock_tool.sh").c_str(), &link_st) == 0);
	REQUIRE(S_ISLNK(link_st.st_mode));
	REQUIRE(x.read(x.box.at("elsewhere.sh")) == sayingTool("one"));
	REQUIRE(::unlink((x.web + "/scripts/clock_tool.sh").c_str()) == 0);

	// An extension directory that is a link is no extension: not written through, not removed, not run.
	REQUIRE(::mkdir(x.box.at("outside").c_str(), 0755) == 0);
	REQUIRE(std::system(("mkdir -p '" + x.box.at("outside") + "/scripts'").c_str()) == 0);
	ExtFixture::write(x.box.at("outside/manifest.json"), "{\"scripts\":[\"clock_tool\"]}");
	ExtFixture::write(x.box.at("outside/scripts/clock_tool.sh"), sayingTool("outside"));
	REQUIRE(chmod(x.box.at("outside/scripts/clock_tool.sh").c_str(), 0755) == 0);
	REQUIRE(symlink(x.box.at("outside").c_str(), (x.web + "/extensions/clock").c_str()) == 0);
	REQUIRE(runClock("clock_tool", "[]").code == 404);
	REQUIRE(authedDelete("/api/v1/webui/extensions/clock").code == 404);
	REQUIRE(x.pack("clock.tar.gz", "clock"));
	REQUIRE(authedPost("/api/v1/webui/extensions", "{\"file\":\"clock.tar.gz\",\"unsigned\":true}").code == 201);
	REQUIRE(x.read(x.box.at("outside/manifest.json")) == "{\"scripts\":[\"clock_tool\"]}");
	struct stat st;
	REQUIRE(lstat((x.web + "/extensions/clock").c_str(), &st) == 0);
	REQUIRE(S_ISDIR(st.st_mode));

	/* What runs is the file in its own scripts/: not one changed into a link
	   at the box, not through a scripts/ that is a link, not one that cannot
	   be run, and nothing a manifest changed at the box no longer names. */
	const std::string own = x.web + "/extensions/clock/scripts";
	REQUIRE(::rename((own + "/clock_tool.sh").c_str(), (own + "/kept.sh").c_str()) == 0);
	REQUIRE(symlink(x.box.at("outside/scripts/clock_tool.sh").c_str(), (own + "/clock_tool.sh").c_str()) == 0);
	const Reply through = runClock("clock_tool", "[]");
	REQUIRE(through.code == 404);
	REQUIRE(through.body.find("script-missing") != std::string::npos);
	REQUIRE(::unlink((own + "/clock_tool.sh").c_str()) == 0);
	REQUIRE(::rename((own + "/kept.sh").c_str(), (own + "/clock_tool.sh").c_str()) == 0);
	REQUIRE(::rename(own.c_str(), (own + ".kept").c_str()) == 0);
	REQUIRE(symlink(x.box.at("outside/scripts").c_str(), own.c_str()) == 0);
	REQUIRE(runClock("clock_tool", "[]").code == 404);
	REQUIRE(parsed(send(Get, "/api/v1/webui/extensions", "", AuthLevel::Read, "192.168.1.20").body)["items"][0]["scripts"].size() == 0);
	REQUIRE(::unlink(own.c_str()) == 0);
	REQUIRE(::rename((own + ".kept").c_str(), own.c_str()) == 0);
	REQUIRE(chmod((own + "/clock_tool.sh").c_str(), 0644) == 0);
	REQUIRE(runClock("clock_tool", "[]").code == 404);
	REQUIRE(chmod((own + "/clock_tool.sh").c_str(), 0755) == 0);
	REQUIRE(parsed(runClock("clock_tool", "[]").body)["output"].asString() == "one:|");
	ExtFixture::write(x.web + "/extensions/clock/manifest.json", "{\"scripts\":[]}");
	REQUIRE(runClock("clock_tool", "[]").code == 404);
	ExtFixture::write(x.web + "/extensions/clock/manifest.json", "not json");
	REQUIRE(runClock("clock_tool", "[]").code == 404);
	REQUIRE(parsed(send(Get, "/api/v1/webui/extensions", "", AuthLevel::Read, "192.168.1.20").body)["items"][0]["scripts"].size() == 0);
	REQUIRE(authedDelete("/api/v1/webui/extensions/clock").code == 204);

	// A script in a directory below scripts/ is unpacked as a file of it, not one it runs.
	std::vector<std::pair<std::string, std::string> > deeper = clockFiles("1.0", sayingTool("one"));
	deeper.push_back(std::make_pair("clock/scripts/lib/helper.sh", "#!/bin/sh\n"));
	x.tree(deeper);
	REQUIRE(x.pack("clock.tar.gz", "clock"));
	const Reply in = authedPost("/api/v1/webui/extensions", "{\"file\":\"clock.tar.gz\",\"unsigned\":true}");
	REQUIRE(in.code == 201);
	REQUIRE(parsed(in.body)["scripts"].size() == 1);
	REQUIRE(stat((x.web + "/extensions/clock/scripts/lib/helper.sh").c_str(), &st) == 0);
	REQUIRE((st.st_mode & 0777) == 0644);
	REQUIRE(access((x.web + "/scripts/helper.sh").c_str(), F_OK) != 0);

	// An upload past 16 MiB is refused and goes.
	{
		std::FILE *f = std::fopen((x.tmp + "/big.tar.gz").c_str(), "w");
		REQUIRE(f != NULL);
		REQUIRE(ftruncate(fileno(f), (off_t) coreapi::extensions::kMaxArchiveBytes + 1) == 0);
		std::fclose(f);
	}
	const Reply big = authedPost("/api/v1/webui/extensions", "{\"file\":\"big.tar.gz\",\"unsigned\":true}");
	REQUIRE(big.code == 400);
	REQUIRE(big.body.find("16 MiB") != std::string::npos);
	REQUIRE(access((x.tmp + "/big.tar.gz").c_str(), F_OK) != 0);

	// A second change while one is under way: the upload goes all the same.
	REQUIRE(x.pack("clock.tar.gz", "clock"));
	coreapi::extensions::internal::holdTurnForTest(true);
	const Reply busy = authedPost("/api/v1/webui/extensions", "{\"file\":\"clock.tar.gz\",\"unsigned\":true}");
	coreapi::extensions::internal::holdTurnForTest(false);
	REQUIRE(busy.code == 409);
	REQUIRE(access((x.tmp + "/clock.tar.gz").c_str(), F_OK) != 0);

	if (geteuid() != 0)
	{
		// The pages' scripts directory is no concern of an install any more.
		x.tree(clockFiles("2.0", sayingTool("two")));
		REQUIRE(x.pack("clock.tar.gz", "clock"));
		chmod((x.web + "/scripts").c_str(), 0555);
		const Reply in_place = authedPost("/api/v1/webui/extensions", "{\"file\":\"clock.tar.gz\",\"unsigned\":true}");
		chmod((x.web + "/scripts").c_str(), 0755);
		REQUIRE(in_place.code == 201);
		REQUIRE(x.read(x.web + "/extensions/clock/index.html") == "<p>clock 2.0</p>");
	}
	REQUIRE(x.leftovers().empty());
}

TEST_CASE("the switch that shows a logo is a line of this server's own file", "[write]")
{
	/* It decides what a page draws and not what the television draws, which is
	   why it is a line here and not a setting of the box: a switch in
	   neutrino.conf would take the pictures off the screen in the room as
	   well. */
	ShippedRoutes shipped;
	WebFile file("logos", kWebFileHead);

	// On for a box whose file says nothing about it, which is the box every
	// upgrade arrives as.
	REQUIRE(config().channel_logos);

	const Response r = putWebserver("{\"channel_logos\":false}");
	REQUIRE(r.code == 200);

	const ::Json::Value root = parsed(r.body);
	REQUIRE_FALSE(root["webserver"]["channel_logos"].asBool());
	// Nothing about where the server answers moved, so an address a caller
	// holds is still an address.
	REQUIRE_FALSE(root["caller"]["port_moved"].asBool());
	REQUIRE(root["webserver"]["port"].asInt() == 8081);

	REQUIRE(wholeOf(file.path).find("channel_logos=false") != std::string::npos);

	/* And the server is put on it. What is in effect is read without a lock and
	   is only ever replaced while the daemon is down, so a change written and
	   not put in effect would leave the file saying one thing and the server
	   answering another until the next start. */
	REQUIRE(root["caller"]["restarting"].asBool());
	REQUIRE(r.reload_after == file.path);
}

TEST_CASE("asking for the logos a box already shows moves nothing", "[write]")
{
	ShippedRoutes shipped;
	WebFile file("logos-same", kWebFileHead + std::string("channel_logos=true\n"));

	REQUIRE(config().channel_logos);

	const Response r = putWebserver("{\"channel_logos\":true}");
	REQUIRE(r.code == 200);
	REQUIRE_FALSE(parsed(r.body)["caller"]["restarting"].asBool());
	REQUIRE(r.reload_after.empty());
}

TEST_CASE("a line about the logos this server cannot read turns them off", "[write]")
{
	/* The rule every key of this file keeps: a key the file does not name falls
	   back to the default, and a key it names and this cannot read falls back to
	   the least the key can mean, which for a switch that shows something is
	   off. */
	ShippedRoutes shipped;
	WebFile file("logos-junk", kWebFileHead + std::string("channel_logos=vielleicht\n"));

	REQUIRE_FALSE(config().channel_logos);

	const Reply r = authedGet("/api/v1/system/webserver");
	REQUIRE(r.code == 200);
	REQUIRE_FALSE(parsed(r.body)["channel_logos"].asBool());
}

namespace
{

/* The lists the box reads web channels and guides from, held here instead of in
   the program's settings: what a write was handed, and whether it asked for the
   reload. */
struct FakeWebSources : coreapi::websources::Source
{
	coreapi::websources::Lists lists;
	bool reload_asked;
	int writes;

	FakeWebSources() : reload_asked(false), writes(0)
	{
		lists.webtv.push_back("http://example.com/tv.xml");
		lists.xmltv.push_back("/var/tuxbox/xmltv/guide.xml");
	}

	coreapi::Status read(coreapi::websources::Lists &out) const
	{
		out = lists;
		return coreapi::Status::Ok;
	}

	coreapi::Status write(coreapi::websources::Kind kind, const std::vector<std::string> &entries,
	                      bool reload)
	{
		if (kind == coreapi::websources::WebTv)
			lists.webtv = entries;
		else if (kind == coreapi::websources::WebRadio)
			lists.webradio = entries;
		else
			lists.xmltv = entries;
		reload_asked = reload;
		++writes;
		return coreapi::Status::Ok;
	}
};

struct WebSourcesFixture
{
	FakeWebSources fake;
	WebSourcesFixture() { coreapi::websources::setSource(&fake); }
	~WebSourcesFixture() { coreapi::websources::setSource(NULL); }
};

Response putWebSources(const std::string &kind, const std::string &body, const std::string &query = "",
                       AuthLevel as = AuthLevel::Write)
{
	return dispatch(Put, "/api/v1/channels/sources/" + kind, query, body, "192.0.2.7", as);
}

} // namespace

TEST_CASE("the lists the box reads web channels and guides from are read and replaced one at a time", "[write]")
{
	ShippedRoutes shipped;
	WebSourcesFixture sources;

	// A read asks for what a write does: an address in these lists often carries a key.
	REQUIRE(send(Get, "/api/v1/channels/sources", "", AuthLevel::Read, "192.0.2.7").code == 403);
	const Reply read = send(Get, "/api/v1/channels/sources", "", AuthLevel::Write, "192.0.2.7");
	REQUIRE(read.code == 200);
	const ::Json::Value was = parsed(read.body);
	REQUIRE(was["webtv"].size() == 1);
	REQUIRE(was["webtv"][0].asString() == "http://example.com/tv.xml");
	REQUIRE(was["webradio"].size() == 0);
	REQUIRE(was["xmltv"][0].asString() == "/var/tuxbox/xmltv/guide.xml");
	REQUIRE(was["webtv_dir"].asString() == coreapi::websources::webtvDir());
	REQUIRE(was["webradio_dir"].asString() == coreapi::websources::webradioDir());

	// One list replaced, in order, and the other two left as they are.
	const Response r = putWebSources("webradio",
		"[\"https://radio.example/list.m3u\",\"/var/tuxbox/webradio/mine.xml\"]", "reload=true");
	REQUIRE(r.code == 200);
	const ::Json::Value now = parsed(r.body);
	REQUIRE(now["webradio"].size() == 2);
	REQUIRE(now["webradio"][1].asString() == "/var/tuxbox/webradio/mine.xml");
	REQUIRE(now["webtv"].size() == 1);
	REQUIRE(now["xmltv"].size() == 1);
	REQUIRE(sources.fake.reload_asked);

	// [] empties one, and without reload nothing is reread.
	REQUIRE(putWebSources("xmltv", "[]").code == 200);
	REQUIRE(sources.fake.lists.xmltv.empty());
	REQUIRE_FALSE(sources.fake.reload_asked);

	const Endpoint *ep = routeFor(Put, "/api/v1/channels/sources/{kind}");
	REQUIRE(ep != NULL);
	answerFitsShape(now, *ep->schema, "PUT /api/v1/channels/sources/{kind}");
}

TEST_CASE("a list with an entry the box cannot read is turned down whole", "[write]")
{
	ShippedRoutes shipped;
	WebSourcesFixture sources;

	const char *const bodies[] = {
		"[\"http://ok.example/a.xml\",\"ftp://example.com/b.xml\"]",
		"[\"relative/list.xml\"]",
		"[\"http://example.com/a\\nb.xml\"]",
		"[\"\"]",
		"[\"/media/News#1.m3u\"]",
		"{\"webtv\":[]}",
		"[1,2]",
	};
	for (size_t i = 0; i < sizeof(bodies) / sizeof(bodies[0]); ++i)
	{
		INFO(bodies[i]);
		const Response r = putWebSources("webtv", bodies[i]);
		REQUIRE(r.code == 400);
		REQUIRE(r.body.find("bad-string") != std::string::npos);
	}
	REQUIRE(sources.fake.writes == 0);
	REQUIRE(sources.fake.lists.webtv.size() == 1);

	// The detail names the entry.
	REQUIRE(putWebSources("webtv", bodies[0]).body.find("entry 2") != std::string::npos);

	// More than the box reads.
	std::string many = "[";
	for (int i = 0; i < 33; ++i)
		many += std::string(i ? "," : "") + "\"http://example.com/" + std::to_string(i) + ".xml\"";
	many += "]";
	REQUIRE(putWebSources("webtv", many).code == 400);
	REQUIRE(sources.fake.writes == 0);

	// A kind the box does not have is not a list.
	REQUIRE(putWebSources("webcam", "[]").code == 400);

	// A guide address longer than the guide daemon takes is refused, where the
	// same address is a channel list the box can read.
	const std::string longer = "http://example.com/" + std::string(82, 'g') + ".xml";
	REQUIRE(longer.size() == 105);
	REQUIRE(putWebSources("xmltv", "[\"" + longer + "\"]").code == 400);
	REQUIRE(sources.fake.writes == 0);
	REQUIRE(putWebSources("webtv", "[\"" + longer + "\"]").code == 200);
}

namespace
{

/* Directories of the case's own for the channel files: an upload directory,
   and a zapit directory with the box's ubouquets.xml in it. */
struct ChannelFileFixture
{
	Sandbox     dirs;
	std::string uploads;
	std::string zapit;
	const char *before_uploads;
	const char *before_zapit;

	ChannelFileFixture()
		: uploads(dirs.at("tmp")), zapit(dirs.at("zapit")),
		  before_uploads(coreapi::channels::internal::upload_dir),
		  before_zapit(coreapi::channels::internal::zapit_dir)
	{
		mkdir(uploads.c_str(), 0700);
		mkdir(zapit.c_str(), 0700);
		write(zapit + "/ubouquets.xml", kOwnBouquets);
		coreapi::channels::internal::upload_dir = uploads.c_str();
		coreapi::channels::internal::zapit_dir = zapit.c_str();
	}

	~ChannelFileFixture()
	{
		coreapi::channels::internal::upload_dir = before_uploads;
		coreapi::channels::internal::zapit_dir = before_zapit;
	}

	static const char kOwnBouquets[];

	static void write(const std::string &path, const std::string &text)
	{
		FILE *f = std::fopen(path.c_str(), "w");
		if (f != NULL)
		{
			std::fwrite(text.data(), 1, text.size(), f);
			std::fclose(f);
		}
	}

	static std::string read(const std::string &path)
	{
		std::string out;
		FILE *f = std::fopen(path.c_str(), "r");
		if (f == NULL)
			return "(none)";
		char buf[4096];
		size_t n;
		while ((n = std::fread(buf, 1, sizeof(buf), f)) > 0)
			out.append(buf, n);
		std::fclose(f);
		return out;
	}

	// What is left in a directory besides the names given.
	static std::vector<std::string> others(const std::string &dir, const char *keep)
	{
		std::vector<std::string> out;
		DIR *d = opendir(dir.c_str());
		if (d == NULL)
			return out;
		struct dirent *e;
		while ((e = readdir(d)) != NULL)
		{
			const std::string n(e->d_name);
			if (n != "." && n != ".." && (keep == NULL || n != keep))
				out.push_back(n);
		}
		closedir(d);
		return out;
	}

	private:
		ChannelFileFixture(const ChannelFileFixture &);
		ChannelFileFixture &operator=(const ChannelFileFixture &);
};

const char ChannelFileFixture::kOwnBouquets[] =
	"<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<zapit>\n\t<Bouquet name=\"own\"/>\n</zapit>\n";

const char kBouquetsUp[] =
	"<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<zapit>\n\t<Bouquet name=\"Neu\" hidden=\"0\" locked=\"0\">\n"
	"\t\t<S i=\"0001\" t=\"0001\" on=\"0001\" s=\"192\" frq=\"11494\"/>\n\t</Bouquet>\n</zapit>\n";

const char kServicesUp[] =
	"<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<zapit api=\"4\">\n"
	"\t<sat name=\"Astra 19.2E\" position=\"192\" diseqc=\"0\" uncommited=\"0\">\n\t</sat>\n</zapit>\n";

Reply putFile(const std::string &name, const std::string &file)
{
	return authedPost("/api/v1/channels/files/" + name, "{\"file\":\"" + file + "\"}");
}

} // namespace

TEST_CASE("the body example a route is documented with is a request that route accepts", "[write][openapi]")
{
	ShippedRoutes shipped;
	BoxFixture box;
	giveBouquets(box.channels);
	ProcFixture proc;
	FakeInputDevice input;
	InstalledInputDevice installed_input(&input);
	// Read by the icon route before it writes.
	box.store.ints["mode_icons"] = 0;
	box.store.ints["mode_icons_skin"] = 0;

	const std::string example_channel = parsed(openapi::document())["paths"]["/api/v1/zap"]["post"]
		["requestBody"]["content"]["application/json"]["example"]["channel_id"].asString();
	box.channels.channels.push_back(makeChannel(std::strtoull(example_channel.c_str(), NULL, 16),
	                                            "the example"));

	std::map<std::string, std::string> none;
	std::map<std::string, std::string> first;
	first["bouquet"] = "Erstes";
	std::map<std::string, std::string> second;
	second["bouquet"] = "Zweites";

	REQUIRE(sendBodyExample("POST", "/api/v1/zap", none) == 202);
	REQUIRE(sendBodyExample("POST", "/api/v1/mode", none) == 202);
	{
		FakeRecordingSource recordings;
		InstalledRecordingSource installed_recordings(&recordings);
		REQUIRE(sendBodyExample("POST", "/api/v1/recordings", none) == 202);
	}
	{
		WebuiFixture webui;
		REQUIRE(sendBodyExample("PATCH", "/api/v1/webui/settings", none) == 200);
	}
	{
		WakeFixture wake;
		REQUIRE(sendBodyExample("POST", "/api/v1/network/wake", none) == 204);
	}
	{
		BackupFixture b;
		b.archive("settings.tar.gz", b.config + "/neutrino.conf");
		REQUIRE(sendBodyExample("POST", "/api/v1/system/restore", none) == 202);
	}
	REQUIRE(sendBodyExample("POST", "/api/v1/channels/reload", none) == 202);
	{
		ChannelFileFixture files;
		files.write(files.uploads + "/ubouquets.xml", ChannelFileFixture::kOwnBouquets);
		std::map<std::string, std::string> name;
		name["name"] = "ubouquets";
		REQUIRE(sendBodyExample("POST", "/api/v1/channels/files/{name}", name) == 204);
	}
	{
		WebSourcesFixture sources;
		std::map<std::string, std::string> kind;
		kind["kind"] = "webtv";
		REQUIRE(sendBodyExample("PUT", "/api/v1/channels/sources/{kind}", kind) == 200);
	}
	REQUIRE(sendBodyExample("PUT", "/api/v1/bouquets/{bouquet}/channels", first) == 204);
	REQUIRE(sendBodyExample("PUT", "/api/v1/bouquets/{bouquet}/hidden", first) == 204);
	REQUIRE(sendBodyExample("PUT", "/api/v1/bouquets/{bouquet}/locked", first) == 204);
	// The first one is already at the top.
	REQUIRE(sendBodyExample("PUT", "/api/v1/bouquets/{bouquet}/position", second) == 204);
	REQUIRE(sendBodyExample("PUT", "/api/v1/bouquets/{bouquet}/name", first) == 204);
	REQUIRE(sendBodyExample("POST", "/api/v1/bouquets", none) == 201);

	REQUIRE(sendBodyExample("POST", "/api/v1/timers", none) == 201);
	REQUIRE(sendBodyExample("PATCH", "/api/v1/timers/{id}", none) == 200);

	REQUIRE(sendBodyExample("PATCH", "/api/v1/settings/{section}", none) == 200);
	REQUIRE(sendBodyExample("POST", "/api/v1/settings/secret/clear", none) == 200);
	REQUIRE(sendBodyExample("POST", "/api/v1/system/standby", none) == 202);

	REQUIRE(sendBodyExample("PUT", "/api/v1/osd/volume", none) == 202);
	REQUIRE(sendBodyExample("PUT", "/api/v1/osd/mute", none) == 202);
	REQUIRE(sendBodyExample("POST", "/api/v1/osd/message", none) == 202);
	REQUIRE(sendBodyExample("PUT", "/api/v1/osd/remote", none) == 202);
	REQUIRE(sendBodyExample("POST", "/api/v1/osd/remote/key", none) == 202);
	REQUIRE(sendBodyExample("PUT", "/api/v1/osd/infoicons", none) == 204);
}

TEST_CASE("the body examples of the routes that write files are requests those routes accept", "[write][openapi]")
{
	ShippedRoutes shipped;

	{
		RootsFixture roots;
		REQUIRE(::mkdir(roots.box.at("movie").c_str(), 0700) == 0);
		REQUIRE(sendBodyExample("POST", "/api/v1/storage/directory",
		                        std::map<std::string, std::string>(),
		                        "/media/hdd", roots.box.dir) == 201);
	}
	{
		RootsFixture roots;
		FILE *old = std::fopen(roots.box.at("old.txt").c_str(), "w");
		REQUIRE(old != NULL);
		REQUIRE(std::fclose(old) == 0);
		REQUIRE(sendBodyExample("POST", "/api/v1/storage/rename",
		                        std::map<std::string, std::string>(),
		                        "/media/hdd", roots.box.dir) == 204);
	}
	{
		NetfsFixture netfs;
		REQUIRE(sendBodyExample("PUT", "/api/v1/storage/netfs/{table}/{slot}",
		                        std::map<std::string, std::string>()) == 200);
	}
	{
		FakeShares shares;
		REQUIRE(sendBodyExample("PUT", "/api/v1/storage/shares/{slot}",
		                        std::map<std::string, std::string>()) == 200);
	}
	{
		ExtFixture x;
		x.tree(clockFiles("1.0", "#!/bin/sh\necho one\n"));
		REQUIRE(x.pack("clock.tar.gz", "clock"));
		REQUIRE(sendBodyExample("POST", "/api/v1/webui/extensions",
		                        std::map<std::string, std::string>()) == 201);
		std::map<std::string, std::string> script;
		script["dir"] = "clock";
		script["name"] = "clock_tool";
		REQUIRE(sendBodyExample("POST", "/api/v1/webui/extensions/{dir}/scripts/{name}", script) == 200);
	}
	{
		WebFile file("example", kWebFileHead);
		REQUIRE(sendBodyExample("PUT", "/api/v1/system/webserver",
		                        std::map<std::string, std::string>()) == 200);
	}
	{
		ConfigFixture cfg;
		WebConfig c = config();
		c.username = "root";
		c.password_hash = hashSecret(kShippedPassword, 2000);
		setConfigForTest(c);
		REQUIRE(sendBodyExample("POST", "/api/v1/login", std::map<std::string, std::string>()) == 200);
	}
	{
		// the example replaces the shipped password, which the box starts from
		WebFile file("example-password", std::string(kWebFileHead) + "password_hash=" +
		             hashSecret(kShippedPassword, 2000) + "\n");
		REQUIRE(sendBodyExample("PUT", "/api/v1/account/password",
		                        std::map<std::string, std::string>()) == 204);
	}
}

/* What follows sends each route the request that provokes a refusal its table
   states and no other case here reaches. The suite keeps a record of what every
   shipped route answered (support/answers.cpp), and on a run of the whole suite a
   refusal that is stated and never answered fails it: a statement nothing holds
   to its route is one that can go on saying what the route no longer does. */

namespace
{

// The web server's configuration as the case found it, put back on the way out.
struct WebConfigKept
{
	WebConfig before;
	WebConfigKept() : before(config()) {}
	~WebConfigKept() { setConfigForTest(before); }
};

bool writeText(const std::string &path, const std::string &text)
{
	FILE *f = std::fopen(path.c_str(), "w");
	if (f == NULL)
		return false;
	const bool all = std::fwrite(text.data(), 1, text.size(), f) == text.size();
	return (std::fclose(f) == 0) && all;
}

std::string readText(const std::string &path)
{
	std::string got;
	FILE *f = std::fopen(path.c_str(), "r");
	if (f == NULL)
		return got;
	char buf[4096];
	size_t n;
	while ((n = std::fread(buf, 1, sizeof(buf), f)) > 0)
		got.append(buf, n);
	std::fclose(f);
	return got;
}

std::string renameBody(const std::string &from, const std::string &to, bool overwrite = false)
{
	return "{\"from\":\"" + from + "\",\"to\":\"" + to + "\"" +
	       (overwrite ? ",\"overwrite\":true}" : "}");
}

} // namespace

TEST_CASE("a logo route says whether logos are offered and whether there is one", "[write][refusals]")
{
	ShippedRoutes shipped;
	BoxFixture box;
	FakeLogoSource logos;
	InstalledLogoSource installed_logos(&logos);
	WebConfigKept kept;

	WebConfig c = config();
	c.channel_logos = false;
	setConfigForTest(c);
	const Reply off = authedGet("/api/v1/channels/2b66/logo");
	REQUIRE(off.code == 404);
	REQUIRE(off.body.find("logos-not-offered") != std::string::npos);

	c.channel_logos = true;
	setConfigForTest(c);
	logos.status = coreapi::Status::NotFound;
	const Reply none = authedGet("/api/v1/channels/2b66/logo");
	REQUIRE(none.code == 404);
	REQUIRE(none.body.find("no-such-logo") != std::string::npos);
}

TEST_CASE("the guide as XMLTV says when the bouquet list cannot be read", "[write][refusals]")
{
	ShippedRoutes shipped;
	BoxFixture box;
	box.channels.bouquets_status = coreapi::Status::Internal;

	const Reply r = authedGet("/api/v1/epg/xmltv");
	REQUIRE(r.code == 500);
	REQUIRE(r.body.find("bouquet-list-unavailable") != std::string::npos);
}

TEST_CASE("a picture this box cannot take is refused as not captured", "[write][refusals]")
{
	ShippedRoutes shipped;
	BoxFixture box;
	FakeScreenshotSource screen;
	InstalledScreenshotSource installed_screen(&screen);
	screen.screen_status = coreapi::Status::NotSupported;
	screen.display_status = coreapi::Status::NotSupported;

	const Reply osd = authedGet("/api/v1/osd/screenshot");
	REQUIRE(osd.code == 501);
	REQUIRE(osd.body.find("screen-not-captured") != std::string::npos);

	const Reply display = authedGet("/api/v1/osd/display/screenshot");
	REQUIRE(display.code == 501);
	REQUIRE(display.body.find("display-not-captured") != std::string::npos);
}

TEST_CASE("the display routes list, refuse and tell unknown from off", "[write][refusals]")
{
	ShippedRoutes shipped;
	BoxFixture box;
	FakeScreenshotSource screen;
	InstalledScreenshotSource installed_screen(&screen);

	Reply r = authedGet("/api/v1/osd/displays");
	REQUIRE(r.code == 200);
	REQUIRE(parsed(r.body)["items"].size() == 0u);

	r = authedGet("/api/v1/osd/displays/graphlcd/screenshot");
	REQUIRE(r.code == 501);
	REQUIRE(r.body.find("display-not-captured") != std::string::npos);

	r = authedGet("/api/v1/osd/displays/tft/screenshot");
	REQUIRE(r.code == 404);
	REQUIRE(r.body.find("no-such-display") != std::string::npos);

	// Both drawing: listed in that order, each pictured under its name, and
	// the older route that names none answers the first.
	box.store.ints["glcd_enable"] = 1;
	box.store.ints["lcd4l_support"] = 2;
	box.store.ints["lcd4l_screenshots"] = 1;
	screen.display_status = coreapi::Status::Ok;
	screen.live_displays.insert("graphlcd");
	screen.live_displays.insert("lcd4linux");
	r = authedGet("/api/v1/osd/displays");
	REQUIRE(r.code == 200);
	const ::Json::Value items = parsed(r.body)["items"];
	REQUIRE(items.size() == 2u);
	REQUIRE(items[0u]["name"].asString() == "graphlcd");
	REQUIRE(items[1u]["name"].asString() == "lcd4linux");

	r = authedGet("/api/v1/osd/displays/lcd4linux/screenshot?at=3");
	REQUIRE(r.code == 200);
	REQUIRE(r.content_type == "image/png");
	REQUIRE(screen.last_display == "lcd4linux");

	r = authedGet("/api/v1/osd/display/screenshot");
	REQUIRE(r.code == 200);
	REQUIRE(screen.last_display == "graphlcd");

	// One form only, so naming one is naming a parameter the route lacks.
	r = authedGet("/api/v1/osd/displays/lcd4linux/screenshot?format=jpeg");
	REQUIRE(r.code == 400);

	// A picture that could not be written is a fault, on either route.
	screen.display_status = coreapi::Status::Internal;
	r = authedGet("/api/v1/osd/displays/graphlcd/screenshot");
	REQUIRE(r.code == 500);
	REQUIRE(r.body.find("display-not-captured") != std::string::npos);
	r = authedGet("/api/v1/osd/display/screenshot");
	REQUIRE(r.code == 500);
}

TEST_CASE("the tuner routes say when there is no tuner and when the list cannot be read", "[write][refusals]")
{
	ShippedRoutes shipped;
	BoxFixture box;
	FakeTunerSource tuner;
	InstalledTunerSource installed_tuner(&tuner);
	// Live television, so the answer is about the tuner and not about the mode.
	box.channels.mode = NeutrinoModes::mode_tv;
	tuner.live_status = coreapi::Status::NotSupported;
	tuner.list_status = coreapi::Status::Internal;

	const Reply signal = authedGet("/api/v1/tuner/signal");
	REQUIRE(signal.code == 501);
	REQUIRE(signal.body.find("no-tuner") != std::string::npos);

	const Reply list = authedGet("/api/v1/tuner/frontends");
	REQUIRE(list.code == 500);
	REQUIRE(list.body.find("frontend-list-unavailable") != std::string::npos);
}

TEST_CASE("each tuner that is receiving says what it measures, and an idle one is not asked", "[write]")
{
	ShippedRoutes shipped;
	BoxFixture box;
	FakeTunerSource tuner;
	InstalledTunerSource installed_tuner(&tuner);
	box.channels.mode = NeutrinoModes::mode_tv;

	coreapi::FrontendInfo live;
	live.number = 0;
	live.name = "Si2166";
	live.satellite = true;
	live.live = true;
	tuner.tuners.push_back(live);
	coreapi::FrontendInfo idle;
	idle.number = 1;
	idle.name = "Si2166";
	idle.satellite = true;
	tuner.tuners.push_back(idle);

	coreapi::SignalInfo reading;
	reading.strength = 0xc000;
	reading.snr = 0x8000;
	reading.full_scale = 0xFFFF;
	reading.bit_error_rate = 3;
	reading.locked = true;
	tuner.readings[std::make_pair(0, 0)] = reading;
	// filed for the idle one as well, so a reading it shows was asked for
	tuner.readings[std::make_pair(0, 1)] = reading;

	Reply r = authedGet("/api/v1/tuner/frontends");
	REQUIRE(r.code == 200);
	::Json::Value got = parsed(r.body);
	REQUIRE(got["items"].size() == 2);

	const ::Json::Value first = got["items"][0];
	REQUIRE(first["live"].asBool());
	REQUIRE(first["measured"].asBool());
	REQUIRE(first["signal"]["strength"].asUInt() == 0xc000);
	REQUIRE(first["signal"]["snr"].asUInt() == 0x8000);
	REQUIRE(first["signal"]["full_scale"].asUInt() == 0xFFFF);
	REQUIRE(first["signal"]["bit_error_rate"].asUInt() == 3);
	REQUIRE(first["signal"]["locked"].asBool());
	// the pair names the tuner beside the readings and not inside them
	REQUIRE_FALSE(first["signal"].isMember("adapter"));

	const ::Json::Value second = got["items"][1];
	REQUIRE_FALSE(second["live"].asBool());
	REQUIRE_FALSE(second["measured"].asBool());
	REQUIRE(second["signal"]["strength"].asUInt() == 0);
	REQUIRE_FALSE(second["signal"]["locked"].asBool());
	REQUIRE(tuner.signal_reads == 1u);

	/* A web channel plays: the assignment still names the first frontend,
	   which nothing uses now, and it is not asked. One held by a recording
	   receives whatever the box shows, and is. */
	box.channels.mode = NeutrinoModes::mode_webtv;
	tuner.tuners[1].in_use = true;
	r = authedGet("/api/v1/tuner/frontends");
	REQUIRE(r.code == 200);
	got = parsed(r.body);
	REQUIRE_FALSE(got["items"][0]["live"].asBool());
	REQUIRE_FALSE(got["items"][0]["measured"].asBool());
	REQUIRE(got["items"][1]["measured"].asBool());
	REQUIRE(got["items"][1]["signal"]["strength"].asUInt() == 0xc000);
	REQUIRE(tuner.signal_reads == 2u);

	// a reading the box cannot have leaves the row unmeasured, the list whole
	tuner.readings.clear();
	r = authedGet("/api/v1/tuner/frontends");
	REQUIRE(r.code == 200);
	REQUIRE_FALSE(parsed(r.body)["items"][1]["measured"].asBool());
}

TEST_CASE("a configuration document the box has never written is not found", "[write][refusals]")
{
	ShippedRoutes shipped;
	Sandbox empty;
	const std::string before = coreapi::config::configRoot();
	coreapi::config::setConfigRoot(empty.dir);
	const Reply r = authedGet("/api/v1/config/ubouquets");
	coreapi::config::setConfigRoot(before);

	REQUIRE(r.code == 404);
	REQUIRE(r.body.find("config-document-unreadable") != std::string::npos);
}

TEST_CASE("a key the remote control has no name for is refused before anything is sent", "[write][refusals]")
{
	ShippedRoutes shipped;
	BoxFixture box;
	FakeInputDevice input;
	InstalledInputDevice installed_input(&input);

	const Reply r = authedPost("/api/v1/osd/remote/key", "{\"name\":\"KEY_NOTHING_OF_THE_KIND\"}");
	REQUIRE(r.code == 400);
	REQUIRE(r.body.find("no-such-key") != std::string::npos);
	REQUIRE(input.writes == 0u);
}

TEST_CASE("a script route refuses a name that is no script's and one that is not there", "[write][refusals]")
{
	ShippedRoutes shipped;
	Sandbox scripts;
	const std::string before = coreapi::plugins::scriptRoot();
	coreapi::plugins::setScriptRoot(scripts.dir);
	const Reply option = authedPost("/api/v1/scripts/-x", "");
	const Reply missing = authedPost("/api/v1/scripts/nothere", "");
	coreapi::plugins::setScriptRoot(before);

	REQUIRE(option.code == 400);
	REQUIRE(option.body.find("bad-name") != std::string::npos);
	REQUIRE(missing.code == 404);
	REQUIRE(missing.body.find("script-missing") != std::string::npos);
}

TEST_CASE("a recording of what the box shows goes to its loop, once and not unasked from standby", "[write][recordings]")
{
	ShippedRoutes shipped;
	BoxFixture box;
	FakeRecordingSource recordings;
	InstalledRecordingSource installed_recordings(&recordings);
	box.channels.current.id = kChannel;

	const Reply started = authedPost("/api/v1/recordings", "");
	REQUIRE(started.code == 202);
	REQUIRE(box.events.sent.size() == 1);
	REQUIRE(box.events.sent[0].id == (unsigned) NeutrinoMessages::RECORD_START);
	// The loop tells this apart from a timer's recording by the missing event.
	CTimerd::RecordingInfo sent;
	REQUIRE(box.events.sent[0].body.size() == sizeof(sent));
	std::memcpy(&sent, box.events.sent[0].body.data(), sizeof(sent));
	REQUIRE(sent.eventID == 0);
	REQUIRE(sent.channel_id == kChannel);

	// A shift of that channel does not stand in the way: the recording starts beside it.
	coreapi::RecordingInfo shift;
	shift.id = 7;
	shift.channel_id = kChannel;
	shift.timeshift = true;
	recordings.recordings.push_back(shift);
	REQUIRE(authedPost("/api/v1/recordings", "{}").code == 202);

	// A recording of it does, and nothing is sent.
	coreapi::RecordingInfo rec;
	rec.id = 8;
	rec.channel_id = kChannel;
	recordings.recordings.push_back(rec);
	const size_t before = box.events.sent.size();
	const Reply twice = authedPost("/api/v1/recordings", "");
	REQUIRE(twice.code == 409);
	REQUIRE(twice.body.find("recording-running") != std::string::npos);
	REQUIRE(box.events.sent.size() == before);

	// In standby only with leave to switch the box on.
	recordings.recordings.clear();
	box.channels.mode = NeutrinoModes::mode_standby;
	const Reply asleep = authedPost("/api/v1/recordings", "{\"wake\":false}");
	REQUIRE(asleep.code == 409);
	REQUIRE(asleep.body.find("box-in-standby") != std::string::npos);
	REQUIRE(box.events.sent.size() == before);
	REQUIRE(authedPost("/api/v1/recordings", "{\"wake\":true}").code == 202);
	REQUIRE(box.events.sent.size() == before + 1);
}

TEST_CASE("the recording routes refuse a recording that is not there and a second shift", "[write][refusals]")
{
	ShippedRoutes shipped;
	BoxFixture box;
	FakeRecordingSource recordings;
	InstalledRecordingSource installed_recordings(&recordings);

	const Reply gone = authedDelete("/api/v1/recordings/99");
	REQUIRE(gone.code == 404);
	REQUIRE(gone.body.find("no-such-recording") != std::string::npos);

	coreapi::RecordingInfo shift;
	shift.id = 7;
	shift.channel_id = kChannel;
	shift.timeshift = true;
	recordings.recordings.push_back(shift);
	const Reply second = authedPost("/api/v1/recordings/timeshift", "");
	REQUIRE(second.code == 409);
	REQUIRE(second.body.find("timeshift-running") != std::string::npos);
}

TEST_CASE("the file routes refuse names outside the roots, a taken name and what is no plain file", "[write][refusals]")
{
	ShippedRoutes shipped;
	RootsFixture roots;

	const Reply read_outside = authedGet("/api/v1/storage/file?path=/etc/hostname");
	REQUIRE(read_outside.code == 400);
	REQUIRE(read_outside.body.find("outside-roots") != std::string::npos);

	const Reply write_outside = authedPut("/api/v1/storage/file?path=/etc/a-new-name", "bytes");
	REQUIRE(write_outside.code == 400);
	REQUIRE(write_outside.body.find("outside-roots") != std::string::npos);

	REQUIRE(writeText(roots.box.at("taken"), "was there"));
	const Reply taken = authedPut("/api/v1/storage/file?path=" + roots.box.at("taken"), "new");
	REQUIRE(taken.code == 409);
	REQUIRE(taken.body.find("name-taken") != std::string::npos);

	REQUIRE(::mkdir(roots.box.at("folder").c_str(), 0700) == 0);
	const Reply folder = authedPut("/api/v1/storage/file?path=" + roots.box.at("folder") + "&overwrite=true", "new");
	REQUIRE(folder.code == 409);
	REQUIRE(folder.body.find("not-a-plain-file") != std::string::npos);
}

TEST_CASE("a file too large for one request goes up in pieces and takes its name at the end", "[write][storage]")
{
	ShippedRoutes shipped;
	RootsFixture roots;
	const std::string part = roots.box.at(".big.bin.upload");
	const std::string name = roots.box.at("big.bin");

	REQUIRE(authedPut("/api/v1/storage/file?path=" + part, "first-").code == 201);
	REQUIRE(authedPatch("/api/v1/storage/file?path=" + part + "&offset=6", "second-").code == 204);

	// The same piece again finds the file further on, and is refused.
	const Reply twice = authedPatch("/api/v1/storage/file?path=" + part + "&offset=6", "second-");
	REQUIRE(twice.code == 409);
	REQUIRE(twice.body.find("offset-mismatch") != std::string::npos);
	REQUIRE(readText(part) == "first-second-");

	REQUIRE(authedPost("/api/v1/storage/rename", renameBody(part, name)).code == 204);
	REQUIRE(readText(name) == "first-second-");

	// A finished upload takes a name already in use only with leave to.
	REQUIRE(writeText(roots.box.at(".next.upload"), "newer"));
	const Reply taken = authedPost("/api/v1/storage/rename", renameBody(roots.box.at(".next.upload"), name));
	REQUIRE(taken.code == 409);
	REQUIRE(taken.body.find("name-taken") != std::string::npos);
	REQUIRE(readText(name) == "first-second-");
	REQUIRE(authedPost("/api/v1/storage/rename", renameBody(roots.box.at(".next.upload"), name, true)).code == 204);
	REQUIRE(readText(name) == "newer");
}

TEST_CASE("the roots route names every root and whether it is there", "[write][storage]")
{
	ShippedRoutes shipped;
	RootsFixture roots;

	const Reply r = authedGet("/api/v1/storage/roots");
	REQUIRE(r.code == 200);
	REQUIRE(r.body.find("{\"id\":\"" + roots.box.dir + "\",\"present\":true}") != std::string::npos);
}

TEST_CASE("the rename and the piece routes refuse what the layer below refuses", "[write][refusals]")
{
	ShippedRoutes shipped;
	RootsFixture roots;

	REQUIRE(writeText(roots.box.at("file"), "x"));
	const Reply out_from = authedPost("/api/v1/storage/rename", renameBody("/etc/hostname", roots.box.at("h")));
	REQUIRE(out_from.code == 400);
	REQUIRE(out_from.body.find("outside-roots") != std::string::npos);
	const Reply out_piece = authedPatch("/api/v1/storage/file?path=/etc/hostname&offset=0", "x");
	REQUIRE(out_piece.code == 400);
	REQUIRE(out_piece.body.find("outside-roots") != std::string::npos);

	const Reply gone = authedPost("/api/v1/storage/rename", renameBody(roots.box.at("missing"), roots.box.at("m")));
	REQUIRE(gone.code == 404);
	REQUIRE(gone.body.find("no-such-name") != std::string::npos);
	const Reply gone_piece = authedPatch("/api/v1/storage/file?path=" + roots.box.at("missing") + "&offset=0", "x");
	REQUIRE(gone_piece.code == 404);
	REQUIRE(gone_piece.body.find("no-such-name") != std::string::npos);

	const Reply no_dir = authedPost("/api/v1/storage/rename", renameBody(roots.box.at("file"), roots.box.at("no/dir")));
	REQUIRE(no_dir.code == 404);
	REQUIRE(no_dir.body.find("no-such-parent") != std::string::npos);
	const Reply no_dir_piece = authedPatch("/api/v1/storage/file?path=" + roots.box.at("no/dir") + "&offset=0", "x");
	REQUIRE(no_dir_piece.code == 404);
	REQUIRE(no_dir_piece.body.find("no-such-parent") != std::string::npos);

	const Reply dots = authedPost("/api/v1/storage/rename", renameBody(roots.box.dir + "/..", roots.box.at("up")));
	REQUIRE(dots.code == 400);
	REQUIRE(dots.body.find("bad-name") != std::string::npos);
	const Reply dot_piece = authedPatch("/api/v1/storage/file?path=" + roots.box.dir + "/.&offset=0", "x");
	REQUIRE(dot_piece.code == 400);
	REQUIRE(dot_piece.body.find("bad-name") != std::string::npos);

	REQUIRE(::mkdir(roots.box.at("d").c_str(), 0700) == 0);
	REQUIRE(::mkdir(roots.box.at("d/e").c_str(), 0700) == 0);
	const Reply itself = authedPost("/api/v1/storage/rename", renameBody(roots.box.at("d"), roots.box.at("d/e/d")));
	REQUIRE(itself.code == 400);
	REQUIRE(itself.body.find("into-itself") != std::string::npos);

	REQUIRE(::mkdir(roots.box.at("folder").c_str(), 0700) == 0);
	const Reply over_dir = authedPost("/api/v1/storage/rename", renameBody(roots.box.at("file"), roots.box.at("folder"), true));
	REQUIRE(over_dir.code == 409);
	REQUIRE(over_dir.body.find("not-a-plain-file") != std::string::npos);
	const Reply piece_dir = authedPatch("/api/v1/storage/file?path=" + roots.box.at("folder") + "&offset=0", "x");
	REQUIRE(piece_dir.code == 409);
	REQUIRE(piece_dir.body.find("not-a-plain-file") != std::string::npos);

	/* A root inside another one keeps its name, as the recording directory
	   under /media does. The roots are worked out again on every request, so
	   the second one goes into the list they start from. */
	REQUIRE(::mkdir(roots.box.at("movie").c_str(), 0700) == 0);
	const std::string both = roots.box.dir + ":" + roots.box.at("movie");
	coreapi::storage::internal::default_roots = both.c_str();
	const Reply root = authedPost("/api/v1/storage/rename", renameBody(roots.box.at("movie"), roots.box.at("films")));
	coreapi::storage::internal::default_roots = roots.box.dir.c_str();
	REQUIRE(root.code == 400);
	REQUIRE(root.body.find("is-a-root") != std::string::npos);
}

TEST_CASE("a rename across two filesystems is refused", "[write][refusals]")
{
	ShippedRoutes shipped;
	RootsFixture roots;

	/* /dev/shm, where the host keeps it apart from the sandbox's filesystem.
	   Without a second one there is nothing to cross, and the case says so. */
	struct stat a, b;
	if (::stat("/dev/shm", &b) != 0 || ::stat(roots.box.dir.c_str(), &a) != 0 || a.st_dev == b.st_dev)
	{
		WARN("no second filesystem beside " + roots.box.dir + " on this host");
		return;
	}
	char tmpl[] = "/dev/shm/endpoints_move_XXXXXX";
	REQUIRE(::mkdtemp(tmpl) != NULL);
	const std::string shm(tmpl);
	const std::string both = roots.box.dir + ":" + shm;
	coreapi::storage::internal::default_roots = both.c_str();

	REQUIRE(writeText(roots.box.at("f"), "here"));
	const Reply r = authedPost("/api/v1/storage/rename", renameBody(roots.box.at("f"), shm + "/f"));
	coreapi::storage::internal::default_roots = roots.box.dir.c_str();
	(void) std::system(("rm -rf '" + shm + "'").c_str());
	REQUIRE(r.code == 400);
	REQUIRE(r.body.find("cross-device") != std::string::npos);
	REQUIRE(readText(roots.box.at("f")) == "here");
}

TEST_CASE("the file routes leave the web server's own store of who may reach the box alone", "[write][refusals]")
{
	/* The store is named from the configuration directory this build was made
	   for, which has to exist for its name to resolve; a host that has none
	   cannot name the store, and says so rather than passing. */
	char where[PATH_MAX];
	if (realpath(CONFIGDIR, where) == NULL)
	{
		WARN("no configuration directory " CONFIGDIR " on this host, so the store cannot be named");
		return;
	}
	ShippedRoutes shipped;
	RootsFixture roots;
	// The roots are worked out again from these on every request; the fixture puts
	// them back.
	const std::string config_root(where);
	coreapi::storage::internal::default_roots = config_root.c_str();
	coreapi::storage::setRoots(config_root);
	const std::string store = config_root + "/nhttpd.conf";

	const Reply read = authedGet("/api/v1/storage/file?path=" + store);
	REQUIRE(read.code == 400);
	REQUIRE(read.body.find("access-store") != std::string::npos);

	const Reply write = authedPut("/api/v1/storage/file?path=" + store + "&overwrite=true", "x");
	REQUIRE(write.code == 400);
	REQUIRE(write.body.find("access-store") != std::string::npos);

	const Reply removed = authedDelete("/api/v1/storage/path?path=" + store);
	REQUIRE(removed.code == 400);
	REQUIRE(removed.body.find("access-store") != std::string::npos);

	const Reply renamed = authedPost("/api/v1/storage/rename", renameBody(store, config_root + "/taken-away"));
	REQUIRE(renamed.code == 400);
	REQUIRE(renamed.body.find("access-store") != std::string::npos);

	const Reply piece = authedPatch("/api/v1/storage/file?path=" + store + "&offset=0", "x");
	REQUIRE(piece.code == 400);
	REQUIRE(piece.body.find("access-store") != std::string::npos);
}

TEST_CASE("the body examples of the script and the plugin routes are requests they accept", "[write][openapi][refusals]")
{
	ShippedRoutes shipped;
	BoxFixture box;
	Sandbox dir;

	const std::string script = dir.at("hello.sh");
	REQUIRE(writeText(script, "#!/bin/sh\necho hello\n"));
	REQUIRE(::chmod(script.c_str(), 0700) == 0);
	const std::string before = coreapi::plugins::scriptRoot();
	coreapi::plugins::setScriptRoot(dir.dir);
	std::map<std::string, std::string> script_name;
	script_name["name"] = "hello";
	const int ran = sendBodyExample("POST", "/api/v1/scripts/{name}", script_name);
	coreapi::plugins::setScriptRoot(before);
	REQUIRE(ran == 200);

	FakePluginSource plugins;
	InstalledPluginSource installed_plugins(&plugins);
	coreapi::PluginInfo example;
	example.name = "example";
	example.title = "Example";
	example.config = dir.at("example.cfg");
	REQUIRE(writeText(example.config, "name=Example\nhide=0\n"));
	plugins.plugins.push_back(example);
	std::map<std::string, std::string> plugin_name;
	plugin_name["name"] = "example";
	REQUIRE(sendBodyExample("PUT", "/api/v1/plugins/{name}/hidden", plugin_name) == 204);
}

TEST_CASE("the file the web interface kept before is taken over once, and nobody's values are shipped ones", "[write][webui]")
{
	ShippedRoutes shipped;
	WebuiFixture webui;

	// a box that never set anything: the shipped values, and a file holding nothing of them
	std::remove(webui.path.c_str());
	coreapi::webui::Settings now = coreapi::webui::read();
	REQUIRE(now["startup"] == "true");
	REQUIRE(now["start_page"] == "none");
	REQUIRE(now["yweb_box_color"] == "fd6d13");
	REQUIRE(access(webui.path.c_str(), F_OK) != 0);

	// the file kept before: a start page under its old name, a key the new file
	// holds already, and a line the new file could not keep
	const std::string former_text =
		"start_page=epg.yhtm\nyweb_box_name=Kueche\nwol_mac_1=aa:bb:cc:dd:ee:ff\nbad key=1\n";
	FILE *f = std::fopen(webui.former.c_str(), "w");
	REQUIRE(f != NULL);
	std::fputs(former_text.c_str(), f);
	std::fclose(f);
	f = std::fopen(webui.path.c_str(), "w");
	REQUIRE(f != NULL);
	std::fputs("yweb_box_name=Wohnzimmer\n", f);
	std::fclose(f);

	REQUIRE(coreapi::webui::migrate());
	const std::string taken = webui.file();
	REQUIRE(taken.find("start_page=epg.html\n") != std::string::npos);
	REQUIRE(taken.find("yweb_box_name=Wohnzimmer\n") != std::string::npos);
	REQUIRE(taken.find("wol_mac_1=aa:bb:cc:dd:ee:ff\n") != std::string::npos);
	REQUIRE(taken.find("bad key") == std::string::npos);
	REQUIRE(taken.find("migrated_from=yWeb.conf\n") != std::string::npos);
	// the shipped values are answered, not written
	REQUIRE(taken.find("startup=") == std::string::npos);

	// done once: a later change to the old file comes over no more, and the old file stays
	f = std::fopen(webui.former.c_str(), "a");
	REQUIRE(f != NULL);
	std::fputs("style=light\n", f);
	std::fclose(f);
	REQUIRE(coreapi::webui::migrate());
	REQUIRE(webui.file() == taken);
	REQUIRE(access(webui.former.c_str(), R_OK) == 0);

	// the routes answer the file as it now reads
	const Reply read = authedGet("/api/v1/webui/settings");
	REQUIRE(read.body.find("{\"key\":\"start_page\",\"value\":\"epg.html\"}") != std::string::npos);
	REQUIRE(read.body.find("{\"key\":\"startup\",\"value\":\"true\"}") != std::string::npos);
}

TEST_CASE("only the interface's own pages are taken over under their new names", "[write][webui]")
{
	WebuiFixture webui;
	std::remove(webui.path.c_str());
	FILE *f = std::fopen(webui.former.c_str(), "w");
	REQUIRE(f != NULL);
	std::fputs("start_page=extensions/foo/index.yhtm\n", f);
	std::fclose(f);
	REQUIRE(coreapi::webui::migrate());
	// an extension's page keeps the name its manifest gives it
	REQUIRE(webui.file().find("start_page=extensions/foo/index.yhtm\n") != std::string::npos);
}

TEST_CASE("a full file taken over still takes a change, its mark counting against no limit", "[write][webui]")
{
	WebuiFixture webui;
	std::remove(webui.path.c_str());
	FILE *f = std::fopen(webui.former.c_str(), "w");
	REQUIRE(f != NULL);
	for (size_t i = 0; i < coreapi::webui::kMaxKeys; ++i)
		std::fprintf(f, "k%03u=1\n", (unsigned) i);
	std::fclose(f);

	REQUIRE(coreapi::webui::migrate());
	const std::string taken = webui.file();
	REQUIRE(taken.find("k255=1\n") != std::string::npos);
	REQUIRE(taken.find("migrated_from=yWeb.conf\n") != std::string::npos);

	// a key that is there changes, one more does not fit
	coreapi::webui::Settings one;
	one["k000"] = "2";
	REQUIRE(coreapi::webui::change(one).ok());
	coreapi::webui::Settings more;
	more["k999"] = "1";
	const coreapi::Result<coreapi::webui::Settings> refused = coreapi::webui::change(more);
	REQUIRE_FALSE(refused.ok());
	REQUIRE(refused.error().code == coreapi::ErrorCode::TooManySettings);
}

TEST_CASE("the web interface's own settings are read and written whole, held to their shape", "[write][webui]")
{
	ShippedRoutes shipped;
	WebuiFixture webui;

	// What is read back, and the key that is not.
	const Reply read = authedGet("/api/v1/webui/settings");
	REQUIRE(read.code == 200);
	REQUIRE(read.body.find("\"start_page\"") != std::string::npos);
	REQUIRE(read.body.find("SECRETVALUE") == std::string::npos);
	REQUIRE(read.body.find("\"withheld\":[\"tmdb_api_key\"]") != std::string::npos);
	REQUIRE(send(Get, "/api/v1/webui/settings", "", AuthLevel::Read, "192.168.1.9").code == 200);

	// Written in one go: a value set, a value taken out with an empty one, the rest kept.
	const Reply wrote = authedPatch("/api/v1/webui/settings",
		"{\"yweb_box_name\":\"Wohnzimmer\",\"start_page\":\"\",\"wol_mac_1\":\"aa:bb:cc:dd:ee:ff\","
		"\"ext_epg-plus.v2_enabled\":\"1\"}");
	REQUIRE(wrote.code == 200);
	REQUIRE(wrote.body.find("Wohnzimmer") != std::string::npos);
	// a key taken out is answered as shipped again
	REQUIRE(wrote.body.find("{\"key\":\"start_page\",\"value\":\"none\"}") != std::string::npos);
	const std::string after = webui.file();
	REQUIRE(after.find("yweb_box_name=Wohnzimmer") != std::string::npos);
	REQUIRE(after.find("start_page") == std::string::npos);
	REQUIRE(after.find("wol_mac_1=aa:bb:cc:dd:ee:ff") != std::string::npos);
	REQUIRE(after.find("ext_epg-plus.v2_enabled=1") != std::string::npos);
	// What was written is what is read back.
	REQUIRE(authedGet("/api/v1/webui/settings").body.find("\"value\":\"aa:bb:cc:dd:ee:ff\"") != std::string::npos);
	REQUIRE(after.find("tmdb_api_key=SECRETVALUE") != std::string::npos);

	// Only with the box's own credential: one key is who may reach the old management menus.
	REQUIRE(send(Patch, "/api/v1/webui/settings", "{\"management_ip\":\"1.2.3.4\"}",
	             AuthLevel::Write, "127.0.0.1").code == 403);

	// Every refusal leaves the file as it was.
	struct Case { const char *body; int code; const char *wire; };
	const Case refused[] = {
		{ "{\"1st\":\"x\"}",                      400, "bad-name" },
		{ "{\"a b\":\"x\"}",                      400, "bad-name" },
		{ "{\"_x\":\"x\"}",                       400, "bad-name" },
		{ "{\"page\":\"{=script:Y_Tools=}\"}",    400, "bad-string" },
		{ "{\"page\":\"<img src=x onerror=alert(1)>\"}", 400, "bad-string" },
		{ "{\"page\":\"x\\\" onclick=\\\"y\"}",  400, "bad-string" },
		{ "{\"page\":\"it's\"}",                  400, "bad-string" },
		{ "{\"page\":\"a\\\\b\"}",            400, "bad-string" },
		{ "{\"page\":\"NAS #1\"}",                400, "bad-string" },
		{ "{\"page\":\"one\\ntwo\"}",            400, "bad-string" },
		{ "{\"page\":{\"nested\":1}}",            400, "bad-string" },
		{ "{\"page\":\"1\",\"page\":\"2\"}",  400, "duplicate-parameter" },
	};
	for (size_t i = 0; i < sizeof(refused) / sizeof(refused[0]); ++i)
	{
		INFO(refused[i].body);
		const Reply r = authedPatch("/api/v1/webui/settings", refused[i].body);
		REQUIRE(r.code == refused[i].code);
		REQUIRE(r.body.find(refused[i].wire) != std::string::npos);
		REQUIRE(webui.file() == after);
	}
	const Reply none = authedPatch("/api/v1/webui/settings", "{}");
	REQUIRE(none.code == 400);
	REQUIRE(none.body.find("missing-parameter") != std::string::npos);
	const Reply longer = authedPatch("/api/v1/webui/settings",
		"{\"page\":\"" + std::string(coreapi::webui::kMaxValueLength + 1, 'x') + "\"}");
	REQUIRE(longer.code == 400);
	REQUIRE(longer.body.find("value-too-long") != std::string::npos);
	REQUIRE(webui.file() == after);

	// More keys than the file takes.
	std::string many = "{";
	for (size_t i = 0; i < coreapi::webui::kMaxKeys; ++i)
		many += std::string(i ? "," : "") + "\"k" + std::to_string(i) + "\":\"v\"";
	many += "}";
	const Reply full = authedPatch("/api/v1/webui/settings", many);
	REQUIRE(full.code == 409);
	REQUIRE(full.body.find("too-many-settings") != std::string::npos);
	REQUIRE(webui.file() == after);

	// A key as long as an extension's can make it, and one longer.
	const std::string longest(coreapi::webui::kMaxKeyLength, 'k');
	REQUIRE(authedPatch("/api/v1/webui/settings", "{\"" + longest + "\":\"1\"}").code == 200);
	const Reply overlong = authedPatch("/api/v1/webui/settings", "{\"" + longest + "k\":\"1\"}");
	REQUIRE(overlong.code == 400);
	REQUIRE(overlong.body.find("bad-name") != std::string::npos);
	REQUIRE(authedPatch("/api/v1/webui/settings", "{\"" + longest + "\":\"\"}").code == 200);
	REQUIRE(webui.file() == after);

	// A file that cannot be saved says so: no directory to write it in, and a
	// file that cannot be put in place, which leaves what stood there.
	const std::string nowhere = webui.box.at("no-such-dir/webui.conf");
	coreapi::webui::internal::config_path = nowhere.c_str();
	const Reply unsaved = authedPatch("/api/v1/webui/settings", "{\"page\":\"x\"}");
	const std::string occupied = webui.box.at("taken");
	REQUIRE(mkdir(occupied.c_str(), 0700) == 0);
	coreapi::webui::internal::config_path = occupied.c_str();
	const Reply unplaced = authedPatch("/api/v1/webui/settings", "{\"page\":\"x\"}");
	coreapi::webui::internal::config_path = webui.path.c_str();
	REQUIRE(unsaved.code == 500);
	REQUIRE(unsaved.body.find("setting-not-written") != std::string::npos);
	REQUIRE(unplaced.code == 500);
	REQUIRE(unplaced.body.find("setting-not-written") != std::string::npos);
	struct stat st;
	REQUIRE(stat(occupied.c_str(), &st) == 0);
	REQUIRE(S_ISDIR(st.st_mode));
	REQUIRE(access((occupied + ".tmp").c_str(), F_OK) != 0);
}

namespace
{

/* A pages directory of a case's own with the language files named, taken away
   whole on the way out. */
struct LanguagePages
{
	std::string root;
	std::vector<std::string> made;

	LanguagePages()
	{
		char dir[] = "/tmp/ni-webroute-pages-XXXXXX";
		REQUIRE(::mkdtemp(dir) != NULL);
		root = dir;
		const std::string langs = root + "/languages";
		REQUIRE(::mkdir(langs.c_str(), 0755) == 0);
		made.push_back(langs);
	}

	void language(const std::string &name, const std::string &text)
	{
		const std::string path = root + "/languages/" + name;
		WebFile::put(path, text);
		made.push_back(path);
	}

	~LanguagePages()
	{
		for (size_t i = made.size(); i > 0; --i)
			if (::unlink(made[i - 1].c_str()) != 0)
				::rmdir(made[i - 1].c_str());
		::rmdir(root.c_str());
	}
};

} // namespace

TEST_CASE("the pages' language is read and written by its code, and the session says it", "[write][webui]")
{
	ShippedRoutes shipped;
	LanguagePages pages;
	pages.language("English", "Y_Old=old\n");
	pages.language("en.json", "{}\n");
	pages.language("Deutsch", "Y_Old=alt\n");
	pages.language("de.json", "{}\n");
	pages.language("Polski", "Y_Old=stare\n");
	WebFile web("language", kWebFileHead, std::string(kServerFileHead) + "Language.selected=English\n"
		"WebsiteMain.directory=" + pages.root + "\nWebsiteMain.override_directory=" + pages.root + "/none\n");

	const Reply read = authedGet("/api/v1/webui/language");
	REQUIRE(read.code == 200);
	REQUIRE(read.body == "{\"selected\":\"en\",\"items\":[{\"code\":\"en\",\"name\":\"English\"},"
		"{\"code\":\"de\",\"name\":\"Deutsch\"}]}");
	REQUIRE(send(Get, "/api/v1/webui/language", "", AuthLevel::Read, "192.168.1.9").code == 200);
	// the example the document gives is a request the route takes
	REQUIRE(sendBodyExample("PUT", "/api/v1/webui/language", std::map<std::string, std::string>()) == 200);

	// written by its code, kept by its file's name, and answered as GET reads it
	const Reply wrote = authedPut("/api/v1/webui/language", "{\"language\":\"de\"}");
	REQUIRE(wrote.code == 200);
	REQUIRE(wrote.body.find("\"selected\":\"de\"") != std::string::npos);
	REQUIRE(wholeOf(web.server).find("Language.selected=Deutsch\n") != std::string::npos);

	// the session says it to a caller that holds nothing yet
	const Reply session = send(Get, "/api/v1/session", "", AuthLevel::Public, "192.168.1.9");
	REQUIRE(session.code == 200);
	REQUIRE(session.body.find("\"language\":\"de\"") != std::string::npos);

	// what the pages cannot be shown in, and a caller below system, change nothing
	const std::string before = wholeOf(web.server);
	const Reply old = authedPut("/api/v1/webui/language", "{\"language\":\"pl\"}");
	REQUIRE(old.code == 400);
	REQUIRE(old.body.find("not-a-listed-value") != std::string::npos);
	REQUIRE(authedPut("/api/v1/webui/language", "{\"language\":\"Deutsch\"}").code == 400);
	REQUIRE(authedPut("/api/v1/webui/language", "{}").code == 400);
	REQUIRE(send(Put, "/api/v1/webui/language", "{\"language\":\"en\"}", AuthLevel::Write, "127.0.0.1").code == 403);
	REQUIRE(wholeOf(web.server) == before);

	// a server file that cannot be read back and written again says so
	REQUIRE(::unlink(web.server.c_str()) == 0);
	REQUIRE(::mkdir(web.server.c_str(), 0700) == 0);
	const Reply unwritten = authedPut("/api/v1/webui/language", "{\"language\":\"en\"}");
	REQUIRE(::rmdir(web.server.c_str()) == 0);
	WebFile::put(web.server, before);
	REQUIRE(unwritten.code == 500);
	REQUIRE(unwritten.body.find("setting-not-written") != std::string::npos);
}

TEST_CASE("a device on the box's network is woken with the packet its card listens for", "[write][network]")
{
	ShippedRoutes shipped;
	WakeFixture wake;

	const unsigned char hw[6] = { 0x00, 0x11, 0x22, 0xaa, 0xbb, 0xcc };
	const char *const spellings[] = { "00:11:22:aa:BB:cc", "00-11-22-AA-bb-CC", "001122aabbcc" };
	for (size_t i = 0; i < sizeof(spellings) / sizeof(spellings[0]); ++i)
	{
		INFO(spellings[i]);
		wakePackets().clear();
		REQUIRE(authedPost("/api/v1/network/wake", std::string("{\"mac\":\"") + spellings[i] + "\"}").code == 204);
		const std::vector<unsigned char> &p = wakePackets();
		REQUIRE(p.size() == 6 + 16 * 6);
		for (size_t b = 0; b < 6; ++b)
			REQUIRE(p[b] == 0xff);
		for (size_t r = 0; r < 16; ++r)
			REQUIRE(std::memcmp(&p[6 + r * 6], hw, 6) == 0);
	}

	const char *const wrong[] = {
		"00:11:22:aa:bb", "00:11-22:aa:bb:cc", "zz:11:22:aa:bb:cc", "00:11:22:aa:bb:cc:", "0011.22aa.bbcc", ""
	};
	for (size_t i = 0; i < sizeof(wrong) / sizeof(wrong[0]); ++i)
	{
		INFO(wrong[i]);
		wakePackets().clear();
		const Reply r = authedPost("/api/v1/network/wake", std::string("{\"mac\":\"") + wrong[i] + "\"}");
		REQUIRE(r.code == 400);
		REQUIRE(wakePackets().empty());
	}
	REQUIRE(authedPost("/api/v1/network/wake", "{\"mac\":\"00:11:22:aa:bb:cc:dd\"}").body.find("value-too-long") != std::string::npos);
	REQUIRE(authedPost("/api/v1/network/wake", "{\"mac\":\"0011:22aabbcc\"}").body.find("bad-string") != std::string::npos);

	wakeSendsOk = false;
	const Reply unsent = authedPost("/api/v1/network/wake", "{\"mac\":\"001122aabbcc\"}");
	REQUIRE(unsent.code == 500);
	REQUIRE(unsent.body.find("wake-not-sent") != std::string::npos);

	// A caller the network exemption granted a read wakes nothing.
	wakeSendsOk = true;
	wakePackets().clear();
	REQUIRE(lanPost("/api/v1/network/wake", "{\"mac\":\"001122aabbcc\"}").code == 403);
	REQUIRE(wakePackets().empty());
}

TEST_CASE("a freeze of the settings writes waits for the ones under way and holds off the rest", "[write][backup]")
{
	REQUIRE_FALSE(coreapi::SettingsWrite::frozen());
	coreapi::SettingsWrite *under_way = new coreapi::SettingsWrite;
	REQUIRE(under_way->allowed());
	volatile bool frozen = false;
	std::thread freezer([&frozen]() {
		coreapi::SettingsWrite::freeze();
		frozen = true;
	});
	usleep(200 * 1000);
	// the freeze is on, and waits for the write under way to finish
	REQUIRE(coreapi::SettingsWrite::frozen());
	REQUIRE_FALSE(frozen);
	{
		coreapi::SettingsWrite later;
		REQUIRE_FALSE(later.allowed());
	}
	delete under_way;
	freezer.join();
	REQUIRE(frozen);
	// the writer of every key=value settings file asks as well
	char dir[] = "/tmp/settingswrites_XXXXXX";
	REQUIRE(mkdtemp(dir) != NULL);
	const std::string conf = std::string(dir) + "/scan.conf";
	CConfigFile cfg('=');
	cfg.setString("diseqc", "1");
	REQUIRE_FALSE(cfg.saveConfig(conf));
	REQUIRE(access(conf.c_str(), F_OK) != 0);
	coreapi::SettingsWrite::thaw();
	coreapi::SettingsWrite after;
	REQUIRE(after.allowed());
	REQUIRE(cfg.saveConfig(conf));
	unlink(conf.c_str());
	rmdir(dir);
}

TEST_CASE("the settings menu's restore and the web's take turns", "[write][backup]")
{
	ShippedRoutes shipped;
	BackupFixture b;
	const std::string rel = b.box.dir.substr(1);
	const std::string okPaths = "'" + rel + "/config/neutrino.conf' '" + rel + "/extra/a.conf'";

	// While the menu restores, the web's restore waits its turn.
	{
		coreapi::backup::MenuRestore menu;
		REQUIRE(menu.taken());
		coreapi::backup::MenuRestore second;
		REQUIRE_FALSE(second.taken());
		REQUIRE(b.archive("during.tar.gz", okPaths));
		const Reply during = authedPost("/api/v1/system/restore", "{\"file\":\"during.tar.gz\"}");
		REQUIRE(during.code == 409);
		REQUIRE(during.body.find("another restore") != std::string::npos);
	}

	// A menu restore that put nothing back leaves the turn free for the next.
	{
		coreapi::backup::MenuRestore menu;
		REQUIRE(menu.taken());
	}
	REQUIRE(b.archive("free.tar.gz", okPaths));
	REQUIRE(authedPost("/api/v1/system/restore", "{\"file\":\"free.tar.gz\"}").code == 202);

	// Settings the web put back wait for the restart: the menu does not start
	// another restore, and cannot let go of the hold on the settings writes.
	coreapi::backup::internal::setRestarterForTest(&restartLater);
	REQUIRE(b.archive("web.tar.gz", okPaths));
	REQUIRE(authedPost("/api/v1/system/restore", "{\"file\":\"web.tar.gz\"}").code == 202);
	{
		coreapi::backup::MenuRestore menu;
		REQUIRE_FALSE(menu.taken());
		menu.putBack();
	}
	REQUIRE(coreapi::SettingsWrite::frozen());
	coreapi::backup::internal::restartedForTest();

	// Settings the menu put back, in whole or in part, keep the web's restore off
	// until the restart.
	{
		coreapi::backup::MenuRestore menu;
		REQUIRE(menu.taken());
		coreapi::SettingsWrite::freeze();
		menu.putBack();
	}
	REQUIRE(b.archive("aftermenu.tar.gz", okPaths));
	const Reply aftermenu = authedPost("/api/v1/system/restore", "{\"file\":\"aftermenu.tar.gz\"}");
	REQUIRE(aftermenu.code == 409);
	REQUIRE(aftermenu.body.find("wait for the restart") != std::string::npos);
	REQUIRE(coreapi::SettingsWrite::frozen());
	coreapi::backup::internal::restartedForTest();
	coreapi::backup::internal::setRestarterForTest(&catchReboot);
	REQUIRE_FALSE(coreapi::SettingsWrite::frozen());
	REQUIRE(b.leftovers().empty());
}

TEST_CASE("the box's settings go out as one archive and come back only where it backs up", "[write][backup]")
{
	ShippedRoutes shipped;
	BackupFixture b;

	// The archive backup.sh packed, as a file to keep, and nothing left behind.
	const Reply got = authedGet("/api/v1/system/backup");
	REQUIRE(got.code == 200);
	REQUIRE(got.content_type == "application/gzip");
	REQUIRE(got.body.size() > 2);
	REQUIRE((unsigned char) got.body[0] == 0x1f);
	REQUIRE((unsigned char) got.body[1] == 0x8b);
	REQUIRE(headerOf(got, "Content-Disposition") == "attachment; filename=\"settings_testbox_20261013_1200.tar.gz\"");
	DIR *d = opendir(b.box.dir.c_str());
	REQUIRE(d != NULL);
	struct dirent *e;
	bool leftover = false;
	while ((e = readdir(d)) != NULL)
		leftover = leftover || std::strncmp(e->d_name, "backup.", 7) == 0;
	closedir(d);
	REQUIRE_FALSE(leftover);
	REQUIRE(send(Get, "/api/v1/system/backup", "", AuthLevel::Write, "127.0.0.1").code == 403);

	// Into the configuration directory, and onto what tobackup.conf names: what
	// the archive holds is back, with the permissions it carries and no set-id
	// bit, the box is restarted, the upload is gone and nothing of the unpacking
	// is left behind.
	const std::string rel = b.box.dir.substr(1);
	const std::string okPaths = "'" + rel + "/config/neutrino.conf' '" + rel + "/extra/a.conf'";
	chmod(b.box.at("extra/a.conf").c_str(), 0640);
	b.write(b.config + "/suid.conf", "s=1\n");
	chmod((b.config + "/suid.conf").c_str(), 04755);
	REQUIRE(b.archive("ok.tar.gz", okPaths + " '" + rel + "/config/suid.conf'"));
	b.write(b.config + "/neutrino.conf", "volume=99\n");
	b.write(b.box.at("extra/a.conf"), "a=2\n");
	chmod(b.box.at("extra/a.conf").c_str(), 0600);
	REQUIRE(send(Post, "/api/v1/system/restore", "{\"file\":\"ok.tar.gz\"}", AuthLevel::Write, "127.0.0.1").code == 403);
	REQUIRE(b.read(b.config + "/neutrino.conf") == "volume=99\n");
	REQUIRE(authedPost("/api/v1/system/restore", "{\"file\":\"ok.tar.gz\"}").code == 202);
	REQUIRE(rebootsStarted() == 1);
	REQUIRE(b.read(b.config + "/neutrino.conf") == "volume=50\n");
	REQUIRE(b.read(b.box.at("extra/a.conf")) == "a=1\n");
	struct stat mode;
	REQUIRE(stat(b.box.at("extra/a.conf").c_str(), &mode) == 0);
	REQUIRE((mode.st_mode & 07777) == 0640);
	REQUIRE(stat((b.config + "/suid.conf").c_str(), &mode) == 0);
	REQUIRE((mode.st_mode & 07777) == 0755);
	REQUIRE(access(b.box.at("ok.tar.gz").c_str(), F_OK) != 0);
	REQUIRE(b.leftovers().empty());

	// A pattern from the list, and the box's own layout under config/.
	mkdir(b.box.at("cfg").c_str(), 0700);
	b.write(b.box.at("cfg/x.cfg"), "x\n");
	REQUIRE(b.archive("pattern.tar.gz", "'" + rel + "/cfg/x.cfg'"));
	REQUIRE(authedPost("/api/v1/system/restore", "{\"file\":\"pattern.tar.gz\"}").code == 202);
	REQUIRE(b.read(b.box.at("cfg/x.cfg")) == "x\n");
	// config/... lands below the box's /var/tuxbox, here the sandbox
	REQUIRE(b.archive("boxstyle.tar.gz", "config/neutrino.conf", b.box.dir));
	b.write(b.config + "/neutrino.conf", "volume=98\n");
	REQUIRE(authedPost("/api/v1/system/restore", "{\"file\":\"boxstyle.tar.gz\"}").code == 202);
	REQUIRE(b.read(b.config + "/neutrino.conf") == "volume=50\n");
	// a pax record naming a place inside, with its header inside as well
	{
		std::vector<BackupFixture::RawEntry> raw;
		BackupFixture::RawEntry pax = { "PaxHeaders/x", 'x', BackupFixture::paxRecord("path", rel + "/config/long-name.conf") };
		BackupFixture::RawEntry plain = { rel + "/config/long-na", '0', "x\n" };
		raw.push_back(pax);
		raw.push_back(plain);
		b.rawTarOf("paxok.tar", raw);
	}
	REQUIRE(authedPost("/api/v1/system/restore", "{\"file\":\"paxok.tar\"}").code == 202);
	// unpacked as the pax path says, as GNU tar does, and not as the header's name
	REQUIRE(b.read(b.config + "/long-name.conf") == "x\n");
	REQUIRE(access((b.config + "/long-na").c_str(), F_OK) != 0);
	REQUIRE(rebootsStarted() == 4);

	// Refused, and nothing started: elsewhere, left out of a wider entry, a link, unreadable.
	struct Refused { const char *name; const char *why; };
	mkdir(b.box.at("elsewhere").c_str(), 0700);
	b.write(b.box.at("elsewhere/x.conf"), "x\n");
	REQUIRE(symlink(b.box.at("elsewhere").c_str(), (b.config + "/external").c_str()) == 0);
	REQUIRE(b.archive("through.tar.gz", "'" + rel + "/config/external/x.conf'"));
	REQUIRE(b.archive("elsewhere.tar.gz", "'" + rel + "/other/x.conf'"));
	REQUIRE(b.archive("mixed.tar.gz", "'" + rel + "/config/neutrino.conf' '" + rel + "/other/x.conf'"));
	REQUIRE(symlink("/etc/passwd", (b.config + "/link").c_str()) == 0);
	REQUIRE(b.archive("link.tar.gz", "'" + rel + "/config/link'"));
	b.write(b.box.at("junk.tar.gz"), "not an archive at all\n");
	b.rawTar("climb.tar", rel + "/config/../other/x.conf");
	b.rawTar("newline.tar", rel + "/config/a\nb");
	{
		std::vector<BackupFixture::RawEntry> raw;
		BackupFixture::RawEntry gnu = { "././@LongLink", 'L', rel + "/other/x.conf" + std::string(1, '\0') };
		BackupFixture::RawEntry plain = { rel + "/config/ok.conf", '0', "x\n" };
		raw.push_back(gnu);
		raw.push_back(plain);
		b.rawTarOf("longname.tar", raw);
		raw.clear();
		BackupFixture::RawEntry pax = { "PaxHeaders/ok.conf", 'x', BackupFixture::paxRecord("path", rel + "/other/x.conf") };
		raw.push_back(pax);
		raw.push_back(plain);
		b.rawTarOf("pax.tar", raw);
		raw.clear();
		BackupFixture::RawEntry device = { rel + "/config/dev", '3', "" };
		raw.push_back(device);
		b.rawTarOf("device.tar", raw);
		raw.clear();
		// a pax size telling tar to step elsewhere than the header does
		BackupFixture::RawEntry paxsize = { "PaxHeaders/ok.conf", 'x', BackupFixture::paxRecord("size", "1024") };
		raw.push_back(paxsize);
		raw.push_back(plain);
		b.rawTarOf("paxsize.tar", raw);
		raw.clear();
		BackupFixture::RawEntry sparse = { "PaxHeaders/ok.conf", 'x', BackupFixture::paxRecord("GNU.sparse.size", "2") };
		raw.push_back(sparse);
		raw.push_back(plain);
		b.rawTarOf("sparse.tar", raw);
		raw.clear();
		/* A record length that wraps around once added to where it starts and
		   points back at a record already read: read on, it would be read for ever. */
		BackupFixture::RawEntry wrap = { "PaxHeaders/ok.conf", 'x',
			"13 comment=x\n13 comment=x\n18446744073709551603 comment=z\n" };
		raw.push_back(wrap);
		raw.push_back(plain);
		b.rawTarOf("paxwrap.tar", raw);
	}
	{
		/* An old GNU header keeps times where ustar keeps the prefix: a tar reading
		   it as GNU unpacks the name field alone, here outside, while the prefix
		   in front of it would land inside. */
		std::vector<BackupFixture::RawEntry> raw;
		BackupFixture::RawEntry plain = { "etc-evil.conf", '0', "x\n" };
		raw.push_back(plain);
		b.rawTarOf("prefix.tar", raw, rel + "/config");
		raw.clear();
		/* A directory whose header claims data, and in that data a header of its
		   own: tar does not step over data for a directory, so it would unpack
		   that header. */
		const std::string hidden = BackupFixture::rawHeader("etc/hidden.conf", '0', 2) +
					   std::string("x\n") + std::string(510, '\0');
		BackupFixture::RawEntry dir = { rel + "/config/safe/", '5', hidden };
		raw.push_back(dir);
		b.rawTarOf("dirdata.tar", raw);
		raw.clear();
		// two pax headers before one entry
		BackupFixture::RawEntry pax1 = { "PaxHeaders/a", 'x', BackupFixture::paxRecord("path", rel + "/config/a.conf") };
		BackupFixture::RawEntry pax2 = { "PaxHeaders/b", 'x', BackupFixture::paxRecord("path", rel + "/config/b.conf") };
		BackupFixture::RawEntry inside = { rel + "/config/c.conf", '0', "x\n" };
		raw.push_back(pax1);
		raw.push_back(pax2);
		raw.push_back(inside);
		b.rawTarOf("stacked.tar", raw);
		raw.clear();
		// a file named like a directory, with data an old tar would not step over
		BackupFixture::RawEntry slash = { rel + "/config/dir/", '0', hidden };
		raw.push_back(slash);
		b.rawTarOf("slashfile.tar", raw);
	}
	{
		/* A lone zero block and an entry behind it: GNU tar reads on past it and
		   unpacks that entry. */
		std::string tar = BackupFixture::rawHeader(rel + "/config/ok.conf", '0', 2) + std::string("x\n") +
				  std::string(510, '\0') + std::string(512, '\0') +
				  BackupFixture::rawHeader("etc/behind.conf", '0', 2) + std::string("x\n") +
				  std::string(510, '\0') + std::string(1024, '\0');
		FILE *f = std::fopen(b.box.at("lonezero.tar").c_str(), "wb");
		REQUIRE(f != NULL);
		std::fwrite(tar.data(), 1, tar.size(), f);
		std::fclose(f);
	}
	mkdir(b.box.at("wild").c_str(), 0700);
	REQUIRE(symlink(b.box.at("elsewhere").c_str(), b.box.at("wild/sub").c_str()) == 0);
	b.rawTar("wildlink.tar", rel + "/wild/sub/s.conf");
	// names read alike by no two readers: a . part, and an empty one
	b.rawTar("dotpart.tar", rel + "/config/./a.conf");
	b.rawTar("emptypart.tar", rel + "/config//a.conf");
	// a file where the box has a directory, and a directory where it has a file
	mkdir((b.config + "/adir").c_str(), 0700);
	b.rawTar("ondir.tar", rel + "/config/adir");
	{
		std::vector<BackupFixture::RawEntry> raw;
		BackupFixture::RawEntry dir = { rel + "/config/neutrino.conf/", '5', "" };
		raw.push_back(dir);
		b.rawTarOf("onfile.tar", raw);
		// the archive at odds with itself: a file and a directory of one name, and
		// an entry below a file of its own
		raw.clear();
		BackupFixture::RawEntry both_dir = { rel + "/config/both/", '5', "" };
		BackupFixture::RawEntry both_file = { rel + "/config/both", '0', "x\n" };
		raw.push_back(both_dir);
		raw.push_back(both_file);
		b.rawTarOf("both.tar", raw);
		raw.clear();
		BackupFixture::RawEntry outer = { rel + "/config/outer", '0', "x\n" };
		BackupFixture::RawEntry inner = { rel + "/config/outer/inner.conf", '0', "y\n" };
		raw.push_back(outer);
		raw.push_back(inner);
		b.rawTarOf("below.tar", raw);
	}
	const Refused refused[] = {
		{ "elsewhere.tar.gz", "backs nothing up" },
		{ "mixed.tar.gz",     "backs nothing up" },
		{ "link.tar.gz",      "link" },
		{ "junk.tar.gz",      "cannot be read" },
		{ "climb.tar",        "climbs" },
		{ "through.tar.gz",   "through a link" },
		{ "newline.tar",      "control character" },
		{ "longname.tar",     "backs nothing up" },
		{ "pax.tar",          "backs nothing up" },
		{ "device.tar",       "a device" },
		{ "wildlink.tar",     "through a link" },
		{ "paxsize.tar",      "cannot be read" },
		{ "sparse.tar",       "cannot be read" },
		{ "paxwrap.tar",      "cannot be read" },
		{ "prefix.tar",       "backs nothing up" },
		{ "dirdata.tar",      "cannot be read" },
		{ "stacked.tar",      "cannot be read" },
		{ "slashfile.tar",    "cannot be read" },
		{ "lonezero.tar",     "cannot be read" },
		{ "ondir.tar",        "file where the box has a directory" },
		{ "onfile.tar",       "directory where the box has a file" },
		{ "both.tar",         "as a file and as a directory" },
		{ "below.tar",        "as a file and puts" },
		{ "dotpart.tar",      "or an empty part" },
		{ "emptypart.tar",    "or an empty part" },
	};
	for (size_t i = 0; i < sizeof(refused) / sizeof(refused[0]); ++i)
	{
		INFO(refused[i].name);
		const Reply r = authedPost("/api/v1/system/restore", std::string("{\"file\":\"") + refused[i].name + "\"}");
		REQUIRE(r.code == 400);
		REQUIRE(r.body.find("restore-refused") != std::string::npos);
		REQUIRE(r.body.find(refused[i].why) != std::string::npos);
		REQUIRE(access(b.box.at(refused[i].name).c_str(), F_OK) != 0);
	}
	REQUIRE(rebootsStarted() == 4);
	REQUIRE(b.read(b.config + "/neutrino.conf") == "volume=50\n");
	// a copy goes with its directory, refused or put back
	REQUIRE(b.leftovers().empty());

	// More than the settings of a box come to.
	REQUIRE(b.archive("big.tar.gz", okPaths));
	coreapi::backup::internal::max_unpacked = 5;
	const Reply big = authedPost("/api/v1/system/restore", "{\"file\":\"big.tar.gz\"}");
	coreapi::backup::internal::max_unpacked = 256ull * 1024 * 1024;
	REQUIRE(big.code == 400);
	REQUIRE(big.body.find("unpacks to more") != std::string::npos);
	REQUIRE(b.archive("many.tar.gz", okPaths));
	coreapi::backup::internal::max_members = 1;
	const Reply many = authedPost("/api/v1/system/restore", "{\"file\":\"many.tar.gz\"}");
	coreapi::backup::internal::max_members = 20000;
	REQUIRE(many.code == 400);
	REQUIRE(many.body.find("more names") != std::string::npos);

	// Names that are no upload of this kind, and one with nothing behind it.
	const char *const names[] = { "x.zip", "../x.tar.gz", ".hidden.tar.gz", "sub/x.tar.gz",
	                              "evil.tar --exclude=.tar", "a b.tar.gz", "-x.tar.gz" };
	for (size_t i = 0; i < sizeof(names) / sizeof(names[0]); ++i)
	{
		INFO(names[i]);
		const Reply r = authedPost("/api/v1/system/restore", std::string("{\"file\":\"") + names[i] + "\"}");
		REQUIRE(r.code == 400);
		REQUIRE(r.body.find("bad-name") != std::string::npos);
	}
	const Reply none = authedPost("/api/v1/system/restore", "{\"file\":\"none.tar.gz\"}");
	REQUIRE(none.code == 404);
	REQUIRE(none.body.find("no-such-name") != std::string::npos);

	// A file that cannot be written: the settings as they were, no restart.
	mkdir((b.config + "/ro").c_str(), 0700);
	b.write(b.config + "/ro/x.conf", "x\n");
	REQUIRE(b.archive("ro.tar.gz", "'" + rel + "/config/neutrino.conf' '" + rel + "/config/ro/x.conf'"));
	b.write(b.config + "/neutrino.conf", "volume=97\n");
	chmod((b.config + "/ro").c_str(), 0500);
	const Reply ro = authedPost("/api/v1/system/restore", "{\"file\":\"ro.tar.gz\"}");
	chmod((b.config + "/ro").c_str(), 0700);
	if (geteuid() != 0)
	{
		REQUIRE(ro.code == 500);
		REQUIRE(ro.body.find("could not be written") != std::string::npos);
		REQUIRE(ro.body.find("not restarted") != std::string::npos);
		REQUIRE(b.read(b.config + "/neutrino.conf") == "volume=97\n");
		REQUIRE(rebootsStarted() == 4);
	}
	REQUIRE(b.leftovers().empty());
	// nothing changed, so the box may save its settings again
	REQUIRE_FALSE(coreapi::SettingsWrite::frozen());

	// Back, and the restart not started: said so.
	b.write(b.config + "/neutrino.conf", "volume=50\n");
	REQUIRE(b.archive("noboot.tar.gz", okPaths));
	b.write(b.config + "/neutrino.conf", "volume=96\n");
	rebootStartsOk = false;
	const Reply noboot = authedPost("/api/v1/system/restore", "{\"file\":\"noboot.tar.gz\"}");
	rebootStartsOk = true;
	REQUIRE(noboot.code == 500);
	REQUIRE(noboot.body.find("the settings are back") != std::string::npos);
	REQUIRE(b.read(b.config + "/neutrino.conf") == "volume=50\n");
	// and until the box is restarted by hand it saves no settings and puts none back
	{
		coreapi::SettingsWrite held;
		REQUIRE_FALSE(held.allowed());
	}
	REQUIRE(b.archive("afternoboot.tar.gz", okPaths));
	const Reply afternoboot = authedPost("/api/v1/system/restore", "{\"file\":\"afternoboot.tar.gz\"}");
	REQUIRE(afternoboot.code == 409);
	REQUIRE(afternoboot.body.find("wait for the restart") != std::string::npos);
	REQUIRE(b.read(b.config + "/neutrino.conf") == "volume=50\n");
	coreapi::backup::internal::restartedForTest();

	/* Back and waiting for the restart: the box saves no settings file, and a
	   second restore is turned down, until the restart. */
	coreapi::backup::internal::setRestarterForTest(&restartLater);
	REQUIRE(b.archive("waits.tar.gz", okPaths));
	REQUIRE(authedPost("/api/v1/system/restore", "{\"file\":\"waits.tar.gz\"}").code == 202);
	{
		coreapi::SettingsWrite later;
		REQUIRE_FALSE(later.allowed());
	}
	REQUIRE(b.archive("second.tar.gz", okPaths));
	const Reply second = authedPost("/api/v1/system/restore", "{\"file\":\"second.tar.gz\"}");
	REQUIRE(second.code == 409);
	REQUIRE(second.body.find("wait for the restart") != std::string::npos);
	coreapi::backup::internal::restartedForTest();
	{
		coreapi::SettingsWrite again;
		REQUIRE(again.allowed());
	}
	coreapi::backup::internal::setRestarterForTest(&catchReboot);

	// A directory entry sets its directory's permissions, whatever it has on the box.
	mkdir((b.config + "/secret").c_str(), 0700);
	chmod((b.config + "/secret").c_str(), 0700);
	b.write(b.config + "/secret/s.conf", "s=1\n");
	REQUIRE(b.archive("secret.tar.gz", "'" + rel + "/config/secret'"));
	chmod((b.config + "/secret").c_str(), 0755);
	REQUIRE(authedPost("/api/v1/system/restore", "{\"file\":\"secret.tar.gz\"}").code == 202);
	struct stat secret;
	REQUIRE(stat((b.config + "/secret").c_str(), &secret) == 0);
	REQUIRE((secret.st_mode & 07777) == 0700);

	/* A directory the box does not have yet, read-only in the archive, with a
	   file in it: the file goes in first and the permissions come last, so this
	   works for a program that is not root as well. */
	mkdir((b.config + "/rodir").c_str(), 0700);
	b.write(b.config + "/rodir/r.conf", "r=1\n");
	chmod((b.config + "/rodir").c_str(), 0555);
	REQUIRE(b.archive("rodir.tar.gz", "'" + rel + "/config/rodir'"));
	chmod((b.config + "/rodir").c_str(), 0700);
	REQUIRE(unlink((b.config + "/rodir/r.conf").c_str()) == 0);
	REQUIRE(rmdir((b.config + "/rodir").c_str()) == 0);
	REQUIRE(authedPost("/api/v1/system/restore", "{\"file\":\"rodir.tar.gz\"}").code == 202);
	REQUIRE(b.read(b.config + "/rodir/r.conf") == "r=1\n");
	struct stat rodir;
	REQUIRE(stat((b.config + "/rodir").c_str(), &rodir) == 0);
	REQUIRE((rodir.st_mode & 07777) == 0555);
	chmod((b.config + "/rodir").c_str(), 0700);

	// Started for real: the shell after the unpacking, here running a command
	// that only notes it ran, a moment after the answer. The restart of the
	// program in its own place would replace the test, so the shell runs.
	coreapi::backup::internal::setRestarterForTest(NULL);
	coreapi::backup::internal::restart_in_place = false;
	// with what the shell is handed, which the restart of this program alone runs on
	const std::string note = "[ -x \"$0\" ] && [ \"$1\" = \"" + std::to_string((long) getpid()) +
				 "\" ] && echo started > '" + b.box.at("started") + "'";
	coreapi::backup::internal::restart_command = note.c_str();
	const unsigned before_real = rebootsStarted();
	REQUIRE(b.archive("real.tar.gz", okPaths));
	REQUIRE(authedPost("/api/v1/system/restore", "{\"file\":\"real.tar.gz\"}").code == 202);
	std::string noted;
	for (int i = 0; i < 50 && noted.empty(); ++i)
	{
		usleep(100 * 1000);
		noted = b.read(b.box.at("started"));
		if (noted == "(none)")
			noted.clear();
	}
	REQUIRE(noted == "started\n");
	REQUIRE(rebootsStarted() == before_real);
	coreapi::backup::internal::restartedForTest();
	coreapi::backup::internal::setRestarterForTest(&catchReboot);

	/* Listed places the box does not have yet, as on a freshly flashed box: a
	   listed directory, the directory of a listed file and that of a pattern are
	   made, and what goes into them is put back. */
	{
		const unsigned before = rebootsStarted();
		b.write(b.list, b.read(b.list) + b.box.at("gone") + "/\n" + b.box.at("lone/f.conf") + "\n" +
			b.box.at("pat") + "/*.cfg\n");
		b.rawTar("gone.tar", rel + "/gone/sub/x.conf");
		b.rawTar("lone.tar", rel + "/lone/f.conf");
		b.rawTar("pat.tar", rel + "/pat/y.cfg");
		REQUIRE(access(b.box.at("gone").c_str(), F_OK) != 0);
		REQUIRE(authedPost("/api/v1/system/restore", "{\"file\":\"gone.tar\"}").code == 202);
		REQUIRE(b.read(b.box.at("gone/sub/x.conf")) == "x\n");
		REQUIRE(authedPost("/api/v1/system/restore", "{\"file\":\"lone.tar\"}").code == 202);
		REQUIRE(b.read(b.box.at("lone/f.conf")) == "x\n");
		REQUIRE(authedPost("/api/v1/system/restore", "{\"file\":\"pat.tar\"}").code == 202);
		REQUIRE(b.read(b.box.at("pat/y.cfg")) == "x\n");
		REQUIRE(rebootsStarted() == before + 3);
	}

	/* A working name a restore cut short left behind is no reason to refuse the
	   next one, and is not this restore's to touch; an archive name of that form
	   is refused, as no restore could tell it from its own. */
	{
		const std::string stale = b.config + "/.restore-0123456789abcdef-0";
		b.write(stale, "left over\n");
		b.rawTar("stale.tar", rel + "/config/st.conf");
		REQUIRE(authedPost("/api/v1/system/restore", "{\"file\":\"stale.tar\"}").code == 202);
		REQUIRE(b.read(b.config + "/st.conf") == "x\n");
		REQUIRE(b.read(stale) == "left over\n");
		unlink(stale.c_str());
		b.rawTar("reserved.tar", rel + "/config/.restore-x");
		const Reply reserved = authedPost("/api/v1/system/restore", "{\"file\":\"reserved.tar\"}");
		REQUIRE(reserved.code == 400);
		REQUIRE(reserved.body.find(".restore-") != std::string::npos);
	}

	/* A GNU long name whose header name, cut at 100 bytes, ends in a slash: the
	   name it is unpacked as is the long one, and only that one is held to the
	   rule that a file's name does not end in a slash. */
	{
		std::string head = rel + "/config/";
		REQUIRE(head.size() < 99);
		const std::string long_name = rel + "/config/" + std::string(120, 'l') + ".conf";
		std::vector<BackupFixture::RawEntry> raw;
		BackupFixture::RawEntry gnu = { "././@LongLink", 'L', long_name + std::string(1, '\0') };
		BackupFixture::RawEntry plain = { head, '0', "x\n" };
		raw.push_back(gnu);
		raw.push_back(plain);
		b.rawTarOf("longslash.tar", raw);
		REQUIRE(authedPost("/api/v1/system/restore", "{\"file\":\"longslash.tar\"}").code == 202);
		REQUIRE(b.read("/" + long_name) == "x\n");
	}
	REQUIRE(b.leftovers().empty());

	// No restore.sh is needed to put settings back; without backup.sh nothing is packed.
	REQUIRE(access((b.bin + "/restore.sh").c_str(), F_OK) != 0);
	unlink((b.bin + "/backup.sh").c_str());
	const Reply nobackup = authedGet("/api/v1/system/backup");
	REQUIRE(nobackup.code == 500);
	REQUIRE(nobackup.body.find("backup-failed") != std::string::npos);
	REQUIRE(b.leftovers().empty());
}

TEST_CASE("an uploaded channel or bouquet file takes the place of the box's own and is read in", "[write][channelfiles]")
{
	ShippedRoutes shipped;
	BoxFixture box;
	ChannelFileFixture c;
	const std::string own = c.zapit + "/ubouquets.xml";

	// A bouquet list for the user's bouquets: put in place, the upload gone, nothing
	// else left in either directory, the mode the box's files have, and read in
	// the hard way, so what is in memory is not written over it.
	c.write(c.uploads + "/my list.xml", kBouquetsUp);
	REQUIRE(putFile("ubouquets", "my list.xml").code == 400);
	c.write(c.uploads + "/up.xml", kBouquetsUp);
	REQUIRE(send(Post, "/api/v1/channels/files/ubouquets", "{\"file\":\"up.xml\"}", AuthLevel::Write,
	             "127.0.0.1").code == 403);
	REQUIRE(c.read(own) == ChannelFileFixture::kOwnBouquets);
	const unsigned reinits = box.channels.reinits;
	const unsigned saved = box.channels.services_saved;
	REQUIRE(putFile("ubouquets", "up.xml").code == 204);
	REQUIRE(c.read(own) == kBouquetsUp);
	struct stat st;
	REQUIRE(lstat(own.c_str(), &st) == 0);
	REQUIRE((st.st_mode & 07777) == 0644);
	REQUIRE(box.channels.reinits == reinits + 1);
	REQUIRE(box.channels.services_saved == saved);
	REQUIRE(box.channels.reload_reason == "webif_hardreload");
	REQUIRE(c.others(c.uploads, "my list.xml").empty());
	REQUIRE(c.others(c.zapit, "ubouquets.xml").empty());

	// A services list goes where services belong, under the box's own name for it.
	c.write(c.uploads + "/sat.xml", kServicesUp);
	REQUIRE(putFile("services", "sat.xml").code == 204);
	REQUIRE(c.read(c.zapit + "/services.xml") == kServicesUp);
	REQUIRE(box.channels.reinits == reinits + 2);

	// Refused, each with the box's file untouched, the upload gone and no reading.
	struct Refused
	{
		const char *as;
		const char *text;
		const char *says;
	};
	const Refused refused[] = {
		{ "services", "<zapit api=\"4\">\n\t<cable/>\n</zapit>\n", "cable element without the name" },
		{ "myservices", "<zapit>\n\t<terrestrial position=\"1\"/>\n</zapit>\n", "terrestrial element without the name" },
		{ "services", "<zapit>\n\t<sat position=\"192\"/>\n</zapit>\n", "sat element without the name" },
		{ "bouquets", kServicesUp, "only Bouquet belongs" },
		{ "myservices", kBouquetsUp, "only sat, cable and terrestrial belong" },
		{ "ubouquets", "<?xml version=\"1.0\"?>\n<channels><Bouquet name=\"x\"/></channels>\n", "root element is not zapit" },
		{ "ubouquets", "this is not xml at all <<<", "no XML the box can read" },
	};
	for (size_t i = 0; i < sizeof(refused) / sizeof(refused[0]); i++)
	{
		INFO(refused[i].says);
		c.write(c.uploads + "/bad.xml", refused[i].text);
		const Reply r = putFile(refused[i].as, "bad.xml");
		REQUIRE(r.code == 400);
		REQUIRE(r.body.find("channel-file-refused") != std::string::npos);
		REQUIRE(r.body.find(refused[i].says) != std::string::npos);
		REQUIRE(c.others(c.uploads, "my list.xml").empty());
	}
	REQUIRE(c.read(own) == kBouquetsUp);
	REQUIRE(c.read(c.zapit + "/bouquets.xml") == "(none)");
	REQUIRE(c.read(c.zapit + "/myservices.xml") == "(none)");
	REQUIRE(box.channels.reinits == reinits + 2);

	// Larger than any box's lists.
	{
		std::string big = "<zapit>";
		big.resize(coreapi::channels::kMaxChannelFileBytes + 1, ' ');
		c.write(c.uploads + "/big.xml", big);
		const Reply r = putFile("ubouquets", "big.xml");
		REQUIRE(r.code == 400);
		REQUIRE(r.body.find("larger than 16 MiB") != std::string::npos);
		REQUIRE(c.others(c.uploads, "my list.xml").empty());
	}

	// Names that are no plain upload, one with nothing behind it, and a link.
	const char *names[] = { "../up.xml", ".up.xml", "up.txt", "up", "-up.xml" };
	for (size_t i = 0; i < sizeof(names) / sizeof(names[0]); i++)
	{
		INFO(names[i]);
		const Reply r = putFile("ubouquets", names[i]);
		REQUIRE(r.code == 400);
		REQUIRE(r.body.find("bad-name") != std::string::npos);
	}
	REQUIRE(putFile("ubouquets", "none.xml").code == 404);
	c.write(c.dirs.at("elsewhere.xml"), kBouquetsUp);
	REQUIRE(symlink(c.dirs.at("elsewhere.xml").c_str(), (c.uploads + "/link.xml").c_str()) == 0);
	REQUIRE(putFile("ubouquets", "link.xml").code == 404);
	REQUIRE(putFile("frontend", "up.xml").code == 400);
	REQUIRE(box.channels.reinits == reinits + 2);

	// A link in the file's place is replaced and what it pointed at left alone.
	c.write(c.dirs.at("kept.xml"), "kept");
	REQUIRE(symlink(c.dirs.at("kept.xml").c_str(), (c.zapit + "/bouquets.xml").c_str()) == 0);
	c.write(c.uploads + "/b.xml", kBouquetsUp);
	REQUIRE(putFile("bouquets", "b.xml").code == 204);
	REQUIRE(lstat((c.zapit + "/bouquets.xml").c_str(), &st) == 0);
	REQUIRE(S_ISREG(st.st_mode));
	REQUIRE(c.read(c.dirs.at("kept.xml")) == "kept");

	// A zapit directory nothing can be written to: refused, the box's file as it was.
	chmod(c.zapit.c_str(), 0500);
	c.write(c.uploads + "/u.xml", ChannelFileFixture::kOwnBouquets);
	const Reply locked = putFile("ubouquets", "u.xml");
	chmod(c.zapit.c_str(), 0700);
	if (geteuid() != 0)
	{
		REQUIRE(locked.code == 500);
		REQUIRE(locked.body.find("channel-file-refused") != std::string::npos);
		REQUIRE(c.read(own) == kBouquetsUp);
	}

	// The box not answering the wait before the file goes in: nothing put in
	// place and no reading.
	box.channels.await_status = coreapi::Status::Internal;
	c.write(c.uploads + "/w.xml", ChannelFileFixture::kOwnBouquets);
	const unsigned before_wait = box.channels.reinits;
	const Reply unanswered = putFile("ubouquets", "w.xml");
	box.channels.await_status = coreapi::Status::Ok;
	REQUIRE(unanswered.code == 500);
	REQUIRE(unanswered.body.find("did not answer") != std::string::npos);
	REQUIRE(c.read(own) == kBouquetsUp);
	REQUIRE(box.channels.reinits == before_wait);
	// the upload and the directory it was checked in gone, whatever the cases
	// above left there on purpose
	REQUIRE(access((c.uploads + "/w.xml").c_str(), F_OK) != 0);
	const std::vector<std::string> left = c.others(c.uploads, NULL);
	for (size_t i = 0; i < left.size(); ++i)
		REQUIRE(left[i].compare(0, 12, "channelfile.") != 0);

	// In place, and the channel stack not there to read it: said so.
	box.channels.reload_channels_status = coreapi::Status::Internal;
	c.write(c.uploads + "/u.xml", ChannelFileFixture::kOwnBouquets);
	const Reply unread = putFile("ubouquets", "u.xml");
	REQUIRE(unread.code >= 500);
	REQUIRE(unread.body.find("channel-list-unavailable") != std::string::npos);
	REQUIRE(c.read(own) == ChannelFileFixture::kOwnBouquets);
}

namespace
{

bool lockedAtAwait = false;

void noteFilesLock()
{
	lockedAtAwait = coreapi::channels::internal::filesLockTaken();
}

} // namespace

TEST_CASE("a bouquet write holds the channel files until the box is through with its save", "[write][channelfiles]")
{
	ShippedRoutes shipped;
	BoxFixture box;
	giveBouquets(box.channels);
	REQUIRE_FALSE(coreapi::channels::internal::filesLockTaken());
	/* The box answers the save before it writes the services that go with it,
	   so a file put in place in between would be written over: the lock is
	   still held at the wait that follows the save, and given back after. */
	lockedAtAwait = false;
	box.channels.on_await = &noteFilesLock;
	REQUIRE(authedPost("/api/v1/bouquets", "{\"name\":\"Drittes\"}").code == 201);
	REQUIRE(box.channels.awaits > 0);
	REQUIRE(lockedAtAwait);
	REQUIRE_FALSE(coreapi::channels::internal::filesLockTaken());
	box.channels.on_await = NULL;
}

namespace
{

// The question the server asks installed for a case, and taken back after it.
struct PasswordOwedInstalled
{
	PasswordOwedInstalled() { httpd::setPasswordOwed(&login_bridge::passwordIsShipped); }
	~PasswordOwedInstalled() { httpd::setPasswordOwed(NULL); }
};

} // namespace

TEST_CASE("a box still opening to its shipped password asks for one of the owner's own first", "[write][firstlogin]")
{
	ShippedRoutes shipped;
	BoxFixture box;
	WebFile file("firstlogin", std::string(kWebFileHead) + "password_hash=" +
	             hashSecret(kShippedPassword, 2000) + "\n");
	REQUIRE(login_bridge::passwordIsShipped());
	// Asked only where the server installed the question: the router alone holds nothing back.
	REQUIRE(send(Get, "/api/v1/channels", "", AuthLevel::Write, "127.0.0.1").code == 200);
	PasswordOwedInstalled owed;

	// A caller that signed in reaches the routes that let it set its own, and nothing else.
	const Reply blocked = send(Post, "/api/v1/system/standby", "{\"on\":true}", AuthLevel::System, "127.0.0.1");
	REQUIRE(blocked.code == 403);
	REQUIRE(blocked.body.find("password-change-required") != std::string::npos);
	REQUIRE(send(Get, "/api/v1/channels", "", AuthLevel::Write, "127.0.0.1").code == 403);
	const Reply state = send(Get, "/api/v1/session", "", AuthLevel::System, "127.0.0.1");
	REQUIRE(state.code == 200);
	REQUIRE(parsed(state.body)["password_change_required"].asBool());
	// one reading without a password off the home network presented none, and is not asked
	REQUIRE(send(Get, "/api/v1/channels", "", AuthLevel::Read, "192.168.1.20").code == 200);
	REQUIRE_FALSE(parsed(send(Get, "/api/v1/session", "", AuthLevel::Read, "192.168.1.20").body)
	              ["password_change_required"].asBool());
	// and one signed in with the shipped password may still leave
	{
		const std::string token = openSession("root");
		REQUIRE_FALSE(token.empty());
		const Response out = dispatch(Post, "/api/v1/logout", "", "", "127.0.0.1",
		                              AuthLevel::System, std::string(), token);
		REQUIRE(out.code == 204);
		REQUIRE(csrfFor(token).empty());
	}

	// What a password of one's own has to be.
	struct Refused
	{
		const char *current;
		const char *password;
		int code;
		const char *says;
	};
	const Refused refused[] = {
		{ "wrong", "my own pass phrase", 403, "not the one in effect" },
		{ kShippedPassword, "short", 400, "fewer than 8" },
		{ kShippedPassword, "coolstream", 400, "shipped with" },
		{ kShippedPassword, kShippedPassword, 400, "shipped with" },
		// refused by the router already: a text member holds no control characters
		{ kShippedPassword, "with\ttab inside", 400, "bad-string" },
	};
	for (size_t i = 0; i < sizeof(refused) / sizeof(refused[0]); ++i)
	{
		INFO(refused[i].says);
		const Reply r = send(Put, "/api/v1/account/password",
		                     std::string("{\"current\":\"") + refused[i].current + "\",\"password\":\"" +
		                     refused[i].password + "\"}", AuthLevel::System, "127.0.0.1");
		REQUIRE(r.code == refused[i].code);
		REQUIRE(r.body.find(refused[i].says) != std::string::npos);
		REQUIRE(login_bridge::passwordIsShipped());
	}

	// Set: in effect at once, and the box answers everything again.
	const Reply set = send(Put, "/api/v1/account/password",
	                       std::string("{\"current\":\"") + kShippedPassword +
	                       "\",\"password\":\"my own pass phrase\"}", AuthLevel::System, "127.0.0.1");
	REQUIRE(set.code == 204);
	REQUIRE_FALSE(login_bridge::passwordIsShipped());
	REQUIRE(verifySecret("my own pass phrase", loginInEffect().password_hash));
	REQUIRE_FALSE(parsed(send(Get, "/api/v1/session", "", AuthLevel::System, "127.0.0.1").body)
	              ["password_change_required"].asBool());
	REQUIRE(send(Get, "/api/v1/channels", "", AuthLevel::Write, "127.0.0.1").code == 200);
	REQUIRE(ChannelFileFixture::read(file.path).find("password_hash=") != std::string::npos);
	// the same one again is no change
	const Reply same = send(Put, "/api/v1/account/password",
	                        "{\"current\":\"my own pass phrase\",\"password\":\"my own pass phrase\"}",
	                        AuthLevel::System, "127.0.0.1");
	REQUIRE(same.code == 400);
	REQUIRE(same.body.find("the one it is to replace") != std::string::npos);
}

TEST_CASE("the current password is guessed no faster here than at the login, and a file that cannot be written is said", "[write][firstlogin]")
{
	ShippedRoutes shipped;
	ConfigFixture cfg;
	WebFile file("pw-refused", std::string(kWebFileHead) + "password_hash=" +
	             hashSecret(kShippedPassword, 2000) + "\n");
	const std::string wrong = "{\"current\":\"wrong one\",\"password\":\"my own pass phrase\"}";
	const std::string right = std::string("{\"current\":\"") + kShippedPassword +
	                          "\",\"password\":\"my own pass phrase\"}";

	// The wrong ones count against the address as wrong logins do.
	setLoginClockForTest(1735689600);
	for (int i = 0; i < 3; ++i)
		REQUIRE(send(Put, "/api/v1/account/password", wrong, AuthLevel::System, "192.0.2.77").code == 403);
	const Reply waits = send(Put, "/api/v1/account/password", right, AuthLevel::System, "192.0.2.77");
	REQUIRE(waits.code == 429);
	REQUIRE(waits.body.find("too-many-attempts") != std::string::npos);
	REQUIRE(verifySecret(kShippedPassword, loginInEffect().password_hash));
	forgetLoginAttemptsForTest();

	// A file the password cannot be written to is a fault of this box, and the old one stays.
	REQUIRE(::unlink(file.path.c_str()) == 0);
	REQUIRE(::mkdir(file.path.c_str(), 0700) == 0);
	const Reply fault = send(Put, "/api/v1/account/password", right, AuthLevel::System, "127.0.0.1");
	REQUIRE(::rmdir(file.path.c_str()) == 0);
	REQUIRE(fault.code == 500);
	REQUIRE(fault.body.find("webserver-not-configured") != std::string::npos);
	REQUIRE(verifySecret(kShippedPassword, loginInEffect().password_hash));
}
