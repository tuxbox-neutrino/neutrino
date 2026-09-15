/*
 * storage.cpp - mounts, directories and files on the box
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

#include <config.h>

#include "storage.h"
#include "coreapi/base/errors.h"
#include "coreapi/box/storage_internal.h"
#include "coreapi/settings/settings.h"

#include "system/helpers.h"

#include <configfile.h>

#include <cerrno>
#include <cstdio>
#include <cstring>

#include <dirent.h>
#include <fcntl.h>
#include <limits.h>
#include <stdint.h>
#include <sys/stat.h>
#include <sys/statfs.h>
#include <sys/types.h>
#include <unistd.h>

#include <OpenThreads/Mutex>
#include <OpenThreads/ScopedLock>

namespace coreapi
{
namespace storage
{

namespace internal
{
const char *mounts_path = "/proc/self/mounts";

// The media the box records to and the three directories a plugin can be installed into. /
// deliberately is not among them, so a listing cannot start at the root of the filesystem
// and work down, and insideRoots below throws out an element that resolves to /.
const char kShippedRoots[] =
	"/media:/mnt:/var:/tmp:/hdd:"
	PLUGINDIR ":" PLUGINDIR_VAR ":" PLUGINDIR_MNT ":"
	LUAPLUGINDIR ":" LUAPLUGINDIR_VAR;

const char *default_roots = kShippedRoots;

// The movie browser keeps its directories in a file of its own rather than in
// the settings, so they are read where it writes them.
const char *moviebrowser_config_path = CONFIGDIR "/moviebrowser.conf";

// Every kind a directory entry can carry, so that a reader can print back the
// number it was given rather than the one kind this layer had a name for.
FileKind kindOf(unsigned char d_type)
{
	switch (d_type)
	{
		case DT_FIFO: return FileKind::Fifo;
		case DT_CHR:  return FileKind::CharDevice;
		case DT_DIR:  return FileKind::Dir;
		case DT_BLK:  return FileKind::BlockDevice;
		case DT_REG:  return FileKind::Regular;
		case DT_LNK:  return FileKind::Link;
		case DT_SOCK: return FileKind::Socket;
		case DT_WHT:  return FileKind::Whiteout;
		default:      return FileKind::Unknown;
	}
}

// Read on the thread that is answering, written by a case before it asks and
// cleared after. Nothing in the product writes either.
Probe open_probe = NULL;
Probe write_probe = NULL;

void setOpenProbeForTest(Probe p) { open_probe = p; }
void setWriteProbeForTest(Probe p) { write_probe = p; }
}

namespace
{

OpenThreads::Mutex &rootsMutex()
{
	static OpenThreads::Mutex m;
	return m;
}

std::string &rootsStore()
{
	static std::string s(internal::kShippedRoots);
	return s;
}

// Up to this many directories in the movie browser's own file, which is what
// the screen behind it offers.
const int kMovieBrowserDirs = 8;

// One element of the list, left out where it names nothing this layer could
// resolve later. Not checked for being there: a medium that is not mounted yet
// is resolved again at every check, which is where a root is allowed to appear.
void addRoot(std::string &all, const std::string &dir)
{
	if (dir.empty() || dir[0] != '/')
		return;
	all += ":";
	all += dir;
}

bool hasZeroByte(const std::string &v)
{
	return v.find('\0') != std::string::npos;
}

// A root list is not a prefix list: the character behind the root has to be the
// separator, or /media would admit /mediaXYZ. The separator itself is never a
// root here.
bool insideOne(const std::string &resolved, const std::string &root)
{
	if (resolved == root)
		return true;
	if (resolved.size() <= root.size())
		return false;
	if (resolved.compare(0, root.size(), root) != 0)
		return false;
	return resolved[root.size()] == '/';
}

// Each directory is resolved here rather than when it was set, because one that is a link,
// or that was not mounted yet, answers differently later. One that resolves to / is thrown
// out: it would admit every name there is.
bool insideAny(const std::string &resolved, const std::string &all)
{
	size_t i = 0;
	while (i <= all.size())
	{
		size_t e = all.find(':', i);
		if (e == std::string::npos)
			e = all.size();
		const std::string one = all.substr(i, e - i);
		i = e + 1;
		if (one.empty())
			continue;
		char buf[PATH_MAX];
		if (realpath(one.c_str(), buf) == NULL)
			continue;
		if (strcmp(buf, "/") == 0)
			continue;
		if (insideOne(resolved, std::string(buf)))
			return true;
	}
	return false;
}

bool insideRoots(const std::string &resolved)
{
	return insideAny(resolved, roots());
}

Failure outside(const std::string &path)
{
	return fail(Status::InvalidArgument, ErrorCode::OutsideRoots,
		    "the name " + path + " is not inside any permitted directory");
}

/* The files this layer hands to nobody and lets nobody write, whatever the roots say and
   whatever credential the caller holds.

   Each is where a web server on this box keeps the password it checks a caller against and
   the policy that decides who is granted what without one. Reading one hands over a hash to
   be attacked at leisure. Writing one is worse: a caller that can replace it grants itself
   the box for good, and no later change of the password undoes that.

   Only these, and not the directory they sit in: everything else there is a configuration
   somebody has a good reason to fetch, to back up or to compare.

   Not a list of names to refuse, and that difference is the whole reason it is sound. A list
   over names a caller typed only ever catches the spellings somebody thought of. This
   compares a canonical absolute path, after realpath has run on both sides, against a path
   this build works out from its own macro.

   What it cannot see is a second hard link to the same file, a hard link being a name of its
   own with nothing in the filesystem to say what it shares. Making one needs a shell on the
   box. */
const char *const kAccessStoreNames[] = { "ni-web.conf", "nhttpd.conf" };

bool isAccessStore(const std::string &resolved)
{
	// Resolved rather than written down, for the reason the roots are: a
	// directory that is a link answers differently later.
	char dir[PATH_MAX];
	if (realpath(CONFIGDIR, dir) == NULL)
		return false;
	const std::string where(dir);

	for (size_t i = 0; i < sizeof(kAccessStoreNames) / sizeof(kAccessStoreNames[0]); i++)
	{
		const std::string named = where + "/" + kAccessStoreNames[i];
		// The name as this build writes it, which is what answers on a box
		// where the file has not been written yet: there is nothing to resolve,
		// and it still must not be made from here.
		if (resolved == named)
			return true;
		// And what it resolves to where it is there, so that a store which is
		// itself a link is the same file under either of its names.
		char buf[PATH_MAX];
		if (realpath(named.c_str(), buf) != NULL && resolved == buf)
			return true;
	}
	return false;
}

/* Not the refusal above, because the file is inside the roots and an answer saying it is
   not sends whoever reads it looking for a root that is missing. Not Denied, because that
   one invites a caller to come back with a better credential and there is none. */
Failure theAccessStore(const std::string &path)
{
	return fail(Status::InvalidArgument, ErrorCode::AccessStore,
		    "the name " + path + " is a web server's own store of who may reach this box, "
		    "which is read, written and removed through no file route");
}

// The three things a name has to be before any of it is looked at: something,
// absolute, and free of the byte every call below here would read as its end.
Result<void> checkName(const std::string &path)
{
	if (path.empty())
		return fail(Status::InvalidArgument, ErrorCode::EmptyPath, "no name was given");
	if (hasZeroByte(path))
		return fail(Status::InvalidArgument, ErrorCode::PathHasZeroByte,
			    "the name carries a zero byte, which every call below it would read as its end");
	if (path[0] != '/')
		return fail(Status::InvalidArgument, ErrorCode::RelativePath,
			    "the name " + path + " is not absolute, and this layer has no directory to read it against");
	if (path.size() >= PATH_MAX)
		return fail(Status::InvalidArgument, ErrorCode::PathTooLong, "the name is longer than this system allows");
	return ok();
}

// Walks up to the deepest part of the name that is there, resolves that and asks the roots
// about it, leaving whether the rest exists to the caller. Resolving the whole name in one go
// fails for anything missing, which would answer the same for a name missing inside a root and
// one missing outside every root.
//
// whole says whether anything had to be walked off. What was walked off is not carried back:
// the callers either want the whole name or split the last component off themselves.
Result<std::string> resolveInRoots(const std::string &path, bool &whole)
{
	whole = true;
	Result<void> named = checkName(path);
	if (!named.ok())
		return fail(named.error());

	std::string head = path;
	char buf[PATH_MAX];
	while (realpath(head.c_str(), buf) == NULL)
	{
		const size_t cut = head.find_last_of('/');
		// Unreachable while every name that gets here is absolute, and here so
		// that a caller that stops checking cannot turn this into a loop that
		// never ends.
		if (cut == std::string::npos)
			return fail(Status::Internal, ErrorCode::NoRootToResolve, "not even / could be resolved");
		whole = false;
		head = (cut == 0) ? "/" : head.substr(0, cut);
	}

	const std::string resolved(buf);
	if (!insideRoots(resolved))
		return outside(path);
	return ok(resolved);
}

Result<std::string> resolveExisting(const std::string &path)
{
	bool whole = false;
	Result<std::string> r = resolveInRoots(path, whole);
	if (!r.ok())
		return fail(r.error());
	if (!whole)
		return fail(Status::NotFound, ErrorCode::NoSuchName, "there is nothing called " + path);
	// Here and at the other resolver, rather than at each of the calls below:
	// every one of them reaches a name through one of the two, and a rule
	// written at one call is a rule the others do not have.
	if (isAccessStore(r.value()))
		return theAccessStore(path);
	return r;
}

// Resolves the directory that holds a name and leaves the name itself alone, so that a call
// about to make or remove something works on the name and not on what it points at: removing
// a resolved name would take what the link pointed at.
Result<std::string> resolveHolder(const std::string &path)
{
	Result<void> named = checkName(path);
	if (!named.ok())
		return fail(named.error());

	std::string trimmed = path;
	while (trimmed.size() > 1 && trimmed[trimmed.size() - 1] == '/')
		trimmed.erase(trimmed.size() - 1);

	const size_t cut = trimmed.find_last_of('/');
	const std::string leaf = trimmed.substr(cut + 1);
	if (leaf.empty() || leaf == "." || leaf == "..")
		return fail(Status::InvalidArgument, ErrorCode::BadName,
			    "the name " + path + " does not end in something this can act on");

	bool whole = false;
	Result<std::string> holder = resolveInRoots((cut == 0) ? "/" : trimmed.substr(0, cut), whole);
	if (!holder.ok())
		return fail(holder.error());
	if (!whole)
		return fail(Status::NotFound, ErrorCode::NoSuchParent,
			    "the directory that would hold " + path + " is not there");

	const std::string target = holder.value() + "/" + leaf;
	/* The name itself and not only the directory holding it, because what the
	   calls behind this one do is make, write and remove that name, and on a box
	   where the store has not been written yet there is nothing to resolve. */
	if (isAccessStore(target))
		return theAccessStore(path);

	return ok(target);
}

// A mount table writes a space, a tab, a newline and a backslash as a backslash
// and three octal digits, so a mount point carrying one arrives escaped.
std::string unescapeMountField(const std::string &field)
{
	std::string out;
	for (size_t i = 0; i < field.size(); i++)
	{
		if (field[i] == '\\' && i + 3 < field.size() &&
		    field[i + 1] >= '0' && field[i + 1] <= '3' &&
		    field[i + 2] >= '0' && field[i + 2] <= '7' &&
		    field[i + 3] >= '0' && field[i + 3] <= '7')
		{
			out += (char)(((field[i + 1] - '0') << 6) |
				      ((field[i + 2] - '0') << 3) |
				      (field[i + 3] - '0'));
			i += 3;
			continue;
		}
		out += field[i];
	}
	return out;
}

} // namespace

void setRoots(const std::string &colon_separated)
{
	OpenThreads::ScopedLock<OpenThreads::Mutex> lock(rootsMutex());
	rootsStore() = colon_separated;
}

std::string roots()
{
	OpenThreads::ScopedLock<OpenThreads::Mutex> lock(rootsMutex());
	return rootsStore();
}

std::string defaultRoots()
{
	return std::string(internal::default_roots);
}

void refreshRoots()
{
	std::string all = defaultRoots();

	/* Through the settings layer rather than off the member, so the one place that says
	   where this box records is the declaration the program already carries, and so the
	   copy is taken under the lock a screen writing that member holds. A store that
	   cannot answer leaves the list as it is: the media are still reachable, which is
	   better than a request refused because a setting could not be read. */
	Result<std::string> recording = settings::get("network_nfs_recordingdir");
	if (recording.ok())
		addRoot(all, recording.value());

	/* Asked for before it is opened, because the reader writes a line to the error
	   stream for a file that is not there and this runs on every request. A box that has
	   never opened the movie browser has no such file, and that is not a fault. */
	if (access(internal::moviebrowser_config_path, R_OK) == 0)
	{
		CConfigFile browser(',');
		browser.loadConfig(internal::moviebrowser_config_path);
		for (int i = 0; i < kMovieBrowserDirs; i++)
		{
			char key[32];
			snprintf(key, sizeof(key), "mb_dir_%d", i);
			addRoot(all, browser.getString(key, ""));
		}
	}

	setRoots(all);
}

bool inside(const std::string &path, const std::string &directories)
{
	/* The same three things checkName refuses, asked here because nothing below this
	   answers for them: a relative name resolves against whatever directory this
	   process happens to be in, a zero byte ends what the system call reads and not
	   what the string holds, and a name over the system's length is one realpath
	   cannot be given. */
	if (path.empty() || path[0] != '/')
		return false;
	if (hasZeroByte(path) || path.size() >= PATH_MAX)
		return false;

	char buf[PATH_MAX];
	if (realpath(path.c_str(), buf) == NULL)
		return false;
	return insideAny(std::string(buf), directories);
}

Result<std::vector<FileEntry> > list(const std::string &path)
{
	Result<std::string> resolved = resolveExisting(path);
	if (!resolved.ok())
		return fail(resolved.error());
	const std::string dir = resolved.value();

	DIR *d = opendir(dir.c_str());
	if (d == NULL)
		return fail(Status::NotFound, ErrorCode::NoSuchName, "there is nothing to list called " + path);

	// Against the open directory and not against a name built out of its path. The name a
	// directory hands back is the one it holds, so this reads the entry that was just listed
	// rather than whatever that path names by the time it is walked again.
	const int at = dirfd(d);

	std::vector<FileEntry> out;
	for (;;)
	{
		struct dirent *e = readdir(d);
		if (e == NULL)
			break;
		if (strcmp(e->d_name, ".") == 0 || strcmp(e->d_name, "..") == 0)
			continue;

		FileEntry entry;
		entry.name = e->d_name;
		entry.kind = internal::kindOf(e->d_type);

		// One stat and no more, so that everything a reader prints about a
		// name comes from one look at it and cannot disagree with itself.
		struct stat st;
		if (at >= 0 && fstatat(at, e->d_name, &st, 0) == 0)
		{
			entry.attributes_read = true;
			entry.mode = (uint32_t) st.st_mode;
			entry.nlink = (uint64_t) st.st_nlink;
			entry.size = (uint64_t) st.st_size;
			entry.mtime = st.st_mtime;
			entry.uid = st.st_uid;
			entry.gid = st.st_gid;
		}
		out.push_back(entry);
	}
	closedir(d);
	return ok(std::move(out));
}

Result<void> createDirectory(const std::string &path)
{
	// The directory that would hold it has to be there already, so one call
	// cannot bring a whole branch into being.
	Result<std::string> resolved = resolveHolder(path);
	if (!resolved.ok())
		return fail(resolved.error());

	if (mkdir(resolved.value().c_str(), 0755) != 0)
	{
		if (errno == EEXIST)
			return fail(Status::Conflict, ErrorCode::NameTaken, "there is already something called " + path);
		return fail(Status::Internal, ErrorCode::ChangeRefused, "the directory " + path + " could not be made");
	}
	return ok();
}

Result<void> removePath(const std::string &path)
{
	Result<std::string> resolved = resolveHolder(path);
	if (!resolved.ok())
		return fail(resolved.error());

	const std::string target = resolved.value();
	// lstat and not stat, so that a link is answered for as a link and removed
	// itself rather than what it points at.
	struct stat st;
	if (lstat(target.c_str(), &st) != 0)
		return fail(Status::NotFound, ErrorCode::NoSuchName, "there is nothing called " + path);

	if (S_ISDIR(st.st_mode))
	{
		if (rmdir(target.c_str()) != 0)
		{
			if (errno == ENOTEMPTY || errno == EEXIST)
				return fail(Status::Conflict, ErrorCode::NotEmpty,
					    "the directory " + path + " still holds something");
			return fail(Status::Internal, ErrorCode::ChangeRefused, "the directory " + path + " could not be removed");
		}
		return ok();
	}

	if (unlink(target.c_str()) != 0)
		return fail(Status::Internal, ErrorCode::ChangeRefused, "the name " + path + " could not be removed");
	return ok();
}

Result<int> openFile(const std::string &path)
{
	Result<std::string> resolved = resolveExisting(path);
	if (!resolved.ok())
		return fail(resolved.error());

	/* The name the resolution answered and not the one that was written, so that what is
	   opened is what was checked.

	   O_NOFOLLOW closes the one step between those two that resolution cannot cover: the last
	   component can be replaced by a link after it was looked at. Everything further up the
	   name can be replaced as well and nothing here can see that.

	   O_NONBLOCK because a name inside the roots can be a pipe, and opening one for reading
	   waits for a writer that may never come; this runs on the thread answering a request.
	   Taken off again below once the kind is known. */
	if (internal::open_probe != NULL)
		internal::open_probe();

	const int fd = open(resolved.value().c_str(),
			    O_RDONLY | O_CLOEXEC | O_NOFOLLOW | O_NONBLOCK);
	if (fd < 0)
		return fail(Status::NotFound, ErrorCode::NoSuchName, "there is nothing to read called " + path);

	struct stat st;
	// The same answer for a name that is not a plain file as for one that is
	// not there, so that what a name is stays as unreadable from outside as
	// whether it is there at all.
	if (fstat(fd, &st) != 0 || !S_ISREG(st.st_mode))
	{
		close(fd);
		return fail(Status::NotFound, ErrorCode::NoSuchName, "there is nothing to read called " + path);
	}

	const int flags = fcntl(fd, F_GETFL);
	if (flags < 0 || fcntl(fd, F_SETFL, flags & ~O_NONBLOCK) < 0)
	{
		close(fd);
		return fail(Status::Internal, ErrorCode::ChangeRefused, "the file " + path + " could not be made ready to read");
	}

	return ok(fd);
}

Result<Wrote> writeFile(const std::string &path, const std::string &bytes, bool overwrite)
{
	// The directory is what has to be inside a root, and the name itself is
	// left alone, so that what is written is the name and not what a link of
	// that name points at.
	Result<std::string> resolved = resolveHolder(path);
	if (!resolved.ok())
		return fail(resolved.error());
	const std::string target = resolved.value();

	struct stat st;
	Wrote what = Wrote::Created;
	// lstat and not stat, so that a name which is a link is answered for as a
	// link rather than as whatever it leads to.
	if (lstat(target.c_str(), &st) == 0)
	{
		what = Wrote::Replaced;
		if (!overwrite)
			return fail(Status::Conflict, ErrorCode::NameTaken, "there is already something called " + path);
		if (!S_ISREG(st.st_mode))
			return fail(Status::Conflict, ErrorCode::NotAPlainFile,
				    "what is called " + path + " is not a plain file, so nothing is put in its place");
	}

	/* Where the bytes go before they take the name. Asked of the writer rather than spelled
	   again here, a second spelling being able to drift from the name actually opened.
	   Something already there is either a second write of this file still running or what one
	   that was cut off left behind: the first would interleave two callers' bytes into one
	   file, and the second is a name somebody has to look at. */
	const std::string side = CAtomicFileWriter::sideNameFor(target);
	if (lstat(side.c_str(), &st) == 0)
		return fail(Status::Conflict, ErrorCode::SideFileInTheWay,
			    "a file is already being written beside " + path);

	CAtomicFileWriter writer(target);
	FILE *out = writer.file();
	if (out == NULL)
		return fail(Status::Internal, ErrorCode::ChangeRefused, "the file " + path + " could not be opened for writing");

	if (internal::write_probe != NULL)
		internal::write_probe();

	if (!bytes.empty() && fwrite(bytes.data(), 1, bytes.size(), out) != bytes.size())
		// The writer takes the side file away again by going out of scope, so
		// what was there before this call is what is there after it.
		return fail(Status::Internal, ErrorCode::ChangeRefused, "the file " + path + " could not be written");

	if (!writer.commit())
		return fail(Status::Internal, ErrorCode::ChangeRefused, "the file " + path + " could not be written");

	return ok(what);
}

Result<std::vector<MountInfo> > mounts()
{
	FILE *f = fopen(internal::mounts_path, "r");
	if (f == NULL)
		return fail(Status::Internal, ErrorCode::NoMountTable, "the kernel's list of mounted filesystems could not be read");

	std::vector<MountInfo> out;
	char line[4096];
	while (fgets(line, sizeof(line), f) != NULL)
	{
		char device[1024];
		char mountpoint[1024];
		char fstype[256];
		if (sscanf(line, "%1023s %1023s %255s", device, mountpoint, fstype) != 3)
			continue;

		MountInfo m;
		m.device = unescapeMountField(device);
		m.mountpoint = unescapeMountField(mountpoint);
		m.fstype = unescapeMountField(fstype);

		struct statfs s;
		if (statfs(m.mountpoint.c_str(), &s) == 0)
		{
			m.total = (uint64_t) s.f_blocks * (uint64_t) s.f_bsize;
			m.free = (uint64_t) s.f_bavail * (uint64_t) s.f_bsize;
		}
		out.push_back(m);
	}
	fclose(f);
	return ok(std::move(out));
}

Result<FsUsage> stats(const std::string &path)
{
	Result<std::string> resolved = resolveExisting(path);
	if (!resolved.ok())
		return fail(resolved.error());

	struct statfs s;
	if (statfs(resolved.value().c_str(), &s) != 0)
		return fail(Status::NotFound, ErrorCode::NoSuchFilesystem,
			    "there is no filesystem the kernel will answer for at " + path);

	FsUsage out;
	out.total = (uint64_t) s.f_blocks * (uint64_t) s.f_bsize;
	// What is available rather than what is unallocated: the reserve a
	// filesystem keeps for the superuser is not room a recording can use.
	out.free = (uint64_t) s.f_bavail * (uint64_t) s.f_bsize;
	return ok(out);
}

} // namespace storage
} // namespace coreapi
