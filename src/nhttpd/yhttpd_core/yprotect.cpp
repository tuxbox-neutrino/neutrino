//=============================================================================
// YHTTPD
// yprotect : the files that decide who may reach the box, which no answer
// hands out and no log is written into
//=============================================================================

#include <climits>
#include <cstdio>
#include <cstdlib>
#include <errno.h>
#include <pthread.h>
#include <sys/stat.h>
#include <unistd.h>

#include "yconfig.h"

#include "coreapi/base/accessfiles.h"
#include "yprotect.h"

namespace yprotect
{

namespace
{

pthread_mutex_t filesLock = PTHREAD_MUTEX_INITIALIZER;

std::vector<std::string> &files()
{
	static std::vector<std::string> names;
	return names;
}

void (*lookedHook)(size_t) = NULL;

std::vector<std::string> &keptOverride()
{
	static std::vector<std::string> places;
	return places;
}

// the path with every link followed, or empty
std::string realOf(const std::string &path)
{
	char buf[PATH_MAX];
	if (path.empty() || realpath(path.c_str(), buf) == NULL)
		return std::string();
	return buf;
}

// a lies in b or is b
bool within(const std::string &a, const std::string &b)
{
	if (b == "/")
		return true;
	return a == b || (a.size() > b.size() && a.compare(0, b.size(), b) == 0 && a[b.size()] == '/');
}

// a copy, so that no stat runs under the lock
std::vector<std::string> currentFiles()
{
	pthread_mutex_lock(&filesLock);
	const std::vector<std::string> names = files();
	pthread_mutex_unlock(&filesLock);
	return names;
}

bool sameFile(const struct stat &a, const struct stat &b)
{
	return a.st_dev == b.st_dev && a.st_ino == b.st_ino;
}

void split(const std::string &path, std::string &dir, std::string &base)
{
	const size_t slash = path.rfind('/');
	if (slash == std::string::npos) {
		dir = ".";
		base = path;
	} else {
		dir = (slash == 0) ? std::string("/") : path.substr(0, slash);
		base = path.substr(slash + 1);
	}
}

} // namespace

bool setFiles(const std::vector<std::string> &names)
{
	pthread_mutex_lock(&filesLock);
	files() = names;
	pthread_mutex_unlock(&filesLock);
	// a server with no such files keeps none back; one with them keeps
	// what each name stands for now, as a writer of this run keeps its own
	coreapi::accessfiles::Making making;
	if (names.empty())
		coreapi::accessfiles::forgetAll();
	bool all = true;
	for (size_t i = 0; i < names.size(); i++)
		if (!coreapi::accessfiles::named(names[i]))
			all = false;
	return all;
}

bool isProtected(int fd)
{
	return isProtected(fd, false);
}

bool isProtected(int fd, bool own_unnamed)
{
	/* Asked while no such file is being made (coreapi::accessfiles::Making):
	   one opened before it was kept or lost its name is asked about only
	   after that. */
	coreapi::accessfiles::Asking asking;
	struct stat st;
	if (fd < 0 || fstat(fd, &st) != 0)
		return true;
	const std::vector<std::string> names = currentFiles();
	for (size_t i = 0; i < names.size(); i++) {
		struct stat p;
		if (stat(names[i].c_str(), &p) == 0) {
			if (sameFile(p, st))
				return true;
		} else if (errno != ENOENT && errno != ENOTDIR)
			// a name that cannot be looked at may be this file
			return true;
		if (lookedHook != NULL)
			lookedHook(i);
	}
	/* Kept from the moment it was made (coreapi::accessfiles), whatever
	   it holds now and whatever name it has, if any is left. */
	if (coreapi::accessfiles::held(st.st_dev, st.st_ino))
		return true;
	/* A file no name reaches any more goes out through no gate: what it
	   held cannot be told, and the kept set lets a file go once no name
	   reaches it. Asked after the set; a link count never comes back
	   from 0. */
	return fstat(fd, &st) != 0 || (st.st_nlink == 0 && !own_unnamed);
}

std::vector<std::string> keptPlaces()
{
	if (!keptOverride().empty())
		return keptOverride();
	std::vector<std::string> kept;
	kept.push_back(HTTPD_CONFIGDIR);
	kept.push_back("/proc");
	kept.push_back("/sys");
	kept.push_back("/dev");
	return kept;
}

void setKeptPlacesForTest(const std::vector<std::string> &places)
{
	keptOverride() = places;
}

bool inKeptPlace(int fd, bool config_ok)
{
	char link[64];
	snprintf(link, sizeof(link), "/proc/self/fd/%d", fd);
	char buf[PATH_MAX];
	const ssize_t n = readlink(link, buf, sizeof(buf) - 1);
	if (n <= 0)
		return true;
	buf[n] = '\0';
	std::string real(buf);
	// an unlinked file is named where it lay
	static const char deleted[] = " (deleted)";
	if (real.size() > sizeof(deleted) - 1 &&
	    real.compare(real.size() - (sizeof(deleted) - 1), sizeof(deleted) - 1, deleted) == 0)
		real.erase(real.size() - (sizeof(deleted) - 1));
	// a pipe, a socket or anything else that is no file of a directory
	if (real.empty() || real[0] != '/')
		return true;
	const std::vector<std::string> kept = keptPlaces();
	for (size_t i = 0; i < kept.size(); i++) {
		if (config_ok && i == 0)
			continue;
		const std::string k = realOf(kept[i]);
		if (!k.empty() && within(real, k))
			return true;
	}
	return false;
}

void setLookedForTest(void (*looked)(size_t name))
{
	lookedHook = looked;
}

bool namesProtected(const std::string &path)
{
	const std::vector<std::string> names = currentFiles();
	std::string dir, base;
	split(path, dir, base);
	struct stat in;
	if (stat(dir.c_str(), &in) != 0)
		return false;
	for (size_t i = 0; i < names.size(); i++) {
		std::string pdir, pbase;
		split(names[i], pdir, pbase);
		struct stat p;
		if (pbase == base && stat(pdir.c_str(), &p) == 0 && sameFile(p, in))
			return true;
	}
	return false;
}

} // namespace yprotect
