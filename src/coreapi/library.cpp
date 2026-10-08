/*
 * library.cpp - the directories the movie browser looks for films in
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

#include "coreapi/library.h"

#include "coreapi/box/storage_internal.h"
#include "coreapi/settings/settings.h"

#include <configfile.h>
#include <system/settings.h>

#include <cstdio>

#include <unistd.h>

namespace coreapi
{
namespace library
{

// The movie browser sizes its list by this; the GUI and this reader must not drift apart.
static_assert(kMaxDirs == NETWORK_NFS_NR_OF_ENTRIES, "the movie browser offers another number of directories");

Dirs read()
{
	Dirs d;
	Result<std::string> record = settings::get("network_nfs_recordingdir");
	d.record = record.ok() ? record.value() : std::string();
	// The movie browser's defaults, which also stand when it never wrote its file.
	d.record_used = true;
	d.own.resize(kMaxDirs);
	for (int i = 0; i < kMaxDirs; i++)
		d.own[i].used = false;

	// Asked first: the reader complains on the error stream about a missing file.
	if (access(storage::internal::moviebrowser_config_path, R_OK) != 0)
		return d;
	CConfigFile browser(',');
	browser.loadConfig(storage::internal::moviebrowser_config_path);
	d.record_used = browser.getInt32("mb_storageDir_rec", 1) != 0;
	for (int i = 0; i < kMaxDirs; i++)
	{
		char key[32];
		snprintf(key, sizeof(key), "mb_dir_%d", i);
		d.own[i].path = browser.getString(key, "");
		snprintf(key, sizeof(key), "mb_dir_used%d", i);
		d.own[i].used = browser.getInt32(key, 0) != 0;
	}
	return d;
}

namespace
{

// The movie browser refuses an empty name and / itself; a relative one names nothing here.
void addRoot(std::vector<std::string> &out, std::string dir)
{
	while (dir.size() > 1 && dir[dir.size() - 1] == '/')
		dir.erase(dir.size() - 1);
	if (dir.empty() || dir[0] != '/' || dir == "/")
		return;
	for (size_t i = 0; i < out.size(); ++i)
		if (out[i] == dir)
			return;
	out.push_back(dir);
}

} // namespace

std::vector<std::string> roots()
{
	const Dirs d = read();
	std::vector<std::string> out;
	/* The movie directory is left out although the movie browser offers a switch
	   for it: the browser itself never scans it, and this list is what it finds. */
	if (d.record_used)
		addRoot(out, d.record);
	for (size_t i = 0; i < d.own.size(); ++i)
		if (d.own[i].used)
			addRoot(out, d.own[i].path);
	return out;
}

} // namespace library
} // namespace coreapi
