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
