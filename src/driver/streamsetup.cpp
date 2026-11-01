/*
 * streamsetup.cpp - the steps from an accepted client to a running stream
 *
 * Copyright (C) 2026 Thilo Graf
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

#include "streamsetup.h"

#include <cerrno>
#include <cstdio>
#include <cstring>

#include <poll.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>

#include <OpenThreads/ScopedLock>

namespace streamsetup
{

namespace
{

const size_t kLineMax = 511;
const size_t kHeadMax = 8192;
const unsigned kWriteMs = 2000;
const unsigned kDrainMs = 500;

int64_t nowMs()
{
	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	return (int64_t) ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}

// Waits for fd to be readable until the moment given; false when it passed.
bool readableBy(int fd, int64_t until)
{
	for (;;)
	{
		const int64_t left = until - nowMs();
		if (left <= 0)
			return false;
		struct pollfd p;
		p.fd = fd;
		p.events = POLLIN;
		p.revents = 0;
		const int r = poll(&p, 1, (int) left);
		if (r > 0)
			return true;
		if (r < 0 && errno != EINTR)
			return false;
	}
}

void sendTimeout(int fd, unsigned ms)
{
	struct timeval tv;
	tv.tv_sec = ms / 1000;
	tv.tv_usec = (ms % 1000) * 1000;
	setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
}

bool sendAll(int fd, const std::string &bytes)
{
	size_t done = 0;
	while (done < bytes.size())
	{
		const ssize_t n = send(fd, bytes.data() + done, bytes.size() - done, MSG_NOSIGNAL);
		if (n > 0)
			done += (size_t) n;
		else if (n < 0 && errno == EINTR)
			continue;
		else
			return false;
	}
	return true;
}

bool isHex(char c)
{
	return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
}

} // namespace

bool readRequest(int fd, unsigned budget_ms, std::string &line)
{
	line.clear();
	const int64_t until = nowMs() + budget_ms;

	while (line.size() < kLineMax)
	{
		char c;
		const ssize_t n = recv(fd, &c, 1, MSG_DONTWAIT);
		if (n == 1)
		{
			line += c;
			if (c == '\n')
				break;
			continue;
		}
		if (n == 0)
			return !line.empty();      // the peer closed: what came is the line
		if (errno == EINTR)
			continue;
		if (errno != EAGAIN && errno != EWOULDBLOCK)
			return false;
		if (!readableBy(fd, until))
			return false;              // the time ran out before the line end
	}

	/* What else is there already, read and let go, without waiting. */
	char rest[1024];
	size_t taken = 0;
	while (taken < kHeadMax)
	{
		const size_t want = kHeadMax - taken < sizeof(rest) ? kHeadMax - taken : sizeof(rest);
		const ssize_t n = recv(fd, rest, want, MSG_DONTWAIT);
		if (n <= 0)
			break;
		taken += (size_t) n;
	}
	return true;
}

Request parseRequestLine(const std::string &line)
{
	Request r;
	if (line.compare(0, 5, "GET /") != 0)
		return r;

	// The path, up to the protocol behind it or the line end.
	const size_t end = line.find_first_of(" \r\n", 4);
	const std::string path = line.substr(4, end == std::string::npos ? std::string::npos : end - 4);

	const size_t id = path.find("id=");
	if (id != std::string::npos)
	{
		size_t i = id + 3;
		size_t digits = 0;
		uint64_t v = 0;
		while (i < path.size() && isHex(path[i]) && digits < 17)
		{
			const char c = path[i];
			v = v * 16 + (uint64_t) (c <= '9' ? c - '0' : (c | 0x20) - 'a' + 10);
			++i;
			++digits;
		}
		if (digits == 0 || digits > 16 || (i < path.size() && path[i] != '&'))
		{
			r.form = BadTarget;
			return r;
		}
		r.form = ById;
		r.id = v;
		return r;
	}

	unsigned service, i1, i2, i3, i4, satpos;
	if (sscanf(path.c_str() + 1, "%X:0:%X:%X:%X:%X:%X:0:0:0:", &service, &i1, &i2, &i3, &i4, &satpos) == 6)
	{
		r.form = E2;
		r.id = (((uint64_t) i3) << 32) | (((uint64_t) i4) << 16) | (uint64_t) i2;
		r.satpos = satpos >> 16;
		return r;
	}

	if (path == "/" || path.compare(0, 2, "/?") == 0)
	{
		r.form = NoTarget;
		return r;
	}
	r.form = BadTarget;
	return r;
}

std::string statusBytes(int status, const char *reason)
{
	if (status == 200)
		return "HTTP/1.1 200 OK\r\nServer: streamts (ts)\r\n\r\n";

	const std::string body = std::string(reason ? reason : "") + "\n";
	char length[32];
	snprintf(length, sizeof(length), "%u", (unsigned) body.size());
	std::string head = status == 404 ? "HTTP/1.1 404 Not Found\r\n"
					 : "HTTP/1.1 503 Service Unavailable\r\n";
	head += "Server: streamts (ts)\r\n";
	head += "Content-Type: text/plain\r\n";
	head += std::string("Content-Length: ") + length + "\r\n";
	if (status != 404)
		head += "Retry-After: 30\r\n";
	head += "Connection: close\r\n\r\n";
	return head + body;
}

bool writeOk(int fd)
{
	sendTimeout(fd, kWriteMs);
	return sendAll(fd, statusBytes(200, 0));
}

void refuse(int fd, int status, const char *reason)
{
	sendTimeout(fd, kWriteMs);
	if (!sendAll(fd, statusBytes(status, reason)))
		return;
	shutdown(fd, SHUT_WR);
	/* A close with unread bytes in the socket resets the connection, and a
	   reset can reach the client before the refusal does. */
	const int64_t until = nowMs() + kDrainMs;
	char rest[1024];
	while (readableBy(fd, until))
	{
		const ssize_t n = recv(fd, rest, sizeof(rest), MSG_DONTWAIT);
		if (n == 0 || (n < 0 && errno != EINTR && errno != EAGAIN && errno != EWOULDBLOCK))
			break;
	}
}

StartGate::StartGate(OpenThreads::Mutex &mx, std::atomic<bool> &r)
	: m(mx), running(r), pending(0)
{
}

bool StartGate::enter()
{
	OpenThreads::ScopedLock<OpenThreads::Mutex> l(m);
	if (!running.load())
		return false;
	pending++;
	return true;
}

void StartGate::leaveLocked()
{
	pending--;
}

bool StartGate::mayPublishLocked() const
{
	return running.load();
}

int StartGate::pendingLocked() const
{
	return pending;
}

namespace
{

/* One setup's place in the count: left exactly once, in the publishing hold
   of the mutex or, on any other way out, here. */
class Pending
{
public:
	explicit Pending(StartGate &g) : gate(g), counted(true) {}
	~Pending()
	{
		if (!counted)
			return;
		OpenThreads::ScopedLock<OpenThreads::Mutex> l(gate.mutex());
		gate.leaveLocked();
	}
	void leaveLocked()
	{
		counted = false;
		gate.leaveLocked();
	}

private:
	Pending(const Pending &);
	Pending &operator=(const Pending &);
	StartGate &gate;
	bool counted;
};

/* The hold zapit answered, until a stream takes it: dropped on every way out
   that does not hand it over. */
class HoldRef
{
public:
	HoldRef(Ops &o, uint32_t i) : ops(o), held(i) {}
	~HoldRef()
	{
		if (held)
			ops.drop(held);
	}
	uint32_t id() const { return held; }
	// Dropped now, before a refusal is written, so zapit can give the
	// frontend back sooner.
	void dropNow()
	{
		if (held)
			ops.drop(held);
		held = 0;
	}
	uint32_t release()
	{
		const uint32_t i = held;
		held = 0;
		return i;
	}

private:
	HoldRef(const HoldRef &);
	HoldRef &operator=(const HoldRef &);
	Ops &ops;
	uint32_t held;
};

Outcome outcome(Result r, bool published)
{
	Outcome o;
	o.result = r;
	o.published = published;
	return o;
}

/* Under the mutex: builds the stream, which takes fd and the hold together,
   and publishes it if it starts. Consumed either way: a stream that did not
   start is discarded and has closed fd. */
Outcome publish(Ops &ops, Pending &pending, int fd, const Target &t, void *fe, uint32_t hold_id)
{
	void *stream = ops.buildLocked(fd, t, fe, hold_id);
	if (ops.startLocked(stream))
	{
		ops.publishLocked(stream);
		pending.leaveLocked();
		return outcome(Consumed, true);
	}
	ops.discardLocked(stream);
	return outcome(Consumed, false);
}


Outcome refusal(Ops &ops, int fd, int status, const char *reason)
{
	ops.refuse(fd, status, reason);
	return outcome(Refused, false);
}

const char kNoTuner[] = "no tuner is free for this channel";

} // namespace

Outcome run(int fd, StartGate &gate, Ops &ops)
{
	if (!gate.enter())
		return outcome(Refused, false);
	Pending pending(gate);

	Target t;
	switch (ops.readTarget(fd, t))
	{
	case Ops::ReadOk:
		break;
	case Ops::ReadUnknown:
		return refusal(ops, fd, 404, "no such channel");
	case Ops::ReadNoLive:
		return refusal(ops, fd, 404, "no live channel");
	case Ops::ReadGarbage:
	default:
		return outcome(Refused, false);
	}

	{
		OpenThreads::ScopedLock<OpenThreads::Mutex> l(gate.mutex());
		if (ops.runningLocked(t.channel))
		{
			/* A small write on a fresh socket, bounded, so the mutex is not
			   held for long. */
			if (!ops.writeOk(fd))
				return outcome(Refused, false);
			ops.joinLocked(t.channel, fd);
			return outcome(Consumed, false);
		}
	}

	if (t.kind == Web)
	{
		OpenThreads::ScopedLock<OpenThreads::Mutex> l(gate.mutex());
		if (!gate.mayPublishLocked() || !ops.writeOk(fd))
			return outcome(Refused, false);
		return publish(ops, pending, fd, t, 0, 0);
	}
	if (t.kind != Broadcast)
		return refusal(ops, fd, 404, "no such channel");

	if (ops.recording(t.channel))
		return refusal(ops, fd, 503, kNoTuner);

	HoldRef hold(ops, ops.hold(t.channel));
	if (!hold.id())
		return refusal(ops, fd, 503, kNoTuner);
	void *fe = ops.adopt(hold.id());
	if (!fe || !ops.preparePids(t))
	{
		hold.dropNow();
		return refusal(ops, fd, 503, kNoTuner);
	}

	OpenThreads::ScopedLock<OpenThreads::Mutex> l(gate.mutex());
	if (!gate.mayPublishLocked() || !ops.writeOk(fd))
		return outcome(Refused, false);
	return publish(ops, pending, fd, t, fe, hold.release());
}

} // namespace streamsetup
