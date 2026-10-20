/*
 * test_yhttpd_static.cpp - which file a request ends up at, and whether it
 * may go out without a login
 *
 * Copyright (C) 2026 Thilo Graf
 *
 * License: GPL-2.0-or-later
 *
 * The files of the web directory load before anybody signs in. Everything
 * else stays behind the login: the override and hosted directories, any
 * other file on the box, templates, scripts, extensions and the spaces the
 * other hooks answer. What is checked is the file that was opened, and
 * that descriptor is the one sent, so a name swapped for a link after the
 * check changes nothing.
 */

#include "support/catch.hpp"

#include "helper.h"
#include "yhook.h"
#include "ystatic.h"
#include "yhttpd_mods/mod_cache.h"

#include <configfile.h>

#include <climits>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include <fcntl.h>
#include <ftw.h>
#include <sys/stat.h>
#include <unistd.h>

namespace {

int removeEntry(const char *path, const struct stat *, int, struct FTW *)
{
	return remove(path);
}

// A web directory, an override directory and a place outside both.
struct Box
{
	std::string root, core, over, outside;
	CyhookHandler hh;

	Box()
	{
		char dir[] = "/tmp/test_ystatic_XXXXXX";
		REQUIRE(mkdtemp(dir) != NULL);
		root = dir;
		core = root + "/core";
		over = root + "/override";
		outside = root + "/outside";
		REQUIRE(mkdir(core.c_str(), 0755) == 0);
		REQUIRE(mkdir(over.c_str(), 0755) == 0);
		REQUIRE(mkdir(outside.c_str(), 0755) == 0);
		hh.WebserverConfigList["WebsiteMain.directory"] = core;
		hh.WebserverConfigList["WebsiteMain.override_directory"] = over;
		hh.Method = M_GET;
	}
	~Box()
	{
		nftw(root.c_str(), removeEntry, 16, FTW_DEPTH | FTW_PHYS);
	}

	void file(const std::string &path, const std::string &text)
	{
		const std::string::size_type slash = path.rfind('/');
		if (slash != std::string::npos && slash > 0)
			mkdirs(path.substr(0, slash));
		const int fd = open(path.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
		REQUIRE(fd >= 0);
		REQUIRE(write(fd, text.data(), text.size()) == (ssize_t) text.size());
		close(fd);
	}
	void mkdirs(const std::string &dir)
	{
		std::string at;
		size_t pos = 0;
		while ((pos = dir.find('/', pos + 1)) != std::string::npos) {
			at = dir.substr(0, pos);
			mkdir(at.c_str(), 0755);
		}
		mkdir(dir.c_str(), 0755);
	}
	void link(const std::string &target, const std::string &name)
	{
		REQUIRE(symlink(target.c_str(), name.c_str()) == 0);
	}

	// as analyzeURL leaves a request for url
	void ask(const std::string &url)
	{
		hh.UrlData["url"] = url;
		std::string path, filename, pure, ext;
		if (!ySplitStringLast(url, "/", path, filename))
			path = "/";
		else
			path += "/";
		if (url.length() == 1 || url[url.length() - 1] == '/') {
			path = url;
			filename = "index.html";
		}
		ySplitStringLast(filename, ".", pure, ext);
		hh.UrlData["path"] = path;
		hh.UrlData["filename"] = filename;
		hh.UrlData["fileext"] = ext;
	}

	// the descriptor openPublic answers for url, and what it reads
	int open_public(const std::string &url)
	{
		ask(url);
		return ystatic::openPublic(&hh, ystatic::resolve(&hh));
	}
};

std::string readAll(int fd)
{
	std::string out;
	char buf[256];
	lseek(fd, 0, SEEK_SET);
	ssize_t n;
	while ((n = read(fd, buf, sizeof(buf))) > 0)
		out.append(buf, (size_t) n);
	return out;
}

} // namespace

TEST_CASE("a file is looked for in the override directory first, then in the web directory, each also as .gz", "[nhttpd]")
{
	Box b;
	b.file(b.over + "/css/a.css", "override");
	b.file(b.core + "/css/a.css", "core");
	b.file(b.core + "/css/b.css", "core");
	b.file(b.core + "/js/c.js.gz", "gz");

	CHECK(ystatic::resolveName(&b.hh, "/css/", "a.css").origin == ystatic::Target::OVERRIDE);
	CHECK(ystatic::resolveName(&b.hh, "/css/", "b.css").origin == ystatic::Target::CORE);
	const ystatic::Target gz = ystatic::resolveName(&b.hh, "/js/", "c.js");
	CHECK(gz.origin == ystatic::Target::CORE);
	CHECK(gz.file == b.core + "/js/c.js.gz");
	CHECK(ystatic::resolveName(&b.hh, "/css/", "none.css").origin != ystatic::Target::CORE);
}

namespace {

// the one folder the test below lets through, by its real path
std::string rawLogos;

bool rawLogosOnly(const std::string &real)
{
	return ystatic::rawPlaceAllowed(real, std::vector<std::string>(1, rawLogos), "/nonexistent-tmp");
}

std::string realOf(const std::string &name)
{
	char buf[PATH_MAX];
	return realpath(name.c_str(), buf) ? std::string(buf) : std::string();
}

} // namespace

TEST_CASE("a file outside the web folders is found only where the program's test lets it through", "[nhttpd]")
{
	Box b;
	b.file(b.outside + "/logos/1.png", "logo");
	b.file(b.outside + "/secret.txt", "secret");
	b.link(b.outside + "/secret.txt", b.outside + "/logos/2.png");
	rawLogos = b.outside + "/logos";

	// without a test, which is how a server starts: none at all
	ystatic::setRawPlaceTest(NULL);
	CHECK(ystatic::resolveName(&b.hh, b.outside + "/logos/", "1.png").origin == ystatic::Target::NONE);

	ystatic::setRawPlaceTest(rawLogosOnly);
	const ystatic::Target logo = ystatic::resolveName(&b.hh, b.outside + "/logos/", "1.png");
	CHECK(logo.origin == ystatic::Target::RAW);
	CHECK(logo.file == realOf(b.outside + "/logos/1.png"));
	// a link inside the folder that leads out of it is judged by where it leads
	CHECK(ystatic::resolveName(&b.hh, b.outside + "/logos/", "2.png").origin == ystatic::Target::NONE);
	CHECK(ystatic::resolveName(&b.hh, b.outside + "/", "secret.txt").origin == ystatic::Target::NONE);
	CHECK(ystatic::resolveName(&b.hh, b.outside + "/logos/../", "secret.txt").origin == ystatic::Target::NONE);
	ystatic::setRawPlaceTest(NULL);
}

TEST_CASE("the hosted folder is served by its own name with or without the program's test", "[nhttpd]")
{
	Box b;
	b.file(b.outside + "/hosted/app/index.html", "hosted");
	b.file(b.outside + "/secret.txt", "secret");
	b.hh.WebserverConfigList["WebsiteMain.hosted_directory"] = b.outside + "/hosted";
	ystatic::setRawPlaceTest(NULL);

	const ystatic::Target hosted = ystatic::resolveName(&b.hh, b.outside + "/hosted/app/", "index.html");
	CHECK(hosted.origin == ystatic::Target::RAW);
	CHECK(hosted.file == realOf(b.outside + "/hosted/app/index.html"));
	CHECK(ystatic::resolveName(&b.hh, b.outside + "/", "secret.txt").origin == ystatic::Target::NONE);
	// an empty hosted folder setting holds nothing
	b.hh.WebserverConfigList["WebsiteMain.hosted_directory"] = "";
	CHECK(ystatic::resolveName(&b.hh, b.outside + "/hosted/app/", "index.html").origin == ystatic::Target::NONE);
}

TEST_CASE("the file opened for a name outside the web folders is tested once more", "[nhttpd]")
{
	Box b;
	b.file(b.outside + "/logos/1.png", "logo");
	b.file(b.outside + "/secret.txt", "secret");
	rawLogos = b.outside + "/logos";
	ystatic::setRawPlaceTest(rawLogosOnly);

	int fd = open((b.outside + "/logos/1.png").c_str(), O_RDONLY);
	REQUIRE(fd >= 0);
	CHECK(ystatic::rawOpenedAllowed(&b.hh, fd));
	close(fd);
	// what a link swapped in after the test would open
	fd = open((b.outside + "/secret.txt").c_str(), O_RDONLY);
	REQUIRE(fd >= 0);
	CHECK_FALSE(ystatic::rawOpenedAllowed(&b.hh, fd));
	close(fd);
	CHECK_FALSE(ystatic::rawOpenedAllowed(&b.hh, -1));
	ystatic::setRawPlaceTest(NULL);
}

TEST_CASE("the box's raw places: inside a logo folder, or a .png directly in /tmp", "[nhttpd]")
{
	Box b;
	b.file(b.outside + "/logos/sub/1.png", "logo");
	b.file(b.outside + "/logos-not/1.png", "neighbour");
	b.file(b.root + "/tmp/shot.png", "png");
	b.file(b.root + "/tmp/sub/shot.png", "png");
	b.file(b.root + "/tmp/list.m3u", "m3u");
	b.file(b.root + "/tmp/.png", "nameless");
	const std::vector<std::string> dirs(1, b.outside + "/logos");
	const std::string tmp = b.root + "/tmp";

	CHECK(ystatic::rawPlaceAllowed(realOf(b.outside + "/logos/sub/1.png"), dirs, tmp));
	CHECK_FALSE(ystatic::rawPlaceAllowed(realOf(b.outside + "/logos"), dirs, tmp));
	CHECK_FALSE(ystatic::rawPlaceAllowed(realOf(b.outside + "/logos-not/1.png"), dirs, tmp));
	CHECK(ystatic::rawPlaceAllowed(realOf(tmp + "/shot.png"), dirs, tmp));
	CHECK_FALSE(ystatic::rawPlaceAllowed(realOf(tmp + "/sub/shot.png"), dirs, tmp));
	CHECK_FALSE(ystatic::rawPlaceAllowed(realOf(tmp + "/list.m3u"), dirs, tmp));
	CHECK_FALSE(ystatic::rawPlaceAllowed(realOf(tmp + "/.png"), dirs, tmp));
	CHECK_FALSE(ystatic::rawPlaceAllowed("relative/1.png", dirs, tmp));
	// an empty folder or / in the list lets nothing through
	std::vector<std::string> wide;
	wide.push_back("");
	wide.push_back("/");
	CHECK_FALSE(ystatic::rawPlaceAllowed(realOf(b.outside + "/logos/sub/1.png"), wide, "/nonexistent-tmp"));
}

TEST_CASE("a name with a .. segment or a NUL is found nowhere", "[nhttpd]")
{
	Box b;
	b.file(b.core + "/css/a.css", "core");
	b.file(b.core + "/css/a..b.css", "dots");
	b.file(b.outside + "/secret.txt", "secret");
	b.file(b.outside + "/hosted/x.txt", "hosted");
	b.hh.WebserverConfigList["WebsiteMain.hosted_directory"] = b.outside + "/hosted";
	rawLogos = b.outside;
	ystatic::setRawPlaceTest(rawLogosOnly);

	// out of the web folder, the override folder, the hosted folder and a raw place
	CHECK(ystatic::resolveName(&b.hh, "/../outside/", "secret.txt").origin == ystatic::Target::NONE);
	CHECK(ystatic::resolveName(&b.hh, "/css/../../outside/", "secret.txt").origin == ystatic::Target::NONE);
	CHECK(ystatic::resolveName(&b.hh, b.outside + "/hosted/../", "secret.txt").origin == ystatic::Target::NONE);
	CHECK(ystatic::resolveName(&b.hh, "/css/", "..").origin == ystatic::Target::NONE);
	// a NUL that would cut the name short for open()
	CHECK(ystatic::resolveName(&b.hh, "/css/", std::string("a.css\0.png", 11)).origin == ystatic::Target::NONE);
	CHECK(ystatic::resolveName(&b.hh, b.outside + "/", std::string("secret.txt\0.png", 16)).origin == ystatic::Target::NONE);
	// dots inside a segment are a name like any other
	CHECK(ystatic::resolveName(&b.hh, "/css/", "a..b.css").origin == ystatic::Target::CORE);
	CHECK(ystatic::resolveName(&b.hh, "/css/", "a.css").origin == ystatic::Target::CORE);
	ystatic::setRawPlaceTest(NULL);
}

TEST_CASE("the spaces other hooks answer are reserved as themselves and with anything below", "[nhttpd]")
{
	const char *const reserved[] = {
		"/api", "/api/v1/session", "/control/exec", "/cgi-bin/x", "/fb/timer.dbox2",
		"/y/cgi", "/y", "/hosted/x.html", "/extensions/hallo/manifest.json", "/extensions",
		"/login", "/logout"
	};
	for (size_t i = 0; i < sizeof(reserved) / sizeof(reserved[0]); i++)
		CHECK(ystatic::inReservedSpace(reserved[i]));
	const char *const open[] = {
		"/", "/index.html", "/apis", "/yweb", "/login.yhtm", "/login.html", "/css/x.css",
		"/controller.js", "/extensionsx"
	};
	for (size_t i = 0; i < sizeof(open) / sizeof(open[0]); i++)
		CHECK_FALSE(ystatic::inReservedSpace(open[i]));
}

TEST_CASE("templates and scripts are known by their name in any case and compressed", "[nhttpd]")
{
	CHECK(ystatic::templateOrScript("/x/a.yhtm"));
	CHECK(ystatic::templateOrScript("/x/a.YHTM"));
	CHECK(ystatic::templateOrScript("/x/a.yhtml"));
	CHECK(ystatic::templateOrScript("/x/a.yjs"));
	CHECK(ystatic::templateOrScript("/x/a.ysh"));
	CHECK(ystatic::templateOrScript("/x/a.sh"));
	CHECK(ystatic::templateOrScript("/x/a.sh.gz"));
	CHECK_FALSE(ystatic::templateOrScript("/x/a.js"));
	CHECK_FALSE(ystatic::templateOrScript("/x/fresh.html"));
}

TEST_CASE("a regular file of the web directory goes out without a login", "[nhttpd]")
{
	Box b;
	b.file(b.core + "/css/webui.css", "body{}");
	b.file(b.core + "/index.html", "<html>");

	const int css = b.open_public("/css/webui.css");
	REQUIRE(css >= 0);
	CHECK(readAll(css) == "body{}");
	close(css);

	const int index = b.open_public("/");
	REQUIRE(index >= 0);
	CHECK(readAll(index) == "<html>");
	close(index);
}

TEST_CASE("nothing outside the web directory's own files goes out without a login", "[nhttpd]")
{
	Box b;
	b.file(b.core + "/css/webui.css", "core");
	b.file(b.over + "/css/webui.css", "override over core");
	b.file(b.over + "/css/own.css", "override only");
	b.file(b.core + "/extensions/hallo/hallo.js", "extension");
	b.file(b.core + "/page.yhtm", "template");
	b.file(b.core + "/scripts/run.sh", "script");
	b.file(b.core + "/scripts/zip.sh.gz", "script");
	b.file(b.outside + "/secret.txt", "secret");
	b.link(b.outside + "/secret.txt", b.core + "/out.txt");
	b.link(b.core + "/extensions/hallo/hallo.js", b.core + "/ext.js");
	b.mkdirs(b.core + "/adir");

	int fd;
	CHECK((fd = b.open_public("/css/webui.css")) < 0);	// the override copy is what would be sent
	CHECK((fd = b.open_public("/css/own.css")) < 0);
	CHECK((fd = b.open_public("/extensions/hallo/hallo.js")) < 0);
	CHECK((fd = b.open_public("/page.yhtm")) < 0);
	CHECK((fd = b.open_public("/scripts/run.sh")) < 0);
	CHECK((fd = b.open_public("/scripts/zip.sh")) < 0);	// found as .sh.gz
	CHECK((fd = b.open_public("/out.txt")) < 0);	// a link out of the web directory
	CHECK((fd = b.open_public("/ext.js")) < 0);	// a link into extensions/
	CHECK((fd = b.open_public("/adir")) < 0);
	CHECK((fd = b.open_public(b.outside + "/secret.txt")) < 0);	// the raw path
	CHECK((fd = b.open_public("/none.css")) < 0);
}

TEST_CASE("the sign-in page counts only as the web directory's own", "[nhttpd]")
{
	Box b;
	b.file(b.core + "/login.html", "core");
	CHECK(ystatic::ownLoginPage(&b.hh));

	// a copy in the override directory is behind the login
	b.file(b.over + "/login.html", "override");
	CHECK_FALSE(ystatic::ownLoginPage(&b.hh));

	// another interface in the web directory's place has none, nor the
	// sign-in page of the template days
	Box c;
	c.file(c.core + "/index.html", "<shell>");
	c.file(c.core + "/login.yhtm", "template");
	CHECK_FALSE(ystatic::ownLoginPage(&c.hh));

	// a link out of the web directory or into its extensions/
	Box d;
	d.file(d.outside + "/login.html", "elsewhere");
	d.link(d.outside + "/login.html", d.core + "/login.html");
	CHECK_FALSE(ystatic::ownLoginPage(&d.hh));
	Box e;
	e.file(e.core + "/extensions/x/login.html", "extension");
	e.link(e.core + "/extensions/x/login.html", e.core + "/login.html");
	CHECK_FALSE(ystatic::ownLoginPage(&e.hh));

	// a link to a template of the web directory, which never goes out openly
	Box g;
	g.file(g.core + "/dashboard.yhtm", "a template");
	g.link(g.core + "/dashboard.yhtm", g.core + "/login.html");
	CHECK_FALSE(ystatic::ownLoginPage(&g.hh));
}

namespace
{
// Whether what the URL resolves to waits for the login (pageNeedsLogin), the
// file opened as openPublic opens it; -1 where it is not open at all.
int waits(Box &b, const std::string &url)
{
	b.ask(url);
	const ystatic::Target t = ystatic::resolve(&b.hh);
	const int fd = ystatic::openPublic(&b.hh, t);
	if (fd < 0)
		return -1;
	const bool need = ystatic::pageNeedsLogin(&b.hh, t, fd);
	close(fd);
	return need ? 1 : 0;
}
} // namespace

TEST_CASE("the interface's own pages wait for the login, what they load does not", "[nhttpd]")
{
	Box b;
	b.file(b.core + "/login.html", "sign in");
	b.file(b.core + "/dashboard.html", "page");
	b.file(b.core + "/index.html", "start");
	b.file(b.core + "/swagger/index.html", "page in a directory");
	b.file(b.core + "/js/dashboard.js", "script");
	b.file(b.core + "/css/webui.css", "style");
	b.file(b.core + "/languages/de.json", "{}");
	b.file(b.core + "/OLD.HTM", "page under another spelling");
	CHECK(waits(b, "/login.html") == 0);
	CHECK(waits(b, "/dashboard.html") == 1);
	CHECK(waits(b, "/index.html") == 1);
	CHECK(waits(b, "/") == 1);
	CHECK(waits(b, "/swagger/index.html") == 1);
	CHECK(waits(b, "/OLD.HTM") == 1);
	CHECK(waits(b, "/js/dashboard.js") == 0);
	CHECK(waits(b, "/css/webui.css") == 0);
	CHECK(waits(b, "/languages/de.json") == 0);

	// a name that is no page, reaching one, waits as the page does
	b.link(b.core + "/dashboard.html", b.core + "/css/look.css");
	CHECK(waits(b, "/css/look.css") == 1);

	// a sign-in page copied into the override directory keeps the pages
	// behind the login (the copy itself never goes out openly)
	Box o;
	o.file(o.core + "/login.html", "sign in");
	o.file(o.core + "/dashboard.html", "page");
	o.file(o.over + "/login.html", "override");
	CHECK(waits(o, "/dashboard.html") == 1);
	CHECK(waits(o, "/login.html") == -1);

	// the sign-in page reached through a link is the sign-in page, so a
	// browser sent there is not sent there again
	Box l;
	l.file(l.core + "/signin.html", "sign in");
	l.link(l.core + "/signin.html", l.core + "/login.html");
	l.file(l.core + "/dashboard.html", "page");
	CHECK(waits(l, "/login.html") == 0);
	CHECK(waits(l, "/dashboard.html") == 1);

	// another interface in the web directory's place, without a sign-in
	// page of its own, signs in by itself: every file of it stays open
	Box c;
	c.file(c.core + "/index.html", "<shell>");
	c.file(c.core + "/assets/app.js", "app");
	CHECK(waits(c, "/index.html") == 0);
	CHECK(waits(c, "/") == 0);
	CHECK(waits(c, "/assets/app.js") == 0);
}

TEST_CASE("a request let through without a login is not answered from the cache", "[nhttpd]")
{
	Box b;
	b.file(b.core + "/css/webui.css", "the public file");
	CConfigFile config('\t');
	config.setString("mod_cache.cache_directory", b.root + "/cache");
	CStringList list;
	CmodCache cache;
	cache.Hook_ReadConfig(&config, list);
	// a page cached under the public file's address
	cache.AddToCache(&b.hh, "/css/webui.css", "a cached page", "text/html");

	b.ask("/css/webui.css");
	b.hh.UrlData["fullurl"] = "/css/webui.css";
	b.hh.PublicFd = ystatic::openPublic(&b.hh, ystatic::resolve(&b.hh));
	REQUIRE(b.hh.PublicFd >= 0);
	cache.Hook_PrepareResponse(&b.hh);
	CHECK(b.hh.status == HANDLED_NONE);
	CHECK_FALSE(b.hh.cached);
	CHECK(b.hh.NewURL.empty());

	// signed in, the same address still comes from the cache
	b.hh.dropPublicFd();
	cache.Hook_PrepareResponse(&b.hh);
	CHECK(b.hh.cached);
	CmodCache::DeleteCache();
}

TEST_CASE("a FIFO in the web directory is turned down without waiting for a writer", "[nhttpd]")
{
	Box b;
	REQUIRE(mkfifo((b.core + "/status.txt").c_str(), 0644) == 0);
	b.link(b.core + "/status.txt", b.core + "/alias.txt");
	alarm(10);	// opened the plain way, this would wait for a writer for ever
	CHECK(b.open_public("/status.txt") < 0);
	CHECK(b.open_public("/alias.txt") < 0);
	alarm(0);
}

TEST_CASE("a file unlinked after it was opened is no file of the web directory", "[nhttpd]")
{
	Box b;
	b.file(b.core + "/private.yhtm", "template source");
	b.link(b.core + "/private.yhtm", b.core + "/app.js");
	b.file(b.core + "/plain.js", "a script");

	std::string below;
	int fd = open((b.core + "/plain.js").c_str(), O_RDONLY);
	REQUIRE(fd >= 0);
	CHECK(ystatic::ownOpened(&b.hh, fd, below));
	CHECK(below == "plain.js");
	close(fd);

	// the template behind the link goes away between opening and checking:
	// its name would no longer say that it is one
	fd = open((b.core + "/app.js").c_str(), O_RDONLY);
	REQUIRE(fd >= 0);
	REQUIRE(unlink((b.core + "/private.yhtm").c_str()) == 0);
	CHECK_FALSE(ystatic::ownOpened(&b.hh, fd, below));
	close(fd);
}

TEST_CASE("the file checked is the file sent, whatever the name points at afterwards", "[nhttpd]")
{
	Box b;
	b.file(b.core + "/app.js", "the checked file");
	b.file(b.outside + "/secret.txt", "secret");

	const int fd = b.open_public("/app.js");
	REQUIRE(fd >= 0);
	// the name is swapped for a link out of the web directory
	REQUIRE(rename((b.core + "/app.js").c_str(), (b.root + "/moved.js").c_str()) == 0);
	b.link(b.outside + "/secret.txt", b.core + "/app.js");
	CHECK(readAll(fd) == "the checked file");
	close(fd);
}

TEST_CASE("a web directory reached through a link still has its files open", "[nhttpd]")
{
	Box b;
	b.file(b.core + "/js/a.js", "a");
	b.link(b.core, b.root + "/linked");
	b.hh.WebserverConfigList["WebsiteMain.directory"] = b.root + "/linked";
	const int fd = b.open_public("/js/a.js");
	CHECK(fd >= 0);
	if (fd >= 0)
		close(fd);
}

TEST_CASE("a page path is answered with index.html only where the switch says so", "[nhttpd]")
{
	Box b;
	b.file(b.core + "/index.html", "<shell>");

	b.ask("/epg/grid");
	CHECK(ystatic::resolve(&b.hh).origin == ystatic::Target::NONE);

	b.hh.WebserverConfigList["WebsiteMain.spa_fallback"] = "true";
	const ystatic::Target page = ystatic::resolve(&b.hh);
	CHECK(page.origin == ystatic::Target::CORE);
	CHECK(page.fallback);
	CHECK(page.file == b.core + "/index.html");
	const int fd = ystatic::openPublic(&b.hh, page);
	REQUIRE(fd >= 0);
	CHECK(readAll(fd) == "<shell>");
	close(fd);

	// with a slash at the end, which analyzeURL answers as index.html
	b.ask("/epg/");
	CHECK(ystatic::resolve(&b.hh).fallback);
	b.ask("/epg/grid/");
	CHECK(ystatic::resolve(&b.hh).fallback);

	b.ask("/control/info");
	CHECK(ystatic::resolve(&b.hh).origin == ystatic::Target::NONE);
	b.ask("/control/");
	CHECK(ystatic::resolve(&b.hh).origin == ystatic::Target::NONE);
	b.ask("/extensions");
	CHECK(ystatic::resolve(&b.hh).origin == ystatic::Target::NONE);
	b.ask("/missing.js");
	CHECK(ystatic::resolve(&b.hh).origin == ystatic::Target::NONE);
	b.ask("/epg/grid");
	b.hh.Method = M_POST;
	CHECK(ystatic::resolve(&b.hh).origin == ystatic::Target::NONE);
}

TEST_CASE("an index.html that is not the web directory's own file is no page to fall back on", "[nhttpd]")
{
	Box b;
	b.hh.WebserverConfigList["WebsiteMain.spa_fallback"] = "true";
	b.file(b.outside + "/index.html", "elsewhere");
	b.link(b.outside + "/index.html", b.core + "/index.html");
	b.ask("/epg");
	CHECK(ystatic::openPublic(&b.hh, ystatic::resolve(&b.hh)) < 0);

	Box c;
	c.hh.WebserverConfigList["WebsiteMain.spa_fallback"] = "true";
	c.file(c.core + "/extensions/e/index.html", "extension");
	c.link(c.core + "/extensions/e/index.html", c.core + "/index.html");
	c.ask("/epg");
	CHECK(ystatic::openPublic(&c.hh, ystatic::resolve(&c.hh)) < 0);

	Box d;
	d.hh.WebserverConfigList["WebsiteMain.spa_fallback"] = "true";
	d.file(d.over + "/index.html", "override");
	d.file(d.core + "/index.html", "core");
	d.ask("/epg");
	CHECK(ystatic::openPublic(&d.hh, ystatic::resolve(&d.hh)) < 0);

	Box e;
	e.hh.WebserverConfigList["WebsiteMain.spa_fallback"] = "true";
	e.file(e.core + "/index.html.gz", "compressed");
	e.ask("/epg");
	const int fd = ystatic::openPublic(&e.hh, ystatic::resolve(&e.hh));
	CHECK(fd >= 0);
	if (fd >= 0)
		close(fd);

	Box f;
	f.hh.WebserverConfigList["WebsiteMain.spa_fallback"] = "true";
	f.ask("/epg");
	CHECK(ystatic::resolve(&f.hh).origin == ystatic::Target::NONE);
}

TEST_CASE("a page path answered with the index is not a cached page of the same address", "[nhttpd]")
{
	Box b;
	b.hh.WebserverConfigList["WebsiteMain.spa_fallback"] = "true";
	b.file(b.core + "/index.html", "<shell>");
	CConfigFile config('\t');
	config.setString("mod_cache.cache_directory", b.root + "/cache");
	CStringList list;
	CmodCache cache;
	cache.Hook_ReadConfig(&config, list);
	// a template's page, cached under its decoded address with the query
	cache.AddToCache(&b.hh, "/private.yhtm?route=/epg", "a protected page", "text/html");

	// the same address with the ? encoded: no parameters, and a page path
	b.ask("/private.yhtm?route=/epg");
	b.hh.UrlData["fullurl"] = "/private.yhtm?route=/epg";
	const ystatic::Target page = ystatic::resolve(&b.hh);
	REQUIRE(page.fallback);
	b.hh.PublicFd = ystatic::openPublic(&b.hh, page);
	REQUIRE(b.hh.PublicFd >= 0);
	cache.Hook_PrepareResponse(&b.hh);
	CHECK_FALSE(b.hh.cached);
	CHECK(b.hh.NewURL.empty());
	CmodCache::DeleteCache();
}

TEST_CASE("a name with %, + or ? comes back whole from the second URL pass", "[nhttpd]")
{
	const std::string name = "/srv/web/a%41+b?c.css";
	const std::string url = ystatic::reparseSafe(name);
	CHECK(url.find('?') == std::string::npos);
	CHECK(decodeString(url) == name);
}

TEST_CASE("a page and a script or style named without its version are asked for again", "[nhttpd]")
{
	struct Case { const char *mime; const char *ext; const char *v; bool kept; const char *asked; };
	const Case cases[] = {
		{ "text/html", "html", "", false, NULL },
		{ "text/html; charset=UTF-8", "html", "abc", false, NULL },
		// an extension's page names the interface's own files without a version
		{ "text/css", "css", "", false, NULL },
		{ "application/javascript", "js", "", false, NULL },
		{ "application/javascript", "mjs", "", false, NULL },
		// the same answered from its .gz carries gz as its extension, its type stays
		{ "text/css", "gz", "", false, NULL },
		// a type the configuration changed, the name still says what it is
		{ "text/plain", "js", "", false, NULL },
		// both at once: sent from its .gz under a configured type, known by
		// the extension it was asked by
		{ "text/plain", "gz", "", false, "js" },
		{ "application/x-javascript", "gz", "", false, "js" },
		{ "application/gzip", "gz", "", true, "gz" },
		// named by its version: kept, the next version is another address
		{ "text/css", "css", "1f20aec2", true, NULL },
		{ "application/javascript", "js", "1f20aec2", true, NULL },
		// anything else is kept as before
		{ "image/png", "png", "", true, NULL },
		{ "font/woff2", "woff2", "", true, NULL },
	};
	for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); ++i)
	{
		CyhookHandler hh;
		hh.ResponseMimeType = cases[i].mime;
		hh.UrlData["fileext"] = cases[i].ext;
		if (*cases[i].v)
			hh.ParamList["v"] = cases[i].v;
		if (cases[i].asked != NULL)
			hh.HookVarList["sendfile.ext"] = cases[i].asked;
		INFO(cases[i].mime << " ." << cases[i].ext << " v=" << cases[i].v);
		CHECK(ystatic::mayBeKept(&hh) == cases[i].kept);
	}
}
