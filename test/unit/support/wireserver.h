/*
 * wireserver.h - nhttpd on a loopback port, with the bridge attached, for
 * the cases that drive the whole chain over a real socket
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
 *
 * The server is the production path minus the accept loop of
 * CWebserver::run(): a real listening socket, CySocket::accept() (which is
 * what puts the cork on), and per connection the same wiring WebThread does -
 * CWebserverConnection::HandleConnection() end to end - with the bridge
 * attached. Threaded, each connection gets a thread of its own; serial, the
 * accept thread serves each connection itself before it takes the next one,
 * which is what webserver.threading=false does on a box.
 */

#ifndef __test_unit_support_wireserver_h__
#define __test_unit_support_wireserver_h__

#include "httpd/nhttpd_bridge.h"

#include "yhook.h"
#include "ywebserver.h"

#include <atomic>
#include <cstddef>
#include <stdint.h>
#include <string>
#include <vector>

#include <pthread.h>

class CySocket;

// What the header beside the code says these are, so that a case restoring
// them puts back what ships and a case reading them fails when the shipped
// value moves without anybody saying so.
const unsigned kShippedHeartbeatMs = 20000;
const size_t   kShippedCeiling     = 256u * 1024u;

int64_t wireNowMs();
void napMs(int ms);

struct WireServer
{
	enum Mode
	{
		Threaded,
		Serial
	};

	// the request boundary first, as the server attaches it (yhttpd.cpp)
	CAuthorityGate gate;
	CApiBridge  bridge;
	CWebserver  ws;
	CySocket   *listener;
	int         port;
	pthread_t   acceptor;
	pthread_mutex_t          workers_lock;
	std::vector<pthread_t>   workers;
	Mode        mode;
	// a hook of the case's, asked before the bridge, NULL for none
	Cyhook     *front;
	bool        threading_was;
	// connections whose HandleConnection() has returned and whose socket
	// object is gone
	std::atomic<unsigned> served;

	explicit WireServer(unsigned max_streams = 4, unsigned beat_ms = 100,
	                    Mode m = Threaded, Cyhook *front_hook = NULL);
	~WireServer();

	static void *acceptLoop(void *arg);

private:
	WireServer(const WireServer &);
	WireServer &operator=(const WireServer &);
};

/* One client. HTTP/1.0 and close-delimited, so there is no framing to
   decode: the header runs to the blank line and the body is bytes until the
   connection closes. */
struct WireStream
{
	int         fd;
	bool        opened;
	// the far end closed (or reset) the connection
	bool        eof;
	int         status;
	std::string header;
	std::string body;
	size_t      taken;

	explicit WireStream(int port, const char *request =
			"GET /api/v1/events HTTP/1.0\r\nHost: box\r\n\r\n",
			int rcvbuf = 0);
	~WireStream();

	bool ok() const { return opened; }
	int code() const { return status; }

	// case-insensitive, value up to the line's end
	std::string headerOf(const char *name) const;
	bool hasHeader(const char *name) const;

	/* The next frame, terminator included, or "" at the deadline. A frame
	   ends at the blank line, which in this format is two newlines. */
	std::string readFrame(int wait_ms);

	// Frames until every wanted piece was seen, or the deadline; answers
	// everything read.
	std::string readUntilAll(const std::vector<std::string> &wanted, int wait_ms);

	// Everything until the peer closes, within the deadline.
	std::string readToEof(int wait_ms);

	// Whether the peer closed within the deadline, everything before it
	// appended to the body.
	bool closedWithin(int wait_ms);

private:
	bool readSome(std::string &into, int64_t deadline);

	WireStream(const WireStream &);
	WireStream &operator=(const WireStream &);
};

#endif
