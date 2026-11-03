/*
 * tsgrid.h - whole transport stream packets for every client of a stream
 *
 * Copyright (C) 2026 Tuxbox-Project (stream packet grid)
 *
 * License: GPL-2.0-or-later
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
 * A client finds each packet of a transport stream by its place: every
 * 188 bytes a packet starts. The stream server reads in pieces that need
 * not end on a packet, and it sends to a client without waiting, so a
 * client that falls behind takes less than it was offered. Both are kept
 * on the grid here: a reader hands on whole packets only, and a client
 * that took part of a packet gets the rest of that packet first and
 * otherwise loses whole packets only. A client that falls behind stays
 * connected.
 */

#ifndef __tsgrid_h__
#define __tsgrid_h__

#include <cstddef>
#include <map>
#include <set>
#include <string>
#include <vector>

#include <stdint.h>
#include <sys/types.h>

namespace tsgrid
{

const size_t kPacket = 188;

/* For a reader that reads into a buffer of its own: the part of a packet a
   read ended in stays at the front of the buffer, and the next read goes
   behind it. */
class Collector
{
public:
	Collector() : held(0), filled(0) {}

	// Where the next read goes, as an offset into the buffer.
	size_t carry() const { return held; }
	/* After a read that answered r into buf + carry(): how many bytes of
	   whole packets are at the front of buf. An error (r < 0) drops the
	   part held: the demux empties its buffer on an overflow, and the next
	   read starts on a packet. A read without data (r == 0) keeps it. */
	size_t add(ssize_t r);
	// Once the whole packets went out: moves the part after them to the
	// front of buf.
	void next(unsigned char *buf);

private:
	size_t held;
	size_t filled;
};

/* Takes the whole packets a collector hands on. */
class Out
{
public:
	virtual ~Out() {}
	virtual void whole(const unsigned char *p, size_t n) = 0;
};

/* For bytes in a buffer the reader does not own, such as the one ffmpeg's
   muxer writes into: nothing may be put in front of those. Bytes that are
   whole packets while nothing is held go on in place; any other bytes are
   gathered in a work buffer of the collector's own. */
class CopyCollector
{
public:
	// size: the most one call brings; a call that brings more is taken in
	// parts.
	explicit CopyCollector(size_t size);

	void feed(const unsigned char *p, size_t n, Out &out);
	size_t carry() const { return grid.carry(); }

private:
	std::vector<unsigned char> work;   // size + kPacket - 1 bytes
	Collector grid;
};

/* Sends to a client without waiting: answers what send(2) with
   MSG_DONTWAIT answers. */
class Sink
{
public:
	virtual ~Sink() {}
	virtual ssize_t send(int fd, const unsigned char *p, size_t n) = 0;
};

/* send(2) on a socket, without waiting and without SIGPIPE. */
class SocketSink : public Sink
{
public:
	ssize_t send(int fd, const unsigned char *p, size_t n);
};

/* How often one client is offered one buffer, the rest of a packet it
   still owes included, before what is left is dropped for it. Each try
   returns at once, so the reader is never held up by a client. */
const int kTries = 11;
/* A client that keeps falling behind is reported once, then at most this
   often, with the packets it lost since the last report. */
const int64_t kReportMs = 10000;

/* What each client of one stream is still owed. The caller keeps it under
   the lock that guards its set of clients and holds that lock over send(),
   so that a client cannot be removed, nor its descriptor be closed and
   handed to a new client, while bytes go out to it. */
class Clients
{
public:
	/* Offers n bytes of whole packets at buf to every client in fds: first
	   the rest of the packet a client took part of before, then the
	   buffer. A client that cannot take that rest skips the whole buffer;
	   one that takes part of the buffer is owed the rest of the packet it
	   stopped in, and loses the packets after it. Report lines are added
	   to report, for the caller to print once it let go of its lock. */
	void send(const std::set<int> &fds, const unsigned char *buf, size_t n,
		  Sink &sink, int64_t now_ms, std::vector<std::string> &report);
	/* A client that is removed: its state goes with it, so a new client
	   on the same descriptor starts on a packet. */
	void forget(int fd);
	void clear();
	// The bytes of a packet still owed to fd.
	size_t owed(int fd) const;

private:
	struct Client
	{
		unsigned char rest[kPacket - 1];
		size_t        rest_len;
		bool          reported;
		int64_t       reported_ms;
		unsigned long dropped;    // packets lost since the last report

		Client() : rest_len(0), reported(false), reported_ms(0), dropped(0) {}
	};

	void sendTo(int fd, Client &c, const unsigned char *buf, size_t n,
		    Sink &sink, int64_t now_ms, std::vector<std::string> &report);
	void lost(int fd, Client &c, unsigned long packets, int64_t now_ms,
		  std::vector<std::string> &report);

	std::map<int, Client> clients;
};

} // namespace tsgrid

#endif
