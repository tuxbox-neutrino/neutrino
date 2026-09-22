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
 * and without gzip, and the refusals the bridge makes itself (413, the
 * answer shapes nhttpd cannot carry yet). Routes that reach a box backend are
 * only checked for reaching the layer, not for what the box says.
 */

#include "support/catch.hpp"

#include "httpd/nhttpd_bridge.h"
#include "httpd/doc/openapi.h"

#include "yhook.h"
#include "ytypes_globals.h"

#include <cstdlib>
#include <cstring>
#include <string>

#include <fcntl.h>
#include <strings.h>
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
	~Attached() { CyhookHandler::clearApiPrefixes(); }
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

	Handler bad(M_POST, "/api/v1/system/standby", "{\"on\": [1,2]}");
	REQUIRE(a.bridge.Hook_SendResponse(&bad.hh) == HANDLED_READY);
	CHECK(bad.hh.httpStatus == HTTP_BAD_REQUEST);

	Handler missing(M_POST, "/api/v1/system/standby", "{}");
	REQUIRE(a.bridge.Hook_SendResponse(&missing.hh) == HANDLED_READY);
	CHECK(missing.hh.httpStatus == HTTP_BAD_REQUEST);
	CHECK(missing.hh.yresult.find("\"status\":400") != std::string::npos);

	// PUT is dispatched and the path is matched: the router refuses the empty
	// body before any handler runs, so no box backend is needed here.
	Handler put(M_PUT, "/api/v1/bouquets/none-such/name", "{}");
	REQUIRE(a.bridge.Hook_SendResponse(&put.hh) == HANDLED_READY);
	CHECK(put.hh.httpStatus == HTTP_BAD_REQUEST);
}

TEST_CASE("a method nhttpd cannot name is 501 from the router", "[bridge]")
{
	Attached a;
	Handler h(M_TRACE, "/api/v1/system/info");

	REQUIRE(a.bridge.Hook_SendResponse(&h.hh) == HANDLED_READY);
	CHECK(h.hh.httpStatus == HTTP_NOT_IMPLEMENTED);
	CHECK(h.hh.ResponseMimeType == "application/problem+json");
}

TEST_CASE("an answer shape nhttpd cannot carry yet is refused as 501 by the bridge", "[bridge]")
{
	Attached a;
	httpd::Response r;
	r.code = 200;
	r.content_type = "video/mp2t";
	r.stream_argv.push_back("cat");

	Handler h(M_GET, "/api/v1/stream/1");
	CApiBridge::answerWith(&h.hh, r);
	CHECK(h.hh.httpStatus == HTTP_NOT_IMPLEMENTED);
	CHECK(h.hh.yresult.find("\"status\":501") != std::string::npos);

	httpd::Response sse;
	sse.code = 200;
	sse.content_type = "text/event-stream";
	Handler e(M_GET, "/api/v1/events");
	CApiBridge::answerWith(&e.hh, sse);
	CHECK(e.hh.httpStatus == HTTP_NOT_IMPLEMENTED);
}

TEST_CASE("a file answer is read into the body and the descriptor is closed", "[bridge]")
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
	REQUIRE(h.hh.yresult.size() == sizeof(bytes));
	CHECK(h.hh.yresult[3] == '\0');
	// the descriptor was handed over and is closed now
	CHECK(fcntl(fd, F_GETFD) == -1);
	unlink(name);
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

TEST_CASE("a file answer this server cannot carry is refused and the descriptor closed", "[bridge]")
{
	Attached a;
	char name[] = "/tmp/test_bridge_XXXXXX";

	SECTION("a file past the ceiling is not read")
	{
		int fd = mkstemp(name);
		REQUIRE(fd >= 0);
		httpd::Response r;
		r.code = 200;
		r.content_type = "video/mp2t";
		r.fd = fd;
		r.length = (uint64_t) 9 << 20;
		Handler h(M_GET, "/api/v1/recordings/1");
		CApiBridge::answerWith(&h.hh, r);
		CHECK(h.hh.httpStatus == HTTP_INTERNAL_SERVER_ERROR);
		CHECK(h.hh.yresult.find("\"status\":500") != std::string::npos);
		CHECK(fcntl(fd, F_GETFD) == -1);
		unlink(name);
	}
	SECTION("a file shorter than it said is not answered as whole")
	{
		int fd = mkstemp(name);
		REQUIRE(fd >= 0);
		REQUIRE(write(fd, "abc", 3) == 3);
		httpd::Response r;
		r.code = 200;
		r.content_type = "image/png";
		r.fd = fd;
		r.length = 10;
		Handler h(M_GET, "/api/v1/channels/1/logo");
		CApiBridge::answerWith(&h.hh, r);
		CHECK(h.hh.httpStatus == HTTP_INTERNAL_SERVER_ERROR);
		CHECK(fcntl(fd, F_GETFD) == -1);
		unlink(name);
	}
}
