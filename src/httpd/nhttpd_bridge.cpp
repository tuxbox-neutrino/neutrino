/*
 * nhttpd_bridge.cpp - nhttpd's hook into the REST layer
 *
 * Copyright (C) 2026 Thilo Graf
 *
 * License: GPL-2.0-or-later
 */

#include "nhttpd_bridge.h"

#include "httpd/auth.h"
#include "httpd/byterange.h"
#include "httpd/carry.h"
#include "httpd/endpoint.h"
#include "httpd/events.h"
#include "httpd/http.h"
#include "httpd/router.h"
#include "httpd/login_bridge.h"
#include "httpd/server.h"
#include "httpd/status.h"
#include "httpd/doc/openapi.h"
#include "coreapi/base/errors.h"

#include "helper.h"
#include "ysocket.h"
#include "ywebserver.h"
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
		case M_PATCH:  return httpd::Patch;
		case M_HEAD:   return httpd::Head;
		default:       return httpd::UnknownMethod;
	}
}

/* One value out of the raw query, decoded the way nhttpd decodes its own
   parameter list. Read here rather than off hh->ParamList so the bridge
   holds one reading of the request it was handed, target and all. */
std::string queryValue(const std::string &query, const char *name)
{
	const size_t nlen = strlen(name);
	size_t at = 0;
	while (at < query.size())
	{
		const size_t stop_at = query.find('&', at);
		const size_t stop = (stop_at == std::string::npos) ? query.size() : stop_at;
		const size_t eq = query.find('=', at);
		if (eq != std::string::npos && eq < stop && eq - at == nlen &&
		    query.compare(at, nlen, name) == 0)
			return decodeString(query.substr(eq + 1, stop - eq - 1));
		at = stop + 1;
	}
	return std::string();
}

/* The credentials off an nhttpd request: credentialsOf as compatible
   forks carry it in their server, with the MHD lookups replaced by
   nhttpd's own lists. The basic member is read whether or not the header carries the Bearer
   scheme, exactly as upstream does, and granted() never looks at it. */
httpd::Credentials credentialsOf(CyhookHandler *hh, const std::string &query)
{
	httpd::Credentials c;
	c.peer = hh->UrlData["clientaddr"];

	const std::string cookies = header_value(hh->HeaderList, "Cookie");
	if (!cookies.empty())
		c.cookie_token = httpd::cookieValue(cookies, httpd::sessionCookieName());

	const std::string authz = header_value(hh->HeaderList, "Authorization");
	if (!authz.empty())
	{
		c.bearer_token = httpd::bearerToken(authz);
		c.basic = httpd::basicCredential(authz);
	}

	c.csrf_header = header_value(hh->HeaderList, httpd::csrfHeaderName());
	c.query_token = queryValue(query, httpd::queryTokenName());
	c.forwarded_for = header_value(hh->HeaderList, "X-Forwarded-For");
	return c;
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

/* The answer as it leaves, for the suite's record of what each route
   answered, as upstream's server notes it after sending: the router notes
   what a route said, and this adds what only the door gives - the login
   gate's refusals, a body over the limit, and a code the range handling or
   the transport turned the route's answer into. Only for those, so the
   ordinary answer is not copied; one noted twice counts once. */
void noteSent(const CyhookHandler *hh, httpd::Method m, const std::string &path)
{
	httpd::Response sent;
	sent.code = (int) hh->httpStatus;
	sent.content_type = hh->ResponseMimeType;
	sent.body = hh->yresult;
	httpd::noteAnswer(m, path, sent);
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

void CApiBridge::startStreams()
{
	try
	{
		httpd::carry::endStop();
		httpd::events::start();
	}
	catch (...)
	{
		// the stream service stays down and refuses streams; the rest of
		// the layer serves as before
		std::fprintf(stderr, "httpd: the event stream did not start\n");
	}
}

void CApiBridge::stopStreams()
{
	try
	{
		// first, so nothing new is admitted while the rest stops, and every
		// carried answer starts ending while the event streams are waited for
		httpd::carry::beginStop();
		try
		{
			httpd::events::stop();
		}
		catch (...)
		{
			std::fprintf(stderr, "httpd: the event stream did not stop cleanly\n");
		}
		// bounded, unlike the wait above: a carried answer holds no hook
		httpd::carry::waitIdle(httpd::carry::stopBoundMs());
	}
	catch (...)
	{
		std::fprintf(stderr, "httpd: the carried answers did not stop cleanly\n");
	}
}

void CApiBridge::registerPrefixes()
{
	CyhookHandler::registerApiPrefix(kPrefixApi);
	CyhookHandler::registerApiTarget(kPrefixDoc);
	// a caller signed in with a shipped or a first password sets its own before anything else
	httpd::setPasswordOwed(&login_bridge::passwordOwed);
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
			            httpd::tooLargeDetail());
			noteSent(hh, m, path);
			return hh->status;
		}

		/* The route first, then the caller, in upstream serve()'s order
		   (server.cpp): the route declares which credentials count, so the
		   request cannot be judged before it is known what it would reach.
		   nhttpd hands the request over whole, so the gate is asked once
		   where upstream's two-phase daemon asks it twice; the router asks
		   the level half again at dispatch, through the same sentence, so
		   the two refusals cannot be told apart. */
		httpd::AuthLevel need = httpd::AuthLevel::Public;
		bool query_token_ok = false;
		const bool routed = httpd::routeLevelFor(m, path, &need, &query_token_ok);

		const httpd::Credentials cred = credentialsOf(hh, query);
		std::string scope;
		const httpd::AuthLevel have = httpd::granted(cred, query_token_ok, &scope);

		if (routed)
		{
			httpd::Response refusal;
			if (!httpd::allowed(need, have, m, cred, &refusal))
			{
				httpd::addApiHeaders(refusal);
				answerWith(hh, refusal);
				noteSent(hh, m, path);
				return hh->status;
			}
		}

		httpd::Response r = httpd::dispatch(m, path, query, hh->RawBody,
		                                    cred.peer, have,
		                                    httpd::clientAddress(cred), cred.cookie_token,
		                                    header_value(hh->HeaderList, "Host"), scope,
		                                    hh->UrlData["localaddr"]);

		if ((m == httpd::Get || m == httpd::Head) && httpd::openapi::isDocument(r))
			r = httpd::openapi::withEncoding(r, header_value(hh->HeaderList, "Accept-Encoding"));

		answerWith(hh, r);
		// the router noted what the route said; this is what the door made of it
		if ((int) hh->httpStatus != r.code)
			noteSent(hh, m, path);
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

namespace
{

// The pump's way onto the wire: every block whole, or the stream ends.
bool sendBlock(void *ctx, const char *bytes, size_t n)
{
	return ((CySocket *) ctx)->SendAll(bytes, n);
}

/* A registration between opening and spending. Whatever path leaves the
   stream hook - a refused header send, a throw out of a string - gives the
   seat back exactly once; spending it hands that duty to pumpStream. */
struct HeldStream
{
	httpd::events::StreamHandle h;
	explicit HeldStream(httpd::events::StreamHandle x) : h(x) {}
	~HeldStream()
	{
		if (h != NULL)
			httpd::events::closeStream(h);
	}
	httpd::events::StreamHandle spend()
	{
		httpd::events::StreamHandle out = h;
		h = NULL;
		return out;
	}
private:
	HeldStream(const HeldStream &);
	HeldStream &operator=(const HeldStream &);
};

/* The stream pass of a carried answer: the answer comes off the handler
   and goes to its job, which runs on a thread of its own on a copy of this
   socket, so this connection thread - the accept thread itself without
   connection threads - is free again at once. A refusal comes back while
   nothing has been written and becomes an ordinary answer out of the
   handler. */
THandleStatus carryAnswer(CyhookHandler *hh, CySocket *sock)
{
	const httpd::Method m = methodOf(hh->Method);
	try
	{
		// Read while the handler still owns the answer: a throw here leaves
		// it to the handler's own release.
		const std::string target = hh->UrlData["target"];
		const std::string path = target.substr(0, target.find('?'));

		// Taken exactly once. From here start() owns it on every path,
		// a throw out of it included.
		void *held = hh->Carried;
		hh->Carried = NULL;
		hh->CarriedRelease = NULL;

		httpd::Response refusal;
		if (httpd::carry::start(held, (int) sock->get_socket(), m, path, refusal)
				== httpd::carry::Carrying)
			return HANDLED_STREAM;

		httpd::addApiHeaders(refusal);
		hh->api_mode = true;
		hh->httpStatus = (HttpResponseType) refusal.code;
		hh->ResponseMimeType = refusal.content_type;
		hh->ResponseHeaders = refusal.headers;
		hh->yresult = refusal.body;
		hh->LengthUnknown = false;
		hh->status = HANDLED_READY;
		noteSent(hh, m, path);
		return HANDLED_READY;
	}
	catch (const std::exception &e)
	{
		log_level_printf(1, "api bridge: %s\n", e.what());
	}
	catch (...)
	{
	}
	// nothing was written: the fixed document still fits, where it can be had
	takeFixedFailure(hh);
	return (hh->status == HANDLED_READY) ? HANDLED_READY : HANDLED_STREAM;
}

} // namespace

THandleStatus CApiBridge::Hook_SendStream(CyhookHandler *hh, CySocket *sock)
{
	if (!claims(hh))
		return HANDLED_NONE;

	try
	{
		if (hh->Carried != NULL)
			return carryAnswer(hh, sock);

		/* Registered before the header goes out: the seat is counted and the
		   opening frames are seeded first, so an event published while the
		   header travels is waiting in the stream, not lost - and a refusal
		   happens while nothing has been written, so it can still be an
		   ordinary answer. */
		httpd::events::StreamHandle handle = NULL;
		httpd::Response refusal;
		const httpd::events::Opened got =
			httpd::events::openStream((int) sock->get_socket(), handle, refusal);

		// The two refusals of this pass are the door's own, noted as the send
		// pass notes its own: the route said stream, and this is what it got.
		const std::string target = hh->UrlData["target"];
		const std::string path = target.substr(0, target.find('?'));
		if (got == httpd::events::StreamRefused)
		{
			httpd::addApiHeaders(refusal);
			hh->api_mode = true;
			hh->httpStatus = (HttpResponseType) refusal.code;
			hh->ResponseMimeType = refusal.content_type;
			hh->ResponseHeaders = refusal.headers;
			hh->yresult = refusal.body;
			noteSent(hh, methodOf(hh->Method), path);
			return HANDLED_READY;
		}
		if (got != httpd::events::StreamOpened)
		{
			takeProblem(hh, 500, coreapi::ErrorCode::BoxUnreadable,
			            "the stream could not be opened");
			noteSent(hh, methodOf(hh->Method), path);
			return HANDLED_READY;
		}

		HeldStream held(handle);
		const std::string header = hh->BuildHeader();
		if (!sock->SendAll(header.data(), header.size()))
			// held gives the seat back; bytes may be on the wire, so the
			// connection is spent and no other answer may follow
			return HANDLED_STREAM;

		httpd::events::pumpStream(held.spend(), &sendBlock, sock);
		return HANDLED_STREAM;
	}
	catch (const std::exception &e)
	{
		log_level_printf(1, "api bridge: %s\n", e.what());
	}
	catch (...)
	{
	}
	/* A throw lands here only out of the header build or the bookkeeping
	   around it; the guard above has given the seat back. Whether a byte
	   made it out cannot be known cheaply, so the connection is treated as
	   spent rather than answered twice. */
	return HANDLED_STREAM;
}

void CApiBridge::answerWith(CyhookHandler *hh, const httpd::Response &r)
{
	/* The reload path rides on the handler for the transport: the restart
	   happens after the answer's bytes are gone (yresponse.cpp), never in
	   the handler's own frame. */
	hh->ReloadAfter = r.reload_after;

	HeldDescriptor held(r.fd);
	const int fd = held.fd;

	httpd::carry::Kind kind = httpd::carry::Fetch;
	if (httpd::carry::carried(r, &kind))
	{
		/* A program's output or a relayed address: an answer that runs as
		   long as somebody listens, carried on a thread of its own whatever
		   the threading setting says (carry.h). A HEAD reports what the GET
		   would meet - a missing carrier, a server that is stopping, seats
		   that are taken, the carrier's own check - and starts nothing. The
		   head it answers with states no length: the GET's states none
		   unless its carrier learns one on the way. */
		if (hh->Method == M_HEAD)
		{
			httpd::Response refusal;
			if (!httpd::carry::admissible(r, refusal))
			{
				httpd::addApiHeaders(refusal);
				hh->api_mode = true;
				hh->httpStatus = (HttpResponseType) refusal.code;
				hh->ResponseMimeType = refusal.content_type;
				hh->ResponseHeaders = refusal.headers;
				hh->yresult = refusal.body;
				hh->status = HANDLED_READY;
				return;
			}
			hh->api_mode = true;
			hh->httpStatus = (HttpResponseType) r.code;
			hh->ResponseMimeType = r.content_type;
			hh->ResponseHeaders = r.headers;
			hh->yresult.clear();
			hh->LengthUnknown = true;
			hh->status = HANDLED_READY;
			return;
		}
		if (!httpd::carry::hasCarrier(kind))
		{
			takeProblem(hh, 501, coreapi::ErrorCode::NoSuchMethod,
			            "this server does not carry an answer of this kind");
			return;
		}
		/* Parked on the handler for the stream pass, which hands it to its
		   job; the handler releases it if the exchange ends first. */
		hh->api_mode = true;
		hh->httpStatus = (HttpResponseType) r.code;
		hh->ResponseMimeType = r.content_type;
		hh->ResponseHeaders = r.headers;
		hh->yresult.clear();
		hh->dropCarried();
		hh->Carried = httpd::carry::hold(r);
		hh->CarriedRelease = &httpd::carry::release;
		hh->status = HANDLED_STREAM;
		return;
	}

	if (httpd::events::isStream(r))
	{
		/* Without connection threads, WebThread runs on the accept thread
		   itself, and one healthy stream would be the last request the box
		   ever takes. Refused before a seat exists, as a finite answer. An
		   event stream runs on its connection's thread; only a carried
		   answer has a thread of its own. */
		if (hh->Method != M_HEAD && !CWebserver::is_threading)
		{
			takeProblem(hh, 501, coreapi::ErrorCode::NoSuchMethod,
			            "this server carries no event stream without connection threads");
			return;
		}

		/* The handler said what the answer is; the connection becomes it in
		   the stream pass. The headers travel now, before the status is
		   given back, because the pass writes the header out of this handler
		   and nothing after this sees the Response again. HEAD stays a
		   finite answer: the stream's head, no body and, like the stream,
		   no length - and no stream is opened for it. */
		hh->api_mode = true;
		hh->httpStatus = (HttpResponseType) r.code;
		hh->ResponseMimeType = r.content_type;
		hh->ResponseHeaders = r.headers;
		hh->yresult.clear();
		/* Only the head says so: a stream that is then refused becomes a
		   finite answer that states its length. */
		hh->LengthUnknown = (hh->Method == M_HEAD);
		hh->status = (hh->Method == M_HEAD) ? HANDLED_READY : HANDLED_STREAM;
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
		/* Said on every file answer: a media player that is never told
		   offers no seeking, even though a range it asked for would be
		   answered. */
		const std::pair<std::string, std::string> ranges("Accept-Ranges", "bytes");
		if (plan.code == 416)
		{
			// the problem document is the body; held closes the file
			takeProblem(hh, 416, coreapi::ErrorCode::RangeOutsideFile,
			            httpd::rangeRefusedDetail());
			hh->ResponseHeaders.push_back(std::make_pair(
					std::string("Content-Range"), plan.content_range));
			hh->ResponseHeaders.push_back(ranges);
			return;
		}
		hh->ResponseHeaders.push_back(ranges);
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
