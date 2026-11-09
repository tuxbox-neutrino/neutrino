/*
 * backup.cpp - the box's settings packed into one archive, and put back
 *
 * Copyright (C) 2026 Thilo Graf
 *
 * License: GPL-2.0-or-later
 */

#include <config.h>

#include "coreapi/backup.h"
#include "coreapi/box/storage_internal.h"

#include "coreapi/base/accessfiles.h"

#include "coreapi/base/errors.h"
#include "coreapi/base/settingswrites.h"
#include "coreapi/storage.h"
#include "coreapi/tarfile.h"
#include "coreapi/shellcontract.h"

#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <utility>
#include <vector>

#include <dirent.h>
#include <fcntl.h>
#include <pthread.h>
#include <sched.h>
#include <signal.h>
#include <fnmatch.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

namespace coreapi
{
namespace backup
{

namespace internal
{
const char *tmp_dir = "/tmp";
const char *config_dir = CONFIGDIR;
const char *backup_list = CONFIGDIR "/tobackup.conf";
const char *tuxbox_dir = "/var/tuxbox";
const char *script_dir = NULL;
const char *restart_command = "sleep 2; sync; exec reboot -f";
#ifdef RESTORE_RESTARTS_NEUTRINO
bool restart_in_place = true;
#else
bool restart_in_place = false;
#endif
size_t max_members = 20000;
unsigned long long max_unpacked = 256ull * 1024 * 1024;

namespace
{
AccountLinesSeen accountLinesSeen = NULL;
} // anonymous namespace

void setAccountLinesSeenForTest(AccountLinesSeen seen)
{
	accountLinesSeen = seen;
}

StagingMade stagingMade = NULL;
bool stagingNamed = false;

void setStagingMadeForTest(StagingMade made, bool with_a_name)
{
	stagingMade = made;
	stagingNamed = with_a_name;
}
} // namespace internal

namespace
{

internal::Restarter restarter = NULL;

/* One restore at a time, from taking the upload over to the restart: two at
   once would write the same files, and one started while another waits for
   its restart would be cut short by it. Pending is set once settings are put
   back and stays until the program is gone. */
OpenThreads::Mutex &restoring()
{
	static OpenThreads::Mutex m;
	return m;
}
bool pending = false;

bool runnable(const std::string &p)
{
	struct stat st;
	return stat(p.c_str(), &st) == 0 && S_ISREG(st.st_mode) && access(p.c_str(), X_OK) == 0;
}

/* Where the program's own search path finds it, as the settings menu finds its
   scripts, and otherwise in the directories the shell runner hands its scripts
   as their search path. */
std::string findProgram(const char *name, bool ours)
{
	if (ours && internal::script_dir != NULL)
	{
		const std::string p = std::string(internal::script_dir) + "/" + name;
		return runnable(p) ? p : std::string();
	}
	const char *env = getenv("PATH");
	std::string path = env != NULL ? env : "";
	path += ":/sbin:/usr/sbin:/bin:/usr/bin";
	size_t at = 0;
	while (at <= path.size())
	{
		size_t end = path.find(':', at);
		if (end == std::string::npos)
			end = path.size();
		if (end > at && path[at] == '/')
		{
			const std::string p = path.substr(at, end - at) + "/" + name;
			if (runnable(p))
				return p;
		}
		at = end + 1;
	}
	return std::string();
}

bool endsWith(const std::string &s, const char *tail)
{
	const size_t n = strlen(tail);
	return s.size() >= n && s.compare(s.size() - n, n, tail) == 0;
}

bool startsWith(const std::string &s, const std::string &head)
{
	return s.compare(0, head.size(), head) == 0;
}

// Everything in a directory of the case's own or of make(), and the directory.
void removeTree(const std::string &dir)
{
	DIR *d = opendir(dir.c_str());
	if (d != NULL)
	{
		struct dirent *e;
		while ((e = readdir(d)) != NULL)
		{
			if (strcmp(e->d_name, ".") == 0 || strcmp(e->d_name, "..") == 0)
				continue;
			unlink((dir + "/" + e->d_name).c_str());
		}
		closedir(d);
	}
	rmdir(dir.c_str());
}

bool readWhole(const std::string &path, size_t limit, std::string &out)
{
	const int fd = open(path.c_str(), O_RDONLY | O_CLOEXEC);
	if (fd < 0)
		return false;
	struct stat st;
	if (fstat(fd, &st) != 0 || !S_ISREG(st.st_mode) || (size_t) st.st_size > limit)
	{
		close(fd);
		return false;
	}
	out.resize((size_t) st.st_size);
	size_t done = 0;
	while (done < out.size())
	{
		const ssize_t n = read(fd, &out[done], out.size() - done);
		if (n <= 0)
		{
			close(fd);
			return false;
		}
		done += (size_t) n;
	}
	close(fd);
	return true;
}

std::vector<std::string> linesOf(const std::string &text)
{
	std::vector<std::string> out;
	size_t at = 0;
	while (at < text.size())
	{
		size_t end = text.find('\n', at);
		if (end == std::string::npos)
			end = text.size();
		if (end > at)
			out.push_back(text.substr(at, end - at));
		at = end + 1;
	}
	return out;
}

std::string trimmed(const std::string &s)
{
	size_t a = 0, b = s.size();
	while (a < b && (s[a] == ' ' || s[a] == '\t' || s[a] == '\r'))
		++a;
	while (b > a && (s[b - 1] == ' ' || s[b - 1] == '\t' || s[b - 1] == '\r'))
		--b;
	return s.substr(a, b - a);
}

std::string withoutTrailingSlash(std::string p)
{
	while (p.size() > 1 && p[p.size() - 1] == '/')
		p.erase(p.size() - 1);
	return p;
}

/* What tobackup.conf names besides the configuration directory: a line each,
   a comment after #, a leading - for something left out of a wider entry,
   which is nothing to put back. */
std::vector<std::string> listedPaths()
{
	std::vector<std::string> out;
	std::string text;
	if (!readWhole(internal::backup_list, 1024 * 1024, text))
		return out;
	const std::vector<std::string> lines = linesOf(text);
	for (size_t i = 0; i < lines.size(); ++i)
	{
		std::string l = lines[i];
		const size_t hash = l.find('#');
		if (hash != std::string::npos)
			l.erase(hash);
		l = trimmed(l);
		if (l.empty() || l[0] == '-' || l[0] != '/')
			continue;
		out.push_back(l);
	}
	return out;
}

bool hasPattern(const std::string &s)
{
	return s.find_first_of("*?[") != std::string::npos;
}

/* Whether a path the archive would write lands where the box backs up, and if so
   the directory it lands below that the box was set up with: the configuration
   directory, a directory tobackup.conf names, or the directory of a file or of a
   pattern it names. What is below that directory is the archive's to fill. */
bool namedBase(const std::string &path, const std::vector<std::string> &listed, std::string &base)
{
	const std::string config = withoutTrailingSlash(internal::config_dir);
	if (path == config || startsWith(path, config + "/"))
	{
		base = config;
		return true;
	}
	for (size_t i = 0; i < listed.size(); ++i)
	{
		const std::string &e = listed[i];
		const bool dir_entry = e[e.size() - 1] == '/';
		const std::string entry = withoutTrailingSlash(e);
		const std::string parent = entry.substr(0, entry.rfind('/'));
		if (hasPattern(e))
		{
			if (fnmatch(e.c_str(), path.c_str(), FNM_PATHNAME) == 0)
			{
				// the directories before the first one a pattern stands for
				const size_t wild = e.find_first_of("*?[");
				const size_t cut = e.rfind('/', wild);
				base = cut == 0 || cut == std::string::npos ? "/" : e.substr(0, cut);
				return true;
			}
			continue;
		}
		if (path == entry || startsWith(path, entry + "/"))
		{
			base = (dir_entry || path != entry) ? entry : (parent.empty() ? "/" : parent);
			return true;
		}
	}
	return false;
}

/* The deepest part of dir the box has. A directory tobackup.conf names need not
   be there yet: the plugins make theirs on their first run, and a freshly
   flashed box has none of them, nor /etc/samba. What is missing of it is made
   like any directory below it, without following a link on the way. */
std::string deepestPresent(std::string dir)
{
	struct stat st;
	while (dir != "/" && lstat(dir.c_str(), &st) != 0)
	{
		const size_t cut = dir.rfind('/');
		dir = (cut == 0 || cut == std::string::npos) ? std::string("/") : dir.substr(0, cut);
	}
	return dir;
}

// namedBase, with the base the deepest part of it the box has.
bool backedUp(const std::string &path, const std::vector<std::string> &listed, std::string &base)
{
	if (!namedBase(path, listed, base))
		return false;
	base = deepestPresent(base);
	return true;
}

/* Whether something on the box already stands below base on the way to path as
   a link: tar unpacks through a directory that is a link, so an archive could
   write wherever it points. The base itself is the box's own setting and may be
   one. */
bool linkOnTheWay(const std::string &base, const std::string &path)
{
	std::string at = base == "/" ? std::string() : base;
	size_t from = base == "/" ? 1 : base.size() + 1;
	while (from <= path.size())
	{
		size_t end = path.find('/', from);
		if (end == std::string::npos)
			end = path.size();
		at += "/" + path.substr(from, end - from);
		struct stat st;
		if (lstat(at.c_str(), &st) != 0)
			return false;	// nothing there yet, and nothing below it either
		if (S_ISLNK(st.st_mode))
			return true;
		from = end + 1;
	}
	return false;
}

// What a file of the archive is written under before it takes its place.
const char *const kWorkingPrefix = storage::kRestoreWorkingPrefix;

/* A name as the archive carries it, made into the path it is unpacked to: below
   the box's /var/tuxbox when any name begins with config/, the archives of the
   box's own layout, and below / when none does, as restore.sh does it for the
   menu. Empty for a name that may not be unpacked at all. */
std::string landing(std::string name, bool under_tuxbox)
{
	while (startsWith(name, "./"))
		name.erase(0, 2);
	name = withoutTrailingSlash(name);
	if (name.empty() || name == "." || name[0] == '/')
		return std::string();
	size_t at = 0;
	while (at <= name.size())
	{
		size_t end = name.find('/', at);
		if (end == std::string::npos)
			end = name.size();
		// a part that climbs, one that stands for nothing (. or an empty part
		// between two slashes), read alike by no two readers, and the names this
		// layer writes its files under before they take their place
		const std::string part = name.substr(at, end - at);
		if (part == ".." || part == "." || part.empty() || startsWith(part, kWorkingPrefix))
			return std::string();
		at = end + 1;
	}
	return (under_tuxbox ? withoutTrailingSlash(internal::tuxbox_dir) + "/" : std::string("/")) + name;
}

bool controlFree(const std::string &s)
{
	for (size_t i = 0; i < s.size(); ++i)
		if ((unsigned char) s[i] < 0x20 || s[i] == 0x7f)
			return false;
	return true;
}

/* What this program was started as: its file, without the mark the kernel
   puts behind one replaced since, and its arguments, the first included. */
bool selfAndArguments(std::string &self, std::vector<std::string> &args)
{
	char exe[PATH_MAX];
	const ssize_t n = readlink("/proc/self/exe", exe, sizeof(exe) - 1);
	if (n <= 0)
		return false;
	self.assign(exe, (size_t) n);
	const std::string gone(" (deleted)");
	if (endsWith(self, gone.c_str()))
		self.erase(self.size() - gone.size());

	const int fd = open("/proc/self/cmdline", O_RDONLY | O_CLOEXEC);
	if (fd < 0)
		return false;
	std::string cmdline;
	char buf[4096];
	for (;;)
	{
		const ssize_t got = read(fd, buf, sizeof(buf));
		if (got < 0 && errno == EINTR)
			continue;
		if (got <= 0)
			break;
		cmdline.append(buf, (size_t) got);
	}
	close(fd);
	size_t at = 0;
	while (at < cmdline.size())
	{
		size_t end = cmdline.find('\0', at);
		if (end == std::string::npos)
			end = cmdline.size();
		args.push_back(cmdline.substr(at, end - at));
		at = end + 1;
	}
	return !args.empty();
}

/* Whether the restart after a restore can be started at all, asked before
   anything is put back: a box that cannot restart would go on with its old
   settings over the ones put back. */
bool restartPossible()
{
	if (internal::restart_in_place)
	{
		std::string self;
		std::vector<std::string> args;
		return selfAndArguments(self, args) && access(self.c_str(), X_OK) == 0;
	}
	return access("/bin/sh", X_OK) == 0;
}

// What the restart of this program in its own place runs from.
struct InPlace
{
	std::string self;
	std::vector<std::string> args;
	// whether the restarting thread has a table of open files of its own:
	// 0 not said yet, 1 it has, -1 it could not take one
	pthread_mutex_t lock;
	pthread_cond_t said;
	int isolated;
};

/* This program started again in its own place after a moment, without saving
   anything: the file it was started from, with its arguments, takes over the
   process. Its number stays, so whatever started it and watches it, a start
   script that restarts it or a service manager, sees nothing end, and nothing
   starts it twice. What it holds open is closed on the way, its ports among
   it, and the signals a thread of this layer blocks are let through again.

   This thread first takes a table of open files of its own, a copy of the
   program's, and says whether it got one before anything else: the other
   threads go on accepting connections and opening files until the start, and
   one opened after the sweep below would otherwise reach the new program
   open, with nothing left to serve or close it. Without a table of its own
   there is no restart; the starter says so. The sweep reads this thread's own
   table, which /proc/self/fd, the table of the program's first thread, is no
   longer. */
void *restartInPlace(void *arg)
{
	InPlace *in = static_cast<InPlace *>(arg);
	const int isolated = unshare(CLONE_FILES) == 0 ? 1 : -1;
	const int why = errno;
	pthread_mutex_lock(&in->lock);
	in->isolated = isolated;
	pthread_cond_signal(&in->said);
	pthread_mutex_unlock(&in->lock);
	if (isolated < 0)
	{
		// the starter waits for this thread and takes what it was handed back
		std::fprintf(stderr, "[backup] the restart could not take open files of its own: %s\n",
			     std::strerror(why));
		return NULL;
	}
	sleep(2);
	sync();
	char table[64];
	std::snprintf(table, sizeof(table), "/proc/self/task/%ld/fd", (long) syscall(SYS_gettid));
	DIR *d = opendir(table);
	if (d != NULL)
	{
		const int own = dirfd(d);
		struct dirent *e;
		while ((e = readdir(d)) != NULL)
		{
			const int fd = atoi(e->d_name);
			if (fd > 2 && fd != own)
				fcntl(fd, F_SETFD, FD_CLOEXEC);
		}
		closedir(d);
	}
	sigset_t none;
	sigemptyset(&none);
	pthread_sigmask(SIG_SETMASK, &none, NULL);
	std::vector<char *> argv;
	for (size_t i = 0; i < in->args.size(); ++i)
		argv.push_back((char *) in->args[i].c_str());
	argv.push_back(NULL);
	execv(in->self.c_str(), &argv[0]);
	std::fprintf(stderr, "[backup] the settings are back, and %s could not be started again: %s; "
		     "restart it by hand before anything is saved\n", in->self.c_str(), std::strerror(errno));
	return NULL;
}

bool startInPlace()
{
	InPlace *in = new InPlace;
	if (!selfAndArguments(in->self, in->args) || access(in->self.c_str(), X_OK) != 0)
	{
		delete in;
		return false;
	}
	pthread_mutex_init(&in->lock, NULL);
	pthread_cond_init(&in->said, NULL);
	in->isolated = 0;
	pthread_t t;
	if (pthread_create(&t, NULL, &restartInPlace, in) != 0)
	{
		pthread_cond_destroy(&in->said);
		pthread_mutex_destroy(&in->lock);
		delete in;
		return false;
	}
	pthread_mutex_lock(&in->lock);
	while (in->isolated == 0)
		pthread_cond_wait(&in->said, &in->lock);
	const bool isolated = in->isolated > 0;
	pthread_mutex_unlock(&in->lock);
	if (!isolated)
	{
		pthread_join(t, NULL);
		pthread_cond_destroy(&in->said);
		pthread_mutex_destroy(&in->lock);
		delete in;
		return false;
	}
	/* The thread runs on into the start and holds what it was handed until the
	   program is replaced; a start that fails leaves it held, which is the
	   least of what a program that has to be restarted by hand holds. */
	pthread_detach(t);
	return true;
}

/* The system restarted at once once the settings are back, as restore.sh does
   it for the menu, without the program saving its own settings over them.
   Started on its own, out of this program's process group, through a shell
   running internal::restart_command after a moment: the answer that says it
   was started has to leave before. The shell gets this program's file as $0,
   its number as $1 and its arguments after that, which the restart of the
   system leaves unread. Whether the shell itself started is read from a pipe
   that its start closes; between the forks only calls that are safe there. */
bool startShell()
{
	// everything the shell is handed made before the forks, where nothing may be allocated
	std::string self;
	std::vector<std::string> args;
	if (!selfAndArguments(self, args))
		return false;
	char pid[24];
	std::snprintf(pid, sizeof(pid), "%ld", (long) getpid());
	std::vector<char *> argv;
	argv.push_back((char *) "/bin/sh");
	argv.push_back((char *) "-c");
	argv.push_back((char *) internal::restart_command);
	argv.push_back((char *) self.c_str());
	argv.push_back(pid);
	for (size_t i = 1; i < args.size(); ++i)
		argv.push_back((char *) args[i].c_str());
	argv.push_back(NULL);

	int ack[2];
	if (pipe2(ack, O_CLOEXEC) != 0)
		return false;
	const pid_t child = fork();
	if (child < 0)
	{
		close(ack[0]);
		close(ack[1]);
		return false;
	}
	if (child == 0)
	{
		const pid_t grandchild = fork();
		if (grandchild < 0)
			_exit(1);
		if (grandchild != 0)
			_exit(0);
		setsid();
		for (int fd = 3; fd < 1024; ++fd)
			if (fd != ack[1])
				close(fd);
		execv("/bin/sh", &argv[0]);
		const int why = errno;
		ssize_t ignored = write(ack[1], &why, sizeof(why));
		(void) ignored;
		_exit(127);
	}
	close(ack[1]);
	int status = 0;
	pid_t waited;
	do
		waited = waitpid(child, &status, 0);
	while (waited < 0 && errno == EINTR);
	int why = 0;
	ssize_t got;
	do
		got = read(ack[0], &why, sizeof(why));
	while (got < 0 && errno == EINTR);
	close(ack[0]);
	// ECHILD: the program discards what its children exit with, and then the
	// pipe alone says whether the shell started
	const bool forked = waited == child ? (WIFEXITED(status) && WEXITSTATUS(status) == 0)
	                                    : (waited < 0 && errno == ECHILD);
	return forked && got == 0;
}

/* The directories below base on the way to path, made where they are missing
   and opened one at a time without following a link, so what is written lands
   where the check said it would, whatever is put on the way meanwhile; base
   itself is the box's own setting and may be one. The whole path for a
   directory entry and all but the last component for a file, which leaf
   names. The directory the entry goes into, or -1. dirs keeps every directory
   opened, by its path, for the renames. A directory entry the box does not
   have yet is made for its owner alone, so the files going into it can be
   written whatever the entry says, and gets the permissions it carries once
   they are in (unpack). */
int walkDown(const std::string &base, const std::string &path, bool directory,
	     std::map<std::string, int> &dirs, std::string &leaf)
{
	std::string at = withoutTrailingSlash(base);
	int fd;
	std::map<std::string, int>::iterator known = dirs.find(at);
	if (known != dirs.end())
		fd = known->second;
	else
	{
		fd = open(at.c_str(), O_RDONLY | O_DIRECTORY | O_CLOEXEC);
		if (fd < 0)
			return -1;
		dirs[at] = fd;
	}
	if (path.size() <= at.size())
		return directory ? fd : -1;
	std::vector<std::string> parts;
	size_t from = at == "/" ? 1 : at.size() + 1;
	while (from <= path.size())
	{
		size_t end = path.find('/', from);
		if (end == std::string::npos)
			end = path.size();
		parts.push_back(path.substr(from, end - from));
		from = end + 1;
	}
	const size_t walk = directory ? parts.size() : parts.size() - 1;
	for (size_t i = 0; i < walk; ++i)
	{
		const std::string next = (at == "/" ? std::string() : at) + "/" + parts[i];
		known = dirs.find(next);
		if (known != dirs.end())
		{
			fd = known->second;
			at = next;
			continue;
		}
		const bool own = directory && i + 1 == parts.size();
		if (mkdirat(fd, parts[i].c_str(), own ? 0700 : 0755) != 0 && errno != EEXIST)
			return -1;
		const int sub = openat(fd, parts[i].c_str(), O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC);
		if (sub < 0)
			return -1;
		dirs[next] = sub;
		fd = sub;
		at = next;
	}
	leaf = directory ? std::string() : parts[parts.size() - 1];
	return fd;
}

/* The keys of nhttpd.conf that name the account, as the two servers read them (up to
   the first =): the user name and the password the first start takes over where the
   box has no access store, and the two a file older than its first version carried
   them under, which the old server's start turns into the first two. */
const char *const kAccountKeys[] = { "mod_auth.username", "mod_auth.password", "AuthUser", "AuthPassword" };

// More than nhttpd.conf comes to on any box; a larger one is not put back.
const size_t kServerFileLimit = 1024 * 1024;

/* Whether a line of nhttpd.conf names the account. The key as the readers take it, and
   also with blanks around it, so that no spelling a reader might take is left in. */
bool namesAccount(const std::string &line)
{
	const size_t eq = line.find('=');
	if (eq == std::string::npos)
		return false;
	const std::string key = trimmed(line.substr(0, eq));
	for (size_t i = 0; i < sizeof(kAccountKeys) / sizeof(kAccountKeys[0]); ++i)
		if (key == kAccountKeys[i])
			return true;
	return false;
}

// What an open file holds from its start, no more than limit bytes of it.
bool readFrom(int fd, size_t limit, std::string &out)
{
	out.clear();
	if (lseek(fd, 0, SEEK_SET) != 0)
		return false;
	char buf[4096];
	for (;;)
	{
		const ssize_t n = read(fd, buf, sizeof(buf));
		if (n < 0)
			return false;
		if (n == 0)
			return true;
		out.append(buf, (size_t) n);
		if (out.size() > limit)
			return false;
	}
}

/* nhttpd.conf as the archive has it, written to fd, with every line that names the
   account taken out and the box's own such lines put in: an archive never brings a
   login along, whoever made it. The box's file is read in dir under leaf; one the box
   does not have leaves the archive's file without an account. False where either could
   not be read or is larger than any such file. */
bool keepAccountLines(int dir, const std::string &leaf, int fd)
{
	std::string own;
	const int had = openat(dir, leaf.c_str(), O_RDONLY | O_NOFOLLOW | O_CLOEXEC);
	if (had >= 0)
	{
		const bool read_own = readFrom(had, kServerFileLimit, own);
		close(had);
		if (!read_own)
			return false;
	}
	else if (errno != ENOENT)
		return false;

	std::string archived;
	if (!readFrom(fd, kServerFileLimit, archived))
		return false;

	std::string out;
	const std::vector<std::string> put = linesOf(archived);
	for (size_t i = 0; i < put.size(); ++i)
		if (!namesAccount(put[i]))
			out += put[i] + "\n";
	const std::vector<std::string> kept = linesOf(own);
	for (size_t i = 0; i < kept.size(); ++i)
		if (namesAccount(kept[i]))
			out += kept[i] + "\n";

	if (ftruncate(fd, 0) != 0 || lseek(fd, 0, SEEK_SET) != 0)
		return false;
	size_t done = 0;
	while (done < out.size())
	{
		const ssize_t n = write(fd, out.data() + done, out.size() - done);
		if (n <= 0)
			return false;
		done += (size_t) n;
	}
	return true;
}

// A file of the archive written beside the name it is to take.
struct Written
{
	int dir;
	std::string temp;
	std::string name;
};

/* The archive that passed, unpacked with the reader that checked it. First every
   file is written beside its place under a name of its own, and only once all
   of them are whole does each take its name, in the order of the archive, so a
   later entry of the same name wins as it does with tar. A file that cannot be
   written leaves the box's settings as they were; what is new by then is no
   more than a directory. Each file gets the permissions its entry carries,
   without set-id bits. The entries are read again and have to be the ones
   checked. */
bool unpack(const std::string &archive, const std::vector<tarfile::Member> &members, bool under_tuxbox,
	    const std::vector<std::string> &listed, bool &changed, std::string &why)
{
	changed = false;
	tarfile::Reader tar(archive, internal::max_members);
	std::map<std::string, int> dirs;
	std::vector<Written> written;
	// the directory entries, by their path, with the permissions they carry
	std::vector<std::pair<std::string, mode_t> > modes;
	/* A word of this restore's own in every working name, which no archive
	   name can carry (landing() refuses the prefix), and which no restore cut
	   short before has used: nothing that is there already is touched. */
	char word[17];
	{
		unsigned char raw[8] = { 0 };
		const int r = open("/dev/urandom", O_RDONLY | O_CLOEXEC);
		const bool random = r >= 0 && read(r, raw, sizeof(raw)) == (ssize_t) sizeof(raw);
		if (r >= 0)
			close(r);
		if (!random)
		{
			const unsigned long long t = (unsigned long long) time(NULL) ^ ((unsigned long long) getpid() << 32);
			std::memcpy(raw, &t, sizeof(raw));
		}
		for (size_t k = 0; k < sizeof(raw); ++k)
			std::snprintf(word + 2 * k, 3, "%02x", raw[k]);
	}
	bool ok = tar.opened();
	why = "the archive could not be read again";
	for (size_t i = 0; ok && i < members.size(); ++i)
	{
		tarfile::Member m;
		ok = tar.next(m) == tarfile::Reader::Entry && m.path == members[i].path &&
		     m.type == members[i].type && m.size == members[i].size;
		if (!ok)
			break;
		const std::string path = landing(m.path, under_tuxbox);
		std::string base;
		std::string leaf;
		const int dir = !path.empty() && backedUp(path, listed, base) ?
				walkDown(base, path, m.type == '5', dirs, leaf) : -1;
		if (dir < 0)
		{
			why = "a directory on the way to " + path + " could not be made, or is a link";
			ok = false;
			break;
		}
		if (m.type == '5')
		{
			modes.push_back(std::make_pair(path, (mode_t) (m.mode & 0777)));
			ok = tar.skip(m);
			continue;
		}
		/* The login stays the box's: the access store, the name it had before and the
		   working files are not put back, and nhttpd.conf is, with the box's own lines
		   for the account. Whoever can reach this route can restore settings; taking
		   the login over is not among them. */
		const storage::InStore store = storage::restoreOf(dir, leaf);
		if (store == storage::InStore::Kept)
		{
			ok = tar.skip(m);
			if (!ok)
				why = "the archive could not be read again";
			continue;
		}
		char temp[64];
		std::snprintf(temp, sizeof(temp), "%s%s-%lu", kWorkingPrefix, word, (unsigned long) i);
		const bool account = store == storage::InStore::AccountKept;
		/* The copy of nhttpd.conf gets the box's own account lines, a
		   password among them on a box whose login was never carried over:
		   it is one of the files that hold who may reach the box from the
		   moment it is made, before a byte is written (accessfiles), and
		   the nhttpd.conf it replaces is kept before as well. Made without a
		   name where the filesystem can (O_TMPFILE: Linux 3.11 on, ext4,
		   tmpfs, ubifs from 4.9, not jffs2) and named only once it is
		   whole; elsewhere made under its name while no gate asks about a
		   descriptor (accessfiles::Making), so none opened on it before it
		   was kept is let through. Not kept, nothing is written. */
		int fd = -1;
		bool unnamed = false;
		if (account)
		{
			accessfiles::Making making;
			if (!accessfiles::named(path))
			{
				why = path + " could not be kept as a file that holds who may reach the box";
				ok = false;
				break;
			}
#ifdef O_TMPFILE
			if (!internal::stagingNamed)
				fd = openat(dir, ".", O_TMPFILE | O_RDWR | O_CLOEXEC, 0600);
			unnamed = fd >= 0;
#endif
			if (!unnamed)
				fd = openat(dir, temp, O_RDWR | O_CREAT | O_EXCL | O_NOFOLLOW | O_CLOEXEC, 0600);
			if (fd >= 0 && internal::stagingMade != NULL)
				internal::stagingMade(dir, unnamed ? std::string() : std::string(temp));
			if (fd >= 0 && !accessfiles::created(fd))
			{
				close(fd);
				fd = -1;
				if (!unnamed)
					unlinkat(dir, temp, 0);
				why = path + " could not be kept as a file that holds who may reach the box";
				ok = false;
				break;
			}
		}
		else
			fd = openat(dir, temp, O_WRONLY | O_CREAT | O_EXCL | O_NOFOLLOW | O_CLOEXEC, 0600);
		if (fd < 0)
		{
			why = path + " could not be written";
			ok = false;
			break;
		}
		if (!unnamed)
		{
			Written w = { dir, temp, leaf };
			written.push_back(w);
		}
		/* A file the box has keeps its owner, a service's own file among them,
		   which a restore by this program would otherwise hand to it; a new one
		   is this program's. The owner first, the permissions after, as a
		   change of owner can take bits away. */
		struct stat had;
		bool owned = true;
		if (fstatat(dir, leaf.c_str(), &had, AT_SYMLINK_NOFOLLOW) == 0 &&
		    (had.st_uid != geteuid() || had.st_gid != getegid()))
			owned = fchown(fd, had.st_uid, had.st_gid) == 0 || geteuid() != 0;
		ok = owned && tar.copy(m, fd) &&
		     (!account || keepAccountLines(dir, leaf, fd)) &&
		     fchmod(fd, (mode_t) (m.mode & 0777)) == 0;
		// whole now: the copy made without a name gets its working name
		if (ok && unnamed)
		{
			char own[64];
			std::snprintf(own, sizeof(own), "/proc/self/fd/%d", fd);
			ok = linkat(AT_FDCWD, own, dir, temp, AT_SYMLINK_FOLLOW) == 0 ||
			     linkat(fd, "", dir, temp, AT_EMPTY_PATH) == 0;
			if (ok)
			{
				Written w = { dir, temp, leaf };
				written.push_back(w);
			}
		}
		if (ok && account && internal::accountLinesSeen != NULL)
			internal::accountLinesSeen(dir, temp);
		ok = close(fd) == 0 && ok;
		if (!ok)
			why = path + " could not be written";
	}
	size_t renamed = 0;
	for (; ok && renamed < written.size(); ++renamed)
	{
		const Written &w = written[renamed];
		if (renameat(w.dir, w.temp.c_str(), w.dir, w.name.c_str()) != 0)
		{
			why = "the settings were put back in part: " + w.name + " could not take its place";
			ok = false;
			break;
		}
		changed = true;
	}
	for (size_t i = renamed; i < written.size(); ++i)
		unlinkat(written[i].dir, written[i].temp.c_str(), 0);
	/* The permissions of the directory entries last, in the order of the
	   archive, so a later entry of one name wins and none depends on whether
	   the directory was there before or on the program's file mask. One that
	   cannot be set is said and is no reason to keep the box from restarting
	   with what is back. */
	for (size_t i = 0; ok && i < modes.size(); ++i)
	{
		std::map<std::string, int>::iterator d = dirs.find(modes[i].first);
		if (d != dirs.end() && fchmod(d->second, modes[i].second) != 0)
			std::fprintf(stderr, "[backup] the permissions of %s could not be set: %s\n",
				     modes[i].first.c_str(), std::strerror(errno));
	}
	for (std::map<std::string, int>::iterator d = dirs.begin(); d != dirs.end(); ++d)
		close(d->second);
	return ok;
}

/* The upload, copied into a directory of its own that only this program writes
   to, under a name it chose: what was checked is what is put back, whatever
   happens to the upload's name afterwards. */
enum class Staged { Done, TooLarge, Broken };

Staged stage(const std::string &from, const std::string &to)
{
	// without waiting, so a pipe put in the upload's place is not waited on
	const int in = open(from.c_str(), O_RDONLY | O_NOFOLLOW | O_NONBLOCK | O_CLOEXEC);
	if (in < 0)
		return Staged::Broken;
	struct stat st;
	if (fstat(in, &st) != 0 || !S_ISREG(st.st_mode))
	{
		close(in);
		return Staged::Broken;
	}
	// what is opened, and what is read of it, not what the name had before
	if ((unsigned long long) st.st_size > kMaxBytes)
	{
		close(in);
		return Staged::TooLarge;
	}
	const int out = open(to.c_str(), O_WRONLY | O_CREAT | O_EXCL | O_NOFOLLOW | O_CLOEXEC, 0600);
	if (out < 0)
	{
		close(in);
		return Staged::Broken;
	}
	bool ok = true;
	bool large = false;
	unsigned long long total = 0;
	char buf[65536];
	for (;;)
	{
		const ssize_t n = read(in, buf, sizeof(buf));
		if (n < 0 && errno == EINTR)
			continue;
		if (n < 0)
		{
			ok = false;
			break;
		}
		if (n == 0)
			break;
		total += (unsigned long long) n;
		if (total > kMaxBytes)
		{
			large = true;
			ok = false;
			break;
		}
		ssize_t done = 0;
		while (done < n)
		{
			const ssize_t w = write(out, buf + done, n - done);
			if (w < 0 && errno == EINTR)
				continue;
			if (w <= 0)
			{
				ok = false;
				break;
			}
			done += w;
		}
		if (!ok)
			break;
	}
	close(in);
	if (close(out) != 0)
		ok = false;
	return large ? Staged::TooLarge : ok ? Staged::Done : Staged::Broken;
}

bool plainName(const std::string &file)
{
	if (file.empty() || file.size() > 128 || !((file[0] >= 'a' && file[0] <= 'z') ||
	    (file[0] >= 'A' && file[0] <= 'Z') || (file[0] >= '0' && file[0] <= '9')))
		return false;
	for (size_t i = 0; i < file.size(); ++i)
	{
		const char c = file[i];
		if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') ||
		      c == '.' || c == '_' || c == '-'))
			return false;
	}
	return endsWith(file, ".tar.gz") || endsWith(file, ".tgz") || endsWith(file, ".tar");
}

Result<void> refuse(const std::string &dir, const std::string &why)
{
	removeTree(dir);
	return fail(Status::InvalidArgument, ErrorCode::RestoreRefused, why);
}

} // namespace

Result<Archive> make()
{
	const std::string script = findProgram("backup.sh", true);
	if (script.empty())
		return fail(Status::Internal, ErrorCode::BackupFailed, "the box has no backup.sh");

	std::string pattern = std::string(internal::tmp_dir) + "/backup.XXXXXX";
	std::vector<char> buf(pattern.begin(), pattern.end());
	buf.push_back('\0');
	if (mkdtemp(&buf[0]) == NULL)
		return fail(Status::Internal, ErrorCode::BackupFailed, "no directory to pack the settings in");
	const std::string dir(&buf[0]);

	const Param params[] = {
		{ "dir", ParamType::Path, true, "where the archive is written", 0, 4096, NULL, internal::tmp_dir },
	};
	const ShellCommand cmd = { script.c_str(), NULL, params, 1, OutputFormat::Raw, 120000, 64 * 1024, false };
	std::vector<std::pair<std::string, std::string> > args;
	args.push_back(std::make_pair(std::string("dir"), dir));
	Result<std::string> ran = shell::run(cmd, args);
	if (!ran.ok())
	{
		removeTree(dir);
		return fail(Status::Internal, ErrorCode::BackupFailed, "backup.sh failed: " + ran.error().message);
	}

	Archive out;
	DIR *d = opendir(dir.c_str());
	if (d != NULL)
	{
		struct dirent *e;
		while ((e = readdir(d)) != NULL)
			if (endsWith(e->d_name, ".tar.gz") && e->d_name[0] != '.')
				out.name = e->d_name;
		closedir(d);
	}
	const bool read = !out.name.empty() && readWhole(dir + "/" + out.name, kMaxBytes, out.bytes);
	removeTree(dir);
	if (!read)
		return fail(Status::Internal, ErrorCode::BackupFailed,
			    "backup.sh left no archive, or one larger than 64 MiB");
	return ok(std::move(out));
}

Result<void> restore(const std::string &file)
{
	if (!plainName(file))
		return fail(Status::InvalidArgument, ErrorCode::BadName,
			    "the archive is a plain name in the temporary directory, of letters, digits, dots, "
			    "dashes and underscores, ending in .tar.gz, .tgz or .tar");
	const std::string upload = std::string(internal::tmp_dir) + "/" + file;
	struct stat st;
	if (lstat(upload.c_str(), &st) != 0 || !S_ISREG(st.st_mode))
		return fail(Status::NotFound, ErrorCode::NoSuchName, "there is no uploaded archive of that name");
	if ((unsigned long long) st.st_size > kMaxBytes)
		return fail(Status::InvalidArgument, ErrorCode::RestoreRefused, "the archive is larger than 64 MiB");

	if (restoring().trylock() != 0)
		return fail(Status::Conflict, ErrorCode::RestoreRefused, "another restore is under way");
	struct Release
	{
		~Release() { restoring().unlock(); }
	} release;
	if (pending)
		return fail(Status::Conflict, ErrorCode::RestoreRefused,
			    "settings put back before wait for the restart that reads them");
	// asked before anything is put back, which a box that cannot restart would sit on
	if (restarter == NULL && !restartPossible())
		return fail(Status::Internal, ErrorCode::RestoreRefused, "the box cannot restart itself");

	std::string pattern = std::string(internal::tmp_dir) + "/restore.XXXXXX";
	std::vector<char> buf(pattern.begin(), pattern.end());
	buf.push_back('\0');
	if (mkdtemp(&buf[0]) == NULL)
		return fail(Status::Internal, ErrorCode::RestoreRefused, "no directory to check the archive in");
	const std::string dir(&buf[0]);
	const std::string archive = dir + (endsWith(file, ".tar") ? "/settings.tar" : "/settings.tar.gz");
	const Staged staged = stage(upload, archive);
	unlink(upload.c_str());
	if (staged == Staged::TooLarge)
		return refuse(dir, "the archive is larger than 64 MiB");
	if (staged != Staged::Done)
	{
		removeTree(dir);
		return fail(Status::Internal, ErrorCode::RestoreRefused, "the archive could not be taken over");
	}

	std::vector<tarfile::Member> members;
	std::string why;
	if (!tarfile::readMembers(archive, internal::max_members, internal::max_unpacked,
				  "the settings of a box", members, why))
		return refuse(dir, why);

	bool under_tuxbox = false;
	for (size_t i = 0; i < members.size(); ++i)
	{
		const tarfile::Member &m = members[i];
		if (m.type != '0' && m.type != '7' && m.type != '5')
			return refuse(dir, "the archive holds a link or a device, which could carry what follows "
				      "it anywhere or is no setting");
		for (size_t k = 0; k < m.names.size(); ++k)
		{
			if (!controlFree(m.names[k]))
				return refuse(dir, "the archive holds a name with a control character");
			under_tuxbox = under_tuxbox || startsWith(m.names[k], "config/");
		}
	}
	const std::vector<std::string> listed = listedPaths();
	for (size_t i = 0; i < members.size(); ++i)
	{
		// every name a tar could unpack the entry as
		const std::vector<std::string> &names = members[i].names;
		for (size_t k = 0; k < names.size(); ++k)
		{
			const std::string path = landing(names[k], under_tuxbox);
			if (path.empty())
				return refuse(dir, "the archive holds an absolute name, one that climbs with .., one "
					      "with a . or an empty part, or one beginning with .restore-: " + names[k]);
			std::string base;
			if (!backedUp(path, listed, base))
				return refuse(dir, "the archive would write where the box backs nothing up: " + path);
			if (linkOnTheWay(base, path))
				return refuse(dir, "the archive would write through a link on the box: " + path);
			// what the box has there has to be what the entry is
			struct stat there;
			if (path != base && lstat(path.c_str(), &there) == 0 &&
			    (members[i].type == '5') != S_ISDIR(there.st_mode))
				return refuse(dir, std::string("the archive would put a ") +
					      (members[i].type == '5' ? "directory where the box has a file: " :
					       "file where the box has a directory: ") + path);
		}
	}

	/* The archive at odds with itself: one name both a file and a directory, or a
	   file another entry would go below. The box has no say in that, so it is
	   refused here and not found half way through the renames. */
	std::map<std::string, bool> kinds;
	for (size_t i = 0; i < members.size(); ++i)
	{
		const std::string path = landing(members[i].path, under_tuxbox);
		const bool directory = members[i].type == '5';
		std::map<std::string, bool>::iterator known = kinds.find(path);
		if (known != kinds.end() && known->second != directory)
			return refuse(dir, "the archive holds " + path + " as a file and as a directory");
		kinds[path] = directory;
	}
	for (std::map<std::string, bool>::iterator k = kinds.begin(); k != kinds.end(); ++k)
	{
		std::string above = k->first;
		for (size_t cut = above.rfind('/'); cut != std::string::npos && cut > 0; cut = above.rfind('/'))
		{
			above.erase(cut);
			std::map<std::string, bool>::iterator parent = kinds.find(above);
			if (parent != kinds.end() && !parent->second)
				return refuse(dir, "the archive holds " + above + " as a file and puts " + k->first +
					      " below it");
		}
	}

	/* From here on nothing of the box writes a settings file, the writes under
	   way finished first: whatever it saved now would go over what is put back,
	   or what is put back over it. Let go again only where nothing changed. */
	SettingsWrite::freeze();
	bool changed = false;
	const bool unpacked = unpack(archive, members, under_tuxbox, listed, changed, why);
	removeTree(dir);
	if (!unpacked && !changed)
	{
		SettingsWrite::thaw();
		return fail(Status::Internal, ErrorCode::RestoreRefused, why + "; the box is not restarted");
	}
	sync();
	pending = true;
	if (!unpacked)
		return fail(Status::Internal, ErrorCode::RestoreRefused,
			    why + "; the box is not restarted, saves no settings and has to be restarted by hand");

	if (!restartWithoutSaving())
		return fail(Status::Internal, ErrorCode::RestoreRefused,
			    "the settings are back and the restart could not be started: the box saves no "
			    "settings until it is restarted by hand");
	return ok();
}

bool restartWithoutSaving()
{
	if (restarter != NULL)
		return restarter();
	return internal::restart_in_place ? startInPlace() : startShell();
}

MenuRestore::MenuRestore() : taken_(false)
{
	if (restoring().trylock() != 0)
		return;
	if (pending)
	{
		restoring().unlock();
		return;
	}
	taken_ = true;
}

namespace
{
// The restore's working files in the store's directory, by their leaf names.
std::vector<std::string> leftoverLeaves(int dir)
{
	std::vector<std::string> out;
	const int again = dup(dir);
	DIR *d = again >= 0 ? fdopendir(again) : NULL;
	if (d == NULL)
	{
		if (again >= 0)
			close(again);
		return out;
	}
	struct dirent *e;
	while ((e = readdir(d)) != NULL)
		if (startsWith(e->d_name, kWorkingPrefix))
			out.push_back(e->d_name);
	closedir(d);
	return out;
}
} // namespace

bool keepLeftovers(std::string *why)
{
	const std::string where = storage::internal::access_store_dir;
	const int dir = open(where.c_str(), O_RDONLY | O_DIRECTORY | O_CLOEXEC);
	if (dir < 0)
		return true;
	const std::vector<std::string> leaves = leftoverLeaves(dir);
	std::string unkept;
	for (size_t i = 0; i < leaves.size(); ++i)
	{
		const std::string path = where + "/" + leaves[i];
		struct stat st;
		if (fstatat(dir, leaves[i].c_str(), &st, AT_SYMLINK_NOFOLLOW) != 0)
			continue;
		/* A regular file is kept first, which holds back what it shares with a
		   second name; a link is removed and leaves nothing, a directory is no
		   file a restore made. */
		if (S_ISDIR(st.st_mode))
			continue;
		if (S_ISREG(st.st_mode) && !accessfiles::named(path))
		{
			unkept += (unkept.empty() ? "" : ", ") + path;
			continue;
		}
		if (unlinkat(dir, leaves[i].c_str(), 0) != 0 && errno != ENOENT)
			std::fprintf(stderr, "[backup] %s, left by a restore that did not end, could not be removed: %s\n",
				     path.c_str(), std::strerror(errno));
	}
	close(dir);
	if (unkept.empty())
		return true;
	accessfiles::closeTheirDirectory(true);
	if (why != NULL)
		*why = unkept + ", left by a restore that did not end, could not be kept as a file that holds "
		       "who may reach the box; it stays, and no file of " + where + " goes out of the web server";
	return false;
}

std::vector<std::string> leftovers()
{
	std::vector<std::string> out;
	const std::string where = storage::internal::access_store_dir;
	const int dir = open(where.c_str(), O_RDONLY | O_DIRECTORY | O_CLOEXEC);
	if (dir < 0)
		return out;
	const std::vector<std::string> leaves = leftoverLeaves(dir);
	close(dir);
	for (size_t i = 0; i < leaves.size(); ++i)
		out.push_back(where + "/" + leaves[i]);
	return out;
}

MenuRestore::~MenuRestore()
{
	if (taken_)
		restoring().unlock();
}

void MenuRestore::putBack()
{
	if (taken_)
		pending = true;
}

namespace internal
{

void setRestarterForTest(Restarter r)
{
	restarter = r;
}

void restartedForTest()
{
	pending = false;
	SettingsWrite::thaw();
}

} // namespace internal

} // namespace backup
} // namespace coreapi
