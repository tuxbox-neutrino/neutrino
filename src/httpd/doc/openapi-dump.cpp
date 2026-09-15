/*
 * openapi-dump.cpp - writing the OpenAPI document at build time
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

/* Writes the document the server answers with to standard output, and starts nothing.

   A program of its own because what reads the document is a build step of the pages,
   and asking that step for a running box would mean a box to hand before a page can be
   compiled. The tables are constants the linker wrote and the walk over them touches
   nothing else, so the same bytes come out here as come out of the route.

   The tables are checked first and the program refuses rather than printing: a
   document written out of tables the server would not start on is the one thing this
   must not leave behind. */

#include "openapi.h"
#include "httpd/router.h"

#include <cstdio>
#include <string>

int main()
{
	std::string why;
	if (!httpd::routesAreSane(&why))
	{
		std::fprintf(stderr, "the routes are not ones this server can answer from: %s\n",
		             why.c_str());
		return 1;
	}

	const std::string &doc = httpd::openapi::document();
	// No newline after it. What the route answers is the document and nothing
	// besides, and a byte written here that the route does not write is the
	// one difference a comparison of the two would find.
	if (std::fwrite(doc.data(), 1, doc.size(), stdout) != doc.size())
	{
		std::fprintf(stderr, "the document could not be written whole\n");
		return 1;
	}
	if (std::fflush(stdout) != 0)
	{
		std::fprintf(stderr, "the document could not be flushed\n");
		return 1;
	}
	return 0;
}
