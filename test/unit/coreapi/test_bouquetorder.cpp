/*
 * test_bouquetorder.cpp - tests for bouquet order
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
#include "coreapi/bouquetorder.h"

#include <cstddef>
#include <string>

using namespace coreapi;

namespace
{
ChannelIdList list(const char *spell)
{
	/* One digit per channel, so a case states an order as a word and the
	   expectation beside it reads as one. The identifiers a box hands out are
	   sixty four bits wide and nothing here turns on how wide they are; what
	   the cases are about is which of them ends up where. */
	ChannelIdList out;
	for (const char *p = spell; *p != '\0'; p++)
		out.push_back((ChannelId) *p);
	return out;
}

/* The moves applied the way the channel stack applies them: the channel at
   from taken out, and put back in at to. Written here rather than borrowed,
   because a case that applied the moves with the same code that planned them
   would agree with itself about a plan that is wrong. */
std::string applied(const char *have, const ChannelMoveList &moves)
{
	ChannelIdList now = list(have);
	for (size_t i = 0; i < moves.size(); i++)
	{
		REQUIRE(moves[i].from < now.size());
		REQUIRE(moves[i].to < now.size());
		const ChannelId one = now[moves[i].from];
		now.erase(now.begin() + (ptrdiff_t) moves[i].from);
		now.insert(now.begin() + (ptrdiff_t) moves[i].to, one);
	}

	std::string out;
	for (size_t i = 0; i < now.size(); i++)
		out += (char) now[i];
	return out;
}

std::string ordered(const char *have, const char *want)
{
	ChannelMoveList moves;
	channelOrderMoves(list(have), list(want), moves);
	return applied(have, moves);
}
}

TEST_CASE("a half already in the order that was asked for is not moved at all", "[bouquetorder]")
{
	/* Nothing and not a move that lands where it started: a caller handing
	   back the order a listing gave it is the common request, and every move
	   this plans is a write into a list the box draws from. */
	ChannelMoveList moves;
	channelOrderMoves(list("abcd"), list("abcd"), moves);
	REQUIRE(moves.empty());
}

TEST_CASE("a channel is taken from where it is and put where it was asked for", "[bouquetorder]")
{
	// Counted from nought and read against the list as it stands, which is
	// what the channel stack's own move reads them as.
	ChannelMoveList moves;
	channelOrderMoves(list("abcd"), list("cabd"), moves);
	REQUIRE(moves.size() == 1u);
	REQUIRE(moves[0].from == 2u);
	REQUIRE(moves[0].to == 0u);
}

TEST_CASE("the moves one after another leave the order that was asked for", "[bouquetorder]")
{
	/* Each of these is applied to what the one before it left, so a plan whose
	   second pair was worked out against the list as it arrived is caught here
	   and not by the shape of any single move. */
	REQUIRE(ordered("abcd", "dcba") == "dcba");
	REQUIRE(ordered("abcd", "badc") == "badc");
	REQUIRE(ordered("abcdef", "fedcba") == "fedcba");
	REQUIRE(ordered("ab", "ba") == "ba");
	REQUIRE(ordered("a", "a") == "a");
}

TEST_CASE("a channel the half does not hold is passed over", "[bouquetorder]")
{
	/* The call that puts a channel in is answered with nothing, so a request
	   naming one that never arrived has to leave the rest in order rather than
	   move some other channel into the place it was meant for. */
	REQUIRE(ordered("abc", "xbxaxc") == "bac");

	ChannelMoveList moves;
	channelOrderMoves(list("abc"), list("xyz"), moves);
	REQUIRE(moves.empty());
}

TEST_CASE("a channel the list does not name ends up behind the ones it does", "[bouquetorder]")
{
	/* Taking it out is the business of the call before this one. What ordering
	   owes it is a place that is not in the middle of what was asked for, and
	   the order those channels were in among themselves. */
	REQUIRE(ordered("abcde", "ec") == "ecabd");
	REQUIRE(ordered("abc", "c") == "cab");
}

TEST_CASE("a half with nothing in it is not moved", "[bouquetorder]")
{
	ChannelMoveList moves;
	channelOrderMoves(ChannelIdList(), list("ab"), moves);
	REQUIRE(moves.empty());

	channelOrderMoves(list("ab"), ChannelIdList(), moves);
	REQUIRE(moves.empty());
}

TEST_CASE("a channel named twice is placed once", "[bouquetorder]")
{
	// The route above this one keeps the first mention and drops the rest, and
	// this answers for a list that reached here with both: the second mention
	// must not take the channel back out of the place the first gave it.
	REQUIRE(ordered("abc", "ccab") == "cab");
}

TEST_CASE("the plan is written against what the moves leave and not against what arrived",
          "[bouquetorder]")
{
	/* The shape that separates a plan made against the list as each move
	   leaves it from one made against the list as it arrived: b sits at one to
	   begin with and at two by the time it is wanted, because c has been put
	   in front of it. A plan that said one there would put a in b's place. */
	ChannelMoveList moves;
	channelOrderMoves(list("abcd"), list("cbad"), moves);
	REQUIRE(moves.size() == 2u);
	REQUIRE(moves[0].from == 2u);
	REQUIRE(moves[0].to == 0u);
	REQUIRE(moves[1].from == 2u);
	REQUIRE(moves[1].to == 1u);
	REQUIRE(applied("abcd", moves) == "cbad");
}
