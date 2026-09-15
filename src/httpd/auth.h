/*
 * auth.h - who is asking, and what that permits
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

#ifndef __httpd_auth_h__
#define __httpd_auth_h__

#include "endpoint.h"
#include "http.h"

#include <cstddef>
#include <string>

#include <time.h>

namespace httpd
{

/* What one request carried that could say who is asking.

   Strings and not a connection, because what this decides has to be decidable without a
   socket: the interesting failures are combinations of a peer, a header and a cookie, and
   a check reachable only by opening a connection would be a check on the combinations a
   client can be made to send.

   A member nobody filled in is empty, so a value left half filled in grants the least.
   forwarded_for is every X-Forwarded-For the request carried joined with commas and not
   one of them: taking one of several is taking whichever a client wrote. */
struct Credentials
{
	// The far end of the socket as text, from the transport and never from a
	// header, because a header is written by whoever is asking.
	std::string peer;
	std::string cookie_token;
	std::string bearer_token;
	std::string csrf_header;
	std::string forwarded_for;

	/* The token the request carried in its query, which is the one credential a
	   program that cannot set a header can present. A cookie is never read from
	   there: a cookie in an address outlives the request in a history list and in
	   a log, which is the whole of what a cookie is meant to avoid. */
	std::string query_token;

	/* The decoded "user:pass" out of an Authorization header carrying the Basic
	   scheme, and empty for anything else. granted() never reads this member:
	   Basic is accepted only on the legacy surface under /control/, which reads it
	   through its own gate, and a general gate that answered for it too would make
	   Basic good for every route this server has. */
	std::string basic;
};

/* The name the session cookie and the second token travel under, stated here
   so that whatever hands a session out and whatever reads one back cannot
   disagree about what to call them. */
const char *sessionCookieName();
const char *csrfHeaderName();

/* The name a token travels under in a query, stated here for the same reason the two
   above are: the transport reads it off the request, the router drops it before it
   compares what a route declares, and the document names it as a scheme. Three readers
   of one name, and a fourth spelling of it would be a route that takes a credential
   nothing else knows about. */
const char *queryTokenName();

/* What a level is called where a level is written out rather than compared. Here rather
   than beside either of the two that write one, because both describe the same request to
   the same reader, and a page holding the document's spelling against the route's across
   two spellings holds it against nothing.

   A value that is none of the four answers the highest: it can only be one cast into the
   enum from outside, and a caller told less than it holds is a caller that asks for a
   credential it already has. */
const char *authLevelName(AuthLevel a);

/* The token out of an Authorization header, and empty for anything that is not a
   bearer one. The scheme is compared without regard to case, which is what the
   protocol says of it. A Basic header answers empty, so a password a browser was
   talked into putting in a URL never arrives here as a token to look up. */
std::string bearerToken(const std::string &authorization);

/* The "user:pass" out of an Authorization header carrying the Basic scheme,
   decoded from the base64 that follows it, and empty for anything that is not that
   scheme or whose payload will not decode. What is answered here is not itself a
   credential this file confirms: it is the pair a caller offered, and the one place
   that reads it, httpd::compat::legacyAllows, is where it is checked against the
   box's own account. */
std::string basicCredential(const std::string &authorization);

/* Which address this request is judged to have come from.

   The peer, unless a proxy list exists and the peer is on it, and then the last element
   of the forwarded header, because a proxy appends what it saw to what it was given. An
   element that is not an address is believed no more than one from an untrusted peer and
   the answer falls back to the peer; the list is never walked backwards for an earlier
   element that does parse, or a client able to append one unusable element would choose
   which of its own elements is believed.

   Whenever a proxy list exists this may be a string the caller chose, so it is what gets
   written down and shown and nothing else: the exemption below reads the socket. */
std::string clientAddress(const Credentials &c);

/* What the request carries, before anything is known about what it asked for.

   Read in the order a credential is presented in: a bearer token, then a token out of the
   query, then a session cookie, then the address it came from, then nothing. A caller
   that presented something is judged on what it presented, the address being a property
   of the network.

   A session cookie is System. There is one account on this box and the pages are that
   account, so a browser holding a live session is the box's owner. A bearer token is the
   level it was minted at and never more.

   The address raises to Read and to nothing above it, whatever else the request carries,
   or a box on a shared network would be one broadcast away from being reprogrammed by
   anything that can reach the port. And it does not apply at all once the configuration
   names a proxy list: behind a proxy every request arrives from the loopback, which is
   inside the seeded list, so the exemption would hand the local network's read to the
   whole internet. Narrowing it there is not enough, a proxy that forwards nothing leaving
   the loopback as the only address. Named, and not holding anybody: a file whose proxy
   line cannot be read leaves an empty list, which also describes a box with nothing in
   front of it.

   A credential that was presented and did not resolve leaves the request where one that
   was never presented would be, and not below it.

   TOKENS WITH A SCOPE, AND THE TWO THINGS query_token_ok DECIDES. A token minted for a
   scope stands for a part of this box and not for the whole of it, and it is worth its
   level only where the route can hold it to that part. So the flag says both: whether a
   scoped token counts here at all, and whether one may arrive in the query. Off is the
   answer for every route that does not declare it, for a path no route answers, and for
   the legacy surface, so a scope reaches exactly the routes written to read one and a
   leaked address is no more than the scope it names.

   A token out of the query is read only where the flag is set and only when it carries a
   scope. An unscoped token is the box's standing credential, and a way of presenting that
   one in an address is a way of writing it into a history list.

   scope, when it is not null, is left carrying the scope of whatever token answered, and
   left empty for every other way of arriving. A caller granted something without a scope
   is granted it for whatever the route does.

   Walks the session table for entries that have run out, at most once every interval
   below, which is the only thing that walks it in a running server. */
AuthLevel granted(const Credentials &c, bool query_token_ok = false,
                  std::string *scope = NULL);

/* The same question asked of a caller with the browser session taken off it.

   For the one surface that does not honour a session: the legacy /control/ prefix, where
   every route that changes something is a GET and no safe method is ever asked for the
   second token, so a cookie a browser attached to a top-level navigation another site
   started would be the whole of the credential. Answered by asking granted() again rather
   than by restating any part of it here.

   A caller holding a bearer token pays for its derivation a second time. Nothing that
   reaches this holds both. */
AuthLevel grantedWithoutSession(const Credentials &c);

/* Both halves of the gate in one call, and in the product each half is asked somewhere
   else.

   The level half is what the caller is against what the route declares. The method half
   is whether a request that changes something has shown it was not made by another site:
   a cookie is attached by the browser to whatever asks for it, and the second token says
   who asked. A request whose bearer token resolved needs no second token, no other site
   being able to make a browser send that header.

   In the product this is the method half and nothing else. The transport is the only
   caller and passes need as Public, because what a request needs is not known until a
   route is matched; the level half is asked where the route's own declaration is read and
   refuses through the same sentence below, so the two cannot be told apart. Both are here
   so the policy is stated once.

   refusal, when it is not null, is left carrying the answer to send and left empty when
   the request may go on. It names no path, no parameter and nothing about what this
   server has. */
bool allowed(AuthLevel need, AuthLevel have, Method m, const Credentials &c,
             Response *refusal);

/* What a refusal for want of a level says, written once because the router sends
   it too. Two copies would be two answers a client can tell apart the moment one
   of them is reworded, and telling them apart is telling a caller which floor
   turned it down. */
const char *notPermittedDetail();

/* Opens a session and answers the cookie token to hand out, or empty when it could
   not draw one. Empty is no session rather than a short one: nothing is written
   into the table, so a caller that hands the answer out without reading it hands
   out a cookie that resolves to nothing. The second token is drawn at the same
   time and a session is only written when both were drawn, a session whose second
   token is empty being one that every request sending no header satisfies. */
std::string openSession(const std::string &user);

/* What the route that hands that token out sets on the cookie is written where that route
   is: this file mints a token and knows nothing about the answer it travels in.

   The half this file does hold: the cookie says who a caller is and not who asked, the
   second token below says the second thing, and the gate refuses a request that changes
   something and carries a live cookie without it. Everything the cookie's own attributes
   buy is on top of that and not instead of it. */

// Whether there was a session under this token to drop. The next request
// carrying it is a request carrying nothing, with no interval in between.
bool closeSession(const std::string &token);

/* Whether this token still names a session that has not run out.

   A lookup and not a derivation, which is why it is worth asking on its own: the transport
   resolves what a caller is once per request, off the head, and this is the one part of
   that answer which can stop being true before the answer goes out. A bearer token cannot:
   nothing here removes one and none of them expires. */
bool sessionIsLive(const std::string &token);

/* What a request carrying this session is granted, and Public for a token naming no
   session or one that has run out.

   Here rather than written out wherever it is wanted, because the gate above and the route
   that opens a session both have to say it, and that route cannot ask the gate: the gate
   answers about a request, and the request that asked arrived carrying nothing. Also the
   one place a level that differed per account would be read. */
AuthLevel sessionLevel(const std::string &token);

/* The second token belonging to a session, and empty for a token naming no session
   or naming one that has run out. Empty for one that has run out because handing
   that one back would pair a dead cookie with a live looking header, and the two
   are checked as a pair. */
std::string csrfFor(const std::string &token);

/* The name a session was opened for, and empty for a token naming no session or naming one
   that has run out.

   A lookup of its own rather than a second value out of the call above, so a caller wanting
   one of the two pays for one and the gate, which wants neither, goes on copying nothing at
   all. */
std::string sessionUser(const std::string &token);

/* How long a session has left in seconds, and nought for a token naming no session or
   naming one that has run out.

   Seconds and not the moment it runs out, because the two clocks that would have to agree
   for a moment to mean anything are this box's and the caller's, and nothing makes them.
   Nought is also what a session with less than a second left answers. */
time_t sessionExpiresIn(const std::string &token);

/* How many sessions the table holds, counting the ones that have run out and
   have not been walked over yet. What has run out is refused when it is looked
   up, so the two figures differ only in what has been given back. */
size_t sessionCount();

// Drops every session that had run out by now. Called from the gate on a
// timer; a caller that wants it to have happened calls it.
void expireSessions(time_t now);

/* How long the gate leaves between walks of the table, in seconds. A walk is the
   length of the table and a busy server would otherwise do one per request. A case
   sets it to nothing so that the next request walks, which is the only way to see
   from outside that the gate walks at all. */
void setSweepIntervalForTest(time_t seconds);

/* When a session opened at now runs out, which is the one piece of arithmetic here
   that no clock a case can reach will exercise. A clock before the epoch answers a
   moment already gone and a lifetime that will not fit answers the furthest a
   time_t reaches, neither of which is reachable through openSession on a box whose
   clock is set. */
time_t expiryForTest(time_t now, unsigned lifetime);

/* Whether a login may be answered now, and what stands in the way when it may not.

   Login is the one route reachable without a credential and the one that derives a key on
   every attempt, a derivation being thousands of rounds of a hash. Without this an
   anonymous caller can hold the box's processor by asking repeatedly, and the same asking
   is how a password is guessed. The two are one mechanism here, which is why this is a
   gate and not a counter somebody reads afterwards.

   Two things close it. TooSoon is this caller having guessed wrong lately: the first few
   are free, and after that each wrong answer doubles how long the next attempt waits.
   TooBusy is the ceiling on how many are being answered at once.

   It cannot lock the only account out. There is one account and nobody to unlock it, so
   the delay has a ceiling of a minute however many wrong answers came before, a record is
   forgotten after a quiet spell, and a right answer clears it.

   Keyed on the address the transport observed and never on one a caller wrote. Behind a
   proxy the delay is then shared by everyone behind it, which is the strict direction.

   retry_after, when it is not null, is left carrying how many seconds to wait, and nought
   when the answer is Open. Every Open has to be answered by a call to endLoginAttempt. */
enum class LoginAttempt
{
	Open,
	TooSoon,
	TooBusy
};

LoginAttempt beginLoginAttempt(const std::string &peer, unsigned *retry_after);
void endLoginAttempt(const std::string &peer, bool granted);

/* How long an attempt waits after this many wrong answers, which is the policy
   above written as one function so that a case can walk it rather than sleep
   through it. Nought while the free ones last, doubling after that, and never above
   the ceiling. */
unsigned loginDelayFor(unsigned failures);

// Empties the table of who has been guessing, and how many addresses it holds.
void forgetLoginAttemptsForTest();
size_t loginAttemptCountForTest();

/* What the gate above calls now. Nought puts the real clock back, which is what
   the product runs on. A case sets it so that the growing delay can be walked
   through without waiting out the delay it is walking. */
void setLoginClockForTest(time_t now);

/* Records an API token, as the first characters of it and the stored form of the whole of
   it. The prefix is a parameter and not read out of the stored form, because the stored
   form is a hash and carries nothing of the token: without it the only way to find the
   record for a token would be to derive a key against every record there is, which is a
   cost an unauthenticated caller would choose by sending a header. A prefix two tokens
   share is not a collision anything has to resolve.

   An empty scope is a token worth its level wherever its level is enough, which is what
   every token a configuration names is. An expiry of nought is a token that does not run
   out, which is the same set: a line in a file is taken away by editing the file. */
void addApiToken(const std::string &prefix, const std::string &hashed, AuthLevel level,
                 const std::string &scope = std::string(), time_t expires = 0);

/* Draws a token that stands for one scope, writes it down and answers the token itself,
   or empty when it could not draw one. Empty is no token rather than a weak one: nothing
   is written into the table, so a caller handing the answer on hands on nothing that
   resolves.

   The scope is the caller's, and every caller of this is a route that states a constant.
   A scope out of a request would be a caller choosing how much of the box its own token
   is worth.

   It lives as long as a session does and in the same place: in memory, until it runs out
   or the daemon stops. Nothing here writes a token into a file, so a token that leaked
   cannot outlive the box's uptime, and there is no line in a configuration for anyone to
   find one in. */
std::string openScopedToken(const std::string &scope, AuthLevel level);

/* Empties that table. The thing that fills it from a file calls this first, so
   reading the file twice leaves the tokens the file names rather than two of each.
   There is no window in which a request could be answered out of an empty table:
   that file is read before the daemon is started and nothing reloads it under a
   running one. */
void forgetApiTokens();

} // namespace httpd

#endif
