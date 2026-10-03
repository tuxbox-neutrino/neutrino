/*
 * test_storage_write.cpp - the file manager's whole-file write
 *
 * Copyright (C) 2026 Thilo Graf
 *
 * License: GPL-2.0-or-later
 */

#include "support/catch.hpp"

#include "coreapi/base/errors.h"
#include "coreapi/base/result.h"
#include "coreapi/storage.h"
#include "coreapi/box/storage_internal.h"

#include <system/helpers.h>

#include <atomic>
#include <cstdlib>
#include <cstring>
#include <string>

#include <dirent.h>
#include <fcntl.h>
#include <limits.h>
#include <pthread.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#include <OpenThreads/Mutex>

/* What PUT /api/v1/storage/file writes through. The writer underneath has its
   own cases in test/unit/system; these hold storage::writeFile to what it
   promises on top of it: the looks it takes before it writes, the lock that
   keeps two writes of one file apart, and what each way of failing answers and
   leaves on the disc. */

using namespace coreapi;

namespace
{

/* One directory per case, holding two: the one the roots are set to, and one
   beside it that no root names. Whatever a case expects to stay untouched lives
   in the second, so a refusal can be checked without reaching outside the
   case's own directory. The roots are put back on the way out, because the next
   case in this binary may never set them. */
struct Scratch
{
	std::string top;
	std::string saved_roots;

	Scratch() : saved_roots(storage::roots())
	{
		char tmpl[] = "/tmp/coreapi_writefile_XXXXXX";
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

// The bytes under a name, empty for a name that is not there; a case that has
// to tell the two apart asks present() as well.
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

bool regular(const std::string &path)
{
	struct stat st;
	return lstat(path.c_str(), &st) == 0 && S_ISREG(st.st_mode);
}

mode_t permsOf(const std::string &path)
{
	struct stat st;
	return lstat(path.c_str(), &st) == 0 ? (st.st_mode & 07777) : (mode_t) -1;
}

std::string pointsTo(const std::string &link)
{
	char buf[PATH_MAX];
	const ssize_t n = readlink(link.c_str(), buf, sizeof(buf) - 1);
	return n < 0 ? std::string() : std::string(buf, (size_t) n);
}

size_t countIn(const std::string &dir)
{
	DIR *d = opendir(dir.c_str());
	if (d == NULL)
		return (size_t) -1;
	size_t n = 0;
	struct dirent *e;
	while ((e = readdir(d)) != NULL)
		if (strcmp(e->d_name, ".") != 0 && strcmp(e->d_name, "..") != 0)
			++n;
	closedir(d);
	return n;
}

void expectRefusal(const Result<storage::Wrote> &r, Status s, ErrorCode c)
{
	REQUIRE_FALSE(r.ok());
	REQUIRE(r.error().status == s);
	REQUIRE(r.error().code == c);
}

/* Installed for one case and cleared however it ends: a check that fails
   unwinds past the last line of a case, and the next write in this binary would
   run a probe it never asked for. A probe runs on the writing thread with the
   write lock held, so it may look, record and start a thread, but it neither
   writes through this layer nor locks that lock, and it asserts nothing,
   Catch2 answering for one thread only. */
struct WriteProbe
{
	explicit WriteProbe(storage::internal::Probe p) { storage::internal::setWriteProbeForTest(p); }
	~WriteProbe() { storage::internal::setWriteProbeForTest(NULL); }
};

struct OpenProbe
{
	explicit OpenProbe(storage::internal::Probe p) { storage::internal::setOpenProbeForTest(p); }
	~OpenProbe() { storage::internal::setOpenProbeForTest(NULL); }
};

// A directory a case takes rights away from, given them back however the case
// ends: nothing can empty a directory nobody may read.
struct RightsBack
{
	std::string dir;
	explicit RightsBack(const std::string &d) : dir(d) {}
	~RightsBack() { chmod(dir.c_str(), 0755); }
};

long long msNow()
{
	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	return (long long) ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}

// What the probes below see, written on the writing thread and read by the
// case once the write has returned.
std::string g_name;
std::string g_seen_at_name;
bool g_side_there = false;
mode_t g_side_perms = 0;

void lookMidWrite()
{
	g_seen_at_name = slurp(g_name);
	g_side_there = present(CAtomicFileWriter::sideNameFor(g_name));
	g_side_perms = permsOf(CAtomicFileWriter::sideNameFor(g_name));
}

/* The lock tried from a thread of its own while the write holds it. A thread
   that tries a lock it holds itself is answered by the kind of the lock, so the
   try is made from a thread that holds nothing; it never blocks, so waiting for
   it here is safe. */
std::atomic<int> g_try(-1);

void *tryTheLock(void *)
{
	OpenThreads::Mutex &m = storage::internal::wholeFileWriteMutex();
	const int got = m.trylock();
	if (got == 0)
		m.unlock();
	g_try = got;
	return NULL;
}

void tryWhileWriting()
{
	pthread_t t;
	if (pthread_create(&t, NULL, tryTheLock, NULL) == 0)
		pthread_join(t, NULL);
}

// Tried from the case's own thread once nothing else runs: whatever it gets, it
// gives back, so a lock left held by the write fails the check instead of
// leaving this thread stuck on the next write.
int tryFromHere()
{
	OpenThreads::Mutex &m = storage::internal::wholeFileWriteMutex();
	const int got = m.trylock();
	if (got == 0)
		m.unlock();
	return got;
}

/* A second write of the same name started from inside the first one, which
   then gives it a moment to get through. It must not: the first holds the lock
   from its first look to its commit. */
std::atomic<bool> g_second_started(false);
std::atomic<bool> g_second_done(false);
bool g_second_early = false;
pthread_t g_second;
Result<storage::Wrote> g_second_result = fail(Status::Internal, ErrorCode::ChangeRefused, "not run");

void *secondWrite(void *)
{
	g_second_result = storage::writeFile(g_name, "second", false);
	g_second_done = true;
	return NULL;
}

void startSecondWrite()
{
	if (g_second_started.exchange(true))
		return;
	if (pthread_create(&g_second, NULL, secondWrite, NULL) != 0)
	{
		g_second_started = false;
		return;
	}
	const long long until = msNow() + 100;
	while (msNow() < until && !g_second_done)
		usleep(1000);
	g_second_early = g_second_done;
}

/* Puts a descriptor that refuses writes behind the side file's own, found by
   what it names, so the bytes the write sends next go nowhere. A file in the
   case's directory and not a device, so a writer that went on regardless could
   not change anything outside it. */
std::string g_refuser;
bool g_swapped = false;

void refuseWrites()
{
	struct stat side;
	if (stat(CAtomicFileWriter::sideNameFor(g_name).c_str(), &side) != 0)
		return;
	const int ro = open(g_refuser.c_str(), O_RDONLY | O_CLOEXEC);
	if (ro < 0)
		return;
	DIR *d = opendir("/proc/self/fd");
	if (d != NULL)
	{
		struct dirent *e;
		while ((e = readdir(d)) != NULL)
		{
			const int fd = atoi(e->d_name);
			if (fd <= 2 || fd == ro || fd == dirfd(d))
				continue;
			struct stat st;
			if (fstat(fd, &st) == 0 && st.st_ino == side.st_ino && st.st_dev == side.st_dev)
			{
				g_swapped = dup2(ro, fd) >= 0;
				break;
			}
		}
		closedir(d);
	}
	close(ro);
}

// The name looked at and found a file, and then a link put in its place before
// the open.
std::string g_link_to;

void swapForLink()
{
	unlink(g_name.c_str());
	(void) symlink(g_link_to.c_str(), g_name.c_str());
}

} // namespace

TEST_CASE("a write to a new name makes a whole file in the writer's mode", "[storage][writefile]")
{
	Scratch s;
	REQUIRE_FALSE(s.top.empty());

	// Every byte value, a zero among the first ones, so a body handed on as a
	// string that stops at its first zero would arrive as almost nothing.
	std::string bytes;
	for (int i = 0; i < 256; ++i)
		bytes += (char) (255 - i);
	bytes += '\0';

	const std::string name = s.at("new.bin");
	const Result<storage::Wrote> r = storage::writeFile(name, bytes, false);
	REQUIRE(r.ok());
	REQUIRE(r.value() == storage::Wrote::Created);
	REQUIRE(slurp(name) == bytes);
	REQUIRE_FALSE(present(CAtomicFileWriter::sideNameFor(name)));

	/* The bytes arrive in a file only its owner may read, so the bits for the
	   group and for everybody can only have come from the commit, which applies
	   the mode this layer hands the writer. Copied out first, which keeps the
	   check from taking the constant's address. */
	const mode_t wanted = CAtomicFileWriter::DefaultMode;
	REQUIRE(permsOf(name) == wanted);

	// No bytes at all make a file of no bytes and not a refusal.
	const std::string empty = s.at("empty");
	const Result<storage::Wrote> e = storage::writeFile(empty, std::string(), false);
	REQUIRE(e.ok());
	REQUIRE(regular(empty));
	REQUIRE(slurp(empty).empty());
}

TEST_CASE("a name in use is written over only with leave to", "[storage][writefile]")
{
	Scratch s;
	REQUIRE_FALSE(s.top.empty());

	// What the look found decides the answer, not the leave: given leave on a
	// name nobody uses, the file is still made.
	const Result<storage::Wrote> fresh = storage::writeFile(s.at("unused"), "x", true);
	REQUIRE(fresh.ok());
	REQUIRE(fresh.value() == storage::Wrote::Created);

	const std::string name = s.at("used");
	REQUIRE(make(name, "before"));

	expectRefusal(storage::writeFile(name, "after", false), Status::Conflict, ErrorCode::NameTaken);
	REQUIRE(slurp(name) == "before");
	REQUIRE_FALSE(present(CAtomicFileWriter::sideNameFor(name)));

	const Result<storage::Wrote> over = storage::writeFile(name, "after", true);
	REQUIRE(over.ok());
	REQUIRE(over.value() == storage::Wrote::Replaced);
	REQUIRE(slurp(name) == "after");
	REQUIRE_FALSE(present(CAtomicFileWriter::sideNameFor(name)));
}

TEST_CASE("a link at the name is refused whether or not it leads anywhere", "[storage][writefile]")
{
	Scratch s;
	REQUIRE_FALSE(s.top.empty());
	const std::string target = s.off("target");
	REQUIRE(make(target, "kept"));
	const std::string missing = s.off("missing");
	const std::string live = s.at("live-link");
	const std::string dead = s.at("dead-link");
	REQUIRE(symlink(target.c_str(), live.c_str()) == 0);
	REQUIRE(symlink(missing.c_str(), dead.c_str()) == 0);

	/* Looked at without following. Followed, the first would read as a file
	   that may be written over and the second as a name nobody uses, and both
	   writes would put a file of their own where the link stood. */
	const std::string links[] = { live, dead };
	for (size_t i = 0; i < 2; ++i)
	{
		INFO(links[i]);
		expectRefusal(storage::writeFile(links[i], "inside", true), Status::Conflict, ErrorCode::NotAPlainFile);
		// Without leave it is a conflict too. Which of the two conflicts is
		// not held here: the order of the two looks is not part of the call's
		// promise.
		const Result<storage::Wrote> plain = storage::writeFile(links[i], "inside", false);
		REQUIRE_FALSE(plain.ok());
		REQUIRE(plain.error().status == Status::Conflict);
		REQUIRE_FALSE(present(CAtomicFileWriter::sideNameFor(links[i])));
	}
	REQUIRE(pointsTo(live) == target);
	REQUIRE(pointsTo(dead) == missing);
	REQUIRE(slurp(target) == "kept");
	REQUIRE_FALSE(present(missing));
}

TEST_CASE("neither a directory nor a pipe is replaced by a file", "[storage][writefile]")
{
	Scratch s;
	REQUIRE_FALSE(s.top.empty());
	const std::string dir = s.at("a-directory");
	const std::string fifo = s.at("a-pipe");
	REQUIRE(mkdir(dir.c_str(), 0755) == 0);
	REQUIRE(mkfifo(fifo.c_str(), 0600) == 0);

	expectRefusal(storage::writeFile(dir, "x", true), Status::Conflict, ErrorCode::NotAPlainFile);
	expectRefusal(storage::writeFile(fifo, "x", true), Status::Conflict, ErrorCode::NotAPlainFile);

	struct stat st;
	REQUIRE(lstat(dir.c_str(), &st) == 0);
	REQUIRE(S_ISDIR(st.st_mode));
	REQUIRE(lstat(fifo.c_str(), &st) == 0);
	REQUIRE(S_ISFIFO(st.st_mode));
	REQUIRE_FALSE(present(CAtomicFileWriter::sideNameFor(dir)));
	REQUIRE_FALSE(present(CAtomicFileWriter::sideNameFor(fifo)));
}

TEST_CASE("whatever stands at the side name stops the write and stays", "[storage][writefile]")
{
	Scratch s;
	REQUIRE_FALSE(s.top.empty());

	// A file there is what a write cut off by the end of the program leaves,
	// or a write by one of the box's own savers still running.
	const std::string busy = s.at("busy");
	REQUIRE(make(busy, "before"));
	REQUIRE(make(CAtomicFileWriter::sideNameFor(busy), "half"));
	expectRefusal(storage::writeFile(busy, "after", true), Status::Conflict, ErrorCode::SideFileInTheWay);
	REQUIRE(slurp(busy) == "before");
	REQUIRE(slurp(CAtomicFileWriter::sideNameFor(busy)) == "half");

	/* A link there, to a file outside the roots or to nothing, is looked at
	   without following as well: one leading nowhere is not a free side name,
	   and the writer's own refusal to follow it would answer a different fault. */
	const std::string target = s.off("target");
	REQUIRE(make(target, "kept"));
	const std::string missing = s.off("missing");
	const std::string a = s.at("a");
	const std::string b = s.at("b");
	REQUIRE(symlink(target.c_str(), CAtomicFileWriter::sideNameFor(a).c_str()) == 0);
	REQUIRE(symlink(missing.c_str(), CAtomicFileWriter::sideNameFor(b).c_str()) == 0);
	expectRefusal(storage::writeFile(a, "new", false), Status::Conflict, ErrorCode::SideFileInTheWay);
	expectRefusal(storage::writeFile(b, "new", false), Status::Conflict, ErrorCode::SideFileInTheWay);
	REQUIRE(slurp(target) == "kept");
	REQUIRE_FALSE(present(missing));
	REQUIRE_FALSE(present(a));
	REQUIRE_FALSE(present(b));
}

TEST_CASE("a name outside the roots or under a missing directory writes nothing", "[storage][writefile]")
{
	Scratch s;
	REQUIRE_FALSE(s.top.empty());
	const std::string exit = s.at("exit");
	REQUIRE(symlink(s.beside().c_str(), exit.c_str()) == 0);

	// Straight outside, out by dots, and out through a link standing for a
	// directory: what is resolved is the directory, not the spelling.
	expectRefusal(storage::writeFile(s.off("f"), "x", false), Status::InvalidArgument, ErrorCode::OutsideRoots);
	expectRefusal(storage::writeFile(s.allowed() + "/../beside/f", "x", false),
		      Status::InvalidArgument, ErrorCode::OutsideRoots);
	expectRefusal(storage::writeFile(exit + "/f", "x", false), Status::InvalidArgument, ErrorCode::OutsideRoots);
	REQUIRE(countIn(s.beside()) == 0);

	// Inside, but the directory that would hold it is not there; nothing is
	// made on the way.
	expectRefusal(storage::writeFile(s.at("no/such/f"), "x", false), Status::NotFound, ErrorCode::NoSuchParent);
	REQUIRE_FALSE(present(s.at("no")));
	// Outside and missing both: where it is decides before whether it is there.
	expectRefusal(storage::writeFile(s.off("no/such/f"), "x", false), Status::InvalidArgument,
		      ErrorCode::OutsideRoots);
}

TEST_CASE("a name that is no name is refused before anything is looked at", "[storage][writefile]")
{
	Scratch s;
	REQUIRE_FALSE(s.top.empty());

	expectRefusal(storage::writeFile("", "x", false), Status::InvalidArgument, ErrorCode::EmptyPath);
	expectRefusal(storage::writeFile("not/absolute", "x", false), Status::InvalidArgument, ErrorCode::RelativePath);
	expectRefusal(storage::writeFile(s.at("cut") + std::string(1, '\0') + "rest", "x", false),
		      Status::InvalidArgument, ErrorCode::PathHasZeroByte);
	expectRefusal(storage::writeFile("/" + std::string(PATH_MAX + 10, 'p'), "x", false),
		      Status::InvalidArgument, ErrorCode::PathTooLong);
	expectRefusal(storage::writeFile(s.allowed() + "/.", "x", false), Status::InvalidArgument, ErrorCode::BadName);
	expectRefusal(storage::writeFile(s.allowed() + "/..", "x", false), Status::InvalidArgument, ErrorCode::BadName);
	REQUIRE(countIn(s.allowed()) == 0);
}

TEST_CASE("while the bytes arrive the old file stays whole and the new ones sit beside it", "[storage][writefile]")
{
	Scratch s;
	REQUIRE_FALSE(s.top.empty());
	g_name = s.at("watched");
	REQUIRE(make(g_name, "old"));
	g_seen_at_name.clear();
	g_side_there = false;
	g_side_perms = 0;

	{
		WriteProbe probe(lookMidWrite);
		const Result<storage::Wrote> r = storage::writeFile(g_name, "new", true);
		REQUIRE(r.ok());
		REQUIRE(r.value() == storage::Wrote::Replaced);
	}

	// A write straight at the name would have shown it emptied here.
	REQUIRE(g_seen_at_name == "old");
	REQUIRE(g_side_there);
	REQUIRE(g_side_perms == (S_IRUSR | S_IWUSR));
	REQUIRE(slurp(g_name) == "new");
	REQUIRE_FALSE(present(CAtomicFileWriter::sideNameFor(g_name)));
}

TEST_CASE("the write lock is held while the bytes arrive and given back after", "[storage][writefile]")
{
	Scratch s;
	REQUIRE_FALSE(s.top.empty());
	g_try = -1;

	{
		WriteProbe probe(tryWhileWriting);
		REQUIRE(storage::writeFile(s.at("locked"), "x", false).ok());
	}
	// Taken by the write, so the other thread could not have it.
	REQUIRE(g_try.load() != -1);
	REQUIRE(g_try.load() != 0);
	REQUIRE(tryFromHere() == 0);

	// And given back on a way out that refuses, after the lock was taken.
	expectRefusal(storage::writeFile(s.at("locked"), "y", false), Status::Conflict, ErrorCode::NameTaken);
	REQUIRE(tryFromHere() == 0);
}

TEST_CASE("a second write of one name waits for the first and finds its file", "[storage][writefile]")
{
	Scratch s;
	REQUIRE_FALSE(s.top.empty());
	g_name = s.at("contended");
	g_second_started = false;
	g_second_done = false;
	g_second_early = false;

	Result<storage::Wrote> first = fail(Status::Internal, ErrorCode::ChangeRefused, "not run");
	{
		WriteProbe probe(startSecondWrite);
		first = storage::writeFile(g_name, "first", false);
		// Joined with the probe still in, so nothing the second write reaches
		// can find it gone half way; joined here and not in the probe, because
		// the second write waits for the lock the probe's thread holds.
		if (g_second_started)
			pthread_join(g_second, NULL);
	}

	REQUIRE(g_second_started.load());
	REQUIRE(first.ok());
	REQUIRE(first.value() == storage::Wrote::Created);
	// It did not get through while the first held the lock, and once it did,
	// it found the file the first one made.
	REQUIRE_FALSE(g_second_early);
	expectRefusal(g_second_result, Status::Conflict, ErrorCode::NameTaken);
	REQUIRE(slurp(g_name) == "first");
	REQUIRE_FALSE(present(CAtomicFileWriter::sideNameFor(g_name)));
}

TEST_CASE("a save whose directory cannot be synced says the new content may be in place", "[storage][writefile]")
{
	Scratch s;
	REQUIRE_FALSE(s.top.empty());
	const std::string dir = s.at("unreadable");
	REQUIRE(mkdir(dir.c_str(), 0755) == 0);
	const std::string name = dir + "/f";
	REQUIRE(make(name, "before"));

	/* Writable and passable, not readable: the side file is made and renamed,
	   and the directory cannot be opened to be synced afterwards. Root reads
	   every directory, so the runner leaves this case out of a run as root. */
	RightsBack back(dir);
	REQUIRE(chmod(dir.c_str(), 0300) == 0);

	const Result<storage::Wrote> r = storage::writeFile(name, "after", true);
	expectRefusal(r, Status::Internal, ErrorCode::ChangeRefused);
	REQUIRE(r.error().message == "saving the file " + name + " failed: it holds what it held, "
				     "or the new content with its directory not synced");
	REQUIRE(slurp(name) == "after");
	REQUIRE_FALSE(present(CAtomicFileWriter::sideNameFor(name)));
}

TEST_CASE("a name the system will not open for writing leaves nothing behind", "[storage][writefile]")
{
	Scratch s;
	REQUIRE_FALSE(s.top.empty());
	// Within the length a path may have and over the one a single name may.
	const std::string name = s.at(std::string(NAME_MAX + 50, 'n'));

	const Result<storage::Wrote> r = storage::writeFile(name, "x", false);
	expectRefusal(r, Status::Internal, ErrorCode::ChangeRefused);
	REQUIRE(r.error().message == "the file " + name + " could not be opened for writing");
	REQUIRE(countIn(s.allowed()) == 0);
}

TEST_CASE("bytes nobody takes leave the old file and no side file", "[storage][writefile]")
{
	Scratch s;
	REQUIRE_FALSE(s.top.empty());
	g_refuser = s.off("read-only");
	REQUIRE(make(g_refuser, ""));

	// More than the stream holds, so the bytes go out inside the write itself.
	g_name = s.at("big");
	REQUIRE(make(g_name, "before"));
	g_swapped = false;
	{
		WriteProbe probe(refuseWrites);
		const Result<storage::Wrote> r = storage::writeFile(g_name, std::string(65536, 'b'), true);
		REQUIRE(g_swapped);
		expectRefusal(r, Status::Internal, ErrorCode::ChangeRefused);
		REQUIRE(r.error().message == "the file " + g_name + " could not be written");
	}
	REQUIRE(slurp(g_name) == "before");
	REQUIRE_FALSE(present(CAtomicFileWriter::sideNameFor(g_name)));

	// Few enough to wait in the stream, so they go out in the commit.
	g_name = s.at("small");
	REQUIRE(make(g_name, "before"));
	g_swapped = false;
	{
		WriteProbe probe(refuseWrites);
		const Result<storage::Wrote> r = storage::writeFile(g_name, "abc", true);
		REQUIRE(g_swapped);
		expectRefusal(r, Status::Internal, ErrorCode::ChangeRefused);
		REQUIRE(r.error().message == "saving the file " + g_name + " failed: it holds what it held, "
					     "or the new content with its directory not synced");
	}
	REQUIRE(slurp(g_name) == "before");
	REQUIRE_FALSE(present(CAtomicFileWriter::sideNameFor(g_name)));
	REQUIRE(slurp(g_refuser).empty());
}

TEST_CASE("a link put at a name between its look and its open is not read through", "[storage][writefile]")
{
	/* The read beside the write, because its probe is the other moment
	   storage_internal.h names. The name is found a file, and a link is put in
	   its place before the open; the open does not follow it, so what comes back
	   is the refusal for a name with nothing to read, and the file outside the
	   roots is not handed out. */
	Scratch s;
	REQUIRE_FALSE(s.top.empty());
	g_name = s.at("read");
	REQUIRE(make(g_name, "inside"));
	g_link_to = s.off("secret");
	REQUIRE(make(g_link_to, "outside"));

	Result<int> r = fail(Status::Internal, ErrorCode::ChangeRefused, "not run");
	{
		OpenProbe probe(swapForLink);
		r = storage::openFile(g_name);
	}
	if (r.ok())
		close(r.value());
	REQUIRE_FALSE(r.ok());
	REQUIRE(r.error().status == Status::NotFound);
	REQUIRE(r.error().code == ErrorCode::NoSuchName);
	REQUIRE(pointsTo(g_name) == g_link_to);
	REQUIRE(slurp(g_link_to) == "outside");
}
