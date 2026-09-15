/*
 * narrow-probe.cpp - probe checking that a narrow build still links
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

/* The router driven at the ABI the boxes have, which is the one thing the suite cannot
   do.

   Two guards in checkValue compare a value against itself narrowed to the target's long
   and refuse it when the two differ. Where long is eight bytes those comparisons are
   between a value and itself, the compiler removes them, and the object is byte
   identical whether they are written or not: no case can be made to fail over them.
   Where long is four bytes they are the difference between refusing an identifier of
   two to the thirty two plus one and deleting timer one.

   So this is not a case in the suite. It is a program that runs where long is four
   bytes, and the script beside it refuses to run it anywhere else rather than passing
   quietly. The table it drives carries the row the timer module ships, written out here
   rather than linked.

   Three answers, and each names which half is being asked for: a value past what a
   thirty two bit unsigned holds is refused before a handler is entered, which is the
   narrowing round trip and the row's own ceiling together; a value past the row's
   ceiling but inside what the type holds is refused, which is the ceiling on its own;
   and a value inside both reaches the handler as itself. */

#include "httpd/endpoint.h"
#include "httpd/router.h"

#include <cstdio>
#include <cstring>
#include <string>

using namespace httpd;

namespace
{

// What the handler was given, so a refusal that let a value through under
// another number is visible as that number rather than as a code.
bool               entered = false;
unsigned long long seen = 0;

Response removeTimer(const Request &r)
{
	entered = true;
	seen = r.asUInt("id");

	Response out;
	out.code = StatusNoContent;
	return out;
}

// The shipped row, bound and all. 2147483647 because the timer manager keeps
// its counter in an int.
const Param kIdParams[] = {
	HTTPD_SEGMENT_IN("id", ParamType::UInt, "the timer, as the daemon numbers them", 1, 2147483647L),
};

const Endpoint kEndpoints[] = {
	{ Method::Delete, "/api/v1/timers/{id}", AuthLevel::System, "removes one timer",
	  HTTPD_PARAMS(kIdParams), NULL, &removeTimer },
};

const RouteTable kTable = { HTTPD_TABLE("probe", kEndpoints) };

int failures = 0;

void expectRefused(const char *what, const std::string &path)
{
	entered = false;
	seen = 0;

	const Response r = dispatchIn(kTable, Delete, path, "", "", "127.0.0.1", AuthLevel::System);
	if (r.code == 400 && !entered)
	{
		std::printf("ok   %s: %s answered %d and no handler ran\n", what, path.c_str(), r.code);
		return;
	}

	std::printf("FAIL %s: %s answered %d, handler %s, id %llu\n",
	            what, path.c_str(), r.code, entered ? "ran" : "did not run", seen);
	++failures;
}

void expectHandled(const std::string &path, unsigned long long want)
{
	entered = false;
	seen = 0;

	const Response r = dispatchIn(kTable, Delete, path, "", "", "127.0.0.1", AuthLevel::System);
	if (r.code == 204 && entered && seen == want)
	{
		std::printf("ok   inside both: %s answered %d and the handler saw %llu\n",
		            path.c_str(), r.code, seen);
		return;
	}

	std::printf("FAIL inside both: %s answered %d, handler %s, id %llu, wanted %llu\n",
	            path.c_str(), r.code, entered ? "ran" : "did not run", seen, want);
	++failures;
}

} // namespace

int main()
{
	std::printf("sizeof(long) is %u\n", (unsigned) sizeof(long));
	if (sizeof(long) != 4)
	{
		std::printf("FAIL this proves nothing where long is not four bytes\n");
		return 2;
	}

	// Past what a thirty two bit unsigned holds. Cast rather than checked, this
	// arrives as one and removes timer one.
	expectRefused("past the type", "/api/v1/timers/4294967297");
	// Past the row and inside the type.
	expectRefused("past the row", "/api/v1/timers/2147483648");
	// Inside both.
	expectHandled("/api/v1/timers/7", 7);

	std::printf("%s\n", failures == 0 ? "all three answered as they should" : "SOMETHING ANSWERED WRONGLY");
	return failures == 0 ? 0 : 1;
}
