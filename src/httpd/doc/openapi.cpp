/*
 * openapi.cpp - the OpenAPI document, built from the route tables
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

/* What configure wrote, for the one thing wanted from it here: whether this build was
   asked to leave the prose out. The check below says so out loud rather than trusting
   that there is one header of that name in the include path. */
#include <config.h>

#if !defined(PACKAGE_NAME) || !defined(PACKAGE_VERSION)
#error "config.h here is not the one configure wrote"
#endif

#include "openapi.h"

#include "httpd/auth.h"
#include "httpd/endpoint.h"
#include "httpd/endpoints.h"
#include "httpd/events.h"
#include "httpd/http.h"
#include "httpd/json.h"
#include "httpd/router.h"
#include "httpd/schema.h"
#include "httpd/static.h"

#include "coreapi/base/version.h"

#include <cstdio>
#include <cstring>
#include <string>
#include <utility>
#include <vector>

#include <OpenThreads/Mutex>
#include <OpenThreads/ScopedLock>

#include <zlib.h>

namespace httpd
{

namespace openapi
{

namespace
{

const char kPath[] = "/api/openapi.json";

/* 3.1 and not 3.0 because a member alongside a reference means something in 3.1
   and is ignored in 3.0, and what a member of an answer says about itself is
   written alongside the reference to the shape of it. */
const char kOpenApiVersion[] = "3.1.0";

// The key the refusal every layer here answers with is filed under, taken
// before the tables are read so that a shape of theirs called the same thing
// is the one that gets moved rather than the one every operation points at.
const char kProblemKey[] = "problem";

/* What the third way of presenting a credential is filed under, which is not the name
   the credential travels under: the scheme beside it is already called after the header
   it uses, and two schemes under one key is one scheme. The name on the wire is stated
   inside it, from the one place that spells it. */
const char kQueryTokenScheme[] = "query_token";

/* As deep as a chain of shapes is walked, the same bound the check over a shape uses,
   so a table this walk accepts that check accepts too. Without a bound the walk would
   answer nothing at all for a pair of tables naming each other. */
const size_t kMaxSchemaDepth = 8;

/* Room asked for once, before the walk, and deliberately above what the document comes
   to: the whole point of appending rather than building a tree of nodes is that the
   answer costs the bytes of the answer, and a growth step that copies fifty kilobytes
   several times is that cost paid again. What is kept afterwards is a buffer of
   exactly the document's size, so the slack is local to the build. */
const size_t kBuildReserve = 256 * 1024;

/* Whether an operation needs the second token. A route that changes something and is
   reached without presenting anything is refused already, and Get and Options never
   carry it.

   Not every caller who needs one needs it from this: a bearer token answers for itself,
   so what this names is a parameter a session-cookie caller fills in and a bearer-token
   caller may leave empty, which is why it is written as optional below. */
bool needsCsrfHeader(const Endpoint &ep)
{
	return ep.auth != AuthLevel::Public && ep.method != Get && ep.method != Options;
}

/* The name of the member an operation is written under, which the format spells in
   lower case where the request line spells it in upper. NULL for the three a route
   never declares, so a table carrying one puts no member into the document rather than
   a member no reader has a meaning for. */
const char *methodKey(Method m)
{
	switch (m)
	{
		case Get:           return "get";
		case Post:          return "post";
		case Put:           return "put";
		case Patch:         return "patch";
		case Delete:        return "delete";
		case Options:       return "options";
		case Head:          return NULL;
		case UnknownMethod: return NULL;
	}
	return NULL;
}

// Everything a name may carry into a key and a reference, and one underscore
// for everything else, so that a shape named with a slash or a tilde cannot
// make a reference point at a place the document does not have.
std::string sanitised(const char *name)
{
	std::string out;
	for (const char *p = name; p != NULL && *p != '\0'; ++p)
	{
		const char c = *p;
		const bool plain = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
		                   (c >= '0' && c <= '9') || c == '.' || c == '-' || c == '_';
		out += plain ? c : '_';
	}
	if (out.empty())
		out = "schema";
	return out;
}

// One shape and the key the document files it under.
struct SchemaRef
{
	const Schema *schema;
	std::string   key;
};

/* A key nothing else has taken. Two shapes with one name is a table this layer wrote
   wrong and not something the document can be made right about, so the second is filed
   one along rather than written over the first: a reference points at the shape it was
   made for. */
std::string freeKey(const std::string &base, const std::vector<SchemaRef> &taken)
{
	std::string key = base;
	unsigned n = 1;
	for (;;)
	{
		bool used = (key == kProblemKey);
		for (size_t i = 0; i < taken.size() && !used; ++i)
			used = (taken[i].key == key);
		if (!used)
			return key;

		++n;
		char suffix[16];
		std::snprintf(suffix, sizeof(suffix), "-%u", n);
		key = base + suffix;
	}
}

// Every shape an endpoint reaches, itself and the ones its members name, each
// once and by address: two members naming one shape are one entry, and a shape
// naming itself is one entry as well, which is what ends the walk.
void collect(const Schema *s, std::vector<SchemaRef> &out, size_t depth)
{
	if (s == NULL || depth > kMaxSchemaDepth)
		return;
	for (size_t i = 0; i < out.size(); ++i)
	{
		if (out[i].schema == s)
			return;
	}

	SchemaRef r;
	r.schema = s;
	r.key = freeKey(sanitised(s->name), out);
	out.push_back(r);

	if (s->fields == NULL)
		return;
	for (size_t i = 0; i < s->count; ++i)
		collect(s->fields[i].nested, out, depth + 1);
}

/* Where a shape is filed, and empty for one the walk above never reached. Empty rather
   than a reference built on the spot, a reference to a key the document does not carry
   being a document a reader stops on. What is written instead is a member with no shape
   stated, which says less and is still readable. */
std::string refFor(const Schema *s, const std::vector<SchemaRef> &refs)
{
	for (size_t i = 0; i < refs.size(); ++i)
	{
		if (refs[i].schema == s)
			return std::string("#/components/schemas/") + refs[i].key;
	}
	return std::string();
}

// The identifier an operation is known by, which is its method and its path
// with everything that is not a letter or a digit turned into a separator, so
// that two routes cannot arrive at one name and a name never has to be typed.
std::string operationId(const Endpoint &ep)
{
	std::string out = methodKey(ep.method) != NULL ? methodKey(ep.method) : "unknown";
	// False, so that the separator the path opens with is the one that keeps
	// the method from running into the first segment of it.
	bool last_was_break = false;
	for (const char *p = ep.path; p != NULL && *p != '\0'; ++p)
	{
		const char c = *p;
		const bool plain = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
		                   (c >= '0' && c <= '9');
		if (plain)
		{
			out += c;
			last_was_break = false;
			continue;
		}
		// A run of them is one separator, so a path written with a brace round
		// a segment and one written without arrive at different names for a
		// reason a reader can see rather than at the same name.
		if (!last_was_break)
		{
			out += '_';
			last_was_break = true;
		}
	}
	if (!out.empty() && out[out.size() - 1] == '_')
		out.erase(out.size() - 1);
	return out;
}

/* What an identifier reads as, prefix and all, in one place. The row that declares a
   parameter and the row that describes a member of an answer are two places one
   identifier is described, and a generated reader reads the same value through both. */
const char kChannelIdPattern[] = "^(0[xX])?[0-9a-fA-F]{1,16}$";

// A comma separated set as the array the format states it in, written into an
// object the caller has already opened.
void appendEnum(Json &j, const char *values)
{
	j.key("enum");
	j.beginArray();
	for (const char *q = values; q != NULL && *q != '\0';)
	{
		const char *e = std::strchr(q, ',');
		const size_t n = (e != NULL) ? (size_t)(e - q) : std::strlen(q);
		j.value(std::string(q, n));
		if (e == NULL)
			break;
		q = e + 1;
	}
	j.endArray();
}

/* A set the build decides, asked for where the document is written and stated nowhere
   in the tables. Left off altogether when nothing comes back rather than written empty:
   an empty set is a value nothing could carry, and a reader would turn down everything.
   One function for a route's own segment and for a member of an answer, which keeps the
   two from being written out differently. */
void appendAskedEnum(Json &j, void (*asks)(std::vector<std::string> &))
{
	std::vector<std::string> asked;
	asks(asked);
	if (asked.empty())
		return;

	j.key("enum");
	j.beginArray();
	for (size_t i = 0; i < asked.size(); ++i)
		j.value(asked[i]);
	j.endArray();
}

/* What a value of this kind looks like on the wire, written into an object the caller has
   already opened. A parameter off a query or a path is text whatever it is declared as and
   the format says how a client is to write that text; a value in a body is the JSON kind
   named here.

   with_bounds is false for the one caller whose row counts what a body carries rather than
   bounding a value, where writing those numbers would put the ceiling of a list onto each
   of its elements. */
void appendParamType(Json &j, const Param &p, bool with_bounds = true)
{
	const bool bounded = with_bounds && (p.min != 0 || p.max != 0);

	switch (p.type)
	{
		case ParamType::Int:
			j.key("type");
			j.value("integer");
			if (bounded)
			{
				j.key("minimum");
				j.value(p.min);
				j.key("maximum");
				j.value(p.max);
			}
			return;
		case ParamType::UInt:
			j.key("type");
			j.value("integer");
			j.key("minimum");
			// A row bounding one of these below zero is refused where the
			// tables are checked, so the floor is the row's when it has one
			// and nought when it has none, and never a negative number.
			j.value((bounded && p.min > 0) ? p.min : 0L);
			if (bounded)
			{
				j.key("maximum");
				j.value(p.max);
			}
			return;
		case ParamType::Bool:
			j.key("type");
			j.value("boolean");
			return;
		case ParamType::String:
			j.key("type");
			j.value("string");
			// A set the build decides, asked for here and stated nowhere in
			// the tables.
			if (p.choices != NULL)
				appendAskedEnum(j, p.choices);
			if (with_bounds && p.max > 0)
			{
				/* Bytes and not characters, which is what the check counts. The format says
				   characters, so a ceiling written as one would let a client through with a value this
				   server turns down the moment the text is not plain ASCII. */
				j.key("maxLength");
				j.value(p.max);
				j.key("x-max-bytes");
				j.value(p.max);
			}
			return;
		case ParamType::Enum:
			j.key("type");
			j.value("string");
			appendEnum(j, p.values);
			return;
		case ParamType::ChannelId:
			/* Text and not a number, and that is the whole of why it is its own kind: sixty four
			   bits wide, where the numbers a browser parses out of JSON carry fifty three, and an
			   identifier is above that line most of the time it is not zero. */
			j.key("type");
			j.value("string");
			j.key("pattern");
			j.value(kChannelIdPattern);
			return;
		case ParamType::Time:
			j.key("type");
			j.value("integer");
			// Not one of the formats the specification lists, and named rather than left to the
			// prose, which is what a build without it drops: a reader that renders a moment as one
			// has to be able to tell which of the numbers are moments.
			j.key("format");
			j.value("unix-time");
			if (bounded)
			{
				j.key("minimum");
				j.value(p.min);
				j.key("maximum");
				j.value(p.max);
			}
			return;
	}
	/* No default above, and an unhandled enumerator is an error in this
	   directory, so a kind added without a description stops the build and
	   never reaches this line. */
}

/* What the plain values of a list are, written into an object the caller has opened. A
   kind of its own and not the member's, so nothing here can describe an element as an
   object or as a list: there is no second shape beside a member to describe it with. */
void appendElementType(Json &j, ElementType e)
{
	switch (e)
	{
		case ElementType::None:
			// A list of plain values that says nothing about them is refused
			// where shapes are checked, so this is a shape nothing reaches
			// with. Written as nothing rather than as a guess at what it is.
			return;
		case ElementType::Bool:
			j.key("type");
			j.value("boolean");
			return;
		case ElementType::Int:
			j.key("type");
			j.value("integer");
			return;
		case ElementType::UInt:
			j.key("type");
			j.value("integer");
			j.key("minimum");
			j.value(0L);
			return;
		case ElementType::Number:
			j.key("type");
			j.value("number");
			return;
		case ElementType::String:
			j.key("type");
			j.value("string");
			return;
		case ElementType::Time:
			j.key("type");
			j.value("integer");
			j.key("format");
			j.value("unix-time");
			return;
		case ElementType::ChannelId:
			j.key("type");
			j.value("string");
			j.key("pattern");
			j.value(kChannelIdPattern);
			return;
	}
	/* No default above, for the reason the switch over parameters has none. */
}

// One member of an answer, written into an object the caller has opened.
void appendFieldType(Json &j, const FieldDesc &f, const std::vector<SchemaRef> &refs)
{
	switch (f.type)
	{
		case FieldType::Bool:
			j.key("type");
			j.value("boolean");
			return;
		case FieldType::Int:
			j.key("type");
			j.value("integer");
			return;
		case FieldType::UInt:
			j.key("type");
			j.value("integer");
			j.key("minimum");
			j.value(0L);
			return;
		case FieldType::Number:
			j.key("type");
			j.value("number");
			return;
		case FieldType::String:
			j.key("type");
			j.value("string");
			/* The whole of what this member answers, where the handler cannot answer anything
			   else. A set written here that the server may step outside of would make a generated
			   client turn down an answer the box was right to give, which is why every one of these
			   is held to its writer by a guard beside the cases. */
			if (f.values != NULL)
				appendEnum(j, f.values);
			/* The same, where the set is one the build decides. Asked here rather than copied into
			   the row, so the names a reader is handed are the ones the layer below keeps. A read
			   that answers nothing leaves the member described as text. */
			else if (f.asks != NULL)
				appendAskedEnum(j, f.asks);
			return;
		case FieldType::Time:
			j.key("type");
			j.value("integer");
			j.key("format");
			j.value("unix-time");
			return;
		case FieldType::ChannelId:
			// The same pattern the parameter of this kind is described by,
			// from the same place, because it is the same value read back.
			j.key("type");
			j.value("string");
			j.key("pattern");
			j.value(kChannelIdPattern);
			return;
		case FieldType::Object:
		{
			const std::string ref = refFor(f.nested, refs);
			if (!ref.empty())
			{
				j.key("$ref");
				j.value(ref);
			}
			else
			{
				// A member declared an object and carrying no shape is refused where shapes are
				// checked. Written as an object with nothing said about its members rather than as a
				// reference to a key that is not there.
				j.key("type");
				j.value("object");
			}
			return;
		}
		case FieldType::Array:
		{
			j.key("type");
			j.value("array");
			const std::string ref = refFor(f.nested, refs);
			if (!ref.empty())
			{
				j.key("items");
				j.beginObject();
				j.key("$ref");
				j.value(ref);
				j.endObject();
			}
			else
			{
				/* An array of plain values carries no shape, there being no members to name, so what
				   the elements are is the one thing the row itself says. Written out rather than left
				   off: a list of numbers and a list of words are the same member to a reader told
				   nothing. */
				j.key("items");
				j.beginObject();
				appendElementType(j, f.element);
				j.endObject();
			}
			return;
		}
	}
	/* No default above, for the reason the switch over parameters has none. */
}

// One shape as the object the document files under its key.
void appendSchema(Json &j, const Schema &s, const std::vector<SchemaRef> &refs, bool prose)
{
	j.beginObject();
	j.key("type");
	j.value("object");

	j.key("properties");
	j.beginObject();
	for (size_t i = 0; i < s.count && s.fields != NULL; ++i)
	{
		const FieldDesc &f = s.fields[i];
		if (f.name == NULL || f.name[0] == '\0')
			continue;
		j.key(f.name);
		j.beginObject();
		appendFieldType(j, f, refs);
		if (prose && f.doc != NULL && f.doc[0] != '\0')
		{
			j.key("description");
			j.value(f.doc);
		}
		j.endObject();
	}
	j.endObject();

	/* Only the members that are always there. A member marked absent from some
	   answers is the exception here and is the one a reader has to test for,
	   which is exactly what leaving it out of this list says. */
	size_t required = 0;
	for (size_t i = 0; i < s.count && s.fields != NULL; ++i)
	{
		if (!s.fields[i].optional && s.fields[i].name != NULL && s.fields[i].name[0] != '\0')
			++required;
	}
	if (required > 0)
	{
		j.key("required");
		j.beginArray();
		for (size_t i = 0; i < s.count && s.fields != NULL; ++i)
		{
			const FieldDesc &f = s.fields[i];
			if (!f.optional && f.name != NULL && f.name[0] != '\0')
				j.value(f.name);
		}
		j.endArray();
	}

	/* Every member an answer carries is one the shape names, which is held to
	   by the cases that compare an answer with its shape, so a reader is right
	   to refuse one that carries anything else. */
	j.key("additionalProperties");
	j.value(false);
	j.endObject();
}

// The refusal every layer of this server answers with, which is one document
// and is written here once rather than under each of the operations.
void appendProblemSchema(Json &j, bool prose)
{
	static const char *const kNames[] = { "type", "title", "status", "detail" };
	static const char *const kDocs[] = {
		"the kind of refusal, as a path carrying the code a client branches on",
		"what that code is called",
		"the status this answer was sent with",
		"what was found wrong, which is the one member here that quotes what the caller sent"
	};

	j.beginObject();
	j.key("type");
	j.value("object");
	j.key("properties");
	j.beginObject();
	for (size_t i = 0; i < 4; ++i)
	{
		j.key(kNames[i]);
		j.beginObject();
		j.key("type");
		j.value(std::strcmp(kNames[i], "status") == 0 ? "integer" : "string");
		if (prose)
		{
			j.key("description");
			j.value(kDocs[i]);
		}
		j.endObject();
	}
	j.endObject();
	j.key("required");
	j.beginArray();
	for (size_t i = 0; i < 4; ++i)
		j.value(kNames[i]);
	j.endArray();
	j.key("additionalProperties");
	j.value(false);
	j.endObject();
}

/* The row that names the whole of a body, and NULL for a route whose body is named
   members or is nothing. At most one, a route declaring two or declaring one beside a
   named member being refused where the tables are checked. */
const Param *wholeBodyRow(const Endpoint &ep)
{
	for (size_t i = 0; i < ep.param_count; ++i)
	{
		const Param &p = ep.params[i];
		if (p.name != NULL && p.name[0] != '\0' && namesWholeBody(p.in))
			return &p;
	}
	return NULL;
}

// Whether a row is drawn in the list of parameters, which is the two placements
// a caller writes beside the body and not everything that is not a body member.
bool isDrawnAsParameter(In in)
{
	return in == In::Path || in == In::Query;
}

void appendParameters(Json &j, const Endpoint &ep, bool prose)
{
	const bool csrf = needsCsrfHeader(ep);

	/* Counted by what a row is and not by what it is not, so a placement added later is
	   left out of this list until somebody says what it is rather than written into it as a
	   query component. A field drawn for one of those would be a field a caller fills in
	   and this server has never heard of. */
	size_t carried = csrf ? 1 : 0;
	for (size_t i = 0; i < ep.param_count; ++i)
		carried += isDrawnAsParameter(ep.params[i].in) ? 1 : 0;
	if (carried == 0)
		return;

	j.key("parameters");
	j.beginArray();
	for (size_t i = 0; i < ep.param_count; ++i)
	{
		const Param &p = ep.params[i];
		if (!isDrawnAsParameter(p.in) || p.name == NULL || p.name[0] == '\0')
			continue;

		j.beginObject();
		j.key("name");
		j.value(p.name);
		j.key("in");
		j.value(p.in == In::Path ? "path" : "query");
		/* A segment the path binds is always there, so a parameter carried by the
		   path is required whatever the row says. A row saying otherwise is refused
		   where the tables are checked. */
		j.key("required");
		j.value(p.in == In::Path ? true : p.required);
		if (prose && p.doc != NULL && p.doc[0] != '\0')
		{
			j.key("description");
			j.value(p.doc);
		}
		j.key("schema");
		j.beginObject();
		appendParamType(j, p);
		j.endObject();
		j.endObject();
	}
	/* The same header x-csrf-header above names, drawn here rather than left for a reader
	   to notice the two have to agree.

	   Not required: it answers for a session cookie, and a caller presenting a bearer token
	   needs no second one. A schema cannot say "required with one credential and not the
	   other", and the direction to be wrong in is the one that does not reject a bearer
	   caller's otherwise complete request. */
	if (csrf)
	{
		j.beginObject();
		j.key("name");
		j.value(csrfHeaderName());
		j.key("in");
		j.value("header");
		j.key("required");
		j.value(false);
		if (prose)
		{
			j.key("description");
			j.value("the second token a session cookie needs beside it on a request that "
				"changes something, and never asked of a caller presenting a bearer token "
				"instead");
		}
		j.key("schema");
		j.beginObject();
		j.key("type");
		j.value("string");
		j.endObject();
		j.endObject();
	}
	j.endArray();
}

void appendRequestBody(Json &j, const Endpoint &ep, bool prose)
{
	/* A body that is one whole thing rather than a set of named values, which is the one
	   shape the members below cannot state. Written out of the row that declares it and out
	   of nothing beside it: a sentence typed here naming a path would be a second statement
	   of a route, and the two part company the first time one is edited. */
	const Param *const whole = wholeBodyRow(ep);
	if (whole != NULL)
	{
		j.key("requestBody");
		j.beginObject();
		j.key("required");
		j.value(whole->required);
		/* On the body and not on a member of it: there are no members here for
		   the words to sit beside, and what the row says about itself is the
		   only thing that says what to send. */
		if (prose && whole->doc != NULL && whole->doc[0] != '\0')
		{
			j.key("description");
			j.value(whole->doc);
		}
		j.key("content");
		j.beginObject();
		/* A body of bytes is not a document, so it is not written under the
		   kind a document travels as. Everything else here is, and says so in
		   one place rather than beside each shape. */
		j.key(whole->in == In::BodyBytes ? "application/octet-stream"
						 : "application/json");
		j.beginObject();
		j.key("schema");
		j.beginObject();
		if (whole->in == In::BodyBytes)
		{
			/* What the format says and the type does not: these are bytes and
			   not text, and a reader that ignores the format is left with the
			   only kind the format language has for a run of bytes. */
			j.key("type");
			j.value("string");
			j.key("format");
			j.value("binary");
			if (whole->min > 0)
			{
				j.key("minLength");
				j.value(whole->min);
			}
			if (whole->max > 0)
			{
				j.key("maxLength");
				j.value(whole->max);
			}
		}
		else if (whole->in == In::BodyList)
		{
			j.key("type");
			j.value("array");
			if (whole->min > 0)
			{
				j.key("minItems");
				j.value(whole->min);
			}
			if (whole->max > 0)
			{
				j.key("maxItems");
				j.value(whole->max);
			}
			j.key("items");
			j.beginObject();
			// Without the row's bounds: those count elements and would read
			// here as a ceiling on each one of them.
			appendParamType(j, *whole, false);
			j.endObject();
		}
		else
		{
			j.key("type");
			j.value("object");
			if (whole->min > 0)
			{
				j.key("minProperties");
				j.value(whole->min);
			}
			if (whole->max > 0)
			{
				j.key("maxProperties");
				j.value(whole->max);
			}
			/* What every member carries, there being no names to list. The opposite of the members
			   below, where additionalProperties is false because the router turns down a name no row
			   declares: here the names are the caller's and what is stated is the kind. */
			j.key("additionalProperties");
			j.beginObject();
			appendParamType(j, *whole, false);
			j.endObject();
		}
		j.endObject();
		j.endObject();
		j.endObject();
		j.endObject();
		return;
	}

	size_t carried = 0;
	size_t required = 0;
	for (size_t i = 0; i < ep.param_count; ++i)
	{
		if (ep.params[i].in != In::Body)
			continue;
		++carried;
		required += ep.params[i].required ? 1 : 0;
	}
	if (carried == 0)
		return;

	j.key("requestBody");
	j.beginObject();
	// A body carrying nothing that has to be there is a body a request may
	// leave out altogether, and the reader takes an absent one as an object
	// with no members.
	j.key("required");
	j.value(required > 0);
	j.key("content");
	j.beginObject();
	j.key("application/json");
	j.beginObject();
	j.key("schema");
	j.beginObject();
	j.key("type");
	j.value("object");
	j.key("properties");
	j.beginObject();
	for (size_t i = 0; i < ep.param_count; ++i)
	{
		const Param &p = ep.params[i];
		if (p.in != In::Body || p.name == NULL || p.name[0] == '\0')
			continue;
		j.key(p.name);
		j.beginObject();
		appendParamType(j, p);
		if (prose && p.doc != NULL && p.doc[0] != '\0')
		{
			j.key("description");
			j.value(p.doc);
		}
		j.endObject();
	}
	j.endObject();
	if (required > 0)
	{
		j.key("required");
		j.beginArray();
		for (size_t i = 0; i < ep.param_count; ++i)
		{
			const Param &p = ep.params[i];
			if (p.in == In::Body && p.required && p.name != NULL && p.name[0] != '\0')
				j.value(p.name);
		}
		j.endArray();
	}
	// A name the route does not declare is refused before a handler is
	// entered, so a reader is right to refuse a body that carries one.
	j.key("additionalProperties");
	j.value(false);
	j.endObject();
	j.endObject();
	j.endObject();
	j.endObject();
}

void appendResponses(Json &j, const Endpoint &ep, const std::vector<SchemaRef> &refs)
{
	j.key("responses");
	j.beginObject();

	/* The range and not a number. The tables declare what an answer carries and not what
	   it is sent with, and the codes this server answers with include the one for a thing
	   that was made and the one for a message handed to the box's own loop, so a document
	   naming 200 for every route would be wrong about the ones that do not answer it. The
	   description is the format's and not the tables': a Response Object without one is not
	   a document a reader accepts, so it stays in a build compiled without the prose. */
	j.key("2XX");
	j.beginObject();
	j.key("description");
	j.value("the answer");
	if (events::isStreamRoute(ep))
	{
		/* A stream and not a document, so what is stated is the media type and nothing about a
		   body: the answer never ends, and the frames carry their own shape which no table here
		   declares. Without this a generated client is told the route answers nothing. */
		j.key("content");
		j.beginObject();
		j.key(events::streamContentType());
		j.beginObject();
		j.key("schema");
		j.beginObject();
		j.key("type");
		j.value("string");
		j.endObject();
		j.endObject();
		j.endObject();
	}
	else if (ep.schema != NULL)
	{
		const std::string ref = refFor(ep.schema, refs);
		j.key("content");
		j.beginObject();
		j.key("application/json");
		j.beginObject();
		j.key("schema");
		j.beginObject();
		if (!ref.empty())
		{
			j.key("$ref");
			j.value(ref);
		}
		else
		{
			j.key("type");
			j.value("object");
		}
		j.endObject();
		j.endObject();
		j.endObject();
	}
	j.endObject();

	// One refusal for every way this server turns a request down, pointed at
	// rather than written out per operation, so the document carries it once.
	j.key("default");
	j.beginObject();
	j.key("$ref");
	j.value("#/components/responses/problem");
	j.endObject();

	j.endObject();
}

void appendOperation(Json &j, const Endpoint &ep, const std::vector<SchemaRef> &refs, bool prose,
                     const char *tag)
{
	j.beginObject();

	j.key("operationId");
	j.value(operationId(ep));

	/* The group this route is read and generated under, from the table it is in and from
	   nothing written per route, so a route moved to another table moves with it. Written
	   whether or not this build carries the prose, because a mark is not prose: a generated
	   client turns it into the name of a class, and a document without them puts every route
	   this box has into one. */
	if (tag != NULL && tag[0] != '\0')
	{
		j.key("tags");
		j.beginArray();
		j.value(tag);
		j.endArray();
	}
	if (prose && ep.summary != NULL && ep.summary[0] != '\0')
	{
		j.key("summary");
		j.value(ep.summary);
	}

	/* What a caller has to be, as the level the route declares rather than as the
	   schemes below. The two say different things: the schemes say what a credential
	   looks like, and this says how much of one is wanted. */
	j.key("x-auth-level");
	j.value(authLevelName(ep.auth));

	j.key("security");
	j.beginArray();
	if (ep.auth != AuthLevel::Public)
	{
		j.beginObject();
		j.key("session");
		j.beginArray();
		j.endArray();
		j.endObject();
		j.beginObject();
		j.key("token");
		j.beginArray();
		j.endArray();
		j.endObject();
		/* Named only where the row says the route takes one, and so absent from
		   every other route in this document. A scheme named everywhere would tell
		   a client it may put a credential in the address of any route it likes,
		   which is a client writing a secret into a history list for routes that
		   never read one. */
		if (ep.query_token_ok)
		{
			j.beginObject();
			j.key(kQueryTokenScheme);
			j.beginArray();
			j.endArray();
			j.endObject();
		}
	}
	j.endArray();

	/* A cookie is attached by the browser to whatever asks for it, so a request that
	   changes something and arrived on one has to carry the second token as well. A
	   generated client not told that would fail every write it makes on a session, so the
	   header is named here rather than left to prose a build can be compiled without. */
	if (needsCsrfHeader(ep))
	{
		j.key("x-csrf-header");
		j.value(csrfHeaderName());
	}

	appendParameters(j, ep, prose);
	appendRequestBody(j, ep, prose);
	appendResponses(j, ep, refs);

	j.endObject();
}

bool tableUsable(const RouteTable *t)
{
	return t != NULL && (t->count == 0 || t->endpoints != NULL);
}

/* The marks this document groups its routes under, in the order they are meant to be met,
   and what each one covers.

   The order is the point of the list. A tool reading a document that declares no order
   sorts the groups by name, and then the way in is at whatever letter it happens to begin
   with. This order is the one somebody meets a box in: sign in, see what is on, change
   what is on, record it, then the screen in front of the person, then what the box is and
   how it is run, then the parts a client reaches past the interface, and the document
   about the document last. It is deliberately neither the alphabet nor the order the
   tables stand in routes.cpp, which is a dispatch order.

   The marks themselves are not kept here. Each table carries its own, and this list only
   says in what order and with what sentence. A mark named here that no table carries is
   left out rather than made into an empty group, and a mark a table carries that is not
   named here is written after these. What holds the two together is a case that compares
   both ways. */
struct TagDoc
{
	const char *name;
	const char *what;
};

const TagDoc kTags[] = {
	{ "auth",
	  "Opening a session, saying what the request carries, and closing it. Everything that "
	  "changes this box wants one of these first, and a caller that skips them meets a refusal "
	  "rather than an answer." },
	{ "channels",
	  "What this box can tune to and what it is tuned to now, the bouquets that group and order "
	  "them, the logos beside them, and switching over." },
	{ "epg",
	  "The guide: what is on now, what is on one channel later, a window across several channels "
	  "at once, a search over all of it, and what an outside guide file contributed." },
	{ "timers",
	  "What this box has been told to do later, and the records that say so: record, switch over, "
	  "stand by, and wake." },
	{ "recordings",
	  "What is being recorded at this moment, timeshift included, and stopping one of them." },
	{ "osd",
	  "The screen somebody is sitting in front of: the message on it, the volume, the keys a "
	  "remote sends, and a picture of what it shows." },
	{ "settings",
	  "The box's own settings, as the sections they are kept in, the rows each section declares, "
	  "and the shape a caller has to send to change one." },
	{ "system",
	  "What this box is and how it is doing: its build, its load, its time, the programs it runs, "
	  "and the four calls that stop or restart it." },
	{ "webserver",
	  "The server answering this request, as it is set up: the port and the address it listens "
	  "on, the one account it takes a password under, who reads from it without presenting one, "
	  "and whether it hands channel pictures over. The only part of this box that can cut the "
	  "way it is being changed through." },
	{ "daemons",
	  "Starting, stopping and restarting one of the programs beside the one answering here. What "
	  "they are and how they are doing is under system." },
	{ "storage",
	  "The places this box writes to and the files on them: the mounts it knows, a directory, and "
	  "one file read or written whole." },
	{ "netfs",
	  "The filesystems this box mounts from another machine, as the eight entries each of its two "
	  "tables holds rather than as the lines of a file: the ones mounted at start up and kept, and "
	  "the ones the automounter brings up when somebody looks at them. Passwords go in and never "
	  "come back out." },
	{ "tuner",
	  "The tuners this hardware has, what each of them is doing, the signal it is seeing, and "
	  "putting them down and up again for a picture that has gone." },
	{ "plugins",
	  "The plugins installed on this box and starting one of them." },
	{ "stream",
	  "Where a client fetches live television and radio: an address this box hands over so the "
	  "picture travels from the source to the browser and not through here." },
	{ "webtv",
	  "The channels this box plays from an address somewhere else rather than off an aerial, "
	  "fetched here and handed on under this box's own name, so that a browser talks to one "
	  "server instead of two. No route here takes an address: it takes a channel of this box, or "
	  "a name this box minted for an address it read out of that channel's playlist." },
	{ "config",
	  "The box's configuration documents as whole files, read and written as they are on disk, "
	  "and telling the program to read one again. Not the settings above: these are the files "
	  "underneath them, and nothing here checks what is put in one." },
	{ "events",
	  "The one stream a client stays connected to, to be told what changed instead of asking "
	  "again." },
	{ "openapi",
	  "This document, as the box builds it out of the tables above rather than out of anything "
	  "written by hand, and the one route that hands it over." },
};

const size_t kTagCount = sizeof(kTags) / sizeof(kTags[0]);

// Whether any table that contributes an operation carries this mark, so that a
// mark named above and carried by nobody is not written into an empty group.
bool tagInTables(const RouteTable *const *tables, size_t table_count, const char *tag)
{
	for (size_t t = 0; t < table_count; ++t)
	{
		if (!tableUsable(tables[t]) || tables[t]->count == 0)
			continue;
		if (tables[t]->tag != NULL && std::strcmp(tables[t]->tag, tag) == 0)
			return true;
	}
	return false;
}

bool alreadyNamed(const std::vector<const char *> &written, const char *tag)
{
	for (size_t i = 0; i < written.size(); ++i)
	{
		if (std::strcmp(written[i], tag) == 0)
			return true;
	}
	return false;
}

void appendTag(Json &j, const char *name, const char *what)
{
	j.beginObject();
	j.key("name");
	j.value(name);
	if (what != NULL && what[0] != '\0')
	{
		j.key("description");
		j.value(what);
	}
	j.endObject();
}

// Whether an earlier route in the walk already named this path, which is what
// keeps the operations of one path in one member without anything having to be
// sorted or held in a second list.
bool pathSeenBefore(const RouteTable *const *tables, size_t table_count,
                    size_t table, size_t index)
{
	const char *const path = tables[table]->endpoints[index].path;
	if (path == NULL)
		return false;
	for (size_t t = 0; t <= table && t < table_count; ++t)
	{
		if (!tableUsable(tables[t]))
			continue;
		const size_t last = (t == table) ? index : tables[t]->count;
		for (size_t i = 0; i < last; ++i)
		{
			const char *const other = tables[t]->endpoints[i].path;
			if (other != NULL && std::strcmp(other, path) == 0)
				return true;
		}
	}
	return false;
}

} // namespace

const char *documentPath()
{
	return kPath;
}

bool descriptionsCompiledIn()
{
#ifdef DISABLE_API_DOC
	return false;
#else
	return true;
#endif
}

void appendDocument(std::string &out, const RouteTable *const *tables,
                    size_t table_count, bool with_descriptions)
{
	if (tables == NULL)
		table_count = 0;

	// Every shape any route names, gathered before anything is written,
	// because an operation points at a key and the key has to have been
	// settled by the time it does.
	std::vector<SchemaRef> refs;
	for (size_t t = 0; t < table_count; ++t)
	{
		if (!tableUsable(tables[t]))
			continue;
		for (size_t i = 0; i < tables[t]->count; ++i)
			collect(tables[t]->endpoints[i].schema, refs, 0);
	}

	Json j(out, kBuildReserve);
	j.beginObject();

	j.key("openapi");
	j.value(kOpenApiVersion);

	j.key("info");
	j.beginObject();
	j.key("title");
	j.value("Neutrino CoreAPI");
	{
		char version[32];
		std::snprintf(version, sizeof(version), "%d.%d",
		              NEUTRINO_API_VERSION_MAJOR, NEUTRINO_API_VERSION_MINOR);
		j.key("version");
		j.value(version);
	}
	if (with_descriptions)
	{
		j.key("description");
		j.value("Every route this box answers, written out of the tables the server is built from.");
	}
	j.endObject();

	/* One server and it is the box this document came off. Written as a path and not as a
	   name, because the document is fetched from the box and the name a client reached it
	   under is the name it should keep asking under: a box has several, and one written here
	   would be this build's answer to a question the caller already answered. */
	j.key("servers");
	j.beginArray();
	j.beginObject();
	j.key("url");
	j.value("/");
	j.endObject();
	j.endArray();

	/* Written before the paths and not left to a reader to work out, because a
	   document that declares no order gets whatever order the tool reading it
	   invents, and the one they all invent is the alphabet. */
	j.key("tags");
	j.beginArray();
	{
		std::vector<const char *> named;
		for (size_t k = 0; k < kTagCount; ++k)
		{
			if (!tagInTables(tables, table_count, kTags[k].name))
				continue;
			appendTag(j, kTags[k].name, with_descriptions ? kTags[k].what : NULL);
			named.push_back(kTags[k].name);
		}
		/* A mark the list above does not name still gets a place, after the ones it does: a
		   table a case put in front of the shipped ones, and a table added without a line in
		   that list, are both better grouped at the end than sorted into the middle. */
		for (size_t t = 0; t < table_count; ++t)
		{
			if (!tableUsable(tables[t]) || tables[t]->count == 0)
				continue;
			const char *const tag = tables[t]->tag;
			if (tag == NULL || tag[0] == '\0' || alreadyNamed(named, tag))
				continue;
			appendTag(j, tag, NULL);
			named.push_back(tag);
		}
	}
	j.endArray();

	j.key("components");
	j.beginObject();

	j.key("securitySchemes");
	j.beginObject();
	j.key("session");
	j.beginObject();
	j.key("type");
	j.value("apiKey");
	j.key("in");
	j.value("cookie");
	j.key("name");
	j.value(sessionCookieName());
	j.endObject();
	j.key("token");
	j.beginObject();
	j.key("type");
	j.value("http");
	j.key("scheme");
	j.value("bearer");
	j.endObject();
	/* The third, and the one a generated client would otherwise have no way of
	   presenting: a token in the query, for the routes whose row says they take one.
	   Stated as a scheme and never as a parameter, which is what says it is a
	   credential: a client turns a parameter into an argument its caller fills in and
	   a scheme into the one place it keeps a credential, and the router drops this
	   name out of the query before it compares what a route declares. */
	j.key(kQueryTokenScheme);
	j.beginObject();
	j.key("type");
	j.value("apiKey");
	j.key("in");
	j.value("query");
	j.key("name");
	j.value(queryTokenName());
	j.endObject();
	j.endObject();

	j.key("responses");
	j.beginObject();
	j.key(kProblemKey);
	j.beginObject();
	j.key("description");
	j.value("the request was turned down");
	j.key("content");
	j.beginObject();
	j.key("application/problem+json");
	j.beginObject();
	j.key("schema");
	j.beginObject();
	j.key("$ref");
	j.value(std::string("#/components/schemas/") + kProblemKey);
	j.endObject();
	j.endObject();
	j.endObject();
	j.endObject();
	j.endObject();

	j.key("schemas");
	j.beginObject();
	j.key(kProblemKey);
	appendProblemSchema(j, with_descriptions);
	for (size_t i = 0; i < refs.size(); ++i)
	{
		j.key(refs[i].key.c_str());
		appendSchema(j, *refs[i].schema, refs, with_descriptions);
	}
	j.endObject();

	j.endObject();

	j.key("paths");
	j.beginObject();
	for (size_t t = 0; t < table_count; ++t)
	{
		if (!tableUsable(tables[t]))
			continue;
		for (size_t i = 0; i < tables[t]->count; ++i)
		{
			const Endpoint &ep = tables[t]->endpoints[i];
			if (ep.path == NULL || ep.path[0] == '\0')
				continue;
			if (pathSeenBefore(tables, table_count, t, i))
				continue;

			j.key(ep.path);
			j.beginObject();
			// Every route of this path and not only this one, because a path
			// answered by two methods is one member here carrying both.
			for (size_t t2 = 0; t2 < table_count; ++t2)
			{
				if (!tableUsable(tables[t2]))
					continue;
				for (size_t i2 = 0; i2 < tables[t2]->count; ++i2)
				{
					const Endpoint &other = tables[t2]->endpoints[i2];
					if (other.path == NULL || std::strcmp(other.path, ep.path) != 0)
						continue;
					const char *const key = methodKey(other.method);
					if (key == NULL)
						continue;
					if (other.param_count > 0 && other.params == NULL)
						continue;
					j.key(key);
					appendOperation(j, other, refs, with_descriptions, tables[t2]->tag);
				}
			}
			j.endObject();
		}
	}
	j.endObject();

	j.endObject();
}

namespace
{

/* What has been built and what it was built from. The list is kept because the document
   describes the tables and a case may install another set. In the product nothing does:
   the tables are constants the linker wrote, so the comparison below never differs and
   the document is built once for the life of the process. */
struct Cache
{
	std::vector<const RouteTable *> from;
	std::string                     doc;
	std::string                     gz;
	size_t                          builds;

	Cache() : builds(0) {}
};

Cache &cache()
{
	static Cache c;
	return c;
}

OpenThreads::Mutex &lock()
{
	static OpenThreads::Mutex m;
	return m;
}

bool builtFrom(const RouteTable *const *tables, size_t table_count)
{
	if (cache().builds == 0)
		return false;
	if (cache().from.size() != table_count)
		return false;
	for (size_t i = 0; i < table_count; ++i)
	{
		if (cache().from[i] != tables[i])
			return false;
	}
	return true;
}

/* The bytes compressed as a gzip member, and false for anything the library would
   not take. False rather than a fault, because what this buys is a smaller answer
   and nothing else: a box whose library refuses answers the plain form. */
bool gzipInto(const std::string &in, std::string &out)
{
	out.clear();
	/* The library counts what it is handed in a thirty two bit type, and a size is that
	   wide where this runs and twice that where it is built. So this is what makes the
	   narrowing below safe rather than the cast, and it is a comparison that can never be
	   true on the box. What it guards against is a table that made a document too big to
	   hand over whole. */
	if (in.size() > (size_t) 0xffffffffUL)
		return false;

	/* Given back however this leaves. The resize below allocates the whole bound at
	   once, which is the one allocation here big enough to fail, and an allocation
	   that cannot be met leaves through a throw rather than through a return. */
	struct Stream
	{
		z_stream s;
		bool     started;

		Stream() : started(false) { std::memset(&s, 0, sizeof(s)); }
		~Stream()
		{
			if (started)
				deflateEnd(&s);
		}

	private:
		Stream(const Stream &);
		Stream &operator=(const Stream &);
	};

	Stream z;
	z_stream &s = z.s;

	// Fifteen for the window and sixteen on top of it, which is what asks for
	// a gzip wrapper rather than a zlib one: the header names the encoding
	// gzip and that is the stream a client expects under it.
	if (deflateInit2(&s, Z_BEST_COMPRESSION, Z_DEFLATED, 15 + 16, 8, Z_DEFAULT_STRATEGY) != Z_OK)
		return false;
	z.started = true;

	std::string buffer;
	buffer.resize((size_t) deflateBound(&s, (uLong) in.size()));

	s.next_in = (Bytef *)(in.data());
	s.avail_in = (uInt) in.size();
	s.next_out = buffer.empty() ? NULL : (Bytef *)(&buffer[0]);
	s.avail_out = (uInt) buffer.size();

	const int r = deflate(&s, Z_FINISH);
	const size_t produced = (size_t) s.total_out;
	if (r != Z_STREAM_END || produced > buffer.size())
		return false;

	// Copied into a buffer of its own size rather than handed over with the
	// room the bound asked for, which is several times what a compressed
	// document comes to and would be held for the life of the process.
	out.assign(buffer.data(), produced);
	return true;
}

// Rebuilds when the list is not the one the answer was built from. Called with
// the lock held.
void rebuildIfNeeded()
{
	size_t table_count = 0;
	const RouteTable *const *tables = allRoutes(&table_count);
	if (builtFrom(tables, table_count))
		return;

	std::string built;
	appendDocument(built, tables, table_count, descriptionsCompiledIn());

	/* Copied out of the buffer the walk ran in, so what is kept is the size of
	   the document and not the room the walk asked for. The walk's buffer is
	   given back at the end of this call. */
	cache().doc.assign(built.data(), built.size());
	std::string().swap(built);

	std::string packed;
	if (gzipInto(cache().doc, packed))
		cache().gz.swap(packed);
	else
		std::string().swap(cache().gz);

	cache().from.assign(tables, tables + table_count);
	++cache().builds;
}

/* The compressed form of these bytes, when these bytes are the document. One hold of
   the lock and one look at the cache: the two are rebuilt together, and a caller that
   read one and then the other would be comparing against a document rebuilt in
   between. */
/* Whether these bytes are the document, asked with the lock held. Nothing is built to
   answer it: a caller reaching here before anything has been built is asking about some
   other answer, the only thing that makes this one building it first. */
bool isCachedDocument(const std::string &body)
{
	if (cache().builds == 0)
		return false;
	rebuildIfNeeded();
	return body.size() == cache().doc.size() && body == cache().doc;
}

bool packedForm(const std::string &body, std::string &packed)
{
	OpenThreads::ScopedLock<OpenThreads::Mutex> held(lock());
	if (!isCachedDocument(body))
		return false;
	if (cache().gz.empty() || cache().gz.size() >= cache().doc.size())
		return false;
	packed = cache().gz;
	return true;
}

} // namespace

const std::string &document()
{
	OpenThreads::ScopedLock<OpenThreads::Mutex> held(lock());
	rebuildIfNeeded();
	return cache().doc;
}

const std::string &documentGzip()
{
	OpenThreads::ScopedLock<OpenThreads::Mutex> held(lock());
	rebuildIfNeeded();
	return cache().gz;
}

size_t buildCount()
{
	OpenThreads::ScopedLock<OpenThreads::Mutex> held(lock());
	return cache().builds;
}

bool isDocument(const Response &r)
{
	if (r.code != StatusOk || r.body.empty())
		return false;
	OpenThreads::ScopedLock<OpenThreads::Mutex> held(lock());
	return isCachedDocument(r.body);
}

Response withEncoding(const Response &r, const std::string &accept_encoding)
{
	if (!acceptsGzip(accept_encoding))
		return r;

	std::string packed;
	if (!packedForm(r.body, packed))
		return r;

	/* Built out of the head of the answer and the compressed bytes, and never copied from
	   the answer and then written over. A copy of the answer is the whole document, which is
	   the one allocation this route exists to stop making per request. */
	Response out;
	out.code = r.code;
	out.content_type = r.content_type;
	out.headers = r.headers;
	out.body.swap(packed);
	out.headers.push_back(std::make_pair(std::string("Content-Encoding"), std::string("gzip")));
	return out;
}

namespace
{

Response serveDocument(const Request &)
{
	Response r = okJson();
	r.body = document();
	/* The answer comes in two forms and which one is sent depends on a header, so anything
	   keeping a copy has to keep them apart. Written here and not where the form is chosen,
	   because the answer varies whether or not this caller took the other form. */
	r.headers.push_back(std::make_pair(std::string("Vary"), std::string("Accept-Encoding")));
	return r;
}

/* Read and not written to, and it publishes the whole of what this server offers, so it
   asks for the least a route can ask for above being open to everybody. Public would put
   the surface in front of whoever can reach the port, and building the document is the
   most memory this server spends on one answer. */
const Endpoint kOpenApiEndpoints[] = {
	{ Method::Get, kPath, AuthLevel::Read,
	  "every route this server answers, as an OpenAPI document",
	  NULL, 0, NULL, &serveDocument, false },
};

} // namespace

// The pair, written where the array is, so that the length beside it is the
// length of the array and not a number somebody kept in step by hand.
extern const RouteTable openapiTable = {
	HTTPD_TABLE("openapi", kOpenApiEndpoints)
};

} // namespace openapi

} // namespace httpd
