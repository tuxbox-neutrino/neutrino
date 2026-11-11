/*
 * test_keyrepeat.cpp - the repeat delays the input nodes are given
 *
 * Copyright (C) 2026 Tuxbox-Project (input repeat delays, tests)
 *
 * License: GPL-2.0-or-later
 *
 * The remote control's settings give a repeat delay and period, which the
 * loop writes to every input node it has open as EV_REP events. A node keeps
 * what it was given: one opened later, such as a keyboard plugged in, is
 * given the same, and the nodes open before are not written again. The nodes
 * here are pseudo terminals in raw mode: what the loop writes to one comes
 * out at its master, and nothing the loop writes comes back to its own
 * reads, as with an event node.
 */

#include "support/catch.hpp"
#include "support/rcfixture.h"

#include <driver/rcinput.h>

#include <fcntl.h>
#include <poll.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <termios.h>
#include <unistd.h>

#include <linux/input.h>

#include <cstring>
#include <string>
#include <utility>
#include <vector>

namespace
{

// The EV_REP events a node got, as (code, value), in the order written.
typedef std::vector<std::pair<int, int> > Delays;

Delays delays(int delay, int period)
{
	Delays d;
	d.push_back(std::make_pair((int) REP_DELAY, delay));
	d.push_back(std::make_pair((int) REP_PERIOD, period));
	return d;
}

struct Node
{
	int master;
	int slave;   // held, so the raw mode stays the terminal's
	std::string path;

	Node() : master(-1), slave(-1)
	{
		master = posix_openpt(O_RDWR | O_NOCTTY | O_NONBLOCK);
		REQUIRE(master >= 0);
		REQUIRE(grantpt(master) == 0);
		REQUIRE(unlockpt(master) == 0);
		const char *name = ptsname(master);
		REQUIRE(name != NULL);
		path = name;
		slave = open(path.c_str(), O_RDWR | O_NOCTTY | O_NONBLOCK);
		REQUIRE(slave >= 0);
		struct termios t;
		REQUIRE(tcgetattr(slave, &t) == 0);
		cfmakeraw(&t);
		REQUIRE(tcsetattr(slave, TCSANOW, &t) == 0);
	}

	~Node()
	{
		if (slave >= 0)
			close(slave);
		if (master >= 0)
			close(master);
	}

	/* What was written to the node since the last call: waited for up to
	   wait_ms for a first byte, then for as long as more follows within
	   100 ms. */
	Delays written(int wait_ms)
	{
		std::vector<unsigned char> bytes;
		int timeout = wait_ms;
		for (;;)
		{
			struct pollfd p;
			p.fd = master;
			p.events = POLLIN;
			p.revents = 0;
			if (poll(&p, 1, timeout) <= 0)
				break;
			unsigned char buf[512];
			const ssize_t n = read(master, buf, sizeof(buf));
			if (n <= 0)
				break;
			bytes.insert(bytes.end(), buf, buf + n);
			timeout = 100;
		}
		REQUIRE(bytes.size() % sizeof(struct input_event) == 0);
		Delays d;
		for (size_t off = 0; off < bytes.size(); off += sizeof(struct input_event))
		{
			struct input_event ev;
			memcpy(&ev, &bytes[off], sizeof(ev));
			REQUIRE(ev.type == EV_REP);
			d.push_back(std::make_pair((int) ev.code, (int) ev.value));
		}
		return d;
	}

	private:
		Node(const Node &);
		Node &operator=(const Node &);
};

/* The loop's own node, moved off the one a box uses (as in test_rc.cpp), so
   that a box running beside the suite keeps its keys. */
const char *shippedLoopNode()
{
	static const std::string held = injectedKeyNodes().back();
	return held.c_str();
}

struct LoopNode
{
	std::string path;

	LoopNode() : path(std::string("/tmp/keyrepeat-loop.") + std::to_string(getpid()))
	{
		REQUIRE_FALSE(std::string(shippedLoopNode()).empty());
		unlink(path.c_str());
		REQUIRE(mkfifo(path.c_str(), 0600) == 0);
		setInjectedKeyNodeForTest(path.c_str());
	}

	~LoopNode()
	{
		setInjectedKeyNodeForTest(shippedLoopNode());
		unlink(path.c_str());
	}

	private:
		LoopNode(const LoopNode &);
		LoopNode &operator=(const LoopNode &);
};

// The case's event node, taken away again however the case ends.
struct Plugged
{
	explicit Plugged(const Node &n) { setEventNodeForTest(n.path.c_str()); }
	~Plugged() { setEventNodeForTest(NULL); }
};

void waitOnce(CRCInput &rc)
{
	neutrino_msg_t msg;
	neutrino_msg_data_t data;
	rc.getMsg_ms(&msg, &data, 1);
}

} // namespace

TEST_CASE("a node opened before the settings are applied keeps the driver's own delays", "[rcinput][repeat]")
{
	requireFreeEventSocket();
	LoopNode loop;
	Node remote;
	Plugged plugged(remote);
	CRCInput rc;

	REQUIRE(remote.written(300).empty());

	rc.setKeyRepeatDelay(450, 100);
	// 10 ms on top of each, for the loop's own repeat check.
	REQUIRE(remote.written(1000) == delays(460, 110));
}

TEST_CASE("the remote control keeps its delays when a keyboard is plugged in, and the keyboard gets them", "[rcinput][repeat]")
{
	requireFreeEventSocket();
	LoopNode loop;
	Node remote;
	Node keyboard;
	Plugged plugged(remote);
	CRCInput rc;
	rc.setKeyRepeatDelay(450, 100);
	REQUIRE(remote.written(1000) == delays(460, 110));

	// Plugged in: the next wait for a message opens it.
	setEventNodeForTest(keyboard.path.c_str());
	waitOnce(rc);
	REQUIRE(keyboard.written(1000) == delays(460, 110));
	REQUIRE(remote.written(300).empty());

	// Another check of the nodes finds nothing new and writes nothing.
	setEventNodeForTest(keyboard.path.c_str());
	waitOnce(rc);
	REQUIRE(keyboard.written(300).empty());
	REQUIRE(remote.written(300).empty());
}

TEST_CASE("delays of nought are written as such", "[rcinput][repeat]")
{
	requireFreeEventSocket();
	LoopNode loop;
	Node remote;
	Plugged plugged(remote);
	CRCInput rc;
	rc.setKeyRepeatDelay(450, 100);
	REQUIRE(remote.written(1000) == delays(460, 110));

	// A setting of nought is a value, not a request to write the last ones again.
	rc.setKeyRepeatDelay(0, 0);
	REQUIRE(remote.written(1000) == delays(10, 10));
}
