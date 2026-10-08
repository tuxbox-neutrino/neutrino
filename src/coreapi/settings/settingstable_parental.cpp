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

#include <string.h>

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
constexpr EnumValue kLockage[] =
{
	option(12).label("parentallock.lockage12"),
	option(16).label("parentallock.lockage16"),
	option(18).label("parentallock.lockage18")
};

// src/gui/parentallock_setup.cpp:93
constexpr EnumValue kDefaultLocked[] =
{
	option(0).label("parentallock.defaultunlocked"),
	option(1).label("parentallock.defaultlocked")
};

constexpr EnumValue kPrompt[] =
{
	option(0).label("parentallock.never"),
	option(2).label("parentallock.changetolocked"),
	option(3).label("parentallock.onsignal")
};

constexpr Descriptor kSettings[] =
{
	/* The three the screen offers, src/gui/parentallock_setup.cpp:78; the fourth
	   of the enum, src/system/settings.h:1425-1428, sits in an arm the screen
	   never takes and names a locale the program no longer has. */
	// src/neutrino.cpp:1129 src/gui/parentallock_setup.cpp:111
	enumRow("parentallock_prompt")
		.section("parental")
		.label("parentallock.prompt")
		.hint("menu.hint_parentallock_prompt")
		.defaultValue(0)
		.values(kPrompt)
		.field(COREAPI_NUMBER_FIELD(parentallock_prompt)),
	// src/neutrino.cpp:1130 src/gui/parentallock_setup.cpp:115
	enumRow("parentallock_lockage")
		.section("parental")
		.label("parentallock.lockage")
		.hint("menu.hint_parentallock_lockage")
		.defaultValue(12)
		.values(kLockage)
		.field(COREAPI_NUMBER_FIELD(parentallock_lockage)),
	/* A choice and not a flag: the words are what a new bouquet starts as and
	   not an on and an off, so a flag would carry the values and lose them. */
	// src/neutrino.cpp:1137 src/gui/parentallock_setup.cpp:119
	enumRow("parentallock_defaultlocked")
		.section("parental")
		.label("parentallock.bouquetmode")
		.defaultValue(0)
		.values(kDefaultLocked)
		.field(COREAPI_NUMBER_FIELD(parentallock_defaultlocked)),
	// Seconds a locked channel stays watchable after the pin was given.
	// src/neutrino.cpp:1139 src/gui/parentallock_setup.cpp:121
	intRow("parentallock_zaptime")
		.section("parental")
		.label("parentallock.zaptime")
		.range(0, 10000)
		.defaultValue(60)
		.field(COREAPI_NUMBER_FIELD(parentallock_zaptime)),
	/* The pin itself, which is why it is secret: a read answers nothing and an
	   empty write is refused, so a form that round trips its fields cannot
	   clear it. The screen takes four digits and no more,
	   src/gui/parentallock_setup.cpp:125, which a String row cannot say. */
	// src/neutrino.cpp:1138 src/gui/parentallock_setup.cpp:126
	textRow("parentallock_pincode")
		.section("parental")
		.label("parentallock.changepin")
		.hint("menu.hint_parentallock_changepin")
		.defaultValue("0000")
		.text(kRulePin)
		.secret()
		.field(COREAPI_TEXT_FIELD(parentallock_pincode)),
};

/* What the lock holds. The pin is not among them: it stays changeable on a
   locked box. */
const char *const kLocked[] =
{
	"parentallock_prompt",
	"parentallock_lockage",
	"parentallock_defaultlocked",
	"parentallock_zaptime"
};

} // anonymous namespace

const Descriptor *settingsTableParental(size_t &count)
{
	count = sizeof(kSettings) / sizeof(kSettings[0]);
	return kSettings;
}

bool heldByParentalLock(const char *key)
{
	if (key == NULL)
		return false;
	for (size_t i = 0; i < sizeof(kLocked) / sizeof(kLocked[0]); ++i)
		if (strcmp(key, kLocked[i]) == 0)
			return true;
	return false;
}

} // namespace coreapi
