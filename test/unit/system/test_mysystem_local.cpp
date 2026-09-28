/*
 * test_mysystem_local.cpp - my_system's background call returns at once
 *
 * Copyright (C) 2026 Thilo Graf
 *
 * License: GPL-2.0-or-later
 *
 * With "&" my_system hands the command to a shell that starts it in the
 * background and ends at once, and the call waits for that shell alone:
 * it comes back while the command is still running.
 */

#include "support/catch.hpp"

#include <system/helpers.h>

#include <time.h>

namespace
{

long long nowMs()
{
	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	return (long long) ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}

} // anonymous namespace

TEST_CASE("a background call returns before its child is done", "[mysystemlocal]")
{
	const long long began = nowMs();
	int ret = my_system(4, "/bin/sh", "-c", "sleep 2", "&");
	const long long took = nowMs() - began;

	REQUIRE(ret == 0);
	// The child sleeps two seconds; a call that waited for it cannot get
	// under this line. Generous against a loaded host, far under the sleep.
	REQUIRE(took < 1000);
}
