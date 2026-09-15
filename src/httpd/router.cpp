/*
 * router.cpp - matching a request to a route
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

#include "router.h"

#include "auth.h"
#include "endpoint.h"
#include "http.h"
#include "json.h"
#include "status.h"

#include "coreapi/base/errors.h"
#include "coreapi/base/result.h"

#include <cstdio>
#include <cstring>
#include <string>
#include <utility>
#include <vector>

#include <stdint.h>
#include <strings.h>
#include <time.h>

namespace httpd
{

namespace
{

/* A ceiling every value has whatever its row declares, so a request cannot hand a
   handler a value the size of the request. The same number the layer below caps a
   value at. */
const size_t kMaxValueBytes = 4096;

/* The two endpoints reachable without a credential, and the whole of what they
   have in common: neither hands a caller anything it did not already have. One
   takes a name and a password and gives back a session; the other says what the
   request asking carries. Named here rather than left to whoever writes a table,
   so a third endpoint open to everybody is a change to this file.

   Only the second answers a method the browser attaches the session cookie to on a
   request another site started, and what makes that safe is whether such a site
   can read the answer. */
const char kLoginPath[] = "/api/v1/login";
const char kSessionPath[] = "/api/v1/session";

// Whether this is one of the two above, as the pair of a method and a path:
// the path alone would open a POST to the one that only ever answers a GET.
bool openToEverybody(Method m, const char *path)
{
	if (path == NULL)
		return false;
	if (m == Post && std::strcmp(path, kLoginPath) == 0)
		return true;
	return m == Get && std::strcmp(path, kSessionPath) == 0;
}

/* The widest a moment the box's clock type carries. Computed from the type because
   it is four bytes where this runs and eight where this is built. */
const long long kTimeMax = (long long)((((unsigned long long) 1) << (8 * sizeof(time_t) - 1)) - 1);
const long long kTimeMin = -kTimeMax - 1;

std::string num(long long v)
{
	char buf[32];
	// Not a locale aware conversion: a window elsewhere in this program sets
	// the process locale out of the environment and never puts it back.
	std::snprintf(buf, sizeof(buf), "%lld", v);
	return std::string(buf);
}

// Anything below reading a value as a C string stops at the first one, so a value
// carrying one would arrive shorter than what was checked and say nothing about
// it.
bool hasZeroByte(const std::string &v)
{
	return v.find('\0') != std::string::npos;
}

int hexDigit(char c)
{
	if (c >= '0' && c <= '9')
		return c - '0';
	if (c >= 'a' && c <= 'f')
		return c - 'a' + 10;
	if (c >= 'A' && c <= 'F')
		return c - 'A' + 10;
	return -1;
}

/* One percent escape is two hex digits and nothing else. A truncated or misspelt
   one is refused rather than passed through as the bytes it is written with,
   because the two readings differ and letting the request pick which one applies
   is how one reaches a check written for the other.

   plus_is_space because a browser building a query out of a form writes a space as
   a plus. A path is not built that way, so a plus in a path segment is a plus. */
bool percentDecode(const std::string &in, bool plus_is_space, std::string &out)
{
	out.clear();
	out.reserve(in.size());
	for (size_t i = 0; i < in.size(); ++i)
	{
		const char c = in[i];
		if (c == '%')
		{
			if (i + 2 >= in.size())
				return false;
			const int hi = hexDigit(in[i + 1]);
			const int lo = hexDigit(in[i + 2]);
			if (hi < 0 || lo < 0)
				return false;
			out += (char)((hi << 4) | lo);
			i += 2;
			continue;
		}
		if (c == '+' && plus_is_space)
		{
			out += ' ';
			continue;
		}
		out += c;
	}
	return true;
}

/* The segments of an absolute path, with nothing decoded. False for anything that
   is not one, a relative target needing a base to resolve it against.

   Every separator makes a segment, the empty ones included, so a doubled separator
   and a trailing one each match a route with fewer. A path that reached a route by
   having its empty segments dropped would be a second spelling of it. */
bool splitPath(const std::string &path, std::vector<std::string> &out)
{
	out.clear();
	if (path.empty() || path[0] != '/')
		return false;

	size_t start = 1;
	for (;;)
	{
		const size_t slash = path.find('/', start);
		if (slash == std::string::npos)
		{
			out.push_back(path.substr(start));
			return true;
		}
		out.push_back(path.substr(start, slash - start));
		start = slash + 1;
	}
}

/* Digits and nothing else, refused rather than wrapped when they run past what the
   widest type holds. The library's converters take leading space, stop at the
   first byte they do not understand, which reads twelve out of "12abc", and read a
   sign for an unsigned type by wrapping it. */
bool parseUnsignedDec(const std::string &v, unsigned long long &out)
{
	if (v.empty())
		return false;

	unsigned long long n = 0;
	for (size_t i = 0; i < v.size(); ++i)
	{
		if (v[i] < '0' || v[i] > '9')
			return false;
		const unsigned d = (unsigned)(v[i] - '0');
		const unsigned long long ceiling = ~(unsigned long long) 0;
		if (n > (ceiling - d) / 10ULL)
			return false;
		n = n * 10ULL + d;
	}
	out = n;
	return true;
}

bool parseSignedDec(const std::string &v, long long &out)
{
	if (v.empty())
		return false;

	const bool negative = (v[0] == '-');
	const std::string digits = (negative || v[0] == '+') ? v.substr(1) : v;

	unsigned long long mag = 0;
	if (!parseUnsignedDec(digits, mag))
		return false;

	// The negative range is one wider than the positive one, so the magnitude
	// is bounded rather than the number, which cannot be built to compare.
	const unsigned long long positive_ceiling = (~(unsigned long long) 0) >> 1;
	if (negative)
	{
		if (mag > positive_ceiling + 1)
			return false;
		out = (mag == positive_ceiling + 1) ? (-(long long) positive_ceiling - 1) : -(long long) mag;
		return true;
	}
	if (mag > positive_ceiling)
		return false;
	out = (long long) mag;
	return true;
}

// Hexadecimal, with the prefix a reader writes it with allowed and not required,
// because an identifier is quoted both ways wherever this box states one.
bool parseHex(const std::string &v, unsigned long long &out)
{
	size_t i = 0;
	if (v.size() > 2 && v[0] == '0' && (v[1] == 'x' || v[1] == 'X'))
		i = 2;
	if (i >= v.size())
		return false;

	unsigned long long n = 0;
	for (; i < v.size(); ++i)
	{
		const int d = hexDigit(v[i]);
		if (d < 0)
			return false;
		// Sixteen digits fill the widest type there is, so the seventeenth is
		// where this stops rather than where it wraps.
		if ((n >> 60) != 0)
			return false;
		n = (n << 4) | (unsigned long long) d;
	}
	out = n;
	return true;
}

// The spellings the layer below accepts, so a value is not a yes on one path and
// a fault on the other.
bool parseBool(const std::string &v, bool &out)
{
	static const char *const yes[] = { "1", "true", "yes", "on" };
	static const char *const no[]  = { "0", "false", "no", "off" };
	for (size_t i = 0; i < 4; ++i)
	{
		if (strcasecmp(v.c_str(), yes[i]) == 0)
		{
			out = true;
			return true;
		}
		if (strcasecmp(v.c_str(), no[i]) == 0)
		{
			out = false;
			return true;
		}
	}
	return false;
}

bool inList(const char *list, const std::string &v)
{
	if (list == NULL)
		return false;
	const char *p = list;
	for (;;)
	{
		const char *e = std::strchr(p, ',');
		const size_t n = (e != NULL) ? (size_t)(e - p) : std::strlen(p);
		/* Nothing guards an empty element here, because two rules elsewhere already mean
		   one can never match: a list carrying one is refused where tables are checked,
		   and an empty value is treated as absent. */
		if (n == v.size() && std::memcmp(p, v.data(), n) == 0)
			return true;
		if (e == NULL)
			return false;
		p = e + 1;
	}
}

/* Every refusal this file makes, written through the one builder beside the problem
   document, so two answers to one kind of refusal cannot take two shapes. */
Response refuse(int code, coreapi::ErrorCode e, const std::string &detail)
{
	return problemResponse(code, e, detail);
}

/* A refusal that names the parameter it is about. The name comes from the table and
   never from the request: the value and the name a caller sent are the parts a
   caller wrote, and the answer travels back to places that render it. */
Response badParam(coreapi::ErrorCode e, const char *name, const std::string &what)
{
	return refuse(StatusBadRequest, e,
	              std::string("parameter ") + ((name != NULL) ? name : "") + " " + what);
}

Response notFound()
{
	// The path is not quoted back, for the reason above and because an answer
	// that echoes a target is one a caller can use to find out what this server
	// has by reading its own words back.
	return refuse(StatusNotFound, coreapi::ErrorCode::NoSuchRoute,
	              "this server has no such path");
}

// The order of the members of AuthLevel is the order of privilege, which is stated
// where they are declared and is the whole of this check.
bool atLeast(AuthLevel granted, AuthLevel needed)
{
	return (int) granted >= (int) needed;
}

bool isPlaceholder(const char *seg, size_t n)
{
	return n >= 2 && seg[0] == '{' && seg[n - 1] == '}';
}

/* Whether a route matches these already decoded segments, and how many of them it
   matched as itself rather than as a placeholder. That count decides between two
   routes that both match, so a route naming a segment answers ahead of one that
   would bind it, whatever order the tables are linked in.

   binds, when it is not null, is filled with the name and value of every segment
   the route binds; it is passed null while looking, so looking costs no
   allocation. */
bool matchPattern(const char *pattern, const std::vector<std::string> &seg, size_t &literals,
                  std::vector<std::pair<std::string, std::string> > *binds)
{
	if (pattern == NULL || pattern[0] != '/')
		return false;

	literals = 0;
	const char *p = pattern + 1;
	size_t i = 0;
	for (;;)
	{
		const char *e = std::strchr(p, '/');
		const size_t n = (e != NULL) ? (size_t)(e - p) : std::strlen(p);

		if (i >= seg.size())
			return false;

		if (isPlaceholder(p, n))
		{
			/* A placeholder binds a value and an empty segment is not one.
			   Without this a doubled separator would hand a handler the empty
			   string where the row said an identifier would be. */
			if (seg[i].empty())
				return false;
			if (binds != NULL)
				binds->push_back(std::make_pair(std::string(p + 1, n - 2), seg[i]));
		}
		else
		{
			if (seg[i].size() != n || std::memcmp(seg[i].data(), p, n) != 0)
				return false;
			++literals;
		}

		++i;
		if (e == NULL)
			break;
		p = e + 1;
	}

	// A route with fewer segments than the request is a route the request only
	// begins with, and one with more is a route the request stops short of.
	return i == seg.size();
}

const Param *findParam(const Endpoint &e, const std::string &name, In where)
{
	for (size_t i = 0; i < e.param_count; ++i)
	{
		if (e.params[i].in != where || e.params[i].name == NULL)
			continue;
		if (name == e.params[i].name)
			return &e.params[i];
	}
	return NULL;
}

/* The name and the value of every component of a query, as they arrived.

   A component with nothing between its separators is dropped: a trailing separator
   is what a client writes. A component that is only a value is not dropped, and
   reaches the check below as a parameter with an empty name, which no table can
   declare. A value that is nothing is read the same way whether it was written
   that way or left out. */
void splitQuery(const std::string &q, std::vector<std::pair<std::string, std::string> > &out)
{
	out.clear();
	size_t start = 0;
	for (;;)
	{
		size_t amp = q.find('&', start);
		if (amp == std::string::npos)
			amp = q.size();

		if (amp > start)
		{
			const std::string one = q.substr(start, amp - start);
			const size_t eq = one.find('=');
			if (eq == std::string::npos)
				out.push_back(std::make_pair(one, std::string()));
			else
				out.push_back(std::make_pair(one.substr(0, eq), one.substr(eq + 1)));
		}

		if (amp >= q.size())
			return;
		start = amp + 1;
	}
}

/* Whether the text is a value the row declares, and what it is when it is.
   Otherwise refusal carries the answer naming the first thing found wrong with it.

   This is the whole of what stands between a request and a handler. A value that
   comes out of here has been decoded, is inside its declared bounds and fits the
   type the accessor answers. */
bool checkValue(const Param &p, const std::string &text, Value &v, Response &refusal)
{
	v.type = p.type;
	v.text = text;

	if (hasZeroByte(text))
	{
		refusal = badParam(coreapi::ErrorCode::ValueHasZeroByte, p.name, "carries a zero byte");
		return false;
	}
	if (text.size() > kMaxValueBytes)
	{
		refusal = badParam(coreapi::ErrorCode::ValueTooLong, p.name,
		                   "is longer than " + num((long long) kMaxValueBytes) + " bytes");
		return false;
	}

	switch (p.type)
	{
		case ParamType::Int:
		{
			long long n = 0;
			if (!parseSignedDec(text, n))
			{
				refusal = badParam(coreapi::ErrorCode::BadInt, p.name, "is not a whole number");
				return false;
			}
			/* The accessor answers a long, four bytes where this runs and eight where it is
			   built, so a number that does not survive the narrowing is out of range whatever
			   the row declares. This cannot be false where the suite runs and is not dead: the
			   cross build exercises it. Written as a round trip rather than against the limits
			   of the type, which on this host reads as a comparison that cannot be true. */
			if ((long long)(long) n != n)
			{
				refusal = badParam(coreapi::ErrorCode::OutOfRange, p.name,
				                   "is wider than a whole number here holds");
				return false;
			}
			if ((p.min != 0 || p.max != 0) && (n < (long long) p.min || n > (long long) p.max))
			{
				refusal = badParam(coreapi::ErrorCode::OutOfRange, p.name,
				                   "is outside " + num(p.min) + " to " + num(p.max));
				return false;
			}
			v.number = n;
			return true;
		}
		case ParamType::UInt:
		{
			unsigned long long n = 0;
			if (!parseUnsignedDec(text, n))
			{
				refusal = badParam(coreapi::ErrorCode::BadInt, p.name, "is not a whole number of none or more");
				return false;
			}
			// The same narrowing as above, and unexercised here for the same
			// reason.
			if ((unsigned long long)(unsigned long) n != n)
			{
				refusal = badParam(coreapi::ErrorCode::OutOfRange, p.name,
				                   "is wider than a whole number here holds");
				return false;
			}
			if (p.min != 0 || p.max != 0)
			{
				// A row bounding an unsigned parameter below zero is refused where
				// tables are checked; read here as no bound at all rather than as a
				// huge one, which is what casting would make of it.
				const unsigned long long lo = (p.min > 0) ? (unsigned long long) p.min : 0;
				const unsigned long long hi = (p.max > 0) ? (unsigned long long) p.max : 0;
				if (n < lo || n > hi)
				{
					refusal = badParam(coreapi::ErrorCode::OutOfRange, p.name,
					                   "is outside " + num(p.min) + " to " + num(p.max));
					return false;
				}
			}
			v.unumber = n;
			return true;
		}
		case ParamType::Bool:
		{
			bool b = false;
			if (!parseBool(text, b))
			{
				refusal = badParam(coreapi::ErrorCode::BadBool, p.name, "is not a yes or a no");
				return false;
			}
			v.flag = b;
			return true;
		}
		case ParamType::String:
		{
			if (p.max > 0 && (long) text.size() > p.max)
			{
				refusal = badParam(coreapi::ErrorCode::ValueTooLong, p.name,
				                   "is longer than " + num(p.max) + " bytes");
				return false;
			}
			return true;
		}
		case ParamType::Enum:
		{
			if (p.values == NULL || p.values[0] == '\0')
			{
				refusal = refuse(StatusInternalServerError,
				                 coreapi::ErrorCode::NoValues,
				                 std::string("parameter ") + p.name + " lists no accepted values");
				return false;
			}
			if (!inList(p.values, text))
			{
				refusal = badParam(coreapi::ErrorCode::BadEnum, p.name,
				                   std::string("is not one of ") + p.values);
				return false;
			}
			return true;
		}
		case ParamType::ChannelId:
		{
			unsigned long long n = 0;
			if (!parseHex(text, n))
			{
				refusal = badParam(coreapi::ErrorCode::BadInt, p.name, "is not a hexadecimal identifier");
				return false;
			}
			// No bound is read: an identifier is eight bytes and the bounds a
			// row carries are four, so no pair of them could describe one.
			v.unumber = n;
			return true;
		}
		case ParamType::Time:
		{
			long long n = 0;
			if (!parseSignedDec(text, n))
			{
				refusal = badParam(coreapi::ErrorCode::BadInt, p.name, "is not a whole number of seconds");
				return false;
			}
			if (n < kTimeMin || n > kTimeMax)
			{
				refusal = badParam(coreapi::ErrorCode::OutOfRange, p.name,
				                   "is outside what the clock here holds");
				return false;
			}
			if ((p.min != 0 || p.max != 0) && (n < (long long) p.min || n > (long long) p.max))
			{
				refusal = badParam(coreapi::ErrorCode::OutOfRange, p.name,
				                   "is outside " + num(p.min) + " to " + num(p.max));
				return false;
			}
			v.number = n;
			return true;
		}
	}

	/* No default above, and an unhandled enumerator is an error in this directory, so a
	   type added without a check stops the build. What is left is a value cast into the
	   enum from outside, refused rather than let through. */
	refusal = refuse(StatusInternalServerError,
	                 coreapi::ErrorCode::BadTable,
	                 std::string("parameter ") + ((p.name != NULL) ? p.name : "") +
	                 " is declared as something this server cannot read");
	return false;
}

/* Whether every name a caller wrote in one place is a name the endpoint declares as
   carried there.

   Asked per place and not over the declaration as a whole. A parameter the route
   says is carried in the body is no more one the query offers than one nobody
   declared, and folding the two would let a caller reach a check written for one
   place by writing the value in the other. */
bool namesAreDeclared(const Endpoint &ep,
                      const std::vector<std::pair<std::string, std::string> > &given,
                      In where, Response &refusal)
{
	for (size_t i = 0; i < given.size(); ++i)
	{
		if (findParam(ep, given[i].first, where) != NULL)
			continue;
		refusal = refuse(StatusBadRequest,
		                 coreapi::ErrorCode::NoSuchParameter,
		                 "this endpoint declares no parameter of that name");
		return false;
	}
	return true;
}

// And whether none of them was written twice. Every name reaching here is one the
// table declares, so the table's spelling of it is what the answer carries.
bool namesAreDistinct(const Endpoint &ep,
                      const std::vector<std::pair<std::string, std::string> > &given,
                      In where, Response &refusal)
{
	for (size_t i = 0; i < given.size(); ++i)
	{
		for (size_t j = 0; j < i; ++j)
		{
			if (given[i].first != given[j].first)
				continue;
			const Param *p = findParam(ep, given[i].first, where);
			refusal = badParam(coreapi::ErrorCode::DuplicateParameter,
			                   (p != NULL) ? p->name : NULL, "was given twice");
			return false;
		}
	}
	return true;
}

/* One route matched, its parameters checked, and the handler entered or not entered.
   Every way out of here before the last line is a refusal, which is what makes the
   handler unable to see a value it would have to check.

   The order of the checks is the order the answer names a fault in: a name nobody
   declared, then a name given twice, then a required one left out, then what the
   value is. A request wrong in two ways is told about the one furthest from being a
   request this endpoint could answer. */
Response runEndpoint(const Endpoint &ep,
                     const std::vector<std::pair<std::string, std::string> > &path_values,
                     const std::string &query, const std::string &body,
                     const std::string &peer, AuthLevel granted,
                     const std::string &reported, const std::string &session,
                     const std::string &host, const std::string &scope)
{
	/* The sentence is the gate's and not written out again here, because two
	   copies of one refusal are two answers a client can tell apart the moment
	   one of them is reworded. */
	if (!atLeast(granted, ep.auth))
		return refuse(StatusForbidden, coreapi::ErrorCode::NotPermitted, notPermittedDetail());

	// A table this layer wrote with no handler in it would otherwise be a jump
	// through nothing.
	if (ep.handler == NULL)
		return refuse(StatusInternalServerError,
		              coreapi::ErrorCode::BadTable, "this route names no handler");
	if (ep.param_count > 0 && ep.params == NULL)
		return refuse(StatusInternalServerError,
		              coreapi::ErrorCode::BadTable, "this route declares parameters it does not carry");

	std::vector<std::pair<std::string, std::string> > raw;
	splitQuery(query, raw);

	std::vector<std::pair<std::string, std::string> > given;
	given.reserve(raw.size());
	for (size_t i = 0; i < raw.size(); ++i)
	{
		std::pair<std::string, std::string> one;
		if (!percentDecode(raw[i].first, true, one.first) ||
		    !percentDecode(raw[i].second, true, one.second))
		{
			// Before any name is compared, because an escape that is not one
			// leaves nothing to compare.
			return refuse(StatusBadRequest,
			              coreapi::ErrorCode::BadString,
			              "the query carries an escape that is not one");
		}
		given.push_back(one);
	}

	/* The credential a route may take out of its query is taken out of the query here,
	   before anything compares a name against what the route declares. It is not a
	   parameter: the transport has already read it and the gate has already answered
	   with it, and a route declaring it would be a route whose handler could read a
	   secret it has no business holding. Left in, it would be a name nothing declared,
	   which is the one thing the comparison below is for.

	   Only on a route that says so. Everywhere else the name is an ordinary one, and a
	   route that happened to declare a parameter called this goes on carrying it. */
	if (ep.query_token_ok)
	{
		const std::string credential = queryTokenName();
		std::vector<std::pair<std::string, std::string> > rest;
		rest.reserve(given.size());
		for (size_t i = 0; i < given.size(); ++i)
		{
			if (given[i].first != credential)
				rest.push_back(given[i]);
		}
		given.swap(rest);
	}

	/* The body is taken apart only for a route that declares a value carried in one. A
	   route that declares none is answered whatever it sent, refusing over bytes nobody
	   looks at turning every stray body into an error about a request that was otherwise
	   right. A member arrives here as text whatever it was written as. */
	std::vector<std::pair<std::string, std::string> > sent;
	bool takes_body = false;
	for (size_t i = 0; i < ep.param_count && !takes_body; ++i)
		takes_body = ep.params[i].in == In::Body;
	if (takes_body)
	{
		std::vector<JsonMember> members;
		if (!readFlatObject(body, members))
		{
			/* One sentence for everything the reader turns down, and it names none of what
			   arrived. Which byte it stopped at is a description of what the caller sent, and
			   this answer travels back to places that render it. */
			return refuse(StatusBadRequest,
			              coreapi::ErrorCode::BadString,
			              "the body is not one flat object of strings, numbers and booleans");
		}
		sent.reserve(members.size());
		for (size_t i = 0; i < members.size(); ++i)
			sent.push_back(std::make_pair(members[i].name, members[i].text));
	}

	/* Both places a caller writes a name, in the order the answer names a fault
	   in and with the query first. */
	Response named;
	if (!namesAreDeclared(ep, given, In::Query, named))
		return named;
	if (!namesAreDeclared(ep, sent, In::Body, named))
		return named;
	if (!namesAreDistinct(ep, given, In::Query, named))
		return named;
	if (!namesAreDistinct(ep, sent, In::Body, named))
		return named;

	Request req;
	req.setBody(body);
	req.setPeer(peer);
	req.setReportedPeer(reported);
	req.setGranted(granted);
	req.setSession(session);
	req.setHost(host);
	req.setScope(scope);

	/* A value that is nothing is no value, whether the name was left out or handed over
	   empty: a form that submits a field nobody touched sends the second and means the
	   first, and reading the two differently would make the answer depend on how a
	   client happened to build its query. */
	std::vector<std::string> text(ep.param_count);
	std::vector<char> present(ep.param_count, 0);
	for (size_t i = 0; i < ep.param_count; ++i)
	{
		const Param &p = ep.params[i];
		if (p.name == NULL || p.name[0] == '\0')
			continue;
		/* A row naming the whole of a body carries nothing under its own name: there is no
		   member and no query component to find, and the handler reads the body itself.
		   Nothing distinguishes this from leaving it out today. It is written because a
		   guard left out on the grounds that something further off happens to cover it is
		   how the covering becomes load bearing without anybody deciding it should be. */
		if (namesWholeBody(p.in))
			continue;

		const std::vector<std::pair<std::string, std::string> > *carrier = &given;
		if (p.in == In::Path)
			carrier = &path_values;
		else if (p.in == In::Body)
			carrier = &sent;
		const std::vector<std::pair<std::string, std::string> > &from = *carrier;
		for (size_t j = 0; j < from.size(); ++j)
		{
			if (from[j].first != p.name)
				continue;
			text[i] = from[j].second;
			present[i] = text[i].empty() ? 0 : 1;
			break;
		}
	}

	for (size_t i = 0; i < ep.param_count; ++i)
	{
		const Param &p = ep.params[i];
		// Nothing binds such a row, so nothing could ever be found missing:
		// asked here it would turn down every request to the routes that
		// carry one.
		if (namesWholeBody(p.in))
			continue;
		if (p.required && !present[i])
			return badParam(coreapi::ErrorCode::MissingParameter, p.name, "is required");
	}

	for (size_t i = 0; i < ep.param_count; ++i)
	{
		const Param &p = ep.params[i];
		if (!present[i])
			continue;

		Value v;
		Response refusal;
		if (!checkValue(p, text[i], v, refusal))
			return refusal;
		req.bind(p.name, v);
	}

	return ep.handler(req);
}

// Whether a list of methods already names one. A walk and not a set: no path offers
// enough of them to pay for one.
bool namesMethod(const std::vector<Method> &list, Method m)
{
	for (size_t i = 0; i < list.size(); ++i)
	{
		if (list[i] == m)
			return true;
	}
	return false;
}

/* What the tables answer about a request, before any value in it is read. One walk,
   written once: the transport asks this before it accepts a body and the dispatch
   below asks it to answer the request, and two walks would be two readings of one
   question with only one of them deciding. */
enum Matched
{
	MatchedRoute,     // one route answers, and best is it
	MatchedBadPath,   // nothing this server can take apart
	MatchedBadEscape, // an escape in the path that is not one
	MatchedNoPath,    // no route names this path at all
	MatchedNoMethod,  // the path is here and this method is not, allowed says which are
	MatchedTwoRoutes  // two routes answer it equally well, which is a table written wrong
};

struct Match
{
	const Endpoint *best;
	std::vector<std::pair<std::string, std::string> > binds;
	std::vector<Method> allowed;

	Match() : best(NULL) {}
};

Matched matchIn(const RouteTable *const *tables, size_t table_count, Method m,
                const std::string &path, Match &out)
{
	/* A route that answers a GET answers the head of that same answer, so the two are
	   one route here. Answered rather than dropped: a client asking what a resource is
	   before fetching it is told what it would have been told, down to the length, which
	   a body dropped before the answer was built could not say. */
	const Method want = (m == Head) ? Get : m;

	std::vector<std::string> raw;
	if (!splitPath(path, raw))
		return MatchedBadPath;

	std::vector<std::string> seg(raw.size());
	for (size_t i = 0; i < raw.size(); ++i)
	{
		/* After the split and never before it. Decoding first would turn an encoded
		   separator into a separator, which is one segment written as two, and a request
		   could then spell its way past the segment that was going to be checked. */
		if (!percentDecode(raw[i], false, seg[i]))
			return MatchedBadEscape;
	}

	size_t best_literals = 0;
	bool ambiguous = false;
	/* What the winning route binds, kept as the winner is found rather than worked out
	   again once it is, so no route is walked twice. The scratch buffer is cleared and
	   reused, so a walk that ends in nothing costs no allocation after the first. */
	std::vector<std::pair<std::string, std::string> > binds;

	for (size_t t = 0; t < table_count; ++t)
	{
		/* Refused where the tables are checked, so this answers only for a list
		   that was never checked. Passed over rather than followed: reading
		   through the address anyway would take the process down over a request. */
		if (tables[t] == NULL)
			continue;
		for (size_t e = 0; e < tables[t]->count; ++e)
		{
			const Endpoint &ep = tables[t]->endpoints[e];
			size_t literals = 0;
			binds.clear();
			if (!matchPattern(ep.path, seg, literals, &binds))
				continue;

			if (!namesMethod(out.allowed, ep.method))
				out.allowed.push_back(ep.method);
			/* Named beside the GET it is answered by, because a list of what a path takes
			   that leaves it out sends a caller away from an answer this server would have
			   given. Asked for separately, or a table naming HEAD itself would list it
			   twice. */
			if (ep.method == Get && !namesMethod(out.allowed, Head))
				out.allowed.push_back(Head);

			if (ep.method != want)
				continue;
			if (out.best == NULL || literals > best_literals)
			{
				out.best = &ep;
				best_literals = literals;
				ambiguous = false;
				out.binds.swap(binds);
			}
			else if (literals == best_literals)
			{
				ambiguous = true;
			}
		}
	}

	if (out.allowed.empty())
		return MatchedNoPath;
	if (out.best == NULL)
		return MatchedNoMethod;
	if (ambiguous)
		return MatchedTwoRoutes;
	return MatchedRoute;
}

Response dispatchTables(const RouteTable *const *tables, size_t table_count, Method m,
                        const std::string &path, const std::string &query,
                        const std::string &body, const std::string &peer,
                        AuthLevel granted, const std::string &reported,
                        const std::string &session, const std::string &host,
                        const std::string &scope)
{
	/* A method this server does not have is not a path it does not have. The
	   answer says the verb is one nothing here implements, rather than sending a
	   caller looking for a path that is right where it thought. */
	if (m == UnknownMethod)
		return refuse(StatusNotImplemented,
		              coreapi::ErrorCode::NoSuchMethod, "this server does not offer that method");

	Match found;
	const Matched what = matchIn(tables, table_count, m, path, found);

	if (what == MatchedBadPath || what == MatchedNoPath)
		return notFound();

	if (what == MatchedBadEscape)
		return refuse(StatusBadRequest,
		              coreapi::ErrorCode::BadString,
		              "the path carries an escape that is not one");

	const Endpoint *const best = found.best;
	const std::vector<Method> &allowed = found.allowed;

	if (what == MatchedNoMethod)
	{
		/* The path is one this server has and the method is not one it has there,
		   which is a different answer from a path it does not know, and the answer
		   says which methods it does have so a caller does not have to try them. */
		Response r = refuse(StatusMethodNotAllowed,
		                    coreapi::ErrorCode::NoSuchMethod,
		                    "this path does not answer that method");
		std::string list;
		for (size_t i = 0; i < allowed.size(); ++i)
		{
			const char *name = methodName(allowed[i]);
			if (name == NULL || name[0] == '\0')
				continue;
			if (!list.empty())
				list += ", ";
			list += name;
		}
		r.headers.push_back(std::make_pair(std::string("Allow"), list));
		return r;
	}

	/* Two routes matching one request equally well is a table this layer wrote wrong,
	   and the check that tables are sane refuses to start on one. This answers rather
	   than picking whichever the linker put first, because a server picking one of two
	   would answer differently after a rebuild that changed nothing anybody wrote. */
	if (what == MatchedTwoRoutes)
		return refuse(StatusInternalServerError,
		              coreapi::ErrorCode::BadTable, "two routes answer this request");

	return runEndpoint(*best, found.binds, query, body, peer, granted, reported, session, host,
	                   scope);
}

/* A route as the thing it matches rather than as the text it is written with,
   which is what two of them have to be compared as. */
struct Shape
{
	Method                   method;
	const char              *path;
	std::vector<std::string> seg;
	size_t                   literals;
};

/* Whether some request exists that both routes match, and match with the same number
   of segments written out.

   That is the pair a table must not hold, and it is a wider set than two routes that
   read alike. Which of two matching routes answers is settled by how many of its
   segments are written out rather than bound, so a pair that ties on that count has
   no answer left but the order they were linked in. Two routes tie exactly when they
   are the same length, agree wherever they both write a segment out, and write out
   the same number of them.

   Spelling is not compared at all. Two routes that differ only in the name inside
   their braces are caught by this as the one route they are, and so is the pair that
   reads as two different routes and is not: one naming a segment the other binds, in
   a position the other one names back. */
bool wouldTie(const Shape &a, const Shape &b)
{
	if (a.method != b.method)
		return false;
	if (a.seg.size() != b.seg.size())
		return false;
	if (a.literals != b.literals)
		return false;
	for (size_t i = 0; i < a.seg.size(); ++i)
	{
		const bool pa = isPlaceholder(a.seg[i].data(), a.seg[i].size());
		const bool pb = isPlaceholder(b.seg[i].data(), b.seg[i].size());
		// Two segments both written out and written differently: no request
		// carries both, so these two never meet.
		if (!pa && !pb && a.seg[i] != b.seg[i])
			return false;
	}
	return true;
}

// Named the same way wherever the answer is given, so one message covers a pair
// inside a table and a pair spread over two of them.
std::string tieSays(const Shape &a, const Shape &b)
{
	return std::string("two routes answer one request equally well: ") +
	       methodName(a.method) + " " + a.path + " and " + methodName(b.method) + " " + b.path;
}

bool say(std::string *why, const std::string &what)
{
	if (why != NULL)
		*why = what;
	return false;
}

/* One route against the rules a route has to keep, and the shape of it read off the
   same walk, so the path is split once and what that split found is what the
   comparison between routes then uses. */
bool endpointIsSane(const Endpoint &ep, Shape &shape, std::string *why)
{
	shape.method = ep.method;
	shape.path = (ep.path != NULL) ? ep.path : "";
	shape.literals = 0;

	const std::string where = std::string(methodName(ep.method)) + " " +
	                          ((ep.path != NULL) ? ep.path : "(no path)") + ": ";

	if (ep.method == UnknownMethod)
		return say(why, where + "the method is not one this server has");
	/* The head of an answer is sent for the route that answers the GET, so a route
	   of its own here is one no request ever reaches, and the list of what a path
	   takes would name the verb twice. */
	if (ep.method == Head)
		return say(why, where + "the route answers a HEAD, which the route that answers the GET already does");
	if (ep.path == NULL)
		return say(why, where + "the route names no path");
	if (ep.handler == NULL)
		return say(why, where + "the route names no handler");
	if (ep.summary == NULL || ep.summary[0] == '\0')
		return say(why, where + "the route says nothing about itself");
	if (ep.param_count > 0 && ep.params == NULL)
		return say(why, where + "the route declares parameters it does not carry");
	/* Public is what the two endpoints named at the top of this file are, and the one
	   posture this project exists to remove is everything being reachable without a
	   credential. A third endpoint open to everybody is a change somebody has to make
	   there rather than a field set in passing. */
	if (ep.auth == AuthLevel::Public && !openToEverybody(ep.method, ep.path))
		return say(why, where + "the route is open to everybody and is not one of the two that may be");

	/* A caller on the box's own network is granted a read and is asked for no
	   credential, so a route that changes something and asks for no more than a read is
	   one that any page a browser on that network visits can reach in that browser's
	   name. Nothing carries a second token there either, because there is no session to
	   have one. Without this a table could undo that exemption one field at a time.

	   GET and OPTIONS are what a route may declare and ask for less than a write. HEAD
	   is not named because a route declaring one is refused above. The exception is the
	   same pair the rule above names, of which only the one that hands out a credential
	   is written with a method this reaches: reaching it grants nothing. */
	if (ep.method != Get && ep.method != Options &&
	    (int) ep.auth < (int) AuthLevel::Write &&
	    !openToEverybody(ep.method, ep.path))
		return say(why, where + "the route changes something and asks for no more than a read");

	if (!splitPath(std::string(ep.path), shape.seg))
		return say(why, where + "the path is not an absolute one");
	std::vector<std::string> placeholders;
	for (size_t i = 0; i < shape.seg.size(); ++i)
	{
		// An empty segment in a route is a separator too many, and no request
		// path is written with one, so the route would answer nothing.
		if (shape.seg[i].empty())
			return say(why, where + "the path carries an empty segment");
		if (shape.seg[i].find('%') != std::string::npos)
			return say(why, where + "the path carries an escape, which a route is never matched with");
		if (!isPlaceholder(shape.seg[i].data(), shape.seg[i].size()))
		{
			++shape.literals;
			continue;
		}

		const std::string name = shape.seg[i].substr(1, shape.seg[i].size() - 2);
		if (name.empty())
			return say(why, where + "the path binds a segment to no name");
		for (size_t j = 0; j < placeholders.size(); ++j)
		{
			if (placeholders[j] == name)
				return say(why, where + "the path binds " + name + " twice");
		}
		placeholders.push_back(name);

		const Param *p = findParam(ep, name, In::Path);
		if (p == NULL)
			return say(why, where + "the path binds " + name + ", which no parameter declares");
	}

	for (size_t i = 0; i < ep.param_count; ++i)
	{
		const Param &p = ep.params[i];
		if (p.name == NULL || p.name[0] == '\0')
			return say(why, where + "a parameter has no name");
		if (p.doc == NULL || p.doc[0] == '\0')
			return say(why, std::string(where) + "parameter " + p.name + " says nothing about itself");

		for (size_t j = 0; j < i; ++j)
		{
			if (ep.params[j].name != NULL && std::strcmp(ep.params[j].name, p.name) == 0)
				return say(why, where + "parameter " + p.name + " is declared twice");
		}

		const bool whole = namesWholeBody(p.in);

		/* A body is read for the methods that are written with one and for no others. A
		   value carried in the body of a method that carries none would never be extracted,
		   never checked and a required one never missed, leaving a handler reachable without
		   a value its own row says it gets. DELETE is here because what sits between a caller
		   and this box drops or rewrites a body on one often enough that a route depending on
		   it would work from some networks and not from others. */
		if ((p.in == In::Body || whole) &&
		    (ep.method == Get || ep.method == Options || ep.method == Delete))
			return say(why, where + "parameter " + p.name + " is carried in the body of a method that is not written with one");

		if (whole)
		{
			/* One whole body at a time, and never beside a member of one. A body is one thing
			   or a set of named values; a route declaring both describes a body that cannot
			   arrive, whichever of the two a caller sent leaving the other row unanswered. */
			for (size_t j = 0; j < ep.param_count; ++j)
			{
				if (j == i || ep.params[j].name == NULL)
					continue;
				if (namesWholeBody(ep.params[j].in))
					return say(why, where + "the route names the whole of its body twice");
				if (ep.params[j].in == In::Body)
					return say(why, where + "the route names the whole of its body and a value inside it");
			}

			/* What the reader of such a body takes. The array reader takes strings and nothing
			   else, so a row naming elements of a kind that does not travel as text would put
			   into the document a body this server turns down. */
			if (p.in == In::BodyList && p.type != ParamType::String &&
			    p.type != ParamType::Enum && p.type != ParamType::ChannelId)
				return say(why, where + "parameter " + p.name + " names elements of a kind that does not travel as text");
		}

		if (p.in == In::Path)
		{
			bool bound = false;
			for (size_t j = 0; j < placeholders.size(); ++j)
				bound = bound || (placeholders[j] == p.name);
			if (!bound)
				return say(why, where + "parameter " + p.name + " is carried by the path and the path does not bind it");
			// A segment the path binds is always there, so a path parameter
			// that says it is optional describes a request that cannot arrive.
			if (!p.required)
				return say(why, where + "parameter " + p.name + " is carried by the path and is not required");
		}

		const bool numeric = (p.type == ParamType::Int || p.type == ParamType::UInt ||
		                      p.type == ParamType::Time);
		if (whole)
		{
			/* Read as a count of what the body carries and not as bounds on one value, there
			   being no one value on such a row. Below zero is a count no body has, and a floor
			   above a ceiling describes a body that cannot be written. */
			if (p.min < 0 || p.max < 0)
				return say(why, where + "parameter " + p.name + " counts what the body carries below zero");
			if (p.max > 0 && p.min > p.max)
				return say(why, where + "parameter " + p.name + " takes fewer at most than it takes at least");
		}
		else if (numeric)
		{
			if (p.min > p.max)
				return say(why, where + "parameter " + p.name + " is bounded below above its ceiling");
			if (p.type == ParamType::UInt && p.min < 0)
				return say(why, where + "parameter " + p.name + " is bounded below zero and holds none or more");
		}
		else if (p.type == ParamType::String)
		{
			// A floor on a length is not read where values are checked, so a
			// row carrying one would declare something nothing enforces.
			if (p.min != 0)
				return say(why, where + "parameter " + p.name + " bounds the length of a string below, which is not read");
			if (p.max < 0 || (size_t) p.max > kMaxValueBytes)
				return say(why, where + "parameter " + p.name + " sets a ceiling this server does not carry");
		}
		else if (p.min != 0 || p.max != 0)
		{
			return say(why, where + "parameter " + p.name + " carries bounds its type does not read");
		}

		if (p.type == ParamType::Enum)
		{
			if (p.values == NULL || p.values[0] == '\0')
				return say(why, where + "parameter " + p.name + " lists no accepted values");
			const char *q = p.values;
			for (;;)
			{
				const char *e = std::strchr(q, ',');
				if ((e != NULL ? (size_t)(e - q) : std::strlen(q)) == 0)
					return say(why, where + "parameter " + p.name + " lists an empty value");
				if (e == NULL)
					break;
				q = e + 1;
			}
		}
		else if (p.values != NULL)
		{
			return say(why, where + "parameter " + p.name + " lists accepted values and is not one of a set");
		}

		if (p.choices != NULL)
		{
			/* One set or the other and never both. A row that listed its values and
			   named something to ask as well would be two sets, and a reader of the
			   document has no way to tell which of them the box holds to. */
			if (p.values != NULL)
				return say(why, where + "parameter " + p.name + " lists accepted values and names a set to ask for as well");
			// The set is text whatever answers it, and the row stays text here:
			// what a set beside any other kind would mean is settled above, and
			// asking for one changes none of that.
			if (p.type != ParamType::String)
				return say(why, where + "parameter " + p.name + " names a set to ask for and is not text");
		}
	}

	return true;
}

} // namespace

bool tablesAreSane(const RouteTable *const *tables, size_t table_count, std::string *why)
{
	if (why != NULL)
		why->clear();
	if (table_count > 0 && tables == NULL)
		return say(why, "the list names tables it does not carry");

	/* Every route of every table against every other, and not each table on its
	   own, because two tables can carry one answer between them and neither of
	   them is wrong read alone. */
	std::vector<Shape> shapes;
	for (size_t t = 0; t < table_count; ++t)
	{
		// A list is addresses of tables, so a member that is not one is a list
		// this layer wrote wrong, the same kind of fault as a table counting
		// routes it does not carry.
		if (tables[t] == NULL)
			return say(why, "the list names a table that is not there");
		if (tables[t]->count > 0 && tables[t]->endpoints == NULL)
			return say(why, "the table declares routes it does not carry");

		for (size_t i = 0; i < tables[t]->count; ++i)
		{
			Shape shape;
			if (!endpointIsSane(tables[t]->endpoints[i], shape, why))
				return false;

			for (size_t j = 0; j < shapes.size(); ++j)
			{
				if (wouldTie(shapes[j], shape))
					return say(why, tieSays(shapes[j], shape));
			}
			shapes.push_back(shape);
		}
	}
	return true;
}

bool tableIsSane(const RouteTable &t, std::string *why)
{
	// The list of one this table makes, built where it is passed rather than
	// anywhere that outlives the call.
	const RouteTable *const one[] = { &t };
	return tablesAreSane(one, 1, why);
}

bool routesAreSane(std::string *why)
{
	size_t count = 0;
	const RouteTable *const *tables = allRoutes(&count);
	return tablesAreSane(tables, count, why);
}

Response dispatchIn(const RouteTable *const *tables, size_t table_count, Method m,
                    const std::string &path, const std::string &query,
                    const std::string &body, const std::string &peer, AuthLevel granted,
                    const std::string &reported, const std::string &session,
                    const std::string &host, const std::string &scope)
{
	return dispatchTables(tables, table_count, m, path, query, body, peer, granted, reported,
	                      session, host, scope);
}

Response dispatchIn(const RouteTable &t, Method m, const std::string &path,
                    const std::string &query, const std::string &body,
                    const std::string &peer, AuthLevel granted,
                    const std::string &reported, const std::string &session,
                    const std::string &host, const std::string &scope)
{
	const RouteTable *const one[] = { &t };
	return dispatchTables(one, 1, m, path, query, body, peer, granted, reported, session, host,
	                      scope);
}

Response dispatch(Method m, const std::string &path,
                  const std::string &query, const std::string &body,
                  const std::string &peer, AuthLevel granted,
                  const std::string &reported, const std::string &session,
                  const std::string &host, const std::string &scope)
{
	size_t count = 0;
	const RouteTable *const *tables = allRoutes(&count);
	Response r = dispatchTables(tables, count, m, path, query, body, peer, granted, reported,
	                            session, host, scope);

	/* Here rather than at each handler and each refusal, because what these three say
	   is true of every answer this API gives and a header written at each site is a
	   header missing from the site added next. What an answer already says for itself
	   is left alone. */
	addApiHeaders(r);
	return r;
}

bool routeLevelFor(Method m, const std::string &path, AuthLevel *level, bool *query_token_ok)
{
	if (query_token_ok != NULL)
		*query_token_ok = false;

	if (m == UnknownMethod)
		return false;

	size_t count = 0;
	const RouteTable *const *tables = allRoutes(&count);

	Match found;
	if (matchIn(tables, count, m, path, found) != MatchedRoute)
		return false;

	if (level != NULL)
		*level = found.best->auth;
	if (query_token_ok != NULL)
		*query_token_ok = found.best->query_token_ok;
	return true;
}

} // namespace httpd
