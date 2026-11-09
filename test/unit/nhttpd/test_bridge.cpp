/*
 * test_bridge.cpp - the hook that hands nhttpd's requests to the REST layer
 *
 * Copyright (C) 2026 Thilo Graf
 *
 * License: GPL-2.0-or-later
 *
 * The bridge is driven the way nhttpd drives it: a CyhookHandler filled the
 * way session_init fills it, Hook_PrepareResponse to claim, Hook_SendResponse
 * to answer. What is checked is the translation on both sides and the cases
 * the router decides on its own (404, 405, 400, 501), the document route with
 * and without gzip, the refusals the bridge makes itself (413, a carried
 * answer of a kind nothing carries), and how a carried answer is handed to
 * the stream pass and released. Routes that reach a box backend are only
 * checked for reaching the layer, not for what the box says.
 */

#include "support/catch.hpp"

#include "httpd/nhttpd_bridge.h"
#include "httpd/auth.h"
#include "httpd/carry.h"
#include "httpd/credentials.h"
#include "httpd/endpoints.h"
#include "httpd/router.h"
#include "coreapi/storage.h"
#include "coreapi/box/storage_internal.h"
#include "coreapi/system.h"
#include "httpd/doc/openapi.h"
#include "httpd/http.h"
#include "httpd/status.h"
#include "coreapi/base/errors.h"

#include "yhook.h"
#include "yprotect.h"
#include "ysocket.h"
#include "ytypes_globals.h"
#include "ywebserver.h"

#include "support/fakes.h"

#include <cstdlib>
#include <cstring>
#include <string>

#include <fcntl.h>
#include <strings.h>
#include <sys/socket.h>
#include <unistd.h>
#include <zlib.h>

namespace {

struct Handler
{
	CyhookHandler hh;

	Handler(THttp_Method method, const std::string &target, const std::string &body = "")
	{
		CStringList params, url, headers, config;
		url["target"] = target;
		url["url"] = target.substr(0, target.find('?'));
		url["clientaddr"] = "192.168.1.20";
		headers["Host"] = "box.lan";
		hh.session_init(params, url, headers, config, method, false);
		hh.RawBody = body;
	}
};

size_t countOf(const std::string &hay, const std::string &needle)
{
	size_t n = 0;
	for (size_t at = hay.find(needle); at != std::string::npos; at = hay.find(needle, at + needle.size()))
		n++;
	return n;
}

std::string headerOf(const CyhookHandler &hh, const char *name)
{
	for (size_t i = 0; i < hh.ResponseHeaders.size(); i++)
		if (strcasecmp(hh.ResponseHeaders[i].first.c_str(), name) == 0)
			return hh.ResponseHeaders[i].second;
	return "";
}

std::string gunzip(const std::string &in)
{
	z_stream z;
	std::memset(&z, 0, sizeof(z));
	if (inflateInit2(&z, 16 + MAX_WBITS) != Z_OK)
		return "";
	std::string out;
	char buf[65536];
	z.next_in = (Bytef *) in.data();
	z.avail_in = (uInt) in.size();
	int rc;
	do
	{
		z.next_out = (Bytef *) buf;
		z.avail_out = sizeof(buf);
		rc = inflate(&z, Z_NO_FLUSH);
		if (rc != Z_OK && rc != Z_STREAM_END)
			break;
		out.append(buf, sizeof(buf) - z.avail_out);
	} while (rc != Z_STREAM_END);
	inflateEnd(&z);
	return out;
}

struct Attached
{
	CApiBridge bridge;
	Attached()
	{
		CyhookHandler::clearApiPrefixes();
		CApiBridge::registerPrefixes();
	}
	~Attached()
	{
		CyhookHandler::clearApiPrefixes();
		// registerPrefixes installs the first-login gate, which the other cases do not expect
		httpd::setPasswordOwed(NULL);
	}
};

} // namespace

TEST_CASE("the bridge claims its namespace in prepare and nothing else", "[bridge]")
{
	Attached a;

	Handler mine(M_GET, "/api/v1/system/info");
	CHECK(a.bridge.Hook_PrepareResponse(&mine.hh) == HANDLED_READY);

	Handler doc(M_GET, "/api/openapi.json");
	CHECK(a.bridge.Hook_PrepareResponse(&doc.hh) == HANDLED_READY);

	Handler legacy(M_GET, "/control/getversion");
	CHECK(a.bridge.Hook_PrepareResponse(&legacy.hh) == HANDLED_NONE);
	CHECK(a.bridge.Hook_SendResponse(&legacy.hh) == HANDLED_NONE);
	CHECK(legacy.hh.yresult.empty());

	Handler v4(M_GET, "/api/v4/bouquets");
	CHECK(a.bridge.Hook_PrepareResponse(&v4.hh) == HANDLED_NONE);

	Handler page(M_GET, "/api/v1.html");
	CHECK(a.bridge.Hook_PrepareResponse(&page.hh) == HANDLED_NONE);
}

TEST_CASE("an unknown path is a problem document with the router's headers", "[bridge]")
{
	Attached a;
	Handler h(M_GET, "/api/v1/nothing/here");

	REQUIRE(a.bridge.Hook_SendResponse(&h.hh) == HANDLED_READY);
	CHECK(h.hh.api_mode);
	CHECK(h.hh.httpStatus == HTTP_NOT_FOUND);
	CHECK(h.hh.ResponseMimeType == "application/problem+json");
	CHECK(h.hh.yresult.find("\"status\":404") != std::string::npos);
	CHECK(h.hh.yresult.find("\"type\":\"/errors/") != std::string::npos);
	CHECK(headerOf(h.hh, "Cache-Control") == "no-store");
	CHECK(headerOf(h.hh, "X-Content-Type-Options") == "nosniff");

	const std::string header = h.hh.BuildHeader();
	CHECK(countOf(header, "Content-Length: " + std::to_string(h.hh.yresult.size()) + "\r\n") == 1);
	CHECK(countOf(header, "Cache-Control: ") == 1);
}

TEST_CASE("a method the path does not answer is 405 with Allow", "[bridge]")
{
	Attached a;
	Handler h(M_DELETE, "/api/v1/system/info");

	REQUIRE(a.bridge.Hook_SendResponse(&h.hh) == HANDLED_READY);
	CHECK(h.hh.httpStatus == HTTP_METHOD_NOT_ALLOWED);
	const std::string allow = headerOf(h.hh, "Allow");
	CHECK(allow.find("GET") != std::string::npos);
	CHECK(allow.find("HEAD") != std::string::npos);
}

TEST_CASE("HEAD is answered like GET and the transport drops the body", "[bridge]")
{
	Attached a;
	Handler h(M_HEAD, "/api/openapi.json");

	REQUIRE(a.bridge.Hook_SendResponse(&h.hh) == HANDLED_READY);
	CHECK(h.hh.httpStatus == HTTP_OK);
	CHECK(h.hh.yresult.size() > 1000);
	CHECK(h.hh.Method == M_HEAD);
}

TEST_CASE("the document route answers the bytes the dump tool writes, gzipped on request", "[bridge]")
{
	Attached a;

	Handler plain(M_GET, "/api/openapi.json");
	REQUIRE(a.bridge.Hook_SendResponse(&plain.hh) == HANDLED_READY);
	CHECK(plain.hh.httpStatus == HTTP_OK);
	CHECK(plain.hh.ResponseMimeType.find("application/") == 0);
	CHECK(plain.hh.yresult == httpd::openapi::document());
	CHECK(headerOf(plain.hh, "Content-Encoding").empty());

	Handler zipped(M_GET, "/api/openapi.json");
	zipped.hh.HeaderList["accept-encoding"] = "deflate, gzip;q=0.8";
	REQUIRE(a.bridge.Hook_SendResponse(&zipped.hh) == HANDLED_READY);
	CHECK(zipped.hh.httpStatus == HTTP_OK);
	CHECK(headerOf(zipped.hh, "Content-Encoding") == "gzip");
	CHECK(zipped.hh.yresult.size() < plain.hh.yresult.size());
	CHECK(zipped.hh.yresult.find('\0') != std::string::npos);
	CHECK(gunzip(zipped.hh.yresult) == plain.hh.yresult);

	Handler refused(M_GET, "/api/openapi.json");
	refused.hh.HeaderList["Accept-Encoding"] = "gzip;q=0";
	REQUIRE(a.bridge.Hook_SendResponse(&refused.hh) == HANDLED_READY);
	CHECK(headerOf(refused.hh, "Content-Encoding").empty());
}

TEST_CASE("the raw query reaches the router undecoded", "[bridge]")
{
	Attached a;
	Handler h(M_GET, "/api/v1/channels?x=%ZZ");

	REQUIRE(a.bridge.Hook_SendResponse(&h.hh) == HANDLED_READY);
	CHECK(h.hh.httpStatus == HTTP_BAD_REQUEST);
	CHECK(h.hh.yresult.find("escape") != std::string::npos);
}

TEST_CASE("a body the server refused unread is answered with 413", "[bridge]")
{
	Attached a;
	Handler h(M_POST, "/api/v1/system/standby");
	h.hh.RawBodyTooLarge = true;

	REQUIRE(a.bridge.Hook_SendResponse(&h.hh) == HANDLED_READY);
	CHECK(h.hh.httpStatus == HTTP_REQUEST_ENTITY_TOO_LARGE);
	CHECK(h.hh.ResponseMimeType == "application/problem+json");
	CHECK(h.hh.yresult.find("\"status\":413") != std::string::npos);
}

TEST_CASE("a JSON body reaches the router and a malformed one is refused there", "[bridge]")
{
	Attached a;

	// A write needs a credential since the gate landed; what this case is
	// about is the router's reading of the body behind it.
	const std::string token = "00112233445566778899aabbccddeeff";
	httpd::addApiToken(httpd::tokenLookupPrefix(token), httpd::hashSecret(token),
	                   httpd::AuthLevel::System, "", 0);
	const std::string bearer = "Bearer " + token;

	Handler bad(M_POST, "/api/v1/system/standby", "{\"on\": [1,2]}");
	bad.hh.HeaderList["Authorization"] = bearer;
	REQUIRE(a.bridge.Hook_SendResponse(&bad.hh) == HANDLED_READY);
	CHECK(bad.hh.httpStatus == HTTP_BAD_REQUEST);

	Handler missing(M_POST, "/api/v1/system/standby", "{}");
	missing.hh.HeaderList["Authorization"] = bearer;
	REQUIRE(a.bridge.Hook_SendResponse(&missing.hh) == HANDLED_READY);
	CHECK(missing.hh.httpStatus == HTTP_BAD_REQUEST);
	CHECK(missing.hh.yresult.find("\"status\":400") != std::string::npos);

	// PUT is dispatched and the path is matched: the router refuses the empty
	// body before any handler runs, so no box backend is needed here.
	Handler put(M_PUT, "/api/v1/bouquets/none-such/name", "{}");
	put.hh.HeaderList["Authorization"] = bearer;
	REQUIRE(a.bridge.Hook_SendResponse(&put.hh) == HANDLED_READY);
	CHECK(put.hh.httpStatus == HTTP_BAD_REQUEST);

	httpd::forgetApiTokens();
}

TEST_CASE("the webserver table answers the port the bridge said", "[bridge]")
{
	Attached a;

	const std::string token = "abcdef0123456789abcdef0123456789";
	httpd::addApiToken(httpd::tokenLookupPrefix(token), httpd::hashSecret(token),
	                   httpd::AuthLevel::System, "", 0);

	httpd::setBoundPortFromBridge(31999);
	Handler h(M_GET, "/api/v1/system/webserver");
	h.hh.HeaderList["Authorization"] = "Bearer " + token;
	REQUIRE(a.bridge.Hook_SendResponse(&h.hh) == HANDLED_READY);
	CHECK(h.hh.httpStatus == HTTP_OK);
	CHECK(h.hh.yresult.find("\"bound_port\":31999") != std::string::npos);
	CHECK(h.hh.yresult.find("\"has_password\"") != std::string::npos);
	// never the hash, never a password member
	CHECK(h.hh.yresult.find("password_hash") == std::string::npos);

	httpd::setBoundPortFromBridge(0);
	httpd::forgetApiTokens();
}

TEST_CASE("a reload answer is carried to the transport, not done in the handler", "[bridge]")
{
	Attached a;

	httpd::Response r;
	r.code = 200;
	r.content_type = "application/json";
	r.body = "{}";
	r.reload_after = "/somewhere/nhttpd-access.conf";

	Handler h(M_PUT, "/api/v1/system/webserver");
	CApiBridge::answerWith(&h.hh, r);
	CHECK(h.hh.httpStatus == HTTP_OK);
	// The path rides on the handler for whoever sends the answer: the
	// restart happens after the bytes are gone, never inside the handler.
	CHECK(h.hh.ReloadAfter == "/somewhere/nhttpd-access.conf");
}

TEST_CASE("PATCH is a method the bridge can name", "[bridge]")
{
	Attached a;

	// Wrong path on purpose: the router's 404 proves the method arrived as
	// itself, where an unnamed method would have been the 501 below. The
	// one PATCH route this tree serves writes settings and needs the box;
	// the wire check drives it for real (check-v1.sh section 17).
	Handler h(M_PATCH, "/api/v1/no-such-thing", "{}");
	REQUIRE(a.bridge.Hook_SendResponse(&h.hh) == HANDLED_READY);
	CHECK(h.hh.httpStatus == HTTP_NOT_FOUND);
}

TEST_CASE("a method nhttpd cannot name is 501 from the router", "[bridge]")
{
	Attached a;
	Handler h(M_TRACE, "/api/v1/system/info");

	REQUIRE(a.bridge.Hook_SendResponse(&h.hh) == HANDLED_READY);
	CHECK(h.hh.httpStatus == HTTP_NOT_IMPLEMENTED);
	CHECK(h.hh.ResponseMimeType == "application/problem+json");
}

namespace
{

// A carrier that writes its header and is done, for the cases that only
// need a job to start and end.
void headOnly(httpd::carry::Job &j)
{
	httpd::carry::sendHead(j, 0);
}

bool refuseAll(const httpd::Response &, httpd::Response &refusal)
{
	refusal = httpd::problemResponse(httpd::StatusServiceUnavailable,
	                                 coreapi::ErrorCode::TooManyConversions,
	                                 "the case's check refuses");
	return false;
}

const httpd::carry::Carrier kHeadOnly = { NULL, &headOnly };
const httpd::carry::Carrier kRefusing = { &refuseAll, &headOnly };

struct Carrying
{
	size_t held_before;
	explicit Carrying(const httpd::carry::Carrier *c = &kHeadOnly)
		: held_before(httpd::carry::heldAnswersForTest())
	{
		httpd::carry::setCarrier(httpd::carry::Fetch, c);
		httpd::carry::setCarrier(httpd::carry::Program, c);
	}
	~Carrying()
	{
		httpd::carry::waitIdle(5000);
		httpd::carry::setCarrier(httpd::carry::Fetch, NULL);
		httpd::carry::setCarrier(httpd::carry::Program, NULL);
		httpd::carry::endStop();
	}
};

/* A path no route answers: these cases hand the bridge answers of their own,
   and what it does with them is noted against no route the box serves. */
const char kNoRoute[] = "/api/v1/carried/1";

httpd::Response programAnswer()
{
	httpd::Response r;
	r.code = 200;
	r.content_type = "audio/aac";
	r.stream_argv.push_back("/bin/cat");
	return r;
}

httpd::Response relayAnswer()
{
	httpd::Response r;
	r.code = 200;
	r.content_type = "video/mp2t";
	r.relay_url = "http://192.0.2.1/stream.ts";
	return r;
}

} // namespace

TEST_CASE("a program or an address with nothing to carry it is refused as 501", "[bridge]")
{
	Attached a;
	const size_t held = httpd::carry::heldAnswersForTest();
	const httpd::Response answers[] = { programAnswer(), relayAnswer() };
	for (size_t i = 0; i < 2; ++i)
	{
		Handler h(M_GET, kNoRoute);
		CApiBridge::answerWith(&h.hh, answers[i]);
		CHECK(h.hh.httpStatus == HTTP_NOT_IMPLEMENTED);
		CHECK(h.hh.status == HANDLED_READY);
		CHECK(h.hh.Carried == NULL);
		CHECK(h.hh.yresult.find("\"status\":501") != std::string::npos);
	}
	CHECK(httpd::carry::heldAnswersForTest() == held);
}

TEST_CASE("a program or an address is handed to the stream pass with the answer", "[bridge]")
{
	Attached a;
	Carrying c;
	const httpd::Response answers[] = { programAnswer(), relayAnswer() };
	for (size_t i = 0; i < 2; ++i)
	{
		Handler h(M_GET, kNoRoute);
		CApiBridge::answerWith(&h.hh, answers[i]);
		CHECK(h.hh.status == HANDLED_STREAM);
		CHECK(h.hh.httpStatus == HTTP_OK);
		CHECK(h.hh.ResponseMimeType == answers[i].content_type);
		REQUIRE(h.hh.Carried != NULL);
		CHECK(h.hh.CarriedRelease == &httpd::carry::release);
		const httpd::Response *held = (const httpd::Response *) h.hh.Carried;
		CHECK(held->stream_argv == answers[i].stream_argv);
		CHECK(held->relay_url == answers[i].relay_url);
		CHECK(httpd::carry::heldAnswersForTest() == c.held_before + 1);
	}
	// each handler gave its answer back when it went
	CHECK(httpd::carry::heldAnswersForTest() == c.held_before);
	CHECK(httpd::carry::running() == 0);
}

TEST_CASE("a HEAD for a carried answer is finite and starts nothing", "[bridge]")
{
	Attached a;
	Carrying c;
	Handler h(M_HEAD, kNoRoute);
	CApiBridge::answerWith(&h.hh, programAnswer());
	CHECK(h.hh.status == HANDLED_READY);
	CHECK(h.hh.httpStatus == HTTP_OK);
	CHECK(h.hh.ResponseMimeType == "audio/aac");
	CHECK(h.hh.Carried == NULL);
	CHECK(h.hh.yresult.empty());
	// no length: the GET it stands for states none
	CHECK(h.hh.BuildHeader().find("Content-Length:") == std::string::npos);
	CHECK(httpd::carry::running() == 0);
	CHECK(httpd::carry::seatsTaken(httpd::carry::Program) == 0);
	CHECK(httpd::carry::heldAnswersForTest() == c.held_before);
}

TEST_CASE("a HEAD reports the refusal the GET would meet", "[bridge]")
{
	Attached a;
	SECTION("nothing carries the kind")
	{
		Handler h(M_HEAD, kNoRoute);
		CApiBridge::answerWith(&h.hh, relayAnswer());
		CHECK(h.hh.httpStatus == HTTP_NOT_IMPLEMENTED);
		CHECK(h.hh.status == HANDLED_READY);
	}
	SECTION("the server is stopping")
	{
		Carrying c;
		httpd::carry::beginStop();
		Handler h(M_HEAD, kNoRoute);
		CApiBridge::answerWith(&h.hh, relayAnswer());
		CHECK(h.hh.httpStatus == HTTP_SERVICE_UNAVAILABLE);
		CHECK(h.hh.yresult.find("server-stopping") != std::string::npos);
	}
	SECTION("the carrier's own check refuses")
	{
		Carrying c(&kRefusing);
		Handler h(M_HEAD, kNoRoute);
		CApiBridge::answerWith(&h.hh, programAnswer());
		CHECK(h.hh.httpStatus == HTTP_SERVICE_UNAVAILABLE);
		CHECK(h.hh.yresult.find("the case's check refuses") != std::string::npos);
	}
	CHECK(httpd::carry::running() == 0);
}

TEST_CASE("a program or an address under another code is an ordinary answer", "[bridge]")
{
	Attached a;
	Carrying c;
	httpd::Response r = programAnswer();
	r.code = 404;
	r.content_type = "application/problem+json";
	r.body = "{\"status\":404}";
	Handler h(M_GET, kNoRoute);
	CApiBridge::answerWith(&h.hh, r);
	CHECK(h.hh.status == HANDLED_READY);
	CHECK(h.hh.httpStatus == HTTP_NOT_FOUND);
	CHECK(h.hh.Carried == NULL);
	CHECK(h.hh.yresult == r.body);
}

TEST_CASE("a carried answer is given back exactly once on every way", "[bridge]")
{
	Attached a;
	Carrying c;
	int sp[2];
	REQUIRE(socketpair(AF_UNIX, SOCK_STREAM, 0, sp) == 0);
	CySocket sock(sp[0]);
	sock.isOpened = true;
	sock.isValid = true;

	SECTION("a new exchange on the same handler")
	{
		Handler h(M_GET, kNoRoute);
		CApiBridge::answerWith(&h.hh, programAnswer());
		REQUIRE(httpd::carry::heldAnswersForTest() == c.held_before + 1);
		CStringList none, config;
		h.hh.session_init(none, none, none, config, M_GET, false);
		CHECK(h.hh.Carried == NULL);
		CHECK(httpd::carry::heldAnswersForTest() == c.held_before);
	}
	SECTION("the handler goes")
	{
		{
			Handler h(M_GET, kNoRoute);
			CApiBridge::answerWith(&h.hh, programAnswer());
			REQUIRE(httpd::carry::heldAnswersForTest() == c.held_before + 1);
		}
		CHECK(httpd::carry::heldAnswersForTest() == c.held_before);
	}
	SECTION("the stream pass refuses")
	{
		Handler h(M_GET, kNoRoute);
		CApiBridge::answerWith(&h.hh, programAnswer());
		httpd::carry::beginStop();
		CHECK(a.bridge.Hook_SendStream(&h.hh, &sock) == HANDLED_READY);
		CHECK(h.hh.Carried == NULL);
		CHECK(h.hh.httpStatus == HTTP_SERVICE_UNAVAILABLE);
		CHECK(h.hh.yresult.find("server-stopping") != std::string::npos);
		CHECK(httpd::carry::heldAnswersForTest() == c.held_before);
		httpd::carry::endStop();
	}
	SECTION("the stream pass carries it")
	{
		Handler h(M_GET, kNoRoute);
		CApiBridge::answerWith(&h.hh, programAnswer());
		CHECK(a.bridge.Hook_SendStream(&h.hh, &sock) == HANDLED_STREAM);
		CHECK(h.hh.Carried == NULL);
		CHECK(httpd::carry::waitIdle(2000));
		CHECK(httpd::carry::heldAnswersForTest() == c.held_before);
		char buf[64];
		const ssize_t n = read(sp[1], buf, sizeof(buf));
		CHECK(n > 0);
		CHECK(std::string(buf, n > 0 ? (size_t) n : 0).find(" 200 ") != std::string::npos);
	}
	sock.close();
	close(sp[1]);
}

TEST_CASE("an event-stream answer is handed to the stream pass", "[bridge]")
{
	Attached a;
	httpd::Response sse;
	sse.code = 200;
	sse.content_type = "text/event-stream";
	sse.headers.push_back(std::make_pair(std::string("Cache-Control"),
	                                     std::string("no-cache, no-transform")));

	SECTION("GET carries the handler's headers and asks for the stream")
	{
		Handler e(M_GET, "/api/v1/events");
		CApiBridge::answerWith(&e.hh, sse);
		CHECK(e.hh.status == HANDLED_STREAM);
		CHECK(e.hh.httpStatus == HTTP_OK);
		CHECK(e.hh.ResponseMimeType == "text/event-stream");
		bool found = false;
		for (size_t i = 0; i < e.hh.ResponseHeaders.size(); i++)
			if (e.hh.ResponseHeaders[i].first == "Cache-Control"
					&& e.hh.ResponseHeaders[i].second == "no-cache, no-transform")
				found = true;
		CHECK(found);
		CHECK(e.hh.yresult.empty());
	}
	SECTION("HEAD is a finite answer with the stream's headers and no body")
	{
		Handler e(M_HEAD, "/api/v1/events");
		CApiBridge::answerWith(&e.hh, sse);
		CHECK(e.hh.status == HANDLED_READY);
		CHECK(e.hh.httpStatus == HTTP_OK);
		CHECK(e.hh.ResponseMimeType == "text/event-stream");
		CHECK(e.hh.yresult.empty());
		// No length: the GET it stands for has none, and a HEAD may only
		// state the length that GET would carry (RFC 9110 8.6).
		CHECK(e.hh.BuildHeader().find("Content-Length:") == std::string::npos);
	}
}

TEST_CASE("a file answer hands the descriptor to the transport", "[bridge]")
{
	Attached a;
	char name[] = "/tmp/test_bridge_XXXXXX";
	int fd = mkstemp(name);
	REQUIRE(fd >= 0);
	const char bytes[] = { 'P', 'N', 'G', '\0', '\r', '\n', 'x' };
	REQUIRE(write(fd, bytes, sizeof(bytes)) == (ssize_t) sizeof(bytes));
	lseek(fd, 0, SEEK_SET);

	httpd::Response r;
	REQUIRE(httpd::answerFromDescriptor(r, fd));
	r.code = 200;
	r.content_type = "image/png";

	Handler h(M_GET, "/api/v1/channels/1/logo");
	CApiBridge::answerWith(&h.hh, r);
	CHECK(h.hh.httpStatus == HTTP_OK);
	CHECK(h.hh.ResponseMimeType == "image/png");
	CHECK(h.hh.status == HANDLED_SENDFD);
	CHECK(h.hh.yresult.empty());
	REQUIRE(h.hh.SendFd == fd);
	CHECK(h.hh.SendFdOffset == 0);
	CHECK(h.hh.SendFdLength == (off_t) sizeof(bytes));
	// still open: the transport sends it and closes it
	CHECK(fcntl(fd, F_GETFD) != -1);
	close(h.hh.SendFd);
	h.hh.SendFd = -1;
	unlink(name);
}

namespace {

struct StoreBack
{
	const char *was;
	StoreBack() : was(coreapi::storage::internal::access_store_dir) {}
	~StoreBack()
	{
		coreapi::storage::internal::access_store_dir = was;
		yprotect::setKeptPlacesForTest(std::vector<std::string>());
		yprotect::setFiles(std::vector<std::string>());
	}
};

void putFile(const std::string &path, const char *text)
{
	FILE *f = fopen(path.c_str(), "w");
	REQUIRE(f != NULL);
	fputs(text, f);
	fclose(f);
}

// GET /api/v1/storage/file for path as a System caller gets it, through the bridge.
int storageAnswer(const std::string &path)
{
	httpd::Response r = httpd::dispatch(httpd::Get, "/api/v1/storage/file", "path=" + path, "",
	                                    "127.0.0.1", httpd::AuthLevel::System);
	Handler h(M_GET, "/api/v1/storage/file");
	CApiBridge::answerWith(&h.hh, r);
	if (h.hh.SendFd >= 0) {
		close(h.hh.SendFd);
		h.hh.SendFd = -1;
	}
	return h.hh.httpStatus;
}

} // namespace

/* The file manager downloads a file of the configuration directory as before; the
   access store, its working file and a second name of the store stay held back,
   by name in the storage layer and by inode at the bridge. */
TEST_CASE("the file manager reads the configuration directory but for the files that hold who may reach the box", "[bridge]")
{
	Attached a;
	StoreBack back;
	char dir[] = "/tmp/test_bridge_config_XXXXXX";
	REQUIRE(mkdtemp(dir) != NULL);
	const std::string config(dir);
	const std::string plain = config + "/neutrino.conf";
	const std::string store = config + "/nhttpd-access.conf";
	const std::string working = store + ".new";
	const std::string alias = config + "/copy.conf";
	putFile(plain, "volume=7\n");
	putFile(store, "password_hash=S3NT1NEL\n");
	putFile(working, "password_hash=S3NT1NEL\n");
	REQUIRE(link(store.c_str(), alias.c_str()) == 0);
	coreapi::storage::internal::access_store_dir = dir;
	std::vector<std::string> kept;
	kept.push_back(config);
	kept.push_back("/proc");
	kept.push_back("/sys");
	kept.push_back("/dev");
	yprotect::setKeptPlacesForTest(kept);
	std::vector<std::string> names;
	names.push_back(store);
	names.push_back(working);
	yprotect::setFiles(names);

	CHECK(storageAnswer(plain) == HTTP_OK);
	CHECK(storageAnswer(store) == HTTP_BAD_REQUEST);
	CHECK(storageAnswer(working) == HTTP_BAD_REQUEST);
	CHECK(storageAnswer(alias) == HTTP_NOT_FOUND);

	// a file of the kernel's trees goes out of no answer
	const int proc = open("/proc/self/status", O_RDONLY | O_CLOEXEC);
	REQUIRE(proc >= 0);
	CHECK(nhttpd_bridge::heldBack(proc, false));
	close(proc);
	const int fd = open(plain.c_str(), O_RDONLY | O_CLOEXEC);
	REQUIRE(fd >= 0);
	CHECK_FALSE(nhttpd_bridge::heldBack(fd, false));
	close(fd);

	unlink(alias.c_str());
	unlink(working.c_str());
	unlink(store.c_str());
	unlink(plain.c_str());
	rmdir(dir);
}

namespace {
// what the cases below set, set back however they end
struct Unprotected
{
	~Unprotected() { yprotect::setFiles(std::vector<std::string>()); }
};
} // namespace

TEST_CASE("a file answer that holds who may reach the box is a 404 and the file is closed", "[bridge]")
{
	Attached a;
	Unprotected unprotected;
	char name[] = "/tmp/test_bridge_XXXXXX";
	int fd = mkstemp(name);
	REQUIRE(fd >= 0);
	REQUIRE(write(fd, "hash", 4) == 4);
	lseek(fd, 0, SEEK_SET);
	// named by another spelling than the one opened: the inode decides
	char link_name[] = "/tmp/test_bridge_link_XXXXXX";
	REQUIRE(mkstemp(link_name) >= 0);
	unlink(link_name);
	REQUIRE(symlink(name, link_name) == 0);
	yprotect::setFiles(std::vector<std::string>(1, link_name));

	httpd::Response r;
	REQUIRE(httpd::answerFromDescriptor(r, fd));
	r.code = 200;
	r.content_type = "image/png";
	Handler h(M_GET, "/api/v1/channels/1/logo");
	CApiBridge::answerWith(&h.hh, r);
	CHECK(h.hh.httpStatus == HTTP_NOT_FOUND);
	CHECK(h.hh.status == HANDLED_READY);
	CHECK(h.hh.SendFd == -1);
	CHECK(h.hh.yresult.find("\"status\":404") != std::string::npos);
	CHECK(h.hh.yresult.find("hash") == std::string::npos);
	CHECK(fcntl(fd, F_GETFD) == -1);
	unlink(link_name);
	unlink(name);
}

TEST_CASE("the channel logo route sends no file that holds who may reach the box", "[bridge]")
{
	Attached a;
	Unprotected unprotected;
	InstalledDependencies box;
	char store[] = "/tmp/test_bridge_XXXXXX";
	int made = mkstemp(store);
	REQUIRE(made >= 0);
	REQUIRE(write(made, "hash", 4) == 4);
	close(made);
	char picture[] = "/tmp/test_bridge_XXXXXX";
	made = mkstemp(picture);
	REQUIRE(made >= 0);
	REQUIRE(write(made, "PNG", 3) == 3);
	close(made);
	yprotect::setFiles(std::vector<std::string>(1, store));

	// a channel list that names the store as a channel's picture
	box.logos.put(1, store);
	Handler held(M_GET, "/api/v1/channels/1/logo");
	REQUIRE(a.bridge.Hook_SendResponse(&held.hh) == HANDLED_READY);
	CHECK(held.hh.httpStatus == HTTP_NOT_FOUND);
	CHECK(held.hh.SendFd == -1);
	CHECK(held.hh.yresult.find("hash") == std::string::npos);
	CHECK(held.hh.yresult.find("no-such-name") != std::string::npos);

	// and a picture beside it goes out as before
	box.logos.put(2, picture);
	Handler sent(M_GET, "/api/v1/channels/2/logo");
	a.bridge.Hook_SendResponse(&sent.hh);
	CHECK(sent.hh.httpStatus == HTTP_OK);
	CHECK(sent.hh.status == HANDLED_SENDFD);
	REQUIRE(sent.hh.SendFd >= 0);
	CHECK(sent.hh.SendFdLength == 3);
	close(sent.hh.SendFd);
	sent.hh.SendFd = -1;
	unlink(store);
	unlink(picture);
}

TEST_CASE("the bridge and the request reader classify the same targets", "[bridge]")
{
	Attached a;
	const char *urls[] = {
		"/api/v1", "/api/v1/system/info", "/api/openapi.json", "/api/openapi.json/x",
		"/api/v10/x", "/api/v4/bouquets", "/api/v1.html", "/control/getversion", "/", "/api",
	};
	for (size_t i = 0; i < sizeof(urls) / sizeof(urls[0]); i++)
	{
		Handler h(M_GET, urls[i]);
		INFO(urls[i]);
		CHECK(CApiBridge::claims(&h.hh) == CyhookHandler::isApiTarget(urls[i]));
	}
	// what the reader saw undecoded and the bridge sees decoded agree because
	// both look at the decoded url
	Handler enc(M_GET, "/%61pi/v1/system/info");
	enc.hh.UrlData["url"] = "/api/v1/system/info";
	CHECK(CApiBridge::claims(&enc.hh));
	CHECK(CyhookHandler::isApiTarget(enc.hh.UrlData["url"]));
}

TEST_CASE("a file past the old ceiling is handed over, not refused", "[bridge]")
{
	Attached a;
	char name[] = "/tmp/test_bridge_XXXXXX";
	int fd = mkstemp(name);
	REQUIRE(fd >= 0);
	httpd::Response r;
	r.code = 200;
	r.content_type = "video/mp2t";
	r.fd = fd;
	r.length = (uint64_t) 9 << 20;
	Handler h(M_GET, "/api/v1/recordings/1");
	CApiBridge::answerWith(&h.hh, r);
	CHECK(h.hh.httpStatus == HTTP_OK);
	CHECK(h.hh.status == HANDLED_SENDFD);
	CHECK(h.hh.SendFdLength == (off_t) ((uint64_t) 9 << 20));
	CHECK(fcntl(fd, F_GETFD) != -1);
	close(h.hh.SendFd);
	h.hh.SendFd = -1;
	unlink(name);
}

TEST_CASE("a ranged ask against a file answer", "[bridge]")
{
	Attached a;
	char name[] = "/tmp/test_bridge_XXXXXX";
	int fd = mkstemp(name);
	REQUIRE(fd >= 0);
	REQUIRE(write(fd, "0123456", 7) == 7);
	lseek(fd, 0, SEEK_SET);

	httpd::Response r;
	REQUIRE(httpd::answerFromDescriptor(r, fd));
	r.code = 200;
	r.content_type = "image/png";

	SECTION("a stretch inside the file is 206 and the stretch")
	{
		Handler h(M_GET, "/api/v1/channels/1/logo");
		h.hh.HeaderList["Range"] = "bytes=2-4";
		CApiBridge::answerWith(&h.hh, r);
		CHECK(h.hh.httpStatus == HTTP_PARTIAL_CONTENT);
		CHECK(h.hh.status == HANDLED_SENDFD);
		CHECK(h.hh.SendFdOffset == 2);
		CHECK(h.hh.SendFdLength == 3);
		bool found = false;
		for (size_t i = 0; i < h.hh.ResponseHeaders.size(); i++)
			if (h.hh.ResponseHeaders[i].first == "Content-Range"
					&& h.hh.ResponseHeaders[i].second == "bytes 2-4/7")
				found = true;
		CHECK(found);
		close(h.hh.SendFd);
		h.hh.SendFd = -1;
	}
	SECTION("a stretch past the end is a problem document and the file is closed")
	{
		Handler h(M_GET, "/api/v1/channels/1/logo");
		h.hh.HeaderList["Range"] = "bytes=7-";
		CApiBridge::answerWith(&h.hh, r);
		CHECK(h.hh.httpStatus == HTTP_REQUEST_RANGE_NOT_SATISFIABLE);
		CHECK(h.hh.status == HANDLED_READY);
		CHECK(h.hh.SendFd == -1);
		CHECK(h.hh.yresult.find("\"status\":416") != std::string::npos);
		bool found = false;
		for (size_t i = 0; i < h.hh.ResponseHeaders.size(); i++)
			if (h.hh.ResponseHeaders[i].first == "Content-Range"
					&& h.hh.ResponseHeaders[i].second == "bytes */7")
				found = true;
		CHECK(found);
		CHECK(fcntl(fd, F_GETFD) == -1);
	}
	unlink(name);
}

TEST_CASE("a file answer says that it takes ranges", "[bridge]")
{
	/* A player decides from this header alone whether it may seek, so it has
	   to be there on the whole file as well as on a stretch of it. */
	Attached a;
	char name[] = "/tmp/test_bridge_XXXXXX";
	int fd = mkstemp(name);
	REQUIRE(fd >= 0);
	REQUIRE(write(fd, "0123456", 7) == 7);
	lseek(fd, 0, SEEK_SET);

	httpd::Response r;
	REQUIRE(httpd::answerFromDescriptor(r, fd));
	r.code = 200;
	r.content_type = "video/mp2t";

	const char *const asks[] = { "", "bytes=2-4", "bytes=7-" };
	for (size_t k = 0; k < sizeof(asks) / sizeof(asks[0]); ++k)
	{
		INFO("Range: " << asks[k]);
		if (k > 0)
		{
			fd = open(name, O_RDONLY);
			REQUIRE(fd >= 0);
			REQUIRE(httpd::answerFromDescriptor(r, fd));
			r.code = 200;
		}
		Handler h(M_GET, "/api/v1/recordings/archive/0123456789abcdef/file");
		if (asks[k][0] != '\0')
			h.hh.HeaderList["Range"] = asks[k];
		CApiBridge::answerWith(&h.hh, r);
		size_t seen = 0;
		for (size_t i = 0; i < h.hh.ResponseHeaders.size(); i++)
			if (h.hh.ResponseHeaders[i].first == "Accept-Ranges"
					&& h.hh.ResponseHeaders[i].second == "bytes")
				seen++;
		CHECK(seen == 1);
		if (h.hh.SendFd >= 0)
		{
			close(h.hh.SendFd);
			h.hh.SendFd = -1;
		}
	}
	unlink(name);
}

TEST_CASE("a ranged ask past the end of a document goes out as the range refusal", "[bridge]")
{
	/* Through the send pass and a route that answers a file, rather than handed
	   to answerWith: the range is the door's work and not the route's, so only
	   the door can note what it made of the answer, and the suite's record of
	   what the routes answered is where that has to show. */
	Attached a;
	char dir[] = "/tmp/test_bridge_config_XXXXXX";
	REQUIRE(mkdtemp(dir) != NULL);
	const std::string doc = std::string(dir) + "/ubouquets.xml";
	const int fd = open(doc.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0600);
	REQUIRE(fd >= 0);
	REQUIRE(write(fd, "<zapit/>", 8) == 8);
	close(fd);
	const std::string before = coreapi::config::configRoot();
	coreapi::config::setConfigRoot(dir);

	Handler h(M_GET, "/api/v1/config/ubouquets");
	h.hh.HeaderList["Range"] = "bytes=100-";
	const THandleStatus sent = a.bridge.Hook_SendResponse(&h.hh);
	coreapi::config::setConfigRoot(before);
	unlink(doc.c_str());
	rmdir(dir);

	REQUIRE(sent == HANDLED_READY);
	CHECK(h.hh.httpStatus == HTTP_REQUEST_RANGE_NOT_SATISFIABLE);
	CHECK(h.hh.yresult.find("range-outside-file") != std::string::npos);
}

TEST_CASE("a whole-file answer takes one range where the caller asked", "[bridge]")
{
	using nhttpd_bridge::FdPlan;

	SECTION("no header: the whole file under its own code")
	{
		FdPlan p = nhttpd_bridge::planFdAnswer(200, 10, "");
		CHECK(p.code == 200);
		CHECK(p.offset == 0);
		CHECK(p.length == 10);
		CHECK(p.content_range.empty());
	}
	SECTION("a stretch inside is 206 with the stretch")
	{
		FdPlan p = nhttpd_bridge::planFdAnswer(200, 10, "bytes=3-6");
		CHECK(p.code == 206);
		CHECK(p.offset == 3);
		CHECK(p.length == 4);
		CHECK(p.content_range == "bytes 3-6/10");
	}
	SECTION("an open end runs to the end of the file")
	{
		FdPlan p = nhttpd_bridge::planFdAnswer(200, 10, "bytes=6-");
		CHECK(p.code == 206);
		CHECK(p.offset == 6);
		CHECK(p.length == 4);
		CHECK(p.content_range == "bytes 6-9/10");
	}
	SECTION("a suffix longer than the file is the file, said as a stretch")
	{
		FdPlan p = nhttpd_bridge::planFdAnswer(200, 10, "bytes=-20");
		CHECK(p.code == 206);
		CHECK(p.offset == 0);
		CHECK(p.length == 10);
		CHECK(p.content_range == "bytes 0-9/10");
	}
	SECTION("a stretch past the end is refused with the length")
	{
		FdPlan p = nhttpd_bridge::planFdAnswer(200, 10, "bytes=10-");
		CHECK(p.code == 416);
		CHECK(p.content_range == "bytes */10");
	}
	SECTION("an answer that is not a 200 is sent whole, range unread")
	{
		FdPlan p = nhttpd_bridge::planFdAnswer(500, 10, "bytes=3-6");
		CHECK(p.code == 500);
		CHECK(p.offset == 0);
		CHECK(p.length == 10);
		CHECK(p.content_range.empty());
	}
}

TEST_CASE("the gate: the lan may read and a stranger may not", "[bridge]")
{
	Attached a;

	// The harness peer is on the lan (192.168.1.20, inside the shipped
	// lan_read), so a read passes with nothing presented.
	Handler lan(M_GET, "/api/v1/system/legacy-usage");
	REQUIRE(a.bridge.Hook_SendResponse(&lan.hh) == HANDLED_READY);
	CHECK(lan.hh.httpStatus == HTTP_OK);

	// Off the lan the same read is refused before any route runs, with the
	// one refusal the gate ever writes - 403, never a 401 that would ask a
	// browser for Basic - and it carries the API headers like any answer.
	Handler off(M_GET, "/api/v1/system/legacy-usage");
	off.hh.UrlData["clientaddr"] = "203.0.113.9";
	REQUIRE(a.bridge.Hook_SendResponse(&off.hh) == HANDLED_READY);
	CHECK(off.hh.httpStatus == HTTP_FORBIDDEN);
	CHECK(headerOf(off.hh, "Cache-Control") == "no-store");
}

TEST_CASE("the gate: a write asks for more than the lan grants", "[bridge]")
{
	Attached a;

	// The same request the router-refusal case sends with a credential: from
	// the lan alone it must not get as far as the router's 400.
	Handler put(M_PUT, "/api/v1/bouquets/none-such/name", "{}");
	REQUIRE(a.bridge.Hook_SendResponse(&put.hh) == HANDLED_READY);
	CHECK(put.hh.httpStatus == HTTP_FORBIDDEN);
}

TEST_CASE("the gate: a minted bearer token opens the write gate", "[bridge]")
{
	Attached a;

	// The way a configured token arrives: the stored form into the table,
	// the plain form into the header. A minted token would carry a scope,
	// and a scoped token counts only where a route reads one.
	const std::string token = "0123456789abcdef0123456789abcdef";
	httpd::addApiToken(httpd::tokenLookupPrefix(token), httpd::hashSecret(token),
	                   httpd::AuthLevel::System, "", 0);

	Handler put(M_PUT, "/api/v1/bouquets/none-such/name", "{}");
	put.hh.HeaderList["Authorization"] = "Bearer " + token;
	REQUIRE(a.bridge.Hook_SendResponse(&put.hh) == HANDLED_READY);
	// Through the gate: the router's own refusal of the empty body, not 401.
	CHECK(put.hh.httpStatus == HTTP_BAD_REQUEST);

	httpd::forgetApiTokens();
}

namespace
{
bool alwaysOwed()
{
	return true;
}
} // namespace

TEST_CASE("the gate: while the shipped password is in effect a bearer token is held to it too", "[bridge][firstlogin]")
{
	Attached a;
	httpd::setPasswordOwed(&alwaysOwed);

	// A script holding a token of the box's own is refused the way a browser is,
	// and told why: what anybody can look up opens no write, whatever carries it.
	const std::string token = "0123456789abcdef0123456789abcdef";
	httpd::addApiToken(httpd::tokenLookupPrefix(token), httpd::hashSecret(token),
	                   httpd::AuthLevel::System, "", 0);
	Handler put(M_PUT, "/api/v1/bouquets/none-such/name", "{}");
	put.hh.HeaderList["Authorization"] = "Bearer " + token;
	REQUIRE(a.bridge.Hook_SendResponse(&put.hh) == HANDLED_READY);
	CHECK(put.hh.httpStatus == HTTP_FORBIDDEN);
	CHECK(put.hh.yresult.find("password-change-required") != std::string::npos);

	// And the session document says so to that caller as well.
	Handler state(M_GET, "/api/v1/session");
	state.hh.HeaderList["Authorization"] = "Bearer " + token;
	REQUIRE(a.bridge.Hook_SendResponse(&state.hh) == HANDLED_READY);
	CHECK(state.hh.httpStatus == HTTP_OK);
	CHECK(state.hh.yresult.find("\"password_change_required\":true") != std::string::npos);

	httpd::forgetApiTokens();
}

TEST_CASE("the gate: a session reads as the owner and a mutation asks for the second token", "[bridge]")
{
	Attached a;

	const std::string session = httpd::openSessionForTest("root");
	REQUIRE(!session.empty());
	const std::string csrf = httpd::csrfFor(session);
	REQUIRE(!csrf.empty());
	const std::string cookie = std::string(httpd::sessionCookieName()) + "=" + session;

	// From off the lan, so the cookie and nothing else is the credential.
	Handler get(M_GET, "/api/v1/system/legacy-usage");
	get.hh.UrlData["clientaddr"] = "203.0.113.9";
	get.hh.HeaderList["Cookie"] = cookie;
	REQUIRE(a.bridge.Hook_SendResponse(&get.hh) == HANDLED_READY);
	CHECK(get.hh.httpStatus == HTTP_OK);

	// A mutation with the cookie alone is what another site can make a
	// browser send, and it is refused.
	Handler bare(M_PUT, "/api/v1/bouquets/none-such/name", "{}");
	bare.hh.UrlData["clientaddr"] = "203.0.113.9";
	bare.hh.HeaderList["Cookie"] = cookie;
	REQUIRE(a.bridge.Hook_SendResponse(&bare.hh) == HANDLED_READY);
	CHECK(bare.hh.httpStatus == HTTP_FORBIDDEN);

	// With the second token the gate opens and the router answers.
	Handler both(M_PUT, "/api/v1/bouquets/none-such/name", "{}");
	both.hh.UrlData["clientaddr"] = "203.0.113.9";
	both.hh.HeaderList["Cookie"] = cookie;
	both.hh.HeaderList[httpd::csrfHeaderName()] = csrf;
	REQUIRE(a.bridge.Hook_SendResponse(&both.hh) == HANDLED_READY);
	CHECK(both.hh.httpStatus == HTTP_BAD_REQUEST);
}

TEST_CASE("the gate: the handler is held to the credential that let the request in", "[bridge][token]")
{
	/* A bearer token answers before the cookie beside it, so a media token drawn with
	   both is not tied to that cookie's session; one drawn by the session alone is, and
	   ends with it. */
	Attached a;
	const std::string bearer = "00112233445566778899aabbccddeeff";
	httpd::addApiToken(httpd::tokenLookupPrefix(bearer), httpd::hashSecret(bearer),
	                   httpd::AuthLevel::System, "", 0);
	const std::string session = httpd::openSessionForTest("root");
	REQUIRE(!session.empty());
	const std::string cookie = std::string(httpd::sessionCookieName()) + "=" + session;
	const auto holds = [](const std::string &yresult) {
		const std::string key = "\"token\":\"";
		const size_t at = yresult.find(key);
		REQUIRE(at != std::string::npos);
		httpd::Credentials player;
		player.peer = "203.0.113.9";
		player.query_token = yresult.substr(at + key.size(), yresult.find('"', at + key.size()) - at - key.size());
		return httpd::granted(player, true);
	};

	Handler both(M_POST, "/api/v1/token/media");
	both.hh.UrlData["clientaddr"] = "203.0.113.9";
	both.hh.HeaderList["Authorization"] = "Bearer " + bearer;
	both.hh.HeaderList["Cookie"] = cookie;
	REQUIRE(a.bridge.Hook_SendResponse(&both.hh) == HANDLED_READY);
	REQUIRE(both.hh.httpStatus == HTTP_OK);

	Handler alone(M_POST, "/api/v1/token/media");
	alone.hh.UrlData["clientaddr"] = "203.0.113.9";
	alone.hh.HeaderList["Cookie"] = cookie;
	alone.hh.HeaderList[httpd::csrfHeaderName()] = httpd::csrfFor(session);
	REQUIRE(a.bridge.Hook_SendResponse(&alone.hh) == HANDLED_READY);
	REQUIRE(alone.hh.httpStatus == HTTP_OK);

	REQUIRE(httpd::closeSession(session));
	CHECK(holds(both.hh.yresult) == httpd::AuthLevel::System);
	CHECK(holds(alone.hh.yresult) == httpd::AuthLevel::Public);
	httpd::forgetApiTokens();
}

TEST_CASE("the gate: the query token counts only where the route reads one, and only scoped", "[bridge]")
{
	Attached a;

	// The scoped media token, minted the way /api/v1/token/media mints one.
	const std::string media = httpd::openScopedToken(httpd::mediaScopeName(),
	                                                 httpd::AuthLevel::System);
	REQUIRE(!media.empty());

	// The core first: the minted token resolves in the query where the route
	// allows one, with its scope beside it.
	{
		httpd::Credentials probe;
		probe.peer = "203.0.113.9";
		probe.query_token = media;
		std::string named;
		REQUIRE(httpd::granted(probe, true, &named) == httpd::AuthLevel::System);
		REQUIRE(named == httpd::mediaScopeName());

		httpd::AuthLevel need = httpd::AuthLevel::Public;
		bool qok = false;
		REQUIRE(httpd::routeLevelFor(httpd::Get, "/api/v1/storage/file", &need, &qok));
		REQUIRE(qok);
	}

	// A route that was not written to read one: the token in the address is
	// nothing, and the stranger is refused.
	Handler info(M_GET, "/api/v1/system/legacy-usage?token=" + media);
	info.hh.UrlData["clientaddr"] = "203.0.113.9";
	REQUIRE(a.bridge.Hook_SendResponse(&info.hh) == HANDLED_READY);
	CHECK(info.hh.httpStatus == HTTP_FORBIDDEN);

	// The box's standing credential does not belong in an address: at the one
	// route that reads a query token, a configured token there is still
	// nothing - only a scoped one counts.
	const std::string standing = "0f1e2d3c4b5a69780f1e2d3c4b5a6978";
	httpd::addApiToken(httpd::tokenLookupPrefix(standing), httpd::hashSecret(standing),
	                   httpd::AuthLevel::System, "", 0);
	Handler wrong(M_GET, "/api/v1/storage/file?path=%2Ftmp%2Fx&token=" + standing);
	wrong.hh.UrlData["clientaddr"] = "203.0.113.9";
	REQUIRE(a.bridge.Hook_SendResponse(&wrong.hh) == HANDLED_READY);
	CHECK(wrong.hh.httpStatus == HTTP_FORBIDDEN);

	/* The end-to-end file answer for a scoped token is the wire check's
	   (check-v1.sh): readFile calls confine() and rebuilds the roots from
	   the box's own settings before every request, so a root a case sets
	   here is gone by the time the handler looks - by design, a directory
	   entered in a menu has to work without a restart. What this file can
	   pin is above: the token resolves, the gate opens only where the route
	   reads one, and only a scoped token counts in an address. */

	httpd::forgetApiTokens();
}

TEST_CASE("without connection threads an event stream is refused and a carried answer is not", "[bridge]")
{
	/* webserver.threading=false runs WebThread on the accept thread itself;
	   one healthy event stream would then be the last request the box ever
	   takes. Refused before a seat is taken, as an ordinary finite answer. A
	   carried answer runs on a thread of its own, so it is handed on all the
	   same. */
	Attached a;
	Carrying c;
	httpd::Response sse;
	sse.code = 200;
	sse.content_type = "text/event-stream";

	CWebserver::is_threading = false;
	Handler e(M_GET, "/api/v1/events");
	CApiBridge::answerWith(&e.hh, sse);
	Handler p(M_GET, kNoRoute);
	CApiBridge::answerWith(&p.hh, programAnswer());
	CWebserver::is_threading = true;

	CHECK(e.hh.httpStatus == HTTP_NOT_IMPLEMENTED);
	CHECK(e.hh.status == HANDLED_READY);
	CHECK(e.hh.yresult.find("\"status\":501") != std::string::npos);

	CHECK(p.hh.status == HANDLED_STREAM);
	CHECK(p.hh.Carried != NULL);
}
