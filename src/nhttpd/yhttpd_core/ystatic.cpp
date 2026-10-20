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

RawPlaceTest rawPlaceTest = NULL;

// name with every link followed, or empty where that cannot be said
std::string realOf(const std::string &name)
{
	char buf[PATH_MAX];
	if (name.empty() || realpath(name.c_str(), buf) == NULL)
		return std::string();
	return buf;
}

// whether the name has a .. segment, which would step out of the folder it
// is looked for in
bool stepsOut(const std::string &name)
{
	size_t at = 0;
	while (at <= name.size()) {
		const size_t slash = name.find('/', at);
		const size_t end = slash == std::string::npos ? name.size() : slash;
		if (end - at == 2 && name.compare(at, 2, "..") == 0)
			return true;
		if (slash == std::string::npos)
			break;
		at = slash + 1;
	}
	return false;
}

// whether the real path real lies below the folder dir, taken by its own
// real path; an empty folder or / holds nothing here
bool insideDir(const std::string &real, const std::string &dir)
{
	const std::string d = realOf(dir);
	if (d.empty() || d == "/")
		return false;
	return real.size() > d.size() && real.compare(0, d.size(), d) == 0 && real[d.size()] == '/';
}

/* A file outside the web folders that a request may read by its own name:
   one in the hosted folder, which the server serves as a web folder of its
   own under /hosted/, or one the program's test lets through. */
bool rawAllowed(CyhookHandler *hh, const std::string &real)
{
	if (real.empty())
		return false;
	if (insideDir(real, hh->WebserverConfigList["WebsiteMain.hosted_directory"]))
		return true;
	return rawPlaceTest && rawPlaceTest(real);
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
	/* A name that steps out of the folder it is looked for in names no file,
	   and neither does one with a NUL, which would cut it short for open()
	   while its type is still read from its end (x%00.png). */
	if (name.find('\0') != std::string::npos || stepsOut(name))
		return Target();

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
		// the real path is what is tested and what is opened, so a link
		// that leads out of the places does not take the request along;
		// rawOpenedAllowed tests the opened file once more
		const std::string real = realOf(name);
		if (rawAllowed(hh, real)) {
			t.origin = Target::RAW;
			t.file = real;
		}
	}
#endif
	return t;
}

void setRawPlaceTest(RawPlaceTest test)
{
	rawPlaceTest = test;
}

bool rawOpenedAllowed(CyhookHandler *hh, int fd)
{
	struct stat st;
	if (fd < 0 || fstat(fd, &st) != 0 || !S_ISREG(st.st_mode) || st.st_nlink == 0)
		return false;
	char link[48];
	char opened[PATH_MAX];
	snprintf(link, sizeof(link), "/proc/self/fd/%d", fd);
	const ssize_t n = readlink(link, opened, sizeof(opened) - 1);
	if (n <= 0)
		return false;
	opened[n] = '\0';
	return rawAllowed(hh, opened);
}

bool rawPlaceAllowed(const std::string &real, const std::vector<std::string> &dirs,
		     const std::string &tmp)
{
	if (real.empty() || real[0] != '/')
		return false;
	for (size_t i = 0; i < dirs.size(); i++)
		if (insideDir(real, dirs[i]))
			return true;
	const std::string t = realOf(tmp);
	const size_t slash = real.rfind('/');
	return !t.empty() && t != "/" && slash == t.size() && real.compare(0, t.size(), t) == 0
		&& endsWithNoCase(real, ".png") && real.size() > t.size() + 1 + 4;
}

Target resolve(CyhookHandler *hh)
{
	Target t = resolveName(hh, hh->UrlData["path"], hh->UrlData["filename"]);
	if (t.origin != Target::NONE)
		return t;
	if (hh->WebserverConfigList["WebsiteMain.spa_fallback"] != "true")
		return t;
	if (hh->Method != M_GET && hh->Method != M_HEAD)
		return t;
	// a page path names no file type in its last segment; analyzeURL has
	// already turned one ending in / into index.html, so its fileext
	// does not tell
	const std::string &url = hh->UrlData["url"];
	const size_t last = url.rfind('/');
	if (url.find('.', last == std::string::npos ? 0 : last) != std::string::npos
			|| inReservedSpace(url))
		return t;
	Target index = resolveName(hh, "/", "index.html");
	index.fallback = index.origin != Target::NONE;
	return index;
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

bool mayBeKept(CyhookHandler *hh)
{
	const std::string &mime = hh->ResponseMimeType;
	if (mime.compare(0, 9, "text/html") == 0)
		return false;
	// by the type the request's name was given, and by the extension it was
	// asked by: mod_sendfile keeps that one, since an answer sent from its .gz
	// carries gz as its extension, and a configured type may say text/plain
	const std::string asked = hh->HookVarList["sendfile.ext"];
	const std::string ext = asked.empty() ? hh->UrlData["fileext"] : asked;
	const bool code = mime.compare(0, 8, "text/css") == 0
		|| mime.compare(0, 22, "application/javascript") == 0
		|| mime.compare(0, 15, "text/javascript") == 0
		|| ext == "css" || ext == "js" || ext == "mjs";
	return !(code && hh->ParamList["v"].empty());
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

bool ownLoginPage(CyhookHandler *hh)
{
	const int fd = openPublic(hh, resolveName(hh, "/", "login.html"));
	if (fd < 0)
		return false;
	close(fd);
	return true;
}

namespace
{
// in any case, also compressed, as templateOrScript reads names
bool htmlPage(const std::string &name)
{
	std::string n = name;
	if (endsWithNoCase(n, ".gz"))
		n = n.substr(0, n.size() - 3);
	return endsWithNoCase(n, ".html") || endsWithNoCase(n, ".htm");
}
} // namespace

namespace
{
// The web directory carries a sign-in page of its own: this interface, as
// opposed to another one in its place. Asked of the directory itself, so a
// copy in the override directory, which never goes out openly, leaves the
// pages behind the login rather than letting them out.
bool hasOwnSignIn(CyhookHandler *hh)
{
	const std::string core = hh->WebserverConfigList["WebsiteMain.directory"];
	struct stat st;
	return (stat((core + "/login.html").c_str(), &st) == 0 && S_ISREG(st.st_mode))
		|| (stat((core + "/login.html.gz").c_str(), &st) == 0 && S_ISREG(st.st_mode));
}

// Whether fd holds the file the sign-in page goes out from, by what was
// opened and not by a name, so a link in either place cannot tell it apart.
bool isSignIn(CyhookHandler *hh, int fd)
{
	const int own = openPublic(hh, resolveName(hh, "/", "login.html"));
	if (own < 0)
		return false;
	struct stat a, b;
	const bool same = fstat(fd, &a) == 0 && fstat(own, &b) == 0
		&& a.st_dev == b.st_dev && a.st_ino == b.st_ino;
	close(own);
	return same;
}
} // namespace

bool pageNeedsLogin(CyhookHandler *hh, const Target &t, int fd)
{
	std::string below;
	if (!ownOpened(hh, fd, below))
		return true;	// what it is cannot be told: behind the login
	if (!htmlPage(t.file) && !htmlPage(below))
		return false;
	if (!hasOwnSignIn(hh))
		return false;
	return !isSignIn(hh, fd);
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
