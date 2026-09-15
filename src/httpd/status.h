/*
 * status.h - HTTP status codes, and the problems they carry
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

#ifndef __httpd_status_h__
#define __httpd_status_h__

#include "endpoint.h"

#include "coreapi/base/errors.h"
#include "coreapi/base/result.h"

#include <string>

namespace httpd
{

// The one place a domain status becomes an HTTP one, so that two handlers
// answering the same refusal cannot answer it with two different codes.
int httpStatus(coreapi::Status s);

/* And the way back, for a refusal that has a code first and needs a status to carry
   beside it. Here rather than wherever such a refusal is written, so the two
   directions are one pair.

   405, 413 and 429 are the codes this server sends that no status projects to, and
   each answers the half of itself that is true. The near misses are what must not be
   answered: NotSupported projects to 501, an answer about the whole server rather
   than one path, and Internal says the fault is in the server. */
coreapi::Status statusForCode(int http_code);

/* An RFC 9457 problem document. Appended rather than returned, so a handler
   assembling a reply keeps the one buffer it has. The type member is "/errors/" and
   the wire form of the code, which is the half a client branches on.

   http_code is a parameter and not read off the error, a refusal the transport made
   having a code and no error behind it. The title follows that code rather than the
   error's status, so the two cannot contradict each other. */
void appendProblem(std::string &out, const coreapi::Error &e, int http_code);

const char *problemContentType();

/* What every answer this API sends carries besides its own headers, and the reason
   each of the three is not optional.

   No-store. Several of these answers are the credential itself: the one the login
   route sends carries the second token in its body and sits beside a Set-Cookie, and
   every answer above the read level was built for one caller.

   Vary on the credential. One URL answers 403 to one caller and 200 to the next, and
   what tells them apart is a cookie and an Authorization header, neither of which
   anything keeping answers looks at unless it is told to. The recommended way to run
   this is behind a reverse proxy, so without this line the proxy is free to hand one
   caller's answer to another. Appended to whatever the answer already varies on
   rather than written over it.

   Nosniff. The type this server states is the type it means. The one place it cannot
   state a true one is a file in the pages directory with an extension the table does
   not name, which goes out as bytes of no stated kind, and that directory is readable
   by anyone who reaches the port.

   Each is left alone where the answer already carries one of that name. */
void addApiHeaders(Response &r);

// The last of the three on its own, for whoever needs that one alone.
void addNoSniff(Response &r);

/* What the answers built out of the pages directory carry instead: the one
   above, which those answers need for the same reason the API's do, and a
   refusal to be framed, which is about a rendered page and so belongs to no
   other answer this server makes.

   Not the other two of the set above, and that has not changed: these answers
   carry their own directive about keeping, and they vary on what they are
   encoded with rather than on who asked.

   WHY A FRAME IS WORTH REFUSING. This is the control panel of a box, and a
   page somewhere else that can put it in a frame can lay its own surface over
   it and collect a click that lands on a button here. The second token is no
   answer to that: it stops a cross site request that carries a body, and a
   click on a page already loaded in a frame carries the session the browser
   already holds.

   WHY IT HAS TO BE A HEADER. The page states the rest of what it allows in a
   meta element, because the page knows what it loads and this file does not.
   The framing directive is the one a meta element does not carry, and this
   header carries that directive and nothing else, so the two never have to
   agree about anything and neither has to be changed when the other is.

   WHY 'self' AND NOT 'none'. What is worth refusing is a foreign page framing
   this one. A page from here framing another page from here gives a caller
   nothing it does not already have, and a screen that wants to show the docs
   page inside itself is a screen somebody may write. The older header says
   the same as well as it can, for a browser that does not read the directive:
   it is the weaker of the two even so, because several browsers that had it
   compared the top window only and not every frame on the way.

   ON EVERY ANSWER BUILT HERE AND NOT ONLY ON THE PAGES. What tells these
   answers apart from the API's is the surface they belong to and not what
   they carry, so the refusal and the page it stands in for carry the same
   headers. A rule that applied to some answers of one surface is a rule
   somebody has to decide about per answer.

   AND NOT ON THE API'S ANSWERS. Those go through the call above this one,
   which reaches every one of them. A framed document that is never rendered
   cannot be clicked through, and what a frame of a JSON answer would show its
   framer is what the same origin policy already keeps from it. */
void addPageHeaders(Response &r);

/* One refusal, as the thing that goes back to the caller. Every layer that turns a
   request down answers through this, the router and the transport alike, two shapes
   for one kind of answer being two documents a client has to know about. */
Response problemResponse(int http_code, coreapi::ErrorCode code, const std::string &detail);

/* The same document for the one refusal above written out already, for the answer
   that has to be sent when writing one has just failed.

   Everything the builder above does allocates, and the moment that matters is the
   moment an allocation has already thrown. This is a string literal the transport
   sends without copying it. Written by hand and held to the builder by a case
   comparing the two, a document that drifted being a second shape that only appears
   when nothing else is working. */
const char *fixedProblem();

} // namespace httpd

#endif
