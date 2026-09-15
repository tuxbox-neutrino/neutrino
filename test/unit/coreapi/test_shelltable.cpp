/*
 * test_shelltable.cpp - tests for the shell table
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
#include "coreapi/shelltable.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

#include <fcntl.h>
#include <signal.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <config.h>

using namespace coreapi;

namespace
{

typedef std::vector<std::pair<std::string, std::string> > ArgList;

bool writeScript(const std::string &path, const std::string &body)
{
	int fd = open(path.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0700);
	if (fd < 0)
		return false;
	ssize_t n = write(fd, body.data(), body.size());
	if (close(fd) != 0)
		return false;
	return n == (ssize_t) body.size();
}

std::string readAll(const std::string &path)
{
	FILE *f = fopen(path.c_str(), "r");
	if (f == NULL)
		return std::string();
	char buf[512];
	size_t got = fread(buf, 1, sizeof(buf) - 1, f);
	fclose(f);
	buf[got] = '\0';
	return std::string(buf);
}

// Read while this file is being loaded, because by the time any case runs the
// script root is whatever the case before it set.
const std::string g_initial_root = shell::scriptRoot();

// A directory of scripts, and the script root put back the way it was found.
struct Fixture
{
	std::string      dir;
	std::string      root_before;
	struct sigaction chld_before;
	bool             chld_saved;

	Fixture() : chld_saved(false)
	{
		char tmpl[] = "/tmp/coreapi_shelltable_XXXXXX";
		if (mkdtemp(tmpl) != NULL)
			dir = tmpl;
		root_before = shell::scriptRoot();

		// Another case may have left the process discarding what its children
		// exit with, and the runner refuses to start anything in that state.
		struct sigaction dfl;
		chld_saved = (sigaction(SIGCHLD, NULL, &chld_before) == 0);
		dfl.sa_handler = SIG_DFL;
		dfl.sa_flags = 0;
		sigemptyset(&dfl.sa_mask);
		sigaction(SIGCHLD, &dfl, NULL);
	}

	~Fixture()
	{
		shell::setScriptRoot(root_before);
		if (chld_saved)
			sigaction(SIGCHLD, &chld_before, NULL);
		if (!dir.empty())
			(void) system(("rm -rf " + dir).c_str());
	}

	std::string at(const std::string &name) const { return dir + "/" + name; }
};

} // namespace

TEST_CASE("the script root loses a separator it does not need", "[shelltable]")
{
	const std::string before = shell::scriptRoot();

	shell::setScriptRoot("/opt/scripts/");
	REQUIRE(shell::scriptRoot() == "/opt/scripts");

	shell::setScriptRoot("/opt/scripts///");
	REQUIRE(shell::scriptRoot() == "/opt/scripts");

	shell::setScriptRoot("/opt/scripts");
	REQUIRE(shell::scriptRoot() == "/opt/scripts");

	// The whole of the root is a separator, and dropping it would leave
	// nothing for the tail to be joined to.
	shell::setScriptRoot("/");
	REQUIRE(shell::scriptRoot() == "/");

	shell::setScriptRoot(before);
	REQUIRE(shell::scriptRoot() == before);
}

TEST_CASE("every entry names a tail the runner can be given", "[shelltable]")
{
	REQUIRE(shell::entryCount() > 0);
	for (size_t i = 0; i < shell::entryCount(); i++)
	{
		const shell::TableEntry *e = shell::entryAt(i);
		REQUIRE(e != NULL);
		REQUIRE(e->name != NULL);
		REQUIRE(e->name[0] != '\0');
		// The tail is joined to a root, so it has to begin with the separator
		// and carry no walk of its own.
		REQUIRE(e->script != NULL);
		REQUIRE(e->script[0] == '/');
		REQUIRE(strstr(e->script, "..") == NULL);
		REQUIRE(e->verb != NULL);
		REQUIRE(e->verb[0] != '\0');
		// A table that stopped early would leave these at zero, and the runner
		// refuses both rather than obeying them.
		REQUIRE(e->timeout_ms > 0);
		REQUIRE(e->max_output > 0);
		// Nothing in this table takes a value from a caller.
		REQUIRE(e->param_count == 0);
		REQUIRE(e->params == NULL);
	}
	REQUIRE(shell::entryAt(shell::entryCount()) == NULL);
}

TEST_CASE("no two entries carry the same name", "[shelltable]")
{
	for (size_t i = 0; i < shell::entryCount(); i++)
	{
		// find() answers with the first match, so a repeated name would make
		// one of the two unreachable.
		REQUIRE(shell::find(shell::entryAt(i)->name) == shell::entryAt(i));
	}
}

TEST_CASE("a name the table does not carry is not found", "[shelltable]")
{
	REQUIRE(shell::find("oscam.start") != NULL);
	REQUIRE(shell::find("oscam.explode") == NULL);
	REQUIRE(shell::find("") == NULL);

	ArgList none;
	Result<std::string> r = shell::runNamed("oscam.explode", none);
	REQUIRE_FALSE(r.ok());
	REQUIRE(r.error().status == Status::NotFound);
	// Its own code, so that a name the table does not carry is told apart from
	// a script the table names and the box has not got.
	REQUIRE(r.error().code == ErrorCode::NoSuchCommand);
}

TEST_CASE("the entry's tail is joined to the root that is set now", "[shelltable]")
{
	Fixture fx;
	REQUIRE_FALSE(fx.dir.empty());

	REQUIRE(writeScript(fx.at("Y_Camds.sh"),
			    "#!/bin/sh\nprintf '%s|%s\\n' \"$#\" \"$1\" > " + fx.at("argv") + "\n"
			    "printf 'done\\n'\n"));
	shell::setScriptRoot(fx.dir);

	ArgList none;
	Result<std::string> r = shell::runNamed("oscam.start", none);
	REQUIRE(r.ok());
	REQUIRE(r.value() == "done\n");
	// One argument, and it is the verb the table wrote: nothing a caller could
	// have written reaches the script.
	REQUIRE(readAll(fx.at("argv")) == "1|oscam_start\n");
}

TEST_CASE("a script the root does not hold is not found", "[shelltable]")
{
	Fixture fx;
	REQUIRE_FALSE(fx.dir.empty());
	shell::setScriptRoot(fx.dir);

	ArgList none;
	Result<std::string> r = shell::runNamed("oscam.start", none);
	REQUIRE_FALSE(r.ok());
	REQUIRE(r.error().status == Status::NotFound);
	// The runner's code and not the table's: the name was found, the script was
	// not.
	REQUIRE(r.error().code == ErrorCode::ScriptMissing);
}

TEST_CASE("a name the table does not declare is refused before the script runs", "[shelltable]")
{
	Fixture fx;
	REQUIRE_FALSE(fx.dir.empty());

	REQUIRE(writeScript(fx.at("Y_Camds.sh"),
			    "#!/bin/sh\nprintf 'ran\\n' > " + fx.at("marker") + "\n"));
	shell::setScriptRoot(fx.dir);

	ArgList extra;
	extra.push_back(std::make_pair("path", "/etc/passwd"));
	Result<std::string> r = shell::runNamed("oscam.start", extra);
	REQUIRE_FALSE(r.ok());
	REQUIRE(r.error().status == Status::InvalidArgument);
	REQUIRE(r.error().code == ErrorCode::NoSuchParameter);
	REQUIRE(access(fx.at("marker").c_str(), F_OK) != 0);
}

TEST_CASE("an entry's verb is the one its name promises", "[shelltable]")
{
	// A verb that named the wrong branch would reach the script's default arm,
	// which answers nothing and exits zero, so the run would look like a
	// success and the daemon would never move.
	for (size_t i = 0; i < shell::entryCount(); i++)
	{
		const shell::TableEntry *e = shell::entryAt(i);
		const std::string name(e->name);
		const std::string verb(e->verb);

		const size_t dot = name.find_last_of('.');
		REQUIRE(dot != std::string::npos);
		const std::string daemon = name.substr(0, dot);
		const std::string action = name.substr(dot + 1);

		const size_t bar = verb.find_last_of('_');
		REQUIRE(bar != std::string::npos);
		const std::string prefix = verb.substr(0, bar);
		const std::string suffix = verb.substr(bar + 1);

		// The scripts call a restart a reset, and nothing else is renamed.
		if (action == "restart")
			REQUIRE(suffix == "reset");
		else
			REQUIRE(suffix == action);

		// The one place the daemon and the branch are not called the same.
		if (daemon == "fritzcallmonitor")
			REQUIRE(prefix == "fcm");
		else
			REQUIRE(prefix == daemon);

		// Only the softcams are stopped and started again by one branch, so
		// having a restart is what says which of the two scripts a daemon
		// belongs to, and all of its verbs have to be in that one.
		const bool is_camd = (shell::find(daemon + ".restart") != NULL);
		REQUIRE(std::string(e->script) == (is_camd ? "/Y_Camds.sh" : "/Y_Plugins.sh"));
	}
}

TEST_CASE("the script root a fresh layer starts with is the one the image installs", "[shelltable]")
{
	REQUIRE(g_initial_root == PRIVATE_HTTPDDIR "/scripts");
}
