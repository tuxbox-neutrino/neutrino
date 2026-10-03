/*
 * test_atomicfile.cpp - tests for writing a file atomically
 *
 * Copyright (C) 2026 NI-Team
 * Copyright (C) 2026 Tuxbox-Project (tests)
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

#include <cstdio>
#include <string>

#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <system/helpers.h>

/* The bouquet files are replaced rather than written over, and these drive the object
   that does the replacing rather than a copy of what it does.

   What it is for is the write that fails. Writing over the file in place is only whole
   once the last byte is in it, and a write that stopped before that used to leave a
   truncated file under the name the next start reads. None of that shows up in a case
   that only checks a successful save, so each case below makes a failure and reads the
   file afterwards.

   The bouquet manager that calls this cannot be linked into this binary: that it goes
   through this object at all is held to it as text by check-bouquet-save-atomic.sh. */

namespace
{

const char *kDir = "/tmp/ni-atomic-write";

std::string pathIn(const char *name)
{
	mkdir(kDir, 0755);
	return std::string(kDir) + "/" + name;
}

void put(const std::string &path, const char *what)
{
	FILE *f = fopen(path.c_str(), "w");
	REQUIRE(f != NULL);
	fputs(what, f);
	REQUIRE(fclose(f) == 0);
}

// Empty for a file that is not there, which is what the cases below ask about
// as often as they ask what is in one.
std::string get(const std::string &path)
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

bool exists(const std::string &path)
{
	return access(path.c_str(), F_OK) == 0;
}

} // namespace

TEST_CASE("a replaced file holds what was written and leaves nothing beside it", "[helpers][atomicwrite]")
{
	const std::string target = pathIn("replaced.xml");
	const std::string sidecar = target + ".new";
	put(target, "old");

	{
		CAtomicFileWriter out(target);
		REQUIRE(out.file() != NULL);
		fputs("new", out.file());

		// Nothing of the new content is under the name a reader reads while
		// the writing is going on. That is the whole of what a side file buys.
		REQUIRE(get(target) == "old");

		REQUIRE(out.commit());
	}

	REQUIRE(get(target) == "new");
	REQUIRE_FALSE(exists(sidecar));

	unlink(target.c_str());
}

TEST_CASE("a write that fails partway leaves the file that was there", "[helpers][atomicwrite]")
{
	const std::string target = pathIn("failed.xml");
	const std::string sidecar = target + ".new";
	put(target, "<zapit>whole</zapit>");

	bool committed = true;
	{
		CAtomicFileWriter out(target);
		REQUIRE(out.file() != NULL);
		fputs("<zapit>half", out.file());

		/* A real failure and not a stub of one: what the stream writes through
		   is taken out from under it, so the half still in its buffer can
		   never reach the disk and the flush and the close are what say so. A
		   full disk fails in exactly that place and cannot be arranged here. */
		REQUIRE(close(fileno(out.file())) == 0);

		committed = out.commit();
	}

	REQUIRE_FALSE(committed);
	REQUIRE(get(target) == "<zapit>whole</zapit>");
	REQUIRE_FALSE(exists(sidecar));

	unlink(target.c_str());
}

TEST_CASE("a file in a place that is not there is refused and writes nothing", "[helpers][atomicwrite]")
{
	const std::string target = "/tmp/ni-atomic-write-nowhere/bouquets.xml";

	CAtomicFileWriter out(target);
	REQUIRE(out.file() == NULL);
	REQUIRE_FALSE(out.commit());
	REQUIRE_FALSE(exists(target));
}

TEST_CASE("a writer dropped without a commit leaves the file alone and takes the side file away", "[helpers][atomicwrite]")
{
	const std::string target = pathIn("dropped.xml");
	const std::string sidecar = target + ".new";
	put(target, "old");

	{
		CAtomicFileWriter out(target);
		REQUIRE(out.file() != NULL);
		fputs("new", out.file());
	}

	REQUIRE(get(target) == "old");
	REQUIRE_FALSE(exists(sidecar));

	unlink(target.c_str());
}

TEST_CASE("a replaced file carries the mode it was asked for", "[helpers][atomicwrite]")
{
	const std::string target = pathIn("mode.xml");
	unlink(target.c_str());

	/* Not owner only, which is what the side file is made with and set back
	   to when it is opened again: a group bit can only have come from the
	   commit's fchmod, which the process mask does not narrow. While the
	   bytes arrive the side file stays owner only. */
	const mode_t wanted = S_IRUSR | S_IWUSR | S_IRGRP;
	{
		CAtomicFileWriter out(target, wanted);
		REQUIRE(out.file() != NULL);
		struct stat side;
		REQUIRE(fstat(fileno(out.file()), &side) == 0);
		REQUIRE((side.st_mode & 07777) == (mode_t)(S_IRUSR | S_IWUSR));
		fputs("x", out.file());
		REQUIRE(out.commit());
	}

	struct stat st;
	REQUIRE(stat(target.c_str(), &st) == 0);
	REQUIRE((st.st_mode & 07777) == wanted);

	unlink(target.c_str());
}

TEST_CASE("a side file left behind is opened again empty and owner only", "[helpers][atomicwrite]")
{
	/* What a write cut off by the end of the process leaves: the side file,
	   carrying the file's own mode already if the cut came between the
	   commit's fchmod and its rename. The next write opens it again, and it
	   has to start empty and owner only: otherwise the bytes arriving would be
	   readable by everybody, and a shorter write would keep the old tail. */
	const std::string target = pathIn("leftover.xml");
	const std::string sidecar = target + ".new";
	unlink(target.c_str());
	put(sidecar, "stale and longer");
	REQUIRE(chmod(sidecar.c_str(), 0644) == 0);

	{
		CAtomicFileWriter out(target);
		REQUIRE(out.file() != NULL);
		struct stat side;
		REQUIRE(fstat(fileno(out.file()), &side) == 0);
		REQUIRE((side.st_mode & 07777) == (mode_t)(S_IRUSR | S_IWUSR));
		REQUIRE(side.st_size == 0);
		fputs("new", out.file());
		REQUIRE(out.commit());
	}

	const mode_t dflt = CAtomicFileWriter::DefaultMode;
	struct stat st;
	REQUIRE(stat(target.c_str(), &st) == 0);
	REQUIRE((st.st_mode & 07777) == dflt);
	REQUIRE(get(target) == "new");
	REQUIRE_FALSE(exists(sidecar));

	unlink(target.c_str());
}

TEST_CASE("a directory that cannot be synced makes the commit answer no with the file in place", "[helpers][atomicwrite]")
{
	/* Write and search but no read: the side file can be made there and put
	   in the file's place, but the directory cannot be opened to sync it. The
	   answer is no, because the new content may not survive a power cut, and
	   nothing is undone, because it is in place. Root opens any directory,
	   which is why the runner leaves this out of a privileged run. */
	mkdir(kDir, 0755);
	const std::string dir = std::string(kDir) + "/nosync";
	chmod(dir.c_str(), 0755);
	mkdir(dir.c_str(), 0755);
	const std::string target = dir + "/f.xml";
	const std::string sidecar = target + ".new";
	put(target, "old");
	/* Back to a mode the next run can work in however the case ends: a
	   failed assertion below leaves the scope at once. */
	struct ModeBack
	{
		std::string p;
		explicit ModeBack(const std::string &d) : p(d) {}
		~ModeBack() { chmod(p.c_str(), 0755); }
	} back(dir);
	REQUIRE(chmod(dir.c_str(), 0300) == 0);

	bool committed;
	{
		CAtomicFileWriter out(target);
		REQUIRE(out.file() != NULL);
		fputs("new", out.file());
		committed = out.commit();
	}
	REQUIRE(chmod(dir.c_str(), 0755) == 0);

	REQUIRE_FALSE(committed);
	REQUIRE(get(target) == "new");
	REQUIRE_FALSE(exists(sidecar));

	unlink(target.c_str());
	rmdir(dir.c_str());
}

TEST_CASE("the name the bytes go to is the one the writer opens", "[helpers][atomicwrite]")
{
	// Asked of the object rather than spelled a second time, because a caller
	// that has to know whether one write is already running would otherwise be
	// holding a name that can drift from the one actually opened.
	REQUIRE(CAtomicFileWriter::sideNameFor("/tmp/x/bouquets.xml") == "/tmp/x/bouquets.xml.new");
}

TEST_CASE("a link planted where the bytes go is not written through", "[helpers][atomicwrite]")
{
	/* The side name sits in the same directory as the file and is worked out rather
	   than given, so whoever can write into that directory can put a link there
	   pointing anywhere and wait. Followed, this would write through it: bytes meant
	   for one directory landing in another. What refuses it is a flag on the open that
	   no mode string can say, which is why the open is not an fopen. */
	const std::string target = pathIn("planted.xml");
	const std::string sidecar = target + ".new";
	const std::string elsewhere = "/tmp/ni-atomic-write-elsewhere";

	unlink(target.c_str());
	unlink(sidecar.c_str());
	put(elsewhere, "not yours");
	REQUIRE(symlink(elsewhere.c_str(), sidecar.c_str()) == 0);

	{
		CAtomicFileWriter out(target);
		REQUIRE(out.file() == NULL);
		REQUIRE_FALSE(out.commit());
	}

	REQUIRE(get(elsewhere) == "not yours");
	REQUIRE_FALSE(exists(target));

	unlink(elsewhere.c_str());
	unlink(sidecar.c_str());
}
