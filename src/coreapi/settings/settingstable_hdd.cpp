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
#include "predicates.h"

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
constexpr EnumValue kHddSleep[] =
{
	option(0).label("options.off"),
	option(60).label("hdd_5min"),
	option(120).label("hdd_10min"),
	option(240).label("hdd_20min"),
	option(241).label("hdd_30min"),
	option(242).label("hdd_60min")
};

// How loud the disk is allowed to be, src/gui/hdd_menu.cpp:832.
constexpr EnumValue kHddNoise[] =
{
	option(0).label("options.off"),
	option(128).label("hdd_slow"),
	option(190).label("hdd_middle"),
	option(254).label("hdd_fast")
};

/* Positions in the disk manager's tool table, in the same order; named by the
   file system, which the program has no other words for. An entry is offered
   where its mkfs is there. */
constexpr EnumValue kHddFs[] =
{
	option(0).text("ext4").availableIf(formatsExt4),
	option(1).text("ext3").availableIf(formatsExt3),
	option(2).text("ext2").availableIf(formatsExt2),
	option(3).text("f2fs").availableIf(formatsF2fs),
	option(4).text("vfat").availableIf(formatsVfat),
	option(5).text("exfat").availableIf(formatsExfat),
	option(6).text("xfs").availableIf(formatsXfs)
};

constexpr Descriptor kHdd[] =
{
	// src/neutrino.cpp:657 src/gui/hdd_menu.cpp:663
	enumRow("hdd_fs")
		.section("hdd")
		.label("hdd_fs")
		.hint("menu.hint_hdd_fmt")
		.defaultValue(0)
		.values(kHddFs)
		.field(COREAPI_NUMBER_FIELD(hdd_fs)),
	// src/neutrino.cpp:658 src/gui/hdd_menu.cpp:823
	// Local resolution: this fork falls back to 120 (ten minutes), not the source's 60.
	enumRow("hdd_sleep")
		.section("hdd")
		.label("hdd_sleep")
		.hint("menu.hint_hdd_sleep")
		.defaultValue(120)
		.values(kHddSleep)
		.field(COREAPI_NUMBER_FIELD(hdd_sleep)),
	/* The screen offers this one only where the box found a full hdparm,
	   src/gui/hdd_menu.cpp:830, which is a file it looks for and not a setting,
	   so the row carries no condition and is always shown. */
	// src/neutrino.cpp:659 src/gui/hdd_menu.cpp:832
	enumRow("hdd_noise")
		.section("hdd")
		.label("hdd_noise")
		.hint("menu.hint_hdd_noise")
		.defaultValue(254)
		.values(kHddNoise)
		.field(COREAPI_NUMBER_FIELD(hdd_noise)),
};

} // anonymous namespace

const Descriptor *settingsTableHdd(size_t &count)
{
	count = sizeof(kHdd) / sizeof(kHdd[0]);
	return kHdd;
}

} // namespace coreapi
