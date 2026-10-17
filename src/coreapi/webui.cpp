/*
 * webui.cpp - the settings the web interface keeps of its own
 *
 * Copyright (C) 2026 Thilo Graf
 *
 * License: GPL-2.0-or-later
 */

#include <config.h>

#include "coreapi/webui.h"

#include "coreapi/base/errors.h"

#include <configfile.h>

#include <OpenThreads/Mutex>

#include <cerrno>
#include <cstdio>
#include <cstring>
#include <utility>

#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#include "coreapi/base/settingswrites.h"

namespace coreapi
{
namespace webui
{

namespace internal
{
const char *config_path = CONFIGDIR "/webui.conf";
const char *former_path = CONFIGDIR "/yWeb.conf";
} // namespace internal

namespace
{

OpenThreads::Mutex &fileMutex()
{
	static OpenThreads::Mutex *m = new OpenThreads::Mutex;
	return *m;
}

bool endsWith(const std::string &s, const char *tail)
{
	const size_t n = strlen(tail);
	return s.size() >= n && s.compare(s.size() - n, n, tail) == 0;
}

bool isLetter(char c)
{
	return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}

bool keyOk(const std::string &key)
{
	if (key.empty() || key.size() > kMaxKeyLength || !isLetter(key[0]))
		return false;
	for (size_t i = 1; i < key.size(); ++i)
	{
		const char c = key[i];
		// An extension's directory name goes into its keys, and those carry dots and dashes.
		if (!isLetter(c) && !(c >= '0' && c <= '9') && c != '_' && c != '.' && c != '-')
			return false;
	}
	return true;
}

/* A byte below a space would end the line the file keeps the value on, or
   carry something no page means to show. The old pages put these values into
   their markup and their scripts as they stand, through a template engine whose
   calls begin with {= and end with =}: a value is kept clear of those markers and
   of the characters that would end an attribute, open a tag or close a string
   there. A number sign starts a comment where the file is read, so the value
   would come back cut short. */
bool valueOk(const std::string &value)
{
	for (size_t i = 0; i < value.size(); ++i)
	{
		const unsigned char c = (unsigned char) value[i];
		if (c < 0x20 || c == 0x7f || strchr("<>\"'`\\#", c) != NULL)
			return false;
	}
	return value.find("{=") == std::string::npos && value.find("=}") == std::string::npos;
}

bool writeAll(int fd, const std::string &text)
{
	size_t done = 0;
	while (done < text.size())
	{
		const ssize_t n = write(fd, text.data() + done, text.size() - done);
		if (n < 0 && errno == EINTR)
			continue;
		if (n <= 0)
			return false;
		done += (size_t) n;
	}
	return true;
}

/* The whole file written beside the old one, synced, and put in its place in
   one step, then the directory synced, so a power cut leaves one file or the
   other and a full disc leaves the old one. Every step is checked: the reader
   this file is written for answers success once the temporary file opened. */
bool save(const Settings &all)
{
	// settings put back from an archive wait for the restart that reads them
	SettingsWrite may;
	if (!may.allowed())
		return false;
	std::string text;
	for (Settings::const_iterator it = all.begin(); it != all.end(); ++it)
		text += it->first + "=" + it->second + "\n";

	const std::string path = internal::config_path;
	const std::string tmp = path + ".tmp";
	const int fd = open(tmp.c_str(), O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, 0644);
	if (fd < 0)
		return false;
	bool ok = writeAll(fd, text) && fsync(fd) == 0;
	if (close(fd) != 0)
		ok = false;
	if (!ok || rename(tmp.c_str(), path.c_str()) != 0)
	{
		unlink(tmp.c_str());
		return false;
	}
	const std::string::size_type slash = path.rfind('/');
	const std::string dir = slash == std::string::npos ? "." : (slash == 0 ? "/" : path.substr(0, slash));
	const int dfd = open(dir.c_str(), O_RDONLY | O_DIRECTORY | O_CLOEXEC);
	if (dfd < 0)
		return false;
	const bool synced = fsync(dfd) == 0;
	close(dfd);
	return synced;
}

Settings mapOf(CConfigFile &file)
{
	const ConfigDataMap all = file.getConfigDataMap();
	Settings out;
	for (ConfigDataMap::const_iterator it = all.begin(); it != all.end(); ++it)
		out[it->first] = it->second;
	return out;
}

// Asked first: the reader complains on the error stream about a missing file.
void load(CConfigFile &file, const char *path = NULL)
{
	if (path == NULL)
		path = internal::config_path;
	if (access(path, R_OK) == 0)
		file.loadConfig(path);
}

/* What a box nobody set anything on runs on: what the file this one replaces
   was shipped with, so a box answers as it did. */
const char *const kShipped[][2] = {
	{ "start_page", "none" },
	{ "startup", "true" },
	{ "style", "dark" },
	{ "yweb_box_color", "fd6d13" },
	{ "yweb_box_name", "${HOSTNAME}" },
};

// The mark the new file keeps once the former one was taken over.
const char kTakenOver[] = "migrated_from";

/* A value carried over as the pages name it now: the interface's own pages
   were templates of the old engine (.yhtm) and are plain files (.html). Only
   those; an extension's page keeps the name its manifest gives it. */
const char *const kRenamedPages[] = {
	"bouquet-editor", "bouquets", "dashboard", "epg", "files", "recordings",
	"screenshot", "settings", "sysinfo", "timer", "tools", "wizard",
};

std::string carried(const std::string &key, const std::string &value)
{
	if (key != "start_page" || !endsWith(value, ".yhtm"))
		return value;
	const std::string name = value.substr(value[0] == '/' ? 1 : 0, value.size() - (value[0] == '/' ? 1 : 0) - 5);
	for (size_t i = 0; i < sizeof(kRenamedPages) / sizeof(kRenamedPages[0]); ++i)
		if (name == kRenamedPages[i])
			return name + ".html";
	return value;
}

// The keys somebody set, the mark of the takeover not counted.
size_t ownKeys(const Settings &set)
{
	return set.size() - set.count(kTakenOver);
}

// Under FileLock.
bool migrateHeld()
{
	CConfigFile file(',');
	load(file);
	Settings now = mapOf(file);
	if (now.count(kTakenOver) != 0 || access(internal::former_path, R_OK) != 0)
		return true;
	CConfigFile former(',');
	load(former, internal::former_path);
	const Settings was = mapOf(former);
	for (Settings::const_iterator it = was.begin(); it != was.end(); ++it)
	{
		if (now.count(it->first) != 0)
			continue;
		const std::string value = carried(it->first, it->second);
		if (keyOk(it->first) && !value.empty() && value.size() <= kMaxValueLength
				&& valueOk(value) && ownKeys(now) < kMaxKeys)
			now[it->first] = value;
		else
			/* A line the new file could not keep stays behind in the old one,
			   named here so whoever misses it knows where it is; the takeover
			   is not tried again. The value is not repeated: it may be a key
			   to a service. */
			std::fprintf(stderr, "[webui] %s in %s is not taken over into %s; it stays in the old file\n",
				     it->first.c_str(), internal::former_path, internal::config_path);
	}
	now[kTakenOver] = "yWeb.conf";
	return save(now);
}

Settings withShipped(const Settings &set)
{
	Settings all = set;
	for (size_t i = 0; i < sizeof(kShipped) / sizeof(kShipped[0]); ++i)
		all.insert(std::make_pair(std::string(kShipped[i][0]), std::string(kShipped[i][1])));
	return all;
}

} // namespace

FileLock::FileLock() { fileMutex().lock(); }
FileLock::~FileLock() { fileMutex().unlock(); }

bool isSecret(const std::string &key)
{
	return endsWith(key, "_key") || endsWith(key, "_dev_id") ||
	       key.find("password") != std::string::npos ||
	       key.find("pincode") != std::string::npos ||
	       key.find("_token") != std::string::npos;
}

bool migrate()
{
	FileLock held;
	return migrateHeld();
}

Settings read()
{
	FileLock held;
	migrateHeld();
	CConfigFile file(',');
	load(file);
	return withShipped(mapOf(file));
}

Result<Settings> change(const Settings &set)
{
	for (Settings::const_iterator it = set.begin(); it != set.end(); ++it)
	{
		if (!keyOk(it->first))
			return fail(Status::InvalidArgument, ErrorCode::BadName,
				    "a key is a letter followed by up to 299 letters, digits, underscores, dots and dashes");
		if (it->second.size() > kMaxValueLength)
			return fail(Status::InvalidArgument, ErrorCode::ValueTooLong,
				    "a value is 512 bytes at most");
		if (!valueOk(it->second))
			return fail(Status::InvalidArgument, ErrorCode::BadString,
				    "a value carries a control byte, {= or =}, one of < > \" ' ` or a backslash, "
				    "which the old pages would carry into their markup, or #, which the file "
				    "reads as a comment");
	}

	FileLock held;
	migrateHeld();
	CConfigFile file(',');
	load(file);
	Settings now = mapOf(file);
	for (Settings::const_iterator it = set.begin(); it != set.end(); ++it)
	{
		if (it->second.empty())
			now.erase(it->first);
		else
			now[it->first] = it->second;
	}
	if (ownKeys(now) > kMaxKeys)
		return fail(Status::Conflict, ErrorCode::TooManySettings,
			    "the file holds as many keys as it takes; take one out first");

	if (!save(now))
		return fail(Status::Internal, ErrorCode::SettingNotWritten,
			    "the web interface's settings file could not be saved");
	return ok(withShipped(now));
}

} // namespace webui
} // namespace coreapi
