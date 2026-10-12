/*
 * test_storage_move.cpp - the file manager's roots, renames and pieces
 *
 * Copyright (C) 2026 Thilo Graf
 *
 * License: GPL-2.0-or-later
 */

#include "support/catch.hpp"

#include "coreapi/base/errors.h"
#include "coreapi/base/result.h"
#include "coreapi/storage.h"

#include <cstdlib>
#include <string>
#include <vector>

#include <fcntl.h>
#include <limits.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

/* What GET /api/v1/storage/roots, POST /api/v1/storage/rename and
   PATCH /api/v1/storage/file answer through: the list of roots as a file
   manager offers them, a rename that keeps to the roots and to one
   filesystem, and the pieces a file too large for one request goes up in. */

using namespace coreapi;

namespace
{

// One directory per case with two inside: the one the roots are set to and one
// beside it no root names. The roots are put back on the way out.
struct Scratch
{
	std::string top;
	std::string saved_roots;

	Scratch() : saved_roots(storage::roots())
	{
		char tmpl[] = "/tmp/coreapi_move_XXXXXX";
		if (mkdtemp(tmpl) == NULL)
			return;
		top = tmpl;
		if (mkdir(allowed().c_str(), 0755) != 0 || mkdir(beside().c_str(), 0755) != 0)
		{
			wipe();
			top.clear();
			return;
		}
		storage::setRoots(allowed());
	}

	~Scratch()
	{
		storage::setRoots(saved_roots);
		wipe();
	}

	std::string allowed() const { return top + "/allowed"; }
	std::string beside() const { return top + "/beside"; }
	std::string at(const std::string &leaf) const { return allowed() + "/" + leaf; }
	std::string off(const std::string &leaf) const { return beside() + "/" + leaf; }

	private:
		void wipe()
		{
			if (!top.empty())
				(void) system(("rm -rf '" + top + "'").c_str());
		}
		Scratch(const Scratch &);
		Scratch &operator=(const Scratch &);
};

bool make(const std::string &path, const std::string &bytes)
{
	const int fd = open(path.c_str(), O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, 0600);
	if (fd < 0)
		return false;
	const ssize_t n = write(fd, bytes.data(), bytes.size());
	return close(fd) == 0 && n == (ssize_t) bytes.size();
}

std::string slurp(const std::string &path)
{
	std::string got;
	const int fd = open(path.c_str(), O_RDONLY | O_CLOEXEC);
	if (fd < 0)
		return got;
	char buf[4096];
	ssize_t n;
	while ((n = read(fd, buf, sizeof(buf))) > 0)
		got.append(buf, (size_t) n);
	close(fd);
	return got;
}

// With lstat, so a link counts as there whether or not it leads anywhere.
bool present(const std::string &path)
{
	struct stat st;
	return lstat(path.c_str(), &st) == 0;
}

bool isLink(const std::string &path)
{
	struct stat st;
	return lstat(path.c_str(), &st) == 0 && S_ISLNK(st.st_mode);
}

void expectRefusal(const Result<void> &r, Status s, ErrorCode c)
{
	REQUIRE_FALSE(r.ok());
	REQUIRE(r.error().status == s);
	REQUIRE(r.error().code == c);
}

} // namespace

TEST_CASE("the roots are listed once each, in order, with whether they are there", "[storage][roots]")
{
	Scratch s;
	REQUIRE_FALSE(s.top.empty());

	const std::string missing = s.top + "/not-mounted-yet";
	storage::setRoots(s.allowed() + ":" + missing + "::" + s.allowed() + ":" + s.beside());

	const std::vector<storage::RootEntry> got = storage::rootList();
	REQUIRE(got.size() == 3);
	REQUIRE(got[0].path == s.allowed());
	REQUIRE(got[0].present);
	REQUIRE(got[1].path == missing);
	REQUIRE_FALSE(got[1].present);
	REQUIRE(got[2].path == s.beside());
	REQUIRE(got[2].present);

	// A root that is a file names no place to start, and one that resolves to /
	// admits nothing.
	REQUIRE(make(s.at("plain"), "x"));
	storage::setRoots(s.at("plain") + ":/");
	const std::vector<storage::RootEntry> odd = storage::rootList();
	REQUIRE(odd.size() == 2);
	REQUIRE_FALSE(odd[0].present);
	REQUIRE_FALSE(odd[1].present);
}

TEST_CASE("a rename gives the name and leaves the content and what a link leads to", "[storage][rename]")
{
	Scratch s;
	REQUIRE_FALSE(s.top.empty());

	REQUIRE(make(s.at("old.txt"), "content"));
	REQUIRE(storage::renamePath(s.at("old.txt"), s.at("new.txt"), false).ok());
	REQUIRE_FALSE(present(s.at("old.txt")));
	REQUIRE(slurp(s.at("new.txt")) == "content");

	// A directory with what it holds, into a directory beside it.
	REQUIRE(mkdir(s.at("dir").c_str(), 0755) == 0);
	REQUIRE(mkdir(s.at("other").c_str(), 0755) == 0);
	REQUIRE(make(s.at("dir/inner"), "in"));
	REQUIRE(storage::renamePath(s.at("dir"), s.at("other/dir"), false).ok());
	REQUIRE(slurp(s.at("other/dir/inner")) == "in");

	// A link moves as a link: what it leads to, outside the roots, stays.
	REQUIRE(make(s.off("target"), "out there"));
	REQUIRE(symlink(s.off("target").c_str(), s.at("link").c_str()) == 0);
	REQUIRE(storage::renamePath(s.at("link"), s.at("link2"), false).ok());
	REQUIRE(isLink(s.at("link2")));
	REQUIRE(slurp(s.off("target")) == "out there");
	REQUIRE(present(s.off("target")));

	// Already the name it is to have: nothing to do, and nothing refused,
	// a directory as much as a file.
	REQUIRE(storage::renamePath(s.at("new.txt"), s.at("new.txt"), false).ok());
	REQUIRE(slurp(s.at("new.txt")) == "content");
	REQUIRE(storage::renamePath(s.at("other"), s.at("other"), false).ok());
	REQUIRE(slurp(s.at("other/dir/inner")) == "in");
}

TEST_CASE("a taken name is replaced only with leave and only by a plain file over a plain file", "[storage][rename]")
{
	Scratch s;
	REQUIRE_FALSE(s.top.empty());

	REQUIRE(make(s.at("a"), "A"));
	REQUIRE(make(s.at("b"), "B"));
	expectRefusal(storage::renamePath(s.at("a"), s.at("b"), false), Status::Conflict, ErrorCode::NameTaken);
	REQUIRE(slurp(s.at("a")) == "A");
	REQUIRE(slurp(s.at("b")) == "B");

	REQUIRE(storage::renamePath(s.at("a"), s.at("b"), true).ok());
	REQUIRE_FALSE(present(s.at("a")));
	REQUIRE(slurp(s.at("b")) == "A");

	// Neither a directory nor anything but a plain file is replaced.
	REQUIRE(mkdir(s.at("folder").c_str(), 0755) == 0);
	expectRefusal(storage::renamePath(s.at("b"), s.at("folder"), true), Status::Conflict, ErrorCode::NotAPlainFile);
	REQUIRE(mkdir(s.at("folder2").c_str(), 0755) == 0);
	REQUIRE(make(s.at("c"), "C"));
	expectRefusal(storage::renamePath(s.at("folder2"), s.at("c"), true), Status::Conflict, ErrorCode::NotAPlainFile);
	REQUIRE(slurp(s.at("c")) == "C");
	// And an empty directory is no free name either.
	expectRefusal(storage::renamePath(s.at("folder2"), s.at("folder"), false), Status::Conflict, ErrorCode::NameTaken);
}

TEST_CASE("a rename keeps to the roots, keeps a root's name and keeps a directory out of itself", "[storage][rename]")
{
	Scratch s;
	REQUIRE_FALSE(s.top.empty());

	REQUIRE(make(s.at("in"), "in"));
	REQUIRE(make(s.off("out"), "out"));
	expectRefusal(storage::renamePath(s.at("in"), s.off("moved"), false), Status::InvalidArgument, ErrorCode::OutsideRoots);
	expectRefusal(storage::renamePath(s.off("out"), s.at("moved"), false), Status::InvalidArgument, ErrorCode::OutsideRoots);
	REQUIRE(slurp(s.at("in")) == "in");
	REQUIRE(slurp(s.off("out")) == "out");

	/* A root whose parent lies outside the roots is out of reach already; the
	   case the refusal of its own is for is a root inside another one, as the
	   recording directory under /media is. */
	expectRefusal(storage::renamePath(s.allowed(), s.at("renamed"), false), Status::InvalidArgument, ErrorCode::OutsideRoots);
	REQUIRE(mkdir(s.at("movie").c_str(), 0755) == 0);
	storage::setRoots(s.allowed() + ":" + s.at("movie"));
	expectRefusal(storage::renamePath(s.at("movie"), s.at("films"), false), Status::InvalidArgument, ErrorCode::IsARoot);
	REQUIRE(present(s.at("movie")));

	// And the directory that holds such a root, which would take it along.
	REQUIRE(mkdir(s.at("hdd").c_str(), 0755) == 0);
	REQUIRE(mkdir(s.at("hdd/movie").c_str(), 0755) == 0);
	storage::setRoots(s.allowed() + ":" + s.at("hdd/movie"));
	expectRefusal(storage::renamePath(s.at("hdd"), s.at("hdd2"), false), Status::InvalidArgument, ErrorCode::IsARoot);
	REQUIRE(present(s.at("hdd/movie")));
	storage::setRoots(s.allowed());

	REQUIRE(mkdir(s.at("d").c_str(), 0755) == 0);
	REQUIRE(mkdir(s.at("d/e").c_str(), 0755) == 0);
	expectRefusal(storage::renamePath(s.at("d"), s.at("d/e/d"), false), Status::InvalidArgument, ErrorCode::IntoItself);
	REQUIRE(present(s.at("d/e")));

	expectRefusal(storage::renamePath(s.at("nothing"), s.at("x"), false), Status::NotFound, ErrorCode::NoSuchName);
	expectRefusal(storage::renamePath(s.at("in"), s.at("no/such/dir"), false), Status::NotFound, ErrorCode::NoSuchParent);
	expectRefusal(storage::renamePath("relative", s.at("x"), false), Status::InvalidArgument, ErrorCode::RelativePath);
}

TEST_CASE("a rename across two filesystems is refused and leaves both", "[storage][rename]")
{
	Scratch s;
	REQUIRE_FALSE(s.top.empty());

	/* A second filesystem the roots can take in: /dev/shm, where the host has
	   one apart from the scratch directory's. Without one there is nothing to
	   cross, and the case says so rather than passing. */
	struct stat a, b;
	if (stat("/dev/shm", &b) != 0 || stat(s.allowed().c_str(), &a) != 0 || a.st_dev == b.st_dev)
	{
		WARN("no second filesystem beside " + s.allowed() + " on this host");
		return;
	}
	char tmpl[] = "/dev/shm/coreapi_move_XXXXXX";
	REQUIRE(mkdtemp(tmpl) != NULL);
	const std::string shm(tmpl);
	storage::setRoots(s.allowed() + ":" + shm);

	REQUIRE(make(s.at("f"), "here"));
	const Result<void> r = storage::renamePath(s.at("f"), shm + "/f", false);
	(void) system(("rm -rf '" + shm + "'").c_str());
	expectRefusal(r, Status::InvalidArgument, ErrorCode::CrossDevice);
	REQUIRE(slurp(s.at("f")) == "here");
}

TEST_CASE("pieces are added only where the file has got to", "[storage][append]")
{
	Scratch s;
	REQUIRE_FALSE(s.top.empty());

	const std::string name = s.at("big.part");
	REQUIRE(make(name, "abc"));
	REQUIRE(storage::appendFile(name, 3, "def").ok());
	REQUIRE(slurp(name) == "abcdef");

	// A piece that came twice, and one that skipped a piece: both refused, and
	// the file left as it was.
	expectRefusal(storage::appendFile(name, 3, "def"), Status::Conflict, ErrorCode::OffsetMismatch);
	expectRefusal(storage::appendFile(name, 9, "jkl"), Status::Conflict, ErrorCode::OffsetMismatch);
	REQUIRE(slurp(name) == "abcdef");

	// Every byte value, a zero among them, arrives as sent.
	std::string bytes;
	for (int i = 0; i < 256; ++i)
		bytes += (char) i;
	REQUIRE(storage::appendFile(name, 6, bytes).ok());
	REQUIRE(slurp(name) == "abcdef" + bytes);

	// No bytes at the right offset change nothing and are not refused.
	REQUIRE(storage::appendFile(name, 6 + 256, std::string()).ok());
	REQUIRE(slurp(name).size() == 6 + 256);
}

TEST_CASE("a piece goes to a plain file inside the roots and nowhere else", "[storage][append]")
{
	Scratch s;
	REQUIRE_FALSE(s.top.empty());

	expectRefusal(storage::appendFile(s.at("none"), 0, "x"), Status::NotFound, ErrorCode::NoSuchName);
	REQUIRE_FALSE(present(s.at("none")));

	REQUIRE(mkdir(s.at("dir").c_str(), 0755) == 0);
	expectRefusal(storage::appendFile(s.at("dir"), 0, "x"), Status::Conflict, ErrorCode::NotAPlainFile);

	// A link at the name is refused and not written through, out of the roots.
	REQUIRE(make(s.off("outside"), ""));
	REQUIRE(symlink(s.off("outside").c_str(), s.at("link").c_str()) == 0);
	expectRefusal(storage::appendFile(s.at("link"), 0, "x"), Status::Conflict, ErrorCode::NotAPlainFile);
	REQUIRE(slurp(s.off("outside")).empty());

	// A pipe answers at once rather than holding the request for a reader.
	REQUIRE(mkfifo(s.at("pipe").c_str(), 0600) == 0);
	expectRefusal(storage::appendFile(s.at("pipe"), 0, "x"), Status::Conflict, ErrorCode::NotAPlainFile);

	REQUIRE(make(s.off("beside"), ""));
	expectRefusal(storage::appendFile(s.off("beside"), 0, "x"), Status::InvalidArgument, ErrorCode::OutsideRoots);
	REQUIRE(slurp(s.off("beside")).empty());
}
