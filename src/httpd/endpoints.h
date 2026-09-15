/*
 * endpoints.h - the route tables every endpoint group registers
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

#ifndef __httpd_endpoints_h__
#define __httpd_endpoints_h__

#include "endpoint.h"
#include "http.h"
#include "status.h"

#include "coreapi/base/result.h"

#include <cstdio>
#include <string>
#include <vector>

#include <stdint.h>

namespace httpd
{

/* What the modules that answer requests share, and the tables they state.

   endpoint.h beside this is the other half of the pair and holds the types one route
   is made of. This one holds the routes themselves.

   Declared in a header rather than by hand where the list is read. Declared by hand a
   table is checked in no translation unit that also holds its definition, so a
   declaration that had drifted from one would link and the server would answer out of
   whatever the drift made of it. Included by the module that defines each table for
   exactly that reason.

   Nothing registers itself. A translation unit whose only effect is a constructor
   before main defines no symbol anything asks for, and the linker never pulls such a
   unit out of the archives this program is built from. */

// The media type every answer below carries. One place, so that two modules
// cannot answer one kind of document under two names.
inline const char *jsonContentType()
{
	return "application/json";
}

/* A refusal the layer below made, as the answer that goes back. The code comes from the
   one projection there is and the document from the one builder there is, so a refusal
   cannot take a second shape by being written out again at a handler.

   Never called for a result that succeeded: the status of one projects to 200, and
   reading the error off such a result ends the process one floor down. Every call site
   is inside a branch that has already asked. */
inline Response problemFor(const coreapi::Error &e)
{
	return problemResponse(httpStatus(e.status), e.code, e.message);
}

// The head of an answer that succeeded, so that the code and the media type
// are written once rather than at each handler.
inline Response okJson()
{
	Response r;
	r.code = StatusOk;
	r.content_type = jsonContentType();
	return r;
}

/* What a command answers with.

   Accepted and not done, because the box is asked on its own loop: the message is handed
   to a queue and nothing comes back from what does it, so an answer saying the change
   has happened would be this server stating something it has no way to have learnt. The
   routes that answer this are the ones whose act is a message: a directory that was made
   and a daemon whose script has run are things that did happen.

   No body. There is nothing to put in one that the caller does not already know. */
inline Response accepted()
{
	Response r;
	r.code = StatusAccepted;
	return r;
}

// An act that was carried out and has nothing to say about itself. Used where
// a read beside it already answers what the state is, so that this answer does
// not become a second and staler statement of it.
inline Response noContent()
{
	Response r;
	r.code = StatusNoContent;
	return r;
}

/* The thing asked for now exists, in the two shapes that answer takes: with a document
   naming it, and without one where there is nothing to name that the caller does not
   already have.

   A pair and not one call with a flag, because the two differ in whether they carry a
   media type, and a media type written beside no body is a statement about bytes that
   are not there. Both are here rather than built at a handler, an answer assembled by
   hand at each site being one that can come to differ from the answer beside it. */
inline Response created()
{
	Response r;
	r.code = StatusCreated;
	return r;
}

inline Response createdJson()
{
	Response r = okJson();
	r.code = StatusCreated;
	return r;
}

/* An identifier as the text it travels as.

   Hexadecimal and a string rather than a number, for the reason the event stream states:
   it is sixty four bits wide, the numbers a browser parses out of JSON carry fifty
   three, and a channel identifier is above that line most of the time it is not zero. A
   page cursor is one of these, which is why the spelling has to be the one the parameter
   reader accepts back.

   The caller's buffer and not a string, so a page of five hundred items costs no
   allocation per item. */
inline const char *hexId(uint64_t v, char (&buf)[24])
{
	std::snprintf(buf, sizeof(buf), "%llx", (unsigned long long) v);
	return buf;
}

/* The daemons this box drives, by the names the routes that drive them take and the
   listing answers.

   Declared here, across the two modules that need it, because it is one list and it is
   stated in two documents: the three routes that act on a daemon name their segment out
   of it, and the listing names its identifier out of it. Written down in only one of
   those two, a reader crossing from the listing to a route has nothing holding the two
   together.

   Nothing is said when the layer below cannot be read, which leaves both described as
   text. */
void daemonNames(std::vector<std::string> &out);

/* What a file an answer hands over is called, as the one header that says so.

   The name reaches the header through nothing but this: every byte that is not plainly
   safe becomes three characters that cannot end a line or close a quotation, so a
   channel called Something"\r\nSet-Cookie: writes no header of its own. Only the encoded
   spelling is written and never the older plain one beside it, for the reason spelled
   out where this is defined.

   Shared across the modules that hand a file over, because a second writer of one header
   is a second set of bytes it lets through. */
std::string dispositionFor(const std::string &name);

/* The one part of this box a token may be minted to stand for, as the name that stands in
   the token.

   A name and not a list of directories, because what a recording directory is called
   changes when a disk is added or taken out or the setting is edited, and it is resolved
   against the roots on every request rather than written down. Shared across the modules
   that say it: one resolves it, one stamps it into a token, and the document states it as
   the whole of what that answer can carry. */
const char *mediaScopeName();

/* Every table this server answers a request out of. Each is stated by the module that
   writes the handlers behind it, as the pair rather than as an array and a count: a
   count declared in one unit and read in another is not a constant the linker can write,
   and a list built from one would be built by a constructor at startup.

   Declared here and not only in the file that reads the list, because a table declared
   where the list is read is declared in no translation unit that also carries its
   definition, and a declaration that had drifted from the table it names would then link
   into whatever the drift made of it. */
extern const RouteTable channelsTable;
extern const RouteTable epgTable;
extern const RouteTable settingsTable;
extern const RouteTable systemTable;
extern const RouteTable storageTable;
/* The two files the box keeps its network filesystems in, as entries. Its own
   table beside the one above and not a corner of it: that one answers about any
   file under the roots and about what the kernel has mounted, and this one
   answers about a table with a shape that a page must not have to know. */
extern const RouteTable netfsTable;
extern const RouteTable authTable;
extern const RouteTable timersTable;
extern const RouteTable osdTable;
extern const RouteTable daemonsTable;
extern const RouteTable tunerTable;
extern const RouteTable pluginsTable;
// The routes that hand out the box's own configuration documents, and the one
// that asks the box to read them again.
extern const RouteTable configTable;

/* The server that is answering, as something it can be asked about and told.
   Its own table and not part of the one above it: what those answer out of is
   neutrino.conf and this one is a different file with rules of its own, and it
   carries the one value that decides whether anything else here is reachable. */
extern const RouteTable webserverTable;

// The addresses a caller is given for a stream of a channel.
extern const RouteTable streamTable;

// What the box is writing to disc right now, and the two acts on it: ending
// one of them, and the shift it keeps of what it is showing.
extern const RouteTable recordingsTable;

} // namespace httpd

#endif
