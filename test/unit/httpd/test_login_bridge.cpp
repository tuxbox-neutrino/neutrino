/*
 * test_login_bridge.cpp - the one login as the old server and the screen ask it
 *
 * Copyright (C) 2026 Thilo Graf
 *
 * License: GPL-2.0-or-later
 */

#include "support/catch.hpp"

#include "httpd/auth.h"
#include "httpd/credentials.h"
#include "httpd/login_bridge.h"
#include "httpd/server.h"
#include "httpd/server_control.h"
#include "httpd/webconfig.h"

#include <cstdio>
#include <string>

#include <sys/stat.h>
#include <unistd.h>

using namespace httpd;

namespace
{

const char kUser[]     = "owner";
const char kPassword[] = "right-one";

// "owner:right-one" and "owner:wrong-one" in base64.
const char kRightBasic[] = "Basic b3duZXI6cmlnaHQtb25l";
const char kWrongBasic[] = "Basic b3duZXI6d3Jvbmctb25l";

/* The account under a cheap hash, and the login gate cleared going in and coming out,
   because it counts wrong answers per address across cases. */
struct Account
{
	WebConfig before;

	Account() : before(config())
	{
		forgetLoginAttemptsForTest();
		WebConfig c = defaultWebConfig();
		c.username = kUser;
		c.password_hash = hashSecret(kPassword, 1000);
		setConfigForTest(c);
	}

	~Account()
	{
		forgetLoginAttemptsForTest();
		setLoginClockForTest(0);
		setConfigForTest(before);
	}
};

std::string cookieFor(const std::string &token)
{
	return std::string("theme=dark; ") + sessionCookieName() + "=" + token;
}

login_bridge::Ask askFor(const std::string &path, const std::string &token)
{
	login_bridge::Ask a;
	a.path = path;
	a.peer = "203.0.113.7";
	a.cookie = cookieFor(token);
	a.host = "box.lan:8080";
	return a;
}

} // namespace

TEST_CASE("a session opens the pages, and the targets that change with a GET only from the box's own page",
          "[login]")
{
	Account account;
	const std::string token = openSession(kUser);
	REQUIRE_FALSE(token.empty());

	// A page: the cookie is enough, whoever linked to it.
	login_bridge::Ask page = askFor("/", token);
	REQUIRE(login_bridge::sessionAdmits(page));

	// ControlAPI and the template commands want to see where the request came from.
	const char *const guarded[] = { "/control/", "/cgi-bin/", "/fb/", "/api/v4/", "/y/" };
	for (size_t i = 0; i < sizeof(guarded) / sizeof(guarded[0]); ++i)
	{
		INFO(guarded[i]);
		login_bridge::Ask a = askFor(guarded[i], token);
		REQUIRE_FALSE(login_bridge::sessionAdmits(a));

		a.sec_fetch_site = "same-origin";
		REQUIRE(login_bridge::sessionAdmits(a));

		// Where the browser says, nothing else is read: a page of another site
		// cannot talk its way in through a Referer it chose.
		a.sec_fetch_site = "cross-site";
		a.referer = "http://box.lan:8080/dashboard.yhtm";
		REQUIRE_FALSE(login_bridge::sessionAdmits(a));
		a.sec_fetch_site = "none";
		REQUIRE_FALSE(login_bridge::sessionAdmits(a));
	}

	// A browser without Sec-Fetch-Site: Origin, else Referer, naming this host.
	login_bridge::Ask old = askFor("/control/", token);
	old.origin = "http://box.lan:8080";
	REQUIRE(login_bridge::sessionAdmits(old));
	old.origin = "http://box.lan:8080.example.org";
	REQUIRE_FALSE(login_bridge::sessionAdmits(old));
	old.origin = "null";
	REQUIRE_FALSE(login_bridge::sessionAdmits(old));
	old.origin.clear();
	old.referer = "HTTP://BOX.LAN:8080/settings.yhtm?x=1";
	REQUIRE(login_bridge::sessionAdmits(old));
	old.referer = "http://box.lan:8080@example.org/";
	REQUIRE_FALSE(login_bridge::sessionAdmits(old));
	old.referer = "http://example.org/?http://box.lan:8080/";
	REQUIRE_FALSE(login_bridge::sessionAdmits(old));
	old.host.clear();
	old.referer = "http:///";
	REQUIRE_FALSE(login_bridge::sessionAdmits(old));

	// A page asked to run a template command changes the box like the targets
	// above, and is held to the same rule.
	login_bridge::Ask command = askFor("/dashboard.yhtm", token);
	command.runs_commands = true;
	REQUIRE_FALSE(login_bridge::sessionAdmits(command));
	command.sec_fetch_site = "same-origin";
	REQUIRE(login_bridge::sessionAdmits(command));

	// A session that was closed opens nothing, and closing one says so once.
	REQUIRE(login_bridge::closeSessionOf(cookieFor(token)));
	REQUIRE_FALSE(login_bridge::closeSessionOf(cookieFor(token)));
	REQUIRE_FALSE(login_bridge::sessionAdmits(page));
	REQUIRE(login_bridge::forgetSessionCookie().find("Max-Age=0") != std::string::npos);
}

TEST_CASE("the old surface takes the Basic pair of the one account against its stored form", "[login]")
{
	Account account;

	login_bridge::Ask a;
	a.peer = "203.0.113.8";
	a.authorization = kRightBasic;
	REQUIRE(login_bridge::basicAdmits(a));
	// Remembered: the second time is not a login attempt at all.
	REQUIRE(login_bridge::basicAdmits(a));
	REQUIRE(loginAttemptCountForTest() == 0);

	a.authorization = kWrongBasic;
	REQUIRE_FALSE(login_bridge::basicAdmits(a));
	a.authorization = "Bearer b3duZXI6cmlnaHQtb25l";
	REQUIRE_FALSE(login_bridge::basicAdmits(a));

	// The remembered pair is worth nothing once the name changes either, as the
	// REST login would refuse it.
	a.authorization = kRightBasic;
	REQUIRE(login_bridge::basicAdmits(a));
	WebConfig renamed = config();
	renamed.username = "admin";
	setConfigForTest(renamed);
	REQUIRE_FALSE(login_bridge::basicAdmits(a));
	renamed.username = kUser;
	setConfigForTest(renamed);
	REQUIRE(login_bridge::basicAdmits(a));

	// The remembered pair is worth nothing once the stored form changes.
	WebConfig c = config();
	c.password_hash = hashSecret("another-one", 1000);
	setConfigForTest(c);
	a.authorization = kRightBasic;
	REQUIRE_FALSE(login_bridge::basicAdmits(a));
}

TEST_CASE("wrong Basic pairs are counted by the login's own gate", "[login]")
{
	Account account;
	setLoginClockForTest(1900000000);

	login_bridge::Ask a;
	a.peer = "203.0.113.9";
	a.authorization = kWrongBasic;
	for (int i = 0; i < 3; ++i)
		REQUIRE_FALSE(login_bridge::basicAdmits(a));

	// Inside the delay even the right pair is not checked, as at the REST login.
	unsigned retry_after = 0;
	REQUIRE(beginLoginAttempt(a.peer, &retry_after) == LoginAttempt::TooSoon);
	a.authorization = kRightBasic;
	REQUIRE_FALSE(login_bridge::basicAdmits(a));

	// Once it has passed, it is.
	setLoginClockForTest(1900000000 + 60);
	REQUIRE(login_bridge::basicAdmits(a));
}

TEST_CASE("a change at the box is what every login takes at once, and an empty field leaves it", "[login]")
{
	Account account;

	char name[96];
	std::snprintf(name, sizeof(name), "/tmp/ni-login-bridge-%d", (int) getpid());
	const std::string path = name;
	std::FILE *f = std::fopen(path.c_str(), "w");
	REQUIRE(f != NULL);
	std::fprintf(f, "username=%s\npassword_hash=%s\n", kUser, hashSecret(kPassword, 1000).c_str());
	REQUIRE(std::fclose(f) == 0);
	const WebConfig before = config();
	REQUIRE(load(path));

	std::string why;
	// Nothing typed: nothing changes, and the old password still opens the box.
	REQUIRE(login_bridge::change("", "", &why));
	login_bridge::Ask a;
	a.peer = "203.0.113.10";
	a.authorization = kRightBasic;
	REQUIRE(login_bridge::basicAdmits(a));

	// A new password is in effect when the call returns, for both logins.
	REQUIRE(login_bridge::change("", "brand-new", &why));
	REQUIRE(why.empty());
	REQUIRE_FALSE(login_bridge::basicAdmits(a));
	REQUIRE(verifySecret("brand-new", loginInEffect().password_hash));
	REQUIRE(login_bridge::user() == kUser);

	// And it is what the file says, so the next start keeps it.
	WebConfig reread = config();
	REQUIRE(load(path));
	REQUIRE(verifySecret("brand-new", config().password_hash));

	/* Only the account's lines: a REST save waiting for its restart wrote the port,
	   the address and the logo switch, and a change here must not write the values
	   still in effect back over them. */
	{
		std::string text;
		std::FILE *in = std::fopen(path.c_str(), "r");
		REQUIRE(in != NULL);
		char buf[512];
		size_t got;
		while ((got = std::fread(buf, 1, sizeof(buf), in)) > 0)
			text.append(buf, got);
		std::fclose(in);
		REQUIRE(text.find("\nport=") == std::string::npos);
		REQUIRE(text.find("\nbind_address=") == std::string::npos);
		REQUIRE(text.find("\nchannel_logos=") == std::string::npos);
	}

	// A new name alone keeps the password.
	REQUIRE(login_bridge::change("admin", "", &why));
	REQUIRE(login_bridge::user() == "admin");
	REQUIRE(verifySecret("brand-new", loginInEffect().password_hash));

	// Over the old server, only from a page of this box: another site's link
	// changes nothing, whatever it names.
	login_bridge::Ask from;
	from.host = "box.lan";
	REQUIRE_FALSE(login_bridge::change("", "from-elsewhere", &why, &from));
	REQUIRE_FALSE(why.empty());
	from.sec_fetch_site = "cross-site";
	from.referer = "http://box.lan/settings.yhtm";
	REQUIRE_FALSE(login_bridge::change("", "from-elsewhere", &why, &from));
	REQUIRE(verifySecret("brand-new", loginInEffect().password_hash));
	from.sec_fetch_site = "same-origin";
	REQUIRE(login_bridge::change("", "from-the-box", &why, &from));
	REQUIRE(verifySecret("from-the-box", loginInEffect().password_hash));

	// A name the file would not take changes nothing and says why.
	REQUIRE_FALSE(login_bridge::change(" spaced ", "", &why));
	REQUIRE_FALSE(why.empty());
	REQUIRE(login_bridge::user() == "admin");

	::unlink(path.c_str());
	setConfigForTest(before);
	(void) reread;
}

TEST_CASE("a change after a save waiting for its restart keeps what that save wrote", "[login]")
{
	Account account;

	char name[96];
	std::snprintf(name, sizeof(name), "/tmp/ni-login-pending-%d", (int) getpid());
	const std::string path = name;
	const std::string server = path + ".nhttpd";
	std::FILE *f = std::fopen(path.c_str(), "w");
	REQUIRE(f != NULL);
	std::fprintf(f, "username=%s\npassword_hash=%s\n", kUser, hashSecret(kPassword, 1000).c_str());
	REQUIRE(std::fclose(f) == 0);
	f = std::fopen(server.c_str(), "w");
	REQUIRE(f != NULL);
	std::fprintf(f, "WebsiteMain.port=8080\nconfigfile.version=5\n");
	REQUIRE(std::fclose(f) == 0);
	const WebConfig before = config();
	REQUIRE(load(path, server));

	// A REST save names another user and another port and waits for its restart.
	WebSettings put = currentWebSettings();
	put.username = "alice";
	put.port = 8099;
	REQUIRE(saveWebSettings(path, put));
	REQUIRE(loginInEffect().username == kUser);
	REQUIRE(currentWebSettings().username == "alice");
	REQUIRE(currentWebSettings().port == 8099);

	// In that window the box changes the password and nothing else.
	std::string why;
	REQUIRE(login_bridge::change("", "after-the-put", &why));

	// The restart reads the files: that save's name and port, and the new password.
	REQUIRE(load(path, server));
	REQUIRE(config().username == "alice");
	REQUIRE(config().server.port == 8099);
	REQUIRE(verifySecret("after-the-put", config().password_hash));
	REQUIRE(loginInEffect().username == "alice");

	// And once the file was read back, what is in effect is where a writer starts.
	REQUIRE(currentWebSettings().username == "alice");

	::unlink(path.c_str());
	::unlink(server.c_str());
	setConfigForTest(before);
}

namespace
{

// Hands that start on whatever they are given, so a restart here fails only
// where its read does and never for want of a server.
int startsAnywhere(int port, const char *) { return (port > 0) ? port : 80; }
bool stopsAtOnce() { return true; }

struct ServerHands
{
	ServerHands() { installNhttpdServerControl(&startsAnywhere, &stopsAtOnce); }
	~ServerHands()
	{
		stop();
		installNhttpdServerControl(0, 0);
	}
};

} // namespace

TEST_CASE("a restart whose read failed is no longer due, and the save it was for still waits", "[login]")
{
	Account account;
	ServerHands hands;

	char name[96];
	std::snprintf(name, sizeof(name), "/tmp/ni-login-restart-%d", (int) getpid());
	const std::string path = name;
	const std::string server = path + ".nhttpd";
	std::FILE *f = std::fopen(path.c_str(), "w");
	REQUIRE(f != NULL);
	std::fprintf(f, "username=%s\npassword_hash=%s\n", kUser, hashSecret(kPassword, 1000).c_str());
	REQUIRE(std::fclose(f) == 0);
	f = std::fopen(server.c_str(), "w");
	REQUIRE(f != NULL);
	std::fprintf(f, "WebsiteMain.port=8080\nconfigfile.version=5\n");
	REQUIRE(std::fclose(f) == 0);
	const WebConfig before = config();
	REQUIRE(load(path, server));

	WebSettings put = currentWebSettings();
	put.port = 8099;
	REQUIRE(saveWebSettings(path, put));
	armRestart();
	REQUIRE(restartAwaitsLoad());

	/* The file cannot be read when the restart comes round: a directory stands at
	   its name, which no reader takes for a file whoever runs the suite. */
	REQUIRE(::unlink(path.c_str()) == 0);
	REQUIRE(::mkdir(path.c_str(), 0700) == 0);
	REQUIRE_FALSE(reloadAndRestart(path));
	REQUIRE_FALSE(restartAwaitsLoad());
	REQUIRE(currentWebSettings().port == 8099);

	::rmdir(path.c_str());
	::unlink(server.c_str());
	setConfigForTest(before);
}

TEST_CASE("a stored password the boxes shipped with is recognised, and another one is not", "[login]")
{
	Account account;
	REQUIRE(login_bridge::passwordSet());
	REQUIRE_FALSE(login_bridge::passwordIsShipped());

	WebConfig c = config();
	c.password_hash = hashSecret("tuxbox", 1000);
	setConfigForTest(c);
	REQUIRE(login_bridge::passwordIsShipped());

	c.password_hash.clear();
	setConfigForTest(c);
	REQUIRE_FALSE(login_bridge::passwordSet());
	REQUIRE_FALSE(login_bridge::passwordIsShipped());
}
