/*
 * ep_webserver.cpp - routes for the webserver own configuration
 *
 * Copyright (C) 2026 NI-Team
 * Copyright (C) 2026 Tuxbox-Project (additions, fixes)
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
#include "httpd/endpoint.h"
#include "httpd/http.h"
#include "httpd/login_bridge.h"
#include "httpd/json.h"
#include "httpd/netmatch.h"
#include "httpd/router.h"
#include "httpd/schema.h"
#include "httpd/server.h"
#include "httpd/status.h"
#include "httpd/webconfig.h"

#include "coreapi/base/errors.h"

#include <cstddef>
#include <cstdio>
#include <string>
#include <vector>

#include <arpa/inet.h>
#include <sys/socket.h>

/* The server that answers this request, as a thing it can be asked about.

   A TABLE OF ITS OWN AND NOT A SECTION OF THE SETTINGS. Everything under
   /api/v1/settings comes out of neutrino.conf, which is one file with one reader and
   one set of rules; this is a different file with different rules, and it holds the
   one value in this box that decides whether any of the rest can be reached at all.

   WHAT THE ROUTES HERE OFFER AND WHAT THEY DO NOT. The configuration behind this
   server carries eleven things (webconfig.h), and nhttpd.conf the lines of nhttpd's
   own the settings page offers. Most are answered here, some can be written, and
   each of the rest is left out for a reason of its own:

     port, bind                   read and written, and written into nhttpd.conf and
                                  not into this server's own file: nhttpd binds them,
                                  and the menu at the box sets them in that file too,
                                  so a second copy anywhere else would be one that
                                  moves back. bind is an address of the first family,
                                  the only one nhttpd listens on as written.

     user, password               read and written. They are the account the box's
                                  own screen offers, kept in this server's own file.
                                  Whether the password in effect still waits to be
                                  replaced (one the boxes shipped with, or the first
                                  password the box drew for itself) is read beside
                                  them.

     page_login, no_login_prefix, read and written, and lines of nhttpd.conf the
     threading, send_all_types,   settings page has always offered: whether the pages
     override_directory,          ask for the login and of whom not, and how nhttpd
     hosted_directory             serves them. nhttpd reads them when it starts, so a
                                  change to one restarts it as a new port does.

     channel_logos                read and written, and a line of this server's own
                                  file. The two below it are read only because
                                  writing them shuts somebody out or breaks a caller
                                  the caller cannot see; neither is true of this one.
                                  It is a line of this file and not of neutrino.conf
                                  because it is about what this server serves and not
                                  about the television.

                                  What acts on it is the route that carries a picture
                                  (ep_channels.cpp) and not a page: this one is
                                  System, so a page answered at Read cannot learn the
                                  switch, and a switch only the owner can see is one
                                  that holds only for the owner.

     lan_read, trusted_proxies    read and not written. Read because they are the
                                  answer to who reaches this box without presenting
                                  anything. Not written because narrowing lan_read is
                                  the one change here that locks out somebody other
                                  than the caller, every unauthenticated reader on the
                                  network at once, and because a line of a file is
                                  owned by the save that writes it.

     legacy_enabled               read and not written, and the one that was argued
                                  over. Its own header calls it the switch a running
                                  box can turn off without being rebuilt. It is still
                                  only read here, for the writing reason above and for
                                  one of its own: what it switches off is the surface
                                  the plugins on this very box talk through, so turning
                                  it off remotely breaks callers the caller cannot see.
                                  What says whether that is safe is the count at
                                  /api/v1/system/legacy-usage, which is a read.

     session_lifetime_s           not offered. How long a session lasts is answered for
                                  the caller that holds one, by the route that answers
                                  about its own session. A second statement of it here
                                  would be one that can differ from the session in
                                  hand.

     sse_max_streams              not offered. A ceiling on what this box will carry at
                                  once, which nobody sets from outside.

     docroot                      not offered. It is a path on this box's disc, and the
                                  route that lists a directory is System precisely
                                  because a path is worth something to somebody looking
                                  for a way in.

     api_tokens                   not offered, in either direction. A token is a
                                  credential, and handing one back is the thing the
                                  password below is kept out of an answer to avoid.
                                  Taking one in would be worse: nothing in this product
                                  writes a stored form of a token, so a route that
                                  accepted one would put the credential itself into a
                                  file over the network.

   BOTH ROUTES ARE System. The four beside them that stop the box are System because a
   caller that can take the box off the air can stop every recording on it, and moving
   the port does exactly that from the network's side. The read is the same level
   rather than the Read its neighbours carry, because what it answers includes the
   networks this box lets read without a password. */

namespace httpd
{

namespace
{

/* One network as the line a file writes it on.

   Written from what was parsed and not from what was read, so the answer names the
   network this box compares against: "10.1.2.3/8" and "10.0.0.0/8" are one network
   here, and the second is the one that decides. A caller reading its own file back
   would otherwise see a line it did not write.

   The empty string for a prefix of neither family, and the caller below drops those
   rather than writing an empty line into a list of networks. */
std::string prefixText(const NetPrefix &p)
{
	if (p.family != AF_INET && p.family != AF_INET6)
		return std::string();

	char addr[INET6_ADDRSTRLEN];
	if (::inet_ntop(p.family, p.bits, addr, sizeof(addr)) == NULL)
		return std::string();

	char tail[16];
	std::snprintf(tail, sizeof(tail), "/%d", p.len);
	return std::string(addr) + tail;
}

void writePrefixes(Json &j, const char *name, const std::vector<NetPrefix> &all)
{
	j.key(name);
	j.beginArray();
	for (size_t i = 0; i < all.size(); ++i)
	{
		const std::string one = prefixText(all[i]);
		if (!one.empty())
			j.value(one);
	}
	j.endArray();
}

/* What the server is set to, as the members both routes answer it with.

   One writer for both, because the answer to a write says what the server is now set
   to and a caller has to be able to read that the same way it reads the answer to a
   read.

   port and the three beside it are handed in rather than read here, because the answer
   to a write is what the file now says and the running server is still on the old one
   at the moment that answer is built: nothing is put on the new configuration until the
   answer has gone out. Everything this route does not write is read from what is in
   effect, which for those keys is what the file says too. The options are what
   nhttpd.conf says, which is what nhttpd runs on: it reads them when it starts, and the
   menu at the box puts its own changes in effect as it writes them. */
void writeWebserver(Json &j, int port, const std::string &bind,
                    const std::string &user, bool has_password, bool logos,
                    const ServerOptions &o)
{
	const WebConfig &c = config();

	j.beginObject();
	j.key("port");
	j.value((unsigned long) ((port > 0) ? port : 0));
	j.key("bind");
	j.value(bind);
	/* The port that answers now, which is not the one above after the menu at the
	   box moved it and before anything restarted the server, nor where nhttpd
	   fell back from a port it could not take. Asked for and whether the server
	   is running is not: tearing the daemon down waits for the threads handlers
	   run on, and the bound port is kept apart for that reason (server.h). */
	j.key("bound_port");
	j.value((unsigned long) boundPort());
	j.key("restarts");
	j.value((unsigned long) startCount());
	j.key("user");
	j.value(user);
	/* Whether one is set and never which one. A stored form is what an offline
	   attack is mounted against, and a route that hands it out is a route that
	   supplies the material for an attack that could not have happened without
	   it. The plain value is held only while it is a first password, for the
	   screen at the box, and no route hands that out either. */
	j.key("has_password");
	j.value(has_password);
	/* Of the password in effect, so a page can say that the box still opens to a
	   login it shipped with or to its first password, under the name this member
	   had before the box drew first passwords. A password a write sets is not
	   judged in its answer: it is not in effect yet, and the read after the
	   restart says. */
	j.key("password_shipped");
	j.value(login_bridge::passwordOwed());
	j.key("page_login");
	j.value(o.page_login);
	j.key("no_login_prefix");
	j.value(o.no_login_prefix);
	writePrefixes(j, "lan_read", c.lan_read);
	writePrefixes(j, "trusted_proxies", c.trusted_proxies);
	j.key("legacy_enabled");
	j.value(c.legacy_enabled);
	j.key("channel_logos");
	j.value(logos);
	j.key("threading");
	j.value(o.threading);
	j.key("override_directory");
	j.value(o.override_directory);
	j.key("hosted_directory");
	j.value(o.hosted_directory);
	j.key("send_all_types");
	j.value(o.send_all_types);
	j.endObject();
}

const FieldDesc kWebserverFields[] = {
	HTTPD_MEMBER("port", FieldType::UInt,
		"the port nhttpd.conf names, which the server answers on from its next start, and now as well unless the menu at the box moved it since"),
	HTTPD_MEMBER("bound_port", FieldType::UInt,
		"the port the server is answering on now, 0 while nothing is bound; it differs from port after the menu at the box moved that and before a restart, and where nhttpd fell back from a port it could not take"),
	HTTPD_MEMBER("restarts", FieldType::UInt,
		"how many times the server was put on its configuration again since the box came up; a caller compares it across a write to see whether the restart it announced has happened"),
	HTTPD_MEMBER("bind", FieldType::String,
		"the address it listens on, and \"0.0.0.0\" for every address of the first family"),
	HTTPD_MEMBER("user", FieldType::String, "the one name a password is presented under"),
	HTTPD_MEMBER("has_password", FieldType::Bool,
		"whether a password is set at all, which is the whole of what is said about it: neither the password nor the form it is stored as is ever answered here"),
	HTTPD_MEMBER("password_shipped", FieldType::Bool,
		"whether the password in effect still waits to be replaced: one these boxes have shipped with, which anybody can look up, or the first password the box set for itself and showed on its screen; a password a write sets is judged by the read after the restart"),
	HTTPD_MEMBER("page_login", FieldType::Bool,
		"whether the pages ask for the login, a line of nhttpd.conf: with it off the pages are open, and a change made over this interface still asks for a sign in"),
	HTTPD_MEMBER("no_login_prefix", FieldType::String,
		"the start of an IPv4 address whose callers the pages never ask for the login, such as \"192.168.1.\", and empty for none; the box itself is never asked. It opens the pages and not this interface, whose reads without a password are lan_read"),
	HTTPD_LIST_OF_VALUES("lan_read", ElementType::String,
		"the networks a caller may read from without presenting anything, as this box compares them and not as a file happened to write them, and empty where nobody may"),
	HTTPD_LIST_OF_VALUES("trusted_proxies", ElementType::String,
		"the networks whose forwarded address is believed, as this box read them; empty where the file has no trusted_proxies line, and also where the line is empty or none of its entries is a network, which still keeps reading without a password off"),
	HTTPD_MEMBER("legacy_enabled", FieldType::Bool,
		"the line of the file meant to say whether the surface under /control/ is answered; nothing acts on it, so that surface is answered whatever it says, and this route does not write it"),
	HTTPD_MEMBER("channel_logos", FieldType::Bool,
		"whether this server hands a channel picture over at all: with it off the route that carries one answers not found, as it does for a channel that has none, so the switch holds for every caller and a page has nothing to read. The television is drawn from the box's own settings and is not touched by it"),
	HTTPD_MEMBER("threading", FieldType::Bool,
		"whether nhttpd answers each request on a thread of its own, a line of nhttpd.conf"),
	HTTPD_MEMBER("override_directory", FieldType::String,
		"a directory whose files are served in place of the pages' own of the same name, a line of nhttpd.conf"),
	HTTPD_MEMBER("hosted_directory", FieldType::String,
		"the directory served under /hosted/, a line of nhttpd.conf"),
	HTTPD_MEMBER("send_all_types", FieldType::Bool,
		"whether a file of a type nhttpd has no name for is sent all the same rather than refused, a line of nhttpd.conf"),
};

const Schema kWebserverSchema = { "webserver", HTTPD_FIELDS(kWebserverFields) };

/* Where the server answers and how nhttpd serves are what nhttpd.conf says, the
   same reading a write starts from, so a caller that writes back what it read
   writes back what the file says and not a port the menu at the box has moved
   away from since. */
Response webserverRead(const Request &)
{
	const WebConfig &c = config();
	const Login l = loginInEffect();
	const WebSettings now = currentWebSettings();

	Response out = okJson();
	Json j(out.body, 1024);
	writeWebserver(j, now.port, now.bind_address, l.username,
	               !l.password_hash.empty(), c.channel_logos, now.options);
	return out;
}

/* The host of an authority, with a port dropped if one is there.

   Spelled here and not shared with the one other reading of the same question in this
   tree (coreapi/streaming.cpp), because the two layers do not include one another:
   nothing in coreapi may reach into this directory, and a route here reaching into that
   file's private half would be reaching past the interface that layer states.

   Bracketed first, because an address of the second family is written with colons of
   its own and only the brackets say where it ends. Otherwise the last colon, and only
   when what follows it is a port, a name being allowed a colon and a number not. */
std::string hostOf(const std::string &authority)
{
	if (!authority.empty() && authority[0] == '[')
	{
		const std::string::size_type close = authority.find(']');
		if (close == std::string::npos)
			return authority;
		return authority.substr(0, close + 1);
	}

	const std::string::size_type colon = authority.rfind(':');
	if (colon == std::string::npos)
		return authority;
	for (std::string::size_type i = colon + 1; i < authority.size(); ++i)
	{
		if (authority[i] < '0' || authority[i] > '9')
			return authority;
	}
	return authority.substr(0, colon);
}

/* Where this caller reaches the server once it is back up, and empty for a request that
   named no authority to build one under.

   Built from the name the caller reached this box under and never from an address the
   box knows about itself: a box answers under as many names as there are ways to reach
   it, and the only one right for an address handed back is the one that caller used.
   Nothing is put in its place when there is none, because a name this side made up
   reads to a caller exactly like the one it gave.

   The port is always written out, even where it is the one a scheme would assume: this
   route's whole subject is that the assumption has just changed.

   WHAT IT CANNOT SAY is whether the address the server is about to listen on still
   covers this caller. A narrowed bind is the second of the four ways this route can
   lock somebody out, and the member beside this one that says which address it will
   listen on is where that is read. */
std::string addressAfter(const std::string &authority, int port)
{
	const std::string host = hostOf(authority);
	if (host.empty())
		return std::string();

	char tail[16];
	std::snprintf(tail, sizeof(tail), ":%d/", port);
	return "http://" + host + tail;
}

/* What the box said about a save it would not do, which is the only thing in
   this program that knows which value it was.

   The last line and not the whole list, and the same reading the screen at the
   box makes of it: the list is cleared as a save begins, so what is left after
   one that refused is that save's own account of itself. */
std::string whatWentWrong()
{
	const std::vector<std::string> &said = configProblems();
	if (said.empty())
		return "the configuration could not be written, and nothing was said about why";
	return said[said.size() - 1];
}

const FieldDesc kCallerFields[] = {
	HTTPD_MEMBER("address", FieldType::String,
		"where this caller reaches the server once it is back, built from the name it reached the box under with the new port written in, and empty where the request named none"),
	HTTPD_MEMBER("port_moved", FieldType::Bool,
		"whether the port changed, which is what decides whether an address already in a caller's hands is still one"),
	HTTPD_MEMBER("password_changed", FieldType::Bool,
		"whether the next sign in needs a different password; once the restart has put a new password or user name in effect, every session opened before it ends except the one of the caller that asked, which this answer gave new tokens"),
	HTTPD_MEMBER("restarting", FieldType::Bool,
		"whether the server is being put on what the file now says, which ends every connection it is holding, this one included, and false where the request asked for what was already in effect"),
	HTTPD_MEMBER("csrf", FieldType::String,
		"the second token of the session the caller holds from now on, where this change of the user name or the password gave the session it came with new tokens, the new cookie travelling in this answer's Set-Cookie and the old one naming nothing any more; empty where the request named neither or came without a session"),
};

const Schema kCallerSchema = { "webserver-caller", HTTPD_FIELDS(kCallerFields) };

const FieldDesc kChangedFields[] = {
	HTTPD_OBJECT("webserver", &kWebserverSchema,
		"how the server is set up now, which is the same document the read answers with"),
	HTTPD_OBJECT("caller", &kCallerSchema,
		"what the change means for whoever asked for it, said before it happens rather than after"),
};

const Schema kChangedSchema = { "webserver-change", HTTPD_FIELDS(kChangedFields) };

// What a change of the login without the password in effect is answered with.
const char kCurrentNeeded[] = "a new password or user name is set only with the current password, in current";

/* THE ORDER IS WRITE, ANSWER, MOVE, AND IT IS THE POINT OF THIS ROUTE.

   Putting the server on a new configuration stops the daemon. Done inside this handler
   it would take down the connection the handler is answering on, so the caller that
   asked to move the port would lose the answer saying where the box moved to, on a box
   it no longer knows the address of. So the file is written, the answer says what is
   about to happen, and the response carries the path the transport is to put the server
   on once those bytes have gone out (endpoint.h).

   A CHANGE THAT WAS NOT SAVED MOVES NOTHING. The only line that arms the move is below
   the save and is not reached when the save refused, which keeps the box from running
   until its next start on something its file does not say.

   A REQUEST THAT ASKS FOR WHAT IS ALREADY IN EFFECT MOVES NOTHING EITHER. The file is
   rewritten, which is harmless and tidies it, and the daemon is left alone: a restart
   for a change that is not one would be a way to take the box off the network for a
   moment by asking it politely for what it already has.

   A MEMBER LEFT OUT MEANS THE VALUE STAYS, and so does an empty one: the router reads an
   empty member as one left out, the way the screen at the box reads an untouched field,
   unless the member takes the empty string as a value of its own. no_login_prefix is
   that one (clearable): empty, it exempts nobody. */
Response webserverWrite(const Request &r)
{
	const std::string &path = configPath();
	if (path.empty())
	{
		/* Nothing on a box reaches this: the program names the file on the way
		   up and the read that names it is what records it. A case that has put
		   a policy in front of this without a file behind it does, and being
		   told so is better than a save into a name nobody chose. */
		return problemResponse(StatusInternalServerError,
				       coreapi::ErrorCode::WebserverNotConfigured,
				       "this server was never told which file its configuration is kept in, so there is none to write");
	}

	/* A new password or a new user name is set only by a caller that says the password
	   in effect, as PUT /api/v1/account/password asks for it: a session left open on
	   somebody else's screen is not a way of taking the box's login, and the same check
	   counts a wrong one against the address. Asked before the account's lock is
	   taken, being a key derivation, against the account read with it; where nothing
	   is asked the account is read here as well. Under the lock the request is refused
	   where that account is gone since, whichever way it was decided here, and where
	   what the save then changes needed the password after all. A box with no
	   password in effect has none to say, and the caller, holding System without a
	   sign-in, sets the first one. */
	const unsigned long long seen = accountRevision();
	const bool names_new_user = r.has("user") && r.asString("user") != currentWebSettings().username;
	const bool asks_current = (r.has("password") || names_new_user) && !loginInEffect().password_hash.empty();
	unsigned long long checked = seen;
	if (asks_current)
	{
		if (!r.has("current"))
			return problemResponse(StatusBadRequest, coreapi::ErrorCode::MissingParameter, kCurrentNeeded);
		Response refusal;
		if (!currentPasswordHolds(r, r.asString("current"), &checked, &refusal))
			return refusal;
	}

	/* From reading what is in effect to the save, shared with a change to the
	   account at the box and with load, so the two writers of this file cannot
	   interleave. Held to the end of the answer and released before the restart,
	   which runs once these bytes are gone and takes the lock for its read. */
	AccountLock account;

	/* A change to the account from a caller the gate let in before this lock was
	   taken. A reset at the box or another change landing in between ends the session
	   the request came with, or leaves the box waiting for a password of the owner's
	   own, and the request then writes nothing, answered as the gate answers either. */
	if (r.has("user") || r.has("password"))
	{
		if (!r.session().empty() && !sessionIsLive(r.session()))
			return problemResponse(StatusForbidden, coreapi::ErrorCode::NotPermitted, notPermittedDetail());
		if (accountRevision() != checked)
			return problemResponse(StatusForbidden, coreapi::ErrorCode::NotPermitted,
			                       "the account changed while the request was being checked");
		if (passwordChangeOwed())
			return problemResponse(StatusForbidden, coreapi::ErrorCode::PasswordChangeRequired,
			                       passwordChangeRequiredDetail());
	}

	const WebSettings before = currentWebSettings();
	WebSettings s = before;

	bool named = false;
	if (r.has("port"))
	{
		s.port = (int) r.asUInt("port");
		named = true;
	}
	if (r.has("bind"))
	{
		s.bind_address = r.asString("bind");
		named = true;
	}
	if (r.has("user"))
	{
		s.username = r.asString("user");
		named = true;
	}
	s.change_password = r.has("password");
	if (s.change_password)
	{
		s.password = r.asString("password");
		named = true;
		// the rules every writer of the password is held to
		std::string why;
		if (!s.password.empty() && !passwordAcceptable(s.password, &why))
			return problemResponse(StatusBadRequest, coreapi::ErrorCode::BadString, why);
	}
	if (r.has("channel_logos"))
	{
		s.channel_logos = r.asBool("channel_logos");
		named = true;
	}

	// The lines of nhttpd.conf beside the port and the address.
	if (r.has("page_login"))
	{
		s.options.page_login = r.asBool("page_login");
		named = true;
	}
	if (r.has("no_login_prefix"))
	{
		s.options.no_login_prefix = r.asString("no_login_prefix");
		named = true;
	}
	if (r.has("threading"))
	{
		s.options.threading = r.asBool("threading");
		named = true;
	}
	if (r.has("override_directory"))
	{
		s.options.override_directory = r.asString("override_directory");
		named = true;
	}
	if (r.has("hosted_directory"))
	{
		s.options.hosted_directory = r.asString("hosted_directory");
		named = true;
	}
	if (r.has("send_all_types"))
	{
		s.options.send_all_types = r.asBool("send_all_types");
		named = true;
	}

	/* A body naming nothing is turned down rather than answered. It changes no
	   value, so the only thing left for it to be is a way of asking the box to
	   restart its own server, and that is not what this route is. */
	if (!named)
	{
		return problemResponse(StatusBadRequest, coreapi::ErrorCode::MissingParameter,
				       "the body names nothing to change");
	}

	/* What the save changes, read under the lock: a new password or user name that the
	   look before it did not ask the password for, the name having moved meanwhile, is
	   not written without it. */
	if ((s.change_password || s.username != before.username) && !asks_current &&
	    !loginInEffect().password_hash.empty())
		return problemResponse(StatusBadRequest, coreapi::ErrorCode::MissingParameter, kCurrentNeeded);

	/* Every rule about these values is the save's, which is where the files
	   they are written into are understood: what a port may be, what address
	   nhttpd can listen on, what a name may carry, and what an empty password
	   verifies against. A second copy of any of them here would be a rule that
	   can disagree with the one that decides. */
	/* From what this request started from, so a value the menu at the box wrote
	   into nhttpd.conf since then stays unless this request names it. */
	if (!saveWebSettings(path, s, NULL, &before))
	{
		/* Answered as the caller's request being wrong, because the values the
		   save was handed all came out of this request and one of them is what
		   it refused in every case but one. The exception is a file itself
		   being unwritable, and the line carried back names the file when that
		   is what happened, which is the one thing that tells a caller it is
		   not its own fault. */
		return problemResponse(StatusBadRequest, coreapi::ErrorCode::WebserverNotConfigured,
				       whatWentWrong());
	}

	/* A new name or password ends every session once the restart puts it in effect,
	   except the one of the caller that asked, which the page goes on with. That one
	   gets new tokens now, under the account still in effect, and is the one the
	   restart's read carries across; the cookie the request came with names nothing from
	   here on, so a copy of it is signed out at once. A caller let in by a token holds
	   none, and then nobody's stays. */
	std::string successor;
	if (s.change_password || s.username != before.username)
	{
		if (sessionIsLive(r.session()))
		{
			const unsigned long long now = accountRevision();
			successor = reissueSession(r.session(), now, now);
			/* Held until the restart's read carries it across: that read publishes the
			   new account first, and a walk of the table in between would drop the
			   session for being opened under the old one. */
			(void) holdSession(successor, now);
		}
		keepSessionAtLoad(successor);
	}

	/* Measured against the server as it runs and not against `before`, which is
	   where a save waiting for its restart left the file: the caller is told what
	   the coming restart does to it, that earlier save's part included. */
	const WebSettings effective = settingsInEffect();
	const bool port_moved = s.port != effective.port;
	const bool password_changed = s.change_password || newPasswordAwaitsLoad();
	/* The logo switch is in here with the four that decide where the server answers,
	   although nothing about it needs a socket rebound, and the reason is the one
	   arrangement this file rests on: what is in effect is read without a lock and is
	   only ever replaced while the daemon is down. A change written to the file and not
	   put in effect would leave the file saying one thing and the server answering
	   another until the next start. */
	const bool moved = port_moved
		|| s.bind_address != effective.bind_address
		|| s.username != effective.username
		|| password_changed
		|| s.channel_logos != effective.channel_logos
		/* Against what nhttpd.conf said and not against what is in effect, which this
		   layer does not hold for these: nhttpd reads them when it starts, and the
		   menu puts its own changes in effect as it writes them. */
		|| !sameServerOptions(s.options, before.options);
	const bool restarting = moved || restartAwaitsLoad();

	Response out = okJson();
	Json j(out.body, 1280);
	j.beginObject();
	j.key("webserver");
	/* What the file now says and not what is in effect, the two being different
	   things for as long as this answer takes: nothing is put on the new
	   configuration until these bytes have gone. Everything this route does not
	   write is read from what is in effect, which is what the file says too,
	   every line a save does not own reaching the new file as it stood. */
	writeWebserver(j, s.port, s.bind_address, s.username,
		       s.change_password || !loginInEffect().password_hash.empty(),
		       s.channel_logos, s.options);
	j.key("caller");
	j.beginObject();
	j.key("address");
	j.value(addressAfter(r.host(), s.port));
	j.key("port_moved");
	j.value(port_moved);
	j.key("password_changed");
	j.value(password_changed);
	j.key("restarting");
	j.value(restarting);
	j.key("csrf");
	j.value(successor.empty() ? std::string() : csrfFor(successor));
	j.endObject();
	j.endObject();
	if (!successor.empty())
		out.headers.push_back(std::make_pair(std::string("Set-Cookie"), sessionSetCookie(successor)));

	// The last lines of the handler, and the only ones that arm anything.
	if (moved)
	{
		out.reload_after = path;
		armRestart();
	}
	return out;
}

const Param kWriteParams[] = {
	/* Every one of them may be left out, because a caller changing a port has no
	   business restating a user name it was not asked about: a route that required all
	   four would have every caller read the four back and write them again, and a
	   caller that got one of them stale would write it back stale.

	   The bounds are the same numbers the save refuses outside of, and they are here as
	   well because a reader of the document is owed that before it sends one. Where
	   they are the same rule said twice, the save is the one that decides. */
	HTTPD_BODY_IN("port", ParamType::UInt,
		"the TCP port to answer on once the server restarts on this file, from 1 to 65535", 1, 65535),
	HTTPD_BODY_TEXT("bind", "the address to listen on once the server restarts, a literal of the first family, and \"0.0.0.0\" for every one of them", 15),
	HTTPD_BODY_TEXT("user", "the one name a password is presented under", 64),
	HTTPD_BODY_TEXT("current", "the password in effect, the one the caller signs in with, as it is typed: a new password and a new user name are set only with it, unless the box has no password at all; read only together with password or a user that differs from the one there", 512),
	/* The password as somebody types it, and never the form it is stored as.
	   What becomes of it is credentials.h's, and a route taking a stored form
	   would let whoever stole one put it back without ever knowing the password
	   it was made from. Left out means the password stays; there is nothing to
	   read back that would make any other reading possible. */
	HTTPD_BODY_TEXT("password", "a new password, as it is typed and never as it is stored: at least 8 characters, no control characters and none of the passwords these boxes have shipped with; left out where it is to stay as it is", 256),
	/* The one member here that cannot lock anybody out, and the only reason it
	   restarts the server at all is that this file replaces what is in effect
	   only while the daemon is down. */
	HTTPD_BODY("channel_logos", ParamType::Bool,
		"whether the pages are to show a picture beside a channel"),
	/* The lines of nhttpd.conf the settings page offers besides where the server
	   answers. What each may be is the save's to judge, as for the four above. */
	HTTPD_BODY("page_login", ParamType::Bool,
		"whether the pages are to ask for the login"),
	/* Empty is a value here, the one that exempts nobody, so a caller can take
	   a prefix away again. */
	HTTPD_BODY_TEXT_CLEARABLE("no_login_prefix", "the start of an IPv4 address in dotted decimal whose callers the pages are never to ask for the login, such as \"192.168.1.\", and empty for nobody", 15),
	HTTPD_BODY("threading", ParamType::Bool,
		"whether nhttpd is to answer each request on a thread of its own"),
	HTTPD_BODY_TEXT("override_directory", "an absolute path whose files are to be served in place of the pages' own of the same name", 255),
	HTTPD_BODY_TEXT("hosted_directory", "an absolute path to serve under /hosted/", 255),
	HTTPD_BODY("send_all_types", ParamType::Bool,
		"whether a file of a type nhttpd has no name for is to be sent all the same"),
};

const RouteRefusal kWebserverWriteRefusals[] = {
	HTTPD_REFUSES(InvalidArgument, MissingParameter,
		"the body names nothing to change"),
	HTTPD_REFUSES_AS(403, WrongCurrentPassword,
		"the current password is not the one in effect"),
	HTTPD_REFUSES_AS(403, NotPermitted,
		"the account changed while the request was being checked"),
	HTTPD_REFUSES_AS(429, TooManyAttempts,
		"too many attempts are being made here; come back in a moment"),
	HTTPD_REFUSES_AS(403, PasswordChangeRequired,
		"this box still opens to a password it shipped with or to the first password it set for itself: set a password of your own first, with PUT /api/v1/account/password"),
	HTTPD_REFUSES(InvalidArgument, WebserverNotConfigured,
		"WebsiteMain.host: \"::1\" is not an address this server can listen on, and nothing is written. It listens on addresses of the first family only, and every interface is written as \"0.0.0.0\""),
};

const Endpoint kWebserverEndpoints[] = {
	{ Method::Get, "/api/v1/system/webserver", AuthLevel::System,
	  "how this server is set up: where it answers, who may tell it anything, and who reads from it without presenting anything",
	  "Reads how this HTTP server itself is set up: the port and address it answers on, the account a "
	  "caller signs in under, whether a password is set at all and whether it still waits to be replaced "
	  "(one the boxes shipped with, or the first password the box set for itself), whether the pages ask for the login and of which callers not, how nhttpd serves the pages "
	  "(threads, the override and hosted directories, files of unknown type), the networks that may read without "
	  "presenting anything and the proxies whose forwarded address is believed, the file's "
	  "`legacy_enabled` line, which nothing acts on (the surface under `/control/` is answered whatever "
	  "it says), and whether a channel picture is served at all. This is "
	  "a different file from `neutrino.conf`, which `/api/v1/settings` reads and writes.\n"
	  "\n"
	  "**Related:** `PUT /api/v1/system/webserver`.",
	  NULL, 0, &kWebserverSchema, &webserverRead, false,
	  Answers200, HTTPD_NO_REFUSALS },
	{ Method::Put, "/api/v1/system/webserver", AuthLevel::System,
	  "sets this server up, naming only what is to change, and answers what the change means for the caller before the server is put on it",
	  "Changes how this HTTP server itself is set up, naming only the members to change; a member left "
	  "out keeps its stored value, and so does one sent as an empty string, except `no_login_prefix`, "
	  "where an empty string is the value that exempts nobody. A new `password` and a new `user` are "
	  "set only together with `current`, the password in effect, the one the caller signs in with, "
	  "checked as `PUT /api/v1/account/password` checks it and counted against the address like a "
	  "wrong login when it is wrong; a box with no password at all needs none. The file "
	  "is written first, then this "
	  "answer is sent, and only after it has gone out does "
	  "the server, if anything that needs one changed, restart on the new file; a caller that moved the "
	  "port or the bind address therefore still receives this answer on the connection it asked over.\n"
	  "\n"
	  "The answer carries `webserver`, the configuration as the file now reads, and `caller`, what the "
	  "change means for whoever asked: `address` is where to reach the server afterward, built from "
	  "the name this request reached the box under and the new port, and empty when the request named "
	  "no host; `port_moved` says whether the port changed; `password_changed` says whether the next "
	  "sign in needs a different password, and once the restart has put a new password or user name in "
	  "effect every session opened before it ends, except the one this request arrived with, and so does "
	  "every token drawn from a session; that session is given new tokens by this answer already, the "
	  "cookie in `Set-Cookie` and the second token in `csrf`, and the cookie the request came with names "
	  "nothing from then on, so a copy of it is signed out at once; `restarting` "
	  "says whether anything changed enough to restart the server at all, since a request asking for "
	  "what is already in effect changes nothing and restarts nothing.\n"
	  "\n"
	  "**Side effects:** rewrites this server's own configuration file, and `nhttpd.conf` where `port`, "
	  "`bind` or another member kept there differs from what that file says, that file being where the "
	  "server answers and how nhttpd serves. When any member actually changed, the server is restarted "
	  "on the new files once the answer has been sent, which drops every connection it is holding, this "
	  "one included. Where nothing comes up on a new port or address, it goes back to the one it "
	  "answered on before.\n"
	  "\n"
	  "**Refusals:**\n"
	  "- `400 missing-parameter`: the body names nothing to change, or it sets `password` or a new "
	  "`user` without `current` while the box has a password.\n"
	  "- `400 webserver-not-configured`: a value given could not be written, for instance a `bind` "
	  "that is not an address literal of the first family, the only one this server listens on, a "
	  "`no_login_prefix` that is not the start of an IPv4 address, a directory that is not an absolute path, or a "
	  "configuration file itself could not be written; the detail names which.\n"
	  "- `403 wrong-current-password`: `current` is not the password in effect.\n"
	  "- `403 not-permitted`: the request names `user` or `password` and the session it came with "
	  "ended after the gate let it in, by a reset at the box, another change of the login or a "
	  "logout, or the account changed after the request was checked; nothing is written.\n"
	  "- `403 password-change-required`: the request names `user` or `password`, and since the gate "
	  "let it in the box has come to wait for a password of the owner's own, as after the reset at "
	  "the box; nothing is written.\n"
	  "- `429 too-many-attempts`: wrong current passwords were sent from this address too often, too "
	  "many are being checked at once, or too many wrong ones came from every address together; "
	  "`Retry-After` says how long to wait.\n"
	  "\n"
	  "**Related:** `GET /api/v1/system/webserver`, `GET /api/v1/system/legacy-usage`.",
	  HTTPD_PARAMS(kWriteParams), &kChangedSchema, &webserverWrite, false,
	  Answers200, HTTPD_REFUSALS_AND_BODY(kWebserverWriteRefusals,
		"{\"port\":8081,\"bind\":\"0.0.0.0\",\"channel_logos\":true}") },
};

} // namespace

extern const RouteTable webserverTable = {
	HTTPD_TABLE("webserver", kWebserverEndpoints)
};

} // namespace httpd
