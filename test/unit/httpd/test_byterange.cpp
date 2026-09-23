/*
 * test_byterange.cpp - tests for range requests
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

#include "httpd/byterange.h"

#include <string>

#include <stdint.h>

/* The reading of one Range header, driven on its own, because a header is a handful of
   characters and the number of ways of writing one that must not be read as a range is
   larger than the number of files a case could plant to drive it through a socket.

   What every case here is really about is the difference between the two ways of not
   answering a range: ignoring one, which sends the whole file, and refusing one, which
   sends no bytes at all. Reading a header the wrong way round turns a download that
   would have worked into a refusal, or a refusal into an answer of the wrong length
   that a player reads as the file being shorter than it is. */

using namespace httpd;

namespace
{

// Five bytes, which is the length nearly every case here is written against.
const uint64_t kLength = 5;

RangeAnswer read(const char *header, uint64_t length, uint64_t &first, uint64_t &last)
{
	return byteRangeOf(std::string(header), length, first, last);
}

// The two ends are seeded with a value no case expects, so that a reading which
// answers Partial without writing them is not read as one that wrote nought.
const uint64_t kUntouched = 4242;

} // namespace

TEST_CASE("a request carrying no range asks for the whole file", "[byterange]")
{
	uint64_t first = kUntouched;
	uint64_t last = kUntouched;
	REQUIRE(read("", kLength, first, last) == RangeAnswer::Whole);
	REQUIRE(first == kUntouched);
	REQUIRE(last == kUntouched);
}

TEST_CASE("a stretch inside the file is the stretch that was asked for", "[byterange]")
{
	uint64_t first = kUntouched;
	uint64_t last = kUntouched;
	REQUIRE(read("bytes=1-3", kLength, first, last) == RangeAnswer::Partial);
	REQUIRE(first == 1);
	REQUIRE(last == 3);
}

TEST_CASE("one byte is a stretch", "[byterange]")
{
	uint64_t first = kUntouched;
	uint64_t last = kUntouched;
	REQUIRE(read("bytes=0-0", kLength, first, last) == RangeAnswer::Partial);
	REQUIRE(first == 0);
	REQUIRE(last == 0);
}

TEST_CASE("a stretch with no end is read to the end of the file", "[byterange]")
{
	// What a download resumed from where it stopped asks for.
	uint64_t first = kUntouched;
	uint64_t last = kUntouched;
	REQUIRE(read("bytes=2-", kLength, first, last) == RangeAnswer::Partial);
	REQUIRE(first == 2);
	REQUIRE(last == 4);
}

TEST_CASE("a stretch with no start is the last so many bytes", "[byterange]")
{
	uint64_t first = kUntouched;
	uint64_t last = kUntouched;
	REQUIRE(read("bytes=-2", kLength, first, last) == RangeAnswer::Partial);
	REQUIRE(first == 3);
	REQUIRE(last == 4);
}

TEST_CASE("the last more bytes than there are reads as the whole file", "[byterange]")
{
	uint64_t first = kUntouched;
	uint64_t last = kUntouched;
	REQUIRE(read("bytes=-99", kLength, first, last) == RangeAnswer::Partial);
	REQUIRE(first == 0);
	REQUIRE(last == 4);
}

TEST_CASE("an end past the end of the file is brought back to it", "[byterange]")
{
	/* The stretch begins inside the file, so the part of it that exists is what
	   gets sent. This is not the same as the refusal below: there the caller
	   named nothing the file holds at all. */
	uint64_t first = kUntouched;
	uint64_t last = kUntouched;
	REQUIRE(read("bytes=2-99", kLength, first, last) == RangeAnswer::Partial);
	REQUIRE(first == 2);
	REQUIRE(last == 4);
}

TEST_CASE("a start at or past the end of the file is refused", "[byterange]")
{
	uint64_t first = kUntouched;
	uint64_t last = kUntouched;
	REQUIRE(read("bytes=5-", kLength, first, last) == RangeAnswer::Unsatisfiable);
	REQUIRE(read("bytes=5-9", kLength, first, last) == RangeAnswer::Unsatisfiable);
	REQUIRE(read("bytes=99-", kLength, first, last) == RangeAnswer::Unsatisfiable);
	REQUIRE(first == kUntouched);
	REQUIRE(last == kUntouched);
}

TEST_CASE("the last nought bytes is refused", "[byterange]")
{
	// Read and understood completely, and naming no bytes, which is the other
	// of the two refusals and not a header this could not read.
	uint64_t first = kUntouched;
	uint64_t last = kUntouched;
	REQUIRE(read("bytes=-0", kLength, first, last) == RangeAnswer::Unsatisfiable);
	REQUIRE(first == kUntouched);
}

TEST_CASE("a file of no bytes holds no stretch at all", "[byterange]")
{
	uint64_t first = kUntouched;
	uint64_t last = kUntouched;
	REQUIRE(read("bytes=0-", 0, first, last) == RangeAnswer::Unsatisfiable);
	REQUIRE(read("bytes=0-0", 0, first, last) == RangeAnswer::Unsatisfiable);
	REQUIRE(read("bytes=-1", 0, first, last) == RangeAnswer::Unsatisfiable);
}

TEST_CASE("a unit that is not bytes is not read at all", "[byterange]")
{
	uint64_t first = kUntouched;
	uint64_t last = kUntouched;
	REQUIRE(read("items=0-1", kLength, first, last) == RangeAnswer::Whole);
	REQUIRE(read("0-1", kLength, first, last) == RangeAnswer::Whole);
	REQUIRE(read("notbytes=0-1", kLength, first, last) == RangeAnswer::Whole);
	REQUIRE(first == kUntouched);
}

TEST_CASE("the unit is read whatever case it is written in", "[byterange]")
{
	// A token, so the case it is written in is the sender's business.
	uint64_t first = kUntouched;
	uint64_t last = kUntouched;
	REQUIRE(read("BYTES=1-2", kLength, first, last) == RangeAnswer::Partial);
	REQUIRE(first == 1);
	REQUIRE(last == 2);
}

TEST_CASE("more than one stretch is answered with the whole file", "[byterange]")
{
	/* And not with the first of them, which would be answering a request
	   nobody made, and not with a refusal, which would leave a caller that
	   asked for two stretches of a file it may read with nothing. */
	uint64_t first = kUntouched;
	uint64_t last = kUntouched;
	REQUIRE(read("bytes=0-1,3-4", kLength, first, last) == RangeAnswer::Whole);
	REQUIRE(first == kUntouched);
	REQUIRE(last == kUntouched);
}

TEST_CASE("ends the wrong way round name no stretch", "[byterange]")
{
	uint64_t first = kUntouched;
	uint64_t last = kUntouched;
	REQUIRE(read("bytes=3-1", kLength, first, last) == RangeAnswer::Whole);
	REQUIRE(first == kUntouched);
}

TEST_CASE("a header that is not numbers is not read", "[byterange]")
{
	uint64_t first = kUntouched;
	uint64_t last = kUntouched;
	REQUIRE(read("bytes=", kLength, first, last) == RangeAnswer::Whole);
	REQUIRE(read("bytes=-", kLength, first, last) == RangeAnswer::Whole);
	REQUIRE(read("bytes=a-b", kLength, first, last) == RangeAnswer::Whole);
	REQUIRE(read("bytes=1", kLength, first, last) == RangeAnswer::Whole);
	REQUIRE(read("bytes=1-2x", kLength, first, last) == RangeAnswer::Whole);
	REQUIRE(read("bytes=+1-2", kLength, first, last) == RangeAnswer::Whole);
	REQUIRE(first == kUntouched);
}

TEST_CASE("a number too large to hold is not read as a small one", "[byterange]")
{
	/* The sum is checked before each digit is added rather than after, because
	   a sum that has already wrapped is a number nothing can tell from one that
	   fits, and the one it would be mistaken for is small enough to be inside
	   the file. */
	uint64_t first = kUntouched;
	uint64_t last = kUntouched;
	REQUIRE(read("bytes=18446744073709551616-", kLength, first, last) == RangeAnswer::Whole);
	REQUIRE(read("bytes=99999999999999999999999-", kLength, first, last) == RangeAnswer::Whole);
	REQUIRE(read("bytes=-99999999999999999999999", kLength, first, last) == RangeAnswer::Whole);
	REQUIRE(first == kUntouched);
}

TEST_CASE("the largest number that does fit is read as itself", "[byterange]")
{
	// The other side of the check above: a guard one digit too eager would
	// turn this into a header nothing reads.
	uint64_t first = kUntouched;
	uint64_t last = kUntouched;
	REQUIRE(read("bytes=18446744073709551615-", kLength, first, last) == RangeAnswer::Unsatisfiable);
	REQUIRE(first == kUntouched);
}

TEST_CASE("a stretch of a file larger than four bytes of length is read whole", "[byterange]")
{
	/* Where this runs a long is four bytes, so a reading that worked in one
	   would stop at four gigabytes, which is under the size of a single evening
	   recorded in high definition. */
	const uint64_t six_gigabytes = ((uint64_t) 6) << 30;
	uint64_t first = kUntouched;
	uint64_t last = kUntouched;
	REQUIRE(read("bytes=5368709120-5368709220", six_gigabytes, first, last) == RangeAnswer::Partial);
	REQUIRE(first == 5368709120ULL);
	REQUIRE(last == 5368709220ULL);
}
