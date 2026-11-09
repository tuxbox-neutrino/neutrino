/*
 * auth.cpp - who is asking, and what that permits
 *
 * Copyright (C) 2026 NI-Team
 * Copyright (C) 2026 Tuxbox-Project (additions)
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

#include "auth.h"

#include "webconfig.h"
#include "credentials.h"
#include "netmatch.h"
#include "status.h"

#include "coreapi/base/errors.h"

#include <openssl/crypto.h>

#include <cstring>
#include <limits>
#include <map>
#include <string>
#include <vector>

#include <time.h>

#include <arpa/inet.h>
#include <sys/socket.h>

#include <OpenThreads/Condition>
#include <OpenThreads/Mutex>
#include <OpenThreads/ScopedLock>

namespace httpd
{

namespace
{

/* One name each, so that whatever hands a session out and whatever reads one
   back cannot be written against two spellings. The header carries the same
   two out to whoever writes the endpoint that hands one out. */
const char kSessionCookie[] = "nhttpd-session";
const char kCsrfHeader[]    = "X-CSRF-Token";

/* The name a token travels under in a query. Plain, because the three that read it are a
   transport, a router and a document generator, and a name with a prefix on it would be a
   name a person copying an address has to get right too. */
const char kQueryToken[]    = "token";

/* How long the gate leaves between walks of the session table. A walk is the length of
   the table, and one per request would make the cost of answering anything depend on how
   many browsers had ever logged in. A minute is short beside the shortest session the
   configuration permits.

   A session that has run out is refused the moment it is looked up whether or not the
   walk has been round, so this is about giving memory back and never about who is let
   in. */
time_t sweep_every = 60;

/* A session runs out once it has been left alone for the configured lifetime, and
   every request it carries moves that moment on. opened is kept so the moving stops
   at the ceiling below: a session somebody uses every day still ends, and a cookie
   copied off a machine is not good for as long as the machine keeps using it. */
struct Session
{
	std::string user;
	std::string csrf;
	time_t      opened;
	time_t      expires;

	// the account it answers for (webconfig.h, Login::revision)
	unsigned long long revision;

	// kept out of the walk by a change to the account that goes on with it (holdSession)
	bool held;
	// a change to the account commits with it, which a logout waits for (commitSession)
	bool committing;
	// signed out while a change committed with it: it ends when the change lets go
	bool closing;

	Session() : opened(0), expires(0), revision(0), held(false), committing(false), closing(false) {}
};

/* The longest a session lasts however busy it is, which is also the longest
   lifetime the configuration accepts, so a lifetime set to the maximum is a session
   that does not slide at all. Sixty four sessions at once, as many as the page
   login before this one kept; a full table gives up the session that would run out
   soonest. */
const time_t kSessionCeiling = 30 * 24 * 60 * 60;
const size_t kMaxSessions    = 64;

/* How many seconds a logout waits at most for a change committing with its session
   (closeSession); a change takes moments. */
const unsigned kLogoutWaitSteps = 30;

/* One minted token: the first characters of it, the stored form of the whole of it, what
   it was minted at, what part of the box it stands for, and when it stops. The token
   itself is not here, which is the point of storing a hash at all.

   An empty scope is the whole of whatever the level reaches, which is what a token named
   in the configuration is. An expiry of nought is one that does not run out, which is the
   same set. Both defaults are the shape this table already had, so a record written
   without either is the record it used to be. */
struct ApiToken
{
	std::string prefix;
	std::string stored;
	AuthLevel   level;
	std::string scope;
	time_t      expires;

	/* The cookie token of the session that drew this one, and empty for a token
	   drawn any other way. A token drawn from a session ends with it: signing out
	   ends what the page handed to a player as well. */
	std::string session;

	/* The account the session was opened under when it drew this, read only where
	   session is set. Kept apart from the session's own, which a change to the
	   account can carry across: a token travels in addresses, and one handed to a
	   player before the password changed is the credential most likely to have
	   been seen by somebody else. */
	unsigned long long revision;

	ApiToken() : level(AuthLevel::Public), expires(0), revision(0) {}
};

/* What one address has been doing at the login route. Nothing about who it
   claims to be: the whole of what this records is that a guess was wrong and
   when, so there is nothing here to learn by reading it. */
struct Attempts
{
	unsigned failures;
	time_t   last;

	Attempts() : failures(0), last(0) {}
};

/* The free attempts, the ceiling on the delay, how long a quiet address is remembered
   for, and how many of these may be answered at once.

   Two free, because the ordinary reason a password is wrong is that somebody mistyped
   it. A minute at the top, and that number is the whole of the promise that this cannot
   lock the only account out: there is one account and nobody to unlock it. Four at once,
   a box serving one household. Two hundred and fifty six addresses, so a caller cannot
   make this table the thing it exhausts; when it is full the address quiet longest goes,
   which is the direction that cannot lock anybody out. */
const unsigned kFreeAttempts     = 2;
const unsigned kMaxLoginDelay    = 60;
const time_t   kForgetAttempts   = 600;
const unsigned kMaxLoginsInFlight = 4;
const size_t   kMaxAttemptRecords = 256;

struct Table
{
	std::map<std::string, Session> sessions;
	std::vector<ApiToken>          tokens;
	time_t                         swept;

	std::map<std::string, Attempts> attempts;
	unsigned                        in_flight;

	/* The wrong answers the box still takes (auth.h, kLoginFailureBudget), and the
	   moment from which time counts what it gives back: when the budget was last
	   whole, or when time last gave one back. */
	unsigned                        budget;
	time_t                          budget_since;

	// The addresses answered right lately and when (auth.h, kKnownAddressFor).
	std::map<std::string, time_t>   known;

	Table() : swept(0), in_flight(0), budget(kLoginFailureBudget), budget_since(0) {}
};

// Nought is the real clock, which is what the product runs on.
time_t test_clock = 0;

// The same for the session table, kept apart so that walking the login delay
// does not age every session a case opened.
time_t session_clock = 0;

time_t sessionNow()
{
	return (session_clock != 0) ? session_clock : time(NULL);
}

/* Built on first use rather than at namespace scope, for the reason the server
   beside this gives: nothing in this directory may need a constructor to have
   run before main. */
Table &table()
{
	static Table t;
	return t;
}

// Held by everything that reads or writes the table above. The daemon answers
// requests on a pool of threads, so every one of those reads is a read another
// thread can be in the middle of writing.
OpenThreads::Mutex &lock()
{
	static OpenThreads::Mutex m;
	return m;
}

// Woken whenever a change lets go of a session it committed with (letGoLocked).
OpenThreads::Condition &letGo()
{
	static OpenThreads::Condition *c = new OpenThreads::Condition;
	return *c;
}

/* Whether a session answers for anything at now: opened under the account in effect
   and not run out. account is accountRevision(), which every caller reads before it
   takes the lock above, the account having a lock of its own. */
bool standing(const Session &s, time_t now, unsigned long long account)
{
	return s.revision == account && s.expires > now && !s.closing;
}

/* Whether a token drawn from a session still stands: drawn under the account in
   effect, and the session it was drawn from standing. Called with the lock held, and
   only for a token that names a session. */
bool drawnTokenStandsLocked(const ApiToken &t, time_t now, unsigned long long account)
{
	if (t.revision != account)
		return false;
	const std::map<std::string, Session>::const_iterator owner = table().sessions.find(t.session);
	return owner != table().sessions.end() && standing(owner->second, now, account);
}

/* Whether two secrets are the same, in a time that does not depend on how much of one
   matches the other.

   Neither an empty expectation nor an empty answer is a match. Without that a session
   whose second token could not be drawn would be one that every request sending no
   header satisfies. The lengths are compared first and that comparison is not constant
   time: the length of these is fixed and public. */
bool sameSecret(const std::string &want, const std::string &given)
{
	if (want.empty() || given.empty())
		return false;
	if (want.size() != given.size())
		return false;
	return CRYPTO_memcmp(want.data(), given.data(), want.size()) == 0;
}

// Whether text is an address of either family and nothing else. A zero byte
// ends what the system call reads and not what the string holds, so a value
// carrying one is refused rather than judged on the part in front of it.
bool isAddress(const std::string &text)
{
	if (text.empty() || text.size() >= INET6_ADDRSTRLEN)
		return false;
	if (text.find('\0') != std::string::npos)
		return false;

	unsigned char bits[16];
	if (inet_pton(AF_INET, text.c_str(), bits) == 1)
		return true;
	return inet_pton(AF_INET6, text.c_str(), bits) == 1;
}

std::string trimmed(const std::string &text)
{
	size_t from = 0;
	while (from < text.size() && (text[from] == ' ' || text[from] == '\t'))
		++from;

	size_t to = text.size();
	while (to > from && (text[to - 1] == ' ' || text[to - 1] == '\t'))
		--to;

	return text.substr(from, to - from);
}

/* One element of a forwarded header as an address, and empty for anything that is
   not one. A port is taken off only where the colon cannot be part of the address,
   which is the v4 form and the bracketed v6 one: a v6 address written bare is all
   colons, and cutting at the first would answer a prefix of it as the whole. */
std::string forwardedAddress(const std::string &element)
{
	std::string v = trimmed(element);
	if (v.empty())
		return std::string();

	if (v[0] == '[')
	{
		const size_t close = v.find(']');
		if (close == std::string::npos)
			return std::string();
		v = v.substr(1, close - 1);
	}
	else
	{
		const size_t colon = v.find(':');
		if (colon != std::string::npos && v.find(':', colon + 1) == std::string::npos)
			v = v.substr(0, colon);
	}

	return isAddress(v) ? v : std::string();
}

/* The last element of the forwarded header, and empty when that element is not an
   address. Only the last is looked at: walking back for an earlier element that
   parses would let a client that can append one unusable element choose which of
   its own elements is believed. */
std::string lastForwarded(const std::string &header)
{
	const size_t comma = header.rfind(',');
	if (comma == std::string::npos)
		return forwardedAddress(header);
	return forwardedAddress(header.substr(comma + 1));
}

/* When a session opened now runs out.

   A sum past what a time_t holds would wrap to a moment already gone. That fails closed,
   since a session whose expiry is in the past is refused, but it is a refusal by accident
   of arithmetic and this makes it arithmetic. A clock reading before the epoch answers
   now, which is a session already over. Every other input this file cannot use answers
   the least it can mean. */
time_t expiryFrom(time_t now, unsigned lifetime)
{
	if (now < 0)
		return now;

	const time_t ceiling = std::numeric_limits<time_t>::max();
	const unsigned long long room = (unsigned long long) (ceiling - now);
	if ((unsigned long long) lifetime > room)
		return ceiling;
	return now + (time_t) lifetime;
}

// Drops the tokens a session drew. Called with the lock held.
void dropTokensOfLocked(const std::string &session)
{
	size_t kept = 0;
	for (size_t k = 0; k < table().tokens.size(); ++k)
	{
		if (table().tokens[k].session == session)
			continue;
		if (kept != k)
			table().tokens[kept] = table().tokens[k];
		++kept;
	}
	table().tokens.resize(kept);
}

/* Drops what has run out, and what was opened under an account older than account.
   Called with the lock held.

   Older and not other: account was read before the lock was taken, and a change can
   publish and carry a session across in between. Such a session holds a revision
   newer than the reading, and dropping it would sign out the caller the change was
   made to keep. Revisions only grow, so a session older than the reading never
   stands again.

   A session a change holds (holdSession) is dropped only once it has run out: that
   change has published the account it moves the session to, and is about to. One a
   change is committing with (commitSession) is not dropped at all, run out or not:
   a logout of it waits for the write to end, and finds the session gone only when the
   change let go of it or the logout gave up waiting. It answers for nothing
   meanwhile, the lookups asking about its end and its commit as before. */
void expireLocked(time_t now, unsigned long long account)
{
	std::map<std::string, Session>::iterator i = table().sessions.begin();
	while (i != table().sessions.end())
	{
		std::map<std::string, Session>::iterator here = i;
		++i;
		if (here->second.committing)
			continue;
		if (here->second.expires <= now || (here->second.revision < account && !here->second.held))
		{
			dropTokensOfLocked(here->first);
			table().sessions.erase(here);
		}
	}

	/* The tokens with an end to them go the same way and in the same walk. A record
	   that has run out is refused where it is looked up whether or not this has been
	   round, so this is about giving memory back: a page that mints one per visit
	   would otherwise leave the table as long as the daemon runs. Nought is a record
	   with no end, which is every token a configuration named. A token drawn under
	   an older account goes as well, its session carried across or not. */
	size_t kept = 0;
	for (size_t k = 0; k < table().tokens.size(); ++k)
	{
		if (table().tokens[k].expires != 0 && table().tokens[k].expires <= now)
			continue;
		if (!table().tokens[k].session.empty() && table().tokens[k].revision < account)
			continue;
		if (kept != k)
			table().tokens[kept] = table().tokens[k];
		++kept;
	}
	table().tokens.resize(kept);
}

/* The walk the gate does, at most once an interval. A clock that went backwards
   sweeps at once and takes the mark back with it, because the difference read the
   other way round is negative, is under any interval, and would leave the table
   never walked again until the clock had caught up. */
void sweepIfDue()
{
	const unsigned long long account = accountRevision();
	OpenThreads::ScopedLock<OpenThreads::Mutex> held(lock());
	const time_t now = time(NULL);
	if (now >= table().swept && now - table().swept < sweep_every)
		return;
	table().swept = now;
	expireLocked(now, account);
}

/* The session a cookie token names, when there is one and it has not run out.
   Expiry is read here and not left to the walk above: the walk happens on a timer,
   and a session that stayed usable until the next one came round would have a
   lifetime of the setting plus up to an interval. */
bool liveSession(const std::string &token, std::string *csrf,
                 std::string *user = NULL, time_t *expires = NULL)
{
	if (token.empty())
		return false;

	const unsigned long long account = accountRevision();
	OpenThreads::ScopedLock<OpenThreads::Mutex> held(lock());

	/* This lookup is the one comparison in this file that is not constant time: a
	   map orders with a comparison that stops at the first byte that differs.
	   Judged and left. The token is thirty two bytes out of the kernel's pool, the
	   difference is a handful of byte comparisons inside a tree walk, and it is
	   measured across a network by whoever would use it. Closing it would mean
	   keying the table by a prefix and comparing the rest constant time. */
	const std::map<std::string, Session>::const_iterator i = table().sessions.find(token);
	if (i == table().sessions.end())
		return false;
	if (!standing(i->second, sessionNow(), account))
		return false;

	if (csrf != NULL)
		*csrf = i->second.csrf;
	if (user != NULL)
		*user = i->second.user;
	if (expires != NULL)
		*expires = i->second.expires;
	return true;
}

/* Moves a live session's end on by the configured lifetime, and never past the
   ceiling counted from when it was opened. Asked by the gate once a session has
   answered for a request, so it is a request that keeps a session alive and not
   somebody asking how long one has left. */
void slideSession(const std::string &token)
{
	if (token.empty())
		return;

	const unsigned long long account = accountRevision();
	OpenThreads::ScopedLock<OpenThreads::Mutex> held(lock());
	const std::map<std::string, Session>::iterator i = table().sessions.find(token);
	if (i == table().sessions.end())
		return;

	const time_t now = sessionNow();
	if (!standing(i->second, now, account))
		return;

	const time_t moved   = expiryFrom(now, config().session_lifetime_s);
	const time_t ceiling = expiryFrom(i->second.opened, (unsigned) kSessionCeiling);
	const time_t want    = (moved < ceiling) ? moved : ceiling;
	if (want > i->second.expires)
		i->second.expires = want;
}

/* What a token is worth and what part of the box it stands for, and false when it is not
   one this ever minted.

   The records are found by the prefix and copied out, and the lock is given up before any
   of them is confirmed: confirming one is thousands of rounds of a hash, and a worker
   that held the table through that would stop every other worker.

   Nothing is derived at all for a token whose prefix matches no record, or an
   unauthenticated caller could choose the cost of answering it.

   A record that has run out is left out of the copy, so it is refused here whether or not
   the walk has been round. Expiry is read under the lock along with everything else, so
   what is confirmed afterwards is a record that was live when it was chosen.

   The scope answered is the scope of the record that decided the level and never the
   highest of several: a level from one record beside a scope from another would be a
   credential neither was minted as. */
bool tokenLevel(const std::string &token, AuthLevel *out, std::string *scope = NULL)
{
	const std::string prefix = tokenLookupPrefix(token);
	if (prefix.empty())
		return false;

	std::vector<ApiToken> candidates;
	{
		const unsigned long long account = accountRevision();
		OpenThreads::ScopedLock<OpenThreads::Mutex> held(lock());
		const time_t now = time(NULL);
		for (size_t i = 0; i < table().tokens.size(); ++i)
		{
			if (table().tokens[i].prefix != prefix)
				continue;
			if (table().tokens[i].expires != 0 && table().tokens[i].expires <= now)
				continue;
			/* A token drawn from a session is refused once that session has run
			   out or the account has changed, whether or not the walk has been
			   round to take both away. */
			if (!table().tokens[i].session.empty() &&
			    !drawnTokenStandsLocked(table().tokens[i], sessionNow(), account))
				continue;
			candidates.push_back(table().tokens[i]);
		}
	}

	bool found = false;
	size_t best = 0;
	for (size_t i = 0; i < candidates.size(); ++i)
	{
		if (!verifySecret(token, candidates[i].stored))
			continue;
		if (!found || (int) candidates[i].level > (int) candidates[best].level)
			best = i;
		found = true;
	}

	if (!found)
		return false;
	if (out != NULL)
		*out = candidates[best].level;
	if (scope != NULL)
		*scope = candidates[best].scope;
	return true;
}

/* The methods that ask for something rather than change it. Named one by one,
   so that a method added to the enum is one that changes something until
   somebody decides otherwise, rather than one that does not by falling through
   a comparison. */
bool safeMethod(Method m)
{
	return m == Get || m == Head || m == Options;
}

/* Whether a request that changes something has shown it was not made by another site.

   A cookie is attached by the browser to whatever asks for it, including a page on
   another site, so a cookie on its own says nothing about who asked. The second token
   says it: it is read out of the page and put in a header, and another site can do
   neither.

   A request carrying an Authorization header needs no second token. A form cannot set
   that header and a script that sets it asks this server for permission first, which this
   server never gives. A cookie that names no live session granted nothing. */
bool csrfSatisfied(Method m, const Credentials &c)
{
	if (safeMethod(m))
		return true;

	/* The cookie is asked about first and the bearer last, because of what each
	   costs. A cookie that names no live session granted nothing, and that answer
	   is a lookup; confirming a bearer is thousands of rounds of a hash, and in
	   this order it is reached only by a request that already holds a live session,
	   changes something, and did not carry the second token. */
	std::string want;
	if (!liveSession(c.cookie_token, &want))
		return true;

	if (sameSecret(want, c.csrf_header))
		return true;

	/* A bearer token that resolved, and not a bearer header that arrived. Presence
	   alone would have been the wider rule: a page on another site that could get
	   any bytes at all into it would be exempted by the ones it chose. Nothing else
	   in this file accepts a credential it did not confirm.

	   A token with a scope exempts nothing. It is the one credential this server
	   hands out to be carried in an address, so it is the one that ends up in places
	   a token is read out of, and a credential that can leak is not a credential that
	   says who asked. */
	if (c.bearer_token.empty())
		return false;

	std::string named;
	return tokenLevel(c.bearer_token, NULL, &named) && named.empty();
}

Response refused(const std::string &detail)
{
	return problemResponse(StatusForbidden, coreapi::ErrorCode::NotPermitted, detail);
}

// Read through one call, so the gate and everything it writes down agree about
// when now is whether or not a case has moved it.
time_t nowFor()
{
	return (test_clock != 0) ? test_clock : time(NULL);
}

/* Drops every address that has been quiet long enough to be forgotten, and then
   the one that has been quiet longest if the table is still full.

   Held by the caller. */
void forgetQuietLocked(time_t now)
{
	std::map<std::string, Attempts>::iterator i = table().attempts.begin();
	while (i != table().attempts.end())
	{
		if (now - i->second.last >= kForgetAttempts)
			table().attempts.erase(i++);
		else
			++i;
	}

	while (table().attempts.size() >= kMaxAttemptRecords)
	{
		std::map<std::string, Attempts>::iterator oldest = table().attempts.begin();
		for (i = table().attempts.begin(); i != table().attempts.end(); ++i)
		{
			if (i->second.last < oldest->second.last)
				oldest = i;
		}
		table().attempts.erase(oldest);
	}
}

/* Whether an address answered right within kKnownAddressFor. A clock that went back
   since reads as within, the table being bounded in any case. Held by the caller. */
bool knownLocked(const std::string &peer, time_t now)
{
	const std::map<std::string, time_t>::const_iterator k = table().known.find(peer);
	return k != table().known.end() && now - k->second < kKnownAddressFor;
}

// Notes a right answer from an address, giving up the oldest when full. Held by the caller.
void noteKnownLocked(const std::string &peer, time_t now)
{
	std::map<std::string, time_t> &known = table().known;
	if (known.find(peer) == known.end())
	{
		while (known.size() >= kMaxKnownAddresses)
		{
			std::map<std::string, time_t>::iterator oldest = known.begin();
			for (std::map<std::string, time_t>::iterator k = known.begin(); k != known.end(); ++k)
				if (k->second < oldest->second)
					oldest = k;
			known.erase(oldest);
		}
	}
	known[peer] = now;
}

/* What time gave back to the budget since it was last counted, a whole one every
   interval and never more than it holds. A clock that went backwards gives nothing back
   and counts again from now, which turns nobody away for longer than one interval more.
   Held by the caller. */
void refillLocked(time_t now)
{
	Table &t = table();
	if (t.budget >= kLoginFailureBudget)
		return;
	if (now < t.budget_since)
	{
		t.budget_since = now;
		return;
	}
	const time_t back = (now - t.budget_since) / (time_t) kLoginBudgetRefill;
	if (back >= (time_t) (kLoginFailureBudget - t.budget))
	{
		t.budget = kLoginFailureBudget;
		return;
	}
	t.budget += (unsigned) back;
	t.budget_since += back * (time_t) kLoginBudgetRefill;
}

// The value of one character of the base64 alphabet, or a negative number
// for anything else, padding included.
int base64Value(char c)
{
	if (c >= 'A' && c <= 'Z')
		return c - 'A';
	if (c >= 'a' && c <= 'z')
		return (c - 'a') + 26;
	if (c >= '0' && c <= '9')
		return (c - '0') + 52;
	if (c == '+')
		return 62;
	if (c == '/')
		return 63;
	return -1;
}

/* A plain decode of whatever base64 a client's own library produced, unlike
   credentials.cpp's decoder: that one polices a stored secret this program wrote
   itself, down to the bit pattern padding leaves behind. This decodes a header a
   browser or curl chose to send, there is no canonical spelling to hold it to, and
   a payload whose padding bits are not all zero is not a forgery to refuse. */
bool decodeBasicPayload(const std::string &in, std::string &out)
{
	out.clear();
	if (in.empty() || (in.size() % 4) != 0)
		return false;

	size_t pad = 0;
	while (pad < 2 && in[in.size() - 1 - pad] == '=')
		++pad;

	for (size_t i = 0; i < in.size(); i += 4)
	{
		const bool last = (i + 4 == in.size());
		const size_t here = last ? pad : 0;
		unsigned v = 0;

		for (size_t j = 0; j < 4; ++j)
		{
			const char c = in[i + j];
			if (last && j >= (4 - here))
			{
				if (c != '=')
					return false;
				v <<= 6;
				continue;
			}
			const int d = base64Value(c);
			if (d < 0)
				return false;
			v = (v << 6) | static_cast<unsigned>(d);
		}

		out += static_cast<char>((v >> 16) & 0xFF);
		if (here < 2)
			out += static_cast<char>((v >> 8) & 0xFF);
		if (here < 1)
			out += static_cast<char>(v & 0xFF);
	}
	return true;
}

const char kNotPermitted[] = "this endpoint is not open to this caller";

} // namespace

const char *notPermittedDetail()
{
	return kNotPermitted;
}

const char *csrfRefusedDetail()
{
	return "this request changes something and carries no matching token";
}

const char *authLevelName(AuthLevel a)
{
	switch (a)
	{
		case AuthLevel::Public: return "public";
		case AuthLevel::Read:   return "read";
		case AuthLevel::Write:  return "write";
		case AuthLevel::System: return "system";
	}
	/* No default above, and an unhandled enumerator is an error in this directory,
	   so a level added without a name stops the build. What is left for it is a
	   value cast into the enum from outside, and the highest is what that answers
	   for the reason the header gives. */
	return "system";
}

const char *sessionCookieName()
{
	return kSessionCookie;
}

const char *csrfHeaderName()
{
	return kCsrfHeader;
}

const char *queryTokenName()
{
	return kQueryToken;
}

std::string bearerToken(const std::string &authorization)
{
	const char scheme[] = "bearer";
	const size_t n = sizeof(scheme) - 1;

	if (authorization.size() <= n)
		return std::string();

	for (size_t i = 0; i < n; ++i)
	{
		const char c = authorization[i];
		const char lower = (c >= 'A' && c <= 'Z') ? (char) (c - 'A' + 'a') : c;
		if (lower != scheme[i])
			return std::string();
	}

	if (authorization[n] != ' ' && authorization[n] != '\t')
		return std::string();

	return trimmed(authorization.substr(n));
}

std::string basicCredential(const std::string &authorization)
{
	const char scheme[] = "basic";
	const size_t n = sizeof(scheme) - 1;

	if (authorization.size() <= n)
		return std::string();

	for (size_t i = 0; i < n; ++i)
	{
		const char c = authorization[i];
		const char lower = (c >= 'A' && c <= 'Z') ? (char) (c - 'A' + 'a') : c;
		if (lower != scheme[i])
			return std::string();
	}

	if (authorization[n] != ' ' && authorization[n] != '\t')
		return std::string();

	std::string decoded;
	if (!decodeBasicPayload(trimmed(authorization.substr(n)), decoded))
		return std::string();
	return decoded;
}

std::string clientAddress(const Credentials &c)
{
	const WebConfig &cfg = config();
	if (cfg.trusted_proxies.empty())
		return c.peer;
	if (!addressInAnyPrefix(c.peer, cfg.trusted_proxies))
		return c.peer;

	const std::string forwarded = lastForwarded(c.forwarded_for);
	return forwarded.empty() ? c.peer : forwarded;
}

std::string cookieValue(const std::string &header, const char *name)
{
	const size_t nlen = strlen(name);
	size_t at = 0;
	while (at < header.size())
	{
		while (at < header.size() && (header[at] == ' ' || header[at] == ';'))
			at++;
		const size_t stop_at = header.find(';', at);
		const size_t stop = (stop_at == std::string::npos) ? header.size() : stop_at;
		const size_t eq = header.find('=', at);
		if (eq != std::string::npos && eq < stop && eq - at == nlen &&
		    header.compare(at, nlen, name) == 0)
			return header.substr(eq + 1, stop - eq - 1);
		at = stop + 1;
	}
	return std::string();
}

AuthLevel sessionLevel(const std::string &token)
{
	if (!liveSession(token, NULL))
		return AuthLevel::Public;

	/* The same for every session there is, because a session is opened for the one
	   account the configuration names. Written as a question asked of a session
	   rather than as the constant it currently answers, so whoever gives an account
	   a level of its own changes this and nothing else. */
	return AuthLevel::System;
}

AuthLevel granted(const Credentials &c, bool query_token_ok, std::string *scope,
                  std::string *session)
{
	sweepIfDue();

	if (scope != NULL)
		scope->clear();
	if (session != NULL)
		session->clear();

	/* A token in a header, which is the way every caller that can set one presents
	   one. A scope on it is honoured only where the route reads one; on every other
	   route such a token is a credential this request cannot use, and the reading
	   goes on to whatever else the request carried rather than stopping here. Without
	   that the scope would be worth nothing the moment the same token was moved out
	   of the query and into a header.

	   Each of the two writes into a level of its own, so a token that resolved and is
	   not usable here leaves nothing behind for the branch after it to answer with. */
	if (!c.bearer_token.empty())
	{
		AuthLevel   presented = AuthLevel::Public;
		std::string named;
		if (tokenLevel(c.bearer_token, &presented, &named) &&
		    (named.empty() || query_token_ok))
		{
			if (scope != NULL)
				*scope = named;
			return presented;
		}
	}

	/* A token in the query, for a program that can present a credential no other way.
	   Only where the route says so, and only when the token carries a scope: a token
	   without one is this box's standing credential, and a way of putting that in an
	   address is a way of leaving it in a history list and in a log. */
	if (query_token_ok && !c.query_token.empty())
	{
		AuthLevel   presented = AuthLevel::Public;
		std::string named;
		if (tokenLevel(c.query_token, &presented, &named) && !named.empty())
		{
			if (scope != NULL)
				*scope = named;
			return presented;
		}
	}

	/* A token naming no live session answers Public, and Public here is not an
	   answer but the fall through to the network rules below. Anything above it
	   is a session, and a session is the answer. */
	const AuthLevel from_session = sessionLevel(c.cookie_token);
	if (from_session != AuthLevel::Public)
	{
		slideSession(c.cookie_token);
		if (session != NULL)
			*session = c.cookie_token;
		return from_session;
	}

	const WebConfig &cfg = config();

	/* Off entirely and not narrowed, for the reason the header gives: behind a proxy
	   the loopback is the only address there is, and it is inside the list this
	   would be matched against. Asked of the configuration and not of the list,
	   because a line naming a proxy that the file could not deliver leaves the list
	   empty, and an empty list read here is a box with nothing in front of it. */
	if (proxiesConfigured(cfg))
		return AuthLevel::Public;

	/* The address off the socket, and never the one clientAddress resolves.
	   The guard above already means a forwarded address cannot reach this
	   line, and reading the socket's own address makes that true a second time
	   rather than by that guard alone. */
	if (addressInAnyPrefix(c.peer, cfg.lan_read))
		return AuthLevel::Read;

	return AuthLevel::Public;
}

AuthLevel grantedWithoutSession(const Credentials &c)
{
	Credentials without = c;
	without.cookie_token.clear();
	return granted(without);
}

bool allowed(AuthLevel need, AuthLevel have, Method m, const Credentials &c,
             Response *refusal)
{
	if (refusal != NULL)
		*refusal = Response();

	/* The order of the members of AuthLevel is the order of privilege, which is
	   stated where they are declared and is the whole of this comparison. */
	if ((int) have < (int) need)
	{
		if (refusal != NULL)
			*refusal = refused(notPermittedDetail());
		return false;
	}

	if (!csrfSatisfied(m, c))
	{
		/* Says what is missing and not what was asked for. Only a caller
		   already holding a live session ever reads this, and it still names
		   no path and nothing about what this server has. */
		if (refusal != NULL)
			*refusal = refused(csrfRefusedDetail());
		return false;
	}

	return true;
}

std::string openSessionForTest(const std::string &user)
{
	return openSession(user, accountRevision());
}

std::string openSession(const std::string &user, unsigned long long revision)
{
	const std::string token = randomToken();
	const std::string csrf  = randomToken();
	if (token.empty() || csrf.empty())
		return std::string();

	Session s;
	s.user = user;
	s.csrf = csrf;
	s.revision = revision;

	const unsigned long long account = accountRevision();
	/* Checked against an account that has been replaced since: a session opened
	   now would answer for an account that never had this password. */
	if (revision != account)
		return std::string();

	OpenThreads::ScopedLock<OpenThreads::Mutex> held(lock());
	s.opened  = sessionNow();
	s.expires = expiryFrom(s.opened, config().session_lifetime_s);

	/* Full: what has run out goes first, and if that was nothing the session that
	   would run out soonest, with whatever it drew. A caller signing in over and
	   over pushes out its own sessions before anybody else's busy one. */
	if (table().sessions.size() >= kMaxSessions)
		expireLocked(s.opened, account);
	if (table().sessions.size() >= kMaxSessions)
	{
		// never one a change holds, which that change is about to move
		std::map<std::string, Session>::iterator victim = table().sessions.end();
		for (std::map<std::string, Session>::iterator i = table().sessions.begin();
		     i != table().sessions.end(); ++i)
			if (!i->second.held &&
			    (victim == table().sessions.end() || i->second.expires < victim->second.expires))
				victim = i;
		if (victim == table().sessions.end())
			return std::string();
		dropTokensOfLocked(victim->first);
		table().sessions.erase(victim);
	}

	/* A token already in the table is not a token to hand out twice, and the
	   answer for one is no session rather than the other session under it. */
	if (!table().sessions.insert(std::make_pair(token, s)).second)
		return std::string();

	return token;
}

bool sessionIsLive(const std::string &token)
{
	return liveSession(token, NULL);
}

/* The change that held a session is done with it: back into the walk, and a logout
   waiting for the change woken. Where one came meanwhile the session ends here, in place
   of whatever the change was about to do with it, and false says so. Called with the
   lock held. */
bool letGoLocked(std::map<std::string, Session>::iterator i)
{
	const bool closing = i->second.closing;
	i->second.held = false;
	i->second.committing = false;
	if (closing)
	{
		dropTokensOfLocked(i->first);
		table().sessions.erase(i);
	}
	letGo().broadcast();
	return !closing;
}

bool commitSession(const std::string &token, unsigned long long revision)
{
	if (token.empty())
		return false;

	OpenThreads::ScopedLock<OpenThreads::Mutex> held(lock());
	const std::map<std::string, Session>::iterator i = table().sessions.find(token);
	if (i == table().sessions.end() || !standing(i->second, sessionNow(), revision))
		return false;
	i->second.held = true;
	i->second.committing = true;
	return true;
}

bool holdSession(const std::string &token, unsigned long long revision)
{
	if (token.empty())
		return false;

	OpenThreads::ScopedLock<OpenThreads::Mutex> held(lock());
	const std::map<std::string, Session>::iterator i = table().sessions.find(token);
	if (i == table().sessions.end() || !standing(i->second, sessionNow(), revision))
		return false;
	i->second.held = true;
	return true;
}

void releaseSession(const std::string &token)
{
	if (token.empty())
		return;

	OpenThreads::ScopedLock<OpenThreads::Mutex> held(lock());
	const std::map<std::string, Session>::iterator i = table().sessions.find(token);
	if (i != table().sessions.end())
		(void) letGoLocked(i);
}

bool carrySession(const std::string &token, unsigned long long from, unsigned long long to)
{
	if (token.empty())
		return false;

	OpenThreads::ScopedLock<OpenThreads::Mutex> held(lock());
	const std::map<std::string, Session>::iterator i = table().sessions.find(token);
	if (i == table().sessions.end())
		return false;
	// moved or not, the change that held it is done with it
	if (!letGoLocked(i) || !standing(i->second, sessionNow(), from))
		return false;
	i->second.revision = to;
	return true;
}

// Set by failNextReissueForTest and taken back by the reissue it fails.
static bool reissue_draw_fails = false;

std::string reissueSession(const std::string &token, unsigned long long from, unsigned long long to)
{
	if (token.empty())
		return std::string();
	std::string fresh = randomToken();
	std::string csrf  = randomToken();
	if (reissue_draw_fails)
	{
		reissue_draw_fails = false;
		fresh.clear();
	}
	if (fresh.empty() || csrf.empty())
	{
		releaseSession(token);
		return std::string();
	}

	OpenThreads::ScopedLock<OpenThreads::Mutex> held(lock());
	const std::map<std::string, Session>::iterator i = table().sessions.find(token);
	if (i == table().sessions.end())
		return std::string();
	// given new tokens or not, the change that held it is done with it
	if (!letGoLocked(i) || !standing(i->second, sessionNow(), from))
		return std::string();
	/* A token already in the table is not one to hand out twice; nothing is changed,
	   and the session goes on under the old token until the change ends it. */
	if (table().sessions.find(fresh) != table().sessions.end())
		return std::string();

	Session s = i->second;
	s.csrf = csrf;
	s.revision = to;
	dropTokensOfLocked(token);
	table().sessions.erase(i);
	table().sessions.insert(std::make_pair(fresh, s));
	return fresh;
}

void failNextReissueForTest()
{
	reissue_draw_fails = true;
}

std::string sessionSetCookie(const std::string &token)
{
	std::string out = sessionCookieName();
	out += "=";
	out += token;
	out += "; HttpOnly; SameSite=Lax; Path=/";
	return out;
}

bool closeSession(const std::string &token)
{
	if (token.empty())
		return false;

	OpenThreads::ScopedLock<OpenThreads::Mutex> held(lock());
	std::map<std::string, Session>::iterator i = table().sessions.find(token);
	if (i == table().sessions.end())
		return false;
	dropTokensOfLocked(token);

	/* A change to the account committing with this session has taken its go-ahead, and
	   the change is written whatever happens now: the logout waits for it, so that it
	   does not answer before a write it cannot stop, and the session ends as the change
	   lets go of it, in place of its renewal. Waited for in steps, and no longer than
	   the bound below: a change takes moments, and one that never let go would hold
	   this request for good. */
	if (i->second.committing)
	{
		i->second.closing = true;
		for (unsigned waited = 0; waited < kLogoutWaitSteps; ++waited)
		{
			i = table().sessions.find(token);
			if (i == table().sessions.end() || !i->second.committing)
				break;
			letGo().wait(&lock(), 1000);
		}
		i = table().sessions.find(token);
		if (i == table().sessions.end())
			return true;
	}
	table().sessions.erase(i);
	return true;
}

std::string csrfFor(const std::string &token)
{
	std::string csrf;
	if (!liveSession(token, &csrf))
		return std::string();
	return csrf;
}

std::string sessionUser(const std::string &token)
{
	std::string user;
	if (!liveSession(token, NULL, &user))
		return std::string();
	return user;
}

time_t sessionExpiresIn(const std::string &token)
{
	time_t expires = 0;
	if (!liveSession(token, NULL, NULL, &expires))
		return 0;

	/* The clock is read again here and not inside the lookup, so the two readings
	   are a moment apart and the difference can have gone under nought since the
	   lookup found the session live. A clock somebody set forward while this was
	   being answered does exactly that. */
	const time_t now = sessionNow();
	if (expires <= now)
		return 0;
	return expires - now;
}

size_t sessionCount()
{
	OpenThreads::ScopedLock<OpenThreads::Mutex> held(lock());
	return table().sessions.size();
}

void expireSessions(time_t now)
{
	const unsigned long long account = accountRevision();
	OpenThreads::ScopedLock<OpenThreads::Mutex> held(lock());
	expireLocked(now, account);
}

void setSweepIntervalForTest(time_t seconds)
{
	OpenThreads::ScopedLock<OpenThreads::Mutex> held(lock());
	sweep_every = seconds;
}

time_t expiryForTest(time_t now, unsigned lifetime)
{
	return expiryFrom(now, lifetime);
}

unsigned loginDelayFor(unsigned failures)
{
	if (failures <= kFreeAttempts)
		return 0;

	/* Doubling, written as a loop rather than as a shift, because a shift by
	   more than the width of the type is not a number and the count here is one
	   a caller chooses how large to make. */
	unsigned long delay = 1;
	for (unsigned i = kFreeAttempts + 1; i < failures; ++i)
	{
		delay *= 2;
		if (delay >= kMaxLoginDelay)
			return kMaxLoginDelay;
	}
	return (unsigned) delay;
}

LoginAttempt beginLoginAttempt(const std::string &peer, unsigned *retry_after, bool *debited)
{
	if (retry_after != NULL)
		*retry_after = 0;
	if (debited != NULL)
		*debited = false;

	const time_t now = nowFor();

	OpenThreads::ScopedLock<OpenThreads::Mutex> held(lock());

	/* The ceiling on what is being answered at once is asked first and is not
	   per address, because it is the half that bounds the processor: an
	   attacker with a thousand addresses defeats a per address delay and does
	   not defeat this. */
	if (table().in_flight >= kMaxLoginsInFlight)
	{
		if (retry_after != NULL)
			*retry_after = 1;
		return LoginAttempt::TooBusy;
	}

	forgetQuietLocked(now);

	const std::map<std::string, Attempts>::iterator i = table().attempts.find(peer);
	unsigned delay = 0;
	if (i != table().attempts.end())
	{
		delay = loginDelayFor(i->second.failures);
		/* A clock that went backwards under this leaves a moment in the future
		   here, and the comparison is written so that such a record delays
		   rather than being read as one whose delay has passed. It runs out
		   when the record is forgotten in any case. */
		if (delay > 0 && now < i->second.last + (time_t) delay)
		{
			if (retry_after != NULL)
				*retry_after = (unsigned)((i->second.last + (time_t) delay) - now);
			return LoginAttempt::TooSoon;
		}
	}

	/* The budget every address draws on, after the address's own delay, so an
	   address waiting out its own spends nothing of everybody's, and not at all for an
	   address answered right lately. Taken now and given back by a right answer
	   (endLoginAttempt), so attempts being answered count. */
	if (!knownLocked(peer, now))
	{
		refillLocked(now);
		if (table().budget == 0)
		{
			if (retry_after != NULL)
				*retry_after = (unsigned) ((table().budget_since + (time_t) kLoginBudgetRefill) - now);
			return LoginAttempt::TooManyWrong;
		}
		if (table().budget == kLoginFailureBudget)
			table().budget_since = now;
		--table().budget;
		if (debited != NULL)
			*debited = true;
	}

	// Stamped now, so a second attempt from this address waits for this one.
	if (delay > 0)
		i->second.last = now;

	++table().in_flight;
	return LoginAttempt::Open;
}

void endLoginAttempt(const std::string &peer, bool granted, bool debited)
{
	const time_t now = nowFor();

	OpenThreads::ScopedLock<OpenThreads::Mutex> held(lock());

	// Never below nothing, so an end without a begin cannot make room that was
	// never taken.
	if (table().in_flight > 0)
		--table().in_flight;

	if (granted)
	{
		// A right answer clears the record outright, which is half of why this
		// cannot lock the only account out, and gives back to the budget what its
		// attempt took from it: that attempt's own, and not whatever the address is
		// by now, the first right answer of several at once making it known.
		table().attempts.erase(peer);
		if (debited && table().budget < kLoginFailureBudget)
			++table().budget;
		noteKnownLocked(peer, now);
		return;
	}

	Attempts &a = table().attempts[peer];
	// Held below what the delay reads, so a caller cannot make the count wrap
	// round to a short delay by guessing four billion times.
	if (a.failures < 64)
		++a.failures;
	a.last = now;
}

void forgetLoginAttemptsForTest()
{
	OpenThreads::ScopedLock<OpenThreads::Mutex> held(lock());
	table().attempts.clear();
	table().in_flight = 0;
	table().budget = kLoginFailureBudget;
	table().budget_since = 0;
	table().known.clear();
}

size_t loginAttemptCountForTest()
{
	OpenThreads::ScopedLock<OpenThreads::Mutex> held(lock());
	return table().attempts.size();
}

void setLoginClockForTest(time_t now)
{
	OpenThreads::ScopedLock<OpenThreads::Mutex> held(lock());
	test_clock = now;
}

void setSessionClockForTest(time_t now)
{
	OpenThreads::ScopedLock<OpenThreads::Mutex> held(lock());
	session_clock = now;
}

bool addApiToken(const std::string &prefix, const std::string &hashed, AuthLevel level,
                 const std::string &scope, time_t expires, const std::string &session)
{
	/* A record with nothing to find it by or nothing to confirm it against
	   would be a record that answers for every token or for none, and which of
	   the two it is would be decided by whatever compares an empty string. */
	if (prefix.empty() || hashed.empty())
		return false;

	ApiToken t;
	t.prefix = prefix;
	t.stored = hashed;
	t.level = level;
	t.scope = scope;
	t.expires = expires;
	t.session = session;

	const unsigned long long account = accountRevision();
	OpenThreads::ScopedLock<OpenThreads::Mutex> held(lock());

	/* Drawn for a session that has run out, was closed or belongs to an account
	   replaced in the meantime: nothing is written, or the token would outlive what
	   it was drawn from after all. */
	if (!session.empty())
	{
		const std::map<std::string, Session>::const_iterator owner = table().sessions.find(session);
		if (owner == table().sessions.end() || !standing(owner->second, sessionNow(), account))
			return false;
		t.revision = owner->second.revision;
	}

	if (!scope.empty())
	{
		size_t scoped = 0;
		for (size_t i = 0; i < table().tokens.size(); ++i)
			if (!table().tokens[i].scope.empty())
				++scoped;

		/* What goes is a token that no longer stands, the walk of the table not having
		   been round to it since the account changed or it ran out, and only where
		   there is none, the one running out soonest. */
		const time_t now = time(NULL);
		while (scoped >= kMaxScopedTokens)
		{
			size_t victim = table().tokens.size();
			bool victim_stands = true;
			for (size_t i = 0; i < table().tokens.size(); ++i)
			{
				const ApiToken &k = table().tokens[i];
				if (k.scope.empty())
					continue;
				const bool stands = (k.expires == 0 || k.expires > now) &&
				                    (k.session.empty() || drawnTokenStandsLocked(k, sessionNow(), account));
				if (victim == table().tokens.size() || (victim_stands && !stands) ||
				    (stands == victim_stands && k.expires < table().tokens[victim].expires))
				{
					victim = i;
					victim_stands = stands;
				}
			}
			if (victim == table().tokens.size())
				break;
			table().tokens.erase(table().tokens.begin() + victim);
			--scoped;
		}
	}
	table().tokens.push_back(t);
	return true;
}

std::string openScopedToken(const std::string &scope, AuthLevel level,
                            const std::string &session)
{
	/* A scope is the whole of what makes this token narrower than the box, so one
	   without a scope is not drawn at all. The answer is empty, which is no token,
	   rather than a token worth everything. */
	if (scope.empty())
		return std::string();

	const std::string token = randomToken();
	if (token.empty())
		return std::string();

	const std::string prefix = tokenLookupPrefix(token);
	const std::string stored = hashSecret(token);
	if (prefix.empty() || stored.empty())
		return std::string();

	/* A few hours and no longer, which is a film and not a week, because what holds
	   one is a player somebody started from a page. Drawn from a session it also ends
	   with that session, so a credential nobody is watching any more does not outlive
	   the sign-in it was asked for. */
	if (!addApiToken(prefix, stored, level, scope,
	                 expiryFrom(time(NULL), kScopedTokenLifetime), session))
		return std::string();
	return token;
}

size_t scopedTokenCountForTest()
{
	OpenThreads::ScopedLock<OpenThreads::Mutex> held(lock());
	size_t scoped = 0;
	for (size_t i = 0; i < table().tokens.size(); ++i)
		if (!table().tokens[i].scope.empty())
			++scoped;
	return scoped;
}

void forgetApiTokens()
{
	OpenThreads::ScopedLock<OpenThreads::Mutex> held(lock());
	table().tokens.clear();
}

void forgetConfiguredApiTokens()
{
	OpenThreads::ScopedLock<OpenThreads::Mutex> held(lock());
	size_t kept = 0;
	for (size_t k = 0; k < table().tokens.size(); ++k)
	{
		if (table().tokens[k].session.empty())
			continue;
		if (kept != k)
			table().tokens[kept] = table().tokens[k];
		++kept;
	}
	table().tokens.resize(kept);
}

} // namespace httpd
