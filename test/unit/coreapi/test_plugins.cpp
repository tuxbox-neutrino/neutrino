/*
 * test_plugins.cpp - tests for plugins and scripts
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

#include "coreapi/base/deps.h"
#include "coreapi/base/errors.h"
#include "coreapi/plugins.h"
#include "coreapi/base/result.h"
#include "coreapi/base/types.h"

#include "httpd/endpoint.h"
#include "httpd/endpoints.h"
#include "httpd/http.h"
#include "httpd/router.h"

#include <neutrinoMessages.h>

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include <fcntl.h>
#include <signal.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

using namespace coreapi;

namespace
{

bool writeFile(const std::string &path, const std::string &body, mode_t mode)
{
	int fd = open(path.c_str(), O_WRONLY | O_CREAT | O_TRUNC, mode);
	if (fd < 0)
		return false;
	ssize_t n = write(fd, body.data(), body.size());
	if (close(fd) != 0)
		return false;
	return n == (ssize_t) body.size();
}

bool exists(const std::string &path)
{
	struct stat st;
	return stat(path.c_str(), &st) == 0;
}

// A zombie still answers kill(pid, 0), so the state field is what separates a
// process that is gone from one that is merely waiting to be collected.
bool stillRunning(pid_t pid)
{
	char path[64];
	std::snprintf(path, sizeof(path), "/proc/%d/stat", (int) pid);
	FILE *f = std::fopen(path, "r");
	if (f == NULL)
		return false;
	char buf[512];
	size_t n = std::fread(buf, 1, sizeof(buf) - 1, f);
	std::fclose(f);
	buf[n] = '\0';
	const char *end = std::strrchr(buf, ')');
	if (end == NULL || end[1] == '\0' || end[2] == '\0')
		return false;
	return end[2] != 'Z' && end[2] != 'X';
}

/* A root of this case's own, beside a directory that is not the root, and the script
   root pointed at the first for as long as the case runs.

   The root has to be a real directory with real scripts in it: what the layer under
   this does is start a program, and a case that stopped short of that would say nothing
   about the one thing this endpoint is. Every script the fixture writes touches the
   same file first, so a case meaning that nothing ran reads one name whatever it asked
   for. */
struct Fixture
{
	bool        ready;
	std::string base;
	std::string dir;
	std::string outside;
	std::string marker;
	std::string previous_root;

	Fixture() : ready(false)
	{
		previous_root = plugins::scriptRoot();

		char tmpl[] = "/tmp/coreapi_plugins_XXXXXX";
		if (mkdtemp(tmpl) == NULL)
			return;
		base = tmpl;
		dir = base + "/root";
		outside = base + "/elsewhere";
		marker = base + "/ran";
		if (mkdir(dir.c_str(), 0700) != 0)
			return;
		if (mkdir(outside.c_str(), 0700) != 0)
			return;
		plugins::setScriptRoot(dir);
		ready = true;
	}

	~Fixture()
	{
		plugins::setScriptRoot(previous_root);
		if (base.empty())
			return;
		// Named one at a time rather than swept, so nothing here can be asked
		// to remove a tree it did not make.
		remove(dir + "/hello.sh");
		remove(dir + "/echo.sh");
		remove(dir + "/forever.sh");
		remove(dir + "/alias.sh");
		remove(dir + "/esc.sh");
		remove(dir + "/pwned");
		remove(outside + "/target.sh");
		remove(outside + "/reached");
		remove(marker);
		remove(base + "/pid");
		rmdir(dir.c_str());
		rmdir(outside.c_str());
		rmdir(base.c_str());
	}

	void remove(const std::string &path) { unlink(path.c_str()); }

	// The body with a line in front of it that says the script ran at all.
	bool writeScript(const std::string &name, const std::string &body)
	{
		return writeFile(dir + "/" + name,
				 "#!/bin/sh\n: > '" + marker + "'\n" + body, 0700);
	}

	bool writeOutside(const std::string &name, const std::string &body)
	{
		return writeFile(outside + "/" + name,
				 "#!/bin/sh\n: > '" + outside + "/reached'\n" + body, 0700);
	}

	bool ranNothing() const { return !exists(marker); }

	private:
		Fixture(const Fixture &);
		Fixture &operator=(const Fixture &);
};

} // namespace

TEST_CASE("starting a plugin sends its name to the loop", "[plugins]")
{
	/* The number and the body, not the status: this is posted and forgotten,
	   so a case that checked an ok answer would be green whatever went out. */
	InstalledDependencies deps;
	REQUIRE(plugins::start("tuxtxt").ok());
	REQUIRE(deps.events.sent.size() == 1);
	REQUIRE(deps.events.sent[0].id == (unsigned) NeutrinoMessages::EVT_START_PLUGIN);
	// With the byte the loop reads the name up to.
	REQUIRE(deps.events.sent[0].body == std::string("tuxtxt\0", 7));
}

TEST_CASE("starting nothing names nothing and goes nowhere", "[plugins]")
{
	InstalledDependencies deps;
	const Result<void> r = plugins::start("");
	REQUIRE_FALSE(r.ok());
	REQUIRE(r.error().status == Status::InvalidArgument);
	REQUIRE(deps.events.sent.empty());
}

TEST_CASE("reloading is a message and not something done here", "[plugins]")
{
	/* The whole of the change: the list the screens draw from is rebuilt by
	   the thread that draws them, so what leaves here is a number and the
	   answer says the box was asked. */
	InstalledDependencies deps;
	REQUIRE(plugins::reload().ok());
	REQUIRE(deps.events.sent.size() == 1);
	REQUIRE(deps.events.sent[0].id == (unsigned) NeutrinoMessages::EVT_RELOAD_PLUGINS);
	REQUIRE(deps.events.sent[0].body.empty());
	// Nothing was read out of whatever holds the plugins, which is what a
	// reload done on this thread would have had to do.
	REQUIRE(deps.plugins.reads == 0);
}

TEST_CASE("the command that reloads travels under the number the loop reads", "[plugins]")
{
	// Not a transcription: the number comes from the loop's own header, and a
	// number written out here would be somebody typing one thing twice.
	REQUIRE(eventNumber(BoxEvent::ReloadPlugins)
		== (unsigned) NeutrinoMessages::EVT_RELOAD_PLUGINS);
	REQUIRE(eventNumber(BoxEvent::ReloadPlugins) != eventNumber(BoxEvent::StartPlugin));
}

TEST_CASE("the plugins the box carries come back in the order it keeps them", "[plugins]")
{
	InstalledDependencies deps;
	deps.plugins.add("tuxtxt", "Teletext");
	deps.plugins.add("rtprobe", "Roundtrip Probe");

	const Result<PluginList> got = plugins::list();
	REQUIRE(got.ok());
	REQUIRE(got.value().size() == 2);
	// The name the box starts it by and the name a person reads are two
	// different things, and both arrive.
	REQUIRE(got.value()[0].name == "tuxtxt");
	REQUIRE(got.value()[0].title == "Teletext");
	REQUIRE(got.value()[1].name == "rtprobe");
	REQUIRE(deps.plugins.reads == 1);
}

TEST_CASE("a box with nothing holding a plugin list says so", "[plugins]")
{
	// Not an empty list: a box carrying no plugins and a build with no list to
	// answer out of are two answers, and only the second is worth reporting.
	InstalledDependencies deps;
	deps.plugins.list_status = Status::NotSupported;

	const Result<PluginList> got = plugins::list();
	REQUIRE_FALSE(got.ok());
	REQUIRE(got.error().status == Status::NotSupported);
	REQUIRE(got.error().code == ErrorCode::PluginListUnavailable);
}

namespace
{

/* A plugin with a real file behind it, because what the write under test does
   is rewrite that file: a case with a fake path would say nothing about the
   one thing this is. */
struct CfgFixture
{
	bool        ready;
	std::string base;
	std::string cfg;

	CfgFixture() : ready(false)
	{
		char tmpl[] = "/tmp/coreapi_plugincfg_XXXXXX";
		if (mkdtemp(tmpl) == NULL)
			return;
		base = tmpl;
		cfg = base + "/tuxtxt.cfg";
		ready = true;
	}

	~CfgFixture()
	{
		if (base.empty())
			return;
		unlink(cfg.c_str());
		rmdir(base.c_str());
	}

	bool write(const std::string &body) { return writeFile(cfg, body, 0600); }

	std::string read() const
	{
		FILE *f = std::fopen(cfg.c_str(), "r");
		if (f == NULL)
			return std::string();
		std::string out;
		char buf[256];
		size_t n;
		while ((n = std::fread(buf, 1, sizeof(buf), f)) > 0)
			out.append(buf, n);
		std::fclose(f);
		return out;
	}

	private:
		CfgFixture(const CfgFixture &);
		CfgFixture &operator=(const CfgFixture &);
};

// One entry that says where it came from, which the plain adder does not.
void addWithCfg(FakePluginSource &src, const std::string &name, const std::string &cfg)
{
	src.add(name, name);
	src.plugins.back().config = cfg;
}

} // namespace

TEST_CASE("hiding a plugin writes the one line and carries the rest across", "[plugins]")
{
	InstalledDependencies deps;
	CfgFixture fx;
	REQUIRE(fx.ready);
	REQUIRE(fx.write("name=Teletext\npluginversion=1\nhide=0\ntype=3\n"));
	addWithCfg(deps.plugins, "tuxtxt", fx.cfg);

	REQUIRE(plugins::setHidden("tuxtxt", true).ok());
	REQUIRE(fx.read() == "name=Teletext\npluginversion=1\nhide=1\ntype=3\n");

	REQUIRE(plugins::setHidden("tuxtxt", false).ok());
	REQUIRE(fx.read() == "name=Teletext\npluginversion=1\nhide=0\ntype=3\n");
}

TEST_CASE("a file that never said hide gets the line and keeps its last one", "[plugins]")
{
	InstalledDependencies deps;
	CfgFixture fx;
	REQUIRE(fx.ready);
	// No line that decides, and no break behind the last line either, which is
	// what the surface this replaces had to patch up before it could append.
	REQUIRE(fx.write("name=Teletext\ntype=3"));
	addWithCfg(deps.plugins, "tuxtxt", fx.cfg);

	REQUIRE(plugins::setHidden("tuxtxt", true).ok());
	REQUIRE(fx.read() == "name=Teletext\ntype=3\nhide=1\n");
}

TEST_CASE("a file naming the flag twice is left with the one that decides", "[plugins]")
{
	/* The box reads the whole file and is left holding the last of them, so
	   the last is the one a write has to reach. Rewriting the first would
	   leave the box reading the value nobody changed. */
	InstalledDependencies deps;
	CfgFixture fx;
	REQUIRE(fx.ready);
	REQUIRE(fx.write("hide=0\nname=Teletext\nhide=0\n"));
	addWithCfg(deps.plugins, "tuxtxt", fx.cfg);

	REQUIRE(plugins::setHidden("tuxtxt", true).ok());
	REQUIRE(fx.read() == "hide=0\nname=Teletext\nhide=1\n");
}

TEST_CASE("a plugin nobody has and one with no file behind it are two refusals", "[plugins]")
{
	InstalledDependencies deps;
	CfgFixture fx;
	REQUIRE(fx.ready);
	addWithCfg(deps.plugins, "tuxtxt", fx.cfg);
	// One the box holds and whose source could not say where it came from.
	deps.plugins.add("rtprobe", "Roundtrip Probe");

	const Result<void> unknown = plugins::setHidden("nosuchplugin", true);
	REQUIRE_FALSE(unknown.ok());
	REQUIRE(unknown.error().status == Status::NotFound);
	REQUIRE(unknown.error().code == ErrorCode::NoSuchName);

	const Result<void> unplaced = plugins::setHidden("rtprobe", true);
	REQUIRE_FALSE(unplaced.ok());
	REQUIRE(unplaced.error().status == Status::NotSupported);
	REQUIRE(unplaced.error().code == ErrorCode::PluginNotChanged);

	const Result<void> unnamed = plugins::setHidden("", true);
	REQUIRE_FALSE(unnamed.ok());
	REQUIRE(unnamed.error().status == Status::InvalidArgument);
	REQUIRE(unnamed.error().code == ErrorCode::BadName);
}

TEST_CASE("a file that is not there is a refusal and not a file that appears", "[plugins]")
{
	InstalledDependencies deps;
	CfgFixture fx;
	REQUIRE(fx.ready);
	addWithCfg(deps.plugins, "tuxtxt", fx.cfg);

	const Result<void> r = plugins::setHidden("tuxtxt", true);
	REQUIRE_FALSE(r.ok());
	REQUIRE(r.error().status == Status::Internal);
	REQUIRE(r.error().code == ErrorCode::PluginNotChanged);
	REQUIRE_FALSE(exists(fx.cfg));
}

TEST_CASE("a script name with a separator is refused and nothing runs", "[plugins]")
{
	Fixture fx;
	REQUIRE(fx.ready);
	REQUIRE(fx.writeScript("hello.sh", "printf 'here'\n"));

	const Result<std::string> up = plugins::run("../../bin/sh", "");
	REQUIRE_FALSE(up.ok());
	REQUIRE(up.error().status == Status::InvalidArgument);
	REQUIRE(up.error().code == ErrorCode::BadName);

	// The same name written as an absolute one, which is the other way a
	// caller writes a path where a name is asked for.
	const Result<std::string> absolute = plugins::run("/bin/sh", "");
	REQUIRE_FALSE(absolute.ok());
	REQUIRE(absolute.error().code == ErrorCode::BadName);

	REQUIRE(fx.ranNothing());
}

TEST_CASE("a script name that is only dots is refused", "[plugins]")
{
	Fixture fx;
	REQUIRE(fx.ready);
	REQUIRE(fx.writeScript("hello.sh", "printf 'here'\n"));

	const Result<std::string> up = plugins::run("..", "");
	REQUIRE_FALSE(up.ok());
	REQUIRE(up.error().code == ErrorCode::BadName);

	const Result<std::string> here = plugins::run(".", "");
	REQUIRE_FALSE(here.ok());
	REQUIRE(here.error().code == ErrorCode::BadName);

	REQUIRE(fx.ranNothing());
}

TEST_CASE("a name that would be read as an option is refused", "[plugins]")
{
	Fixture fx;
	REQUIRE(fx.ready);
	REQUIRE(fx.writeScript("hello.sh", "printf 'here'\n"));

	const Result<std::string> r = plugins::run("-rf", "");
	REQUIRE_FALSE(r.ok());
	REQUIRE(r.error().status == Status::InvalidArgument);
	REQUIRE(r.error().code == ErrorCode::BadName);
	REQUIRE(fx.ranNothing());
}

TEST_CASE("a link out of the root reaches nothing, one inside it still runs", "[plugins]")
{
	/* The one way out of the root a name with no separator in it still has.
	   The check is worth what the comment at the call says it is worth and no
	   more, but a link is what an attacker with a foothold in the root writes,
	   and it is refused. */
	Fixture fx;
	REQUIRE(fx.ready);
	REQUIRE(fx.writeScript("hello.sh", "printf 'inside'\n"));
	REQUIRE(fx.writeOutside("target.sh", "printf 'outside'\n"));
	REQUIRE(symlink((fx.outside + "/target.sh").c_str(), (fx.dir + "/esc.sh").c_str()) == 0);
	REQUIRE(symlink("hello.sh", (fx.dir + "/alias.sh").c_str()) == 0);

	const Result<std::string> out = plugins::run("esc", "");
	REQUIRE_FALSE(out.ok());
	REQUIRE(out.error().status == Status::InvalidArgument);
	REQUIRE(out.error().code == ErrorCode::OutsideRoots);
	REQUIRE_FALSE(exists(fx.outside + "/reached"));
	REQUIRE(fx.ranNothing());

	// A link that stays inside names something the root holds, so nothing
	// above refuses links as such.
	const Result<std::string> in = plugins::run("alias", "");
	REQUIRE(in.ok());
	REQUIRE(in.value() == "inside");
}

TEST_CASE("a script in the root runs and its output comes back", "[plugins]")
{
	Fixture fx;
	REQUIRE(fx.ready);
	REQUIRE(fx.writeScript("hello.sh", "printf '%s|%s' \"$1\" \"$2\"\n"));

	const Result<std::string> r = plugins::run("hello", "eins zwei");
	REQUIRE(r.ok());
	// In the order they were written, which is the only order a caller can
	// have meant.
	REQUIRE(r.value() == "eins|zwei");
	REQUIRE_FALSE(fx.ranNothing());
}

TEST_CASE("an argument does not become syntax", "[plugins]")
{
	// The one thing a pipe to a shell could never promise. No shell is
	// involved, so a semicolon reaches the script as a character.
	Fixture fx;
	REQUIRE(fx.ready);
	REQUIRE(fx.writeScript("echo.sh", "printf '%s' \"$1\"\n"));

	const Result<std::string> r = plugins::run("echo", "a;touch~pwned");
	REQUIRE(r.ok());
	REQUIRE(r.value() == "a;touch~pwned");
	REQUIRE_FALSE(exists(fx.dir + "/pwned"));
}

TEST_CASE("an argument that would be read as an option is refused", "[plugins]")
{
	Fixture fx;
	REQUIRE(fx.ready);
	REQUIRE(fx.writeScript("echo.sh", "printf '%s' \"$1\"\n"));

	const Result<std::string> r = plugins::run("echo", "-rf");
	REQUIRE_FALSE(r.ok());
	REQUIRE(r.error().status == Status::InvalidArgument);
	REQUIRE(r.error().code == ErrorCode::LeadingDash);
	REQUIRE(fx.ranNothing());
}

TEST_CASE("a script that never ends is cut, and its group with it", "[plugins]")
{
	Fixture fx;
	REQUIRE(fx.ready);
	REQUIRE(fx.writeScript("forever.sh", "echo $$ > '" + fx.base + "/pid'\nsleep 300\n"));

	const Result<std::string> r = plugins::run("forever", "");
	REQUIRE_FALSE(r.ok());
	REQUIRE(r.error().status == Status::Busy);
	REQUIRE(r.error().code == ErrorCode::Timeout);

	FILE *f = std::fopen((fx.base + "/pid").c_str(), "r");
	REQUIRE(f != NULL);
	int child = 0;
	const int read_back = std::fscanf(f, "%d", &child);
	std::fclose(f);
	REQUIRE(read_back == 1);
	REQUIRE(child > 0);
	// ESRCH rather than a state check: a zombie would still be signalable, so
	// this is what says the script was collected and not merely stopped.
	REQUIRE(kill((pid_t) child, 0) == -1);
	REQUIRE(errno == ESRCH);
	REQUIRE_FALSE(stillRunning((pid_t) child));
}

TEST_CASE("a name nobody has is not found, and the root is not the filesystem", "[plugins]")
{
	/* The shape of the fault this replaces: an empty root turned the search
	   into a walk of the filesystem root, and anything an attacker could put
	   there became reachable. An empty root here is refused outright, and so
	   are the two other roots that are no root. */
	Fixture fx;
	REQUIRE(fx.ready);

	plugins::setScriptRoot("");
	const Result<std::string> none = plugins::run("sh", "");
	REQUIRE_FALSE(none.ok());
	REQUIRE(none.error().status == Status::Internal);
	// The code and not only the status: a root that is not there makes every
	// name fail, so a case reading no more than "not ok" would be green with
	// the check taken out.
	REQUIRE(none.error().code == ErrorCode::NoRoot);

	plugins::setScriptRoot("/");
	const Result<std::string> slash = plugins::run("sh", "");
	REQUIRE_FALSE(slash.ok());
	REQUIRE(slash.error().code == ErrorCode::NoRoot);

	plugins::setScriptRoot("relative/place");
	const Result<std::string> relative = plugins::run("sh", "");
	REQUIRE_FALSE(relative.ok());
	REQUIRE(relative.error().code == ErrorCode::NoRoot);

	// And a root that is one, with no script of that name in it.
	plugins::setScriptRoot(fx.dir);
	const Result<std::string> missing = plugins::run("sh", "");
	REQUIRE_FALSE(missing.ok());
	REQUIRE(missing.error().status == Status::NotFound);
	REQUIRE(missing.error().code == ErrorCode::ScriptMissing);
}

TEST_CASE("the root a name is joined to is the one that was set", "[plugins]")
{
	Fixture fx;
	REQUIRE(fx.ready);
	// A trailing separator is dropped, so the joined name never carries a
	// doubled one and the two spellings are one root.
	plugins::setScriptRoot(fx.dir + "/");
	REQUIRE(plugins::scriptRoot() == fx.dir);
}

/* From here down the same three names are asked for over the router, against
   the same fakes, because what the two layers promise apart is not what a
   request meets: which level a route asks for, what a path segment is, and
   what comes back are all settled between them. */

namespace
{

/* Cleared going in as well as coming out, so a case elsewhere that failed and
   unwound past its own restore cannot decide what these read. */
struct ShippedRoutes
{
	ShippedRoutes() { httpd::setRoutesForTest(NULL); }
	~ShippedRoutes() { httpd::setRoutesForTest(NULL); }

	private:
		ShippedRoutes(const ShippedRoutes &);
		ShippedRoutes &operator=(const ShippedRoutes &);
};

httpd::Response ask(httpd::Method m, const std::string &path, const std::string &body,
		    httpd::AuthLevel granted, const std::string &peer)
{
	return httpd::dispatch(m, path, std::string(), body, peer, granted);
}

httpd::Response asOwner(httpd::Method m, const std::string &path, const std::string &body)
{
	return ask(m, path, body, httpd::AuthLevel::System, "127.0.0.1");
}

// What a caller on the box's own network is granted without presenting
// anything, which is a read and nothing above it.
httpd::Response asNeighbour(httpd::Method m, const std::string &path, const std::string &body)
{
	return ask(m, path, body, httpd::AuthLevel::Read, "192.168.1.9");
}

httpd::Response nothing(const httpd::Request &)
{
	return httpd::accepted();
}

} // namespace

TEST_CASE("a caller on the box's own network reaches none of the three", "[plugins]")
{
	/* A caller on that network is granted a read and is asked for no
	   credential, so a route that changed something at that level would be one
	   any page a browser on that network visits could reach in that browser's
	   name. */
	ShippedRoutes shipped;
	InstalledDependencies deps;
	Fixture fx;
	REQUIRE(fx.ready);
	REQUIRE(fx.writeScript("hello.sh", "printf 'here'\n"));

	REQUIRE(asNeighbour(httpd::Post, "/api/v1/plugins/tuxtxt/start", "{}").code == 403);
	REQUIRE(asNeighbour(httpd::Post, "/api/v1/plugins/reload", "{}").code == 403);
	REQUIRE(asNeighbour(httpd::Post, "/api/v1/scripts/hello", "{}").code == 403);

	// Nothing reached the box: a refusal that sent the message first would
	// answer 403 and start the plugin anyway.
	REQUIRE(deps.events.sent.empty());
	REQUIRE(fx.ranNothing());

	// And the listing beside them is a read, which is what that level is for.
	REQUIRE(asNeighbour(httpd::Get, "/api/v1/plugins", "").code == 200);
}

TEST_CASE("writing the flag is a write and it says nothing to the loop", "[plugins]")
{
	ShippedRoutes shipped;
	InstalledDependencies deps;
	CfgFixture fx;
	REQUIRE(fx.ready);
	REQUIRE(fx.write("name=Teletext\nhide=0\n"));
	addWithCfg(deps.plugins, "tuxtxt", fx.cfg);

	REQUIRE(asNeighbour(httpd::Put, "/api/v1/plugins/tuxtxt/hidden",
			    "{\"hidden\":true}").code == 403);
	REQUIRE(fx.read() == "name=Teletext\nhide=0\n");

	const httpd::Response r = ask(httpd::Put, "/api/v1/plugins/tuxtxt/hidden",
				      "{\"hidden\":true}", httpd::AuthLevel::Write, "127.0.0.1");
	REQUIRE(r.code == 204);
	REQUIRE(fx.read() == "name=Teletext\nhide=1\n");
	/* Nothing was posted. What draws the menus reads the list it already
	   holds, and asking it to read the directories again is a route of its
	   own; a message sent from here would be this route doing a second thing
	   nobody asked it for. */
	REQUIRE(deps.events.sent.empty());

	// And the body is not optional: a request that names neither direction is
	// refused before anything is written.
	const httpd::Response bare = ask(httpd::Put, "/api/v1/plugins/tuxtxt/hidden",
					 "{}", httpd::AuthLevel::Write, "127.0.0.1");
	REQUIRE(bare.code == 400);
	REQUIRE(fx.read() == "name=Teletext\nhide=1\n");
}

TEST_CASE("a write drives the plugins and does not run a script", "[plugins]")
{
	// The line that carries System, and the only guard on it: a table with it
	// at Write satisfies every rule the server checks before it starts.
	ShippedRoutes shipped;
	InstalledDependencies deps;
	Fixture fx;
	REQUIRE(fx.ready);
	REQUIRE(fx.writeScript("hello.sh", "printf 'here'\n"));

	REQUIRE(ask(httpd::Post, "/api/v1/scripts/hello", "{}",
		    httpd::AuthLevel::Write, "127.0.0.1").code == 403);
	REQUIRE(fx.ranNothing());

	REQUIRE(ask(httpd::Post, "/api/v1/plugins/tuxtxt/start", "{}",
		    httpd::AuthLevel::Write, "127.0.0.1").code == 202);
	REQUIRE(ask(httpd::Post, "/api/v1/plugins/reload", "{}",
		    httpd::AuthLevel::Write, "127.0.0.1").code == 202);
	REQUIRE(deps.events.sent.size() == 2);

	// And the credential that owns the box does reach the script.
	REQUIRE(asOwner(httpd::Post, "/api/v1/scripts/hello", "{}").code == 200);
	REQUIRE_FALSE(fx.ranNothing());
}

TEST_CASE("a table that changes something at a read is one this server will not start on",
          "[plugins]")
{
	/* The other half of the level above, and the half no request can show: a
	   route entered below a write is refused where the tables are checked,
	   which is before anything answers at all. */
	const httpd::Param kName[] = {
		HTTPD_SEGMENT_TEXT("name", "the plugin", 64),
	};
	const httpd::Endpoint too_low[] = {
		{ httpd::Post, "/api/v1/plugins/{name}/start", httpd::AuthLevel::Read,
		  "asks the box to start one plugin", kName, 1, NULL, &nothing, false },
	};
	const httpd::RouteTable low = { HTTPD_TABLE_N("low", too_low, 1) };
	std::string why;
	REQUIRE_FALSE(httpd::tableIsSane(low, &why));
	REQUIRE(why.find("no more than a read") != std::string::npos);

	// The same route at a write is one it will start on, so what the check
	// refuses is the level and not the route.
	const httpd::Endpoint high[] = {
		{ httpd::Post, "/api/v1/plugins/{name}/start", httpd::AuthLevel::Write,
		  "asks the box to start one plugin", kName, 1, NULL, &nothing, false },
	};
	const httpd::RouteTable ok_table = { HTTPD_TABLE_N("ok", high, 1) };
	REQUIRE(httpd::tableIsSane(ok_table, &why));
}

TEST_CASE("an encoded separator in a name stays one name", "[plugins]")
{
	/* A path is split before anything is decoded, so an encoded separator is
	   one segment and not two. What that segment holds then reaches the layer
	   below whole, and that is where a name with a separator in it is refused.
	   Split the other way round, the request would reach a route nobody
	   declared or none at all. */
	ShippedRoutes shipped;
	InstalledDependencies deps;
	Fixture fx;
	REQUIRE(fx.ready);

	const httpd::Response refused =
		asOwner(httpd::Post, "/api/v1/scripts/..%2F..%2Fbin%2Fsh", "{}");
	REQUIRE(refused.code == 400);
	REQUIRE(refused.body.find("bad-name") != std::string::npos);
	REQUIRE(fx.ranNothing());

	// The start route reads the same segment the same way, and what travels is
	// the one name the caller wrote rather than the first piece of it.
	REQUIRE(asOwner(httpd::Post, "/api/v1/plugins/one%2Ftwo/start", "{}").code == 202);
	REQUIRE(deps.events.sent.size() == 1);
	REQUIRE(deps.events.sent[0].body == std::string("one/two\0", 8));
}

TEST_CASE("the plugins of the box are answered as a document", "[plugins]")
{
	ShippedRoutes shipped;
	InstalledDependencies deps;
	deps.plugins.add("tuxtxt", "Teletext");

	const httpd::Response r = asOwner(httpd::Get, "/api/v1/plugins", "");
	REQUIRE(r.code == 200);
	REQUIRE(r.body.find("\"name\":\"tuxtxt\"") != std::string::npos);
	REQUIRE(r.body.find("\"title\":\"Teletext\"") != std::string::npos);
	// The sort as a word and not as the number the box sorts by.
	REQUIRE(r.body.find("\"kind\":\"script\"") != std::string::npos);
	REQUIRE(deps.plugins.reads == 1);
}

TEST_CASE("what a script wrote is what comes back", "[plugins]")
{
	ShippedRoutes shipped;
	InstalledDependencies deps;
	Fixture fx;
	REQUIRE(fx.ready);
	REQUIRE(fx.writeScript("hello.sh", "printf '%s|%s' \"$1\" \"$2\"\n"));

	const httpd::Response r =
		asOwner(httpd::Post, "/api/v1/scripts/hello", "{\"args\":\"eins zwei\"}");
	REQUIRE(r.code == 200);
	REQUIRE(r.body.find("\"output\":\"eins|zwei\"") != std::string::npos);

	// A name the directory holds no script for is a miss and not a fault.
	const httpd::Response missing = asOwner(httpd::Post, "/api/v1/scripts/nobody", "{}");
	REQUIRE(missing.code == 404);
	REQUIRE(missing.body.find("script-missing") != std::string::npos);
}
