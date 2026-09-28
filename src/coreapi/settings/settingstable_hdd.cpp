/*
 * settingstable_hdd.cpp - hard disk settings, one row per field
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

#include "settingstable.h"
#include "settingsfield.h"

namespace coreapi
{

namespace
{

/* The hard disk section, off src/gui/hdd_menu.cpp. That screen is the disk
   manager the service menu opens rather than a settings screen, and everything
   else it binds is a device it found at run time, which is why only these three
   are here.

   Local resolution: the source declares four more (hdd_format_on_mount_failed,
   hdd_wakeup, hdd_wakeup_msg, hdd_allow_set_recdir). This fork stilled those
   options in 2021 - their load and save lines are commented out in
   src/neutrino.cpp and no screen of this tree offers them - so a row here
   would declare a key nothing reads. They are carried in
   settingsundeclared.cpp instead and are port candidates if the feature
   returns (WORK-114). */

/* How long the disk waits before it spins down, src/gui/hdd_menu.cpp:823. The
   last three are not minutes: the driver reads them as its own codes. */
const EnumValue kHddSleep[] =
{
	{   0, "options.off" },
	{  60, "hdd_5min" },
	{ 120, "hdd_10min" },
	{ 240, "hdd_20min" },
	{ 241, "hdd_30min" },
	{ 242, "hdd_60min" }
};

// How loud the disk is allowed to be, src/gui/hdd_menu.cpp:832.
const EnumValue kHddNoise[] =
{
	{   0, "options.off" },
	{ 128, "hdd_slow" },
	{ 190, "hdd_middle" },
	{ 254, "hdd_fast" }
};

const Descriptor kHdd[] =
{
	/* Which file system the box writes when it formats a disk, as an index into
	   the tool table the screen walks. A number and not a choice: the screen
	   names each entry with the tool's own name rather than a locale,
	   src/gui/hdd_menu.cpp:627, so a choice here would offer words the program
	   does not have. The ceiling is the last entry of that table,
	   src/gui/hdd_menu.cpp:92. */
	// src/neutrino.cpp:657 src/gui/hdd_menu.cpp:663
	{
		"hdd_fs", ValueType::Int, "hdd",
		"hdd_fs", "menu.hint_hdd_fmt",
		0, 6, NULL, 0, 0, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(hdd_fs)
	},
	// src/neutrino.cpp:658 src/gui/hdd_menu.cpp:823
	// Local resolution: this fork falls back to 120 (ten minutes), not the source's 60.
	{
		"hdd_sleep", ValueType::Enum, "hdd",
		"hdd_sleep", "menu.hint_hdd_sleep",
		0, 0, COREAPI_VALUES(kHddSleep), 120, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(hdd_sleep)
	},
	/* The screen offers this one only where the box found a full hdparm,
	   src/gui/hdd_menu.cpp:830, which is a file it looks for and not a setting,
	   so the row carries no condition and is always shown. */
	// src/neutrino.cpp:659 src/gui/hdd_menu.cpp:832
	{
		"hdd_noise", ValueType::Enum, "hdd",
		"hdd_noise", "menu.hint_hdd_noise",
		0, 0, COREAPI_VALUES(kHddNoise), 254, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(hdd_noise)
	},
};

} // anonymous namespace

const Descriptor *settingsTableHdd(size_t &count)
{
	count = sizeof(kHdd) / sizeof(kHdd[0]);
	return kHdd;
}

} // namespace coreapi
