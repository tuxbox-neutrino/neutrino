/*
 * Whether a folder lies on the disk the root file system is on
 *
 * Copyright (C) 2026 Tuxbox-Project
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
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <climits>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <sys/stat.h>
#include <sys/sysmacros.h>

#include "rootdisk.h"

/* The sysfs directory of the whole disk a block device belongs to: its own
   for a disk, its parent's for a partition, which has a partition file. */
static bool whole_disk(dev_t dev, const char *sysdev, std::string &disk)
{
	char path[PATH_MAX];
	snprintf(path, sizeof(path), "%s/%u:%u", sysdev, major(dev), minor(dev));
	char real[PATH_MAX];
	if (!realpath(path, real))
		return false;
	disk = real;
	struct stat st;
	if (stat((disk + "/partition").c_str(), &st) == 0)
	{
		std::string::size_type slash = disk.rfind('/');
		if (slash != std::string::npos && slash > 0)
			disk.erase(slash);
	}
	return true;
}

bool devs_on_same_disk(dev_t a, dev_t b, const char *sysdev)
{
	if (a == b)
		return true;
	std::string disk_a, disk_b;
	return whole_disk(a, sysdev, disk_a) && whole_disk(b, sysdev, disk_b) && disk_a == disk_b;
}

/* The first part of the path that exists, walking up from the folder. False
   for a relative path no part of which exists. */
static bool existing_part(std::string dir, struct stat &st)
{
	while (stat(dir.c_str(), &st) != 0)
	{
		std::string::size_type slash = dir.rfind('/');
		if (slash == std::string::npos)
			return false;
		dir = (slash == 0) ? std::string("/") : dir.substr(0, slash);
	}
	return true;
}

bool dir_on_disk_of(const char *dir, const char *ref, const char *sysdev)
{
	struct stat dir_st, ref_st;
	if (!dir || !*dir || !existing_part(dir, dir_st) || stat(ref, &ref_st) != 0)
		return false;
	return devs_on_same_disk(dir_st.st_dev, ref_st.st_dev, sysdev);
}

bool dir_on_root_disk(const char *dir)
{
#if HAVE_GENERIC_HARDWARE
	(void) dir;
	return false;
#else
	return dir_on_disk_of(dir, "/", "/sys/dev/block");
#endif
}
