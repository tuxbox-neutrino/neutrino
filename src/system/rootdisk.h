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
#ifndef __ROOTDISK_H__
#define __ROOTDISK_H__

#include <sys/types.h>

/* Whether a folder lies on the box's own storage: on the root file system or
   on another partition of the disk the root file system is on, which on a box
   is the internal flash. Asked of the first part of the path that exists, as a
   folder still to be made goes where that part is. A share, another drive or
   a file system on no block device is not on it. Always false on a PC build,
   whose root file system is the disk recordings go to. */
bool dir_on_root_disk(const char *dir);

/* The same for any reference folder in place of the root, with the directory
   that maps a device number to its sysfs device (/sys/dev/block) given. */
bool dir_on_disk_of(const char *dir, const char *ref, const char *sysdev);

/* Whether two device numbers belong to the same disk: equal, or partitions of
   one disk, found through sysdev/<major>:<minor>. A device with no entry
   there (a network share, tmpfs) shares a disk with nothing. */
bool devs_on_same_disk(dev_t a, dev_t b, const char *sysdev);

#endif
