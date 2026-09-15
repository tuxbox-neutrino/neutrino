/*
 * settingstable_update.cpp - update settings, one row per field
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

/* The software update section, off src/gui/update_settings.cpp and
   src/gui/update.cpp. Neither is a settings screen: the first is the update
   menu's own settings page and the second is the image writer, which offers the
   seven questions about what goes into an image it builds. */

// What the box calls the image it applies, src/gui/update_settings.cpp:151.
const EnumValue kNameModeApply[] =
{
	{ 0, "flashupdate.namemode1_default" },
	{ 1, "flashupdate.namemode1_hostname_time" },
	{ 2, "flashupdate.namemode1_orgname_time" }
};

// And the one it keeps, src/gui/update_settings.cpp:144.
const EnumValue kNameModeBackup[] =
{
	{ 0, "flashupdate.namemode2_default" },
	{ 1, "flashupdate.namemode2_hostname_time" }
};

// How often the box looks for new packages, src/gui/update_settings.cpp:171.
const EnumValue kAutoCheckPackages[] =
{
	{  -1, "auto_update_check_on_start_only" },
	{   0, "auto_update_check_off" },
	{   6, "auto_update_check_6_hours" },
	{  24, "auto_update_check_daily" },
	{ 168, "auto_update_check_weekly" },
	{ 672, "auto_update_check_monthly" }
};

/* The screen makes the name of the applied image editable only while the box is
   told to carry the settings over, which it states as the item's own state.
   src/gui/update_settings.cpp:151 */
const Condition kApplyingSettings[] =
{
	{ "apply_settings", CompareOp::Ne, 0, NULL, 0 }
};

const Descriptor kUpdate[] =
{
	// src/neutrino.cpp:1084 src/gui/update_settings.cpp:164
	{
		"softupdate_autocheck", ValueType::Bool, "update",
		"flashupdate.autocheck", "menu.hint_auto_update_check",
		0, 1, NULL, 0, 0, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(softupdate_autocheck)
	},
#if ENABLE_PKG_MANAGEMENT
	/* Behind the arm the settings struct puts the field behind,
	   src/system/settings.h:852. The screen offers it only where the box has a
	   package manager, src/gui/update_settings.cpp:169, which is a call and not
	   a setting, so the row carries no condition. */
	// src/neutrino.cpp:1086 src/gui/update_settings.cpp:171
	{
		"softupdate_autocheck_packages", ValueType::Enum, "update",
		"flashupdate.autocheck_packages", "menu.hint_auto_update_check",
		0, 0, COREAPI_VALUES(kAutoCheckPackages), 0, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(softupdate_autocheck_packages)
	},
#endif
	/* The screen puts this one and the two below it behind the arm that builds
	   the extended update, src/gui/update_settings.cpp:143, and this one behind
	   a box model besides, :147. The settings struct holds all three whatever
	   the build and the loader reads and writes all three the same way, so the
	   rows are unconditional and cite the item that states each label. */
	// src/neutrino.cpp:1090 src/gui/update_settings.cpp:148
	{
		"apply_settings", ValueType::Bool, "update",
		"flashupdate.menu_apply_settings", NULL,
		0, 1, NULL, 0, 0, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(apply_settings)
	},
	// src/neutrino.cpp:1093 src/gui/update_settings.cpp:151
	{
		"softupdate_name_mode_apply", ValueType::Enum, "update",
		"flashupdate.namemode1", NULL,
		0, 0, COREAPI_VALUES(kNameModeApply), 0, NULL, false, false,
		COREAPI_CONDITIONS(kApplyingSettings),
		COREAPI_NUMBER_FIELD(softupdate_name_mode_apply)
	},
	// src/neutrino.cpp:1094 src/gui/update_settings.cpp:144
	{
		"softupdate_name_mode_backup", ValueType::Enum, "update",
		"flashupdate.namemode2", NULL,
		0, 0, COREAPI_VALUES(kNameModeBackup), 0, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(softupdate_name_mode_backup)
	},
	/* Thirty characters on the screen and a restricted set of them,
	   src/gui/update_settings.cpp:55, and a String carries neither, so that is
	   stated nowhere a caller can read. */
	// src/neutrino.cpp:1091 src/gui/update_settings.cpp:139
	{
		"softupdate_url_file", ValueType::String, "update",
		"flashupdate.url_file", NULL,
		0, 0, NULL, 0, 0, "/var/etc/update.urls", false, false, COREAPI_ALWAYS,
		COREAPI_TEXT_FIELD(softupdate_url_file)
	},
	// src/neutrino.cpp:1123 src/gui/update_settings.cpp:141
	{
		"update_dir", ValueType::String, "update",
		"extra.update_dir", NULL,
		0, 0, NULL, 0, 0, "/tmp", false, false, COREAPI_ALWAYS,
		COREAPI_TEXT_FIELD(update_dir)
	},
	/* Where the package manager puts what it downloads. No item anywhere names
	   it, so the program has no name for it either; the file browser the
	   manager opens writes it, src/gui/opkg_manager.cpp:139. Its default is the
	   directory beside it rather than a constant, so the literal below is what
	   that one falls back to. src/neutrino.cpp:1123 */
	// src/neutrino.cpp:1124
	{
		"update_dir_opkg", ValueType::String, "update",
		NULL, NULL,
		0, 0, NULL, 0, 0, "/tmp", false, false, COREAPI_ALWAYS,
		COREAPI_TEXT_FIELD(update_dir_opkg)
	},
	/* The seven below are what the image writer puts into the image it builds.
	   Four of them the screen offers only where the box has the partition or
	   the build has the binary, src/gui/update.cpp:1314, :1320, :1326 and
	   :1335, and where it does not the screen writes nought over the value on
	   its way in. None of that is a comparison against another setting, so none
	   of the rows carries a condition. */
	// src/neutrino.cpp:1120 src/gui/update.cpp:1315
	{
		"flashupdate_createimage_add_var", ValueType::Bool, "update",
		"flashupdate.createimage_add_var", NULL,
		0, 1, NULL, 0, 1, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(flashupdate_createimage_add_var)
	},
	// src/neutrino.cpp:1116 src/gui/update.cpp:1321
	{
		"flashupdate_createimage_add_root1", ValueType::Bool, "update",
		"flashupdate.createimage_add_root1", NULL,
		0, 1, NULL, 0, 0, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(flashupdate_createimage_add_root1)
	},
	// src/neutrino.cpp:1119 src/gui/update.cpp:1324
	{
		"flashupdate_createimage_add_uldr", ValueType::Bool, "update",
		"flashupdate.createimage_add_uldr", NULL,
		0, 1, NULL, 0, 1, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(flashupdate_createimage_add_uldr)
	},
	// src/neutrino.cpp:1118 src/gui/update.cpp:1327
	{
		"flashupdate_createimage_add_u_boot", ValueType::Bool, "update",
		"flashupdate.createimage_add_u_boot", NULL,
		0, 1, NULL, 0, 0, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(flashupdate_createimage_add_u_boot)
	},
	// src/neutrino.cpp:1114 src/gui/update.cpp:1329
	{
		"flashupdate_createimage_add_env", ValueType::Bool, "update",
		"flashupdate.createimage_add_env", NULL,
		0, 1, NULL, 0, 0, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(flashupdate_createimage_add_env)
	},
	// src/neutrino.cpp:1117 src/gui/update.cpp:1336
	{
		"flashupdate_createimage_add_spare", ValueType::Bool, "update",
		"flashupdate.createimage_add_spare", NULL,
		0, 1, NULL, 0, 0, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(flashupdate_createimage_add_spare)
	},
	// src/neutrino.cpp:1115 src/gui/update.cpp:1341
	{
		"flashupdate_createimage_add_kernel", ValueType::Bool, "update",
		"flashupdate.createimage_add_kernel", NULL,
		0, 1, NULL, 0, 1, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(flashupdate_createimage_add_kernel)
	},
};

} // anonymous namespace

const Descriptor *settingsTableUpdate(size_t &count)
{
	count = sizeof(kUpdate) / sizeof(kUpdate[0]);
	return kUpdate;
}

} // namespace coreapi
