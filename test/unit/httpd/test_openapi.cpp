/*
 * test_openapi.cpp - tests for the OpenAPI document
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

#include "support/catch.hpp"
#include "support/counts.h"

/* What configure wrote, read here for the one thing the server also reads from
   it: whether this build was asked to leave the prose out. Reached through the
   build directory, and this directory has no header of that name, so what
   arrives here is the generated one. */
#include <config.h>


#include "httpd/auth.h"
#include "httpd/endpoint.h"
#include "httpd/events.h"
#include "httpd/http.h"
#include "httpd/json.h"
#include "httpd/doc/openapi.h"
#include "httpd/router.h"
#include "httpd/schema.h"

#include "jsoncpp/json/json.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

#include <zlib.h>

using namespace httpd;

namespace
{

/* Cleared going in as well as coming out, so a case elsewhere that failed and
   unwound past its own restore cannot decide what these read. */
struct ShippedRoutes
{
	ShippedRoutes() { setRoutesForTest(NULL); }
	~ShippedRoutes() { setRoutesForTest(NULL); }

	private:
		ShippedRoutes(const ShippedRoutes &);
		ShippedRoutes &operator=(const ShippedRoutes &);
};

/* The tree's own parser, used here as a second opinion. A document only this
   server can read is not one a client can use, and a case that searched the
   bytes for a substring would pass on exactly that: the substring is there in a
   document with a comma missing from it too. */
bool parseDocument(const std::string &doc, ::Json::Value &root)
{
	::Json::CharReaderBuilder builder;
	std::unique_ptr< ::Json::CharReader> reader(builder.newCharReader());
	std::string errs;
	return reader->parse(doc.data(), doc.data() + doc.size(), &root, &errs);
}

::Json::Value parsed(const std::string &doc)
{
	::Json::Value root;
	REQUIRE(parseDocument(doc, root));
	return root;
}

const char *authName(AuthLevel a)
{
	switch (a)
	{
		case AuthLevel::Public: return "public";
		case AuthLevel::Read:   return "read";
		case AuthLevel::Write:  return "write";
		case AuthLevel::System: return "system";
	}
	return "";
}

// Written out here rather than taken from the server, so that the name the
// document uses is compared against a second statement of it and not against
// the one that wrote it.
const char *methodKey(Method m)
{
	switch (m)
	{
		case Get:    return "get";
		case Post:   return "post";
		case Put:    return "put";
		case Patch:  return "patch";
		case Delete: return "delete";
		default:     return NULL;
	}
}

// What a value of this kind is written as, again as a second statement.
std::string wireType(ParamType t)
{
	switch (t)
	{
		case ParamType::Int:       return "integer";
		case ParamType::UInt:      return "integer";
		case ParamType::Time:      return "integer";
		case ParamType::Bool:      return "boolean";
		case ParamType::String:    return "string";
		case ParamType::Enum:      return "string";
		case ParamType::ChannelId: return "string";
	}
	return "";
}

std::string fieldType(FieldType t)
{
	switch (t)
	{
		case FieldType::Bool:   return "boolean";
		case FieldType::Int:    return "integer";
		case FieldType::UInt:   return "integer";
		case FieldType::Number: return "number";
		case FieldType::String: return "string";
		case FieldType::Time:   return "integer";
		case FieldType::ChannelId: return "string";
		case FieldType::Object: return "object";
		case FieldType::Array:  return "array";
	}
	return "";
}

// The operation of this route in the parsed document, and a null value for one
// the document does not carry, so a route left out fails where it is looked up
// rather than where a member of it is read.
::Json::Value operationOf(const ::Json::Value &doc, const Endpoint &ep)
{
	const char *const key = methodKey(ep.method);
	if (key == NULL)
		return ::Json::Value();
	const ::Json::Value paths = doc["paths"];
	if (!paths.isMember(ep.path))
		return ::Json::Value();
	const ::Json::Value item = paths[ep.path];
	if (!item.isMember(key))
		return ::Json::Value();
	return item[key];
}

::Json::Value parameterOf(const ::Json::Value &op, const char *name)
{
	const ::Json::Value list = op["parameters"];
	for (::Json::ArrayIndex i = 0; i < list.size(); ++i)
	{
		if (list[i]["name"].asString() == name)
			return list[i];
	}
	return ::Json::Value();
}

::Json::Value bodyPropertyOf(const ::Json::Value &op, const char *name)
{
	const ::Json::Value schema =
		op["requestBody"]["content"]["application/json"]["schema"];
	if (!schema["properties"].isMember(name))
		return ::Json::Value();
	return schema["properties"][name];
}

bool listedIn(const ::Json::Value &array, const std::string &want)
{
	for (::Json::ArrayIndex i = 0; i < array.size(); ++i)
	{
		if (array[i].asString() == want)
			return true;
	}
	return false;
}

// Whatever a reference names, and a null value for one the document does not
// carry, so a reference that points nowhere fails rather than reads as empty.
::Json::Value pointed(const ::Json::Value &doc, const std::string &ref)
{
	if (ref.compare(0, 2, "#/") != 0)
		return ::Json::Value();
	::Json::Value at = doc;
	size_t i = 2;
	while (i <= ref.size())
	{
		const size_t end = ref.find('/', i);
		std::string seg = ref.substr(i, (end == std::string::npos) ? std::string::npos : end - i);
		// The two escapes a pointer has, undone in the order the format says.
		std::string plain;
		for (size_t k = 0; k < seg.size(); ++k)
		{
			if (seg[k] == '~' && k + 1 < seg.size() && seg[k + 1] == '1')
			{
				plain += '/';
				++k;
			}
			else if (seg[k] == '~' && k + 1 < seg.size() && seg[k + 1] == '0')
			{
				plain += '~';
				++k;
			}
			else
			{
				plain += seg[k];
			}
		}
		if (!at.isObject() || !at.isMember(plain))
			return ::Json::Value();
		at = at[plain];
		if (end == std::string::npos)
			break;
		i = end + 1;
	}
	return at;
}

// Every reference anywhere in the document, so that one written into a corner
// nothing else here reads is still held to pointing at something.
void collectRefs(const ::Json::Value &at, std::vector<std::string> &out)
{
	if (at.isObject())
	{
		const ::Json::Value::Members names = at.getMemberNames();
		for (size_t i = 0; i < names.size(); ++i)
		{
			if (names[i] == "$ref" && at[names[i]].isString())
				out.push_back(at[names[i]].asString());
			else
				collectRefs(at[names[i]], out);
		}
		return;
	}
	if (at.isArray())
	{
		for (::Json::ArrayIndex i = 0; i < at.size(); ++i)
			collectRefs(at[i], out);
	}
}

/* How many of the things the tables put words on carry them here: the summary of an
   operation, what a parameter says about itself, and what a member of a shape says about
   itself.

   Counted where each of them sits and not as members named a certain thing anywhere in
   the document, because one shape has a member of its own called description. A count of
   names would read those as prose. */
size_t proseIn(const ::Json::Value &doc)
{
	size_t n = 0;

	const ::Json::Value paths = doc["paths"];
	const ::Json::Value::Members names = paths.getMemberNames();
	for (size_t i = 0; i < names.size(); ++i)
	{
		const ::Json::Value item = paths[names[i]];
		const ::Json::Value::Members verbs = item.getMemberNames();
		for (size_t j = 0; j < verbs.size(); ++j)
		{
			const ::Json::Value op = item[verbs[j]];
			n += op.isMember("summary") ? 1 : 0;

			const ::Json::Value list = op["parameters"];
			for (::Json::ArrayIndex k = 0; k < list.size(); ++k)
				n += list[k].isMember("description") ? 1 : 0;

			const ::Json::Value body =
				op["requestBody"]["content"]["application/json"]["schema"]["properties"];
			const ::Json::Value::Members carried = body.getMemberNames();
			for (size_t k = 0; k < carried.size(); ++k)
				n += body[carried[k]].isMember("description") ? 1 : 0;

			// The one place the words of a body of its own sit, there being no
			// members for them to sit beside.
			n += op["requestBody"].isMember("description") ? 1 : 0;
		}
	}

	const ::Json::Value shapes = doc["components"]["schemas"];
	const ::Json::Value::Members shape_names = shapes.getMemberNames();
	for (size_t i = 0; i < shape_names.size(); ++i)
	{
		const ::Json::Value props = shapes[shape_names[i]]["properties"];
		const ::Json::Value::Members members = props.getMemberNames();
		for (size_t j = 0; j < members.size(); ++j)
			n += props[members[j]].isMember("description") ? 1 : 0;
	}

	n += doc["info"].isMember("description") ? 1 : 0;
	return n;
}

/* Every constraint a row implies, written out here as a second statement of the rule
   rather than read back off the thing under test. The count is what makes the other
   direction answerable: a document carrying a constraint the row does not imply has one
   member more than this says. */
struct Constraints
{
	std::string              type;
	bool                     has_min;
	long                     min;
	bool                     has_max;
	long                     max;
	bool                     has_max_length;
	bool                     has_pattern;
	bool                     has_format;
	std::vector<std::string> values;

	Constraints()
		: has_min(false), min(0), has_max(false), max(0),
		  has_max_length(false), has_pattern(false), has_format(false) {}

	// type, and one member per thing below that is there. maxLength brings the
	// byte count beside it, which is two.
	size_t members() const
	{
		return 1 + (has_min ? 1u : 0u) + (has_max ? 1u : 0u) +
		       (has_max_length ? 2u : 0u) + (has_pattern ? 1u : 0u) +
		       (has_format ? 1u : 0u) + (values.empty() ? 0u : 1u);
	}
};

std::vector<std::string> splitValues(const char *csv)
{
	std::vector<std::string> out;
	std::string one;
	for (const char *q = csv; q != NULL && *q != '\0'; ++q)
	{
		if (*q == ',')
		{
			out.push_back(one);
			one.clear();
			continue;
		}
		one += *q;
	}
	if (csv != NULL && csv[0] != '\0')
		out.push_back(one);
	return out;
}

Constraints impliedBy(const Param &p)
{
	const bool bounded = (p.min != 0 || p.max != 0);
	Constraints c;
	c.type = wireType(p.type);

	switch (p.type)
	{
		case ParamType::Int:
			if (bounded)
			{
				c.has_min = true; c.min = p.min;
				c.has_max = true; c.max = p.max;
			}
			break;
		case ParamType::UInt:
			// None or more whatever the row says, and the row's floor when it
			// has one. A row bounding one of these below zero is refused where
			// the tables are checked.
			c.has_min = true;
			c.min = (bounded && p.min > 0) ? p.min : 0;
			if (bounded)
			{
				c.has_max = true; c.max = p.max;
			}
			break;
		case ParamType::Time:
			c.has_format = true;
			if (bounded)
			{
				c.has_min = true; c.min = p.min;
				c.has_max = true; c.max = p.max;
			}
			break;
		case ParamType::String:
			c.has_max_length = (p.max > 0);
			/* A set the build decides, which the table does not state and the document
			   asks for. Asked here too rather than written down: a list typed into a
			   case would be right for this box and fail on the next. What this holds
			   is that the document carries what the box answered. */
			if (p.choices != NULL)
				p.choices(c.values);
			break;
		case ParamType::Enum:
			c.values = splitValues(p.values);
			break;
		case ParamType::ChannelId:
			c.has_pattern = true;
			break;
		case ParamType::Bool:
			break;
	}
	return c;
}

// One row against the object the document states for it, in both directions.
void checkConstraints(const ::Json::Value &schema, const Param &p)
{
	const Constraints c = impliedBy(p);

	REQUIRE(schema.isObject());
	REQUIRE(schema["type"].asString() == c.type);

	REQUIRE(schema.isMember("minimum") == c.has_min);
	if (c.has_min)
		REQUIRE(schema["minimum"].asInt64() == (::Json::Int64) c.min);

	REQUIRE(schema.isMember("maximum") == c.has_max);
	if (c.has_max)
		REQUIRE(schema["maximum"].asInt64() == (::Json::Int64) c.max);

	REQUIRE(schema.isMember("maxLength") == c.has_max_length);
	REQUIRE(schema.isMember("x-max-bytes") == c.has_max_length);
	if (c.has_max_length)
	{
		REQUIRE(schema["maxLength"].asInt64() == (::Json::Int64) p.max);
		// The format counts characters where this server counts bytes, so the
		// count it really enforces is stated beside the one the format has.
		REQUIRE(schema["x-max-bytes"].asInt64() == (::Json::Int64) p.max);
	}

	REQUIRE(schema.isMember("pattern") == c.has_pattern);
	if (c.has_pattern)
		REQUIRE(schema["pattern"].asString() == "^(0[xX])?[0-9a-fA-F]{1,16}$");

	REQUIRE(schema.isMember("format") == c.has_format);
	if (c.has_format)
		REQUIRE(schema["format"].asString() == "unix-time");

	REQUIRE(schema.isMember("enum") == !c.values.empty());
	if (!c.values.empty())
	{
		REQUIRE(schema["enum"].size() == (::Json::ArrayIndex) c.values.size());
		for (size_t i = 0; i < c.values.size(); ++i)
			REQUIRE(listedIn(schema["enum"], c.values[i]));
	}

	// Nothing else. A constraint the row does not imply is one more member
	// than this, and every lookup above would still have passed.
	REQUIRE(schema.getMemberNames().size() == c.members());
}

/* The row as it is written into one element of a list or one value of an open
   object. Its two numbers count what the body carries and bound no one thing
   inside it, so what the element is held to is the same row with them taken
   off, and every constraint the pair would have implied is one member more than
   this expects. */
Param withoutBounds(const Param &p)
{
	Param q = p;
	q.min = 0;
	q.max = 0;
	return q;
}

/* The body of this operation as the row that names the whole of it states it, in both
   directions and down to the count of members. Written as a walk over whatever row it
   is handed rather than as a lookup of the two paths that carry one today, because a
   case naming those two would say nothing about the third route to be written this
   way. */
void checkWholeBody(const ::Json::Value &op, const Param &row)
{
	const ::Json::Value body = op["requestBody"];
	REQUIRE(body.isObject());
	REQUIRE(body["required"].asBool() == row.required);
	REQUIRE(body.isMember("description") == openapi::descriptionsCompiledIn());
	if (openapi::descriptionsCompiledIn())
		REQUIRE(body["description"].asString() == std::string(row.doc));

	/* Bytes are not a document, so they are not offered under the kind a
	   document travels as, and a caller reading the document has that
	   difference before it sends anything. */
	const bool bytes = (row.in == In::BodyBytes);
	REQUIRE(body["content"].getMemberNames().size() == 1u);
	const ::Json::Value schema =
		body["content"][bytes ? "application/octet-stream" : "application/json"]["schema"];
	REQUIRE(schema.isObject());

	if (bytes)
	{
		/* The one shape with nothing inside it to describe: what says these
		   are bytes and not text is the format beside the type, there being
		   no other kind in the format language for a run of them. */
		REQUIRE(schema["type"].asString() == "string");
		REQUIRE(schema["format"].asString() == "binary");
		REQUIRE(schema.isMember("minLength") == (row.min > 0));
		REQUIRE(schema.isMember("maxLength") == (row.max > 0));
		if (row.min > 0)
			REQUIRE(schema["minLength"].asInt64() == (::Json::Int64) row.min);
		if (row.max > 0)
			REQUIRE(schema["maxLength"].asInt64() == (::Json::Int64) row.max);

		const size_t counted = 2 + (row.min > 0 ? 1u : 0u) + (row.max > 0 ? 1u : 0u);
		REQUIRE(schema.getMemberNames().size() == counted);
		REQUIRE_FALSE(parameterOf(op, row.name).isObject());
		return;
	}

	// A list and an open object are two shapes, and a reader has to tell them
	// apart before it can send either.
	REQUIRE(schema["type"].asString() ==
	        (row.in == In::BodyList ? "array" : "object"));

	const char *const least = (row.in == In::BodyList) ? "minItems" : "minProperties";
	const char *const most  = (row.in == In::BodyList) ? "maxItems" : "maxProperties";
	REQUIRE(schema.isMember(least) == (row.min > 0));
	REQUIRE(schema.isMember(most) == (row.max > 0));
	if (row.min > 0)
		REQUIRE(schema[least].asInt64() == (::Json::Int64) row.min);
	if (row.max > 0)
		REQUIRE(schema[most].asInt64() == (::Json::Int64) row.max);

	// The element or the value, out of the kind the row names and without the
	// row's own numbers, which count things and not characters.
	const char *const each = (row.in == In::BodyList) ? "items" : "additionalProperties";
	REQUIRE(schema.isMember(each));
	checkConstraints(schema[each], withoutBounds(row));

	// type, the shape's own two numbers where the row has them, and the one
	// member describing what it carries. Nothing else: a member more than this
	// is one every lookup above would still have passed.
	const size_t members = 2 + (row.min > 0 ? 1u : 0u) + (row.max > 0 ? 1u : 0u);
	REQUIRE(schema.getMemberNames().size() == members);

	/* And it is not also drawn as a query component, which is what it would
	   have become had the parameter list been written as everything that is not
	   a body member. */
	REQUIRE_FALSE(parameterOf(op, row.name).isObject());
}

std::string readWholeFile(const char *path)
{
	std::FILE *f = std::fopen(path, "rb");
	if (f == NULL)
		return std::string();
	std::string out;
	char buf[8192];
	for (;;)
	{
		const size_t n = std::fread(buf, 1, sizeof(buf), f);
		if (n == 0)
			break;
		out.append(buf, n);
	}
	std::fclose(f);
	return out;
}

/* The bytes back out of a gzip member, so that what the server calls a
   compressed answer is held to being one a client can read rather than to
   being shorter. A case comparing lengths would pass on any bytes at all. */
bool gunzip(const std::string &in, std::string &out)
{
	out.clear();
	z_stream s;
	std::memset(&s, 0, sizeof(s));
	if (inflateInit2(&s, 15 + 16) != Z_OK)
		return false;

	s.next_in = (Bytef *)(in.data());
	s.avail_in = (uInt) in.size();

	char buf[16384];
	int r = Z_OK;
	do
	{
		s.next_out = (Bytef *) buf;
		s.avail_out = (uInt) sizeof(buf);
		r = inflate(&s, Z_NO_FLUSH);
		if (r != Z_OK && r != Z_STREAM_END)
		{
			inflateEnd(&s);
			return false;
		}
		out.append(buf, sizeof(buf) - s.avail_out);
	}
	while (r != Z_STREAM_END);

	inflateEnd(&s);
	return true;
}

Response probeHandler(const Request &)
{
	Response r;
	r.code = StatusOk;
	r.content_type = "application/json";
	r.body = "{}";
	return r;
}

const FieldDesc kProbeFields[] = {
	HTTPD_MEMBER("one", FieldType::String, "the only member it has"),
};
const Schema kProbeSchema = { "probe-shape", HTTPD_FIELDS(kProbeFields) };

const Param kProbeParams[] = {
	HTTPD_SEGMENT_IN("id", ParamType::UInt, "which one", 1, 9),
};

const Endpoint kProbeEndpoints[] = {
	{ Method::Get, "/api/probe/{id}", AuthLevel::Read, "a route written where this case is",
	  HTTPD_PARAMS(kProbeParams), &kProbeSchema, &probeHandler, false },
};

const RouteTable kProbeTable = {
	HTTPD_TABLE("probe", kProbeEndpoints)
};

/* Two shapes carrying one name, which is a table this layer would have written
   wrong. The document has to stay a document over it: a reference has to reach
   the shape it was made for rather than the other one. */
const FieldDesc kFirstFields[] = {
	HTTPD_MEMBER("a", FieldType::Int, "the first one's member"),
};
const Schema kFirstSchema = { "twice", HTTPD_FIELDS(kFirstFields) };

const FieldDesc kSecondFields[] = {
	HTTPD_MEMBER("b", FieldType::Int, "the second one's member"),
};
const Schema kSecondSchema = { "twice", HTTPD_FIELDS(kSecondFields) };

const Endpoint kClashEndpoints[] = {
	{ Method::Get, "/api/clash/one", AuthLevel::Read, "the first", NULL, 0, &kFirstSchema, &probeHandler, false },
	{ Method::Get, "/api/clash/two", AuthLevel::Read, "the second", NULL, 0, &kSecondSchema, &probeHandler, false },
};

const RouteTable kClashTable = {
	HTTPD_TABLE("clash", kClashEndpoints)
};

} // namespace

TEST_CASE("the document is one a parser reads and names the version it is written to", "[openapi]")
{
	ShippedRoutes shipped;

	const ::Json::Value doc = parsed(openapi::document());
	REQUIRE(doc.isObject());
	REQUIRE(doc["openapi"].asString() == "3.1.0");
	/* That it is named, not what the name is. A title is what a reader sees
	   at the top of the page, and a document whose product was renamed is
	   not a document that broke. */
	REQUIRE_FALSE(doc["info"]["title"].asString().empty());
	REQUIRE_FALSE(doc["info"]["version"].asString().empty());
	REQUIRE(doc["paths"].isObject());
	REQUIRE(doc["components"]["schemas"].isObject());
	REQUIRE(doc["components"]["securitySchemes"].isObject());
}

TEST_CASE("every route the tables carry is in the document, with what the table says about it", "[openapi]")
{
	/* Walked out of the tables and not out of a list typed here, which is the
	   whole of what this document is for: a route added to a table appears
	   because there is no other way for it to get in, and one the emitter
	   passed over fails here. */
	ShippedRoutes shipped;

	const ::Json::Value doc = parsed(openapi::document());
	size_t tables = 0;
	const RouteTable *const *t = allRoutes(&tables);
	REQUIRE(tables > 0);

	size_t described = 0;
	for (size_t i = 0; i < tables; ++i)
	{
		for (size_t j = 0; j < t[i]->count; ++j)
		{
			const Endpoint &ep = t[i]->endpoints[j];
			INFO(methodName(ep.method) << " " << ep.path);

			const ::Json::Value op = operationOf(doc, ep);
			REQUIRE(op.isObject());
			/* A build asked to leave the prose out leaves out the summary and
			   nothing else, so both readings are checked and neither of them
			   is a case that passes because it did not look. */
			REQUIRE(op.isMember("summary") == openapi::descriptionsCompiledIn());
			if (openapi::descriptionsCompiledIn())
				REQUIRE(op["summary"].asString() == std::string(ep.summary));
			REQUIRE(op["x-auth-level"].asString() == std::string(authName(ep.auth)));
			REQUIRE_FALSE(op["operationId"].asString().empty());
			REQUIRE(op["responses"].isMember("default"));

			/* The group, out of the table this route is in and out of nothing written
			   beside the route: a route cannot be filed under a group its own table is
			   not, and a route moved between tables is regrouped by the move. Not
			   behind the prose flag, unlike the summary above: a generated client turns
			   a mark into the name of a class. */
			REQUIRE(op["tags"].isArray());
			REQUIRE(op["tags"].size() == 1u);
			REQUIRE(op["tags"][0].asString() == std::string(t[i]->tag));
			++described;
		}
	}

	recordCount("routes the document describes", described);
}

TEST_CASE("the groups are declared in the order somebody meets this box in", "[openapi]")
{
	/* A document that declares no order gets the order the tool reading it invents, and
	   every one of them invents the alphabet. That would open this API on whichever
	   group happens to start with the earliest letter.

	   Written out here rather than read back off the same list the emitter reads, which
	   would be this case agreeing with itself: the order is a decision somebody made. */
	ShippedRoutes shipped;

	static const char *const kMeant[] = {
		"auth", "channels", "epg", "timers", "recordings", "osd", "settings",
		"system", "webserver", "daemons", "storage", "netfs", "tuner", "plugins",
		"stream", "webtv", "config", "events", "openapi"
	};
	const size_t meant = sizeof(kMeant) / sizeof(kMeant[0]);

	const ::Json::Value doc = parsed(openapi::document());
	const ::Json::Value tags = doc["tags"];
	REQUIRE(tags.isArray());
	REQUIRE(tags.size() == meant);

	for (size_t i = 0; i < meant; ++i)
	{
		INFO("group " << i);
		REQUIRE(tags[(int) i]["name"].asString() == std::string(kMeant[i]));
		/* A sentence each, because a group called config that leaves a reader
		   guessing whether it means the box's settings or the server's is
		   hardly better than no group at all. */
		REQUIRE(tags[(int) i].isMember("description") == openapi::descriptionsCompiledIn());
		if (openapi::descriptionsCompiledIn())
			REQUIRE(tags[(int) i]["description"].asString().size() > 20u);
	}

	/* And the two lists are held to each other in both directions, because
	   they are two: the tables carry the marks and this list orders them. A
	   table added without a line in it would otherwise be grouped at the end
	   with no sentence, quietly, and a line left behind by a table that went
	   away would name a group nothing is in. */
	size_t tables = 0;
	const RouteTable *const *t = allRoutes(&tables);
	for (size_t i = 0; i < tables; ++i)
	{
		INFO(t[i]->tag);
		REQUIRE(t[i]->tag != NULL);
		REQUIRE(t[i]->tag[0] != '\0');
		bool declared = false;
		for (size_t k = 0; k < meant; ++k)
			declared = declared || std::strcmp(t[i]->tag, kMeant[k]) == 0;
		REQUIRE(declared);
	}
	for (size_t k = 0; k < meant; ++k)
	{
		INFO(kMeant[k]);
		bool carried = false;
		for (size_t i = 0; i < tables; ++i)
			carried = carried || std::strcmp(t[i]->tag, kMeant[k]) == 0;
		REQUIRE(carried);
	}

	recordCount("groups the document declares", tags.size());
}

TEST_CASE("every parameter is in the document, where it travels and as what", "[openapi]")
{
	ShippedRoutes shipped;

	const ::Json::Value doc = parsed(openapi::document());
	size_t tables = 0;
	const RouteTable *const *t = allRoutes(&tables);

	size_t described = 0;
	for (size_t i = 0; i < tables; ++i)
	{
		for (size_t j = 0; j < t[i]->count; ++j)
		{
			const Endpoint &ep = t[i]->endpoints[j];
			const ::Json::Value op = operationOf(doc, ep);
			REQUIRE(op.isObject());

			for (size_t k = 0; k < ep.param_count; ++k)
			{
				const Param &p = ep.params[k];
				INFO(ep.path << " " << p.name);

				if (p.in == In::Body)
				{
					::Json::Value member = bodyPropertyOf(op, p.name);
					REQUIRE(member.isObject());
					REQUIRE(member.isMember("description") ==
					        openapi::descriptionsCompiledIn());
					if (openapi::descriptionsCompiledIn())
					{
						REQUIRE(member["description"].asString() == std::string(p.doc));
						// Taken off before the constraints are counted, the
						// prose not being one of them.
						member.removeMember("description");
					}
					checkConstraints(member, p);

					const ::Json::Value schema =
						op["requestBody"]["content"]["application/json"]["schema"];
					REQUIRE(listedIn(schema["required"], p.name) == p.required);
				}
				else if (namesWholeBody(p.in))
				{
					// The body itself, which is neither a member of one nor
					// anything a caller writes beside it.
					checkWholeBody(op, p);
				}
				else
				{
					const ::Json::Value entry = parameterOf(op, p.name);
					REQUIRE(entry.isObject());
					REQUIRE(entry["in"].asString() ==
					        std::string(p.in == In::Path ? "path" : "query"));
					REQUIRE(entry["required"].asBool() == (p.in == In::Path ? true : p.required));
					REQUIRE(entry.isMember("description") ==
					        openapi::descriptionsCompiledIn());
					if (openapi::descriptionsCompiledIn())
						REQUIRE(entry["description"].asString() == std::string(p.doc));
					/* Every constraint the row implies and no other, which is
					   what a client holds a value to before it sends one. All
					   fifty two rows and not the handful a case could name:
					   a bound dropped on any of the rest is a value this server
					   turns down and a client was told to send. */
					checkConstraints(entry["schema"], p);
				}
				++described;
			}
		}
	}
	recordCount("parameters the document describes", described);
}

TEST_CASE("a body of its own is in the document for the routes that declare one and for no others", "[openapi]")
{
	/* The other direction, and the one a case that only looked up what it expected
	   would never ask: a route declaring no body of any kind carries none in the
	   document either, and a route declaring named members carries theirs and not a
	   shape of its own. Walked over the tables, so a third route written this way is
	   answered for the day it is written, and counted at the end, because a walk that
	   found none would pass this whatever the tables held. */
	ShippedRoutes shipped;

	const ::Json::Value doc = parsed(openapi::document());
	size_t tables = 0;
	const RouteTable *const *t = allRoutes(&tables);

	size_t whole = 0;
	for (size_t i = 0; i < tables; ++i)
	{
		for (size_t j = 0; j < t[i]->count; ++j)
		{
			const Endpoint &ep = t[i]->endpoints[j];
			INFO(ep.path);

			const Param *row = NULL;
			bool members = false;
			for (size_t k = 0; k < ep.param_count; ++k)
			{
				if (namesWholeBody(ep.params[k].in))
					row = &ep.params[k];
				members = members || ep.params[k].in == In::Body;
			}

			const ::Json::Value op = operationOf(doc, ep);
			REQUIRE(op.isObject());

			if (row == NULL)
			{
				REQUIRE(op.isMember("requestBody") == members);
				continue;
			}

			// A route naming the whole of its body and a member of one at the
			// same time is refused where the tables are checked, so the two
			// shapes never meet in one operation.
			REQUIRE_FALSE(members);
			checkWholeBody(op, *row);
			++whole;
		}
	}
	REQUIRE(whole == 3);
}

const Param kWholeListParams[] = {
	// A list of plain words and bounded, which the shipped routes do not
	// carry: an identifier reads no bounds at all, so the two routes that ship
	// one would go on being described right with the numbers handed to their
	// elements, and the mistake this is about is exactly that.
	HTTPD_BODY_IS_LIST_OF("words", ParamType::String, "two to five words", 2, 5),
};

const Param kWholeMapParams[] = {
	// An open object of identifiers and bounded by neither end, which the
	// shipped routes do not carry either.
	HTTPD_BODY_IS_MAP_OF("named", ParamType::ChannelId, "a channel per name", 0, 0),
};

const Param kWholeBytesParams[] = {
	// Bytes with both numbers, which the one shipped route of this shape has
	// neither of: written here so that a pair counted as characters of the
	// body is answered for rather than left until a route needs one.
	HTTPD_BODY_IS_BYTES("bytes", "between two and five bytes", 2, 5),
};

const Endpoint kWholeEndpoints[] = {
	{ Method::Post, "/api/probe/list", AuthLevel::Write, "takes a list of words",
	  HTTPD_PARAMS(kWholeListParams), NULL, &probeHandler, false },
	{ Method::Post, "/api/probe/map", AuthLevel::Write, "takes an object of identifiers",
	  HTTPD_PARAMS(kWholeMapParams), NULL, &probeHandler, false },
	{ Method::Post, "/api/probe/bytes", AuthLevel::Write, "takes the bytes themselves",
	  HTTPD_PARAMS(kWholeBytesParams), NULL, &probeHandler, false },
};

const RouteTable kWholeTable = {
	HTTPD_TABLE("probe", kWholeEndpoints)
};

TEST_CASE("a body of its own is described out of the kind and the count, whichever they are", "[openapi]")
{
	/* Two shapes the shipped routes do not have between them, put in front of the list so
	   the writer is asked about them rather than left untested until a route needs one.

	   The list is of plain words and has both numbers, so an element handed the row's pair
	   would be given a ceiling on its length, which counts characters where the pair counts
	   words. The object has neither number, so a shape written with a floor and a ceiling
	   regardless would state that a body of none is too few. */
	ShippedRoutes shipped;
	setRoutesForTest(&kWholeTable);

	const ::Json::Value doc = parsed(openapi::document());

	const ::Json::Value list = doc["paths"]["/api/probe/list"]["post"];
	REQUIRE(list.isObject());
	checkWholeBody(list, kWholeListParams[0]);

	const ::Json::Value map = doc["paths"]["/api/probe/map"]["post"];
	REQUIRE(map.isObject());
	checkWholeBody(map, kWholeMapParams[0]);

	const ::Json::Value bytes = doc["paths"]["/api/probe/bytes"]["post"];
	REQUIRE(bytes.isObject());
	checkWholeBody(bytes, kWholeBytesParams[0]);

	/* And the same two spelled out, because everything above is read off the
	   rows and a writer that answered the rows with nothing at all would agree
	   with a checker that asked the rows for nothing at all. */
	const ::Json::Value words =
		list["requestBody"]["content"]["application/json"]["schema"];
	REQUIRE(words["type"].asString() == "array");
	REQUIRE(words["minItems"].asInt() == 2);
	REQUIRE(words["maxItems"].asInt() == 5);
	REQUIRE(words["items"]["type"].asString() == "string");
	REQUIRE_FALSE(words["items"].isMember("maxLength"));
	REQUIRE_FALSE(words["items"].isMember("x-max-bytes"));

	const ::Json::Value named =
		map["requestBody"]["content"]["application/json"]["schema"];
	REQUIRE(named["type"].asString() == "object");
	REQUIRE_FALSE(named.isMember("minProperties"));
	REQUIRE_FALSE(named.isMember("maxProperties"));
	REQUIRE(named["additionalProperties"]["type"].asString() == "string");
	REQUIRE(named["additionalProperties"]["pattern"].asString() ==
	        "^(0[xX])?[0-9a-fA-F]{1,16}$");

	/* And the third, which is the one that is not offered as a document at
	   all: a caller told application/json would wrap the bytes in quotes and
	   escapes and put a different file on the box. */
	REQUIRE(bytes["requestBody"]["content"].getMemberNames().size() == 1u);
	const ::Json::Value raw =
		bytes["requestBody"]["content"]["application/octet-stream"]["schema"];
	REQUIRE(raw["type"].asString() == "string");
	REQUIRE(raw["format"].asString() == "binary");
	REQUIRE(raw["minLength"].asInt() == 2);
	REQUIRE(raw["maxLength"].asInt() == 5);
	REQUIRE_FALSE(raw.isMember("items"));
	REQUIRE_FALSE(raw.isMember("additionalProperties"));
}

/* The one shipped route whose body is the bytes themselves. Before it declared
   one the document said this route takes no body at all, which is not what the
   handler does: it writes what arrived to a file. A caller generated from that
   document had no type for the one thing the route exists to carry, and the
   page had to say what the body is beside the document rather than out of it. */
TEST_CASE("the route that takes a file says so", "[openapi]")
{
	ShippedRoutes shipped;

	const ::Json::Value doc = parsed(openapi::document());
	const ::Json::Value put = doc["paths"]["/api/v1/storage/file"]["put"];
	REQUIRE(put.isObject());

	const ::Json::Value body = put["requestBody"];
	REQUIRE(body.isObject());
	REQUIRE(body["required"].asBool());
	REQUIRE(body["content"].getMemberNames().size() == 1u);

	const ::Json::Value schema = body["content"]["application/octet-stream"]["schema"];
	REQUIRE(schema["type"].asString() == "string");
	REQUIRE(schema["format"].asString() == "binary");
	// A file of no bytes is a file, and how large one may be is not this
	// table's answer, so neither end is stated.
	REQUIRE_FALSE(schema.isMember("minLength"));
	REQUIRE_FALSE(schema.isMember("maxLength"));

	/* And the two that are still in the query, because a route whose body is
	   the file has nowhere else to put what it is told about it. */
	REQUIRE(parameterOf(put, "path")["in"].asString() == "query");
	REQUIRE(parameterOf(put, "overwrite")["in"].asString() == "query");
	// The body is not also drawn as a component somebody could try to send.
	REQUIRE_FALSE(parameterOf(put, "file").isObject());
}

TEST_CASE("the bounds and the sets a row declares are the ones the document states", "[openapi]")
{
	ShippedRoutes shipped;

	const ::Json::Value doc = parsed(openapi::document());

	// A number bounded at both ends, written where a client can hold a value
	// to it before it sends one.
	const ::Json::Value limit = parameterOf(doc["paths"]["/api/v1/channels"]["get"], "limit");
	REQUIRE(limit["schema"]["minimum"].asInt() == 1);
	REQUIRE(limit["schema"]["maximum"].asInt() == 500);

	// A set, as the members it lists and not as the comma separated string the
	// table keeps it in.
	const ::Json::Value mode = parameterOf(doc["paths"]["/api/v1/channels"]["get"], "mode");
	REQUIRE(mode["schema"]["enum"].size() == 2);
	REQUIRE(listedIn(mode["schema"]["enum"], "tv"));
	REQUIRE(listedIn(mode["schema"]["enum"], "radio"));

	// A ceiling on a length, and the byte count beside it, because the format
	// counts characters and this server counts bytes.
	const ::Json::Value password =
		bodyPropertyOf(doc["paths"]["/api/v1/login"]["post"], "password");
	REQUIRE(password["maxLength"].asInt() == 512);
	REQUIRE(password["x-max-bytes"].asInt() == 512);

	// One that has no bounds of its own carries none, rather than a pair of
	// noughts that would refuse every value but nought.
	const ::Json::Value from = parameterOf(doc["paths"]["/api/v1/epg"]["get"], "from");
	REQUIRE(from["schema"]["type"].asString() == "integer");
	REQUIRE(from["schema"]["format"].asString() == "unix-time");
	REQUIRE_FALSE(from["schema"].isMember("minimum"));
	REQUIRE_FALSE(from["schema"].isMember("maximum"));

	// An identifier travels as text and is above what a browser's numbers
	// carry, so the document says text and says what that text looks like.
	const ::Json::Value id = parameterOf(doc["paths"]["/api/v1/channels/{id}"]["get"], "id");
	REQUIRE(id["schema"]["type"].asString() == "string");
	REQUIRE_FALSE(id["schema"]["pattern"].asString().empty());

	// A signed number bounded at both ends, and nought is a real floor rather
	// than the absence of one.
	const ::Json::Value percent =
		bodyPropertyOf(doc["paths"]["/api/v1/osd/volume"]["put"], "percent");
	REQUIRE(percent["minimum"].asInt() == 0);
	REQUIRE(percent["maximum"].asInt() == 100);
}

TEST_CASE("the shape a route declares is filed under a key the route points at", "[openapi]")
{
	ShippedRoutes shipped;

	const ::Json::Value doc = parsed(openapi::document());
	size_t tables = 0;
	const RouteTable *const *t = allRoutes(&tables);

	size_t filed = 0;
	for (size_t i = 0; i < tables; ++i)
	{
		for (size_t j = 0; j < t[i]->count; ++j)
		{
			const Endpoint &ep = t[i]->endpoints[j];
			if (ep.schema == NULL)
				continue;
			INFO(ep.path);

			const ::Json::Value op = operationOf(doc, ep);
			const ::Json::Value body =
				op["responses"]["2XX"]["content"]["application/json"]["schema"];
			REQUIRE(body["$ref"].isString());

			const ::Json::Value shape = pointed(doc, body["$ref"].asString());
			REQUIRE(shape.isObject());
			REQUIRE(shape["type"].asString() == "object");
			REQUIRE(shape["additionalProperties"].asBool() == false);

			for (size_t k = 0; k < ep.schema->count; ++k)
			{
				const FieldDesc &f = ep.schema->fields[k];
				INFO("member " << f.name);
				const ::Json::Value member = shape["properties"][f.name];
				REQUIRE(member.isObject());
				REQUIRE(member.isMember("description") == openapi::descriptionsCompiledIn());
				if (openapi::descriptionsCompiledIn())
					REQUIRE(member["description"].asString() == std::string(f.doc));
				if (f.type == FieldType::Object)
					REQUIRE(member["$ref"].isString());
				else
					REQUIRE(member["type"].asString() == fieldType(f.type));
				// A member that is always there is one a reader may count on,
				// and the one absent from some answers is the exception.
				REQUIRE(listedIn(shape["required"], f.name) == !f.optional);
			}
			++filed;
		}
	}

	recordCount("shapes the document files against a route", filed);
}

TEST_CASE("a shape named by a member is carried too, and a list of plain values says what they are", "[openapi]")
{
	ShippedRoutes shipped;

	const ::Json::Value doc = parsed(openapi::document());

	// A page of channels names the shape of one channel, and that shape has to
	// be in the document rather than only pointed at from it.
	const ::Json::Value page = pointed(doc, "#/components/schemas/channel-page");
	REQUIRE(page.isObject());
	const ::Json::Value items = page["properties"]["items"];
	REQUIRE(items["type"].asString() == "array");
	REQUIRE(items["items"]["$ref"].asString() == "#/components/schemas/channel");
	REQUIRE(pointed(doc, items["items"]["$ref"].asString()).isObject());

	/* A list of plain values carries no shape beside it, there being no members to
	   name, so what the elements are is the one thing the row itself says. These are
	   whole numbers, and a reader told nothing about them would read the same member
	   for a list of words. Both halves, because a case that only asked for the elements
	   to be described would have passed on a document calling them strings. */
	const ::Json::Value condition = pointed(doc, "#/components/schemas/setting-condition");
	REQUIRE(condition.isObject());
	REQUIRE(condition["properties"]["values"]["type"].asString() == "array");
	REQUIRE(condition["properties"]["values"]["items"]["type"].asString() == "integer");
	REQUIRE_FALSE(condition["properties"]["values"]["items"].isMember("$ref"));

	// And the other of the two, whose elements are words, so that what is
	// written is the element and not one kind written for every such list.
	const ::Json::Value keys = pointed(doc, "#/components/schemas/keys");
	REQUIRE(keys.isObject());
	REQUIRE(keys["properties"]["items"]["type"].asString() == "array");
	REQUIRE(keys["properties"]["items"]["items"]["type"].asString() == "string");
}

TEST_CASE("the route that answers with a stream says so rather than saying nothing", "[openapi]")
{
	/* It declares no shape, a stream not being a document with members, and a
	   route that declares no shape is written with no body at all. A client
	   generated from that is told the route answers nothing, which is the one
	   place the gap changes what a caller has to write. */
	ShippedRoutes shipped;

	const ::Json::Value doc = parsed(openapi::document());

	size_t tables = 0;
	const RouteTable *const *t = allRoutes(&tables);
	size_t streams = 0;
	for (size_t i = 0; i < tables; ++i)
	{
		for (size_t j = 0; j < t[i]->count; ++j)
		{
			const Endpoint &ep = t[i]->endpoints[j];
			const ::Json::Value answer = operationOf(doc, ep)["responses"]["2XX"];
			INFO(ep.path);

			if (!events::isStreamRoute(ep))
			{
				REQUIRE_FALSE(answer["content"].isMember(events::streamContentType()));
				// And a route that declares a shape says what it answers with,
				// so the two readings cannot both be silent.
				REQUIRE(answer.isMember("content") == (ep.schema != NULL));
				continue;
			}

			REQUIRE(answer["content"].isMember(events::streamContentType()));
			REQUIRE_FALSE(answer["content"].isMember("application/json"));
			++streams;
		}
	}
	// The one this server has, so a build where the module stopped answering
	// is not a build where this passed over nothing.
	REQUIRE(streams == 1);
}

TEST_CASE("every reference the document makes points at something the document carries", "[openapi]")
{
	ShippedRoutes shipped;

	const ::Json::Value doc = parsed(openapi::document());
	std::vector<std::string> refs;
	collectRefs(doc, refs);
	REQUIRE(refs.size() > 20);
	for (size_t i = 0; i < refs.size(); ++i)
	{
		INFO(refs[i]);
		REQUIRE(pointed(doc, refs[i]).isObject());
	}
}

TEST_CASE("the refusal every layer answers with is described once and pointed at", "[openapi]")
{
	ShippedRoutes shipped;

	const ::Json::Value doc = parsed(openapi::document());
	const ::Json::Value shape = pointed(doc, "#/components/schemas/problem");
	REQUIRE(shape.isObject());
	REQUIRE(shape["properties"]["type"]["type"].asString() == "string");
	REQUIRE(shape["properties"]["title"]["type"].asString() == "string");
	REQUIRE(shape["properties"]["status"]["type"].asString() == "integer");
	REQUIRE(shape["properties"]["detail"]["type"].asString() == "string");

	const ::Json::Value response = pointed(doc, "#/components/responses/problem");
	REQUIRE(response.isObject());
	REQUIRE_FALSE(response["description"].asString().empty());
	REQUIRE(response["content"]["application/problem+json"]["schema"]["$ref"].asString() ==
	        "#/components/schemas/problem");

	// Every operation points at it, so a client reads one refusal and not one
	// per route.
	const ::Json::Value paths = doc["paths"];
	const ::Json::Value::Members names = paths.getMemberNames();
	size_t pointed_at = 0;
	for (size_t i = 0; i < names.size(); ++i)
	{
		const ::Json::Value item = paths[names[i]];
		const ::Json::Value::Members verbs = item.getMemberNames();
		for (size_t j = 0; j < verbs.size(); ++j)
		{
			INFO(names[i] << " " << verbs[j]);
			REQUIRE(item[verbs[j]]["responses"]["default"]["$ref"].asString() ==
			        "#/components/responses/problem");
			++pointed_at;
		}
	}
	REQUIRE(pointed_at > 30);
}

TEST_CASE("what a caller has to present is stated, and the second token is named where it is wanted", "[openapi]")
{
	ShippedRoutes shipped;

	const ::Json::Value doc = parsed(openapi::document());
	REQUIRE(doc["components"]["securitySchemes"]["session"]["in"].asString() == "cookie");
	REQUIRE(doc["components"]["securitySchemes"]["session"]["name"].asString() ==
	        std::string(sessionCookieName()));
	REQUIRE(doc["components"]["securitySchemes"]["token"]["scheme"].asString() == "bearer");

	/* The third, which is how a caller that cannot set a header presents anything at
	   all. An apiKey in the query and never a parameter: a client turns a parameter
	   into an argument its caller fills in and a scheme into the one place it keeps a
	   credential, and the router drops this name before it compares what a route
	   declares, so a route listing it would be describing a value it never binds. */
	const ::Json::Value in_query = doc["components"]["securitySchemes"]["query_token"];
	REQUIRE(in_query["type"].asString() == "apiKey");
	REQUIRE(in_query["in"].asString() == "query");
	REQUIRE(in_query["name"].asString() == std::string(queryTokenName()));

	size_t tables = 0;
	const RouteTable *const *t = allRoutes(&tables);
	size_t needing = 0;
	size_t open = 0;
	size_t in_address = 0;
	for (size_t i = 0; i < tables; ++i)
	{
		for (size_t j = 0; j < t[i]->count; ++j)
		{
			const Endpoint &ep = t[i]->endpoints[j];
			const ::Json::Value op = operationOf(doc, ep);
			INFO(ep.path);

			if (ep.auth == AuthLevel::Public)
			{
				// Nothing to present, which is what an empty list says.
				REQUIRE(op["security"].size() == 0);
				REQUIRE_FALSE(op.isMember("x-csrf-header"));
				REQUIRE_FALSE(parameterOf(op, csrfHeaderName()).isObject());
				++open;
				continue;
			}
			/* Two ways in for every route, and a third only where the row says
			   the route takes a credential out of its address. Named anywhere
			   else it would tell a client it may write a secret into the
			   address of a route that never reads one. */
			REQUIRE(op["security"].size() == (ep.query_token_ok ? 3u : 2u));
			REQUIRE(op["security"][0u].isMember("session"));
			REQUIRE(op["security"][1u].isMember("token"));
			if (ep.query_token_ok)
			{
				REQUIRE(op["security"][2u].isMember("query_token"));
				// And the same name is not also drawn as a field to fill in.
				REQUIRE_FALSE(parameterOf(op, queryTokenName()).isObject());
				++in_address;
			}

			/* A cookie is attached by the browser to whatever asks for it, so a request
			   that changes something and arrived on one has to carry the second token.
			   Read off the method alone and not off the level as well: a route can sit
			   at System and still be a Get, and a Get never carries the second token
			   because it never changes anything for one to ride beside. */
			const bool changes = (ep.method != Get && ep.method != Options);
			REQUIRE(op.isMember("x-csrf-header") == changes);

			/* The same answer, asked of the parameter Swagger UI actually
			   draws a field for and not only of the metadata string above:
			   the two are written from one function (needsCsrfHeader) and a
			   fall out of step here is the fall out of step a generated
			   client, and not only this suite, would meet. */
			const ::Json::Value token = parameterOf(op, csrfHeaderName());
			REQUIRE(token.isObject() == changes);
			if (changes)
			{
				REQUIRE(op["x-csrf-header"].asString() == std::string(csrfHeaderName()));
				REQUIRE(token["in"].asString() == "header");
				// Optional and not required: a caller presenting a bearer
				// token instead of a session cookie needs no second one at
				// all, and a schema cannot say "required with one credential
				// and not the other".
				REQUIRE_FALSE(token["required"].asBool());
				REQUIRE(token.isMember("description") == openapi::descriptionsCompiledIn());
				++needing;
			}
		}
	}
	// The two routes a caller may reach with nothing to present, which is what
	// the check over the tables allows and no more: a third would be a route
	// described here as asking for nothing at all.
	REQUIRE(open == 2);
	REQUIRE(needing > 15);
	/* Exactly one route takes a credential out of an address, and this is where that
	   is said out loud. A second one appearing is a change somebody has to have meant:
	   what a leaked address reaches is the sum of these, and a number nobody is holding
	   is a number that grows. */
	REQUIRE(in_address == 1);
}

TEST_CASE("the document is built once and handed out from where it was kept", "[openapi]")
{
	ShippedRoutes shipped;

	// Whatever the first one costs is paid here, so that what follows is the
	// question this case asks and not the order the cases happen to run in.
	openapi::document();
	const size_t after_first = openapi::buildCount();
	REQUIRE(after_first > 0);

	const std::string a = openapi::document();
	const std::string b = openapi::document();
	REQUIRE(a == b);
	REQUIRE_FALSE(a.empty());
	REQUIRE(openapi::buildCount() == after_first);
}

TEST_CASE("a table put in front of the list is the table the document then describes", "[openapi]")
{
	/* The answer is kept, and a kept answer that outlived what it was made
	   from would describe a server that is not there. Nothing installs a table
	   in the product, which is exactly why the rebuild has to be driven here:
	   an answer kept for ever is right in the product for a reason and not by
	   accident. */
	ShippedRoutes shipped;

	openapi::document();
	const size_t before = openapi::buildCount();

	setRoutesForTest(&kProbeTable);
	const ::Json::Value swapped = parsed(openapi::document());
	REQUIRE(openapi::buildCount() == before + 1);
	REQUIRE(swapped["paths"].isMember("/api/probe/{id}"));
	REQUIRE_FALSE(swapped["paths"].isMember("/api/v1/channels"));
	REQUIRE(swapped["components"]["schemas"].isMember("probe-shape"));
	// And asking again does not build again, so the list is the key and not
	// the asking.
	openapi::document();
	REQUIRE(openapi::buildCount() == before + 1);

	setRoutesForTest(NULL);
	const ::Json::Value back = parsed(openapi::document());
	REQUIRE(openapi::buildCount() == before + 2);
	REQUIRE(back["paths"].isMember("/api/v1/channels"));
	REQUIRE_FALSE(back["paths"].isMember("/api/probe/{id}"));
}

TEST_CASE("two shapes carrying one name are filed apart rather than one over the other", "[openapi]")
{
	ShippedRoutes shipped;

	std::string out;
	const RouteTable *const one[] = { &kClashTable };
	openapi::appendDocument(out, one, 1, true);

	const ::Json::Value doc = parsed(out);
	const std::string first = doc["paths"]["/api/clash/one"]["get"]["responses"]["2XX"]
	                             ["content"]["application/json"]["schema"]["$ref"].asString();
	const std::string second = doc["paths"]["/api/clash/two"]["get"]["responses"]["2XX"]
	                              ["content"]["application/json"]["schema"]["$ref"].asString();
	REQUIRE_FALSE(first.empty());
	REQUIRE(first != second);
	// Each reference reaches the shape it was made for and not the other one.
	REQUIRE(pointed(doc, first)["properties"].isMember("a"));
	REQUIRE(pointed(doc, second)["properties"].isMember("b"));
}

TEST_CASE("a build without the prose keeps every path and every shape and drops the words", "[openapi]")
{
	/* The flag takes the prose out and nothing else, because what reads this
	   document is the type generation and the contract cases and both of those
	   read paths and shapes. Both readings are built here rather than one of
	   them being whatever this build was configured for, so this case asks the
	   same question and can fail the same way in either configuration. */
	ShippedRoutes shipped;

	size_t tables = 0;
	const RouteTable *const *t = allRoutes(&tables);

	std::string bare;
	std::string wordy;
	openapi::appendDocument(bare, t, tables, false);
	openapi::appendDocument(wordy, t, tables, true);

	const ::Json::Value stripped = parsed(bare);
	const ::Json::Value full = parsed(wordy);

	REQUIRE(bare.size() < wordy.size());
	REQUIRE(stripped["paths"].getMemberNames().size() ==
	        full["paths"].getMemberNames().size());
	REQUIRE(stripped["components"]["schemas"].getMemberNames().size() ==
	        full["components"]["schemas"].getMemberNames().size());

	/* Not one word the tables carry, and the same walk over the document built
	   with them finds a great many, so a walk that had stopped looking would
	   fail on the second of these rather than pass on the first. */
	REQUIRE(proseIn(stripped) == 0);
	REQUIRE(proseIn(full) > 100);

	/* One shape has a member of its own called description. Prose taken out
	   does not take that with it: it is a member of an answer and it stays
	   named what the box calls it. */
	REQUIRE(stripped["components"]["schemas"]["event"]["properties"].isMember("description"));

	/* The description left on a Response Object is the format's own, one per
	   operation, and its words are a fixed sentence here rather than anything
	   out of a table. A Response Object without one is not a document a reader
	   accepts, so leaving it out would answer a request for a shorter document
	   by making it an unreadable one. */
	size_t operations = 0;
	const ::Json::Value::Members walked = stripped["paths"].getMemberNames();
	for (size_t i = 0; i < walked.size(); ++i)
	{
		const ::Json::Value item = stripped["paths"][walked[i]];
		const ::Json::Value::Members verbs = item.getMemberNames();
		for (size_t j = 0; j < verbs.size(); ++j)
		{
			INFO(walked[i] << " " << verbs[j]);
			REQUIRE(item[verbs[j]]["responses"]["2XX"]["description"].asString() == "the answer");
			++operations;
		}
	}
	REQUIRE(operations > 30);
	REQUIRE_FALSE(stripped["components"]["responses"]["problem"]["description"].asString().empty());

	/* The groups are structure and their sentences are prose, so a build
	   without the words keeps every one of them and every route's mark and
	   drops only what the sentence said. A generated client names its classes
	   after these, and a document that dropped them would generate one class
	   with every route this box has in it. */
	REQUIRE(stripped["tags"].size() == full["tags"].size());
	REQUIRE(stripped["tags"].size() > 1u);
	for (::Json::ArrayIndex i = 0; i < stripped["tags"].size(); ++i)
	{
		INFO(stripped["tags"][i]["name"].asString());
		REQUIRE(stripped["tags"][i]["name"].asString() == full["tags"][i]["name"].asString());
		REQUIRE_FALSE(stripped["tags"][i]["name"].asString().empty());
		REQUIRE_FALSE(stripped["tags"][i].isMember("description"));
		REQUIRE(full["tags"][i].isMember("description"));
	}
	REQUIRE(stripped["paths"]["/api/v1/channels"]["get"]["tags"] ==
	        full["paths"]["/api/v1/channels"]["get"]["tags"]);

	// And what the tables say is still all there, so the flag took prose and
	// nothing structural.
	REQUIRE(stripped["paths"]["/api/v1/channels"]["get"]["parameters"].size() ==
	        full["paths"]["/api/v1/channels"]["get"]["parameters"].size());
	REQUIRE(stripped["components"]["schemas"]["channel"]["properties"].getMemberNames().size() ==
	        full["components"]["schemas"]["channel"]["properties"].getMemberNames().size());

	// And the one this build keeps is the one it was configured for, so the
	// flag reaching the emitter is not something only a case can see.
	REQUIRE(openapi::document() == (openapi::descriptionsCompiledIn() ? wordy : bare));
}

TEST_CASE("the flag configure was given is the flag this build compiled", "[openapi]")
{
	/* The macro is read in one file, and if the header that carries it were ever a
	   different header of the same name that file would compile clean with the flag
	   invisible and the document would carry the prose whatever configure was told.
	   This is what says so: what is read here is the generated header, reached through
	   the build directory, compared against what the server compiled. */
#ifdef DISABLE_API_DOC
	REQUIRE_FALSE(openapi::descriptionsCompiledIn());
#else
	REQUIRE(openapi::descriptionsCompiledIn());
#endif

	// And the sentinel that would have stopped that build in the first place
	// is a macro only the generated header defines.
	REQUIRE(std::string(PACKAGE_NAME).size() > 0);
}

TEST_CASE("the document is served where it says it is, to a caller with a read and to nobody less", "[openapi]")
{
	/* It publishes the whole of what this server offers and building it is the
	   most memory one answer here costs, so it asks for more than being able to
	   reach the port. */
	ShippedRoutes shipped;

	size_t tables = 0;
	const RouteTable *const *t = allRoutes(&tables);
	const Endpoint *found = NULL;
	for (size_t i = 0; i < tables && found == NULL; ++i)
	{
		for (size_t j = 0; j < t[i]->count; ++j)
		{
			if (std::strcmp(t[i]->endpoints[j].path, openapi::documentPath()) == 0)
			{
				found = &t[i]->endpoints[j];
				break;
			}
		}
	}
	REQUIRE(found != NULL);
	REQUIRE(found->auth == AuthLevel::Read);
	REQUIRE(found->method == Get);

	const Response refused = dispatch(Get, openapi::documentPath(), "", "",
	                                  "8.8.8.8", AuthLevel::Public);
	REQUIRE(refused.code == StatusForbidden);

	const Response answered = dispatch(Get, openapi::documentPath(), "", "",
	                                   "127.0.0.1", AuthLevel::Read);
	REQUIRE(answered.code == StatusOk);
	REQUIRE(answered.content_type == "application/json");
	REQUIRE(answered.body == openapi::document());

	/* The document comes in two forms and which one is sent depends on a
	   header, and it is answered at a level, so it varies on what a caller
	   takes and on what a caller presented. Read as a list of names rather than
	   as one string, because what the header has to name is settled and the
	   order it names them in is not. */
	std::string vary;
	for (size_t i = 0; i < answered.headers.size(); ++i)
	{
		if (answered.headers[i].first == "Vary")
			vary = answered.headers[i].second;
	}
	INFO(vary);
	REQUIRE(vary.find("Accept-Encoding") != std::string::npos);
	REQUIRE(vary.find("Cookie") != std::string::npos);
	REQUIRE(vary.find("Authorization") != std::string::npos);
}

TEST_CASE("the compressed form goes only to a caller that said it takes one", "[openapi]")
{
	ShippedRoutes shipped;

	const Response plain = dispatch(Get, openapi::documentPath(), "", "",
	                                "127.0.0.1", AuthLevel::Read);
	REQUIRE(plain.code == StatusOk);

	// A caller that named no encoding gets the answer as it was made, because
	// a plugin reaching this with plain curl sends no such header at all.
	const Response as_is = openapi::withEncoding(plain, "");
	REQUIRE(as_is.body == plain.body);
	REQUIRE(as_is.headers.size() == plain.headers.size());

	const Response packed = openapi::withEncoding(plain, "gzip");
	REQUIRE(packed.body.size() < plain.body.size());
	bool encoded = false;
	for (size_t i = 0; i < packed.headers.size(); ++i)
	{
		if (packed.headers[i].first == "Content-Encoding")
			encoded = packed.headers[i].second == "gzip";
	}
	REQUIRE(encoded);

	// And it is a stream a client can read back, not merely fewer bytes.
	std::string back;
	REQUIRE(gunzip(packed.body, back));
	REQUIRE(back == plain.body);

	// A refusal is refused, which is what keeps the swap from answering one
	// request with another request's body.
	Response other;
	other.code = StatusOk;
	other.body = "{\"not\":\"the document\"}";
	const Response untouched = openapi::withEncoding(other, "gzip");
	REQUIRE(untouched.body == other.body);
	REQUIRE(untouched.headers.empty());

	// The weight is read and not the letters: this one refuses it.
	const Response refused_encoding = openapi::withEncoding(plain, "gzip;q=0");
	REQUIRE(refused_encoding.body == plain.body);
}

TEST_CASE("the target that writes the document out writes the bytes the route answers", "[openapi]")
{
	/* What generates a client's types reads the file and never the box, so a
	   file that had drifted from the answer would generate a client for a
	   server that is not there. */
	ShippedRoutes shipped;

	const std::string written = readWholeFile(NI_WEB_OPENAPI_DUMP);
	INFO(NI_WEB_OPENAPI_DUMP);
	REQUIRE_FALSE(written.empty());
	REQUIRE(written == openapi::document());
}

TEST_CASE("a shape this layer wrote wrong is refused, and says what is wrong with it", "[openapi]")
{
	/* The other half of the shapes the server ships, which every case that
	   holds an answer to its shape asks about first. A check that answered
	   true for everything would leave all of those cases comparing an answer
	   with a shape nothing had looked at, so each row below breaks one rule
	   and nothing else. */
	struct Case
	{
		const char *what;
		FieldDesc   field;
	};

	static const Case broken[] = {
		{ "a member with no name",
		  { HTTPD_MEMBER(NULL, FieldType::Int, "a number") } },
		{ "a member whose name is empty",
		  { HTTPD_MEMBER("", FieldType::Int, "a number") } },
		{ "a member that says nothing about itself",
		  { HTTPD_MEMBER("n", FieldType::Int, NULL) } },
		{ "a member whose words are empty",
		  { HTTPD_MEMBER("n", FieldType::Int, "") } },
		{ "a member that is an object and carries no shape",
		  { HTTPD_MEMBER("o", FieldType::Object, "an object") } },
		{ "a member that carries a shape and is neither an object nor a list",
		  { HTTPD_MEMBER_AS_WRITTEN("n", FieldType::Int, false, "a number", &kProbeSchema, NULL,
					    ElementType::None) } },
		{ "a list of plain values that says nothing about them",
		  { HTTPD_MEMBER("l", FieldType::Array, "a list") } },
		{ "a member that says what its elements are and is not a list of plain values",
		  { HTTPD_MEMBER_AS_WRITTEN("n", FieldType::Int, false, "a number", NULL, NULL,
					    ElementType::Int) } },
		{ "a list of objects that says what its elements are as well",
		  { HTTPD_MEMBER_AS_WRITTEN("l", FieldType::Array, false, "a list", &kProbeSchema, NULL,
					    ElementType::Int) } },
		{ "a member that states a set and is not text",
		  { HTTPD_MEMBER_AS_WRITTEN("n", FieldType::Int, false, "a number", NULL, "one,two",
					    ElementType::None) } },
		{ "an identifier that states a set, which would be a list and not a kind",
		  { HTTPD_MEMBER_AS_WRITTEN("id", FieldType::ChannelId, false, "an identifier", NULL, "1,2",
					    ElementType::None) } },
		{ "a member that states a set with nothing in it",
		  { HTTPD_MEMBER_OF_SET("s", "", "a word") } },
		{ "a member that states a set with an empty value in it",
		  { HTTPD_MEMBER_OF_SET("s", "one,,two", "a word") } },
		{ "a member that states a set beginning with an empty value",
		  { HTTPD_MEMBER_OF_SET("s", ",one", "a word") } },
		{ "a member that states one value of its set twice",
		  { HTTPD_MEMBER_OF_SET("s", "one,two,one", "a word") } },
	};

	for (size_t i = 0; i < sizeof(broken) / sizeof(broken[0]); ++i)
	{
		const Schema s = { "probe-shape", &broken[i].field, 1 };
		const char *why = "";
		INFO(broken[i].what);
		REQUIRE_FALSE(schemaIsSane(s, &why));
		// A server that refuses to start and says only that it refused leaves
		// whoever wrote the shape reading all of it.
		REQUIRE(why != NULL);
		REQUIRE(why[0] != '\0');
	}
}

TEST_CASE("what a member of an answer may say about itself", "[openapi]")
{
	/* The other side of the rows above. A rule that refused every set, or
	   every list, would pass all of them and leave the server unable to
	   describe anything it actually answers with. */
	struct Fine
	{
		const char *what;
		FieldDesc   field;
	};

	static const Fine fine[] = {
		{ "a member every answer carries",
		  { HTTPD_MEMBER("n", FieldType::Int, "a number") } },
		{ "a member some answers leave out",
		  { HTTPD_MEMBER_OPTIONAL("n", FieldType::Int, "a number") } },
		{ "an identifier",
		  { HTTPD_MEMBER("id", FieldType::ChannelId, "an identifier") } },
		{ "an object of a named shape",
		  { HTTPD_OBJECT("o", &kProbeSchema, "an object") } },
		{ "a list of objects of a named shape",
		  { HTTPD_LIST_OF("l", &kProbeSchema, "a list") } },
		{ "the same list left out of some answers",
		  { HTTPD_LIST_OF_OPTIONAL("l", &kProbeSchema, "a list") } },
		{ "a list of plain values that says what they are",
		  { HTTPD_LIST_OF_VALUES("l", ElementType::Int, "a list") } },
		{ "a set of one value, which is a set",
		  { HTTPD_MEMBER_OF_SET("s", "one", "a word") } },
		{ "a set of several",
		  { HTTPD_MEMBER_OF_SET("s", "one,two,three", "a word") } },
	};

	for (size_t i = 0; i < sizeof(fine) / sizeof(fine[0]); ++i)
	{
		const Schema s = { "probe-shape", &fine[i].field, 1 };
		const char *why = "not asked";
		INFO(fine[i].what);
		REQUIRE(schemaIsSane(s, &why));
		REQUIRE(why != NULL);
		REQUIRE(why[0] == '\0');
	}
}

TEST_CASE("what a member is narrower than text reaches the document as that", "[openapi]")
{
	/* Every shape the tables reach, and not only the ones a route points at
	   directly: a channel, an event and a timer are each named by a member of
	   a shape above them, and that is where every identifier this server
	   answers with lives. A walk of the top level alone would have looked at
	   none of them. */
	ShippedRoutes shipped;

	const ::Json::Value doc = parsed(openapi::document());
	size_t tables = 0;
	const RouteTable *const *t = allRoutes(&tables);

	std::vector<const Schema *> shapes;
	for (size_t i = 0; i < tables; ++i)
	{
		for (size_t j = 0; j < t[i]->count; ++j)
		{
			const Schema *s = t[i]->endpoints[j].schema;
			if (s != NULL && std::find(shapes.begin(), shapes.end(), s) == shapes.end())
				shapes.push_back(s);
		}
	}
	// The list grows while it is walked, so a shape named by a shape named by
	// a route is reached without the depth being written down anywhere.
	for (size_t i = 0; i < shapes.size(); ++i)
	{
		for (size_t k = 0; k < shapes[i]->count; ++k)
		{
			const Schema *n = shapes[i]->fields[k].nested;
			if (n != NULL && std::find(shapes.begin(), shapes.end(), n) == shapes.end())
				shapes.push_back(n);
		}
	}

	size_t identifiers = 0;
	size_t sets = 0;
	size_t listed_elements = 0;
	for (size_t i = 0; i < shapes.size(); ++i)
	{
		const Schema &s = *shapes[i];
		INFO("shape " << s.name);
		const ::Json::Value shape = pointed(doc, std::string("#/components/schemas/") + s.name);
		REQUIRE(shape.isObject());

		for (size_t k = 0; k < s.count; ++k)
		{
			const FieldDesc &f = s.fields[k];
			INFO("member " << f.name);
			const ::Json::Value member = shape["properties"][f.name];
			REQUIRE(member.isObject());

			if (f.type == FieldType::ChannelId)
			{
				/* The pattern and not only the kind. Told that a member is a
				   string, a reader knows nothing it did not know before; the
				   pattern is the whole of what naming this kind apart buys,
				   and it is the one the parameter of the same kind carries. */
				REQUIRE(member["type"].asString() == "string");
				REQUIRE(member["pattern"].asString() == "^(0[xX])?[0-9a-fA-F]{1,16}$");
				++identifiers;
			}
			else
			{
				REQUIRE_FALSE(member.isMember("pattern"));
			}

			if (f.values != NULL)
			{
				REQUIRE(member["type"].asString() == "string");
				REQUIRE(member["enum"].isArray());
				// Every value of the set and in the order the row states them,
				// so a set that reached the document short would fail here
				// rather than quietly offer a client fewer than the box has.
				std::vector<std::string> stated;
				for (const char *q = f.values;;)
				{
					const char *e = std::strchr(q, ',');
					stated.push_back(e != NULL ? std::string(q, e - q) : std::string(q));
					if (e == NULL)
						break;
					q = e + 1;
				}
				REQUIRE(member["enum"].size() == stated.size());
				for (size_t v = 0; v < stated.size(); ++v)
					REQUIRE(member["enum"][(int) v].asString() == stated[v]);
				++sets;
			}
			else if (f.asks != NULL)
			{
				/* The same, where the set is not in the table at all: the row
				   names something to ask and the document carries what came
				   back. Held to the answer of that same function rather than
				   to a list written here, which would be the copy the row
				   exists to avoid. */
				std::vector<std::string> asked;
				f.asks(asked);
				REQUIRE_FALSE(asked.empty());
				REQUIRE(member["type"].asString() == "string");
				REQUIRE(member["enum"].isArray());
				REQUIRE(member["enum"].size() == asked.size());
				for (size_t v = 0; v < asked.size(); ++v)
					REQUIRE(member["enum"][(int) v].asString() == asked[v]);
				++sets;
			}
			else
			{
				REQUIRE_FALSE(member.isMember("enum"));
			}

			if (f.element != ElementType::None)
			{
				REQUIRE(member["type"].asString() == "array");
				REQUIRE(member["items"]["type"].isString());
				++listed_elements;
			}
		}
	}

	/* What the document says about its answers beyond that they are text,
	   counted so that it cannot fall back unremarked. Every one of these was a
	   bare string before the tables could carry what it is. */
	recordCount("answer members narrower than free text", identifiers + sets + listed_elements);
}

TEST_CASE("a set the build decides reaches the document as the box's own names", "[openapi]")
{
	/* Two segments whose set is settled by the build and stated nowhere in the tables:
	   which sections the settings are laid out in, and which daemons this box drives. A
	   list written into the tables would be right for one box and quietly wrong for
	   another, and a generated client then turns down a request the box would have
	   answered.

	   Held against what the routes that list those names answer with, and not against a
	   list written out in this case: a document given a list of its own would differ from
	   the route the moment a build moved a row. */
	ShippedRoutes shipped;

	const ::Json::Value doc = parsed(openapi::document());
	size_t tables = 0;
	const RouteTable *const *t = allRoutes(&tables);

	struct Pair
	{
		const char *listed_at;   // the route that answers the names
		const char *taken_at;    // the operation whose segment is out of them
		const char *segment;
	};
	static const Pair pairs[] = {
		{ "/api/v1/settings/sections", "/api/v1/settings/{section}", "section" },
		{ "/api/v1/daemons", "/api/v1/daemons/{name}/start", "name" },
	};

	for (size_t i = 0; i < sizeof(pairs) / sizeof(pairs[0]); ++i)
	{
		INFO(pairs[i].taken_at);

		const Endpoint *listing = NULL;
		for (size_t a = 0; a < tables && listing == NULL; ++a)
		{
			for (size_t b = 0; b < t[a]->count; ++b)
			{
				const Endpoint &ep = t[a]->endpoints[b];
				if (ep.method == Method::Get && std::strcmp(ep.path, pairs[i].listed_at) == 0)
				{
					listing = &ep;
					break;
				}
			}
		}
		REQUIRE(listing != NULL);
		REQUIRE(listing->handler != NULL);

		// The names as a caller reading that route gets them, out of the
		// handler itself, which takes nothing from the request.
		Request asked;
		const Response answered = listing->handler(asked);
		REQUIRE(answered.code == StatusOk);
		const ::Json::Value listed = parsed(answered.body);
		REQUIRE(listed["items"].isArray());
		REQUIRE(listed["items"].size() > 0);

		const ::Json::Value taken =
			parameterOf(doc["paths"][pairs[i].taken_at][std::strcmp(pairs[i].segment, "section") == 0 ? "get" : "post"],
			            pairs[i].segment);
		REQUIRE(taken.isObject());
		REQUIRE(taken["schema"]["type"].asString() == "string");
		REQUIRE(taken["schema"]["enum"].isArray());
		REQUIRE(taken["schema"]["enum"].size() == listed["items"].size());
		for (::Json::ArrayIndex k = 0; k < listed["items"].size(); ++k)
		{
			const std::string name = listed["items"][k]["id"].asString();
			INFO("name " << name);
			REQUIRE_FALSE(name.empty());
			REQUIRE(listedIn(taken["schema"]["enum"], name));
		}
	}
}

/* One value travelling in both directions, and the document saying the same thing
   about it at both ends. The listing of daemons answers a name and the three routes
   that act on one take that name back. While only the routes stated the set, a reader
   crossing from the listing to a route held a free text on one side and one of nineteen
   words on the other. */
TEST_CASE("a name a listing answers and a route takes back is one kind of thing", "[openapi]")
{
	ShippedRoutes shipped;

	const ::Json::Value doc = parsed(openapi::document());

	const ::Json::Value answered =
		doc["components"]["schemas"]["daemon"]["properties"]["id"];
	REQUIRE(answered["type"].asString() == "string");
	REQUIRE(answered["enum"].isArray());
	REQUIRE(answered["enum"].size() > 0);

	const ::Json::Value taken =
		parameterOf(doc["paths"]["/api/v1/daemons/{name}/start"]["post"], "name");
	REQUIRE(taken["schema"]["enum"].isArray());

	// The same names, and the same order, which is what makes the two one set
	// rather than two that happen to overlap today.
	REQUIRE(answered["enum"].size() == taken["schema"]["enum"].size());
	for (::Json::ArrayIndex i = 0; i < answered["enum"].size(); ++i)
		REQUIRE(answered["enum"][i].asString() == taken["schema"]["enum"][i].asString());

	/* And the other two verbs, because the set is on the row and the row is
	   shared: a route that had been given a row of its own would pass
	   everything above and still leave one of the three out. */
	const char *const verbs[] = { "stop", "restart" };
	for (size_t i = 0; i < sizeof(verbs) / sizeof(verbs[0]); ++i)
	{
		std::string path = std::string("/api/v1/daemons/{name}/") + verbs[i];
		INFO(path);
		const ::Json::Value other = parameterOf(doc["paths"][path]["post"], "name");
		REQUIRE(other["schema"]["enum"].size() == answered["enum"].size());
	}
}
