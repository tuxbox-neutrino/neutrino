/*
 * test_yhttpd_api.cpp - the pieces nhttpd's core grew for the REST bridge
 *
 * Copyright (C) 2026 Thilo Graf
 *
 * License: GPL-2.0-or-later
 *
 * What is checked here is the contract the bridge relies on and nothing the
 * legacy hooks could notice: the API namespace is matched on the raw target,
 * the API header carries the hook's own headers and the true body length,
 * the socket delivers and receives exact byte counts, and the API body reader
 * refuses what it cannot frame. The legacy header is checked too, so a change
 * that leaks into it is caught here rather than on a box.
 */

#include "support/catch.hpp"

#include "yhook.h"
#include "yrequest.h"
#include "ysocket.h"
#include "ytypes_globals.h"

#include <cstring>
#include <string>
#include <thread>
#include <vector>

#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>

namespace {

long long millisNow()
{
	struct timeval tv;
	gettimeofday(&tv, NULL);
	return (long long) tv.tv_sec * 1000 + tv.tv_usec / 1000;
}

size_t countOf(const std::string &hay, const std::string &needle)
{
	size_t n = 0;
	for (size_t at = hay.find(needle); at != std::string::npos; at = hay.find(needle, at + needle.size()))
		n++;
	return n;
}

struct RegisteredPrefixes
{
	RegisteredPrefixes()
	{
		CyhookHandler::clearApiPrefixes();
		CyhookHandler::registerApiPrefix("/api/v1");
		CyhookHandler::registerApiPrefix("/api/openapi.json");
	}
	~RegisteredPrefixes() { CyhookHandler::clearApiPrefixes(); }
};

CyhookHandler apiHandler(THttp_Method method, HttpResponseType code, const std::string &body)
{
	CyhookHandler hh;
	CStringList none, config;
	hh.session_init(none, none, none, config, method, false);
	hh.api_mode = true;
	hh.SetHeader(code, "application/problem+json");
	hh.yresult = body;
	return hh;
}

} // namespace

TEST_CASE("an api prefix matches the raw target only where it was registered", "[nhttpd]")
{
	RegisteredPrefixes reg;

	CHECK(CyhookHandler::isApiTarget("/api/v1"));
	CHECK(CyhookHandler::isApiTarget("/api/v1/system/info"));
	CHECK(CyhookHandler::isApiTarget("/api/v1/channels?x=%ZZ"));
	CHECK(CyhookHandler::isApiTarget("/api/openapi.json"));

	CHECK_FALSE(CyhookHandler::isApiTarget("/api/v10/system/info"));
	CHECK_FALSE(CyhookHandler::isApiTarget("/api/v4/bouquets"));
	CHECK_FALSE(CyhookHandler::isApiTarget("/api/openapi.json.gz"));
	CHECK_FALSE(CyhookHandler::isApiTarget("/control/getversion"));
	CHECK_FALSE(CyhookHandler::isApiTarget("/"));
	CHECK_FALSE(CyhookHandler::isApiTarget(""));
}

TEST_CASE("with nothing registered no target is an api target", "[nhttpd]")
{
	CyhookHandler::clearApiPrefixes();
	CHECK_FALSE(CyhookHandler::isApiTarget("/api/v1/system/info"));
}

TEST_CASE("the api header carries the hook's headers and the true body length", "[nhttpd]")
{
	std::string body("{\"a\":\"");
	body.push_back('\0');
	body += "\"}";
	CyhookHandler hh = apiHandler(M_GET, HTTP_NOT_FOUND, body);
	hh.UrlData["fileext"] = "gz";
	hh.ResponseHeaders.push_back(std::make_pair("Cache-Control", "no-store"));
	hh.ResponseHeaders.push_back(std::make_pair("X-Content-Type-Options", "nosniff"));
	hh.ResponseHeaders.push_back(std::make_pair("Content-Length", "999"));
	hh.ResponseHeaders.push_back(std::make_pair("Connection", "keep-alive"));

	const std::string header = hh.BuildHeader();

	CHECK(header.compare(0, 22, "HTTP/1.0 404 Not Found") == 0);
	CHECK(countOf(header, "Content-Type: application/problem+json\r\n") == 1);
	CHECK(countOf(header, "Content-Length: " + std::to_string(body.size()) + "\r\n") == 1);
	CHECK(countOf(header, "Content-Length: 999") == 0);
	CHECK(countOf(header, "Content-Length: 0") == 0);
	CHECK(countOf(header, "Cache-Control: no-store\r\n") == 1);
	CHECK(countOf(header, "Cache-Control: no-cache") == 0);
	CHECK(countOf(header, "X-Content-Type-Options: nosniff\r\n") == 1);
	CHECK(countOf(header, "Connection: close\r\n") == 1);
	CHECK(countOf(header, "Connection: keep-alive") == 0);
	CHECK(countOf(header, "Content-Encoding") == 0);
	CHECK(header.size() >= 4);
	CHECK(header.compare(header.size() - 4, 4, "\r\n\r\n") == 0);
	CHECK(header.find("<html>") == std::string::npos);
}

TEST_CASE("the api header states the length for HEAD and appends no page to a redirect", "[nhttpd]")
{
	CyhookHandler head = apiHandler(M_HEAD, HTTP_OK, "abc");
	const std::string h1 = head.BuildHeader();
	CHECK(countOf(h1, "Content-Length: 3\r\n") == 1);
	CHECK(h1.compare(h1.size() - 4, 4, "\r\n\r\n") == 0);

	CyhookHandler moved = apiHandler(M_GET, HTTP_MOVED_PERMANENTLY, "");
	moved.NewURL = "/api/v1/elsewhere";
	const std::string h2 = moved.BuildHeader();
	CHECK(countOf(h2, "Location: /api/v1/elsewhere\r\n") == 1);
	CHECK(h2.find("<html>") == std::string::npos);
	CHECK(h2.compare(h2.size() - 4, 4, "\r\n\r\n") == 0);

	CyhookHandler unauth = apiHandler(M_GET, HTTP_UNAUTHORIZED, "{}");
	const std::string h3 = unauth.BuildHeader();
	CHECK(countOf(h3, "WWW-Authenticate: Basic realm=") == 1);
	CHECK(countOf(h3, "Content-Length: 2\r\n") == 1);
	CHECK(h3.compare(h3.size() - 4, 4, "\r\n\r\n") == 0);
}

TEST_CASE("the legacy header is what it was", "[nhttpd]")
{
	CyhookHandler hh = apiHandler(M_GET, HTTP_NOT_FOUND, "ignored");
	hh.api_mode = false;
	hh.ResponseHeaders.push_back(std::make_pair("Cache-Control", "no-store"));
	hh.UrlData["fileext"] = "gz";

	const std::string header = hh.BuildHeader();

	CHECK(countOf(header, "Content-Length: 0\r\n") == 1);
	CHECK(countOf(header, "Cache-Control: no-cache\r\n") == 1);
	CHECK(countOf(header, "Cache-Control: no-store") == 0);
	CHECK(countOf(header, "Content-Encoding: gzip\r\n") == 1);
}

TEST_CASE("ReceiveExact reads the bytes it was promised and no more", "[nhttpd]")
{
	int fd[2];
	REQUIRE(socketpair(AF_UNIX, SOCK_STREAM, 0, fd) == 0);
	CySocket s(fd[0]);
	s.isOpened = true;
	s.isValid = true;

	SECTION("exact count, zero bytes kept, the rest left on the socket")
	{
		const char sent[] = { 'a', 'b', '\0', 'c', 'd', 'X' };
		REQUIRE(write(fd[1], sent, sizeof(sent)) == (ssize_t) sizeof(sent));
		std::string out;
		CHECK(s.ReceiveExact(out, 5, 1000));
		REQUIRE(out.size() == 5);
		CHECK(out[2] == '\0');
		CHECK(out[4] == 'd');
		char rest = 0;
		CHECK(read(fd[0], &rest, 1) == 1);
		CHECK(rest == 'X');
	}
	SECTION("a body that stops short fails at the deadline, not never")
	{
		REQUIRE(write(fd[1], "ab", 2) == 2);
		std::string out;
		const long long t0 = millisNow();
		CHECK_FALSE(s.ReceiveExact(out, 5, 200));
		const long long took = millisNow() - t0;
		CHECK(took >= 150);
		CHECK(took < 2000);
	}
	SECTION("a peer that closes fails at once")
	{
		REQUIRE(write(fd[1], "ab", 2) == 2);
		close(fd[1]);
		fd[1] = -1;
		std::string out;
		const long long t0 = millisNow();
		CHECK_FALSE(s.ReceiveExact(out, 5, 5000));
		CHECK(millisNow() - t0 < 1000);
	}
	SECTION("nothing to read is fine")
	{
		std::string out("keep");
		CHECK(s.ReceiveExact(out, 0, 100));
		CHECK(out.empty());
	}
	if (fd[1] >= 0)
		close(fd[1]);
	s.close();
}

TEST_CASE("SendAll delivers every byte even when the socket takes them in pieces", "[nhttpd]")
{
	int fd[2];
	REQUIRE(socketpair(AF_UNIX, SOCK_STREAM, 0, fd) == 0);
	int small = 4096;
	setsockopt(fd[0], SOL_SOCKET, SO_SNDBUF, &small, sizeof(small));
	CySocket s(fd[0]);
	s.isOpened = true;
	s.isValid = true;

	std::string payload(1024 * 1024, 'x');
	for (size_t i = 0; i < payload.size(); i += 251)
		payload[i] = '\0';

	std::string got;
	std::thread reader([&got, fd]() {
		char buf[65536];
		for (;;)
		{
			ssize_t n = read(fd[1], buf, sizeof(buf));
			if (n <= 0)
				break;
			got.append(buf, (size_t) n);
		}
	});

	CHECK(s.SendAll(payload.data(), payload.size()));
	s.close();
	reader.join();
	close(fd[1]);

	CHECK(got.size() == payload.size());
	CHECK(got == payload);
}

TEST_CASE("the api body reader refuses what it cannot frame", "[nhttpd]")
{
	long long len = -1;

	CHECK(CWebserverRequest::apiContentLength("Host: box\r\n\r\n", len) == CWebserverRequest::BODY_NONE);
	CHECK(len == 0);

	CHECK(CWebserverRequest::apiContentLength("content-length: 12\r\n\r\n", len) == CWebserverRequest::BODY_OK);
	CHECK(len == 12);

	CHECK(CWebserverRequest::apiContentLength("Content-Length: 12\r\nContent-Length: 12\r\n\r\n", len) == CWebserverRequest::BODY_OK);
	CHECK(len == 12);

	CHECK(CWebserverRequest::apiContentLength("Content-Length: 12\r\ncontent-length: 13\r\n\r\n", len) == CWebserverRequest::BODY_BAD);
	CHECK(CWebserverRequest::apiContentLength("Content-Length: -1\r\n\r\n", len) == CWebserverRequest::BODY_BAD);
	CHECK(CWebserverRequest::apiContentLength("Content-Length: 12abc\r\n\r\n", len) == CWebserverRequest::BODY_BAD);
	CHECK(CWebserverRequest::apiContentLength("Content-Length:\r\n\r\n", len) == CWebserverRequest::BODY_BAD);
	CHECK(CWebserverRequest::apiContentLength("Transfer-Encoding: chunked\r\n\r\n", len) == CWebserverRequest::BODY_BAD);
	CHECK(CWebserverRequest::apiContentLength("Content-Length: 5\r\nTransfer-Encoding: chunked\r\n\r\n", len) == CWebserverRequest::BODY_BAD);
	CHECK(CWebserverRequest::apiContentLength("Content-Length: 99999999999999999999\r\n\r\n", len) == CWebserverRequest::BODY_BAD);
}
