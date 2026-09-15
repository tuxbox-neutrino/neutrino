/*
 * status.cpp - HTTP status codes, and the problems they carry
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

#include "status.h"

#include "http.h"
#include "json.h"

#include "coreapi/base/errors.h"

namespace httpd
{

namespace
{

/* A phrase for the code that is written into the document beside it, and not for the
   status the error carries. The two members have to agree whatever the call site
   passes, and a title read off the status could not: a refusal the transport made has
   a code and no error behind it, so the code is the only one of the two that is
   always the answer being sent.

   Per code and not per error code, because the latter is already the type member. */
const char *titleForCode(int http_code)
{
	switch (http_code)
	{
		case StatusBadRequest:          return "Bad request";
		case StatusUnauthorized:        return "Unauthorized";
		case StatusForbidden:           return "Forbidden";
		case StatusNotFound:            return "Not found";
		case StatusMethodNotAllowed:    return "Method not allowed";
		case StatusConflict:            return "Conflict";
		case StatusPayloadTooLarge:     return "Payload too large";
		case StatusUnsupportedMedia:    return "Unsupported media type";
		case StatusRangeNotSatisfiable: return "Range not satisfiable";
		case StatusTooManyRequests:     return "Too many requests";
		case StatusInternalServerError: return "Internal error";
		case StatusNotImplemented:      return "Not implemented";
		case StatusServiceUnavailable:  return "Service unavailable";
		default:                        break;
	}
	/* Anything else is a code no answer here is built from, a success code
	   included, and there is no phrase for it that would not be a guess. Saying
	   so leaves a wrong code visible in the document rather than dressed as one
	   of the answers above. */
	return "Error";
}

} // namespace

int httpStatus(coreapi::Status s)
{
	switch (s)
	{
		case coreapi::Status::Ok:              return StatusOk;
		case coreapi::Status::NotFound:        return StatusNotFound;
		case coreapi::Status::InvalidArgument: return StatusBadRequest;
		case coreapi::Status::Conflict:        return StatusConflict;
		case coreapi::Status::NotSupported:    return StatusNotImplemented;
		/* The box is busy with something it cannot do twice, which is a conflict
		   over the box. Service unavailable says this server is the thing that
		   is not there, and invites a client to retry the whole request against
		   a server that never stopped answering. */
		case coreapi::Status::Busy:            return StatusConflict;
		case coreapi::Status::Denied:          return StatusForbidden;
		case coreapi::Status::Internal:        return StatusInternalServerError;
	}
	/* No default case above, and an unhandled enumerator is an error in this
	   directory, so a status added without a mapping stops the build. What is left
	   for this line is a value cast into the enum from outside, and it answers zero
	   rather than a plausible code because a plausible one would leave the case that
	   walks the enum unable to fail: 500 is inside the range that case checks. */
	return 0;
}

coreapi::Status statusForCode(int http_code)
{
	switch (http_code)
	{
		case StatusBadRequest:       return coreapi::Status::InvalidArgument;
		case StatusForbidden:        return coreapi::Status::Denied;
		/* What was presented is not a credential, which is a refusal about the
		   caller. It does not come back as this code, the one that does being the
		   code above, and the near miss here is Internal, which would say the fault
		   is in the server. */
		case StatusUnauthorized:     return coreapi::Status::Denied;
		case StatusNotFound:         return coreapi::Status::NotFound;
		case StatusMethodNotAllowed: return coreapi::Status::NotFound;
		/* What was sent was wrong, which is the half of the answer that is true:
		   nothing here projects to this code, so the code the projection back gives
		   is 400 and not this one. The near miss is Internal, which would say the
		   fault is in the server. */
		case StatusPayloadTooLarge:  return coreapi::Status::InvalidArgument;
		/* The third code this server sends that no status projects to, and the same
		   treatment: what it answers is that the box is already doing as much of this
		   as it will and the caller's part is to come back. The near miss is Denied,
		   which projects to 403 and would say the caller may not do this at all. */
		case StatusTooManyRequests:  return coreapi::Status::Busy;
		// The form what was sent was written in is not one this reads, which
		// is the request being wrong and not the server being broken.
		case StatusUnsupportedMedia: return coreapi::Status::InvalidArgument;
		/* The stretch that was asked for is not in the file, which is the
		   request naming bytes that are not there and not the server failing
		   to find them. The near miss is NotFound, which projects to 404 and
		   would say the file itself is not there. */
		case StatusRangeNotSatisfiable: return coreapi::Status::InvalidArgument;
		case StatusConflict:         return coreapi::Status::Conflict;
		case StatusNotImplemented:   return coreapi::Status::NotSupported;
		/* The box is carrying as many of these at once as it will, which is
		   the same thing the code above says and the same answer. What both of
		   them are not is Internal: the caller's part is to come back. */
		case StatusServiceUnavailable: return coreapi::Status::Busy;
		default:                     break;
	}
	/* Every other code reaching here is one this server sends because
	   something inside it is wrong rather than because the request was, which
	   is what Internal says, and a code nobody sends at all has no better
	   answer than that either. */
	return coreapi::Status::Internal;
}

void appendProblem(std::string &out, const coreapi::Error &e, int http_code)
{
	Json j(out);
	j.beginObject();

	/* Two pieces, so the value is assembled before it is written: the writer
	   quotes and escapes one string at a time, and a member written as a prefix
	   and a tail would be two members to the writer counting separators. */
	std::string type = "/errors/";
	type += coreapi::codeString(e.code);
	j.key("type");
	j.value(type);

	j.key("title");
	j.value(titleForCode(http_code));

	j.key("status");
	j.value(http_code);

	// The message quotes what the caller sent, so it is the one member here
	// that carries bytes this layer did not write.
	j.key("detail");
	j.value(e.message);

	j.endObject();
}

const char *problemContentType()
{
	return "application/problem+json";
}

const char *fixedProblem()
{
	return "{\"type\":\"/errors/box-unreadable\","
	       "\"title\":\"Internal error\","
	       "\"status\":500,"
	       "\"detail\":\"this server failed to answer the request\"}";
}

namespace
{

bool sameHeaderName(const std::string &a, const char *b)
{
	size_t i = 0;
	for (; i < a.size() && b[i] != '\0'; ++i)
	{
		char x = a[i];
		char y = b[i];
		if (x >= 'A' && x <= 'Z')
			x = (char)(x - 'A' + 'a');
		if (y >= 'A' && y <= 'Z')
			y = (char)(y - 'A' + 'a');
		if (x != y)
			return false;
	}
	return i == a.size() && b[i] == '\0';
}

// A field name is read without regard to case wherever it is read, so a header
// this file adds beside one an answer already carries would be two headers of
// one name to everything downstream.
std::string *headerNamed(Response &r, const char *name)
{
	for (size_t i = 0; i < r.headers.size(); ++i)
	{
		if (sameHeaderName(r.headers[i].first, name))
			return &r.headers[i].second;
	}
	return NULL;
}

void addIfAbsent(Response &r, const char *name, const char *value)
{
	if (headerNamed(r, name) == NULL)
		r.headers.push_back(std::make_pair(std::string(name), std::string(value)));
}

// Whether a list of field names already names this one, so that a second
// answer built out of a first does not say it twice.
bool listNames(const std::string &list, const char *name)
{
	size_t at = 0;
	while (at < list.size())
	{
		size_t end = list.find(',', at);
		if (end == std::string::npos)
			end = list.size();

		size_t from = at;
		size_t to = end;
		while (from < to && (list[from] == ' ' || list[from] == '\t'))
			++from;
		while (to > from && (list[to - 1] == ' ' || list[to - 1] == '\t'))
			--to;

		if (sameHeaderName(list.substr(from, to - from), name))
			return true;
		at = end + 1;
	}
	return false;
}

} // namespace

void addNoSniff(Response &r)
{
	addIfAbsent(r, "X-Content-Type-Options", "nosniff");
}

void addPageHeaders(Response &r)
{
	addNoSniff(r);
	addIfAbsent(r, "Content-Security-Policy", "frame-ancestors 'self'");
	addIfAbsent(r, "X-Frame-Options", "SAMEORIGIN");
}

void addApiHeaders(Response &r)
{
	addIfAbsent(r, "Cache-Control", "no-store");
	addNoSniff(r);

	static const char *const on[] = { "Cookie", "Authorization" };
	std::string *vary = headerNamed(r, "Vary");
	if (vary == NULL)
	{
		r.headers.push_back(std::make_pair(std::string("Vary"),
		                                   std::string("Cookie, Authorization")));
		return;
	}

	for (size_t i = 0; i < sizeof(on) / sizeof(on[0]); ++i)
	{
		if (listNames(*vary, on[i]))
			continue;
		if (!vary->empty())
			*vary += ", ";
		*vary += on[i];
	}
}

Response problemResponse(int http_code, coreapi::ErrorCode code, const std::string &detail)
{
	Response r;
	r.code = http_code;
	r.content_type = problemContentType();
	appendProblem(r.body, coreapi::Error(statusForCode(http_code), code, detail), http_code);
	return r;
}

} // namespace httpd
