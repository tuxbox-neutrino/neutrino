/*
 * test_endpoints_write.cpp - tests for every writing route
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

#include "httpd/auth.h"
#include "httpd/webconfig.h"
#include "httpd/credentials.h"
#include "httpd/endpoint.h"
#include "httpd/endpoints.h"
#include "httpd/http.h"
#include "httpd/json.h"
#include "httpd/doc/openapi.h"
#include "httpd/router.h"
#include "httpd/schema.h"
#include "httpd/server.h"
#include "httpd/status.h"

#include "coreapi/daemons.h"
#include "coreapi/base/errors.h"
#include "coreapi/netfs.h"
#include "coreapi/osd.h"
#include "coreapi/base/schema.h"
#include "coreapi/settings/settings.h"
#include "coreapi/storage.h"
#include "coreapi/box/storage_internal.h"
#include "coreapi/base/types.h"

#include "jsoncpp/json/json.h"

#include <cstdio>
#include <cstdlib>
#include <memory>
#include <string>
#include <vector>

#include <fcntl.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

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

struct Reply
{
	int                                              code;
	std::string                                      content_type;
	std::string                                      body;
	std::vector<std::pair<std::string, std::string> > headers;

	Reply() : code(0) {}
};

Reply replyOf(const Response &r)
{
	Reply out;
	out.code = r.code;
	out.content_type = r.content_type;
	out.body = r.body;
	out.headers = r.headers;
	return out;
}

// The value of a header the answer carries, and empty for one it does not, so
// that a header absent and a header empty cannot be told apart by accident: no
// header this server writes is ever empty.
std::string headerOf(const Reply &r, const char *name)
{
	for (size_t i = 0; i < r.headers.size(); ++i)
	{
		if (r.headers[i].first == name)
			return r.headers[i].second;
	}
	return std::string();
}

/* One request, split where a transport splits it and with the level already
   resolved, which is what the router is handed. The level is a parameter of every
   one of these because that is the whole of what half of these cases are about: the
   same request from a caller the network exemption granted a read and from a caller
   holding the box's credential must not come back the same. */
Reply send(Method m, const std::string &target, const std::string &body,
           AuthLevel granted, const std::string &peer)
{
	std::string path = target;
	std::string query;
	const size_t q = target.find('?');
	if (q != std::string::npos)
	{
		path = target.substr(0, q);
		query = target.substr(q + 1);
	}
	return replyOf(dispatch(m, path, query, body, peer, granted));
}

Reply authedPost(const std::string &t, const std::string &b)
{
	return send(Post, t, b, AuthLevel::System, "127.0.0.1");
}

Reply authedPut(const std::string &t, const std::string &b)
{
	return send(Put, t, b, AuthLevel::System, "127.0.0.1");
}

Reply authedPatch(const std::string &t, const std::string &b)
{
	return send(Patch, t, b, AuthLevel::System, "127.0.0.1");
}

Reply authedDelete(const std::string &t)
{
	return send(Delete, t, "", AuthLevel::System, "127.0.0.1");
}

Reply authedGet(const std::string &t)
{
	return send(Get, t, "", AuthLevel::System, "127.0.0.1");
}

// What a caller on the box's own network is granted without presenting
// anything, which is a read and nothing above it.
Reply lanPost(const std::string &t, const std::string &b)
{
	return send(Post, t, b, AuthLevel::Read, "192.168.1.9");
}

Reply lanPatch(const std::string &t, const std::string &b)
{
	return send(Patch, t, b, AuthLevel::Read, "192.168.1.9");
}

Reply lanPut(const std::string &t, const std::string &b)
{
	return send(Put, t, b, AuthLevel::Read, "192.168.1.9");
}

Reply lanDelete(const std::string &t)
{
	return send(Delete, t, "", AuthLevel::Read, "192.168.1.9");
}

// The tree's own parser as a second opinion: a body only this server can read
// is not a document anybody else can use.
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

std::string stringField(const std::string &body, const char *name)
{
	const ::Json::Value root = parsed(body);
	REQUIRE(root.isMember(name));
	return root[name].asString();
}

/* The configuration is process wide and is put back from a destructor, because a
   check that fails unwinds past a last line. The password is hashed rather than
   written down, so what the login route goes through here is the derivation it goes
   through on the box. */
const char kUser[] = "root";
const char kPassword[] = "ni";

// The address the login cases are answered from, in the range set aside for
// documentation so it is nobody's network.
const char kPeer[] = "203.0.113.7";

struct ConfigFixture
{
	WebConfig before;

	ConfigFixture() : before(config())
	{
		/* The gate in front of the login route is process wide and counts wrong
		   answers per address, so a case that guessed wrong would otherwise
		   decide what the case after it is answered with. Cleared going in as
		   well as coming out, because a case that failed unwinds past its own
		   restore. */
		forgetLoginAttemptsForTest();

		WebConfig c = defaultWebConfig();
		c.username = kUser;
		/* Well under what the box is configured with, because every case here pays
		   for it and one of them pays a hundred times: what is being exercised is
		   that both answers pay the same. High enough that the three intervals the
		   timing case measures are milliseconds rather than microseconds, so the
		   scheduler is not what the comparison is reading. */
		c.password_hash = hashSecret(kPassword, 2000);
		setConfigForTest(c);
	}

	~ConfigFixture()
	{
		forgetLoginAttemptsForTest();
		setLoginClockForTest(0);
		setConfigForTest(before);
	}

	private:
		ConfigFixture(const ConfigFixture &);
		ConfigFixture &operator=(const ConfigFixture &);
};

std::string loginBody(const char *user, const char *password)
{
	std::string out = "{\"user\":\"";
	out += user;
	out += "\",\"password\":\"";
	out += password;
	out += "\"}";
	return out;
}

Reply login(const char *user, const char *password)
{
	// Public, which is what a caller presenting nothing is, so this is the one
	// route these cases reach without a level.
	return send(Post, "/api/v1/login", loginBody(user, password), AuthLevel::Public, kPeer);
}

// The session an answer handed out, read off the cookie it set, because that is
// where a session travels and nothing in the body names it.
std::string tokenOf(const Reply &r)
{
	const std::string c = headerOf(r, "Set-Cookie");
	const size_t eq = c.find('=');
	const size_t end = c.find(';');
	REQUIRE(eq != std::string::npos);
	REQUIRE(end > eq);
	return c.substr(eq + 1, end - eq - 1);
}

/* Seconds this thread spent on a processor, not seconds of wall clock. Both cases below
   compare one answer's cost against another's, and what makes one cost more is work the
   box did: a derivation run or not run. Wall clock also carries the time this thread was
   ready and not running, which on a loaded machine is larger than the difference being
   measured. A request here is dispatched in this thread and returns in it.

   Sleeping is the one thing this cannot see, and no answer here sleeps. */
double cpuSecondsOf(void (*run)())
{
	struct timespec a;
	struct timespec b;
	REQUIRE(clock_gettime(CLOCK_THREAD_CPUTIME_ID, &a) == 0);
	run();
	REQUIRE(clock_gettime(CLOCK_THREAD_CPUTIME_ID, &b) == 0);
	return (double)(b.tv_sec - a.tv_sec) + (double)(b.tv_nsec - a.tv_nsec) / 1e9;
}

const int kTimingRounds = 32;

/* Every round starts with the gate cleared, in all three of these and not only
   in the two that guess wrong, so that what the three measure differs by the
   answer being measured and by nothing else. Without it the wrong ones would be
   refused without deriving anything after the free attempts ran out, which is
   the gate working and would read here as an answer that costs nothing. */
void manyWrongLogins()
{
	for (int i = 0; i < kTimingRounds; ++i)
	{
		forgetLoginAttemptsForTest();
		(void) login(kUser, "not the password");
	}
}

void manyRightLogins()
{
	for (int i = 0; i < kTimingRounds; ++i)
	{
		forgetLoginAttemptsForTest();
		const Reply r = login(kUser, kPassword);
		// Every one of them opens a session, and a session left in the table
		// outlives this case. Closed by the token out of the cookie, which is
		// what the route that closes one is given.
		const std::string cookie = headerOf(r, "Set-Cookie");
		const size_t eq = cookie.find('=');
		const size_t end = cookie.find(';');
		if (eq != std::string::npos && end != std::string::npos && end > eq)
			(void) closeSession(cookie.substr(eq + 1, end - eq - 1));
	}
}

void manyWrongNames()
{
	for (int i = 0; i < kTimingRounds; ++i)
	{
		forgetLoginAttemptsForTest();
		(void) login("nobody at all", kPassword);
	}
}

// Eight attempts that each reach the derivation because the record in front of
// them is cleared first, against eight that the gate turns down because it is
// not. Written out here rather than inside the case so that both are measured
// through the same reading as everything else timed in this file.
void eightDerivedRefusals()
{
	for (int i = 0; i < 8; ++i)
	{
		forgetLoginAttemptsForTest();
		(void) login(kUser, "nope");
	}
}

void eightGatedRefusals()
{
	for (int i = 0; i < 8; ++i)
		REQUIRE(login(kUser, "nope").code == 429);
}

const uint64_t kChannel = 0x2b66ULL;

// An identifier the way every answer here writes one, so a case builds a
// request out of the same spelling it reads back rather than out of a second
// one written by hand.
std::string hexOf(uint64_t v)
{
	char buf[24];
	std::snprintf(buf, sizeof(buf), "%llx", (unsigned long long) v);
	return std::string(buf);
}

coreapi::ChannelInfo makeChannel(uint64_t id, const char *name)
{
	coreapi::ChannelInfo c;
	c.id = id;
	c.epg_id = id;
	c.name = name;
	c.kind = coreapi::ServiceKind::Tv;
	return c;
}

// Two bouquets on the channel fake, numbered as the box numbers them, because
// the fixture below ships none and every bouquet route names one.
void giveBouquets(FakeChannelSource &f)
{
	coreapi::BouquetInfo first;
	first.id = 1;
	first.name = "Erstes";
	first.user_bouquet = true;
	f.bouquets.push_back(first);

	coreapi::BouquetInfo second;
	second.id = 2;
	second.name = "Zweites";
	second.user_bouquet = true;
	f.bouquets.push_back(second);
}

// The bouquet of that name out of a listing, and a null value for a name the
// listing does not carry, so a bouquet that is gone and one that is unchanged
// cannot read alike.
::Json::Value bouquetNamed(const std::string &body, const std::string &name)
{
	const ::Json::Value items = parsed(body)["items"];
	for (::Json::ArrayIndex i = 0; i < items.size(); ++i)
	{
		if (items[i]["name"].asString() == name)
			return items[i];
	}
	return ::Json::Value();
}

// For a table written here rather than shipped: what the check over tables
// says about it is the subject, and nothing calls it.
Response unreachedHandler(const Request &)
{
	return noContent();
}

/* The number the document states for this route's body, and nought for a member it
   does not carry. Read out of the document rather than written down here, which is
   the whole point of the cases below: one carrying its own copy of the number would
   go on passing after the document and the server had parted company, and a page
   drawn from a document that offers what this box turns down looks like it worked. */
long documentSays(const char *path, const char *verb, const char *member)
{
	const ::Json::Value schema =
		parsed(openapi::document())["paths"][path][verb]["requestBody"]
			["content"]["application/json"]["schema"];
	return schema.isMember(member) ? (long) schema[member].asInt64() : 0L;
}

// A body of that many identifiers, written the way every answer here writes
// one, with the last of them spoiled where a case wants the refusal to be about
// something other than how many there are.
std::string channelList(uint64_t base, long how_many, bool last_is_junk)
{
	std::string body = "[";
	for (long i = 0; i < how_many; ++i)
	{
		if (i > 0)
			body += ',';
		body += '"';
		body += (last_is_junk && i + 1 == how_many) ? std::string("not-hex") : hexOf(base + (uint64_t) i);
		body += '"';
	}
	body += ']';
	return body;
}

// The same for the other body, whose members are the caller's own names.
std::string settingsObject(long how_many)
{
	std::string body = "{";
	for (long i = 0; i < how_many; ++i)
	{
		char one[64];
		std::snprintf(one, sizeof(one), "%s\"k%ld\":\"1\"", i > 0 ? "," : "", i);
		body += one;
	}
	body += '}';
	return body;
}

/* Everything a mutating route reaches, installed at once. One fixture and not one per
   case, because most of these routes touch two of them at a time: a zap is looked up
   in the channel list and posted to the command queue, and a case installing only one
   would be measuring whichever it left out. */
struct BoxFixture
{
	FakeChannelSource        channels;
	FakeCommandSink          commands;
	FakeEventSink            events;
	FakeSystemSource         box;
	FakeTimerSource          timers;
	FakeSettingsSource       store;

	InstalledChannelSource   installed_channels;
	InstalledSink            installed_commands;
	InstalledEventSink       installed_events;
	InstalledSystemSource    installed_box;
	InstalledTimerSource     installed_timers;
	InstalledSettingsSource  installed_store;

	BoxFixture()
		: installed_channels(&channels),
		  installed_commands(&commands),
		  installed_events(&events),
		  installed_box(&box),
		  installed_timers(&timers),
		  installed_store(&store)
	{
		channels.channels.push_back(makeChannel(kChannel, "the one"));
		channels.current = channels.channels[0];
		channels.current_status = coreapi::Status::Ok;
		box.volume_level = 40;
		// Well clear of the epoch, so a timer made an hour from now is not a
		// timer in the past on a clock the fixture never set.
		timers.clock = 1735689600;
	}

	private:
		BoxFixture(const BoxFixture &);
		BoxFixture &operator=(const BoxFixture &);
};

// A directory of its own per case, so nothing a case leaves behind is what the
// next one reads.
struct Sandbox
{
	std::string dir;

	Sandbox()
	{
		char tmpl[] = "/tmp/httpd_write_XXXXXX";
		if (mkdtemp(tmpl) != NULL)
			dir = tmpl;
	}

	~Sandbox()
	{
		if (!dir.empty())
			(void) system(("rm -rf " + dir).c_str());
	}

	std::string at(const std::string &name) const { return dir + "/" + name; }

	private:
		Sandbox(const Sandbox &);
		Sandbox &operator=(const Sandbox &);
};

/* The default list and the movie browser's file go with the roots, because the routes
   below put the roots together again out of both before they look at a name. Left as
   they ship, what the sandbox admits would be the media of whatever machine this runs
   on rather than the one directory the case made. */
struct RootsFixture
{
	Sandbox     box;
	std::string browser;
	std::string before;
	const char *before_defaults;
	const char *before_browser;

	RootsFixture()
		: browser(box.at("moviebrowser.conf")),
		  before(coreapi::storage::roots()),
		  before_defaults(coreapi::storage::internal::default_roots),
		  before_browser(coreapi::storage::internal::moviebrowser_config_path)
	{
		coreapi::storage::internal::default_roots = box.dir.c_str();
		coreapi::storage::internal::moviebrowser_config_path = browser.c_str();
		coreapi::storage::setRoots(box.dir);
	}

	~RootsFixture()
	{
		coreapi::storage::setRoots(before);
		coreapi::storage::internal::default_roots = before_defaults;
		coreapi::storage::internal::moviebrowser_config_path = before_browser;
	}

	private:
		RootsFixture(const RootsFixture &);
		RootsFixture &operator=(const RootsFixture &);
};

// An empty directory where the process table is read, so a daemon answers the
// same whatever else is running on the machine this is built on.
struct ProcFixture
{
	Sandbox     box;
	std::string before;

	ProcFixture() : before(coreapi::daemons::procRoot())
	{
		coreapi::daemons::setProcRoot(box.dir);
	}

	~ProcFixture() { coreapi::daemons::setProcRoot(before); }

	private:
		ProcFixture(const ProcFixture &);
		ProcFixture &operator=(const ProcFixture &);
};

/* A key the program declares, found in the schema rather than written out here,
   so that a row renamed in the tables fails this as the missing row it is
   instead of as a write that was refused. */
std::string secretKeyIn(const char *section)
{
	coreapi::Result<std::vector<coreapi::Descriptor> > got = coreapi::settings::schema();
	REQUIRE(got.ok());
	const std::vector<coreapi::Descriptor> rows = std::move(got).value();
	for (size_t i = 0; i < rows.size(); ++i)
	{
		if (rows[i].section == NULL || std::string(section) != rows[i].section)
			continue;
		if (!rows[i].secret || rows[i].type != coreapi::ValueType::String)
			continue;
		return rows[i].key;
	}
	return std::string();
}

std::string plainKeyIn(const char *section)
{
	coreapi::Result<std::vector<coreapi::Descriptor> > got = coreapi::settings::schema();
	REQUIRE(got.ok());
	const std::vector<coreapi::Descriptor> rows = std::move(got).value();
	for (size_t i = 0; i < rows.size(); ++i)
	{
		if (rows[i].section == NULL || std::string(section) != rows[i].section)
			continue;
		if (rows[i].secret || rows[i].type != coreapi::ValueType::String)
			continue;
		return rows[i].key;
	}
	return std::string();
}

std::string messageOfBytes(size_t n)
{
	std::string out = "{\"text\":\"";
	out.append(n, 'x');
	out += "\"}";
	return out;
}

std::string timerBody(time_t start)
{
	char buf[64];
	std::snprintf(buf, sizeof(buf), "%lld", (long long) start);
	std::string out = "{\"kind\":\"record\",\"channel_id\":\"2b66\",\"title\":\"the news\",\"start\":";
	out += buf;
	std::snprintf(buf, sizeof(buf), "%lld", (long long) (start + 3600));
	out += ",\"stop\":";
	out += buf;
	out += "}";
	return out;
}

// The route of that path and method, out of the tables the server ships, so a
// case holding an answer to a shape holds it to the shape the server declares.
const Endpoint *routeFor(Method m, const char *path)
{
	size_t tables = 0;
	const RouteTable *const *t = allRoutes(&tables);
	for (size_t i = 0; i < tables; ++i)
	{
		for (size_t j = 0; j < t[i]->count; ++j)
		{
			if (t[i]->endpoints[j].method == m && std::string(path) == t[i]->endpoints[j].path)
				return &t[i]->endpoints[j];
		}
	}
	return NULL;
}

} // namespace

TEST_CASE("login answers a cookie and a wrong password does not", "[write]")
{
	ShippedRoutes shipped;
	ConfigFixture cfg;

	const Reply bad = login(kUser, "nope");
	REQUIRE(bad.code == 401);
	REQUIRE(headerOf(bad, "Set-Cookie").empty());
	// Says what a yes is made of without saying which half of this one was
	// wrong, and names nothing this server has.
	REQUIRE(headerOf(bad, "WWW-Authenticate") == "Bearer");

	const Reply ok = login(kUser, kPassword);
	REQUIRE(ok.code == 200);

	const std::string c = headerOf(ok, "Set-Cookie");
	REQUIRE(c.find("HttpOnly") != std::string::npos);
	REQUIRE(c.find("SameSite=Lax") != std::string::npos);
	REQUIRE(c.find("Path=/") != std::string::npos);
	// A cookie marked Secure is never sent over a plain connection, and this
	// server has no TLS to offer, so one carrying it would be a session nothing
	// on this box could use.
	REQUIRE(c.find("Secure") == std::string::npos);

	/* The second token comes back in the body and not in the cookie. A token
	   the browser attaches by itself is attached to another site's requests as
	   well, which is the whole of what a second one is for. */
	REQUIRE(ok.body.find("\"csrf\"") != std::string::npos);
	const std::string csrf = stringField(ok.body, "csrf");
	REQUIRE_FALSE(csrf.empty());
	REQUIRE(c.find(csrf) == std::string::npos);
	// And the answer says which header to send it back in, so a page does not
	// carry the name written out in a second place.
	REQUIRE(stringField(ok.body, "csrf_header") == std::string(csrfHeaderName()));

	const size_t eq = c.find('=');
	const size_t end = c.find(';');
	REQUIRE(eq != std::string::npos);
	REQUIRE(end > eq);
	const std::string token = c.substr(eq + 1, end - eq - 1);
	REQUIRE(csrfFor(token) == csrf);
	REQUIRE(closeSession(token));
}

/* The route that opens a session and the route that describes one answer one
   document, and these three are what holds them to it.

   What they are worth is what they replace: opening a session used to answer three
   members, so a caller that had just logged in still had to ask a second route what
   it was allowed to do, and a page showed a session that may write as one that may
   only read for as long as that second answer took to arrive. */
TEST_CASE("opening a session answers the shape the session route answers", "[write]")
{
	ShippedRoutes shipped;
	ConfigFixture cfg;

	const Endpoint *opened = routeFor(Post, "/api/v1/login");
	const Endpoint *described = routeFor(Get, "/api/v1/session");
	REQUIRE(opened != NULL);
	REQUIRE(described != NULL);
	REQUIRE(opened->schema != NULL);
	/* One shape and not two that agree today. Two rows a person compared by
	   reading them are two rows that come apart the next time one of them is
	   edited, and a caller generating itself from the document would carry two
	   types for one thing. */
	REQUIRE(opened->schema == described->schema);

	const Reply ok = login(kUser, kPassword);
	REQUIRE(ok.code == 200);
	const std::string token = tokenOf(ok);

	const ::Json::Value root = parsed(ok.body);
	REQUIRE(root.isObject());

	/* Both directions, which is the whole of what a shape is worth: every
	   member the document promises has to be there, and a member the answer
	   carries that the document does not name would be one a caller is never
	   told about. */
	const Schema &shape = *opened->schema;
	for (size_t i = 0; i < shape.count; ++i)
	{
		INFO(shape.fields[i].name);
		REQUIRE(root.isMember(shape.fields[i].name));
	}
	REQUIRE(root.getMemberNames().size() == shape.count);

	REQUIRE(root["authenticated"].asBool() == true);
	REQUIRE(root["user"].asString() == std::string(kUser));
	REQUIRE(root["csrf"].asString() == csrfFor(token));
	REQUIRE(root["csrf_header"].asString() == std::string(csrfHeaderName()));

	/* Counted down and not handed out as a moment, and inside what the
	   configuration allows, so a member answering nought or answering the
	   moment the session started would fail here rather than read as a
	   plausible number. It is in this answer because knowing when to renew is a
	   question a caller has the moment it logs in. */
	const long left = (long) root["expires_in"].asInt64();
	REQUIRE(left > 0);
	REQUIRE(left <= (long) config().session_lifetime_s);

	REQUIRE(closeSession(token));
}

TEST_CASE("the level a login answers is what the session it opened is worth", "[write]")
{
	/* And never what the request that asked was granted. That request arrived
	   carrying nothing and was Public, and a caller told public a moment after it
	   logged in would go and ask for a credential it is already holding.

	   The member is constant today and is in the answer anyway: a caller has no
	   business holding the rule that a live session is granted the highest level. */
	ShippedRoutes shipped;
	ConfigFixture cfg;

	const Reply ok = login(kUser, kPassword);
	REQUIRE(ok.code == 200);
	const std::string token = tokenOf(ok);

	const std::string said = stringField(ok.body, "level");
	REQUIRE(said == std::string(authLevelName(AuthLevel::System)));
	// Written out a second time rather than read off the server alone, because
	// the name in the answer is the name the document uses and a page holds one
	// against the other.
	REQUIRE(said == "system");
	// What the asking request was granted, which is what an answer describing
	// the request instead of the session it made would have said.
	REQUIRE(said != std::string(authLevelName(AuthLevel::Public)));

	REQUIRE(closeSession(token));
}

TEST_CASE("a login and the session route describe one session the same way", "[write]")
{
	/* Member for member, which is what one writer means and what two writers
	   could not be held to: a member answered by one of them and not the other
	   is exactly the fault these cases exist for, and comparing the two answers
	   catches it whichever of them is the one that changed. */
	ShippedRoutes shipped;
	ConfigFixture cfg;

	const Reply ok = login(kUser, kPassword);
	REQUIRE(ok.code == 200);
	const std::string token = tokenOf(ok);

	// The same session, asked about through the other route, at the level a
	// request carrying it is granted.
	const Reply asked = replyOf(dispatch(Get, "/api/v1/session", "", "", kPeer,
	                                     sessionLevel(token), std::string(), token));
	REQUIRE(asked.code == 200);

	const ::Json::Value a = parsed(ok.body);
	const ::Json::Value b = parsed(asked.body);

	const Endpoint *opened = routeFor(Post, "/api/v1/login");
	REQUIRE(opened != NULL);
	REQUIRE(opened->schema != NULL);
	const Schema &shape = *opened->schema;
	REQUIRE(shape.count > 0);

	for (size_t i = 0; i < shape.count; ++i)
	{
		const std::string name = shape.fields[i].name;
		INFO(name);
		REQUIRE(a.isMember(name));
		REQUIRE(b.isMember(name));
		if (name == "expires_in")
		{
			/* The one member that is a countdown and not a fact: a second can
			   pass between the two answers, so the later one is the smaller and
			   both are still a session that has time left. */
			REQUIRE(b[name].asInt64() > 0);
			REQUIRE(b[name].asInt64() <= a[name].asInt64());
			continue;
		}
		REQUIRE(a[name] == b[name]);
	}

	REQUIRE(closeSession(token));
}

TEST_CASE("a login says nothing about which half was wrong", "[write]")
{
	ShippedRoutes shipped;
	ConfigFixture cfg;

	const Reply wrong_name = login("nobody at all", kPassword);
	const Reply wrong_secret = login(kUser, "nope");

	REQUIRE(wrong_name.code == wrong_secret.code);
	// Word for word, because a sentence that differed would answer the question
	// the code above refuses to answer.
	REQUIRE(wrong_name.body == wrong_secret.body);
	REQUIRE(wrong_name.body.find("nobody at all") == std::string::npos);
}

TEST_CASE("a wrong password takes as long as a right one", "[write]")
{
	/* A comparison that returns early tells an attacker how much of the secret
	   they have, and a name checked before the derivation tells them which
	   names exist. This is a coarse check and it says so: it fails only on a
	   difference large enough to be usable, which is the only kind that can be
	   measured across a network in any case. */
	ShippedRoutes shipped;
	ConfigFixture cfg;

	const double right = cpuSecondsOf(&manyRightLogins);
	const double wrong = cpuSecondsOf(&manyWrongLogins);
	const double name = cpuSecondsOf(&manyWrongNames);

	INFO("right " << right << " wrong " << wrong << " unknown name " << name);
	REQUIRE(right > 0.0);
	REQUIRE(wrong > right * 0.5);
	REQUIRE(wrong < right * 2.0);
	// The half that is easy to get wrong: a name nobody has must cost what a
	// name somebody has costs, or the box answers whether an account exists.
	REQUIRE(name > right * 0.5);
	REQUIRE(name < right * 2.0);
}

TEST_CASE("logging out drops the session the request arrived with", "[write]")
{
	ShippedRoutes shipped;
	ConfigFixture cfg;

	const Reply in = login(kUser, kPassword);
	REQUIRE(in.code == 200);
	const std::string c = headerOf(in, "Set-Cookie");
	const std::string token = c.substr(c.find('=') + 1, c.find(';') - c.find('=') - 1);
	REQUIRE_FALSE(csrfFor(token).empty());

	/* The token travels beside the request and is never named in what was sent:
	   a route taking the session to close out of a body would close anybody's
	   session for whoever can guess a token. */
	const Response out = dispatch(Post, "/api/v1/logout", "", "", "127.0.0.1",
	                              AuthLevel::System, std::string(), token);
	REQUIRE(out.code == 204);
	REQUIRE(csrfFor(token).empty());

	// And the browser is told to forget it, so a caller that logged out does
	// not go on presenting a token that names nothing.
	const Reply gone = replyOf(out);
	REQUIRE(headerOf(gone, "Set-Cookie").find("Max-Age=0") != std::string::npos);
}

TEST_CASE("a cookie alone does not carry a request that changes something", "[write]")
{
	/* The gate, driven with what a browser would send. A cookie is attached by
	   the browser to whatever asks for it, another site's page included, so the
	   cookie says who the caller is and the second token says who asked. */
	ShippedRoutes shipped;
	ConfigFixture cfg;

	const Reply in = login(kUser, kPassword);
	REQUIRE(in.code == 200);
	const std::string c = headerOf(in, "Set-Cookie");
	const std::string token = c.substr(c.find('=') + 1, c.find(';') - c.find('=') - 1);
	const std::string csrf = stringField(in.body, "csrf");

	Credentials cred;
	cred.peer = "192.168.1.9";
	cred.cookie_token = token;

	REQUIRE(granted(cred) == AuthLevel::System);

	Response refusal;
	// A read goes through on the cookie alone, which is what makes the page
	// work at all.
	REQUIRE(allowed(AuthLevel::Read, AuthLevel::System, Get, cred, &refusal));
	// A change does not.
	REQUIRE_FALSE(allowed(AuthLevel::Write, AuthLevel::System, Post, cred, &refusal));
	REQUIRE(refusal.code == 403);

	cred.csrf_header = csrf;
	REQUIRE(allowed(AuthLevel::Write, AuthLevel::System, Post, cred, &refusal));

	// A token that is not the one this session was given is no token at all.
	cred.csrf_header = std::string(csrf.size(), 'a');
	REQUIRE_FALSE(allowed(AuthLevel::Write, AuthLevel::System, Post, cred, &refusal));

	REQUIRE(closeSession(token));
}

TEST_CASE("every route that changes something asks for more than a read", "[write]")
{
	/* The one thing that must never regress, walked over the real tables rather than
	   over a list written here. A caller on the box's own network is granted a read
	   and is asked for no credential, so a route that changed something at that level
	   would be one any page a browser on that network visits could reach in that
	   browser's name. */
	ShippedRoutes shipped;

	size_t tables = 0;
	const RouteTable *const *t = allRoutes(&tables);
	size_t changing = 0;
	for (size_t i = 0; i < tables; ++i)
	{
		for (size_t j = 0; j < t[i]->count; ++j)
		{
			const Endpoint &e = t[i]->endpoints[j];
			if (e.method == Get || e.method == Head || e.method == Options)
				continue;
			INFO(e.path);
			if (std::string(e.path) == "/api/v1/login")
			{
				// The single exception, and the same one the check over the
				// tables makes: being reached grants nothing.
				REQUIRE(e.auth == AuthLevel::Public);
				continue;
			}
			REQUIRE((e.auth == AuthLevel::Write || e.auth == AuthLevel::System));
			++changing;
		}
	}
	// A walk that found nothing would pass this whatever the tables held, and a
	// route added at a level below a write has to move this number by hand.
	REQUIRE(changing == 48);
}

TEST_CASE("a caller on the local network cannot zap, reboot or write a setting", "[write]")
{
	ShippedRoutes shipped;
	BoxFixture box;

	REQUIRE(lanPost("/api/v1/zap", "{\"channel_id\":\"2b66\"}").code == 403);
	REQUIRE(lanPost("/api/v1/system/reboot", "{}").code == 403);
	REQUIRE(lanPatch("/api/v1/settings/audio", "{\"audio_volume_percent_ac3\":\"3\"}").code == 403);
	REQUIRE(lanPost("/api/v1/timers", timerBody(box.timers.clock + 3600)).code == 403);
	REQUIRE(lanPost("/api/v1/osd/message", "{\"text\":\"hello\"}").code == 403);

	// Nothing reached the box: a refusal that posted the command first would
	// answer 403 and change the channel anyway.
	REQUIRE(box.commands.posted.empty());
	REQUIRE(box.events.sent.empty());
	REQUIRE(box.timers.timers.empty());
}

TEST_CASE("a write asking for the box's own credential is refused a write", "[write]")
{
	// The other half of the rule above: a caller granted a write is still not
	// one that may take the box off the air or empty a credential.
	ShippedRoutes shipped;
	BoxFixture box;

	REQUIRE(send(Post, "/api/v1/system/reboot", "{}", AuthLevel::Write, "127.0.0.1").code == 403);
	REQUIRE(send(Post, "/api/v1/daemons/oscam/start", "{}", AuthLevel::Write, "127.0.0.1").code == 403);
	REQUIRE(send(Delete, "/api/v1/storage/path?path=/tmp/x", "", AuthLevel::Write, "127.0.0.1").code == 403);
	REQUIRE(box.events.sent.empty());

	// And a write is enough for the routes that ask for one.
	REQUIRE(send(Post, "/api/v1/zap", "{\"channel_id\":\"2b66\"}", AuthLevel::Write, "127.0.0.1").code == 202);
}

TEST_CASE("a state change answers accepted and not done", "[write]")
{
	/* The box is asked on its own loop and nothing comes back from what does
	   it, so the answer says the box was asked. Standby off while not in
	   standby is accepted and changes nothing, which is exactly the thing a 200
	   here would be claiming otherwise. */
	ShippedRoutes shipped;
	BoxFixture box;

	REQUIRE(authedPost("/api/v1/zap", "{\"channel_id\":\"2b66\"}").code == 202);
	REQUIRE(authedPost("/api/v1/system/standby", "{\"on\":true}").code == 202);
	REQUIRE(authedPost("/api/v1/system/reboot", "{}").code == 202);
	REQUIRE(authedPost("/api/v1/mode", "{\"mode\":\"radio\"}").code == 202);
	REQUIRE(authedPut("/api/v1/osd/volume", "{\"percent\":30}").code == 202);
	REQUIRE(authedPut("/api/v1/osd/mute", "{\"on\":true}").code == 202);

	// Each of them reached the box, so the code above is not what a route that
	// did nothing would answer either.
	REQUIRE(box.commands.posted.size() == 2);
	REQUIRE(box.events.sent.size() == 4);
}

TEST_CASE("a zap to a channel the box does not have posts nothing", "[write]")
{
	ShippedRoutes shipped;
	BoxFixture box;

	const Reply r = authedPost("/api/v1/zap", "{\"channel_id\":\"dead\"}");
	REQUIRE(r.code == 404);
	REQUIRE(r.body.find("no-such-channel") != std::string::npos);
	// A command is posted and forgotten, so a zap to an id nobody has would
	// otherwise be accepted and change nothing with nothing to say it.
	REQUIRE(box.commands.posted.empty());
}

TEST_CASE("a settings write reaches the layer and a refused one says which rule", "[write]")
{
	ShippedRoutes shipped;
	BoxFixture box;

	const Reply ok = authedPatch("/api/v1/settings/audio", "{\"audio_volume_percent_ac3\":\"50\"}");
	REQUIRE(ok.code == 200);
	// It reached the store rather than being answered here.
	REQUIRE(box.store.ints["audio_volume_percent_ac3"] == 50);
	REQUIRE(box.store.persisted == 1);
	// And the answer is the section as it reads now, which is what a caller
	// would get by asking.
	REQUIRE(authedGet("/api/v1/settings/audio").body == ok.body);

	const Reply bad = authedPatch("/api/v1/settings/audio", "{\"audio_volume_percent_ac3\":\"5000\"}");
	REQUIRE(bad.code == 400);
	REQUIRE(bad.body.find("out-of-range") != std::string::npos);
	// Refused before the store was asked, so the value the box was running on
	// is what it is still running on.
	REQUIRE(box.store.ints["audio_volume_percent_ac3"] == 50);

	const Reply none = authedPatch("/api/v1/settings/audio", "{\"there_is_no_such_key\":\"1\"}");
	REQUIRE(none.code == 404);
	REQUIRE(none.body.find("no-such-setting") != std::string::npos);

	// A section nobody declared is that, and not a page whose every setting is
	// unknown.
	const Reply nowhere = authedPatch("/api/v1/settings/no-such-page", "{\"audio_volume_percent_ac3\":\"50\"}");
	REQUIRE(nowhere.code == 404);
	REQUIRE(nowhere.body.find("no-such-name") != std::string::npos);

	/* A key declared on another page is refused as one this page does not
	   declare. A second code for it would tell a caller which keys exist
	   elsewhere on the box, one guess at a time. */
	const Reply elsewhere = authedPatch("/api/v1/settings/audio", "{\"network_ntprefresh\":\"30\"}");
	REQUIRE(elsewhere.code == 404);
	REQUIRE(elsewhere.body.find("no-such-setting") != std::string::npos);
}

TEST_CASE("a settings write of several keys reports each key's outcome", "[write]")
{
	/* One bad key must not silently discard the good ones, and must not hide
	   that it was discarded either. A single code cannot say both. */
	ShippedRoutes shipped;
	BoxFixture box;

	const Reply r = authedPatch("/api/v1/settings/audio",
	                            "{\"audio_volume_percent_ac3\":\"50\",\"nope\":\"1\"}");
	REQUIRE(r.code == 207);
	REQUIRE(r.body.find("\"audio_volume_percent_ac3\":{\"status\":200}") != std::string::npos);
	REQUIRE(r.body.find("no-such-setting") != std::string::npos);

	// The good one landed rather than being dropped with the bad one.
	REQUIRE(box.store.ints["audio_volume_percent_ac3"] == 50);

	const ::Json::Value results = parsed(r.body)["results"];
	REQUIRE(results.isObject());
	REQUIRE(results.size() == 2);
	REQUIRE(results["audio_volume_percent_ac3"]["status"].asInt() == 200);
	REQUIRE(results["nope"]["status"].asInt() == 404);
	REQUIRE(results["nope"]["code"].asString() == "no-such-setting");

	// Every one of them landing is answered as one thing and not as a document
	// a caller has to walk to find out that nothing went wrong.
	const Reply all = authedPatch("/api/v1/settings/audio",
	                              "{\"audio_volume_percent_ac3\":\"40\",\"audio_volume_percent_pcm\":\"60\"}");
	REQUIRE(all.code == 200);
	REQUIRE(box.store.ints["audio_volume_percent_ac3"] == 40);
	REQUIRE(box.store.ints["audio_volume_percent_pcm"] == 60);
}

TEST_CASE("a settings write of nothing at all is refused rather than answered ok", "[write]")
{
	ShippedRoutes shipped;
	BoxFixture box;

	const Reply empty = authedPatch("/api/v1/settings/audio", "{}");
	REQUIRE(empty.code == 400);
	REQUIRE(empty.body.find("missing-parameter") != std::string::npos);

	const Reply nonsense = authedPatch("/api/v1/settings/audio", "[1,2]");
	REQUIRE(nonsense.code == 400);
	REQUIRE(nonsense.body.find("bad-string") != std::string::npos);

	const Reply nested = authedPatch("/api/v1/settings/audio", "{\"a\":{\"b\":1}}");
	REQUIRE(nested.code == 400);
	REQUIRE(nested.body.find("bad-string") != std::string::npos);

	REQUIRE(box.store.persisted == 0);
}

TEST_CASE("a credential cannot be emptied by writing nothing to it", "[write]")
{
	/* A form redrawn from a read sends an empty field back, because the read of
	   a credential answers nothing, and taking that would wipe the value the
	   read protected. */
	ShippedRoutes shipped;
	BoxFixture box;

	const std::string secret = secretKeyIn("network");
	REQUIRE_FALSE(secret.empty());
	box.store.strings[secret] = "the password";

	const Reply r = authedPatch("/api/v1/settings/network", "{\"" + secret + "\":\"\"}");
	REQUIRE(r.code == 400);
	REQUIRE(r.body.find("empty-credential") != std::string::npos);
	REQUIRE(box.store.strings[secret] == "the password");
}

TEST_CASE("clearing a credential is its own act", "[write]")
{
	/* The verb writing to it could not express: an explicit clear, so emptying
	   one is deliberate and a round trip still cannot do it by accident. */
	ShippedRoutes shipped;
	BoxFixture box;

	const std::string secret = secretKeyIn("network");
	REQUIRE_FALSE(secret.empty());
	box.store.strings[secret] = "the password";

	const Reply r = authedPost("/api/v1/settings/secret/clear", "{\"key\":\"" + secret + "\"}");
	REQUIRE(r.code == 200);
	REQUIRE(box.store.strings[secret].empty());
	REQUIRE(box.store.persisted == 1);
	// Answered in the shape one setting has in a section listing, which for a
	// credential is nothing: clearing one is not what makes it unreadable.
	REQUIRE(stringField(r.body, "id") == secret);
	REQUIRE(stringField(r.body, "value").empty());

	// And the rule it exists beside is still there afterwards.
	REQUIRE(authedPatch("/api/v1/settings/network", "{\"" + secret + "\":\"\"}").code == 400);
}

TEST_CASE("clearing refuses a row that is not a credential", "[write]")
{
	ShippedRoutes shipped;
	BoxFixture box;

	const std::string plain = plainKeyIn("network");
	REQUIRE_FALSE(plain.empty());
	box.store.strings[plain] = "keep me";

	const Reply r = authedPost("/api/v1/settings/secret/clear", "{\"key\":\"" + plain + "\"}");
	REQUIRE(r.code == 400);
	/* Its own answer and not the one for a key nothing declares: the key is
	   right, and telling a caller there is no such setting would send it
	   looking for a name it already has. */
	REQUIRE(r.body.find("not-a-credential") != std::string::npos);
	REQUIRE(box.store.strings[plain] == "keep me");

	const Reply unknown = authedPost("/api/v1/settings/secret/clear", "{\"key\":\"there_is_no_such_key\"}");
	REQUIRE(unknown.code == 404);
	REQUIRE(unknown.body.find("no-such-setting") != std::string::npos);

	// It is the most a route can ask for, because it writes a value the write
	// beside it refuses to write.
	const Endpoint *ep = routeFor(Post, "/api/v1/settings/secret/clear");
	REQUIRE(ep != NULL);
	REQUIRE(ep->auth == AuthLevel::System);
}

TEST_CASE("a timer round trip creates, changes and removes", "[write]")
{
	ShippedRoutes shipped;
	BoxFixture box;

	const Reply created = authedPost("/api/v1/timers", timerBody(box.timers.clock + 3600));
	REQUIRE(created.code == 201);
	const std::string id = stringField(created.body, "id");
	REQUIRE_FALSE(id.empty());
	REQUIRE(headerOf(created, "Location") == "/api/v1/timers/" + id);
	REQUIRE(box.timers.timers.size() == 1);

	const Reply changed = authedPatch("/api/v1/timers/" + id, "{\"title\":\"other\"}");
	REQUIRE(changed.code == 200);
	// The change reached the daemon and the answer is read back from it rather
	// than written out of what was sent.
	REQUIRE(stringField(changed.body, "title") == "other");
	REQUIRE(box.timers.timers[0].title == "other");
	// What a correction left out is what it did not change, which is the whole
	// of the difference between a correction and a replacement.
	REQUIRE(box.timers.timers[0].channel_id == kChannel);
	REQUIRE(box.timers.timers[0].start == box.timers.clock + 3600);

	REQUIRE(authedDelete("/api/v1/timers/" + id).code == 204);
	REQUIRE(box.timers.timers.empty());
	// The daemon is asked whether it holds the timer before the removal, so a
	// second one is the removal of a timer that is not there.
	const Reply again = authedDelete("/api/v1/timers/" + id);
	REQUIRE(again.code == 404);
	REQUIRE(again.body.find("no-such-timer") != std::string::npos);

	REQUIRE(authedPatch("/api/v1/timers/" + id, "{\"title\":\"x\"}").code == 404);
}

TEST_CASE("a timer in the past is refused with the reason", "[write]")
{
	ShippedRoutes shipped;
	BoxFixture box;

	const Reply r = authedPost("/api/v1/timers", timerBody(box.timers.clock - 3600));
	REQUIRE(r.code == 400);
	REQUIRE(r.body.find("timer-in-the-past") != std::string::npos);
	REQUIRE(box.timers.timers.empty());
}

TEST_CASE("a timer of a kind the box does not make is refused by name", "[write]")
{
	ShippedRoutes shipped;
	BoxFixture box;

	/* The kinds are words and not the numbers the daemon files them under: one
	   of those numbers is kept for a kind the daemon no longer has and one is
	   the label a timer read off another box wears, so a caller naming a number
	   would be naming a file format. */
	const Reply r = authedPost("/api/v1/timers",
	                           "{\"kind\":\"remotebox\",\"start\":1735693200}");
	REQUIRE(r.code == 400);
	REQUIRE(r.body.find("bad-enum") != std::string::npos);
	REQUIRE(box.timers.timers.empty());

	// And a recording that ends no later than it begins, which is the layer
	// below's rule and not a second copy of it here.
	const Reply short_one = authedPost("/api/v1/timers",
	                                   "{\"kind\":\"record\",\"channel_id\":\"2b66\","
	                                   "\"start\":1735693200,\"stop\":1735693200}");
	REQUIRE(short_one.code == 400);
	REQUIRE(short_one.body.find("recording-without-duration") != std::string::npos);
}

TEST_CASE("the timer list is answered and carries what its shape says", "[write]")
{
	ShippedRoutes shipped;
	BoxFixture box;

	REQUIRE(authedPost("/api/v1/timers", timerBody(box.timers.clock + 3600)).code == 201);

	const Reply r = authedGet("/api/v1/timers");
	REQUIRE(r.code == 200);
	REQUIRE(r.content_type == "application/json");

	const ::Json::Value items = parsed(r.body)["items"];
	REQUIRE(items.size() == 1);
	REQUIRE(items[0]["kind"].asString() == "record");
	REQUIRE(items[0]["channel_id"].asString() == "2b66");

	const Endpoint *ep = routeFor(Get, "/api/v1/timers");
	REQUIRE(ep != NULL);
	REQUIRE(ep->auth == AuthLevel::Read);
	const char *why = "";
	INFO(why);
	REQUIRE(schemaIsSane(*ep->schema, &why));
}

TEST_CASE("a daemon nobody declared is not found and nothing is started", "[write]")
{
	ShippedRoutes shipped;
	ProcFixture proc;

	const Reply r = authedPost("/api/v1/daemons/rm-rf/start", "{}");
	REQUIRE(r.code == 404);
	REQUIRE(r.body.find("no-such-daemon") != std::string::npos);

	// The verb is a segment of the path and not a value in the body, so a verb
	// this server does not have is a path it does not have.
	REQUIRE(authedPost("/api/v1/daemons/oscam/detonate", "{}").code == 404);
	REQUIRE(authedPost("/api/v1/daemons/oscam/detonate", "{}").body.find("no-such-route") != std::string::npos);
}

TEST_CASE("an OSD message over the ceiling is refused before the box sees it", "[write]")
{
	ShippedRoutes shipped;
	BoxFixture box;

	const Reply over = authedPost("/api/v1/osd/message", messageOfBytes(5000));
	REQUIRE(over.code == 400);
	// Refused where the values are checked, which is before a handler is
	// entered, so the length never becomes an allocation on the thread the box
	// draws on.
	REQUIRE(box.events.sent.empty());

	REQUIRE(authedPost("/api/v1/osd/message", messageOfBytes(coreapi::osd::MAX_MESSAGE_BYTES)).code == 202);
	REQUIRE(box.events.sent.size() == 1);

	// Nothing to show is not a message, and it is the layer below that says so.
	const Reply nothing = authedPost("/api/v1/osd/message", "{\"text\":\"\"}");
	REQUIRE(nothing.code == 400);
	REQUIRE(nothing.body.find("missing-parameter") != std::string::npos);
}

TEST_CASE("the volume is answered and refused by the same numbers", "[write]")
{
	ShippedRoutes shipped;
	BoxFixture box;
	box.box.volume_level = 42;
	box.box.is_muted = true;

	const Reply r = authedGet("/api/v1/osd/volume");
	REQUIRE(r.code == 200);
	REQUIRE(parsed(r.body)["percent"].asInt() == 42);
	REQUIRE(parsed(r.body)["muted"].asBool());

	// Both readings or neither: a level with the mute state missing would be
	// drawn as sound that is on.
	box.box.mute_status = coreapi::Status::Internal;
	REQUIRE(authedGet("/api/v1/osd/volume").code == 500);
	box.box.mute_status = coreapi::Status::Ok;

	REQUIRE(authedPut("/api/v1/osd/volume", "{\"percent\":101}").code == 400);
	REQUIRE(authedPut("/api/v1/osd/volume", "{\"percent\":-1}").code == 400);
	REQUIRE(authedPut("/api/v1/osd/volume", "{\"percent\":100}").code == 202);
	REQUIRE(authedPut("/api/v1/osd/volume", "{\"percent\":0}").code == 202);
	// The event carries the number in a single byte, so a value that got past
	// the row would arrive as a different volume rather than as the loudest.
	REQUIRE(box.events.sent.size() == 2);
}

TEST_CASE("a directory is made and a path removed inside the roots and nowhere else", "[write]")
{
	ShippedRoutes shipped;
	RootsFixture roots;

	const std::string inside = roots.box.at("made");
	const Reply made = authedPost("/api/v1/storage/directory", "{\"path\":\"" + inside + "\"}");
	// Made rather than asked for: this one is a call to the filesystem that
	// answered before the response was built.
	REQUIRE(made.code == 201);
	struct stat st;
	REQUIRE(stat(inside.c_str(), &st) == 0);
	REQUIRE(S_ISDIR(st.st_mode));

	// The name is taken, which is a conflict and not a second creation.
	REQUIRE(authedPost("/api/v1/storage/directory", "{\"path\":\"" + inside + "\"}").code == 409);

	// Outside the roots is refused before the filesystem is asked, so what
	// exists elsewhere on the box cannot be read off which refusal comes back.
	const Reply outside = authedPost("/api/v1/storage/directory", "{\"path\":\"/etc/made\"}");
	REQUIRE(outside.code == 400);
	REQUIRE(outside.body.find("outside-roots") != std::string::npos);
	REQUIRE(stat("/etc/made", &st) != 0);

	REQUIRE(authedDelete("/api/v1/storage/path?path=" + inside).code == 204);
	REQUIRE(stat(inside.c_str(), &st) != 0);
	REQUIRE(authedDelete("/api/v1/storage/path?path=" + inside).code == 404);
	REQUIRE(authedDelete("/api/v1/storage/path?path=/etc/passwd").code == 400);
}

TEST_CASE("an id wider than the daemon's own is refused and removes nothing", "[write]")
{
	/* Driven rather than reasoned about, because the reasoning was wrong. The
	   accessor answers an unsigned long and the value below is a thirty two bit id,
	   and those are the same width on the box and not here, so a row that left the
	   bound to the cast was checked by nothing on this build: two to the thirty two
	   plus one arrived as one, and the removal of a timer nobody asked about answered
	   204 and took it. */
	ShippedRoutes shipped;
	BoxFixture box;

	REQUIRE(authedPost("/api/v1/timers", timerBody(box.timers.clock + 3600)).code == 201);
	REQUIRE(box.timers.timers.size() == 1);
	REQUIRE(box.timers.timers[0].id == 1);

	const Reply gone = authedDelete("/api/v1/timers/4294967297");
	REQUIRE(gone.code == 400);
	REQUIRE(gone.body.find("out-of-range") != std::string::npos);
	// The one that was there is still there, which is the half a status alone
	// does not say.
	REQUIRE(box.timers.timers.size() == 1);
	REQUIRE(box.timers.timers[0].id == 1);

	REQUIRE(authedPatch("/api/v1/timers/4294967297", "{\"title\":\"other\"}").code == 400);
	REQUIRE(box.timers.timers[0].title == "the news");

	// The daemon numbers from one, so nought names no timer and the row says so
	// rather than leaving it to be answered further down.
	REQUIRE(authedDelete("/api/v1/timers/0").code == 400);

	// And the widest id it can hand out is one the row still takes, so the
	// bound is the daemon's ceiling and not one below it.
	REQUIRE(authedDelete("/api/v1/timers/2147483647").code == 404);
}

TEST_CASE("a timer number wider than its field is refused and nothing is stored", "[write]")
{
	ShippedRoutes shipped;
	BoxFixture box;

	const time_t start = box.timers.clock + 3600;
	char at[64];
	std::snprintf(at, sizeof(at), "%lld", (long long) start);

	const std::string wide_repeat =
		std::string("{\"kind\":\"record\",\"channel_id\":\"2b66\",\"start\":") + at +
		",\"stop\":" + at + ",\"repeat\":4294967296}";
	const Reply repeat = authedPost("/api/v1/timers", wide_repeat);
	REQUIRE(repeat.code == 400);
	REQUIRE(repeat.body.find("out-of-range") != std::string::npos);
	// It was stored as nought before, which is a timer that runs once rather
	// than the one that was asked for.
	REQUIRE(box.timers.timers.empty());

	const std::string wide_count =
		std::string("{\"kind\":\"record\",\"channel_id\":\"2b66\",\"start\":") + at +
		",\"stop\":" + at + ",\"repeat_count\":4294967296}";
	REQUIRE(authedPost("/api/v1/timers", wide_count).code == 400);
	REQUIRE(box.timers.timers.empty());

	// The widest each of them means is still taken, so neither bound refuses
	// anything a caller can mean. 0xff00 is the weekday flag with every day set.
	char ends[64];
	std::snprintf(ends, sizeof(ends), "%lld", (long long) (start + 3600));
	const std::string widest =
		std::string("{\"kind\":\"record\",\"channel_id\":\"2b66\",\"start\":") + at +
		",\"stop\":" + ends + ",\"repeat\":65280,\"repeat_count\":2147483647}";
	const Reply made = authedPost("/api/v1/timers", widest);
	REQUIRE(made.code == 201);
	REQUIRE(box.timers.timers.size() == 1);
	REQUIRE(box.timers.timers[0].repeat == 65280);
	REQUIRE(box.timers.timers[0].repeat_count == 2147483647u);
}

TEST_CASE("a settings write naming one setting twice is refused whole", "[write]")
{
	/* This handler reads its own body, so the router's check of a name written twice
	   never ran over these. Without this the two halves of one server disagreed: the
	   route beside it answered duplicate-parameter while a repeat here was taken,
	   both writes ran, the first was discarded with nothing said, and the answer named
	   the key twice. */
	ShippedRoutes shipped;
	BoxFixture box;

	const Reply r = authedPatch("/api/v1/settings/audio",
	                            "{\"audio_volume_percent_ac3\":\"30\",\"audio_volume_percent_ac3\":\"70\"}");
	REQUIRE(r.code == 400);
	REQUIRE(r.body.find("duplicate-parameter") != std::string::npos);
	// Refused whole and before anything was written, so neither of the two
	// values is what the box is running on.
	REQUIRE(box.store.ints.find("audio_volume_percent_ac3") == box.store.ints.end());
	REQUIRE(box.store.persisted == 0);

	// The one that was refused, and the one that would have been misreported:
	// two values of one key where the first passes and the second does not.
	const Reply mixed = authedPatch("/api/v1/settings/audio",
	                                "{\"audio_volume_percent_ac3\":\"3\",\"audio_volume_percent_ac3\":\"5000\"}");
	REQUIRE(mixed.code == 400);
	REQUIRE(box.store.ints.find("audio_volume_percent_ac3") == box.store.ints.end());

	// And the route beside it answers a repeat the same way, through the
	// router, which is what the two halves now agree about.
	const Reply twice = authedPost("/api/v1/settings/secret/clear",
	                               "{\"key\":\"a\",\"key\":\"b\"}");
	REQUIRE(twice.code == 400);
	REQUIRE(twice.body.find("duplicate-parameter") != std::string::npos);
}

TEST_CASE("every key of an answer with a result per key is named once", "[write]")
{
	/* One member per result is a document only while the keys are distinct, and
	   what makes them distinct is the refusal above. Read back through a parser
	   rather than by looking at the bytes, because a document carrying one name
	   twice is one a parser silently collapses. */
	ShippedRoutes shipped;
	BoxFixture box;

	const Reply r = authedPatch("/api/v1/settings/audio",
	                            "{\"audio_volume_percent_ac3\":\"50\",\"nope\":\"1\",\"also_nope\":\"2\"}");
	REQUIRE(r.code == 207);

	const ::Json::Value results = parsed(r.body)["results"];
	REQUIRE(results.size() == 3);
	const ::Json::Value::Members names = results.getMemberNames();
	REQUIRE(names.size() == 3);
	for (size_t i = 0; i < names.size(); ++i)
	{
		size_t seen = 0;
		for (size_t j = 0; j < names.size(); ++j)
			seen += (names[i] == names[j]) ? 1 : 0;
		INFO(names[i]);
		REQUIRE(seen == 1);
	}
}

TEST_CASE("a settings value that cannot be answered back is not stored", "[write]")
{
	/* The writer replaces a byte it cannot read with the character that says
	   so, which is right for a name off a broadcast stream and wrong for a
	   value this server stores: written and read back it came out different, so
	   a client writing and reading never converged on what the box holds.
	   Refused on the way in, where substituting was silent. */
	ShippedRoutes shipped;
	BoxFixture box;

	const std::string plain = plainKeyIn("network");
	REQUIRE_FALSE(plain.empty());
	box.store.strings[plain] = "before";

	// Two bytes that begin no sequence there is, between two that are text.
	const std::string bad = "{\"" + plain + "\":\"a\xff\xfe b\"}";
	const Reply r = authedPatch("/api/v1/settings/network", bad);
	REQUIRE(r.code == 400);
	REQUIRE(r.body.find("bad-string") != std::string::npos);
	REQUIRE(box.store.strings[plain] == "before");

	// Text above the first hundred and twenty eight characters is still text
	// and is still stored, so the refusal is about what cannot be read and not
	// about what is not plain.
	const Reply fine = authedPatch("/api/v1/settings/network",
	                               "{\"" + plain + "\":\"\xc3\xa4\xe2\x82\xac\"}");
	REQUIRE(fine.code == 200);
	REQUIRE(box.store.strings[plain] == "\xc3\xa4\xe2\x82\xac");

	// And what is stored reads back as what was sent, which is the round trip
	// that did not settle before.
	REQUIRE(valueOf(authedGet("/api/v1/settings/network").body, plain) == "\xc3\xa4\xe2\x82\xac");
}

TEST_CASE("the wait after a wrong guess grows and stops growing", "[write]")
{
	/* The policy on its own, walked rather than slept through. The ceiling is
	   the whole of the promise that this cannot lock the only account out:
	   there is one account on this box and nobody to unlock it. */
	REQUIRE(loginDelayFor(0) == 0);
	REQUIRE(loginDelayFor(1) == 0);
	REQUIRE(loginDelayFor(2) == 0);
	REQUIRE(loginDelayFor(3) == 1);
	REQUIRE(loginDelayFor(4) == 2);
	REQUIRE(loginDelayFor(5) == 4);
	REQUIRE(loginDelayFor(6) == 8);

	// It rises, and it stops. Both halves, because a delay that stopped rising
	// at the third guess and one that never stopped are each wrong.
	unsigned last = 0;
	unsigned ceiling = 0;
	for (unsigned f = 3; f < 200; ++f)
	{
		const unsigned d = loginDelayFor(f);
		INFO("after " << f << " wrong answers");
		REQUIRE(d >= last);
		REQUIRE(d <= 60);
		if (d == 60)
			++ceiling;
		last = d;
	}
	// A ceiling nothing reaches would pass the bound above while leaving the
	// wait to grow for ever.
	REQUIRE(ceiling > 0);
	REQUIRE(loginDelayFor(4000000000u) == 60);
}

TEST_CASE("guessing is slowed down and the only account is never locked out", "[write]")
{
	ShippedRoutes shipped;
	ConfigFixture cfg;

	// The gate's clock, so the wait can be walked through without waiting it
	// out. Put back by the fixture.
	time_t now = 1735689600;
	setLoginClockForTest(now);

	// The free ones, because the ordinary reason a password is wrong is that
	// somebody mistyped it.
	REQUIRE(login(kUser, "nope").code == 401);
	REQUIRE(login(kUser, "nope").code == 401);

	const Reply third = login(kUser, "nope");
	REQUIRE(third.code == 401);

	// And now the address waits.
	const Reply refused = login(kUser, "nope");
	REQUIRE(refused.code == 429);
	REQUIRE(refused.body.find("too-many-attempts") != std::string::npos);
	REQUIRE(headerOf(refused, "Retry-After") == "1");
	// It says that too much is being asked and nothing about which of the two
	// things closed the gate, and it names neither the account nor the address.
	REQUIRE(refused.body.find(kUser) == std::string::npos);
	REQUIRE(refused.body.find(kPeer) == std::string::npos);

	// The right password is refused too while the wait stands, which is what
	// makes this a wait and not a filter on wrong guesses.
	REQUIRE(login(kUser, kPassword).code == 429);

	// It runs out on its own. Nobody has to unlock anything, because there is
	// nobody to do it.
	now += 2;
	setLoginClockForTest(now);
	const Reply in = login(kUser, kPassword);
	REQUIRE(in.code == 200);

	// And a right answer clears the record outright, so the next wrong one
	// starts from the free attempts again.
	REQUIRE(login(kUser, "nope").code == 401);
	REQUIRE(login(kUser, "nope").code == 401);
	REQUIRE(login(kUser, "nope").code == 401);

	const std::string c = headerOf(in, "Set-Cookie");
	REQUIRE(closeSession(c.substr(c.find('=') + 1, c.find(';') - c.find('=') - 1)));
}

TEST_CASE("the gate turns an attempt down before it derives anything", "[write]")
{
	/* The point of the gate is the processor and not only the guessing: login
	   is the one route reachable without a credential and it derives a key on
	   every attempt, so an answer that turned a caller down after deriving
	   would have paid for the attempt it was refusing. Coarse, and it says so:
	   it fails only on a difference large enough to matter. */
	ShippedRoutes shipped;
	ConfigFixture cfg;

	setLoginClockForTest(1735689600);

	const double derived = cpuSecondsOf(&eightDerivedRefusals);

	// Three wrong answers, and then eight refusals that cost nothing.
	for (int i = 0; i < 3; ++i)
		(void) login(kUser, "nope");
	const double turned = cpuSecondsOf(&eightGatedRefusals);

	INFO("derived " << derived << " turned down " << turned);
	REQUIRE(derived > 0.0);
	REQUIRE(turned < derived / 4.0);
}

TEST_CASE("one address guessing does not turn another one away", "[write]")
{
	ShippedRoutes shipped;
	ConfigFixture cfg;

	setLoginClockForTest(1735689600);

	for (int i = 0; i < 6; ++i)
		(void) send(Post, "/api/v1/login", loginBody(kUser, "nope"), AuthLevel::Public, "203.0.113.9");
	REQUIRE(send(Post, "/api/v1/login", loginBody(kUser, kPassword), AuthLevel::Public, "203.0.113.9").code == 429);

	// The record is per address, so somebody else's guessing is not what stands
	// between this caller and the box.
	const Reply in = send(Post, "/api/v1/login", loginBody(kUser, kPassword), AuthLevel::Public, "198.51.100.4");
	REQUIRE(in.code == 200);

	const std::string c = headerOf(in, "Set-Cookie");
	REQUIRE(closeSession(c.substr(c.find('=') + 1, c.find(';') - c.find('=') - 1)));
}

TEST_CASE("only so many attempts are answered at once", "[write]")
{
	/* The half a per address delay does not buy: an attacker with a thousand addresses
	   defeats that and does not defeat this, because every one of these is a
	   derivation on a thread of this daemon. Driven through the gate itself rather
	   than through the route, because what it bounds is how many are in flight and a
	   case that answered them one after another would never have two. */
	ShippedRoutes shipped;
	ConfigFixture cfg;

	unsigned retry = 0;
	REQUIRE(beginLoginAttempt("198.51.100.1", &retry) == LoginAttempt::Open);
	REQUIRE(beginLoginAttempt("198.51.100.2", &retry) == LoginAttempt::Open);
	REQUIRE(beginLoginAttempt("198.51.100.3", &retry) == LoginAttempt::Open);
	REQUIRE(beginLoginAttempt("198.51.100.4", &retry) == LoginAttempt::Open);

	// A fifth, from an address that has never guessed wrong at all.
	REQUIRE(beginLoginAttempt("198.51.100.5", &retry) == LoginAttempt::TooBusy);
	REQUIRE(retry > 0);

	// And the room comes back, so a burst is a wait and not a door that shuts.
	endLoginAttempt("198.51.100.1", false);
	REQUIRE(beginLoginAttempt("198.51.100.5", &retry) == LoginAttempt::Open);

	endLoginAttempt("198.51.100.2", false);
	endLoginAttempt("198.51.100.3", false);
	endLoginAttempt("198.51.100.4", false);
	endLoginAttempt("198.51.100.5", false);
}

TEST_CASE("the table of who has been guessing does not grow without bound", "[write]")
{
	/* Otherwise the thing an anonymous caller exhausts is this rather than the
	   processor. What goes when it is full is the address that has been quiet
	   longest, which is the direction that cannot lock anybody out. */
	ShippedRoutes shipped;
	ConfigFixture cfg;

	setLoginClockForTest(1735689600);

	char peer[32];
	for (int i = 0; i < 400; ++i)
	{
		std::snprintf(peer, sizeof(peer), "198.51.%d.%d", i / 200, i % 200);
		endLoginAttempt(peer, false);
	}
	REQUIRE(loginAttemptCountForTest() > 0);
	REQUIRE(loginAttemptCountForTest() <= 400u);

	// The sweep runs where the gate is asked, so this is what a request does.
	unsigned retry = 0;
	(void) beginLoginAttempt("198.51.100.250", &retry);
	endLoginAttempt("198.51.100.250", false);
	REQUIRE(loginAttemptCountForTest() <= 256u);
}

TEST_CASE("a bouquet round trip makes, renames, moves and takes away", "[write]")
{
	ShippedRoutes shipped;
	BoxFixture box;
	giveBouquets(box.channels);

	REQUIRE(authedPost("/api/v1/bouquets", "{\"name\":\"Drittes\"}").code == 201);

	/* Read back over the wire and not out of the answer. An answer of 201 says
	   what this server decided to send and nothing about the list the box
	   holds, and every check below is about the list. */
	const ::Json::Value made = bouquetNamed(authedGet("/api/v1/bouquets").body, "Drittes");
	REQUIRE(made["id"].asUInt() == 3u);
	REQUIRE(made["user_bouquet"].asBool());

	REQUIRE(authedPut("/api/v1/bouquets/Drittes/name", "{\"name\":\"Viertes\"}").code == 204);
	const std::string renamed = authedGet("/api/v1/bouquets").body;
	REQUIRE(bouquetNamed(renamed, "Viertes")["id"].asUInt() == 3u);
	// The key moved with it, so the old name names nothing.
	REQUIRE(bouquetNamed(renamed, "Drittes").isNull());

	REQUIRE(authedPut("/api/v1/bouquets/Viertes/position", "{\"direction\":\"up\"}").code == 204);
	const ::Json::Value moved = bouquetNamed(authedGet("/api/v1/bouquets").body, "Viertes");
	REQUIRE(moved["id"].asUInt() == 2u);

	REQUIRE(authedPut("/api/v1/bouquets/Viertes/hidden", "{\"on\":true}").code == 204);
	REQUIRE(authedPut("/api/v1/bouquets/Viertes/locked", "{\"on\":true}").code == 204);
	const std::string flagged = authedGet("/api/v1/bouquets").body;
	REQUIRE(bouquetNamed(flagged, "Viertes")["hidden"].asBool());
	REQUIRE(bouquetNamed(flagged, "Viertes")["locked"].asBool());
	// And the bouquets beside it are not what was meant.
	REQUIRE_FALSE(bouquetNamed(flagged, "Erstes")["hidden"].asBool());
	REQUIRE_FALSE(bouquetNamed(flagged, "Zweites")["locked"].asBool());

	REQUIRE(authedDelete("/api/v1/bouquets/Viertes").code == 204);
	REQUIRE(bouquetNamed(authedGet("/api/v1/bouquets").body, "Viertes").isNull());

	// A second removal is the removal of a bouquet that is not there.
	const Reply again = authedDelete("/api/v1/bouquets/Viertes");
	REQUIRE(again.code == 404);
	REQUIRE(again.body.find("no-such-bouquet") != std::string::npos);

	// A name something else carries is a conflict and moves nothing.
	const Reply taken = authedPost("/api/v1/bouquets", "{\"name\":\"Erstes\"}");
	REQUIRE(taken.code == 409);
	REQUIRE(taken.body.find("name-taken") != std::string::npos);
	REQUIRE(parsed(authedGet("/api/v1/bouquets").body)["items"].size() == 2u);
}

TEST_CASE("a bouquet name carrying a separator and a percent is one name", "[write]")
{
	ShippedRoutes shipped;
	BoxFixture box;
	giveBouquets(box.channels);

	/* The name is one path segment, and a segment is decoded after the path has
	   been split, so an encoded separator stays a byte of the name rather than
	   making a second segment out of it. */
	REQUIRE(authedPost("/api/v1/bouquets", "{\"name\":\"Sport/Film 50%\"}").code == 201);
	REQUIRE(authedPut("/api/v1/bouquets/Sport%2FFilm%2050%25/hidden", "{\"on\":true}").code == 204);

	const std::string all = authedGet("/api/v1/bouquets").body;
	// The bouquet that name means and no other, which is what a name split at
	// the separator would have reached instead.
	REQUIRE(bouquetNamed(all, "Sport/Film 50%")["hidden"].asBool());
	REQUIRE_FALSE(bouquetNamed(all, "Erstes")["hidden"].asBool());
	REQUIRE_FALSE(bouquetNamed(all, "Zweites")["hidden"].asBool());

	// And the same name written with the separator as a separator reaches no
	// route at all, which is the shape the encoding exists to avoid.
	REQUIRE(authedPut("/api/v1/bouquets/Sport/Film/hidden", "{\"on\":false}").code == 404);
}

TEST_CASE("a caller on the local network can change no bouquet", "[write]")
{
	ShippedRoutes shipped;
	BoxFixture box;
	giveBouquets(box.channels);

	REQUIRE(lanPost("/api/v1/bouquets", "{\"name\":\"Drittes\"}").code == 403);
	REQUIRE(lanPut("/api/v1/bouquets/Erstes/name", "{\"name\":\"Anders\"}").code == 403);
	REQUIRE(lanPut("/api/v1/bouquets/Zweites/position", "{\"direction\":\"up\"}").code == 403);
	REQUIRE(lanPut("/api/v1/bouquets/Erstes/hidden", "{\"on\":true}").code == 403);
	REQUIRE(lanPut("/api/v1/bouquets/Erstes/locked", "{\"on\":true}").code == 403);
	REQUIRE(lanPut("/api/v1/bouquets/Erstes/channels?mode=tv", "[\"2b66\"]").code == 403);
	REQUIRE(lanDelete("/api/v1/bouquets/Erstes").code == 403);

	// Nothing reached the box. A refusal written after the act would answer 403
	// and change the list anyway, and the code alone cannot tell the two apart.
	REQUIRE(box.channels.bouquets.size() == 2u);
	REQUIRE(box.channels.bouquets[0].name == "Erstes");
	REQUIRE(box.channels.bouquets[1].name == "Zweites");
	REQUIRE_FALSE(box.channels.bouquets[0].hidden);
	REQUIRE_FALSE(box.channels.bouquets[0].locked);
	REQUIRE(box.channels.saves == 0u);
	REQUIRE(box.channels.reloads == 0u);
}

TEST_CASE("a bouquet route written below a write is a table the server will not start on",
          "[write]")
{
	/* The other half of the rule, and the half the refusal above cannot show:
	   a route written at a read is refused where tables are checked, so the
	   server does not start and the route never answers anything at all. */
	static const Param params[] = {
		HTTPD_SEGMENT_TEXT("bouquet", "the bouquet", 255),
	};
	static const Endpoint too_low[] = {
		{ Delete, "/api/v1/bouquets/{bouquet}", AuthLevel::Read,
		  "takes a bouquet away", HTTPD_PARAMS(params), NULL, &unreachedHandler, false },
	};
	const RouteTable low = { HTTPD_TABLE_N("low", too_low, 1) };
	std::string why;
	REQUIRE_FALSE(tableIsSane(low, &why));
	INFO(why);
	REQUIRE(why.find("no more than a read") != std::string::npos);

	// The same table at a write is one it would start on, so what the check
	// turned down is the level and not anything else about the route.
	static const Endpoint high[] = {
		{ Delete, "/api/v1/bouquets/{bouquet}", AuthLevel::Write,
		  "takes a bouquet away", HTTPD_PARAMS(params), NULL, &unreachedHandler, false },
	};
	const RouteTable fine = { HTTPD_TABLE_N("fine", high, 1) };
	std::string nothing_wrong;
	REQUIRE(tableIsSane(fine, &nothing_wrong));
	REQUIRE(nothing_wrong.empty());
}

TEST_CASE("filling one half of a bouquet leaves the other half alone", "[write]")
{
	ShippedRoutes shipped;
	BoxFixture box;
	giveBouquets(box.channels);

	// One of each kind, and both of them in the first bouquet, so that a call
	// naming one half can be seen to leave the other where it was.
	coreapi::ChannelInfo radio = makeChannel(0x4c21ULL, "the radio one");
	radio.kind = coreapi::ServiceKind::Radio;
	box.channels.channels.push_back(radio);
	box.channels.bouquet_members[1].push_back(box.channels.channels[0]);
	box.channels.bouquet_members[1].push_back(radio);

	/* A channel the box does not have is refused, and the bouquet still holds
	   what it held. A handler that applied the list as it read it would have
	   emptied the television half and answered that the request failed. */
	const Reply absent = authedPut("/api/v1/bouquets/Erstes/channels?mode=tv",
	                               "[\"2b66\",\"dead\"]");
	REQUIRE(absent.code == 404);
	REQUIRE(absent.body.find("no-such-channel") != std::string::npos);
	REQUIRE(box.channels.bouquet_members[1].size() == 2u);
	REQUIRE(box.channels.saves == 0u);

	// A list that is not identifiers is refused before the box is asked at all.
	const Reply junk = authedPut("/api/v1/bouquets/Erstes/channels?mode=tv",
	                             "[\"2b66\",\"\",\"4c21\"]");
	REQUIRE(junk.code == 400);
	REQUIRE(box.channels.saves == 0u);

	// And so is a body that is not a list at all, which is what the body of
	// every other route here is.
	const Reply object = authedPut("/api/v1/bouquets/Erstes/channels?mode=tv",
	                               "{\"channel_ids\":\"2b66\"}");
	REQUIRE(object.code == 400);
	REQUIRE(box.channels.saves == 0u);

	// Which half the list is belongs to the request and not to the body, and a
	// caller that leaves it out is told it wrote half a request rather than
	// having the other half replaced.
	const Reply nohalf = authedPut("/api/v1/bouquets/Erstes/channels", "[\"2b66\"]");
	REQUIRE(nohalf.code == 400);
	REQUIRE(nohalf.body.find("missing-parameter") != std::string::npos);
	REQUIRE(box.channels.saves == 0u);

	REQUIRE(authedPut("/api/v1/bouquets/Erstes/channels?mode=tv",
	                  "[\"2b66\"]").code == 204);
	// Two, because the radio member stayed: the television half is what was
	// replaced and it now holds the one channel that was named.
	REQUIRE(box.channels.bouquet_members[1].size() == 2u);
	REQUIRE(box.channels.saves == 1u);
	REQUIRE(box.channels.reloads == 1u);

	const ::Json::Value now = bouquetNamed(authedGet("/api/v1/bouquets").body, "Erstes");
	REQUIRE(now["tv_count"].asUInt() == 1u);
	REQUIRE(now["radio_count"].asUInt() == 1u);
}

TEST_CASE("a bouquet holds its channels in the order the list was written in", "[write]")
{
	/* What a caller sends is what it can read back. The half held whatever
	   order the box happened to put things in before, so a page that let
	   somebody sort a bouquet had nothing to send, and the numbers the box
	   draws beside a channel are counted off that order. */
	ShippedRoutes shipped;
	BoxFixture box;
	giveBouquets(box.channels);

	const uint64_t first = 0x2b66ULL;
	const uint64_t second = 0x4c21ULL;
	const uint64_t third = 0x9f07ULL;
	box.channels.channels.push_back(makeChannel(second, "the second"));
	box.channels.channels.push_back(makeChannel(third, "the third"));

	REQUIRE(authedPut("/api/v1/bouquets/Erstes/channels?mode=tv",
	                  "[\"9f07\",\"2b66\",\"4c21\"]").code == 204);

	const ::Json::Value written = parsed(authedGet("/api/v1/channels?bouquet=1").body)["items"];
	REQUIRE(written.size() == 3u);
	REQUIRE(written[0]["id"].asString() == hexOf(third));
	REQUIRE(written[1]["id"].asString() == hexOf(first));
	REQUIRE(written[2]["id"].asString() == hexOf(second));

	// The other way round, because a list that arrived in the order the half
	// already held would say nothing about whether the order was read at all.
	REQUIRE(authedPut("/api/v1/bouquets/Erstes/channels?mode=tv",
	                  "[\"4c21\",\"2b66\",\"9f07\"]").code == 204);

	const ::Json::Value again = parsed(authedGet("/api/v1/channels?bouquet=1").body)["items"];
	REQUIRE(again.size() == 3u);
	REQUIRE(again[0]["id"].asString() == hexOf(second));
	REQUIRE(again[1]["id"].asString() == hexOf(first));
	REQUIRE(again[2]["id"].asString() == hexOf(third));

	/* A channel named twice is kept where it was first named. Dropping the
	   second mention is what keeps a bouquet from holding one channel twice,
	   and keeping the first place is what keeps the rest of the order the
	   caller's. */
	REQUIRE(authedPut("/api/v1/bouquets/Erstes/channels?mode=tv",
	                  "[\"9f07\",\"2b66\",\"9f07\",\"4c21\"]").code == 204);

	const ::Json::Value twice = parsed(authedGet("/api/v1/channels?bouquet=1").body)["items"];
	REQUIRE(twice.size() == 3u);
	REQUIRE(twice[0]["id"].asString() == hexOf(third));
	REQUIRE(twice[1]["id"].asString() == hexOf(first));
	REQUIRE(twice[2]["id"].asString() == hexOf(second));
}

TEST_CASE("a list no single value could carry is one this route takes", "[write]")
{
	/* Three hundred channels, which as one value was more than this server
	   lets a value be: an identifier and its separator are seventeen bytes and
	   a value stops at four thousand and ninety six, so a provider's bouquet
	   could not be written at all. The body carries the list now, and what
	   bounds it is the number of channels the route states. */
	ShippedRoutes shipped;
	BoxFixture box;
	giveBouquets(box.channels);

	const size_t many = 300;
	const uint64_t base = 0x2b66000000010000ULL;
	std::string body = "[";
	for (size_t i = 0; i < many; ++i)
	{
		box.channels.channels.push_back(makeChannel(base + i, "one of many"));
		if (i > 0)
			body += ',';
		body += '"';
		body += hexOf(base + i);
		body += '"';
	}
	body += ']';

	// The ceiling that used to stop this, measured rather than asserted from
	// memory: a case that sent a short list would pass whatever the ceiling is.
	REQUIRE(body.size() > 4096u);

	REQUIRE(authedPut("/api/v1/bouquets/Erstes/channels?mode=tv", body).code == 204);

	const ::Json::Value now = bouquetNamed(authedGet("/api/v1/bouquets").body, "Erstes");
	REQUIRE(now["tv_count"].asUInt() == (unsigned) many);

	// And in the order they were written in, which is the other half of the
	// same request: a page of the listing reads the beginning of that order.
	const ::Json::Value page = parsed(authedGet("/api/v1/channels?bouquet=1&limit=3").body)["items"];
	REQUIRE(page.size() == 3u);
	REQUIRE(page[0]["id"].asString() == hexOf(base));
	REQUIRE(page[1]["id"].asString() == hexOf(base + 1));
	REQUIRE(page[2]["id"].asString() == hexOf(base + 2));
}

TEST_CASE("a list longer than a bouquet takes is refused by name and changes nothing", "[write]")
{
	/* A ceiling there has to be, and this is the one the route chose rather
	   than the one that fell out of how a value happens to travel. Named, so a
	   caller can tell it from a body that is wrong, and refused rather than
	   met by taking the first four thousand of them. */
	ShippedRoutes shipped;
	BoxFixture box;
	giveBouquets(box.channels);
	box.channels.bouquet_members[1].push_back(box.channels.channels[0]);

	const uint64_t base = 0x2b66000000010000ULL;
	std::string body = "[";
	for (size_t i = 0; i <= 4096; ++i)
	{
		if (i > 0)
			body += ',';
		body += '"';
		body += hexOf(base + i);
		body += '"';
	}
	body += ']';

	const Reply over = authedPut("/api/v1/bouquets/Erstes/channels?mode=tv", body);
	REQUIRE(over.code == 400);
	REQUIRE(over.body.find("too-many-channels") != std::string::npos);
	// The number the route states, out of the answer, so the sentence and the
	// ceiling cannot drift apart with nothing said.
	REQUIRE(over.body.find("4096") != std::string::npos);

	// Nothing of it was applied, and none of those channels exists on this
	// box: a route that read the list first would have answered no-such-channel
	// and told a caller to go looking for a channel rather than to send less.
	REQUIRE(box.channels.bouquet_members[1].size() == 1u);
	REQUIRE(box.channels.saves == 0u);

	// One below the ceiling is a request, so the refusal is the ceiling and not
	// everything above some smaller number.
	std::string under = body.substr(0, body.rfind(','));
	under += ']';
	const Reply at = authedPut("/api/v1/bouquets/Erstes/channels?mode=tv", under);
	REQUIRE(at.code == 404);
	REQUIRE(at.body.find("no-such-channel") != std::string::npos);
}

TEST_CASE("a bouquet write the box never received is not answered as done", "[write]")
{
	/* The one answer none of the seven may give. With the channel daemon's socket gone
	   the box goes on running and every command written to it is written into nothing,
	   and each of these answered that it had done what it was asked while the bouquet
	   list stood still. */
	ShippedRoutes shipped;
	BoxFixture box;
	giveBouquets(box.channels);
	box.channels.bouquet_members[1].push_back(box.channels.channels[0]);
	box.channels.write_status = coreapi::Status::Internal;

	Reply answers[7];
	answers[0] = authedPost("/api/v1/bouquets", "{\"name\":\"Drittes\"}");
	answers[1] = authedDelete("/api/v1/bouquets/Zweites");
	answers[2] = authedPut("/api/v1/bouquets/Erstes/name", "{\"name\":\"Anders\"}");
	answers[3] = authedPut("/api/v1/bouquets/Erstes/position", "{\"direction\":\"down\"}");
	answers[4] = authedPut("/api/v1/bouquets/Erstes/hidden", "{\"on\":true}");
	answers[5] = authedPut("/api/v1/bouquets/Erstes/locked", "{\"on\":true}");
	answers[6] = authedPut("/api/v1/bouquets/Erstes/channels?mode=tv",
	                       "[\"" + hexOf(kChannel) + "\"]");

	for (size_t i = 0; i < sizeof(answers) / sizeof(answers[0]); ++i)
	{
		INFO("answer " << i);
		// Named rather than left to the code alone, because those two are what
		// a caller reads as done and this server has no third way of saying it.
		REQUIRE(answers[i].code != 201);
		REQUIRE(answers[i].code != 204);
		REQUIRE(answers[i].code == 500);
		REQUIRE(answers[i].body.find("bouquet-not-changed") != std::string::npos);
	}

	// And the list is where it was, which is the half the code alone cannot
	// say: an answer that refused and changed something would read the same.
	const std::string all = authedGet("/api/v1/bouquets").body;
	REQUIRE(bouquetNamed(all, "Drittes").isNull());
	REQUIRE_FALSE(bouquetNamed(all, "Zweites").isNull());
	REQUIRE(bouquetNamed(all, "Anders").isNull());
	REQUIRE(bouquetNamed(all, "Erstes")["id"].asUInt() == 1u);
	REQUIRE_FALSE(bouquetNamed(all, "Erstes")["hidden"].asBool());
	REQUIRE_FALSE(bouquetNamed(all, "Erstes")["locked"].asBool());
	REQUIRE(box.channels.bouquet_members[1].size() == 1u);

	// Nothing was written out or read back either, because a change that did
	// not happen has nothing to save.
	REQUIRE(box.channels.saves == 0u);
	REQUIRE(box.channels.reloads == 0u);
}

TEST_CASE("an empty list empties that half and a body carrying no list does not", "[write]")
{
	/* The two belong in one case, because what separates them is the whole of the
	   rule. A body that was never written and a body written as an empty array were
	   answered alike, the reason being that a half nobody meant to empty must not be
	   emptied. That holds for the first and not for the second: the body of this
	   route is the list, so a request that forgot it carries no array at all. */
	ShippedRoutes shipped;
	BoxFixture box;
	giveBouquets(box.channels);

	coreapi::ChannelInfo radio = makeChannel(0x4c21ULL, "the radio one");
	radio.kind = coreapi::ServiceKind::Radio;
	box.channels.channels.push_back(radio);
	box.channels.bouquet_members[1].push_back(box.channels.channels[0]);
	box.channels.bouquet_members[1].push_back(radio);

	// No list at all, which is what a client that built its body wrong sends.
	const Reply nothing = authedPut("/api/v1/bouquets/Erstes/channels?mode=tv", "");
	REQUIRE(nothing.code == 400);
	REQUIRE(nothing.body.find("bad-string") != std::string::npos);
	REQUIRE(box.channels.bouquet_members[1].size() == 2u);
	REQUIRE(box.channels.saves == 0u);

	// Nor is the half the request is addressed to something that may be left
	// out: that is a parameter the route requires, and a request without it is
	// turned away before a handler is entered, whatever its body says.
	const Reply nohalf = authedPut("/api/v1/bouquets/Erstes/channels", "[]");
	REQUIRE(nohalf.code == 400);
	REQUIRE(nohalf.body.find("missing-parameter") != std::string::npos);
	REQUIRE(box.channels.bouquet_members[1].size() == 2u);
	REQUIRE(box.channels.saves == 0u);

	// The same route and the same bouquet as the refusal above, one written
	// pair of brackets apart.
	const Reply emptied = authedPut("/api/v1/bouquets/Erstes/channels?mode=tv", "[]");
	REQUIRE(emptied.code == 204);
	REQUIRE(box.channels.saves == 1u);
	REQUIRE(box.channels.reloads == 1u);

	/* The television half holds nothing and the radio half is where it was,
	   because a list is one half of a bouquet and never both. A route that
	   read an empty list as both would take a bouquet apart. */
	const ::Json::Value now = bouquetNamed(authedGet("/api/v1/bouquets").body, "Erstes");
	REQUIRE(now["tv_count"].asUInt() == 0u);
	REQUIRE(now["radio_count"].asUInt() == 1u);
	REQUIRE(box.channels.bouquet_members[1].size() == 1u);

	// And the channel it held is still on the box, because taking a channel
	// out of a bouquet is not taking it off the box.
	REQUIRE(authedGet("/api/v1/channels/" + hexOf(kChannel)).code == 200);
}

TEST_CASE("a bouquet at the end of the list is not moved past it", "[write]")
{
	ShippedRoutes shipped;
	BoxFixture box;
	giveBouquets(box.channels);

	const Reply top = authedPut("/api/v1/bouquets/Erstes/position", "{\"direction\":\"up\"}");
	REQUIRE(top.code == 400);
	REQUIRE(top.body.find("already-at-the-end") != std::string::npos);

	const Reply bottom = authedPut("/api/v1/bouquets/Zweites/position",
	                               "{\"direction\":\"down\"}");
	REQUIRE(bottom.code == 400);
	REQUIRE(bottom.body.find("already-at-the-end") != std::string::npos);

	// And the list is where it was, which an answer that shifted nothing and
	// said ok would look exactly like from the outside.
	const std::string all = authedGet("/api/v1/bouquets").body;
	REQUIRE(bouquetNamed(all, "Erstes")["id"].asUInt() == 1u);
	REQUIRE(bouquetNamed(all, "Zweites")["id"].asUInt() == 2u);
	REQUIRE(box.channels.saves == 0u);
}


namespace
{

/* A configuration of this case's own, in a file of its own, installed the way the
   program installs one: through the read that also records where the file is, which
   is what the route writes back into. What is in effect is put back on the way out,
   because it is a global of this program. */
struct WebFile
{
	std::string path;
	WebConfig   before;

	WebFile(const char *tag, const std::string &text) : before(config())
	{
		char name[160];
		std::snprintf(name, sizeof(name), "/tmp/ni-webroute-%s-%d", tag, (int) getpid());
		path = name;

		std::FILE *f = std::fopen(path.c_str(), "w");
		REQUIRE(f != NULL);
		REQUIRE(std::fwrite(text.data(), 1, text.size(), f) == text.size());
		REQUIRE(std::fclose(f) == 0);

		REQUIRE(load(path));
		REQUIRE(configPath() == path);
	}

	~WebFile()
	{
		::unlink(path.c_str());
		setConfigForTest(before);
	}

	private:
		WebFile(const WebFile &);
		WebFile &operator=(const WebFile &);
};

std::string wholeOf(const std::string &path)
{
	std::string out;
	std::FILE *f = std::fopen(path.c_str(), "r");
	if (f == NULL)
		return out;
	char buf[1024];
	size_t got = 0;
	while ((got = std::fread(buf, 1, sizeof(buf), f)) > 0)
		out.append(buf, got);
	std::fclose(f);
	return out;
}

/* The whole answer and not the part a Reply keeps, because what half these
   cases are about is the field of it that never reaches a wire: what the
   transport is to put the server on once the answer has gone. */
Response putWebserver(const std::string &body,
                      const std::string &host = "box.example:8081",
                      AuthLevel as = AuthLevel::System)
{
	return dispatch(Put, "/api/v1/system/webserver", "", body, "192.0.2.7", as,
	                std::string(), std::string(), host);
}

const char kWebFileHead[] = "port=8081\nbind_address=0.0.0.0\nusername=root\n";

/* Every member the shape names is in the answer and nothing else is, one level down as
   well.

   A walk of its own and not the one the read cases keep, which is private to that file:
   what the two share is a rule rather than a function, and a third file for a dozen lines
   would be a place for the rule to go stale in. */
void answerFitsShape(const ::Json::Value &v, const Schema &sch, const std::string &where)
{
	INFO(where << " against " << sch.name);
	REQUIRE(v.isObject());

	for (size_t i = 0; i < sch.count; ++i)
	{
		const FieldDesc &f = sch.fields[i];
		INFO("member " << f.name);
		if (!f.optional)
			REQUIRE(v.isMember(f.name));
		if (v.isMember(f.name) && f.type == FieldType::Object && f.nested != NULL)
			answerFitsShape(v[f.name], *f.nested, where + "." + f.name);
	}

	const std::vector<std::string> named = v.getMemberNames();
	for (size_t i = 0; i < named.size(); ++i)
	{
		INFO("member " << named[i] << " is in the answer and in no row of the shape");
		bool declared = false;
		for (size_t k = 0; k < sch.count; ++k)
			declared = declared || named[i] == sch.fields[k].name;
		REQUIRE(declared);
	}
}

} // namespace

TEST_CASE("moving the port says where the box will be, and moves it afterwards", "[write]")
{
	/* The first of the four ways this route can lock its caller out. The
	   connection this answer arrives on is the last one at the old address, and
	   nothing else in this program would tell the caller where to go next. */
	ShippedRoutes shipped;
	WebFile file("port", kWebFileHead);

	const Response r = putWebserver("{\"port\":8099}");
	REQUIRE(r.code == 200);

	const ::Json::Value root = parsed(r.body);
	REQUIRE(root["webserver"]["port"].asInt() == 8099);
	REQUIRE(root["caller"]["address"].asString() == "http://box.example:8099/");
	REQUIRE(root["caller"]["port_moved"].asBool());
	REQUIRE(root["caller"]["restarting"].asBool());

	// The file says so before the answer does anything about it.
	REQUIRE(wholeOf(file.path).find("port=8099") != std::string::npos);

	/* And the server is not on it yet. The answer names the file the transport
	   is to read once these bytes have gone out, and that is the whole of the
	   order this route exists to keep. */
	REQUIRE(r.reload_after == file.path);
	REQUIRE(config().server.port == 8081);
}

TEST_CASE("narrowing what the server listens on is said before it happens", "[write]")
{
	/* The second way out. A caller that is not on the loopback will not reach
	   this box again after this, and the answer is the last thing it hears. */
	ShippedRoutes shipped;
	WebFile file("bind", kWebFileHead);

	const Response r = putWebserver("{\"bind\":\"127.0.0.1\"}");
	REQUIRE(r.code == 200);

	const ::Json::Value root = parsed(r.body);
	REQUIRE(root["webserver"]["bind"].asString() == "127.0.0.1");
	// The port did not move, so the address a caller holds is still an address.
	REQUIRE_FALSE(root["caller"]["port_moved"].asBool());
	REQUIRE(root["caller"]["restarting"].asBool());
	REQUIRE(r.reload_after == file.path);
	REQUIRE(wholeOf(file.path).find("bind_address=127.0.0.1") != std::string::npos);
}

TEST_CASE("a new password is taken as a password and answered as neither form", "[write]")
{
	/* The third way out, and the one with something to hide. What goes in is a
	   password; what the file holds is the stored form; what the answer says is
	   that the next sign in needs a different one. */
	ShippedRoutes shipped;
	WebFile file("password", kWebFileHead);

	const Response r = putWebserver("{\"password\":\"a password nobody guesses\"}");
	REQUIRE(r.code == 200);

	const ::Json::Value root = parsed(r.body);
	REQUIRE(root["caller"]["password_changed"].asBool());
	REQUIRE(root["webserver"]["has_password"].asBool());
	REQUIRE(r.body.find("a password nobody guesses") == std::string::npos);
	REQUIRE(r.body.find("pbkdf2") == std::string::npos);
	REQUIRE(r.reload_after == file.path);

	/* The file carries the stored form and never the value, which is what
	   verifies against it. */
	const std::string written = wholeOf(file.path);
	REQUIRE(written.find("a password nobody guesses") == std::string::npos);
	const size_t at = written.find("password_hash=");
	REQUIRE(at != std::string::npos);
	const size_t end = written.find('\n', at);
	REQUIRE(end != std::string::npos);
	const std::string stored = written.substr(at + 14, end - at - 14);
	REQUIRE(verifySecret("a password nobody guesses", stored));
}

TEST_CASE("the networks that read without a password are not narrowed from here", "[write]")
{
	/* The fourth way out, and the one this route does not offer. Narrowing that
	   list locks out every caller on the network at once rather than the one
	   asking, and the line belongs to the save that owns it. A body naming it is
	   a body naming something this route does not take, and nothing is written
	   at all. */
	ShippedRoutes shipped;
	WebFile file("lanread", kWebFileHead);

	const std::string before = wholeOf(file.path);
	const Response r = putWebserver("{\"lan_read\":\"127.0.0.1/32\"}");
	REQUIRE(r.code == 400);
	REQUIRE(r.reload_after.empty());
	REQUIRE(wholeOf(file.path) == before);

	// And the list is still the one a read answers with.
	const Response read = dispatch(Get, "/api/v1/system/webserver", "", "", "192.0.2.7",
	                               AuthLevel::System);
	REQUIRE(read.code == 200);
	REQUIRE(parsed(read.body)["lan_read"].size() > 0u);
}

TEST_CASE("a change that could not be saved does not move the server", "[write]")
{
	/* Otherwise the box runs until its next start on something its file does not
	   say, which is a state nobody can see from outside and which the next start
	   silently takes back. */
	ShippedRoutes shipped;
	WebFile file("refused", kWebFileHead);

	const std::string before = wholeOf(file.path);
	const Response r = putWebserver("{\"bind\":\"not an address\"}");
	REQUIRE(r.code == 400);
	REQUIRE(r.content_type == "application/problem+json");
	REQUIRE(r.reload_after.empty());
	REQUIRE(wholeOf(file.path) == before);

	// The box says which value it was and not only that one was.
	REQUIRE(r.body.find("bind_address") != std::string::npos);
}

TEST_CASE("asking for what is already in effect takes the box off nothing", "[write]")
{
	/* A restart for a change that is not one would be a way to take the box off
	   the network for a moment by asking it politely for what it already has. */
	ShippedRoutes shipped;
	WebFile file("same", kWebFileHead);

	const Response r = putWebserver("{\"port\":8081,\"user\":\"root\"}");
	REQUIRE(r.code == 200);

	const ::Json::Value root = parsed(r.body);
	REQUIRE_FALSE(root["caller"]["port_moved"].asBool());
	REQUIRE_FALSE(root["caller"]["restarting"].asBool());
	REQUIRE(r.reload_after.empty());
}

TEST_CASE("a body naming nothing is not a way to restart the server", "[write]")
{
	ShippedRoutes shipped;
	WebFile file("empty", kWebFileHead);

	const Response r = putWebserver("{}");
	REQUIRE(r.code == 400);
	REQUIRE(r.reload_after.empty());
}

TEST_CASE("what a request does not name keeps the value it had", "[write]")
{
	/* A caller changing a port has no business restating a user name it was not
	   asked about, and one that had to would write back whatever it last read.
	   An empty member is a different thing: that is a value the caller wrote,
	   and the save turns an empty one down. */
	ShippedRoutes shipped;
	WebFile file("kept", "port=8081\nbind_address=0.0.0.0\nusername=someone\n");

	const Response r = putWebserver("{\"port\":8099}");
	REQUIRE(r.code == 200);
	REQUIRE(parsed(r.body)["webserver"]["user"].asString() == "someone");
	REQUIRE(wholeOf(file.path).find("username=someone") != std::string::npos);

	const Response emptied = putWebserver("{\"user\":\"\"}");
	REQUIRE(emptied.code == 400);
	REQUIRE(emptied.reload_after.empty());
}

TEST_CASE("setting the server up asks for what the calls that stop the box ask for", "[write]")
{
	ShippedRoutes shipped;
	WebFile file("level", kWebFileHead);

	const std::string before = wholeOf(file.path);
	const Response r = putWebserver("{\"port\":8099}", "box.example:8081", AuthLevel::Write);
	REQUIRE(r.code == 403);
	REQUIRE(r.reload_after.empty());
	REQUIRE(wholeOf(file.path) == before);
}

TEST_CASE("the answer to a write is the shape the document states for it", "[write]")
{
	ShippedRoutes shipped;
	WebFile file("shape", kWebFileHead);

	const Response r = putWebserver("{\"port\":8099}");
	REQUIRE(r.code == 200);

	const Endpoint *ep = routeFor(Put, "/api/v1/system/webserver");
	REQUIRE(ep != NULL);
	REQUIRE(ep->schema != NULL);
	const char *why = "";
	INFO(why);
	REQUIRE(schemaIsSane(*ep->schema, &why));
	answerFitsShape(parsed(r.body), *ep->schema, "PUT /api/v1/system/webserver");
}

TEST_CASE("a request that named no authority is handed no address it made up", "[write]")
{
	/* A name this side invented reads to a caller exactly like the one it gave,
	   so a handler with no authority to build on says it has none. */
	ShippedRoutes shipped;
	WebFile file("noauthority", kWebFileHead);

	const Response r = putWebserver("{\"port\":8099}", std::string());
	REQUIRE(r.code == 200);
	REQUIRE(parsed(r.body)["caller"]["address"].asString().empty());
}

TEST_CASE("every table this server ships is still one it can start on", "[write]")
{
	/* Two tables can carry one answer between them and neither of them is wrong
	   read alone, so the routes added here are checked against every route
	   there was as well as against each other. */
	ShippedRoutes shipped;

	size_t tables = 0;
	const RouteTable *const *t = allRoutes(&tables);
	REQUIRE(tables >= 10);

	std::string why;
	const bool sane = tablesAreSane(t, tables, &why);
	INFO(why);
	REQUIRE(sane);
}

TEST_CASE("the routes that drive the plugins ask for what their acts are worth", "[write]")
{
	/* Read off the shipped tables rather than written out here. Two of them
	   change what the box is running and one of them runs a file as the account
	   this server runs as, and only the last of those is the whole of the box.
	   The walk above counts them; this says which is which. */
	ShippedRoutes shipped;

	size_t tables = 0;
	const RouteTable *const *t = allRoutes(&tables);
	size_t seen = 0;
	for (size_t i = 0; i < tables; ++i)
	{
		for (size_t j = 0; j < t[i]->count; ++j)
		{
			const Endpoint &e = t[i]->endpoints[j];
			const std::string path = e.path;
			INFO(path);
			if (path == "/api/v1/scripts/{name}")
			{
				REQUIRE(e.auth == AuthLevel::System);
				++seen;
			}
			else if (path == "/api/v1/plugins/{name}/start" || path == "/api/v1/plugins/reload")
			{
				REQUIRE(e.auth == AuthLevel::Write);
				++seen;
			}
			else if (path == "/api/v1/plugins")
			{
				REQUIRE(e.auth == AuthLevel::Read);
				++seen;
			}
		}
	}
	// A walk that found none of them would pass every line above.
	REQUIRE(seen == 4);
}

TEST_CASE("the ceiling the document states for a bouquet's list is the one the server keeps", "[write]")
{
	/* Not that a number is there, but that it is the right one. Both sides are
	   read rather than written: the document says how many, and the server is
	   asked with exactly that many and with one more. A document saying seven
	   or forty thousand fails here, and a case comparing the document against a
	   constant typed beside it would pass with either. */
	ShippedRoutes shipped;
	BoxFixture box;
	giveBouquets(box.channels);
	box.channels.bouquet_members[1].push_back(box.channels.channels[0]);

	const long most = documentSays("/api/v1/bouquets/{bouquet}/channels", "put", "maxItems");
	REQUIRE(most > 0);

	const uint64_t base = 0x2b66000000010000ULL;
	const Reply over = authedPut("/api/v1/bouquets/Erstes/channels?mode=tv",
	                             channelList(base, most + 1, false));
	REQUIRE(over.code == 400);
	REQUIRE(over.body.find("too-many-channels") != std::string::npos);

	/* And exactly that many is not too many. The last of them is not
	   hexadecimal, so the answer names that instead of walking four thousand
	   identifiers against a source that holds none of them: what is asked here
	   is that the refusal is no longer about how many there are. */
	const Reply at = authedPut("/api/v1/bouquets/Erstes/channels?mode=tv",
	                           channelList(base, most, true));
	REQUIRE(at.code == 400);
	REQUIRE(at.body.find("too-many-channels") == std::string::npos);

	REQUIRE(box.channels.bouquet_members[1].size() == 1u);
	REQUIRE(box.channels.saves == 0u);
}

TEST_CASE("the floor the document states for a bouquet's list is the one the server keeps", "[write]")
{
	/* Asked of both sides rather than written down, because this is the number
	   that has moved once already: the branch that lets an empty list empty a
	   half took the floor from one to none. A tree where one of the two moved
	   and the other did not fails here, rather than shipping a page that offers
	   what the box turns down or hides what it takes. */
	ShippedRoutes shipped;
	BoxFixture box;
	giveBouquets(box.channels);
	box.channels.bouquet_members[1].push_back(box.channels.channels[0]);

	const long least = documentSays("/api/v1/bouquets/{bouquet}/channels", "put", "minItems");
	const Reply empty = authedPut("/api/v1/bouquets/Erstes/channels?mode=tv", "[]");
	REQUIRE((empty.code >= 400) == (least > 0));
}

TEST_CASE("the document describes the settings body as the server reads it", "[write]")
{
	// Three numbers and one kind, each asked of both sides.
	ShippedRoutes shipped;
	BoxFixture box;

	const long most  = documentSays("/api/v1/settings/{section}", "patch", "maxProperties");
	const long least = documentSays("/api/v1/settings/{section}", "patch", "minProperties");
	REQUIRE(most > 0);

	/* A section nobody declares, so the answer turns on the body alone: the
	   body reader runs first and a body it refuses never reaches the section
	   check. One over the ceiling is a body this server will not read; exactly
	   the ceiling gets as far as a section that is not there. */
	const Reply over = authedPatch("/api/v1/settings/no-such-page", settingsObject(most + 1));
	REQUIRE(over.code == 400);

	const Reply at = authedPatch("/api/v1/settings/no-such-page", settingsObject(most));
	REQUIRE(at.code == 404);

	// The floor, the same way.
	const Reply none = authedPatch("/api/v1/settings/audio", "{}");
	REQUIRE((none.code >= 400) == (least > 0));

	/* And the kind: what the document says a value is, is what the route takes.
	   A setting read as text and written as text is what lets a page show a
	   value and send it again. */
	const ::Json::Value value =
		parsed(openapi::document())["paths"]["/api/v1/settings/{section}"]["patch"]
			["requestBody"]["content"]["application/json"]["schema"]["additionalProperties"];
	REQUIRE(value["type"].asString() == "string");
	REQUIRE(authedPatch("/api/v1/settings/audio",
	                    "{\"audio_volume_percent_ac3\":\"50\"}").code == 200);
}

/* The tuner put down and up again, which is what somebody reaches for when the
   picture has gone and the box has carried on as though it had not.

   A fixture of its own, because the one the rest of this file uses installs the
   five seams a body driven route needs and the tuner is not one of them. What
   is installed is put back from a destructor, because a check that fails
   unwinds past a last line. */
namespace
{

struct TunerFixture
{
	FakeTunerSource      tuner;
	InstalledTunerSource installed;

	TunerFixture() : installed(&tuner) {}

	private:
		TunerFixture(const TunerFixture &);
		TunerFixture &operator=(const TunerFixture &);
};

} // namespace

TEST_CASE("a tuner reset is asked of the box and answers nothing of its own", "[write]")
{
	ShippedRoutes shipped;
	TunerFixture fx;

	REQUIRE(fx.tuner.resets == 0);

	const Reply r = authedPost("/api/v1/tuner/reset", "");
	// No content and not accepted: the channel daemon is told and answers, so
	// by the time this is written the reset has been taken.
	REQUIRE(r.code == 204);
	REQUIRE(r.body.empty());
	REQUIRE(fx.tuner.resets == 1);
}

TEST_CASE("a box that would not take the reset says so", "[write]")
{
	ShippedRoutes shipped;
	TunerFixture fx;

	fx.tuner.reset_status = coreapi::Status::Internal;

	const Reply r = authedPost("/api/v1/tuner/reset", "");
	REQUIRE(r.code == 500);
	// Its own code, because what a caller does about it is not what it does
	// about a signal it cannot read: the box did not even take the request.
	REQUIRE(r.body.find("tuner-not-reset") != std::string::npos);
	REQUIRE(fx.tuner.resets == 1);
}

TEST_CASE("a tuner reset is not something a write level reaches", "[write]")
{
	/* It drives the channel daemon's own standby, which is what the route that
	   puts the box in standby is System for, and every live viewer loses the
	   picture while it runs. The reload of the channel lists beside it is Write
	   because it rewrites what the box holds and leaves the picture alone. */
	ShippedRoutes shipped;
	TunerFixture fx;

	REQUIRE(lanPost("/api/v1/tuner/reset", "").code == 403);
	REQUIRE(send(Post, "/api/v1/tuner/reset", "", AuthLevel::Write, "127.0.0.1").code == 403);
	REQUIRE(fx.tuner.resets == 0);
}

/* The two files the box keeps its network filesystems in, written as entries.

   Both paths are process wide and are put back from a destructor, because a
   check that fails unwinds past a last line and the case after this one would
   otherwise be writing whatever this one pointed at. */
namespace
{

struct NetfsFixture
{
	Sandbox     box;
	std::string before_fstab;
	std::string before_auto;
	std::string before_init;

	NetfsFixture()
		: before_fstab(coreapi::netfs::pathOf(coreapi::netfs::Table::Fstab)),
		  before_auto(coreapi::netfs::pathOf(coreapi::netfs::Table::Automount)),
		  before_init(coreapi::netfs::initRoot())
	{
		coreapi::netfs::setPath(coreapi::netfs::Table::Fstab, box.at("fstab"));
		coreapi::netfs::setPath(coreapi::netfs::Table::Automount, box.at("auto.net"));
		/* A directory of its own for the scripts, because one of them is called
		   fstab and so is one of the files: on the box they sit in different
		   directories, and a fixture that put them in one would have the script
		   and the file at one name. */
		REQUIRE(::mkdir(box.at("init.d").c_str(), 0700) == 0);
		coreapi::netfs::setInitRoot(box.at("init.d"));
	}

	~NetfsFixture()
	{
		coreapi::netfs::setPath(coreapi::netfs::Table::Fstab, before_fstab);
		coreapi::netfs::setPath(coreapi::netfs::Table::Automount, before_auto);
		coreapi::netfs::setInitRoot(before_init);
	}

	void put(const char *name, const std::string &text) const
	{
		std::FILE *f = std::fopen(box.at(name).c_str(), "w");
		REQUIRE(f != NULL);
		REQUIRE(std::fwrite(text.data(), 1, text.size(), f) == text.size());
		REQUIRE(std::fclose(f) == 0);
	}

	void script(const char *name, const std::string &text) const
	{
		const std::string at = box.at("init.d") + "/" + name;
		std::FILE *f = std::fopen(at.c_str(), "w");
		REQUIRE(f != NULL);
		REQUIRE(std::fwrite(text.data(), 1, text.size(), f) == text.size());
		REQUIRE(std::fclose(f) == 0);
		REQUIRE(::chmod(at.c_str(), 0700) == 0);
	}

	std::string fstab() const { return wholeOf(box.at("fstab")); }

	private:
		NetfsFixture(const NetfsFixture &);
		NetfsFixture &operator=(const NetfsFixture &);
};

// One entry out of a table the route answered with, found by its slot rather
// than by where it sits, although the two agree: what a write addresses is the
// slot, and a case reading by position would pass over a table that had moved.
::Json::Value slotOf(const std::string &body, unsigned slot)
{
	const ::Json::Value items = parsed(body)["items"];
	for (::Json::ArrayIndex i = 0; i < items.size(); ++i)
	{
		if (items[i]["slot"].asUInt() == slot)
			return items[i];
	}
	return ::Json::Value();
}

const char kNfsEntry[] =
	"{\"active\":true,\"type\":\"nfs\",\"host\":\"192.168.0.10\","
	"\"remote_dir\":\"/nas/video\",\"local_dir\":\"/srv\",\"options\":\"soft,nolock\"}";

} // namespace

TEST_CASE("an entry written into a slot is what the file then holds", "[write][netfs]")
{
	ShippedRoutes shipped;
	NetfsFixture netfs;

	const Reply r = authedPut("/api/v1/storage/netfs/fstab/0", kNfsEntry);
	REQUIRE(r.code == 200);

	// The answer is the whole table, because the write rewrote the whole file
	// and what a caller has to be able to read afterwards is the file.
	REQUIRE(parsed(r.body)["items"].size() == 8);
	const ::Json::Value put = slotOf(r.body, 0);
	REQUIRE(put["host"].asString() == "192.168.0.10");
	REQUIRE(put["remote_dir"].asString() == "/nas/video");
	REQUIRE(put["local_dir"].asString() == "/srv");
	REQUIRE(put["active"].asBool());

	/* And the file reads as a filesystem table and not only as something this
	   server can read back: the columns are the six such a file has, in the
	   order a reader of one splits them. */
	const std::string written = netfs.fstab();
	REQUIRE(written.find("192.168.0.10:/nas/video\t/srv\t\tnfs\tsoft,nolock\t\t0\t0\n")
		!= std::string::npos);
	// The head the screen at the box writes, so that the file is the same file
	// whichever of the two wrote it last.
	REQUIRE(written.compare(0, 2, "# ") == 0);
}

TEST_CASE("a write leaves the password where a read could not show it", "[write][netfs]")
{
	/* A read answers nothing for the password, so a form that redraws itself
	   from what it read sends nothing back, and taking that nothing would wipe
	   the value the read protected. */
	ShippedRoutes shipped;
	NetfsFixture netfs;

	netfs.put("fstab",
		  "//192.168.1.99/folder\t/mnt/folder\t\tcifs\tusername=uli,password=geheim\t\t0\t0\n");

	const Reply r = authedPut("/api/v1/storage/netfs/fstab/0",
				  "{\"active\":true,\"type\":\"cifs\",\"host\":\"192.168.1.99\","
				  "\"remote_dir\":\"/folder\",\"local_dir\":\"/mnt/anders\","
				  "\"user\":\"uli\"}");
	REQUIRE(r.code == 200);
	REQUIRE(r.body.find("geheim") == std::string::npos);
	REQUIRE(slotOf(r.body, 0)["has_password"].asBool());
	REQUIRE(slotOf(r.body, 0)["local_dir"].asString() == "/mnt/anders");

	// Still in the file, which is where the kernel reads it from.
	REQUIRE(netfs.fstab().find("password=geheim") != std::string::npos);
}

TEST_CASE("a password that was typed replaces the one that was there", "[write][netfs]")
{
	ShippedRoutes shipped;
	NetfsFixture netfs;

	netfs.put("fstab",
		  "//192.168.1.99/folder\t/mnt/folder\t\tcifs\tusername=uli,password=alt\t\t0\t0\n");

	const Reply r = authedPut("/api/v1/storage/netfs/fstab/0",
				  "{\"active\":true,\"type\":\"cifs\",\"host\":\"192.168.1.99\","
				  "\"remote_dir\":\"/folder\",\"local_dir\":\"/mnt/folder\","
				  "\"user\":\"uli\",\"password\":\"neu\"}");
	REQUIRE(r.code == 200);
	// Neither form of it is in the answer: the one that went in is not read
	// back, and the one that was there is gone.
	REQUIRE(r.body.find("neu") == std::string::npos);
	REQUIRE(r.body.find("alt") == std::string::npos);

	const std::string written = netfs.fstab();
	REQUIRE(written.find("password=neu") != std::string::npos);
	REQUIRE(written.find("password=alt") == std::string::npos);
}

TEST_CASE("writing one slot leaves every other slot where it stands", "[write][netfs]")
{
	ShippedRoutes shipped;
	NetfsFixture netfs;

	netfs.put("fstab",
		  "192.168.0.10:/first\t/srv\t\tnfs\tsoft\t\t0\t0\n"
		  "192.168.0.10:/second\t/mnt/two\t\tnfs\tsoft\t\t0\t0\n");

	const Reply r = authedPut("/api/v1/storage/netfs/fstab/1", kNfsEntry);
	REQUIRE(r.code == 200);

	REQUIRE(slotOf(r.body, 0)["remote_dir"].asString() == "/first");
	REQUIRE(slotOf(r.body, 1)["remote_dir"].asString() == "/nas/video");
	REQUIRE(slotOf(r.body, 2)["host"].asString() == "");
}

TEST_CASE("an emptied slot is still a slot and carries nothing", "[write][netfs]")
{
	/* Emptying the slot is also the one way a password comes off a mount, there
	   being no other reading of a member left out. */
	ShippedRoutes shipped;
	NetfsFixture netfs;

	netfs.put("fstab",
		  "//192.168.1.99/folder\t/mnt/folder\t\tcifs\tusername=uli,password=geheim\t\t0\t0\n"
		  "192.168.0.10:/second\t/mnt/two\t\tnfs\tsoft\t\t0\t0\n");

	const Reply r = authedDelete("/api/v1/storage/netfs/fstab/0");
	REQUIRE(r.code == 200);

	REQUIRE(parsed(r.body)["items"].size() == 8);
	REQUIRE(slotOf(r.body, 0)["host"].asString() == "");
	REQUIRE_FALSE(slotOf(r.body, 0)["has_password"].asBool());
	REQUIRE_FALSE(slotOf(r.body, 0)["active"].asBool());
	// The slot behind it did not move up into the one that was emptied.
	REQUIRE(slotOf(r.body, 1)["remote_dir"].asString() == "/second");

	REQUIRE(netfs.fstab().find("geheim") == std::string::npos);
	// An empty slot is written as a line that mounts nothing, so the start up
	// script passes over it rather than trying to reach a server nobody named.
	REQUIRE(netfs.fstab().find("noauto") != std::string::npos);
}

TEST_CASE("a value that would not survive the file is refused and nothing is written",
	  "[write][netfs]")
{
	ShippedRoutes shipped;
	NetfsFixture netfs;

	netfs.put("fstab", "192.168.0.10:/first\t/srv\t\tnfs\tsoft\t\t0\t0\n");
	const std::string before = netfs.fstab();

	struct Bad
	{
		const char *what;
		const char *body;
	};
	const Bad bad[] = {
		// The columns are split on whitespace, so a value carrying any would be
		// read back as two columns and take the ones behind it with it.
		{ "a space in the mount point",
		  "{\"active\":true,\"type\":\"nfs\",\"host\":\"192.168.0.10\",\"remote_dir\":\"/x\","
		  "\"local_dir\":\"/mnt/two words\"}" },
		// A number sign ends a line in the second file, and in the first it is
		// what a comment begins with.
		{ "a number sign in the server",
		  "{\"active\":true,\"type\":\"nfs\",\"host\":\"one#two\",\"remote_dir\":\"/x\","
		  "\"local_dir\":\"/mnt/two\"}" },
		// The colon is what separates the server from what it exports.
		{ "a colon in an export server",
		  "{\"active\":true,\"type\":\"nfs\",\"host\":\"a:b\",\"remote_dir\":\"/x\","
		  "\"local_dir\":\"/mnt/two\"}" },
		// Three options this layer writes itself out of members of their own.
		{ "an option this layer writes itself",
		  "{\"active\":true,\"type\":\"nfs\",\"host\":\"192.168.0.10\",\"remote_dir\":\"/x\","
		  "\"local_dir\":\"/mnt/two\",\"options\":\"soft,noauto\"}" },
		{ "a password written as an option",
		  "{\"active\":true,\"type\":\"cifs\",\"host\":\"192.168.0.10\",\"remote_dir\":\"/x\","
		  "\"local_dir\":\"/mnt/two\",\"options\":\"password=geheim\"}" }
	};

	for (size_t i = 0; i < sizeof(bad) / sizeof(bad[0]); ++i)
	{
		INFO(bad[i].what);
		const Reply r = authedPut("/api/v1/storage/netfs/fstab/1", bad[i].body);
		REQUIRE(r.code == 400);
		REQUIRE(r.body.find("bad-string") != std::string::npos);
		REQUIRE(netfs.fstab() == before);
	}
}

TEST_CASE("an entry written to the automounter goes in that file's own shape", "[write][netfs]")
{
	ShippedRoutes shipped;
	NetfsFixture netfs;

	const Reply r = authedPut("/api/v1/storage/netfs/automount/0",
				  "{\"active\":false,\"type\":\"cifs\",\"host\":\"192.168.0.11\","
				  "\"remote_dir\":\"/pictures\",\"local_dir\":\"pictures\","
				  "\"user\":\"uli\",\"password\":\"geheim\"}");
	REQUIRE(r.code == 200);

	const std::string written = wholeOf(netfs.box.at("auto.net"));
	// Switched off is a line commented out there and never the word the other
	// file uses, and the server is named without the stray pair of characters
	// the screen at the box writes in front of it.
	REQUIRE(written.find("#pictures\t-fstype=cifs,username=uli,password=geheim"
			     "\t\t//192.168.0.11/pictures\n") != std::string::npos);
	REQUIRE(written.find("noauto") == std::string::npos);
	REQUIRE(written.find("://") == std::string::npos);

	// And it reads back as what went in.
	const ::Json::Value put = slotOf(r.body, 0);
	REQUIRE_FALSE(put["active"].asBool());
	REQUIRE(put["local_dir"].asString() == "pictures");
	REQUIRE(put["host"].asString() == "192.168.0.11");
	REQUIRE(put["user"].asString() == "uli");
	REQUIRE(put["has_password"].asBool());
	REQUIRE(put["options"].asString() == "");
}

TEST_CASE("mounting and unmounting are the box's own script and not a mount from here",
	  "[write][netfs]")
{
	/* What is mounted at start up and what a caller asks for afterwards have to
	   be one piece of work, and they are only one if one script does both. */
	ShippedRoutes shipped;
	NetfsFixture netfs;

	netfs.script("fstab", "#!/bin/sh\nprintf 'ran %s\\n' \"$1\"\n");

	const Reply mounted = authedPost("/api/v1/storage/netfs/fstab/mount", "");
	REQUIRE(mounted.code == 200);
	REQUIRE(stringField(mounted.body, "output") == "ran start_netfs\n");

	const Reply unmounted = authedPost("/api/v1/storage/netfs/fstab/unmount", "");
	REQUIRE(unmounted.code == 200);
	REQUIRE(stringField(unmounted.body, "output") == "ran stop_netfs\n");
}

TEST_CASE("the automounter is started again so that it reads its file afresh", "[write][netfs]")
{
	ShippedRoutes shipped;
	NetfsFixture netfs;

	netfs.script("autofs", "#!/bin/sh\nprintf 'ran %s\\n' \"$1\"\n");

	const Reply r = authedPost("/api/v1/storage/netfs/automount/reload", "");
	REQUIRE(r.code == 200);
	REQUIRE(stringField(r.body, "output") == "ran restart\n");
}

TEST_CASE("none of the network filesystem routes is reached below system", "[write][netfs]")
{
	ShippedRoutes shipped;
	NetfsFixture netfs;

	REQUIRE(lanPut("/api/v1/storage/netfs/fstab/0", kNfsEntry).code == 403);
	REQUIRE(send(Put, "/api/v1/storage/netfs/fstab/0", kNfsEntry,
		     AuthLevel::Write, "127.0.0.1").code == 403);
	REQUIRE(lanDelete("/api/v1/storage/netfs/fstab/0").code == 403);
	REQUIRE(lanPost("/api/v1/storage/netfs/fstab/mount", "").code == 403);
	REQUIRE(lanPost("/api/v1/storage/netfs/automount/reload", "").code == 403);
}

TEST_CASE("the switch that shows a logo is a line of this server's own file", "[write]")
{
	/* It decides what a page draws and not what the television draws, which is
	   why it is a line here and not a setting of the box: a switch in
	   neutrino.conf would take the pictures off the screen in the room as
	   well. */
	ShippedRoutes shipped;
	WebFile file("logos", kWebFileHead);

	// On for a box whose file says nothing about it, which is the box every
	// upgrade arrives as.
	REQUIRE(config().channel_logos);

	const Response r = putWebserver("{\"channel_logos\":false}");
	REQUIRE(r.code == 200);

	const ::Json::Value root = parsed(r.body);
	REQUIRE_FALSE(root["webserver"]["channel_logos"].asBool());
	// Nothing about where the server answers moved, so an address a caller
	// holds is still an address.
	REQUIRE_FALSE(root["caller"]["port_moved"].asBool());
	REQUIRE(root["webserver"]["port"].asInt() == 8081);

	REQUIRE(wholeOf(file.path).find("channel_logos=false") != std::string::npos);

	/* And the server is put on it. What is in effect is read without a lock and
	   is only ever replaced while the daemon is down, so a change written and
	   not put in effect would leave the file saying one thing and the server
	   answering another until the next start. */
	REQUIRE(root["caller"]["restarting"].asBool());
	REQUIRE(r.reload_after == file.path);
}

TEST_CASE("asking for the logos a box already shows moves nothing", "[write]")
{
	ShippedRoutes shipped;
	WebFile file("logos-same", kWebFileHead + std::string("channel_logos=true\n"));

	REQUIRE(config().channel_logos);

	const Response r = putWebserver("{\"channel_logos\":true}");
	REQUIRE(r.code == 200);
	REQUIRE_FALSE(parsed(r.body)["caller"]["restarting"].asBool());
	REQUIRE(r.reload_after.empty());
}

TEST_CASE("a line about the logos this server cannot read turns them off", "[write]")
{
	/* The rule every key of this file keeps: a key the file does not name falls
	   back to the default, and a key it names and this cannot read falls back to
	   the least the key can mean, which for a switch that shows something is
	   off. */
	ShippedRoutes shipped;
	WebFile file("logos-junk", kWebFileHead + std::string("channel_logos=vielleicht\n"));

	REQUIRE_FALSE(config().channel_logos);

	const Reply r = authedGet("/api/v1/system/webserver");
	REQUIRE(r.code == 200);
	REQUIRE_FALSE(parsed(r.body)["channel_logos"].asBool());
}
