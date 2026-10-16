//=============================================================================
// YHTTPD
// ystatic : where a request for a file ends up, and whether it may be
// answered without a login
//=============================================================================

#include <cerrno>
#include <climits>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <strings.h>

#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

#include "yconfig.h"
#include "yhook.h"
#include "ystatic.h"

namespace ystatic
{

namespace
{

bool readable(const std::string &name)
{
	return access(name.c_str(), R_OK) == 0;
}

bool endsWithNoCase(const std::string &s, const char *tail)
{
	const size_t n = strlen(tail);
	return s.size() >= n && strcasecmp(s.c_str() + s.size() - n, tail) == 0;
}

// Opens name, without waiting on a FIFO, and answers the descriptor when
// ownOpened accepts it; else -1. below as there.
int openOwnFile(CyhookHandler *hh, const std::string &name, std::string &below)
{
	const int fd = open(name.c_str(), O_RDONLY | O_CLOEXEC | O_NONBLOCK);
	if (fd < 0)
		return -1;
	if (!ownOpened(hh, fd, below)) {
		close(fd);
		return -1;
	}
	// a regular file reads the same either way; the sender gets it as
	// any other file
	fcntl(fd, F_SETFL, fcntl(fd, F_GETFL) & ~O_NONBLOCK);
	return fd;
}

} // namespace

Target resolveName(CyhookHandler *hh, const std::string &path, const std::string &filename)
{
	std::string name = path;
	if (name.empty() || name[name.length() - 1] != '/')
		name += "/";
	name += filename;

	const std::string over = hh->WebserverConfigList["WebsiteMain.override_directory"];
	const std::string core = hh->WebserverConfigList["WebsiteMain.directory"];
	Target t;
	if (readable(over + name)) {
		t.origin = Target::OVERRIDE;
		t.file = over + name;
	} else if (readable(over + name + ".gz")) {
		t.origin = Target::OVERRIDE;
		t.file = over + name + ".gz";
	} else if (readable(core + name)) {
		t.origin = Target::CORE;
		t.file = core + name;
	} else if (readable(core + name + ".gz")) {
		t.origin = Target::CORE;
		t.file = core + name + ".gz";
	}
#ifdef Y_CONFIG_FEATUE_SENDFILE_CAN_ACCESS_ALL
	else if (readable(name)) {
		t.origin = Target::RAW;
		t.file = name;
	}
#endif
	return t;
}

Target resolve(CyhookHandler *hh)
{
	return resolveName(hh, hh->UrlData["path"], hh->UrlData["filename"]);
}

bool inReservedSpace(const std::string &url)
{
	static const char *const spaces[] = {
		"/api", "/control", "/cgi-bin", "/fb", "/y", "/hosted", "/extensions",
		"/login", "/logout"
	};
	for (size_t i = 0; i < sizeof(spaces) / sizeof(spaces[0]); i++) {
		const size_t n = strlen(spaces[i]);
		if (url.compare(0, n, spaces[i]) == 0 && (url.size() == n || url[n] == '/'))
			return true;
	}
	return false;
}

bool templateOrScript(const std::string &name)
{
	std::string n = name;
	if (endsWithNoCase(n, ".gz"))
		n.erase(n.size() - 3);
	return endsWithNoCase(n, ".yhtm") || endsWithNoCase(n, ".yhtml")
		|| endsWithNoCase(n, ".yjs") || endsWithNoCase(n, ".ysh")
		|| endsWithNoCase(n, ".sh");
}

bool ownOpened(CyhookHandler *hh, int fd, std::string &below)
{
	const std::string core = hh->WebserverConfigList["WebsiteMain.directory"];
	char root[PATH_MAX];
	if (core.empty() || realpath(core.c_str(), root) == NULL)
		return false;

	struct stat st;
	if (fstat(fd, &st) != 0 || !S_ISREG(st.st_mode) || st.st_nlink == 0)
		return false;
	char link[48];
	char opened[PATH_MAX];
	snprintf(link, sizeof(link), "/proc/self/fd/%d", fd);
	const ssize_t n = readlink(link, opened, sizeof(opened) - 1);
	if (n <= 0)
		return false;
	opened[n] = '\0';
	const std::string real(opened);
	// unlinked after the fstat: the name no longer tells what the file is
	static const std::string deleted(" (deleted)");
	if (real.size() >= deleted.size()
			&& real.compare(real.size() - deleted.size(), deleted.size(), deleted) == 0)
		return false;

	std::string base(root);
	if (base[base.size() - 1] != '/')
		base += "/";
	if (real.size() <= base.size()
			|| real.compare(0, base.size(), base) != 0
			|| real.compare(base.size(), 11, "extensions/") == 0)
		return false;
	below = real.substr(base.size());
	return true;
}

int openLoginPage(CyhookHandler *hh)
{
	if (resolveName(hh, "/", "login.yhtm").origin != Target::CORE)
		return -1;
	// the file itself, not a link to another page or to a compressed copy
	std::string below;
	const int fd = openOwnFile(hh, hh->WebserverConfigList["WebsiteMain.directory"] + "/login.yhtm", below);
	if (fd >= 0 && below != "login.yhtm") {
		close(fd);
		return -1;
	}
	return fd;
}

bool ownLoginPage(CyhookHandler *hh)
{
	const int fd = openLoginPage(hh);
	if (fd < 0)
		return false;
	close(fd);
	return true;
}

int openPublic(CyhookHandler *hh, const Target &t)
{
	if (t.origin != Target::CORE || templateOrScript(t.file))
		return -1;
	std::string below;
	const int fd = openOwnFile(hh, t.file, below);
	if (fd >= 0 && templateOrScript(below)) {
		close(fd);
		return -1;
	}
	return fd;
}

std::string reparseSafe(const std::string &name)
{
	std::string out;
	for (size_t i = 0; i < name.size(); i++) {
		const char c = name[i];
		if (c == '%')
			out += "%25";
		else if (c == '+')
			out += "%2B";
		else if (c == '?')
			out += "%3F";
		else
			out += c;
	}
	return out;
}

} // namespace ystatic
