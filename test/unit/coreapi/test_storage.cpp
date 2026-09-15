/*
 * test_storage.cpp - tests for mounts and directories
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
#include "coreapi/storage.h"
#include "coreapi/box/storage_internal.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

#include <dirent.h>
#include <fcntl.h>
#include <limits.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/statfs.h>
#include <sys/sysmacros.h>
#include <sys/types.h>
#include <sys/un.h>
#include <unistd.h>

#include <config.h>

using namespace coreapi;

namespace
{

typedef Result<std::vector<FileEntry> > Listing;

bool writeFile(const std::string &path, const std::string &body)
{
	int fd = open(path.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0600);
	if (fd < 0)
		return false;
	ssize_t n = write(fd, body.data(), body.size());
	if (close(fd) != 0)
		return false;
	return n == (ssize_t) body.size();
}

// A name outside the sandbox, removed whatever becomes of the case that made
// it. A check that fails leaves by throwing, so anything written after it is
// never reached, and a name left behind out there is read by the next run as
// the state of the box rather than as the wreckage of a case that failed.
struct Sweep
{
	std::string path;

	explicit Sweep(const std::string &p) : path(p) {}
	~Sweep() { remove(path.c_str()); }
};

// A directory of its own per case, so nothing a case leaves behind can be what
// the next one reads.
struct Sandbox
{
	std::string dir;

	Sandbox()
	{
		char tmpl[] = "/tmp/coreapi_storage_XXXXXX";
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

// A number the kernel handed out between two readings of its own is neither
// wrong nor pinned to either one of them.
bool between64(uint64_t value, uint64_t a, uint64_t b)
{
	const uint64_t low = (a < b) ? a : b;
	const uint64_t high = (a < b) ? b : a;
	return value >= low && value <= high;
}

// Read while this file is being loaded, because by the time any case runs the
// roots are whatever the case before it set.
const std::string g_initial_roots = storage::roots();

const FileEntry *findEntry(const std::vector<FileEntry> &entries, const char *name)
{
	for (size_t i = 0; i < entries.size(); i++)
	{
		if (entries[i].name == name)
			return &entries[i];
	}
	return NULL;
}

} // namespace

TEST_CASE("a path outside every root is rejected", "[storage]")
{
	storage::setRoots("/tmp");

	Result<std::vector<FileEntry> > ok_path = storage::list("/tmp");
	REQUIRE(ok_path.ok());

	Result<std::vector<FileEntry> > bad = storage::list("/etc");
	REQUIRE_FALSE(bad.ok());
	REQUIRE(bad.error().status == Status::InvalidArgument);
	REQUIRE(bad.error().code == ErrorCode::OutsideRoots);
}

TEST_CASE("traversal out of a root is rejected", "[storage]")
{
	storage::setRoots("/tmp");
	Result<std::vector<FileEntry> > r = storage::list("/tmp/../etc");
	REQUIRE_FALSE(r.ok());
	REQUIRE(r.error().status == Status::InvalidArgument);
	REQUIRE(r.error().code == ErrorCode::OutsideRoots);
}

TEST_CASE("a symlink pointing out of a root is not followed", "[storage]")
{
	// Named with the process, so a second run of this suite planting the same
	// tree at the same moment neither rm's this one's root out from under it
	// nor lists a symlink this one never made.
	const std::string root_dir = "/tmp/coreapi-root." + std::to_string(getpid());
	const std::string escape_link = root_dir + "/escape";
	system(("rm -rf " + root_dir + " && mkdir -p " + root_dir).c_str());
	system(("ln -s /etc " + escape_link).c_str());
	// The directory last, so that what it holds goes before it does.
	Sweep root(root_dir);
	Sweep escape(escape_link);
	storage::setRoots(root_dir);

	Result<std::vector<FileEntry> > r = storage::list(escape_link);
	REQUIRE_FALSE(r.ok());
	REQUIRE(r.error().status == Status::InvalidArgument);
	REQUIRE(r.error().code == ErrorCode::OutsideRoots);
}

TEST_CASE("a missing directory is NotFound rather than an empty list", "[storage]")
{
	storage::setRoots("/tmp");
	Result<std::vector<FileEntry> > r = storage::list("/tmp/does-not-exist-4711");
	REQUIRE_FALSE(r.ok());
	REQUIRE(r.error().status == Status::NotFound);
	REQUIRE(r.error().code == ErrorCode::NoSuchName);
}

TEST_CASE("containment is decided before existence", "[storage]")
{
	storage::setRoots("/tmp");

	// The same name is missing in both, and the two answers have to differ:
	// one refusal says the box has no such name, the other says nothing at
	// all about what is there.
	Listing inside = storage::list("/tmp/nothing-of-that-name-4711");
	REQUIRE_FALSE(inside.ok());
	REQUIRE(inside.error().status == Status::NotFound);
	REQUIRE(inside.error().code == ErrorCode::NoSuchName);

	Listing outside = storage::list("/etc/nothing-of-that-name-4711");
	REQUIRE_FALSE(outside.ok());
	REQUIRE(outside.error().status == Status::InvalidArgument);
	REQUIRE(outside.error().code == ErrorCode::OutsideRoots);
}

TEST_CASE("a root does not admit a name that only starts like it", "[storage]")
{
	Sandbox box;
	REQUIRE_FALSE(box.dir.empty());
	REQUIRE(mkdir(box.at("media").c_str(), 0700) == 0);
	REQUIRE(mkdir(box.at("mediaXYZ").c_str(), 0700) == 0);

	storage::setRoots(box.at("media"));

	REQUIRE(storage::list(box.at("media")).ok());

	Listing sibling = storage::list(box.at("mediaXYZ"));
	REQUIRE_FALSE(sibling.ok());
	REQUIRE(sibling.error().status == Status::InvalidArgument);
	REQUIRE(sibling.error().code == ErrorCode::OutsideRoots);
}

TEST_CASE("a name beside a root is outside it", "[storage]")
{
	Sandbox box;
	REQUIRE_FALSE(box.dir.empty());
	REQUIRE(mkdir(box.at("aaa").c_str(), 0700) == 0);
	REQUIRE(mkdir(box.at("bbb").c_str(), 0700) == 0);
	REQUIRE(mkdir(box.at("bbb/inner").c_str(), 0700) == 0);

	storage::setRoots(box.at("aaa"));

	// The two names are the same length up to the separator, so anything that
	// only counts characters and looks for the separator lets this one in.
	Listing sibling = storage::list(box.at("bbb/inner"));
	REQUIRE_FALSE(sibling.ok());
	REQUIRE(sibling.error().status == Status::InvalidArgument);
	REQUIRE(sibling.error().code == ErrorCode::OutsideRoots);
}

TEST_CASE("a root of the whole filesystem admits nothing", "[storage]")
{
	Sandbox box;
	REQUIRE_FALSE(box.dir.empty());
	REQUIRE(mkdir(box.at("real").c_str(), 0700) == 0);

	// Every name on the box sits under this one, so a list holding it is the
	// absence of a list rather than a list, and the rule that a listing cannot
	// start at the top of the filesystem is kept here and not left to whoever
	// writes the setting.
	storage::setRoots("/");

	Listing elsewhere = storage::list("/etc");
	REQUIRE_FALSE(elsewhere.ok());
	REQUIRE(elsewhere.error().status == Status::InvalidArgument);
	REQUIRE(elsewhere.error().code == ErrorCode::OutsideRoots);

	Listing top = storage::list("/");
	REQUIRE_FALSE(top.ok());
	REQUIRE(top.error().code == ErrorCode::OutsideRoots);

	// Nor does it widen a list it is only one element of.
	storage::setRoots(box.at("real") + ":/");
	REQUIRE(storage::list(box.at("real")).ok());
	Listing beside = storage::list("/etc");
	REQUIRE_FALSE(beside.ok());
	REQUIRE(beside.error().code == ErrorCode::OutsideRoots);

	// The check is on what an element resolves to and not on how it was spelt,
	// so neither another spelling of it nor a link to it gets through.
	storage::setRoots("//");
	REQUIRE_FALSE(storage::list("/etc").ok());

	REQUIRE(symlink("/", box.at("toroot").c_str()) == 0);
	storage::setRoots(box.at("toroot"));
	Listing through = storage::list("/etc");
	REQUIRE_FALSE(through.ok());
	REQUIRE(through.error().code == ErrorCode::OutsideRoots);

	// And what may not be listed may not be written to either. Swept whatever
	// happens, because a check that fails here would leave it in the way of the
	// next run.
	Sweep swept("/coreapi-should-not-be-here");
	Result<void> made = storage::createDirectory("/coreapi-should-not-be-here");
	REQUIRE_FALSE(made.ok());
	REQUIRE(made.error().code == ErrorCode::OutsideRoots);
	struct stat st;
	REQUIRE(stat("/coreapi-should-not-be-here", &st) != 0);
}

TEST_CASE("an element that names nothing admits nothing", "[storage]")
{
	Sandbox box;
	REQUIRE_FALSE(box.dir.empty());
	REQUIRE(mkdir(box.at("real").c_str(), 0700) == 0);

	// A list written with a stray separator, which is what a setting assembled
	// out of parts that were not all filled in looks like.
	storage::setRoots(":" + box.at("real") + "::");
	REQUIRE(storage::list(box.at("real")).ok());

	Listing outside = storage::list("/etc");
	REQUIRE_FALSE(outside.ok());
	REQUIRE(outside.error().status == Status::InvalidArgument);
	REQUIRE(outside.error().code == ErrorCode::OutsideRoots);
}

TEST_CASE("a root that is not there admits nothing while the others still answer", "[storage]")
{
	Sandbox box;
	REQUIRE_FALSE(box.dir.empty());
	REQUIRE(mkdir(box.at("real").c_str(), 0700) == 0);

	storage::setRoots(box.at("gone") + ":" + box.at("real"));
	REQUIRE(storage::list(box.at("real")).ok());

	Listing gone = storage::list(box.at("gone"));
	REQUIRE_FALSE(gone.ok());
	REQUIRE(gone.error().status == Status::InvalidArgument);
	REQUIRE(gone.error().code == ErrorCode::OutsideRoots);
}

TEST_CASE("a listing says what kind each name is", "[storage]")
{
	Sandbox box;
	REQUIRE_FALSE(box.dir.empty());
	storage::setRoots(box.dir);

	REQUIRE(writeFile(box.at("plain.txt"), "0123456789"));
	REQUIRE(mkdir(box.at("subdir").c_str(), 0700) == 0);
	REQUIRE(symlink(box.at("plain.txt").c_str(), box.at("alink").c_str()) == 0);

	// Given away to numbers nothing here is, so that an owner that was never
	// filled in cannot pass for the one this process happens to have.
	const bool given_away = (chown(box.at("plain.txt").c_str(), 1234, 5678) == 0);

	Listing r = storage::list(box.dir);
	REQUIRE(r.ok());
	const std::vector<FileEntry> &entries = r.value();
	REQUIRE(entries.size() == 3);

	REQUIRE(findEntry(entries, ".") == NULL);
	REQUIRE(findEntry(entries, "..") == NULL);

	const FileEntry *file = findEntry(entries, "plain.txt");
	REQUIRE(file != NULL);
	REQUIRE(file->kind == FileKind::Regular);
	REQUIRE(file->size == 10);
	if (given_away)
	{
		REQUIRE(file->uid == 1234);
		REQUIRE(file->gid == 5678);
	}
	else
	{
		REQUIRE(file->uid == getuid());
		REQUIRE(file->gid == getgid());
	}
	REQUIRE(file->mtime > 0);

	const FileEntry *dir = findEntry(entries, "subdir");
	REQUIRE(dir != NULL);
	REQUIRE(dir->kind == FileKind::Dir);

	// The kind is what the directory said, so a link stays a link; the size
	// is what a stat answered, so it is the size of what the link points at.
	const FileEntry *link = findEntry(entries, "alink");
	REQUIRE(link != NULL);
	REQUIRE(link->kind == FileKind::Link);
	REQUIRE(link->size == 10);
}

TEST_CASE("a link that points at nothing stays in the listing", "[storage]")
{
	Sandbox box;
	REQUIRE_FALSE(box.dir.empty());
	storage::setRoots(box.dir);

	REQUIRE(symlink(box.at("never-was").c_str(), box.at("broken").c_str()) == 0);

	Listing r = storage::list(box.dir);
	REQUIRE(r.ok());
	const FileEntry *broken = findEntry(r.value(), "broken");
	REQUIRE(broken != NULL);
	REQUIRE(broken->kind == FileKind::Link);
	// Nothing to stat, so everything a stat would have filled stays at zero
	// rather than the name being dropped.
	REQUIRE(broken->size == 0);
	REQUIRE(broken->mtime == 0);
	REQUIRE(broken->uid == 0);
	REQUIRE(broken->gid == 0);
}

TEST_CASE("listing something that is not a directory is NotFound", "[storage]")
{
	Sandbox box;
	REQUIRE_FALSE(box.dir.empty());
	storage::setRoots(box.dir);
	REQUIRE(writeFile(box.at("plain.txt"), "x"));

	Listing r = storage::list(box.at("plain.txt"));
	REQUIRE_FALSE(r.ok());
	REQUIRE(r.error().status == Status::NotFound);
	REQUIRE(r.error().code == ErrorCode::NoSuchName);
}

TEST_CASE("a separator at the end names the directory and not something under it", "[storage]")
{
	Sandbox box;
	REQUIRE_FALSE(box.dir.empty());
	storage::setRoots(box.dir);
	REQUIRE(mkdir(box.at("inner").c_str(), 0700) == 0);
	REQUIRE(writeFile(box.at("inner/plain.txt"), "x"));

	// The same directory under three spellings, and none of them may read as a
	// name that is not there.
	Listing bare = storage::list(box.at("inner"));
	REQUIRE(bare.ok());
	REQUIRE(bare.value().size() == 1);

	Listing trailing = storage::list(box.at("inner") + "/");
	REQUIRE(trailing.ok());
	REQUIRE(trailing.value().size() == 1);

	Listing doubled = storage::list(box.at("inner") + "///");
	REQUIRE(doubled.ok());
	REQUIRE(doubled.value().size() == 1);

	// And the same when the last component is what is being made.
	REQUIRE(storage::createDirectory(box.at("fresh") + "/").ok());
	struct stat st;
	REQUIRE(stat(box.at("fresh").c_str(), &st) == 0);
	REQUIRE(S_ISDIR(st.st_mode));
	REQUIRE(storage::removePath(box.at("fresh") + "/").ok());
	REQUIRE(stat(box.at("fresh").c_str(), &st) != 0);
}

TEST_CASE("a name that ends in a dot names no leaf to act on", "[storage]")
{
	Sandbox box;
	REQUIRE_FALSE(box.dir.empty());
	storage::setRoots(box.dir);
	REQUIRE(mkdir(box.at("keep").c_str(), 0700) == 0);

	// Both of these resolve to a directory that is already there, so acting on
	// them would either fail confusingly or remove the wrong thing.
	Result<void> dot = storage::removePath(box.at("keep") + "/.");
	REQUIRE_FALSE(dot.ok());
	REQUIRE(dot.error().status == Status::InvalidArgument);
	REQUIRE(dot.error().code == ErrorCode::BadName);

	Result<void> dotdot = storage::removePath(box.at("keep") + "/..");
	REQUIRE_FALSE(dotdot.ok());
	REQUIRE(dotdot.error().status == Status::InvalidArgument);
	REQUIRE(dotdot.error().code == ErrorCode::BadName);

	Result<void> made = storage::createDirectory(box.at("keep") + "/.");
	REQUIRE_FALSE(made.ok());
	REQUIRE(made.error().status == Status::InvalidArgument);
	REQUIRE(made.error().code == ErrorCode::BadName);

	REQUIRE(access(box.at("keep").c_str(), F_OK) == 0);
}

TEST_CASE("a name that is not absolute is refused", "[storage]")
{
	storage::setRoots("/tmp");

	Listing relative = storage::list("tmp");
	REQUIRE_FALSE(relative.ok());
	REQUIRE(relative.error().status == Status::InvalidArgument);
	REQUIRE(relative.error().code == ErrorCode::RelativePath);

	Listing empty = storage::list("");
	REQUIRE_FALSE(empty.ok());
	REQUIRE(empty.error().status == Status::InvalidArgument);
	REQUIRE(empty.error().code == ErrorCode::EmptyPath);
}

TEST_CASE("a name carrying a zero byte is refused", "[storage]")
{
	storage::setRoots("/tmp");
	std::string sneaky("/tmp", 4);
	sneaky.push_back('\0');
	sneaky += "/../../etc";

	Listing r = storage::list(sneaky);
	REQUIRE_FALSE(r.ok());
	REQUIRE(r.error().status == Status::InvalidArgument);
	REQUIRE(r.error().code == ErrorCode::PathHasZeroByte);

	// The counters are read for any name, so this is the one check between a
	// caller and a call that would stop at the byte and answer for /.
	std::string half("/", 1);
	half.push_back('\0');
	half += "no-such-place";
	Result<FsUsage> counters = storage::stats(half);
	REQUIRE_FALSE(counters.ok());
	REQUIRE(counters.error().status == Status::InvalidArgument);
	REQUIRE(counters.error().code == ErrorCode::PathHasZeroByte);
}

TEST_CASE("a name longer than this system allows is refused rather than walked up", "[storage]")
{
	storage::setRoots("/tmp");
	// Long enough that nothing below can resolve it, and inside a root, so
	// that walking up would end at a directory that is there and answer as if
	// only the last name were missing.
	Listing r = storage::list("/tmp/" + std::string(5000, 'a'));
	REQUIRE_FALSE(r.ok());
	REQUIRE(r.error().status == Status::InvalidArgument);
	REQUIRE(r.error().code == ErrorCode::PathTooLong);
}

TEST_CASE("a directory is made only inside a root and only once", "[storage]")
{
	Sandbox box;
	REQUIRE_FALSE(box.dir.empty());
	storage::setRoots(box.dir);

	REQUIRE(storage::createDirectory(box.at("fresh")).ok());
	struct stat st;
	REQUIRE(stat(box.at("fresh").c_str(), &st) == 0);
	REQUIRE(S_ISDIR(st.st_mode));

	Result<void> again = storage::createDirectory(box.at("fresh"));
	REQUIRE_FALSE(again.ok());
	REQUIRE(again.error().status == Status::Conflict);
	REQUIRE(again.error().code == ErrorCode::NameTaken);

	Sweep swept("/etc/coreapi-should-not-be-here");
	Result<void> outside = storage::createDirectory("/etc/coreapi-should-not-be-here");
	REQUIRE_FALSE(outside.ok());
	REQUIRE(outside.error().status == Status::InvalidArgument);
	REQUIRE(outside.error().code == ErrorCode::OutsideRoots);
	REQUIRE(stat("/etc/coreapi-should-not-be-here", &st) != 0);

	// Only the last component is made, so a whole branch cannot be brought
	// into being by one call.
	Result<void> deep = storage::createDirectory(box.at("no/such/parent"));
	REQUIRE_FALSE(deep.ok());
	REQUIRE(deep.error().status == Status::NotFound);
	REQUIRE(deep.error().code == ErrorCode::NoSuchParent);
	REQUIRE(stat(box.at("no").c_str(), &st) != 0);
}

TEST_CASE("removing takes a file or an empty directory and nothing else", "[storage]")
{
	Sandbox box;
	REQUIRE_FALSE(box.dir.empty());
	storage::setRoots(box.dir);

	REQUIRE(writeFile(box.at("gone.txt"), "x"));
	REQUIRE(storage::removePath(box.at("gone.txt")).ok());
	REQUIRE(access(box.at("gone.txt").c_str(), F_OK) != 0);

	Result<void> missing = storage::removePath(box.at("gone.txt"));
	REQUIRE_FALSE(missing.ok());
	REQUIRE(missing.error().status == Status::NotFound);
	REQUIRE(missing.error().code == ErrorCode::NoSuchName);

	REQUIRE(mkdir(box.at("full").c_str(), 0700) == 0);
	REQUIRE(writeFile(box.at("full/inside.txt"), "x"));
	Result<void> full = storage::removePath(box.at("full"));
	REQUIRE_FALSE(full.ok());
	REQUIRE(full.error().status == Status::Conflict);
	REQUIRE(full.error().code == ErrorCode::NotEmpty);
	REQUIRE(access(box.at("full/inside.txt").c_str(), F_OK) == 0);

	REQUIRE(mkdir(box.at("bare").c_str(), 0700) == 0);
	REQUIRE(storage::removePath(box.at("bare")).ok());
	REQUIRE(access(box.at("bare").c_str(), F_OK) != 0);

	Result<void> outside = storage::removePath("/etc/passwd");
	REQUIRE_FALSE(outside.ok());
	REQUIRE(outside.error().status == Status::InvalidArgument);
	REQUIRE(outside.error().code == ErrorCode::OutsideRoots);
	REQUIRE(access("/etc/passwd", F_OK) == 0);
}

TEST_CASE("removing a link takes the link and not what it points at", "[storage]")
{
	Sandbox box;
	REQUIRE_FALSE(box.dir.empty());
	storage::setRoots(box.dir);

	REQUIRE(writeFile(box.at("target.txt"), "x"));
	REQUIRE(symlink(box.at("target.txt").c_str(), box.at("pointer").c_str()) == 0);

	REQUIRE(storage::removePath(box.at("pointer")).ok());
	REQUIRE(access(box.at("target.txt").c_str(), F_OK) == 0);
	struct stat st;
	REQUIRE(lstat(box.at("pointer").c_str(), &st) != 0);

	// A link to a directory is the case that tells the two kinds of stat
	// apart: one that followed it would try to remove a directory under the
	// link's own name and get nowhere.
	REQUIRE(mkdir(box.at("adir").c_str(), 0700) == 0);
	REQUIRE(writeFile(box.at("adir/inside.txt"), "x"));
	REQUIRE(symlink(box.at("adir").c_str(), box.at("todir").c_str()) == 0);

	REQUIRE(storage::removePath(box.at("todir")).ok());
	REQUIRE(lstat(box.at("todir").c_str(), &st) != 0);
	REQUIRE(access(box.at("adir/inside.txt").c_str(), F_OK) == 0);
}

TEST_CASE("a call the kernel turned down is said to have been turned down", "[storage]")
{
	Sandbox box;
	REQUIRE_FALSE(box.dir.empty());
	storage::setRoots(box.dir);

	// The last component is longer than a name may be, while the whole of it is
	// still short enough to get past the length gate, so what fails is the call
	// and not a check in front of it.
	Result<void> too_long = storage::createDirectory(box.at(std::string(400, 'n')));
	REQUIRE_FALSE(too_long.ok());
	REQUIRE(too_long.error().status == Status::Internal);
	REQUIRE(too_long.error().code == ErrorCode::ChangeRefused);

	// The process table is a filesystem the kernel answers for and refuses
	// every write to, whoever is asking, so these two reach the call and come
	// back refused without the suite having to give up a privilege first.
	storage::setRoots("/proc");

	Result<void> no_unlink = storage::removePath("/proc/version");
	REQUIRE_FALSE(no_unlink.ok());
	REQUIRE(no_unlink.error().status == Status::Internal);
	REQUIRE(no_unlink.error().code == ErrorCode::ChangeRefused);
	REQUIRE(access("/proc/version", F_OK) == 0);

	Result<void> no_rmdir = storage::removePath("/proc/sys");
	REQUIRE_FALSE(no_rmdir.ok());
	REQUIRE(no_rmdir.error().status == Status::Internal);
	REQUIRE(no_rmdir.error().code == ErrorCode::ChangeRefused);
	REQUIRE(access("/proc/sys", F_OK) == 0);
}

TEST_CASE("every kind a directory can report comes back as itself", "[storage]")
{
	Sandbox box;
	REQUIRE_FALSE(box.dir.empty());
	storage::setRoots(box.dir);

	REQUIRE(writeFile(box.at("aregular"), "x"));
	REQUIRE(mkdir(box.at("adir").c_str(), 0700) == 0);
	REQUIRE(symlink(box.at("aregular").c_str(), box.at("alink").c_str()) == 0);
	REQUIRE(mkfifo(box.at("afifo").c_str(), 0600) == 0);

	// A name in the filesystem is what makes a socket, and binding one needs
	// nothing but the right to write here.
	const int sock = socket(AF_UNIX, SOCK_STREAM, 0);
	REQUIRE(sock >= 0);
	struct sockaddr_un addr;
	memset(&addr, 0, sizeof(addr));
	addr.sun_family = AF_UNIX;
	const std::string sockpath = box.at("asock");
	REQUIRE(sockpath.size() < sizeof(addr.sun_path));
	memcpy(addr.sun_path, sockpath.data(), sockpath.size());
	REQUIRE(bind(sock, (struct sockaddr *) &addr, sizeof(addr)) == 0);
	close(sock);

	// Making a device node needs a privilege the suite may not have.
	const bool nodes = (mknod(box.at("achr").c_str(), S_IFCHR | 0600, makedev(1, 3)) == 0 &&
			    mknod(box.at("ablk").c_str(), S_IFBLK | 0600, makedev(7, 0)) == 0);

	Listing r = storage::list(box.dir);
	REQUIRE(r.ok());
	const std::vector<FileEntry> &entries = r.value();

	REQUIRE(findEntry(entries, "aregular")->kind == FileKind::Regular);
	REQUIRE(findEntry(entries, "adir")->kind == FileKind::Dir);
	REQUIRE(findEntry(entries, "alink")->kind == FileKind::Link);
	REQUIRE(findEntry(entries, "afifo")->kind == FileKind::Fifo);
	REQUIRE(findEntry(entries, "asock")->kind == FileKind::Socket);
	if (nodes)
	{
		REQUIRE(findEntry(entries, "achr")->kind == FileKind::CharDevice);
		REQUIRE(findEntry(entries, "ablk")->kind == FileKind::BlockDevice);
	}

	// None of the five folds into another, which is what a reader that prints
	// the kind back as the number it came from depends on.
	REQUIRE(findEntry(entries, "afifo")->kind != FileKind::Unknown);
	REQUIRE(findEntry(entries, "asock")->kind != FileKind::Unknown);
	REQUIRE(findEntry(entries, "afifo")->kind != findEntry(entries, "asock")->kind);
}

TEST_CASE("the two kinds no sandbox can make are named too", "[storage]")
{
	// A union filesystem hides a name with a whiteout, and a filesystem that
	// does not keep the kind beside the name answers unknown. Neither can be
	// made in a directory this suite owns, so the translation is driven
	// straight rather than through a listing.
	REQUIRE(storage::internal::kindOf(DT_WHT) == FileKind::Whiteout);
	REQUIRE(storage::internal::kindOf(DT_UNKNOWN) == FileKind::Unknown);

	// And a number no directory defines is not quietly read as one that is.
	REQUIRE(storage::internal::kindOf(15) == FileKind::Unknown);

	// The seven a directory here can report, driven the same way, so that the
	// mapping is pinned whole in one place.
	REQUIRE(storage::internal::kindOf(DT_FIFO) == FileKind::Fifo);
	REQUIRE(storage::internal::kindOf(DT_CHR) == FileKind::CharDevice);
	REQUIRE(storage::internal::kindOf(DT_DIR) == FileKind::Dir);
	REQUIRE(storage::internal::kindOf(DT_BLK) == FileKind::BlockDevice);
	REQUIRE(storage::internal::kindOf(DT_REG) == FileKind::Regular);
	REQUIRE(storage::internal::kindOf(DT_LNK) == FileKind::Link);
	REQUIRE(storage::internal::kindOf(DT_SOCK) == FileKind::Socket);
}

TEST_CASE("one look at a name answers for every field of it", "[storage]")
{
	Sandbox box;
	REQUIRE_FALSE(box.dir.empty());
	storage::setRoots(box.dir);

	REQUIRE(writeFile(box.at("plain.txt"), "0123456789"));
	REQUIRE(chmod(box.at("plain.txt").c_str(), 0641) == 0);
	REQUIRE(link(box.at("plain.txt").c_str(), box.at("second.txt").c_str()) == 0);

	Listing r = storage::list(box.dir);
	REQUIRE(r.ok());
	const FileEntry *file = findEntry(r.value(), "plain.txt");
	REQUIRE(file != NULL);

	REQUIRE(file->attributes_read);
	REQUIRE((file->mode & 07777) == 0641);
	REQUIRE(S_ISREG(file->mode));
	// Two names for the one file, which is what a link count counts.
	REQUIRE(file->nlink == 2);
	REQUIRE(file->size == 10);
}

TEST_CASE("a name that could not be looked at says so", "[storage]")
{
	Sandbox box;
	REQUIRE_FALSE(box.dir.empty());
	storage::setRoots(box.dir);

	REQUIRE(writeFile(box.at("there"), "x"));
	REQUIRE(symlink(box.at("never-was").c_str(), box.at("broken").c_str()) == 0);

	Listing r = storage::list(box.dir);
	REQUIRE(r.ok());

	REQUIRE(findEntry(r.value(), "there")->attributes_read);
	// A reader has to be able to tell nought because that is the owner from
	// nought because nothing was read.
	REQUIRE_FALSE(findEntry(r.value(), "broken")->attributes_read);
	REQUIRE(findEntry(r.value(), "broken")->mode == 0);
	REQUIRE(findEntry(r.value(), "broken")->nlink == 0);
}

TEST_CASE("a name under a directory too long to spell is still read", "[storage]")
{
	Sandbox box;
	REQUIRE_FALSE(box.dir.empty());

	// A directory whose own name is nearly as long as a name may be, reached
	// through a short link. Anything that builds a path out of the resolved
	// directory and the name under it runs past the limit and reads nothing,
	// where the entry is perfectly readable through the directory itself.
	std::string deep = box.dir;
	while (deep.size() < 3900)
	{
		deep += "/" + std::string(200, 'd');
		REQUIRE(mkdir(deep.c_str(), 0700) == 0);
	}
	REQUIRE(deep.size() < PATH_MAX);

	const std::string shortcut = box.at("s");
	REQUIRE(symlink(deep.c_str(), shortcut.c_str()) == 0);

	// Written through the link, because the name it is written under is one
	// nothing can spell out in full.
	const std::string name(64, 'n');
	REQUIRE(deep.size() + 1 + name.size() > PATH_MAX);
	REQUIRE(writeFile(shortcut + "/" + name, "0123456789012"));
	REQUIRE(chown((shortcut + "/" + name).c_str(), 1234, 5678) == 0);

	storage::setRoots(box.dir);
	Listing r = storage::list(shortcut);
	REQUIRE(r.ok());
	const FileEntry *entry = findEntry(r.value(), name.c_str());
	REQUIRE(entry != NULL);
	REQUIRE(entry->attributes_read);
	REQUIRE(entry->uid == 1234);
	REQUIRE(entry->gid == 5678);
	REQUIRE(entry->size == 13);
}

TEST_CASE("the mounted filesystems carry at least the one this is running on", "[storage]")
{
	Result<std::vector<MountInfo> > r = storage::mounts();
	REQUIRE(r.ok());
	const std::vector<MountInfo> &all = r.value();
	REQUIRE_FALSE(all.empty());

	const MountInfo *root = NULL;
	for (size_t i = 0; i < all.size(); i++)
	{
		if (all[i].mountpoint == "/")
			root = &all[i];
	}
	REQUIRE(root != NULL);
	REQUIRE_FALSE(root->device.empty());
	REQUIRE_FALSE(root->fstype.empty());
	REQUIRE(root->total > 0);
	REQUIRE(root->free <= root->total);
}

TEST_CASE("a mount point carrying a space arrives with the space back in it", "[storage]")
{
	Sandbox box;
	REQUIRE_FALSE(box.dir.empty());
	const std::string table = box.at("mounts");
	REQUIRE(writeFile(table,
			  "/dev/sda1 /media/My\\040Disk ext4 rw,relatime 0 0\n"
			  "\n"
			  "onlytwo fields\n"
			  "tmpfs /run tmpfs rw 0 0\n"));

	const char *before = storage::internal::mounts_path;
	storage::internal::mounts_path = table.c_str();
	Result<std::vector<MountInfo> > r = storage::mounts();
	storage::internal::mounts_path = before;

	REQUIRE(r.ok());
	const std::vector<MountInfo> &all = r.value();
	// Two, because a line that does not carry the three fields a mount is
	// named by is not a mount.
	REQUIRE(all.size() == 2);
	REQUIRE(all[0].device == "/dev/sda1");
	REQUIRE(all[0].mountpoint == "/media/My Disk");
	REQUIRE(all[0].fstype == "ext4");
	// Nothing is mounted there, so the counters stay at zero rather than the
	// line being dropped.
	REQUIRE(all[0].total == 0);
	REQUIRE(all[1].device == "tmpfs");
	REQUIRE(all[1].mountpoint == "/run");
	REQUIRE(all[1].fstype == "tmpfs");
}

TEST_CASE("a mount table that cannot be read is an error rather than an empty list", "[storage]")
{
	const char *before = storage::internal::mounts_path;
	storage::internal::mounts_path = "/tmp/coreapi-no-such-mount-table-4711";
	Result<std::vector<MountInfo> > r = storage::mounts();
	storage::internal::mounts_path = before;

	REQUIRE_FALSE(r.ok());
	REQUIRE(r.error().status == Status::Internal);
	REQUIRE(r.error().code == ErrorCode::NoMountTable);
}

TEST_CASE("the room a filesystem has is answered only for a name inside a root", "[storage]")
{
	Sandbox box;
	REQUIRE_FALSE(box.dir.empty());
	storage::setRoots(box.dir);

	Result<FsUsage> inside = storage::stats(box.dir);
	REQUIRE(inside.ok());
	REQUIRE(inside.value().total > 0);
	REQUIRE(inside.value().free <= inside.value().total);

	// The top of the filesystem is in no root, and asking about it is asking
	// about a name this layer does not answer for, whatever the reason for
	// asking. It used to be exempted so that one endpoint could keep its old
	// answer; that endpoint reads the kernel itself now.
	Result<FsUsage> top = storage::stats("/");
	REQUIRE_FALSE(top.ok());
	REQUIRE(top.error().status == Status::InvalidArgument);
	REQUIRE(top.error().code == ErrorCode::OutsideRoots);

	Result<FsUsage> elsewhere = storage::stats("/etc");
	REQUIRE_FALSE(elsewhere.ok());
	REQUIRE(elsewhere.error().code == ErrorCode::OutsideRoots);

	// Inside a root and not there is the other answer, and the two have to
	// stay apart for the same reason every other call here keeps them apart.
	Result<FsUsage> missing = storage::stats(box.at("no-such-name-4711"));
	REQUIRE_FALSE(missing.ok());
	REQUIRE(missing.error().status == Status::NotFound);
	REQUIRE(missing.error().code == ErrorCode::NoSuchName);

	Result<FsUsage> empty = storage::stats("");
	REQUIRE_FALSE(empty.ok());
	REQUIRE(empty.error().status == Status::InvalidArgument);
	REQUIRE(empty.error().code == ErrorCode::EmptyPath);
}

TEST_CASE("the room is the kernel's numbers in bytes", "[storage]")
{
	Sandbox box;
	REQUIRE_FALSE(box.dir.empty());
	storage::setRoots(box.dir);

	struct statfs before;
	REQUIRE(statfs(box.dir.c_str(), &before) == 0);
	Result<FsUsage> r = storage::stats(box.dir);
	struct statfs after;
	REQUIRE(statfs(box.dir.c_str(), &after) == 0);

	REQUIRE(r.ok());
	const FsUsage &u = r.value();

	REQUIRE(u.total == (uint64_t) before.f_blocks * (uint64_t) before.f_bsize);
	// Available and not unallocated, the same way a mount is described, so the
	// reserve the superuser keeps is not offered as room for a recording.
	REQUIRE(between64(u.free,
			  (uint64_t) before.f_bavail * (uint64_t) before.f_bsize,
			  (uint64_t) after.f_bavail * (uint64_t) after.f_bsize));
	if (before.f_bavail != before.f_bfree)
		REQUIRE(u.free != (uint64_t) before.f_bfree * (uint64_t) before.f_bsize);
}

TEST_CASE("a mount's size is its blocks and what is left of them", "[storage]")
{
	struct statfs direct;
	REQUIRE(statfs("/", &direct) == 0);

	Result<std::vector<MountInfo> > r = storage::mounts();
	struct statfs after;
	REQUIRE(statfs("/", &after) == 0);
	REQUIRE(r.ok());
	const std::vector<MountInfo> &all = r.value();

	const MountInfo *root = NULL;
	for (size_t i = 0; i < all.size(); i++)
	{
		if (all[i].mountpoint == "/")
			root = &all[i];
	}
	REQUIRE(root != NULL);
	REQUIRE(root->total == (uint64_t) direct.f_blocks * (uint64_t) direct.f_bsize);
	// What is available and not what is unallocated: the reserve a filesystem
	// keeps for the superuser is not room a recording can use. Pinned between a
	// reading taken before and one after, because the box writes while the
	// suite runs and an exact match against one of them is a coin toss.
	REQUIRE(between64(root->free,
			  (uint64_t) direct.f_bavail * (uint64_t) direct.f_bsize,
			  (uint64_t) after.f_bavail * (uint64_t) after.f_bsize));
	if (direct.f_bavail != direct.f_bfree)
		REQUIRE(root->free != (uint64_t) direct.f_bfree * (uint64_t) direct.f_bsize);
}

TEST_CASE("the roots a fresh layer starts with are the media and the plugins", "[storage]")
{
	const std::string &initial = g_initial_roots;

	// The defaults are what a consumer widens rather than replaces, so they
	// have to stay what they were whatever anyone has since set.
	REQUIRE(storage::defaultRoots() == initial);
	storage::setRoots("/tmp");
	REQUIRE(storage::defaultRoots() == initial);
	REQUIRE(storage::roots() == "/tmp");

	static const char *const wanted[] = {
		"/media", "/mnt", "/var", "/tmp", "/hdd",
		PLUGINDIR, PLUGINDIR_VAR, PLUGINDIR_MNT, LUAPLUGINDIR, LUAPLUGINDIR_VAR
	};
	for (size_t i = 0; i < sizeof(wanted) / sizeof(wanted[0]); i++)
	{
		// Whole elements, so that a root that is only part of another one
		// cannot pass for it.
		const std::string needle = std::string(":") + wanted[i] + ":";
		REQUIRE((":" + initial + ":").find(needle) != std::string::npos);
	}
	// And / is not one of them, or a listing could start at the top of the
	// filesystem and work down.
	REQUIRE((":" + initial + ":").find(":/:") == std::string::npos);
}
