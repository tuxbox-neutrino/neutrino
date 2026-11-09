/*
 * wireserver.cpp - nhttpd on a loopback port, with the boundary and the bridge attached
 *
 * Copyright (C) 2026 NI-Team
 * Copyright (C) 2026 Tuxbox-Project (serial mode, bridge hook, tests)
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
#include "support/wireserver.h"

#include "httpd/carry.h"
#include "httpd/events.h"
#include "httpd/router.h"
#include "httpd/webconfig.h"

#include "yconnection.h"
#include "ysocket.h"

#include <cstdlib>
#include <cstring>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <poll.h>
#include <strings.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>

using namespace httpd;

int64_t wireNowMs()
{
	struct timespec t;
	if (clock_gettime(CLOCK_MONOTONIC, &t) != 0)
		return 0;
	return (int64_t) t.tv_sec * 1000 + t.tv_nsec / 1000000;
}

void napMs(int ms)
{
	struct timespec t;
	t.tv_sec = ms / 1000;
	t.tv_nsec = (long) (ms % 1000) * 1000 * 1000;
	nanosleep(&t, NULL);
}

namespace
{

struct ServeArgs
{
	WireServer *server;
	CySocket   *c;
};

void *serveOne(void *arg)
{
	ServeArgs *a = (ServeArgs *) arg;
	CWebserverConnection *con = new CWebserverConnection(&a->server->ws);
	con->Request.UrlData["clientaddr"] = a->c->get_client_ip();
	con->sock = a->c;
	a->c->handling = true;
	con->HandleConnection();
	if (!con->keep_alive)
		con->sock->isValid = false;
	a->c->handling = false;
	delete a->c;
	delete con;
	++a->server->served;
	delete a;
	return NULL;
}

} // namespace

WireServer::WireServer(unsigned max_streams, unsigned beat_ms, Mode m, Cyhook *front_hook)
	: listener(NULL), port(0), mode(m), front(front_hook),
	  threading_was(CWebserver::is_threading), served(0)
{
	WebConfig c = defaultWebConfig();
	c.sse_max_streams = max_streams;
	setConfigForTest(c);

	if (mode == Serial)
		CWebserver::is_threading = false;

	CyhookHandler::clearApiPrefixes();
	CApiBridge::registerPrefixes();
	CyhookHandler::attach(&gate);
	if (front != NULL)
		CyhookHandler::attach(front);
	CyhookHandler::attach(&bridge);
	CApiBridge::startStreams();
	events::setHeartbeatMs(beat_ms);

	int lfd = socket(AF_INET, SOCK_STREAM, 0);
	REQUIRE(lfd >= 0);
	int one = 1;
	setsockopt(lfd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));
	struct sockaddr_in a;
	std::memset(&a, 0, sizeof(a));
	a.sin_family = AF_INET;
	a.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
	REQUIRE(bind(lfd, (struct sockaddr *) &a, sizeof(a)) == 0);
	socklen_t alen = sizeof(a);
	REQUIRE(getsockname(lfd, (struct sockaddr *) &a, &alen) == 0);
	port = (int) ntohs(a.sin_port);
	REQUIRE(listen(lfd, 8) == 0);

	listener = new CySocket(lfd);
	listener->isOpened = true;
	listener->isValid = true;

	pthread_mutex_init(&workers_lock, NULL);
	REQUIRE(pthread_create(&acceptor, NULL, &WireServer::acceptLoop, this) == 0);
}

void *WireServer::acceptLoop(void *arg)
{
	WireServer *self = (WireServer *) arg;
	for (;;)
	{
		CySocket *c = self->listener->accept();
		if (c == NULL)
			return NULL;
		ServeArgs *sa = new ServeArgs;
		sa->server = self;
		sa->c = c;
		if (self->mode == Serial)
		{
			// the next connection waits until this one is answered
			serveOne(sa);
			continue;
		}
		pthread_t t;
		if (pthread_create(&t, NULL, &serveOne, sa) != 0)
		{
			delete sa->c;
			delete sa;
			continue;
		}
		pthread_mutex_lock(&self->workers_lock);
		self->workers.push_back(t);
		pthread_mutex_unlock(&self->workers_lock);
	}
}

WireServer::~WireServer()
{
	/* The order the server's own stop has: every carried answer is told to
	   end and every stream is closed and waited out first, so no connection
	   thread is inside the bridge or the events module when they go; then the
	   acceptor; then every worker is joined, because the hooks and the config
	   below must outlive the last HandleConnection. */
	CApiBridge::stopStreams();
	// registerPrefixes installed the first-login gate, which the other cases do not expect
	httpd::setPasswordOwed(NULL);
	::shutdown(listener->get_socket(), SHUT_RDWR);
	pthread_join(acceptor, NULL);
	for (;;)
	{
		pthread_t t;
		{
			pthread_mutex_lock(&workers_lock);
			if (workers.empty())
			{
				pthread_mutex_unlock(&workers_lock);
				break;
			}
			t = workers.back();
			workers.pop_back();
			pthread_mutex_unlock(&workers_lock);
		}
		pthread_join(t, NULL);
	}
	pthread_mutex_destroy(&workers_lock);
	listener->close();
	delete listener;
	CyhookHandler::detach(&bridge);
	if (front != NULL)
		CyhookHandler::detach(front);
	CyhookHandler::detach(&gate);
	CyhookHandler::clearApiPrefixes();
	setConfigForTest(defaultWebConfig());
	events::setHeartbeatMs(kShippedHeartbeatMs);
	events::setPendingCeiling(kShippedCeiling);
	CWebserver::is_threading = threading_was;
	// the next case finds the door open, as a fresh process does
	carry::endStop();
}

WireStream::WireStream(int port, const char *request, int rcvbuf)
	: fd(-1), opened(false), eof(false), status(0), taken(0)
{
	fd = socket(AF_INET, SOCK_STREAM, 0);
	if (fd < 0)
		return;
	// a small window, where a case wants the far end to ask for little
	if (rcvbuf > 0)
		setsockopt(fd, SOL_SOCKET, SO_RCVBUF, &rcvbuf, sizeof(rcvbuf));
	struct sockaddr_in a;
	std::memset(&a, 0, sizeof(a));
	a.sin_family = AF_INET;
	a.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
	a.sin_port = htons((uint16_t) port);
	if (connect(fd, (struct sockaddr *) &a, sizeof(a)) != 0)
		return;
	const size_t n = std::strlen(request);
	if (send(fd, request, n, MSG_NOSIGNAL) != (ssize_t) n)
		return;

	// the header, up to the blank line
	std::string got;
	const int64_t deadline = wireNowMs() + 5000;
	for (;;)
	{
		const size_t at = got.find("\r\n\r\n");
		if (at != std::string::npos)
		{
			header = got.substr(0, at + 4);
			body = got.substr(at + 4);
			break;
		}
		if (!readSome(got, deadline))
		{
			// what came before a close that left no header, for a case
			// that wants to know nothing came
			body = got;
			return;
		}
	}
	status = (int) std::strtol(header.c_str() + 9, NULL, 10);
	opened = true;
}

WireStream::~WireStream()
{
	if (fd >= 0)
		close(fd);
}

std::string WireStream::headerOf(const char *name) const
{
	std::string want(name);
	want += ":";
	for (size_t at = 0; at < header.size();)
	{
		size_t end = header.find("\r\n", at);
		if (end == std::string::npos)
			break;
		if (header.size() - at > want.size()
				&& strncasecmp(header.c_str() + at, want.c_str(), want.size()) == 0)
		{
			std::string v = header.substr(at + want.size(), end - at - want.size());
			while (!v.empty() && v[0] == ' ')
				v.erase(0, 1);
			return v;
		}
		at = end + 2;
	}
	return "";
}

bool WireStream::hasHeader(const char *name) const
{
	return !headerOf(name).empty();
}

std::string WireStream::readFrame(int wait_ms)
{
	const int64_t deadline = wireNowMs() + wait_ms;
	for (;;)
	{
		const size_t at = body.find("\n\n", taken);
		if (at != std::string::npos)
		{
			std::string out = body.substr(taken, at + 2 - taken);
			taken = at + 2;
			return out;
		}
		if (!readSome(body, deadline))
			return "";
	}
}

std::string WireStream::readUntilAll(const std::vector<std::string> &wanted, int wait_ms)
{
	const int64_t deadline = wireNowMs() + wait_ms;
	for (;;)
	{
		bool all = true;
		for (size_t i = 0; all && i < wanted.size(); ++i)
			all = body.find(wanted[i]) != std::string::npos;
		if (all)
			return body;
		if (!readSome(body, deadline))
			return body;
	}
}

std::string WireStream::readToEof(int wait_ms)
{
	const int64_t deadline = wireNowMs() + wait_ms;
	while (readSome(body, deadline))
	{
	}
	return body.substr(taken);
}

bool WireStream::closedWithin(int wait_ms)
{
	const int64_t deadline = wireNowMs() + wait_ms;
	while (readSome(body, deadline))
	{
	}
	return eof;
}

bool WireStream::readSome(std::string &into, int64_t deadline)
{
	const int64_t left = deadline - wireNowMs();
	if (left <= 0)
		return false;
	struct pollfd p;
	p.fd = fd;
	p.events = POLLIN;
	p.revents = 0;
	if (poll(&p, 1, (int) left) <= 0)
		return false;
	char buf[4096];
	const ssize_t n = read(fd, buf, sizeof(buf));
	if (n <= 0)
	{
		eof = true;
		return false;
	}
	into.append(buf, (size_t) n);
	return true;
}
