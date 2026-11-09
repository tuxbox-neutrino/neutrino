/*
 * test_control_ask.cpp - the box asks before a browser runs a command of the old interface
 *
 * Copyright (C) 2026 Thilo Graf
 *
 * License: GPL-2.0-or-later
 *
 * Driven the way nhttpd drives it: a request parsed by nhttpd's own reader,
 * handed to mod_auth's prepare hook as session_init hands it, and the access
 * file loaded as the program loads it, so the switch is read from that file
 * and written into it.
 */

#include "support/catch.hpp"
#include "support/wireserver.h"

#include "httpd/auth.h"
#include "httpd/control_ask.h"
#include "httpd/credentials.h"
#include "httpd/login_bridge.h"
#include "httpd/nhttpd_bridge.h"
#include "httpd/router.h"
#include "httpd/webconfig.h"
#include "yhook.h"
#include "ylogging.h"
#include "yrequest.h"
#include "yhttpd_mods/mod_auth.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include <netinet/in.h>
#include <poll.h>
#include <strings.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <unistd.h>

using namespace httpd;

namespace
{

const char kBrowser[] = "Mozilla/5.0 (X11; Linux x86_64; rv:140.0) Gecko/20100101 Firefox/140.0";
const char kPassword[] = "right-one";
// "owner:right-one" and "owner:wrong-one" in base64.
const char kRightBasic[] = "Basic b3duZXI6cmlnaHQtb25l";
const char kWrongBasic[] = "Basic b3duZXI6d3Jvbmctb25l";

struct Gate : CmAuth
{
	Gate(bool page_login, const std::string &let_in)
	{
		authenticate = page_login;
		no_auth_client = let_in;
	}
};

void put(const std::string &where, const std::string &text)
{
	std::FILE *f = std::fopen(where.c_str(), "w");
	REQUIRE(f != NULL);
	REQUIRE(std::fwrite(text.data(), 1, text.size(), f) == text.size());
	REQUIRE(std::fclose(f) == 0);
}

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

/* An access file of the case's own and an nhttpd.conf naming the language of the
   pages, loaded as the program loads them, so configPath() names this file. What was
   in effect comes back on the way out, the tokens and the clock with it. */
struct Box
{
	WebConfig   before;
	std::string path;
	std::string server;

	explicit Box(const std::string &more = std::string(), const char *language = "English") : before(config())
	{
		char name[96];
		std::snprintf(name, sizeof(name), "/tmp/control-ask-%d", (int) getpid());
		path = name;
		server = path + ".nhttpd";
		put(path, "username=owner\npassword_hash=" + hashSecret(kPassword, 1000) + "\n" + more);
		put(server, std::string("WebsiteMain.port=80\nLanguage.selected=") + language + "\n");
		REQUIRE(load(path, server));
		forgetAskTokensForTest();
		setAskClockForTest(0);
		forgetLoginAttemptsForTest();
	}

	~Box()
	{
		forgetAskTokensForTest();
		setAskClockForTest(0);
		forgetLoginAttemptsForTest();
		::unlink(path.c_str());
		::unlink(server.c_str());
		setConfigForTest(before);
	}

	// the switch as an editor writes it, a new file in place of the old under the running box
	void say(const std::string &line)
	{
		const std::string fresh = path + ".edit";
		put(fresh, "username=owner\npassword_hash=" + hashSecret(kPassword, 1000) + "\n" + line);
		REQUIRE(::rename(fresh.c_str(), path.c_str()) == 0);
	}
};

/* A request as nhttpd's reader hands it to the hooks: the target parsed by the reader
   itself, as written in UrlData["target"], and for a POST the form body parsed after
   the query, as HandlePost does. A browser's navigation unless told otherwise. */
struct Asked
{
	CyhookHandler hh;

	Asked(THttp_Method method, const std::string &target, const char *agent = kBrowser,
	      const std::string &peer = "192.168.1.20", const std::string &body = std::string())
	{
		CWebserverRequest r;
		r.analyzeURL(target);
		r.UrlData["target"] = target;
		r.UrlData["clientaddr"] = peer;
		CStringList params = r.ParameterList;
		if (!body.empty())
		{
			// the one parse of the query and then the body, as the reader makes it
			CWebserverRequest both;
			both.analyzeURL(target + (target.find('?') == std::string::npos ? "?" : "&") + body);
			params = both.ParameterList;
		}
		CStringList headers, config;
		headers["Host"] = "box.lan";
		headers["Accept"] = "text/html,application/xhtml+xml";
		if (agent != NULL)
			headers["User-Agent"] = agent;
		hh.session_init(params, r.UrlData, headers, config, method, false);
	}
};

std::string headerOf(const CyhookHandler &hh, const char *name)
{
	for (size_t i = 0; i < hh.ResponseHeaders.size(); i++)
		if (strcasecmp(hh.ResponseHeaders[i].first.c_str(), name) == 0)
			return hh.ResponseHeaders[i].second;
	return "";
}

// What the page carries in its form: the token, read the way a browser would read the field.
std::string tokenOf(const std::string &page)
{
	const std::string field = std::string("name=\"") + kAskTokenField + "\" value=\"";
	const size_t at = page.find(field);
	if (at == std::string::npos)
		return std::string();
	const size_t from = at + field.size();
	return page.substr(from, page.find('"', from) - from);
}

bool asked(const CyhookHandler &hh)
{
	return hh.httpStatus == HTTP_FORBIDDEN && hh.ResponseMimeType == "text/html; charset=UTF-8" &&
	       hh.yresult.find("id=\"ask-form\"") != std::string::npos;
}

// A page of the box confirming target with token, the box ticked where remember is.
Asked confirmation(const std::string &target, const std::string &token, bool remember = false,
                   const std::string &origin = "http://box.lan")
{
	std::string body = std::string(kAskTokenField) + "=" + token;
	if (remember)
		body += std::string("&") + kAskRememberField + "=true";
	Asked c(M_POST, target, kBrowser, "192.168.1.20", body);
	if (!origin.empty())
		c.hh.HeaderList["Origin"] = origin;
	return c;
}

} // namespace

TEST_CASE("a browser opening a command without a sign of the box's page is asked, and the command waits", "[ask]")
{
	Box box;
	Gate gate(false, "");

	Asked a(M_GET, "/control/standby?on");
	REQUIRE(gate.Hook_PrepareResponse(&a.hh) == HANDLED_ERROR);
	CHECK(asked(a.hh));
	const std::string &page = a.hh.yresult;
	CHECK(page.find("Switch the box to standby") != std::string::npos);
	// posted back to the address the browser opened, which the page does not repeat
	CHECK(page.find("<form id=\"ask-form\" method=\"post\" ") != std::string::npos);
	CHECK(page.find("action=") == std::string::npos);
	CHECK(tokenOf(page).size() == 32);
	CHECK(page.find(std::string("name=\"") + kAskRememberField + "\" value=\"true\"") != std::string::npos);
	CHECK(page.find("Run without asking from now on") != std::string::npos);
	CHECK(page.find("A page on another site could then run such commands on the box too.") != std::string::npos);
	CHECK(page.find("Settings \xE2\x86\x92" " Webserver \xE2\x86\x92" " Access") != std::string::npos);
	CHECK(page.find(">Run</button>") != std::string::npos);
	CHECK(page.find("href=\"/\">Cancel</a>") != std::string::npos);
	CHECK(page.find("<html lang=\"en\"") != std::string::npos);
	// a box ticked by nobody: the page does not choose for its reader
	CHECK(page.find("checked") == std::string::npos);

	// nothing frames it, nothing keeps it, nothing reads it as another type
	CHECK(a.hh.api_mode);
	CHECK(headerOf(a.hh, "X-Frame-Options") == "DENY");
	CHECK(headerOf(a.hh, "Content-Security-Policy").find("frame-ancestors 'none'") != std::string::npos);
	CHECK(headerOf(a.hh, "Content-Security-Policy").find("script-src 'sha256-") != std::string::npos);
	CHECK(headerOf(a.hh, "Cache-Control") == "no-store");
	CHECK(headerOf(a.hh, "X-Content-Type-Options") == "nosniff");

	// every way the old interface is addressed, and a template command on a page
	const char *const commands[] = {
		"/control/zapto?283d000103f2", "/cgi-bin/zapTo?path=1", "/fb/timer.dbox2", "/api/v4/standby?on",
		"/y/cache-clear", "/dashboard.html?execute=script:x",
	};
	for (size_t i = 0; i < sizeof(commands) / sizeof(commands[0]); ++i)
	{
		INFO(commands[i]);
		Asked c(M_GET, commands[i]);
		CHECK(gate.Hook_PrepareResponse(&c.hh) == HANDLED_ERROR);
		CHECK(asked(c.hh));
	}
}

TEST_CASE("a command from a page of the box, from a script, and a page are not asked", "[ask]")
{
	Box box;
	Gate gate(false, "");

	struct Case
	{
		const char *what;
		const char *agent;
		const char *header;
		const char *value;
		bool asks;
	};
	const Case cases[] = {
		{ "same origin", kBrowser, "Sec-Fetch-Site", "same-origin", false },
		{ "an Origin naming the box", kBrowser, "Origin", "http://box.lan", false },
		{ "a Referer naming the box", kBrowser, "Referer", "http://box.lan/remote.html", false },
		{ "a script", "curl/8.5.0", "", "", false },
		{ "no User-Agent at all", NULL, "", "", false },
		{ "another site", kBrowser, "Sec-Fetch-Site", "cross-site", true },
		{ "typed in the address bar", kBrowser, "Sec-Fetch-Site", "none", true },
		{ "an Origin naming another box", kBrowser, "Origin", "http://evil.example", true },
		{ "a Referer naming another site", kBrowser, "Referer", "http://evil.example/?http://box.lan/", true },
		{ "a script calling itself Mozilla", "Mozilla/5.0 (Windows NT 10.0) WindowsPowerShell/5.1", "", "", true },
	};
	for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); ++i)
	{
		INFO(cases[i].what);
		Asked a(M_GET, "/control/volume?mute", cases[i].agent);
		if (cases[i].header[0] != '\0')
			a.hh.HeaderList[cases[i].header] = cases[i].value;
		const THandleStatus st = gate.Hook_PrepareResponse(&a.hh);
		CHECK((st == HANDLED_ERROR) == cases[i].asks);
		CHECK((a.hh.httpStatus == HTTP_FORBIDDEN) == cases[i].asks);
	}

	// a page of the interface is no command
	Asked page(M_GET, "/dashboard.html");
	CHECK(gate.Hook_PrepareResponse(&page.hh) == HANDLED_CONTINUE);
}

TEST_CASE("an asked request that is no navigation gets a text and no token", "[ask]")
{
	/* A picture, a script's call or a form of another site cannot show a page, and a
	   token handed out to one is one nobody could use. They are answered with the text
	   that says where to open the address, as the Basic hold-back was. */
	Box box;
	Gate gate(false, "");

	Asked picture(M_GET, "/control/standby?on");
	picture.hh.HeaderList["Accept"] = "image/avif,image/webp,*/*";
	REQUIRE(gate.Hook_PrepareResponse(&picture.hh) == HANDLED_ERROR);
	CHECK(picture.hh.httpStatus == HTTP_FORBIDDEN);
	CHECK(picture.hh.ResponseMimeType == "text/plain; charset=UTF-8");
	CHECK(picture.hh.yresult == login_bridge::heldBackText());
	CHECK(tokenOf(picture.hh.yresult).empty());

	Asked form(M_POST, "/control/standby?on", kBrowser, "192.168.1.20", "x=1");
	form.hh.HeaderList["Origin"] = "http://evil.example";
	REQUIRE(gate.Hook_PrepareResponse(&form.hh) == HANDLED_ERROR);
	CHECK(form.hh.ResponseMimeType == "text/plain; charset=UTF-8");

	Asked head(M_HEAD, "/control/standby?on");
	REQUIRE(gate.Hook_PrepareResponse(&head.hh) == HANDLED_ERROR);
	CHECK(head.hh.ResponseMimeType == "text/plain; charset=UTF-8");

	// nor an address that is no path on this box
	Asked far(M_GET, "//evil.example/x?execute=1");
	REQUIRE(gate.Hook_PrepareResponse(&far.hh) == HANDLED_ERROR);
	CHECK(far.hh.ResponseMimeType == "text/plain; charset=UTF-8");
}

TEST_CASE("the page's confirmation runs the request it asked about, once", "[ask]")
{
	Box box;
	Gate gate(false, "");

	Asked a(M_GET, "/control/rcem?KEY_OK&delay=100");
	REQUIRE(gate.Hook_PrepareResponse(&a.hh) == HANDLED_ERROR);
	const CStringList asked_with = a.hh.ParamList;
	const std::string token = tokenOf(a.hh.yresult);
	REQUIRE(token.size() == 32);

	Asked run = confirmation("/control/rcem?KEY_OK&delay=100", token);
	REQUIRE(run.hh.ParamList.count(kAskTokenField) == 1);
	CHECK(gate.Hook_PrepareResponse(&run.hh) == HANDLED_CONTINUE);
	// the stored request and not the form: its parameters and its method
	CHECK(run.hh.ParamList == asked_with);
	CHECK(run.hh.ParamList.count(kAskTokenField) == 0);
	CHECK(run.hh.Method == M_GET);

	// the same token again is refused, and nothing runs
	Asked again = confirmation("/control/rcem?KEY_OK&delay=100", token);
	REQUIRE(gate.Hook_PrepareResponse(&again.hh) == HANDLED_ERROR);
	CHECK(again.hh.httpStatus == HTTP_FORBIDDEN);
	CHECK(again.hh.ResponseMimeType == "text/plain; charset=UTF-8");
	CHECK(again.hh.yresult == askRefusedText());
	CHECK(again.hh.yresult.find("did not run the command") != std::string::npos);
}

TEST_CASE("a confirmation that ran out, came from elsewhere or names another command runs nothing", "[ask]")
{
	Box box;
	Gate gate(false, "");
	setAskClockForTest(1000);

	struct Way
	{
		const char *what;
		const char *target;   // where the confirmation is posted
		const char *origin;   // its Origin, none where empty
		time_t later;         // seconds after the page was handed out
		bool runs;
	};
	const Way ways[] = {
		{ "in time, from the page", "/control/standby?off", "http://box.lan", kAskSeconds - 1, true },
		{ "after the token ran out", "/control/standby?off", "http://box.lan", kAskSeconds, false },
		{ "from a page of another site", "/control/standby?off", "http://evil.example", 1, false },
		{ "without anything naming the box", "/control/standby?off", "", 1, false },
		{ "posted to another command", "/control/reboot", "http://box.lan", 1, false },
		{ "posted to the same command written otherwise", "/control/standby?%6Fff", "http://box.lan", 1, false },
	};
	for (size_t i = 0; i < sizeof(ways) / sizeof(ways[0]); ++i)
	{
		INFO(ways[i].what);
		forgetAskTokensForTest();
		setAskClockForTest(1000);
		Asked a(M_GET, "/control/standby?off");
		REQUIRE(gate.Hook_PrepareResponse(&a.hh) == HANDLED_ERROR);
		const std::string token = tokenOf(a.hh.yresult);
		REQUIRE(!token.empty());

		setAskClockForTest(1000 + ways[i].later);
		Asked c = confirmation(ways[i].target, token, false, ways[i].origin);
		CHECK((gate.Hook_PrepareResponse(&c.hh) == HANDLED_CONTINUE) == ways[i].runs);

		// the token is spent either way: the page's own confirmation after it runs nothing
		Asked after = confirmation("/control/standby?off", token);
		CHECK(gate.Hook_PrepareResponse(&after.hh) == HANDLED_ERROR);
	}

	// a token the box never handed out
	Asked made_up = confirmation("/control/standby?off", std::string(32, 'a'));
	CHECK(gate.Hook_PrepareResponse(&made_up.hh) == HANDLED_ERROR);
}

TEST_CASE("the switch lets a browser run commands without asking, read again from the file", "[ask]")
{
	Box box;
	Gate gate(false, "");

	struct Line
	{
		const char *line;
		bool asks;
	};
	const Line lines[] = {
		{ "control_without_asking=true\n", false },
		{ "control_without_asking=false\n", true },
		{ "control_without_asking=1\n", false },
		{ "control_without_asking=0\n", true },
		// what cannot be read is the least the switch can mean: the box asks
		{ "control_without_asking=yes\n", true },
		{ "control_without_asking=true # off later\n", true },
		{ "Control_Without_Asking=true\n", true },
		{ "", true },
	};
	for (size_t i = 0; i < sizeof(lines) / sizeof(lines[0]); ++i)
	{
		INFO(lines[i].line);
		box.say(lines[i].line);
		CHECK(controlWithoutAsking() == !lines[i].asks);
		Asked a(M_GET, "/control/standby?on");
		CHECK((gate.Hook_PrepareResponse(&a.hh) == HANDLED_ERROR) == lines[i].asks);
	}

	// a file that is gone says nothing, and the box asks
	box.say("control_without_asking=true\n");
	REQUIRE(controlWithoutAsking());
	::unlink(box.path.c_str());
	CHECK_FALSE(controlWithoutAsking());
	Asked gone(M_GET, "/control/standby?on");
	CHECK(gate.Hook_PrepareResponse(&gone.hh) == HANDLED_ERROR);
	CHECK(asked(gone.hh));
	// and so does one that cannot be read, a directory in its place
	REQUIRE(::mkdir(box.path.c_str(), 0700) == 0);
	CHECK_FALSE(controlWithoutAsking());
	::rmdir(box.path.c_str());
}

TEST_CASE("the box remembers the choice made on its page", "[ask]")
{
	Box box;
	Gate gate(false, "");
	REQUIRE_FALSE(controlWithoutAsking());

	Asked a(M_GET, "/control/volume?mute");
	REQUIRE(gate.Hook_PrepareResponse(&a.hh) == HANDLED_ERROR);
	Asked run = confirmation("/control/volume?mute", tokenOf(a.hh.yresult), true);
	CHECK(gate.Hook_PrepareResponse(&run.hh) == HANDLED_CONTINUE);
	CHECK(run.hh.ParamList.count(kAskRememberField) == 0);

	CHECK(controlWithoutAsking());
	const std::string file = wholeOf(box.path);
	CHECK(file.find("control_without_asking=true\n") != std::string::npos);
	CHECK(file.find("username=owner\n") != std::string::npos);
	Asked next(M_GET, "/control/volume?unmute");
	CHECK(gate.Hook_PrepareResponse(&next.hh) == HANDLED_CONTINUE);

	// a confirmation that is refused writes nothing
	setControlWithoutAsking(false, NULL);
	Asked b(M_GET, "/control/volume?mute");
	REQUIRE(gate.Hook_PrepareResponse(&b.hh) == HANDLED_ERROR);
	Asked foreign = confirmation("/control/volume?mute", tokenOf(b.hh.yresult), true, "http://evil.example");
	CHECK(gate.Hook_PrepareResponse(&foreign.hh) == HANDLED_ERROR);
	CHECK_FALSE(controlWithoutAsking());
}

TEST_CASE("a link cannot fill in the fields of the page that asks", "[ask]")
{
	Box box;
	Gate gate(false, "");

	// a request naming a field of the page's form gets no page and no token
	const char *const named[] = {
		"/control/volume?control_without_asking=true",
		"/control/volume?x=1&control_without_asking=true",
		"/control/volume?%63ontrol_without_asking=true",
		"/control/volume?ask_token=0123456789abcdef0123456789abcdef",
	};
	for (size_t i = 0; i < sizeof(named) / sizeof(named[0]); ++i)
	{
		INFO(named[i]);
		Asked a(M_GET, named[i]);
		REQUIRE(gate.Hook_PrepareResponse(&a.hh) == HANDLED_ERROR);
		CHECK(a.hh.ResponseMimeType == "text/plain; charset=UTF-8");
		CHECK(tokenOf(a.hh.yresult).empty());
	}

	/* Where such a page was handed out all the same, the tick its address carries is
	   not the reader's: the form posts back to that address, so the field reaches
	   the confirmation beside the form's own. */
	const std::string target = "/control/volume?control_without_asking=true";
	Asked asked_for(M_GET, target);
	const std::string token = tokenOf(askingPage(target, "GET", asked_for.hh.UrlData["url"],
	                                             asked_for.hh.UrlData["paramstring"], asked_for.hh.ParamList));
	REQUIRE(token.size() == 32);
	Asked run = confirmation(target, token);
	REQUIRE(run.hh.ParamList[kAskRememberField] == "true");
	CHECK(gate.Hook_PrepareResponse(&run.hh) == HANDLED_CONTINUE);
	CHECK_FALSE(controlWithoutAsking());
	CHECK(wholeOf(box.path).find("control_without_asking") == std::string::npos);
}

TEST_CASE("only a navigation of the window gets the page and a token", "[ask]")
{
	/* A frame is never drawn and a picture or a fetch shows nothing, so a token
	   handed to one could only push a question somebody sees out of the table. */
	Box box;
	Gate gate(false, "");
	struct Dest
	{
		const char *value;
		bool page;
	};
	const Dest dests[] = {
		{ NULL, true }, { "document", true }, { "iframe", false }, { "frame", false }, { "object", false },
		{ "embed", false }, { "image", false }, { "empty", false }, { "script", false },
	};
	for (size_t i = 0; i < sizeof(dests) / sizeof(dests[0]); ++i)
	{
		INFO((dests[i].value ? dests[i].value : "no Sec-Fetch-Dest"));
		Asked a(M_GET, "/control/standby?on");
		if (dests[i].value != NULL)
			a.hh.HeaderList["Sec-Fetch-Dest"] = dests[i].value;
		REQUIRE(gate.Hook_PrepareResponse(&a.hh) == HANDLED_ERROR);
		CHECK(asked(a.hh) == dests[i].page);
		CHECK(tokenOf(a.hh.yresult).empty() == !dests[i].page);
	}
}

TEST_CASE("the box holds the 32 newest questions, and the oldest gives way", "[ask]")
{
	Box box;
	Gate gate(false, "");
	std::vector<std::string> tokens;
	for (int i = 0; i < 33; ++i)
	{
		Asked a(M_GET, "/control/volume?" + std::to_string(i));
		REQUIRE(gate.Hook_PrepareResponse(&a.hh) == HANDLED_ERROR);
		tokens.push_back(tokenOf(a.hh.yresult));
		REQUIRE(tokens.back().size() == 32);
	}
	// the first gave way to the 33rd
	Asked first = confirmation("/control/volume?0", tokens[0]);
	CHECK(gate.Hook_PrepareResponse(&first.hh) == HANDLED_ERROR);
	// the second is the oldest still held, the 33rd the newest
	Asked second = confirmation("/control/volume?1", tokens[1]);
	CHECK(gate.Hook_PrepareResponse(&second.hh) == HANDLED_CONTINUE);
	Asked last = confirmation("/control/volume?32", tokens[32]);
	CHECK(gate.Hook_PrepareResponse(&last.hh) == HANDLED_CONTINUE);
}

TEST_CASE("a clock that went back makes a question run out", "[ask]")
{
	Box box;
	Gate gate(false, "");
	setAskClockForTest(5000);
	Asked a(M_GET, "/control/standby?on");
	REQUIRE(gate.Hook_PrepareResponse(&a.hh) == HANDLED_ERROR);
	setAskClockForTest(4999);
	Asked c = confirmation("/control/standby?on", tokenOf(a.hh.yresult));
	CHECK(gate.Hook_PrepareResponse(&c.hh) == HANDLED_ERROR);
}

TEST_CASE("a change to the account is never asked about", "[ask]")
{
	/* It is taken only from a page of the box that made it, and the page that asks
	   could only show the new password as hidden. */
	const char *const changes[] = {
		"/control/config?config=nhttpd&action=submit&mod_auth_dot_password=chosen-elsewhere",
		"/control/config?config=nhttpd&action=submit&mod_auth.username=someone",
		"/cgi-bin/config?config=nhttpd&action=submit&mod_auth_dot_password=chosen-elsewhere",
		"/api/v4/config?config=nhttpd&action=submit&mod_auth_dot_password=chosen-elsewhere",
		"/control/Config?config=nhttpd&action=submit&mod_auth_dot_username=someone",
	};
	for (int login = 0; login < 2; ++login)
	{
		INFO("page login " << (login ? "on" : "off"));
		Box box;
		Gate gate(login == 1, "");
		const std::string session = openSessionForTest("owner");
		REQUIRE(!session.empty());
		const std::string cookie = std::string(sessionCookieName()) + "=" + session;
		for (size_t i = 0; i < sizeof(changes) / sizeof(changes[0]); ++i)
		{
			INFO(changes[i]);
			Asked a(M_GET, changes[i], kBrowser, "203.0.113.37");
			a.hh.HeaderList["Cookie"] = cookie;
			REQUIRE(gate.Hook_PrepareResponse(&a.hh) == HANDLED_ERROR);
			CHECK(a.hh.httpStatus == HTTP_FORBIDDEN);
			CHECK(a.hh.ResponseMimeType == "text/plain; charset=UTF-8");
			CHECK(tokenOf(a.hh.yresult).empty());
		}
		// the same file without the account, or an empty field, is asked about as any command
		const char *const others[] = {
			"/control/config?config=nhttpd&action=submit&WebsiteMain_dot_port=8080",
			"/control/config?config=nhttpd&action=submit&mod_auth_dot_password=",
		};
		for (size_t i = 0; i < sizeof(others) / sizeof(others[0]); ++i)
		{
			INFO(others[i]);
			Asked a(M_GET, others[i], kBrowser, "203.0.113.37");
			a.hh.HeaderList["Cookie"] = cookie;
			REQUIRE(gate.Hook_PrepareResponse(&a.hh) == HANDLED_ERROR);
			CHECK(asked(a.hh));
		}
		login_bridge::closeSessionOf(cookie);
	}
}

TEST_CASE("a request run on a confirmation is no request of a page of the box", "[ask]")
{
	/* Where a page for a change to the account was handed out all the same, the request
	   run on its confirmation carries the headers of the box's own POST, and the account
	   check that trusts them must not take it for one of the box's pages. */
	const std::string target = "/control/config?config=nhttpd&action=submit&mod_auth_dot_password=chosen-elsewhere";
	for (int login = 0; login < 2; ++login)
	{
		INFO("page login " << (login ? "on" : "off"));
		Box box;
		Gate gate(login == 1, "");
		const std::string session = openSessionForTest("owner");
		REQUIRE(!session.empty());
		const std::string cookie = std::string(sessionCookieName()) + "=" + session;

		Asked asked_for(M_GET, target);
		const std::string token = tokenOf(askingPage(target, "GET", asked_for.hh.UrlData["url"],
		                                             asked_for.hh.UrlData["paramstring"], asked_for.hh.ParamList));
		REQUIRE(token.size() == 32);
		Asked run = confirmation(target, token);
		run.hh.HeaderList["Cookie"] = cookie;
		REQUIRE(gate.Hook_PrepareResponse(&run.hh) == HANDLED_CONTINUE);
		const login_bridge::Ask replayed = CmAuth::AskOf(&run.hh);
		CHECK(replayed.replayed);
		CHECK_FALSE(login_bridge::fromThisBox(replayed));
		std::string why;
		CHECK_FALSE(login_bridge::mayChange(replayed, "", "chosen-elsewhere", &why));
		CHECK(why == "the account is changed only from a page of this box");

		// a POST that a page of the box made itself is one of its pages
		Asked own(M_POST, "/control/config?config=nhttpd&action=submit", kBrowser, "192.168.1.20", "x=1");
		own.hh.HeaderList["Origin"] = "http://box.lan";
		own.hh.HeaderList["Cookie"] = cookie;
		REQUIRE(gate.Hook_PrepareResponse(&own.hh) == HANDLED_CONTINUE);
		CHECK_FALSE(CmAuth::AskOf(&own.hh).replayed);
		CHECK(login_bridge::fromThisBox(CmAuth::AskOf(&own.hh)));
		login_bridge::closeSessionOf(cookie);
	}
}

TEST_CASE("a script that holds a session is not asked about a command", "[ask]")
{
	Box box;
	Gate gate(true, "");
	const std::string session = openSessionForTest("owner");
	REQUIRE(!session.empty());
	const std::string cookie = std::string(sessionCookieName()) + "=" + session;
	Asked script(M_GET, "/control/standby?on", "curl/8.5.0", "203.0.113.36");
	script.hh.HeaderList["Cookie"] = cookie;
	REQUIRE(gate.Hook_PrepareResponse(&script.hh) == HANDLED_ERROR);
	CHECK_FALSE(asked(script.hh));
	CHECK(script.hh.httpStatus == HTTP_UNAUTHORIZED);
	login_bridge::closeSessionOf(cookie);
}

TEST_CASE("with the page login on the box asks the browsers it lets in, and those held back for where they came from", "[ask]")
{
	Box box;
	Gate gate(true, "198.51.100.");

	// an address let in without the login, and the box itself
	const char *const exempt[] = { "198.51.100.7", "127.0.0.1" };
	for (size_t i = 0; i < sizeof(exempt) / sizeof(exempt[0]); ++i)
	{
		INFO(exempt[i]);
		Asked a(M_GET, "/control/standby?on", kBrowser, exempt[i]);
		REQUIRE(gate.Hook_PrepareResponse(&a.hh) == HANDLED_ERROR);
		CHECK(asked(a.hh));
		Asked script(M_GET, "/control/standby?on", "curl/8.5.0", exempt[i]);
		CHECK(gate.Hook_PrepareResponse(&script.hh) == HANDLED_CONTINUE);
	}

	// a browser's Basic pair from elsewhere: the page in place of the hold-back's text
	Asked basic(M_GET, "/control/standby?on", kBrowser, "203.0.113.31");
	basic.hh.HeaderList["Authorization"] = kRightBasic;
	REQUIRE(gate.Hook_PrepareResponse(&basic.hh) == HANDLED_ERROR);
	CHECK(asked(basic.hh));
	// confirmed from the page, which sends the pair along, it runs; a wrong pair does not
	Asked basic_run = confirmation("/control/standby?on", tokenOf(basic.hh.yresult));
	basic_run.hh.UrlData["clientaddr"] = "203.0.113.31";
	basic_run.hh.HeaderList["Authorization"] = kRightBasic;
	CHECK(gate.Hook_PrepareResponse(&basic_run.hh) == HANDLED_CONTINUE);
	Asked wrong(M_GET, "/control/standby?on", kBrowser, "203.0.113.32");
	wrong.hh.HeaderList["Authorization"] = kWrongBasic;
	REQUIRE(gate.Hook_PrepareResponse(&wrong.hh) == HANDLED_ERROR);
	Asked wrong_run = confirmation("/control/standby?on", tokenOf(wrong.hh.yresult));
	wrong_run.hh.UrlData["clientaddr"] = "203.0.113.32";
	wrong_run.hh.HeaderList["Authorization"] = kWrongBasic;
	CHECK(gate.Hook_PrepareResponse(&wrong_run.hh) != HANDLED_CONTINUE);

	// a browser that is signed in: the page in place of the sign-in it already did
	const std::string session = openSessionForTest("owner");
	REQUIRE(!session.empty());
	const std::string cookie = std::string(sessionCookieName()) + "=" + session;
	Asked signed_in(M_GET, "/control/standby?on", kBrowser, "203.0.113.33");
	signed_in.hh.HeaderList["Cookie"] = cookie;
	REQUIRE(gate.Hook_PrepareResponse(&signed_in.hh) == HANDLED_ERROR);
	CHECK(asked(signed_in.hh));
	Asked session_run = confirmation("/control/standby?on", tokenOf(signed_in.hh.yresult));
	session_run.hh.UrlData["clientaddr"] = "203.0.113.33";
	session_run.hh.HeaderList["Cookie"] = cookie;
	CHECK(gate.Hook_PrepareResponse(&session_run.hh) == HANDLED_CONTINUE);

	// a browser holding nothing is asked to sign in, as before (here, without a sign-in page, by 401)
	Asked nobody(M_GET, "/control/standby?on", kBrowser, "203.0.113.34");
	CHECK(gate.Hook_PrepareResponse(&nobody.hh) == HANDLED_ERROR);
	CHECK(nobody.hh.httpStatus == HTTP_UNAUTHORIZED);

	// with the switch on the box asks none of them, and the pair and the session count as a script's do
	box.say("control_without_asking=true\n");
	Asked basic_on(M_GET, "/control/standby?on", kBrowser, "203.0.113.31");
	basic_on.hh.HeaderList["Authorization"] = kRightBasic;
	CHECK(gate.Hook_PrepareResponse(&basic_on.hh) == HANDLED_CONTINUE);
	Asked signed_on(M_GET, "/control/standby?on", kBrowser, "203.0.113.33");
	signed_on.hh.HeaderList["Cookie"] = cookie;
	CHECK(gate.Hook_PrepareResponse(&signed_on.hh) == HANDLED_CONTINUE);
	Asked exempt_on(M_GET, "/control/standby?on", kBrowser, "198.51.100.7");
	CHECK(gate.Hook_PrepareResponse(&exempt_on.hh) == HANDLED_CONTINUE);
	// and the account is still changed only from a page of the box
	login_bridge::Ask account;
	account.path = "/control/";
	account.peer = "203.0.113.31";
	account.host = "box.lan";
	account.authorization = kRightBasic;
	account.user_agent = kBrowser;
	std::string why;
	CHECK_FALSE(login_bridge::mayChange(account, "", "a-new-password", &why));

	// a session that ended is none: asked to sign in, not asked about the command
	box.say("");
	login_bridge::closeSessionOf(cookie);
	Asked ended(M_GET, "/control/standby?on", kBrowser, "203.0.113.35");
	ended.hh.HeaderList["Cookie"] = cookie;
	CHECK(gate.Hook_PrepareResponse(&ended.hh) == HANDLED_ERROR);
	CHECK(ended.hh.httpStatus == HTTP_UNAUTHORIZED);
}

TEST_CASE("the page names the command in plain words, leaves secrets out and speaks the pages' language", "[ask]")
{
	struct Named
	{
		const char *target;
		const char *says;
	};
	const Named named[] = {
		{ "/control/standby?off", "Wake the box from standby" },
		{ "/control/standby", "Tell whether the box is in standby" },
		{ "/control/shutdown", "Shut the box down" },
		{ "/control/reboot", "Restart the box" },
		{ "/control/zapto?283d000103f2", "Switch to another channel" },
		{ "/api/v4/rcem?KEY_POWER", "Press a key of the remote control: KEY_POWER" },
		{ "/control/volume?40", "Set the volume to 40" },
		{ "/control/volume?mute", "Mute the sound" },
		{ "/cgi-bin/setmode?radio", "Switch to radio" },
		{ "/control/message?popup=Hallo", "Show a message on the TV" },
		{ "/control/exec?backup.sh", "Run the script backup.sh on the box" },
		{ "/control/config?config=nhttpd&action=submit&WebsiteMain_dot_port=8080",
		  "Change the settings of the web server" },
		{ "/control/config?config=neutrino&action=submit&timer_remotebox_pass_0=s3cret",
		  "Change settings of the box" },
	};
	Box box;
	Gate gate(false, "");
	for (size_t i = 0; i < sizeof(named) / sizeof(named[0]); ++i)
	{
		INFO(named[i].target);
		Asked a(M_GET, named[i].target);
		REQUIRE(gate.Hook_PrepareResponse(&a.hh) == HANDLED_ERROR);
		CHECK(a.hh.yresult.find(std::string(">") + named[i].says + "</p>") != std::string::npos);
	}

	// a secret in the address stands as hidden, wherever the page shows the address
	Asked secret(M_GET, "/control/config?config=neutrino&action=submit&timer_remotebox_pass_0=s3cret");
	REQUIRE(gate.Hook_PrepareResponse(&secret.hh) == HANDLED_ERROR);
	CHECK(secret.hh.yresult.find("timer_remotebox_pass_0=[hidden]") != std::string::npos);
	CHECK(secret.hh.yresult.find("s3cret") == std::string::npos);
	CHECK(secret.hh.yresult.find("<code>/control/config?config=neutrino&amp;action=submit&amp;"
	                             "timer_remotebox_pass_0=[hidden]</code>") != std::string::npos);

	// a command it does not know stands as its path, a key's value hidden too
	Asked unknown(M_GET, "/control/getbouquets?format=json&api_key=xyz&x=%3Cb%3E");
	REQUIRE(gate.Hook_PrepareResponse(&unknown.hh) == HANDLED_ERROR);
	CHECK(unknown.hh.yresult.find("<code>/control/getbouquets?format=json&amp;api_key=[hidden]&amp;x=&lt;b&gt;</code>")
	      != std::string::npos);
	CHECK(unknown.hh.yresult.find("xyz") == std::string::npos);
	CHECK(unknown.hh.yresult.find("<b>") == std::string::npos);
}

TEST_CASE("the page hides a value by the rule the web log reads parameter names by", "[ask]")
{
	/* One rule for both (ylog::secretParam): the value of a parameter a log line
	   redacts is hidden on the page, and the value of any other is shown. */
	struct Name
	{
		const char *name;
		bool        secret;
	};
	const Name names[] = {
		// a name that holds one of the words, or ends as an API key's name does
		{ "password", true }, { "mod_auth_dot_password", true }, { "ci_pincode_0", true },
		{ "token", true }, { "ticket", true }, { "client_secret", true }, { "tmdb_api_key", true },
		{ "youtube_dev_id", true }, { "timer_remotebox_pass_3", true },
		// the whole names: the current password a change of the account is asked
		// with, and the short ones a login form may use
		{ "current", true }, { "pass", true }, { "pw", true }, { "pwd", true }, { "pin", true },
		// names that only look like one
		{ "ac3_pass", false }, { "timer_remotebox_pass_x", false }, { "mode", false },
	};
	Box box;
	Gate gate(false, "");
	for (size_t i = 0; i < sizeof(names) / sizeof(names[0]); ++i)
	{
		INFO(names[i].name);
		REQUIRE(ylog::secretParam(names[i].name) == names[i].secret);
		Asked a(M_GET, std::string("/control/foo?") + names[i].name + "=S3NT1NEL");
		REQUIRE(gate.Hook_PrepareResponse(&a.hh) == HANDLED_ERROR);
		CHECK((a.hh.yresult.find(std::string(names[i].name) + "=[hidden]") != std::string::npos) ==
		      names[i].secret);
		CHECK((a.hh.yresult.find("S3NT1NEL") == std::string::npos) == names[i].secret);
	}
}

TEST_CASE("the page shows the whole address, decoded once, without what hides or turns text", "[ask]")
{
	Box box;
	Gate gate(false, "");

	// whole: a parameter far down the address is still there
	Asked whole(M_GET, "/control/foo?x=" + std::string(300, 'a') + "&last=1");
	REQUIRE(gate.Hook_PrepareResponse(&whole.hh) == HANDLED_ERROR);
	CHECK(whole.hh.yresult.find("&amp;last=1</code>") != std::string::npos);
	// and the plain words name the whole value as well
	const std::string script = std::string(100, 'b') + ".sh";
	Asked named(M_GET, "/control/exec?" + script);
	REQUIRE(gate.Hook_PrepareResponse(&named.hh) == HANDLED_ERROR);
	CHECK(named.hh.yresult.find("Run the script " + script + " on the box") != std::string::npos);

	// the path as the reader decoded it, and not once more
	Asked once(M_GET, "/control/a%2541");
	REQUIRE(gate.Hook_PrepareResponse(&once.hh) == HANDLED_ERROR);
	CHECK(once.hh.yresult.find("<code>/control/a%41</code>") != std::string::npos);

	// a character that turns the text around it, or shows nothing, is not passed on
	Asked turned(M_GET, "/control/exec?%E2%80%AEtxt.exe&y=%E2%81%A6x%E2%80%8B%C2%85");
	REQUIRE(gate.Hook_PrepareResponse(&turned.hh) == HANDLED_ERROR);
	CHECK(turned.hh.yresult.find("\xE2\x80\xAE") == std::string::npos);
	CHECK(turned.hh.yresult.find("\xE2\x81\xA6") == std::string::npos);
	CHECK(turned.hh.yresult.find("\xE2\x80\x8B") == std::string::npos);
	CHECK(turned.hh.yresult.find("\xC2\x85") == std::string::npos);
	CHECK(turned.hh.yresult.find("Run the script ?txt.exe on the box") != std::string::npos);
	CHECK(turned.hh.yresult.find("y=?x?" "?</code>") != std::string::npos);

	// an address as a value: its user and password, and its query's values, as the web log leaves them out
	Asked link(M_GET, "/control/foo?vlc_link=http%3A%2F%2Fviewer%3Aletmein%40tv.example%2Fs%3Fk%3Dv");
	REQUIRE(gate.Hook_PrepareResponse(&link.hh) == HANDLED_ERROR);
	CHECK(link.hh.yresult.find("letmein") == std::string::npos);
	CHECK(link.hh.yresult.find("viewer") == std::string::npos);
	CHECK(link.hh.yresult.find("vlc_link=http://[hidden]@tv.example/s?k=</code>") != std::string::npos);
}

TEST_CASE("the page that asks is in German where the pages are", "[ask]")
{
	Box box("", "Deutsch");
	Gate gate(false, "");
	Asked a(M_GET, "/control/standby?on");
	REQUIRE(gate.Hook_PrepareResponse(&a.hh) == HANDLED_ERROR);
	const std::string &page = a.hh.yresult;
	CHECK(page.find("<html lang=\"de\"") != std::string::npos);
	CHECK(page.find("Diesen Befehl ausf\xC3\xBC" "hren?") != std::string::npos);
	CHECK(page.find("Die Box in den Standby schalten") != std::string::npos);
	CHECK(page.find("K\xC3\xBC" "nftig ohne R\xC3\xBC" "ckfrage ausf\xC3\xBC" "hren") != std::string::npos);
	CHECK(page.find("Dann k\xC3\xB6" "nnte auch eine Seite einer anderen Website") != std::string::npos);
	CHECK(page.find("Einstellungen \xE2\x86\x92" " Webserver \xE2\x86\x92" " Zugriff") != std::string::npos);
	CHECK(page.find(">Ausf\xC3\xBC" "hren</button>") != std::string::npos);
	CHECK(page.find(">Abbrechen</a>") != std::string::npos);
	CHECK(page.find("Nur ausf\xC3\xBC" "hren, wenn diese Adresse absichtlich ge\xC3\xB6" "ffnet wurde.") != std::string::npos);
	CHECK(page.find("data-ran=\"Die Box hat den Befehl ausgef\xC3\xBC" "hrt\"") != std::string::npos);
	CHECK(page.find("data-notran=\"Die Box hat den Befehl nicht ausgef\xC3\xBC" "hrt\"") != std::string::npos);
	CHECK(askRefusedText().find("nicht ausgef\xC3\xBC" "hrt") != std::string::npos);
	// addressing nobody, as the web interface's German does, the text beside the page too
	const std::string held = login_bridge::heldBackText();
	CHECK(held.find("\xC3\x96" "ffne ") == std::string::npos);
	CHECK(held.find("Gib ") == std::string::npos);
	CHECK(held.find("aus der Weboberfl\xC3\xA4" "che der Box heraus \xC3\xB6" "ffnen") != std::string::npos);
}

namespace
{

struct Attached
{
	CApiBridge bridge;
	Attached()
	{
		CyhookHandler::clearApiPrefixes();
		CApiBridge::registerPrefixes();
	}
	~Attached()
	{
		CyhookHandler::clearApiPrefixes();
		setPasswordOwed(NULL);
	}
};

// A request to the API as the bridge gets it, from off the home network.
CyhookHandler apiRequest(THttp_Method method, const std::string &target, const std::string &body = "")
{
	CyhookHandler hh;
	CStringList params, url, headers, config;
	url["target"] = target;
	url["url"] = target.substr(0, target.find('?'));
	url["clientaddr"] = "203.0.113.9";
	headers["Host"] = "box.lan";
	headers["Content-Type"] = "application/json";
	hh.session_init(params, url, headers, config, method, false);
	hh.RawBody = body;
	return hh;
}

} // namespace

TEST_CASE("the switch is written over the API only with System and the second token of a session", "[ask][bridge]")
{
	Box box;
	Attached a;
	const std::string session = openSessionForTest("owner");
	REQUIRE(!session.empty());
	const std::string csrf = csrfFor(session);
	const std::string cookie = std::string(sessionCookieName()) + "=" + session;
	const std::string before = wholeOf(box.path);

	// the cookie alone is what another site can have a browser send
	CyhookHandler bare = apiRequest(M_PUT, "/api/v1/system/webserver", "{\"control_without_asking\":true}");
	bare.HeaderList["Cookie"] = cookie;
	REQUIRE(a.bridge.Hook_SendResponse(&bare) == HANDLED_READY);
	CHECK(bare.httpStatus == HTTP_FORBIDDEN);
	CHECK(wholeOf(box.path) == before);

	// a token below System
	const std::string writer = "abcdef0123456789abcdef0123456789";
	addApiToken(tokenLookupPrefix(writer), hashSecret(writer), AuthLevel::Write, "", 0);
	CyhookHandler low = apiRequest(M_PUT, "/api/v1/system/webserver", "{\"control_without_asking\":true}");
	low.HeaderList["Authorization"] = "Bearer " + writer;
	REQUIRE(a.bridge.Hook_SendResponse(&low) == HANDLED_READY);
	CHECK(low.httpStatus == HTTP_FORBIDDEN);
	CHECK(wholeOf(box.path) == before);
	forgetApiTokens();

	// the session with its second token writes it, at once and without a restart
	CyhookHandler both = apiRequest(M_PUT, "/api/v1/system/webserver", "{\"control_without_asking\":true}");
	both.HeaderList["Cookie"] = cookie;
	both.HeaderList[csrfHeaderName()] = csrf;
	REQUIRE(a.bridge.Hook_SendResponse(&both) == HANDLED_READY);
	CHECK(both.httpStatus == HTTP_OK);
	CHECK(both.yresult.find("\"control_without_asking\":true") != std::string::npos);
	CHECK(both.yresult.find("\"restarting\":false") != std::string::npos);
	CHECK(both.ReloadAfter.empty());
	CHECK(wholeOf(box.path).find("control_without_asking=true\n") != std::string::npos);
	CHECK(controlWithoutAsking());

	// read back, and turned off again
	CyhookHandler read = apiRequest(M_GET, "/api/v1/system/webserver");
	read.HeaderList["Cookie"] = cookie;
	REQUIRE(a.bridge.Hook_SendResponse(&read) == HANDLED_READY);
	CHECK(read.yresult.find("\"control_without_asking\":true") != std::string::npos);
	CyhookHandler off = apiRequest(M_PUT, "/api/v1/system/webserver", "{\"control_without_asking\":false}");
	off.HeaderList["Cookie"] = cookie;
	off.HeaderList[csrfHeaderName()] = csrf;
	REQUIRE(a.bridge.Hook_SendResponse(&off) == HANDLED_READY);
	CHECK(off.httpStatus == HTTP_OK);
	CHECK_FALSE(controlWithoutAsking());
	CHECK(wholeOf(box.path).find("control_without_asking=false\n") != std::string::npos);
	closeSession(session);
}

TEST_CASE("the API puts the switch back where the rest of a write is refused, and writes it alone on its own", "[ask][bridge]")
{
	// a line the save of the other members takes out, and a write of the switch alone leaves
	Box box("port=8080\n");
	Attached a;
	const std::string session = openSessionForTest("owner");
	REQUIRE(!session.empty());
	const std::string csrf = csrfFor(session);
	const std::string cookie = std::string(sessionCookieName()) + "=" + session;

	CyhookHandler refused = apiRequest(M_PUT, "/api/v1/system/webserver",
	                                   "{\"control_without_asking\":true,\"bind\":\"::1\"}");
	refused.HeaderList["Cookie"] = cookie;
	refused.HeaderList[csrfHeaderName()] = csrf;
	REQUIRE(a.bridge.Hook_SendResponse(&refused) == HANDLED_READY);
	CHECK(refused.httpStatus == HTTP_BAD_REQUEST);
	CHECK_FALSE(controlWithoutAsking());
	CHECK(wholeOf(box.path).find("control_without_asking=true") == std::string::npos);

	CyhookHandler alone = apiRequest(M_PUT, "/api/v1/system/webserver", "{\"control_without_asking\":true}");
	alone.HeaderList["Cookie"] = cookie;
	alone.HeaderList[csrfHeaderName()] = csrf;
	REQUIRE(a.bridge.Hook_SendResponse(&alone) == HANDLED_READY);
	CHECK(alone.httpStatus == HTTP_OK);
	CHECK(alone.yresult.find("\"restarting\":false") != std::string::npos);
	CHECK(alone.ReloadAfter.empty());
	CHECK(controlWithoutAsking());
	CHECK(wholeOf(box.path).find("port=8080\n") != std::string::npos);

	// a new password still waiting for its restart: the switch alone arms no restart of its own
	CyhookHandler password = apiRequest(M_PUT, "/api/v1/system/webserver", std::string("{\"password\":\"another-right-one\",\"current\":\"") + kPassword + "\"}");
	password.HeaderList["Cookie"] = cookie;
	password.HeaderList[csrfHeaderName()] = csrf;
	REQUIRE(a.bridge.Hook_SendResponse(&password) == HANDLED_READY);
	REQUIRE(password.httpStatus == HTTP_OK);
	REQUIRE(!password.ReloadAfter.empty());
	REQUIRE(newPasswordAwaitsLoad());
	const std::string again = openSessionForTest("owner");
	REQUIRE(!again.empty());
	CyhookHandler later = apiRequest(M_PUT, "/api/v1/system/webserver", "{\"control_without_asking\":false}");
	later.HeaderList["Cookie"] = std::string(sessionCookieName()) + "=" + again;
	later.HeaderList[csrfHeaderName()] = csrfFor(again);
	REQUIRE(a.bridge.Hook_SendResponse(&later) == HANDLED_READY);
	CHECK(later.httpStatus == HTTP_OK);
	CHECK(later.ReloadAfter.empty());
	CHECK_FALSE(controlWithoutAsking());
	closeSession(again);
	closeSession(session);
}

TEST_CASE("the switch is written in place of every line naming it, the rest of the file as it stands", "[ask][webconf]")
{
	Box box("# kept\nlan_read=10.0.0.0/8\nControl_Without_Asking=yes\ncontrol_without_asking=false\n"
	        "control_without_asking=true\n");
	{
		AccountLock held;
		REQUIRE(writeControlWithoutAsking(box.path, true));
	}
	const std::string file = wholeOf(box.path);
	CHECK(file.find("# kept\nlan_read=10.0.0.0/8\ncontrol_without_asking=true\n") != std::string::npos);
	size_t lines = 0;
	for (size_t at = file.find("ithout_asking"); at != std::string::npos; at = file.find("ithout_asking", at + 1))
		lines++;
	CHECK(lines == 1);
	CHECK(file.find("username=owner\n") == 0);

	// a link is replaced by nothing
	const std::string link = box.path + ".link";
	REQUIRE(::symlink(box.path.c_str(), link.c_str()) == 0);
	{
		AccountLock held;
		CHECK_FALSE(writeControlWithoutAsking(link, false));
	}
	::unlink(link.c_str());
	CHECK(wholeOf(box.path) == file);
}

TEST_CASE("an access file that can no longer be read counts as off, its size and times the same", "[ask]")
{
	// root reads a file whatever its mode says, so there is nothing to lose
	if (::geteuid() == 0)
	{
		WARN("run as root: a mode takes no read away");
		return;
	}
	Box box("control_without_asking=true\n");
	Gate gate(false, "");
	REQUIRE(controlWithoutAsking());
	struct stat was;
	REQUIRE(::stat(box.path.c_str(), &was) == 0);

	REQUIRE(::chmod(box.path.c_str(), 0) == 0);
	struct stat now;
	REQUIRE(::stat(box.path.c_str(), &now) == 0);
	REQUIRE(now.st_ino == was.st_ino);
	REQUIRE(now.st_size == was.st_size);
	REQUIRE(now.st_mtim.tv_sec == was.st_mtim.tv_sec);
	REQUIRE(now.st_mtim.tv_nsec == was.st_mtim.tv_nsec);
	CHECK_FALSE(controlWithoutAsking());
	Asked a(M_GET, "/control/standby?on");
	CHECK(gate.Hook_PrepareResponse(&a.hh) == HANDLED_ERROR);
	CHECK(asked(a.hh));

	// readable again, and read again
	REQUIRE(::chmod(box.path.c_str(), 0600) == 0);
	CHECK(controlWithoutAsking());
}

namespace
{

/* A request written in two pieces, the rest after a pause, as a slow network or a
   proxy that passes on what it has can deliver one. Whether anything was answered
   during the pause goes to early; the answer, whole, is returned. */
std::string sentInTwo(int port, const std::string &first, const std::string &rest, int pause_ms, bool *early)
{
	const int fd = socket(AF_INET, SOCK_STREAM, 0);
	REQUIRE(fd >= 0);
	struct sockaddr_in a;
	std::memset(&a, 0, sizeof(a));
	a.sin_family = AF_INET;
	a.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
	a.sin_port = htons((uint16_t) port);
	REQUIRE(connect(fd, (struct sockaddr *) &a, sizeof(a)) == 0);
	REQUIRE(send(fd, first.data(), first.size(), MSG_NOSIGNAL) == (ssize_t) first.size());
	struct pollfd p;
	p.fd = fd;
	p.events = POLLIN;
	p.revents = 0;
	*early = poll(&p, 1, pause_ms) > 0;
	send(fd, rest.data(), rest.size(), MSG_NOSIGNAL);
	std::string answer;
	const int64_t until = wireNowMs() + 5000;
	for (;;)
	{
		const int64_t left = until - wireNowMs();
		if (left <= 0)
			break;
		p.revents = 0;
		if (poll(&p, 1, (int) left) <= 0)
			break;
		char buf[4096];
		const ssize_t n = recv(fd, buf, sizeof(buf), 0);
		if (n <= 0)
			break;
		answer.append(buf, (size_t) n);
	}
	close(fd);
	return answer;
}

// The code of an answer's status line.
int statusOf(const std::string &answer)
{
	return answer.size() > 12 ? (int) std::strtol(answer.c_str() + 9, NULL, 10) : 0;
}

} // namespace

TEST_CASE("a header that arrives in pieces is read whole before anything is decided on it", "[ask][nhttpd]")
{
	/* Through the reader and the hooks as the server runs them, without connection
	   threads. The part with the browser's name comes after a pause: nothing is
	   answered before it is in, and then the box asks. A second Host that comes late
	   is a second Host all the same. */
	Box box;
	Gate gate(false, "");
	WireServer serving(1, 5000, WireServer::Serial, &gate);
	const std::string head = "GET /control/volume?30 HTTP/1.1\r\nHost: box\r\n";
	const std::string browser = std::string("User-Agent: ") + kBrowser + "\r\nAccept: text/html\r\n"
		"Sec-Fetch-Dest: document\r\n\r\n";

	bool early = true;
	const std::string asked_page = sentInTwo(serving.port, head, browser, 400, &early);
	CHECK_FALSE(early);
	CHECK(statusOf(asked_page) == 403);
	CHECK(asked_page.find("id=\"ask-form\"") != std::string::npos);

	early = true;
	const std::string twice = sentInTwo(serving.port, head, "Host: evil.example\r\n" + browser, 400, &early);
	CHECK_FALSE(early);
	CHECK(statusOf(twice) == 400);
	CHECK(twice.find("allowed_hosts") != std::string::npos);

	// a line that ends in a LF alone, as the old interface always took it
	early = true;
	const std::string lf = sentInTwo(serving.port, "GET /control/volume?30 HTTP/1.0\nHost: box\n",
		std::string("User-Agent: ") + kBrowser + "\nAccept: text/html\n\n", 200, &early);
	CHECK_FALSE(early);
	CHECK(statusOf(lf) == 403);
	CHECK(lf.find("id=\"ask-form\"") != std::string::npos);
}

TEST_CASE("a header that is not one is refused before any hook, on the old interface and the API alike", "[ask][nhttpd]")
{
	/* Through the reader and the hooks as the server runs them. A line that continues
	   the one before it, a space before the colon and a control character are not read
	   as some other header: the reader answers 400 itself, with no body, and nothing
	   behind it sees the request. */
	Box box;
	Gate gate(false, "");
	WireServer serving(1, 5000, WireServer::Serial, &gate);
	const std::string ua = std::string("User-Agent: ") + kBrowser + "\r\n";
	struct Head
	{
		const char *what;
		std::string sent;
	};
	const Head refused[] = {
		{ "a User-Agent folded onto the next line", "GET /control/volume?30 HTTP/1.1\r\nHost: box\r\nUser-Agent:\r\n " +
			std::string(kBrowser) + "\r\nAccept: text/html\r\n\r\n" },
		{ "a Host folded onto the next line", "GET /control/volume?30 HTTP/1.1\r\nHost: box\r\n\tevil.example\r\n" + ua +
			"Accept: text/html\r\n\r\n" },
		{ "a space before the colon", "GET /control/volume?30 HTTP/1.1\r\nHost: box\r\nHost : evil.example\r\n" + ua + "\r\n" },
		{ "only a space before the colon", "GET /control/volume?30 HTTP/1.1\r\nHost : evil.example\r\n" + ua + "\r\n" },
		{ "a line without a colon", "GET /control/volume?30 HTTP/1.1\r\nHost: box\r\nMozilla\r\n" + ua + "\r\n" },
		{ "a control character in a value", "GET /control/volume?30 HTTP/1.1\r\nHost: box\r\nUser-Agent: Mozilla/5.0\x01x\r\n\r\n" },
		{ "a CR alone in a value", "GET /control/volume?30 HTTP/1.1\r\nHost: box\rX: y\r\n" + ua + "\r\n" },
		{ "a name with a byte no name has", "GET /control/volume?30 HTTP/1.1\r\nHost: box\r\nUser\x7f-Agent: x\r\n\r\n" },
		{ "the same on the API", "GET /api/v1/session HTTP/1.0\r\nHost: box\r\nX-A: a\r\n folded\r\n\r\n" },
		{ "a space before the colon on the API", "GET /api/v1/session HTTP/1.0\r\nHost : evil.example\r\n\r\n" },
		{ "a folded line on a POST", "POST /control/volume HTTP/1.1\r\nHost: box\r\nContent-Type: application/x-www-form-urlencoded\r\n"
			"Content-Length: 4\r\n folded\r\n\r\nx=30" },
	};
	for (size_t i = 0; i < sizeof(refused) / sizeof(refused[0]); ++i)
	{
		INFO(refused[i].what);
		bool early = true;
		const std::string answer = sentInTwo(serving.port, refused[i].sent, "", 0, &early);
		CHECK(statusOf(answer) == 400);
		// the reader's own refusal, a status line and headers: not the boundary's page
		CHECK(answer.find("allowed_hosts") == std::string::npos);
		CHECK(answer.find("ask-form") == std::string::npos);
	}

	// a tab in a value, and a byte above ASCII, are what a value may carry
	bool early = true;
	const std::string tab = sentInTwo(serving.port, std::string("GET /control/volume?30 HTTP/1.1\r\nHost: box\r\nUser-Agent:\t") +
		kBrowser + "\r\nX-Name: M\xC3\xBCller\r\nAccept: text/html\r\n\r\n", "", 0, &early);
	CHECK(statusOf(tab) == 403);
	CHECK(tab.find("id=\"ask-form\"") != std::string::npos);
}

TEST_CASE("a field that says where a request came from, named twice, is refused before any hook", "[ask][nhttpd]")
{
	/* The reader keeps one value of a field, and a proxy in front may keep the other:
	   a browser's User-Agent followed by a script's would pass as a script. Named twice,
	   in either order and in any spelling, the request is refused by the reader. */
	Box box;
	Gate gate(false, "");
	WireServer serving(1, 5000, WireServer::Serial, &gate);
	const std::string mozilla = std::string(kBrowser);
	struct Twice
	{
		const char *what;
		std::string lines;
	};
	const Twice twice[] = {
		{ "a browser's User-Agent, then a script's", "User-Agent: " + mozilla + "\r\nUser-Agent: curl/8.5.0\r\n" },
		{ "a script's, then a browser's", "User-Agent: curl/8.5.0\r\nUser-Agent: " + mozilla + "\r\n" },
		{ "two spellings", "User-Agent: " + mozilla + "\r\nuser-agent: curl/8.5.0\r\n" },
		{ "Origin twice", "User-Agent: " + mozilla + "\r\nOrigin: http://box\r\nORIGIN: http://evil.example\r\n" },
		{ "Referer twice", "User-Agent: " + mozilla + "\r\nReferer: http://evil.example/\r\nreferer: http://box/\r\n" },
		{ "Sec-Fetch-Site twice", "User-Agent: " + mozilla + "\r\nSec-Fetch-Site: cross-site\r\nsec-fetch-site: same-origin\r\n" },
		{ "Sec-Fetch-Dest twice", "User-Agent: " + mozilla + "\r\nSec-Fetch-Dest: document\r\nSec-Fetch-Dest: iframe\r\n" },
	};
	const char *const heads[] = {
		"GET /control/volume?30 HTTP/1.1\r\nHost: box\r\nAccept: text/html\r\n",
		"GET /api/v1/session HTTP/1.0\r\nHost: box\r\n",
	};
	for (size_t h = 0; h < sizeof(heads) / sizeof(heads[0]); ++h)
		for (size_t i = 0; i < sizeof(twice) / sizeof(twice[0]); ++i)
		{
			INFO(heads[h] << twice[i].what);
			bool early = true;
			const std::string answer = sentInTwo(serving.port, heads[h] + twice[i].lines + "\r\n", "", 0, &early);
			CHECK(statusOf(answer) == 400);
			CHECK(answer.find("ask-form") == std::string::npos);
			CHECK(answer.find("allowed_hosts") == std::string::npos);
		}

	// a POST alike
	bool early = true;
	const std::string post = sentInTwo(serving.port, "POST /control/volume HTTP/1.1\r\nHost: box\r\nUser-Agent: " + mozilla +
		"\r\nUser-Agent: curl/8.5.0\r\nContent-Type: application/x-www-form-urlencoded\r\nContent-Length: 4\r\n\r\nx=30", "", 0, &early);
	CHECK(statusOf(post) == 400);

	// once each, as a browser sends them: the page that asks
	const std::string once = sentInTwo(serving.port, "GET /control/volume?30 HTTP/1.1\r\nHost: box\r\nUser-Agent: " + mozilla +
		"\r\nAccept: text/html\r\nSec-Fetch-Site: none\r\nSec-Fetch-Mode: navigate\r\nSec-Fetch-Dest: document\r\n"
		"Sec-Fetch-User: ?1\r\n\r\n", "", 0, &early);
	CHECK(statusOf(once) == 403);
	CHECK(once.find("id=\"ask-form\"") != std::string::npos);
}
