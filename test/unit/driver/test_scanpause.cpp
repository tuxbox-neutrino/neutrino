/*
 * test_scanpause.cpp - when the EPG scan stops for a stream and when it goes on
 *
 * Copyright (C) 2026 Thilo Graf
 *
 * License: GPL-2.0-or-later
 *
 * scanpause holds what the EPG scan decides when a stream ends, when
 * zapit has done the scan's zap and when sectionsd reports a read complete,
 * as plain functions. These cases drive the shipped object.
 */

#include "support/catch.hpp"

#include <driver/scanpause.h>

TEST_CASE("P1: only a scan that paused and is on goes on when the last stream ends", "[driver][scanpause]")
{
	REQUIRE(scanpause::resumeNow(true, true));
	// A scan reading a transponder is not moved on to the next.
	REQUIRE_FALSE(scanpause::resumeNow(false, true));
	// A scan that is off in the box's mode asks nothing of zapit, whatever
	// Next() left behind when it was called anyway.
	REQUIRE_FALSE(scanpause::resumeNow(true, false));
	REQUIRE_FALSE(scanpause::resumeNow(false, false));
}

TEST_CASE("P2: a tuned scan zap with no stream about reads the transponder", "[driver][scanpause]")
{
	REQUIRE(scanpause::onBackZap(true, false) == scanpause::Read);
}

TEST_CASE("P3: a stream running or being set up pauses the scan, tuned or not", "[driver][scanpause]")
{
	REQUIRE(scanpause::onBackZap(true, true) == scanpause::Pause);
	// The hold took the frontend before the scan's zap: not a tune failure.
	REQUIRE(scanpause::onBackZap(false, true) == scanpause::Pause);
}

TEST_CASE("P4: a scan zap that did not tune with no stream about skips the transponder", "[driver][scanpause]")
{
	// As before, also when only a recording runs.
	REQUIRE(scanpause::onBackZap(false, false) == scanpause::Skip);
}

TEST_CASE("P5: a read a stream started in is read again, also when the stream ended before the report", "[driver][scanpause]")
{
	// The stream ended, then sectionsd reported: what the box does.
	REQUIRE(scanpause::onEitComplete(true, true, false, 0) == scanpause::ReadAgain);
	// The stream still runs.
	REQUIRE(scanpause::onEitComplete(true, true, true, 0) == scanpause::ReadAgain);
}

TEST_CASE("P6: a report while a stream runs or is set up does not count, its start unseen or not", "[driver][scanpause]")
{
	REQUIRE(scanpause::onEitComplete(true, false, true, 0) == scanpause::ReadAgain);
}

TEST_CASE("P7: a read no stream came into counts", "[driver][scanpause]")
{
	REQUIRE(scanpause::onEitComplete(true, false, false, 0) == scanpause::Count);
}

TEST_CASE("P8: a report for another transponder counts as before", "[driver][scanpause]")
{
	/* A recording's zap gave sectionsd another service, or nothing was
	   handed over: the stream says nothing about that one. */
	REQUIRE(scanpause::onEitComplete(false, true, false, 0) == scanpause::Count);
	REQUIRE(scanpause::onEitComplete(false, true, true, 0) == scanpause::Count);
	REQUIRE(scanpause::onEitComplete(false, false, true, 0) == scanpause::Count);
	REQUIRE(scanpause::onEitComplete(false, false, false, 0) == scanpause::Count);
}

TEST_CASE("P9: a read disturbed after each of a few re-reads handed over is read again, then counts", "[driver][scanpause]")
{
	/* n: the re-reads handed over already. Reports while the scan stays
	   paused hand nothing over and leave n as it is. */
	for (int n = 0; n < scanpause::kMaxReReads; n++) {
		REQUIRE(scanpause::onEitComplete(true, true, false, n) == scanpause::ReadAgain);
		REQUIRE(scanpause::onEitComplete(true, false, true, n) == scanpause::ReadAgain);
	}
	// Streams that keep coming do not keep the scan from ending.
	REQUIRE(scanpause::onEitComplete(true, true, false, scanpause::kMaxReReads) == scanpause::CountDisturbed);
	REQUIRE(scanpause::onEitComplete(true, false, true, scanpause::kMaxReReads) == scanpause::CountDisturbed);
	REQUIRE(scanpause::onEitComplete(true, true, true, scanpause::kMaxReReads + 1) == scanpause::CountDisturbed);
}

TEST_CASE("P10: the re-reads do not change what an undisturbed read or another transponder's report does", "[driver][scanpause]")
{
	REQUIRE(scanpause::kMaxReReads > 0);
	REQUIRE(scanpause::onEitComplete(true, false, false, scanpause::kMaxReReads) == scanpause::Count);
	REQUIRE(scanpause::onEitComplete(false, true, true, scanpause::kMaxReReads) == scanpause::Count);
	REQUIRE(scanpause::onEitComplete(false, false, false, 1) == scanpause::Count);
}

TEST_CASE("P11: a re-read counts when the same transponder is handed over after a disturbed read", "[driver][scanpause]")
{
	REQUIRE(scanpause::rereadsAtHandOver(true, true, 0) == 1);
	REQUIRE(scanpause::rereadsAtHandOver(true, true, 2) == 3);
	// The same transponder handed over after a read nothing came into.
	REQUIRE(scanpause::rereadsAtHandOver(true, false, 2) == 2);
	REQUIRE(scanpause::rereadsAtHandOver(true, false, 0) == 0);
}

TEST_CASE("P12: another transponder handed over starts the re-reads anew", "[driver][scanpause]")
{
	REQUIRE(scanpause::rereadsAtHandOver(false, true, 2) == 0);
	REQUIRE(scanpause::rereadsAtHandOver(false, false, 3) == 0);
	REQUIRE(scanpause::rereadsAtHandOver(false, true, 0) == 0);
}

TEST_CASE("P13: hourly repeats of a disturbed report during one long stream do not use up the re-reads", "[driver][scanpause]")
{
	/* One hand-over, then the scan stays paused for many reports: the
	   count only moves when the transponder is handed over again. */
	int rereads = scanpause::rereadsAtHandOver(false, false, 0);
	for (int report = 0; report < 2 * scanpause::kMaxReReads + 2; report++)
		REQUIRE(scanpause::onEitComplete(true, true, true, rereads) == scanpause::ReadAgain);
	for (int n = 1; n <= scanpause::kMaxReReads; n++) {
		rereads = scanpause::rereadsAtHandOver(true, true, rereads);
		REQUIRE(rereads == n);
	}
	REQUIRE(scanpause::onEitComplete(true, true, false, rereads) == scanpause::CountDisturbed);
}

TEST_CASE("P14: a frontend not tuned since the scan's zap leaves the read to the stream marks", "[driver][scanpause]")
{
	REQUIRE_FALSE(scanpause::readDisturbed(false, true, 7, 7));
	REQUIRE(scanpause::onEitComplete(true, scanpause::readDisturbed(false, true, 7, 7), false, 0) == scanpause::Count);
	// A stream's mark still counts.
	REQUIRE(scanpause::readDisturbed(true, true, 7, 7));
}

TEST_CASE("P15: a frontend tuned since the scan's zap disturbs the read without any mark", "[driver][scanpause]")
{
	// A recording, a stream hold or a scan of another transponder tuned it
	// and said nothing to the scan.
	REQUIRE(scanpause::readDisturbed(false, true, 7, 8));
	REQUIRE(scanpause::onEitComplete(true, scanpause::readDisturbed(false, true, 7, 8), false, 0) == scanpause::ReadAgain);
	// A count that went round still differs.
	REQUIRE(scanpause::readDisturbed(false, true, 0xffffffffu, 0));
}

TEST_CASE("P16: without zapit's record of the scan's zap for the channel handed over the read is disturbed", "[driver][scanpause]")
{
	// Two scan zaps in flight: the record names the other channel.
	REQUIRE(scanpause::readDisturbed(false, false, 7, 7));
	REQUIRE(scanpause::onEitComplete(true, scanpause::readDisturbed(false, false, 0, 0), false, 0) == scanpause::ReadAgain);
}

TEST_CASE("P17: retunes still count the transponder after a few re-reads", "[driver][scanpause]")
{
	int rereads = 0;
	for (int n = 0; n < scanpause::kMaxReReads; n++) {
		REQUIRE(scanpause::onEitComplete(true, scanpause::readDisturbed(false, true, 1, 2), false, rereads) == scanpause::ReadAgain);
		rereads = scanpause::rereadsAtHandOver(true, true, rereads);
	}
	REQUIRE(scanpause::onEitComplete(true, scanpause::readDisturbed(false, true, 1, 2), false, rereads) == scanpause::CountDisturbed);
}
