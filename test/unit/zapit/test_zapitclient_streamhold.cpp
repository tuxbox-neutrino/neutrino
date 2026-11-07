/*
 * test_zapitclient_streamhold.cpp - asking zapit to hold a frontend for a stream
 *
 * Copyright (C) 2026 Thilo Graf
 *
 * License: GPL-2.0-or-later
 *
 * CZapitClient::holdForStream, probeStream and yieldStreams over a real
 * socket, against a peer that stands in for zapit: one that answers, one
 * that answers late, one that answers short, and none at all. The client is the one the program
 * ships with its address replaced.
 */

#include "support/catch.hpp"

#include <cerrno>
#include <cstdio>
#include <cstring>
#include <string>

#include <pthread.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/un.h>
#include <time.h>
#include <unistd.h>

#include <connection/basicclient.h>
#include <connection/basicmessage.h>
#include <zapit/client/zapitclient.h>
#include <zapit/client/msgtypes.h>
#include <zapit/streamholdcmd.h>

namespace
{

const char *privateSocketName()
{
	static std::string name;
	if (name.empty())
	{
		char buf[64];
		std::snprintf(buf, sizeof(buf), "/tmp/coreapi-zapit-hold.%d.sock", (int) getpid());
		name = buf;
	}
	return name.c_str();
}

class ZapitClientHere : public CZapitClient
{
private:
	const char *getSocketName() const { return privateSocketName(); }
};

int64_t monotonicMs()
{
	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	return (int64_t) ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}

template <typename T>
std::string bytesOf(const T &r)
{
	return std::string(reinterpret_cast<const char *>(&r), sizeof(r));
}

/* One exchange: reads the header and the body the client sends, waits as long
   as it is told to, sends as many bytes of the reply as it is told to. */
class Peer
{
public:
	Peer(const std::string &r, size_t bytes, unsigned delay_ms)
		: got_header(false), got_body(false), reply(r), reply_len(bytes), delay(delay_ms),
		  listen_fd(-1), joined(false)
	{
		memset(&header, 0, sizeof(header));
		unlink(privateSocketName());
		listen_fd = socket(AF_UNIX, SOCK_STREAM, 0);
		REQUIRE(listen_fd != -1);
		sockaddr_un addr;
		memset(&addr, 0, sizeof(addr));
		addr.sun_family = AF_UNIX;
		strncpy(addr.sun_path, privateSocketName(), sizeof(addr.sun_path) - 1);
		REQUIRE(bind(listen_fd, (sockaddr *) &addr, sizeof(addr)) == 0);
		REQUIRE(listen(listen_fd, 1) == 0);
		struct timeval tv;
		tv.tv_sec = 30;
		tv.tv_usec = 0;
		REQUIRE(setsockopt(listen_fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) == 0);
		REQUIRE(pthread_create(&thread, 0, &serve, this) == 0);
	}

	~Peer()
	{
		finish();
		close(listen_fd);
		unlink(privateSocketName());
	}

	void finish()
	{
		if (!joined)
			pthread_join(thread, 0);
		joined = true;
	}

	CBasicMessage::Header header;
	CZapitMessages::commandStreamHold body;
	bool got_header;
	bool got_body;

private:
	static void *serve(void *arg)
	{
		static_cast<Peer *>(arg)->loop();
		return 0;
	}

	bool readAll(int fd, void *to, size_t n)
	{
		char *p = static_cast<char *>(to);
		while (n > 0)
		{
			const ssize_t got = recv(fd, p, n, 0);
			if (got <= 0)
				return false;
			p += got;
			n -= (size_t) got;
		}
		return true;
	}

	void loop()
	{
		const int fd = accept(listen_fd, 0, 0);
		if (fd < 0)
			return;
		got_header = readAll(fd, &header, sizeof(header));
		got_body = got_header && readAll(fd, &body, sizeof(body));
		if (delay)
			usleep(delay * 1000);
		size_t sent = 0;
		const char *p = reply.data();
		while (sent < reply_len)
		{
			const ssize_t one = send(fd, p + sent, reply_len - sent, MSG_NOSIGNAL);
			if (one <= 0)
				break;
			sent += (size_t) one;
		}
		close(fd);
	}

	std::string reply;
	size_t reply_len;
	unsigned delay;
	int listen_fd;
	pthread_t thread;
	bool joined;
};

CZapitClient::responseStreamHold granted(uint32_t id, int fe)
{
	CZapitClient::responseStreamHold r;
	r.verdict = 0;
	r.hold_id = id;
	r.fenumber = fe;
	return r;
}

} // namespace

TEST_CASE("a hold sends its channel and deadline and reads the whole answer", "[zapit][client][streamhold]")
{
	Peer peer(bytesOf(granted(0x1234, 2)), sizeof(CZapitClient::responseStreamHold), 0);
	ZapitClientHere client;
	CZapitClient::responseStreamHold r;

	const int64_t before = monotonicMs();
	REQUIRE(client.holdForStream(0xb544000800850088ULL, 10000, r));
	peer.finish();

	REQUIRE(r.verdict == 0);
	REQUIRE(r.hold_id == 0x1234u);
	REQUIRE(r.fenumber == 2);

	REQUIRE(peer.got_body);
	REQUIRE(peer.header.cmd == CZapitMessages::CMD_STREAM_HOLD);
	const int version = CZapitMessages::ACTVERSION;   // a copy: the constant has no storage
	REQUIRE(peer.header.version == version);
	REQUIRE(peer.body.channel_id == 0xb544000800850088ULL);
	// On the clock zapit reads, ten seconds out.
	REQUIRE(peer.body.deadline_ms >= before + 10000);
	REQUIRE(peer.body.deadline_ms <= monotonicMs() + 10000);
	REQUIRE(CZapitMessages::streamHoldClockMs() >= before);
}

TEST_CASE("a probe sends the probe command and reads a refusal as it came", "[zapit][client][streamhold]")
{
	CZapitClient::responseStreamHold busy;
	busy.verdict = 1;
	busy.hold_id = 0;
	busy.fenumber = -1;
	Peer peer(bytesOf(busy), sizeof(busy), 0);
	ZapitClientHere client;
	CZapitClient::responseStreamHold r = granted(7, 7);

	REQUIRE(client.probeStream(0x1001, r));
	peer.finish();
	REQUIRE(peer.header.cmd == CZapitMessages::CMD_STREAM_PROBE);
	REQUIRE(peer.body.channel_id == 0x1001);
	REQUIRE(r.verdict == 1);
	REQUIRE(r.hold_id == 0);
	REQUIRE(r.fenumber == -1);
}

TEST_CASE("an answer that comes too late is given up at the thread's deadline", "[zapit][client][streamhold]")
{
	Peer peer(bytesOf(granted(0x99, 1)), sizeof(CZapitClient::responseStreamHold), 1500);
	ZapitClientHere client;
	CZapitClient::responseStreamHold r = granted(5, 5);

	const int64_t start = monotonicMs();
	bool ok;
	{
		CBasicClient::Deadline d(300);
		ok = client.holdForStream(0x1001, 10000, r);
	}
	const int64_t took = monotonicMs() - start;
	REQUIRE_FALSE(ok);
	REQUIRE(took < 1200);
	// A failure without a hold, not the caller's earlier value.
	REQUIRE(r.verdict == 3);
	REQUIRE(r.hold_id == 0);
	REQUIRE(r.fenumber == -1);
}

TEST_CASE("an answer cut short is not taken in part", "[zapit][client][streamhold]")
{
	Peer peer(bytesOf(granted(0xabcd, 1)), sizeof(int32_t) + 2, 0);
	ZapitClientHere client;
	CZapitClient::responseStreamHold r = granted(5, 5);

	REQUIRE_FALSE(client.holdForStream(0x1001, 10000, r));
	peer.finish();
	REQUIRE(r.verdict == 3);
	REQUIRE(r.hold_id == 0);
	REQUIRE(r.fenumber == -1);
}

TEST_CASE("no zapit to ask is a failure without a hold", "[zapit][client][streamhold]")
{
	unlink(privateSocketName());
	ZapitClientHere client;
	CZapitClient::responseStreamHold r = granted(5, 5);

	REQUIRE_FALSE(client.holdForStream(0x1001, 10000, r));
	REQUIRE(r.verdict == 3);
	REQUIRE(r.hold_id == 0);
	REQUIRE_FALSE(client.probeStream(0x1001, r));
	REQUIRE(r.verdict == 3);
}

namespace
{

CZapitClient::responseStreamYield yielded(int32_t stage, uint32_t mask)
{
	CZapitClient::responseStreamYield r;
	r.stage = stage;
	r.fe_mask = mask;
	return r;
}

} // namespace

TEST_CASE("a yield sends its channel and deadline and reads the whole answer", "[zapit][client][streamhold][streamyield]")
{
	Peer peer(bytesOf(yielded(2, 0x2)), sizeof(CZapitClient::responseStreamYield), 0);
	ZapitClientHere client;
	CZapitClient::responseStreamYield r;

	const int64_t before = monotonicMs();
	REQUIRE(client.yieldStreams(0xb544000800850088ULL, 12000, r));
	peer.finish();

	REQUIRE(r.stage == 2);
	REQUIRE(r.fe_mask == 0x2u);
	REQUIRE(peer.got_body);
	REQUIRE(peer.header.cmd == CZapitMessages::CMD_STREAM_YIELD);
	REQUIRE(peer.body.channel_id == 0xb544000800850088ULL);
	REQUIRE(peer.body.deadline_ms >= before + 12000);
	REQUIRE(peer.body.deadline_ms <= monotonicMs() + 12000);
}

TEST_CASE("a yield answer that comes too late is given up and ended nothing", "[zapit][client][streamhold][streamyield]")
{
	Peer peer(bytesOf(yielded(2, 0x1)), sizeof(CZapitClient::responseStreamYield), 1500);
	ZapitClientHere client;
	CZapitClient::responseStreamYield r = yielded(3, 0xf);

	const int64_t start = monotonicMs();
	bool ok;
	{
		CBasicClient::Deadline d(300);
		ok = client.yieldStreams(0x1001, 12000, r);
	}
	REQUIRE_FALSE(ok);
	REQUIRE(monotonicMs() - start < 1200);
	REQUIRE(r.stage == -1);
	REQUIRE(r.fe_mask == 0);
}

TEST_CASE("a yield answer cut short is not taken in part", "[zapit][client][streamhold][streamyield]")
{
	Peer peer(bytesOf(yielded(2, 0x1)), sizeof(int32_t) + 2, 0);
	ZapitClientHere client;
	CZapitClient::responseStreamYield r = yielded(3, 0xf);

	REQUIRE_FALSE(client.yieldStreams(0x1001, 12000, r));
	peer.finish();
	REQUIRE(r.stage == -1);
	REQUIRE(r.fe_mask == 0);
}

TEST_CASE("no zapit to ask for a yield ended nothing", "[zapit][client][streamhold][streamyield]")
{
	unlink(privateSocketName());
	ZapitClientHere client;
	CZapitClient::responseStreamYield r = yielded(3, 0xf);

	REQUIRE_FALSE(client.yieldStreams(0x1001, 12000, r));
	REQUIRE(r.stage == -1);
	REQUIRE(r.fe_mask == 0);
}

TEST_CASE("a hold and a probe ask about a switch of the picture only when told, and read the answer to it", "[zapit][client][streamhold][switch]")
{
	CZapitClient::responseStreamHold busy;
	busy.verdict = 1;
	busy.hold_id = 0;
	busy.fenumber = -1;
	busy.switch_frees = 1;
	{
		Peer peer(bytesOf(busy), sizeof(busy), 0);
		ZapitClientHere client;
		CZapitClient::responseStreamHold r;
		REQUIRE(client.holdForStream(0x1001, 10000, r, true));
		peer.finish();
		REQUIRE(peer.body.ask_switch == 1);
		REQUIRE(r.verdict == 1);
		REQUIRE(r.switch_frees == 1);
	}
	{
		Peer peer(bytesOf(busy), sizeof(busy), 0);
		ZapitClientHere client;
		CZapitClient::responseStreamHold r;
		REQUIRE(client.probeStream(0x1001, r));
		peer.finish();
		REQUIRE(peer.header.cmd == CZapitMessages::CMD_STREAM_PROBE);
		REQUIRE(peer.body.ask_switch == 0);
	}
	{
		Peer peer(bytesOf(busy), sizeof(busy), 0);
		ZapitClientHere client;
		CZapitClient::responseStreamHold r;
		REQUIRE(client.probeStream(0x1001, r, true));
		peer.finish();
		REQUIRE(peer.body.ask_switch == 1);
		REQUIRE(r.switch_frees == 1);
	}
}

TEST_CASE("a switch for a stream sends its channel and reads whether the picture was switched", "[zapit][client][streamhold][switch]")
{
	CZapitClient::responseStreamSwitch yes;
	yes.switched = 1;
	{
		Peer peer(bytesOf(yes), sizeof(yes), 0);
		ZapitClientHere client;
		int outcome = 0;
		REQUIRE(client.switchForStream(0xb544000800850088ULL, outcome));
		peer.finish();
		REQUIRE(peer.header.cmd == CZapitMessages::CMD_STREAM_SWITCH);
		REQUIRE(peer.body.channel_id == 0xb544000800850088ULL);
		REQUIRE(outcome == 1);
	}
	{
		CZapitClient::responseStreamSwitch no;
		Peer peer(bytesOf(no), sizeof(no), 0);
		ZapitClientHere client;
		int outcome = 1;
		REQUIRE(client.switchForStream(0x1001, outcome));
		peer.finish();
		REQUIRE(outcome == 0);
	}
	{
		CZapitClient::responseStreamSwitch free_now;
		free_now.switched = 2;
		Peer peer(bytesOf(free_now), sizeof(free_now), 0);
		ZapitClientHere client;
		int outcome = 0;
		REQUIRE(client.switchForStream(0x1001, outcome));
		peer.finish();
		REQUIRE(outcome == 2);
	}
	{
		// Cut short: no answer, and nothing switched.
		Peer peer(bytesOf(yes), 2, 0);
		ZapitClientHere client;
		int outcome = 1;
		REQUIRE_FALSE(client.switchForStream(0x1001, outcome));
		peer.finish();
		REQUIRE(outcome == 0);
	}
}

TEST_CASE("the reply zapit sends for a hold is the client's reply, switch prediction included", "[zapit][client][streamhold][switch]")
{
	streamholdcmd::Reply r;
	r.verdict = 1;
	r.hold_id = 0;
	r.fenumber = -1;
	r.switch_frees = 1;
	const CZapitClient::responseStreamHold wire = streamholdcmd::toWire<CZapitClient::responseStreamHold>(r);
	Peer peer(bytesOf(wire), sizeof(wire), 0);
	ZapitClientHere client;
	CZapitClient::responseStreamHold got;
	REQUIRE(client.probeStream(0x1001, got, true));
	peer.finish();
	REQUIRE(got.verdict == 1);
	REQUIRE(got.switch_frees == 1);
}
