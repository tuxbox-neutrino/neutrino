/*
 * carry.h - answers the bridge carries on threads of their own
 *
 * Copyright (C) 2026 Thilo Graf
 *
 * License: GPL-2.0-or-later
 *
 * A route can answer with a program to run (Response::stream_argv), with
 * an address to relay (Response::relay_url) or with the event stream
 * (Response::event_stream). Each of these answers outlives the handler and
 * the request: it runs until the far end stops reading. Not on nhttpd's
 * connection thread, because without connection threads that thread is
 * the accept thread, and one such answer would be the last request the box
 * takes. Each runs on a thread of its own instead, on a copy of the
 * connection's descriptor, whatever the threading setting says; every
 * other request keeps the path it had.
 *
 * What runs on that thread is a carrier, one per kind, installed by the
 * module that knows how to fetch, to convert or to stream the events. This
 * file owns everything around it: the seats, which it counts for every kind
 * and limits for all but the event streams, the copy of the descriptor, a
 * child the carrier starts, the header, a refusal that still fits before
 * it, and the stop.
 *
 * One mutex holds the register of jobs, and four rules keep it honest:
 *  - a job is registered whole (descriptor, answer, seat) before its thread
 *    exists, so a stop cannot miss a job that is about to run;
 *  - a job's cancellation is its own and is never reset, so a server that
 *    is started again does not revive a job its stop ended;
 *  - a descriptor leaves the register before it is closed, so a stop never
 *    shuts down a number that was closed and handed to a new connection;
 *  - a child is entered under the mutex and leaves it before it is reaped,
 *    so a stop never signals a process id the kernel may have reused.
 *
 * Built with exceptions like the rest of the layer. Nothing here is called
 * from nhttpd directly; the bridge is the frame that catches.
 */

#ifndef __httpd_carry_h__
#define __httpd_carry_h__

#include "endpoint.h"
#include "http.h"

#include <cstddef>
#include <stdint.h>
#include <string>

#include <sys/types.h>

namespace httpd
{

namespace carry
{

/* What an answer is carried as. Each kind has its own seats, which this
   module counts for every kind and limits for all but Events, and its own
   code for running out of room. Its jobs' thread is named for it, so the
   threads of each kind can be told apart. */
enum Kind
{
	Fetch,   // relay_url: bytes fetched from elsewhere and passed on
	Program, // stream_argv: the output of a program the box runs
	/* event_stream: the box's events as they happen. Counted here but never
	   limited: the module that keeps the streams holds them to its configured
	   ceiling and refuses one over it itself (events.h). */
	Events
};

// One answer being carried. Opaque: a carrier reaches it through the calls
// below, on the job's own thread.
struct Job;

/* What one kind is carried with.

   check answers whether an answer could start now, without starting or
   taking anything, and fills refusal where it could not. Asked for a HEAD,
   which reports what a GET would meet. NULL accepts every answer.

   run carries the answer on the job's thread and returns when it is done.
   Before its first write it may still refuse, as an ordinary document; after
   it, every failure only ends the connection. A throw ends the job like a
   return does. */
struct Carrier
{
	bool (*check)(const Response &answer, Response &refusal);
	void (*run)(Job &job);
};

// Installs the carrier of a kind, or removes it with NULL. The pointer is
// kept, so it names an object that lives as long as the process.
void setCarrier(Kind k, const Carrier *c);

// Whether r is an answer this module carries, and of which kind: a 200 that
// names a program or an address, or that is the event stream. Anything else
// is an ordinary answer.
bool carried(const Response &r, Kind *kind);

// Whether a carrier is installed for the kind.
bool hasCarrier(Kind k);

/* What a HEAD for a carried answer gets to know, and the GET would meet:
   whether a carrier exists, whether the server takes new answers, whether a
   seat is free, and the carrier's own check. Takes nothing and starts
   nothing. False fills refusal. */
bool admissible(const Response &answer, Response &refusal);

/* The answer on its way through nhttpd's handler (CyhookHandler::Carried):
   a copy owned from here on, and its release for an exchange that ends
   before the stream pass hands it to start(). */
void *hold(const Response &answer);
void release(void *held);

enum Outcome
{
	// A job carries the answer; the connection is spent for nhttpd.
	Carrying,
	// Nothing was written; refusal is the answer to send instead.
	Refused
};

/* Takes held over on every path, copies fd (which stays the caller's), and
   starts the answer's job. On nhttpd's thread, in the stream pass. Refused
   when no carrier exists for the kind (501), the server is stopping or the
   kind's seats are taken (503), or the thread cannot be started (503). A
   throw (out of memory) has given back the answer and the copy before it
   leaves. */
Outcome start(void *held, int fd, Method m, const std::string &path, Response &refusal);

// ---- a carrier's calls, on its job's thread

/* The answer being carried, which is what sendHead() writes: a carrier may
   set its type and headers before that, from what it learned on its way. */
Response &answerOf(Job &j);
Method methodOf(const Job &j);

/* The job's copy of the connection's descriptor, for a carrier that hands it
   to a module of its own to watch the far end with or to set an option on.
   Open until the carrier has returned and closed only here, so a module that
   gives it up before that never meets a number handed to another connection.
   Never to be closed by anyone else. */
int descriptorOf(const Job &j);

// Set by a stop and never cleared. Every wait of a carrier asks it.
bool cancelled(const Job &j);

/* Waits up to ms for the far end, and answers false once it is gone or the
   job is cancelled: a carrier with nothing to send asks this at least every
   quarter second, so a peer that left frees its seat at once. With 0 it looks
   once and does not wait. */
bool waitPeer(Job &j, unsigned ms);

/* The header with the answer's code, type and headers, and length as the
   Content-Length where it is not negative. The first write: from here on the
   exchange is committed, and true says the header went out whole. */
bool sendHead(Job &j, int64_t length);

// Body bytes after sendHead(). False ends the carrying.
bool sendBody(Job &j, const char *bytes, size_t n);

/* A problem document in place of the answer, with its length, while nothing
   has been written yet. False once the exchange is committed or the write
   failed. Noted like every refusal the transport makes. */
bool refuse(Job &j, const Response &problem);

/* Enters a child the carrier started, which leads a process group of its own
   so a stop can signal everything it runs. False when the job was cancelled
   in the meantime: the child has then been ended and reaped already. */
bool adoptChild(Job &j, pid_t pid);

/* Ends the child entered for the job and its whole process group, and reaps
   the child: out of the register first, then SIGTERM to the group, up to
   200 ms for the child to exit, SIGKILL to the group in any case, and only
   then the wait that collects the child, repeated when a signal interrupts
   it. Answers the wait status, -1 when there was no child or none could be
   reaped. */
int endChild(Job &j);

// ---- the server's side

/* Closes the door and ends what runs: no new answer is admitted, every job is
   cancelled, its connection shut down and its child sent SIGTERM. Returns at
   once; waitIdle() is the wait. */
void beginStop();

// Waits until no job is left, up to ms. False when one still runs then.
bool waitIdle(unsigned ms);

/* Opens the door again. A job that a stop cancelled stays cancelled, however
   long it takes to end. */
void endStop();

/* Jobs that have not ended yet, and the seats of a kind in use and in all.
   A kind this module counts but never limits (Events) has SIZE_MAX seats: it
   is never refused here for want of one. */
size_t running();
size_t seatsTaken(Kind k);
size_t seatsOf(Kind k);

// How long a write may wait for a far end that takes nothing.
unsigned sendTimeoutMs();

// What the refusal of a server that is stopping says, for the document.
const char *stoppingDetail();

/* How long a server stop waits for its carried answers: five seconds. A job
   still running then is stuck where no cancellation reaches it (a name
   lookup, say) and ends on its own later; until then it holds its seat, and
   it touches nothing of the server's, only its own answer and descriptor and
   the register, which lives as long as the process. */
unsigned stopBoundMs();

// ---- seams for the suite; nothing in the product sets them

// Waits for every job to end, however long it takes, saying so now and then.
void drainForTest();

// The points of the protocol a case can stop at: the hook runs on the thread
// that reached the point, with no lock held.
enum Point
{
	Admitted,         // registered, before its thread is created (nhttpd's thread)
	JobStarted,       // on the job's thread, before cancellation is read
	ChildSpawned,     // in adoptChild, before the child is entered
	ChildTakenOut,    // in endChild, out of the register, before any signal
	BeforeFinalReap,  // in endChild, before the wait that blocks
	JobEnded          // out of the register and its descriptor closed
};
typedef void (*PointHook)(Point p, Job *j);
void setPointHookForTest(PointHook h);

// Writes the job's bytes instead of send(2); NULL puts send(2) back.
typedef ssize_t (*Writer)(int fd, const char *bytes, size_t n);
void setWriterForTest(Writer w);

// Signals instead of kill(2); NULL puts kill(2) back.
typedef int (*Killer)(pid_t pid, int sig);
void setKillerForTest(Killer k);

// Told every descriptor this module closes, just before it does.
typedef void (*CloseProbe)(int fd);
void setCloseProbeForTest(CloseProbe p);

void setSendTimeoutMsForTest(unsigned ms);
// 0 puts the shipped bound back.
void setStopBoundMsForTest(unsigned ms);
// The next thread start fails, and the next job allocation throws.
void failThreadStartForTest(bool fail);
void failJobAllocationForTest(bool fail);

// Answers held through hold() and not yet released, and how many waits for a
// child a signal interrupted.
size_t heldAnswersForTest();
size_t interruptedReapsForTest();

} // namespace carry

} // namespace httpd

#endif
