/*
 * tsgrid.cpp - whole transport stream packets for every client of a stream
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
 */

#include "tsgrid.h"

#include <cstdio>
#include <cstring>

#include <sys/socket.h>

namespace tsgrid
{

size_t Collector::add(ssize_t r)
{
	if (r < 0)
	{
		held = 0;
		filled = 0;
		return 0;
	}
	filled = held + (size_t) r;
	return filled - filled % kPacket;
}

void Collector::next(unsigned char *buf)
{
	const size_t whole = filled - filled % kPacket;
	held = filled - whole;
	if (held && whole)
		memmove(buf, buf + whole, held);
	filled = held;
}

CopyCollector::CopyCollector(size_t size)
	: work(size + kPacket - 1)
{
}

void CopyCollector::feed(const unsigned char *p, size_t n, Out &out)
{
	if (grid.carry() == 0 && n % kPacket == 0)
	{
		if (n)
			out.whole(p, n);
		return;
	}
	while (n)
	{
		const size_t room = work.size() - grid.carry();
		const size_t take = n < room ? n : room;
		memcpy(&work[grid.carry()], p, take);
		const size_t whole = grid.add((ssize_t) take);
		if (whole)
			out.whole(&work[0], whole);
		grid.next(&work[0]);
		p += take;
		n -= take;
	}
}

ssize_t SocketSink::send(int fd, const unsigned char *p, size_t n)
{
	int flags = MSG_DONTWAIT;
#ifdef MSG_NOSIGNAL
	flags |= MSG_NOSIGNAL;
#endif
	return ::send(fd, p, n, flags);
}

namespace
{

// Offers p[0..n) to fd while tries are left; the bytes it took.
size_t push(Sink &sink, int fd, const unsigned char *p, size_t n, int &tries)
{
	size_t done = 0;
	while (done < n && tries > 0)
	{
		tries--;
		const ssize_t r = sink.send(fd, p + done, n - done);
		if (r > 0)
			done += (size_t) r;
	}
	return done;
}

} // namespace

void Clients::send(const std::set<int> &fds, const unsigned char *buf, size_t n,
		   Sink &sink, int64_t now_ms, std::vector<std::string> &report)
{
	for (std::set<int>::const_iterator it = fds.begin(); it != fds.end(); ++it)
		sendTo(*it, clients[*it], buf, n, sink, now_ms, report);
}

void Clients::sendTo(int fd, Client &c, const unsigned char *buf, size_t n,
		     Sink &sink, int64_t now_ms, std::vector<std::string> &report)
{
	int tries = kTries;
	if (c.rest_len)
	{
		const size_t done = push(sink, fd, c.rest, c.rest_len, tries);
		if (done < c.rest_len)
		{
			memmove(c.rest, c.rest + done, c.rest_len - done);
			c.rest_len -= done;
			lost(fd, c, n / kPacket, now_ms, report);
			return;
		}
		c.rest_len = 0;
	}
	const size_t done = push(sink, fd, buf, n, tries);
	if (done == n)
		return;
	const size_t cut = done % kPacket;
	if (cut)
	{
		c.rest_len = kPacket - cut;
		memcpy(c.rest, buf + done, c.rest_len);
	}
	lost(fd, c, (n - done - c.rest_len) / kPacket, now_ms, report);
}

void Clients::lost(int fd, Client &c, unsigned long packets, int64_t now_ms,
		   std::vector<std::string> &report)
{
	if (!packets)
		return;
	c.dropped += packets;
	if (c.reported && now_ms - c.reported_ms < kReportMs)
		return;
	char line[96];
	snprintf(line, sizeof(line), "send error, fd %d: %lu packets dropped", fd, c.dropped);
	report.push_back(line);
	c.reported = true;
	c.reported_ms = now_ms;
	c.dropped = 0;
}

void Clients::forget(int fd)
{
	clients.erase(fd);
}

void Clients::clear()
{
	clients.clear();
}

size_t Clients::owed(int fd) const
{
	std::map<int, Client>::const_iterator it = clients.find(fd);
	return it == clients.end() ? 0 : it->second.rest_len;
}

} // namespace tsgrid
