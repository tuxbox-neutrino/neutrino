/*
 * test_carry.cpp - answers the bridge carries on threads of their own
 *
 * Copyright (C) 2026 Thilo Graf
 *
 * License: GPL-2.0-or-later
 *
 * What is checked is the transport around a carrier, not a carrier. The
 * cases install a carrier of their own, which does what the answer it
 * carries names ("test:<what>"), and reach the transport two ways: over a
 * real socket through nhttpd, where a hook ahead of the bridge answers a
 * test path with a carried Response, and straight through carry::start() on
 * a socket pair, where a case has to stop the protocol at one of its points.
 */

#include "support/catch.hpp"
#include "support/answers.h"
#include "support/wireserver.h"

#include "httpd/carry.h"
#include "httpd/endpoint.h"
#include "httpd/events.h"
#include "httpd/http.h"
#include "httpd/nhttpd_bridge.h"
#include "httpd/router.h"
#include "httpd/status.h"
#include "coreapi/base/errors.h"

#include "yhook.h"
#include "ywebserver.h"

#include <atomic>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <new>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include <fcntl.h>
#include <poll.h>
#include <pthread.h>
#include <signal.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <unistd.h>

using namespace httpd;

namespace
{

const char kCarriedPath[] = "/api/v1/test/carried";
const char kPlainPath[]   = "/api/v1/test/plain";

// What the case's carrier saw and did, for the case to read back.
struct Probe
{
	std::atomic<int>            started;
	std::atomic<int>            finished;
	std::atomic<bool>           release;
	std::atomic<bool>           saw_cancelled;
	std::atomic<int>            adopted;      // -1 not asked, 0 refused, 1 entered
	std::atomic<int>            child;
	std::atomic<int>            descendant;   // a child of the child, in its group
	std::atomic<int>            reaped;       // what endChild answered
	std::atomic<bool>           refused_after; // refuse() after a broken header
	std::atomic<unsigned>      *served;       // the wire server's count, for "late"

	void reset()
	{
		started = 0;
		finished = 0;
		release = false;
		saw_cancelled = false;
		adopted = -1;
		child = 0;
		descendant = 0;
		reaped = 0;
		refused_after = true;
		served = NULL;
	}
};

Probe &probe()
{
	static Probe p;
	return p;
}

bool waitFor(const std::atomic<bool> &flag, int budget_ms)
{
	for (int waited = 0; waited < budget_ms; waited += 5)
	{
		if (flag.load())
			return true;
		napMs(5);
	}
	return flag.load();
}

bool waitForCount(const std::atomic<int> &count, int want, int budget_ms)
{
	for (int waited = 0; waited < budget_ms; waited += 5)
	{
		if (count.load() >= want)
			return true;
		napMs(5);
	}
	return count.load() >= want;
}

// Polled, because a job counts itself out as the last thing it does.
size_t waitForRunning(size_t want, int budget_ms)
{
	for (int waited = 0; waited < budget_ms; waited += 5)
	{
		if (carry::running() == want)
			return want;
		napMs(5);
	}
	return carry::running();
}

std::string modeOf(const Response &a)
{
	const std::string named = !a.stream_argv.empty() ? a.stream_argv[0] : a.relay_url;
	return (named.compare(0, 5, "test:") == 0) ? named.substr(5) : std::string();
}

/* A child that waits to be ended, leading a process group of its own as a
   carrier's converter does. Only async-signal-safe calls after the fork: the
   suite runs threads. Every signal is blocked across the fork, so a SIGTERM
   that arrives before the child has put the default action back waits for
   it, instead of running the test runner's own handler in the child. */
pid_t spawnWaiter()
{
	sigset_t all, was;
	sigfillset(&all);
	pthread_sigmask(SIG_SETMASK, &all, &was);
	const pid_t pid = fork();
	if (pid == 0)
	{
		setpgid(0, 0);
		struct sigaction dfl;
		std::memset(&dfl, 0, sizeof(dfl));
		dfl.sa_handler = SIG_DFL;
		sigaction(SIGTERM, &dfl, NULL);
		sigset_t none;
		sigemptyset(&none);
		sigprocmask(SIG_SETMASK, &none, NULL);
		for (;;)
			pause();
	}
	pthread_sigmask(SIG_SETMASK, &was, NULL);
	if (pid > 0)
		setpgid(pid, pid);
	return pid;
}

/* A child that leaves on SIGTERM, leading a group in which a child of its
   own ignores SIGTERM: what a converter that started a helper leaves behind
   when only the child it was started as is waited for. The descendant's id
   comes back through a pipe once it ignores the signal, so a SIGTERM to the
   group cannot reach it before that. */
pid_t spawnGroup(pid_t *descendant)
{
	*descendant = 0;
	int ids[2];
	if (pipe(ids) != 0)
		return -1;
	sigset_t all, was;
	sigfillset(&all);
	pthread_sigmask(SIG_SETMASK, &all, &was);
	const pid_t pid = fork();
	if (pid == 0)
	{
		setpgid(0, 0);
		sigset_t none;
		sigemptyset(&none);
		const pid_t inner = fork();
		if (inner == 0)
		{
			struct sigaction ign;
			std::memset(&ign, 0, sizeof(ign));
			ign.sa_handler = SIG_IGN;
			sigaction(SIGTERM, &ign, NULL);
			sigprocmask(SIG_SETMASK, &none, NULL);
			const pid_t self = getpid();
			if (write(ids[1], &self, sizeof(self)) != (ssize_t) sizeof(self))
				_exit(1);
			for (;;)
				pause();
		}
		struct sigaction dfl;
		std::memset(&dfl, 0, sizeof(dfl));
		dfl.sa_handler = SIG_DFL;
		sigaction(SIGTERM, &dfl, NULL);
		sigprocmask(SIG_SETMASK, &none, NULL);
		for (;;)
			pause();
	}
	pthread_sigmask(SIG_SETMASK, &was, NULL);
	close(ids[1]);
	if (pid > 0)
	{
		setpgid(pid, pid);
		pid_t inner = 0;
		if (read(ids[0], &inner, sizeof(inner)) == (ssize_t) sizeof(inner))
			*descendant = inner;
	}
	close(ids[0]);
	return pid;
}

// Whether a process still runs: gone, or a zombie waiting to be collected,
// is not running.
bool processRuns(pid_t pid)
{
	char name[64];
	std::snprintf(name, sizeof(name), "/proc/%d/stat", (int) pid);
	FILE *f = std::fopen(name, "r");
	if (f == NULL)
		return false;
	char line[512] = { 0 };
	const bool read_one = std::fgets(line, sizeof(line), f) != NULL;
	std::fclose(f);
	if (!read_one)
		return false;
	const char *after = std::strrchr(line, ')');
	if (after == NULL || after[1] != ' ')
		return false;
	return after[2] != 'Z' && after[2] != 'X';
}

bool stopsRunning(pid_t pid, int budget_ms)
{
	for (int waited = 0; waited < budget_ms; waited += 10)
	{
		if (!processRuns(pid))
			return true;
		napMs(10);
	}
	return !processRuns(pid);
}

bool sendText(carry::Job &j, const char *text)
{
	return carry::sendBody(j, text, std::strlen(text));
}

void runCase(carry::Job &j)
{
	Probe &p = probe();
	const std::string mode = modeOf(carry::answerOf(j));
	++p.started;

	if (mode == "refuse")
	{
		carry::refuse(j, problemResponse(StatusServiceUnavailable,
		                                 coreapi::ErrorCode::TooManyConversions,
		                                 "the case refuses"));
	}
	else if (mode == "length")
	{
		if (carry::sendHead(j, 5))
			sendText(j, "hello");
	}
	else if (mode == "nolength")
	{
		if (carry::sendHead(j, -1))
			sendText(j, "hello");
	}
	else if (mode == "broken-head")
	{
		// a header the writer breaks off: nothing may follow it
		if (!carry::sendHead(j, -1))
			p.refused_after = carry::refuse(j, problemResponse(StatusServiceUnavailable,
			                                                   coreapi::ErrorCode::TooManyStreams,
			                                                   "a second answer"));
	}
	else if (mode == "trickle")
	{
		if (carry::sendHead(j, -1) && sendText(j, "abc"))
		{
			while (!p.release.load() && carry::waitPeer(j, 20))
			{
			}
			sendText(j, "def");
		}
	}
	else if (mode == "idle")
	{
		if (carry::sendHead(j, -1))
		{
			while (carry::waitPeer(j, 50))
			{
			}
		}
	}
	else if (mode == "flood")
	{
		if (carry::sendHead(j, -1))
		{
			const std::string block(64 * 1024, 'x');
			for (int i = 0; i < 4096; ++i)
				if (!carry::sendBody(j, block.data(), block.size()))
					break;
		}
	}
	else if (mode == "throw-before")
	{
		throw std::runtime_error("the case throws before the header");
	}
	else if (mode == "throw-after")
	{
		if (carry::sendHead(j, -1))
			throw std::runtime_error("the case throws after the header");
	}
	else if (mode == "block")
	{
		// a carrier stuck where no cancellation reaches it, a name lookup say
		for (int waited = 0; waited < 10000 && !p.release.load(); waited += 5)
			napMs(5);
		p.saw_cancelled = carry::cancelled(j);
		if (carry::sendHead(j, -1))
			sendText(j, "stray");
	}
	else if (mode == "late")
	{
		for (int waited = 0; waited < 5000 && (p.served == NULL || p.served->load() < 1); waited += 5)
			napMs(5);
		if (carry::sendHead(j, -1))
			sendText(j, "late");
	}
	else if (mode == "child-wait" || mode == "child-reap")
	{
		const pid_t pid = spawnWaiter();
		p.child = (int) pid;
		const bool entered = carry::adoptChild(j, pid);
		p.adopted = entered ? 1 : 0;
		if (entered)
		{
			if (mode == "child-wait")
			{
				while (carry::waitPeer(j, 50))
				{
				}
			}
			p.reaped = carry::endChild(j);
		}
	}
	else if (mode == "group-reap")
	{
		pid_t descendant = 0;
		const pid_t pid = spawnGroup(&descendant);
		p.child = (int) pid;
		p.descendant = (int) descendant;
		const bool entered = carry::adoptChild(j, pid);
		p.adopted = entered ? 1 : 0;
		if (entered)
			p.reaped = carry::endChild(j);
	}
	++p.finished;
}

const carry::Carrier kCaseCarrier = { NULL, &runCase };

/* The case's carriers for both kinds, and every seam back to the product's on
   the way out, so a case that fails halfway leaves nothing behind. */
struct Carriers
{
	size_t held_before;

	explicit Carriers(const carry::Carrier *fetch = &kCaseCarrier,
	                  const carry::Carrier *program = &kCaseCarrier)
		: held_before(carry::heldAnswersForTest())
	{
		probe().reset();
		carry::setCarrier(carry::Fetch, fetch);
		carry::setCarrier(carry::Program, program);
	}

	~Carriers()
	{
		probe().release = true;
		carry::waitIdle(5000);
		carry::setCarrier(carry::Fetch, NULL);
		carry::setCarrier(carry::Program, NULL);
		carry::setPointHookForTest(NULL);
		carry::setWriterForTest(NULL);
		carry::setKillerForTest(NULL);
		carry::setCloseProbeForTest(NULL);
		carry::setSendTimeoutMsForTest(0);
		carry::setStopBoundMsForTest(0);
		carry::failThreadStartForTest(false);
		carry::failJobAllocationForTest(false);
		carry::endStop();
	}
};

Response fetchAnswer(const char *mode)
{
	Response r;
	r.code = StatusOk;
	r.content_type = "video/mp2t";
	r.relay_url = std::string("test:") + mode;
	addApiHeaders(r);
	return r;
}

Response programAnswer(const char *mode)
{
	Response r;
	r.code = StatusOk;
	r.content_type = "audio/aac";
	r.stream_argv.push_back(std::string("test:") + mode);
	addApiHeaders(r);
	return r;
}

/* A hook ahead of the bridge that answers a few paths with what the case set,
   through the bridge's own answerWith(): to the transport, a route that
   answers with a program or an address. */
struct Front : public Cyhook
{
	pthread_mutex_t lock;
	std::map<std::string, Response> answers;

	Front() { pthread_mutex_init(&lock, NULL); }
	~Front() { pthread_mutex_destroy(&lock); }

	void set(const std::string &path, const Response &r)
	{
		pthread_mutex_lock(&lock);
		answers[path] = r;
		pthread_mutex_unlock(&lock);
	}

	virtual std::string getHookName(void) { return "case front"; }

	virtual THandleStatus Hook_SendResponse(CyhookHandler *hh)
	{
		const std::string url = hh->UrlData["url"];
		Response r;
		bool found = false;
		pthread_mutex_lock(&lock);
		std::map<std::string, Response>::const_iterator it = answers.find(url);
		if (it != answers.end())
		{
			r = it->second;
			found = true;
		}
		pthread_mutex_unlock(&lock);
		if (!found)
			return HANDLED_NONE;
		CApiBridge::answerWith(hh, r);
		return hh->status;
	}

private:
	Front(const Front &);
	Front &operator=(const Front &);
};

std::string request(const char *method, const char *path)
{
	return std::string(method) + " " + path + " HTTP/1.0\r\nHost: box\r\n\r\n";
}

// Everything a carried answer closes, by number.
std::vector<int> &closed()
{
	static std::vector<int> v;
	return v;
}

pthread_mutex_t closed_lock = PTHREAD_MUTEX_INITIALIZER;

void noteClose(int fd)
{
	pthread_mutex_lock(&closed_lock);
	closed().push_back(fd);
	pthread_mutex_unlock(&closed_lock);
}

std::vector<int> closedSoFar()
{
	pthread_mutex_lock(&closed_lock);
	std::vector<int> v = closed();
	pthread_mutex_unlock(&closed_lock);
	return v;
}

void watchCloses()
{
	pthread_mutex_lock(&closed_lock);
	closed().clear();
	pthread_mutex_unlock(&closed_lock);
	carry::setCloseProbeForTest(&noteClose);
}

bool isClosed(int fd)
{
	return fcntl(fd, F_GETFD) == -1 && errno == EBADF;
}

/* Starts an answer straight through the transport on one end of a socket
   pair, as the stream pass does, and gives the end nhttpd would hold back
   at once; the case reads the other end. */
struct Pair
{
	int  near_end;
	int  far_end;
	carry::Outcome outcome;
	Response refusal;

	explicit Pair(const Response &answer)
		: near_end(-1), far_end(-1), outcome(carry::Refused)
	{
		int sp[2];
		REQUIRE(socketpair(AF_UNIX, SOCK_STREAM, 0, sp) == 0);
		near_end = sp[0];
		far_end = sp[1];
		outcome = carry::start(carry::hold(answer), near_end, Get, kCarriedPath, refusal);
		close(near_end);
		near_end = -1;
	}

	~Pair()
	{
		if (far_end >= 0)
			close(far_end);
	}

	// What arrives within the time, and whether the job closed after it.
	std::string read(int wait_ms, bool *eof = NULL)
	{
		std::string out;
		const int64_t until = wireNowMs() + wait_ms;
		if (eof != NULL)
			*eof = false;
		for (;;)
		{
			const int64_t left = until - wireNowMs();
			if (left <= 0)
				return out;
			struct pollfd p;
			p.fd = far_end;
			p.events = POLLIN;
			p.revents = 0;
			if (poll(&p, 1, (int) left) <= 0)
				return out;
			char buf[4096];
			const ssize_t n = ::read(far_end, buf, sizeof(buf));
			if (n <= 0)
			{
				if (eof != NULL)
					*eof = true;
				return out;
			}
			out.append(buf, (size_t) n);
		}
	}

private:
	Pair(const Pair &);
	Pair &operator=(const Pair &);
};

// Signals the case's killer passed on, in order.
struct Signalled
{
	pid_t pid;
	int   sig;
};

pthread_mutex_t kills_lock = PTHREAD_MUTEX_INITIALIZER;

std::vector<Signalled> &kills()
{
	static std::vector<Signalled> v;
	return v;
}

std::atomic<bool> swallow_child_signals(false);

int recordingKiller(pid_t pid, int sig)
{
	Signalled s;
	s.pid = pid;
	s.sig = sig;
	pthread_mutex_lock(&kills_lock);
	kills().push_back(s);
	pthread_mutex_unlock(&kills_lock);
	if (swallow_child_signals.load())
		return 0;
	return ::kill(pid, sig);
}

size_t killsOf(pid_t pid)
{
	size_t n = 0;
	pthread_mutex_lock(&kills_lock);
	for (size_t i = 0; i < kills().size(); ++i)
		if (kills()[i].pid == pid || kills()[i].pid == -pid)
			++n;
	pthread_mutex_unlock(&kills_lock);
	return n;
}

bool sentTo(pid_t target, int sig)
{
	bool found = false;
	pthread_mutex_lock(&kills_lock);
	for (size_t i = 0; i < kills().size(); ++i)
		if (kills()[i].pid == target && kills()[i].sig == sig)
			found = true;
	pthread_mutex_unlock(&kills_lock);
	return found;
}

void watchKills(bool swallow)
{
	pthread_mutex_lock(&kills_lock);
	kills().clear();
	pthread_mutex_unlock(&kills_lock);
	swallow_child_signals = swallow;
	carry::setKillerForTest(&recordingKiller);
}

// The child is gone and was reaped by the job, not left a zombie.
bool reapedByTheJob(pid_t pid)
{
	int status = 0;
	return waitpid(pid, &status, WNOHANG) == -1 && errno == ECHILD;
}

} // namespace

// ---------------------------------------------------------------- the wire

TEST_CASE("a refusal before the first byte is a document with its length, and it is noted", "[carry]")
{
	Carriers c;
	Front front;
	// a path a route has, so the note reaches a watch
	front.set("/api/v1/system/info", fetchAnswer("refuse"));

	struct Seen
	{
		static void watch(const Endpoint &ep, const Response &r)
		{
			if (r.code == StatusServiceUnavailable && r.body.find("the case refuses") != std::string::npos
					&& std::string(ep.path) == "/api/v1/system/info")
				++noted();
		}
		static std::atomic<int> &noted()
		{
			static std::atomic<int> n(0);
			return n;
		}
	};
	Seen::noted() = 0;
	setAnswerWatchForTest(&Seen::watch);

	{
		WireServer serving(4, 100, WireServer::Threaded, &front);
		WireStream s(serving.port, request("GET", "/api/v1/system/info").c_str());
		REQUIRE(s.ok());
		CHECK(s.code() == 503);
		CHECK(s.headerOf("Content-Type") == "application/problem+json");
		const std::string body = s.readToEof(2000);
		CHECK(body.find("too-many-conversions") != std::string::npos);
		CHECK(s.headerOf("Content-Length") == std::to_string(body.size()));
		CHECK(waitForRunning(0, 2000) == 0);
	}
	CHECK(Seen::noted() == 1);
	// the suite's own watch back
	watchAnswers();
}

TEST_CASE("the header states a length only where the carrier knows one", "[carry]")
{
	Carriers c;
	Front front;
	WireServer serving(4, 100, WireServer::Threaded, &front);

	front.set(kCarriedPath, fetchAnswer("length"));
	{
		WireStream s(serving.port, request("GET", kCarriedPath).c_str());
		REQUIRE(s.ok());
		CHECK(s.code() == 200);
		CHECK(s.headerOf("Content-Type") == "video/mp2t");
		CHECK(s.headerOf("Content-Length") == "5");
		CHECK(s.headerOf("Connection") == "close");
		CHECK(s.headerOf("Cache-Control") == "no-store");
		CHECK(s.readToEof(2000) == "hello");
	}
	front.set(kCarriedPath, fetchAnswer("nolength"));
	{
		WireStream s(serving.port, request("GET", kCarriedPath).c_str());
		REQUIRE(s.ok());
		CHECK(s.code() == 200);
		CHECK_FALSE(s.hasHeader("Content-Length"));
		CHECK(s.readToEof(2000) == "hello");
	}
	CHECK(waitForRunning(0, 2000) == 0);
}

TEST_CASE("bytes reach the far end as they are written", "[carry]")
{
	Carriers c;
	Front front;
	WireServer serving(4, 100, WireServer::Threaded, &front);
	front.set(kCarriedPath, fetchAnswer("trickle"));

	WireStream s(serving.port, request("GET", kCarriedPath).c_str());
	REQUIRE(s.ok());
	// the first three while the carrier still holds the rest back
	std::vector<std::string> first(1, "abc");
	CHECK(s.readUntilAll(first, 1000).find("abc") != std::string::npos);
	CHECK(s.body.find("def") == std::string::npos);
	probe().release = true;
	CHECK(s.readToEof(2000) == "abcdef");
	CHECK(waitForRunning(0, 2000) == 0);
}

TEST_CASE("a far end that leaves frees the seat within a second", "[carry]")
{
	Carriers c;
	Front front;
	WireServer serving(4, 100, WireServer::Threaded, &front);
	front.set(kCarriedPath, fetchAnswer("idle"));
	{
		WireStream s(serving.port, request("GET", kCarriedPath).c_str());
		REQUIRE(s.ok());
		REQUIRE(carry::running() == 1);
		REQUIRE(carry::seatsTaken(carry::Fetch) == 1);
	}
	CHECK(waitForRunning(0, 1000) == 0);
	CHECK(carry::seatsTaken(carry::Fetch) == 0);
}

TEST_CASE("a far end that stops reading ends the answer at the send limit", "[carry]")
{
	Carriers c;
	carry::setSendTimeoutMsForTest(200);
	Front front;
	WireServer serving(4, 100, WireServer::Threaded, &front);
	front.set(kCarriedPath, fetchAnswer("flood"));

	// reads the header and then nothing more
	WireStream s(serving.port, request("GET", kCarriedPath).c_str(), 4096);
	REQUIRE(s.ok());
	CHECK(waitForRunning(0, 10000) == 0);
	CHECK(probe().finished == 1);
}

TEST_CASE("a server stop ends every carried answer and every event stream", "[carry]")
{
	Carriers c;
	Front front;
	WireServer serving(4, 100, WireServer::Threaded, &front);
	front.set(kCarriedPath, fetchAnswer("idle"));

	WireStream a(serving.port, request("GET", kCarriedPath).c_str());
	WireStream b(serving.port, request("GET", kCarriedPath).c_str());
	WireStream events_open(serving.port);
	REQUIRE(a.ok());
	REQUIRE(b.ok());
	REQUIRE(events_open.ok());
	REQUIRE(carry::running() == 2);
	REQUIRE(events::openStreams() == 1);

	const int64_t asked = wireNowMs();
	CApiBridge::stopStreams();
	// both ended on the stop, well inside the bound
	CHECK(wireNowMs() - asked < (int64_t) carry::stopBoundMs());
	CHECK(carry::running() == 0);
	CHECK(events::openStreams() == 0);
	CHECK(a.closedWithin(2000));
	CHECK(b.closedWithin(2000));
	CHECK(events_open.closedWithin(2000));

	// closed to new ones until the server starts again
	{
		WireStream refused(serving.port, request("GET", kCarriedPath).c_str());
		REQUIRE(refused.ok());
		CHECK(refused.code() == 503);
		CHECK(refused.readToEof(2000).find("server-stopping") != std::string::npos);
	}
	CApiBridge::startStreams();
	{
		WireStream again(serving.port, request("GET", kCarriedPath).c_str());
		REQUIRE(again.ok());
		CHECK(again.code() == 200);
	}
	CHECK(waitForRunning(0, 2000) == 0);
}

TEST_CASE("a server stop waits for a stuck carried answer only up to its bound", "[carry]")
{
	Carriers c;
	CHECK(carry::stopBoundMs() == 5000);
	carry::setStopBoundMsForTest(300);

	Pair old_job(fetchAnswer("block"));
	REQUIRE(old_job.outcome == carry::Carrying);
	REQUIRE(waitForCount(probe().started, 1, 2000));

	const int64_t asked = wireNowMs();
	CApiBridge::stopStreams();
	const int64_t took = wireNowMs() - asked;
	CHECK(took >= 250);
	CHECK(took < 3000);
	CHECK(carry::running() == 1);

	// the server comes back while the stuck one still holds its seat
	CApiBridge::startStreams();
	Pair new_job(fetchAnswer("idle"));
	REQUIRE(new_job.outcome == carry::Carrying);
	CHECK(new_job.read(1000).find(" 200 ") != std::string::npos);
	CHECK(carry::seatsTaken(carry::Fetch) == 2);

	// it ends late, still cancelled, and what it writes then goes nowhere
	probe().release = true;
	REQUIRE(waitForRunning(1, 2000) == 1);
	CHECK(probe().finished == 1);
	CHECK(probe().saw_cancelled.load());
	bool eof = false;
	CHECK(old_job.read(500, &eof).empty());
	CHECK(eof);
	CHECK(new_job.read(300).empty());

	close(new_job.far_end);
	new_job.far_end = -1;
	CHECK(waitForRunning(0, 2000) == 0);
	CHECK(carry::seatsTaken(carry::Fetch) == 0);
	// the event streams startStreams() brought up go down again
	CApiBridge::stopStreams();
}

TEST_CASE("a kind nothing carries is refused with 501, for a GET and a HEAD", "[carry]")
{
	Carriers c(NULL, NULL);
	Front front;
	WireServer serving(4, 100, WireServer::Threaded, &front);
	front.set(kCarriedPath, programAnswer("idle"));

	WireStream get(serving.port, request("GET", kCarriedPath).c_str());
	REQUIRE(get.ok());
	CHECK(get.code() == 501);
	const std::string body = get.readToEof(2000);
	CHECK(body.find("no-such-method") != std::string::npos);
	CHECK(get.headerOf("Content-Length") == std::to_string(body.size()));

	WireStream head(serving.port, request("HEAD", kCarriedPath).c_str());
	REQUIRE(head.ok());
	CHECK(head.code() == 501);
	CHECK(carry::running() == 0);
	CHECK(carry::heldAnswersForTest() == c.held_before);
}

TEST_CASE("the seats of each kind run out on their own, with their own wait", "[carry]")
{
	Carriers c;
	Front front;
	WireServer serving(4, 100, WireServer::Threaded, &front);

	std::vector<WireStream *> open;
	front.set(kCarriedPath, fetchAnswer("idle"));
	for (size_t i = 0; i < carry::seatsOf(carry::Fetch); ++i)
	{
		open.push_back(new WireStream(serving.port, request("GET", kCarriedPath).c_str()));
		REQUIRE(open.back()->ok());
		REQUIRE(open.back()->code() == 200);
	}
	CHECK(carry::seatsOf(carry::Fetch) == 6);
	{
		WireStream over(serving.port, request("GET", kCarriedPath).c_str());
		REQUIRE(over.ok());
		CHECK(over.code() == 503);
		CHECK(over.headerOf("Retry-After") == "5");
		CHECK(over.readToEof(2000).find("too-many-streams") != std::string::npos);
	}

	// the programs have seats of their own, still free
	front.set(kCarriedPath, programAnswer("idle"));
	for (size_t i = 0; i < carry::seatsOf(carry::Program); ++i)
	{
		open.push_back(new WireStream(serving.port, request("GET", kCarriedPath).c_str()));
		REQUIRE(open.back()->ok());
		REQUIRE(open.back()->code() == 200);
	}
	CHECK(carry::seatsOf(carry::Program) == 2);
	{
		WireStream over(serving.port, request("GET", kCarriedPath).c_str());
		REQUIRE(over.ok());
		CHECK(over.code() == 503);
		CHECK(over.headerOf("Retry-After") == "15");
		CHECK(over.readToEof(2000).find("too-many-conversions") != std::string::npos);
	}
	// and a HEAD says what the GET would meet, taking nothing
	{
		WireStream head(serving.port, request("HEAD", kCarriedPath).c_str());
		REQUIRE(head.ok());
		CHECK(head.code() == 503);
	}

	for (size_t i = 0; i < open.size(); ++i)
		delete open[i];
	CHECK(waitForRunning(0, 2000) == 0);
	CHECK(carry::seatsTaken(carry::Fetch) == 0);
	CHECK(carry::seatsTaken(carry::Program) == 0);
}

TEST_CASE("a carrier that throws is answered before the header and cut off after it", "[carry]")
{
	Carriers c;
	Front front;
	WireServer serving(4, 100, WireServer::Threaded, &front);

	front.set(kCarriedPath, fetchAnswer("throw-before"));
	{
		WireStream s(serving.port, request("GET", kCarriedPath).c_str());
		REQUIRE(s.ok());
		CHECK(s.code() == 500);
		const std::string body = s.readToEof(2000);
		CHECK(body == fixedProblem());
		CHECK(s.headerOf("Content-Length") == std::to_string(body.size()));
	}
	front.set(kCarriedPath, fetchAnswer("throw-after"));
	{
		WireStream s(serving.port, request("GET", kCarriedPath).c_str());
		REQUIRE(s.ok());
		CHECK(s.code() == 200);
		CHECK(s.readToEof(2000).empty());
		CHECK(s.eof);
	}
	CHECK(waitForRunning(0, 2000) == 0);
	CHECK(carry::seatsTaken(carry::Fetch) == 0);
}

TEST_CASE("without connection threads the next request is answered while an answer is carried", "[carry]")
{
	Carriers c;
	Front front;
	WireServer serving(4, 100, WireServer::Serial, &front);
	front.set(kCarriedPath, fetchAnswer("idle"));
	Response plain;
	plain.code = StatusOk;
	plain.content_type = "text/plain";
	plain.body = "ok";
	front.set(kPlainPath, plain);

	WireStream carried(serving.port, request("GET", kCarriedPath).c_str());
	REQUIRE(carried.ok());
	REQUIRE(carry::running() == 1);

	const int64_t asked = wireNowMs();
	WireStream next(serving.port, request("GET", kPlainPath).c_str());
	REQUIRE(next.ok());
	CHECK(next.code() == 200);
	CHECK(next.readToEof(2000) == "ok");
	CHECK(wireNowMs() - asked <= 500);
	CHECK(carry::running() == 1);
}

namespace
{

std::atomic<int> writer_calls(0);

// Takes ten bytes of the first write, and fails every write after it.
ssize_t tenThenFail(int fd, const char *bytes, size_t n)
{
	if (++writer_calls == 1)
		return ::send(fd, bytes, (n < 10) ? n : 10, MSG_NOSIGNAL);
	errno = EPIPE;
	return -1;
}

} // namespace

TEST_CASE("a header broken off halfway is followed by nothing", "[carry]")
{
	Carriers c;
	writer_calls = 0;
	carry::setWriterForTest(&tenThenFail);
	watchCloses();
	Front front;
	WireServer serving(4, 100, WireServer::Threaded, &front);
	front.set(kCarriedPath, fetchAnswer("broken-head"));

	WireStream s(serving.port, request("GET", kCarriedPath).c_str());
	CHECK_FALSE(s.ok());
	CHECK(s.eof);
	CHECK(s.body.size() == 10);
	REQUIRE(waitForRunning(0, 2000) == 0);
	// no second answer: refuse() said no, and nothing more reached the writer
	CHECK_FALSE(probe().refused_after.load());
	CHECK(writer_calls == 2);
	const std::vector<int> fds = closedSoFar();
	REQUIRE(fds.size() == 1);
	CHECK(isClosed(fds[0]));
	CHECK(carry::seatsTaken(carry::Fetch) == 0);
}

TEST_CASE("a far end that resets the connection ends the answer", "[carry]")
{
	Carriers c;
	Front front;
	WireServer serving(4, 100, WireServer::Threaded, &front);
	front.set(kCarriedPath, fetchAnswer("flood"));
	{
		WireStream s(serving.port, request("GET", kCarriedPath).c_str());
		REQUIRE(s.ok());
		struct linger reset;
		reset.l_onoff = 1;
		reset.l_linger = 0;
		REQUIRE(setsockopt(s.fd, SOL_SOCKET, SO_LINGER, &reset, sizeof(reset)) == 0);
	}
	CHECK(waitForRunning(0, 3000) == 0);
	CHECK(probe().finished == 1);
}

TEST_CASE("the answer goes on after nhttpd has closed its connection", "[carry]")
{
	Carriers c;
	Front front;
	WireServer serving(4, 100, WireServer::Threaded, &front);
	probe().served = &serving.served;
	front.set(kCarriedPath, fetchAnswer("late"));

	// the job writes only once nhttpd's connection thread has closed and
	// deleted its socket
	WireStream s(serving.port, request("GET", kCarriedPath).c_str());
	REQUIRE(s.ok());
	CHECK(serving.served >= 1u);
	CHECK(s.readToEof(2000) == "late");
	CHECK(waitForRunning(0, 2000) == 0);
}

TEST_CASE("a job whose thread cannot start is refused, and nothing of it stays", "[carry]")
{
	Carriers c;
	watchCloses();
	carry::failThreadStartForTest(true);
	Front front;
	WireServer serving(4, 100, WireServer::Threaded, &front);
	front.set(kCarriedPath, fetchAnswer("idle"));

	WireStream s(serving.port, request("GET", kCarriedPath).c_str());
	REQUIRE(s.ok());
	CHECK(s.code() == 503);
	CHECK(s.headerOf("Retry-After") == "5");
	CHECK(s.readToEof(2000).find("too-many-streams") != std::string::npos);
	CHECK(carry::running() == 0);
	CHECK(carry::seatsTaken(carry::Fetch) == 0);
	CHECK(probe().started == 0);
	// the answer given back once, the copy closed once
	CHECK(carry::heldAnswersForTest() == c.held_before);
	const std::vector<int> fds = closedSoFar();
	REQUIRE(fds.size() == 1);
	CHECK(isClosed(fds[0]));
}

// ------------------------------------------------- the protocol, point by point

namespace
{

void stopAt(carry::Point at, carry::Point p, carry::Job *)
{
	if (p == at)
		carry::beginStop();
}

void stopWhenAdmitted(carry::Point p, carry::Job *j) { stopAt(carry::Admitted, p, j); }
void stopWhenSpawned(carry::Point p, carry::Job *j) { stopAt(carry::ChildSpawned, p, j); }

} // namespace

TEST_CASE("a stop between admission and the thread: the job writes nothing and closes once", "[carry]")
{
	Carriers c;
	watchCloses();
	carry::setPointHookForTest(&stopWhenAdmitted);

	Pair pair(fetchAnswer("nolength"));
	REQUIRE(pair.outcome == carry::Carrying);
	bool eof = false;
	CHECK(pair.read(2000, &eof).empty());
	CHECK(eof);
	REQUIRE(waitForRunning(0, 2000) == 0);
	CHECK(probe().started == 0);
	CHECK(carry::seatsTaken(carry::Fetch) == 0);
	CHECK(carry::heldAnswersForTest() == c.held_before);
	const std::vector<int> fds = closedSoFar();
	REQUIRE(fds.size() == 1);
	CHECK(isClosed(fds[0]));
}

TEST_CASE("a refused admission and a failed allocation close the copy once", "[carry]")
{
	Carriers c;
	watchCloses();

	SECTION("refused: the server is stopping")
	{
		carry::beginStop();
		Pair pair(fetchAnswer("idle"));
		CHECK(pair.outcome == carry::Refused);
		CHECK(pair.refusal.code == 503);
		CHECK(pair.refusal.body.find("server-stopping") != std::string::npos);
		carry::endStop();
	}
	SECTION("refused: nothing carries the kind")
	{
		carry::setCarrier(carry::Fetch, NULL);
		Pair pair(fetchAnswer("idle"));
		CHECK(pair.outcome == carry::Refused);
		CHECK(pair.refusal.code == 501);
	}
	SECTION("the job cannot be allocated")
	{
		carry::failJobAllocationForTest(true);
		int sp[2];
		REQUIRE(socketpair(AF_UNIX, SOCK_STREAM, 0, sp) == 0);
		Response refusal;
		CHECK_THROWS_AS(carry::start(carry::hold(fetchAnswer("idle")), sp[0], Get, kCarriedPath, refusal),
		                std::bad_alloc);
		close(sp[0]);
		close(sp[1]);
	}
	const std::vector<int> fds = closedSoFar();
	REQUIRE(fds.size() == 1);
	CHECK(isClosed(fds[0]));
	CHECK(carry::running() == 0);
	CHECK(carry::seatsTaken(carry::Fetch) == 0);
	CHECK(carry::heldAnswersForTest() == c.held_before);
}

TEST_CASE("a job a stop ended stays ended when the server starts again", "[carry]")
{
	Carriers c;
	Pair old_job(fetchAnswer("block"));
	REQUIRE(old_job.outcome == carry::Carrying);
	REQUIRE(waitForCount(probe().started, 1, 2000));

	carry::beginStop();
	carry::endStop();

	Pair new_job(fetchAnswer("idle"));
	REQUIRE(new_job.outcome == carry::Carrying);
	CHECK(new_job.read(1000).find(" 200 ") != std::string::npos);
	CHECK(carry::running() == 2);

	probe().release = true;
	// the old job's own end, not just its carrier's: the count drops last
	REQUIRE(waitForRunning(1, 2000) == 1);
	CHECK(probe().finished == 1);
	CHECK(probe().saw_cancelled.load());
	// what the old job tried to write after the stop went nowhere
	bool eof = false;
	CHECK(old_job.read(500, &eof).empty());
	CHECK(eof);

	close(new_job.far_end);
	new_job.far_end = -1;
	CHECK(waitForRunning(0, 2000) == 0);
	CHECK(carry::seatsTaken(carry::Fetch) == 0);
}

namespace
{

/* Holds a job's end right after its descriptor is closed, until the case
   lets it go on: the moment a number the job held can already be handed to
   somebody else. */
struct Barrier
{
	std::atomic<bool> held;
	std::atomic<bool> go_on;
	std::atomic<bool> passed;
};

Barrier &barrier()
{
	static Barrier b;
	return b;
}

void holdAfterClose(carry::Point p, carry::Job *)
{
	if (p != carry::JobEnded || barrier().passed.exchange(true))
		return;
	barrier().held = true;
	for (int waited = 0; waited < 5000 && !barrier().go_on.load(); waited += 2)
		napMs(2);
}

} // namespace

TEST_CASE("a descriptor number handed out again after a job is not shut down by a stop", "[carry]")
{
	Carriers c;
	watchCloses();
	barrier().held = false;
	barrier().go_on = false;
	barrier().passed = false;
	carry::setPointHookForTest(&holdAfterClose);

	Pair done(fetchAnswer("length"));
	REQUIRE(done.outcome == carry::Carrying);
	CHECK(done.read(2000).find("hello") != std::string::npos);

	// the job has closed its copy and has not finished ending
	REQUIRE(waitFor(barrier().held, 3000));
	CHECK(carry::running() == 1);
	const std::vector<int> fds = closedSoFar();
	REQUIRE(fds.size() == 1);
	const int reused = fds[0];
	REQUIRE(isClosed(reused));

	// the number it closed, on a new connection, at once: whichever end the
	// kernel handed it to, or the first end moved onto it
	int sp[2];
	REQUIRE(socketpair(AF_UNIX, SOCK_STREAM, 0, sp) == 0);
	if (sp[1] == reused)
		std::swap(sp[0], sp[1]);
	if (sp[0] != reused)
	{
		REQUIRE(dup2(sp[0], reused) == reused);
		close(sp[0]);
		sp[0] = reused;
	}

	// a stop now must not find that number among the jobs
	carry::beginStop();
	CHECK(::send(sp[0], "still", 5, MSG_NOSIGNAL) == 5);
	char buf[8] = { 0 };
	CHECK(::read(sp[1], buf, sizeof(buf)) == 5);
	CHECK(std::string(buf, 5) == "still");

	barrier().go_on = true;
	CHECK(waitForRunning(0, 2000) == 0);
	carry::endStop();
	close(sp[0]);
	close(sp[1]);
}

TEST_CASE("a stop before a child is entered: the child is ended and reaped", "[carry]")
{
	Carriers c;
	carry::setPointHookForTest(&stopWhenSpawned);

	Pair pair(programAnswer("child-wait"));
	REQUIRE(pair.outcome == carry::Carrying);
	REQUIRE(waitForRunning(0, 3000) == 0);
	CHECK(probe().adopted == 0);
	const pid_t pid = (pid_t) probe().child.load();
	REQUIRE(pid > 0);
	CHECK(reapedByTheJob(pid));
}

TEST_CASE("a stop with a child entered: its group gets SIGTERM and the job reaps it", "[carry]")
{
	Carriers c;
	watchKills(false);

	Pair pair(programAnswer("child-wait"));
	REQUIRE(pair.outcome == carry::Carrying);
	for (int waited = 0; waited < 2000 && probe().adopted != 1; waited += 5)
		napMs(5);
	REQUIRE(probe().adopted == 1);
	const pid_t pid = (pid_t) probe().child.load();

	carry::beginStop();
	CHECK(sentTo(-pid, SIGTERM));
	REQUIRE(waitForRunning(0, 3000) == 0);
	const int status = probe().reaped.load();
	CHECK(WIFSIGNALED(status));
	CHECK(WTERMSIG(status) == SIGTERM);
	CHECK(reapedByTheJob(pid));
}

namespace
{

std::atomic<bool>      reaping(false);
std::atomic<pthread_t> reaper;
std::atomic<int>       interrupts(0);

void markReap(carry::Point p, carry::Job *)
{
	if (p != carry::BeforeFinalReap)
		return;
	reaper = pthread_self();
	reaping = true;
}

void countInterrupt(int)
{
	++interrupts;
}

std::atomic<size_t> sent_by_stop(0);

void stopWhileReaping(carry::Point p, carry::Job *)
{
	if (p != carry::ChildTakenOut)
		return;
	const pid_t pid = (pid_t) probe().child.load();
	const size_t before = killsOf(pid);
	carry::beginStop();
	sent_by_stop = killsOf(pid) - before;
	carry::endStop();
}

} // namespace

TEST_CASE("a reap a signal interrupts is repeated", "[carry]")
{
	Carriers c;
	// the child is spared every signal the job sends, so the last wait blocks
	watchKills(true);
	reaping = false;
	interrupts = 0;
	carry::setPointHookForTest(&markReap);

	struct sigaction interrupting, before;
	std::memset(&interrupting, 0, sizeof(interrupting));
	interrupting.sa_handler = &countInterrupt;
	sigemptyset(&interrupting.sa_mask);
	interrupting.sa_flags = 0; // no SA_RESTART: the wait comes back with EINTR
	REQUIRE(sigaction(SIGUSR1, &interrupting, &before) == 0);

	const size_t interrupted_before = carry::interruptedReapsForTest();
	Pair pair(programAnswer("child-reap"));
	REQUIRE(pair.outcome == carry::Carrying);
	REQUIRE(waitFor(reaping, 3000));
	const pid_t pid = (pid_t) probe().child.load();
	REQUIRE(pid > 0);

	// until a signal has landed inside the wait
	for (int i = 0; i < 100 && carry::interruptedReapsForTest() == interrupted_before; ++i)
	{
		napMs(20);
		pthread_kill(reaper.load(), SIGUSR1);
	}
	CHECK(carry::interruptedReapsForTest() > interrupted_before);
	CHECK(interrupts > 0);
	CHECK(carry::running() == 1);

	// now the child really goes, and the wait the signals interrupted gets it
	::kill(pid, SIGKILL);
	REQUIRE(waitForRunning(0, 3000) == 0);
	CHECK(WIFSIGNALED(probe().reaped.load()));
	CHECK(reapedByTheJob(pid));

	sigaction(SIGUSR1, &before, NULL);
}

TEST_CASE("ending a child ends its whole group, a member that ignores SIGTERM included", "[carry]")
{
	Carriers c;
	Pair pair(programAnswer("group-reap"));
	REQUIRE(pair.outcome == carry::Carrying);
	REQUIRE(waitForRunning(0, 3000) == 0);
	REQUIRE(probe().adopted == 1);
	const pid_t pid = (pid_t) probe().child.load();
	const pid_t descendant = (pid_t) probe().descendant.load();
	REQUIRE(pid > 0);
	REQUIRE(descendant > 0);

	// the child left on SIGTERM and was collected by the job
	const int status = probe().reaped.load();
	CHECK(WIFSIGNALED(status));
	CHECK(WTERMSIG(status) == SIGTERM);
	CHECK(reapedByTheJob(pid));
	// and what it left in its group did not outlive it
	CHECK(stopsRunning(descendant, 2000));
	CHECK(carry::seatsTaken(carry::Program) == 0);
	if (processRuns(descendant))
		::kill(descendant, SIGKILL);
}

TEST_CASE("a stop while a child is being reaped does not signal it", "[carry]")
{
	Carriers c;
	watchKills(false);
	sent_by_stop = 99;
	carry::setPointHookForTest(&stopWhileReaping);

	Pair pair(programAnswer("child-reap"));
	REQUIRE(pair.outcome == carry::Carrying);
	REQUIRE(waitForRunning(0, 3000) == 0);
	const pid_t pid = (pid_t) probe().child.load();
	REQUIRE(pid > 0);
	CHECK(sent_by_stop == 0u);
	// the job's own end did signal it, and reaped it
	CHECK(sentTo(-pid, SIGTERM));
	CHECK(reapedByTheJob(pid));
}
