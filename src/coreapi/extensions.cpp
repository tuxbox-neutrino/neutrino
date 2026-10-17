/*
 * extensions.cpp - the web interface's extensions, installed from an archive
 *
 * Copyright (C) 2026 Thilo Graf
 *
 * License: GPL-2.0-or-later
 */

#include <config.h>

#include "coreapi/extensions.h"
#include "coreapi/base/errors.h"
#include "coreapi/shellcontract.h"
#include "coreapi/tarfile.h"

#include <algorithm>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <exception>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include <OpenThreads/Mutex>
#include <json/json.h>

#include <dirent.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <time.h>
#include <unistd.h>

#ifndef RENAME_EXCHANGE
#define RENAME_EXCHANGE (1 << 1)
#endif

namespace coreapi
{
namespace extensions
{

namespace internal
{
int script_timeout_ms = kScriptTimeoutMs;
const char *web_root = PRIVATE_HTTPDDIR;
const char *tmp_dir = "/tmp";
size_t max_members = 2000;
unsigned long long max_unpacked = 64ULL * 1024ULL * 1024ULL;
} // namespace internal

namespace
{

// One install or removal at a time: two at once would swap the same directory.
OpenThreads::Mutex &changing()
{
	static OpenThreads::Mutex m;
	return m;
}

const char kManifest[] = "manifest.json";

std::string root()
{
	return std::string(internal::web_root) + "/extensions";
}

std::string scriptsDir()
{
	return std::string(internal::web_root) + "/scripts";
}

bool endsWith(const std::string &s, const char *tail)
{
	const size_t n = std::strlen(tail);
	return s.size() >= n && s.compare(s.size() - n, n, tail) == 0;
}

bool plainUpload(const std::string &file)
{
	if (file.empty() || file.size() > 255 || file[0] == '.')
		return false;
	for (size_t i = 0; i < file.size(); ++i)
	{
		const char c = file[i];
		if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') ||
		      c == '.' || c == '-' || c == '_'))
			return false;
	}
	return endsWith(file, ".tar.gz") || endsWith(file, ".tgz") || endsWith(file, ".tar");
}

/* The scripts the pages ship themselves (data/web-ui/scripts), and the two
   they shipped once, the old file manager's and the old extension manager's:
   never an extension's, whatever an extension installed by that manager left
   in their place. */
bool pagesOwn(const std::string &n)
{
	static const char *const own[] = { "_Y_Globals.sh", "_Y_Library.sh", "Y_NAS.sh", "Y_Tools.sh", "Y_Extensions.sh" };
	for (size_t i = 0; i < sizeof(own) / sizeof(own[0]); ++i)
		if (n == own[i])
			return true;
	return false;
}

// A script name an earlier install copied beside the pages' own: letters, digits, dots, dashes and underscores, ending in .sh.
bool scriptName(const std::string &n)
{
	if (n.size() < 4 || n.size() > 255 || n[0] == '.' || !endsWith(n, ".sh"))
		return false;
	for (size_t i = 0; i < n.size(); ++i)
	{
		const char c = n[i];
		if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') ||
		      c == '.' || c == '-' || c == '_'))
			return false;
	}
	return true;
}

/* A script as a manifest names it and the route takes it, without .sh:
   letters, digits, dots, dashes and underscores, beginning with neither a dash,
   which a program reads as an option, nor a dot, which leaves . and .. out. */
bool scriptWord(const std::string &n)
{
	if (n.empty() || n.size() > 64 || n[0] == '-' || n[0] == '.')
		return false;
	for (size_t i = 0; i < n.size(); ++i)
	{
		const char c = n[i];
		if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') ||
		      c == '.' || c == '-' || c == '_'))
			return false;
	}
	return true;
}

// Far more than a manifest comes to, and the most scripts one may name.
const size_t kMaxManifest = 256 * 1024;
const size_t kMaxNamedScripts = 64;

std::string randomWord()
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
	char word[17];
	for (size_t k = 0; k < sizeof(raw); ++k)
		std::snprintf(word + 2 * k, 3, "%02x", raw[k]);
	return word;
}

void removeTree(const std::string &path)
{
	struct stat st;
	if (lstat(path.c_str(), &st) != 0)
		return;
	if (S_ISDIR(st.st_mode))
	{
		DIR *d = opendir(path.c_str());
		if (d != NULL)
		{
			struct dirent *e;
			while ((e = readdir(d)) != NULL)
			{
				if (std::strcmp(e->d_name, ".") == 0 || std::strcmp(e->d_name, "..") == 0)
					continue;
				removeTree(path + "/" + e->d_name);
			}
			closedir(d);
		}
		rmdir(path.c_str());
	}
	else
		unlink(path.c_str());
}

// A regular file opened for reading, not through a link; -1 otherwise.
int openRegular(const std::string &path, struct stat &st)
{
	const int fd = open(path.c_str(), O_RDONLY | O_NOFOLLOW | O_NONBLOCK | O_CLOEXEC);
	if (fd < 0)
		return -1;
	if (fstat(fd, &st) != 0 || !S_ISREG(st.st_mode))
	{
		close(fd);
		return -1;
	}
	return fd;
}

ssize_t readSome(int fd, char *buf, size_t n)
{
	ssize_t got;
	do
		got = read(fd, buf, n);
	while (got < 0 && errno == EINTR);
	return got;
}

// Whether two regular files hold the same bytes, read a piece at a time.
bool sameFile(const std::string &a, const std::string &b)
{
	struct stat sa, sb;
	const int fa = openRegular(a, sa);
	if (fa < 0)
		return false;
	const int fb = openRegular(b, sb);
	if (fb < 0)
	{
		close(fa);
		return false;
	}
	bool same = sa.st_size == sb.st_size;
	char ba[8192], bb[8192];
	while (same)
	{
		const ssize_t na = readSome(fa, ba, sizeof(ba));
		if (na <= 0)
		{
			same = na == 0 && readSome(fb, bb, 1) == 0;
			break;
		}
		size_t have = 0;
		while (same && have < (size_t) na)
		{
			const ssize_t nb = readSome(fb, bb + have, (size_t) na - have);
			same = nb > 0;
			if (same)
				have += (size_t) nb;
		}
		same = same && std::memcmp(ba, bb, (size_t) na) == 0;
	}
	close(fa);
	close(fb);
	return same;
}

bool writeAll(int fd, const std::string &data)
{
	size_t done = 0;
	while (done < data.size())
	{
		const ssize_t n = write(fd, data.data() + done, data.size() - done);
		if (n < 0 && errno == EINTR)
			continue;
		if (n <= 0)
			return false;
		done += (size_t) n;
	}
	return true;
}

bool isDirectory(const std::string &path)
{
	struct stat st;
	return lstat(path.c_str(), &st) == 0 && S_ISDIR(st.st_mode);
}

bool isRegular(const std::string &path)
{
	struct stat st;
	return lstat(path.c_str(), &st) == 0 && S_ISREG(st.st_mode);
}

/* The scripts in an installed extension directory's scripts/, by file name,
   sorted: those an earlier install would have copied beside the pages' own. */
std::vector<std::string> shippedScripts(const std::string &extdir)
{
	std::vector<std::string> out;
	DIR *d = opendir((extdir + "/scripts").c_str());
	if (d == NULL)
		return out;
	struct dirent *e;
	while ((e = readdir(d)) != NULL)
	{
		const std::string n = e->d_name;
		if (scriptName(n) && isRegular(extdir + "/scripts/" + n))
			out.push_back(n);
	}
	closedir(d);
	std::sort(out.begin(), out.end());
	return out;
}

/* Whether the pages' script of that name is still the copy extdir's own is;
   never one of the pages' own, nor anything that is not a regular file. */
bool stillOurs(const std::string &extdir, const std::string &name)
{
	return !pagesOwn(name) && sameFile(extdir + "/scripts/" + name, scriptsDir() + "/" + name);
}

/* The upload copied into a directory of this program's own under a name it
   chose: what is checked is what is unpacked, whatever happens to the upload's
   name afterwards. */
enum class Staged { Done, TooLarge, Broken };

Staged stage(const std::string &from, const std::string &to)
{
	const int in = open(from.c_str(), O_RDONLY | O_NOFOLLOW | O_NONBLOCK | O_CLOEXEC);
	if (in < 0)
		return Staged::Broken;
	struct stat st;
	if (fstat(in, &st) != 0 || !S_ISREG(st.st_mode))
	{
		close(in);
		return Staged::Broken;
	}
	const int out = open(to.c_str(), O_WRONLY | O_CREAT | O_EXCL | O_NOFOLLOW | O_CLOEXEC, 0600);
	if (out < 0)
	{
		close(in);
		return Staged::Broken;
	}
	Staged result = Staged::Done;
	unsigned long long total = 0;
	char buf[16384];
	for (;;)
	{
		const ssize_t n = read(in, buf, sizeof(buf));
		if (n < 0 && errno == EINTR)
			continue;
		if (n < 0)
		{
			result = Staged::Broken;
			break;
		}
		if (n == 0)
			break;
		total += (unsigned long long) n;
		if (total > kMaxArchiveBytes)
		{
			result = Staged::TooLarge;
			break;
		}
		if (!writeAll(out, std::string(buf, (size_t) n)))
		{
			result = Staged::Broken;
			break;
		}
	}
	close(in);
	if (close(out) != 0 && result == Staged::Done)
		result = Staged::Broken;
	return result;
}

Failure refused(const std::string &why)
{
	return fail(Status::InvalidArgument, ErrorCode::ExtensionRefused, why);
}

Failure diskRefused(const std::string &why)
{
	return fail(Status::Internal, ErrorCode::ExtensionRefused, why);
}

// The parts of a name as the archive gives it; false for one that climbs, is
// absolute, or has a . or an empty part (a trailing separator aside).
bool parts(std::string name, bool directory, std::vector<std::string> &out)
{
	out.clear();
	if (directory && name.size() > 1 && name[name.size() - 1] == '/')
		name.erase(name.size() - 1);
	if (name.empty() || name[0] == '/')
		return false;
	for (size_t i = 0; i < name.size(); ++i)
		if ((unsigned char) name[i] < 0x20 || name[i] == 0x7f)
			return false;
	size_t from = 0;
	while (from <= name.size())
	{
		size_t end = name.find('/', from);
		if (end == std::string::npos)
			end = name.size();
		const std::string part = name.substr(from, end - from);
		if (part.empty() || part == "." || part == "..")
			return false;
		out.push_back(part);
		from = end + 1;
	}
	return true;
}

/* Makes and opens the directories of path below base, one at a time and
   without following a link; the directory a file of path goes into, or -1. */
int walkDown(int base, const std::vector<std::string> &path, bool directory, std::map<std::string, int> &dirs)
{
	int fd = base;
	std::string at;
	const size_t walk = directory ? path.size() : path.size() - 1;
	for (size_t i = 0; i < walk; ++i)
	{
		at += "/" + path[i];
		std::map<std::string, int>::iterator known = dirs.find(at);
		if (known != dirs.end())
		{
			fd = known->second;
			continue;
		}
		if (mkdirat(fd, path[i].c_str(), 0755) != 0 && errno != EEXIST)
			return -1;
		const int sub = openat(fd, path[i].c_str(), O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC);
		if (sub < 0)
			return -1;
		dirs[at] = sub;
		fd = sub;
	}
	return fd;
}

/* Whether JSON text carries a slash outside its strings: a comment, which a
   browser refuses and the parser here lets through inside an object, or
   nothing JSON knows at all. */
bool carriesComment(const std::string &text)
{
	bool in_string = false;
	for (size_t i = 0; i < text.size(); ++i)
	{
		const char c = text[i];
		if (in_string)
		{
			if (c == '\\')
				++i;
			else if (c == '"')
				in_string = false;
		}
		else if (c == '"')
			in_string = true;
		else if (c == '/')
			return true;
	}
	return false;
}

/* What a manifest names under "scripts", in its order, each once; nothing for
   a manifest without the key. False, with why, for a manifest that is not a
   JSON object as the pages' own parser reads one, or whose "scripts" is not a
   list of names scriptWord takes. Read whole, from a regular file and not
   through a link. */
bool manifestScripts(const std::string &path, std::vector<std::string> &out, std::string &why)
{
	out.clear();
	struct stat st;
	const int fd = openRegular(path, st);
	if (fd < 0)
	{
		why = "its manifest.json cannot be read";
		return false;
	}
	std::string text;
	char buf[8192];
	ssize_t n = 0;
	while (text.size() <= kMaxManifest && (n = readSome(fd, buf, sizeof(buf))) > 0)
		text.append(buf, (size_t) n);
	close(fd);
	if (n < 0)
	{
		why = "its manifest.json cannot be read";
		return false;
	}
	if (text.size() > kMaxManifest)
	{
		why = "its manifest.json is larger than 256 KiB";
		return false;
	}
	// a mark of the encoding at its start, which a browser reads past
	if (text.compare(0, 3, "\xef\xbb\xbf") == 0)
		text.erase(0, 3);

	/* Strict, near what a browser reads: no single quotes, no trailing
	   commas, nothing behind the object; a key given twice is the last one,
	   as there. */
	Json::CharReaderBuilder builder;
	Json::CharReaderBuilder::strictMode(&builder.settings_);
	builder.settings_["rejectDupKeys"] = false;
	builder.settings_["stackLimit"] = 64;
	std::unique_ptr<Json::CharReader> reader(builder.newCharReader());
	Json::Value root;
	std::string errors;
	bool parsed = false;
	try
	{
		parsed = reader->parse(text.data(), text.data() + text.size(), &root, &errors);
	}
	catch (const std::exception &)
	{
		parsed = false;
	}
	if (!parsed || !root.isObject() || carriesComment(text))
	{
		why = "its manifest.json is not a JSON object";
		return false;
	}
	if (!root.isMember("scripts"))
		return true;
	const Json::Value &named = root["scripts"];
	if (!named.isArray())
	{
		why = "its manifest names its scripts in something other than a list";
		return false;
	}
	if (named.size() > kMaxNamedScripts)
	{
		why = "its manifest names more than 64 scripts";
		return false;
	}
	for (Json::ArrayIndex i = 0; i < named.size(); ++i)
	{
		if (!named[i].isString() || !scriptWord(named[i].asString()))
		{
			why = "its manifest names a script other than by up to 64 letters, digits, dots, dashes and "
			      "underscores, beginning with a letter, a digit or an underscore";
			return false;
		}
		const std::string s = named[i].asString();
		if (std::find(out.begin(), out.end(), s) == out.end())
			out.push_back(s);
	}
	return true;
}

// The position an argument arrives at, as the name it is handed over under.
std::string positionName(size_t n)
{
	char buf[24];
	std::snprintf(buf, sizeof(buf), "%lu", (unsigned long) n);
	return std::string(buf);
}

/* The new directory takes the installed one's place in one step where the
   kernel and the filesystem can exchange two names, so a page loading from it
   never finds it missing; with two renames otherwise. The installed one ends
   up at from, or at aside where the two renames had to be used. */
bool swapIn(const std::string &from, const std::string &target, const std::string &aside, bool had)
{
	if (!had)
	{
		// a link or a file in the way is no extension: replaced, never followed
		struct stat st;
		if (lstat(target.c_str(), &st) == 0 && !S_ISDIR(st.st_mode) && unlink(target.c_str()) != 0)
			return false;
		return rename(from.c_str(), target.c_str()) == 0;
	}
#ifdef SYS_renameat2
	if (syscall(SYS_renameat2, AT_FDCWD, from.c_str(), AT_FDCWD, target.c_str(), RENAME_EXCHANGE) == 0)
		return true;
	if (errno != ENOSYS && errno != EINVAL)
		return false;
#endif
	if (rename(target.c_str(), aside.c_str()) != 0)
		return false;
	if (rename(from.c_str(), target.c_str()) != 0)
	{
		rename(aside.c_str(), target.c_str());
		return false;
	}
	return true;
}

} // namespace

void internal::holdTurnForTest(bool hold)
{
	if (hold)
		changing().lock();
	else
		changing().unlock();
}

bool extensionName(const std::string &name)
{
	if (name.empty() || name.size() > 64)
		return false;
	for (size_t i = 0; i < name.size(); ++i)
	{
		const char c = name[i];
		const bool alnum = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9');
		if (!alnum && (i == 0 || (c != '-' && c != '_')))
			return false;
	}
	return true;
}

Result<std::vector<Extension> > list()
{
	std::vector<Extension> out;
	DIR *d = opendir(root().c_str());
	if (d == NULL)
		return Result<std::vector<Extension> >::success(out);
	struct dirent *e;
	while ((e = readdir(d)) != NULL)
	{
		const std::string n = e->d_name;
		const std::string dir = root() + "/" + n;
		if (!extensionName(n) || !isDirectory(dir) || !isRegular(dir + "/" + kManifest))
			continue;
		Extension x;
		x.dir = n;
		// a manifest no install would take names none
		std::vector<std::string> named;
		std::string why;
		if (manifestScripts(dir + "/" + kManifest, named, why))
		{
			for (size_t i = 0; i < named.size(); ++i)
				if (isDirectory(dir + "/scripts") && isRegular(dir + "/scripts/" + named[i] + ".sh"))
					x.scripts.push_back(named[i]);
		}
		out.push_back(x);
	}
	closedir(d);
	struct ByName
	{
		bool operator()(const Extension &a, const Extension &b) const { return a.dir < b.dir; }
	};
	std::sort(out.begin(), out.end(), ByName());
	return Result<std::vector<Extension> >::success(out);
}

Result<Extension> install(const std::string &file)
{
	if (!plainUpload(file))
		return fail(Status::InvalidArgument, ErrorCode::BadName,
			    "the archive is a plain name in the temporary directory, of letters, digits, dots, "
			    "dashes and underscores, ending in .tar.gz, .tgz or .tar");
	const std::string upload = std::string(internal::tmp_dir) + "/" + file;
	struct stat st;
	if (lstat(upload.c_str(), &st) != 0 || !S_ISREG(st.st_mode))
		return fail(Status::NotFound, ErrorCode::NoSuchName, "there is no uploaded archive of that name");

	// the upload goes whatever comes of it, as it does once taken over
	if (changing().trylock() != 0)
	{
		unlink(upload.c_str());
		return fail(Status::Conflict, ErrorCode::ExtensionRefused, "another extension is being installed or removed");
	}
	struct Release
	{
		~Release() { changing().unlock(); }
	} release;

	// the upload taken over into a directory of this program's own
	std::string pattern = std::string(internal::tmp_dir) + "/extension.XXXXXX";
	std::vector<char> buf(pattern.begin(), pattern.end());
	buf.push_back('\0');
	if (mkdtemp(&buf[0]) == NULL)
	{
		unlink(upload.c_str());
		return diskRefused("no directory to check the archive in");
	}
	const std::string work(&buf[0]);
	struct Work
	{
		std::string dir;
		~Work() { removeTree(dir); }
	} cleanup = { work };
	const std::string archive = work + (endsWith(file, ".tar") ? "/extension.tar" : "/extension.tar.gz");
	const Staged staged = stage(upload, archive);
	unlink(upload.c_str());
	if (staged == Staged::TooLarge)
		return refused("the archive is larger than 16 MiB");
	if (staged != Staged::Done)
		return diskRefused("the archive could not be taken over");

	std::vector<tarfile::Member> members;
	std::string why;
	if (!tarfile::readMembers(archive, internal::max_members, internal::max_unpacked, "an extension", members, why))
		return refused(why);
	if (members.empty())
		return refused("the archive is empty");

	// one top-level directory, a manifest in it, nothing but files and directories
	std::string name;
	bool manifest = false;
	for (size_t i = 0; i < members.size(); ++i)
	{
		const tarfile::Member &m = members[i];
		if (m.type != '0' && m.type != '7' && m.type != '5')
			return refused("the archive holds a link or a device; an extension is files and directories");
		const bool directory = m.type == '5';
		for (size_t k = 0; k < m.names.size(); ++k)
		{
			std::vector<std::string> p;
			if (!parts(m.names[k], directory, p))
				return refused("the archive holds an absolute name, one that climbs with .., one with a . "
					       "or an empty part, or one with a control character: " + m.names[k]);
			if (name.empty())
				name = p[0];
			if (p[0] != name)
				return refused("the archive holds more than one top-level directory");
		}
		std::vector<std::string> p;
		parts(m.path, directory, p);
		if (p.size() == 1 && !directory)
			return refused("the archive holds a file beside its directory: " + m.path);
		if (p.size() == 2 && !directory && p[1] == kManifest)
			manifest = true;
	}
	/* The archive at odds with itself: a file given twice, one name a file and
	   a directory, or an entry below one of its files. Refused here and not
	   found half way through the unpacking. */
	std::map<std::string, bool> kinds;
	for (size_t i = 0; i < members.size(); ++i)
	{
		const bool directory = members[i].type == '5';
		std::vector<std::string> p;
		parts(members[i].path, directory, p);
		std::string at;
		for (size_t k = 0; k < p.size(); ++k)
		{
			at += (k ? "/" : "") + p[k];
			const bool here_dir = directory || k + 1 < p.size();
			std::map<std::string, bool>::iterator known = kinds.find(at);
			if (known == kinds.end())
			{
				kinds[at] = here_dir;
				continue;
			}
			if (known->second != here_dir)
				return refused("the archive holds " + at + " as a file and as a directory");
			if (!here_dir)
				return refused("the archive holds " + at + " twice");
		}
	}
	if (!extensionName(name))
		return refused("the archive's directory is not named with letters, digits, dashes and underscores, "
			       "beginning with a letter or a digit: " + name);
	if (!manifest)
		return refused("the archive holds no " + std::string(kManifest) + " in its directory");

	/* Copies an earlier install of it left beside the pages' own scripts, as
	   installs once did, still as that install left them. */
	const std::string target = root() + "/" + name;
	const bool had = isDirectory(target);
	std::set<std::string> owned;
	if (had)
	{
		const std::vector<std::string> before = shippedScripts(target);
		for (size_t i = 0; i < before.size(); ++i)
			if (stillOurs(target, before[i]))
				owned.insert(before[i]);
	}

	// unpacked beside the extensions under a name of this install's own
	if (mkdir(root().c_str(), 0755) != 0 && errno != EEXIST)
		return diskRefused("the extensions directory could not be made");
	const std::string word = randomWord();
	const std::string fresh = root() + "/.install-" + word;
	if (mkdir(fresh.c_str(), 0755) != 0)
		return diskRefused("no directory to unpack the extension in");
	struct Fresh
	{
		std::string dir;
		~Fresh() { removeTree(dir); }
	} fresh_cleanup = { fresh };
	const int base = open(fresh.c_str(), O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC);
	if (base < 0)
		return diskRefused("no directory to unpack the extension in");
	std::map<std::string, int> dirs;
	bool ok = true;
	why = "the archive could not be read again";
	{
		tarfile::Reader tar(archive, internal::max_members);
		ok = tar.opened();
		for (size_t i = 0; ok && i < members.size(); ++i)
		{
			tarfile::Member m;
			ok = tar.next(m) == tarfile::Reader::Entry && m.path == members[i].path &&
			     m.type == members[i].type && m.size == members[i].size;
			if (!ok)
				break;
			const bool directory = m.type == '5';
			std::vector<std::string> p;
			parts(m.path, directory, p);
			const int dir = walkDown(base, p, directory, dirs);
			if (dir < 0)
			{
				why = "a directory of the extension could not be made: " + m.path;
				ok = false;
				break;
			}
			if (directory)
			{
				ok = tar.skip(m);
				continue;
			}
			// a name given twice is refused rather than one of them guessed
			const int fd = openat(dir, p[p.size() - 1].c_str(),
					      O_WRONLY | O_CREAT | O_EXCL | O_NOFOLLOW | O_CLOEXEC, 0644);
			if (fd < 0)
			{
				why = "a file of the extension could not be written: " + m.path;
				ok = false;
				break;
			}
			ok = tar.copy(m, fd) && fchmod(fd, 0644) == 0;
			ok = close(fd) == 0 && ok;
			if (!ok)
				why = "a file of the extension could not be written: " + m.path;
		}
	}
	for (std::map<std::string, int>::iterator d = dirs.begin(); d != dirs.end(); ++d)
		close(d->second);
	close(base);
	if (!ok)
		return diskRefused(why);

	/* The scripts its manifest names, read from what was unpacked: each a
	   regular file in its scripts/, and the only files made executable. */
	const std::string unpacked = fresh + "/" + name;
	std::vector<std::string> scripts;
	if (!manifestScripts(unpacked + "/" + kManifest, scripts, why))
		return refused(why);
	for (size_t i = 0; i < scripts.size(); ++i)
	{
		const std::string script = unpacked + "/scripts/" + scripts[i] + ".sh";
		if (!isDirectory(unpacked + "/scripts") || !isRegular(script))
			return refused("its manifest names the script " + scripts[i] + ", and the archive holds no scripts/" +
				       scripts[i] + ".sh");
		if (chmod(script.c_str(), 0755) != 0)
			return diskRefused("a script of the extension could not be made executable: " + scripts[i]);
	}
	sync();

	// it takes the place of the installed one only now, whole
	const std::string aside = root() + "/.old-" + word;
	if (!swapIn(unpacked, target, aside, had))
		return diskRefused("the extension could not be put in place");
	removeTree(aside);

	// the copies of an earlier install go, nothing runs them any more
	for (std::set<std::string>::iterator o = owned.begin(); o != owned.end(); ++o)
		unlink((scriptsDir() + "/" + *o).c_str());

	Extension x;
	x.dir = name;
	x.scripts = scripts;
	return Result<Extension>::success(x);
}

Result<void> remove(const std::string &dir)
{
	if (!extensionName(dir))
		return fail(Status::InvalidArgument, ErrorCode::BadName,
			    "an extension is named with letters, digits, dashes and underscores");
	if (changing().trylock() != 0)
		return fail(Status::Conflict, ErrorCode::ExtensionRefused, "another extension is being installed or removed");
	struct Release
	{
		~Release() { changing().unlock(); }
	} release;

	const std::string target = root() + "/" + dir;
	if (!isDirectory(target))
		return fail(Status::NotFound, ErrorCode::NoSuchName, "no extension of that name is installed");

	// out of the way first, so a page never finds it half removed, then its scripts
	const std::string gone = root() + "/.remove-" + randomWord();
	if (rename(target.c_str(), gone.c_str()) != 0)
		return fail(Status::Internal, ErrorCode::ExtensionRefused, "the extension could not be taken away");
	const std::vector<std::string> scripts = shippedScripts(gone);
	for (size_t i = 0; i < scripts.size(); ++i)
		if (stillOurs(gone, scripts[i]))
			unlink((scriptsDir() + "/" + scripts[i]).c_str());
	removeTree(gone);
	return ok();
}

Result<std::string> runScript(const std::string &dir, const std::string &name,
			      const std::vector<std::string> &args)
{
	if (!extensionName(dir))
		return fail(Status::InvalidArgument, ErrorCode::BadName,
			    "an extension is named with letters, digits, dashes and underscores");
	if (!scriptWord(name))
		return fail(Status::InvalidArgument, ErrorCode::BadName,
			    "a script is named with up to 64 letters, digits, dots, dashes and underscores, "
			    "beginning with a letter, a digit or an underscore");

	const std::string extdir = root() + "/" + dir;
	if (!isDirectory(extdir) || !isRegular(extdir + "/" + kManifest))
		return fail(Status::NotFound, ErrorCode::NoSuchName, "no extension of that name is installed");
	std::vector<std::string> named;
	std::string why;
	if (!manifestScripts(extdir + "/" + kManifest, named, why) ||
	    std::find(named.begin(), named.end(), name) == named.end())
		return fail(Status::NotFound, ErrorCode::ScriptMissing,
			    "the extension's manifest names no script of that name");

	/* Its own scripts/ and the file in it, neither a link: what runs is in the
	   extension's directory and nowhere else. */
	const std::string script = extdir + "/scripts/" + name + ".sh";
	if (!isDirectory(extdir + "/scripts") || !isRegular(script))
		return fail(Status::NotFound, ErrorCode::ScriptMissing, "the extension holds no script of that name");

	// one declared position per argument, in the order they came
	std::vector<std::string> names;
	names.reserve(args.size());
	std::vector<Param> params;
	params.reserve(args.size());
	std::vector<std::pair<std::string, std::string> > given;
	given.reserve(args.size());
	for (size_t i = 0; i < args.size(); ++i)
	{
		names.push_back(positionName(i + 1));
		Param p;
		p.name = names[i].c_str();
		p.type = ParamType::String;
		p.required = false;
		p.doc = "";
		p.min = 0;
		p.max = (long) kMaxScriptArgument;
		p.values = NULL;
		p.root = NULL;
		params.push_back(p);
		given.push_back(std::make_pair(names[i], args[i]));
	}

	ShellCommand cmd;
	cmd.script = script.c_str();
	cmd.verb = NULL;
	cmd.params = params.empty() ? NULL : &params[0];
	cmd.param_count = params.size();
	cmd.output = OutputFormat::Raw;
	cmd.timeout_ms = internal::script_timeout_ms;
	cmd.max_output = kMaxScriptOutput;
	// an argument that would be read as an option is refused, not let through
	cmd.end_options = false;
	return shell::run(cmd, given);
}

} // namespace extensions
} // namespace coreapi
