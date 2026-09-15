/*
 * storage.h - mounts, directories and files on the box
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

#ifndef __coreapi_storage_h__
#define __coreapi_storage_h__

#include "coreapi/base/result.h"
#include "coreapi/base/types.h"

#include <string>
#include <vector>

namespace coreapi
{
namespace storage
{

// The directories everything below has to stay inside, colon separated. An element that
// names nothing admits nothing rather than failing the whole list, so a box with no
// removable media still answers for the roots it does have.
//
// A root list is not a prefix list: a name is inside a root when it is the root or
// continues past a separator, so /media does not admit /mediaXYZ. An element that resolves
// to / admits nothing either.
//
// Two files inside the roots are reached by nothing here: the two a web server on this box
// keeps its own access store in, the password it checks a caller against and the policy
// that says who is granted what without one. Reading one hands over a hash to be attacked
// at leisure and writing one hands over the box for good, so neither is read, written, made
// or removed through any call below, whoever wrote the roots. Two files and not the
// directory they sit in. Listing the directory still shows them; what is refused is
// reaching them. Not a list of names to refuse but a comparison of canonical absolute paths.
//
// What the confinement is worth: it answers for a name at the moment it is checked, not for
// what a later open finds under it. Anything in that name can be put back differently in
// between, the root itself as readily as something under it, because replacing a directory
// needs a write to its parent and not to the directory. So it holds only where every
// directory from the filesystem root down to and including the roots, and the contents of
// the roots, are beyond the reach of whoever can reach this.
void setRoots(const std::string &colon_separated);
std::string roots();

// The media a box records to and the directories a plugin is installed into. A consumer
// that knows where this box was told to record adds that to this rather than replacing it,
// so the media stay reachable whatever the setting says.
std::string defaultRoots();

/* Puts the roots together out of the list above, the directory the box was told to
   record into, and the directories the movie browser was given. A name that is not
   absolute is left out.

   Called at the start of every request that reaches a name, not once at startup, so a
   directory entered in a menu works without a restart. The recording directory is read
   through the settings declaration, which copies the member under the lock the box's own
   loop takes to write it. */
void refreshRoots();

/* Whether a name resolves to something inside one of the directories the caller states,
   decided the way every call below decides containment. For a credential good for part of
   a box: a second reader of what "inside" means would be a second answer to the question
   the roots exist to settle.

   False for a name that does not resolve, so "not there" and "not admitted" answer alike
   and this cannot say which names exist outside the list. */
bool inside(const std::string &path, const std::string &directories);

// Containment is decided before existence, so a name that is not there answers NotFound
// when it is inside a root and InvalidArgument when it is not. Folding the two would let a
// caller ask what exists elsewhere on the box by reading which refusal came back.

// In the order the directory gave them, with . and .. left out. Each name is looked at
// exactly once, against the open directory rather than against a path built out of its
// name. A name it could not answer for keeps its kind, says so through attributes_read and
// stays in the listing.
//
// InvalidArgument for a name outside the roots, NotFound for one inside them that is not
// there, is not a directory, or cannot be opened. Those three share an answer so that what
// a name is stays as unreadable from outside as whether it is there at all.
Result<std::vector<FileEntry> > list(const std::string &path);

// Makes one directory whose parent already exists. NotFound when the parent
// does not, Conflict when the name is taken.
Result<void> createDirectory(const std::string &path);

// Removes one regular file, or one directory with nothing in it. A directory
// that still holds something answers Conflict rather than being emptied, and a
// link is removed itself rather than what it points at.
Result<void> removePath(const std::string &path);

/* Opens one plain file inside the roots for reading and hands the descriptor over. The
   caller closes it. The descriptor and not the bytes, because some of these files run to
   gigabytes and the kernel can put them where they are going without this layer ever seeing
   one.

   The name is resolved before it is opened and what is opened is what the resolution
   answered, so a link inside the roots that leads out of them is refused however many hops
   it takes. What is opened is held to being a plain file: a pipe above all, because opening
   one waits for a writer that may never come and this runs on a request thread.

   InvalidArgument for a name outside the roots. NotFound for one inside them that is not
   there, is not a plain file, or cannot be opened; those three share an answer so that what
   a name is stays as unreadable from outside as whether it is there at all. */
Result<int> openFile(const std::string &path);

/* Writes bytes to one name inside the roots.

   A half finished write leaves nothing. The bytes go to a name of their own beside the file
   and are put in its place in one step at the end, so a reader sees either all of what was
   there before or all of what arrived, and a write that fails partway takes the side file
   away again.

   The side name is the file's own with a word after it, so a second write of the same file
   while one is running, and the remains of one that was cut off, are both refused rather
   than written through. That refusal is also what keeps the open from following something
   planted at the side name: this opens a name it worked out rather than one a caller wrote,
   and only when nothing is there.

   Without overwrite a taken name is a conflict and nothing is written. What may be replaced
   this way is a plain file and nothing else.

   InvalidArgument for a name outside the roots and for one that does not end in something
   this can write, NotFound for one whose directory is not there, Conflict for a name that
   is taken and for one whose side name is, Internal for a write that was not taken. */
enum class Wrote
{
	Created,
	Replaced
};

// Answered rather than left for the caller to work out: what a name held is a
// thing only the write knows, the one look that can answer it being the one
// this takes before it writes.
Result<Wrote> writeFile(const std::string &path, const std::string &bytes, bool overwrite);

// One whose counters cannot be read carries zeroes rather than being left out,
// so the list stays the list of what is mounted.
Result<std::vector<MountInfo> > mounts();

// Confined like everything else here: a name outside the roots is refused before the kernel
// is asked, so that what exists elsewhere on the box cannot be read off which refusal comes
// back. NotFound for a name inside a root that is not there or the kernel will not answer
// for.
Result<FsUsage> stats(const std::string &path);

} // namespace storage
} // namespace coreapi

#endif
