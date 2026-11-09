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
 * check changes nothing. The files that hold who may reach the box go out
 * neither way, by whatever name they are reached.
 */

#include "support/catch.hpp"
#include "support/wireserver.h"
#include "httpd/webconfig.h"
#include "httpd/endpoint.h"
#include "httpd/nhttpd_bridge.h"
#include "coreapi/base/accessfiles.h"

#include "helper.h"
#include "yhook.h"
#include "yprotect.h"
#include "ystatic.h"
#include "yhttpd_mods/mod_cache.h"
#include "yhttpd_mods/mod_sendfile.h"

#include <configfile.h>

#include <atomic>
#include <climits>
#include <map>
#include <thread>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include <dirent.h>
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
	/* nor a logo folder set where no web folder may be (ystatic::rootRefused): the
	   kernel's trees, whose files name what the program holds open */
	const std::vector<std::string> kernel(1, "/proc");
	const std::vector<std::string> devices(1, "/dev");
	CHECK_FALSE(ystatic::rawPlaceAllowed(realOf("/proc/self/status"), kernel, "/nonexistent-tmp"));
	CHECK_FALSE(ystatic::rawPlaceAllowed(realOf("/dev/null"), devices, "/nonexistent-tmp"));
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

namespace {

// What mod_sendfile sends for url, after the login: the body, or "-" where
// it sends no file. handed, where given, is the descriptor mod_auth handed
// on for an answer without a login (PublicFd).
std::string sent(Box &b, const std::string &url, int handed = -1)
{
	CmodSendfile sendfile;
	CConfigFile config(',');
	CStringList list;
	sendfile.Hook_ReadConfig(&config, list);
	b.ask(url);
	b.hh.status = HANDLED_NONE;
	b.hh.dropSendFileFd();
	b.hh.dropPublicFd();
	b.hh.HookVarList.erase("sendfile.public");
	if (handed >= 0) {
		b.hh.PublicFd = handed;
		b.hh.HookVarList["sendfile.public"] = b.core + url;
	}
	sendfile.Hook_PrepareResponse(&b.hh);
	if (b.hh.status != HANDLED_SENDFILE || b.hh.SendFileFd < 0)
		return "-";
	const std::string body = readAll(b.hh.SendFileFd);
	b.hh.dropSendFileFd();
	return body;
}

} // namespace

namespace {
// what the case below sets, set back however it ends
struct SetBack
{
	~SetBack()
	{
		yprotect::setFiles(std::vector<std::string>());
		ystatic::setRawPlaceTest(NULL);
	}
};
} // namespace

TEST_CASE("a file that holds who may reach the box goes out by no name, link or folder", "[nhttpd]")
{
	SetBack setBack;
	Box b;
	b.file(b.core + "/store.txt", "hash");		// a type the server sends
	b.file(b.core + "/nhttpd.conf", "secret");	// a type only sendAll sends
	b.file(b.over + "/over.txt", "token");		// an override file
	b.file(b.outside + "/logos/1.png", "logo");	// a raw place
	b.link(b.core + "/store.txt", b.core + "/alias.txt");
	REQUIRE(link((b.core + "/store.txt").c_str(), (b.core + "/hard.txt").c_str()) == 0);
	REQUIRE(link((b.core + "/store.txt").c_str(), (b.outside + "/logos/2.png").c_str()) == 0);
	// and ordinary files beside them, configuration among them
	b.file(b.core + "/page.txt", "page");
	b.file(b.core + "/other.conf", "ordinary");
	b.file(b.over + "/plain.txt", "plain");
	rawLogos = b.outside + "/logos";
	ystatic::setRawPlaceTest(rawLogosOnly);

	// the store named through another spelling of its folder is the file
	b.link(b.core, b.root + "/core-alias");
	std::vector<std::string> files;
	files.push_back(b.root + "/core-alias/store.txt");
	files.push_back(b.core + "/nhttpd.conf");
	files.push_back(b.over + "/over.txt");
	yprotect::setFiles(files);

	// without a login: mod_auth finds nothing to hand on
	const char *const openly[] = { "/store.txt", "/alias.txt", "/hard.txt", "/nhttpd.conf" };
	for (size_t i = 0; i < sizeof(openly) / sizeof(openly[0]); i++) {
		INFO(openly[i]);
		CHECK(b.open_public(openly[i]) < 0);
	}

	// after it: mod_sendfile sends none of them, whatever sendAll says
	const char *const sendAll[] = { "false", "true" };
	for (size_t s = 0; s < 2; s++) {
		INFO(sendAll[s]);
		b.hh.WebserverConfigList["mod_sendfile.sendAll"] = sendAll[s];
		const std::string names[] = {
			"/store.txt", "/alias.txt", "/hard.txt", "/nhttpd.conf", "/over.txt",
			b.outside + "/logos/2.png"
		};
		for (size_t i = 0; i < sizeof(names) / sizeof(names[0]); i++) {
			INFO(names[i]);
			CHECK(sent(b, names[i]) == "-");
		}
		CHECK(sent(b, "/page.txt") == "page");
		CHECK(sent(b, "/plain.txt") == "plain");
		CHECK(sent(b, b.outside + "/logos/1.png") == "logo");
	}
	// an ordinary configuration file still goes out where sendAll lets it
	CHECK(sent(b, "/other.conf") == "ordinary");

	// a descriptor mod_auth handed on is asked again
	const int store = open((b.core + "/store.txt").c_str(), O_RDONLY);
	REQUIRE(store >= 0);
	CHECK(sent(b, "/store.txt", store) == "-");
	const int page = open((b.core + "/page.txt").c_str(), O_RDONLY);
	REQUIRE(page >= 0);
	CHECK(sent(b, "/page.txt", page) == "page");

	// it is the files named that are kept back: once none are, they go out
	yprotect::setFiles(std::vector<std::string>());
	CHECK(sent(b, "/store.txt") == "hash");
	CHECK(sent(b, "/over.txt") == "token");
	const int fd = b.open_public("/alias.txt");
	CHECK(fd >= 0);
	if (fd >= 0)
		close(fd);
}

TEST_CASE("a file sent by a name it was handed is none that holds who may reach the box", "[nhttpd]")
{
	SetBack setBack;
	Box b;
	b.file(b.outside + "/store.conf", "hash");
	b.file(b.outside + "/logo.png", "logo");
	b.link(b.outside + "/store.conf", b.outside + "/alias.png");
	REQUIRE(link((b.outside + "/store.conf").c_str(), (b.outside + "/hard.png").c_str()) == 0);
	REQUIRE(mkfifo((b.outside + "/fifo.png").c_str(), 0600) == 0);
	std::vector<std::string> files(1, b.outside + "/store.conf");
	yprotect::setFiles(files);

	// an ordinary file opens, measured off what was opened
	struct stat st;
	int fd = ystatic::openToSend(b.outside + "/logo.png", st);
	REQUIRE(fd >= 0);
	CHECK(st.st_size == 4);
	CHECK(readAll(fd) == "logo");
	close(fd);

	// the store by its name, a link to it and a second name of it do not
	const char *const held[] = { "/store.conf", "/alias.png", "/hard.png" };
	for (size_t i = 0; i < sizeof(held) / sizeof(held[0]); i++) {
		INFO(held[i]);
		CHECK(ystatic::openToSend(b.outside + held[i], st) < 0);
	}
	// nor does anything that is no regular file, a FIFO without waiting on it
	CHECK(ystatic::openToSend(b.outside + "/fifo.png", st) < 0);
	CHECK(ystatic::openToSend(b.outside, st) < 0);
	CHECK(ystatic::openToSend(b.outside + "/none.png", st) < 0);
	CHECK(ystatic::openToSend("", st) < 0);

	// it is the file named that is held back: once none is, it opens
	yprotect::setFiles(std::vector<std::string>());
	fd = ystatic::openToSend(b.outside + "/alias.png", st);
	CHECK(fd >= 0);
	if (fd >= 0)
		close(fd);
}

namespace {

// A directory of the case's own, removed with what is in it whichever line
// the case leaves through.
struct OwnDir
{
	std::string path;
	OwnDir()
	{
		char name[] = "/tmp/yprotect-test-XXXXXX";
		path = mkdtemp(name) ? name : "";
	}
	~OwnDir()
	{
		if (!path.empty())
			nftw(path.c_str(), removeOne, 8, FTW_DEPTH | FTW_PHYS);
	}
	static int removeOne(const char *p, const struct stat *, int, struct FTW *)
	{
		chmod(p, 0700);
		return remove(p);
	}
};

std::map<std::string, std::string> lines(const char *value)
{
	std::map<std::string, std::string> m;
	m["mod_auth.password"] = value;
	return m;
}

bool waitTill(const std::atomic<bool> &flag)
{
	for (int waited = 0; waited < 5000 && !flag; waited += 5)
		usleep(5 * 1000);
	return flag;
}

// The write held once it made the working file and before a byte of it is
// written, and let go from between two of isProtected's looks.
struct Replacing
{
	std::atomic<bool> held;
	std::atomic<bool> let_go;
	std::atomic<bool> replaced;
	Replacing() : held(false), let_go(false), replaced(false) {}
};
Replacing *replacing = NULL;

void holdTheWrite(const std::string &)
{
	replacing->held = true;
	waitTill(replacing->let_go);
}

void replaceAfterTheStore(size_t name)
{
	if (name != 0 || replacing == NULL || replacing->let_go)
		return;
	replacing->let_go = true;
	waitTill(replacing->replaced);
}

// the seams back however the case ends
struct SeamsBack
{
	~SeamsBack()
	{
		httpd::setAfterCreateForTest(NULL);
		yprotect::setLookedForTest(NULL);
		replacing = NULL;
		yprotect::setFiles(std::vector<std::string>());
	}
};

} // namespace

/* The store opened, and replaced by a change of the account before what was
   opened is asked about: the descriptor holds the store as it was, which no
   name stands for any more. It is still one of the files, and so is a second
   name the old store kept: the writer kept each file it made. */
TEST_CASE("a store replaced while it was being opened is still held back", "[nhttpd]")
{
	SeamsBack back;
	OwnDir dir;
	REQUIRE_FALSE(dir.path.empty());
	const std::string store = dir.path + "/store.conf";
	std::vector<std::string> files;
	files.push_back(store);
	files.push_back(httpd::workingFile(store));
	// named before the store is there, so that only the writer can say what
	// it stood for
	yprotect::setFiles(files);

	REQUIRE(httpd::saveServerFile(store, lines("old")));
	REQUIRE(link(store.c_str(), (dir.path + "/old-alias.txt").c_str()) == 0);
	const int opened = open(store.c_str(), O_RDONLY);
	REQUIRE(opened >= 0);
	REQUIRE(httpd::saveServerFile(store, lines("new")));

	CHECK(yprotect::isProtected(opened));
	close(opened);
	const int alias = open((dir.path + "/old-alias.txt").c_str(), O_RDONLY);
	REQUIRE(alias >= 0);
	CHECK(yprotect::isProtected(alias));
	close(alias);
	const int now = open(store.c_str(), O_RDONLY);
	REQUIRE(now >= 0);
	CHECK(yprotect::isProtected(now));
	close(now);

	// a file that never was one of them is not
	{
		FILE *f = fopen((dir.path + "/page.txt").c_str(), "w");
		REQUIRE(f != NULL);
		fputs("page", f);
		fclose(f);
	}
	const int page = open((dir.path + "/page.txt").c_str(), O_RDONLY);
	REQUIRE(page >= 0);
	CHECK_FALSE(yprotect::isProtected(page));
	close(page);
}

/* The working file made and opened before a byte of it is written, the store
   looked at, and only then the content written and the working file renamed over
   the store, before its own name is looked at: at its looks neither name stands
   for the opened file, and what the file held at the first look is not what it
   holds when it is read. */
TEST_CASE("a working file opened before it was written and renamed between two looks is still held back", "[nhttpd]")
{
	SeamsBack back;
	OwnDir dir;
	REQUIRE_FALSE(dir.path.empty());
	const std::string store = dir.path + "/store.conf";
	const std::string working = httpd::workingFile(store);
	REQUIRE(httpd::saveServerFile(store, lines("old")));
	std::vector<std::string> files;
	files.push_back(store);
	files.push_back(working);
	yprotect::setFiles(files);

	Replacing r;
	replacing = &r;
	httpd::setAfterCreateForTest(holdTheWrite);
	std::thread writer([&r, &store]() {
		httpd::saveServerFile(store, lines("S3NT1NEL"));
		r.replaced = true;
	});
	const bool held = waitTill(r.held);
	const int opened = held ? open(working.c_str(), O_RDONLY) : -1;
	struct stat before;
	const bool empty = opened >= 0 && fstat(opened, &before) == 0 && before.st_size == 0;
	yprotect::setLookedForTest(replaceAfterTheStore);
	const bool kept = opened >= 0 && yprotect::isProtected(opened);
	yprotect::setLookedForTest(NULL);
	r.let_go = true;
	writer.join();
	httpd::setAfterCreateForTest(NULL);

	REQUIRE(held);
	REQUIRE(opened >= 0);
	// the schedule ran: nothing was written at the open, the working name was
	// gone at its look, and the descriptor now reads the new password
	CHECK(empty);
	CHECK(r.replaced);
	CHECK(access(working.c_str(), F_OK) != 0);
	CHECK(readAll(opened).find("S3NT1NEL") != std::string::npos);
	CHECK(kept);
	close(opened);
}

/* The start that takes the store over from the name it had before makes it under
   its working name and gives it its name afterwards: kept from the moment it was
   made, so the store is one of the files whatever list the server is given. */
TEST_CASE("the access store a start writes is kept from the moment it is made", "[nhttpd]")
{
	SeamsBack back;
	OwnDir dir;
	REQUIRE_FALSE(dir.path.empty());
	yprotect::setFiles(std::vector<std::string>());
	const std::string former = dir.path + "/former.conf";
	const std::string out = dir.path + "/nhttpd-access.conf";
	{
		FILE *f = fopen(former.c_str(), "w");
		REQUIRE(f != NULL);
		fputs("password_hash=S3NT1NEL\n", f);
		fclose(f);
	}
	REQUIRE(httpd::ensureConfigFile(dir.path + "/nhttpd.conf", out, former));
	struct stat st;
	REQUIRE(stat(out.c_str(), &st) == 0);
	CHECK(coreapi::accessfiles::held(st.st_dev, st.st_ino));
	const int fd = open(out.c_str(), O_RDONLY);
	REQUIRE(fd >= 0);
	CHECK(yprotect::isProtected(fd));
	close(fd);
}

namespace {

// Takes the kept file's last name from between two looks and has the kept set
// walk, as keeping any other file does: the file is let go.
std::string *let_go_store = NULL;
std::string let_go_other;

void unlinkAndWalk(size_t name)
{
	if (name != 0 || let_go_store == NULL)
		return;
	const std::string store = *let_go_store;
	let_go_store = NULL;
	unlink(store.c_str());
	coreapi::accessfiles::named(let_go_other);
}

} // namespace

/* A kept file opened while it still had a name, its name taken and the file let
   go from the kept set while it is being asked about: the set no longer knows it,
   and the look at its link count that comes after the set does. */
TEST_CASE("a kept file let go between the looks is still held back", "[nhttpd]")
{
	SeamsBack back;
	OwnDir dir;
	REQUIRE_FALSE(dir.path.empty());
	// listed: a name that never stands for it, so only the kept set and the
	// link count can tell
	yprotect::setFiles(std::vector<std::string>(1, dir.path + "/other.conf"));
	const std::string store = dir.path + "/store.conf";
	REQUIRE(httpd::saveServerFile(store, lines("S3NT1NEL")));
	const int opened = open(store.c_str(), O_RDONLY);
	REQUIRE(opened >= 0);
	struct stat st;
	REQUIRE(fstat(opened, &st) == 0);
	REQUIRE(st.st_nlink == 1u);

	std::string name = store;
	let_go_store = &name;
	let_go_other = dir.path + "/other.conf";
	{
		FILE *f = fopen(let_go_other.c_str(), "w");
		REQUIRE(f != NULL);
		fclose(f);
	}
	yprotect::setLookedForTest(unlinkAndWalk);
	const bool kept = yprotect::isProtected(opened);
	yprotect::setLookedForTest(NULL);
	REQUIRE(fstat(opened, &st) == 0);
	CHECK(st.st_nlink == 0u);
	CHECK_FALSE(coreapi::accessfiles::held(st.st_dev, st.st_ino));
	CHECK(kept);
	close(opened);
}

namespace {

// The descriptors this process holds, by number.
size_t openDescriptors()
{
	size_t n = 0;
	DIR *d = opendir("/proc/self/fd");
	if (d == NULL)
		return 0;
	while (readdir(d) != NULL)
		n++;
	closedir(d);
	return n;
}

// Whether nothing holds the kept set's lock, which a failure must not leave held;
// asked without waiting, so a lock left held fails the case and hangs nothing.
bool keptSetAnswers()
{
	return coreapi::accessfiles::unlockedForTest();
}

struct FailBack
{
	~FailBack() { coreapi::accessfiles::setFailForTest(coreapi::accessfiles::FailNone); }
};

} // namespace

/* A file that cannot be kept is not written at all: no descriptor left to
   duplicate, or no memory for its entry. The file it was to replace stays as it
   was, nothing is left beside it, and the kept set neither holds a descriptor nor
   its lock. */
TEST_CASE("a store that cannot be kept is not written, and the failure leaves nothing behind", "[nhttpd]")
{
	SeamsBack back;
	FailBack fail_back;
	OwnDir dir;
	REQUIRE_FALSE(dir.path.empty());
	const std::string store = dir.path + "/store.conf";
	REQUIRE(httpd::saveServerFile(store, lines("old")));

	const coreapi::accessfiles::Fail fails[] = { coreapi::accessfiles::FailDup, coreapi::accessfiles::FailAlloc };
	for (size_t i = 0; i < sizeof(fails) / sizeof(fails[0]); i++) {
		INFO(i);
		const size_t before = openDescriptors();
		coreapi::accessfiles::setFailForTest(fails[i]);
		CHECK_FALSE(httpd::saveServerFile(store, lines("S3NT1NEL")));
		const int fd = open(store.c_str(), O_RDONLY);
		REQUIRE(fd >= 0);
		CHECK_FALSE(coreapi::accessfiles::created(fd));
		// named opens the file itself and duplicates nothing
		CHECK(coreapi::accessfiles::named(store) == (fails[i] == coreapi::accessfiles::FailDup));
		close(fd);
		coreapi::accessfiles::setFailForTest(coreapi::accessfiles::FailNone);
		CHECK(openDescriptors() == before);
		CHECK(keptSetAnswers());
		const int again = open(store.c_str(), O_RDONLY | O_CLOEXEC);
		REQUIRE(again >= 0);
		CHECK(readAll(again).find("old") != std::string::npos);
		close(again);
		CHECK(access(httpd::workingFile(store).c_str(), F_OK) != 0);
	}
	// and once it can be kept again, it is written
	CHECK(httpd::saveServerFile(store, lines("new")));
	// a name with no file there is nothing to keep, and no failure
	CHECK(coreapi::accessfiles::named(dir.path + "/none.conf"));
}

namespace {

// A working file the program left when it ended in the middle of a write, with a
// second name, and a start that remembers nothing of it.
void leaveWorking(const std::string &path, const std::string &alias)
{
	const std::string working = httpd::workingFile(path);
	FILE *f = fopen(working.c_str(), "w");
	REQUIRE(f != NULL);
	fputs("password_hash=S3NT1NEL\n", f);
	fclose(f);
	unlink(alias.c_str());
	REQUIRE(link(working.c_str(), alias.c_str()) == 0);
	coreapi::accessfiles::forgetAll();
}

bool aliasHeldBack(const std::string &alias)
{
	const int fd = open(alias.c_str(), O_RDONLY | O_CLOEXEC);
	if (fd < 0)
		return false;
	const bool held = yprotect::isProtected(fd);
	close(fd);
	return held;
}

} // namespace

/* Such a file is kept before its working name goes, by the save's writer and by
   the first start's: its second name stays held back. Where the start cannot keep
   it, it stays where it is, held back by its name, and nothing is written. */
TEST_CASE("a working file a write left behind is kept before its name goes, or stays", "[nhttpd]")
{
	SeamsBack back;
	FailBack fail_back;
	OwnDir dir;
	REQUIRE_FALSE(dir.path.empty());
	yprotect::setFiles(std::vector<std::string>());
	const std::string alias = dir.path + "/alias.txt";

	// the save's writer
	const std::string store = dir.path + "/store.conf";
	REQUIRE(httpd::saveServerFile(store, lines("old")));
	leaveWorking(store, alias);
	REQUIRE(httpd::saveServerFile(store, lines("new")));
	CHECK(access(httpd::workingFile(store).c_str(), F_OK) != 0);
	CHECK(aliasHeldBack(alias));

	// the first start's
	const std::string server = dir.path + "/nhttpd.conf";
	const std::string out = dir.path + "/nhttpd-access.conf";
	leaveWorking(out, alias);
	REQUIRE(httpd::ensureConfigFile(server, out, std::string()));
	CHECK(access(httpd::workingFile(out).c_str(), F_OK) != 0);
	CHECK(aliasHeldBack(alias));

	// a start that can keep nothing
	REQUIRE(unlink(out.c_str()) == 0);
	leaveWorking(out, alias);
	coreapi::accessfiles::setFailForTest(coreapi::accessfiles::FailAlloc);
	CHECK_FALSE(httpd::ensureConfigFile(server, out, std::string()));
	std::vector<std::string> names;
	names.push_back(out);
	names.push_back(httpd::workingFile(out));
	CHECK_FALSE(yprotect::setFiles(names));
	coreapi::accessfiles::setFailForTest(coreapi::accessfiles::FailNone);
	CHECK(access(httpd::workingFile(out).c_str(), F_OK) == 0);
	CHECK(access(out.c_str(), F_OK) != 0);
	CHECK(coreapi::accessfiles::count() == 0u);
	CHECK(aliasHeldBack(alias));
}

/* What is kept is no more than the files that still have a name: a store
   written again and again keeps one or two, not one for every write. */
TEST_CASE("the files kept since they were made are let go once no name reaches them", "[nhttpd]")
{
	SeamsBack back;
	OwnDir dir;
	REQUIRE_FALSE(dir.path.empty());
	yprotect::setFiles(std::vector<std::string>());
	const std::string store = dir.path + "/store.conf";
	for (int i = 0; i < 20; i++)
		REQUIRE(httpd::saveServerFile(store, lines("again")));
	CHECK(coreapi::accessfiles::count() <= 2u);
	// a second name keeps the file it names
	REQUIRE(link(store.c_str(), (dir.path + "/alias.txt").c_str()) == 0);
	REQUIRE(httpd::saveServerFile(store, lines("once more")));
	REQUIRE(httpd::saveServerFile(store, lines("and again")));
	const int alias = open((dir.path + "/alias.txt").c_str(), O_RDONLY);
	REQUIRE(alias >= 0);
	CHECK(yprotect::isProtected(alias));
	close(alias);
	CHECK(coreapi::accessfiles::count() <= 3u);
}

/* A file that no name reaches any more cannot be told apart from what it was:
   an unlinked capture that waits for captureguard, a store replaced while it was
   opened. It goes out through no gate, the override folder pointed at /proc,
   where /proc/self/fd/ names such files, and everything sent included. */
TEST_CASE("a file no name reaches any more goes out through no gate, whatever the folders are", "[nhttpd]")
{
	SeamsBack back;
	Box b;
	b.file(b.outside + "/pending.txt", "S3NT1NEL");
	b.file(b.outside + "/plain.txt", "plain");
	const int pending = open((b.outside + "/pending.txt").c_str(), O_RDONLY);
	REQUIRE(pending >= 0);
	REQUIRE(unlink((b.outside + "/pending.txt").c_str()) == 0);
	const int plain = open((b.outside + "/plain.txt").c_str(), O_RDONLY);
	REQUIRE(plain >= 0);
	const std::string pending_name = "/self/fd/" + std::to_string(pending);
	const std::string plain_name = "/self/fd/" + std::to_string(plain);

	CHECK(yprotect::isProtected(pending));
	CHECK_FALSE(yprotect::isProtected(plain));
	// a REST answer from such a file goes out only where the route made it
	// and took its name itself, as a playlist written to /tmp is
	CHECK(yprotect::isProtected(pending, false));
	CHECK_FALSE(yprotect::isProtected(pending, true));
	struct stat st;
	CHECK(ystatic::openToSend("/proc" + pending_name, st) < 0);

	// an override folder that came past the reading of the configuration
	b.hh.WebserverConfigList["WebsiteMain.override_directory"] = "/proc";
	b.hh.WebserverConfigList["mod_sendfile.sendAll"] = "true";
	CHECK(sent(b, pending_name) == "-");
	CHECK(b.open_public(pending_name) < 0);
	// it is the missing name and not the folder that keeps it back
	CHECK(sent(b, plain_name) == "plain");
	close(pending);
	close(plain);
}

/* A REST answer out of a file without a name goes out only where the route made
   the file for the answer and took its name itself, as a playlist written to
   /tmp is; any other is refused like one of the files that hold who may reach
   the box. */
TEST_CASE("a REST answer out of a file without a name goes out only where the route made it", "[nhttpd]")
{
	SeamsBack back;
	OwnDir dir;
	REQUIRE_FALSE(dir.path.empty());
	const std::string name = dir.path + "/answer.m3u";
	struct Answer
	{
		bool own;
		bool sent;
	};
	const Answer answers[] = { { false, false }, { true, true } };
	for (size_t i = 0; i < sizeof(answers) / sizeof(answers[0]); i++) {
		INFO(answers[i].own);
		{
			FILE *f = fopen(name.c_str(), "w");
			REQUIRE(f != NULL);
			fputs("#EXTM3U\n", f);
			fclose(f);
		}
		const int fd = open(name.c_str(), O_RDONLY | O_CLOEXEC);
		REQUIRE(fd >= 0);
		REQUIRE(unlink(name.c_str()) == 0);
		httpd::Response r;
		r.code = 200;
		r.content_type = "audio/x-mpegurl";
		REQUIRE((answers[i].own ? httpd::answerFromOwnUnnamed(r, fd) : httpd::answerFromDescriptor(r, fd)));
		CyhookHandler hh;
		CApiBridge::answerWith(&hh, r);
		if (answers[i].sent) {
			CHECK(hh.status == HANDLED_SENDFD);
			CHECK(hh.SendFd >= 0);
		} else {
			CHECK(hh.status != HANDLED_SENDFD);
			CHECK(hh.httpStatus == 404);
		}
		if (hh.SendFd >= 0) {
			close(hh.SendFd);
			hh.SendFd = -1;
		}
	}
}

namespace {
struct KeptBack
{
	~KeptBack() { yprotect::setKeptPlacesForTest(std::vector<std::string>()); }
};
} // namespace

/* A folder link inside an allowed web folder that leads into the configuration
   directory, or into /proc, /sys or /dev, reaches nothing there: every gate asks
   where the opened file really lies. */
TEST_CASE("a link from a web folder into the configuration directory or a kernel tree reaches nothing", "[nhttpd]")
{
	SeamsBack back;
	KeptBack kept_back;
	Box b;
	const std::string config = b.root + "/config";
	b.file(config + "/neutrino.conf", "tmdb_api_key=S3NT1NEL");
	b.file(b.over + "/plain.txt", "plain");
	b.link(config, b.over + "/cfg");
	b.link(config, b.core + "/cfg");
	b.link("/proc/self", b.over + "/self");
	std::vector<std::string> kept;
	kept.push_back(config);
	kept.push_back("/proc");
	kept.push_back("/sys");
	kept.push_back("/dev");
	yprotect::setKeptPlacesForTest(kept);
	b.hh.WebserverConfigList["mod_sendfile.sendAll"] = "true";

	CHECK(sent(b, "/cfg/neutrino.conf") == "-");
	CHECK(sent(b, "/self/status") == "-");
	CHECK(b.open_public("/cfg/neutrino.conf") < 0);
	struct stat st;
	CHECK(ystatic::openToSend(b.over + "/cfg/neutrino.conf", st) < 0);
	// a name the program fixes there itself, as the channel lists are
	const int fixed = ystatic::openToSend(b.over + "/cfg/neutrino.conf", st, true);
	CHECK(fixed >= 0);
	if (fixed >= 0)
		close(fixed);
	// what lies in the folder itself goes out as before
	CHECK(sent(b, "/plain.txt") == "plain");

	// a descriptor whose place cannot be told is in none of them, and refused
	int pipes[2];
	REQUIRE(pipe(pipes) == 0);
	CHECK(yprotect::inKeptPlace(pipes[0]));
	close(pipes[0]);
	close(pipes[1]);
}

TEST_CASE("no web folder is, holds or lies in the configuration directory or a kernel tree", "[nhttpd]")
{
	OwnDir dir;
	REQUIRE_FALSE(dir.path.empty());
	const std::string config = dir.path + "/config";
	REQUIRE(mkdir(config.c_str(), 0700) == 0);
	REQUIRE(mkdir((dir.path + "/web").c_str(), 0700) == 0);
	REQUIRE(symlink("/proc", (dir.path + "/to-proc").c_str()) == 0);
	REQUIRE(symlink(config.c_str(), (dir.path + "/to-config").c_str()) == 0);
	std::vector<std::string> kept;
	kept.push_back(config);
	kept.push_back("/proc");
	kept.push_back("/sys");
	kept.push_back("/dev");

	const std::string refused[] = {
		config, config + "/", config + "/sub", dir.path, "/", "/tmp", "/tmp/", "/proc", "/proc/self/fd",
		"/sys/class", "/dev", "/dev/shm", dir.path + "/to-proc", dir.path + "/to-config",
		dir.path + "/to-proc/self", "relative/web", dir.path + "/web/../config", ""
	};
	for (size_t i = 0; i < sizeof(refused) / sizeof(refused[0]); i++) {
		INFO(refused[i]);
		CHECK_FALSE(ystatic::rootRefused(refused[i], kept).empty());
	}
	const std::string allowed[] = {
		dir.path + "/web", dir.path + "/web/", dir.path + "/not-there-yet", dir.path + "/configured"
	};
	for (size_t i = 0; i < sizeof(allowed) / sizeof(allowed[0]); i++) {
		INFO(allowed[i]);
		CHECK(ystatic::rootRefused(allowed[i], kept).empty());
	}
	// /tmp itself, and a folder holding it, whatever else is kept
	const std::vector<std::string> trees(kept.begin() + 1, kept.end());
	CHECK_FALSE(ystatic::rootRefused("/tmp", trees).empty());
	CHECK_FALSE(ystatic::rootRefused("/tmp/", trees).empty());
	CHECK(ystatic::rootRefused(dir.path + "/web", trees).empty());
	// the box's own list names its configuration directory and the three trees
	const std::vector<std::string> own = ystatic::keptPlaces();
	REQUIRE(own.size() == 4u);
	CHECK(own[1] == "/proc");
	CHECK(own[2] == "/sys");
	CHECK(own[3] == "/dev");
}

// A name that cannot be looked at may be one of the files: held back.
TEST_CASE("a name of the files that cannot be looked at holds every file back", "[nhttpd]")
{
	if (getuid() == 0)
		return;	// root looks into any directory
	SeamsBack back;
	OwnDir dir;
	REQUIRE_FALSE(dir.path.empty());
	const std::string locked = dir.path + "/locked";
	REQUIRE(mkdir(locked.c_str(), 0700) == 0);
	{
		FILE *f = fopen((dir.path + "/page.txt").c_str(), "w");
		REQUIRE(f != NULL);
		fclose(f);
	}
	std::vector<std::string> files(1, locked + "/store.conf");
	yprotect::setFiles(files);
	const int page = open((dir.path + "/page.txt").c_str(), O_RDONLY);
	REQUIRE(page >= 0);
	CHECK_FALSE(yprotect::isProtected(page));
	REQUIRE(chmod(locked.c_str(), 0) == 0);
	CHECK(yprotect::isProtected(page));
	chmod(locked.c_str(), 0700);
	close(page);
}

namespace {

// A hook that answers with a file by its name, as /control/getservicesxml
// and the version route do: the transport opens it.
struct ByName : public Cyhook
{
	std::string dir;
	virtual std::string getHookName(void) { return "case by name"; }
	virtual THandleStatus Hook_SendResponse(CyhookHandler *hh)
	{
		const std::string url = hh->UrlData["url"];
		if (url.compare(0, 8, "/byname/") != 0)
			return HANDLED_NONE;
		hh->SendFile(dir + "/" + url.substr(8));
		return hh->status;
	}
};

} // namespace

TEST_CASE("a file answered by its name goes out of a checked descriptor, never a store", "[nhttpd]")
{
	SeamsBack back;
	OwnDir dir;
	REQUIRE_FALSE(dir.path.empty());
	const std::string store = dir.path + "/store.conf";
	{
		FILE *f = fopen(store.c_str(), "w");
		REQUIRE(f != NULL);
		fputs("S3NT1NEL", f);
		fclose(f);
		f = fopen((dir.path + "/services.xml").c_str(), "w");
		REQUIRE(f != NULL);
		fputs("<zapit/>", f);
		fclose(f);
	}
	REQUIRE(symlink(store.c_str(), (dir.path + "/bouquets.xml").c_str()) == 0);
	REQUIRE(link(store.c_str(), (dir.path + "/ubouquets.xml").c_str()) == 0);
	yprotect::setFiles(std::vector<std::string>(1, store));

	ByName hook;
	hook.dir = dir.path;
	WireServer serving(4, 100, WireServer::Threaded, &hook);
	struct Asked { const char *name; int code; const char *body; };
	const Asked asked[] = {
		{ "services.xml", 200, "<zapit/>" },
		{ "bouquets.xml", 404, NULL },
		{ "ubouquets.xml", 404, NULL },
		{ "store.conf", 404, NULL },
		{ "none.xml", 404, NULL },
	};
	for (size_t i = 0; i < sizeof(asked) / sizeof(asked[0]); i++) {
		INFO(asked[i].name);
		const std::string req = std::string("GET /byname/") + asked[i].name + " HTTP/1.0\r\nHost: box\r\n\r\n";
		WireStream s(serving.port, req.c_str());
		REQUIRE(s.ok());
		const std::string body = s.readToEof(2000);
		CHECK(s.code() == asked[i].code);
		CHECK(body.find("S3NT1NEL") == std::string::npos);
		if (asked[i].body != NULL)
			CHECK(body.find(asked[i].body) != std::string::npos);
	}
}

