/*
 * ep_auth.cpp - routes for sessions
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

#include "httpd/endpoints.h"

#include "httpd/auth.h"
#include "httpd/webconfig.h"
#include "httpd/credentials.h"
#include "httpd/endpoint.h"
#include "httpd/http.h"
#include "httpd/json.h"
#include "httpd/schema.h"
#include "httpd/status.h"

#include "coreapi/base/errors.h"

#include <cstddef>
#include <cstdio>
#include <string>
#include <utility>

namespace httpd
{

namespace
{

/* What the cookie is written with, and the whole of what makes the session it carries
   worth having.

   HttpOnly, so no script on any page can read the token out of the browser. Without it
   every scripting hole anywhere near the browser is a session taken.

   SameSite=Lax, so the browser attaches it to a request another site started only when
   that request is a page the person navigated to, which is a GET and changes nothing.
   What remains of the gap is closed by the second token. Strict would attach it to
   nothing a person clicked into either, so following a link to the box's own pages
   would land on a logged out one, and the request it protects beyond Lax is one the
   second token already refuses.

   Path=/, because the token is for the whole of this server.

   No expiry attribute, so the browser drops it when it closes. How long a session lives
   is settled by the table the session is in, and a cookie outliving the entry is one a
   caller keeps presenting for nothing.

   Secure is not set: this server has no TLS to offer, and setting it on a box serving
   plain HTTP is a session nothing can use. Whatever puts TLS in front of this adds it
   here. */
std::string cookieFor(const std::string &token)
{
	std::string out = sessionCookieName();
	out += "=";
	out += token;
	out += "; HttpOnly; SameSite=Lax; Path=/";
	return out;
}

/* What a caller holds, and the one shape both the route that opens a session and the
   route that says what a request carries answer with.

   One shape and not two. The route that opens a session used to answer three of these
   members, so a caller that had just logged in knew who it was and held the second token
   and still had to ask a second route what it was allowed to do. Two shapes describing
   one thing are two that come apart, and these had: a page showed a session that may
   write as one that may only read, for as long as the second answer took to arrive.

   Every member is always there. A shape whose members came and went would make a page
   read one of them to learn whether it may read the rest. */
const FieldDesc kSessionStateFields[] = {
	HTTPD_MEMBER("authenticated", FieldType::Bool,
		"whether the caller holds a session that has not run out, which the answer that opens one always says it does"),
	HTTPD_MEMBER_OF_SET("level", "public,read,write,system",
		"what the caller is granted, under the name the document describing these routes uses for it"),
	HTTPD_MEMBER("user", FieldType::String,
		"who the session belongs to, as the box knows the name, and empty for a caller holding none"),
	HTTPD_MEMBER("csrf", FieldType::String,
		"the second token, to be sent back in the header this server names on every request that changes something, and empty for a caller holding no session"),
	HTTPD_MEMBER("csrf_header", FieldType::String,
		"the header the token above travels in, said whatever the caller holds, so a page does not carry the name written out in two places"),
	HTTPD_MEMBER("expires_in", FieldType::UInt,
		"how many seconds the session has left, and nought for a caller holding none"),
};

const Schema kSessionStateSchema = { "session-state", HTTPD_FIELDS(kSessionStateFields) };

/* The one writer of the shape above, for both handlers that answer it. Two writers
   for one shape are two places that drift apart with nothing saying so, and the member
   one of them left out is what this replaces. The members are written in the order the
   rows above state them. */
void writeSessionState(Json &j, bool authenticated, AuthLevel level,
                       const std::string &user, const std::string &csrf,
                       time_t expires_in)
{
	j.beginObject();
	j.key("authenticated");
	j.value(authenticated);
	j.key("level");
	j.value(authLevelName(level));
	j.key("user");
	j.value(user);
	j.key("csrf");
	j.value(csrf);
	/* Answered whatever the caller holds, unlike every other member here. The name is
	   this server's and not any session's, and a caller about to log in needs it before it
	   has anything to put in it. Withholding it would leave every page carrying the name
	   written out a second time. */
	j.key("csrf_header");
	j.value(csrfHeaderName());
	j.key("expires_in");
	j.value((unsigned long long) expires_in);
	j.endObject();
}

/* WHAT A TOKEN FOR A PART OF THIS BOX IS ANSWERED AS.

   The token itself, the part it stands for, and how long it has. The part is said although
   the route that hands it out states it: a caller has no business working out from the
   address it asked which of these it holds, and one that did would break on the day a
   second scope exists. How long it has is what a page needs to know when to ask again.

   And nothing else. No identifier, because there is nothing to name one to: this server
   has no route that lists these and none that takes one back, which is a separate piece of
   work with more callers than a media player. */
const FieldDesc kScopedTokenFields[] = {
	HTTPD_MEMBER("token", FieldType::String,
		"the token, to be put in the query of a route that takes one under the name this server states"),
	HTTPD_MEMBER_OF_SET("scope", "media",
		"what part of the box it reaches, which is the whole of what it is worth"),
	HTTPD_MEMBER("expires_in", FieldType::UInt,
		"how many seconds it has left, after which it resolves to nothing"),
};

const Schema kScopedTokenSchema = { "scoped-token", HTTPD_FIELDS(kScopedTokenFields) };

/* One refusal for both halves of a wrong answer. Which of the name and the password
   was wrong is not said and is not answerable from the outside either: the same
   document goes back for a name this box does not have and for the right name with the
   wrong password, and both cost the same, the derivation below running whatever the
   name turned out to be.

   The header names Bearer and not Basic: nothing here reads a Basic header, and naming
   it would make a browser open a dialog for a credential this server refuses. */
Response wrongCredential()
{
	Response r = problemResponse(StatusUnauthorized, coreapi::ErrorCode::NotPermitted,
	                             "the name and the password do not go together");
	r.headers.push_back(std::make_pair(std::string("WWW-Authenticate"), std::string("Bearer")));
	return r;
}

/* Says that too much is being asked and nothing else. One document for both things
   that close the gate, because a caller does the same thing about either, and telling
   them apart would say whether the box is loaded to somebody who has presented
   nothing. Retry-After carries a wait the caller could work out by counting. */
Response tooManyAttempts(unsigned retry_after)
{
	Response r = problemResponse(StatusTooManyRequests, coreapi::ErrorCode::TooManyAttempts,
	                             "too many attempts are being made here; come back in a moment");
	char seconds[24];
	std::snprintf(seconds, sizeof(seconds), "%u", retry_after);
	r.headers.push_back(std::make_pair(std::string("Retry-After"), std::string(seconds)));
	return r;
}

/* Gives the ceiling its room back however this handler leaves. A destructor and not a
   line at the end, because this directory is built with exceptions and a throw between
   the gate and that line would leave a place in the ceiling taken by nothing for as
   long as the daemon runs. Four of those and no login is ever answered again. */
struct AttemptHeld
{
	std::string peer;
	bool        granted;

	explicit AttemptHeld(const std::string &p) : peer(p), granted(false) {}
	~AttemptHeld() { endLoginAttempt(peer, granted); }

	private:
		AttemptHeld(const AttemptHeld &);
		AttemptHeld &operator=(const AttemptHeld &);
};

Response login(const Request &r)
{
	/* Ahead of the derivation and not after it, which is the whole of what this is for:
	   the derivation is what an attempt costs, and a gate that ran afterwards would have
	   paid for the attempt it was turning down. The address off the socket and never the
	   one a proxy header reports, or a caller could empty its own record between
	   guesses. */
	unsigned retry_after = 0;
	if (beginLoginAttempt(r.peer(), &retry_after) != LoginAttempt::Open)
		return tooManyAttempts(retry_after);
	AttemptHeld held(r.peer());

	const WebConfig &cfg = config();

	/* Both halves are asked and neither settles the answer on its own, so the work of
	   answering is the same whichever is wrong. Returning before the derivation for a name
	   that does not match would make a wrong name answer in no time at all and a wrong
	   password answer in the time a right one takes, which is a way of asking the box
	   whether a name exists.

	   A box with no name configured has an empty one here and an empty password with it,
	   so the derivation below refuses whatever is offered. */
	const bool name_matches = !cfg.username.empty() && r.asString("user") == cfg.username;
	const bool secret_matches = verifySecret(r.asString("password"), cfg.password_hash);

	if (!name_matches || !secret_matches)
		return wrongCredential();

	/* Recorded before anything below can fail, so a right answer clears the record
	   whatever this server then makes of it: a caller that got the password right is not
	   somebody to go on delaying because the pool the token comes out of was empty. */
	held.granted = true;

	const std::string token = openSession(cfg.username);
	/* Nothing was written into the table, so there is no session and no cookie
	   to hand out. Answered as a fault here rather than as a refusal, because
	   the credential was right and what failed is this server. */
	if (token.empty())
		return problemResponse(StatusInternalServerError, coreapi::ErrorCode::SessionNotOpened,
		                       "this server could not open a session");

	const std::string csrf = csrfFor(token);
	if (csrf.empty())
	{
		// A session whose second token cannot be read back is one every request
		// sending no header would satisfy, so it is dropped rather than handed
		// out.
		(void) closeSession(token);
		return problemResponse(StatusInternalServerError, coreapi::ErrorCode::SessionNotOpened,
		                       "this server could not open a session");
	}

	Response out = okJson();
	/* The cookie carries the session and the body carries the second token, and the two
	   travel apart on purpose. A token the browser attaches by itself gets attached to
	   another site's requests as well; the second one goes into the page and comes back in
	   a header, and neither of those is something another site can do. */
	out.headers.push_back(std::make_pair(std::string("Set-Cookie"), cookieFor(token)));

	/* The whole of what the session is worth, in the answer that opened it, so a caller
	   which has just logged in never has to ask a second route what it may do.

	   The level is said although every session is worth the same today. A caller has no
	   business holding the rule that a live session is granted the highest level: that
	   is exactly the coupling an interface exists to take off a caller, and a caller
	   that worked it out for itself breaks on the day it stops being true.

	   expires_in is the number a caller needs to know when to renew. */
	Json j(out.body, 224);
	writeSessionState(j, true, sessionLevel(token), cfg.username, csrf,
	                  sessionExpiresIn(token));
	return out;
}

Response logout(const Request &r)
{
	/* The token the request presented, and never one a caller named in a body: a route
	   that took the session to close from what was sent would close anybody's session for
	   whoever can guess a token. Whether there was a session to drop is not answered, a
	   caller holding none and one holding a session that had run out being told the same
	   thing. */
	(void) closeSession(r.session());

	Response out = noContent();
	/* The browser is told to forget the cookie as well, so a caller that logs out does not
	   go on presenting a token that names nothing. Written with the same attributes it was
	   set with, a browser matching a cookie to replace by its name and its path. */
	std::string gone = sessionCookieName();
	gone += "=; HttpOnly; SameSite=Lax; Path=/; Max-Age=0";
	out.headers.push_back(std::make_pair(std::string("Set-Cookie"), gone));
	return out;
}

/* What the request that asked carries, and nothing about any other request and nothing
   this server holds beyond it.

   The second token goes out over a GET, which is the one thing here worth the space to
   explain. A GET is a safe method, so the gate lets one through without the token, and
   another site can therefore have a browser make this request with the cookie attached.
   What that site cannot do is read what comes back: this server sends no Access-Control-
   header of any kind, so a browser hands nothing of a cross-origin answer to the script
   that asked for it; the answer is JSON, which is not a document another site may embed
   and read out of; and every answer the router sends carries X-Content-Type-Options:
   nosniff, so it cannot be pulled in as a script either.

   What it is for: a page that has been reloaded holds the cookie, because the browser
   kept it, and holds nothing the login route put in the body. The gate asks every request
   that changes something and carries a live cookie for the second token, and login and
   logout are both such requests, so without this route such a page can neither write nor
   log in again nor log out. Excusing login from the second token would answer the same
   complaint by letting another site open a session of its own choosing in the person's
   browser.

   The four lookups below are four separate readings of one table and a session can run
   out between any two of them. Every field then falls back to what it says for a request
   carrying no session. */
Response sessionState(const Request &r)
{
	/* The token the transport observed, which is the same one the gate resolved
	   this request against. A token out of a body would answer about a session
	   belonging to whoever can guess one. */
	const std::string &token = r.session();

	/* What the gate settled for this request and not what a session alone would grant,
	   which is the one member this route answers differently from the one that opens a
	   session. A caller reaching this on a bearer token or off the box's own network
	   carries no session at all, and what such a caller may do is still the answer. */
	Response out = okJson();
	Json j(out.body, 192);
	writeSessionState(j, sessionIsLive(token), r.granted(), sessionUser(token),
	                  csrfFor(token), sessionExpiresIn(token));
	return out;
}

/* A token that reaches the media this box holds and nothing else, for a program that can present a
   credential no other way.

   THE CALLER DOES NOT CHOOSE THE SCOPE. It is in the path as a word this table wrote, and
   the handler stamps the constant beside it; nothing out of the request reaches the scope.
   So there is no value to check, no value to refuse, and no value anybody can widen by
   sending something else. A second scope is a second row here.

   SYSTEM AND NOT A LOWER LEVEL, because what this hands out is System: a route minting a
   credential at a level anybody on the home network can ask for would be that network
   granted System by asking twice. So the caller has already shown it is the box's owner,
   and this hands it a narrower way of saying so.

   A POST and not a GET, because this makes something. It also means a page holding a
   session has to send the second token to get one, which is what stops another site from
   having a browser mint one in its owner's name and read the address out of it.

   Nothing is written to any file. The record lives in memory and runs out on its own, so a
   token that leaked is worth nothing once the box has restarted, and there is no line in a
   configuration for anyone to find one in. */
Response mintRecordingsToken(const Request &)
{
	const std::string token = openScopedToken(mediaScopeName(), AuthLevel::System);
	/* Nothing was written down, so there is no token to hand out. A fault and not a
	   refusal: the caller was granted this and what failed is this server. */
	if (token.empty())
		return problemResponse(StatusInternalServerError, coreapi::ErrorCode::SessionNotOpened,
		                       "this server could not draw a token");

	Response out = okJson();
	Json j(out.body, 160);
	j.beginObject();
	j.key("token");
	j.value(token);
	j.key("scope");
	j.value(mediaScopeName());
	j.key("expires_in");
	/* The session's own length, which is what this was drawn for. Read off the
	   configuration rather than out of the record, there being nothing that reads one
	   back and no reason to have one. */
	j.value((unsigned long long) config().session_lifetime_s);
	j.endObject();
	return out;
}

const Param kLoginParams[] = {
	HTTPD_BODY_REQUIRED_TEXT("user", "the account, as the box knows it", 256),
	/* Bounded well under what the derivation would take on the slowest box here. The
	   stored form carries its own iteration count and the derivation is over a key of
	   fixed width whatever is offered, so the length of a guess buys an attacker nothing;
	   the bound keeps the one route reachable without a credential from being made to
	   carry four kilobytes per attempt. */
	HTTPD_BODY_REQUIRED_TEXT("password", "the password for it", 512),
};

/* Four routes and the one exception each of them is.

   Login is one of the two routes open to everybody, and it is where the check over these
   tables says it is: being reached grants nothing.

   The one that says what a request carries is the other, and for the same reason. Open to
   everybody rather than asking for a read, because a page that has lost its session has
   to be able to learn that it has.

   Logout asks for a write although what it removes is the caller's own session. A caller
   holding one is granted the level a session grants, which is above this; and a route
   that changed something at a read would be one any page a browser on the box's own
   network visits could reach in that browser's name.

   The fourth asks for System, which is the level of what it hands out, for the reason
   written where the handler is. */
const Endpoint kAuthEndpoints[] = {
	{ Method::Post, "/api/v1/login", AuthLevel::Public,
	  "opens a session for a name and a password, and says what that session carries and is worth",
	  HTTPD_PARAMS(kLoginParams), &kSessionStateSchema, &login, false },
	{ Method::Get, "/api/v1/session", AuthLevel::Public,
	  "says what the request asking carries: whether it holds a session, what it was granted, and the second token to send back with what it changes",
	  NULL, 0, &kSessionStateSchema, &sessionState, false },
	{ Method::Post, "/api/v1/logout", AuthLevel::Write,
	  "closes the session this request arrived with",
	  NULL, 0, NULL, &logout, false },
	{ Method::Post, "/api/v1/token/media", AuthLevel::System,
	  "draws a short lived token that reaches the media this box holds and nothing else, for a player that can carry a credential only in the address",
	  NULL, 0, &kScopedTokenSchema, &mintRecordingsToken, false },
};

} // namespace

extern const RouteTable authTable = {
	HTTPD_TABLE("auth", kAuthEndpoints)
};

} // namespace httpd
