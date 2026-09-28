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
#include "ywebserver.h"
#include <httpd/server_control.h>

#include <cstdlib>
#include <cstring>
#include <string>
#include <thread>
#include <vector>

#include <fcntl.h>
#include <poll.h>
#include <sys/socket.h>
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
	hh.SendFd = fd;
	hh.SendFdLength = 5;
	CStringList none, config;
	hh.session_init(none, none, none, config, M_GET, false);
	CHECK(hh.SendFd == -1);
	CHECK(hh.SendFdLength == 0);
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
