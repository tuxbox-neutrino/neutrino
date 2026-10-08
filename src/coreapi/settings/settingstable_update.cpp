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
constexpr EnumValue kNameModeApply[] =
{
	option(0).label("flashupdate.namemode1_default"),
	option(1).label("flashupdate.namemode1_hostname_time"),
	option(2).label("flashupdate.namemode1_orgname_time")
};

// And the one it keeps, src/gui/update_settings.cpp:144.
constexpr EnumValue kNameModeBackup[] =
{
	option(0).label("flashupdate.namemode2_default"),
	option(1).label("flashupdate.namemode2_hostname_time")
};

// How often the box looks for new packages, src/gui/update_settings.cpp:171.
constexpr EnumValue kAutoCheckPackages[] =
{
	option(-1).label("auto_update_check_on_start_only"),
	option(0).label("auto_update_check_off"),
	option(6).label("auto_update_check_6_hours"),
	option(24).label("auto_update_check_daily"),
	option(168).label("auto_update_check_weekly"),
	option(672).label("auto_update_check_monthly")
};

/* The screen makes the name of the applied image editable only while the box is
   told to carry the settings over, which it states as the item's own state.
   src/gui/update_settings.cpp:151 */
constexpr Condition kApplyingSettings[] =
{
	when("apply_settings").isNot(0)
};

constexpr Descriptor kUpdate[] =
{
	// src/neutrino.cpp:1084 src/gui/update_settings.cpp:164
	boolRow("softupdate_autocheck")
		.section("update")
		.label("flashupdate.autocheck")
		.hint("menu.hint_auto_update_check")
		.defaultValue(0)
		.field(COREAPI_NUMBER_FIELD(softupdate_autocheck)),
#if ENABLE_PKG_MANAGEMENT
	/* Behind the arm the settings struct puts the field behind,
	   src/system/settings.h:852. The screen offers it only where the box has a
	   package manager, src/gui/update_settings.cpp:169, which is a call and not
	   a setting, so the row carries no condition. */
	// src/neutrino.cpp:1086 src/gui/update_settings.cpp:171
	enumRow("softupdate_autocheck_packages")
		.section("update")
		.label("flashupdate.autocheck_packages")
		.hint("menu.hint_auto_update_check")
		.defaultValue(0)
		.values(kAutoCheckPackages)
		.field(COREAPI_NUMBER_FIELD(softupdate_autocheck_packages)),
#endif
	/* The screen puts this one and the two below it behind the arm that builds
	   the extended update, src/gui/update_settings.cpp:143, and this one behind
	   a box model besides, :147. The settings struct holds all three whatever
	   the build and the loader reads and writes all three the same way, so the
	   rows are unconditional and cite the item that states each label. */
	// src/neutrino.cpp:1090 src/gui/update_settings.cpp:148
	boolRow("apply_settings")
		.section("update")
		.label("flashupdate.menu_apply_settings")
		.defaultValue(0)
		.field(COREAPI_NUMBER_FIELD(apply_settings)),
	// src/neutrino.cpp:1093 src/gui/update_settings.cpp:151
	enumRow("softupdate_name_mode_apply")
		.section("update")
		.label("flashupdate.namemode1")
		.defaultValue(0)
		.values(kNameModeApply)
		.changeableWhen(kApplyingSettings)
		.field(COREAPI_NUMBER_FIELD(softupdate_name_mode_apply)),
	// src/neutrino.cpp:1094 src/gui/update_settings.cpp:144
	enumRow("softupdate_name_mode_backup")
		.section("update")
		.label("flashupdate.namemode2")
		.defaultValue(0)
		.values(kNameModeBackup)
		.field(COREAPI_NUMBER_FIELD(softupdate_name_mode_backup)),
	/* Thirty characters on the screen and a restricted set of them,
	   src/gui/update_settings.cpp:55, and a String carries neither, so that is
	   stated nowhere a caller can read. */
	// src/neutrino.cpp:1091 src/gui/update_settings.cpp:139
	textRow("softupdate_url_file")
		.section("update")
		.label("flashupdate.url_file")
		.defaultValue("/var/etc/update.urls")
		.field(COREAPI_TEXT_FIELD(softupdate_url_file)),
	// src/neutrino.cpp:1123 src/gui/update_settings.cpp:141
	textRow("update_dir")
		.section("update")
		.label("extra.update_dir")
		.defaultValue("/tmp")
		.field(COREAPI_TEXT_FIELD(update_dir)),
	/* Where the package manager puts what it downloads. No item anywhere names
	   it, so the program has no name for it either; the file browser the
	   manager opens writes it, src/gui/opkg_manager.cpp:139. Its default is the
	   directory beside it rather than a constant, so the literal below is what
	   that one falls back to. src/neutrino.cpp:1123 */
	// src/neutrino.cpp:1124
	textRow("update_dir_opkg")
		.section("update")
		.defaultValue("/tmp")
		.field(COREAPI_TEXT_FIELD(update_dir_opkg)),
	/* The seven below are what the image writer puts into the image it builds.
	   Four of them the screen offers only where the box has the partition or
	   the build has the binary, src/gui/update.cpp:1314, :1320, :1326 and
	   :1335, and where it does not the screen writes nought over the value on
	   its way in. None of that is a comparison against another setting, so none
	   of the rows carries a condition. */
	// src/neutrino.cpp:1120 src/gui/update.cpp:1315
	boolRow("flashupdate_createimage_add_var")
		.section("update")
		.label("flashupdate.createimage_add_var")
		.defaultValue(1)
		.field(COREAPI_NUMBER_FIELD(flashupdate_createimage_add_var)),
	// src/neutrino.cpp:1116 src/gui/update.cpp:1321
	boolRow("flashupdate_createimage_add_root1")
		.section("update")
		.label("flashupdate.createimage_add_root1")
		.defaultValue(0)
		.field(COREAPI_NUMBER_FIELD(flashupdate_createimage_add_root1)),
	// src/neutrino.cpp:1119 src/gui/update.cpp:1324
	boolRow("flashupdate_createimage_add_uldr")
		.section("update")
		.label("flashupdate.createimage_add_uldr")
		.defaultValue(1)
		.field(COREAPI_NUMBER_FIELD(flashupdate_createimage_add_uldr)),
	// src/neutrino.cpp:1118 src/gui/update.cpp:1327
	boolRow("flashupdate_createimage_add_u_boot")
		.section("update")
		.label("flashupdate.createimage_add_u_boot")
		.defaultValue(0)
		.field(COREAPI_NUMBER_FIELD(flashupdate_createimage_add_u_boot)),
	// src/neutrino.cpp:1114 src/gui/update.cpp:1329
	boolRow("flashupdate_createimage_add_env")
		.section("update")
		.label("flashupdate.createimage_add_env")
		.defaultValue(0)
		.field(COREAPI_NUMBER_FIELD(flashupdate_createimage_add_env)),
	// src/neutrino.cpp:1117 src/gui/update.cpp:1336
	boolRow("flashupdate_createimage_add_spare")
		.section("update")
		.label("flashupdate.createimage_add_spare")
		.defaultValue(0)
		.field(COREAPI_NUMBER_FIELD(flashupdate_createimage_add_spare)),
	// src/neutrino.cpp:1115 src/gui/update.cpp:1341
	boolRow("flashupdate_createimage_add_kernel")
		.section("update")
		.label("flashupdate.createimage_add_kernel")
		.defaultValue(1)
		.field(COREAPI_NUMBER_FIELD(flashupdate_createimage_add_kernel)),
};

} // anonymous namespace

const Descriptor *settingsTableUpdate(size_t &count)
{
	count = sizeof(kUpdate) / sizeof(kUpdate[0]);
	return kUpdate;
}

} // namespace coreapi
