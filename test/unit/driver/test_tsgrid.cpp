/*
 * test_tsgrid.cpp - whole transport stream packets for every client of a stream
 *
 * Copyright (C) 2026 Tuxbox-Project (stream packet grid, tests)
 *
 * License: GPL-2.0-or-later
 *
 * tsgrid keeps what the stream server reads and what each client gets on
 * the 188-byte packet grid. These cases drive the shipped object over a
 * scripted send: each call to it takes as many bytes as the script says,
 * or none, and what a client took is kept per descriptor, so a case reads
 * the stream a client sees.
 */

#include "support/catch.hpp"

#include <driver/tsgrid.h>

#include <cstring>
#include <deque>
#include <map>
#include <set>
#include <string>
#include <vector>

#include <sys/socket.h>
#include <unistd.h>

using tsgrid::kPacket;

namespace
{

typedef std::vector<unsigned char> Bytes;

/* Packet number k of a source: the sync byte, then k in the next four bytes
   and as the fill byte, so a case can tell packets apart and find a cut. */
Bytes packet(unsigned k)
{
	Bytes p(kPacket, (unsigned char) (k & 0x7f));
	p[0] = 0x47;
	p[1] = (unsigned char) (k >> 24);
	p[2] = (unsigned char) (k >> 16);
	p[3] = (unsigned char) (k >> 8);
	p[4] = (unsigned char) k;
	return p;
}

Bytes packets(unsigned first, unsigned count)
{
	Bytes all;
	for (unsigned k = first; k < first + count; ++k)
	{
		const Bytes p = packet(k);
		all.insert(all.end(), p.begin(), p.end());
	}
	return all;
}

// The packet numbers in a run of bytes that lies on the grid; fails the
// case where a packet does not start with the sync byte or is cut short.
std::vector<unsigned> numbers(const Bytes &b)
{
	REQUIRE(b.size() % kPacket == 0);
	std::vector<unsigned> n;
	for (size_t off = 0; off < b.size(); off += kPacket)
	{
		REQUIRE(b[off] == 0x47);
		const unsigned k = (unsigned) b[off + 1] << 24 | (unsigned) b[off + 2] << 16 |
				   (unsigned) b[off + 3] << 8 | b[off + 4];
		REQUIRE(Bytes(b.begin() + off, b.begin() + off + kPacket) == packet(k));
		n.push_back(k);
	}
	return n;
}

std::vector<unsigned> range(unsigned first, unsigned count)
{
	std::vector<unsigned> n;
	for (unsigned k = first; k < first + count; ++k)
		n.push_back(k);
	return n;
}

std::vector<unsigned> operator+(std::vector<unsigned> a, const std::vector<unsigned> &b)
{
	a.insert(a.end(), b.begin(), b.end());
	return a;
}

/* A send whose answers a case scripts per descriptor: each call takes the
   next entry, at most that many bytes, -1 for none (a full socket). With
   no entry left a call takes all it is offered. */
class ScriptedSink : public tsgrid::Sink
{
public:
	ssize_t send(int fd, const unsigned char *p, size_t n)
	{
		calls[fd]++;
		std::deque<ssize_t> &s = script[fd];
		ssize_t take = (ssize_t) n;
		if (!s.empty())
		{
			take = s.front();
			s.pop_front();
			if (take < 0)
				return -1;
			if ((size_t) take > n)
				take = (ssize_t) n;
		}
		got[fd].insert(got[fd].end(), p, p + take);
		return take;
	}
	void takes(int fd, ssize_t n) { script[fd].push_back(n); }
	// A full socket for the rest of one buffer.
	void full(int fd) { for (int i = 0; i < tsgrid::kTries; ++i) takes(fd, -1); }

	std::map<int, std::deque<ssize_t> > script;
	std::map<int, Bytes> got;
	std::map<int, int> calls;
};

/* Offers one buffer to the clients; the script written for it ends with
   it, whatever of it was not used. */
void offer(tsgrid::Clients &clients, ScriptedSink &sink, const std::set<int> &fds,
	   const Bytes &b, int64_t now_ms, std::vector<std::string> &report)
{
	clients.send(fds, &b[0], b.size(), sink, now_ms, report);
	sink.script.clear();
}

std::set<int> fdSet(int a, int b = -1)
{
	std::set<int> s;
	s.insert(a);
	if (b >= 0)
		s.insert(b);
	return s;
}

// A demux to read from: a run of packets, handed out in reads of the size
// asked for, as far as the room given and the run allow.
struct Source
{
	Bytes bytes;
	size_t pos;
	Source(unsigned first, unsigned count) : bytes(packets(first, count)), pos(0) {}
	ssize_t read(unsigned char *to, size_t room, size_t want)
	{
		size_t n = want < room ? want : room;
		if (n > bytes.size() - pos)
			n = bytes.size() - pos;
		memcpy(to, &bytes[pos], n);
		pos += n;
		return (ssize_t) n;
	}
};

class Gather : public tsgrid::Out
{
public:
	void whole(const unsigned char *p, size_t n)
	{
		REQUIRE(n % kPacket == 0);
		at.push_back(p);
		all.insert(all.end(), p, p + n);
	}
	std::vector<const unsigned char *> at;
	Bytes all;
};

} // namespace

TEST_CASE("G1: a client that took part of a packet gets the rest of it first", "[driver][tsgrid]")
{
	tsgrid::Clients clients;
	ScriptedSink sink;
	std::vector<std::string> report;

	// The first buffer: 200 bytes go out, the socket is full for the rest.
	sink.takes(5, 200);
	sink.full(5);
	offer(clients, sink, fdSet(5), packets(0, 3), 0, report);
	REQUIRE(sink.got[5].size() == 200);
	REQUIRE(clients.owed(5) == 2 * kPacket - 200);

	// The next one goes out whole, after the rest of packet 1.
	offer(clients, sink, fdSet(5), packets(3, 3), 0, report);
	REQUIRE(clients.owed(5) == 0);
	REQUIRE(numbers(sink.got[5]) == range(0, 2) + range(3, 3));
}

TEST_CASE("G2: buffers are skipped whole while the rest of a packet is still owed", "[driver][tsgrid]")
{
	tsgrid::Clients clients;
	ScriptedSink sink;
	std::vector<std::string> report;

	sink.takes(5, 100);
	sink.full(5);
	offer(clients, sink, fdSet(5), packets(0, 3), 0, report);
	REQUIRE(clients.owed(5) == kPacket - 100);

	// 40 bytes of the rest, then full: the second buffer is not offered.
	sink.takes(5, 40);
	sink.full(5);
	offer(clients, sink, fdSet(5), packets(3, 3), 0, report);
	REQUIRE(clients.owed(5) == kPacket - 140);
	REQUIRE(sink.got[5].size() == 140);

	// The rest goes out in two calls, then the third buffer whole.
	sink.takes(5, 20);
	offer(clients, sink, fdSet(5), packets(6, 3), 0, report);
	REQUIRE(clients.owed(5) == 0);
	REQUIRE(numbers(sink.got[5]) == range(0, 1) + range(6, 3));
}

TEST_CASE("G3: reads that end inside a packet hand on whole packets in order", "[driver][tsgrid]")
{
	const size_t size = 5 * kPacket;   // the read buffer
	Bytes buf(size);
	Source src(0, 40);
	tsgrid::Collector grid;
	Bytes out;
	const size_t reads[] = { 100, 300, 50, 1000, 187, 189, 376, 1, 940 };
	const size_t scripted = sizeof(reads) / sizeof(reads[0]);
	for (size_t i = 0; src.pos < src.bytes.size() && i < 100; ++i)
	{
		// The scripted sizes, then whatever fits until the source is read.
		const size_t want = i < scripted ? reads[i] : size;
		const ssize_t r = src.read(&buf[grid.carry()], size - grid.carry(), want);
		const size_t whole = grid.add(r);
		REQUIRE(whole % kPacket == 0);
		out.insert(out.end(), buf.begin(), buf.begin() + whole);
		grid.next(&buf[0]);
		REQUIRE(grid.carry() < kPacket);
	}
	REQUIRE(src.pos == src.bytes.size());
	REQUIRE(numbers(out) == range(0, 40));
	REQUIRE(grid.carry() == 0);
}

TEST_CASE("G4: a read error drops the part held and a read without data keeps it", "[driver][tsgrid]")
{
	const size_t size = 5 * kPacket;
	Bytes buf(size);
	tsgrid::Collector grid;
	Bytes out;

	Source a(0, 3);
	// 300 bytes: packet 0 and part of packet 1.
	size_t whole = grid.add(a.read(&buf[grid.carry()], size - grid.carry(), 300));
	out.insert(out.end(), buf.begin(), buf.begin() + whole);
	grid.next(&buf[0]);
	REQUIRE(grid.carry() == 300 - kPacket);

	// No data: the part stays.
	REQUIRE(grid.add(0) == 0);
	grid.next(&buf[0]);
	REQUIRE(grid.carry() == 300 - kPacket);

	// An error: the demux starts over on a packet, here packet 7.
	REQUIRE(grid.add(-1) == 0);
	grid.next(&buf[0]);
	REQUIRE(grid.carry() == 0);
	Source b(7, 2);
	whole = grid.add(b.read(&buf[grid.carry()], size - grid.carry(), 2 * kPacket));
	out.insert(out.end(), buf.begin(), buf.begin() + whole);
	grid.next(&buf[0]);

	REQUIRE(numbers(out) == range(0, 1) + range(7, 2));
}

TEST_CASE("G4b: a buffer the reader does not own goes on in place or through the collector's own", "[driver][tsgrid]")
{
	tsgrid::CopyCollector collector(4 * kPacket);
	Gather g;

	// Whole packets with nothing held: in place.
	const Bytes first = packets(0, 4);
	collector.feed(&first[0], first.size(), g);
	REQUIRE(g.at.size() == 1);
	REQUIRE(g.at[0] == &first[0]);

	// Then pieces that end anywhere, one larger than the size given.
	const Bytes rest = packets(4, 12);
	size_t off = 0;
	const size_t pieces[] = { 100, 4 * kPacket, 7 * kPacket + 5, 0 };
	for (size_t i = 0; off < rest.size(); ++i)
	{
		size_t n = pieces[i] ? pieces[i] : rest.size() - off;
		if (n > rest.size() - off)
			n = rest.size() - off;
		collector.feed(&rest[off], n, g);
		off += n;
	}
	REQUIRE(collector.carry() == 0);
	for (size_t i = 1; i < g.at.size(); ++i)
		REQUIRE((g.at[i] < &rest[0] || g.at[i] >= &rest[0] + rest.size()));
	REQUIRE(numbers(g.all) == range(0, 16));
}

TEST_CASE("G5: a full client does not keep a free one from getting everything", "[driver][tsgrid]")
{
	tsgrid::Clients clients;
	ScriptedSink sink;
	std::vector<std::string> report;
	for (unsigned i = 0; i < 4; ++i)
	{
		sink.full(5);
		sink.calls[5] = 0;
		offer(clients, sink, fdSet(5, 6), packets(3 * i, 3), 0, report);
		// The full one is tried a bounded number of times per buffer.
		REQUIRE(sink.calls[5] == tsgrid::kTries);
	}
	REQUIRE(sink.got[5].empty());
	REQUIRE(numbers(sink.got[6]) == range(0, 12));
}

TEST_CASE("G5b: the rest of a packet and the buffer share the tries of one buffer", "[driver][tsgrid]")
{
	tsgrid::Clients clients;
	ScriptedSink sink;
	std::vector<std::string> report;

	sink.takes(5, 10);
	sink.full(5);
	offer(clients, sink, fdSet(5), packets(0, 2), 0, report);

	// The rest takes one try per byte; nothing is left for the buffer.
	sink.calls[5] = 0;
	for (int i = 0; i < tsgrid::kTries; ++i)
		sink.takes(5, 1);
	offer(clients, sink, fdSet(5), packets(2, 2), 0, report);
	REQUIRE(sink.calls[5] == tsgrid::kTries);
	REQUIRE(clients.owed(5) == kPacket - 10 - tsgrid::kTries);
}

TEST_CASE("G6: a removed client's rest goes with it, and a new client on its descriptor starts on a packet", "[driver][tsgrid]")
{
	tsgrid::Clients clients;
	ScriptedSink sink;
	std::vector<std::string> report;

	sink.takes(5, 200);
	sink.full(5);
	offer(clients, sink, fdSet(5), packets(0, 3), 0, report);
	REQUIRE(clients.owed(5) > 0);

	clients.forget(5);
	REQUIRE(clients.owed(5) == 0);

	// The descriptor goes to a new client.
	sink.got[5].clear();
	offer(clients, sink, fdSet(5), packets(3, 2), 0, report);
	REQUIRE(numbers(sink.got[5]) == range(3, 2));
}

TEST_CASE("G7: a client that keeps falling behind is reported once, then every ten seconds with the sum", "[driver][tsgrid]")
{
	tsgrid::Clients clients;
	ScriptedSink sink;
	std::vector<std::string> report;
	const Bytes three = packets(0, 3);

	sink.full(5);
	offer(clients, sink, fdSet(5), three, 1000, report);
	REQUIRE(report.size() == 1);
	REQUIRE(report[0] == "send error, fd 5: 3 packets dropped");

	const int64_t later[] = { 2000, 6000, 10999 };
	for (size_t i = 0; i < 3; ++i)
	{
		sink.full(5);
		offer(clients, sink, fdSet(5), three, later[i], report);
	}
	REQUIRE(report.size() == 1);

	sink.full(5);
	offer(clients, sink, fdSet(5), three, 11000, report);
	REQUIRE(report.size() == 2);
	REQUIRE(report[1] == "send error, fd 5: 12 packets dropped");

	// A client that keeps up is not reported.
	offer(clients, sink, fdSet(6), three, 30000, report);
	REQUIRE(report.size() == 2);
}

TEST_CASE("the socket send gives up at once on a full socket", "[driver][tsgrid]")
{
	int sv[2];
	REQUIRE(socketpair(AF_UNIX, SOCK_STREAM, 0, sv) == 0);
	tsgrid::SocketSink sink;
	const Bytes chunk = packets(0, 64);
	ssize_t r = 0;
	size_t sent = 0;
	for (int i = 0; i < 100000 && (r = sink.send(sv[0], &chunk[0], chunk.size())) > 0; ++i)
		sent += (size_t) r;
	REQUIRE(sent > 0);
	REQUIRE(r < 0);
	close(sv[1]);
	// A peer that is gone does not raise a signal.
	REQUIRE(sink.send(sv[0], &chunk[0], chunk.size()) < 0);
	close(sv[0]);
}
