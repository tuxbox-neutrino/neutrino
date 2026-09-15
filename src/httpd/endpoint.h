/*
 * endpoint.h - one route: its method, path, level and handler
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

#ifndef __httpd_endpoint_h__
#define __httpd_endpoint_h__

#include "http.h"

#include <cstddef>
#include <string>
#include <utility>
#include <vector>

#include <stdint.h>
#include <time.h>

namespace httpd
{

/* The shape of what an endpoint answers with. Only a pointer to one is held here
   and the type itself is declared where responses are described, so a table of
   routes does not carry a response concern to compile and a route can exist before
   its answer has been described. */
struct Schema;

/* What a parameter carries, which is what decides how the text off the wire is
   read and what is refused before a handler is entered. */
enum class ParamType
{
	Int,
	UInt,
	Bool,
	String,
	Enum,
	ChannelId,
	Time
};

/* What a caller has to be to reach an endpoint. The order of the members is the
   order of privilege and is read as such, so a level answers for every level below
   it and a comparison is the whole of the check. */
enum class AuthLevel
{
	Public,
	Read,
	Write,
	System
};

/* Where the value is carried. A body is one flat JSON object, and its members
   reach a handler through the same reader a value off the query goes through. Only
   for the methods written with a body: a route declaring one on a method that
   carries none is refused where tables are checked, because such a value would
   never be extracted and a required one never missed. */
enum class In
{
	Path,
	Query,
	// One member of that object, named here and bound by the router.
	Body,
	/* The body itself rather than a member of it, in the three shapes a body takes
	   that no list of named members can state. BodyList says the body is one JSON
	   array and the row names what one element is; BodyMap says it is one JSON object
	   whose member names the table cannot state, and the row names what one value is;
	   BodyBytes says the body is not a document at all but the bytes themselves.

	   A row of either kind is not a value the router handles: nothing on the wire
	   arrives under its name, so the router binds and checks nothing and the handler
	   reads the body itself. What the row is for is what the document tells a caller
	   to send; before these existed the document described no body at all for the two
	   routes that read their own. min and max count what the body carries rather than
	   bounding any one value, and on BodyBytes what is counted is bytes. */
	BodyList,
	BodyMap,
	BodyBytes
};

/* Whether a row names the whole of a body rather than a value carried in one. One
   function rather than the same pair of comparisons written out at each site: the
   router's two loops, the check over the tables and the two places the document is
   written from all have to agree about which rows are which, and five copies of a
   comparison agree right up until one of them is edited. */
inline bool namesWholeBody(In in)
{
	return in == In::BodyList || in == In::BodyMap || in == In::BodyBytes;
}

/* One declared parameter. An aggregate with no constructor of its own, so a table of
   these is a constant in read only memory and nothing has to have run before main for
   a route to exist.

   min and max bound a number and both zero means unbounded: a row that needs a bound
   has one end away from zero, and a pair of zeroes read as a bound would refuse every
   value but zero. They are long, four bytes where this runs, so a value wider than
   that is answered for by its type.

   choices answers the set of a row whose set the build decides, and is NULL for every
   other row. A table cannot state such a set: the sections the settings are laid out
   in come out of tables with rows this box's model decides, so a list typed here would
   be quietly wrong for another box. It is what a reader is told and not what the router
   checks against; the set and the refusal are the layer below's. */
typedef void (*Choices)(std::vector<std::string> &out);

struct Param
{
	const char *name;
	ParamType   type;
	In          in;
	bool        required;
	const char *doc;
	long        min;
	long        max;       // for a String, the byte ceiling when above zero
	const char *values;    // comma separated, Enum only
	Choices     choices;   // a set this table cannot state, asked for instead
};

/* Writes a table's parameter array and its count as one pair, so the two cannot
   disagree: a count one too large reads a Param that is not there, which nothing
   at run time can see. */
#define HTTPD_PARAMS(a) (a), (sizeof(a) / sizeof((a)[0]))

/* One declared parameter, written as what that parameter is.

   Same reason the field array carries its own count: a row spelled out field by field
   is a row every later field of Param has to be added to by hand, a hundred and twenty
   six times.

   What a row says is three things and the name says all three: where the value is
   carried, whether a request may leave it out, and what narrows it. A segment the path
   binds is always there, so there is no form of it that says otherwise.

   IN bounds a number and both ends are read. TEXT is the byte ceiling of a string, a
   different thing said with the same field, and naming the two apart is what keeps a
   row from claiming a range on a value that has none. FROM_SET names the values, and
   the type follows from the macro. */
#define HTTPD_SEGMENT(name, type, doc) \
	(name), (type), httpd::In::Path, true, (doc), 0, 0, NULL, NULL
#define HTTPD_SEGMENT_IN(name, type, doc, lo, hi) \
	(name), (type), httpd::In::Path, true, (doc), (lo), (hi), NULL, NULL
#define HTTPD_SEGMENT_TEXT(name, doc, bytes) \
	(name), httpd::ParamType::String, httpd::In::Path, true, (doc), 0, (bytes), NULL, NULL
#define HTTPD_SEGMENT_FROM_SET(name, doc, set) \
	(name), httpd::ParamType::Enum, httpd::In::Path, true, (doc), 0, 0, (set), NULL

/* A segment out of a set this table cannot state, asked for where the document is
   written. Text and not one of a set as far as the router is concerned: the set is
   the layer below's, so is the refusal for a name outside it, and a second copy
   here would be a rule that can disagree with the one that decides. What this adds
   is that a reader and a generated client are handed the names this box has. */
#define HTTPD_SEGMENT_FROM_ASKED_SET(name, doc, asks) \
	(name), httpd::ParamType::String, httpd::In::Path, true, (doc), 0, 0, NULL, (asks)

// The same, for a segment whose refusal quotes the name back, which is what a byte
// ceiling on it is for: the ceiling keeps a caller from choosing how long its own
// words come back to it.
#define HTTPD_SEGMENT_TEXT_FROM_ASKED_SET(name, doc, bytes, asks) \
	(name), httpd::ParamType::String, httpd::In::Path, true, (doc), 0, (bytes), NULL, (asks)

#define HTTPD_QUERY(name, type, doc) \
	(name), (type), httpd::In::Query, false, (doc), 0, 0, NULL, NULL
#define HTTPD_QUERY_IN(name, type, doc, lo, hi) \
	(name), (type), httpd::In::Query, false, (doc), (lo), (hi), NULL, NULL
#define HTTPD_QUERY_TEXT(name, doc, bytes) \
	(name), httpd::ParamType::String, httpd::In::Query, false, (doc), 0, (bytes), NULL, NULL
#define HTTPD_QUERY_FROM_SET(name, doc, set) \
	(name), httpd::ParamType::Enum, httpd::In::Query, false, (doc), 0, 0, (set), NULL
#define HTTPD_QUERY_REQUIRED(name, type, doc) \
	(name), (type), httpd::In::Query, true, (doc), 0, 0, NULL, NULL
#define HTTPD_QUERY_REQUIRED_IN(name, type, doc, lo, hi) \
	(name), (type), httpd::In::Query, true, (doc), (lo), (hi), NULL, NULL
#define HTTPD_QUERY_REQUIRED_FROM_SET(name, doc, set) \
	(name), httpd::ParamType::Enum, httpd::In::Query, true, (doc), 0, 0, (set), NULL

#define HTTPD_BODY(name, type, doc) \
	(name), (type), httpd::In::Body, false, (doc), 0, 0, NULL, NULL
#define HTTPD_BODY_IN(name, type, doc, lo, hi) \
	(name), (type), httpd::In::Body, false, (doc), (lo), (hi), NULL, NULL
#define HTTPD_BODY_TEXT(name, doc, bytes) \
	(name), httpd::ParamType::String, httpd::In::Body, false, (doc), 0, (bytes), NULL, NULL
#define HTTPD_BODY_FROM_SET(name, doc, set) \
	(name), httpd::ParamType::Enum, httpd::In::Body, false, (doc), 0, 0, (set), NULL
#define HTTPD_BODY_REQUIRED(name, type, doc) \
	(name), (type), httpd::In::Body, true, (doc), 0, 0, NULL, NULL
#define HTTPD_BODY_REQUIRED_IN(name, type, doc, lo, hi) \
	(name), (type), httpd::In::Body, true, (doc), (lo), (hi), NULL, NULL
#define HTTPD_BODY_REQUIRED_TEXT(name, doc, bytes) \
	(name), httpd::ParamType::String, httpd::In::Body, true, (doc), 0, (bytes), NULL, NULL
#define HTTPD_BODY_REQUIRED_FROM_SET(name, doc, set) \
	(name), httpd::ParamType::Enum, httpd::In::Body, true, (doc), 0, 0, (set), NULL

/* The body itself and not a member of it, in the two shapes no list of named members
   can state. The kind names what one element or one value is, and the two numbers
   count what the body carries rather than bounding anything in it.

   Required with no form that says otherwise, because on these two the body is the
   request and the readers both use turn an absent body down first. A body of none is a
   different thing, said with a floor of nought.

   Named for what the body is and not for the row: reaching for the wrong one writes an
   array where a caller has to send an object. */
#define HTTPD_BODY_IS_LIST_OF(name, type, doc, least, most) \
	(name), (type), httpd::In::BodyList, true, (doc), (least), (most), NULL, NULL
#define HTTPD_BODY_IS_MAP_OF(name, type, doc, least, most) \
	(name), (type), httpd::In::BodyMap, true, (doc), (least), (most), NULL, NULL

/* The third shape: the body is the bytes and not a document of any kind, which is
   what a route carrying a file takes. The two numbers count bytes. The kind is
   written String because that is what a body of bytes is answered for by
   everything that describes one, and because nothing reads a kind off such a
   row. */
#define HTTPD_BODY_IS_BYTES(name, doc, least, most) \
	(name), httpd::ParamType::String, httpd::In::BodyBytes, true, (doc), (least), (most), NULL, NULL

/* A row spelled out field by field, for a case about the row itself: a bound on a
   type that reads none, a set beside a type that is not one of a set, a segment
   that says a request may leave it out. Every one of those is a row the macros
   above cannot write, which is the point of them, and a case that checks such a
   row is refused has to be able to write one.

   Nothing that means its row to be right should reach for this. */
#define HTTPD_PARAM_AS_WRITTEN(name, type, in, required, doc, lo, hi, set) \
	(name), (type), (in), (required), (doc), (lo), (hi), (set), NULL

/* A value the router has read off the wire and checked against the row that
   declares it. The member the declared type names carries it and the rest are zero,
   so nothing here has to be read to find out which of them is the answer. */
struct Value
{
	ParamType          type;
	std::string        text;      // as it arrived, once decoded
	long long          number;    // Int and Time
	unsigned long long unumber;   // UInt and ChannelId
	bool               flag;      // Bool

	Value() : type(ParamType::String), number(0), unumber(0), flag(false) {}
};

/* What a handler is given. Built by the router out of a request already checked against
   the declaration above, so a value that reaches here has been decoded, parsed and
   found inside its declared bounds.

   Reading a name the endpoint does not declare answers an empty value rather than
   ending the process. The fault this replaces was a handler reading a parameter that
   was not there and taking the program down with it. */
class Request
{
	public:
		Request();

		bool has(const char *name) const;

		long          asInt(const char *name) const;
		unsigned long asUInt(const char *name) const;
		bool          asBool(const char *name) const;
		uint64_t      asChannelId(const char *name) const;
		time_t        asTime(const char *name) const;
		const std::string &asString(const char *name) const;

		/* The bytes of the body as they arrived and as nothing here changed them.
		   A route that declares what its body carries as named members has those
		   values bound like any other; the two whose body is the whole thing
		   declare that too, with a row carried In::BodyList or In::BodyMap, and
		   read it here. The declaration is what the document is written from; the
		   reading is that handler's business, there being no one kind for the
		   router to check the whole of a body against. */
		const std::string &body() const;

		/* The far end of the socket as text, from the transport and from nothing a
		   caller wrote. Never parsed here. This is the one of the two below that a
		   decision may rest on. */
		const std::string &peer() const;

		/* Where the request says it came from, which is the above unless a proxy
		   list is configured and the peer is on it, and then the address that list's
		   nearest member forwarded.

		   Not a trust decision. Whenever a proxy list exists this may be a string the
		   caller chose, every address of either family reading as an address, the
		   loopback included. It is here to be written down and shown. */
		const std::string &reportedPeer() const;

		AuthLevel granted() const;

		/* What part of the box the credential this request arrived with stands for,
		   and empty for a request whose credential stands for all of it.

		   Read by the handlers of the routes that declare they take one, which are the
		   only routes a scoped credential is worth anything on. A handler reading this
		   is holding the request to the scope; one that does not read it is on a route
		   where no scoped credential ever resolved, so there is nothing to hold. */
		const std::string &scope() const;

		/* The session token this request presented, and empty for one that presented
		   none or presented something that is not one.

		   Here for the single route whose act is on the session itself. A route that
		   took the token from a caller instead would close somebody else's session for
		   whoever can guess a token. It is a secret in reach of every handler, which is
		   the cost of having it in reach of that one: nothing writes it into an answer
		   or a log. */
		const std::string &session() const;

		/* The authority the request named: the name and port a caller reached this box
		   under, as it wrote them. A box answers under as many of these as there are
		   ways to reach it, and the only one right for an address handed back is the one
		   that caller used.

		   Not a trust decision. Empty for a request that named none, and nothing puts an
		   address of the box in its place: a name this side made up reads to a caller
		   exactly like the one it gave. */
		const std::string &host() const;

		/* Filled in by the router before the handler is entered and by nothing
		   else. Public rather than reached through a friend because the work is
		   spread over several helpers, and a value bound here has already been
		   checked by the one that built it. */
		void bind(const char *name, const Value &v);
		void setBody(const std::string &b);
		void setPeer(const std::string &p);
		// Empty puts the peer itself there, which is what the address of a
		// request that passed through no proxy is.
		void setReportedPeer(const std::string &p);
		void setGranted(AuthLevel a);
		void setScope(const std::string &s);
		void setSession(const std::string &t);
		void setHost(const std::string &h);

	private:
		struct Bound
		{
			std::string name;
			Value       value;
		};

		// A walk and not a map: an endpoint declares a handful of parameters,
		// and a map would cost a heap node each for a lookup that never gets
		// long enough to pay for one.
		const Bound *find(const char *name) const;

		std::vector<Bound> bound_;
		std::string        body_;
		std::string        peer_;
		std::string        reported_peer_;
		std::string        session_;
		std::string        host_;
		std::string        scope_;
		AuthLevel          granted_;
};

/* What a handler answers with, and what the router answers with when no handler is
   reached. code is set by the constructor because a response built and returned
   without one would carry whatever the stack held, and a number nobody wrote is a
   status somebody would have to debug off a wire. */
struct Response
{
	int         code;
	std::string content_type;
	std::string body;
	std::vector<std::pair<std::string, std::string> > headers;

	/* An open file the answer is sent out of, and how many of its bytes go on the wire.
	   Below zero says there is no such file and the answer is the body above.

	   Writing one here hands it over. The transport takes it with the answer and gives
	   it back when the answer is done with, and every way out of the transport that does
	   not reach that hand over gives it back itself. So there is one owner at a time,
	   which is why a response carrying one is answered through the transport and through
	   nothing else. The length is that descriptor's own and is read off it rather than
	   off the name it was opened under, those naming two different files the moment
	   anything renames or replaces the name. */
	int         fd;
	uint64_t    length;

	/* The configuration this server is to be put on once these bytes have gone out, and
	   empty for every answer that asks for nothing of the kind.

	   After and not before, which is why it is written here rather than done where it is
	   decided: putting the server on a new configuration stops the daemon, and a daemon
	   stopped inside a handler takes down the connection the handler is answering on,
	   leaving the caller without the answer saying what it was, on a box it may no
	   longer know the address of.

	   A path and not a flag, because what is to be read is the file the server was told
	   to read and a second opinion about which file that is can differ from the first. */
	std::string reload_after;

	/* A program to run, whose output is this answer's body, and empty for every answer
	   that is a document, which is all but one of them.

	   HERE FOR THE REASON reload_after IS HERE: what a handler can do is answer with
	   bytes and return, and what this asks for is a connection that stays open after the
	   handler has gone. So the handler says what is to be run and the transport runs it
	   (src/httpd/livestream.h).

	   An argument list and never a line, because the arguments carry values out of a
	   request and a line assembled around one is a line a caller can put its own
	   commands into. */
	std::vector<std::string> stream_argv;
	/* An address off this box whose bytes are the body, fetched and passed on as they
	   arrive, and empty for every answer that carries its own body.

	   A name and not an open connection, for the reason the descriptor above is the
	   other way round: a descriptor is a thing to give back and this is a thing to
	   start, so a handler that writes one and never reaches the transport has leaked
	   nothing. The transport is what turns it into a connection, because what a relay
	   needs is a connection that stays open after the handler has returned.

	   Read by the transport and by the module that owns relays, and by nothing else. A
	   handler reached any other way, which is how every case drives one, is left holding
	   a response that names what would have been fetched. */
	std::string relay_url;

	Response() : code(0), fd(-1), length(0) {}
};

/* Puts an open file behind a response and states its length from that same
   descriptor. The descriptor is taken over whatever this answers: false says it
   was not one this could answer out of and that it has already been given back, so
   a caller never has to decide whether it still owns one. Here rather than at each
   handler because the length has to come off the descriptor and not off a path,
   and a handler that measured the path would be measuring whatever carries that
   name by the time it asks. */
bool answerFromDescriptor(Response &r, int fd);

/* A plain function pointer and not a std::function, because a translation unit
   whose only effect is a constructor before main defines no symbol anything asks
   for and the linker never pulls it out of the archives this program is built
   from. Four bytes, no heap, and nothing to run at startup. */
typedef Response (*Handler)(const Request &);

// One route. An aggregate for the same reason Param is one.
struct Endpoint
{
	Method        method;
	const char   *path;       // "/api/v1/channels/{id}"
	AuthLevel     auth;
	const char   *summary;
	const Param  *params;
	size_t        param_count;
	const Schema *schema;     // the shape of the answer, until one is declared
	Handler       handler;

	/* Whether this route reads a token with a scope, and with it whether such a token
	   may arrive in the query rather than in a header.

	   For the programs that cannot present a credential at all: a media player is given
	   an address and nothing else, and every other way into this server is a header or a
	   cookie it has no way of setting. What such a route gets in return for taking a
	   credential out of an address is that the credential is worth only the part of the
	   box its scope names, and that the handler has to hold it to that part.

	   EVERY ROW STATES IT, and nothing here is left to fall out of an omission. This
	   directory is built with -Werror=missing-field-initializers (src/httpd/Makefile.am),
	   so a row written without this field does not compile, and one added later cannot
	   quietly take the answer of whichever field the compiler happened to zero. That is
	   the property worth having for this one: what a leaked address reaches is the sum of
	   the rows that say true, so each of them is a line somebody wrote on purpose and a
	   grep over the tables is the whole list.

	   A route that removes something never gets it, and neither does one that was not
	   written to read a scope: a token in an address outlives the request in a history
	   list, in a proxy's log and in whatever the address was pasted into. */
	bool          query_token_ok;
};

/* A module's table, its length, and the name the document groups its routes under,
   together, for the same reason a parameter array carries its own count.

   The mark belongs to the document and not to the pages. What reads it is a developer
   meeting this API for the first time and a client generated out of it, which turns a
   mark into a class name, so it follows the tables and the words the document is
   written in. The places a person who watches television navigates by are a different
   grouping of the same routes, in another language, and they live with the pages.

   Not optional and not defaulted, or a table without one would have its routes fall
   into whatever group a reader's tool invents. The field is first so the compiler's own
   guard against a half written aggregate catches a table added without one. */
struct RouteTable
{
	const char     *tag;
	const Endpoint *endpoints;
	size_t          count;
};

/* Writes a table's mark, its array and its count as one group. Written by hand the
   array and the count can disagree, and a count one too large reads an Endpoint that is
   not there.

   The reason of its own: the tables the server ships are sixteen and the tables the
   cases build are forty one more, so a field added to the struct without this is fifty
   seven hand edits. */
#define HTTPD_TABLE(tag, a) (tag), (a), (sizeof(a) / sizeof((a)[0]))

/* The same, for a table that hands over a count rather than having one measured.
   What uses it is a case about the disagreement itself: a count that overruns its
   array, a count beside no array at all, a count that stops short of one. Nothing
   that means its count to be right should reach for this. */
#define HTTPD_TABLE_N(tag, a, n) (tag), (a), (n)

} // namespace httpd

#endif
