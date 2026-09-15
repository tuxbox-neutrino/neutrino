/*
 * test_zapitclient_bouquetwrite.cpp - tests for writing a bouquet through zapit
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
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/un.h>
#include <unistd.h>

#include <zapit/client/zapitclient.h>
#include <zapit/client/msgtypes.h>

/* A bouquet write hands one command to the channel daemon over a socket and used to
   answer nothing at all, so a caller could not tell a command that was carried out from
   one that was never written: with the daemon's socket gone the layer above went on
   answering that a bouquet had been made while the list was untouched. The cases below
   drive the real methods over a real socket rather than a copy of what they do.

   At an address of their own and not at the daemon's well known one, which is where the
   case beside them binds: two runs on one machine would otherwise bind the same path. */

namespace
{

// Carries this process's number, because a name that does not is one two runs
// on the same machine bind at the same moment, and a case that is red because
// a second build is running is one nobody can read.
const char *kSocket()
{
	static std::string name;
	if (name.empty())
	{
		char buf[64];
		snprintf(buf, sizeof(buf), "/tmp/ni-bouquet-write.%d.sock", (int) getpid());
		name = buf;
	}
	return name.c_str();
}

// The one thing a case needs to change about the client, and the only place it
// can be changed: the address is what the client answers when the transport
// asks for it, and what it answers is a constant.
class BoundClient : public CZapitClient
{
	private:
		const char *getSocketName() const { return kSocket(); }
};

/* Something listening at that address for as long as one of these is alive. It
   answers the save the way it is told to, so that a case can tell a command
   that was merely written from one the daemon read, and a daemon that wrote
   the files from one that could not. */
class Daemon
{
	public:
		enum Answer { SaysNothing, SaysSaved, SaysNotSaved };

		explicit Daemon(Answer answer)
			: answer_(answer), listen_fd_(-1), stop_(false)
		{
			unlink(kSocket());

			listen_fd_ = socket(AF_UNIX, SOCK_STREAM, 0);
			sockaddr_un addr;
			memset(&addr, 0, sizeof(addr));
			addr.sun_family = AF_UNIX;
			strncpy(addr.sun_path, kSocket(), sizeof(addr.sun_path) - 1);
			bind(listen_fd_, (sockaddr *) &addr, sizeof(addr));
			listen(listen_fd_, 4);

			pthread_create(&thread_, 0, serve, this);
		}

		~Daemon()
		{
			stop_ = true;
			pthread_join(thread_, 0);
			if (listen_fd_ != -1)
				close(listen_fd_);
			unlink(kSocket());
		}

	private:
		Daemon(const Daemon &);
		Daemon &operator=(const Daemon &);

		static void *serve(void *arg)
		{
			static_cast<Daemon *>(arg)->loop();
			return 0;
		}

		/* Waited on with a timeout rather than blocked on, because the thread
		   has to come back and read the flag that ends it: an accept nothing
		   ever connects to would hold the run open instead. */
		void loop()
		{
			while (!stop_)
			{
				fd_set waiting;
				FD_ZERO(&waiting);
				FD_SET(listen_fd_, &waiting);
				timeval how_long;
				how_long.tv_sec = 0;
				how_long.tv_usec = 20000;
				if (select(listen_fd_ + 1, &waiting, 0, 0, &how_long) <= 0)
					continue;

				const int fd = accept(listen_fd_, 0, 0);
				if (fd < 0)
					continue;

				if (answer_ == SaysNothing)
				{
					/* Half closed rather than closed: the caller is told at
					   once that nothing is coming, and a command still being
					   written goes through, which is the state that tells a
					   write that only went out from one that was read. */
					shutdown(fd, SHUT_WR);
				}
				else
				{
					CZapitMessages::responseGeneralTrueFalse wrote;
					wrote.status = (answer_ == SaysSaved);
					send(fd, &wrote, sizeof(wrote), MSG_NOSIGNAL);
				}

				// Until the caller hangs up, so that no connection is taken
				// away under a command still being written into it.
				char sink[256];
				while (recv(fd, sink, sizeof(sink), 0) > 0)
					;
				close(fd);
			}
		}

		Answer        answer_;
		int           listen_fd_;
		volatile bool stop_;
		pthread_t     thread_;
};

} // namespace

TEST_CASE("a bouquet write with nothing at the daemon's address says so", "[zapit][client]")
{
	unlink(kSocket()); // guarantee nothing accepts this connect

	BoundClient client;

	// All ten, because the layer above sends all ten and one of them left
	// answering yes is one write that goes on claiming to have happened.
	REQUIRE_FALSE(client.addBouquet("Probe"));
	REQUIRE_FALSE(client.renameBouquet(0, "Probe"));
	REQUIRE_FALSE(client.moveBouquet(1, 0));
	REQUIRE_FALSE(client.deleteBouquet(0));
	REQUIRE_FALSE(client.setBouquetHidden(0, true));
	REQUIRE_FALSE(client.setBouquetLock(0, true));
	REQUIRE_FALSE(client.addChannelToBouquet(0, (t_channel_id) 0x2b66ULL));
	REQUIRE_FALSE(client.removeChannelFromBouquet(0, (t_channel_id) 0x2b66ULL));
	REQUIRE_FALSE(client.renumChannellist());
	REQUIRE_FALSE(client.saveBouquets());
}

TEST_CASE("a bouquet write the daemon took says that as well", "[zapit][client]")
{
	// Without this the case above could not fail: an answer that was no
	// whatever happened would read exactly like a refusal.
	Daemon daemon(Daemon::SaysSaved);

	BoundClient client;

	REQUIRE(client.addBouquet("Probe"));
	REQUIRE(client.renameBouquet(0, "Probe"));
	REQUIRE(client.moveBouquet(1, 0));
	REQUIRE(client.deleteBouquet(0));
	REQUIRE(client.setBouquetHidden(0, true));
	REQUIRE(client.setBouquetLock(0, true));
	REQUIRE(client.addChannelToBouquet(0, (t_channel_id) 0x2b66ULL));
	REQUIRE(client.removeChannelFromBouquet(0, (t_channel_id) 0x2b66ULL));
	REQUIRE(client.renumChannellist());
	REQUIRE(client.saveBouquets());
}

TEST_CASE("a save the daemon never acknowledged is not a save", "[zapit][client]")
{
	/* The save is the one of the ten the daemon answers, and its answer is
	   what says the command was read rather than that the bytes were taken. A
	   daemon that keeps the connection and says nothing leaves every other
	   write looking as if it landed, which is why this is the one that can
	   tell the difference at all. */
	Daemon daemon(Daemon::SaysNothing);

	BoundClient client;

	REQUIRE(client.addBouquet("Probe"));
	REQUIRE_FALSE(client.saveBouquets());
}

TEST_CASE("a save the daemon says it could not write is not a save", "[zapit][client]")
{
	/* The case above answers nothing at all, which a client could refuse for
	   the sole reason that it read nothing. This one answers, and what it
	   answers is no: the two files were not written. A client that took the
	   arrival of an answer for the answer itself passes the case above and
	   fails here, which is the whole difference this command now carries. */
	Daemon daemon(Daemon::SaysNotSaved);

	BoundClient client;

	REQUIRE_FALSE(client.saveBouquets());
}
