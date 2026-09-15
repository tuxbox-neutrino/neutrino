/*
 * test_bouquetname.cpp - tests for bouquet names
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

#include <zapit/bouquets.h>

#include <string>

/* A bouquet carries two names and only one of them is its identity. Name is what
 * parseBouquetsXml writes and reads back, and it is the field existsBouquet compares a
 * name it is offered against, so it decides whether a second bouquet may be created
 * under a given name. bName is what the bouquet list, the bouquet editor, the timer
 * list and both web servers print, and the constructor seeds it from Name because for
 * every bouquet but the two built in ones the two are the same string.
 *
 * A rename that moves only one of them leaves the box showing a name it no longer
 * answers to, or answering to a name it no longer shows. The class is header only where
 * these two fields are concerned, so this reaches the real declaration.
 */

TEST_CASE("a new bouquet shows the name it is known by", "[bouquetname]")
{
	CZapitBouquet b("Erste Programme");

	CHECK(b.Name == "Erste Programme");
	CHECK(b.bName == "Erste Programme");
}

TEST_CASE("renaming a bouquet moves its identity and its caption together", "[bouquetname]")
{
	CZapitBouquet b("Erste Programme");

	b.setName("Zweite Programme");

	CHECK(b.Name == "Zweite Programme");
	CHECK(b.bName == "Zweite Programme");
}

TEST_CASE("a rename replaces a caption the locale had put there", "[bouquetname]")
{
	/* How the two built in bouquets are made: addBouquet and
	 * makeRemainingChannelsBouquet construct with the fixed name that goes
	 * to file and then overwrite the caption with the translated one. That
	 * is the one state in which the two fields legitimately differ, and a
	 * rename out of it has to end it rather than half of it. */
	CZapitBouquet b(DEFAULT_BQ_NAME_FAV);
	b.bName = "Favoriten";

	REQUIRE(b.Name != b.bName);

	b.setName("Meine Sender");

	CHECK(b.Name == "Meine Sender");
	CHECK(b.bName == "Meine Sender");
}

TEST_CASE("a renamed bouquet answers to nothing of its old name", "[bouquetname]")
{
	/* What existsBouquet does, on the one bouquet this can build without the
	 * manager around it: the comparison it runs is Name against the offered
	 * string, and existsUBouquet runs the same comparison against bName as
	 * well. A rename that left either field behind would let one of those two
	 * still find the bouquet under the name the user has given up. */
	CZapitBouquet b("Sport");

	b.setName("Sport HD");

	CHECK_FALSE(b.Name == "Sport");
	CHECK_FALSE(b.bName == "Sport");
	CHECK(b.Name == b.bName);
}
