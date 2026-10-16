/*
 * test_getpidof.cpp - finding a running process by its name
 *
 * Copyright (C) 2026 Thilo Graf
 *
 * License: GPL-2.0-or-later
 *
 * getpidof() walks /proc. It is asked once per request by the display list
 * of the REST interface, so a walk that leaves /proc open costs the program
 * one descriptor per request until it can open nothing more.
 */

#include "support/catch.hpp"

#include <cstdio>
#include <string>

#include <dirent.h>
#include <unistd.h>

#include <system/helpers.h>

namespace {

size_t openDescriptors()
{
	size_t n = 0;
	DIR *d = opendir("/proc/self/fd");
	REQUIRE(d != NULL);
	while (readdir(d) != NULL)
		n++;
	closedir(d);
	return n;
}

// this process's name as its status file gives it, which is what is matched
std::string ownName()
{
	char name[64] = "";
	FILE *f = fopen("/proc/self/status", "r");
	REQUIRE(f != NULL);
	REQUIRE(fscanf(f, "%*s %63s", name) == 1);
	fclose(f);
	return name;
}

} // namespace

TEST_CASE("getpidof finds a running process and gives /proc back every time", "[system]")
{
	const std::string self = ownName();
	REQUIRE_FALSE(self.empty());

	const size_t before = openDescriptors();
	for (int i = 0; i < 200; i++)
		REQUIRE(getpidof(self.c_str()) > 0);
	CHECK(openDescriptors() == before);

	CHECK(getpidof("no-such-process") == 0);
	CHECK(openDescriptors() == before);
}
