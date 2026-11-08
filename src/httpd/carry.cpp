/*
 * carry.cpp - answers the bridge carries on threads of their own
 *
 * Copyright (C) 2026 Thilo Graf
 *
 * License: GPL-2.0-or-later
 */

#include "carry.h"

#include "router.h"
#include "status.h"

#include "coreapi/base/errors.h"

#include "yhook.h"

#include <system/set_threadname.h>

#include <atomic>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <memory>
#include <new>
#include <vector>

#include <fcntl.h>
#include <poll.h>
#include <pthread.h>
#include <signal.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

namespace httpd
{

namespace carry
{

struct Job
{
	uint64_t          id;
	Kind              kind;
	// The copy of the connection's descriptor. Closed only after the job has
	// left the register, so a stop that shuts down what it finds there never
	// reaches a number handed out again.
	int               fd;
	// A child the carrier entered, 0 for none. Written under the register's
	// lock only; a stop signals what it finds here.
	pid_t             child;
	// Set by a stop, never cleared; read on the job's thread at every wait.
	std::atomic<bool> stopped;
	Response         *answer;
	Method            method;
	std::string       path;
	Carrier           carrier;
	// The job's thread only: whether a byte of the answer was attempted. A
	// problem document fits only before that.
	bool              committed;
	/* Whether the job holds a place its carrier reserved and has not taken on
	   (Carrier::reserve). Set before the thread starts, then the thread's only. */
	bool              placed;

	Job()
		: id(0), kind(Fetch), fd(-1), child(0), stopped(false), answer(NULL),
		  method(UnknownMethod), committed(false), placed(false)
	{
		carrier.check = NULL;
		carrier.run = NULL;
		carrier.reserve = NULL;
		carrier.unreserve = NULL;
	}

private:
	Job(const Job &);
	Job &operator=(const Job &);
};

namespace
{

const size_t kKinds = 3;

/* Seats per kind: six fetches (a relay, a segment, a playlist and the one
   nested in it, for a few browsers at once) and two programs (each a running
   converter the box pays for in CPU). Outside nhttpd's connection threads:
   a job holds a connection thread only until it is handed over.

   The event streams are counted here but never limited by a seat. Their
   module holds them to its configured ceiling: its carrier reserves a place
   there before a job's thread is started and refuses with its own 503 where
   none is free, and the place goes back at the end of the stream's pump,
   before the job's last act. A limit here would come free later than that one
   and refuse a stream the module has room for. */
const size_t kUnlimited = SIZE_MAX;
const size_t kSeats[kKinds] = { 6, 2, kUnlimited };

/* What a kind is refused with when its seats are taken or its job cannot be
   started, and when the caller should come back: a fetch frees its seat
   within seconds, a conversion lasts as long as somebody listens. An event
   stream meets this only when its thread could not be started, and is told
   the half minute its module tells a stream it has no place for. */
struct NoRoom
{
	coreapi::ErrorCode code;
	const char        *detail;
	const char        *retry_after;
};
const NoRoom kNoRoom[kKinds] = {
	{ coreapi::ErrorCode::TooManyStreams,
	  "the box is already relaying as many streams as it will", "5" },
	{ coreapi::ErrorCode::TooManyConversions,
	  "the box is already converting as many streams as it will", "15" },
	{ coreapi::ErrorCode::TooManyStreams,
	  "the box is already carrying as many event streams as it can", "30" },
};

// What each kind's jobs are called, as the process lists its threads.
const char *const kThreadName[kKinds] = { "httpd:carry", "httpd:carry", "httpd:events" };

/* Each job's stack, of which only the pages it touches are memory. Eight
   fetches and programs at most, so 4 MiB of address space, and one more per
   open event stream, up to the ceiling of the module that keeps them. */
const size_t kJobStack = 512 * 1024;

const unsigned kSendTimeoutDefaultMs = 30000;
const unsigned kStopBoundDefaultMs = 5000;

/* The register. On the heap and never freed: a job that outlives the stop
   that cancelled it (a fetch stuck in a name lookup, say) still leaves it at
   its end, after anything static may have been destroyed. */
struct Register
{
	pthread_mutex_t     lock;
	std::vector<Job *>  jobs;
	size_t              seats[kKinds];
	// jobs whose thread has not finished with them, the register's included
	size_t              live;
	bool                admitting;
	const Carrier      *carriers[kKinds];
	uint64_t            next_id;

	Register() : live(0), admitting(true), next_id(1)
	{
		pthread_mutex_init(&lock, NULL);
		for (size_t i = 0; i < kKinds; ++i)
		{
			seats[i] = 0;
			carriers[i] = NULL;
		}
	}
};

Register &reg()
{
	static Register *r = new Register;
	return *r;
}

struct Locked
{
	Locked() { pthread_mutex_lock(&reg().lock); }
	~Locked() { pthread_mutex_unlock(&reg().lock); }
private:
	Locked(const Locked &);
	Locked &operator=(const Locked &);
};

// Seams; nothing in the product sets them.
std::atomic<PointHook>  point_hook_(NULL);
std::atomic<Writer>     writer_(NULL);
std::atomic<Killer>     killer_(NULL);
std::atomic<CloseProbe> close_probe_(NULL);
std::atomic<unsigned>   send_timeout_ms_(kSendTimeoutDefaultMs);
std::atomic<unsigned>   stop_bound_ms_(kStopBoundDefaultMs);
std::atomic<bool>       fail_thread_start_(false);
std::atomic<bool>       fail_job_allocation_(false);
std::atomic<size_t>     held_answers_(0);
std::atomic<size_t>     interrupted_reaps_(0);

void point(Point p, Job *j)
{
	PointHook h = point_hook_.load();
	if (h != NULL)
		h(p, j);
}

int64_t monotonicMs()
{
	struct timespec t;
	if (clock_gettime(CLOCK_MONOTONIC, &t) != 0)
		return 0;
	return (int64_t) t.tv_sec * 1000 + t.tv_nsec / 1000000;
}

void sleepMs(unsigned ms)
{
	struct timespec t;
	t.tv_sec = (time_t) (ms / 1000);
	t.tv_nsec = (long) (ms % 1000) * 1000 * 1000;
	nanosleep(&t, NULL);
}

// Every descriptor this module owns is closed here and nowhere else.
void closeOwned(int fd)
{
	if (fd < 0)
		return;
	CloseProbe p = close_probe_.load();
	if (p != NULL)
		p(fd);
	::close(fd);
}

/* The copy of a connection's descriptor between the dup and the hand over to
   its job: closed on every way out that does not reach release(). */
struct OwnedFd
{
	int fd;
	explicit OwnedFd(int f) : fd(f) {}
	~OwnedFd() { closeOwned(fd); }
	int release()
	{
		const int f = fd;
		fd = -1;
		return f;
	}
private:
	OwnedFd(const OwnedFd &);
	OwnedFd &operator=(const OwnedFd &);
};

// The answer between nhttpd's handler and its job, released on every way
// out that does not hand it on.
struct HeldAnswer
{
	Response *a;
	explicit HeldAnswer(Response *p) : a(p) {}
	~HeldAnswer() { release(a); }
	Response *take()
	{
		Response *p = a;
		a = NULL;
		return p;
	}
private:
	HeldAnswer(const HeldAnswer &);
	HeldAnswer &operator=(const HeldAnswer &);
};

int signalProcess(pid_t pid, int sig)
{
	Killer k = killer_.load();
	return (k != NULL) ? k(pid, sig) : ::kill(pid, sig);
}

/* The child's process group, which is everything it started as well; the
   child itself where it leads no group of its own. Only ever called for a
   child that has not been reaped, so the id is still that child's. */
void signalChild(pid_t pid, int sig)
{
	if (pid <= 0)
		return;
	if (signalProcess(-pid, sig) != 0 && errno == ESRCH)
		signalProcess(pid, sig);
}

/* Whether the child has exited, looked at without collecting it: 1 exited,
   0 still running, -1 not a child of this process (any more). */
int peekChild(pid_t pid)
{
	for (;;)
	{
		siginfo_t info;
		std::memset(&info, 0, sizeof(info));
		if (waitid(P_PID, (id_t) pid, &info, WEXITED | WNOHANG | WNOWAIT) == 0)
			return (info.si_pid == pid) ? 1 : 0;
		if (errno != EINTR)
			return -1;
	}
}

/* Ends a child that is no longer in the register, everything in its group
   with it, and reaps it.

   The child is collected last. Until it is, its process id and the id of
   its group stay reserved, so the group signals cannot reach a process that
   is not the child's; and the group outlives the child: a member it started
   that ignores SIGTERM is still running when the child has left. So:
   SIGTERM to the group, up to two hundred milliseconds for the child to
   leave, SIGKILL to the group whatever the child did, then a wait for the
   child for as long as it takes, which a signal to this thread interrupts
   and does not end. */
int reap(Job *j, pid_t pid)
{
	if (peekChild(pid) < 0)
		return -1;
	signalChild(pid, SIGTERM);
	for (int waited = 0; waited < 200; waited += 10)
	{
		const int state = peekChild(pid);
		if (state < 0)
			return -1;
		if (state > 0)
			break;
		sleepMs(10);
	}
	signalChild(pid, SIGKILL);
	point(BeforeFinalReap, j);
	int status = 0;
	for (;;)
	{
		const pid_t got = waitpid(pid, &status, 0);
		if (got == pid)
			return status;
		if (got < 0 && errno == EINTR)
		{
			++interrupted_reaps_;
			continue;
		}
		return -1;
	}
}

enum Reason
{
	Admit,
	NoCarrier,
	Stopping,
	SeatsTaken,
	NoResources
};

/* A refusal of the transport's own, built with no lock held: it allocates
   and formats, and every job and a stop wait on that lock. */
Response refusalFor(Reason why, Kind k)
{
	switch (why)
	{
		case NoCarrier:
			return problemResponse(StatusNotImplemented, coreapi::ErrorCode::NoSuchMethod,
			                       "this server does not carry an answer of this kind");
		case Stopping:
			return problemResponse(StatusServiceUnavailable, coreapi::ErrorCode::ServerStopping,
			                       stoppingDetail());
		case Admit:
		case SeatsTaken:
		case NoResources:
			break;
	}
	Response r = problemResponse(StatusServiceUnavailable, kNoRoom[k].code, kNoRoom[k].detail);
	r.headers.push_back(std::make_pair(std::string("Retry-After"),
	                                   std::string(kNoRoom[k].retry_after)));
	return r;
}

// Whether the door admits an answer of this kind now, and its carrier.
Reason admission(Kind k, Carrier *carrier)
{
	Locked held;
	const Carrier *c = reg().carriers[k];
	if (c == NULL)
		return NoCarrier;
	if (!reg().admitting)
		return Stopping;
	if (reg().seats[k] >= kSeats[k])
		return SeatsTaken;
	if (carrier != NULL)
		*carrier = *c;
	return Admit;
}

Job *newJob()
{
	if (fail_job_allocation_.exchange(false))
		throw std::bad_alloc();
	return new Job;
}

// Out of the register and its seat given back; the caller holds the lock.
void unregister(Job *j)
{
	std::vector<Job *> &jobs = reg().jobs;
	for (size_t i = 0; i < jobs.size(); ++i)
	{
		if (jobs[i] != j)
			continue;
		jobs.erase(jobs.begin() + (std::ptrdiff_t) i);
		--reg().seats[j->kind];
		return;
	}
}

// The descriptor, the answer and the job, in that order and each once.
void destroy(Job *j)
{
	const int fd = j->fd;
	j->fd = -1;
	closeOwned(fd);
	point(JobEnded, j);
	release(j->answer);
	j->answer = NULL;
	delete j;
}

/* Everything the job's own thread writes, header and refusal included:
   whole, or false. A write waits for room at most the send limit, so a far
   end that stops reading ends the job rather than holding its seat; a stop
   wakes the wait by shutting the descriptor down. */
bool writeAll(Job &j, const char *bytes, size_t n)
{
	while (n > 0)
	{
		if (j.stopped.load())
			return false;
		struct pollfd p;
		p.fd = j.fd;
		p.events = POLLOUT;
		p.revents = 0;
		const int ready = poll(&p, 1, (int) send_timeout_ms_.load());
		if (ready < 0)
		{
			if (errno == EINTR)
				continue;
			return false;
		}
		if (ready == 0 || (p.revents & POLLOUT) == 0)
			return false;

		Writer w = writer_.load();
		const ssize_t sent = (w != NULL) ? w(j.fd, bytes, n)
		                                 : ::send(j.fd, bytes, n, MSG_NOSIGNAL | MSG_DONTWAIT);
		if (sent < 0)
		{
			if (errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK)
				continue;
			return false;
		}
		if (sent == 0)
			return false;
		bytes += sent;
		n -= (size_t) sent;
	}
	return true;
}

/* The header of an answer, built by nhttpd's own writer on a handler of the
   job's: the connection's handler belongs to a thread that has moved on. */
std::string headerOf(const Response &r, int64_t length, bool stream)
{
	CyhookHandler hh;
	hh.api_mode = true;
	hh.httpStatus = (HttpResponseType) r.code;
	hh.ResponseMimeType = r.content_type;
	hh.ResponseHeaders = r.headers;
	if (stream)
	{
		hh.status = HANDLED_STREAM;
		hh.StreamLength = (length >= 0) ? (off_t) length : (off_t) -1;
	}
	else
	{
		hh.status = HANDLED_READY;
		hh.yresult = r.body;
	}
	return hh.BuildHeader();
}

/* A throw out of a carrier: before the first write the caller still gets an
   answer, the fixed document, which needs no formatting; after it, nothing
   may follow what was sent and the close says the rest. */
void failInJob(Job &j)
{
	try
	{
		if (j.committed || j.stopped.load())
			return;
		Response p;
		p.code = StatusInternalServerError;
		p.content_type = problemContentType();
		p.body = fixedProblem();
		refuse(j, p);
	}
	catch (...)
	{
	}
}

/* The end of every job, on its own thread: the child reaped, the job out of
   the register and its seat free, then the descriptor closed, then the job
   and its answer freed. Counted out last, so a wait for idle that returns has
   nothing of a job left running. */
void endJob(Job *j)
{
	pid_t child = 0;
	{
		Locked held;
		child = j->child;
	}
	if (child > 0)
		endChild(*j);
	{
		Locked held;
		unregister(j);
	}
	destroy(j);
	{
		Locked held;
		--reg().live;
	}
}

// The place the job's carrier reserved, given back where the job still holds it.
void giveBackPlace(Job &j)
{
	if (!j.placed)
		return;
	j.placed = false;
	try
	{
		if (j.carrier.unreserve != NULL)
			j.carrier.unreserve();
	}
	catch (...)
	{
	}
}

void *jobMain(void *arg)
{
	Job *j = (Job *) arg;
	set_threadname(kThreadName[j->kind]);
	point(JobStarted, j);
	/* Nothing leaves this frame: the thread is nobody's to catch for. A job a
	   stop reached before it ran writes nothing at all. */
	if (!j->stopped.load())
	{
		try
		{
			j->carrier.run(*j);
		}
		catch (...)
		{
			failInJob(*j);
		}
	}
	// a place the carrier did not take on: a job stopped before it ran, or one that failed first
	giveBackPlace(*j);
	endJob(j);
	return NULL;
}

bool spawn(Job *j)
{
	if (fail_thread_start_.exchange(false))
		return false;
	pthread_attr_t attr;
	if (pthread_attr_init(&attr) != 0)
		return false;
	pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
	pthread_attr_setstacksize(&attr, kJobStack);
	pthread_t t;
	const int rc = pthread_create(&t, &attr, &jobMain, j);
	pthread_attr_destroy(&attr);
	return rc == 0;
}

} // namespace

void setCarrier(Kind k, const Carrier *c)
{
	Locked held;
	reg().carriers[k] = c;
}

bool carried(const Response &r, Kind *kind)
{
	if (r.code != StatusOk)
		return false;
	Kind k;
	if (!r.stream_argv.empty())
		k = Program;
	else if (!r.relay_url.empty())
		k = Fetch;
	else if (r.event_stream)
		k = Events;
	else
		return false;
	if (kind != NULL)
		*kind = k;
	return true;
}

bool hasCarrier(Kind k)
{
	Locked held;
	return reg().carriers[k] != NULL;
}

bool admissible(const Response &answer, Response &refusal)
{
	Kind k = Fetch;
	if (!carried(answer, &k))
		return true;
	Carrier c;
	c.check = NULL;
	c.run = NULL;
	const Reason why = admission(k, &c);
	if (why != Admit)
	{
		refusal = refusalFor(why, k);
		return false;
	}
	if (c.check != NULL && !c.check(answer, refusal))
		return false;
	return true;
}

void *hold(const Response &answer)
{
	Response *p = new Response(answer);
	++held_answers_;
	return p;
}

void release(void *held)
{
	if (held == NULL)
		return;
	delete (Response *) held;
	--held_answers_;
}

Outcome start(void *held, int fd, Method m, const std::string &path, Response &refusal)
{
	HeldAnswer answer((Response *) held);
	Kind k = Fetch;
	if (answer.a == NULL || !carried(*answer.a, &k))
	{
		refusal = problemResponse(StatusInternalServerError, coreapi::ErrorCode::BoxUnreadable,
		                          "the stream could not be opened");
		return Refused;
	}

	// The copy is taken outside the lock and is owned from the first
	// moment, so every refusal and every throw below closes it once.
	OwnedFd copy(fcntl(fd, F_DUPFD_CLOEXEC, 3));
	if (copy.fd < 0)
	{
		refusal = refusalFor(NoResources, k);
		return Refused;
	}

	Reason why = Admit;
	Job *job = NULL;
	{
		Locked hold_register;
		const Carrier *c = reg().carriers[k];
		if (c == NULL)
			why = NoCarrier;
		else if (!reg().admitting)
			why = Stopping;
		else if (reg().seats[k] >= kSeats[k])
			why = SeatsTaken;
		else
		{
			std::unique_ptr<Job> made(newJob());
			made->id = reg().next_id++;
			made->kind = k;
			made->method = m;
			made->path = path;
			made->carrier = *c;
			reg().jobs.push_back(made.get());
			job = made.release();
			++reg().seats[k];
			++reg().live;
			// registered whole: only now are the descriptor and the answer its
			job->fd = copy.release();
			job->answer = answer.take();
		}
	}
	if (job == NULL)
	{
		refusal = refusalFor(why, k);
		return Refused;
	}

	point(Admitted, job);

	/* A place with the module of a kind it holds to a ceiling of its own, before a
	   thread is started for it: none free, and no thread is started at all. */
	Response no_place;
	bool placed = true;
	if (job->carrier.reserve != NULL)
	{
		try
		{
			placed = job->carrier.reserve(no_place);
		}
		catch (...)
		{
			placed = false;
			no_place = refusalFor(NoResources, k);
		}
		job->placed = placed;
	}

	// Published. A thread that cannot be started takes everything back.
	if (!placed || !spawn(job))
	{
		giveBackPlace(*job);
		{
			Locked hold_register;
			unregister(job);
			--reg().live;
		}
		destroy(job);
		refusal = placed ? refusalFor(NoResources, k) : no_place;
		return Refused;
	}
	return Carrying;
}

Response &answerOf(Job &j)
{
	return *j.answer;
}

Method methodOf(const Job &j)
{
	return j.method;
}

int descriptorOf(const Job &j)
{
	return j.fd;
}

bool cancelled(const Job &j)
{
	return j.stopped.load();
}

bool holdsPlace(const Job &j)
{
	return j.placed;
}

void placeTaken(Job &j)
{
	j.placed = false;
}

bool waitPeer(Job &j, unsigned ms)
{
	const int64_t until = monotonicMs() + ms;
	for (;;)
	{
		if (j.stopped.load())
			return false;
		int64_t left = until - monotonicMs();
		if (left < 0)
			left = 0;
		struct pollfd p;
		p.fd = j.fd;
		/* The half close and not POLLIN: a peer that sends more after its
		   request would otherwise wake this at once for as long as it is
		   there. Hang-up and errors are reported unasked. */
		p.events = POLLRDHUP;
		p.revents = 0;
		const int ready = poll(&p, 1, (int) left);
		if (ready < 0)
		{
			if (errno == EINTR)
				continue;
			return false;
		}
		if (ready > 0 && (p.revents & (POLLRDHUP | POLLHUP | POLLERR | POLLNVAL)) != 0)
			return false;
		if (left == 0)
			return true;
	}
}

bool sendHead(Job &j, int64_t length)
{
	if (j.committed || j.stopped.load())
		return false;
	const std::string header = headerOf(*j.answer, length, true);
	// committed before the first attempt: a header broken off halfway is
	// never followed by a second answer
	j.committed = true;
	return writeAll(j, header.data(), header.size());
}

bool sendBody(Job &j, const char *bytes, size_t n)
{
	if (!j.committed)
		return false;
	return writeAll(j, bytes, n);
}

bool refuse(Job &j, const Response &problem)
{
	if (j.committed || j.stopped.load())
		return false;
	Response p(problem);
	addApiHeaders(p);
	std::string out = headerOf(p, -1, false);
	if (j.method != Head)
		out += p.body;
	noteAnswer(j.method, j.path, p);
	j.committed = true;
	return writeAll(j, out.data(), out.size());
}

bool adoptChild(Job &j, pid_t pid)
{
	if (pid <= 0)
		return false;
	point(ChildSpawned, &j);
	bool entered = false;
	{
		Locked held;
		if (!j.stopped.load())
		{
			j.child = pid;
			entered = true;
		}
	}
	if (!entered)
		reap(&j, pid);
	return entered;
}

int endChild(Job &j)
{
	pid_t pid = 0;
	{
		Locked held;
		pid = j.child;
		j.child = 0;
	}
	if (pid <= 0)
		return -1;
	point(ChildTakenOut, &j);
	return reap(&j, pid);
}

void beginStop()
{
	Locked held;
	reg().admitting = false;
	std::vector<Job *> &jobs = reg().jobs;
	for (size_t i = 0; i < jobs.size(); ++i)
	{
		Job *j = jobs[i];
		j->stopped = true;
		// valid: a job leaves the register before its descriptor is closed
		if (j->fd >= 0)
			::shutdown(j->fd, SHUT_RDWR);
		if (j->child > 0)
			signalChild(j->child, SIGTERM);
	}
}

bool waitIdle(unsigned ms)
{
	const int64_t until = monotonicMs() + ms;
	for (;;)
	{
		const size_t left = running();
		if (left == 0)
			return true;
		if (monotonicMs() >= until)
		{
			std::fprintf(stderr, "httpd: %u carried answer(s) still running after %u ms\n",
			             (unsigned) left, ms);
			return false;
		}
		sleepMs(10);
	}
}

void endStop()
{
	Locked held;
	reg().admitting = true;
}

size_t running()
{
	Locked held;
	return reg().live;
}

size_t seatsTaken(Kind k)
{
	Locked held;
	return reg().seats[k];
}

size_t seatsOf(Kind k)
{
	return kSeats[k];
}

unsigned sendTimeoutMs()
{
	return send_timeout_ms_.load();
}

const char *stoppingDetail()
{
	return "the server is stopping and starts no new stream";
}

unsigned stopBoundMs()
{
	return stop_bound_ms_.load();
}

void drainForTest()
{
	while (!waitIdle(5000))
	{
	}
}

void setPointHookForTest(PointHook h)
{
	point_hook_ = h;
}

void setWriterForTest(Writer w)
{
	writer_ = w;
}

void setKillerForTest(Killer k)
{
	killer_ = k;
}

void setCloseProbeForTest(CloseProbe p)
{
	close_probe_ = p;
}

void setSendTimeoutMsForTest(unsigned ms)
{
	send_timeout_ms_ = (ms == 0) ? kSendTimeoutDefaultMs : ms;
}

void setStopBoundMsForTest(unsigned ms)
{
	stop_bound_ms_ = (ms == 0) ? kStopBoundDefaultMs : ms;
}

void failThreadStartForTest(bool fail)
{
	fail_thread_start_ = fail;
}

void failJobAllocationForTest(bool fail)
{
	fail_job_allocation_ = fail;
}

size_t heldAnswersForTest()
{
	return held_answers_.load();
}

size_t interruptedReapsForTest()
{
	return interrupted_reaps_.load();
}

} // namespace carry

} // namespace httpd
