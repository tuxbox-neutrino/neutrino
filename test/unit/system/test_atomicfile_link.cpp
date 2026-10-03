/*
 * test_atomicfile_link.cpp - a file the user keeps as a link
 *
 * Copyright (C) 2026 Thilo Graf
 *
 * License: GPL-2.0-or-later
 */

#include "support/catch.hpp"

#include <system/helpers.h>

#include <cstdio>
#include <string>

#include <sys/stat.h>
#include <unistd.h>

namespace {

const char *kLinkDir = "/tmp/atomic-write-link";

std::string inDir(const char *name)
{
	mkdir(kLinkDir, 0755);
	return std::string(kLinkDir) + "/" + name;
}

std::string read(const std::string &path)
{
	FILE *f = fopen(path.c_str(), "r");
	if (f == NULL)
		return std::string();
	std::string out;
	int c;
	while ((c = fgetc(f)) != EOF)
		out += (char) c;
	fclose(f);
	return out;
}

} // namespace

TEST_CASE("a file kept as a link is written through it and the link stays", "[helpers][atomicwrite]")
{
	/* A user keeps the bouquet file as a link to storage of their own. The
	   new content belongs in the file the link names; replacing the link
	   with a file of its own would leave that storage stale and the link
	   gone, where a plain fopen("w") had always followed it. */
	const std::string real = inDir("shared.xml");
	const std::string link = inDir("ubouquets.xml");
	unlink(link.c_str());
	{
		FILE *f = fopen(real.c_str(), "w");
		REQUIRE(f != NULL);
		fputs("old", f);
		fclose(f);
	}
	REQUIRE(symlink(real.c_str(), link.c_str()) == 0);

	{
		CAtomicFileWriter out(link);
		REQUIRE(out.file() != NULL);
		fputs("new", out.file());
		REQUIRE(out.commit());
	}

	struct stat st;
	REQUIRE(lstat(link.c_str(), &st) == 0);
	CHECK(S_ISLNK(st.st_mode));
	CHECK(read(real) == "new");
	CHECK(read(link) == "new");

	unlink(link.c_str());
	unlink(real.c_str());
}

TEST_CASE("a writer told to replace links puts a file where the link was and leaves what it named as it was", "[helpers][atomicwrite]")
{
	/* A caller that has checked the name itself, and found a plain file,
	   must not be led elsewhere by a link put there after its check: the
	   file manager writes inside a root, and a link swapped in between its
	   check and the write would otherwise carry the bytes out of it. Told
	   so, the writer puts a file of its own where the link was and leaves
	   what the link named as it was. */
	const std::string elsewhere = inDir("outside.txt");
	const std::string name = inDir("inside.txt");
	unlink(name.c_str());
	{
		FILE *f = fopen(elsewhere.c_str(), "w");
		REQUIRE(f != NULL);
		fputs("old", f);
		fclose(f);
	}
	REQUIRE(symlink(elsewhere.c_str(), name.c_str()) == 0);

	{
		CAtomicFileWriter out(name, CAtomicFileWriter::DefaultMode, CAtomicFileWriter::ReplaceLink);
		REQUIRE(out.file() != NULL);
		fputs("new", out.file());
		REQUIRE(out.commit());
	}

	struct stat st;
	REQUIRE(lstat(name.c_str(), &st) == 0);
	CHECK(S_ISREG(st.st_mode));
	CHECK(read(name) == "new");
	CHECK(read(elsewhere) == "old");

	unlink(name.c_str());
	unlink(elsewhere.c_str());
}

TEST_CASE("a chain of relative links is followed to the file at its end", "[helpers][atomicwrite]")
{
	/* Each link is read against the directory it sits in, and a link may
	   name another one. What gets the bytes is the file the last link names,
	   and both links stay as they were. */
	const std::string real = inDir("chain-real.xml");
	const std::string middle = inDir("chain-middle.xml");
	const std::string link = inDir("chain-link.xml");
	unlink(middle.c_str());
	unlink(link.c_str());
	{
		FILE *f = fopen(real.c_str(), "w");
		REQUIRE(f != NULL);
		fputs("old", f);
		fclose(f);
	}
	REQUIRE(symlink("chain-real.xml", middle.c_str()) == 0);
	REQUIRE(symlink("chain-middle.xml", link.c_str()) == 0);

	{
		CAtomicFileWriter out(link);
		REQUIRE(out.file() != NULL);
		fputs("new", out.file());
		REQUIRE(out.commit());
	}

	struct stat st;
	REQUIRE(lstat(link.c_str(), &st) == 0);
	CHECK(S_ISLNK(st.st_mode));
	REQUIRE(lstat(middle.c_str(), &st) == 0);
	CHECK(S_ISLNK(st.st_mode));
	CHECK(read(real) == "new");

	unlink(link.c_str());
	unlink(middle.c_str());
	unlink(real.c_str());
}

TEST_CASE("a link to a file not there yet makes that file", "[helpers][atomicwrite]")
{
	// The file the link names is what the user wants written, whether or not
	// it is there already; the link stays and names it afterwards.
	const std::string absent = inDir("dangling-target.xml");
	const std::string link = inDir("dangling-link.xml");
	unlink(absent.c_str());
	unlink(link.c_str());
	REQUIRE(symlink("dangling-target.xml", link.c_str()) == 0);

	{
		CAtomicFileWriter out(link);
		REQUIRE(out.file() != NULL);
		fputs("new", out.file());
		REQUIRE(out.commit());
	}

	struct stat st;
	REQUIRE(lstat(link.c_str(), &st) == 0);
	CHECK(S_ISLNK(st.st_mode));
	CHECK(read(absent) == "new");

	unlink(link.c_str());
	unlink(absent.c_str());
}

TEST_CASE("a link that ends in no plain file or goes round is refused", "[helpers][atomicwrite]")
{
	/* A device, a FIFO, a socket or a directory is no file to put in place,
	   and a link that leads back to itself never ends. Nothing is written
	   and the links stay as they were. */
	const std::string device = inDir("to-device.xml");
	const std::string loop_a = inDir("loop-a.xml");
	const std::string loop_b = inDir("loop-b.xml");
	unlink(device.c_str());
	unlink(loop_a.c_str());
	unlink(loop_b.c_str());
	REQUIRE(symlink("/dev/null", device.c_str()) == 0);
	REQUIRE(symlink("loop-b.xml", loop_a.c_str()) == 0);
	REQUIRE(symlink("loop-a.xml", loop_b.c_str()) == 0);

	const char *names[] = { "to-device.xml", "loop-a.xml" };
	for (size_t i = 0; i < sizeof(names) / sizeof(names[0]); i++)
	{
		INFO("written through " << names[i]);
		CAtomicFileWriter out(inDir(names[i]));
		CHECK(out.file() == NULL);
		CHECK_FALSE(out.commit());
	}

	struct stat st;
	REQUIRE(lstat(device.c_str(), &st) == 0);
	CHECK(S_ISLNK(st.st_mode));
	REQUIRE(lstat(loop_a.c_str(), &st) == 0);
	CHECK(S_ISLNK(st.st_mode));

	unlink(device.c_str());
	unlink(loop_a.c_str());
	unlink(loop_b.c_str());
}
