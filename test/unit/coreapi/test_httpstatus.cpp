/*
 * test_httpstatus.cpp - tests for the status codes
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

#include "support/catch.hpp"
#include "httpd/status.h"
#include "coreapi/base/errors.h"
#include "coreapi/base/result.h"

#include "support/counts.h"

#include <cstdio>
#include <string>
#include <utility>
#include <vector>

using coreapi::Status;

namespace
{

/* The statuses the enum declares, name and value, written out by the script in
   test before the suite runs. Read rather than transcribed, because a bound
   written here is a bound that stops where its author last looked: the one this
   replaced stopped at Denied and never reached Internal beside it. */
struct Declared
{
	std::string name;
	int         value;
};

const std::vector<Declared> &declaredStatuses()
{
	static std::vector<Declared> v;
	static bool done = false;
	if (done)
		return v;
	done = true;

	FILE *f = fopen(COREAPI_STATUSES_FILE, "r");
	if (f == NULL)
		return v;

	std::string line;
	int c;
	for (;;)
	{
		c = fgetc(f);
		if (c != EOF && c != '\n')
		{
			line += (char) c;
			continue;
		}
		const size_t tab = line.find('\t');
		if (tab != std::string::npos && tab + 1 < line.size())
		{
			Declared d;
			d.name = line.substr(0, tab);
			d.value = atoi(line.c_str() + tab + 1);
			v.push_back(d);
		}
		line.clear();
		if (c == EOF)
			break;
	}

	fclose(f);
	return v;
}

/* The codes the server has names for, read out of its own header by the script
   in test. Beside the reader above rather than shared with it, because the two
   files are two questions and a reader that took the path as a parameter would
   be one line shorter and one indirection longer. */
const std::vector<Declared> &declaredCodes()
{
	static std::vector<Declared> v;
	static bool done = false;
	if (done)
		return v;
	done = true;

	FILE *f = fopen(HTTPD_CODES_FILE, "r");
	if (f == NULL)
		return v;

	std::string line;
	int c;
	for (;;)
	{
		c = fgetc(f);
		if (c != EOF && c != '\n')
		{
			line += (char) c;
			continue;
		}
		const size_t tab = line.find('\t');
		if (tab != std::string::npos && tab + 1 < line.size())
		{
			Declared d;
			d.name = line.substr(0, tab);
			d.value = atoi(line.c_str() + tab + 1);
			v.push_back(d);
		}
		line.clear();
		if (c == EOF)
			break;
	}

	fclose(f);
	return v;
}

} // namespace

TEST_CASE("every status maps to the code the contract names", "[httpstatus]")
{
	REQUIRE(httpd::httpStatus(Status::Ok)              == 200);
	REQUIRE(httpd::httpStatus(Status::NotFound)        == 404);
	REQUIRE(httpd::httpStatus(Status::InvalidArgument) == 400);
	REQUIRE(httpd::httpStatus(Status::Conflict)        == 409);
	REQUIRE(httpd::httpStatus(Status::NotSupported)    == 501);
	// Busy is a conflict over the box, not the server being unavailable.
	REQUIRE(httpd::httpStatus(Status::Busy)            == 409);
	REQUIRE(httpd::httpStatus(Status::Denied)          == 403);
	REQUIRE(httpd::httpStatus(Status::Internal)        == 500);
}

/* What this is: every status the header declares, cast back from the value the scan
   read beside it, asked for a code and held to that code being one an HTTP answer can
   carry. What it is not: the thing that catches an unmapped status. That is the
   compiler, an unhandled enumerator being an error where httpStatus is built, so this
   case would only ever see the line after the switch, which answers a number no status
   has. The list is read and not written out, so adding a status moves the number
   recorded below rather than sitting outside a bound somebody has to widen. */
TEST_CASE("every status the header declares answers with a code", "[httpstatus]")
{
	const std::vector<Declared> &declared = declaredStatuses();
	INFO("read from " << COREAPI_STATUSES_FILE);
	REQUIRE(declared.size() > 0);

	for (size_t i = 0; i < declared.size(); ++i)
	{
		const int code = httpd::httpStatus((Status) declared[i].value);
		INFO("status " << declared[i].name << ", value " << declared[i].value);
		REQUIRE(code >= 200);
		REQUIRE(code <= 599);
	}

	recordCount("statuses the enum declares", declared.size());
}

TEST_CASE("the problem body carries the code, the status and the detail", "[httpstatus]")
{
	coreapi::Error e(Status::NotFound, coreapi::ErrorCode::NoSuchChannel,
	                 "channel_id 0x2b66 is not in the channel list");
	std::string out;
	httpd::appendProblem(out, e, 404);

	REQUIRE(out.find("\"type\":\"/errors/no-such-channel\"") != std::string::npos);
	REQUIRE(out.find("\"status\":404") != std::string::npos);
	REQUIRE(out.find("channel_id 0x2b66") != std::string::npos);
}

TEST_CASE("a detail that carries a quote does not break the body", "[httpstatus]")
{
	coreapi::Error e(Status::InvalidArgument, coreapi::ErrorCode::BadString,
	                 "value \"a\\b\" has a newline\n");
	std::string out;
	httpd::appendProblem(out, e, 400);

	// The raw bytes must not appear; the escaped forms must.
	REQUIRE(out.find("\n") == std::string::npos);
	REQUIRE(out.find("\\n") != std::string::npos);
	REQUIRE(out.find("\\\"a\\\\b\\\"") != std::string::npos);
}

TEST_CASE("a control byte in the detail leaves the document readable", "[httpstatus]")
{
	// The short forms above are the ones a reader recognises; the rest of the
	// control range has none, and a raw one of them ends the string for whoever
	// parses it. Nothing else in the suite reaches that branch.
	std::string message = "start";
	message += '\x01';
	message += "middle";
	message += '\x1f';
	message += "end";

	coreapi::Error e(Status::Internal, coreapi::ErrorCode::BoxUnreadable, message);
	std::string out;
	httpd::appendProblem(out, e, 500);

	REQUIRE(out.find('\x01') == std::string::npos);
	REQUIRE(out.find('\x1f') == std::string::npos);
	REQUIRE(out.find("\\u0001") != std::string::npos);
	REQUIRE(out.find("\\u001f") != std::string::npos);
	REQUIRE(out.find("start") != std::string::npos);
	REQUIRE(out.find("end") != std::string::npos);
}

TEST_CASE("every status carries the phrase its code is named by", "[httpstatus]")
{
	// The phrase is what a person reads when the code is all a client acted on,
	// so a status whose branch nothing reaches can be wrong and say so to
	// nobody. Busy and Conflict share one, because they share a code.
	struct Case { Status status; const char *title; };
	static const Case expected[] = {
		{ Status::NotFound,        "Not found" },
		{ Status::InvalidArgument, "Bad request" },
		{ Status::Conflict,        "Conflict" },
		{ Status::Busy,            "Conflict" },
		{ Status::NotSupported,    "Not implemented" },
		{ Status::Denied,          "Forbidden" },
		{ Status::Internal,        "Internal error" }
	};

	for (size_t i = 0; i < sizeof(expected) / sizeof(expected[0]); i++)
	{
		coreapi::Error e(expected[i].status, coreapi::ErrorCode::BadEnum, "why");
		std::string out;
		httpd::appendProblem(out, e, httpd::httpStatus(expected[i].status));

		const std::string want = std::string("\"title\":\"") + expected[i].title + "\"";
		INFO(out);
		REQUIRE(out.find(want) != std::string::npos);
	}
}

TEST_CASE("the title follows the code that is sent and not the error", "[httpstatus]")
{
	/* A refusal the transport made carries a code of its own and whatever error
	   the handler had to hand, so the two can disagree. The document has to stay
	   internally consistent when they do, which is why the phrase is read off
	   the code. */
	coreapi::Error e(Status::NotFound, coreapi::ErrorCode::NoSuchChannel, "why");
	std::string out;
	httpd::appendProblem(out, e, 405);

	REQUIRE(out.find("\"title\":\"Method not allowed\"") != std::string::npos);
	REQUIRE(out.find("\"status\":405") != std::string::npos);
	REQUIRE(out.find("Not found") == std::string::npos);
	// The type still names what went wrong, which is the error's and not the
	// transport's to say.
	REQUIRE(out.find("\"type\":\"/errors/no-such-channel\"") != std::string::npos);
}

TEST_CASE("every code this server names has a phrase of its own", "[httpstatus]")
{
	/* Several of these no status maps to: they are the refusals the transport
	   and the gate make, which carry a code and no error. A
	   phrase written for a branch nothing exercises is a phrase that can be
	   wrong and say so to nobody. */
	struct Case { int code; const char *title; };
	static const Case expected[] = {
		{ 400, "Bad request" },
		{ 401, "Unauthorized" },
		{ 403, "Forbidden" },
		{ 404, "Not found" },
		{ 405, "Method not allowed" },
		{ 409, "Conflict" },
		{ 413, "Payload too large" },
		{ 415, "Unsupported media type" },
		{ 416, "Range not satisfiable" },
		{ 429, "Too many requests" },
		{ 500, "Internal error" },
		{ 501, "Not implemented" },
		{ 503, "Service unavailable" }
	};

	for (size_t i = 0; i < sizeof(expected) / sizeof(expected[0]); i++)
	{
		coreapi::Error e(Status::Internal, coreapi::ErrorCode::BoxUnreadable, "why");
		std::string out;
		httpd::appendProblem(out, e, expected[i].code);

		const std::string want = std::string("\"title\":\"") + expected[i].title + "\"";
		INFO(out);
		REQUIRE(out.find(want) != std::string::npos);
	}
}

TEST_CASE("a code no answer here is built from is not given a phrase", "[httpstatus]")
{
	coreapi::Error e(Status::Internal, coreapi::ErrorCode::BoxUnreadable, "why");
	std::string out;
	httpd::appendProblem(out, e, 418);

	REQUIRE(out.find("\"title\":\"Error\"") != std::string::npos);
	REQUIRE(out.find("\"status\":418") != std::string::npos);
}

/* Every refusal the server has a name for, held to having a status of its own.

   Read out of the header rather than written out here, because a list written here is
   one that stops where its author last looked: the one this stands beside walks six
   codes and was transcribed before the login throttle existed, so the code that throttle
   answers with could be mapped to the very answer its own comment calls the near miss
   and the whole suite stayed green.

   What is asked of each is the one thing true of all of them: a code the server sends
   because a request was wrong must not come back as the status that says this server is
   broken. Internal is what an unmapped code falls through to. */
TEST_CASE("every refusal the header names has a status of its own", "[httpstatus]")
{
	const std::vector<Declared> &declared = declaredCodes();
	INFO("read from " << HTTPD_CODES_FILE);
	REQUIRE(declared.size() > 0);

	size_t refusals = 0;
	for (size_t i = 0; i < declared.size(); ++i)
	{
		if (declared[i].value < 400)
			continue;
		++refusals;

		const coreapi::Status s = httpd::statusForCode(declared[i].value);
		INFO("code " << declared[i].name << ", value " << declared[i].value);

		if (declared[i].value != 500)
			REQUIRE(s != Status::Internal);

		// And what it answers is a refusal in the other direction too, so a
		// mapping that landed on a success is caught from both ends.
		REQUIRE(httpd::httpStatus(s) >= 400);
	}

	// Every code above four hundred and none of the ones below it, which is
	// what makes the walk above the walk it says it is.
	REQUIRE(refusals > 0);
	recordCount("refusals the header names", refusals);
}

TEST_CASE("a code answers with a status that answers that code back", "[httpstatus]")
{
	/* A refusal the transport made has a code first and needs a status to
	   carry beside it. If the two disagree, whoever asks the status for a code
	   is told something other than the document beside it states, and the
	   answer changes shape depending on which half was read.

	   These are the codes this server sends before a handler is reached. */
	static const int sent[] = { 400, 403, 404, 409, 500, 501 };
	for (size_t i = 0; i < sizeof(sent) / sizeof(sent[0]); i++)
	{
		INFO("code " << sent[i]);
		REQUIRE(httpd::httpStatus(httpd::statusForCode(sent[i])) == sent[i]);
	}
}

TEST_CASE("the one code no status answers is not given the status that misleads", "[httpstatus]")
{
	/* Nothing this layer maps answers 405, so that code is the one the pair
	   cannot round trip. What matters is which way it fails: the near miss is
	   NotSupported, which projects to 501 and turns an answer about one path
	   into an answer about the whole server. It carries the half of it that is
	   true instead, that the request named something that is not there. */
	const coreapi::Status s = httpd::statusForCode(405);
	REQUIRE(s != coreapi::Status::NotSupported);
	REQUIRE(httpd::httpStatus(s) != 501);
	REQUIRE(httpd::httpStatus(s) == 404);
}

TEST_CASE("the code for a body too big says the request was wrong", "[httpstatus]")
{
	/* The second code the pair cannot round trip, and the near miss here is
	   the answer an unmapped code gets: Internal says the fault is in the
	   server, and this one is the caller having sent more than the server
	   takes. */
	const coreapi::Status s = httpd::statusForCode(413);
	REQUIRE(s != coreapi::Status::Internal);
	REQUIRE(s == coreapi::Status::InvalidArgument);
}

TEST_CASE("a refusal is the one shape whoever wrote it", "[httpstatus]")
{
	/* The router and the transport both turn requests down, and a second
	   builder for the second of them would be a second document shape for one
	   kind of answer. */
	const httpd::Response r = httpd::problemResponse(404, coreapi::ErrorCode::NoSuchRoute, "nowhere");
	REQUIRE(r.code == 404);
	REQUIRE(r.content_type == httpd::problemContentType());
	REQUIRE(r.body.find("\"type\":\"/errors/no-such-route\"") != std::string::npos);
	REQUIRE(r.body.find("\"title\":\"Not found\"") != std::string::npos);
	REQUIRE(r.body.find("\"status\":404") != std::string::npos);
	REQUIRE(r.body.find("\"detail\":\"nowhere\"") != std::string::npos);
}

TEST_CASE("the document written out already is the one the builder writes", "[httpstatus]")
{
	/* The one answer that can be sent when writing an answer is what failed.
	   Written by hand, so nothing but this holds it to the builder every other
	   refusal goes through, and a document that drifted would be a second
	   shape appearing only when nothing else is working. */
	const httpd::Response built = httpd::problemResponse(500, coreapi::ErrorCode::BoxUnreadable,
	                                                     "this server failed to answer the request");
	REQUIRE(std::string(httpd::fixedProblem()) == built.body);
}

TEST_CASE("the body is sent under the media type problems have", "[httpstatus]")
{
	// A problem document under application/json is one a client has to sniff.
	REQUIRE(std::string(httpd::problemContentType()) == "application/problem+json");
}

namespace
{

/* What an answer says under one field name, and the empty string for a name it
   does not carry. Exact on the name and not the way the server reads one,
   because every name compared below is a name written here. */
std::string headerOf(const httpd::Response &r, const std::string &name)
{
	for (size_t i = 0; i < r.headers.size(); ++i)
	{
		if (r.headers[i].first == name)
			return r.headers[i].second;
	}
	return std::string();
}

size_t headersNamed(const httpd::Response &r, const std::string &name)
{
	size_t n = 0;
	for (size_t i = 0; i < r.headers.size(); ++i)
	{
		if (r.headers[i].first == name)
			++n;
	}
	return n;
}

} // namespace

TEST_CASE("a page refuses to be framed, in both spellings of the refusal", "[httpstatus]")
{
	/* Two headers for one thing because two kinds of browser read them. The
	   directive is the one that is exact and is the one a meta element cannot
	   carry, which is the whole reason this is a header at all; the older
	   field is what a browser that does not read the directive is left with,
	   and a browser that reads both ignores it.

	   The value is 'self' and not 'none' on purpose: what is being refused is
	   a page from somewhere else framing this one. */
	httpd::Response r;
	httpd::addPageHeaders(r);

	REQUIRE(headerOf(r, "Content-Security-Policy") == "frame-ancestors 'self'");
	REQUIRE(headerOf(r, "X-Frame-Options") == "SAMEORIGIN");

	// And the one these are said beside, because they replace the call that
	// used to say it alone and a page that lost it would have lost it quietly.
	REQUIRE(headerOf(r, "X-Content-Type-Options") == "nosniff");
}

TEST_CASE("the API's answers are given no header about framing", "[httpstatus]")
{
	/* Decided and not left over. Every answer the router makes goes through
	   this call, and a document that is never rendered cannot be clicked
	   through: what a frame of it would show its framer is what the same
	   origin policy already keeps from it. So the header would be bytes on
	   every answer of the busiest surface here, saying nothing. */
	httpd::Response r;
	httpd::addApiHeaders(r);

	REQUIRE(headerOf(r, "X-Content-Type-Options") == "nosniff");
	REQUIRE(headerOf(r, "Content-Security-Policy").empty());
	REQUIRE(headerOf(r, "X-Frame-Options").empty());
}

TEST_CASE("an answer that stated a framing rule of its own keeps it", "[httpstatus]")
{
	/* A field name is read without regard to case wherever it is read, so a
	   second header of one name is two policies to everything downstream and
	   which of them binds is not this server's to decide. An answer with
	   something of its own to say goes on saying it, which is how every other
	   header this file adds behaves. */
	httpd::Response r;
	r.headers.push_back(std::make_pair(std::string("Content-Security-Policy"),
	                                   std::string("frame-ancestors 'none'")));
	httpd::addPageHeaders(r);

	REQUIRE(headersNamed(r, "Content-Security-Policy") == 1);
	REQUIRE(headerOf(r, "Content-Security-Policy") == "frame-ancestors 'none'");
	REQUIRE(headerOf(r, "X-Frame-Options") == "SAMEORIGIN");
}
