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
#include <vector>

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
	REQUIRE_FALSE(login_bridge::change("", "from-elsewhere", &why, &from));
	REQUIRE_FALSE(why.empty());
	from.sec_fetch_site = "cross-site";
	from.referer = "http://box.lan/settings.html";
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
