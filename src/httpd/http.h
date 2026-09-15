/*
 * http.h - HTTP parsing, and the pieces of a request
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

#ifndef __httpd_http_h__
#define __httpd_http_h__

#include <string>

namespace httpd
{

// The methods this server answers. Anything else is a method the router does
// not offer, which is a different answer from a path it does not know, so the
// unknown one is a member rather than a failed lookup.
enum Method
{
	Get,
	Post,
	Put,
	Patch,
	Delete,
	Head,
	Options,
	UnknownMethod
};

// Case sensitive on purpose: the request line carries the method in upper case
// and a lower case one is a malformed request rather than a spelling to accept.
Method      methodFromString(const std::string &s);
const char *methodName(Method m);

// The status codes stated as names, because a number written into a handler
// reads as an amount and gets compared as one.
const int StatusOk                  = 200;
const int StatusCreated             = 201;
/* The box was asked and the asking is all this server can answer for: several
   of these commands go to a loop that reports nothing back, so 200 would be
   this server claiming something it has no way to know. */
const int StatusAccepted            = 202;
const int StatusNoContent           = 204;
/* One stretch of a file rather than the whole of it, for a caller that asked
   for one. A recording runs to gigabytes, and a player that jumps into the
   middle of one asks for the bytes it lands on and no others; a server that
   only ever answers the whole file makes every jump cost the whole file. */
const int StatusPartialContent      = 206;
/* One answer carrying an outcome per thing that was asked for, for the write
   that takes several at once. A single code cannot say that some of them
   landed and one did not, and answering the worst of them would report the
   ones that landed as though they had not. */
const int StatusMultiStatus         = 207;
/* A directory named without the separator that makes it one. The name is
   good, only the spelling is not, and it stays good under this code for as
   long as the directory does: a bookmark or a link written against it keeps
   working through this rather than pointing at the day it moved. */
const int StatusMovedPermanently    = 301;
const int StatusNotModified         = 304;
const int StatusBadRequest          = 400;
const int StatusUnauthorized        = 401;
const int StatusForbidden           = 403;
const int StatusNotFound            = 404;
const int StatusMethodNotAllowed    = 405;
const int StatusConflict            = 409;
const int StatusPayloadTooLarge     = 413;
const int StatusUnsupportedMedia    = 415;
/* The stretch that was asked for is not in the file. Its own code rather than
   the whole file, because a caller handed the file from the start when it
   asked for bytes past the end has been answered something other than what it
   asked for, with nothing in the answer saying so. */
const int StatusRangeNotSatisfiable = 416;
/* Too much is being asked of the one route that answers before a caller has
   shown who it is. A refusal a caller answers by coming back, which is why it
   is its own code and not one of the two above it. */
const int StatusTooManyRequests     = 429;
const int StatusInternalServerError = 500;
const int StatusNotImplemented      = 501;
const int StatusServiceUnavailable  = 503;

} // namespace httpd

#endif
