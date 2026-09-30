/*
 * test_eventserver_outbox.cpp - an event a busy client could not take yet
 *
 * Copyright (C) 2026 Thilo Graf
 *
 * License: GPL-2.0-or-later
 */

#include "support/catch.hpp"

#include <eventserver.h>

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include <poll.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/un.h>
#include <unistd.h>

namespace {

long long nowMillis()
{
	struct timeval tv;
	gettimeofday(&tv, NULL);
	return (long long) tv.tv_sec * 1000 + tv.tv_usec / 1000;
}

// A client's socket that takes nothing until told to: a GUI loop busy
// elsewhere, as far as its senders can tell.
struct BusyClient
{
	std::string path;
	int fd;

	explicit BusyClient(int n = 0) : fd(-1)
	{
		char name[64];
		snprintf(name, sizeof(name), "/tmp/eventserver-outbox-%d-%d.sock", (int) getpid(), n);
		path = name;
		unlink(path.c_str());
		fd = socket(AF_UNIX, SOCK_STREAM, 0);
		struct sockaddr_un a;
		memset(&a, 0, sizeof(a));
		a.sun_family = AF_UNIX;
		snprintf(a.sun_path, sizeof(a.sun_path), "%s", path.c_str());
		if (fd < 0 || bind(fd, (struct sockaddr *) &a, sizeof(a)) != 0 || listen(fd, 1) != 0) {
			if (fd >= 0)
				close(fd);
			fd = -1;
		}
	}
	~BusyClient()
	{
		if (fd >= 0)
			close(fd);
		unlink(path.c_str());
	}

	// Every event that arrives until count of them did or the time is up.
	std::vector<std::string> take(size_t count, int timeout_ms)
	{
		std::vector<std::string> got;
		const long long end = nowMillis() + timeout_ms;
		while (got.size() < count && nowMillis() < end) {
			struct pollfd p;
			p.fd = fd;
			p.events = POLLIN;
			p.revents = 0;
			if (poll(&p, 1, 50) <= 0)
				continue;
			int c = accept(fd, NULL, NULL);
			if (c < 0)
				continue;
			CEventServer::eventHead head;
			if (read(c, &head, sizeof(head)) == (ssize_t) sizeof(head)) {
				std::string body(head.dataSize, '\0');
				size_t have = 0;
				while (have < body.size()) {
					ssize_t n = read(c, &body[have], body.size() - have);
					if (n <= 0)
						break;
					have += (size_t) n;
				}
				if (have == body.size())
					got.push_back(body);
			}
			close(c);
		}
		return got;
	}
};

// Connections that fill a listening socket's backlog and stay unaccepted, so
// the next connect finds the client busy.
std::vector<int> fillBacklog(const std::string &path)
{
	std::vector<int> held;
	for (int i = 0; i < 16; i++) {
		int c = socket(AF_UNIX, SOCK_STREAM | SOCK_NONBLOCK, 0);
		struct sockaddr_un a;
		memset(&a, 0, sizeof(a));
		a.sun_family = AF_UNIX;
		snprintf(a.sun_path, sizeof(a.sun_path), "%s", path.c_str());
		if (connect(c, (struct sockaddr *) &a, sizeof(a)) != 0) {
			close(c);
			break;
		}
		held.push_back(c);
	}
	return held;
}

CEventServer *probe_server = NULL;
BusyClient *probe_client = NULL;
std::vector<int> probe_held;
bool probe_fired = false;

// The first send has come back busy and not yet gone into the outbox: the
// client takes connections again, and a second sender has a newer event.
void secondSenderInTheGap(const char *)
{
	if (probe_fired)
		return;
	probe_fired = true;
	for (size_t i = 0; i < probe_held.size(); i++) {
		int c = accept(probe_client->fd, NULL, NULL);
		if (c >= 0)
			close(c);
		close(probe_held[i]);
	}
	probe_held.clear();
	probe_server->sendEvent(45, CEventServer::INITID_TIMERD, "second", 6);
}

} // namespace

TEST_CASE("an event sent while an older one is still out to its client does not overtake it", "[eventserver]")
{
	/* Two threads send to one client. The first finds it busy; in the gap
	   before that event goes into the outbox the client takes connections
	   again and a second, newer event comes in. It used to find no outbox
	   entry, go straight out and arrive first. It now queues behind the send
	   that is still out, which puts its busy event in front. */
	BusyClient client(45);
	REQUIRE(client.fd >= 0);
	probe_held = fillBacklog(client.path);
	REQUIRE_FALSE(probe_held.empty());

	CEventServer server;
	server.registerEvent2(45, 1, client.path);
	probe_server = &server;
	probe_client = &client;
	probe_fired = false;
	CEventServer::deliverProbeForTest = secondSenderInTheGap;
	server.sendEvent(45, CEventServer::INITID_TIMERD, "first", 5);
	CEventServer::deliverProbeForTest = NULL;
	REQUIRE(probe_fired);

	const std::vector<std::string> got = client.take(2, 5000);
	REQUIRE(got.size() == 2);
	CHECK(got[0] == "first");
	CHECK(got[1] == "second");
}

TEST_CASE("an event a busy client cannot take yet arrives later and in order", "[eventserver]")
{
	/* The client's backlog fills while it is busy. A sender must not wait
	   for it (the daemon would stop), and the events it could not hand over
	   must not be lost either: a timer's record start is one of them. They
	   wait in the sender's outbox and go out in the order they were sent
	   once the client takes connections again. */
	BusyClient client;
	REQUIRE(client.fd >= 0);

	CEventServer server;
	server.registerEvent2(42, 1, client.path);

	const int count = 8;
	const long long t0 = nowMillis();
	for (int i = 0; i < count; i++) {
		char body[16];
		snprintf(body, sizeof(body), "event-%d", i);
		server.sendEvent(42, CEventServer::INITID_TIMERD, body, (unsigned) strlen(body));
	}
	const long long spent = nowMillis() - t0;

	const std::vector<std::string> got = client.take((size_t) count, 10000);
	REQUIRE(got.size() == (size_t) count);
	for (int i = 0; i < count; i++) {
		char want[16];
		snprintf(want, sizeof(want), "event-%d", i);
		CHECK(got[(size_t) i] == want);
	}
	// the sender paid for the first refusal and not for every event behind it
	CHECK(spent < 2 * EVENT_SEND_TIMEOUT_MS + 500);
}

TEST_CASE("an event for a client that went away is not kept", "[eventserver]")
{
	CEventServer server;
	server.registerEvent2(43, 1, "/tmp/eventserver-outbox-nobody.sock");
	unlink("/tmp/eventserver-outbox-nobody.sock");
	const long long t0 = nowMillis();
	server.sendEvent(43, CEventServer::INITID_TIMERD, "x", 1);
	CHECK(nowMillis() - t0 < EVENT_SEND_TIMEOUT_MS);
	CHECK(server.pendingEvents() == 0);
}

TEST_CASE("a server that goes away stops its outbox between two clients", "[eventserver]")
{
	/* With enough busy clients one turn of the outbox takes longer than the
	   destructor waits: each of them costs a send timeout. A thread that
	   only looked at the stop between turns was still sending when the
	   destructor gave up, and went on into a freed server. It looks before
	   every client now, so the destructor sees it gone after one send. */
	const int clients = 12;
	std::vector<BusyClient *> busy;
	CEventServer *server = new CEventServer;
	for (int i = 0; i < clients; i++) {
		busy.push_back(new BusyClient(i + 1));
		REQUIRE(busy.back()->fd >= 0);
		server->registerEvent2(44, (unsigned) (i + 1), busy.back()->path);
	}
	// two fill each client's backlog, the third finds every one of them busy
	for (int i = 0; i < 3; i++)
		server->sendEvent(44, CEventServer::INITID_TIMERD, "x", 1);
	REQUIRE(server->pendingEvents() > 0);

	const long long t0 = nowMillis();
	delete server;
	const long long took = nowMillis() - t0;
	CHECK(took < 3 * EVENT_SEND_TIMEOUT_MS);

	for (size_t i = 0; i < busy.size(); i++)
		delete busy[i];
}
