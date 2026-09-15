/*
 * test_zapitclient_getpids.cpp - tests for asking zapit for the pids
 *
 * Copyright (C) 2026 NI-Team
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

#include <cstdio>
#include <cstring>
#include <string>
#include <pthread.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/un.h>
#include <unistd.h>

#include <zapit/client/zapitclient.h>
#include <zapit/client/msgtypes.h>

// getPIDS (zapitclient.cpp) reads CZapitClient::responseGetOtherPIDs straight
// off the wire with one receive_data call whose return value used to go
// unchecked. A daemon that answers short, or is not there to answer at all,
// left every field the read never reached at whatever the caller's stack
// already held there. These drive the real function, over a real socket,
// rather than a copy of its logic.

namespace
{

/* An address of this process's own rather than the one the client is built
   with. ZAPIT_UDS_NAME is a fixed name under /tmp that the daemon on a running
   box already listens on and that a second suite would bind at the same
   moment: binding it here takes that name away from whoever holds it, and
   either of them connecting while a case has it is an answer going to the
   wrong reader. Which address the client asks for is not what any of these
   cases is about; what it does with a short answer is. */
const char *privateSocketName()
{
	static std::string name;
	if (name.empty())
	{
		char buf[64];
		std::snprintf(buf, sizeof(buf), "/tmp/coreapi-zapit.%d.sock", (int) getpid());
		name = buf;
	}
	return name.c_str();
}

/* The shipped client with its address replaced and nothing else: getPIDS and
   everything under it is the code that ships. The address is a private virtual
   of the class, which a derived class may still answer for. */
class ZapitClientHere : public CZapitClient
{
private:
	const char *getSocketName() const { return privateSocketName(); }
};

// A single-shot peer at that address.
class ZapitPeer
{
public:
	// Handing back fewer than reply_len's caller passed in is a daemon that
	// answered a truncated struct and then went away mid-message, the shape
	// a caller cannot rule out on a live box.
	ZapitPeer(const void *data, size_t bytes)
		: reply(static_cast<const char *>(data)), reply_len(bytes), listen_fd(-1),
		  joined(false), accepted_it(false), sent_bytes(0), request_bytes(0)
	{
		unlink(privateSocketName());

		listen_fd = socket(AF_UNIX, SOCK_STREAM, 0);
		REQUIRE(listen_fd != -1);
		sockaddr_un addr;
		memset(&addr, 0, sizeof(addr));
		addr.sun_family = AF_UNIX;
		strncpy(addr.sun_path, privateSocketName(), sizeof(addr.sun_path) - 1);
		// Checked, because a bind that failed leaves nothing listening and the
		// case then reads a client that reached no daemon as one that reached
		// a daemon which answered nothing.
		REQUIRE(bind(listen_fd, (sockaddr *) &addr, sizeof(addr)) == 0);
		REQUIRE(listen(listen_fd, 1) == 0);

		/* An accept that is never satisfied would be joined below and hang the
		   whole run with nothing to read about why. A ceiling on it turns that
		   into a case that fails and says which one. */
		struct timeval tv;
		tv.tv_sec = 30;
		tv.tv_usec = 0;
		REQUIRE(setsockopt(listen_fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) == 0);

		REQUIRE(pthread_create(&thread, 0, serve, this) == 0);
	}

	~ZapitPeer()
	{
		finish();
		if (listen_fd != -1)
			close(listen_fd);
		unlink(privateSocketName());
	}

	/* Joins the serving thread. A case calls this before it reads what the
	   peer managed, so that what it reads is settled rather than whatever the
	   other thread had written by then. Called again from the destructor,
	   which is why it remembers.  */
	void finish()
	{
		if (joined)
			return;
		pthread_join(thread, 0);
		joined = true;
	}

	/* What this end actually managed, so that a case which reads nothing says
	   which side of the wire stopped. Read after finish(). Without these, a
	   peer that never got a connection and a peer that sent half its answer
	   both show up as a field comparing against zero, and the reader is left
	   to guess which run they had. */
	bool accepted() const { return accepted_it; }
	ssize_t sentBytes() const { return sent_bytes; }
	ssize_t requestBytes() const { return request_bytes; }

private:
	static void *serve(void *arg)
	{
		static_cast<ZapitPeer *>(arg)->loop();
		return 0;
	}

	void loop()
	{
		int fd = accept(listen_fd, 0, 0);
		if (fd < 0)
			return;
		accepted_it = true;
		/* The command is read before the answer goes out, which is the order the daemon
		   this stands in for works in, and the order that makes these cases about
		   getPIDS rather than about scheduling.

		   Answering first and closing straight after is a race the client loses at
		   random: it connects, is descheduled before it writes its command, and by then
		   this end has already answered and hung up. The write fails, and
		   CBasicClient::send_data closes the socket on a failed write, which throws away
		   the answer already in its receive buffer. Measured at three runs in a hundred
		   on a loaded machine, and none in a hundred with the read in place. */
		char command[64];
		request_bytes = recv(fd, command, sizeof(command), 0);
		/* Written to the end rather than in one call. A stream socket is
		   allowed to take less than it was offered, and a peer that stopped
		   there would be a daemon that truncated its answer, which is what the
		   first case of this file deliberately does: the two would then be the
		   same run and one of them would be lying about what it proves. */
		sent_bytes = 0;
		while ((size_t) sent_bytes < reply_len)
		{
			const ssize_t one = send(fd, reply + sent_bytes, reply_len - sent_bytes, MSG_NOSIGNAL);
			if (one <= 0)
				break;
			sent_bytes += one;
		}
		close(fd);
	}

	const char *reply;
	size_t      reply_len;
	int         listen_fd;
	pthread_t   thread;
	bool             joined;
	volatile bool    accepted_it;
	volatile ssize_t sent_bytes;
	volatile ssize_t request_bytes;
};

} // namespace

TEST_CASE("getPIDS leaves a field the daemon never answered at zero, not at the caller's stack", "[zapit][client]")
{
	CZapitClient::responseGetOtherPIDs sent;
	memset(&sent, 0, sizeof(sent));
	sent.vpid = 0x1234;

	// Only the first two fields: the rest of the struct is never written.
	ZapitPeer peer(&sent, sizeof(sent.vpid) + sizeof(sent.ecmpid));

	ZapitClientHere client;
	CZapitClient::responseGetPIDs pids;
	memset(&pids.PIDs, 0xAA, sizeof(pids.PIDs));

	client.getPIDS(pids);

	REQUIRE(pids.PIDs.vpid == 0x1234u);
	REQUIRE(pids.PIDs.ecmpid == 0);
	REQUIRE(pids.PIDs.vtxtpid == 0);
	REQUIRE(pids.PIDs.pcrpid == 0);
	REQUIRE(pids.PIDs.selected_apid == 0);
	REQUIRE(pids.PIDs.pmtpid == 0);
	REQUIRE(pids.PIDs.privatepid == 0);
}

TEST_CASE("getPIDS with no daemon to answer at all leaves every field at zero", "[zapit][client]")
{
	unlink(privateSocketName()); // guarantee nothing accepts this connect

	ZapitClientHere client;
	CZapitClient::responseGetPIDs pids;
	memset(&pids.PIDs, 0xAA, sizeof(pids.PIDs));

	client.getPIDS(pids);

	REQUIRE(pids.PIDs.vpid == 0);
	REQUIRE(pids.PIDs.ecmpid == 0);
	REQUIRE(pids.PIDs.vtxtpid == 0);
	REQUIRE(pids.PIDs.pcrpid == 0);
	REQUIRE(pids.PIDs.selected_apid == 0);
	REQUIRE(pids.PIDs.pmtpid == 0);
	REQUIRE(pids.PIDs.privatepid == 0);
}

TEST_CASE("getPIDS still delivers a full answer untouched", "[zapit][client]")
{
	CZapitClient::responseGetOtherPIDs sent;
	// Zeroed first: only the seven fields below are named, and a struct that
	// grows a member would otherwise put this case's stack on the wire.
	memset(&sent, 0, sizeof(sent));
	sent.vpid          = 100;
	sent.ecmpid        = 101;
	sent.vtxtpid       = 102;
	sent.pcrpid        = 103;
	sent.selected_apid = 104;
	sent.pmtpid        = 105;
	sent.privatepid    = 106;

	ZapitPeer peer(&sent, sizeof(sent));

	ZapitClientHere client;
	CZapitClient::responseGetPIDs pids;
	memset(&pids.PIDs, 0xAA, sizeof(pids.PIDs));

	client.getPIDS(pids);

	/* Before the fields, because a peer that never got its connection or that
	   stopped halfway through the answer is a run about this machine's
	   scheduling and not about getPIDS, and the two used to be the same
	   message: a field compared against zero.  */
	peer.finish();
	REQUIRE(peer.accepted());
	REQUIRE(peer.requestBytes() > 0);
	REQUIRE(peer.sentBytes() == (ssize_t) sizeof(sent));

	REQUIRE(pids.PIDs.vpid == 100);
	REQUIRE(pids.PIDs.ecmpid == 101);
	REQUIRE(pids.PIDs.vtxtpid == 102);
	REQUIRE(pids.PIDs.pcrpid == 103);
	REQUIRE(pids.PIDs.selected_apid == 104);
	REQUIRE(pids.PIDs.pmtpid == 105);
	REQUIRE(pids.PIDs.privatepid == 106);
}
