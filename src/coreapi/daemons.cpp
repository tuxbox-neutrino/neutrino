/*
 * daemons.cpp - which of the box daemons are up
 *
 * Copyright (C) 2026 NI-Team
 * Copyright (C) 2026 Tuxbox-Project (fixes)
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
#include "shellcontract.h"

#include <cctype>
#include <cstdio>
#include <cstring>
#include <utility>

#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>

#include <OpenThreads/Mutex>
#include <OpenThreads/ScopedLock>

namespace coreapi
{
namespace daemons
{

namespace
{

/* Four names for one thing, because the box uses four and they are not the
   same string often enough to pick any one of them.

   name is what a caller writes and what the three verbs take. process is what
   the program calls itself in the process table, which differs wherever the
   init script and the binary disagree; asking the table for the caller's name
   would answer no for a daemon that is running. unit is the systemd unit the
   image's recipe installs for it, with its suffix, and script the init script a
   box without that unit starts it with. */
struct Daemon
{
	const char *name;
	const char *process;
	const char *unit;
	const char *script;
};

const Daemon kDaemons[] = {
	{ "fritzcallmonitor", "fritzcallmonitor", "fritzcallmonitor.service", "fritzcallmonitor" },
	{ "nfs", "nfsd", "nfs-server.service", "nfsd" },
	{ "samba", "smbd", "smb.service", "samba" },
	{ "tuxcald", "tuxcald", "tuxcald.service", "tuxcald" },
	{ "tuxmaild", "tuxmaild", "tuxmaild.service", "tuxmaild" },
	{ "inadyn", "inadyn", "inadyn.service", "inadyn" },
	{ "dropbear", "dropbear", "dropbear.socket", "dropbear" },
	{ "ushare", "ushare", "ushare.service", "ushare" },
	{ "djmount", "djmount", "djmount.service", "djmount" },
	{ "minidlna", "minidlnad", "minidlna.service", "minidlnad" },
	{ "xupnpd", "xupnpd", "xupnpd.service", "xupnpd" },
	{ "crond", "crond", "crond.service", "crond" }
};

const size_t kDaemonCount = sizeof(kDaemons) / sizeof(kDaemons[0]);

// What says systemd runs the box, as its own tools ask it.
const char kSystemdRunning[] = "/run/systemd/system";

// Where systemctl is, in the order a box and a desktop put it.
const char *const kSystemctl[] = { "/bin/systemctl", "/usr/bin/systemctl" };

const size_t kSystemctlCount = sizeof(kSystemctl) / sizeof(kSystemctl[0]);

// Where unit files are installed, the administrator's first.
const char *const kUnitDirs[] = { "/etc/systemd/system/", "/lib/systemd/system/", "/usr/lib/systemd/system/" };

const size_t kUnitDirCount = sizeof(kUnitDirs) / sizeof(kUnitDirs[0]);

/* The two directories the box's own service command searches, in its order.
   Named here rather than asked of that command, because asking would mean
   running a shell to find out whether something can be run. systemd turns the
   scripts of the first one into units of their name. */
const char *const kInitDirs[] = { "/etc/init.d/", "/var/etc/init.d/" };

const size_t kInitDirCount = sizeof(kInitDirs) / sizeof(kInitDirs[0]);

/* A service waits for the old process to go before it starts again, and a
   caller holds a connection for the whole of it. What a command writes is read
   by nothing here; the ceiling keeps one that writes without end from being
   carried up in full. */
const int kTimeoutMs = 20000;
const size_t kMaxOutput = 4096;

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

// Present and runnable: a program that is there and cannot be executed is one
// the box cannot start either.
bool runnableFile(const std::string &path)
{
	return access(path.c_str(), X_OK) == 0;
}

bool isFile(const std::string &path)
{
	struct stat st;
	return stat(path.c_str(), &st) == 0 && S_ISREG(st.st_mode);
}

bool isDirectory(const std::string &path)
{
	struct stat st;
	return stat(path.c_str(), &st) == 0 && S_ISDIR(st.st_mode);
}

/* How a service is driven on this box: the program run, and the unit it is
   handed where that program is systemctl; no program for a service this box
   does not carry. */
struct Way
{
	std::string program;
	std::string unit;
};

Way wayFor(const Daemon &d)
{
	const std::string root = systemRoot();
	Way way;
	if (isDirectory(root + kSystemdRunning))
	{
		std::string systemctl;
		for (size_t i = 0; i < kSystemctlCount && systemctl.empty(); i++)
		{
			if (runnableFile(root + kSystemctl[i]))
				systemctl = root + kSystemctl[i];
		}
		if (!systemctl.empty())
		{
			for (size_t i = 0; i < kUnitDirCount; i++)
			{
				if (isFile(root + kUnitDirs[i] + d.unit))
				{
					way.program = systemctl;
					way.unit = d.unit;
					return way;
				}
			}
			// the unit systemd makes of an init script in /etc/init.d
			if (runnableFile(root + kInitDirs[0] + d.script))
			{
				way.program = systemctl;
				way.unit = std::string(d.script) + ".service";
				return way;
			}
		}
	}
	for (size_t i = 0; i < kInitDirCount; i++)
	{
		if (runnableFile(root + kInitDirs[i] + d.script))
		{
			way.program = root + kInitDirs[i] + d.script;
			return way;
		}
	}
	return way;
}

Result<void> act(const std::string &name, const char *verb)
{
	const Daemon *d = findDaemon(name);
	if (d == NULL)
		return fail(Status::NotFound, ErrorCode::NoSuchDaemon, "this box has no daemon called " + name);

	const Way way = wayFor(*d);
	if (way.program.empty())
		return fail(Status::NotFound, ErrorCode::ScriptMissing, "this box does not carry " + name);

	// systemctl takes the unit after the verb; an init script takes the verb alone
	Param unit;
	unit.name = "unit";
	unit.type = ParamType::String;
	unit.required = true;
	unit.doc = "";
	unit.min = 0;
	unit.max = 0;
	unit.values = NULL;
	unit.root = NULL;
	std::vector<std::pair<std::string, std::string> > given;
	if (!way.unit.empty())
		given.push_back(std::make_pair(std::string("unit"), way.unit));

	ShellCommand cmd;
	cmd.script = way.program.c_str();
	cmd.verb = verb;
	cmd.params = way.unit.empty() ? NULL : &unit;
	cmd.param_count = way.unit.empty() ? 0 : 1;
	cmd.output = OutputFormat::Raw;
	cmd.timeout_ms = kTimeoutMs;
	cmd.max_output = kMaxOutput;
	cmd.end_options = false;

	Result<std::string> ran = shell::run(cmd, given);
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
		one.kind = Kind::Service;
		one.running = processRunning(kDaemons[i].process);
		one.installed = !wayFor(kDaemons[i]).program.empty();
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
