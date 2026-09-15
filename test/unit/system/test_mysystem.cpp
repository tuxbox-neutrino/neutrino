/*
 * test_mysystem.cpp - tests for running a shell command
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

#include <cerrno>
#include <cstdio>
#include <cstring>
#include <string>

#include <fcntl.h>
#include <limits.h>
#include <signal.h>
#include <stdlib.h>
#include <unistd.h>

// Declared rather than included: the header behind it pulls in a network
// library this binary has no other use for, and only this one function is
// under test.
int my_system(int argc, const char *arg, ...);

namespace
{

// Writes what it was handed, one argument per element, so that an argument
// list cut short shows up as a shorter file and not only as a different
// return.
const char kEcho[] =
	"#!/bin/sh\n"
	"for a in \"$@\" ; do printf '%s|' \"$a\" ; done > ARGFILE\n"
	"exit 3\n";

struct Spawned
{
	bool        ready;
	std::string dir;
	std::string script;
	std::string argfile;

	Spawned() : ready(false)
	{
		char tmpl[] = "/tmp/coreapi_mysystem_XXXXXX";
		if (mkdtemp(tmpl) == NULL)
			return;
		dir = tmpl;
		script = dir + "/echoargs.sh";
		argfile = dir + "/args";

		std::string body(kEcho);
		size_t at = body.find("ARGFILE");
		if (at == std::string::npos)
			return;
		body.replace(at, strlen("ARGFILE"), argfile);

		int fd = open(script.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0700);
		if (fd < 0)
			return;
		ssize_t n = write(fd, body.data(), body.size());
		if (close(fd) != 0 || n != (ssize_t)body.size())
			return;
		ready = true;
	}

	~Spawned()
	{
		if (dir.empty())
			return;
		unlink(script.c_str());
		unlink(argfile.c_str());
		rmdir(dir.c_str());
	}
};

const Spawned &sp()
{
	static Spawned s;
	return s;
}

std::string readArgs()
{
	FILE *f = fopen(sp().argfile.c_str(), "r");
	if (f == NULL)
		return "";
	char buf[512];
	size_t n = fread(buf, 1, sizeof(buf) - 1, f);
	fclose(f);
	buf[n] = '\0';
	return std::string(buf);
}

bool childrenAreDiscarded()
{
	struct sigaction now;
	if (sigaction(SIGCHLD, NULL, &now) != 0)
		return false;
	return now.sa_handler == SIG_IGN;
}

} // namespace

TEST_CASE("an argument that only carries an ampersand is handed on whole", "[mysystem]")
{
	REQUIRE(sp().ready);
	unlink(sp().argfile.c_str());
	REQUIRE_FALSE(childrenAreDiscarded());

	int ret = my_system(3, sp().script.c_str(), "Film & Serie.iso", "second");

	// Before this was one argument that is the ampersand rather than one that
	// contains it, this call cut the list at the file name, told the kernel to
	// discard every later child of this process, and then read a status it had
	// no way to collect.
	REQUIRE(readArgs() == "Film & Serie.iso|second|");
	REQUIRE(ret == 3);
	REQUIRE_FALSE(childrenAreDiscarded());
}

TEST_CASE("an argument that is the ampersand still asks for the background behaviour", "[mysystem]")
{
	REQUIRE(sp().ready);
	unlink(sp().argfile.c_str());

	struct sigaction previous;
	bool saved = (sigaction(SIGCHLD, NULL, &previous) == 0);

	int ret = my_system(3, sp().script.c_str(), "&", "second");
	bool discarded = childrenAreDiscarded();
	std::string args = readArgs();

	// Put back before anything can fail: every later case in this binary would
	// otherwise run in the state this file exists to keep out of the suite.
	if (saved)
		REQUIRE(sigaction(SIGCHLD, &previous, NULL) == 0);

	REQUIRE(saved);
	// The ampersand and everything behind it is dropped, which is what asking
	// for it has always meant.
	REQUIRE(args == "");
	// Collected by the kernel rather than by the call, so there is no status.
	REQUIRE(ret == 0);
	REQUIRE(discarded);
	REQUIRE_FALSE(childrenAreDiscarded());
}
