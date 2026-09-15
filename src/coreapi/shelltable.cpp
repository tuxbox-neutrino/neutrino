/*
 * shelltable.cpp - the shell commands the box offers, one row each
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

#include <config.h>

#include "shelltable.h"
#include "coreapi/base/errors.h"

#include <cstring>

#include <OpenThreads/Mutex>
#include <OpenThreads/ScopedLock>

namespace coreapi
{
namespace shell
{

namespace
{

// Every entry below was picked by reading the branch it names: each takes no
// argument at all and runs a fixed line, which is why none declares a parameter.
// A branch that expands anything a caller wrote is not in here.
//
// The two timeouts are what the branches behind them do. A softcam is stopped
// and started again by an init script that waits for the old process to go, and
// a service script does less.
const int kCamdTimeoutMs = 20000;
const int kPluginTimeoutMs = 15000;

// The branches answer with one line naming what they did. Anything beyond that
// is a script that went wrong, and the runner cuts it off rather than carrying
// it up.
const size_t kMaxOutput = 4096;

#define CAMD_ENTRY(daemon, verb_prefix) \
	{ daemon ".start",   "/Y_Camds.sh", verb_prefix "_start", NULL, 0, OutputFormat::Lines, kCamdTimeoutMs, kMaxOutput, false }, \
	{ daemon ".stop",    "/Y_Camds.sh", verb_prefix "_stop",  NULL, 0, OutputFormat::Lines, kCamdTimeoutMs, kMaxOutput, false }, \
	{ daemon ".restart", "/Y_Camds.sh", verb_prefix "_reset", NULL, 0, OutputFormat::Lines, kCamdTimeoutMs, kMaxOutput, false }

#define PLUGIN_ENTRY(daemon, verb_prefix) \
	{ daemon ".start", "/Y_Plugins.sh", verb_prefix "_start", NULL, 0, OutputFormat::Lines, kPluginTimeoutMs, kMaxOutput, false }, \
	{ daemon ".stop",  "/Y_Plugins.sh", verb_prefix "_stop",  NULL, 0, OutputFormat::Lines, kPluginTimeoutMs, kMaxOutput, false }

const TableEntry kTable[] = {
	CAMD_ENTRY("mgcamd", "mgcamd"),
	CAMD_ENTRY("gbox", "gbox"),
	CAMD_ENTRY("oscam", "oscam"),
	CAMD_ENTRY("osmod", "osmod"),
	CAMD_ENTRY("ncam", "ncam"),
	CAMD_ENTRY("cccam", "cccam"),
	CAMD_ENTRY("doscam", "doscam"),
	PLUGIN_ENTRY("fritzcallmonitor", "fcm"),
	PLUGIN_ENTRY("nfs", "nfs"),
	PLUGIN_ENTRY("samba", "samba"),
	PLUGIN_ENTRY("tuxcald", "tuxcald"),
	PLUGIN_ENTRY("tuxmaild", "tuxmaild"),
	PLUGIN_ENTRY("inadyn", "inadyn"),
	PLUGIN_ENTRY("dropbear", "dropbear"),
	PLUGIN_ENTRY("ushare", "ushare"),
	PLUGIN_ENTRY("djmount", "djmount"),
	PLUGIN_ENTRY("minidlna", "minidlna"),
	PLUGIN_ENTRY("xupnpd", "xupnpd"),
	PLUGIN_ENTRY("crond", "crond")
};

#undef CAMD_ENTRY
#undef PLUGIN_ENTRY

const size_t kTableCount = sizeof(kTable) / sizeof(kTable[0]);

OpenThreads::Mutex &rootMutex()
{
	static OpenThreads::Mutex m;
	return m;
}

std::string &rootStore()
{
	static std::string s(PRIVATE_HTTPDDIR "/scripts");
	return s;
}

} // namespace

void setScriptRoot(const std::string &dir)
{
	std::string trimmed = dir;
	while (trimmed.size() > 1 && trimmed[trimmed.size() - 1] == '/')
		trimmed.erase(trimmed.size() - 1);

	OpenThreads::ScopedLock<OpenThreads::Mutex> lock(rootMutex());
	rootStore() = trimmed;
}

std::string scriptRoot()
{
	OpenThreads::ScopedLock<OpenThreads::Mutex> lock(rootMutex());
	return rootStore();
}

const TableEntry *find(const std::string &name)
{
	for (size_t i = 0; i < kTableCount; i++)
	{
		if (name == kTable[i].name)
			return &kTable[i];
	}
	return NULL;
}

const TableEntry *entryAt(size_t index)
{
	return (index < kTableCount) ? &kTable[index] : NULL;
}

size_t entryCount()
{
	return kTableCount;
}

Result<std::string> runNamed(const std::string &name,
			     const std::vector<std::pair<std::string, std::string> > &args)
{
	const TableEntry *entry = find(name);
	if (entry == NULL)
		return fail(Status::NotFound, ErrorCode::NoSuchCommand, "there is no command called " + name);

	// Built here rather than stored, so that the script's directory can change
	// without the table holding a pointer into a string that has gone.
	const std::string script = scriptRoot() + entry->script;
	ShellCommand cmd;
	cmd.script = script.c_str();
	cmd.verb = entry->verb;
	cmd.params = entry->params;
	cmd.param_count = entry->param_count;
	cmd.output = entry->output;
	cmd.timeout_ms = entry->timeout_ms;
	cmd.max_output = entry->max_output;
	cmd.end_options = entry->end_options;

	return run(cmd, args);
}

} // namespace shell
} // namespace coreapi
