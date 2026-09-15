/*
 * test_storagefile.cpp - tests for reading and writing one file
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

#include "httpd/endpoint.h"
#include "httpd/endpoints.h"
#include "httpd/http.h"
#include "httpd/router.h"

#include "coreapi/base/errors.h"
#include "coreapi/base/result.h"
#include "coreapi/storage.h"
#include "coreapi/box/storage_internal.h"

#include <config.h>

#include <cstdio>
#include <cstdlib>
#include <string>
#include <utility>
#include <vector>

#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

/* Reading one file out of the box's disks and putting one back.

   The defect these exist to refuse: the server this one replaces could be asked over the
   network, by anybody who could reach the port and without presenting anything, to read
   any file on the box and to write any file on the box. So most of what follows is not
   about a file being read or written at all: it is about the four shapes a caller uses
   to name something the roots do not hold, and about there being nothing outside the
   roots afterwards.

   The four are a walk upwards, an absolute name somewhere else, a link inside the roots
   that leads out of them, and a name beginning with a hyphen. Each is driven against
   both routes, and each case also looks at the file outside to say that the refusal was
   a refusal and not a redirection.

   What none of them can answer for is what the layer under them says out loud: a check
   answers for a name at the moment it is made, and anything in that name can be put back
   differently before the open. */

using namespace coreapi;

namespace
{

// A directory of its own per case, so nothing one case leaves behind is what
// the next one reads.
struct Sandbox
{
	std::string dir;

	Sandbox()
	{
		char tmpl[] = "/tmp/coreapi_storagefile_XXXXXX";
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

/* A name outside the sandbox, put back the way it was whatever becomes of the case that
   touched it. A failed check leaves by throwing, so anything written after it is never
   reached, and a file left changed out there would be read by the next run as the state
   of the machine rather than as the wreckage of a case. This is what half the cases here
   are about: the name is created with content nobody may change, and the destructor is
   what says nothing did. */
struct Outside
{
	std::string path;

	explicit Outside(const std::string &p) : path(p) {}
	~Outside() { remove(path.c_str()); }

	private:
		Outside(const Outside &);
		Outside &operator=(const Outside &);
};

/* The sandbox is the whole of what the routes below are confined to, and saying that
   takes three things and not one: every one of those routes puts the roots together
   again out of the default list before it looks at a name, so the default list has to
   be the sandbox too, and the movie browser's own file has to be one that is not there.
   Left as they ship, the list would hold /tmp, every name this file keeps outside the
   sandbox is under /tmp, and the cases about reaching out of the roots would be driving
   a layer that was letting them through. */
struct RootsFixture
{
	Sandbox     box;
	std::string browser;
	std::string before;
	const char *before_defaults;
	const char *before_browser;

	RootsFixture()
		: browser(box.at("moviebrowser.conf")),
		  before(storage::roots()),
		  before_defaults(storage::internal::default_roots),
		  before_browser(storage::internal::moviebrowser_config_path)
	{
		storage::internal::default_roots = box.dir.c_str();
		storage::internal::moviebrowser_config_path = browser.c_str();
		storage::setRoots(box.dir);
	}

	~RootsFixture()
	{
		storage::setRoots(before);
		storage::internal::default_roots = before_defaults;
		storage::internal::moviebrowser_config_path = before_browser;
	}

	private:
		RootsFixture(const RootsFixture &);
		RootsFixture &operator=(const RootsFixture &);
};

bool put(const std::string &path, const std::string &what)
{
	const int fd = open(path.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0600);
	if (fd < 0)
		return false;
	const ssize_t wrote = write(fd, what.data(), what.size());
	if (close(fd) != 0)
		return false;
	return wrote == (ssize_t) what.size();
}

// A sentinel that is not the empty string for a file that is not there, so that
// a file missing and a file emptied cannot read the same.
std::string get(const std::string &path)
{
	const int fd = open(path.c_str(), O_RDONLY);
	if (fd < 0)
		return "<no such file>";
	std::string out;
	char buf[512];
	for (;;)
	{
		const ssize_t got = read(fd, buf, sizeof(buf));
		if (got <= 0)
			break;
		out.append(buf, (size_t) got);
	}
	close(fd);
	return out;
}

bool exists(const std::string &path)
{
	struct stat st;
	return lstat(path.c_str(), &st) == 0;
}

// What a descriptor the layer handed over holds, and the descriptor closed
// whichever way the case leaves.
std::string readAllAndClose(int fd)
{
	std::string out;
	char buf[512];
	for (;;)
	{
		const ssize_t got = read(fd, buf, sizeof(buf));
		if (got <= 0)
			break;
		out.append(buf, (size_t) got);
	}
	close(fd);
	return out;
}

} // namespace

TEST_CASE("a file inside the roots is handed over as its own bytes", "[storagefile]")
{
	RootsFixture roots;
	const std::string name = roots.box.at("thing.txt");
	REQUIRE(put(name, "the bytes"));

	Result<int> opened = storage::openFile(name);
	REQUIRE(opened.ok());
	REQUIRE(readAllAndClose(opened.value()) == "the bytes");
}

TEST_CASE("a file of no bytes is a file and not a refusal", "[storagefile]")
{
	RootsFixture roots;
	const std::string name = roots.box.at("empty.txt");
	REQUIRE(put(name, ""));

	Result<int> opened = storage::openFile(name);
	REQUIRE(opened.ok());
	REQUIRE(readAllAndClose(opened.value()).empty());
}

TEST_CASE("a walk out of the roots reads nothing", "[storagefile]")
{
	RootsFixture roots;
	Outside outside("/tmp/coreapi-storagefile-walk");
	REQUIRE(put(outside.path, "not yours"));

	// The same file, named through the sandbox and a walk upwards.
	Result<int> opened = storage::openFile(roots.box.at("../coreapi-storagefile-walk"));
	REQUIRE_FALSE(opened.ok());
	REQUIRE(opened.error().status == Status::InvalidArgument);
	REQUIRE(opened.error().code == ErrorCode::OutsideRoots);
	REQUIRE(get(outside.path) == "not yours");
}

TEST_CASE("an absolute name outside the roots reads nothing", "[storagefile]")
{
	RootsFixture roots;

	Result<int> opened = storage::openFile("/etc/passwd");
	REQUIRE_FALSE(opened.ok());
	REQUIRE(opened.error().status == Status::InvalidArgument);
	REQUIRE(opened.error().code == ErrorCode::OutsideRoots);
}

TEST_CASE("a link inside the roots that leads out of them reads nothing", "[storagefile]")
{
	/* The one shape a check on the spelling of a name cannot see, and the
	   reason the name is resolved before it is opened rather than only read.
	   The link is a name inside the roots by every letter of it. */
	RootsFixture roots;
	Outside outside("/tmp/coreapi-storagefile-target");
	REQUIRE(put(outside.path, "not yours"));

	const std::string link = roots.box.at("innocent.txt");
	REQUIRE(symlink(outside.path.c_str(), link.c_str()) == 0);

	Result<int> opened = storage::openFile(link);
	REQUIRE_FALSE(opened.ok());
	REQUIRE(opened.error().status == Status::InvalidArgument);
	REQUIRE(opened.error().code == ErrorCode::OutsideRoots);
	REQUIRE(get(outside.path) == "not yours");
}

TEST_CASE("a link inside the roots that leads back inside them is followed", "[storagefile]")
{
	// The other half of the case above: what is refused is leaving the roots
	// and not the use of a link, which is how a box arranges its own media.
	RootsFixture roots;
	const std::string real = roots.box.at("real.txt");
	const std::string link = roots.box.at("link.txt");
	REQUIRE(put(real, "inside"));
	REQUIRE(symlink(real.c_str(), link.c_str()) == 0);

	Result<int> opened = storage::openFile(link);
	REQUIRE(opened.ok());
	REQUIRE(readAllAndClose(opened.value()) == "inside");
}

TEST_CASE("a name beginning with a hyphen is a name", "[storagefile]")
{
	/* Nothing here reaches a program that reads options, so a hyphen is a
	   letter of the name and the file is handed over. What the case is for is
	   the other direction: a hyphen must not turn the name into something that
	   is read somewhere else, and the file outside says whether it did. */
	RootsFixture roots;
	Outside outside("/tmp/coreapi-storagefile-hyphen");
	REQUIRE(put(outside.path, "not yours"));

	const std::string dashed = roots.box.at("-rf");
	REQUIRE(put(dashed, "mine"));

	Result<int> opened = storage::openFile(dashed);
	REQUIRE(opened.ok());
	REQUIRE(readAllAndClose(opened.value()) == "mine");
	REQUIRE(get(outside.path) == "not yours");
}

TEST_CASE("a directory is not a file to read", "[storagefile]")
{
	RootsFixture roots;
	const std::string dir = roots.box.at("adir");
	REQUIRE(mkdir(dir.c_str(), 0755) == 0);

	Result<int> opened = storage::openFile(dir);
	REQUIRE_FALSE(opened.ok());
	REQUIRE(opened.error().status == Status::NotFound);
	REQUIRE(opened.error().code == ErrorCode::NoSuchName);
}

TEST_CASE("a pipe inside the roots is refused rather than waited on", "[storagefile]")
{
	/* Opening a pipe for reading waits for somebody to open the other end, and
	   this runs on the thread that is answering a request: a wait here is that
	   request never being answered and one of the server's threads gone for
	   good. A case that got this wrong would not fail, it would stop. */
	RootsFixture roots;
	const std::string pipe = roots.box.at("apipe");
	REQUIRE(mkfifo(pipe.c_str(), 0600) == 0);

	Result<int> opened = storage::openFile(pipe);
	REQUIRE_FALSE(opened.ok());
	REQUIRE(opened.error().status == Status::NotFound);
	REQUIRE(opened.error().code == ErrorCode::NoSuchName);
}

TEST_CASE("a name inside the roots that is not there reads nothing", "[storagefile]")
{
	// NotFound and not the refusal above, because the two are different
	// answers: one says the name may not be asked about at all.
	RootsFixture roots;

	Result<int> opened = storage::openFile(roots.box.at("nothing.txt"));
	REQUIRE_FALSE(opened.ok());
	REQUIRE(opened.error().status == Status::NotFound);
	REQUIRE(opened.error().code == ErrorCode::NoSuchName);
}

TEST_CASE("a file written where nothing was is created and holds what was sent", "[storagefile]")
{
	RootsFixture roots;
	const std::string name = roots.box.at("new.txt");

	Result<storage::Wrote> done = storage::writeFile(name, "arrived", false);
	REQUIRE(done.ok());
	REQUIRE(done.value() == storage::Wrote::Created);
	REQUIRE(get(name) == "arrived");
	// The name the bytes travelled under is gone, which is what makes the
	// write one step rather than two states a reader can catch it between.
	REQUIRE_FALSE(exists(name + ".new"));
}

TEST_CASE("a name that is taken is not written over unless that was asked for", "[storagefile]")
{
	RootsFixture roots;
	const std::string name = roots.box.at("taken.txt");
	REQUIRE(put(name, "what was there"));

	Result<storage::Wrote> refused = storage::writeFile(name, "what came", false);
	REQUIRE_FALSE(refused.ok());
	REQUIRE(refused.error().status == Status::Conflict);
	REQUIRE(refused.error().code == ErrorCode::NameTaken);
	REQUIRE(get(name) == "what was there");

	Result<storage::Wrote> done = storage::writeFile(name, "what came", true);
	REQUIRE(done.ok());
	REQUIRE(done.value() == storage::Wrote::Replaced);
	REQUIRE(get(name) == "what came");
}

TEST_CASE("a write over a link that leads out of the roots writes nothing out there", "[storagefile]")
{
	/* The upload half of the link case. A name that is a link is not a plain
	   file, so what is refused is replacing it at all, and the file it leads to
	   is not touched either way. */
	RootsFixture roots;
	Outside outside("/tmp/coreapi-storagefile-writetarget");
	REQUIRE(put(outside.path, "not yours"));

	const std::string link = roots.box.at("innocent.conf");
	REQUIRE(symlink(outside.path.c_str(), link.c_str()) == 0);

	Result<storage::Wrote> refused = storage::writeFile(link, "mine now", true);
	REQUIRE_FALSE(refused.ok());
	REQUIRE(refused.error().status == Status::Conflict);
	REQUIRE(refused.error().code == ErrorCode::NotAPlainFile);
	REQUIRE(get(outside.path) == "not yours");
}

TEST_CASE("a write through a directory that is a link out of the roots is refused", "[storagefile]")
{
	// The directory holding the name is what has to be inside a root, and it is
	// resolved, so a link standing in for one is followed to where it goes.
	RootsFixture roots;

	const std::string door = roots.box.at("door");
	REQUIRE(symlink("/tmp", door.c_str()) == 0);

	const std::string through = door + "/coreapi-storagefile-through";
	Result<storage::Wrote> refused = storage::writeFile(through, "mine now", true);
	REQUIRE_FALSE(refused.ok());
	REQUIRE(refused.error().status == Status::InvalidArgument);
	REQUIRE(refused.error().code == ErrorCode::OutsideRoots);
	REQUIRE_FALSE(exists("/tmp/coreapi-storagefile-through"));
}

TEST_CASE("a write with a walk out of the roots writes nothing out there", "[storagefile]")
{
	RootsFixture roots;
	Outside outside("/tmp/coreapi-storagefile-walkwrite");
	REQUIRE(put(outside.path, "not yours"));

	Result<storage::Wrote> refused =
		storage::writeFile(roots.box.at("../coreapi-storagefile-walkwrite"), "mine now", true);
	REQUIRE_FALSE(refused.ok());
	REQUIRE(refused.error().status == Status::InvalidArgument);
	REQUIRE(refused.error().code == ErrorCode::OutsideRoots);
	REQUIRE(get(outside.path) == "not yours");
}

TEST_CASE("a write to an absolute name outside the roots writes nothing", "[storagefile]")
{
	RootsFixture roots;
	Outside outside("/tmp/coreapi-storagefile-absolute");

	Result<storage::Wrote> refused = storage::writeFile(outside.path, "mine now", true);
	REQUIRE_FALSE(refused.ok());
	REQUIRE(refused.error().status == Status::InvalidArgument);
	REQUIRE(refused.error().code == ErrorCode::OutsideRoots);
	REQUIRE_FALSE(exists(outside.path));
	REQUIRE_FALSE(exists(outside.path + ".new"));
}

TEST_CASE("a name beginning with a hyphen is written as a name", "[storagefile]")
{
	RootsFixture roots;
	const std::string dashed = roots.box.at("-rf");

	Result<storage::Wrote> done = storage::writeFile(dashed, "mine", false);
	REQUIRE(done.ok());
	REQUIRE(get(dashed) == "mine");
}

TEST_CASE("a directory is not written over", "[storagefile]")
{
	RootsFixture roots;
	const std::string dir = roots.box.at("adir");
	REQUIRE(mkdir(dir.c_str(), 0755) == 0);

	Result<storage::Wrote> refused = storage::writeFile(dir, "mine now", true);
	REQUIRE_FALSE(refused.ok());
	REQUIRE(refused.error().status == Status::Conflict);
	REQUIRE(refused.error().code == ErrorCode::NotAPlainFile);
}

TEST_CASE("a write into a directory that is not there is refused", "[storagefile]")
{
	RootsFixture roots;

	Result<storage::Wrote> refused = storage::writeFile(roots.box.at("nowhere/thing.txt"), "x", false);
	REQUIRE_FALSE(refused.ok());
	REQUIRE(refused.error().status == Status::NotFound);
	REQUIRE(refused.error().code == ErrorCode::NoSuchParent);
}

TEST_CASE("a name already being written beside is not written through", "[storagefile]")
{
	/* Two writes of one name at once would interleave into one file, and the
	   remains of a write that was cut off are a name somebody has to look at.
	   The same refusal is what keeps the open off something planted at that
	   second name. */
	RootsFixture roots;
	const std::string name = roots.box.at("busy.txt");
	REQUIRE(put(name, "what was there"));
	REQUIRE(put(name + ".new", "half of something else"));

	Result<storage::Wrote> refused = storage::writeFile(name, "mine now", true);
	REQUIRE_FALSE(refused.ok());
	REQUIRE(refused.error().status == Status::Conflict);
	REQUIRE(refused.error().code == ErrorCode::SideFileInTheWay);
	REQUIRE(get(name) == "what was there");
	REQUIRE(get(name + ".new") == "half of something else");
}

TEST_CASE("a link planted where the bytes would go is not written through", "[storagefile]")
{
	/* The name the bytes travel under is worked out from the name that was
	   asked for, so somebody who can write into the directory can plant a link
	   there and wait. Two things refuse it: the name is not opened at all while
	   anything is there, and the open itself does not follow a link. */
	RootsFixture roots;
	Outside outside("/tmp/coreapi-storagefile-sidetarget");
	REQUIRE(put(outside.path, "not yours"));

	const std::string name = roots.box.at("planted.txt");
	REQUIRE(symlink(outside.path.c_str(), (name + ".new").c_str()) == 0);

	Result<storage::Wrote> refused = storage::writeFile(name, "mine now", false);
	REQUIRE_FALSE(refused.ok());
	REQUIRE(refused.error().code == ErrorCode::SideFileInTheWay);
	REQUIRE(get(outside.path) == "not yours");
	REQUIRE_FALSE(exists(name));
}

TEST_CASE("a file written and read back through the two calls is the same file", "[storagefile]")
{
	// The pair, because each half is only worth what the other one reads.
	RootsFixture roots;
	const std::string name = roots.box.at("round.bin");

	std::string bytes;
	for (int i = 0; i < 256; ++i)
		bytes += (char) i;

	Result<storage::Wrote> done = storage::writeFile(name, bytes, false);
	REQUIRE(done.ok());

	Result<int> opened = storage::openFile(name);
	REQUIRE(opened.ok());
	REQUIRE(readAllAndClose(opened.value()) == bytes);
}

namespace
{

/* What the two probes do, and where the case tells them to do it.

   Function pointers and not a lambda with state, because what the layer takes
   is a plain function; a file scope name is how the one of them that runs gets
   at what the case set up. */
std::string g_probe_swap_from;
std::string g_probe_swap_to;
std::string g_probe_watch;
std::string g_probe_saw;
bool g_probe_side_was_there = false;

/* A name put back as a link pointing out of the roots, between the moment it
   was resolved and the moment it is opened. What a racer would have to win a
   race to do, done on purpose and in order. */
void swapForALink()
{
	remove(g_probe_swap_from.c_str());
	(void) symlink(g_probe_swap_to.c_str(), g_probe_swap_from.c_str());
}

// What the file holds while the write of it is half done, and whether the
// bytes are going somewhere else in the meantime.
void lookWhileWriting()
{
	g_probe_saw = get(g_probe_watch);
	g_probe_side_was_there = exists(g_probe_watch + ".new");
}

// Cleared whichever way the case leaves, because a failed check unwinds past a
// last line and the next case would run with a probe it never installed.
struct ProbesCleared
{
	ProbesCleared() {}

	~ProbesCleared()
	{
		storage::internal::setOpenProbeForTest(NULL);
		storage::internal::setWriteProbeForTest(NULL);
	}

	private:
		ProbesCleared(const ProbesCleared &);
		ProbesCleared &operator=(const ProbesCleared &);
};

} // namespace

TEST_CASE("a name swapped for a link after it was checked is not opened", "[storagefile]")
{
	/* The gap every check on a name in this tree admits to: the directories are
	   writable on a box, so a name found to be inside the roots can be put back
	   as something else before it is opened. What refuses it is a flag on the
	   open, and that flag is invisible from outside: take it away and every
	   ordinary request answers exactly as it did. */
	RootsFixture roots;
	ProbesCleared cleared;
	Outside outside("/tmp/coreapi-storagefile-swapped");
	REQUIRE(put(outside.path, "not yours"));

	const std::string name = roots.box.at("ordinary.txt");
	REQUIRE(put(name, "mine"));

	g_probe_swap_from = name;
	g_probe_swap_to = outside.path;
	storage::internal::setOpenProbeForTest(&swapForALink);

	Result<int> opened = storage::openFile(name);
	REQUIRE_FALSE(opened.ok());
	REQUIRE(opened.error().status == Status::NotFound);
	REQUIRE(opened.error().code == ErrorCode::NoSuchName);
	REQUIRE(get(outside.path) == "not yours");
}

TEST_CASE("the file that was there is whole while the new one is being written", "[storagefile]")
{
	/* From outside, a write that went beside the file and one that went straight
	   at the name look the same once either has finished. They differ at exactly
	   one moment, and this is a case standing at it: the name still holds every
	   byte of what was there, and what is arriving is under a name of its own. */
	RootsFixture roots;
	ProbesCleared cleared;
	const std::string name = roots.box.at("watched.txt");
	REQUIRE(put(name, "what was there"));

	g_probe_watch = name;
	g_probe_saw.clear();
	g_probe_side_was_there = false;
	storage::internal::setWriteProbeForTest(&lookWhileWriting);

	Result<storage::Wrote> done = storage::writeFile(name, "what came instead", true);
	REQUIRE(done.ok());

	REQUIRE(g_probe_saw == "what was there");
	REQUIRE(g_probe_side_was_there);
	REQUIRE(get(name) == "what came instead");
	REQUIRE_FALSE(exists(name + ".new"));
}

namespace
{

/* A root that takes in the directory a web server keeps its own access store in, which
   is what an operator produces by pointing the directory this box records into one level
   too high. Those directories are settings and can name anywhere, and a consumer adds
   them to the roots as it finds them, so this is the arrangement that happens by
   accident. beside is a file of this case's own in the same directory, so a refusal is
   the two names being refused and not the root admitting nothing. */
struct RootOverTheAccessStore
{
	std::string dir;
	std::string store;
	std::string old_store;
	std::string beside;
	std::string above;
	std::string before;

	/* What the two files held when this began, and whether they were there.

	   The names these cases drive are the real ones, because the rule compares against
	   the real ones and there is no seam that moves them; there must not be, or an
	   attacker could move it too. So a case that writes one is writing the file a box on
	   this machine logs people in with. That is not a thought experiment: this suite
	   shares its build volume with a running box, and an earlier draft of these cases
	   emptied exactly that.

	   So the fixture owns the two names. It reads them here and puts them back byte for
	   byte on the way out, whichever line a case left through. */
	std::string kept[2];
	bool        was_there[2];

	/* The default list and the movie browser's file as well as the roots, because a
	   route puts the roots together again out of both before it looks at a name. Left
	   as they ship, the root this case is about would be one of several rather than the
	   one, and what a refusal meant could not be read off it. */
	std::string browser;
	const char *before_defaults;
	const char *before_browser;

	RootOverTheAccessStore()
		: dir(CONFIGDIR),
		  store(std::string(CONFIGDIR) + "/ni-web.conf"),
		  old_store(std::string(CONFIGDIR) + "/nhttpd.conf"),
		  beside(std::string(CONFIGDIR) + "/coreapi-beside.conf"),
		  before(storage::roots()),
		  browser(std::string(CONFIGDIR) + "/coreapi-no-moviebrowser.conf"),
		  before_defaults(storage::internal::default_roots),
		  before_browser(storage::internal::moviebrowser_config_path)
	{
		const size_t cut = dir.find_last_of('/');
		above = (cut == std::string::npos) ? std::string("/") : dir.substr(0, cut);

		// Made rather than assumed: a case that quietly did nothing because the
		// directory was not there would pass against a layer that protects
		// nothing.
		(void) system(("mkdir -p " + dir).c_str());

		const std::string both[2] = { store, old_store };
		for (size_t i = 0; i < 2; ++i)
		{
			was_there[i] = exists(both[i]);
			// Read through the link where it is one, so that what goes back is
			// what a reader of that name saw.
			kept[i] = was_there[i] ? get(both[i]) : std::string();
		}

		storage::internal::default_roots = above.c_str();
		storage::internal::moviebrowser_config_path = browser.c_str();
		storage::setRoots(above);
	}

	~RootOverTheAccessStore()
	{
		storage::setRoots(before);
		storage::internal::default_roots = before_defaults;
		storage::internal::moviebrowser_config_path = before_browser;
		remove(beside.c_str());

		const std::string both[2] = { store, old_store };
		for (size_t i = 0; i < 2; ++i)
		{
			// Taken away first, because a case may have left a link of that
			// name and writing through one would put the bytes somewhere else.
			remove(both[i].c_str());
			if (was_there[i])
				(void) put(both[i], kept[i]);
		}
	}

	private:
		RootOverTheAccessStore(const RootOverTheAccessStore &);
		RootOverTheAccessStore &operator=(const RootOverTheAccessStore &);
};

} // namespace

TEST_CASE("a case that drives the access store puts it back", "[storagefile][accessstore]")
{
	/* The case that would have caught the worst thing in this file. The cases below
	   drive the real names, so one of them writes the file a box on this machine logs
	   people in with, and an earlier draft of them removed it and left it removed: the
	   box that shares this build volume answered no login at all afterwards, and nothing
	   in the suite said a word. The fixture owns the two names for that reason, and this
	   is what holds it to owning them. */
	const std::string both[2] = { std::string(CONFIGDIR) + "/ni-web.conf",
				      std::string(CONFIGDIR) + "/nhttpd.conf" };
	bool was_there[2];
	std::string held[2];
	for (size_t i = 0; i < 2; ++i)
	{
		was_there[i] = exists(both[i]);
		held[i] = was_there[i] ? get(both[i]) : std::string();
	}

	{
		// Everything a case here can do to them: write one, take the other
		// away, and leave by a path that restores nothing itself.
		RootOverTheAccessStore over;
		REQUIRE(put(over.store, "password_hash=written by a case"));
		remove(over.old_store.c_str());
	}

	for (size_t i = 0; i < 2; ++i)
	{
		INFO(both[i]);
		REQUIRE(exists(both[i]) == was_there[i]);
		if (was_there[i])
			REQUIRE(get(both[i]) == held[i]);
	}
}

TEST_CASE("the directory the access store sits in is reached like any other", "[storagefile][accessstore]")
{
	/* The half that makes every case after this one mean something, and a
	   statement in its own right: a configuration file is something somebody
	   has a good reason to fetch, to keep and to compare, and this surface is
	   how they do it. What is refused below is two files and not this
	   directory. */
	RootOverTheAccessStore over;
	REQUIRE(put(over.beside, "ordinary=1"));

	Result<std::vector<FileEntry> > listed = storage::list(over.dir);
	REQUIRE(listed.ok());

	Result<int> opened = storage::openFile(over.beside);
	REQUIRE(opened.ok());
	REQUIRE(readAllAndClose(opened.value()) == "ordinary=1");

	Result<storage::Wrote> written = storage::writeFile(over.beside, "ordinary=2", true);
	REQUIRE(written.ok());
	REQUIRE(get(over.beside) == "ordinary=2");

	Result<void> removed = storage::removePath(over.beside);
	REQUIRE(removed.ok());
	REQUIRE_FALSE(exists(over.beside));
}

TEST_CASE("the access store is read, written and removed by nothing here", "[storagefile][accessstore]")
{
	/* Both files and all three acts. The removal could reach them before either of the
	   two routes that read and write existed. The answer is its own and is neither of
	   the two it could be mistaken for: not a name outside the roots, because it is
	   inside them, and not a refusal about the caller, because no credential changes
	   it. */
	RootOverTheAccessStore over;

	const std::string both[] = { over.store, over.old_store };
	for (size_t i = 0; i < 2; ++i)
	{
		INFO(both[i]);
		REQUIRE(put(both[i], "password_hash=secret"));

		Result<int> opened = storage::openFile(both[i]);
		REQUIRE_FALSE(opened.ok());
		REQUIRE(opened.error().status == Status::InvalidArgument);
		REQUIRE(opened.error().code == ErrorCode::AccessStore);

		Result<storage::Wrote> written = storage::writeFile(both[i], "password_hash=", true);
		REQUIRE_FALSE(written.ok());
		REQUIRE(written.error().code == ErrorCode::AccessStore);

		Result<void> removed = storage::removePath(both[i]);
		REQUIRE_FALSE(removed.ok());
		REQUIRE(removed.error().code == ErrorCode::AccessStore);

		// And none of the three touched it. Putting it back is the fixture's,
		// not this case's: a check that fails leaves by throwing and would
		// never reach a line that restored anything.
		REQUIRE(get(both[i]) == "password_hash=secret");
	}
}

TEST_CASE("the access store is still refused where there is none yet", "[storagefile][accessstore]")
{
	/* A box that has never been configured has no such file, and a name with
	   nothing under it resolves to nothing. Making it from here would be making
	   the very thing that decides who may make it. */
	RootOverTheAccessStore over;
	/* Taken away to make that box, and put back by the fixture, which read the
	   name before this case began. Not asserted absent to begin with: on a
	   machine where a box is running this file is there, and a case that only
	   held on a machine without one is a case that stops holding. */
	remove(over.store.c_str());
	REQUIRE_FALSE(exists(over.store));

	Result<storage::Wrote> written = storage::writeFile(over.store, "username=whoever", false);
	REQUIRE_FALSE(written.ok());
	REQUIRE(written.error().code == ErrorCode::AccessStore);
	REQUIRE_FALSE(exists(over.store));

	Result<void> made = storage::createDirectory(over.store);
	REQUIRE_FALSE(made.ok());
	REQUIRE(made.error().code == ErrorCode::AccessStore);
	REQUIRE_FALSE(exists(over.store));
}

TEST_CASE("a store that is a link is the same file under either of its names", "[storagefile][accessstore]")
{
	/* Both sides of the comparison are resolved, so what is refused is the file and not
	   one spelling of it. Without that, moving the store aside and leaving a link in its
	   place would hand the password over under the other name. The far end is inside the
	   roots, so a refusal here is this rule and not the confinement. */
	RootOverTheAccessStore over;
	const std::string elsewhere = over.dir + "/coreapi-moved.conf";

	REQUIRE(put(elsewhere, "password_hash=secret"));
	// Inside what the fixture owns: it read this name before the case began and
	// writes it back on the way out, whatever is standing there by then.
	remove(over.store.c_str());
	REQUIRE(symlink(elsewhere.c_str(), over.store.c_str()) == 0);

	// Under the name the server opens it by.
	Result<int> byStore = storage::openFile(over.store);
	const bool store_refused = !byStore.ok() && byStore.error().code == ErrorCode::AccessStore;

	// And under the name it actually sits at, which is the same file.
	Result<int> byTarget = storage::openFile(elsewhere);
	const bool target_refused = !byTarget.ok() && byTarget.error().code == ErrorCode::AccessStore;

	if (byStore.ok())
		close(std::move(byStore).value());
	if (byTarget.ok())
		close(std::move(byTarget).value());
	remove(elsewhere.c_str());

	REQUIRE(store_refused);
	REQUIRE(target_refused);
}

TEST_CASE("a name that only looks like the access store is an ordinary file", "[storagefile][accessstore]")
{
	// The comparison is an identity and not a resemblance. A file whose name
	// begins or ends like one of the two is somebody's own file.
	RootOverTheAccessStore over;

	const std::string near[] = { over.store + ".bak", over.dir + "/my-ni-web.conf" };
	for (size_t i = 0; i < 2; ++i)
	{
		INFO(near[i]);
		REQUIRE(put(near[i], "mine"));
		Result<int> opened = storage::openFile(near[i]);
		const bool reached = opened.ok();
		if (reached)
			REQUIRE(readAllAndClose(std::move(opened).value()) == "mine");
		REQUIRE(remove(near[i].c_str()) == 0);
		REQUIRE(reached);
	}
}

/* From here down the same ground through the routes, because a layer that
   refuses and a route that reaches it are two things and only the second is
   what a caller meets. */

namespace
{

using namespace httpd;

struct ShippedRoutes
{
	ShippedRoutes() { setRoutesForTest(NULL); }
	~ShippedRoutes() { setRoutesForTest(NULL); }

	private:
		ShippedRoutes(const ShippedRoutes &);
		ShippedRoutes &operator=(const ShippedRoutes &);
};

/* A name as it travels in a query. Everything but the bytes a query carries as
   themselves becomes three characters, because one of the cases below names a file with
   a carriage return in it, and a carriage return written into a target straight is not a
   target at all. The separator is left as itself so what a case wrote and what the
   answer names read the same. */
std::string escaped(const std::string &v)
{
	static const char kHex[] = "0123456789ABCDEF";
	std::string out;
	for (size_t i = 0; i < v.size(); ++i)
	{
		const unsigned char c = (unsigned char) v[i];
		const bool plain = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
				   (c >= '0' && c <= '9') || c == '.' || c == '-' ||
				   c == '_' || c == '~' || c == '/';
		if (plain)
		{
			out += (char) c;
			continue;
		}
		out += '%';
		out += kHex[c >> 4];
		out += kHex[c & 0x0f];
	}
	return out;
}

Response send(Method m, const std::string &target, const std::string &body,
              AuthLevel granted)
{
	std::string path = target;
	std::string query;
	const size_t q = target.find('?');
	if (q != std::string::npos)
	{
		path = target.substr(0, q);
		query = target.substr(q + 1);
	}
	return dispatch(m, path, query, body, "127.0.0.1", granted);
}

std::string headerOf(const Response &r, const char *name)
{
	for (size_t i = 0; i < r.headers.size(); ++i)
	{
		if (r.headers[i].first == name)
			return r.headers[i].second;
	}
	return std::string();
}

/* What an answer built out of a file holds, read off the descriptor it carries
   and the descriptor closed here.

   A route that answers a file puts nothing in the body: the transport sends the
   bytes from the kernel to the socket and never through this process, so a case
   reading r.body would be reading an empty string about every file there is. */
std::string bodyOfFileAnswer(Response &r)
{
	if (r.fd < 0)
		return "<no descriptor>";
	const int fd = r.fd;
	r.fd = -1;
	return readAllAndClose(fd);
}

} // namespace

TEST_CASE("the route hands the file over with a type and a name", "[storagefile][endpoints]")
{
	ShippedRoutes shipped;
	RootsFixture roots;
	REQUIRE(put(roots.box.at("clip.ts"), "recorded"));

	Response r = send(Get, "/api/v1/storage/file?path=" + escaped(roots.box.at("clip.ts")), "", AuthLevel::System);
	REQUIRE(r.code == 200);
	REQUIRE(r.content_type == "video/mp2t");
	REQUIRE(headerOf(r, "Content-Disposition") == "attachment; filename*=UTF-8''clip.ts");
	REQUIRE(bodyOfFileAnswer(r) == "recorded");
}

TEST_CASE("a file kept packed is handed over as what it is and not as what is inside it", "[storagefile][endpoints]")
{
	/* The pages directory takes a trailing .gz off a name before it reads the
	   type, because a page kept packed is unpacked by the reader under a header
	   saying so. Nothing unpacks a file coming off a disk, so a type read that
	   way here would name bytes the caller never receives. */
	ShippedRoutes shipped;
	RootsFixture roots;
	REQUIRE(put(roots.box.at("list.xml.gz"), "packed"));

	Response r = send(Get, "/api/v1/storage/file?path=" + escaped(roots.box.at("list.xml.gz")), "", AuthLevel::System);
	REQUIRE(r.code == 200);
	REQUIRE(r.content_type == "application/gzip");
	REQUIRE(headerOf(r, "Content-Encoding").empty());
	REQUIRE(bodyOfFileAnswer(r) == "packed");
}

TEST_CASE("a type the pages directory already names is not named twice", "[storagefile][endpoints]")
{
	// Everything this route has no type of its own for falls through to the one
	// table this server already has, so the two cannot come to disagree.
	ShippedRoutes shipped;
	RootsFixture roots;
	REQUIRE(put(roots.box.at("picon.png"), "not really a picture"));

	Response r = send(Get, "/api/v1/storage/file?path=" + escaped(roots.box.at("picon.png")), "", AuthLevel::System);
	REQUIRE(r.code == 200);
	REQUIRE(r.content_type == "image/png");
	(void) bodyOfFileAnswer(r);
}

TEST_CASE("a file name cannot write a header of its own", "[storagefile][endpoints]")
{
	/* A file name may hold a carriage return, a quotation mark and a byte of
	   any value. Written into a header straight, the first of those ends the
	   line and everything after it is a header of whoever named the file. What
	   goes out instead is worked out from the bytes that are permitted, so a
	   byte nobody thought of is encoded rather than passed. */
	ShippedRoutes shipped;
	RootsFixture roots;

	const std::string nasty = std::string("a\r\nX-Evil: yes\"; b\xc3\xa4.txt");
	REQUIRE(put(roots.box.at(nasty), "x"));

	Response r = send(Get, "/api/v1/storage/file?path=" + escaped(roots.box.at(nasty)), "", AuthLevel::System);
	REQUIRE(r.code == 200);

	/* The letters of the name travel as themselves and that is not the hazard:
	   without the two that end a line, and without the ones that separate a
	   header's name from its value, what is left is a parameter value and reads
	   as one however it is spelt. */
	const std::string said = headerOf(r, "Content-Disposition");
	INFO(said);
	REQUIRE(said.find('\r') == std::string::npos);
	REQUIRE(said.find('\n') == std::string::npos);
	REQUIRE(said.find('"') == std::string::npos);
	REQUIRE(said.find("X-Evil:") == std::string::npos);
	REQUIRE(said == "attachment; filename*=UTF-8''a%0D%0AX-Evil%3A%20yes%22%3B%20b%C3%A4.txt");
	(void) bodyOfFileAnswer(r);
}

TEST_CASE("the file is an attachment whatever it holds", "[storagefile][endpoints]")
{
	/* What a caller may write into these directories is anything at all, a page
	   that would run in this server's own origin among it. A browser told to
	   display such a file displays it as this box. */
	ShippedRoutes shipped;
	RootsFixture roots;
	REQUIRE(put(roots.box.at("page.html"), "<script>alert(1)</script>"));

	Response r = send(Get, "/api/v1/storage/file?path=" + escaped(roots.box.at("page.html")), "", AuthLevel::System);
	REQUIRE(r.code == 200);
	REQUIRE(headerOf(r, "Content-Disposition").compare(0, 10, "attachment") == 0);
	(void) bodyOfFileAnswer(r);
}

TEST_CASE("the four ways out of the roots are refused at the route as well", "[storagefile][endpoints]")
{
	ShippedRoutes shipped;
	RootsFixture roots;
	Outside outside("/tmp/coreapi-storagefile-route");
	REQUIRE(put(outside.path, "not yours"));

	const std::string link = roots.box.at("innocent.txt");
	REQUIRE(symlink(outside.path.c_str(), link.c_str()) == 0);

	/* Three of the four are refused for leaving the roots, and the link is
	   refused twice over: reading it resolves out of the roots, and writing it
	   never gets that far because a name that is a link is not a plain file and
	   nothing is put in its place. Two codes rather than one, because the two
	   are different refusals and a caller does different things about them. */
	struct Way { const char *what; std::string path; int writing; };
	const Way ways[] = {
		{ "a walk upwards",         roots.box.at("../coreapi-storagefile-route"), 400 },
		{ "an absolute name",       outside.path,                                 400 },
		{ "a link that leads out",  link,                                         409 },
		{ "a walk through a link",  link + "/../coreapi-storagefile-route",       400 }
	};

	for (size_t i = 0; i < sizeof(ways) / sizeof(ways[0]); ++i)
	{
		INFO(ways[i].what);
		Response read = send(Get, "/api/v1/storage/file?path=" + escaped(ways[i].path), "", AuthLevel::System);
		REQUIRE(read.code == 400);
		REQUIRE(read.fd < 0);

		Response write = send(Put, "/api/v1/storage/file?path=" + escaped(ways[i].path) + "&overwrite=1",
		                      "mine now", AuthLevel::System);
		REQUIRE(write.code == ways[i].writing);
	}

	// And the whole point of the four: nothing out there moved.
	REQUIRE(get(outside.path) == "not yours");
}

TEST_CASE("a name beginning with a hyphen reaches the route as a name", "[storagefile][endpoints]")
{
	ShippedRoutes shipped;
	RootsFixture roots;
	Outside outside("/tmp/coreapi-storagefile-routehyphen");
	REQUIRE(put(outside.path, "not yours"));

	Response made = send(Put, "/api/v1/storage/file?path=" + escaped(roots.box.at("-rf")), "mine", AuthLevel::System);
	REQUIRE(made.code == 201);
	REQUIRE(get(roots.box.at("-rf")) == "mine");
	REQUIRE(get(outside.path) == "not yours");

	Response read = send(Get, "/api/v1/storage/file?path=" + escaped(roots.box.at("-rf")), "", AuthLevel::System);
	REQUIRE(read.code == 200);
	REQUIRE(bodyOfFileAnswer(read) == "mine");
}

TEST_CASE("the route says whether the name was free", "[storagefile][endpoints]")
{
	ShippedRoutes shipped;
	RootsFixture roots;
	const std::string target = "/api/v1/storage/file?path=" + escaped(roots.box.at("conf.xml"));

	REQUIRE(send(Put, target, "first", AuthLevel::System).code == 201);
	REQUIRE(get(roots.box.at("conf.xml")) == "first");

	// A name that is taken and nothing saying to replace it.
	Response refused = send(Put, target, "second", AuthLevel::System);
	REQUIRE(refused.code == 409);
	REQUIRE(get(roots.box.at("conf.xml")) == "first");

	Response replaced = send(Put, target + "&overwrite=1", "second", AuthLevel::System);
	REQUIRE(replaced.code == 204);
	REQUIRE(get(roots.box.at("conf.xml")) == "second");
}

TEST_CASE("a body of no bytes writes a file of no bytes", "[storagefile][endpoints]")
{
	// And is not read as a request that carried nothing: what was sent is what
	// the file holds, and a caller may mean an empty file.
	ShippedRoutes shipped;
	RootsFixture roots;

	REQUIRE(send(Put, "/api/v1/storage/file?path=" + escaped(roots.box.at("nothing.txt")), "",
	             AuthLevel::System).code == 201);
	REQUIRE(get(roots.box.at("nothing.txt")).empty());
}

TEST_CASE("nothing below the box's own credential reads or writes a file", "[storagefile][endpoints]")
{
	/* The level the two routes ask for, walked here as well as in the tables.

	   Both of the two levels under it, because only the lower is what a caller gets for
	   free: a request from the box's own network is granted a read without presenting
	   anything. The one above it is what every other change in this server asks for, and
	   it is the one a route here would most plausibly be lowered to. What these two
	   reach is the filesystem the box records to, and the roots hold the directory this
	   server keeps its own configuration in. */
	ShippedRoutes shipped;
	RootsFixture roots;
	REQUIRE(put(roots.box.at("secret.txt"), "what was there"));

	const AuthLevel under[] = { AuthLevel::Read, AuthLevel::Write };
	for (size_t i = 0; i < sizeof(under) / sizeof(under[0]); ++i)
	{
		INFO(i);
		Response read = send(Get, "/api/v1/storage/file?path=" + escaped(roots.box.at("secret.txt")), "",
		                     under[i]);
		REQUIRE(read.code == 403);
		REQUIRE(read.fd < 0);

		Response write = send(Put, "/api/v1/storage/file?path=" + escaped(roots.box.at("secret.txt")) + "&overwrite=1",
		                      "mine now", under[i]);
		REQUIRE(write.code == 403);
		REQUIRE(get(roots.box.at("secret.txt")) == "what was there");
	}
}

TEST_CASE("a request with no name is refused before anything is opened", "[storagefile][endpoints]")
{
	ShippedRoutes shipped;
	RootsFixture roots;

	REQUIRE(send(Get, "/api/v1/storage/file", "", AuthLevel::System).code == 400);
	REQUIRE(send(Put, "/api/v1/storage/file", "x", AuthLevel::System).code == 400);
}

TEST_CASE("no route hands out, replaces or removes the access store", "[storagefile][endpoints][accessstore]")
{
	/* What a caller actually meets, with the box's own credential in hand and a
	   root that takes the directory in. */
	ShippedRoutes shipped;
	RootOverTheAccessStore over;
	REQUIRE(put(over.store, "password_hash=secret"));

	Response read = send(Get, "/api/v1/storage/file?path=" + escaped(over.store), "", AuthLevel::System);
	REQUIRE(read.code == 400);
	REQUIRE(read.fd < 0);
	REQUIRE(read.body.find("access-store") != std::string::npos);
	REQUIRE(read.body.find("password_hash") == std::string::npos);

	Response write = send(Put, "/api/v1/storage/file?path=" + escaped(over.store) + "&overwrite=1",
	                      "password_hash=", AuthLevel::System);
	REQUIRE(write.code == 400);

	REQUIRE(send(Delete, "/api/v1/storage/path?path=" + escaped(over.store), "", AuthLevel::System).code == 400);
	REQUIRE(get(over.store) == "password_hash=secret");

	/* And the configuration beside it is fetched and replaced as before, which
	   is the half that keeps the next round from loosening the roots because
	   the file routes "cannot do anything anyway". */
	REQUIRE(put(over.beside, "ordinary=1"));
	Response ordinary = send(Get, "/api/v1/storage/file?path=" + escaped(over.beside), "", AuthLevel::System);
	REQUIRE(ordinary.code == 200);
	REQUIRE(bodyOfFileAnswer(ordinary) == "ordinary=1");
	REQUIRE(send(Put, "/api/v1/storage/file?path=" + escaped(over.beside) + "&overwrite=1",
	             "ordinary=2", AuthLevel::System).code == 204);
	REQUIRE(get(over.beside) == "ordinary=2");

	// The directory itself is listed, and the store is in the listing: what is
	// refused is reaching it, not knowing it is there.
	Response listed = send(Get, "/api/v1/storage/files?path=" + escaped(over.dir), "", AuthLevel::System);
	REQUIRE(listed.code == 200);
	REQUIRE(listed.body.find("ni-web.conf") != std::string::npos);
}
