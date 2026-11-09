/*
 * accessfiles.h - the files that hold who may reach the box, by what they are
 *
 * Copyright (C) 2026 Thilo Graf
 *
 * License: GPL-2.0-or-later
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
 * The access store, nhttpd.conf, the name the store had before, their
 * working files, and a copy a restore makes of nhttpd.conf's account lines,
 * each kept by device and inode from the moment it is made: every writer
 * of such a file says so right after creating it and before it writes a
 * byte, so a descriptor opened on it at any point of the write, or under
 * any name or second name it gets afterwards, is one of them. A name is not
 * needed for that, and neither is what the file holds.
 *
 * A duplicate of a descriptor on the file is held for as long as it is
 * kept, so its inode cannot pass to another file meanwhile and nothing else
 * is ever taken for it. It is let go once the file has no name left: what
 * is opened on it after that is refused by the rule that a file without a
 * name goes out through no gate (yprotect). So the set holds no more than
 * those of the files that still have a name, and the process ending clears
 * it.
 *
 * Here and not in the web server, because a restore in this layer writes
 * such a file as well, and the web server asks it.
 */

#ifndef __coreapi_accessfiles_h__
#define __coreapi_accessfiles_h__

#include <stddef.h>
#include <string>

#include <sys/types.h>

namespace coreapi
{
namespace accessfiles
{

/* Held by whoever makes a file no gate may let a descriptor of through
   before it is kept or has no name: from before the file exists until it
   is kept (created) or its name is gone. Every gate holds an Asking while it
   asks about a descriptor (yprotect), so a descriptor opened on such a file
   in between is asked about only once it is kept or has no name. */
class Making
{
	public:
		Making();
		~Making();

	private:
		Making(const Making &);
		Making &operator=(const Making &);
};

class Asking
{
	public:
		Asking();
		~Asking();

	private:
		Asking(const Asking &);
		Asking &operator=(const Asking &);
};

/* fd was just created for one of the files, and nothing is written yet.
   False where it could not be kept (no descriptor left to duplicate, no
   memory): the writer then writes nothing into it and removes it. */
bool created(int fd);

/* The file path stands for now, followed through links, kept. True where
   it is kept or no file is there; false where one is and could not be
   kept. */
bool named(const std::string &path);

// Whether the file of that device and inode is one of them.
bool held(dev_t dev, ino_t ino);

// Lets go of every file, for a server that has none of them.
void forgetAll();

// How many are held, for the cases.
size_t count();

/* Makes created fail where it duplicates the descriptor (FailDup), the way a
   full descriptor table does, or created and named fail where they make the
   entry (FailAlloc), the way memory that ran out does; for the cases. The
   first after of those calls go through as before. */
enum Fail
{
	FailNone,
	FailDup,
	FailAlloc
};
void setFailForTest(Fail f, unsigned after = 0);

// Whether nothing holds the set's own lock right now, for the cases.
bool unlockedForTest();

/* Called by every Asking before it waits for a Making, so a case knows a gate
   has come as far as its question; for the cases. */
void setAskingForTest(void (*asking)());

} // namespace accessfiles
} // namespace coreapi

#endif
