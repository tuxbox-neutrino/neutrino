/*
 * main.cpp - test runner entry point
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

#define CATCH_CONFIG_RUNNER
#include "catch.hpp"

#include "answers.h"
#include "counts.h"

#include "coreapi/settings/choicesources.h"

#include <cstdio>
#include <stdlib.h>
#include <unistd.h>

/* The runner is written out rather than taken from the header, because what
   each case compared is only whole once every case has run and Catch promises
   nothing about which case runs last. */
int main(int argc, char *argv[])
{
	Catch::Session session;

	const int bad = session.applyCommandLine(argc, argv);
	if (bad != 0)
		return bad;

	/* What a text setting offers is read from the box's places, and the suite
	   answers the same whatever the host it runs on has installed there: every
	   case reads them from an empty directory, and a case that wants a list
	   makes its own. */
	char empty[] = "/tmp/coreapi-unit-XXXXXX";
	if (mkdtemp(empty) == NULL)
	{
		std::perror("no empty directory for the lists of the box");
		return 1;
	}
	coreapi::internal::setChoicePlaceForTest(empty);

	watchAnswers();
	const int failed = session.run();
	rmdir(empty);

	// A run given a name or a tag to pick out is not the whole suite, so the
	// counts of it are not the suite's counts and nothing is compared.
	if (!session.configData().testsOrTags.empty())
	{
		std::fprintf(stderr, "coverage counts not compared: this run was a selection\n");
		return failed;
	}

	/* After a failure the comparison still says which numbers moved, because a
	   case that stopped comparing is often what a failure did, but it does not
	   decide the answer: the run has already failed for a reason somebody is
	   about to read and a count a case never reached is unknown and not fallen. */
	const bool answered = answersAgree(COREAPI_ANSWERS_ACTUAL);
	const bool agree = coverageCountsAgree(COREAPI_COUNTS_FILE, COREAPI_COUNTS_ACTUAL, failed == 0);
	if (failed != 0)
		return failed;
	return (agree && answered) ? 0 : 1;
}
