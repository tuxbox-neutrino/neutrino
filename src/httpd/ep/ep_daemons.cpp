/*
 * ep_daemons.cpp - routes for daemons
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

#include "httpd/endpoints.h"

#include "httpd/endpoint.h"
#include "httpd/http.h"
#include "httpd/schema.h"
#include "httpd/status.h"

#include "coreapi/daemons.h"
#include "coreapi/base/errors.h"
#include "coreapi/base/result.h"

#include <cstddef>
#include <string>
#include <vector>

namespace httpd
{

namespace
{

/* Three routes and not one route with a verb in the body. A verb the box does not
   have is then a path this server does not have, which is answered before anything
   below is asked. With the verb in a body the set of them would be a list written
   here beside the list the layer below keeps, and the two could come to disagree.

   Every one of them is System. Starting a daemon on this box is running a script as
   the account this server runs as, which is the whole of the box. */

Response answerFor(const coreapi::Result<void> &done)
{
	if (!done.ok())
		return problemFor(done.error());
	/* Nothing to say. The verb ran and the read beside it is what answers what
	   is running now: a body stating that here would be this server's own guess
	   at a process table it has not looked at since. */
	return noContent();
}

Response startDaemon(const Request &r)
{
	return answerFor(coreapi::daemons::start(r.asString("name")));
}

Response stopDaemon(const Request &r)
{
	return answerFor(coreapi::daemons::stop(r.asString("name")));
}

Response restartDaemon(const Request &r)
{
	return answerFor(coreapi::daemons::restart(r.asString("name")));
}

const Param kNameParams[] = {
	HTTPD_SEGMENT_TEXT_FROM_ASKED_SET("name", "the daemon, as the daemon list names it", 64, &daemonNames),
};

const Endpoint kDaemonEndpoints[] = {
	{ Method::Post, "/api/v1/daemons/{name}/start", AuthLevel::System,
	  "starts one daemon",
	  HTTPD_PARAMS(kNameParams), NULL, &startDaemon, false },
	{ Method::Post, "/api/v1/daemons/{name}/stop", AuthLevel::System,
	  "stops one daemon",
	  HTTPD_PARAMS(kNameParams), NULL, &stopDaemon, false },
	{ Method::Post, "/api/v1/daemons/{name}/restart", AuthLevel::System,
	  "stops one daemon and starts it again",
	  HTTPD_PARAMS(kNameParams), NULL, &restartDaemon, false },
};

} // namespace

/* The daemons this box drives, for the two documents that state them and for nothing
   else.

   Asked for rather than written down here, for the reason the layer below owns the
   list at all: which daemons a box has is that layer's, so is the refusal for a name
   it does not have. What this adds is that a reader and a generated client are handed
   the names rather than left to find them out by asking. Nothing is said when the
   read fails, which leaves both the segment and the identifier described as text.

   Outside the block above because the listing is answered by another module and states
   the same names: one function, so the two cannot describe one list differently.

   The bound on the segment stays. The refusal for a name nobody has quotes it back, so
   the bound is what keeps a caller from choosing how long its own words come back. */
void daemonNames(std::vector<std::string> &out)
{
	coreapi::Result<std::vector<coreapi::daemons::Entry> > got = coreapi::daemons::list();
	if (!got.ok())
		return;

	const std::vector<coreapi::daemons::Entry> all = std::move(got).value();
	for (size_t i = 0; i < all.size(); ++i)
		out.push_back(all[i].name);
}

extern const RouteTable daemonsTable = {
	HTTPD_TABLE("daemons", kDaemonEndpoints)
};

} // namespace httpd
