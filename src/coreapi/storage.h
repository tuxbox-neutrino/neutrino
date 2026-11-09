/*
 * storage.h - mounts, directories and files on the box
 *
 * Copyright (C) 2026 NI-Team
 * Copyright (C) 2026 Tuxbox-Project (additions, fixes)
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

   The side name is the file's own with a word after it. Writes through this layer are kept
   apart by a lock, and a side file that is there at the time of the look - the remains of a
   write that was cut off, or a write of the box's own still running - is refused rather
   than written through. What keeps the open from following something planted at the side
   name is the writer's own O_NOFOLLOW.

   Without overwrite a taken name is a conflict and nothing is written. What may be replaced
   this way is a plain file and nothing else.

   InvalidArgument for a name outside the roots and for one that does not end in something
   this can write, NotFound for one whose directory is not there, Conflict for a name that
   is taken and for one whose side name is, Internal for a write that was not taken or one
   put in place whose directory could not be synced. */
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
// so the list stays the list of what is mounted. A network share or a FUSE
// filesystem is asked on a thread of its own, all of them at once, and waited
// for about a second (internal::kSizeWaitMs): one that did not answer by then,
// a share whose server went away, is in the list with answered false, and the
// same mount is not asked again while that question is still open; a request
// that comes while it is still young waits for its answer. A local filesystem
// is asked straight away unless a share lies at or above it on the way there,
// an automounter's own mount point not at all.
Result<std::vector<MountInfo> > mounts();

/* The roots as the list stands: every element once, in its order, and whether it names a
   directory that is there right now. For a caller that offers places to start, such as a
   file manager's list of them; nothing any call here decides about containment changes
   with what this answers, and a root that is not there yet still counts once it is. */
struct RootEntry
{
	std::string path;
	bool        present;
};
std::vector<RootEntry> rootList();

/* Gives one name inside the roots another name inside the roots, in one step and on one
   filesystem. What moves is the name itself: a link is renamed as a link, not what it leads
   to, as removePath removes one.

   The directory that is to hold the new name has to be there already. Without overwrite a
   taken name is a conflict, decided in the same step as the rename where the kernel and the
   filesystem can do that and by a look just before it where they cannot. With overwrite only
   a plain file replaces a plain file, in one step, so a reader sees the old file or the new
   one and never neither.

   InvalidArgument for either name outside the roots or naming a web server's access store,
   for a root itself, or a directory that holds one, as the name to rename (IsARoot), for a
   directory to go into something it holds (IntoItself), for names on two filesystems (CrossDevice),
   since a rename does not copy. NotFound when there is nothing under the first name or the
   directory for the second is missing. Conflict NameTaken when the second name is taken and
   overwrite was not given, and NotAPlainFile when it is taken and either of the two is not
   a plain file. */
Result<void> renamePath(const std::string &from, const std::string &to, bool overwrite);

/* Adds bytes to the end of one plain file inside the roots that is exactly offset bytes long.
   For a file larger than one request may carry: sent in pieces to a name of its own and
   given its real name with renamePath at the end, so what was there under that name stays
   whole until the last piece is in.

   The length has to match, so a piece that came twice or one that went missing is refused
   rather than written where it does not belong (Conflict OffsetMismatch, the file left as it
   was). NotFound when there is no file under the name, Conflict NotAPlainFile when what is
   there is not a plain file, InvalidArgument for a name outside the roots or naming an
   access store. A piece the system takes only part of is taken back off again. */
Result<void> appendFile(const std::string &path, uint64_t offset, const std::string &bytes);

// Confined like everything else here: a name outside the roots is refused before the kernel
// is asked, so that what exists elsewhere on the box cannot be read off which refusal comes
// back. NotFound for a name inside a root that is not there or the kernel will not answer
// for.
Result<FsUsage> stats(const std::string &path);

/* What a restore of the settings over the network (backup::restore) does with a file of
   the archive it would put into the directory a web server's access store is in, so that
   no archive replaces the login: the files this layer refuses to hand out (the access
   store, the name it had before and the working files both stores are written through)
   stay as the box has them (Kept); nhttpd.conf, which also says how the server runs, is
   put back with the box's own lines for the account in place of the archive's
   (AccountKept); any other name, and any name in another directory, is put back
   (Restored). dir is an open descriptor of the directory the file lands in, compared with
   the store's by what the two are and not by how either is spelt. */
enum class InStore
{
	Restored,
	Kept,
	AccountKept
};
InStore restoreOf(int dir, const std::string &leaf);

/* What a restore writes a file of the archive under before it takes its place. In the
   directory of the access store such a file may be the copy of nhttpd.conf with the
   box's account lines, which a restart can leave behind: refused there as the store is. */
extern const char kRestoreWorkingPrefix[];

} // namespace storage
} // namespace coreapi

#endif
