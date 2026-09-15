/*
 * settingstable_cam.cpp - descrambling settings, one row per field
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

/* The conditional access section, off src/gui/cam_menu.cpp. That screen is the
   module menu the service menu opens rather than a settings screen, and it
   binds zapit's own configuration beside these six, which is why only these six
   are here. */

/* What the box does with a module, src/gui/cam_menu.cpp:169. */
const EnumValue kCiMode[] =
{
	{ 0, "ci.mode_0" },
	{ 1, "ci.mode_1" },
	{ 2, "ci.mode_2" }
};

const Descriptor kCam[] =
{
	// src/neutrino.cpp:582 src/gui/cam_menu.cpp:162
	{
		"ci_standby_reset", ValueType::Bool, "cam",
		"ci.reset_standby", NULL,
		0, 1, NULL, 0, 0, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(ci_standby_reset)
	},
	// src/neutrino.cpp:583 src/gui/cam_menu.cpp:165
	{
		"ci_check_live", ValueType::Bool, "cam",
		"ci.check_live_slot", NULL,
		0, 1, NULL, 0, 0, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(ci_check_live)
	},
	// src/neutrino.cpp:585 src/gui/cam_menu.cpp:168
	{
		"ci_rec_zapto", ValueType::Bool, "cam",
		"ci.rec_zapto", NULL,
		0, 1, NULL, 0, 0, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(ci_rec_zapto)
	},
	// src/neutrino.cpp:586 src/gui/cam_menu.cpp:169
	{
		"ci_mode", ValueType::Enum, "cam",
		"ci.mode", "menu.hint_ci_mode",
		0, 0, COREAPI_VALUES(kCiMode), 0, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(ci_mode)
	},
	/* Which tuner the module reads, as the number the box gives it, and minus
	   one for none. A number and not a choice: the screen builds its list from
	   the tuners the running box has, src/gui/cam_menu.cpp:175, so no table in
	   the source states the values. The ceiling is the largest tuner count any
	   box is built for, src/zapit/femanager.h:35, which is wider than what most
	   boxes offer; the narrower ceilings belong to other builds and refusing a
	   value the box would take is the worse direction. */
	// src/neutrino.cpp:584 src/gui/cam_menu.cpp:192
	{
		"ci_tuner", ValueType::Int, "cam",
		"ci.tuner", NULL,
		-1, 23, NULL, 0, -1, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(ci_tuner)
	},
#if BOXMODEL_VUPLUS_ALL
	/* Behind the arm the settings struct puts the field behind,
	   src/system/settings.h:423. A number and not a choice: the screen names
	   its five values with literal digits rather than locales,
	   src/gui/cam_menu.cpp:88, so a choice here would offer words the program
	   does not have. The bounds are the first and the last of those five. */
	// src/neutrino.cpp:588 src/gui/cam_menu.cpp:160
	{
		"ci_delay", ValueType::Int, "cam",
		"ci.delay", NULL,
		16, 256, NULL, 0, 128, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(ci_delay)
	},
#endif
};

} // anonymous namespace

const Descriptor *settingsTableCam(size_t &count)
{
	count = sizeof(kCam) / sizeof(kCam[0]);
	return kCam;
}

} // namespace coreapi
