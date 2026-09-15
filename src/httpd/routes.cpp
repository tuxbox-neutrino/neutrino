/*
 * routes.cpp - every route table, gathered in one place
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

#include "router.h"

#include "endpoint.h"
#include "endpoints.h"
#include "events.h"
#include "doc/openapi.h"
#include "webtv.h"

#include <cstddef>

namespace httpd
{

namespace
{

/* What a case has put in front of the list, and nothing the rest of the time.
   A pointer and no object, so this file still needs nothing to have run before
   main. */
const RouteTable *installed = NULL;

/* The tables, in the order they are searched, which is an order nothing rests on:
   a pair of routes that could tie is refused where the tables are checked.

   A module states a whole table and this file names it. Nothing registers itself, for
   the reason the header beside this gives: a translation unit whose only effect is a
   constructor before main defines no symbol anything asks for, and the linker never
   pulls such a unit out of the archives this program is built from. Thirty odd
   archives go into this program. Each table is declared in a header that the module
   defining it also includes, so a declaration that had drifted from its definition is
   caught where the definition is.

   Addresses and not tables, which keeps that same rule from being broken here. A
   table copied out of the module that states it is a value another unit hands over,
   and nothing but code running before main can copy it. An address of an object with
   static storage duration is a constant, so the list below is written into the
   program image and nothing runs to make it: there is no .init_array entry in this
   unit and there must not be one.

   The same reasoning rules out a list sized from a count another unit hands over,
   which is why a module states a whole table and not an array and a count. */
const RouteTable *const kTables[] = {
	&channelsTable,
	&epgTable,
	&settingsTable,
	&systemTable,
	&webserverTable,
	&storageTable,
	&netfsTable,
	&eventsTable,
	&authTable,
	&timersTable,
	&osdTable,
	&daemonsTable,
	&tunerTable,
	&pluginsTable,
	&configTable,
	&streamTable,
	&webtvTable,
	&recordingsTable,
	&openapi::openapiTable,
};

} // namespace

// The one list of tables this server answers from.
const RouteTable *const *allRoutes(size_t *table_count)
{
	if (installed != NULL)
	{
		if (table_count != NULL)
			*table_count = 1;
		/* The address of the pointer above, which is a list of one holding
		   the installed table. Nothing is built to say that: the variable is
		   there whether or not a case has written to it. */
		return &installed;
	}

	if (table_count != NULL)
		*table_count = sizeof(kTables) / sizeof(kTables[0]);
	return kTables;
}

void setRoutesForTest(const RouteTable *t)
{
	installed = t;
}

} // namespace httpd
