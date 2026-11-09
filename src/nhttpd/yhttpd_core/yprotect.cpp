//=============================================================================
// YHTTPD
// yprotect : the files that decide who may reach the box, which no answer
// hands out and no log is written into
//=============================================================================

#include <pthread.h>
#include <sys/stat.h>

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

// st is one of the files, as each name stands now
bool isOne(const struct stat &st, const std::vector<std::string> &names)
{
	for (size_t i = 0; i < names.size(); i++) {
		struct stat p;
		if (stat(names[i].c_str(), &p) == 0 && sameFile(p, st))
			return true;
	}
	return false;
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

void setFiles(const std::vector<std::string> &names)
{
	pthread_mutex_lock(&filesLock);
	files() = names;
	pthread_mutex_unlock(&filesLock);
}

bool isProtected(int fd)
{
	struct stat st;
	if (fd < 0 || fstat(fd, &st) != 0)
		return true;
	return isOne(st, currentFiles());
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
