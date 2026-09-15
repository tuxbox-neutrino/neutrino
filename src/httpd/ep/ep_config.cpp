/*
 * ep_config.cpp - routes for raw configuration files
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
#include "httpd/status.h"

#include "coreapi/base/errors.h"
#include "coreapi/base/result.h"
#include "coreapi/system.h"

#include <cerrno>
#include <cstddef>
#include <string>

#include <fcntl.h>

namespace httpd
{

namespace
{

/* The name a caller writes beside the document it stands for. A table and not a chain
   of comparisons, because the row that declares the segment lists the same three
   names and the two have to be read side by side to stay the same three. The router
   turns away every other name before a handler is entered. */
struct NamedDocument
{
	const char               *name;
	coreapi::config::Document document;
};

const NamedDocument kDocuments[] = {
	{ "services",  coreapi::config::Document::Services },
	{ "bouquets",  coreapi::config::Document::Bouquets },
	{ "ubouquets", coreapi::config::Document::UserBouquets },
};

const size_t kDocumentCount = sizeof(kDocuments) / sizeof(kDocuments[0]);

/* What a configuration document goes out as. One place, so the routes below cannot
   answer one kind of file under two names. The copied interface states no type at all
   for these, because what sends the file there sets neither a status nor a type. */
const char *documentContentType()
{
	return "application/xml";
}

/* The file as the answer, opened here and handed to the transport. Nothing reads the
   document into this process on the way: these run to megabytes on a box with a full
   channel list, and a copy in a response body would be that many bytes of this
   process for as long as the answer takes.

   The status is written before the file is put behind it and not left to the builder:
   what that one does is state the length off the descriptor, and an answer carrying
   no status at all is one the transport replaces with a fault about this layer. */
Response documentAt(coreapi::config::Document d)
{
	coreapi::Result<std::string> where = coreapi::config::pathOf(d);
	if (!where.ok())
		return problemFor(where.error());

	const int fd = ::open(where.value().c_str(), O_RDONLY | O_CLOEXEC);
	if (fd < 0)
	{
		/* Nothing at that name is what a box that has never written this document
		   looks like, and it is the one way the open fails that is about what was
		   asked for rather than about the box. Every other way says the box could not
		   read a file it says it keeps. */
		const int why = errno;
		const int code = (why == ENOENT || why == ENOTDIR)
			? StatusNotFound : StatusInternalServerError;
		return problemResponse(code, coreapi::ErrorCode::ConfigDocumentUnreadable,
				       "the box's copy of that document could not be read");
	}

	Response out;
	out.code = StatusOk;
	out.content_type = documentContentType();
	// The descriptor is taken over whichever way this answers, so there is
	// nothing left here to give back.
	if (!answerFromDescriptor(out, fd))
		return problemResponse(StatusInternalServerError,
				       coreapi::ErrorCode::ConfigDocumentUnreadable,
				       "the box's copy of that document could not be read");
	return out;
}

Response configDocument(const Request &r)
{
	const std::string &want = r.asString("document");
	for (size_t i = 0; i < kDocumentCount; ++i)
	{
		if (want == kDocuments[i].name)
			return documentAt(kDocuments[i].document);
	}

	/* The row below lists these same three names and the router refuses
	   anything else, so this answers only for a table that has come apart from
	   the row beside it. It says the fault is here, because nothing the caller
	   sent is wrong. */
	return problemResponse(StatusInternalServerError,
			       coreapi::ErrorCode::ConfigDocumentUnreadable,
			       "the box does not know which document that name stands for");
}

Response reloadSetup(const Request &)
{
	coreapi::Result<void> done = coreapi::system::reloadSetup();
	if (!done.ok())
		return problemFor(done.error());
	return accepted();
}

const Param kDocumentParams[] = {
	/* An enumeration and not a name a caller writes. A route that put a name
	   off the request into a path would read whatever the box can read the
	   moment that name carries a separator or a pair of dots, and the three
	   documents there are do not need one. */
	HTTPD_SEGMENT_FROM_SET("document", "which of the documents the box keeps its configuration in",
		"services,bouquets,ubouquets"),
};

const Endpoint kConfigEndpoints[] = {
	{ Method::Get, "/api/v1/config/{document}", AuthLevel::Read,
	  "one of the documents the box keeps its own configuration in, as it stands on the box",
	  HTTPD_PARAMS(kDocumentParams), NULL, &configDocument, false },
	/* Beside the three above rather than with the rest of the box, because
	   what it asks for is the other half of the same subject: those hand a
	   configuration document out, this asks the box to read them again. */
	{ Method::Post, "/api/v1/system/reload-setup", AuthLevel::Write,
	  "asks the box to read its configuration again",
	  NULL, 0, NULL, &reloadSetup, false },
};

} // namespace

extern const RouteTable configTable = {
	HTTPD_TABLE("config", kConfigEndpoints)
};

} // namespace httpd
