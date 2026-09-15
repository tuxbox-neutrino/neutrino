/*
 * test_endpoints_read.cpp - tests for every reading route
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
#include "support/fakes.h"

/* What configure wrote. Read here for one thing only: the counter route below
   is answered by every build and what it counts is not, so the case that holds
   it has to know which build it is in. */
#include <config.h>

#include "httpd/auth.h"
#include "httpd/credentials.h"
#include "httpd/endpoint.h"
#include "httpd/endpoints.h"
#include "httpd/events.h"
#include "httpd/http.h"
#include "httpd/router.h"
#include "httpd/schema.h"
#include "httpd/server.h"
#include "httpd/status.h"
#include "httpd/webconfig.h"

// mode_standby, for the one route that reads the box mode as a yes or no.
#include "neutrinoMessages.h"

#include "coreapi/daemons.h"
#include "coreapi/decryption.h"
#include "coreapi/epg.h"
#include "coreapi/base/eventbus.h"
#include "coreapi/netfs.h"
#include "coreapi/base/schema.h"
#include "coreapi/settings/settings.h"
#include "coreapi/storage.h"
#include "coreapi/box/storage_internal.h"
#include "coreapi/base/types.h"
#include "coreapi/base/version.h"

#include "jsoncpp/json/json.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <string>
#include <vector>

#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

using namespace httpd;

namespace
{

/* Answered out of the tables the server ships and never out of one a case installed,
   because that is the whole of what these cases are about: the same list the start up
   check reads and the same list a request off a socket is answered from. Cleared on
   the way in as well as out, so a case elsewhere that failed and unwound past its own
   restore cannot decide what these read. */
struct ShippedRoutes
{
	ShippedRoutes() { setRoutesForTest(NULL); }
	~ShippedRoutes() { setRoutesForTest(NULL); }

	private:
		ShippedRoutes(const ShippedRoutes &);
		ShippedRoutes &operator=(const ShippedRoutes &);
};

struct Reply
{
	int         code;
	std::string content_type;
	std::string body;

	Reply() : code(0) {}
};

/* One request, split where a transport splits it: the query comes off the target
   before anything is decoded, so an encoded separator stays a byte of a segment rather
   than making one segment into two. */
/* A read as a caller granted a level. Read by default, because that is what the
   network exemption grants and what most of these routes declare; the one that reaches
   the filesystem declares System, and a case that asked for it at a read would be
   reading the gate's refusal rather than the answer. */
Reply get(const std::string &target, AuthLevel as = AuthLevel::Read)
{
	std::string path = target;
	std::string query;
	const size_t q = target.find('?');
	if (q != std::string::npos)
	{
		path = target.substr(0, q);
		query = target.substr(q + 1);
	}

	const Response r = dispatch(Get, path, query, "", "127.0.0.1", as);

	Reply out;
	out.code = r.code;
	out.content_type = r.content_type;
	out.body = r.body;
	return out;
}

/* The same, as a caller whose credential stands for one part of the box rather than for
   all of it, which is what a token carried in an address is. System, because that is what
   such a token is minted at: the narrowing is the scope and never the level. */
Reply getScoped(const std::string &target, const std::string &scope)
{
	std::string path = target;
	std::string query;
	const size_t q = target.find('?');
	if (q != std::string::npos)
	{
		path = target.substr(0, q);
		query = target.substr(q + 1);
	}

	const Response r = dispatch(Get, path, query, "", "127.0.0.1", AuthLevel::System,
	                            "", "", "", scope);

	Reply out;
	out.code = r.code;
	out.content_type = r.content_type;
	out.body = r.body;
	return out;
}

bool putFile(const std::string &path, const std::string &what)
{
	const int fd = open(path.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0600);
	if (fd < 0)
		return false;
	const ssize_t wrote = write(fd, what.data(), what.size());
	close(fd);
	return wrote == (ssize_t) what.size();
}

// The tree's own parser, used here as a second opinion: a body this server
// wrote and only this server can read is not a document anybody else can use.
bool parseDocument(const std::string &doc, ::Json::Value &root)
{
	::Json::CharReaderBuilder builder;
	std::unique_ptr< ::Json::CharReader> reader(builder.newCharReader());
	std::string errs;
	return reader->parse(doc.data(), doc.data() + doc.size(), &root, &errs);
}

::Json::Value parsed(const std::string &body)
{
	::Json::Value root;
	REQUIRE(parseDocument(body, root));
	return root;
}

size_t countItems(const std::string &body)
{
	return (size_t) parsed(body)["items"].size();
}

std::string stringField(const std::string &body, const char *name)
{
	const ::Json::Value root = parsed(body);
	REQUIRE(root.isMember(name));
	return root[name].asString();
}

bool hasField(const std::string &body, const char *name)
{
	return parsed(body).isMember(name);
}

std::vector<std::string> ids(const std::string &body)
{
	const ::Json::Value items = parsed(body)["items"];
	std::vector<std::string> out;
	for (::Json::ArrayIndex i = 0; i < items.size(); ++i)
	{
		REQUIRE(items[i].isMember("id"));
		out.push_back(items[i]["id"].asString());
	}
	return out;
}

bool everyItemHas(const std::string &body, const char *member)
{
	const ::Json::Value items = parsed(body)["items"];
	// An answer with nothing in it would otherwise pass this whatever the
	// items carried, there being none to fail it.
	if (items.size() == 0)
		return false;
	for (::Json::ArrayIndex i = 0; i < items.size(); ++i)
	{
		if (!items[i].isMember(member))
			return false;
	}
	return true;
}

// What the item of this id is set to, and a sentinel that is not the empty
// string for one the answer does not carry, so that a value withheld and an
// item missing altogether cannot read the same.
std::string valueOf(const std::string &body, const std::string &id)
{
	const ::Json::Value items = parsed(body)["items"];
	for (::Json::ArrayIndex i = 0; i < items.size(); ++i)
	{
		if (items[i]["id"].asString() == id)
			return items[i]["value"].asString();
	}
	return "<no such item>";
}

std::vector<std::string> intersection(const std::vector<std::string> &a,
                                      const std::vector<std::string> &b)
{
	std::vector<std::string> out;
	for (size_t i = 0; i < a.size(); ++i)
	{
		if (std::find(b.begin(), b.end(), a[i]) != b.end())
			out.push_back(a[i]);
	}
	return out;
}

std::vector<std::string> concat(const std::vector<std::string> &a,
                                const std::vector<std::string> &b)
{
	std::vector<std::string> out = a;
	out.insert(out.end(), b.begin(), b.end());
	return out;
}

std::string hex(uint64_t v)
{
	char buf[24];
	std::snprintf(buf, sizeof(buf), "%llx", (unsigned long long) v);
	return std::string(buf);
}

const uint64_t kFirstTv = 0x2b66000000010000ULL;
const uint64_t kFirstRadio = 0x2b66000000020000ULL;
const size_t   kTvCount = 12;
const size_t   kRadioCount = 3;

coreapi::ChannelInfo makeChannel(uint64_t id, const char *name, coreapi::ServiceKind kind)
{
	coreapi::ChannelInfo c;
	c.id = id;
	c.epg_id = id;
	c.name = name;
	c.kind = kind;
	return c;
}

// Twelve television services and three radio ones, which is enough for three
// pages of five and for a page that ends the list.
void fillChannels(FakeChannelSource &src)
{
	for (size_t i = 0; i < kTvCount; ++i)
	{
		coreapi::ChannelInfo c = makeChannel(kFirstTv + i, "tv", coreapi::ServiceKind::Tv);
		c.number = (int32_t)(i + 1);
		src.channels.push_back(c);
	}
	for (size_t i = 0; i < kRadioCount; ++i)
		src.channels.push_back(makeChannel(kFirstRadio + i, "radio", coreapi::ServiceKind::Radio));

	/* Numbered from three and not from nought, so that an answer naming a
	   bouquet by where it sits in the list rather than by its own number reads
	   differently from one that names it right. */
	coreapi::BouquetInfo b;
	b.id = 3;
	b.name = "favourites";
	b.tv_count = 2;
	b.radio_count = 1;
	src.bouquets.push_back(b);
	b.id = 7;
	b.name = "news";
	src.bouquets.push_back(b);
	b.id = 9;
	b.name = "empty";
	b.tv_count = 0;
	b.radio_count = 0;
	src.bouquets.push_back(b);

	// A bouquet answers its television and its radio members as one list, so
	// this one holds both kinds.
	coreapi::ChannelList members;
	members.push_back(src.channels[0]);
	members.push_back(src.channels[1]);
	members.push_back(src.channels[kTvCount]);
	src.bouquet_members[3] = members;
}

std::vector<std::string> firstTvIds(size_t n)
{
	std::vector<std::string> out;
	for (size_t i = 0; i < n; ++i)
		out.push_back(hex(kFirstTv + i));
	return out;
}

// A directory of its own per case, so nothing a case leaves behind is what the
// next one reads.
struct Sandbox
{
	std::string dir;

	Sandbox()
	{
		char tmpl[] = "/tmp/httpd_endpoints_XXXXXX";
		if (mkdtemp(tmpl) != NULL)
			dir = tmpl;
	}

	~Sandbox()
	{
		if (!dir.empty())
			(void) system(("rm -rf " + dir).c_str());
	}

	std::string at(const std::string &name) const { return dir + "/" + name; }
};

// The roots and the two paths below are process wide and are put back from a
// destructor, because a check that fails unwinds past a last line and would
// leave the case after this one reading whatever this one pointed at.
struct StorageFixture
{
	Sandbox     box;
	std::string before_roots;
	const char *before_mounts;
	std::string mount_table;

	StorageFixture() : before_roots(coreapi::storage::roots()),
	                   before_mounts(coreapi::storage::internal::mounts_path)
	{
		coreapi::storage::setRoots(box.dir);

		mount_table = box.at("mounts");
		const int fd = open(mount_table.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0600);
		if (fd >= 0)
		{
			static const char kTable[] =
				"/dev/sda1 /media/sda1 ext4 rw 0 0\n"
				"tmpfs /run tmpfs rw 0 0\n";
			(void) !write(fd, kTable, sizeof(kTable) - 1);
			close(fd);
		}
		coreapi::storage::internal::mounts_path = mount_table.c_str();
	}

	~StorageFixture()
	{
		coreapi::storage::internal::mounts_path = before_mounts;
		coreapi::storage::setRoots(before_roots);
	}
};

// An empty directory where the process table is read, so that every daemon
// answers the same whatever else is running on the machine this is built on.
struct ProcFixture
{
	Sandbox     box;
	std::string before;

	ProcFixture() : before(coreapi::daemons::procRoot())
	{
		coreapi::daemons::setProcRoot(box.dir);
	}

	~ProcFixture() { coreapi::daemons::setProcRoot(before); }
};

const coreapi::Descriptor *rowIn(const std::vector<coreapi::Descriptor> &rows,
                                 const char *section, bool secret)
{
	for (size_t i = 0; i < rows.size(); ++i)
	{
		if (rows[i].section == NULL || std::string(section) != rows[i].section)
			continue;
		if (rows[i].secret != secret)
			continue;
		// Text, so that a case can put a value in the store under it and see
		// what comes back.
		if (rows[i].type != coreapi::ValueType::String)
			continue;
		return &rows[i];
	}
	return NULL;
}

/* Whether the value that arrived is of the kind the row declares, read off the parser's
   own idea of what it parsed rather than off the bytes.

   This is the half of a FieldDesc a client would build a typed record out of, and a shape
   check that only compares member names leaves it described and unchecked: a row saying
   unsigned over a handler that writes a signed value reads exactly like a row that is
   right. Which is a drift that happened.

   A whole number arrives as one of the parser's two integer kinds depending on its sign
   and width, so both are taken wherever a whole number is declared and the sign is asked
   separately. A number that is not whole is allowed to arrive whole; nothing else
   widens. */
/* What the document says an identifier reads as, asked of the answer itself:
   hexadecimal, no prefix and never empty. */
bool isHexIdentifier(const std::string &s)
{
	if (s.empty() || s.size() > 16)
		return false;
	for (size_t i = 0; i < s.size(); ++i)
	{
		if (std::isxdigit((unsigned char) s[i]) == 0)
			return false;
	}
	return true;
}

bool typeMatches(const ::Json::Value &v, FieldType t)
{
	const ::Json::ValueType got = v.type();
	const bool whole = (got == ::Json::intValue || got == ::Json::uintValue);

	switch (t)
	{
		case FieldType::Bool:   return got == ::Json::booleanValue;
		case FieldType::Int:    return whole;
		// The one that says what the others do not: a row declaring no sign is
		// answered by a number that has none.
		case FieldType::UInt:   return whole && v.asInt64() >= 0;
		case FieldType::Number: return whole || got == ::Json::realValue;
		case FieldType::String: return got == ::Json::stringValue;
		case FieldType::Time:   return whole;
		/* Text, and the text the pattern in the document states: this is the
		   one kind whose shape a reader is told outright, so a member of it
		   answering anything else is a document the server does not hold to. */
		case FieldType::ChannelId: return got == ::Json::stringValue && isHexIdentifier(v.asString());
		case FieldType::Object: return got == ::Json::objectValue;
		case FieldType::Array:  return got == ::Json::arrayValue;
	}
	// A value cast into the enum from outside it, which no table here writes.
	return false;
}

const char *typeName(FieldType t)
{
	switch (t)
	{
		case FieldType::Bool:   return "bool";
		case FieldType::Int:    return "int";
		case FieldType::UInt:   return "uint";
		case FieldType::Number: return "number";
		case FieldType::String: return "string";
		case FieldType::Time:   return "time";
		case FieldType::ChannelId: return "channel id";
		case FieldType::Object: return "object";
		case FieldType::Array:  return "array";
	}
	return "?";
}

/* One answer against the shape its route declares, in three directions. Every member
   the schema calls for and does not mark absent has to be there. Every member the
   answer carries has to be one the schema names, which is the direction that catches a
   member added to a handler and left out of the shape beside it. And every value has to
   be of the kind its row declares, which is the direction the two above cannot see at
   all. */
void checkShape(const ::Json::Value &v, const Schema &s, const std::string &where)
{
	INFO(where << " against " << s.name);
	REQUIRE(v.isObject());

	for (size_t i = 0; i < s.count; ++i)
	{
		const FieldDesc &f = s.fields[i];
		INFO("member " << f.name);
		if (!f.optional)
			REQUIRE(v.isMember(f.name));
		if (!v.isMember(f.name))
			continue;

		const ::Json::Value &member = v[f.name];
		INFO("declared " << typeName(f.type) << ", arrived as kind " << (int) member.type());
		REQUIRE(typeMatches(member, f.type));

		if (f.type == FieldType::Object)
		{
			REQUIRE(f.nested != NULL);
			checkShape(member, *f.nested, where + "." + f.name);
			continue;
		}
		if (f.type == FieldType::Array)
		{
			if (f.nested == NULL)
				continue;
			for (::Json::ArrayIndex e = 0; e < member.size(); ++e)
				checkShape(member[e], *f.nested, where + "." + f.name);
		}
	}

	const ::Json::Value::Members names = v.getMemberNames();
	for (size_t i = 0; i < names.size(); ++i)
	{
		bool declared = false;
		for (size_t j = 0; j < s.count && !declared; ++j)
			declared = names[i] == s.fields[j].name;
		INFO("member " << names[i]);
		REQUIRE(declared);
	}
}

// The route of that path, out of the tables the server ships, so a case that
// holds an answer to a shape holds it to the shape the server declares and not
// to one written out beside the case.
const Endpoint *routeFor(const char *path)
{
	size_t tables = 0;
	const RouteTable *const *t = allRoutes(&tables);
	for (size_t i = 0; i < tables; ++i)
	{
		for (size_t j = 0; j < t[i]->count; ++j)
		{
			if (t[i]->endpoints[j].method == Get && std::string(path) == t[i]->endpoints[j].path)
				return &t[i]->endpoints[j];
		}
	}
	return NULL;
}

/* Every name a label_key, a hint_key or a choice's own label_key names in the schema this
   build carries, mapped to a placeholder rather than left unset. Not the box's translated
   text: what the placeholder says is never read here, only that something came back, so
   checkShape below can walk the settings schema route with these fields present rather
   than vacuously skipping every REQUIRE on their type.

   Nothing here answers what a name resolves to on the box. */
void fillLocaleFromSchema(FakeLocaleSource &cat)
{
	const std::vector<coreapi::Descriptor> rows = coreapi::settings::schema().value();
	for (size_t i = 0; i < rows.size(); ++i)
	{
		const coreapi::Descriptor &d = rows[i];
		if (d.label_key != NULL)
			cat.texts[d.label_key] = "label text";
		if (d.hint_key != NULL)
			cat.texts[d.hint_key] = "hint text";
		for (size_t v = 0; d.values != NULL && v < d.value_count; ++v)
			cat.texts[d.values[v].label_key] = "choice text";
	}
}

/* Whether these bytes are text a strict reader will take, decoded here rather
   than asked of the writer that produced them: a body checked by the same
   reading that wrote it is checked against nothing. Written out long hand for
   that reason and not shared with anything in the layer under test. */
bool isUtf8(const std::string &v)
{
	size_t i = 0;
	while (i < v.size())
	{
		const unsigned char c = (unsigned char) v[i];
		size_t need = 0;
		unsigned char low = 0x80;
		unsigned char high = 0xbf;

		if (c < 0x80)
		{
			i += 1;
			continue;
		}
		if (c >= 0xc2 && c <= 0xdf)
			need = 2;
		else if (c == 0xe0) { need = 3; low = 0xa0; }
		else if (c >= 0xe1 && c <= 0xec) need = 3;
		else if (c == 0xed) { need = 3; high = 0x9f; }
		else if (c >= 0xee && c <= 0xef) need = 3;
		else if (c == 0xf0) { need = 4; low = 0x90; }
		else if (c >= 0xf1 && c <= 0xf3) need = 4;
		else if (c == 0xf4) { need = 4; high = 0x8f; }
		else
			return false;

		if (v.size() - i < need)
			return false;
		if ((unsigned char) v[i + 1] < low || (unsigned char) v[i + 1] > high)
			return false;
		for (size_t k = 2; k < need; ++k)
		{
			const unsigned char cc = (unsigned char) v[i + k];
			if (cc < 0x80 || cc > 0xbf)
				return false;
		}
		i += need;
	}
	return true;
}

} // namespace

TEST_CASE("the channel list is paged and the cursor is stable", "[endpoints]")
{
	// The buildsystem ships lists of 1238 and 2664 services and users import
	// larger ones, so an unpaged list is not a contract that holds.
	ShippedRoutes shipped;
	FakeChannelSource src;
	fillChannels(src);
	InstalledChannelSource installed(&src);

	const Reply first = get("/api/v1/channels?mode=tv&limit=5");
	REQUIRE(first.code == 200);
	REQUIRE(first.content_type == "application/json");
	REQUIRE(countItems(first.body) == 5);
	const std::string cursor = stringField(first.body, "next_cursor");
	REQUIRE_FALSE(cursor.empty());

	const Reply second = get("/api/v1/channels?mode=tv&limit=5&cursor=" + cursor);
	REQUIRE(second.code == 200);
	REQUIRE(countItems(second.body) == 5);
	// No item may appear in both pages and none may be skipped between them.
	REQUIRE(intersection(ids(first.body), ids(second.body)).empty());
	REQUIRE(concat(ids(first.body), ids(second.body)) == firstTvIds(10));

	// The same cursor asked twice answers the same page.
	REQUIRE(get("/api/v1/channels?mode=tv&limit=5&cursor=" + cursor).body == second.body);
}

TEST_CASE("the last page says there is no next", "[endpoints]")
{
	ShippedRoutes shipped;
	FakeChannelSource src;
	fillChannels(src);
	InstalledChannelSource installed(&src);

	/* Asked for with the widest page the table declares. A number above it is
	   refused by the router, and the refusal carries no next_cursor at all, so
	   a case that asked for one would be reading a problem document and
	   finding nothing in it whatever the paging did. */
	const Reply r = get("/api/v1/channels?mode=tv&limit=500");
	REQUIRE(r.code == 200);
	REQUIRE(countItems(r.body) == kTvCount);

	// Absent and not empty: an empty cursor is an absent one and would answer
	// the first page again.
	REQUIRE_FALSE(hasField(r.body, "next_cursor"));
}

TEST_CASE("a page that ends exactly where the list does says there is no next", "[endpoints]")
{
	ShippedRoutes shipped;
	FakeChannelSource src;
	fillChannels(src);
	InstalledChannelSource installed(&src);

	// The one case where a full page and a cut one look alike from the number
	// of items alone.
	const Reply r = get("/api/v1/channels?mode=tv&limit=6&cursor=" + hex(kFirstTv + 5));
	REQUIRE(r.code == 200);
	REQUIRE(countItems(r.body) == 6);
	REQUIRE_FALSE(hasField(r.body, "next_cursor"));
}

TEST_CASE("nothing this route hands out can be handed back and restart the list", "[endpoints]")
{
	/* Two halves: that the empty spelling really is the absent one, and that no page
	   of any length ever answers with one. */
	ShippedRoutes shipped;
	FakeChannelSource src;
	fillChannels(src);
	InstalledChannelSource installed(&src);

	const Reply empty = get("/api/v1/channels?mode=tv&limit=500&cursor=");
	REQUIRE(empty.code == 200);
	REQUIRE(countItems(empty.body) == kTvCount);

	std::string cursor;
	size_t walked = 0;
	for (size_t page = 0; page < kTvCount + 2; ++page)
	{
		const Reply r = get("/api/v1/channels?mode=tv&limit=5" +
		                    (cursor.empty() ? std::string() : "&cursor=" + cursor));
		REQUIRE(r.code == 200);
		walked += countItems(r.body);
		if (!hasField(r.body, "next_cursor"))
			break;
		cursor = stringField(r.body, "next_cursor");
		// The one value that must never go out, because handing it back is
		// handing back nothing.
		REQUIRE_FALSE(cursor.empty());
	}
	REQUIRE(walked == kTvCount);
}

TEST_CASE("a list holding one id twice is walked to its end and not for ever", "[endpoints]")
{
	/* A cursor names an id, so an id in the list twice names two places the next page
	   could begin, and a page whose last item carries the repeated id hands back a
	   cursor that answers that same page again. Measured before the list was reduced to
	   one item per id: the walk answered the same two items for as many pages as this
	   loop allows and never reached the rest of the list. The duplicate is placed at the
	   end of the second page on purpose, because the placement is what decides between a
	   walk that repeats an item and one that does not advance at all. */
	ShippedRoutes shipped;
	FakeChannelSource src;
	fillChannels(src);
	src.channels[3].id = src.channels[1].id;
	InstalledChannelSource installed(&src);

	std::vector<std::string> walked;
	std::string cursor;
	bool ended = false;
	for (size_t page = 0; page < kTvCount + 2 && !ended; ++page)
	{
		const Reply r = get("/api/v1/channels?mode=tv&limit=2" +
		                    (cursor.empty() ? std::string() : "&cursor=" + cursor));
		REQUIRE(r.code == 200);
		walked = concat(walked, ids(r.body));
		if (!hasField(r.body, "next_cursor"))
		{
			ended = true;
			break;
		}
		cursor = stringField(r.body, "next_cursor");
	}

	// The half that fails when the walk does not advance, and the reason the
	// loop above has a bound at all.
	REQUIRE(ended);

	// One item per id, so the repeated one is answered once and the eleven ids
	// the list holds are each there exactly once.
	REQUIRE(walked.size() == kTvCount - 1);
	for (size_t i = 0; i < walked.size(); ++i)
	{
		INFO(walked[i]);
		REQUIRE(std::count(walked.begin(), walked.end(), walked[i]) == 1);
	}

	// The item kept is the first carrying the id, which is the one the route
	// that answers a single channel answers for it, so the two agree.
	const Reply one = get("/api/v1/channels/" + hex(src.channels[1].id));
	REQUIRE(one.code == 200);
	REQUIRE(parsed(one.body)["number"].asInt() == 2);

	// And everything past the repeat is reached, which is what the walk that
	// did not advance never managed.
	REQUIRE(std::find(walked.begin(), walked.end(), hex(src.channels[kTvCount - 1].id)) != walked.end());
}

TEST_CASE("a cursor that is not one is refused rather than restarting the list", "[endpoints]")
{
	// Restarting silently would make a client loop forever without noticing.
	ShippedRoutes shipped;
	FakeChannelSource src;
	fillChannels(src);
	InstalledChannelSource installed(&src);

	/* The mode is given although it is optional, so that the refusal is about
	   the cursor. Without it this would still be a 400 on a build that had
	   made the mode required, and the case would pass while saying nothing
	   about a cursor at all. */
	const Reply r = get("/api/v1/channels?mode=tv&cursor=zzz");
	REQUIRE(r.code == 400);
	REQUIRE(r.content_type == "application/problem+json");
	REQUIRE(r.body.find("cursor") != std::string::npos);
}

TEST_CASE("a cursor naming a channel the list does not hold is refused rather than restarting", "[endpoints]")
{
	ShippedRoutes shipped;
	FakeChannelSource src;
	fillChannels(src);
	InstalledChannelSource installed(&src);

	// Spelled the way a cursor is spelled and naming nothing, which is what a
	// cursor from a list the box has since reloaded looks like.
	const Reply r = get("/api/v1/channels?mode=tv&limit=5&cursor=" + hex(0xdeadbeefULL));
	REQUIRE(r.code == 400);
	REQUIRE(r.body.find("no-such-channel") != std::string::npos);
	// And not the first page dressed as an answer.
	REQUIRE(r.body.find(hex(kFirstTv)) == std::string::npos);
}

TEST_CASE("the radio list is the other one and not the same one", "[endpoints]")
{
	ShippedRoutes shipped;
	FakeChannelSource src;
	fillChannels(src);
	InstalledChannelSource installed(&src);

	const Reply r = get("/api/v1/channels?mode=radio&limit=500");
	REQUIRE(r.code == 200);
	REQUIRE(countItems(r.body) == kRadioCount);
	REQUIRE(ids(r.body)[0] == hex(kFirstRadio));
}

TEST_CASE("a bouquet answers the mode that was asked for and not both", "[endpoints]")
{
	/* A bouquet keeps its television and its radio members in one answer from
	   the layer below, so a listing that handed that answer straight on would
	   put a radio service in a television list. */
	ShippedRoutes shipped;
	FakeChannelSource src;
	fillChannels(src);
	InstalledChannelSource installed(&src);

	const Reply tv = get("/api/v1/channels?mode=tv&bouquet=3");
	REQUIRE(tv.code == 200);
	REQUIRE(countItems(tv.body) == 2);
	REQUIRE(tv.body.find(hex(kFirstRadio)) == std::string::npos);

	const Reply radio = get("/api/v1/channels?mode=radio&bouquet=3");
	REQUIRE(radio.code == 200);
	REQUIRE(countItems(radio.body) == 1);
	REQUIRE(ids(radio.body)[0] == hex(kFirstRadio));
}

TEST_CASE("a bouquet number wider than the field it is handed to is refused", "[endpoints]")
{
	/* The value is handed on as a thirty two bit number and the accessor answers an
	   unsigned long, and those are the same width on the box and not on the machine
	   this is built on. So a row leaving both ends of its bound at nought was checked by
	   nothing here: two to the thirty two plus three arrived as three and this answered
	   with that bouquet's channels. */
	ShippedRoutes shipped;
	FakeChannelSource src;
	fillChannels(src);
	InstalledChannelSource installed(&src);

	const Reply wide = get("/api/v1/channels?bouquet=4294967299");
	REQUIRE(wide.code == 400);
	REQUIRE(wide.body.find("out-of-range") != std::string::npos);

	// Bouquets are counted from one, so nought names none and the row says so.
	REQUIRE(get("/api/v1/channels?bouquet=0").code == 400);

	// And the one that is there is still answered, so the bound refuses nothing
	// a caller can mean.
	REQUIRE(get("/api/v1/channels?mode=tv&bouquet=3").code == 200);
}

TEST_CASE("a bouquet nobody has is a refusal and not an empty list", "[endpoints]")
{
	ShippedRoutes shipped;
	FakeChannelSource src;
	fillChannels(src);
	InstalledChannelSource installed(&src);

	const Reply r = get("/api/v1/channels?bouquet=4711");
	REQUIRE(r.code == 404);
	REQUIRE(r.body.find("no-such-bouquet") != std::string::npos);
}

TEST_CASE("every item carries its own identifier", "[endpoints]")
{
	// Nothing derives an identifier from list position; that is what made an
	// empty bouquet consume an index in the old server.
	ShippedRoutes shipped;
	FakeChannelSource src;
	fillChannels(src);
	InstalledChannelSource installed(&src);

	const Reply r = get("/api/v1/bouquets");
	REQUIRE(r.code == 200);
	REQUIRE(everyItemHas(r.body, "id"));

	// The bouquets' own numbers, which are not the positions they sit at, so
	// an answer counting them off would read differently here.
	const ::Json::Value items = parsed(r.body)["items"];
	REQUIRE(items.size() == 3);
	REQUIRE(items[0]["id"].asUInt() == 3);
	REQUIRE(items[1]["id"].asUInt() == 7);
	REQUIRE(items[2]["id"].asUInt() == 9);
	// The one that holds nothing is still one of them and still its own
	// number.
	REQUIRE(items[2]["tv_count"].asUInt() == 0);
}

/* WHICH BOUQUETS HOLD ONE CHANNEL, WHICH IS THE ONE THING A SCREEN OPENING ON THE
   RUNNING CHANNEL NEEDS AND CANNOT WORK OUT CHEAPLY FOR ITSELF.

   Held in both directions: the narrowing answers only the bouquets that hold the channel,
   in the box's own order, and asking without it answers the whole list as it always did.
   The order matters because the caller opens on the first of them.

   A bouquet the box will not answer for is not one that holds the channel: 7 and 9 have
   no member list at all in the fixture, and 7 is given one here so a channel in two
   bouquets can be told from a channel in one. */
TEST_CASE("the bouquet listing narrows to the ones holding a channel", "[endpoints]")
{
	ShippedRoutes shipped;
	FakeChannelSource src;
	fillChannels(src);
	coreapi::ChannelList also;
	also.push_back(src.channels[1]);
	src.bouquet_members[7] = also;
	InstalledChannelSource installed(&src);

	// In one bouquet, and it is the one that is answered.
	const Reply one = get("/api/v1/bouquets?holds=" + hex(kFirstTv));
	REQUIRE(one.code == 200);
	const ::Json::Value onlyOne = parsed(one.body)["items"];
	REQUIRE(onlyOne.size() == 1);
	REQUIRE(onlyOne[0]["id"].asUInt() == 3);
	// Narrowed and not cut down: an item is the whole item.
	REQUIRE(onlyOne[0]["name"].asString() == "favourites");
	REQUIRE(onlyOne[0]["tv_count"].asUInt() == 2);

	// In two, and both come back in the order the box holds them.
	const Reply two = get("/api/v1/bouquets?holds=" + hex(kFirstTv + 1));
	REQUIRE(two.code == 200);
	const ::Json::Value both = parsed(two.body)["items"];
	REQUIRE(both.size() == 2);
	REQUIRE(both[0]["id"].asUInt() == 3);
	REQUIRE(both[1]["id"].asUInt() == 7);

	// A channel of the box that no bouquet holds is an empty answer and not a
	// refusal: the question was which bouquets hold it, and the answer is none.
	const Reply none = get("/api/v1/bouquets?holds=" + hex(kFirstTv + 4));
	REQUIRE(none.code == 200);
	REQUIRE(parsed(none.body)["items"].size() == 0);

	// And without it, the whole list, unchanged.
	const Reply all = get("/api/v1/bouquets");
	REQUIRE(all.code == 200);
	REQUIRE(parsed(all.body)["items"].size() == 3);
}

TEST_CASE("a channel that is not in the list is 404 with a problem body", "[endpoints]")
{
	ShippedRoutes shipped;
	FakeChannelSource src;
	fillChannels(src);
	InstalledChannelSource installed(&src);

	const Reply r = get("/api/v1/channels/ffffffffffffffff");
	REQUIRE(r.code == 404);
	REQUIRE(r.content_type == "application/problem+json");
	REQUIRE(r.body.find("no-such-channel") != std::string::npos);
}

TEST_CASE("one channel is answered by its own identifier", "[endpoints]")
{
	ShippedRoutes shipped;
	FakeChannelSource src;
	fillChannels(src);
	InstalledChannelSource installed(&src);

	const Reply r = get("/api/v1/channels/" + hex(kFirstTv + 4));
	REQUIRE(r.code == 200);
	REQUIRE(parsed(r.body)["id"].asString() == hex(kFirstTv + 4));
	REQUIRE(parsed(r.body)["number"].asInt() == 5);
}

TEST_CASE("asking for the current channel with none running is not a crash", "[endpoints]")
{
	// The old server dereferenced a null pointer here.
	ShippedRoutes shipped;
	FakeChannelSource src;
	fillChannels(src);
	// Which is what the fake answers by default, an idle box being an answer
	// and not a fault.
	REQUIRE(src.current_status == coreapi::Status::NotFound);
	InstalledChannelSource installed(&src);

	const Reply r = get("/api/v1/channels/current");
	REQUIRE((r.code == 200 || r.code == 404));
	REQUIRE_FALSE(r.body.empty());
	REQUIRE(r.code == 404);
	REQUIRE(r.body.find("no-running-channel") != std::string::npos);
}

TEST_CASE("the current channel is answered while one is running", "[endpoints]")
{
	ShippedRoutes shipped;
	FakeChannelSource src;
	fillChannels(src);
	src.current = src.channels[2];
	src.current_status = coreapi::Status::Ok;
	InstalledChannelSource installed(&src);

	const Reply r = get("/api/v1/channels/current");
	REQUIRE(r.code == 200);
	REQUIRE(parsed(r.body)["id"].asString() == hex(kFirstTv + 2));
}

/* What the box knows about a channel's streams reaches the one channel answers and not
   the listing.

   A page is five hundred channels and the track list is read off each channel where it
   sits, so a listing that carried it would read five hundred of them to answer one
   request. Held here in both directions: the two single answers carry the three members,
   and the listing carries none of them. */
TEST_CASE("one channel says what its streams are", "[endpoints]")
{
	ShippedRoutes shipped;
	FakeChannelSource src;
	fillChannels(src);

	coreapi::ChannelStreams live;
	live.known = true;
	live.video_codec = coreapi::VideoCodec::H264;
	live.video_pid = 0x17de;
	coreapi::AudioTrack mp2;
	mp2.pid = 0x17e8;
	mp2.codec = coreapi::AudioCodec::Mp2;
	mp2.description = "deu";
	mp2.selected = true;
	live.audio.push_back(mp2);
	coreapi::AudioTrack ac3;
	ac3.pid = 0x17e9;
	ac3.codec = coreapi::AudioCodec::Ac3;
	ac3.description = "deu";
	live.audio.push_back(ac3);
	src.streams[kFirstTv + 4] = live;

	InstalledChannelSource installed(&src);

	const Reply r = get("/api/v1/channels/" + hex(kFirstTv + 4));
	REQUIRE(r.code == 200);
	const ::Json::Value got = parsed(r.body);

	REQUIRE(got["streams_known"].asBool());
	REQUIRE(got["video"]["codec"].asString() == "h264");
	REQUIRE(got["video"]["pid"].asUInt() == 0x17de);
	REQUIRE(got["audio"].isArray());
	REQUIRE(got["audio"].size() == 2);
	REQUIRE(got["audio"][0]["pid"].asUInt() == 0x17e8);
	REQUIRE(got["audio"][0]["codec"].asString() == "mp2");
	REQUIRE(got["audio"][0]["description"].asString() == "deu");
	REQUIRE(got["audio"][0]["selected"].asBool());
	REQUIRE(got["audio"][1]["codec"].asString() == "ac3");
	REQUIRE_FALSE(got["audio"][1]["selected"].asBool());
}

TEST_CASE("a channel nobody has switched to says so rather than guessing", "[endpoints]")
{
	ShippedRoutes shipped;
	FakeChannelSource src;
	fillChannels(src);
	// Nothing filed under any channel, which is every channel of a box whose
	// owner has not switched around on it.
	InstalledChannelSource installed(&src);

	const Reply r = get("/api/v1/channels/" + hex(kFirstTv + 4));
	REQUIRE(r.code == 200);
	const ::Json::Value got = parsed(r.body);

	REQUIRE_FALSE(got["streams_known"].asBool());
	// unknown and not mpeg2, which is what the channel stack's own nought
	// would read as, and not none, which would say the channel carries no
	// picture.
	REQUIRE(got["video"]["codec"].asString() == "unknown");
	REQUIRE(got["video"]["pid"].asUInt() == 0);
	REQUIRE(got["audio"].isArray());
	REQUIRE(got["audio"].size() == 0);
}

TEST_CASE("the running channel says what its streams are as well", "[endpoints]")
{
	ShippedRoutes shipped;
	FakeChannelSource src;
	fillChannels(src);
	src.current = src.channels[kTvCount];
	src.current_status = coreapi::Status::Ok;

	coreapi::ChannelStreams radio;
	radio.known = true;
	radio.video_codec = coreapi::VideoCodec::None;
	coreapi::AudioTrack aac;
	aac.pid = 0x300;
	aac.codec = coreapi::AudioCodec::AacPlus;
	aac.selected = true;
	radio.audio.push_back(aac);
	src.streams[kFirstRadio] = radio;

	InstalledChannelSource installed(&src);

	const Reply r = get("/api/v1/channels/current");
	REQUIRE(r.code == 200);
	const ::Json::Value got = parsed(r.body);

	REQUIRE(got["id"].asString() == hex(kFirstRadio));
	REQUIRE(got["streams_known"].asBool());
	REQUIRE(got["video"]["codec"].asString() == "none");
	REQUIRE(got["audio"].size() == 1);
	REQUIRE(got["audio"][0]["codec"].asString() == "aacplus");
}

TEST_CASE("the channel listing carries no stream members", "[endpoints]")
{
	ShippedRoutes shipped;
	FakeChannelSource src;
	fillChannels(src);

	coreapi::ChannelStreams live;
	live.known = true;
	live.video_codec = coreapi::VideoCodec::Hevc;
	live.video_pid = 0x100;
	for (size_t i = 0; i < kTvCount; ++i)
		src.streams[kFirstTv + i] = live;

	InstalledChannelSource installed(&src);

	const Reply r = get("/api/v1/channels?mode=tv&limit=12");
	REQUIRE(r.code == 200);
	const ::Json::Value got = parsed(r.body);
	REQUIRE(got["items"].size() == kTvCount);
	for (::Json::ArrayIndex i = 0; i < got["items"].size(); ++i)
	{
		INFO("item " << i);
		REQUIRE_FALSE(got["items"][i].isMember("video"));
		REQUIRE_FALSE(got["items"][i].isMember("audio"));
		REQUIRE_FALSE(got["items"][i].isMember("streams_known"));
	}
}

TEST_CASE("an epg window with no bound is refused, not answered with everything", "[endpoints]")
{
	ShippedRoutes shipped;
	FakeEpgSource src;
	InstalledGuideSources installed(&src);

	const Reply open = get("/api/v1/epg?channel=1");
	REQUIRE(open.code == 400);
	// Which end was left out and not merely that something was wrong, so a
	// build that had stopped requiring them would be reported as that.
	REQUIRE(open.body.find("missing-parameter") != std::string::npos);
	// Nothing was read to answer it.
	REQUIRE(src.event_reads == 0);

	const Reply backwards = get("/api/v1/epg?channel=1&from=100&to=99");
	REQUIRE(backwards.code == 400);
	REQUIRE(backwards.body.find("empty-window") != std::string::npos);
	REQUIRE(src.event_reads == 0);
}

TEST_CASE("an epg window answers the events inside it", "[endpoints]")
{
	ShippedRoutes shipped;
	FakeEpgSource src;
	coreapi::EventInfo e;
	e.event_id = 0x11;
	e.channel_id = kFirstTv;
	e.title = "the news";
	e.start = 100;
	e.duration = 100;
	src.events.push_back(e);
	e.event_id = 0x12;
	e.title = "the weather";
	e.start = 100000;
	src.events.push_back(e);
	InstalledGuideSources installed(&src);

	const Reply r = get("/api/v1/epg?channel=" + hex(kFirstTv) + "&from=0&to=1000");
	REQUIRE(r.code == 200);
	REQUIRE(countItems(r.body) == 1);
	REQUIRE(ids(r.body)[0] == hex(0x11));
}

TEST_CASE("a search shorter than the floor is refused", "[endpoints]")
{
	ShippedRoutes shipped;
	FakeEpgSource src;
	InstalledGuideSources installed(&src);

	/* Both ends of the window are given, because they are required and a
	   search missing one is refused for that instead. Without them this would
	   be a 400 that says nothing about the floor on a query. */
	const Reply short_query = get("/api/v1/epg/search?q=a&from=0&to=100");
	REQUIRE(short_query.code == 400);
	REQUIRE(short_query.body.find("query-too-short") != std::string::npos);
	REQUIRE(src.last_query.empty());

	const Reply wide = get("/api/v1/epg/search?q=abc&from=0&to=100&limit=100000");
	REQUIRE(wide.code == 400);
	REQUIRE(wide.body.find("out-of-range") != std::string::npos);
	REQUIRE(src.last_query.empty());
}

TEST_CASE("a search says whether the guide held more than it answered", "[endpoints]")
{
	ShippedRoutes shipped;
	FakeEpgSource src;
	src.honours_bounds = true;
	for (size_t i = 0; i < 5; ++i)
	{
		coreapi::EventInfo e;
		e.event_id = 0x20 + i;
		e.channel_id = kFirstTv;
		e.title = "the news";
		e.start = 100;
		e.duration = 100;
		src.events.push_back(e);
	}
	InstalledGuideSources installed(&src);

	const Reply cut = get("/api/v1/epg/search?q=news&from=0&to=1000&limit=2");
	REQUIRE(cut.code == 200);
	REQUIRE(countItems(cut.body) == 2);
	REQUIRE(parsed(cut.body)["truncated"].asBool());

	const Reply whole = get("/api/v1/epg/search?q=news&from=0&to=1000&limit=200");
	REQUIRE(whole.code == 200);
	REQUIRE(countItems(whole.body) == 5);
	REQUIRE_FALSE(parsed(whole.body)["truncated"].asBool());
}

/* The grid, and the one thing about it a screen cannot notice for itself: an
   answer that was too big to send whole. */

// One row of the grid answer, by the channel it is for, so that a case reads a
// row by name rather than by where it happens to sit.
::Json::Value gridRow(const std::string &body, const std::string &channel)
{
	const ::Json::Value items = parsed(body)["items"];
	for (::Json::ArrayIndex i = 0; i < items.size(); ++i)
	{
		if (items[i]["channel_id"].asString() == channel)
			return items[i];
	}
	return ::Json::Value();
}

TEST_CASE("a grid has to say which channels it is for", "[endpoints]")
{
	ShippedRoutes shipped;
	FakeChannelSource channels;
	fillChannels(channels);
	InstalledChannelSource installed_channels(&channels);
	FakeEpgSource src;
	InstalledEpgSource installed(&src);

	/* Neither is not a request for every channel the box has: that is the
	   unbounded answer this route exists to not give. */
	const Reply neither = get("/api/v1/epg/grid?from=0&to=100000");
	REQUIRE(neither.code == 400);
	REQUIRE(neither.body.find("missing-parameter") != std::string::npos);
	REQUIRE(src.event_reads == 0);

	// And both is a request with two answers, so preferring one of them would
	// leave the caller never learning the other half was dropped.
	const Reply both = get("/api/v1/epg/grid?from=0&to=100000&bouquet=3&channels=" +
			       hex(kFirstTv));
	REQUIRE(both.code == 400);
	REQUIRE(both.body.find("conflicting-parameters") != std::string::npos);
	REQUIRE(src.event_reads == 0);
}

TEST_CASE("a grid answers a row for every channel it was given", "[endpoints]")
{
	ShippedRoutes shipped;
	FakeChannelSource channels;
	fillChannels(channels);
	InstalledChannelSource installed_channels(&channels);

	FakeEpgSource src;
	for (size_t i = 0; i < 3; ++i)
	{
		coreapi::EventInfo e;
		e.event_id = 0x40 + i;
		e.channel_id = kFirstTv;
		e.title = "the news";
		e.start = (time_t)(i * 100);
		e.duration = 100;
		src.events.push_back(e);
	}
	InstalledEpgSource installed(&src);

	const Reply named = get("/api/v1/epg/grid?from=0&to=1000&channels=" +
				hex(kFirstTv) + "," + hex(kFirstTv + 1));
	REQUIRE(named.code == 200);
	REQUIRE(countItems(named.body) == 2);
	REQUIRE(gridRow(named.body, hex(kFirstTv))["events"].size() == 3);
	// A channel with nothing scheduled is an empty row and not a row left out,
	// or a caller could not tell it from a channel it never asked about.
	REQUIRE(gridRow(named.body, hex(kFirstTv + 1))["events"].size() == 0);
	REQUIRE_FALSE(hasField(named.body, "next_cursor"));

	/* A bouquet answers both of its halves here. The listing route narrows a
	   bouquet to one of them and defaults to television, and a grid doing the
	   same would leave the radio half missing with nothing saying so. */
	const Reply bouquet = get("/api/v1/epg/grid?from=0&to=1000&bouquet=3");
	REQUIRE(bouquet.code == 200);
	REQUIRE(countItems(bouquet.body) == 3);
	REQUIRE(gridRow(bouquet.body, hex(kFirstRadio))["events"].size() == 0);
}

/* The failure a screen never notices. A page that ended at its own ceiling on
   events holds fewer rows than were asked for and is not the last page, so a
   caller reading the number of rows stops in the middle of the guide and the
   user sees channels that are simply not there. What says otherwise is the
   cursor, and this is what holds it to being there. */
TEST_CASE("a grid page that came back short still says where to go on", "[endpoints]")
{
	ShippedRoutes shipped;
	FakeChannelSource channels;
	fillChannels(channels);
	InstalledChannelSource installed_channels(&channels);

	const size_t rows = (coreapi::epg::MAX_EVENTS_PER_PAGE /
			     coreapi::epg::MAX_EVENTS_PER_CHANNEL) + 2;
	REQUIRE(rows <= coreapi::epg::MAX_GRID_CHANNELS);

	FakeEpgSource src;
	std::string asked;
	for (size_t i = 0; i < rows; ++i)
	{
		const uint64_t channel = kFirstTv + i;
		if (i > 0)
			asked += ",";
		asked += hex(channel);
		for (size_t k = 0; k < coreapi::epg::MAX_EVENTS_PER_CHANNEL; ++k)
		{
			coreapi::EventInfo e;
			e.event_id = 1 + i * 1000 + k;
			e.channel_id = channel;
			e.title = "the news";
			e.start = (time_t)(k * 1800);
			e.duration = 1800;
			src.events.push_back(e);
		}
	}
	InstalledEpgSource installed(&src);

	char limit[32];
	std::snprintf(limit, sizeof(limit), "%lu", (unsigned long) rows);
	const std::string target = "/api/v1/epg/grid?from=0&to=1000000&limit=" +
				   std::string(limit) + "&channels=" + asked;

	const Reply page = get(target);
	REQUIRE(page.code == 200);
	REQUIRE(countItems(page.body) < rows);
	REQUIRE(hasField(page.body, "next_cursor"));

	// And the cursor reaches what the short page did not.
	const Reply rest = get(target + "&cursor=" + stringField(page.body, "next_cursor"));
	REQUIRE(rest.code == 200);
	REQUIRE(countItems(rest.body) == rows - countItems(page.body));
	REQUIRE_FALSE(hasField(rest.body, "next_cursor"));

	// A cursor from some other list is refused rather than answered with the
	// first page, which a caller walking until the cursor stops coming back
	// would never see.
	const Reply astray = get(target + "&cursor=" + hex(kFirstRadio));
	REQUIRE(astray.code == 400);
	REQUIRE(astray.body.find("no-such-channel") != std::string::npos);
}

TEST_CASE("a grid row cut at its ceiling says so", "[endpoints]")
{
	ShippedRoutes shipped;
	FakeChannelSource channels;
	fillChannels(channels);
	InstalledChannelSource installed_channels(&channels);

	FakeEpgSource src;
	for (size_t k = 0; k < coreapi::epg::MAX_EVENTS_PER_CHANNEL + 1; ++k)
	{
		coreapi::EventInfo e;
		e.event_id = 1 + k;
		e.channel_id = kFirstTv;
		e.title = "the news";
		e.start = (time_t)(k * 1800);
		e.duration = 1800;
		src.events.push_back(e);
	}
	coreapi::EventInfo one;
	one.event_id = 0x9000;
	one.channel_id = kFirstTv + 1;
	one.title = "the weather";
	one.start = 0;
	one.duration = 1800;
	src.events.push_back(one);
	InstalledEpgSource installed(&src);

	const Reply r = get("/api/v1/epg/grid?from=0&to=1000000&channels=" +
			    hex(kFirstTv) + "," + hex(kFirstTv + 1));
	REQUIRE(r.code == 200);

	const ::Json::Value cut = gridRow(r.body, hex(kFirstTv));
	REQUIRE(cut["events"].size() == coreapi::epg::MAX_EVENTS_PER_CHANNEL);
	REQUIRE(cut["truncated"].asBool());

	// The row beside it was not cut, so the word is about the row and not
	// about the answer.
	REQUIRE_FALSE(gridRow(r.body, hex(kFirstTv + 1))["truncated"].asBool());
}

TEST_CASE("a grid turns down more channels than one request names", "[endpoints]")
{
	ShippedRoutes shipped;
	FakeEpgSource src;
	InstalledGuideSources installed(&src);

	std::string asked;
	/* One past the ceiling, written out here because the ceiling is private to
	   the module that holds it and a number this case read off the route would
	   be a number the route could move without anything noticing. The answer
	   below names the ceiling, so a run that fails here says what the two
	   disagree about. */
	for (size_t i = 0; i < 201; ++i)
	{
		if (i > 0)
			asked += ",";
		asked += hex(kFirstTv + i);
	}

	const Reply many = get("/api/v1/epg/grid?from=0&to=1000&channels=" + asked);
	REQUIRE(many.code == 400);
	REQUIRE(many.body.find("too-many-channels") != std::string::npos);
	REQUIRE(src.event_reads == 0);

	// Counted in channels and not in bytes: a caller told its value was too
	// long would have to work back from bytes to channels to learn what it may
	// ask for.
	REQUIRE(many.body.find("value-too-long") == std::string::npos);
}

TEST_CASE("the channels of a grid have to be identifiers", "[endpoints]")
{
	ShippedRoutes shipped;
	FakeEpgSource src;
	InstalledGuideSources installed(&src);

	const char *const wrong[] = { "zzzz", "", "1,,2", "1,", "12345678901234567" };
	for (size_t i = 0; i < sizeof(wrong) / sizeof(wrong[0]); ++i)
	{
		INFO(wrong[i]);
		const Reply r = get(std::string("/api/v1/epg/grid?from=0&to=1000&channels=") +
				    wrong[i]);
		REQUIRE(r.code == 400);
		REQUIRE(src.event_reads == 0);
	}
}

TEST_CASE("one event answers the age, the class and the long text", "[endpoints]")
{
	ShippedRoutes shipped;
	FakeEpgSource src;
	coreapi::EventDetail d;
	d.event_id = 0x2b66abcdULL;
	d.channel_id = kFirstTv;
	d.title = "Tatort";
	d.description = "Krimireihe";
	d.long_description = "Ein Fall fuer zwei Kommissare.";
	d.start = 1000;
	d.duration = 5400;
	d.rating = 12;
	d.genre = 0x11;
	src.details.push_back(d);
	InstalledGuideSources installed(&src);

	const Reply r = get("/api/v1/epg/event?id=" + hex(d.event_id) + "&start=1000");
	REQUIRE(r.code == 200);
	const ::Json::Value got = parsed(r.body);
	REQUIRE(got["id"].asString() == hex(d.event_id));
	// The two texts apart, which is the whole of why this route is here beside
	// the listings: a listing carries one field for both.
	REQUIRE(got["description"].asString() == "Krimireihe");
	REQUIRE(got["long_description"].asString() == "Ein Fall fuer zwei Kommissare.");
	REQUIRE(got["rating"].asUInt() == 12);
	REQUIRE(got["genre"].asUInt() == 0x11);

	// The moment is half the name, so the identifier on its own does not
	// address a showing.
	const Reply other = get("/api/v1/epg/event?id=" + hex(d.event_id) + "&start=2000");
	REQUIRE(other.code == 404);
	REQUIRE(other.body.find("no-such-event") != std::string::npos);

	const Reply unnamed = get("/api/v1/epg/event?id=" + hex(d.event_id));
	REQUIRE(unnamed.code == 400);
	REQUIRE(unnamed.body.find("missing-parameter") != std::string::npos);
}

TEST_CASE("the settings schema answers every declared row", "[endpoints]")
{
	ShippedRoutes shipped;

	const Reply r = get("/api/v1/settings/schema");
	REQUIRE(r.code == 200);
	// The number comes from the layer, not from a constant typed here, so a
	// row added later does not need this case edited.
	const size_t declared = coreapi::settings::schema().value().size();
	REQUIRE(declared > 0);
	REQUIRE(countItems(r.body) == declared);
	REQUIRE(everyItemHas(r.body, "id"));
}

/* The one place a caller other than the tables themselves reads label_key: the
   wire. Before this the field carried label_key's own spelling, such as
   videomenu.videoformat_169 for the choice a screen shows as 16:9, and these
   two cases are what is held to that no longer being so. */

TEST_CASE("a schema label resolves through the installed catalog, not the key", "[endpoints]")
{
	ShippedRoutes shipped;

	FakeLocaleSource cat;
	cat.texts["videomenu.videoformat"] = "Format";
	cat.texts["videomenu.videoformat_169"] = "16:9";
	InstalledLocaleSource installed(&cat);

	const Reply r = get("/api/v1/settings/schema");
	REQUIRE(r.code == 200);
	const ::Json::Value items = parsed(r.body)["items"];

	::Json::Value found;
	bool have = false;
	for (::Json::ArrayIndex i = 0; i < items.size() && !have; ++i)
	{
		if (items[i]["id"].asString() == "video_Format")
		{
			found = items[i];
			have = true;
		}
	}
	REQUIRE(have);

	REQUIRE(found.isMember("label"));
	CHECK(found["label"].asString() == "Format");
	// Never the key it was named by, in either direction: this would also pass
	// a route that had forgotten to resolve at all if the fake text happened
	// to equal the key, so the fake text above is deliberately not that.
	CHECK(found["label"].asString() != "videomenu.videoformat");

	bool have_choice = false;
	for (::Json::ArrayIndex i = 0; i < found["values"].size(); ++i)
	{
		if (found["values"][i]["value"].asInt() != 3)
			continue;
		have_choice = true;
		REQUIRE(found["values"][i].isMember("label"));
		CHECK(found["values"][i]["label"].asString() == "16:9");
		CHECK(found["values"][i]["label"].asString() != "videomenu.videoformat_169");
	}
	REQUIRE(have_choice);
}

/* The values of a setting the box decides, which carries none of its own. A
   caller reads one member whichever sort of row arrived, so the schema is the
   whole of the settings surface and there is no second call to make. */
TEST_CASE("the schema answers the values a setting's own box decides", "[endpoints]")
{
	ShippedRoutes shipped;

	FakeSettingChoices screens;
	screens.offer("video_mode", 7, "720p 50Hz");
	screens.offer("video_mode", 12, "1080p 50Hz");
	InstalledSettingChoices installed(&screens);

	const Reply r = get("/api/v1/settings/schema");
	REQUIRE(r.code == 200);
	const ::Json::Value items = parsed(r.body)["items"];

	::Json::Value found;
	bool have = false;
	for (::Json::ArrayIndex i = 0; i < items.size() && !have; ++i)
	{
		if (items[i]["id"].asString() == "video_Mode")
		{
			found = items[i];
			have = true;
		}
	}
	REQUIRE(have);

	REQUIRE(found["values"].size() == 2);
	CHECK(found["values"][0]["value"].asInt() == 7);
	CHECK(found["values"][0]["label"].asString() == "720p 50Hz");
	CHECK(found["values"][1]["value"].asInt() == 12);
	CHECK(found["values"][1]["label"].asString() == "1080p 50Hz");
	CHECK(screens.asked > 0);
}

/* And what the same row looks like while the box cannot be asked, which is not
   a setting without a choice: it is a choice nobody can state at this moment,
   and a write of any value is refused for as long as that lasts. */
TEST_CASE("a setting whose box cannot be asked answers an empty list of values", "[endpoints]")
{
	ShippedRoutes shipped;

	const Reply r = get("/api/v1/settings/schema");
	REQUIRE(r.code == 200);
	const ::Json::Value items = parsed(r.body)["items"];

	bool have = false;
	for (::Json::ArrayIndex i = 0; i < items.size() && !have; ++i)
	{
		if (items[i]["id"].asString() != "video_Mode")
			continue;
		have = true;
		REQUIRE(items[i].isMember("values"));
		CHECK(items[i]["values"].size() == 0);
		// The member is there, which is what says the setting is a choice at
		// all: a row of another kind carries no values member to be empty.
		CHECK(items[i]["type"].asString() == "enum");
	}
	REQUIRE(have);
}

TEST_CASE("a schema label the catalog does not carry is left out, not the key", "[endpoints]")
{
	ShippedRoutes shipped;

	// Installed and empty rather than nothing installed at all, so this reads
	// as the catalog genuinely missing the name rather than as the seam's own
	// default doing the work the route is meant to.
	FakeLocaleSource cat;
	InstalledLocaleSource installed(&cat);

	const Reply r = get("/api/v1/settings/schema");
	REQUIRE(r.code == 200);
	const ::Json::Value items = parsed(r.body)["items"];

	bool checked_one = false;
	for (::Json::ArrayIndex i = 0; i < items.size(); ++i)
	{
		if (items[i]["id"].asString() != "video_Format")
			continue;
		checked_one = true;
		REQUIRE_FALSE(items[i].isMember("label"));
	}
	REQUIRE(checked_one);
	// Nowhere in the whole document, which is the stronger claim the field by
	// field check above cannot make on its own.
	CHECK(r.body.find("videomenu.videoformat") == std::string::npos);
}

TEST_CASE("a secret row is described but never valued", "[endpoints]")
{
	ShippedRoutes shipped;

	const std::vector<coreapi::Descriptor> rows = coreapi::settings::schema().value();
	const coreapi::Descriptor *secret = rowIn(rows, "network", true);
	const coreapi::Descriptor *open = rowIn(rows, "network", false);
	REQUIRE(secret != NULL);
	REQUIRE(open != NULL);

	/* Both are put in the store with something in them. The one that is not a
	   credential is what says the store is being read at all, so the empty
	   answer for the other one is the withholding and not a section nobody
	   wired up. */
	FakeSettingsSource store;
	store.strings[secret->key] = "hunter2";
	store.strings[open->key] = "proxy.example";
	InstalledSettingsSource installed(&store);

	const Reply described = get("/api/v1/settings/schema");
	REQUIRE(described.code == 200);
	REQUIRE(described.body.find("\"secret\":true") != std::string::npos);

	const Reply one = get("/api/v1/settings/network");
	REQUIRE(one.code == 200);
	REQUIRE(valueOf(one.body, open->key) == "proxy.example");
	REQUIRE(valueOf(one.body, secret->key) == "");
	// And the row is there to be drawn a field for, rather than left out.
	REQUIRE(valueOf(one.body, secret->key) != "<no such item>");
	// Nowhere in the answer, and not merely absent from the member read above.
	REQUIRE(one.body.find("hunter2") == std::string::npos);
}

TEST_CASE("a section nobody declared is 404", "[endpoints]")
{
	ShippedRoutes shipped;

	const Reply r = get("/api/v1/settings/not-a-section");
	REQUIRE(r.code == 404);
	REQUIRE(r.content_type == "application/problem+json");
}

TEST_CASE("the sections are the ones the schema names", "[endpoints]")
{
	ShippedRoutes shipped;

	const Reply r = get("/api/v1/settings/sections");
	REQUIRE(r.code == 200);
	const std::vector<std::string> declared = coreapi::settings::sections().value();
	REQUIRE(declared.size() > 0);
	REQUIRE(countItems(r.body) == declared.size());

	const std::vector<std::string> answered = ids(r.body);
	REQUIRE(answered == declared);
}

TEST_CASE("a file listing outside the roots is refused", "[endpoints]")
{
	ShippedRoutes shipped;
	StorageFixture storage;

	const Reply r = get("/api/v1/storage/files?path=/etc", AuthLevel::System);
	REQUIRE(r.code == 400);
	REQUIRE(r.body.find("outside-roots") != std::string::npos);
}

/* Two names, one under a root the media scope admits and one under a root it does not,
   with the roots left as this build ships them.

   THE LIST IS NOT WRITTEN BY THIS CASE, AND THAT IS WHAT MAKES IT HOLD. The routes put
   their own list together out of the shipped one before they look at a name, so a list a
   case writes is a list the next request throws away. What this case rests on is true of
   the shipped list itself and of anything a refresh builds on top of it: /mnt is one of
   the media a box records to and /tmp is not, so one name is admitted and the other is
   refused however the list was arrived at.

   Both names are put back whichever way the case leaves, a failed check leaving by
   throwing. /mnt and /tmp and not a directory of this suite's own, because what the rule
   is written in terms of is the roots the build ships and there is no way to say that in
   a sandbox. */
struct MediaAndElsewhere
{
	std::string dir;
	std::string mine;
	std::string theirs;
	std::string before;

	MediaAndElsewhere() : before(coreapi::storage::roots())
	{
		char tmpl[] = "/mnt/httpd_endpoints_scope_XXXXXX";
		if (mkdtemp(tmpl) != NULL)
			dir = tmpl;
		mine = dir + "/recording.ts";
		theirs = "/tmp/httpd-endpoints-scope-elsewhere";

		/* The shipped list, which is where a refresh starts from too, so what this
		   case asks is answered the same whether or not anything rebuilds it. */
		coreapi::storage::setRoots(coreapi::storage::defaultRoots());
	}

	~MediaAndElsewhere()
	{
		coreapi::storage::setRoots(before);
		remove(mine.c_str());
		remove(theirs.c_str());
		if (!dir.empty())
			rmdir(dir.c_str());
	}

	private:
		MediaAndElsewhere(const MediaAndElsewhere &);
		MediaAndElsewhere &operator=(const MediaAndElsewhere &);
};

TEST_CASE("a credential for part of the box reaches that part and no other root", "[endpoints]")
{
	/* WHAT A LEAKED ADDRESS IS WORTH. The address of a recording carries a token, so it
	   survives in a history list and in whatever it was pasted into, and the whole
	   answer to that is this: the token stands for the media a box records to, and the
	   rest of the roots are out of its reach. The directory this server keeps its own
	   configuration in is among the rest, and so is the one it writes temporary files
	   to, which is the pair the two names below stand for. */
	ShippedRoutes shipped;
	MediaAndElsewhere box;

	REQUIRE_FALSE(box.dir.empty());
	REQUIRE(putFile(box.mine, "the bytes"));
	REQUIRE(putFile(box.theirs, "not yours"));

	// The same two names, asked for by a caller whose credential has no scope. Both are
	// inside the roots, which is what makes the refusal below a refusal about the scope
	// and not about the roots.
	REQUIRE(get("/api/v1/storage/file?path=" + box.mine, AuthLevel::System).code == 200);
	REQUIRE(get("/api/v1/storage/file?path=" + box.theirs, AuthLevel::System).code == 200);

	// And by one whose credential stands for the media of this box.
	REQUIRE(getScoped("/api/v1/storage/file?path=" + box.mine, "media").code == 200);

	const Reply refused = getScoped("/api/v1/storage/file?path=" + box.theirs, "media");
	REQUIRE(refused.code == 403);
	// The refusal names no directory, so the media of this box cannot be read out of it.
	REQUIRE(refused.body.find("/tmp") == std::string::npos);

	/* A walk upwards out of the admitted root is the same refusal, the comparison being
	   over what the name resolves to and not over the characters it was written with. */
	REQUIRE(getScoped("/api/v1/storage/file?path=" + box.dir +
	                  "/../../tmp/httpd-endpoints-scope-elsewhere", "media").code == 403);

	// A scope this server does not resolve reaches nothing at all, rather than
	// everything.
	REQUIRE(getScoped("/api/v1/storage/file?path=" + box.mine, "everything").code == 403);
}

TEST_CASE("the name a token travels under is not a parameter anywhere", "[endpoints]")
{
	/* The router refuses a name a route did not declare, which is what makes a query a
	   closed thing. The credential is not declared, so on the one route that takes one
	   it has to be taken out before that comparison, and everywhere else it has to stay
	   a name nobody declared.

	   Both halves are needed. Without the first the route answers 400 to every address
	   this box shows beside a recording; without the second the name would be quietly
	   accepted on routes that never read it, and a caller putting a secret there would
	   get a 200 telling it nothing was wrong. */
	ShippedRoutes shipped;
	StorageFixture storage;

	const std::string mine = storage.box.at("recording.ts");
	REQUIRE(putFile(mine, "the bytes"));

	const std::string credential = std::string(queryTokenName()) + "=whatever";

	// The route that takes one: the name is the transport's and the handler never
	// sees it, so the answer is the file.
	REQUIRE(get("/api/v1/storage/file?path=" + mine + "&" + credential,
	            AuthLevel::System).code == 200);

	// The listing beside it, which does not take one.
	const Reply listing = get("/api/v1/storage/files?path=" + storage.box.dir + "&" +
	                          credential, AuthLevel::System);
	REQUIRE(listing.code == 400);
	REQUIRE(listing.body.find("no-such-parameter") != std::string::npos);
}

TEST_CASE("a listing does not state attributes it could not read", "[endpoints]")
{
	/* Nought is a real owner and a real size, so a name the filesystem would
	   not answer for has to come back without those members rather than with
	   zeroes in them. */
	ShippedRoutes shipped;
	StorageFixture storage;

	const int fd = open(storage.box.at("real").c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0600);
	REQUIRE(fd >= 0);
	close(fd);
	REQUIRE(symlink(storage.box.at("nothing-there").c_str(), storage.box.at("broken").c_str()) == 0);

	const Reply r = get("/api/v1/storage/files?path=" + storage.box.dir, AuthLevel::System);
	REQUIRE(r.code == 200);

	const ::Json::Value items = parsed(r.body)["items"];
	bool saw_real = false;
	bool saw_broken = false;
	for (::Json::ArrayIndex i = 0; i < items.size(); ++i)
	{
		const std::string name = items[i]["id"].asString();
		if (name == "real")
		{
			saw_real = true;
			REQUIRE(items[i]["attributes_read"].asBool());
			REQUIRE(items[i].isMember("size"));
			REQUIRE(items[i].isMember("uid"));
		}
		if (name == "broken")
		{
			saw_broken = true;
			REQUIRE_FALSE(items[i]["attributes_read"].asBool());
			REQUIRE_FALSE(items[i].isMember("size"));
			REQUIRE_FALSE(items[i].isMember("uid"));
			// Still in the listing, and still saying what the directory said
			// it was.
			REQUIRE(items[i]["kind"].asString() == "link");
		}
	}
	REQUIRE(saw_real);
	REQUIRE(saw_broken);
}

TEST_CASE("the mounts are named by where they are mounted", "[endpoints]")
{
	ShippedRoutes shipped;
	StorageFixture storage;

	const Reply r = get("/api/v1/storage/mounts");
	REQUIRE(r.code == 200);
	REQUIRE(countItems(r.body) == 2);
	REQUIRE(ids(r.body)[0] == "/media/sda1");
	REQUIRE(parsed(r.body)["items"][0]["device"].asString() == "/dev/sda1");
}

TEST_CASE("the daemons are named by the name that drives them", "[endpoints]")
{
	ShippedRoutes shipped;
	ProcFixture proc;

	const Reply r = get("/api/v1/daemons");
	REQUIRE(r.code == 200);
	REQUIRE(everyItemHas(r.body, "id"));
	REQUIRE(countItems(r.body) == coreapi::daemons::list().value().size());
	// Nothing is running under an empty process table, which is what says the
	// answer was read rather than assumed.
	const ::Json::Value items = parsed(r.body)["items"];
	for (::Json::ArrayIndex i = 0; i < items.size(); ++i)
		REQUIRE_FALSE(items[i]["running"].asBool());
}

/* The page asks for this route by name, and the page is the same bytes on every box:
   nothing in it can see whether this build carries the copied surface. A route that is
   there in one build and missing from the other is a page that answers 404 on the
   other with nothing to say why, and test/web/check-web-paths.sh could only be green
   in one of the two. So the route is answered by both, and a build carrying none of
   that surface says there is nothing to count rather than going missing. */
TEST_CASE("the counter for the copied surface is a route in every build", "[endpoints]")
{
	ShippedRoutes shipped;

	const Reply r = get("/api/v1/system/legacy-usage");
	REQUIRE(r.code == 200);
	REQUIRE(parsed(r.body)["items"].isArray());

#ifdef DISABLE_LEGACY_API
	// Nothing was copied into this build, so no name of it was ever asked for.
	REQUIRE(countItems(r.body) == 0);
#else
	/* The whole of the known table, each name carried even at a count of
	   nought, which is what makes an empty list above mean the build and not
	   an idle box. */
	REQUIRE(countItems(r.body) > 0);
	REQUIRE(everyItemHas(r.body, "name"));
	REQUIRE(everyItemHas(r.body, "calls"));
#endif
}

TEST_CASE("the box says what it is and what this interface is", "[endpoints]")
{
	ShippedRoutes shipped;
	FakeSystemSource src;
	src.box.vendor = "a vendor";
	src.box.model = "a model";
	src.box.uptime = 4711;
	InstalledSystemSource installed(&src);

	const Reply r = get("/api/v1/system/info");
	REQUIRE(r.code == 200);
	const ::Json::Value root = parsed(r.body);
	REQUIRE(root["model"].asString() == "a model");
	REQUIRE(root["uptime"].asInt64() == 4711);
	REQUIRE(root["api"]["major"].asUInt() == NEUTRINO_API_VERSION_MAJOR);
	REQUIRE(root["api"]["minor"].asUInt() == NEUTRINO_API_VERSION_MINOR);
}

TEST_CASE("whether the box is in standby is a reading and not only a command", "[endpoints]")
{
	/* The command that puts a box into standby answers accepted and nothing
	   more, so a page offering it had two buttons where the design has one
	   switch. Standby is one of the modes the box settles into, so this reads
	   the mode. */
	ShippedRoutes shipped;
	FakeChannelSource src;
	InstalledChannelSource installed(&src);

	src.mode = NeutrinoModes::mode_standby;
	Reply r = get("/api/v1/system/standby");
	REQUIRE(r.code == 200);
	REQUIRE(parsed(r.body)["on"].asBool());

	const Endpoint *ep = routeFor("/api/v1/system/standby");
	REQUIRE(ep != NULL);
	REQUIRE(ep->schema != NULL);
	checkShape(parsed(r.body), *ep->schema, "/api/v1/system/standby");

	src.mode = NeutrinoModes::mode_tv;
	r = get("/api/v1/system/standby");
	REQUIRE(r.code == 200);
	REQUIRE_FALSE(parsed(r.body)["on"].asBool());

	/* A box that has not settled on a mode is coming up, and coming up is the
	   opposite of standby. Answered rather than refused, because a switch with
	   a refusal behind it is a switch that draws nothing at all. */
	src.mode_status = coreapi::Status::NotFound;
	r = get("/api/v1/system/standby");
	REQUIRE(r.code == 200);
	REQUIRE_FALSE(parsed(r.body)["on"].asBool());
}

namespace
{

/* The file a softcam leaves behind, put somewhere this suite owns. The machine
   running these has a /tmp of its own and may well have a softcam on it, and a
   case reading the real name would answer differently there. */
struct EcmFixture
{
	std::string dir;
	std::string before;

	EcmFixture()
	{
		char tmpl[] = "/tmp/endpoints_decryption_XXXXXX";
		if (mkdtemp(tmpl) != NULL)
			dir = tmpl;
		before = coreapi::decryption::ecmInfoPath();
		coreapi::decryption::setEcmInfoPath(path());
	}

	~EcmFixture()
	{
		coreapi::decryption::setEcmInfoPath(before);
		if (!dir.empty())
			(void) system(("rm -rf " + dir).c_str());
	}

	std::string path() const { return dir + "/ecm.info"; }

	void answering() const
	{
		const int fd = open(path().c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0600);
		REQUIRE(fd >= 0);
		REQUIRE(write(fd, "caid: 1801\n", 11) == 11);
		close(fd);
	}

	void silent() const { (void) unlink(path().c_str()); }
};

} // namespace

TEST_CASE("what the channel is being descrambled with is two answers of its own", "[endpoints]")
{
	ShippedRoutes shipped;
	EcmFixture ecm;
	REQUIRE_FALSE(ecm.dir.empty());

	FakeSystemSource src;
	src.ci_slots = 1;
	src.ci_seated.push_back(true);
	src.ci_used = true;
	InstalledSystemSource installed(&src);

	ecm.answering();
	Reply r = get("/api/v1/system/decryption");
	REQUIRE(r.code == 200);
	REQUIRE(parsed(r.body)["softcam"].asBool());
	REQUIRE(parsed(r.body)["ci_module"].asBool());

	const Endpoint *ep = routeFor("/api/v1/system/decryption");
	REQUIRE(ep != NULL);
	REQUIRE(ep->schema != NULL);
	checkShape(parsed(r.body), *ep->schema, "/api/v1/system/decryption");

	// One at a time, so neither member is being written out of the other.
	ecm.silent();
	r = get("/api/v1/system/decryption");
	REQUIRE(r.code == 200);
	REQUIRE_FALSE(parsed(r.body)["softcam"].asBool());
	REQUIRE(parsed(r.body)["ci_module"].asBool());

	ecm.answering();
	src.ci_used = false;
	r = get("/api/v1/system/decryption");
	REQUIRE(r.code == 200);
	REQUIRE(parsed(r.body)["softcam"].asBool());
	REQUIRE_FALSE(parsed(r.body)["ci_module"].asBool());
}

/* Two boxes that have to be answered rather than refused: one built with no
   common interface at all, and one sitting in standby with nothing tuned. Both
   read as nothing descrambling, and the answer says so at two hundred. */
TEST_CASE("a box with no slot and a box with nothing on answer rather than refuse", "[endpoints]")
{
	ShippedRoutes shipped;
	EcmFixture ecm;
	REQUIRE_FALSE(ecm.dir.empty());
	ecm.silent();

	FakeSystemSource src;
	src.ci_slots = 0;
	src.ci_used = false;
	InstalledSystemSource installed(&src);

	const Reply r = get("/api/v1/system/decryption");
	REQUIRE(r.code == 200);
	REQUIRE_FALSE(parsed(r.body)["softcam"].asBool());
	REQUIRE_FALSE(parsed(r.body)["ci_module"].asBool());
	REQUIRE(src.ci_asked.empty());
}

/* The same level as the box facts beside it. It says how the box descrambles
   and nothing of what with, so it is a reading a caller on the home network is
   already trusted with. */
TEST_CASE("how the box descrambles is read at the level of the facts beside it", "[endpoints]")
{
	ShippedRoutes shipped;

	const Endpoint *mine = routeFor("/api/v1/system/decryption");
	const Endpoint *info = routeFor("/api/v1/system/info");
	REQUIRE(mine != NULL);
	REQUIRE(info != NULL);
	REQUIRE(mine->auth == info->auth);
	REQUIRE(mine->auth == AuthLevel::Read);
}

TEST_CASE("an uptime below nought is a reading that failed and not a duration", "[endpoints]")
{
	/* The row says there is no sign, and the layer below carries the number in
	   the clock's own type, which has one. A box that could not read its uptime
	   at all leaves nought behind, so a reading below nought answers the same
	   thing rather than arriving as a negative number under a member that says
	   there are none. */
	ShippedRoutes shipped;
	FakeSystemSource src;
	src.box.uptime = -1;
	InstalledSystemSource installed(&src);

	const Reply r = get("/api/v1/system/info");
	REQUIRE(r.code == 200);
	REQUIRE(parsed(r.body)["uptime"].asInt64() == 0);
	REQUIRE(r.body.find("-1") == std::string::npos);

	const Endpoint *ep = routeFor("/api/v1/system/info");
	REQUIRE(ep != NULL);
	REQUIRE(ep->schema != NULL);
	checkShape(parsed(r.body), *ep->schema, "/api/v1/system/info");
}

TEST_CASE("a name the box never converted does not leave a body no reader will take", "[endpoints]")
{
	/* A channel name comes off a broadcast stream, which promises no encoding,
	   and a body carrying a byte that is not text is a body a strict reader
	   refuses whole: one channel nobody converted would cost a client every
	   answer that names it. The tree's own parser takes such a body, which is
	   why this is asked of the bytes and not of the parser. */
	ShippedRoutes shipped;
	FakeChannelSource src;
	fillChannels(src);
	std::string broken = "Pro";
	broken += (char) 0xdf;          // a lone lead byte, which begins nothing
	broken += "Sieben";
	src.channels[0].name = broken;
	// And one that is text, so the case is not passed by a writer that replaces
	// everything above ASCII.
	src.channels[1].name = "Ka\xc3\xa4se \xe2\x82\xac";
	InstalledChannelSource installed(&src);

	const Reply r = get("/api/v1/channels?mode=tv&limit=5");
	REQUIRE(r.code == 200);
	REQUIRE(isUtf8(r.body));
	REQUIRE(r.body.find((char) 0xdf) == std::string::npos);

	const ::Json::Value items = parsed(r.body)["items"];
	REQUIRE(items[0]["name"].asString() == "Pro\xef\xbf\xbdSieben");
	REQUIRE(items[1]["name"].asString() == "Ka\xc3\xa4se \xe2\x82\xac");
}

TEST_CASE("a box that cannot be read is a refusal and not an answer full of nothing", "[endpoints]")
{
	ShippedRoutes shipped;
	FakeSystemSource src;
	src.box_status = coreapi::Status::Internal;
	InstalledSystemSource installed(&src);

	const Reply r = get("/api/v1/system/info");
	REQUIRE(r.code == 500);
	REQUIRE(r.content_type == "application/problem+json");
}

namespace
{

/* What is in effect put back on the way out as well as set on the way in,
   because a case that failed unwinds past its own restore and the next one
   would then be reading a policy this one left behind. Every case below sets
   one, so the constructor takes it rather than leaving the last one standing. */
struct HeldWebConfig
{
	WebConfig before;

	explicit HeldWebConfig(const WebConfig &c) : before(config())
	{
		setConfigForTest(c);
	}

	~HeldWebConfig()
	{
		stop();
		setConfigForTest(before);
	}

	private:
		HeldWebConfig(const HeldWebConfig &);
		HeldWebConfig &operator=(const HeldWebConfig &);
};

} // namespace

TEST_CASE("the server says the port it is really answering on", "[endpoints]")
{
	/* A configuration naming no port, which is the one state in which what the
	   file asked for and what a caller can reach are two different numbers: the
	   kernel chose, and nothing but the running daemon knows what it chose. A
	   route answering the configured number answers nought here, which is not a
	   port anybody can reach this box on. */
	ShippedRoutes shipped;
	InstalledDependencies wired;

	WebConfig c = defaultWebConfig();
	c.server.port = 0;
	c.server.bind_address = "127.0.0.1";
	HeldWebConfig held(c);

	REQUIRE(start(c.server));
	const int bound = boundPort();
	REQUIRE(bound > 0);

	const Reply r = get("/api/v1/system/webserver", AuthLevel::System);
	REQUIRE(r.code == 200);
	REQUIRE(parsed(r.body)["port"].asInt() == bound);

	const Endpoint *ep = routeFor("/api/v1/system/webserver");
	REQUIRE(ep != NULL);
	REQUIRE(ep->schema != NULL);
	checkShape(parsed(r.body), *ep->schema, "/api/v1/system/webserver");
}

TEST_CASE("the password does not leave the box, in either of the forms it has", "[endpoints]")
{
	/* A stored form is what an offline attack is mounted against, and an answer
	   carrying one supplies the material for an attack that cannot happen
	   without it. What the answer says is that there is one. */
	ShippedRoutes shipped;

	WebConfig c = defaultWebConfig();
	c.username = "somebody";
	c.password_hash = hashSecret("a password nobody guesses", 2000);
	REQUIRE_FALSE(c.password_hash.empty());
	HeldWebConfig held(c);

	const Reply r = get("/api/v1/system/webserver", AuthLevel::System);
	REQUIRE(r.code == 200);

	const ::Json::Value root = parsed(r.body);
	REQUIRE(root["user"].asString() == "somebody");
	REQUIRE(root["has_password"].asBool());
	REQUIRE(r.body.find(c.password_hash) == std::string::npos);
	REQUIRE(r.body.find("a password nobody guesses") == std::string::npos);
	REQUIRE_FALSE(hasField(r.body, "password"));
	REQUIRE_FALSE(hasField(r.body, "password_hash"));
}

TEST_CASE("a box with no password says so rather than looking configured", "[endpoints]")
{
	ShippedRoutes shipped;

	WebConfig c = defaultWebConfig();
	c.password_hash.clear();
	HeldWebConfig held(c);

	const Reply r = get("/api/v1/system/webserver", AuthLevel::System);
	REQUIRE(r.code == 200);
	REQUIRE_FALSE(parsed(r.body)["has_password"].asBool());
}

TEST_CASE("the networks this box is defended by are not read off an address alone", "[endpoints]")
{
	/* The neighbours under this prefix answer a caller granted a read by where
	   it is asking from. This one carries the list of networks that granted it,
	   which is the shape of the defence rather than a fact about the box, so it
	   asks for the level the four calls that stop the box ask for. */
	ShippedRoutes shipped;

	WebConfig c = defaultWebConfig();
	HeldWebConfig held(c);

	const Reply refused = get("/api/v1/system/webserver", AuthLevel::Read);
	REQUIRE(refused.code == 403);
	REQUIRE(refused.body.find("lan_read") == std::string::npos);

	const Reply allowed = get("/api/v1/system/webserver", AuthLevel::System);
	REQUIRE(allowed.code == 200);
}

TEST_CASE("the networks are answered as this box compares them", "[endpoints]")
{
	/* A file may write a host inside a network and mean the network, and what
	   decides anything here is what was parsed. An answer echoing the line back
	   would show a caller something the box never compares against. */
	ShippedRoutes shipped;

	WebConfig c = defaultWebConfig();
	c.lan_read.clear();
	NetPrefix one;
	REQUIRE(parsePrefix("10.1.2.3/8", &one));
	c.lan_read.push_back(one);
	c.trusted_proxies.clear();
	HeldWebConfig held(c);

	const Reply r = get("/api/v1/system/webserver", AuthLevel::System);
	REQUIRE(r.code == 200);

	const ::Json::Value root = parsed(r.body);
	REQUIRE(root["lan_read"].size() == 1u);
	REQUIRE(root["lan_read"][0].asString() == "10.0.0.0/8");
	REQUIRE(root["trusted_proxies"].isArray());
	REQUIRE(root["trusted_proxies"].size() == 0u);
}

TEST_CASE("the switch the copied surface is answered under is readable", "[endpoints]")
{
	/* Read here and written nowhere, which is the whole of what this member is
	   for: the count beside it says whether anything still calls that surface,
	   and this says whether the box is still answering it. A box that had to be
	   rebuilt to find out would be a box nobody asks. */
	ShippedRoutes shipped;

	WebConfig c = defaultWebConfig();
	c.legacy_enabled = false;
	HeldWebConfig held(c);

	const Reply off = get("/api/v1/system/webserver", AuthLevel::System);
	REQUIRE(off.code == 200);
	REQUIRE_FALSE(parsed(off.body)["legacy_enabled"].asBool());

	c.legacy_enabled = true;
	setConfigForTest(c);
	const Reply on = get("/api/v1/system/webserver", AuthLevel::System);
	REQUIRE(on.code == 200);
	REQUIRE(parsed(on.body)["legacy_enabled"].asBool());
}

TEST_CASE("the route that lists a directory asks what the filesystem is under", "[endpoints]")
{
	/* The level table puts the filesystem under System, and the two routes that change
	   it are System already. The one that reads it was a level below them, so a caller
	   granted a read by its address alone could list the directory this server's own
	   configuration lives in, with its names, modes and owners. Read off the table and
	   driven through the gate, because a level in a table is worth what the gate does
	   with it and nothing more. */
	ShippedRoutes shipped;

	const Endpoint *ep = routeFor("/api/v1/storage/files");
	REQUIRE(ep != NULL);
	REQUIRE(ep->auth == AuthLevel::System);

	const Response refused = dispatch(Get, "/api/v1/storage/files", "path=/tmp", "",
	                                  "127.0.0.1", AuthLevel::Read);
	REQUIRE(refused.code == 403);
}

TEST_CASE("every route these modules read with asks for a read at least", "[endpoints]")
{
	/* The tables these modules state, walked rather than a list typed here, so a route
	   added at the wrong level fails this instead of shipping.

	   At least a read, and not exactly one. What most of these answer is the box
	   describing itself. One of them is not: the listing of a directory reaches the
	   filesystem, which the level table puts under System. What this case refuses is the
	   other direction. */
	ShippedRoutes shipped;

	const RouteTable *const mine[] = {
		&channelsTable, &epgTable, &settingsTable, &systemTable, &storageTable
	};

	size_t read_routes = 0;
	for (size_t i = 0; i < sizeof(mine) / sizeof(mine[0]); ++i)
	{
		for (size_t j = 0; j < mine[i]->count; ++j)
		{
			const Endpoint &ep = mine[i]->endpoints[j];
			INFO(ep.path);
			REQUIRE(std::string(ep.path).compare(0, 8, "/api/v1/") == 0);
			// A route here that changes something would be a write and would
			// have to say so; what this holds is that a read stays a read.
			if (ep.method != Get)
				continue;
			// The order of the members is the order of privilege, which is
			// stated where they are declared.
			REQUIRE((int) ep.auth >= (int) AuthLevel::Read);
			++read_routes;
		}
	}

	REQUIRE(read_routes == 24);
}

TEST_CASE("no route this server ships is reachable without a credential", "[endpoints]")
{
	/* Over every table, which is the rule that has to hold however many modules there
	   come to be. The posture this project exists to remove is everything being reachable
	   without a credential.

	   Two routes are open to everybody and neither hands a caller anything it did not
	   already have. Each is excused here by its path and its method together, and the
	   count below is what keeps those excuses from quietly covering a third route.

	   The pair is written out here rather than read off the server, so a route made Public
	   somewhere else fails this rather than being described by it. */
	ShippedRoutes shipped;

	size_t tables = 0;
	const RouteTable *const *t = allRoutes(&tables);
	size_t seen = 0;
	size_t open_to_everybody = 0;
	for (size_t i = 0; i < tables; ++i)
	{
		for (size_t j = 0; j < t[i]->count; ++j)
		{
			const Endpoint &ep = t[i]->endpoints[j];
			INFO(ep.path);
			if (ep.auth == AuthLevel::Public)
			{
				const std::string what =
					std::string(methodName(ep.method)) + " " + ep.path;
				INFO(what);
				REQUIRE((what == "POST /api/v1/login" ||
				         what == "GET /api/v1/session"));
				++open_to_everybody;
			}
			++seen;
		}
	}
	REQUIRE(seen >= 15);
	REQUIRE(open_to_everybody == 2);
}

TEST_CASE("every declared route table is sane", "[endpoints]")
{
	ShippedRoutes shipped;

	size_t tables = 0;
	const RouteTable *const *t = allRoutes(&tables);
	// More than the one this list held before, or the walk below is a walk of
	// a list that stopped being built.
	REQUIRE(tables >= 6);
	for (size_t i = 0; i < tables; ++i)
	{
		std::string why;
		INFO(why);
		REQUIRE(tableIsSane(*t[i], &why));
	}

	/* And all of them together, which is the check a table read alone cannot
	   make: two modules can carry one answer between them and neither of them
	   is wrong on its own. */
	std::string why;
	const bool sane = tablesAreSane(t, tables, &why);
	INFO(why);
	REQUIRE(sane);
}

TEST_CASE("every route that declares a shape declares a sane one", "[endpoints]")
{
	ShippedRoutes shipped;

	size_t tables = 0;
	const RouteTable *const *t = allRoutes(&tables);
	size_t described = 0;
	for (size_t i = 0; i < tables; ++i)
	{
		for (size_t j = 0; j < t[i]->count; ++j)
		{
			const Endpoint &ep = t[i]->endpoints[j];
			if (ep.schema == NULL)
				continue;
			const char *why = "";
			INFO(ep.path << ": " << why);
			REQUIRE(schemaIsSane(*ep.schema, &why));
			++described;
		}
	}
	// The routes this task declares, so a build where the shapes stopped being
	// declared is not a build where this passes over nothing.
	REQUIRE(described >= 14);
}

TEST_CASE("every answer carries the members its route says it does", "[endpoints]")
{
	ShippedRoutes shipped;

	/* Without this every label and hint in the settings schema answer is absent,
	   nothing installed here answering NotFound for every name, and the walk below
	   would REQUIRE nothing about a member it never finds present. Filled from the
	   schema this build actually ships, so a case order that put this after some other
	   install cannot read as passing for the wrong reason. */
	FakeLocaleSource locale;
	fillLocaleFromSchema(locale);
	InstalledLocaleSource installed_locale(&locale);

	FakeChannelSource channels;
	fillChannels(channels);
	channels.current = channels.channels[0];
	channels.current_status = coreapi::Status::Ok;
	InstalledChannelSource installed_channels(&channels);

	FakeEpgSource events;
	coreapi::EventInfo e;
	e.event_id = 0x31;
	e.channel_id = kFirstTv;
	e.title = "the news";
	e.description = "what happened";
	e.start = 100;
	e.duration = 1000;
	events.events.push_back(e);
	events.now = 500;
	// The one read that answers about a single event reads its own record, so
	// the route that carries the age, the class and the long text has one to
	// answer from here.
	coreapi::EventDetail d;
	d.event_id = 0x31;
	d.channel_id = kFirstTv;
	d.title = "the news";
	d.description = "what happened";
	d.long_description = "what happened, at greater length";
	d.start = 100;
	d.duration = 1000;
	d.rating = 6;
	d.genre = 0x21;
	events.details.push_back(d);
	InstalledEpgSource installed_events(&events);

	FakeSystemSource box;
	InstalledSystemSource installed_box(&box);

	FakeSettingsSource store;
	InstalledSettingsSource installed_store(&store);

	StorageFixture storage;
	ProcFixture proc;

	const int fd = open(storage.box.at("a-file").c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0600);
	REQUIRE(fd >= 0);
	close(fd);

	struct Case
	{
		const char *route;
		std::string target;
	};

	const std::string channel = hex(kFirstTv);
	const Case cases[] = {
		{ "/api/v1/channels",          "/api/v1/channels?mode=tv&limit=3" },
		{ "/api/v1/channels/{id}",     "/api/v1/channels/" + channel },
		{ "/api/v1/channels/current",  "/api/v1/channels/current" },
		{ "/api/v1/bouquets",          "/api/v1/bouquets" },
		{ "/api/v1/epg",               "/api/v1/epg?channel=" + channel + "&from=0&to=100000" },
		{ "/api/v1/epg/current",       "/api/v1/epg/current?channel=" + channel },
		{ "/api/v1/epg/search",        "/api/v1/epg/search?q=news&from=0&to=100000" },
		{ "/api/v1/epg/grid",          "/api/v1/epg/grid?channels=" + channel + "&from=0&to=100000" },
		{ "/api/v1/epg/event",         "/api/v1/epg/event?id=" + hex(0x31) + "&start=100" },
		{ "/api/v1/settings/schema",   "/api/v1/settings/schema" },
		{ "/api/v1/settings/sections", "/api/v1/settings/sections" },
		{ "/api/v1/settings/{section}","/api/v1/settings/network" },
		{ "/api/v1/system/info",       "/api/v1/system/info" },
		{ "/api/v1/storage/mounts",    "/api/v1/storage/mounts" },
		{ "/api/v1/storage/files",     "/api/v1/storage/files?path=" + storage.box.dir },
		{ "/api/v1/daemons",           "/api/v1/daemons" },
		{ "/api/v1/system/decryption", "/api/v1/system/decryption" },
	};

	const size_t count = sizeof(cases) / sizeof(cases[0]);
	// Every route this task declares, so a route left out of this list is one
	// nothing here holds to its shape.
	REQUIRE(count == 17);

	for (size_t i = 0; i < count; ++i)
	{
		INFO(cases[i].target);
		const Endpoint *ep = routeFor(cases[i].route);
		REQUIRE(ep != NULL);
		REQUIRE(ep->schema != NULL);

		// At the level the route itself declares, so that a route moved to a
		// level of its own is still read here rather than answered 403.
		const Reply r = get(cases[i].target, ep->auth);
		REQUIRE(r.code == 200);
		REQUIRE(r.content_type == "application/json");

		::Json::Value root;
		REQUIRE(parseDocument(r.body, root));
		checkShape(root, *ep->schema, cases[i].route);
	}
}

TEST_CASE("an identifier reads the same here as it does on the event stream", "[endpoints]")
{
	/* Two places write a channel identifier into a document and a client reads
	   both, so a spelling that drifted in one of them would leave a caller
	   unable to match a channel it had listed against an event about it. */
	ShippedRoutes shipped;
	FakeChannelSource src;
	fillChannels(src);
	InstalledChannelSource installed(&src);

	coreapi::Event e;
	e.type = coreapi::EventType::Zap;
	e.channel_id = kFirstTv + 3;
	e.value = 0;
	std::string frame;
	events::appendEventJson(frame, e);

	const Reply r = get("/api/v1/channels/" + hex(kFirstTv + 3));
	REQUIRE(r.code == 200);

	const std::string listed = parsed(r.body)["id"].asString();
	REQUIRE(parsed(frame)["channel_id"].asString() == listed);
}

/* The route that tells a page what its own request carries. A session is opened here
   rather than through the login route, because what these are about is what the answer
   says and not how the session was come by. */
namespace
{

const char kSessionRoute[] = "/api/v1/session";

Reply sessionAs(const std::string &token, AuthLevel as)
{
	const Response r = dispatch(Get, kSessionRoute, "", "", "127.0.0.1", as,
	                            std::string(), token);
	Reply out;
	out.code = r.code;
	out.content_type = r.content_type;
	out.body = r.body;
	return out;
}

/* Opened and closed by the case that opened it, because the table is process
   wide and a session left behind outlives the case. Closed from a destructor,
   so a check that fails and unwinds past a last line does not leave one. */
struct HeldSession
{
	std::string token;

	explicit HeldSession(const char *user) : token(openSession(user)) {}
	~HeldSession() { (void) closeSession(token); }

	private:
		HeldSession(const HeldSession &);
		HeldSession &operator=(const HeldSession &);
};

} // namespace

TEST_CASE("the route that says what a request carries answers about that request", "[endpoints]")
{
	ShippedRoutes shipped;

	const Endpoint *ep = routeFor(kSessionRoute);
	REQUIRE(ep != NULL);
	REQUIRE(ep->schema != NULL);
	// Reachable with nothing presented, because a page that has lost its
	// session has to be able to learn that it has. A refusal would say the same
	// thing in a shape a page cannot read.
	REQUIRE(ep->auth == AuthLevel::Public);

	HeldSession held("root");
	REQUIRE_FALSE(held.token.empty());

	const Reply r = sessionAs(held.token, AuthLevel::System);
	REQUIRE(r.code == 200);
	REQUIRE(r.content_type == "application/json");

	::Json::Value root;
	REQUIRE(parseDocument(r.body, root));
	checkShape(root, *ep->schema, kSessionRoute);

	REQUIRE(root["authenticated"].asBool() == true);
	REQUIRE(root["level"].asString() == "system");
	REQUIRE(root["user"].asString() == "root");
	/* The same second token the session holds, which is the whole of what a
	   page that was reloaded is missing: the cookie survived and what the login
	   route put in the body did not. */
	REQUIRE(root["csrf"].asString() == csrfFor(held.token));
	REQUIRE_FALSE(root["csrf"].asString().empty());
	// And the header it goes back in, so a page does not carry that name
	// written out in a second place.
	REQUIRE(root["csrf_header"].asString() == std::string(csrfHeaderName()));

	/* Counted down rather than handed out as a moment, and inside what the
	   configuration allows a session, so a field that answered the moment it
	   runs out or the moment it started would fail here rather than read as a
	   plausible number. */
	const long left = (long) root["expires_in"].asInt64();
	REQUIRE(left > 0);
	REQUIRE(left <= (long) config().session_lifetime_s);
}

TEST_CASE("the answer never carries the token the browser holds", "[endpoints]")
{
	/* The cookie is what a session is, and the answer is read by a script on
	   the page. Putting the cookie's own token in it would undo what HttpOnly
	   buys, which is that no script anywhere near the browser can carry a
	   session off. The second token is not that: it is refused without the
	   cookie, so on its own it is nothing. */
	ShippedRoutes shipped;

	HeldSession held("root");
	REQUIRE_FALSE(held.token.empty());

	const Reply r = sessionAs(held.token, AuthLevel::System);
	REQUIRE(r.code == 200);
	REQUIRE(r.body.find(held.token) == std::string::npos);
}

TEST_CASE("a request carrying no session is told that rather than refused", "[endpoints]")
{
	ShippedRoutes shipped;

	const Endpoint *ep = routeFor(kSessionRoute);
	REQUIRE(ep != NULL);

	// What a browser that has never logged in sends, and what one still holding
	// a cookie from a session that is gone sends. Both carry no session, and
	// the answer is the same for either.
	const std::string gone(64, 'a');
	const char *const tokens[] = { "", gone.c_str() };
	for (size_t i = 0; i < sizeof(tokens) / sizeof(tokens[0]); ++i)
	{
		INFO(i);
		const Reply r = sessionAs(tokens[i], AuthLevel::Public);
		REQUIRE(r.code == 200);

		::Json::Value root;
		REQUIRE(parseDocument(r.body, root));
		checkShape(root, *ep->schema, kSessionRoute);

		REQUIRE(root["authenticated"].asBool() == false);
		REQUIRE(root["user"].asString().empty());
		REQUIRE(root["csrf"].asString().empty());
		REQUIRE(root["expires_in"].asInt64() == 0);
		REQUIRE(root["level"].asString() == "public");

		/* The one member that is answered here as it is answered to a caller
		   holding a session. The name of the header belongs to this server and
		   not to any session, and a caller that is about to log in needs it
		   before it has a session for anything else in this answer to describe.
		   The value that travels in it is the opposite and is empty above. */
		REQUIRE(root["csrf_header"].asString() == std::string(csrfHeaderName()));
	}
}

TEST_CASE("the level in the answer is the name the document uses for the same level", "[endpoints]")
{
	/* Written out here rather than taken from the server, so the four names are
	   compared against a second statement of them and not against the one that wrote
	   them. The document that describes these routes says a level under the same four
	   names, which is why they are one function in the server and two statements
	   outside it. */
	ShippedRoutes shipped;

	struct Named
	{
		AuthLevel   level;
		const char *name;
	};
	const Named named[] = {
		{ AuthLevel::Public, "public" },
		{ AuthLevel::Read,   "read"   },
		{ AuthLevel::Write,  "write"  },
		{ AuthLevel::System, "system" },
	};

	for (size_t i = 0; i < sizeof(named) / sizeof(named[0]); ++i)
	{
		INFO(named[i].name);
		REQUIRE(std::string(authLevelName(named[i].level)) == named[i].name);

		const Reply r = sessionAs("", named[i].level);
		REQUIRE(r.code == 200);
		REQUIRE(stringField(r.body, "level") == named[i].name);
	}
}

/* The two files the box keeps its network filesystems in, read as entries.

   Both paths are process wide and are put back from a destructor, because a
   check that fails unwinds past a last line and the case after this one would
   otherwise be reading whatever this one pointed at. */
namespace
{

struct NetfsFixture
{
	Sandbox     box;
	std::string before_fstab;
	std::string before_auto;

	NetfsFixture()
		: before_fstab(coreapi::netfs::pathOf(coreapi::netfs::Table::Fstab)),
		  before_auto(coreapi::netfs::pathOf(coreapi::netfs::Table::Automount))
	{
		coreapi::netfs::setPath(coreapi::netfs::Table::Fstab, box.at("fstab"));
		coreapi::netfs::setPath(coreapi::netfs::Table::Automount, box.at("auto.net"));
	}

	~NetfsFixture()
	{
		coreapi::netfs::setPath(coreapi::netfs::Table::Fstab, before_fstab);
		coreapi::netfs::setPath(coreapi::netfs::Table::Automount, before_auto);
	}

	void put(const char *name, const std::string &text) const
	{
		const int fd = open(box.at(name).c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0600);
		REQUIRE(fd >= 0);
		REQUIRE(write(fd, text.data(), text.size()) == (ssize_t) text.size());
		close(fd);
	}

	private:
		NetfsFixture(const NetfsFixture &);
		NetfsFixture &operator=(const NetfsFixture &);
};

} // namespace

TEST_CASE("the entries of the mounted at start up file are read as fields", "[endpoints][netfs]")
{
	ShippedRoutes shipped;
	NetfsFixture netfs;

	netfs.put("fstab",
		  "# /var/etc/fstab: static file system information.\n"
		  "#\n"
		  "# <file system> <mount point> <type> <options> <dump> <pass>\n"
		  "192.168.0.10:/nas/video\t/srv\t\tnfs\trw,soft,nolock\t\t0\t0\n"
		  "//192.168.1.99/folder\t/mnt/folder\t\tcifs\tnoauto,username=uli,password=geheim,soft"
		  "\t\t0\t0\n");

	const Reply r = get("/api/v1/storage/netfs/fstab", AuthLevel::System);
	REQUIRE(r.code == 200);

	const ::Json::Value doc = parsed(r.body);
	REQUIRE(doc["table"].asString() == "fstab");
	// Always the eight the file holds, the empty ones among them, because a
	// write addresses a slot and an empty slot is one to fill in.
	REQUIRE(doc["items"].size() == 8);
	REQUIRE(doc["unreadable_lines"].asUInt() == 0);

	const ::Json::Value first = doc["items"][0];
	REQUIRE(first["slot"].asUInt() == 0);
	REQUIRE(first["active"].asBool());
	REQUIRE(first["type"].asString() == "nfs");
	REQUIRE(first["host"].asString() == "192.168.0.10");
	REQUIRE(first["remote_dir"].asString() == "/nas/video");
	REQUIRE(first["local_dir"].asString() == "/srv");
	REQUIRE(first["user"].asString() == "");
	REQUIRE_FALSE(first["has_password"].asBool());
	REQUIRE(first["options"].asString() == "rw,soft,nolock");

	const ::Json::Value second = doc["items"][1];
	// The word that says a mount is not automatic is a field of its own here
	// and is out of the options it was read from.
	REQUIRE_FALSE(second["active"].asBool());
	REQUIRE(second["type"].asString() == "cifs");
	REQUIRE(second["host"].asString() == "192.168.1.99");
	REQUIRE(second["remote_dir"].asString() == "/folder");
	REQUIRE(second["local_dir"].asString() == "/mnt/folder");
	REQUIRE(second["user"].asString() == "uli");
	REQUIRE(second["options"].asString() == "soft");

	// A slot nothing filled in is empty and is still there.
	REQUIRE(doc["items"][7]["host"].asString() == "");
	REQUIRE_FALSE(doc["items"][7]["active"].asBool());
}

TEST_CASE("no password of a network filesystem leaves the box through a read", "[endpoints][netfs]")
{
	/* The file holds it in clear, because the kernel takes one no other way, so
	   the value is in reach of the layer that reads the file and the read must
	   answer with nothing but whether there is one. */
	ShippedRoutes shipped;
	NetfsFixture netfs;

	netfs.put("fstab",
		  "//192.168.1.99/folder\t/mnt/folder\t\tcifs\tusername=uli,password=geheim\t\t0\t0\n");

	const Reply r = get("/api/v1/storage/netfs/fstab", AuthLevel::System);
	REQUIRE(r.code == 200);
	REQUIRE(r.body.find("geheim") == std::string::npos);
	// No member of that name either, only the one that says whether there is
	// one, so a reader walking the answer finds nowhere a value could sit.
	REQUIRE(r.body.find("\"password\"") == std::string::npos);

	const ::Json::Value first = parsed(r.body)["items"][0];
	REQUIRE(first["has_password"].asBool());
	// The name is not the secret and is answered, so a page can show who the
	// mount logs in as without being handed what it logs in with.
	REQUIRE(first["user"].asString() == "uli");

	/* And the taking out happens a floor below the route, which is what keeps
	   the next route written over this file from having to remember it. Asked
	   of that floor directly, because the route above answers no member the
	   value could sit in and so says nothing about whether it is still there. */
	coreapi::Result<coreapi::netfs::Slots> below =
		coreapi::netfs::read(coreapi::netfs::Table::Fstab);
	REQUIRE(below.ok());
	REQUIRE(below.value().items[0].has_password);
	REQUIRE(below.value().items[0].password.empty());
}

TEST_CASE("a line nothing can be made of does not cost the entries beside it", "[endpoints][netfs]")
{
	ShippedRoutes shipped;
	NetfsFixture netfs;

	netfs.put("fstab",
		  "192.168.0.10:/first\t/srv\t\tnfs\tsoft\t\t0\t0\n"
		  "this-line-is-nonsense\n"
		  "no-colon-anywhere\t/mnt/two\t\tnfs\tsoft\t\t0\t0\n"
		  "192.168.0.10:/third\t/mnt/three\t\tnfs\tsoft\t\t0\t0\n");

	const Reply r = get("/api/v1/storage/netfs/fstab", AuthLevel::System);
	REQUIRE(r.code == 200);

	const ::Json::Value doc = parsed(r.body);
	// Two lines were meant to be entries and could not be read, and both of the
	// entries that could be are here.
	REQUIRE(doc["unreadable_lines"].asUInt() == 2);
	REQUIRE(doc["items"][0]["remote_dir"].asString() == "/first");
	REQUIRE(doc["items"][1]["remote_dir"].asString() == "/third");
}

TEST_CASE("the automounter file is read out of the shape that file has", "[endpoints][netfs]")
{
	/* A different file with different columns and a different way of saying an
	   entry is switched off, answered as the same eight fields. */
	ShippedRoutes shipped;
	NetfsFixture netfs;

	netfs.put("auto.net",
		  "# /var/etc/auto.net: dynamic file system information.\n"
		  "#\n"
		  "# <mount point> -fstype=<type>,<options> <file system>\n"
		  "srv\t-fstype=nfs,soft,nolock\t\t192.168.0.10:/nas/video\n"
		  "#pictures\t-fstype=cifs,user=uli,password=geheim\t\t://192.168.0.11/pictures\n");

	const Reply r = get("/api/v1/storage/netfs/automount", AuthLevel::System);
	REQUIRE(r.code == 200);
	REQUIRE(r.body.find("geheim") == std::string::npos);

	const ::Json::Value doc = parsed(r.body);
	REQUIRE(doc["table"].asString() == "automount");
	// The three comment lines are comments and not lines that failed.
	REQUIRE(doc["unreadable_lines"].asUInt() == 0);

	const ::Json::Value first = doc["items"][0];
	REQUIRE(first["active"].asBool());
	REQUIRE(first["local_dir"].asString() == "srv");
	REQUIRE(first["host"].asString() == "192.168.0.10");
	REQUIRE(first["remote_dir"].asString() == "/nas/video");
	REQUIRE(first["options"].asString() == "soft,nolock");

	const ::Json::Value second = doc["items"][1];
	// Commented out is how that file switches an entry off, and the line is
	// still an entry rather than a comment.
	REQUIRE_FALSE(second["active"].asBool());
	REQUIRE(second["type"].asString() == "cifs");
	// The screen at the box writes a stray pair of characters in front of the
	// server there, and a file it wrote has to read back here all the same.
	REQUIRE(second["host"].asString() == "192.168.0.11");
	REQUIRE(second["remote_dir"].asString() == "/pictures");
	REQUIRE(second["user"].asString() == "uli");
	REQUIRE(second["has_password"].asBool());
}

TEST_CASE("a box with no such file has eight empty slots and not a refusal", "[endpoints][netfs]")
{
	ShippedRoutes shipped;
	NetfsFixture netfs;

	const Reply r = get("/api/v1/storage/netfs/automount", AuthLevel::System);
	REQUIRE(r.code == 200);
	REQUIRE(parsed(r.body)["items"].size() == 8);
	REQUIRE(parsed(r.body)["unreadable_lines"].asUInt() == 0);
}

TEST_CASE("reading the network filesystems needs more than the network grants", "[endpoints][netfs]")
{
	/* What it answers is the address of somebody's storage on their own
	   network, the shares on it and the name they log in under, which is not
	   what the exemption that grants a read by address alone is for. */
	ShippedRoutes shipped;
	NetfsFixture netfs;

	const Endpoint *ep = routeFor("/api/v1/storage/netfs/{table}");
	REQUIRE(ep != NULL);
	REQUIRE(ep->auth == AuthLevel::System);

	REQUIRE(get("/api/v1/storage/netfs/fstab", AuthLevel::Read).code == 403);
	REQUIRE(get("/api/v1/storage/netfs/fstab", AuthLevel::Write).code == 403);
}

TEST_CASE("a table this server has no file for is refused as a name", "[endpoints][netfs]")
{
	ShippedRoutes shipped;
	NetfsFixture netfs;

	const Reply r = get("/api/v1/storage/netfs/passwd", AuthLevel::System);
	REQUIRE(r.code == 400);
}
