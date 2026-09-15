/*
 * settingstable_parental.cpp - parental lock settings, one row per field
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

/* The parental lock. The screen refuses every one of these while the box itself
   is locked, src/gui/parentallock_setup.cpp:111, and the loader forces the two
   strictest values in the same state, src/neutrino.cpp:1132-1135. That is a
   state of the running box and not a setting, so no row carries it as a
   condition. */

// The screen offers the age as the three the ratings use.
// src/gui/parentallock_setup.cpp:85
const EnumValue kLockage[] =
{
	{ 12, "parentallock.lockage12" },
	{ 16, "parentallock.lockage16" },
	{ 18, "parentallock.lockage18" }
};

// src/gui/parentallock_setup.cpp:93
const EnumValue kDefaultLocked[] =
{
	{ 0, "parentallock.defaultunlocked" },
	{ 1, "parentallock.defaultlocked" }
};

const Descriptor kSettings[] =
{
	/* A number and not a choice. The screen's table carries a fourth entry
	   naming a locale the program no longer has,
	   src/gui/parentallock_setup.cpp:78, so the scan that holds a choice to the
	   screen cannot read the table at all. The range is the enum's,
	   src/system/settings.h:1425-1428, and one of the four is the entry the
	   screen keeps in an arm it never takes. */
	// src/neutrino.cpp:1129 src/gui/parentallock_setup.cpp:111
	{
		"parentallock_prompt", ValueType::Int, "parental",
		"parentallock.prompt", "menu.hint_parentallock_prompt",
		0, 3, NULL, 0, 0, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(parentallock_prompt)
	},
	// src/neutrino.cpp:1130 src/gui/parentallock_setup.cpp:115
	{
		"parentallock_lockage", ValueType::Enum, "parental",
		"parentallock.lockage", "menu.hint_parentallock_lockage",
		0, 0, COREAPI_ENUM(kLockage), 12, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(parentallock_lockage)
	},
	/* A choice and not a flag: the words are what a new bouquet starts as and
	   not an on and an off, so a flag would carry the values and lose them. */
	// src/neutrino.cpp:1137 src/gui/parentallock_setup.cpp:119
	{
		"parentallock_defaultlocked", ValueType::Enum, "parental",
		"parentallock.bouquetmode", NULL,
		0, 0, COREAPI_ENUM(kDefaultLocked), 0, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(parentallock_defaultlocked)
	},
	// Seconds a locked channel stays watchable after the pin was given.
	// src/neutrino.cpp:1139 src/gui/parentallock_setup.cpp:121
	{
		"parentallock_zaptime", ValueType::Int, "parental",
		"parentallock.zaptime", NULL,
		0, 10000, NULL, 0, 60, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(parentallock_zaptime)
	},
	/* The pin itself, which is why it is secret: a read answers nothing and an
	   empty write is refused, so a form that round trips its fields cannot
	   clear it. The screen takes four digits and no more,
	   src/gui/parentallock_setup.cpp:125, which a String row cannot say. */
	// src/neutrino.cpp:1138 src/gui/parentallock_setup.cpp:126
	{
		"parentallock_pincode", ValueType::String, "parental",
		"parentallock.changepin", "menu.hint_parentallock_changepin",
		0, 0, NULL, 0, 0, "0000", false, true, COREAPI_ALWAYS,
		COREAPI_TEXT_FIELD(parentallock_pincode)
	},
};

} // anonymous namespace

const Descriptor *settingsTableParental(size_t &count)
{
	count = sizeof(kSettings) / sizeof(kSettings[0]);
	return kSettings;
}

} // namespace coreapi
