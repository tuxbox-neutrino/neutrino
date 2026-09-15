/*
 * openapi.h - the OpenAPI document, built from the route tables
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

#ifndef __httpd_openapi_h__
#define __httpd_openapi_h__

#include "httpd/endpoint.h"

#include <cstddef>
#include <string>

namespace httpd
{

namespace openapi
{

/* The document that describes every route this server answers, written out of the
   tables themselves and never beside them. A document typed by hand is a second
   statement of the same surface, and the two part company the first time a parameter
   is added to one of them. So everything below walks the list of tables, and a route
   that appears in the list appears in the document because there is no other way for
   it to get in.

   The tables declare no success code, so the answer is described under the range
   rather than under a number somebody guessed: the routes that make something answer
   201 and the ones that ask the box for something answer 202. */

// Where the document is served, which is also the path the route below
// declares, so the two cannot name different places.
const char *documentPath();

/* The document for these tables, appended to what the caller already holds.

   with_descriptions carries the prose the tables hold: the summary of a route, what a
   parameter says about itself and what a member of an answer says about itself. False
   leaves every one of those out and changes nothing else, so a build without the
   prose still carries every path, every parameter and every shape.

   The response descriptions the format itself demands are not that prose and stay
   whatever this is asked for: a Response Object without one is not a document any
   reader accepts.

   Reachable so a case can drive it against a table written where the case is and
   against either setting of the flag. */
void appendDocument(std::string &out, const RouteTable *const *tables,
                    size_t table_count, bool with_descriptions);

// Whether this build was compiled with the prose in it, which is what the
// cached document below was built with.
bool descriptionsCompiledIn();

/* The document for the tables in effect, built once and kept.

   Kept because building it walks every route, every parameter and every shape, and
   because the alternative shape, a tree of nodes handed to a serialiser, is about a
   megabyte of transient heap on the smallest box this runs on.

   The reference is good for the life of the process in the product, where the tables
   are constants the linker wrote. A case that installs one gets a document built for
   it, and the buffer the previous one was in is the buffer that is rewritten, so a
   reference taken across such a change is not one to hold. */
const std::string &document();

/* The same bytes compressed, built beside the document and kept beside it, and empty
   when the library would not compress them. Beside it rather than instead of it,
   because the caller that takes no compressed form is the documented one: a plugin
   reaching this with plain curl sends no Accept-Encoding at all. */
const std::string &documentGzip();

// How many times the builder has run, which is what says the answer above was
// kept rather than made again.
size_t buildCount();

/* Whether this answer is the document, which is what the transport asks before it does
   the one thing below that only the transport can do.

   Asked of the answer and never of the path it came from. The path off a request is
   not decoded until the router splits it, so an escaped spelling reads as a different
   path to anything comparing text, and the route and the transport would then disagree
   about which path they are on.

   Cheap for everything that is not it: nothing has been built until the route that
   builds it has answered, and after that the length settles almost every call. */
bool isDocument(const Response &r);

/* The compressed form put in place of the plain one when this caller said it takes it,
   and the answer unchanged when it did not. Here and not in the handler because a
   handler is given a request that has been read for the values its own row declares
   and is given no headers, and what a caller accepts is a header. */
Response withEncoding(const Response &r, const std::string &accept_encoding);

/* The table this module states, as the pair rather than as an array and a count: a
   count declared in one unit and read in another is not a constant the linker can
   write. */
extern const RouteTable openapiTable;

} // namespace openapi

} // namespace httpd

#endif
