/*
 * ep_webserver.cpp - routes for the webserver own configuration
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

#include "httpd/endpoint.h"
#include "httpd/http.h"
#include "httpd/json.h"
#include "httpd/netmatch.h"
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
   server carries eleven things (webconfig.h). Seven are answered here, four can be
   written, and each of the rest is left out for a reason of its own:

     port, bind, user, password   read and written. They are the four the box's own
                                  screen offers and four of the five lines the save
                                  this stands on owns.

     channel_logos                read and written, and the fifth line that save
                                  owns. The two below it are read only because
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

/* The port a caller reaches this box on, which is not always the port the
   configuration asked for: a file naming nought asks the kernel to choose, and what it
   chose is the only number that is any use to whoever is asking. The configured one
   answers while nothing is bound.

   The bound port is asked for and whether the server is running is not, which is not a
   shortening. A handler runs on a thread the daemon owns and tearing the daemon down
   waits for those threads, so a handler asking a question that a shutdown holds the
   answer to is a handler waiting for something waiting for it. The bound port is kept
   under a lock of its own for exactly that reason (server.cpp) and reads as none while
   nothing is bound. */
int answeringPort()
{
	const int bound = boundPort();
	if (bound > 0)
		return bound;
	return config().server.port;
}

/* What the server is set to, as the seven members both routes answer it with.

   One writer for both, because the answer to a write says what the server is now set
   to and a caller has to be able to read that the same way it reads the answer to a
   read.

   port and the three beside it are handed in rather than read here, because the answer
   to a write is what the file now says and the running server is still on the old one
   at the moment that answer is built: nothing is put on the new configuration until the
   answer has gone out. Everything this route does not write is read from what is in
   effect, which for those keys is what the file says too. */
void writeWebserver(Json &j, int port, const std::string &bind,
                    const std::string &user, bool has_password, bool logos)
{
	const WebConfig &c = config();

	j.beginObject();
	j.key("port");
	j.value((unsigned long) ((port > 0) ? port : 0));
	j.key("bind");
	j.value(bind);
	j.key("user");
	j.value(user);
	/* Whether one is set and never which one. A stored form is what an offline
	   attack is mounted against, and a route that hands it out is a route that
	   supplies the material for an attack that could not have happened without
	   it. The plain value is not in this process to hand out in the first
	   place. */
	j.key("has_password");
	j.value(has_password);
	writePrefixes(j, "lan_read", c.lan_read);
	writePrefixes(j, "trusted_proxies", c.trusted_proxies);
	j.key("legacy_enabled");
	j.value(c.legacy_enabled);
	j.key("channel_logos");
	j.value(logos);
	j.endObject();
}

const FieldDesc kWebserverFields[] = {
	HTTPD_MEMBER("port", FieldType::UInt,
		"the port this server is answering on, which is the one the kernel chose where the configuration asked for none"),
	HTTPD_MEMBER("bind", FieldType::String,
		"the address it listens on, and \"0.0.0.0\" for every address of the first family"),
	HTTPD_MEMBER("user", FieldType::String, "the one name a password is presented under"),
	HTTPD_MEMBER("has_password", FieldType::Bool,
		"whether a password is set at all, which is the whole of what is said about it: neither the password nor the form it is stored as ever leaves the box"),
	HTTPD_LIST_OF_VALUES("lan_read", ElementType::String,
		"the networks a caller may read from without presenting anything, as this box compares them and not as a file happened to write them, and empty where nobody may"),
	HTTPD_LIST_OF_VALUES("trusted_proxies", ElementType::String,
		"the networks whose forwarded address is believed, and empty where nothing in front of this server is configured"),
	HTTPD_MEMBER("legacy_enabled", FieldType::Bool,
		"whether the copied surface under /control/ is answered at all, which is a line of the file and not something this route writes"),
	HTTPD_MEMBER("channel_logos", FieldType::Bool,
		"whether this server hands a channel picture over at all: with it off the route that carries one answers as it does for a channel that has none, so the switch holds for every caller and a page has nothing to read. The television is drawn from the box's own settings and is not touched by it"),
};

const Schema kWebserverSchema = { "webserver", HTTPD_FIELDS(kWebserverFields) };

Response webserverRead(const Request &)
{
	const WebConfig &c = config();

	Response out = okJson();
	Json j(out.body, 512);
	writeWebserver(j, answeringPort(), c.server.bind_address, c.username,
	               !c.password_hash.empty(), c.channel_logos);
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
		"whether the next sign in needs a different password; a session already open is not ended by it, the program not being restarted"),
	HTTPD_MEMBER("restarting", FieldType::Bool,
		"whether the server is being put on what the file now says, which ends every connection it is holding, this one included, and false where the request asked for what was already in effect"),
};

const Schema kCallerSchema = { "webserver-caller", HTTPD_FIELDS(kCallerFields) };

const FieldDesc kChangedFields[] = {
	HTTPD_OBJECT("webserver", &kWebserverSchema,
		"how the server is set up now, which is the same document the read answers with"),
	HTTPD_OBJECT("caller", &kCallerSchema,
		"what the change means for whoever asked for it, said before it happens rather than after"),
};

const Schema kChangedSchema = { "webserver-change", HTTPD_FIELDS(kChangedFields) };

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

   A MEMBER LEFT OUT MEANS THE VALUE STAYS, and an empty member does not. This is where
   the route and the screen at the box differ and it is deliberate: the screen opens its
   password field empty because there is nothing to put in one, so an untouched field
   there has to mean the stored form stays. A body is not a form. What it names is what
   the caller means to write, and an empty string is something the caller wrote. */
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
	}
	if (r.has("channel_logos"))
	{
		s.channel_logos = r.asBool("channel_logos");
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

	/* Every rule about these four values is the save's, which is where the file
	   they are written into is understood: what a port may be, what an address
	   literal is, what a name may carry, and what an empty password verifies
	   against. A second copy of any of them here would be a rule that can
	   disagree with the one that decides. */
	if (!saveWebSettings(path, s))
	{
		/* Answered as the caller's request being wrong, because the four values
		   the save was handed all came out of this request and one of them is
		   what it refused in every case but one. The exception is the file
		   itself being unwritable, and the line carried back names the file
		   when that is what happened, which is the one thing that tells a
		   caller it is not its own fault. */
		return problemResponse(StatusBadRequest, coreapi::ErrorCode::WebserverNotConfigured,
				       whatWentWrong());
	}

	const bool port_moved = s.port != before.port;
	/* The logo switch is in here with the four that decide where the server answers,
	   although nothing about it needs a socket rebound, and the reason is the one
	   arrangement this file rests on: what is in effect is read without a lock and is
	   only ever replaced while the daemon is down. A change written to the file and not
	   put in effect would leave the file saying one thing and the server answering
	   another until the next start. */
	const bool moved = port_moved
		|| s.bind_address != before.bind_address
		|| s.username != before.username
		|| s.change_password
		|| s.channel_logos != before.channel_logos;

	Response out = okJson();
	Json j(out.body, 640);
	j.beginObject();
	j.key("webserver");
	/* What the file now says and not what is in effect, the two being different
	   things for as long as this answer takes: nothing is put on the new
	   configuration until these bytes have gone. Everything this route does not
	   write is read from what is in effect, which is what the file says too,
	   every line a save does not own reaching the new file as it stood. */
	writeWebserver(j, s.port, s.bind_address, s.username,
		       s.change_password || !config().password_hash.empty(),
		       s.channel_logos);
	j.key("caller");
	j.beginObject();
	j.key("address");
	j.value(addressAfter(r.host(), s.port));
	j.key("port_moved");
	j.value(port_moved);
	j.key("password_changed");
	j.value(s.change_password);
	j.key("restarting");
	j.value(moved);
	j.endObject();
	j.endObject();

	// The last line of the handler, and the only one that arms anything.
	if (moved)
		out.reload_after = path;
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
	HTTPD_BODY_IN("port", ParamType::UInt, "the port to answer on, from 1 to 65535", 1, 65535),
	HTTPD_BODY_TEXT("bind", "the address to listen on, a literal of either family, and \"0.0.0.0\" for every address of the first", 45),
	HTTPD_BODY_TEXT("user", "the one name a password is presented under", 64),
	/* The password as somebody types it, and never the form it is stored as.
	   What becomes of it is credentials.h's, and a route taking a stored form
	   would let whoever stole one put it back without ever knowing the password
	   it was made from. Left out means the password stays; there is nothing to
	   read back that would make any other reading possible. */
	HTTPD_BODY_TEXT("password", "a new password, as it is typed and never as it is stored, and left out where it is to stay as it is", 256),
	/* The one member here that cannot lock anybody out, and the only reason it
	   restarts the server at all is that this file replaces what is in effect
	   only while the daemon is down. */
	HTTPD_BODY("channel_logos", ParamType::Bool,
		"whether the pages are to show a picture beside a channel"),
};

const Endpoint kWebserverEndpoints[] = {
	{ Method::Get, "/api/v1/system/webserver", AuthLevel::System,
	  "how this server is set up: where it answers, who may tell it anything, and who reads from it without presenting anything",
	  NULL, 0, &kWebserverSchema, &webserverRead, false },
	{ Method::Put, "/api/v1/system/webserver", AuthLevel::System,
	  "sets this server up, naming only what is to change, and answers what the change means for the caller before the server is put on it",
	  HTTPD_PARAMS(kWriteParams), &kChangedSchema, &webserverWrite, false },
};

} // namespace

extern const RouteTable webserverTable = {
	HTTPD_TABLE("webserver", kWebserverEndpoints)
};

} // namespace httpd
