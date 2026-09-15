/*
 * router.h - matching a request to a route
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

#ifndef __httpd_router_h__
#define __httpd_router_h__

#include "endpoint.h"
#include "http.h"

#include <cstddef>
#include <string>

namespace httpd
{

/* Every table this server answers from, named one by one in the file that owns the
   list. Nothing registers itself: a translation unit whose only effect is a
   constructor before main defines no symbol anything asks for, and the linker never
   pulls such a unit out of the archives this program is built from, so an endpoint
   that registered itself would be missing with nothing said about it. This program
   is linked out of about thirty such archives.

   A list of pointers to tables and not a list of tables, which keeps the file that
   owns the list free of a constructor as well: a table copied out of the module that
   states it is a value another unit hands over, and a list built from those has to
   be built by code that runs before main. An address is a constant the linker
   writes.

   table_count may be null. The answer is null when there are no tables. */
const RouteTable *const *allRoutes(size_t *table_count);

/* The one table the list above answers with while a case has installed one, and NULL
   to hand the list back to what this server ships.

   A seam rather than a parameter threaded through the daemon, because what drives
   the server from outside is a socket and a socket carries no table. Installed
   before the server is started and cleared after it is stopped: the pointer is read
   by the threads that answer requests. */
void setRoutesForTest(const RouteTable *t);

/* Whether these tables are ones the server can answer from. False for a table this
   layer wrote wrong, which is a fault here and not at any caller, so the server
   refuses to start rather than answering out of them.

   What is checked: no two routes that would answer one request equally well, no
   route answering a HEAD, every name in braces declared as a path parameter and
   every path parameter appearing in the path, bounds that a value could be inside
   and that the type in question reads at all, a listed set for every parameter that
   has one and for no parameter that does not, no parameter carried in the body of a
   method that is not written with one, no endpoint reachable without a credential
   but the two named in the file that checks this, and no endpoint that changes
   something asking for less than a write.

   That last one is where a table would otherwise be able to undo the network
   exemption: a caller on the box's own network is granted a read and is asked for no
   credential, so a route that changes something and asks for no more than that is
   one any page a browser on that network visits can reach in that browser's name.

   Equally well, and not written alike. Which of two matching routes answers is
   settled by how many of its segments are written out rather than bound, so the pair
   with no answer left is the pair that ties on that count, which is wider than the
   pair that reads the same: a route naming a segment another binds, in a position
   that other one names back, reads as two routes and answers as one coin toss.

   Every route is compared with every route in every table, because two tables can
   carry one answer between them and neither is wrong read alone.

   why, when it is not null, is left carrying what was found wrong: a server that
   refuses to start and says only that it refused leaves whoever wrote the tables
   reading all of them. */
bool tablesAreSane(const RouteTable *const *tables, size_t table_count, std::string *why = NULL);

// One table on its own, which is every route in it against every other.
bool tableIsSane(const RouteTable &t, std::string *why = NULL);

// Every table the list above names.
bool routesAreSane(std::string *why = NULL);

/* The injectable form, so the router is exercised against a table written where it
   is used rather than only against the ones the server ships.

   path is the request target with its query already taken off, query is what followed
   the question mark, and neither has been percent decoded: decoding happens after the
   path is split, so an encoded separator cannot make one segment into two.

   granted is what the caller has been found to be, resolved before this is called.
   Nothing here reads a credential.

   peer is the address off the socket. reported is where the request says it came
   from, which is the peer unless a proxy list is configured, and it is carried
   through to the handler beside the peer rather than in place of it, so a handler
   comparing an address compares the one no caller wrote. Empty means the two are the
   same, which is why this one has a default.

   session is the token the request presented, carried through for the single route
   whose act is on the session itself and never read here.

   host is the authority the request named, which is how the caller reached this box
   and not how this box would describe itself. Carried through beside the two
   addresses rather than derived from either, read here for nothing, and defaulted so
   a caller with no authority to hand on writes nothing.

   scope is what the credential this request arrived with stands for, empty for one that
   stands for the whole of what its level reaches, and never anything here but a string
   handed on to the handler. Nothing resolves one here: what a scope is worth is the
   route's own business, and a route that does not read it is one no scoped credential
   ever resolved on.

   A HEAD is answered by the route that answers the GET, whole answer and all, and
   the transport sends the head of it. That is also why a table naming a route of its
   own for HEAD is refused. */
Response dispatchIn(const RouteTable *const *tables, size_t table_count, Method m,
                    const std::string &path, const std::string &query,
                    const std::string &body, const std::string &peer, AuthLevel granted,
                    const std::string &reported = std::string(),
                    const std::string &session = std::string(),
                    const std::string &host = std::string(),
                    const std::string &scope = std::string());

// One table, which is what most of what drives this needs.
Response dispatchIn(const RouteTable &t, Method m, const std::string &path,
                    const std::string &query, const std::string &body,
                    const std::string &peer, AuthLevel granted,
                    const std::string &reported = std::string(),
                    const std::string &session = std::string(),
                    const std::string &host = std::string(),
                    const std::string &scope = std::string());

// The same against every table the server ships.
Response dispatch(Method m, const std::string &path,
                  const std::string &query, const std::string &body,
                  const std::string &peer, AuthLevel granted,
                  const std::string &reported = std::string(),
                  const std::string &session = std::string(),
                  const std::string &host = std::string(),
                  const std::string &scope = std::string());

/* The level the route that would answer a request declares, which the transport
   needs before the request is whole.

   A body arrives after the head, so by the time the last byte of it is in this
   server's memory the caller has already been given as much of the box as it asked
   for. The path is in the head, so the route is known before the first byte of the
   body, and that is what lets a caller who may not reach the route be refused before
   it is invited to send a megabyte.

   Read off the same walk that answers the request, so what the transport judges and
   what the dispatch answers cannot come apart, and the refusal the transport writes
   is the router's own.

   False when no route answers that method at that path, which is every shape the
   dispatch answers on its own: a path nobody has, a method that path does not take,
   a table that answers a request twice, and a target this cannot take apart.

   query_token_ok, when it is not null, is left saying whether that route takes a token
   with a scope, which decides both whether the gate reads one out of the query and
   whether one presented in a header counts here at all. False for every shape this
   answers false for, so a path no route answers takes no credential out of an address.

   What it does not answer is whether the route reads a body. A handler may read the
   whole of one without the table declaring a member of it, which the route that
   writes settings does. */
bool routeLevelFor(Method m, const std::string &path, AuthLevel *level,
                   bool *query_token_ok = NULL);

} // namespace httpd

#endif
