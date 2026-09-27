/*
 * nhttpd_bridge.cpp - nhttpd's hook into the REST layer
 *
 * Copyright (C) 2026 Thilo Graf
 *
 * License: GPL-2.0-or-later
 */

#include "nhttpd_bridge.h"

#include "httpd/byterange.h"
#include "httpd/endpoint.h"
#include "httpd/http.h"
#include "httpd/router.h"
#include "httpd/status.h"
#include "httpd/doc/openapi.h"
#include "coreapi/base/errors.h"

#include "helper.h"
#include <ylogging.h>

#include <cerrno>
#include <cstdio>
#include <cstring>
#include <exception>
#include <string>

#include <unistd.h>

namespace
{

const char kPrefixApi[] = "/api/v1";
const char kPrefixDoc[] = "/api/openapi.json";

httpd::Method methodOf(THttp_Method m)
{
	switch (m)
	{
		case M_GET:    return httpd::Get;
		case M_POST:   return httpd::Post;
		case M_PUT:    return httpd::Put;
		case M_DELETE: return httpd::Delete;
		case M_HEAD:   return httpd::Head;
		default:       return httpd::UnknownMethod;
	}
}

void takeProblem(CyhookHandler *hh, int code, coreapi::ErrorCode why, const char *detail)
{
	httpd::Response p = httpd::problemResponse(code, why, detail);
	httpd::addApiHeaders(p);
	hh->api_mode = true;
	hh->httpStatus = (HttpResponseType) p.code;
	hh->ResponseMimeType = p.content_type;
	hh->ResponseHeaders = p.headers;
	hh->yresult = p.body;
	hh->status = HANDLED_READY;
}

// After a throw. The document is a literal, but a string assignment can
// still throw on a box out of memory, and a throw out of a handler is not
// caught by the block it sits in. So: try, and if even that fails, abort
// the answer without allocating -- the transport writes nothing and closes.
void takeFixedFailure(CyhookHandler *hh)
{
	try
	{
		hh->api_mode = true;
		hh->httpStatus = HTTP_INTERNAL_SERVER_ERROR;
		hh->ResponseHeaders.clear();
		hh->ResponseMimeType = httpd::problemContentType();
		hh->yresult = httpd::fixedProblem();
		hh->status = HANDLED_READY;
	}
	catch (...)
	{
		hh->status = HANDLED_ABORT;
	}
}

// The descriptor of a file answer is the bridge's from the moment the
// Response arrived; whatever path leaves answerWith(), including a throw
// out of a string assignment, closes it once.
struct HeldDescriptor
{
	int fd;
	explicit HeldDescriptor(int f) : fd(f) {}
	~HeldDescriptor()
	{
		if (fd >= 0)
			close(fd);
	}
	// hands the descriptor on; whoever took it closes it
	int take()
	{
		int taken = fd;
		fd = -1;
		return taken;
	}
private:
	HeldDescriptor(const HeldDescriptor &);
	HeldDescriptor &operator=(const HeldDescriptor &);
};

/* The two spellings a Content-Range has, written once so no two sites can
   disagree about the syntax, as compatible forks do in their server. Cast
   to the widest the format has: the values are sixty four bits wide. */
std::string sentRange(uint64_t first, uint64_t last, uint64_t length)
{
	char buf[96];
	std::snprintf(buf, sizeof(buf), "bytes %llu-%llu/%llu",
	              (unsigned long long) first, (unsigned long long) last,
	              (unsigned long long) length);
	return std::string(buf);
}

std::string noSuchRange(uint64_t length)
{
	char buf[64];
	std::snprintf(buf, sizeof(buf), "bytes */%llu", (unsigned long long) length);
	return std::string(buf);
}

} // namespace

namespace nhttpd_bridge
{

FdPlan planFdAnswer(int code, uint64_t file_length, const std::string &range_header)
{
	FdPlan plan;
	plan.code = code;
	plan.offset = 0;
	plan.length = file_length;
	if (code != 200)
		return plan;

	uint64_t first = 0;
	uint64_t last = 0;
	const httpd::RangeAnswer asked = httpd::byteRangeOf(range_header, file_length, first, last);
	if (asked == httpd::RangeAnswer::Partial)
	{
		plan.code = 206;
		plan.offset = first;
		plan.length = last - first + 1;
		plan.content_range = sentRange(first, last, file_length);
	}
	else if (asked == httpd::RangeAnswer::Unsatisfiable)
	{
		plan.code = 416;
		plan.length = 0;
		plan.content_range = noSuchRange(file_length);
	}
	return plan;
}

} // namespace nhttpd_bridge

CApiBridge::CApiBridge()
{
}

CApiBridge::~CApiBridge()
{
}

bool CApiBridge::routesOk(std::string *why)
{
	try
	{
		return httpd::routesAreSane(why);
	}
	catch (...)
	{
		if (why)
			*why = "the route check threw";
		return false;
	}
}

void CApiBridge::registerPrefixes()
{
	CyhookHandler::registerApiPrefix(kPrefixApi);
	CyhookHandler::registerApiPrefix(kPrefixDoc);
}

bool CApiBridge::claims(const CyhookHandler *hh)
{
	// The one predicate the request reader used, on the one string it used
	// (the decoded url): what the core read as an API request is what this
	// hook answers, and nothing else reaches the legacy hooks.
	CStringList::const_iterator it = hh->UrlData.find("url");
	if (it == hh->UrlData.end())
		return false;
	return CyhookHandler::isApiTarget(it->second);
}

THandleStatus CApiBridge::Hook_PrepareResponse(CyhookHandler *hh)
{
	return claims(hh) ? HANDLED_READY : HANDLED_NONE;
}

THandleStatus CApiBridge::Hook_SendResponse(CyhookHandler *hh)
{
	if (!claims(hh))
		return HANDLED_NONE;

	try
	{
		// The target as written: the router decodes segment by segment and
		// an encoded slash must not become one before it looks.
		const std::string target = hh->UrlData["target"];
		const size_t q = target.find('?');
		const std::string path = target.substr(0, q);
		const std::string query = (q == std::string::npos) ? "" : target.substr(q + 1);
		const httpd::Method m = methodOf(hh->Method);

		if (hh->RawBodyTooLarge)
		{
			takeProblem(hh, 413, coreapi::ErrorCode::BodyTooLarge,
			            "the body is larger than this server keeps for one request, 1 MiB");
			return hh->status;
		}

		/* What the caller is: whatever nhttpd let through. With mod_auth on,
		   that is a caller who gave nhttpd's password; with it off, anybody
		   who reached the box, as /control has always been answered. The
		   router's own levels start to mean something with the session layer. */
		httpd::Response r = httpd::dispatch(m, path, query, hh->RawBody,
		                                    hh->UrlData["clientaddr"], httpd::AuthLevel::System,
		                                    "", "", header_value(hh->HeaderList, "Host"), "");

		if ((m == httpd::Get || m == httpd::Head) && httpd::openapi::isDocument(r))
			r = httpd::openapi::withEncoding(r, header_value(hh->HeaderList, "Accept-Encoding"));

		if (!r.reload_after.empty())
			log_level_printf(1, "api bridge: a route asked for a reload of %s, which this server does not do yet\n",
			                 r.reload_after.c_str());

		answerWith(hh, r);
	}
	catch (const std::exception &e)
	{
		log_level_printf(1, "api bridge: %s\n", e.what());
		takeFixedFailure(hh);
	}
	catch (...)
	{
		takeFixedFailure(hh);
	}
	return hh->status;
}

void CApiBridge::answerWith(CyhookHandler *hh, const httpd::Response &r)
{
	HeldDescriptor held(r.fd);
	const int fd = held.fd;

	const bool is_stream = !r.stream_argv.empty() || !r.relay_url.empty()
	                       || r.content_type == "text/event-stream";
	if (r.code == 200 && is_stream)
	{
		takeProblem(hh, 501, coreapi::ErrorCode::NoSuchMethod,
		            "this server does not carry a stream yet");
		return;
	}

	hh->api_mode = true;
	hh->httpStatus = (HttpResponseType) r.code;
	hh->ResponseMimeType = r.content_type;
	hh->ResponseHeaders = r.headers;
	hh->yresult.clear();

	if (fd >= 0)
	{
		/* The file is not read here: the descriptor, the first byte and the
		   count go to the transport (HANDLED_SENDFD), which writes the header
		   with the stretch's length and sends the bytes out of the file. One
		   stretch where the caller asked for one, and here rather than at a
		   handler, for the same reason compatible forks hold it at the door of
		   their server: a route that worked the arithmetic out itself would be
		   the one a caller cannot resume a download from. */
		const nhttpd_bridge::FdPlan plan = nhttpd_bridge::planFdAnswer(
				r.code, r.length, header_value(hh->HeaderList, "Range"));
		if (plan.code == 416)
		{
			// the problem document is the body; held closes the file
			takeProblem(hh, 416, coreapi::ErrorCode::RangeOutsideFile,
			            "the stretch that was asked for is not in this file");
			hh->ResponseHeaders.push_back(std::make_pair(
					std::string("Content-Range"), plan.content_range));
			return;
		}
		if (!plan.content_range.empty())
			hh->ResponseHeaders.push_back(std::make_pair(
					std::string("Content-Range"), plan.content_range));
		hh->httpStatus = (HttpResponseType) plan.code;
		hh->SendFd = held.take();
		hh->SendFdOffset = (off_t) plan.offset;
		hh->SendFdLength = (off_t) plan.length;
		hh->status = HANDLED_SENDFD;
		return;
	}

	hh->yresult = r.body;
	hh->status = HANDLED_READY;
}
