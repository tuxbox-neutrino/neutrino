/*
 * netfs.h - the network filesystems this box mounts, as entries and not as file text
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

#ifndef __coreapi_netfs_h__
#define __coreapi_netfs_h__

#include "coreapi/base/result.h"

#include <cstddef>
#include <string>
#include <vector>

namespace coreapi
{
namespace netfs
{

/* The two files a box keeps its network filesystems in, which differ in when the
   box mounts what they name and in nothing else a caller here sees. Fstab is
   mounted while the box starts and stays mounted; Automount is read by the
   automounter, which mounts an entry the moment somebody looks at it and drops it
   again when nobody is.

   Both are entries with the same eight things in them, so both are read and
   written through the same calls with this saying which. */
enum class Table
{
	Fstab,
	Automount
};

// What is speaking at the other end. The two the box can mount and no others:
// the screen at the box offers these two, both files name the kind in a field
// of their own, and a third would be a kind nothing here can write a line for.
enum class Kind
{
	Nfs,
	Cifs
};

/* How many entries either file holds. Eight, which is the number of slots the
   screen at the box keeps (NETFS_NR_OF_ENTRIES in src/system/settings.h).

   A fixed count and not a list that grows, because that screen and this have to
   be able to write each other's files: a ninth entry written here would be read
   by that one, dropped on its next save, and gone without a word. */
const size_t kSlots = 8;

/* One mount, as the eight things a person is asked for and not as the line they
   end up on. An aggregate with a constructor, because a caller building one to
   write has to be handed something with nothing indeterminate in it. */
struct Entry
{
	// Whether the box mounts it. In the first file that is a word in the
	// options column; in the second it is whether the line is commented out.
	// One member, because what it means to a person is the same either way.
	bool        active;
	Kind        kind;
	// Where the server is and what it offers. The remote directory keeps its
	// leading separator, which is how both files write it.
	std::string host;
	std::string remote_dir;
	/* Where it lands. A path for the first file, which is where the box mounts
	   it; a name for the second, which the automounter offers under
	   /mnt/autofs. Nothing here holds it to either shape beyond what would stop
	   the file being readable, the two files having no way to say which of them
	   a name was meant for. */
	std::string local_dir;
	std::string username;

	/* The password, and never what a read hands back.

	   A READ ANSWERS NOTHING HERE, WHATEVER THE FILE HOLDS. Both files carry the
	   password in clear, because that is the only form the kernel takes one in,
	   so the value is in reach of this layer and must not leave it. What a read
	   says instead is has_password below.

	   Filled only on the way in, and then only when change_password says a new
	   one was typed: a form has nothing to put in the field, so it opens empty,
	   and an empty field left alone has to mean the stored value stays or every
	   save by somebody who came to change a directory would take the password
	   off the mount. */
	std::string password;
	bool        change_password;
	// What a read says about the password instead of the password. Not read on
	// the way in: what decides whether a write carries one is the switch above.
	bool        has_password;

	/* Everything else the options column holds, as it stands and comma
	   separated. What this layer owns is taken out of it: the word that says a
	   mount is not automatic, and the two that carry the credential. A write
	   that named one of them here would be writing a second value for something
	   already said.

	   A line whose options say nothing else reads as empty here, and is written
	   back as the one word a filesystem table has for no options. */
	std::string options;

	/* The two numbers at the end of a line in the first file, which say when a
	   filesystem is backed up and in what order it is checked. Neither means
	   anything for a filesystem on another machine, so neither is offered to
	   anybody; they are here so that a value somebody set at the box survives a
	   write from here. Empty for the second file, whose lines have no such
	   columns. */
	std::string dump;
	std::string pass;

	Entry();
};

/* Every slot of one file, and what the reading could make nothing of.

	items is always kSlots long: a file naming fewer is padded with empty
	entries, because what a caller addresses is a slot and not a line.

	unreadable_lines counts the lines that were neither a comment nor an entry
	this could read. ONE SUCH LINE MUST NOT COST THE OTHERS, which is why it is a
	count and not a refusal: a file somebody edited by hand with one line
	mistyped still holds seven entries that are right. What the count is for is
	saying so, since a rewrite keeps the entries and not the lines nothing could
	be made of. */
struct Slots
{
	std::vector<Entry> items;
	size_t             unreadable_lines;

	Slots() : unreadable_lines(0) {}
};

/* Where each file is, and where the scripts that mount and unmount live.

   Settable so that a case can put all three somewhere it owns. What each starts
   as is what the image installs, stated in netfs.cpp beside the reasoning for
   the first of them: this box has two files called fstab and only one of them is
   this one. */
void setPath(Table t, const std::string &path);
std::string pathOf(Table t);

void setInitRoot(const std::string &dir);
std::string initRoot();

/* Every slot of one file, with the passwords withheld.

   A file that is not there reads as kSlots empty slots, that being what a box
   nobody has configured has, and the write below makes one. Anything else that
   stops the file being read is a refusal.

   Withheld here and not at whoever answers a request, for the reason the
   settings do it here: a caller refused the value by a read must not be handed
   it by something beside the read. */
Result<Slots> read(Table t);

/* Puts one entry in one slot and writes the whole file.

   The file is read first, so every slot but this one is written back as it
   stands, and so are this slot's password when change_password is false and its
   two trailing numbers whatever is asked. A write that could not read the file
   first is refused rather than made from nothing: it would empty every slot
   beside the one the caller named.

   Refuses a value that would not survive the file: a byte the columns are split
   on, a number sign, a control byte, or an option naming something this layer
   writes itself. What is written is written whole or not at all.

   A REWRITE KEEPS THE ENTRIES AND NOT THE LINES. The eight slots, the header the
   box's own screen writes, and nothing else: a line the reading passed over is
   gone afterwards. That is what the screen at the box has always done to these
   two files. */
Result<void> write(Table t, size_t slot, const Entry &e);

// Empties one slot and writes the file the same way. The slot stays a slot; an
// empty one is written as a line that names nothing and mounts nothing, which
// is what the screen at the box writes for a slot nobody filled in.
Result<void> clear(Table t, size_t slot);

/* The three acts on what the files name, each of them the command the screen at
   the box runs for the same button.

   mountAll and unmountAll drive the first file through the box's own start up
   script. Nothing here mounts anything itself, because what is mounted at start
   up and what a caller asks for afterwards are only the same thing if one script
   does both.

   reloadAutomounter drives the second: the automounter reads its file when it
   starts and not while it runs, so an entry written there is not a mount until
   it has been restarted.

   What comes back is whatever the script wrote, as it wrote it. */
Result<std::string> mountAll();
Result<std::string> unmountAll();
Result<std::string> reloadAutomounter();

} // namespace netfs
} // namespace coreapi

#endif
