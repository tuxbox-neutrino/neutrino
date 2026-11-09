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
#include "httpd/endpoints.h"
#include "httpd/login_bridge.h"
#include "httpd/server.h"
#include "httpd/server_control.h"
#include "httpd/webconfig.h"
#include "yhook.h"
#include "yrequest.h"
#include "yhttpd_mods/mod_auth.h"

#include <cstdio>
#include <string>
#include <vector>

#include <fcntl.h>
#include <ftw.h>
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

// A change over the old server, in a transaction of its own as the route makes it.
bool changeFrom(const std::string &user, const std::string &password, std::string *why,
                const login_bridge::Ask &from)
{
	login_bridge::Transaction t;
	return t.change(user, password, why, &from);
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
	const std::string token = openSessionForTest(kUser);
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
		a.referer = "http://box.lan:8080/dashboard.html";
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
	old.referer = "HTTP://BOX.LAN:8080/settings.html?x=1";
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
	login_bridge::Ask command = askFor("/dashboard.html", token);
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
	REQUIRE_FALSE(changeFrom("", "from-elsewhere", &why, from));
	REQUIRE_FALSE(why.empty());
	from.sec_fetch_site = "cross-site";
	from.referer = "http://box.lan/settings.html";
	REQUIRE_FALSE(changeFrom("", "from-elsewhere", &why, from));
	REQUIRE(verifySecret("brand-new", loginInEffect().password_hash));
	from.sec_fetch_site = "same-origin";
	REQUIRE(changeFrom("", "from-the-box", &why, from));
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

TEST_CASE("a password set for the account is held to the rules, and the reset at the box draws a first one", "[login]")
{
	Account account;

	char name[96];
	std::snprintf(name, sizeof(name), "/tmp/ni-login-rules-%d", (int) getpid());
	const std::string path = name;
	std::FILE *f = std::fopen(path.c_str(), "w");
	REQUIRE(f != NULL);
	std::fprintf(f, "username=%s\npassword_hash=%s\n", kUser, hashSecret(kPassword, 1000).c_str());
	REQUIRE(std::fclose(f) == 0);
	const WebConfig before = config();
	REQUIRE(load(path));

	struct Refused
	{
		const char *password;
		const char *says;
	};
	const Refused refused[] = {
		{ "seven77", "fewer than 8" },
		// seven characters in fourteen bytes: counted as a person counts them
		{ "\xc3\xa4\xc3\xb6\xc3\xbc\xc3\xa4\xc3\xb6\xc3\xbc\xc3\xa4", "fewer than 8" },
		{ "with\ttab inside", "control character" },
		{ "coolstream", "shipped with" },
		{ kShippedPassword, "shipped with" },
	};
	std::string why;
	for (size_t i = 0; i < sizeof(refused) / sizeof(refused[0]); ++i)
	{
		INFO(refused[i].says);
		REQUIRE_FALSE(login_bridge::change("", refused[i].password, &why));
		REQUIRE(why.find(refused[i].says) != std::string::npos);
		REQUIRE(verifySecret(kPassword, loginInEffect().password_hash));
	}
	// eight characters, some of them two bytes each
	REQUIRE(login_bridge::change("", "\xc3\xa4\xc3\xb6\xc3\xbc\xc3\xa4\xc3\xb6\xc3\xbc\xc3\xa4x", &why));
	REQUIRE(verifySecret("\xc3\xa4\xc3\xb6\xc3\xbc\xc3\xa4\xc3\xb6\xc3\xbc\xc3\xa4x", loginInEffect().password_hash));

	/* The reset at the box puts user root and a new first password in place, which
	   the screen shows, and the first login asks to replace it. In effect at once,
	   and in the file beside its stored form, so a start after it reads it back. */
	std::string first;
	REQUIRE(login_bridge::resetToFirstPassword(&first, &why));
	REQUIRE(first.size() == kFirstPasswordChars);
	REQUIRE(login_bridge::user() == "root");
	REQUIRE(verifySecret(first, loginInEffect().password_hash));
	REQUIRE_FALSE(verifySecret(kShippedPassword, loginInEffect().password_hash));
	REQUIRE(firstPasswordInEffect() == first);
	REQUIRE(login_bridge::passwordOwed());
	REQUIRE(firstPasswordWrittenThisRun());
	REQUIRE(load(path));
	REQUIRE(firstPasswordInEffect() == first);

	// Every reset draws another one.
	std::string again;
	REQUIRE(login_bridge::resetToFirstPassword(&again, &why));
	REQUIRE(again != first);
	REQUIRE(firstPasswordInEffect() == again);
	REQUIRE_FALSE(verifySecret(first, loginInEffect().password_hash));

	::unlink(path.c_str());
	setConfigForTest(before);
}

namespace
{

// A file holding the account as given, under a cheap hash.
std::string accountFile(const char *tag, const std::string &user, const std::string &password,
                        const std::string &first)
{
	char name[96];
	std::snprintf(name, sizeof(name), "/tmp/ni-login-%s-%d", tag, (int) getpid());
	std::FILE *f = std::fopen(name, "w");
	REQUIRE(f != NULL);
	std::fprintf(f, "username=%s\npassword_hash=%s\n", user.c_str(), hashSecret(password, 1000).c_str());
	if (!first.empty())
		std::fprintf(f, "first_password=%s\n", first.c_str());
	REQUIRE(std::fclose(f) == 0);
	return name;
}

std::string contentOf(const std::string &path)
{
	std::string text;
	std::FILE *f = std::fopen(path.c_str(), "r");
	REQUIRE(f != NULL);
	char buf[4096];
	size_t n;
	while ((n = std::fread(buf, 1, sizeof(buf), f)) > 0)
		text.append(buf, n);
	std::fclose(f);
	return text;
}

} // namespace

TEST_CASE("a first password stands until a password of the owner's own replaces it, and a new name keeps it", "[login]")
{
	Account account;
	const WebConfig before = config();

	const std::string first = drawFirstPassword();
	REQUIRE(first.size() == kFirstPasswordChars);
	const std::string path = accountFile("first", "root", first, first);
	REQUIRE(load(path));
	REQUIRE(firstPasswordStands());
	REQUIRE(firstPasswordInEffect() == first);
	REQUIRE(login_bridge::passwordOwed());

	/* A name changed alone is not the change the box waits for: the first password
	   stays, in effect and in the file, and so does the question. */
	std::string why;
	REQUIRE(login_bridge::change("admin", "", &why));
	REQUIRE(login_bridge::user() == "admin");
	REQUIRE(firstPasswordInEffect() == first);
	REQUIRE(login_bridge::passwordOwed());
	REQUIRE(contentOf(path).find("first_password=" + first + "\n") != std::string::npos);
	REQUIRE(load(path));
	REQUIRE(firstPasswordInEffect() == first);

	/* A password of the owner's own takes it off the screen, out of the file and
	   out of memory, and the box stops asking. */
	REQUIRE(login_bridge::change("", "my-own-password", &why));
	REQUIRE_FALSE(firstPasswordStands());
	REQUIRE_FALSE(login_bridge::passwordOwed());
	const std::string after = contentOf(path);
	REQUIRE(after.find("first_password=") == std::string::npos);
	REQUIRE(after.find(first) == std::string::npos);
	REQUIRE(after.find("my-own-password") == std::string::npos);
	REQUIRE(load(path));
	REQUIRE_FALSE(firstPasswordStands());
	REQUIRE(verifySecret("my-own-password", loginInEffect().password_hash));

	::unlink(path.c_str());
	setConfigForTest(before);
}

TEST_CASE("a line naming a first password that is not the stored one is said and counts for nothing", "[login]")
{
	Account account;
	const WebConfig before = config();

	const std::string path = accountFile("stale", "root", "set-by-hand-1", "abcdefghjk");
	REQUIRE(load(path));
	REQUIRE_FALSE(firstPasswordStands());
	REQUIRE_FALSE(login_bridge::passwordOwed());
	bool said = false;
	const std::vector<std::string> problems = configProblems();
	for (size_t i = 0; i < problems.size(); ++i)
	{
		said = said || problems[i].find("first_password") != std::string::npos;
		REQUIRE(problems[i].find("abcdefghjk") == std::string::npos);
	}
	REQUIRE(said);

	::unlink(path.c_str());
	setConfigForTest(before);
}

TEST_CASE("the head of the file follows the password the box opens to, and nothing else of it moves", "[login]")
{
	/* A file an earlier build wrote names the shipped password as the login. Once
	   the box replaced it, that is a login no longer in force, and the reader of
	   that paragraph is somebody who has just been refused one. */
	Account account;
	const WebConfig before = config();

	char name[96];
	std::snprintf(name, sizeof(name), "/tmp/ni-login-head-%d", (int) getpid());
	const std::string path = name;
	std::FILE *f = std::fopen(path.c_str(), "w");
	REQUIRE(f != NULL);
	std::fprintf(f, "# nhttpd access configuration.\n"
	             "#\n"
	             "# LOGIN: user \"root\", password \"%s\", the login every box ships with. The\n"
	             "# first login asks for a password of your own.\n"
	             "# TO CHANGE IT: the web interface under Settings, or the settings screen at the\n"
	             "# box itself.\n"
	             "# TO START OVER: delete this file, then restart.\n"
	             "#\n"
	             "# Written by the box itself.\n"
	             "username=root\n"
	             "password_hash=%s\n", kShippedPassword, hashSecret(kShippedPassword, 1000).c_str());
	REQUIRE(std::fclose(f) == 0);
	REQUIRE(load(path));

	std::string why;
	REQUIRE(login_bridge::replaceShippedPassword(&why));
	const std::string first = firstPasswordInEffect();
	const std::string converted = contentOf(path);
	REQUIRE(converted.compare(0, 32, "# nhttpd access configuration.\n#") == 0);
	REQUIRE(converted.find(std::string("password \"") + kShippedPassword + "\"") == std::string::npos);
	REQUIRE(converted.find("# LOGIN: user \"root\", with a first password the box drew") != std::string::npos);
	REQUIRE(converted.find("TO START OVER") != std::string::npos);
	REQUIRE(converted.find("#\n# Written by the box itself.\n") != std::string::npos);
	REQUIRE(converted.find(first) == converted.rfind(first));

	// the owner's own: the head says so and no longer speaks of a first password
	REQUIRE(login_bridge::change("", "my-own-password", &why));
	const std::string own = contentOf(path);
	REQUIRE(own.find("# LOGIN: user \"root\", with the password last set at the box") != std::string::npos);
	REQUIRE(own.find("first_password") == std::string::npos);
	REQUIRE(own.find("#\n# Written by the box itself.\n") != std::string::npos);

	// a name changed alone writes no password and leaves the head as it is
	REQUIRE(login_bridge::change("admin", "", &why));
	REQUIRE(contentOf(path).find("# LOGIN: user \"root\", with the password last set at the box") != std::string::npos);

	::unlink(path.c_str());
	setConfigForTest(before);
}

TEST_CASE("a stored password the boxes shipped with gives way to a first one at the start, or stays and says why", "[login]")
{
	Account account;
	const WebConfig before = config();
	std::string why;

	// Nothing to replace: a password of the owner's own is left alone.
	{
		const std::string path = accountFile("own", "owner", kPassword, "");
		REQUIRE(load(path));
		const std::string stored = loginInEffect().password_hash;
		REQUIRE(login_bridge::replaceShippedPassword(&why));
		REQUIRE(loginInEffect().password_hash == stored);
		REQUIRE_FALSE(firstPasswordStands());
		::unlink(path.c_str());
	}

	// One of the shipped ones, under a name of somebody's own, which stays.
	{
		const std::string path = accountFile("shipped", "owner", "coolstream", "");
		REQUIRE(load(path));
		REQUIRE(login_bridge::passwordOwed());
		REQUIRE(login_bridge::replaceShippedPassword(&why));
		const std::string first = firstPasswordInEffect();
		REQUIRE(first.size() == kFirstPasswordChars);
		REQUIRE(login_bridge::user() == "owner");
		REQUIRE(verifySecret(first, loginInEffect().password_hash));
		REQUIRE_FALSE(verifySecret("coolstream", loginInEffect().password_hash));
		REQUIRE(login_bridge::passwordOwed());
		REQUIRE(firstPasswordWrittenThisRun());

		// the start after it finds a first password and leaves it standing
		REQUIRE(load(path));
		REQUIRE(firstPasswordInEffect() == first);
		REQUIRE(login_bridge::replaceShippedPassword(&why));
		REQUIRE(firstPasswordInEffect() == first);
		::unlink(path.c_str());
	}

	/* One that cannot be written over: the directory is gone by the time the start
	   gets to it, which no reader takes for somewhere to write whoever runs the
	   suite. The shipped password stays in effect, the box still asks, and why is
	   said without naming any password. */
	{
		char dir[96];
		std::snprintf(dir, sizeof(dir), "/tmp/ni-login-gone-%d", (int) getpid());
		REQUIRE(::mkdir(dir, 0700) == 0);
		const std::string path = std::string(dir) + "/nhttpd-access.conf";
		std::FILE *f = std::fopen(path.c_str(), "w");
		REQUIRE(f != NULL);
		std::fprintf(f, "username=root\npassword_hash=%s\n", hashSecret(kShippedPassword, 1000).c_str());
		REQUIRE(std::fclose(f) == 0);
		REQUIRE(load(path));
		REQUIRE(::unlink(path.c_str()) == 0);
		REQUIRE(::rmdir(dir) == 0);

		REQUIRE_FALSE(login_bridge::replaceShippedPassword(&why));
		REQUIRE_FALSE(why.empty());
		REQUIRE(why.find(kShippedPassword) == std::string::npos);
		REQUIRE(verifySecret(kShippedPassword, loginInEffect().password_hash));
		REQUIRE_FALSE(firstPasswordStands());
		REQUIRE(login_bridge::passwordOwed());
	}

	setConfigForTest(before);
}

TEST_CASE("a stored password the boxes shipped with is recognised, and another one is not", "[login]")
{
	Account account;
	REQUIRE(login_bridge::passwordSet());
	REQUIRE_FALSE(login_bridge::passwordOwed());

	WebConfig c = config();
	c.password_hash = hashSecret("tuxbox", 1000);
	setConfigForTest(c);
	REQUIRE(login_bridge::passwordOwed());

	c.password_hash.clear();
	setConfigForTest(c);
	REQUIRE_FALSE(login_bridge::passwordSet());
	REQUIRE_FALSE(login_bridge::passwordOwed());
}

namespace
{

const char kRequested[] = "set-by-request";

/* nhttpd's own gate in front of /control/, as nhttpd.conf sets it: the page login on
   or off, and the prefix of the addresses it lets in without asking. */
struct Gate : CmAuth
{
	Gate(bool page_login, const std::string &let_in)
	{
		authenticate = page_login;
		no_auth_client = let_in;
	}
};

/* /control/config?config=nhttpd&action=submit asking for a new name and password, each
   left out where empty, as nhttpd hands it to its hooks, from peer and with no header
   saying where it came from. */
void submitFrom(CyhookHandler &hh, const std::string &peer, const std::string &user,
                const std::string &password)
{
	hh.Method = M_GET;
	hh.UrlData["url"] = "/control/config";
	hh.UrlData["fullurl"] = "/control/config?config=nhttpd&action=submit";
	hh.UrlData["path"] = "/control/";
	hh.UrlData["filename"] = "config";
	hh.UrlData["clientaddr"] = peer;
	hh.HeaderList["Host"] = "box.lan";
	hh.ParamList["config"] = "nhttpd";
	hh.ParamList["action"] = "submit";
	if (!user.empty())
		hh.ParamList["mod_auth_dot_username"] = user;
	if (!password.empty())
		hh.ParamList["mod_auth_dot_password"] = password;
}

/* The account the cases below start from, in a file: kUser with kPassword, or with
   first standing as its first password where one is given, then the lines in more. */
std::string controlAccount(const std::string &first, const char *more)
{
	char name[96];
	std::snprintf(name, sizeof(name), "/tmp/login-control-%d", (int) getpid());
	std::FILE *f = std::fopen(name, "w");
	REQUIRE(f != NULL);
	const std::string password = first.empty() ? std::string(kPassword) : first;
	std::fprintf(f, "username=%s\npassword_hash=%s\n", kUser, hashSecret(password, 1000).c_str());
	if (!first.empty())
		std::fprintf(f, "first_password=%s\n", first.c_str());
	std::fputs(more, f);
	REQUIRE(std::fclose(f) == 0);
	return name;
}

} // namespace

TEST_CASE("the account changes over /control/ only for a signed-in caller or the box itself, page login off or on",
          "[login]")
{
	Account account;
	const WebConfig before = config();
	const std::string first = drawFirstPassword();
	REQUIRE(first.size() == kFirstPasswordChars);

	enum Cookie { kNoCookie, kLiveSession, kClosedSession };
	struct Case
	{
		const char *what;
		const char *peer;
		const char *authorization;  // the Authorization header, empty for none
		Cookie      cookie;         // the session the Cookie header names
		const char *sec_fetch_site; // empty for none
		const char *user;           // the new name asked for, empty for none
		const char *password;       // the new password asked for, empty for none
		bool        first_password; // the account still on its first password
		const char *more;           // further lines of the account's file
		bool        past_gate_on;   // through the gate while the page login is on
		bool        changes;
		const char *says;           // part of the refusal, empty where there is none
	};
	const Case cases[] = {
		{ "no credentials, the headers of a page of this box", "203.0.113.20", "", kNoCookie,
		  "same-origin", "", kRequested, false, "", false, false, "sign-in" },
		{ "the Basic pair of the account", "203.0.113.21", kRightBasic, kNoCookie,
		  "same-origin", "", kRequested, false, "", true, true, "" },
		{ "a wrong Basic pair", "203.0.113.24", kWrongBasic, kNoCookie,
		  "same-origin", "", kRequested, false, "", false, false, "sign-in" },
		{ "the Basic pair of the account, not from a page of this box", "203.0.113.26", kRightBasic, kNoCookie,
		  "", "", kRequested, false, "", true, false, "page of this box" },
		{ "a session, from a page of this box", "203.0.113.22", "", kLiveSession,
		  "same-origin", "", kRequested, false, "", true, false, "settings of the web interface" },
		{ "a session alone, naming a new user", "203.0.113.27", "", kLiveSession,
		  "same-origin", "admin", "", false, "", true, false, "settings of the web interface" },
		{ "a session beside the Basic pair of the account", "203.0.113.28", kRightBasic, kLiveSession,
		  "same-origin", "", kRequested, false, "", true, true, "" },
		{ "a session, from a page of another site", "203.0.113.23", "", kLiveSession,
		  "cross-site", "", kRequested, false, "", false, false, "page of this box" },
		{ "a session that was closed", "203.0.113.25", "", kClosedSession,
		  "same-origin", "", kRequested, false, "", false, false, "sign-in" },
		{ "the box itself", "127.0.0.1", "", kNoCookie,
		  "same-origin", "", kRequested, false, "", true, true, "" },
		{ "the box itself, with a password the rules refuse", "127.0.0.1", "", kNoCookie,
		  "same-origin", "", "seven77", false, "", true, false, "fewer than 8" },
		{ "the box itself, with a name the file would not take", "127.0.0.1", "", kNoCookie,
		  "same-origin", "ro#ot", "", false, "", true, false, "new name" },
		{ "the box itself, while its first password stands", "127.0.0.1", "", kNoCookie,
		  "same-origin", "", kRequested, true, "", true, false, "PUT /api/v1/account/password" },
		{ "an address on the home network, and nothing else", "192.168.4.20", "", kNoCookie,
		  "same-origin", "", kRequested, false, "", false, false, "sign-in" },
		{ "an address the gate lets in unasked, and nothing else", "198.51.100.23", "", kNoCookie,
		  "same-origin", "", kRequested, false, "", true, false, "sign-in" },
		{ "the loopback behind a configured proxy", "127.0.0.1", "", kNoCookie,
		  "same-origin", "", kRequested, false, "trusted_proxies=127.0.0.1/32\n", false, false, "sign-in" },
		{ "the loopback with a proxy line that names none", "127.0.0.1", "", kNoCookie,
		  "same-origin", "", kRequested, false, "trusted_proxies=\n", false, false, "sign-in" },
		{ "the loopback with a proxy line that cannot be read", "127.0.0.1", "", kNoCookie,
		  "same-origin", "", kRequested, false, "trusted_proxies=127.0.0.1/32 # in front\n", false, false, "sign-in" },
	};
	const bool page_login[] = { false, true };

	for (size_t m = 0; m < sizeof(page_login) / sizeof(page_login[0]); ++m)
	{
		for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); ++i)
		{
			const Case &c = cases[i];
			INFO(c.what << (page_login[m] ? ", page login on" : ", page login off"));
			// every case starts with no wrong answer counted against any address
			forgetLoginAttemptsForTest();

			const std::string path = controlAccount(c.first_password ? first : std::string(), c.more);
			REQUIRE(load(path));
			const std::string stored = c.first_password ? first : std::string(kPassword);

			CyhookHandler hh;
			submitFrom(hh, c.peer, c.user, c.password);
			if (c.authorization[0] != '\0')
				hh.HeaderList["Authorization"] = c.authorization;
			if (c.sec_fetch_site[0] != '\0')
				hh.HeaderList["Sec-Fetch-Site"] = c.sec_fetch_site;
			std::string token;
			if (c.cookie != kNoCookie)
			{
				token = openSessionForTest(kUser);
				REQUIRE_FALSE(token.empty());
				hh.HeaderList["Cookie"] = cookieFor(token);
				if (c.cookie == kClosedSession)
					REQUIRE(login_bridge::closeSessionOf(cookieFor(token)));
			}

			Gate gate(page_login[m], "198.51.100.");
			const bool past_gate = gate.Hook_PrepareResponse(&hh) == HANDLED_CONTINUE;
			CHECK(past_gate == (page_login[m] ? c.past_gate_on : true));

			// The rule answers for itself, whatever the gate in front of it did.
			const login_bridge::Ask from = CmAuth::AskOf(&hh);
			std::string why;
			login_bridge::Admitted found;
			const bool admitted = login_bridge::mayChange(from, c.user, c.password, &why, &found);
			CHECK(admitted == c.changes);
			CHECK(admitted == why.empty());
			CHECK(why.find(c.says) != std::string::npos);

			// And in the route's order: the gate, the rule, then the change in its transaction.
			bool changed = false;
			if (past_gate && admitted)
			{
				login_bridge::Transaction t;
				changed = t.admits(found, &why) && t.change(c.user, c.password, &why, &from);
			}
			CHECK(changed == c.changes);
			CHECK(verifySecret((c.changes && c.password[0] != '\0') ? std::string(c.password) : stored,
			                   loginInEffect().password_hash));
			CHECK(login_bridge::user() == ((c.changes && c.user[0] != '\0') ? std::string(c.user) : std::string(kUser)));

			if (c.cookie == kLiveSession)
				login_bridge::closeSessionOf(cookieFor(token));
			::unlink(path.c_str());
		}
	}

	/* The box itself in the other ways its address is written. nhttpd's socket
	   reports an IPv4 address, and the rule reads the peer as one of either family. */
	{
		const std::string path = controlAccount(std::string(), "");
		REQUIRE(load(path));
		const char *const here[] = { "127.0.0.2", "::1", "::ffff:127.0.0.1" };
		for (size_t i = 0; i < sizeof(here) / sizeof(here[0]); ++i)
		{
			INFO(here[i]);
			login_bridge::Ask a;
			a.path = "/control/";
			a.peer = here[i];
			a.host = "box.lan";
			a.sec_fetch_site = "same-origin";
			std::string why;
			CHECK(login_bridge::mayChange(a, "", kRequested, &why));
		}
		::unlink(path.c_str());
	}

	setConfigForTest(before);
}

namespace
{

// What a player holding this token for the media is granted: System while it stands.
AuthLevel playerWith(const std::string &token)
{
	Credentials player;
	player.peer = "203.0.113.30";
	player.query_token = token;
	return granted(player, true);
}

} // namespace

TEST_CASE("a change of the account signs everybody else out and what they drew, and keeps the session that asked",
          "[login][revision]")
{
	Account account;
	const WebConfig before = config();
	const std::string path = controlAccount(std::string(), "");
	REQUIRE(load(path));

	// Three browsers signed in, and a player holding a token one of the others drew.
	const std::string asking = openSessionForTest(kUser);
	const std::string other = openSessionForTest(kUser);
	const std::string third = openSessionForTest(kUser);
	const std::string drawn = openScopedToken(mediaScopeName(), AuthLevel::System, other);
	const std::string drawn_by_asking = openScopedToken(mediaScopeName(), AuthLevel::System, asking);
	REQUIRE_FALSE(drawn.empty());
	REQUIRE_FALSE(drawn_by_asking.empty());
	const std::string asking_csrf = csrfFor(asking);
	REQUIRE_FALSE(asking_csrf.empty());

	// The file read again as it stands is no change: nobody is signed out.
	const unsigned long long revision = accountRevision();
	REQUIRE(load(path));
	REQUIRE(accountRevision() == revision);
	REQUIRE(sessionIsLive(asking));
	REQUIRE(sessionIsLive(other));
	REQUIRE(sessionIsLive(third));
	REQUIRE(playerWith(drawn) == AuthLevel::System);

	// A new password over /control/, from a page of this box, signed in with one of them.
	login_bridge::Ask from = askFor("/control/", asking);
	from.sec_fetch_site = "same-origin";
	from.authorization = kRightBasic;
	std::string why;
	login_bridge::Admitted admitted;
	REQUIRE(login_bridge::mayChange(from, "", kRequested, &why, &admitted));
	REQUIRE(admitted.session == asking);
	std::string renewed;
	{
		login_bridge::Transaction t;
		REQUIRE(t.admits(admitted, &why));
		REQUIRE(t.change("", kRequested, &why, &from));
		renewed = t.session();
	}
	REQUIRE(accountRevision() != revision);

	// The one that asked goes on under new tokens, and a copy of its old cookie is out.
	REQUIRE_FALSE(renewed.empty());
	REQUIRE(renewed != asking);
	REQUIRE(sessionIsLive(renewed));
	REQUIRE_FALSE(csrfFor(renewed).empty());
	REQUIRE(csrfFor(renewed) != asking_csrf);
	REQUIRE(sessionUser(renewed) == kUser);
	REQUIRE_FALSE(sessionIsLive(asking));
	REQUIRE_FALSE(login_bridge::sessionAdmits(from));
	from.cookie = cookieFor(renewed);
	REQUIRE(login_bridge::sessionAdmits(from));
	REQUIRE(login_bridge::sessionCookie(renewed).find(std::string(sessionCookieName()) + "=" + renewed + ";") == 0);
	// Everybody else is out, and so is every token drawn before, its own included.
	REQUIRE_FALSE(sessionIsLive(other));
	REQUIRE_FALSE(sessionIsLive(third));
	REQUIRE(playerWith(drawn) == AuthLevel::Public);
	REQUIRE(playerWith(drawn_by_asking) == AuthLevel::Public);
	// What it draws now stands.
	const std::string drawn_after = openScopedToken(mediaScopeName(), AuthLevel::System, renewed);
	REQUIRE(playerWith(drawn_after) == AuthLevel::System);

	// A new name at the box keeps nobody signed in.
	const std::string later = openSessionForTest(kUser);
	REQUIRE(login_bridge::change("admin", "", &why));
	REQUIRE_FALSE(sessionIsLive(renewed));
	REQUIRE_FALSE(sessionIsLive(later));
	REQUIRE(playerWith(drawn_after) == AuthLevel::Public);

	/* A walk of the table that read the account just before a change and takes the
	   table just after it: a session the change carried across already holds the
	   newer revision, and the walk leaves it. */
	const std::string carried = openSessionForTest("admin");
	const unsigned long long reading = accountRevision();
	REQUIRE(carrySession(carried, reading, reading + 1));
	expireSessions(time(NULL));
	REQUIRE(login_bridge::change("", "after-the-walk", &why));
	REQUIRE(accountRevision() == reading + 1);
	REQUIRE(sessionIsLive(carried));

	// The reset at the box ends every session, and one checked against the account
	// before it opens nothing after it.
	const unsigned long long before_reset = accountRevision();
	const std::string again = openSessionForTest("admin");
	std::string first;
	REQUIRE(login_bridge::resetToFirstPassword(&first, &why));
	REQUIRE_FALSE(sessionIsLive(again));
	REQUIRE(openSession("root", before_reset).empty());
	REQUIRE(sessionIsLive(openSession("root", accountRevision())));

	::unlink(path.c_str());
	setConfigForTest(before);
}

namespace
{

/* What a probe between a check and the write it allows did, and what both files held
   right after it: the files are the account's and nhttpd.conf beside it. */
struct Between
{
	std::string access;
	std::string server;
	std::string closes; // the session closeThenLook closes
	std::string access_after;
	std::string server_after;
	bool        ran;

	Between() : ran(false) {}
};

Between &between()
{
	static Between b;
	return b;
}

void lookAtBoth()
{
	between().access_after = contentOf(between().access);
	between().server_after = contentOf(between().server);
}

// The reset at the box.
void resetThenLook()
{
	std::string first, why;
	between().ran = login_bridge::resetToFirstPassword(&first, &why);
	lookAtBoth();
}

// Signing out in another tab.
void closeThenLook()
{
	between().ran = closeSession(between().closes);
	lookAtBoth();
}

/* The box asking for a password of the owner's own while the account stays the one it
   was: the file read again naming the stored password as its first one. */
void owedThenLook()
{
	std::FILE *f = std::fopen(between().access.c_str(), "a");
	REQUIRE(f != NULL);
	std::fprintf(f, "first_password=%s\n", kPassword);
	REQUIRE(std::fclose(f) == 0);
	const unsigned long long revision = accountRevision();
	between().ran = load(between().access, between().server) && accountRevision() == revision &&
	                login_bridge::passwordOwed();
	lookAtBoth();
}

/* The account's file and an nhttpd.conf beside it, read in, and the probe's notes
   pointed at both. */
std::string stage(std::string *server)
{
	const std::string path = controlAccount(std::string(), "");
	*server = path + ".nhttpd";
	std::FILE *f = std::fopen(server->c_str(), "w");
	REQUIRE(f != NULL);
	std::fprintf(f, "WebsiteMain.port=8080\nmod_auth.authenticate=false\nconfigfile.version=5\n");
	REQUIRE(std::fclose(f) == 0);
	REQUIRE(load(path, *server));
	between() = Between();
	between().access = path;
	between().server = *server;
	return path;
}

login_bridge::Ask controlFromThisBox(const char *peer)
{
	login_bridge::Ask a;
	a.path = "/control/";
	a.peer = peer;
	a.host = "box.lan";
	a.sec_fetch_site = "same-origin";
	return a;
}

// Forgets a probe a failed case left behind.
struct ProbeCleared
{
	~ProbeCleared() { setAccountLockProbeForTest(NULL); }
};

} // namespace

TEST_CASE("a change over /control/ that a reset made stale between its check and its write writes nothing",
          "[login][transaction]")
{
	Account account;
	ProbeCleared cleared;
	const WebConfig before = config();

	const char *const ways[] = { "the Basic pair", "a session beside the Basic pair", "the box itself" };
	for (size_t w = 0; w < sizeof(ways) / sizeof(ways[0]); ++w)
	{
		INFO(ways[w]);
		forgetLoginAttemptsForTest();
		std::string server;
		const std::string path = stage(&server);

		login_bridge::Ask from = controlFromThisBox(w == 2 ? "127.0.0.1" : "203.0.113.60");
		if (w == 0 || w == 1)
			from.authorization = kRightBasic;
		if (w == 1)
			from.cookie = cookieFor(openSessionForTest(kUser));

		std::string why;
		login_bridge::Admitted admitted;
		REQUIRE(login_bridge::mayChange(from, "", kRequested, &why, &admitted));

		setAccountLockProbeForTest(&resetThenLook);
		{
			login_bridge::Transaction t;
			REQUIRE(between().ran);
			REQUIRE_FALSE(t.admits(admitted, &why));
			REQUIRE(why.find("account changed") != std::string::npos);
			// and once it said no, the transaction writes nothing
			REQUIRE_FALSE(t.change("", kRequested, &why, &from));
		}
		REQUIRE(contentOf(path) == between().access_after);
		REQUIRE(contentOf(server) == between().server_after);
		REQUIRE(firstPasswordStands());
		REQUIRE_FALSE(verifySecret(kRequested, loginInEffect().password_hash));

		::unlink(path.c_str());
		::unlink(server.c_str());
	}
	setConfigForTest(before);
}

TEST_CASE("a change over /control/ whose session ended between its check and its write writes nothing",
          "[login][transaction]")
{
	Account account;
	ProbeCleared cleared;
	const WebConfig before = config();
	std::string server;
	const std::string path = stage(&server);

	const std::string session = openSessionForTest(kUser);
	login_bridge::Ask from = controlFromThisBox("203.0.113.61");
	from.cookie = cookieFor(session);
	from.authorization = kRightBasic;
	std::string why;
	login_bridge::Admitted admitted;
	REQUIRE(login_bridge::mayChange(from, "", kRequested, &why, &admitted));
	REQUIRE(admitted.session == session);

	between().closes = session;
	setAccountLockProbeForTest(&closeThenLook);
	{
		login_bridge::Transaction t;
		REQUIRE(between().ran);
		REQUIRE_FALSE(t.admits(admitted, &why));
		REQUIRE(why.find("session") != std::string::npos);
		REQUIRE_FALSE(t.change("", kRequested, &why, &from));
	}
	REQUIRE(contentOf(path) == between().access_after);
	REQUIRE(contentOf(server) == between().server_after);
	REQUIRE(verifySecret(kPassword, loginInEffect().password_hash));

	::unlink(path.c_str());
	::unlink(server.c_str());
	setConfigForTest(before);
}

TEST_CASE("a change over /control/ writes nothing once the box asks for a password of the owner's own in between",
          "[login][transaction]")
{
	Account account;
	ProbeCleared cleared;
	const WebConfig before = config();
	std::string server;
	const std::string path = stage(&server);

	login_bridge::Ask from = controlFromThisBox("203.0.113.62");
	from.authorization = kRightBasic;
	std::string why;
	login_bridge::Admitted admitted;
	REQUIRE(login_bridge::mayChange(from, "", kRequested, &why, &admitted));

	setAccountLockProbeForTest(&owedThenLook);
	{
		login_bridge::Transaction t;
		REQUIRE(between().ran);
		REQUIRE_FALSE(t.admits(admitted, &why));
		REQUIRE(why.find("PUT /api/v1/account/password") != std::string::npos);
	}
	REQUIRE(contentOf(path) == between().access_after);
	REQUIRE(contentOf(server) == between().server_after);
	REQUIRE(verifySecret(kPassword, loginInEffect().password_hash));

	::unlink(path.c_str());
	::unlink(server.c_str());
	setConfigForTest(before);
}

TEST_CASE("behind a proxy the box itself and the addresses let in without a login are asked like anybody", "[login]")
{
	/* Behind a proxy every request arrives from the proxy's address, the loopback
	   for one on the box, so the address says nothing about who asks. Configured is
	   the REST layer's reading: a line that names a proxy, an empty one, and one
	   that cannot be read. */
	Account account;
	const WebConfig before = config();
	const char *const lines[] = {
		"", "trusted_proxies=127.0.0.1/32\n", "trusted_proxies=\n", "trusted_proxies=127.0.0.1/32 # in front\n"
	};
	const char *const peers[] = { "127.0.0.1", "198.51.100.23" };

	for (size_t l = 0; l < sizeof(lines) / sizeof(lines[0]); ++l)
	{
		for (size_t i = 0; i < sizeof(peers) / sizeof(peers[0]); ++i)
		{
			INFO("peer " << peers[i] << ", proxy line \"" << lines[l] << "\"");
			const std::string path = controlAccount(std::string(), lines[l]);
			REQUIRE(load(path));
			REQUIRE(proxiesConfigured(config()) == (l != 0));

			CyhookHandler hh;
			hh.Method = M_GET;
			hh.UrlData["url"] = "/control/info";
			hh.UrlData["fullurl"] = "/control/info";
			hh.UrlData["path"] = "/control/";
			hh.UrlData["filename"] = "info";
			hh.UrlData["clientaddr"] = peers[i];
			hh.HeaderList["Host"] = "box.lan";

			Gate gate(true, "198.51.100.");
			const THandleStatus st = gate.Hook_PrepareResponse(&hh);
			CHECK((st == HANDLED_CONTINUE) == (l == 0));
			if (l != 0)
				CHECK(hh.httpStatus == HTTP_UNAUTHORIZED);
			::unlink(path.c_str());
		}
	}

	// An access file that cannot be read may name a proxy: the same.
	setConfigForTest(defaultWebConfig());
	REQUIRE_FALSE(load("/tmp/login-no-such-access-file"));
	CyhookHandler hh;
	hh.Method = M_GET;
	hh.UrlData["url"] = "/control/info";
	hh.UrlData["path"] = "/control/";
	hh.UrlData["filename"] = "info";
	hh.UrlData["clientaddr"] = "127.0.0.1";
	hh.HeaderList["Host"] = "box.lan";
	Gate gate(true, "198.51.100.");
	CHECK(gate.Hook_PrepareResponse(&hh) != HANDLED_CONTINUE);

	setConfigForTest(before);
}

namespace
{

const char kBrowser[] = "Mozilla/5.0 (X11; Linux x86_64; rv:140.0) Gecko/20100101 Firefox/140.0";

} // namespace

TEST_CASE("a browser's Basic pair counts on the targets that change the box only from the box's own page, a script's always",
          "[login]")
{
	Account account;

	login_bridge::Ask a;
	a.path = "/control/";
	a.peer = "203.0.113.30";
	a.host = "box.lan";
	a.authorization = kRightBasic;

	// A script sends the pair because it was written to, from wherever it runs.
	a.user_agent = "curl/8.5.0";
	REQUIRE_FALSE(login_bridge::basicHeldBack(a));
	REQUIRE(login_bridge::basicAdmits(a));
	a.user_agent.clear();
	REQUIRE(login_bridge::basicAdmits(a));

	// A browser sends the pair it keeps with whatever another page makes it ask
	// for, and the pair remembered from the script above does not change that.
	a.user_agent = kBrowser;
	REQUIRE(login_bridge::basicHeldBack(a));
	REQUIRE_FALSE(login_bridge::basicAdmits(a));
	a.sec_fetch_site = "cross-site";
	a.referer = "http://box.lan/dashboard.html";
	REQUIRE_FALSE(login_bridge::basicAdmits(a));
	a.sec_fetch_site = "none";
	REQUIRE_FALSE(login_bridge::basicAdmits(a));

	// From a page of the box, by the session's own rule.
	a.sec_fetch_site = "same-origin";
	REQUIRE_FALSE(login_bridge::basicHeldBack(a));
	REQUIRE(login_bridge::basicAdmits(a));
	a.sec_fetch_site.clear();
	a.referer = "http://box.lan/dashboard.html";
	REQUIRE(login_bridge::basicAdmits(a));
	a.referer = "http://evil.example/?http://box.lan/";
	REQUIRE_FALSE(login_bridge::basicAdmits(a));
	a.referer.clear();
	a.origin = "http://box.lan";
	REQUIRE(login_bridge::basicAdmits(a));
	a.origin = "http://evil.example";
	REQUIRE_FALSE(login_bridge::basicAdmits(a));
	a.origin.clear();

	// A page that only shows something asks nothing of where the request came from,
	// and a page told to run a template command is held like the targets above.
	login_bridge::Ask page = a;
	page.path = "/";
	REQUIRE(login_bridge::basicAdmits(page));
	page.path = "/dashboard.html";
	page.runs_commands = true;
	REQUIRE(login_bridge::basicHeldBack(page));
	REQUIRE_FALSE(login_bridge::basicAdmits(page));

	/* PowerShell's Invoke-WebRequest calls itself Mozilla and is a browser here: held
	   back as one, and through with an Origin header naming the box, the way its
	   owner is told to send it. */
	login_bridge::Ask shell;
	shell.path = "/control/";
	shell.peer = "203.0.113.32";
	shell.host = "box.lan";
	shell.authorization = kRightBasic;
	shell.user_agent = "Mozilla/5.0 (Windows NT; Windows NT 10.0; de-DE) WindowsPowerShell/5.1.19041.4648";
	REQUIRE(login_bridge::basicHeldBack(shell));
	shell.origin = "http://box.lan";
	REQUIRE(login_bridge::basicAdmits(shell));

	// Held back before the pair is checked: a wrong one costs no login attempt.
	forgetLoginAttemptsForTest();
	a.authorization = kWrongBasic;
	REQUIRE_FALSE(login_bridge::basicAdmits(a));
	REQUIRE(loginAttemptCountForTest() == 0);
	// And a request without the scheme is nothing this holds back.
	a.authorization = "Bearer b3duZXI6cmlnaHQtb25l";
	REQUIRE_FALSE(login_bridge::basicHeldBack(a));
}

namespace
{

// /control/standby?on as a browser's top-level navigation with the account's Basic pair.
void standbyAsked(CyhookHandler &hh, const char *authorization, const char *agent)
{
	hh.Method = M_GET;
	hh.UrlData["url"] = "/control/standby";
	hh.UrlData["fullurl"] = "/control/standby?on";
	hh.UrlData["path"] = "/control/";
	hh.UrlData["filename"] = "standby";
	hh.UrlData["clientaddr"] = "203.0.113.31";
	hh.HeaderList["Host"] = "box.lan";
	hh.HeaderList["Authorization"] = authorization;
	hh.HeaderList["User-Agent"] = agent;
	hh.HeaderList["Accept"] = "text/html";
}

} // namespace

TEST_CASE("the gate tells a browser held back with its Basic pair where to open the address", "[login]")
{
	Account account;
	Gate gate(true, "");

	CyhookHandler hh;
	standbyAsked(hh, kRightBasic, kBrowser);
	REQUIRE(gate.Hook_PrepareResponse(&hh) == HANDLED_ERROR);
	// Not 401, which would have the browser ask for the login and send this again.
	CHECK(hh.httpStatus == HTTP_FORBIDDEN);
	CHECK(hh.ResponseMimeType == "text/plain; charset=UTF-8");
	CHECK(hh.yresult.find("web interface") != std::string::npos);
	CHECK(hh.yresult.find("Weboberfl") != std::string::npos);
	// A script with a browser's User-Agent is told how it passes, in both languages.
	CHECK(hh.yresult.find("as PowerShell does") != std::string::npos);
	CHECK(hh.yresult.find("wie PowerShell") != std::string::npos);
	CHECK(hh.yresult.find("of its own (-UserAgent), or an Origin header naming the box") != std::string::npos);
	CHECK(hh.yresult.find("eigene Kennung (-UserAgent) oder einen Origin-Kopf") != std::string::npos);
	// The head the boundary's page goes out with.
	CHECK(hh.api_mode);
	bool nosniff = false;
	for (size_t i = 0; i < hh.ResponseHeaders.size(); ++i)
		nosniff = nosniff || (hh.ResponseHeaders[i].first == "X-Content-Type-Options" &&
		                      hh.ResponseHeaders[i].second == "nosniff");
	CHECK(nosniff);

	// The same from a page of the box goes on, and so does a script without one.
	CyhookHandler own;
	standbyAsked(own, kRightBasic, kBrowser);
	own.HeaderList["Sec-Fetch-Site"] = "same-origin";
	CHECK(gate.Hook_PrepareResponse(&own) == HANDLED_CONTINUE);

	CyhookHandler script;
	standbyAsked(script, kRightBasic, "curl/8.5.0");
	CHECK(gate.Hook_PrepareResponse(&script) == HANDLED_CONTINUE);

	// A wrong pair from such a browser is held back the same way: asking it
	// for the pair again would bring the same request back.
	CyhookHandler wrong;
	standbyAsked(wrong, kWrongBasic, kBrowser);
	CHECK(gate.Hook_PrepareResponse(&wrong) == HANDLED_ERROR);
	CHECK(wrong.httpStatus == HTTP_FORBIDDEN);
}

namespace
{

int removeEntry(const char *path, const struct stat *, int, struct FTW *)
{
	return remove(path);
}

/* A web directory with a sign-in page of its own, as this interface installs
   one, and pages, a script and an extension in it. */
struct WebDir
{
	std::string root, core;

	WebDir()
	{
		char dir[] = "/tmp/test_signin_next_XXXXXX";
		REQUIRE(mkdtemp(dir) != NULL);
		root = dir;
		core = root + "/core";
		const char *const files[] = {
			"/login.html", "/index.html", "/dashboard.html", "/OLD.HTM", "/js/app.js",
			"/extensions/files/index.html", "/extensions/files/files.js",
		};
		REQUIRE(mkdir(core.c_str(), 0755) == 0);
		REQUIRE(mkdir((core + "/js").c_str(), 0755) == 0);
		REQUIRE(mkdir((core + "/extensions").c_str(), 0755) == 0);
		REQUIRE(mkdir((core + "/extensions/files").c_str(), 0755) == 0);
		for (size_t i = 0; i < sizeof(files) / sizeof(files[0]); ++i)
		{
			const int fd = open((core + files[i]).c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
			REQUIRE(fd >= 0);
			REQUIRE(write(fd, "x", 1) == 1);
			close(fd);
		}
	}
	~WebDir()
	{
		nftw(root.c_str(), removeEntry, 16, FTW_DEPTH | FTW_PHYS);
	}
};

/* Where the gate sends a browser that opens target without a login, the
   address as nhttpd's reader parses it. Empty where it sends it nowhere. */
std::string signInFor(CmAuth &gate, const WebDir &web, const std::string &target)
{
	CWebserverRequest r;
	r.analyzeURL(target);
	// as ParseStartLine keeps it: the target as written, escapes and all
	r.UrlData["target"] = target;
	CStringList headers, config;
	headers["Host"] = "box.lan";
	headers["Accept"] = "text/html,application/xhtml+xml";
	headers["User-Agent"] = kBrowser;
	config["WebsiteMain.directory"] = web.core;
	config["WebsiteMain.override_directory"] = web.root + "/override";
	r.UrlData["clientaddr"] = "203.0.113.40";
	CyhookHandler hh;
	hh.session_init(r.ParameterList, r.UrlData, headers, config, M_GET, false);
	if (gate.Hook_PrepareResponse(&hh) != HANDLED_REDIRECTION)
		return std::string();
	return hh.NewURL;
}

} // namespace

TEST_CASE("a browser sent to sign in is sent back to a page only, never to a command", "[login]")
{
	/* Once signed in, the sign-in page opens where the browser was going, and
	   that request comes from a page of this box: the one thing a command of
	   the old server asks of a session. So a link to /control/standby?on on
	   another site would have the box go to standby when its user signs in.
	   The sign-in page is given no target then, and starts at the start page. */
	Account account;
	Gate gate(true, "");
	WebDir web;

	struct Case
	{
		const char *target;
		const char *sent_to;
	};
	const Case cases[] = {
		// the pages, with what they were asked with
		{ "/", "/login.html?next=/" },
		{ "/dashboard.html", "/login.html?next=/dashboard.html" },
		{ "/dashboard.html?tab=a", "/login.html?next=/dashboard.html%3Ftab%3Da" },
		{ "/OLD.HTM", "/login.html?next=/OLD.HTM" },
		{ "/extensions/files/", "/login.html?next=/extensions/files/" },
		{ "/extensions/files/index.html", "/login.html?next=/extensions/files/index.html" },
		// the old server's commands, under each of its prefixes and as a template command
		{ "/control/standby?on", "/login.html" },
		{ "/control/", "/login.html" },
		{ "/control/exec.html?x", "/login.html" },
		{ "/cgi-bin/zapTo?path=1", "/login.html" },
		{ "/fb/remote.html", "/login.html" },
		{ "/api/v4/standby", "/login.html" },
		{ "/y/cgi?x", "/login.html" },
		{ "/dashboard.html?execute=script:anything", "/login.html" },
		// what is not a page, and a path the browser reads as another one
		{ "/extensions/files/files.js", "/login.html" },
		{ "/logout.sh", "/login.html" },
		{ "/x/../control/standby.html", "/login.html" },
		{ "/x/%2E%2E/control/standby.html", "/login.html" },
		/* an escaped ? or # in the path, decoded into it: a folder or a page
		   by the decoded name, a command once the browser reads it afresh */
		{ "/control/standby%3Fon%23/", "/login.html" },
		{ "/control/standby%3Fon%26a=/x.html", "/login.html" },
		// an escaped slash or backslash, another site once decoded
		{ "/%2Fevil.example/", "/login.html" },
		{ "/%5Cevil.example/", "/login.html" },
		// a page's query comes back as it was written, its escapes kept
		{ "/dashboard.html?path=%2Fmedia%26x", "/login.html?next=/dashboard.html%3Fpath%3D%252Fmedia%2526x" },
	};
	for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); ++i)
	{
		INFO(cases[i].target);
		CHECK(signInFor(gate, web, cases[i].target) == cases[i].sent_to);
	}
}
