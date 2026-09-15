/*
 * daemons.cpp - which of the box daemons are up
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

#include "daemons.h"
#include "coreapi/base/errors.h"
#include "shelltable.h"

#include <cctype>
#include <cstdio>
#include <cstring>

#include <dirent.h>
#include <unistd.h>

#include <OpenThreads/Mutex>
#include <OpenThreads/ScopedLock>

namespace coreapi
{
namespace daemons
{

namespace
{

/* Three names for one thing, because the box uses three and they are not the
   same string often enough to pick any one of them.

   name is what a caller writes and what the three verbs take. process is what
   the program calls itself in the process table, which differs wherever the
   init script and the binary disagree; asking the table for the caller's name
   would answer no for a daemon that is running. carrier is the file whose
   presence says this box has the thing at all: for a softcam the program under
   /var/bin, and for a service the init script the box's own service command
   would run. */
struct Daemon
{
	const char *name;
	const char *process;
	Kind        kind;
	const char *carrier;
};

const Daemon kDaemons[] = {
	{ "mgcamd", "mgcamd", Kind::Softcam, "mgcamd" },
	{ "gbox", "gbox", Kind::Softcam, "gbox" },
	{ "oscam", "oscam", Kind::Softcam, "oscam" },
	{ "osmod", "osmod", Kind::Softcam, "osmod" },
	{ "ncam", "ncam", Kind::Softcam, "ncam" },
	{ "cccam", "cccam", Kind::Softcam, "cccam" },
	{ "doscam", "doscam", Kind::Softcam, "doscam" },
	{ "fritzcallmonitor", "fritzcallmonitor", Kind::Service, "fritzcallmonitor" },
	{ "nfs", "nfsd", Kind::Service, "nfsd" },
	{ "samba", "smbd", Kind::Service, "samba" },
	{ "tuxcald", "tuxcald", Kind::Service, "tuxcald" },
	{ "tuxmaild", "tuxmaild", Kind::Service, "tuxmaild" },
	{ "inadyn", "inadyn", Kind::Service, "inadyn" },
	{ "dropbear", "dropbear", Kind::Service, "dropbear" },
	{ "ushare", "ushare", Kind::Service, "ushare" },
	{ "djmount", "djmount", Kind::Service, "djmount" },
	{ "minidlna", "minidlnad", Kind::Service, "minidlnad" },
	{ "xupnpd", "xupnpd", Kind::Service, "xupnpd" },
	{ "crond", "crond", Kind::Service, "crond" }
};

const size_t kDaemonCount = sizeof(kDaemons) / sizeof(kDaemons[0]);

// Where a softcam sits once it is on the box, which is where the television's
// own softcam screen looks for it (src/gui/daemon_control.cpp).
const char kCamdDir[] = "/var/bin/";

/* The two directories the box's own service command searches, in its order.
   Named here rather than asked of that command, because asking would mean
   running a shell to find out whether something can be run. */
const char *const kInitDirs[] = { "/etc/init.d/", "/var/etc/init.d/" };

const size_t kInitDirCount = sizeof(kInitDirs) / sizeof(kInitDirs[0]);

OpenThreads::Mutex &procMutex()
{
	static OpenThreads::Mutex m;
	return m;
}

std::string &procStore()
{
	static std::string s("/proc");
	return s;
}

OpenThreads::Mutex &systemMutex()
{
	static OpenThreads::Mutex m;
	return m;
}

std::string &systemStore()
{
	static std::string s;
	return s;
}

const Daemon *findDaemon(const std::string &name)
{
	for (size_t i = 0; i < kDaemonCount; i++)
	{
		if (name == kDaemons[i].name)
			return &kDaemons[i];
	}
	return NULL;
}

bool allDigits(const char *s)
{
	if (s[0] == '\0')
		return false;
	for (size_t i = 0; s[i] != '\0'; i++)
	{
		if (!isdigit((unsigned char) s[i]))
			return false;
	}
	return true;
}

std::string firstLine(const std::string &path)
{
	FILE *f = fopen(path.c_str(), "r");
	if (f == NULL)
		return std::string();
	char buf[256];
	const bool got = (fgets(buf, sizeof(buf), f) != NULL);
	fclose(f);
	if (!got)
		return std::string();
	std::string out(buf);
	const size_t nl = out.find('\n');
	if (nl != std::string::npos)
		out.erase(nl);
	return out;
}

// The first argument a process was started with, which is not shortened the way
// its name in the process table is.
std::string firstArgument(const std::string &path)
{
	FILE *f = fopen(path.c_str(), "r");
	if (f == NULL)
		return std::string();
	char buf[512];
	const size_t got = fread(buf, 1, sizeof(buf) - 1, f);
	fclose(f);
	buf[got] = '\0';
	return std::string(buf);
}

std::string baseName(const std::string &path)
{
	const size_t cut = path.find_last_of('/');
	return (cut == std::string::npos) ? path : path.substr(cut + 1);
}

// Both names are asked because each misses something the other catches: the
// process table shortens a name to fifteen characters, and the arguments carry
// whatever the program was told to call itself.
bool processRunning(const std::string &name)
{
	const std::string root = procRoot();
	DIR *d = opendir(root.c_str());
	if (d == NULL)
		return false;

	bool found = false;
	for (;;)
	{
		struct dirent *e = readdir(d);
		if (e == NULL)
			break;
		if (!allDigits(e->d_name))
			continue;
		const std::string dir = root + "/" + e->d_name;
		if (firstLine(dir + "/comm") == name)
		{
			found = true;
			break;
		}
		if (baseName(firstArgument(dir + "/cmdline")) == name)
		{
			found = true;
			break;
		}
	}
	closedir(d);
	return found;
}

// Present and runnable, which for both sorts is the same question about a
// file: a program that is there and cannot be executed is one the box cannot
// start either.
bool runnableFile(const std::string &path)
{
	return access(path.c_str(), X_OK) == 0;
}

bool carrierPresent(const Daemon &d)
{
	const std::string root = systemRoot();
	if (d.kind == Kind::Softcam)
		return runnableFile(root + kCamdDir + d.carrier);
	for (size_t i = 0; i < kInitDirCount; i++)
	{
		if (runnableFile(root + kInitDirs[i] + d.carrier))
			return true;
	}
	return false;
}

Result<void> act(const std::string &name, const char *verb)
{
	if (findDaemon(name) == NULL)
		return fail(Status::NotFound, ErrorCode::NoSuchDaemon, "this box has no daemon called " + name);

	const std::string command = name + "." + verb;
	if (shell::find(command) == NULL)
		return fail(Status::NotSupported, ErrorCode::NoSuchVerb,
			    "the daemon " + name + " has no way to " + verb);

	const std::vector<std::pair<std::string, std::string> > none;
	Result<std::string> ran = shell::runNamed(command, none);
	if (!ran.ok())
		return fail(ran.error());
	return ok();
}

} // namespace

void setProcRoot(const std::string &dir)
{
	OpenThreads::ScopedLock<OpenThreads::Mutex> lock(procMutex());
	procStore() = dir;
}

std::string procRoot()
{
	OpenThreads::ScopedLock<OpenThreads::Mutex> lock(procMutex());
	return procStore();
}

void setSystemRoot(const std::string &dir)
{
	std::string trimmed = dir;
	while (trimmed.size() > 1 && trimmed[trimmed.size() - 1] == '/')
		trimmed.erase(trimmed.size() - 1);

	OpenThreads::ScopedLock<OpenThreads::Mutex> lock(systemMutex());
	systemStore() = trimmed;
}

std::string systemRoot()
{
	OpenThreads::ScopedLock<OpenThreads::Mutex> lock(systemMutex());
	return systemStore();
}

Result<std::vector<Entry> > list()
{
	std::vector<Entry> out;
	out.reserve(kDaemonCount);
	for (size_t i = 0; i < kDaemonCount; i++)
	{
		Entry one;
		one.name = kDaemons[i].name;
		one.kind = kDaemons[i].kind;
		one.running = processRunning(kDaemons[i].process);
		one.installed = carrierPresent(kDaemons[i]);
		out.push_back(one);
	}
	return ok(std::move(out));
}

Result<void> start(const std::string &name)
{
	return act(name, "start");
}

Result<void> stop(const std::string &name)
{
	return act(name, "stop");
}

Result<void> restart(const std::string &name)
{
	return act(name, "restart");
}

} // namespace daemons
} // namespace coreapi
