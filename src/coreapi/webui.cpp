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
const char *config_path = CONFIGDIR "/yWeb.conf";
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

// Asked first: the reader complains on the error stream about a missing file.
void load(CConfigFile &file)
{
	if (access(internal::config_path, R_OK) == 0)
		file.loadConfig(internal::config_path);
}

Settings mapOf(CConfigFile &file)
{
	const ConfigDataMap all = file.getConfigDataMap();
	Settings out;
	for (ConfigDataMap::const_iterator it = all.begin(); it != all.end(); ++it)
		out[it->first] = it->second;
	return out;
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

Settings read()
{
	FileLock held;
	CConfigFile file(',');
	load(file);
	return mapOf(file);
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
	if (now.size() > kMaxKeys)
		return fail(Status::Conflict, ErrorCode::TooManySettings,
			    "the file holds as many keys as it takes; take one out first");

	if (!save(now))
		return fail(Status::Internal, ErrorCode::SettingNotWritten,
			    "the web interface's settings file could not be saved");
	return ok(std::move(now));
}

} // namespace webui
} // namespace coreapi
