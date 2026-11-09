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
#include "support/wireserver.h"

#include "yconnection.h"
#include "yhook.h"
#include "yrequest.h"
#include "ysocket.h"
#include "ytypes_globals.h"
#include "ywebserver.h"
#include <httpd/authority.h>
#include <httpd/server_control.h>

#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <thread>
#include <vector>

#include <fcntl.h>
#include <poll.h>
#include <pthread.h>
#include <signal.h>
#include <stdint.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/time.h>
#include <time.h>
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
		CyhookHandler::registerApiTarget("/api/openapi.json");
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

TEST_CASE("stop alone ends an idle accept loop", "[nhttpd]")
{
	/* The restart seam stops a webserver that may be serving nobody, and a
	   stop that only raises a flag leaves the thread in accept() until a
	   client happens to knock (the review's zombie: a second server comes
	   up beside it and the sleeper's epilogue later tears the living one
	   down). So stop() itself has to wake the loop, and this pins it: no
	   connection is ever made after stop, and the loop still ends. */
	// is_threading is static; put back what the cases after this expect.
	const bool threading_was = CWebserver::is_threading;
	CWebserver ws;
	int port = 34611;
	ws.init((unsigned) port, false, "127.0.0.1");

	pthread_t runner;
	struct Runner
	{
		static void *run(void *arg)
		{
			((CWebserver *) arg)->run();
			return NULL;
		}
	};
	REQUIRE(pthread_create(&runner, NULL, &Runner::run, &ws) == 0);

	// Up means answering: wait until the listener stands.
	bool up = false;
	for (int i = 0; i < 100 && !up; i++)
	{
		usleep(50 * 1000);
		int fd = socket(AF_INET, SOCK_STREAM, 0);
		REQUIRE(fd >= 0);
		struct sockaddr_in to;
		std::memset(&to, 0, sizeof(to));
		to.sin_family = AF_INET;
		to.sin_port = htons((unsigned short) port);
		to.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
		if (connect(fd, (struct sockaddr *) &to, sizeof(to)) == 0)
			up = true;
		close(fd);
	}
	REQUIRE(up);

	// Give the loop time to finish with the probe connection and sit back
	// down in accept: the stop below must find it waiting, not working.
	usleep(300 * 1000);

	// The accept loop is idle again (the probe connection above was taken
	// and refused or handled). Stop, and nobody knocks after this line.
	ws.stop();

	struct timespec until;
	clock_gettime(CLOCK_REALTIME, &until);
	until.tv_sec += 3;
	const int joined = pthread_timedjoin_np(runner, NULL, &until);
	if (joined != 0)
		pthread_cancel(runner);
	CWebserver::is_threading = threading_was;
	httpd::setBoundPortFromBridge(0);
	REQUIRE(joined == 0);
}

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
	// the document is one exact target, not a directory: what lies below
	// its name is legacy ground and keeps the legacy authentication
	CHECK(CyhookHandler::isApiTarget("/api/openapi.json?x=1"));
	CHECK_FALSE(CyhookHandler::isApiTarget("/api/openapi.json/anything"));
	CHECK_FALSE(CyhookHandler::isApiTarget("/api/openapi.json/"));
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
	/* Every legacy 404 (mod_sendfile, a script error) comes with an empty
	   body and keeps announcing a length of 0. */
	CyhookHandler hh = apiHandler(M_GET, HTTP_NOT_FOUND, "");
	hh.api_mode = false;
	hh.ResponseHeaders.push_back(std::make_pair("Cache-Control", "no-store"));
	hh.UrlData["fileext"] = "gz";

	const std::string header = hh.BuildHeader();

	CHECK(countOf(header, "Content-Length: 0\r\n") == 1);
	CHECK(countOf(header, "Cache-Control: no-cache\r\n") == 1);
	CHECK(countOf(header, "Cache-Control: no-store") == 0);
	CHECK(countOf(header, "Content-Encoding: gzip\r\n") == 1);
}

TEST_CASE("a legacy 404 with a body states its length", "[nhttpd]")
{
	/* The JSON error objects of /api/v4 are 404s with a body; a length of
	   0 would make clients drop them. */
	CyhookHandler hh = apiHandler(M_GET, HTTP_NOT_FOUND, "{\"x\":1}");
	hh.api_mode = false;

	const std::string header = hh.BuildHeader();

	CHECK(countOf(header, "Content-Length: 7\r\n") == 1);
	CHECK(countOf(header, "Content-Length: 0\r\n") == 0);
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

	// a second length is refused even where it repeats the first: a reader in
	// front of this one may frame the message differently
	CHECK(CWebserverRequest::apiContentLength("Content-Length: 12\r\nContent-Length: 12\r\n\r\n", len) == CWebserverRequest::BODY_BAD);

	CHECK(CWebserverRequest::apiContentLength("Content-Length: 12\r\ncontent-length: 13\r\n\r\n", len) == CWebserverRequest::BODY_BAD);
	CHECK(CWebserverRequest::apiContentLength("Content-Length: -1\r\n\r\n", len) == CWebserverRequest::BODY_BAD);
	CHECK(CWebserverRequest::apiContentLength("Content-Length: 12abc\r\n\r\n", len) == CWebserverRequest::BODY_BAD);
	CHECK(CWebserverRequest::apiContentLength("Content-Length:\r\n\r\n", len) == CWebserverRequest::BODY_BAD);
	CHECK(CWebserverRequest::apiContentLength("Transfer-Encoding: chunked\r\n\r\n", len) == CWebserverRequest::BODY_BAD);
	CHECK(CWebserverRequest::apiContentLength("Content-Length: 5\r\nTransfer-Encoding: chunked\r\n\r\n", len) == CWebserverRequest::BODY_BAD);
	CHECK(CWebserverRequest::apiContentLength("Content-Length: 99999999999999999999\r\n\r\n", len) == CWebserverRequest::BODY_BAD);
}

TEST_CASE("a handler that never saw session_init builds the legacy header", "[nhttpd]")
{
	// The request reader answers 501/400 before SendResponse() and before
	// session_init(); what it sends must be the legacy header.
	CyhookHandler hh;
	hh.Method = M_PUT;
	hh.SetHeader(HTTP_NOT_IMPLEMENTED, "text/html");
	const std::string header = hh.BuildHeader();
	CHECK(countOf(header, "Cache-Control: no-cache\r\n") == 1);
	CHECK(countOf(header, "HTTP/1.0 501 ") == 1);
	CHECK_FALSE(hh.api_mode);
	CHECK_FALSE(hh.RawBodyTooLarge);
}

TEST_CASE("the api header reader stops at the empty line, at EOF, at the limit and at the deadline", "[nhttpd]")
{
	int fd[2];
	REQUIRE(socketpair(AF_UNIX, SOCK_STREAM, 0, fd) == 0);
	CySocket s(fd[0]);
	s.isOpened = true;
	s.isValid = true;
	std::string raw;

	SECTION("a whole header, and the body stays on the socket")
	{
		const char *req = "Host: box\r\nContent-Length: 2\r\n\r\nxy";
		REQUIRE(write(fd[1], req, strlen(req)) == (ssize_t) strlen(req));
		CHECK(CWebserverRequest::readApiHeader(s, raw, 65536, 1000));
		CHECK(raw == "Host: box\r\nContent-Length: 2\r\n\r\n");
		char rest[2];
		CHECK(read(fd[0], rest, 2) == 2);
	}
	SECTION("a request without header lines ends at its empty line")
	{
		// the request line took its own CRLF, so the empty line is all there
		// is; waiting for a second CRLF pair would sit out the deadline
		REQUIRE(write(fd[1], "\r\nxy", 4) == 4);
		// closed behind the bytes, so a reader that wants more fails at once
		// instead of holding the run until its deadline
		close(fd[1]);
		fd[1] = -1;
		const long long t0 = millisNow();
		CHECK(CWebserverRequest::readApiHeader(s, raw, 65536, 5000));
		CHECK(millisNow() - t0 < 1000);
		CHECK(raw == "\r\n");
		char rest[2];
		CHECK(read(fd[0], rest, 2) == 2);
	}
	SECTION("EOF before the empty line is a refusal, not a loop")
	{
		REQUIRE(write(fd[1], "Host: box\r\n", 11) == 11);
		close(fd[1]);
		fd[1] = -1;
		const long long t0 = millisNow();
		CHECK_FALSE(CWebserverRequest::readApiHeader(s, raw, 65536, 5000));
		CHECK(millisNow() - t0 < 1000);
	}
	SECTION("a header past the limit is a refusal")
	{
		std::string big = "X-Long: " + std::string(70000, 'a') + "\r\n\r\n";
		std::thread writer([&big, fd]() { (void) write(fd[1], big.data(), big.size()); });
		CHECK_FALSE(CWebserverRequest::readApiHeader(s, raw, 65536, 5000));
		close(fd[0]);
		writer.join();
	}
	SECTION("a peer that stops mid-header meets the deadline")
	{
		REQUIRE(write(fd[1], "Host: box\r\n", 11) == 11);
		const long long t0 = millisNow();
		CHECK_FALSE(CWebserverRequest::readApiHeader(s, raw, 65536, 300));
		const long long took = millisNow() - t0;
		CHECK(took >= 200);
		CHECK(took < 3000);
	}
	if (fd[1] >= 0)
		close(fd[1]);
	s.close();
}

TEST_CASE("what to do about a body is decided per method before a byte is read", "[nhttpd]")
{
	long long len = -1;
	using R = CWebserverRequest;

	CHECK(R::apiBodyDecision(M_GET, "Host: b\r\n\r\n", len) == R::API_BODY_NONE);
	CHECK(R::apiBodyDecision(M_DELETE, "Host: b\r\n\r\n", len) == R::API_BODY_NONE);
	CHECK(R::apiBodyDecision(M_POST, "Host: b\r\n\r\n", len) == R::API_BODY_NONE);

	CHECK(R::apiBodyDecision(M_POST, "Content-Length: 10\r\n\r\n", len) == R::API_BODY_READ);
	CHECK(len == 10);
	CHECK(R::apiBodyDecision(M_PUT, "content-length: 0\r\n\r\n", len) == R::API_BODY_NONE);
	CHECK(R::apiBodyDecision(M_POST, "Content-Length: 2097152\r\n\r\n", len) == R::API_BODY_TOO_LARGE);

	// a body on a method that takes none is refused, not skipped
	CHECK(R::apiBodyDecision(M_GET, "Content-Length: 5\r\n\r\n", len) == R::API_BODY_BAD);
	CHECK(R::apiBodyDecision(M_HEAD, "Transfer-Encoding: chunked\r\n\r\n", len) == R::API_BODY_BAD);
	CHECK(R::apiBodyDecision(M_DELETE, "Content-Length: 1\r\n\r\n", len) == R::API_BODY_BAD);
	CHECK(R::apiBodyDecision(M_POST, "Transfer-Encoding: chunked\r\n\r\n", len) == R::API_BODY_BAD);
	CHECK(R::apiBodyDecision(M_POST, "Content-Length: 1\r\nContent-Length: 2\r\n\r\n", len) == R::API_BODY_BAD);
}

TEST_CASE("Discard swallows a body that was refused so the refusal can be read", "[nhttpd]")
{
	int fd[2];
	REQUIRE(socketpair(AF_UNIX, SOCK_STREAM, 0, fd) == 0);
	CySocket s(fd[0]);
	s.isOpened = true;
	s.isValid = true;

	SECTION("the declared bytes go, the byte after them stays")
	{
		std::string body(200000, 'b');
		body += 'X';
		std::thread writer([&body, fd]() { (void) write(fd[1], body.data(), body.size()); });
		CHECK(s.Discard(200000, 2000));
		writer.join();
		char rest = 0;
		CHECK(read(fd[0], &rest, 1) == 1);
		CHECK(rest == 'X');
	}
	SECTION("a peer that stops sending meets the deadline")
	{
		REQUIRE(write(fd[1], "ab", 2) == 2);
		const long long t0 = millisNow();
		CHECK_FALSE(s.Discard(10, 200));
		CHECK(millisNow() - t0 >= 150);
		CHECK(millisNow() - t0 < 3000);
	}
	SECTION("a peer that closes is done")
	{
		REQUIRE(write(fd[1], "ab", 2) == 2);
		close(fd[1]);
		fd[1] = -1;
		CHECK_FALSE(s.Discard(10, 2000));
	}
	if (fd[1] >= 0)
		close(fd[1]);
	s.close();
}

TEST_CASE("the api header states a descriptor's length, not the body's", "[nhttpd]")
{
	// a descriptor the test owns, never the process's own 0
	int fd = open("/dev/null", O_RDONLY | O_CLOEXEC);
	REQUIRE(fd >= 0);

	CyhookHandler hh = apiHandler(M_GET, HTTP_OK, "");
	hh.SendFd = fd;
	hh.SendFdLength = 1234;
	std::string header = hh.BuildHeader();
	CHECK(header.find("Content-Length: 1234\r\n") != std::string::npos);

	CyhookHandler head = apiHandler(M_HEAD, HTTP_OK, "");
	head.SendFd = fd;
	head.SendFdLength = 77;
	CHECK(head.BuildHeader().find("Content-Length: 77\r\n") != std::string::npos);

	hh.SendFd = -1;
	head.SendFd = -1;
	close(fd);
}

TEST_CASE("a fresh session carries no descriptor", "[nhttpd]")
{
	int fd = open("/dev/null", O_RDONLY | O_CLOEXEC);
	REQUIRE(fd >= 0);

	CyhookHandler hh;
	CHECK(hh.SendFd == -1);
	CHECK_FALSE(hh.LengthUnknown);
	hh.SendFd = fd;
	hh.SendFdLength = 5;
	hh.LengthUnknown = true;
	CStringList none, config;
	hh.session_init(none, none, none, config, M_GET, false);
	CHECK(hh.SendFd == -1);
	CHECK(hh.SendFdLength == 0);
	CHECK_FALSE(hh.LengthUnknown);
	// and the descriptor it held was really closed, not merely forgotten
	CHECK(fcntl(fd, F_GETFD) == -1);
}

TEST_CASE("SendFile hands the asked stretch out of the file", "[nhttpd]")
{
	char path[] = "/tmp/ytest-sendfile-XXXXXX";
	int filed = mkstemp(path);
	REQUIRE(filed >= 0);
	unlink(path);
	REQUIRE(write(filed, "0123456789", 10) == 10);

	int fd[2];
	REQUIRE(socketpair(AF_UNIX, SOCK_STREAM, 0, fd) == 0);
	CySocket s(fd[0]);
	s.isOpened = true;
	s.isValid = true;

	char buf[16] = { 0 };
	SECTION("a window inside the file")
	{
		CHECK(s.SendFile(filed, 3, 4));
		REQUIRE(read(fd[1], buf, sizeof(buf)) == 4);
		CHECK(std::string(buf, 4) == "3456");
	}
	SECTION("no size means from the offset to the end")
	{
		CHECK(s.SendFile(filed, 6, -1));
		REQUIRE(read(fd[1], buf, sizeof(buf)) == 4);
		CHECK(std::string(buf, 4) == "6789");
	}
	SECTION("a window that is the whole file")
	{
		CHECK(s.SendFile(filed, 0, 10));
		REQUIRE(read(fd[1], buf, sizeof(buf)) == 10);
		CHECK(std::string(buf, 10) == "0123456789");
	}
	close(fd[1]);
	close(filed);
	s.close();
}

TEST_CASE("a connection the server cannot take is answered and closed", "[nhttpd]")
{
	int fd[2];
	REQUIRE(socketpair(AF_UNIX, SOCK_STREAM, 0, fd) == 0);
	CySocket *s = new CySocket(fd[0]);
	s->isOpened = true;
	s->isValid = true;

	CWebserver::refuse_connection(s); // takes the socket: answers, closes, deletes

	// with a deadline, so a refusal that forgot the close fails instead of
	// hanging the suite on a read that never ends
	std::string got;
	bool eof = false;
	const long long t_end = millisNow() + 5000;
	while (millisNow() < t_end)
	{
		struct pollfd p;
		p.fd = fd[1];
		p.events = POLLIN;
		p.revents = 0;
		if (poll(&p, 1, 200) <= 0)
			continue;
		char buf[256];
		ssize_t n = read(fd[1], buf, sizeof(buf));
		if (n > 0)
		{
			got.append(buf, (size_t) n);
			continue;
		}
		eof = (n == 0);
		break;
	}
	CHECK(eof); // EOF: the server side is really closed
	CHECK(got.find("HTTP/1.0 503 ") == 0);
	CHECK(got.find("Connection: close\r\n") != std::string::npos);
	close(fd[1]);
}

TEST_CASE("the api header of a stream states no length", "[nhttpd]")
{
	CyhookHandler hh = apiHandler(M_GET, HTTP_OK, "");
	hh.ResponseMimeType = "text/event-stream";
	hh.status = HANDLED_STREAM;
	std::string header = hh.BuildHeader();
	CHECK(header.find("Content-Length:") == std::string::npos);
	CHECK(header.find("Connection: close\r\n") != std::string::npos);
	CHECK(header.find("Content-Type: text/event-stream\r\n") != std::string::npos);
	// and only the stream mode drops it
	hh.status = HANDLED_READY;
	CHECK(hh.BuildHeader().find("Content-Length: 0\r\n") != std::string::npos);
}

TEST_CASE("the api header of a stream states the length its hook knows", "[nhttpd]")
{
	CyhookHandler hh = apiHandler(M_GET, HTTP_OK, "");
	hh.ResponseMimeType = "video/mp2t";
	hh.status = HANDLED_STREAM;
	hh.StreamLength = 1234567;
	CHECK(hh.BuildHeader().find("Content-Length: 1234567\r\n") != std::string::npos);
	// none known, none stated
	hh.StreamLength = -1;
	CHECK(hh.BuildHeader().find("Content-Length:") == std::string::npos);
	// an empty one is a length as well
	hh.StreamLength = 0;
	CHECK(hh.BuildHeader().find("Content-Length: 0\r\n") != std::string::npos);
	// the stream's length is the stream's only: a finite answer states its body
	hh.status = HANDLED_READY;
	hh.yresult = "abc";
	CHECK(hh.BuildHeader().find("Content-Length: 3\r\n") != std::string::npos);
}

namespace
{

int released_answers = 0;

void countRelease(void *p)
{
	++released_answers;
	delete (int *) p;
}

} // namespace

TEST_CASE("a fresh session carries no answer and states no stream length", "[nhttpd]")
{
	CyhookHandler hh;
	CHECK(hh.Carried == NULL);
	CHECK(hh.CarriedRelease == NULL);
	CHECK(hh.StreamLength == -1);

	hh.StreamLength = 99;
	CStringList none, config;
	hh.session_init(none, none, none, config, M_GET, false);
	CHECK(hh.Carried == NULL);
	CHECK(hh.CarriedRelease == NULL);
	CHECK(hh.StreamLength == -1);
}

TEST_CASE("a carried answer the stream pass did not take is released exactly once", "[nhttpd]")
{
	CStringList none, config;
	released_answers = 0;

	SECTION("by the next session on the handler")
	{
		CyhookHandler hh;
		hh.Carried = new int(1);
		hh.CarriedRelease = &countRelease;
		hh.session_init(none, none, none, config, M_GET, false);
		CHECK(released_answers == 1);
		CHECK(hh.Carried == NULL);
		CHECK(hh.CarriedRelease == NULL);
		hh.session_init(none, none, none, config, M_GET, false);
		CHECK(released_answers == 1);
	}
	SECTION("by the handler's end")
	{
		{
			CyhookHandler hh;
			hh.Carried = new int(2);
			hh.CarriedRelease = &countRelease;
		}
		CHECK(released_answers == 1);
	}
	SECTION("twice asked, once released")
	{
		CyhookHandler hh;
		hh.Carried = new int(3);
		hh.CarriedRelease = &countRelease;
		hh.dropCarried();
		hh.dropCarried();
		CHECK(released_answers == 1);
	}
	SECTION("not at all once the stream pass took it")
	{
		int *taken = new int(4);
		{
			CyhookHandler hh;
			hh.Carried = taken;
			hh.CarriedRelease = &countRelease;
			// what the hook does in its stream pass
			hh.Carried = NULL;
			hh.CarriedRelease = NULL;
			hh.session_init(none, none, none, config, M_GET, false);
		}
		CHECK(released_answers == 0);
		countRelease(taken);
		CHECK(released_answers == 1);
	}
}

TEST_CASE("set_stream_flush lifts the cork and stops the batching", "[nhttpd]")
{
	// a real TCP pair: TCP_CORK/TCP_NODELAY do not exist on AF_UNIX
	int lfd = socket(AF_INET, SOCK_STREAM, 0);
	REQUIRE(lfd >= 0);
	struct sockaddr_in a;
	memset(&a, 0, sizeof(a));
	a.sin_family = AF_INET;
	a.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
	REQUIRE(bind(lfd, (struct sockaddr *) &a, sizeof(a)) == 0);
	socklen_t alen = sizeof(a);
	REQUIRE(getsockname(lfd, (struct sockaddr *) &a, &alen) == 0);
	REQUIRE(listen(lfd, 1) == 0);
	int cfd = socket(AF_INET, SOCK_STREAM, 0);
	REQUIRE(connect(cfd, (struct sockaddr *) &a, sizeof(a)) == 0);
	int sfd = accept(lfd, NULL, NULL);
	REQUIRE(sfd >= 0);

	int one = 1;
	REQUIRE(setsockopt(sfd, IPPROTO_TCP, TCP_CORK, &one, sizeof(one)) == 0);

	CySocket s(sfd);
	s.isOpened = true;
	s.isValid = true;
	s.set_stream_flush();

	int v = -1;
	socklen_t vl = sizeof(v);
	REQUIRE(getsockopt(sfd, IPPROTO_TCP, TCP_CORK, &v, &vl) == 0);
	CHECK(v == 0);
	v = -1;
	vl = sizeof(v);
	REQUIRE(getsockopt(sfd, IPPROTO_TCP, TCP_NODELAY, &v, &vl) == 0);
	CHECK(v != 0);

	s.close();
	close(cfd);
	close(lfd);
}

namespace
{
struct StreamProbeHook : public Cyhook
{
	bool called;
	StreamProbeHook() : called(false) {}
	virtual std::string getHookName(void) { return "stream probe"; }
	virtual std::string getHookVersion(void) { return "0"; }
	virtual THandleStatus Hook_SendStream(CyhookHandler *, CySocket *sock)
	{
		called = true;
		sock->Send("x", 1);
		return HANDLED_STREAM;
	}
};
} // namespace

TEST_CASE("the stream hook chain reaches the hook and carries its bytes", "[nhttpd]")
{
	int fd[2];
	REQUIRE(socketpair(AF_UNIX, SOCK_STREAM, 0, fd) == 0);
	CySocket s(fd[0]);
	s.isOpened = true;
	s.isValid = true;

	StreamProbeHook probe;
	CyhookHandler::attach(&probe);
	CyhookHandler hh;
	CHECK(hh.Hooks_SendStream(&s) == HANDLED_STREAM);
	CHECK(hh.status == HANDLED_STREAM);
	CHECK(probe.called);
	CyhookHandler::detach(&probe);

	char c = 0;
	REQUIRE(read(fd[1], &c, 1) == 1);
	CHECK(c == 'x');
	close(fd[1]);
	s.close();
}

TEST_CASE("a refused TCP connection still gets its 503 past the cork", "[nhttpd]")
{
	/* The real shape of the refusal: accept() corks the socket, and the
	   client has already queued request bytes the server will never read.
	   A corked answer followed by the close would race the reset those
	   unread bytes provoke; the refusal uncorks first. Read with a
	   deadline, so a refusal that lost the race fails instead of hanging. */
	int lfd = socket(AF_INET, SOCK_STREAM, 0);
	REQUIRE(lfd >= 0);
	struct sockaddr_in a;
	memset(&a, 0, sizeof(a));
	a.sin_family = AF_INET;
	a.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
	REQUIRE(bind(lfd, (struct sockaddr *) &a, sizeof(a)) == 0);
	socklen_t alen = sizeof(a);
	REQUIRE(getsockname(lfd, (struct sockaddr *) &a, &alen) == 0);
	REQUIRE(listen(lfd, 1) == 0);
	int cfd = socket(AF_INET, SOCK_STREAM, 0);
	REQUIRE(connect(cfd, (struct sockaddr *) &a, sizeof(a)) == 0);
	int sfd = accept(lfd, NULL, NULL);
	REQUIRE(sfd >= 0);

	const char req[] = "GET / HTTP/1.0\r\nHost: box\r\n\r\n";
	REQUIRE(write(cfd, req, sizeof(req) - 1) == (ssize_t) (sizeof(req) - 1));

	int one = 1;
	REQUIRE(setsockopt(sfd, IPPROTO_TCP, TCP_CORK, &one, sizeof(one)) == 0);
	CySocket *s = new CySocket(sfd);
	s->isOpened = true;
	s->isValid = true;

	CWebserver::refuse_connection(s);

	std::string got;
	const long long t_end = millisNow() + 5000;
	while (millisNow() < t_end && got.find("\r\n\r\n") == std::string::npos)
	{
		struct pollfd p;
		p.fd = cfd;
		p.events = POLLIN;
		p.revents = 0;
		if (poll(&p, 1, 200) <= 0)
			continue;
		char buf[256];
		ssize_t n = read(cfd, buf, sizeof(buf));
		if (n <= 0)
			break;
		got.append(buf, (size_t) n);
	}
	INFO(got);
	CHECK(got.find("HTTP/1.0 503 ") == 0);
	close(cfd);
	close(lfd);
}

namespace {

// A connected pair over TCP loopback: only TCP turns a close with unread
// bytes into the reset these cases are about.
struct TcpPair
{
	int lfd, cfd, sfd;
	TcpPair() : lfd(-1), cfd(-1), sfd(-1)
	{
		lfd = socket(AF_INET, SOCK_STREAM, 0);
		struct sockaddr_in a;
		memset(&a, 0, sizeof(a));
		a.sin_family = AF_INET;
		a.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
		if (lfd < 0 || bind(lfd, (struct sockaddr *) &a, sizeof(a)) != 0)
			return;
		socklen_t alen = sizeof(a);
		if (getsockname(lfd, (struct sockaddr *) &a, &alen) != 0 || listen(lfd, 1) != 0)
			return;
		cfd = socket(AF_INET, SOCK_STREAM, 0);
		if (cfd < 0 || connect(cfd, (struct sockaddr *) &a, sizeof(a)) != 0)
			return;
		sfd = accept(lfd, NULL, NULL);
	}
	~TcpPair()
	{
		if (cfd >= 0)
			close(cfd);
		if (lfd >= 0)
			close(lfd);
	}
};

// Everything the client end reads until EOF, an error or the deadline;
// how the reading ended goes to *ended (0 EOF, -1 error, 1 deadline).
std::string readAll(int fd, int timeout_ms, int *ended)
{
	std::string got;
	*ended = 1;
	const long long t_end = millisNow() + timeout_ms;
	while (millisNow() < t_end) {
		struct pollfd p;
		p.fd = fd;
		p.events = POLLIN;
		p.revents = 0;
		if (poll(&p, 1, 100) <= 0)
			continue;
		char buf[4096];
		ssize_t n = read(fd, buf, sizeof(buf));
		if (n == 0) {
			*ended = 0;
			break;
		}
		if (n < 0) {
			*ended = -1;
			break;
		}
		got.append(buf, (size_t) n);
	}
	return got;
}

} // namespace

TEST_CASE("a lingering close lets the answer reach a caller that is still sending", "[nhttpd]")
{
	/* A refused request whose body the server does not read: the caller is
	   still sending when the answer goes out. A plain close with those bytes
	   unread resets the connection, and the caller's send fails part way
	   (measured on loopback: 40 of these 64 blocks). Most clients give up
	   at that point without reading the answer. The lingering close sends
	   the answer behind a FIN and drops what arrives until the caller is
	   done, so the caller finishes sending and then reads it. */
	TcpPair tcp;
	REQUIRE(tcp.sfd >= 0);
	const std::string chunk(65536, 'x');
	std::atomic<bool> sent_all(false);
	std::thread sender([&tcp, &chunk, &sent_all]() {
		for (int i = 0; i < 64; i++)
			if (send(tcp.cfd, chunk.data(), chunk.size(), MSG_NOSIGNAL) != (ssize_t) chunk.size())
				return;
		sent_all = true;
		shutdown(tcp.cfd, SHUT_WR);
	});

	CySocket *s = new CySocket(tcp.sfd);
	s->isOpened = true;
	s->isValid = true;
	const std::string answer = "HTTP/1.0 400 Bad Request\r\nContent-Length: 0\r\n\r\n";
	REQUIRE(s->SendAll(answer.data(), answer.size()));
	s->LingeringClose(5000, 64 * 1024 * 1024);
	delete s;

	int ended = 1;
	const std::string got = readAll(tcp.cfd, 5000, &ended);
	sender.join();
	CHECK(sent_all.load());
	CHECK(got == answer);
	CHECK(ended == 0);
}

TEST_CASE("a lingering close gives up at its deadline", "[nhttpd]")
{
	/* A caller that never stops sending holds the connection thread no
	   longer than the deadline, whatever the cap. */
	TcpPair tcp;
	REQUIRE(tcp.sfd >= 0);
	std::atomic<bool> stop(false);
	const std::string chunk(65536, 'x');
	std::thread sender([&tcp, &chunk, &stop]() {
		while (!stop)
			if (send(tcp.cfd, chunk.data(), chunk.size(), MSG_NOSIGNAL) <= 0)
				return;
	});

	CySocket *s = new CySocket(tcp.sfd);
	s->isOpened = true;
	s->isValid = true;
	const long long t0 = millisNow();
	s->LingeringClose(300, (size_t) -1);
	const long long took = millisNow() - t0;
	delete s;
	stop = true;
	shutdown(tcp.cfd, SHUT_RDWR);
	sender.join();
	CHECK(took >= 250);
	CHECK(took < 2000);
}

TEST_CASE("a stop wakes the connection threads and waits for every one of them", "[nhttpd]")
{
	/* The restart seam deletes the hooks and the server after the stop, so
	   no connection thread may still be inside them. A caller that sent half
	   a legacy request holds its thread in a header read without a deadline;
	   the stop has to wake that thread and wait for it, and a wait that only
	   waited would sit here until the caller gave up. (A GET does not do
	   for this: its header read takes what is there and goes on.) */
	const bool threading_was = CWebserver::is_threading;
	CWebserver ws;
	const int port = 34612;
	ws.init((unsigned) port, true, "127.0.0.1");

	pthread_t runner;
	struct Runner
	{
		static void *run(void *arg)
		{
			((CWebserver *) arg)->run();
			return NULL;
		}
	};
	REQUIRE(pthread_create(&runner, NULL, &Runner::run, &ws) == 0);

	struct sockaddr_in to;
	std::memset(&to, 0, sizeof(to));
	to.sin_family = AF_INET;
	to.sin_port = htons((unsigned short) port);
	to.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
	int cfd = -1;
	for (int i = 0; i < 100 && cfd < 0; i++)
	{
		usleep(50 * 1000);
		int fd = socket(AF_INET, SOCK_STREAM, 0);
		REQUIRE(fd >= 0);
		if (connect(fd, (struct sockaddr *) &to, sizeof(to)) == 0)
			cfd = fd;
		else
			close(fd);
	}
	REQUIRE(cfd >= 0);

	// half a request: a POST's line, and never the empty line after it; a
	// POST reads its header line by line, and that read has no deadline
	const char half[] = "POST /nothing HTTP/1.0\r\n";
	REQUIRE(write(cfd, half, sizeof(half) - 1) == (ssize_t) (sizeof(half) - 1));
	usleep(300 * 1000);
	// the thread that took the connection is still there, waiting
	CHECK_FALSE(ws.waitForConnections(0));

	ws.stop();
	struct timespec until;
	clock_gettime(CLOCK_REALTIME, &until);
	until.tv_sec += 3;
	const int joined = pthread_timedjoin_np(runner, NULL, &until);
	if (joined != 0)
		pthread_cancel(runner);
	REQUIRE(joined == 0);

	const long long t0 = millisNow();
	const bool drained = ws.waitForConnections(3000);
	const long long took = millisNow() - t0;

	// whatever the outcome, no thread may outlive the server it belongs to
	close(cfd);
	const bool drained_after_close = ws.waitForConnections(3000);
	CWebserver::is_threading = threading_was;
	httpd::setBoundPortFromBridge(0);

	CHECK(drained);
	CHECK(took < 1000);
	REQUIRE(drained_after_close);
}

TEST_CASE("a POST whose caller closes inside the header ends the request", "[nhttpd]")
{
	/* The header of a legacy POST is read line by line up to the empty
	   line. A read at the end of the connection used to hand back a byte
	   that never arrived, so the loop never met an empty line and grew the
	   header until the box ran out of memory. Watched: a regression here
	   ends the whole run instead of eating the machine. */
	int fd[2];
	REQUIRE(socketpair(AF_UNIX, SOCK_STREAM, 0, fd) == 0);
	CySocket *s = new CySocket(fd[0]);
	s->isOpened = true;
	s->isValid = true;
	const char req[] = "POST /nothing HTTP/1.0\r\nHost: box\r\n";
	REQUIRE(write(fd[1], req, sizeof(req) - 1) == (ssize_t) (sizeof(req) - 1));
	close(fd[1]);

	std::atomic<bool> done(false);
	std::thread watchdog([&done]() {
		for (int i = 0; i < 500 && !done; i++)
			usleep(10 * 1000);
		if (!done) {
			fprintf(stderr, "a POST cut inside its header did not end in 5 s\n");
			_exit(1);
		}
	});

	CWebserver ws;
	CWebserverConnection *con = new CWebserverConnection(&ws);
	con->Request.UrlData["clientaddr"] = "127.0.0.1";
	con->sock = s;
	const bool handled = con->Request.HandleRequest();
	done = true;
	watchdog.join();
	delete con;
	s->close();
	delete s;
	CHECK_FALSE(handled);
}

/* A legacy POST whose caller is gone before its body is complete, handed
   to HandleRequest() over a socket pair. Watched like the header case: a
   loop that never ends fails the whole run in 5 s instead of spinning. */
static bool handleCutPost(const std::string &request)
{
	int fd[2];
	REQUIRE(socketpair(AF_UNIX, SOCK_STREAM, 0, fd) == 0);
	CySocket *s = new CySocket(fd[0]);
	s->isOpened = true;
	s->isValid = true;
	REQUIRE(write(fd[1], request.data(), request.size()) == (ssize_t) request.size());
	close(fd[1]);

	std::atomic<bool> done(false);
	std::thread watchdog([&done]() {
		for (int i = 0; i < 500 && !done; i++)
			usleep(10 * 1000);
		if (!done) {
			fprintf(stderr, "a POST whose caller went away did not end in 5 s\n");
			_exit(1);
		}
	});

	CWebserver ws;
	CWebserverConnection *con = new CWebserverConnection(&ws);
	con->Request.UrlData["clientaddr"] = "127.0.0.1";
	con->sock = s;
	const bool handled = con->Request.HandleRequest();
	done = true;
	watchdog.join();
	delete con;
	s->close();
	delete s;
	return handled;
}

namespace {

/* The box's names read from a resolver file of the case's own, so a name is
   refused the same way on every host; put back at the end. */
struct NamedSources
{
	std::string dir, resolv;

	NamedSources()
	{
		char d[] = "/tmp/test_post_XXXXXX";
		REQUIRE(mkdtemp(d) != NULL);
		dir = d;
		resolv = dir + "/resolv.conf";
		std::FILE *f = std::fopen(resolv.c_str(), "w");
		REQUIRE(f != NULL);
		std::fputs("nameserver 192.168.1.1\nsearch fritz.box\n", f);
		REQUIRE(std::fclose(f) == 0);
		httpd::setNameSourcesForTest("h7", resolv);
	}
	~NamedSources()
	{
		httpd::setNameSourcesForTest("", "");
		::unlink(resolv.c_str());
		::rmdir(dir.c_str());
	}
};

/* A legacy POST handed whole to HandleRequest() over a socket pair, written
   from a thread of its own, the caller's end shut for writing behind it. What
   the reader answered, the bytes of the request it left unread, and what it
   parsed. The reader's end is shut for reading once it is done, so a refused
   request larger than the pair holds ends the writer with EPIPE and fails the
   case on CHECK(sent) instead of keeping the writer in send(). */
struct Posted
{
	bool handled;
	std::string answer;
	int unread;
	// whether the reader left the connection to close by lingering
	bool lingered;
	CStringList params;
};

Posted postWhole(const std::string &request)
{
	Posted out;
	out.handled = false;
	out.unread = -1;
	out.lingered = false;
	int fd[2];
	REQUIRE(socketpair(AF_UNIX, SOCK_STREAM, 0, fd) == 0);
	int big = 1 << 20;
	setsockopt(fd[1], SOL_SOCKET, SO_SNDBUF, &big, sizeof(big));
	CySocket *s = new CySocket(fd[0]);
	s->isOpened = true;
	s->isValid = true;
	std::atomic<bool> sent(false);
	std::thread writer([&fd, &request, &sent]() {
		size_t at = 0;
		while (at < request.size()) {
			ssize_t n = send(fd[1], request.data() + at, request.size() - at, MSG_NOSIGNAL);
			if (n <= 0)
				break;
			at += (size_t) n;
		}
		sent = at == request.size();
		shutdown(fd[1], SHUT_WR);
	});

	std::atomic<bool> done(false);
	std::thread watchdog([&done]() {
		for (int i = 0; i < 3000 && !done; i++)
			usleep(10 * 1000);
		if (!done) {
			fprintf(stderr, "a POST handed whole did not end in 30 s\n");
			_exit(1);
		}
	});

	CWebserver ws;
	CWebserverConnection *con = new CWebserverConnection(&ws);
	con->Request.UrlData["clientaddr"] = "192.168.1.20";
	con->Request.UrlData["localaddr"] = "192.168.1.5:80";
	con->sock = s;
	out.handled = con->Request.HandleRequest();
	shutdown(fd[0], SHUT_RD);
	writer.join();
	done = true;
	watchdog.join();
	CHECK(sent.load());
	ioctl(fd[0], FIONREAD, &out.unread);
	out.params = con->Request.ParameterList;
	out.lingered = s->linger_on_close;
	delete con;
	s->close();
	delete s;

	fcntl(fd[1], F_SETFL, O_NONBLOCK);
	char buf[4096];
	ssize_t n;
	while ((n = read(fd[1], buf, sizeof(buf))) > 0)
		out.answer.append(buf, (size_t) n);
	close(fd[1]);
	return out;
}

const char kUploadBody[] =
	"--b\r\n"
	"Content-Disposition: form-data; name=\"f\"; filename=\"services.xml\"\r\n"
	"Content-Type: text/xml\r\n"
	"\r\n"
	"<zapit>\r\n</zapit>\r\n"
	"--b--\r\n";

const char kFormBody[] = "a=b&c=d";

// A POST of body, with its length, to /control/upload under the given header lines.
std::string postRequest(const std::string &headers, const char *type, const std::string &body)
{
	return "POST /control/upload HTTP/1.1\r\n" + headers + "Content-Type: " + type + "\r\n"
		"Content-Length: " + std::to_string(body.size()) + "\r\n\r\n" + body;
}

} // namespace

TEST_CASE("a POST under a name that is not this box is refused before a byte of its body is read", "[nhttpd][authority]")
{
	/* The hooks see a request once it has been read: the request boundary
	   is asked before the body. Refused with the boundary's own page, the
	   body left unread. */
	NamedSources names;
	{
		Posted ok = postWhole(postRequest("Host: h7\r\n", "application/x-www-form-urlencoded", kFormBody));
		CHECK(ok.handled);
		CHECK_FALSE(ok.lingered);
		CHECK(ok.answer == "HTTP/1.1 100 Continue\r\n\r\n");
		CHECK(ok.params["a"] == "b");
		CHECK(ok.params["c"] == "d");
	}

	const char *const refused[] = {
		"Host: evil.example\r\n",
		"Host: h7.evil.example:80\r\n",
		"Host: h7\r\nhost: h7\r\n",
		"host: evil.example\r\nHost: h7\r\n",
	};
	const char *const types[][2] = {
		{ "application/x-www-form-urlencoded", kFormBody },
		{ "multipart/form-data; boundary=b", kUploadBody },
	};
	for (size_t i = 0; i < sizeof(refused) / sizeof(refused[0]); ++i)
		for (size_t t = 0; t < sizeof(types) / sizeof(types[0]); ++t)
		{
			INFO(refused[i] << types[t][0]);
			const std::string body = types[t][1];
			const Posted p = postWhole(postRequest(refused[i], types[t][0], body));
			CHECK_FALSE(p.handled);
			CHECK(p.answer.find("HTTP/1.0 400") == 0);
			CHECK(p.answer.find("Content-Type: text/plain; charset=UTF-8\r\n") != std::string::npos);
			CHECK(p.answer.find("X-Content-Type-Options: nosniff\r\n") != std::string::npos);
			CHECK(p.answer.find("allowed_hosts") != std::string::npos);
			CHECK(p.answer.find("http://192.168.1.5/") != std::string::npos);
			CHECK(p.answer.find("100 Continue") == std::string::npos);
			CHECK(p.unread == (int) body.size());
			// the body is still on its way in: a plain close could reset the answer away
			CHECK(p.lingered);
			CHECK(p.params.find("a") == p.params.end());
		}
}

TEST_CASE("a multipart POST outside the API is refused before a byte of its body is read", "[nhttpd]")
{
	/* This server takes no uploads, so a multipart body is never read: the
	   web interface sends its files through the API, and nothing reads a
	   file such a POST brought. It was written to a file as it arrived,
	   before the login, of whatever length the caller chose. Refused with
	   415 whatever its length says, and the body left unread. */
	NamedSources names;
	const std::string length = "Content-Length: " + std::to_string(sizeof(kUploadBody) - 1) + "\r\n";
	const char *const heads[] = {
		"Content-Type: multipart/form-data; boundary=b\r\n",
		"Content-Type: Multipart/Form-Data; boundary=b\r\n",
		"content-type: multipart/form-data; boundary=b\r\n",
		"Content-Type: multipart/mixed; boundary=b\r\n",
	};
	const std::string lengths[] = {
		length, "", "Content-Length: 16777217\r\n", "Content-Length: 4294967296\r\n",
		"Content-Length: -1\r\n", "Transfer-Encoding: chunked\r\n",
	};
	for (size_t h = 0; h < sizeof(heads) / sizeof(heads[0]); ++h)
		for (size_t l = 0; l < sizeof(lengths) / sizeof(lengths[0]); ++l)
		{
			INFO(heads[h] << lengths[l]);
			const Posted p = postWhole("POST /control/upload HTTP/1.1\r\nHost: 192.168.1.5\r\n" +
				std::string(heads[h]) + lengths[l] + "\r\n" + kUploadBody);
			CHECK_FALSE(p.handled);
			CHECK(p.answer.find("HTTP/1.0 415") == 0);
			CHECK(p.answer.find("100 Continue") == std::string::npos);
			CHECK(p.unread == (int) (sizeof(kUploadBody) - 1));
			CHECK(p.lingered);
			CHECK(p.params.empty());
		}
}

TEST_CASE("a form POST outside the API is read only by one length of at most 1 MiB", "[nhttpd]")
{
	/* A form is read into memory before any hook, the login among them,
	   looks at the request, and its length was taken as the caller wrote it:
	   up to 4 GB, or a negative one read as that. Settled before a byte of
	   it is read, as on an API address. */
	CHECK(POST_FORM_MAX_LENGTH == 1024 * 1024);
	NamedSources names;
	struct Refused
	{
		std::string length_lines;
		const char *status;
	};
	const Refused cases[] = {
		{ "Content-Length: " + std::to_string(POST_FORM_MAX_LENGTH + 1) + "\r\n", "HTTP/1.0 413" },
		{ "Content-Length: 4294967296\r\n", "HTTP/1.0 413" },
		{ "Content-Length: -1\r\n", "HTTP/1.0 400" },
		{ "Content-Length: 3, 3\r\n", "HTTP/1.0 400" },
		{ "Content-Length: 3\r\nContent-Length: 3\r\n", "HTTP/1.0 400" },
		{ "Transfer-Encoding: chunked\r\n", "HTTP/1.0 400" },
		// without a length only the end of the connection would say where it ends
		{ "", "HTTP/1.0 411" },
	};
	for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); ++i)
	{
		INFO(cases[i].length_lines);
		const Posted p = postWhole("POST /control/x HTTP/1.1\r\nHost: 192.168.1.5\r\n"
			"Content-Type: application/x-www-form-urlencoded\r\n" + cases[i].length_lines + "\r\na=b");
		CHECK_FALSE(p.handled);
		CHECK(p.answer.find(cases[i].status) == 0);
		CHECK(p.answer.find("100 Continue") == std::string::npos);
		CHECK(p.unread == 3);
		// the body is still on its way in: a plain close could reset the answer away
		CHECK(p.lingered);
		CHECK(p.params.find("a") == p.params.end());
	}

	// a form of the whole 1 MiB is read and parsed
	const std::string body = "a=" + std::string(POST_FORM_MAX_LENGTH - 2, 'x');
	Posted whole = postWhole(postRequest("Host: 192.168.1.5\r\n", "application/x-www-form-urlencoded", body));
	CHECK(whole.handled);
	CHECK_FALSE(whole.lingered);
	CHECK(whole.params["a"].size() == (size_t) POST_FORM_MAX_LENGTH - 2);
	CHECK(whole.unread == 0);

	// and of a longer one exactly its length, whatever else came with it
	Posted shorter = postWhole("POST /control/x HTTP/1.1\r\nHost: 192.168.1.5\r\n"
		"Content-Type: application/x-www-form-urlencoded\r\nContent-Length: 3\r\n\r\na=b&c=d");
	CHECK(shorter.handled);
	CHECK(shorter.params["a"] == "b");
	CHECK(shorter.params.find("c") == shorter.params.end());
	CHECK(shorter.unread == 4);
}

TEST_CASE("a form POST outside the API that stops arriving ends at the deadline", "[nhttpd]")
{
	/* Its body was read until it was all there or the caller went away, so a
	   caller that sent part of it and then nothing held a thread of the
	   server, before the login, for as long as it kept the connection open.
	   Read as an API body is, it ends with 408 after 10 seconds. */
	NamedSources names;
	int fd[2];
	REQUIRE(socketpair(AF_UNIX, SOCK_STREAM, 0, fd) == 0);
	const std::string request = "POST /control/x HTTP/1.1\r\nHost: 192.168.1.5\r\n"
		"Content-Type: application/x-www-form-urlencoded\r\nContent-Length: 100\r\n\r\na=b";
	REQUIRE(write(fd[1], request.data(), request.size()) == (ssize_t) request.size());
	// the caller stays for 13 s and then goes, which the reader must not wait for
	std::thread caller([&fd]() {
		usleep(13 * 1000 * 1000);
		shutdown(fd[1], SHUT_WR);
	});

	CySocket *s = new CySocket(fd[0]);
	s->isOpened = true;
	s->isValid = true;
	CWebserver ws;
	CWebserverConnection *con = new CWebserverConnection(&ws);
	con->Request.UrlData["clientaddr"] = "192.168.1.20";
	con->Request.UrlData["localaddr"] = "192.168.1.5:80";
	con->sock = s;
	struct timespec t0, t1;
	clock_gettime(CLOCK_MONOTONIC, &t0);
	const bool handled = con->Request.HandleRequest();
	clock_gettime(CLOCK_MONOTONIC, &t1);
	caller.join();
	const double took = (double) (t1.tv_sec - t0.tv_sec) + (double) (t1.tv_nsec - t0.tv_nsec) / 1e9;
	CHECK_FALSE(handled);
	CHECK(took >= 9.5);
	CHECK(took < 12.0);
	CHECK(con->Request.ParameterList.find("a") == con->Request.ParameterList.end());
	delete con;
	s->close();
	delete s;

	fcntl(fd[1], F_SETFL, O_NONBLOCK);
	std::string answer;
	char buf[4096];
	ssize_t n;
	while ((n = read(fd[1], buf, sizeof(buf))) > 0)
		answer.append(buf, (size_t) n);
	close(fd[1]);
	CHECK(answer.find("HTTP/1.0 408") != std::string::npos);
}

TEST_CASE("a form POST whose caller sends less than it announced ends the request", "[nhttpd]")
{
	/* The body of a form POST was read in blocks until the declared length
	   had arrived. A block read with nothing waiting comes back empty at
	   once, so a caller that announced more than it sent and closed held
	   one thread at full load for good, and one that was merely slow kept
	   it spinning until the rest came. */
	CHECK_FALSE(handleCutPost("POST /nothing HTTP/1.0\r\nHost: box\r\n"
			"Content-Type: application/x-www-form-urlencoded\r\n"
			"Content-Length: 100\r\n\r\na=b"));
	CHECK_FALSE(handleCutPost("POST /nothing HTTP/1.0\r\nHost: box\r\n"
			"Content-Type: application/x-www-form-urlencoded\r\n"
			"Content-Length: 100\r\n\r\n"));
}

TEST_CASE("a method this server does not serve gets its 501 past the unread header", "[nhttpd]")
{
	/* OPTIONS, and a PUT outside the API, are refused right after the
	   request line, with the rest of the request unread. A plain close then
	   resets the connection while the caller is still sending its header,
	   and the caller gives up without the 501. */
	TcpPair tcp;
	REQUIRE(tcp.sfd >= 0);
	std::string request = "OPTIONS / HTTP/1.0\r\n";
	for (int i = 0; i < 4000; i++)
		request += "X-Padding-" + std::to_string(i) + ": " + std::string(1000, 'p') + "\r\n";
	request += "\r\n";
	std::atomic<bool> sent_all(false);
	std::thread sender([&tcp, &request, &sent_all]() {
		size_t done = 0;
		while (done < request.size()) {
			ssize_t n = send(tcp.cfd, request.data() + done, request.size() - done, MSG_NOSIGNAL);
			if (n <= 0)
				return;
			done += (size_t) n;
		}
		sent_all = true;
		shutdown(tcp.cfd, SHUT_WR);
	});

	CWebserver ws;
	CWebserverConnection *con = new CWebserverConnection(&ws);
	CySocket *s = new CySocket(tcp.sfd);
	s->isOpened = true;
	s->isValid = true;
	con->Request.UrlData["clientaddr"] = "127.0.0.1";
	con->sock = s;
	con->HandleConnection();
	delete con;
	delete s;

	int ended = 1;
	const std::string got = readAll(tcp.cfd, 5000, &ended);
	sender.join();
	CHECK(sent_all.load());
	CHECK(got.find("HTTP/1.0 501") == 0);
}

TEST_CASE("an api answer without content states no length", "[nhttpd]")
{
	// RFC 9110 15.3.5: a 204 carries no Content-Length; the logout answers
	// with one, and a strict client or proxy rejects the answer for it
	CyhookHandler hh = apiHandler(M_POST, HTTP_NO_CONTENT, "");
	const std::string h = hh.BuildApiHeader();
	CHECK(h.find("HTTP/1.0 204") == 0);
	CHECK(h.find("Content-Length") == std::string::npos);
	CHECK(h.substr(h.size() - 4) == "\r\n\r\n");
}

namespace
{

// A connection to the wire server, with what is to be sent at once.
int headPeer(int port, const std::string &sent)
{
	const int fd = socket(AF_INET, SOCK_STREAM, 0);
	REQUIRE(fd >= 0);
	struct sockaddr_in a;
	std::memset(&a, 0, sizeof(a));
	a.sin_family = AF_INET;
	a.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
	a.sin_port = htons((uint16_t) port);
	REQUIRE(connect(fd, (struct sockaddr *) &a, sizeof(a)) == 0);
	if (!sent.empty())
		REQUIRE(send(fd, sent.data(), sent.size(), MSG_NOSIGNAL) == (ssize_t) sent.size());
	return fd;
}

// Everything that comes back until the peer closes or the wait is over.
std::string headAnswer(int fd, int wait_ms)
{
	std::string got;
	const int64_t until = wireNowMs() + wait_ms;
	for (;;)
	{
		const int64_t left = until - wireNowMs();
		if (left <= 0)
			break;
		struct pollfd p;
		p.fd = fd;
		p.events = POLLIN;
		p.revents = 0;
		if (poll(&p, 1, (int) left) <= 0)
			break;
		char buf[4096];
		const ssize_t n = recv(fd, buf, sizeof(buf), 0);
		if (n <= 0)
			break;
		got.append(buf, (size_t) n);
	}
	return got;
}

} // namespace

TEST_CASE("a peer that sends nothing is let go at the deadline, and the next request is answered", "[nhttpd][head]")
{
	/* Without connection threads, as a box serves by default: the accept thread
	   serves one connection at a time, so a peer that connects and says nothing
	   holds every other request until it is let go, at 10 seconds. */
	WireServer serving(1, 5000, WireServer::Serial);
	const int64_t t0 = wireNowMs();
	const int silent = headPeer(serving.port, "");
	const int next = headPeer(serving.port, "GET /api/v1/session HTTP/1.0\r\nHost: box\r\n\r\n");

	const std::string answered = headAnswer(next, 15000);
	const int64_t took = wireNowMs() - t0;
	INFO(answered.substr(0, answered.find("\r\n")));
	CHECK(answered.find(" 200 ") != std::string::npos);
	CHECK(answered.find(" 200 ") < answered.find("\r\n"));
	CHECK(took >= 9500);
	CHECK(took < 12500);
	const std::string dropped = headAnswer(silent, 3000);
	CHECK(dropped.find(" 408 ") != std::string::npos);
	close(silent);
	close(next);
}

TEST_CASE("a POST whose header stops coming is let go at the deadline", "[nhttpd][head]")
{
	WireServer serving(1, 5000, WireServer::Threaded);
	const int64_t t0 = wireNowMs();
	const int stalled = headPeer(serving.port, "POST /control/info HTTP/1.1\r\nHost: box\r\n");
	const std::string answered = headAnswer(stalled, 15000);
	const int64_t took = wireNowMs() - t0;
	CHECK(answered.find(" 408 ") != std::string::npos);
	CHECK(took >= 9500);
	CHECK(took < 13000);
	close(stalled);
}

TEST_CASE("header lines without end are refused at the head's limit", "[nhttpd][head]")
{
	/* 64 KiB for the request line and the header together, whatever the method,
	   reached here long before the deadline. */
	WireServer serving(1, 5000, WireServer::Threaded);
	const int64_t t0 = wireNowMs();
	const int flood = headPeer(serving.port, "POST /control/info HTTP/1.1\r\nHost: box\r\n");
	const std::string line = "X-Filler: " + std::string(54, 'x') + "\r\n";
	std::thread writer([flood, &line]() {
		for (int i = 0; i < 4096; ++i)
			if (send(flood, line.data(), line.size(), MSG_NOSIGNAL) <= 0)
				break;
	});
	const std::string answered = headAnswer(flood, 8000);
	const int64_t took = wireNowMs() - t0;
	shutdown(flood, SHUT_RDWR);
	writer.join();
	CHECK(answered.find(" 400 ") != std::string::npos);
	CHECK(took < 8000);
	close(flood);

	// a request line longer than the limit, before any header
	const int far = headPeer(serving.port, "GET /control/info?" + std::string(70 * 1024, 'a'));
	CHECK(headAnswer(far, 8000).find(" 414 ") != std::string::npos);
	close(far);
}

namespace
{

// What the reader made of a head sent in pieces, each after its pause.
struct HeadRun
{
	bool        handled;
	std::string answer;
	// what was still on the socket after the reader was done: nothing of the
	// head, everything after it
	std::string left;
	CStringList params;
	CStringList headers;
	long long   took_ms;
};

HeadRun runHead(const std::vector<std::pair<int, std::string> > &pieces)
{
	HeadRun out;
	out.handled = false;
	out.took_ms = 0;
	int fd[2];
	REQUIRE(socketpair(AF_UNIX, SOCK_STREAM, 0, fd) == 0);
	int big = 1 << 20;
	setsockopt(fd[1], SOL_SOCKET, SO_SNDBUF, &big, sizeof(big));
	CySocket *s = new CySocket(fd[0]);
	s->isOpened = true;
	s->isValid = true;
	std::atomic<bool> read_done(false);
	std::thread writer([&fd, &pieces, &read_done]() {
		for (size_t i = 0; i < pieces.size() && !read_done; i++) {
			for (int waited = 0; waited < pieces[i].first && !read_done; waited += 10)
				usleep(10 * 1000);
			if (read_done)
				break;
			send(fd[1], pieces[i].second.data(), pieces[i].second.size(), MSG_NOSIGNAL);
		}
	});

	CWebserver ws;
	CWebserverConnection *con = new CWebserverConnection(&ws);
	con->Request.UrlData["clientaddr"] = "192.168.1.20";
	con->Request.UrlData["localaddr"] = "192.168.1.5:80";
	con->sock = s;
	const int64_t t0 = wireNowMs();
	out.handled = con->Request.HandleRequest();
	out.took_ms = wireNowMs() - t0;
	read_done = true;
	writer.join();
	// what the reader left on the socket
	char buf[4096];
	ssize_t n;
	while ((n = recv(fd[0], buf, sizeof(buf), MSG_DONTWAIT)) > 0)
		out.left.append(buf, (size_t) n);
	out.params = con->Request.ParameterList;
	out.headers = con->Request.HeaderList;
	delete con;
	s->close();
	delete s;
	fcntl(fd[1], F_SETFL, O_NONBLOCK);
	while ((n = read(fd[1], buf, sizeof(buf))) > 0)
		out.answer.append(buf, (size_t) n);
	close(fd[1]);
	return out;
}

HeadRun runHead(const std::string &whole)
{
	return runHead(std::vector<std::pair<int, std::string> >(1, std::make_pair(0, whole)));
}

/* A head of exactly total bytes on the wire: lead (empty lines before the request
   line), the request line with its padding, a filler header and the empty line,
   every line ending in eol. */
std::string headOf(size_t total, const std::string &eol, const std::string &lead, const std::string &target,
                   size_t request_pad)
{
	const std::string line = "GET " + target + " HTTP/1.0" + std::string(request_pad, ' ') + eol;
	const std::string name = "X-Fill: ";
	const size_t fixed = lead.size() + line.size() + name.size() + eol.size() + eol.size();
	REQUIRE(total > fixed);
	return lead + line + name + std::string(total - fixed, 'a') + eol + eol;
}

} // namespace

TEST_CASE("the head's 64 KiB are counted on the wire, empty lines, padding and line endings included", "[nhttpd][head]")
{
	RegisteredPrefixes api;
	const size_t L = 64 * 1024;
	struct Shape
	{
		const char *what;
		const char *eol;
		const char *lead;
		const char *target;
		size_t pad;
	};
	const Shape shapes[] = {
		{ "CRLF", "\r\n", "", "/x", 0 },
		{ "LF alone", "\n", "", "/x", 0 },
		{ "empty lines before the request line", "\r\n", "\r\n\r\n\n\r\n", "/x", 0 },
		{ "a request line padded with spaces", "\r\n", "", "/x", 3000 },
		{ "an API target", "\r\n", "", "/api/v1/session", 0 },
	};
	for (size_t i = 0; i < sizeof(shapes) / sizeof(shapes[0]); ++i)
	{
		if (std::string(shapes[i].target).compare(0, 5, "/api/") == 0 && std::string(shapes[i].eol) == "\n")
			continue;
		for (int d = -1; d <= 1; ++d)
		{
			INFO(shapes[i].what << ", " << (L + d) << " bytes");
			const std::string head = headOf(L + d, shapes[i].eol, shapes[i].lead, shapes[i].target, shapes[i].pad);
			REQUIRE(head.size() == L + d);
			// what follows the head stays where the next reader would look
			HeadRun r = runHead(head + "SENTINEL");
			if (d <= 0)
			{
				CHECK(r.handled);
				CHECK(r.answer.empty());
				CHECK(r.left == "SENTINEL");
				CHECK(r.headers["X-Fill"].size() > 1000);
			}
			else
			{
				CHECK_FALSE(r.handled);
				CHECK(r.answer.compare(0, 13, "HTTP/1.0 400 ") == 0);
			}
		}
	}

	// a request line longer than all of it
	HeadRun far = runHead("GET /x?" + std::string(L, 'a') + " HTTP/1.0\r\n\r\n");
	CHECK_FALSE(far.handled);
	CHECK(far.answer.compare(0, 13, "HTTP/1.0 414 ") == 0);
}

TEST_CASE("line endings split across reads are read whole, and the body and what follows it stay as sent", "[nhttpd][head]")
{
	RegisteredPrefixes api;
	struct Split
	{
		const char *what;
		std::vector<std::string> parts;
		const char *left;
	};
	const char sentinel[] = "GET /pipelined HTTP/1.1\r\n\r\n";
	std::vector<Split> splits;
	{
		Split a;
		a.what = "a form POST, CRLF split";
		a.parts.push_back("POST /control/info HTTP/1.1\r");
		a.parts.push_back("\nHost: box\r");
		a.parts.push_back("\nContent-Type: application/x-www-form-urlencoded\r\nContent-Length: 3\r");
		a.parts.push_back("\n\r");
		a.parts.push_back(std::string("\na=b") + sentinel);
		a.left = sentinel;
		splits.push_back(a);
		Split b;
		b.what = "a form POST, LF alone";
		b.parts.push_back("POST /control/info HTTP/1.1\n");
		b.parts.push_back("Host: box\nContent-Type: application/x-www-form-urlencoded\n");
		b.parts.push_back("Content-Length: 3\n");
		b.parts.push_back("\n");
		b.parts.push_back(std::string("a=b") + sentinel);
		b.left = sentinel;
		splits.push_back(b);
		Split c;
		c.what = "a GET";
		c.parts.push_back("GET /x HTTP/1.0\r");
		c.parts.push_back("\nHost: box\r");
		c.parts.push_back("\n\r");
		c.parts.push_back("\nSENTINEL\r\n");
		c.left = "SENTINEL\r\n";
		splits.push_back(c);
		Split d;
		d.what = "an API GET";
		d.parts.push_back("GET /api/v1/session HTTP/1.0\r");
		d.parts.push_back("\nHost: box\r\n\r");
		d.parts.push_back("\nSENTINEL");
		d.left = "SENTINEL";
		splits.push_back(d);
	}
	for (size_t i = 0; i < splits.size(); ++i)
	{
		INFO(splits[i].what);
		std::vector<std::pair<int, std::string> > pieces;
		for (size_t k = 0; k < splits[i].parts.size(); ++k)
			pieces.push_back(std::make_pair(k == 0 ? 0 : 60, splits[i].parts[k]));
		HeadRun r = runHead(pieces);
		CHECK(r.handled);
		CHECK(r.left == splits[i].left);
		CHECK(r.headers["Host"] == "box");
		if (std::string(splits[i].what).find("POST") != std::string::npos)
			CHECK(r.params["a"] == "b");
	}
}

TEST_CASE("a head that comes in slowly has 10 seconds in all, not 10 for each line", "[nhttpd][head]")
{
	// the request line after 4 seconds, a header after 8 and the empty line after 12: too late
	std::vector<std::pair<int, std::string> > late;
	late.push_back(std::make_pair(4000, std::string("GET /x HTTP/1.0\r\n")));
	late.push_back(std::make_pair(4000, std::string("X-A: b\r\n")));
	late.push_back(std::make_pair(4000, std::string("\r\n")));
	HeadRun r = runHead(late);
	CHECK_FALSE(r.handled);
	CHECK(r.answer.compare(0, 13, "HTTP/1.0 408 ") == 0);
	CHECK(r.took_ms >= 9500);
	CHECK(r.took_ms < 11500);

	// the same within the 10 seconds, and on the API, where a head out of time is 408 as well
	RegisteredPrefixes api;
	std::vector<std::pair<int, std::string> > in_time;
	in_time.push_back(std::make_pair(2000, std::string("GET /x HTTP/1.0\r\n")));
	in_time.push_back(std::make_pair(2000, std::string("X-A: b\r\n")));
	in_time.push_back(std::make_pair(2000, std::string("\r\n")));
	HeadRun ok = runHead(in_time);
	CHECK(ok.handled);
	CHECK(ok.headers["X-A"] == "b");
	std::vector<std::pair<int, std::string> > api_late;
	api_late.push_back(std::make_pair(0, std::string("GET /api/v1/session HTTP/1.0\r\nHost: box\r\n")));
	HeadRun a = runHead(api_late);
	CHECK_FALSE(a.handled);
	CHECK(a.answer.compare(0, 13, "HTTP/1.0 408 ") == 0);
}

namespace
{

// Answers /test/big with a body of many megabytes out of memory, and /test/bigfile
// with as many out of a file, for a reader that stops taking them.
struct BigAnswers : public Cyhook
{
	std::string body;
	std::string file;
	off_t       file_size;

	BigAnswers() : file_size(0) {}
	virtual std::string getHookName(void) { return "big answers"; }
	virtual THandleStatus Hook_SendResponse(CyhookHandler *hh)
	{
		const std::string url = hh->UrlData["url"];
		if (url == "/test/big") {
			hh->SetHeader(HTTP_OK, "application/octet-stream");
			hh->yresult = body;
			hh->status = HANDLED_READY;
			return HANDLED_READY;
		}
		if (url == "/test/bigfile") {
			hh->SetHeader(HTTP_OK, "application/octet-stream");
			hh->SendFd = ::open(file.c_str(), O_RDONLY | O_CLOEXEC);
			hh->SendFdOffset = 0;
			hh->SendFdLength = file_size;
			hh->status = HANDLED_SENDFD;
			return HANDLED_SENDFD;
		}
		return HANDLED_NONE;
	}
};

// A connection that asks for target and takes almost nothing of the answer.
int stalledReader(int port, const char *target)
{
	const int fd = socket(AF_INET, SOCK_STREAM, 0);
	REQUIRE(fd >= 0);
	int small = 4096;
	setsockopt(fd, SOL_SOCKET, SO_RCVBUF, &small, sizeof(small));
	struct sockaddr_in a;
	std::memset(&a, 0, sizeof(a));
	a.sin_family = AF_INET;
	a.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
	a.sin_port = htons((uint16_t) port);
	REQUIRE(connect(fd, (struct sockaddr *) &a, sizeof(a)) == 0);
	const std::string req = std::string("GET ") + target + " HTTP/1.0\r\nHost: box\r\n\r\n";
	REQUIRE(send(fd, req.data(), req.size(), MSG_NOSIGNAL) == (ssize_t) req.size());
	return fd;
}

} // namespace

TEST_CASE("a reader that takes nothing more of an answer is let go once nothing moved for the send limit", "[nhttpd][send]")
{
	/* Without connection threads: the accept thread writes the answer itself, so a
	   reader that stops taking it used to hold every other request for as long as
	   it stayed connected. With the limit at 2 seconds here, the next request is
	   answered about 2 seconds after the writes stopped moving, for an answer out of
	   memory and for one out of a file alike. */
	CHECK(CySocket::sendIdleMs() == 30000);
	CySocket::setSendIdleMsForTest(2000);
	BigAnswers big;
	big.body.assign(32u << 20, 'x');
	char name[] = "/tmp/test_send_XXXXXX";
	const int tmp = mkstemp(name);
	REQUIRE(tmp >= 0);
	big.file = name;
	big.file_size = 32 << 20;
	REQUIRE(ftruncate(tmp, big.file_size) == 0);
	close(tmp);
	{
		WireServer serving(1, 5000, WireServer::Serial, &big);
		const char *const targets[] = { "/test/big", "/test/bigfile" };
		for (size_t i = 0; i < 2; ++i)
		{
			INFO(targets[i]);
			const int64_t t0 = wireNowMs();
			const int stalled = stalledReader(serving.port, targets[i]);
			const int next = headPeer(serving.port, "GET /api/v1/session HTTP/1.0\r\nHost: box\r\n\r\n");
			const std::string answered = headAnswer(next, 15000);
			const int64_t took = wireNowMs() - t0;
			CHECK(answered.find(" 200 ") != std::string::npos);
			CHECK(took >= 1500);
			CHECK(took < 6000);
			close(next);
			close(stalled);
		}
	}
	::unlink(name);
	CySocket::setSendIdleMsForTest(0);
	CHECK(CySocket::sendIdleMs() == 30000);
}

namespace
{

/* The two ends of a TCP connection over the loopback: the writer's with send_buffer
   bytes of buffer, the reader's with receive_buffer (0 leaves the kernel's own). A
   reader with a buffer of its own also takes segments of a network's size: over the
   loopback a segment may be 64 KiB, and a receive buffer below that is offered no
   room until a timer probes for it, which takes longer the longer it waits. */
void tcpPair(int &writer, int &reader, int send_buffer, int receive_buffer)
{
	const int l = socket(AF_INET, SOCK_STREAM, 0);
	REQUIRE(l >= 0);
	struct sockaddr_in a;
	std::memset(&a, 0, sizeof(a));
	a.sin_family = AF_INET;
	a.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
	REQUIRE(bind(l, (struct sockaddr *) &a, sizeof(a)) == 0);
	socklen_t alen = sizeof(a);
	REQUIRE(getsockname(l, (struct sockaddr *) &a, &alen) == 0);
	REQUIRE(listen(l, 1) == 0);
	reader = socket(AF_INET, SOCK_STREAM, 0);
	REQUIRE(reader >= 0);
	if (receive_buffer > 0) {
		setsockopt(reader, SOL_SOCKET, SO_RCVBUF, &receive_buffer, sizeof(receive_buffer));
		const int segment = 1400;
		setsockopt(reader, IPPROTO_TCP, TCP_MAXSEG, &segment, sizeof(segment));
	}
	REQUIRE(connect(reader, (struct sockaddr *) &a, sizeof(a)) == 0);
	writer = accept(l, NULL, NULL);
	REQUIRE(writer >= 0);
	close(l);
	setsockopt(writer, SOL_SOCKET, SO_SNDBUF, &send_buffer, sizeof(send_buffer));
}

// Bytes that differ from one place to the next, so a piece missing, doubled or out
// of place shows in a comparison.
std::string patterned(size_t n, uint32_t seed)
{
	std::string out(n, '\0');
	uint32_t x = 2463534242u ^ seed;
	for (size_t i = 0; i < n; ++i) {
		x ^= x << 13;
		x ^= x >> 17;
		x ^= x << 5;
		out[i] = (char) (x & 0xff);
	}
	return out;
}

// Where got first differs from expected, -1 where it is all of it; a missing tail
// is a difference at its first byte.
long long firstDifference(const std::string &got, const std::string &expected)
{
	const size_t n = std::min(got.size(), expected.size());
	for (size_t i = 0; i < n; ++i)
		if (got[i] != expected[i])
			return (long long) i;
	return got.size() == expected.size() ? -1 : (long long) n;
}

enum WrittenBy { BySendAll, BySendFile };

// What one answer came to on both ends.
struct Delivered
{
	bool        sent;
	std::string got;
	std::string expected;
	int         flags_before;
	int         flags_after;
	int64_t     took;
	bool        eof;
};

void noSignal(int) {}

/* One answer of length bytes over a TCP connection on the loopback, written by
   SendAll() out of memory or by SendFile() out of a file from offset, a stretch in
   the middle of it. The reader follows plan, a list of (pause in ms, at most so many
   bytes) taken in turn while the writer is busy, the last step held on; once the
   writer is done it takes everything through EOF within 10 seconds. interrupt sends
   the writer a signal that interrupts its waits every few milliseconds meanwhile. */
Delivered deliver(WrittenBy how, size_t length, const std::vector<std::pair<int, size_t> > &plan,
                  int send_buffer, int receive_buffer, bool interrupt = false)
{
	Delivered d;
	d.sent = false;
	d.flags_before = d.flags_after = -1;
	d.took = 0;
	d.eof = false;
	int w = -1, r = -1;
	tcpPair(w, r, send_buffer, receive_buffer);
	CySocket s(w);
	s.isOpened = true;
	s.isValid = true;

	const off_t offset = (how == BySendFile) ? 12345 : 0;
	const std::string whole = patterned(length + (size_t) offset + 777, 7);
	d.expected = whole.substr((size_t) offset, length);
	char name[] = "/tmp/test_deliver_XXXXXX";
	int file = -1;
	if (how == BySendFile) {
		file = mkstemp(name);
		REQUIRE(file >= 0);
		REQUIRE(write(file, whole.data(), whole.size()) == (ssize_t) whole.size());
	}

	struct sigaction quiet, was;
	std::memset(&quiet, 0, sizeof(quiet));
	quiet.sa_handler = &noSignal;
	sigemptyset(&quiet.sa_mask);
	quiet.sa_flags = 0;
	if (interrupt)
		REQUIRE(sigaction(SIGUSR1, &quiet, &was) == 0);

	std::atomic<bool> done(false);
	std::thread writer([&]() {
		d.flags_before = fcntl(w, F_GETFL);
		const int64_t t0 = wireNowMs();
		if (how == BySendAll)
			d.sent = s.SendAll(d.expected.data(), d.expected.size());
		else
			d.sent = s.SendFile(file, offset, (off_t) length);
		d.took = wireNowMs() - t0;
		d.flags_after = fcntl(w, F_GETFL);
		shutdown(w, SHUT_WR);
		done = true;
	});
	std::vector<char> buf(1 << 20);
	size_t step = 0;
	// a writer that would wait for good is ended from the far side after 20 seconds
	const int64_t give_up = wireNowMs() + 20000;
	while (!done) {
		const std::pair<int, size_t> &at = plan[step < plan.size() ? step : plan.size() - 1];
		for (int waited = 0; waited < at.first && !done && wireNowMs() < give_up; waited += 2) {
			napMs(2);
			if (interrupt)
				pthread_kill(writer.native_handle(), SIGUSR1);
		}
		if (done)
			break;
		if (wireNowMs() >= give_up) {
			close(r);
			r = -1;
			break;
		}
		struct pollfd p;
		p.fd = r;
		p.events = POLLIN;
		p.revents = 0;
		if (poll(&p, 1, 20) > 0) {
			const ssize_t n = recv(r, &buf[0], std::min(at.second, buf.size()), MSG_DONTWAIT);
			if (n > 0)
				d.got.append(&buf[0], (size_t) n);
		}
		++step;
	}
	writer.join();
	if (interrupt)
		sigaction(SIGUSR1, &was, NULL);
	const int64_t until = wireNowMs() + 10000;
	while (r >= 0 && wireNowMs() < until) {
		struct pollfd p;
		p.fd = r;
		p.events = POLLIN;
		p.revents = 0;
		if (poll(&p, 1, 500) <= 0)
			continue;
		const ssize_t n = recv(r, &buf[0], buf.size(), 0);
		if (n == 0) {
			d.eof = true;
			break;
		}
		if (n < 0)
			break;
		d.got.append(&buf[0], (size_t) n);
	}
	s.close();
	if (r >= 0)
		close(r);
	if (file >= 0) {
		close(file);
		::unlink(name);
	}
	return d;
}

// The reader takes bytes every pause_ms, held on for as long as the writer runs.
std::vector<std::pair<int, size_t> > every(int pause_ms, size_t bytes)
{
	return std::vector<std::pair<int, size_t> >(1, std::make_pair(pause_ms, bytes));
}

} // namespace

TEST_CASE("an answer a reader keeps taking is never cut, however long it runs", "[nhttpd][send]")
{
	/* The limit counts from the last byte the peer took, not from the start of the
	   answer. A slow reader (a few KiB every 130 ms, through a small window) frees the
	   writer's buffer in steps the socket answers with "takes more" only every two
	   seconds or so, twice the limit here; the kernel's own count of what it still
	   holds shows each step. A fast one keeps the writer busy for many times the
	   limit. Both get every byte, in order, out of memory and out of a file stretch,
	   and the socket blocks again afterwards as it did before. */
	CySocket::setSendIdleMsForTest(1000);
	const WrittenBy writers[] = { BySendAll, BySendFile };
	for (size_t i = 0; i < 2; ++i)
	{
		INFO((writers[i] == BySendAll ? "SendAll" : "SendFile"));
		Delivered slow = deliver(writers[i], 400 * 1024, every(130, 16 * 1024), 256 * 1024, 4096);
		CHECK(slow.sent);
		CHECK(slow.eof);
		CHECK(slow.got.size() == slow.expected.size());
		CHECK(firstDifference(slow.got, slow.expected) == -1);
		CHECK(slow.took > 1500);
		CHECK(slow.flags_after == slow.flags_before);
		CHECK((slow.flags_after & O_NONBLOCK) == 0);

		Delivered fast = deliver(writers[i], 32u << 20, every(4, 64 * 1024), 256 * 1024, 0);
		CHECK(fast.sent);
		CHECK(fast.eof);
		CHECK(fast.got.size() == fast.expected.size());
		CHECK(firstDifference(fast.got, fast.expected) == -1);
		CHECK(fast.took > 1500);
		CHECK(fast.flags_after == fast.flags_before);
	}
	CySocket::setSendIdleMsForTest(0);
}

TEST_CASE("what the peer takes in the first slice of a wait counts as moving", "[nhttpd][send]")
{
	/* The writer's buffer is full almost at once. The reader takes a little half a
	   second in, well inside the first slice of the writer's wait (2 seconds here), and
	   again 2.8 seconds in, past the limit of 2.4 seconds counted from the start but
	   inside it counted from that first take. So the first take has to count, which it
	   does only where the wait knew what the kernel held before that slice. */
	CySocket::setSendIdleMsForTest(2400);
	CySocket::setSendSliceMsForTest(2000);
	std::vector<std::pair<int, size_t> > plan;
	plan.push_back(std::make_pair(500, (size_t) 8192));
	plan.push_back(std::make_pair(2300, (size_t) 8192));
	plan.push_back(std::make_pair(0, (size_t) (1 << 20)));
	Delivered d = deliver(BySendAll, 1 << 20, plan, 256 * 1024, 4096);
	CHECK(d.sent);
	CHECK(firstDifference(d.got, d.expected) == -1);
	CHECK(d.took > 2800);
	CySocket::setSendSliceMsForTest(0);
	CySocket::setSendIdleMsForTest(0);
}

TEST_CASE("a writer interrupted by a signal goes on, and every byte arrives", "[nhttpd][send]")
{
	CySocket::setSendIdleMsForTest(1000);
	const WrittenBy writers[] = { BySendAll, BySendFile };
	for (size_t i = 0; i < 2; ++i)
	{
		INFO((writers[i] == BySendAll ? "SendAll" : "SendFile"));
		Delivered d = deliver(writers[i], 2u << 20, every(20, 64 * 1024), 256 * 1024, 0, true);
		CHECK(d.sent);
		CHECK(firstDifference(d.got, d.expected) == -1);
		CHECK(d.flags_after == d.flags_before);
	}
	CySocket::setSendIdleMsForTest(0);
}

TEST_CASE("a reader that stops taking is let go at the send limit, not before, and one that leaves at once", "[nhttpd][send]")
{
	CySocket::setSendIdleMsForTest(1500);
	const WrittenBy writers[] = { BySendAll, BySendFile };
	for (size_t i = 0; i < 2; ++i)
	{
		INFO((writers[i] == BySendAll ? "SendAll" : "SendFile"));
		// takes 64 KiB and then nothing, staying connected
		std::vector<std::pair<int, size_t> > stops;
		stops.push_back(std::make_pair(0, (size_t) (64 * 1024)));
		stops.push_back(std::make_pair(60000, (size_t) 0));
		Delivered stopped = deliver(writers[i], 8u << 20, stops, 256 * 1024, 4096);
		CHECK_FALSE(stopped.sent);
		CHECK(stopped.took >= 1400);
		CHECK(stopped.took < 4000);
		// the socket blocks again after a timeout too
		CHECK(stopped.flags_after == stopped.flags_before);
		CHECK((stopped.flags_after & O_NONBLOCK) == 0);
		// what arrived is the start of the answer, nothing else
		CHECK(stopped.got.size() < stopped.expected.size());
		CHECK(firstDifference(stopped.got, stopped.expected.substr(0, stopped.got.size())) == -1);
	}

	// a reader that closes with the answer unread: the writer ends at once, not at the limit
	for (size_t i = 0; i < 2; ++i)
	{
		INFO((writers[i] == BySendAll ? "SendAll, the reader gone" : "SendFile, the reader gone"));
		int w = -1, r = -1;
		tcpPair(w, r, 256 * 1024, 4096);
		CySocket s(w);
		s.isOpened = true;
		s.isValid = true;
		const std::string body = patterned(8u << 20, 3);
		char name[] = "/tmp/test_gone_XXXXXX";
		int file = -1;
		if (writers[i] == BySendFile) {
			file = mkstemp(name);
			REQUIRE(file >= 0);
			REQUIRE(write(file, body.data(), body.size()) == (ssize_t) body.size());
		}
		const int flags = fcntl(w, F_GETFL);
		bool sent = true;
		int64_t took = 0;
		std::thread writer([&]() {
			const int64_t t0 = wireNowMs();
			sent = (writers[i] == BySendAll) ? s.SendAll(body.data(), body.size())
			                                 : s.SendFile(file, 0, (off_t) body.size());
			took = wireNowMs() - t0;
		});
		napMs(200);
		close(r);
		writer.join();
		CHECK_FALSE(sent);
		CHECK(took < 1400);
		CHECK(fcntl(w, F_GETFL) == flags);
		s.close();
		if (file >= 0) {
			close(file);
			::unlink(name);
		}
	}
	CySocket::setSendIdleMsForTest(0);
}
