/*
 * settingstable_osd.cpp - on screen display settings, one row per field
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

#include "coreapi/base/deps.h"

namespace coreapi
{

namespace
{

/* The OSD section. Two thirds of the defaults below are written by the program
   as a name rather than a number, so no scan can compare them and every one of
   them was read out of the header that declares it. Those headers are cited
   beside the load site for that reason. */

// src/gui/osd_setup.cpp:476
const EnumValue kSubchanPos[] =
{
	{ 0, "settings.pos_top_right" },
	{ 1, "settings.pos_top_left" },
	{ 2, "settings.pos_bottom_left" },
	{ 3, "settings.pos_bottom_right" },
	{ 4, "infoviewer.subchan_infobar" }
};

// src/gui/osd_setup.cpp:498
const EnumValue kMenuPos[] =
{
	{ 0, "settings.pos_center" },
	{ 1, "settings.pos_top_left" },
	{ 2, "settings.pos_top_right" },
	{ 3, "settings.pos_bottom_left" },
	{ 4, "settings.pos_bottom_right" }
};

// The gap is the program's: the three positions are the bits of a mask.
// src/gui/osd_setup.cpp:586 src/gui/components/cc_frm_header.h:44
const EnumValue kChannelLogoPos[] =
{
	{ 0, "options.off" },
	{ 1, "settings.pos_right" },
	{ 2, "settings.pos_left" },
	{ 4, "settings.pos_center" }
};

// src/gui/osd_setup.cpp:1203
const EnumValue kInfobarDisp[] =
{
	{ 0, "miscsettings.infobar_disp_0" },
	{ 1, "miscsettings.infobar_disp_1" },
	{ 2, "miscsettings.infobar_disp_2" },
	{ 3, "miscsettings.infobar_disp_3" },
	{ 4, "miscsettings.infobar_disp_4" },
	{ 5, "miscsettings.infobar_disp_5" },
	{ 6, "miscsettings.infobar_disp_6" }
};

// Nought is on here and the last value is off, which is why this is a choice
// and not a flag.
// src/gui/osd_setup.cpp:460
const EnumValue kCaSystem[] =
{
	{ 0, "options.on" },
	{ 1, "miscsettings.infobar_casystem_mode" },
	{ 2, "miscsettings.infobar_casystem_mini" },
	{ 3, "options.off" }
};

// src/gui/osd_setup.cpp:1258
const EnumValue kEcmPos[] =
{
	{ 0, "options.off" },
	{ 1, "settings.pos_top_left" },
	{ 2, "settings.pos_top_center" },
	{ 3, "settings.pos_top_right" }
};

// src/gui/osd_setup.cpp:1250 src/system/settings.h:487
const EnumValue kHddStatfs[] =
{
	{ 0, "options.off" },
	{ 1, "hdd_statfs_always" },
	{ 2, "hdd_statfs_recording" }
};

// Nought is on here as well.
// src/gui/osd_setup.cpp:508
const EnumValue kInfobarShowRes[] =
{
	{ 0, "options.on" },
	{ 1, "miscsettings.infobar_show_res_simple" },
	{ 2, "options.off" }
};

// src/gui/osd_setup.cpp:1215
const EnumValue kProgressbarInfobarPos[] =
{
	{ 0, "miscsettings.progressbar_infobar_position_0" },
	{ 1, "miscsettings.progressbar_infobar_position_1" },
	{ 2, "miscsettings.progressbar_infobar_position_2" },
	{ 3, "miscsettings.progressbar_infobar_position_3" }
};

// src/gui/osd_setup.cpp:516
const EnumValue kChannellistAdditional[] =
{
	{ 0, "channellist.additional_off" },
	{ 1, "channellist.additional_on" },
	{ 2, "channellist.additional_on_minitv" }
};

// src/gui/osd_setup.cpp:532 src/gui/channellist.h:68
const EnumValue kEpgtextAlignment[] =
{
	{ 0, "channellist.epgtext_align_left_middle" },
	{ 1, "channellist.epgtext_align_left_bottom" },
	{ 2, "channellist.epgtext_align_right_middle" },
	{ 3, "channellist.epgtext_align_right_bottom" }
};

// src/gui/osd_setup.cpp:524
const EnumValue kChannellistFoot[] =
{
	{ 0, "channellist.foot_freq" },
	{ 1, "channellist.foot_next" },
	{ 2, "channellist.foot_off" }
};

// src/gui/osd_setup.cpp:486 src/gui/volumebar.h:78
const EnumValue kVolumePos[] =
{
	{ 0, "settings.pos_top_right" },
	{ 1, "settings.pos_top_left" },
	{ 2, "settings.pos_bottom_left" },
	{ 3, "settings.pos_bottom_right" },
	{ 4, "settings.pos_top_center" },
	{ 5, "settings.pos_bottom_center" },
	{ 6, "settings.pos_higher_center" }
};

// src/gui/osd_setup.cpp:453 src/gui/osd_setup.h:105
const EnumValue kScreenPreset[] =
{
	{ 0, "osd.preset_screen_a" },
	{ 1, "osd.preset_screen_b" }
};

// src/gui/osd_setup.cpp:1797
const EnumValue kScreenshotMode[] =
{
	{ 0, "screenshot.tv" },
	{ 1, "screenshot.osd" }
};

// src/gui/osd_setup.cpp:1840 src/gui/screensaver.h:85
const EnumValue kScreensaverMode[] =
{
	{ 0, "screensaver.mode_image" },
	{ 1, "screensaver.mode_clock" },
	{ 2, "screensaver.mode_clock_color" }
};

// The screen hands each of these in as the flag that makes the item editable.
const Condition kChannelLogoOn[] =
{
	{ "channellist_show_channellogo", CompareOp::Ne, 0, NULL, 0 }
};

// Two and three are the mini bar and off, and neither has a frame to draw.
const Condition kCaSystemDrawn[] =
{
	{ "infobar_casystem_display", CompareOp::Lt, 2, NULL, 0 }
};

const Condition kSysfsHddOn[] =
{
	{ "infobar_show_sysfs_hdd", CompareOp::Ne, 0, NULL, 0 }
};

const Condition kInfoboxOn[] =
{
	{ "channellist_show_infobox", CompareOp::Ne, 0, NULL, 0 }
};

const Condition kScreensaverOn[] =
{
	{ "screensaver_delay", CompareOp::Ne, 0, NULL, 0 }
};

// The image mode is the only one that reads a directory of its own.
const Condition kScreensaverImage[] =
{
	{ "screensaver_delay", CompareOp::Ne, 0, NULL, 0 },
	{ "screensaver_mode", CompareOp::Eq, 0, NULL, 0 }
};

// src/gui/infoicons_setup.cpp:62
const EnumValue kInfoiconsSkin[] =
{
	{ 0, "infoicons_static" },
	{ 1, "infoicons_infoviewer" },
	{ 2, "infoicons_popup" }
};

/* A choice and not a flag, because the screen names the two by what pressing
   the item does rather than by an on and an off, and a flag row would carry the
   values while nothing held the words to the screen.
   src/gui/widget/menue_options.h:29 */
const EnumValue kInfoiconsMode[] =
{
	{ 0, "options.start" },
	{ 1, "options.stop" }
};

// The skin is offered only while the icons are off.
// src/gui/infoicons_setup.cpp:156
const Condition kIconsOff[] =
{
	{ "mode_icons", CompareOp::Eq, 0, NULL, 0 }
};

// And the icons only where the skin is not the one the infobar draws.
// src/gui/infoicons_setup.cpp:160
const Condition kSkinNotInfoviewer[] =
{
	{ "mode_icons_skin", CompareOp::Ne, 1, NULL, 0 }
};

/* The size the box draws its own screen at, asked of and told to the object
   that keeps it. Not the member of the settings named after it: that one is
   filled at load and is not what the save writes, src/neutrino.cpp:2026, so a
   value written into it would be gone at the next save. */
bool askOsdResolution(long &out)
{
	int mode = 0;
	if (osdResolutionSource().read(mode) != Status::Ok)
		return false;
	out = mode;
	return true;
}

bool tellOsdResolution(long value)
{
	return osdResolutionSource().write((int) value) == Status::Ok;
}

const Descriptor kOsd[] =
{
	// src/neutrino.cpp:702 src/gui/osd_setup.cpp:711
	{
		"radiotext_enable", ValueType::Bool, "osd",
		"miscsettings.radiotext", "menu.hint_infobar_radiotext",
		0, 1, NULL, 0, 1, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(radiotext_enable)
	},
	// src/neutrino.cpp:1327 src/gui/osd_setup.cpp:716
	{
		"scrambled_message", ValueType::Bool, "osd",
		"extra.scrambled_message", "menu.hint_scrambled_message",
		0, 1, NULL, 0, 0, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(scrambled_message)
	},
	// src/neutrino.cpp:938 src/gui/osd_setup.cpp:762
	{
		"widget_fade", ValueType::Bool, "osd",
		"colormenu.fade", "menu.hint_fade",
		0, 1, NULL, 0, 0, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(widget_fade)
	},
	/* The two below are one item on the screen, edited by a key loop rather
	   than by a chooser, so the range is the one that loop enforces and the
	   label is the one item's. The program falls back to whatever window_size
	   holds, and the value below is what that key falls back to in turn, which
	   is the only part of it a constant can carry. */
	// src/neutrino.cpp:1330 src/neutrino.cpp:1331 src/gui/osd_setup.cpp:770
	// src/gui/osd_setup.cpp:1915 src/driver/fb_generic.h:80
	{
		"window_width", ValueType::Int, "osd",
		"window_size", "menu.hint_window_size",
		50, 100, NULL, 0, 100, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(window_width)
	},
	// src/neutrino.cpp:1330 src/neutrino.cpp:1332 src/gui/osd_setup.cpp:770
	// src/gui/osd_setup.cpp:1919 src/driver/fb_generic.h:80
	{
		"window_height", ValueType::Int, "osd",
		"window_size", "menu.hint_window_size",
		50, 100, NULL, 0, 100, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(window_height)
	},
	// src/neutrino.cpp:1324 src/gui/osd_setup.cpp:774
	{
		"infobar_subchan_disp_pos", ValueType::Enum, "osd",
		"infoviewer.subchan_disp_pos", "menu.hint_subchannel_pos",
		0, 0, COREAPI_VALUES(kSubchanPos), 4, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(infobar_subchan_disp_pos)
	},

	// fonts
	// src/neutrino.cpp:1142 src/gui/osd_setup.cpp:1127
	{
		"font_file", ValueType::String, "osd",
		"colormenu.font", "menu.hint_font_gui",
		0, 0, NULL, 0, 0, FONTDIR "/neutrino.ttf", false, false, COREAPI_ALWAYS,
		COREAPI_TEXT_FIELD(font_file)
	},
	// src/neutrino.cpp:1143 src/gui/osd_setup.cpp:1134
	{
		"font_file_monospace", ValueType::String, "osd",
		"colormenu.font_ttx", "menu.hint_font_ttx",
		0, 0, NULL, 0, 0, FONTDIR "/tuxtxt.ttf", false, false, COREAPI_ALWAYS,
		COREAPI_TEXT_FIELD(font_file_monospace)
	},
	// Per cent of the size each font is configured at.
	// src/neutrino.cpp:1151 src/gui/osd_setup.cpp:307
	{
		"font_scaling_x", ValueType::Int, "osd",
		"fontmenu.scaling_x", "fontmenu.scaling_x_hint2",
		50, 200, NULL, 0, 105, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(font_scaling_x)
	},
	// src/neutrino.cpp:1152 src/gui/osd_setup.cpp:313
	{
		"font_scaling_y", ValueType::Int, "osd",
		"fontmenu.scaling_y", "fontmenu.scaling_y_hint2",
		50, 200, NULL, 0, 105, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(font_scaling_y)
	},

	// menus
	// src/neutrino.cpp:1282 src/gui/osd_setup.cpp:1232 src/gui/widget/menue.h:675
	{
		"menu_pos", ValueType::Enum, "osd",
		"settings.menu_pos", "menu.hint_menu_pos",
		0, 0, COREAPI_VALUES(kMenuPos), 0, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(menu_pos)
	},
	// src/neutrino.cpp:1283 src/gui/osd_setup.cpp:1238
	{
		"show_menu_hints", ValueType::Bool, "osd",
		"settings.menu_hints", "menu.hint_menu_hints",
		0, 1, NULL, 0, 1, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(show_menu_hints)
	},
	// src/neutrino.cpp:1284 src/gui/osd_setup.cpp:1244
	{
		"show_menu_hints_line", ValueType::Bool, "osd",
		"settings.menu_hints_line", "menu.hint_menu_hints_line",
		0, 1, NULL, 0, 0, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(show_menu_hints_line)
	},

	// channel logos
	// src/neutrino.cpp:840 src/gui/osd_setup.cpp:1276
	{
		"logo_hdd_dir", ValueType::String, "osd",
		"miscsettings.infobar_logo_hdd_dir", "menu.hint_infobar_logo_dir",
		0, 0, NULL, 0, 0, TARGET_ROOT "/media/sda1/logos", false, false, COREAPI_ALWAYS,
		COREAPI_TEXT_FIELD(logo_hdd_dir)
	},
	// src/neutrino.cpp:1302 src/gui/osd_setup.cpp:1283
	{
		"channellist_show_channellogo", ValueType::Enum, "osd",
		"channellist.show_channellogo", "menu.hint_channellist_show_channellogo",
		0, 0, COREAPI_VALUES(kChannelLogoPos), 1, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(channellist_show_channellogo)
	},
	// src/neutrino.cpp:1303 src/gui/osd_setup.cpp:1288
	{
		"channellist_show_eventlogo", ValueType::Bool, "osd",
		"channellist.show_eventlogo", "menu.hint_channellist_show_eventlogo",
		0, 1, NULL, 0, 1, NULL, false, false, COREAPI_CONDITIONS(kChannelLogoOn),
		COREAPI_NUMBER_FIELD(channellist_show_eventlogo)
	},

	// infobar
	// src/neutrino.cpp:1317 src/gui/osd_setup.cpp:1304
	{
		"infobar_show", ValueType::Bool, "osd",
		"miscsettings.infobar_show", "menu.hint_infobar_on_epg",
		0, 1, NULL, 0, 0, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(infobar_show)
	},
	// src/neutrino.cpp:1311 src/gui/osd_setup.cpp:1309
	{
		"infobar_buttons_usertitle", ValueType::Bool, "osd",
		"miscsettings.infobar_buttons_usertitle", "menu.hint_infobar_buttons_usertitle",
		0, 1, NULL, 0, 0, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(infobar_buttons_usertitle)
	},
	// src/neutrino.cpp:1310 src/gui/osd_setup.cpp:1314
	{
		"infobar_analogclock", ValueType::Bool, "osd",
		"miscsettings.infobar_analogclock", "menu.hint_infobar_analogclock",
		0, 1, NULL, 0, 0, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(infobar_analogclock)
	},
	// The screen offers this one only where the weather is switched on, which
	// is a key another section declares and a condition cannot name yet.
	// src/neutrino.cpp:1325 src/gui/osd_setup.cpp:1319
	{
		"infobar_weather", ValueType::Bool, "osd",
		"miscsettings.infobar_weather", "menu.hint_infobar_weather",
		0, 1, NULL, 0, 0, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(infobar_weather)
	},
	// src/neutrino.cpp:1319 src/gui/osd_setup.cpp:1326
	{
		"infobar_show_channellogo", ValueType::Enum, "osd",
		"miscsettings.infobar_disp", "menu.hint_infobar_logo",
		0, 0, COREAPI_VALUES(kInfobarDisp), 5, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(infobar_show_channellogo)
	},
	// src/neutrino.cpp:1316 src/gui/osd_setup.cpp:1332
	{
		"infobar_sat_display", ValueType::Bool, "osd",
		"miscsettings.infobar_sat_display", "menu.hint_infobar_sat",
		0, 1, NULL, 0, 1, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(infobar_sat_display)
	},
	// src/neutrino.cpp:1312 src/gui/osd_setup.cpp:1341
	{
		"infobar_casystem_display", ValueType::Enum, "osd",
		"miscsettings.infobar_casystem_display", "menu.hint_infobar_casys",
		0, 0, COREAPI_VALUES(kCaSystem), 0, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(infobar_casystem_display)
	},
	// Both halves of this one are behind #if 0: the menu item that would set it
	// (src/gui/osd_setup.cpp:1346) and the drawing that would read it
	// (src/gui/infoviewer_bb.cpp:971). Written and stored, it reaches nothing
	// until one of those comes back, and the row stays so that it works again
	// when they do.
	// src/neutrino.cpp:1313
	{
		"infobar_casystem_dotmatrix", ValueType::Bool, "osd",
		"miscsettings.infobar_casystem_dotmatrix", "menu.hint_infobar_casys_dotmatrix",
		0, 1, NULL, 0, 0, NULL, false, false, COREAPI_CONDITIONS(kCaSystemDrawn),
		COREAPI_NUMBER_FIELD(infobar_casystem_dotmatrix)
	},
	// src/neutrino.cpp:1314 src/gui/osd_setup.cpp:1355
	{
		"infobar_casystem_frame", ValueType::Bool, "osd",
		"miscsettings.infobar_casystem_frame", "menu.hint_infobar_casys_frame",
		0, 1, NULL, 0, 0, NULL, false, false, COREAPI_CONDITIONS(kCaSystemDrawn),
		COREAPI_NUMBER_FIELD(infobar_casystem_frame)
	},
	// src/neutrino.cpp:469 src/gui/osd_setup.cpp:1362
	{
		"show_ecm_pos", ValueType::Enum, "osd",
		"ecminfo_show", "menu.hint_infobar_ecminfo",
		0, 0, COREAPI_VALUES(kEcmPos), 0, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(show_ecm_pos)
	},
	// src/neutrino.cpp:1322 src/gui/osd_setup.cpp:1369
	{
		"infobar_show_sysfs_hdd", ValueType::Bool, "osd",
		"miscsettings.infobar_show_sysfs_hdd", "menu.hint_infobar_filesys",
		0, 1, NULL, 0, 0, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(infobar_show_sysfs_hdd)
	},
	// src/neutrino.cpp:660 src/gui/osd_setup.cpp:1374
	{
		"hdd_statfs_mode", ValueType::Enum, "osd",
		"hdd_statfs", "menu.hint_hdd_statfs",
		0, 0, COREAPI_VALUES(kHddStatfs), 2, NULL, false, false, COREAPI_CONDITIONS(kSysfsHddOn),
		COREAPI_NUMBER_FIELD(hdd_statfs_mode)
	},
	// The screen offers this one only where the box has a second tuner, which
	// is counted at run time and not a setting to condition on.
	// src/neutrino.cpp:1323 src/gui/osd_setup.cpp:1391
	{
		"infobar_show_tuner", ValueType::Bool, "osd",
		"miscsettings.infobar_show_tuner", "menu.hint_infobar_tuner",
		0, 1, NULL, 0, 1, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(infobar_show_tuner)
	},
	// src/neutrino.cpp:1321 src/gui/osd_setup.cpp:1396
	{
		"infobar_show_res", ValueType::Enum, "osd",
		"miscsettings.infobar_show_res", "menu.hint_infobar_res",
		0, 0, COREAPI_VALUES(kInfobarShowRes), 0, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(infobar_show_res)
	},
	// src/neutrino.cpp:1320 src/gui/osd_setup.cpp:1401
	{
		"infobar_show_dd_available", ValueType::Bool, "osd",
		"miscsettings.infobar_show_dd_available", "menu.hint_infobar_dd",
		0, 1, NULL, 0, 1, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(infobar_show_dd_available)
	},
	// src/neutrino.cpp:1315 src/gui/osd_setup.cpp:1408
	{
		"infobar_progressbar", ValueType::Enum, "osd",
		"miscsettings.progressbar_infobar_position", "menu.hint_progressbar_infobar_position",
		0, 0, COREAPI_VALUES(kProgressbarInfobarPos), 2, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(infobar_progressbar)
	},

	// channel list
	// src/neutrino.cpp:1297 src/gui/osd_setup.cpp:1423
	{
		"channellist_additional", ValueType::Enum, "osd",
		"channellist.additional", "menu.hint_channellist_additional",
		0, 0, COREAPI_VALUES(kChannellistAdditional), 1, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(channellist_additional)
	},
	// src/neutrino.cpp:1298 src/gui/osd_setup.cpp:1428
	{
		"channellist_epgtext_alignment", ValueType::Enum, "osd",
		"miscsettings.channellist_epgtext_alignment", "menu.hint_channellist_epg_align",
		0, 0, COREAPI_VALUES(kEpgtextAlignment), 0, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(channellist_epgtext_alignment)
	},
	// src/neutrino.cpp:1306 src/gui/osd_setup.cpp:1433
	{
		"channellist_show_res_icon", ValueType::Bool, "osd",
		"channellist.show_res_icon", "menu.hint_channellist_show_res_icon",
		0, 1, NULL, 0, 0, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(channellist_show_res_icon)
	},
	// src/neutrino.cpp:1304 src/gui/osd_setup.cpp:1443
	{
		"channellist_show_infobox", ValueType::Bool, "osd",
		"channellist.show_infobox", "menu.hint_channellist_show_infobox",
		0, 1, NULL, 0, 1, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(channellist_show_infobox)
	},
	// src/neutrino.cpp:1299 src/gui/osd_setup.cpp:1448
	{
		"channellist_foot", ValueType::Enum, "osd",
		"channellist.foot", "menu.hint_channellist_foot",
		0, 0, COREAPI_VALUES(kChannellistFoot), 1, NULL, false, false, COREAPI_CONDITIONS(kInfoboxOn),
		COREAPI_NUMBER_FIELD(channellist_foot)
	},
	// src/neutrino.cpp:1305 src/gui/osd_setup.cpp:1454
	{
		"channellist_show_numbers", ValueType::Bool, "osd",
		"channellist.show_channelnumber", "menu.hint_channellist_show_channelnumber",
		0, 1, NULL, 0, 1, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(channellist_show_numbers)
	},

	// event list
	// src/neutrino.cpp:1290 src/gui/osd_setup.cpp:1467
	{
		"eventlist_additional", ValueType::Bool, "osd",
		"eventlist.additional", "menu.hint_eventlist_additional",
		0, 1, NULL, 0, 1, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(eventlist_additional)
	},
	// src/neutrino.cpp:1291 src/gui/osd_setup.cpp:1472
	{
		"eventlist_epgplus", ValueType::Bool, "osd",
		"eventlist.epgplus", "menu.hint_eventlist_epgplus",
		0, 1, NULL, 0, 1, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(eventlist_epgplus)
	},

	// volume
	// src/neutrino.cpp:1278 src/gui/osd_setup.cpp:1485
	{
		"volume_pos", ValueType::Enum, "osd",
		"extra.volume_pos", "menu.hint_volume_pos",
		0, 0, COREAPI_VALUES(kVolumePos), 5, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(volume_pos)
	},
	/* A height in pixels. The floor the screen enforces is the height of the
	   volume icon the running box loaded, so the floor below is the widest one
	   that holds every value the screen could offer rather than the screen's
	   own. */
	// src/neutrino.cpp:1279 src/gui/osd_setup.cpp:1490
	{
		"volume_size", ValueType::Int, "osd",
		"extra.volume_size", "menu.hint_volume_size",
		0, 50, NULL, 0, 26, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(volume_size)
	},
	// src/neutrino.cpp:1277 src/gui/osd_setup.cpp:1497
	{
		"volume_digits", ValueType::Bool, "osd",
		"extra.volume_digits", "menu.hint_volume_digits",
		0, 1, NULL, 0, 1, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(volume_digits)
	},
	// src/neutrino.cpp:1276 src/gui/osd_setup.cpp:1502
	{
		"show_mute_icon", ValueType::Bool, "osd",
		"extra.show_mute_icon", "menu.hint_show_mute_icon",
		0, 1, NULL, 0, 0, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(show_mute_icon)
	},

	// info clock
	// src/neutrino.cpp:1267 src/gui/osd_setup.cpp:1512
	{
		"mode_clock", ValueType::Bool, "osd",
		"miscsettings.infoclock", "menu.hint_clock_mode",
		0, 1, NULL, 0, 0, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(mode_clock)
	},
	// A height in pixels again. The program refuses nought and below and puts
	// the default back, which is the same number as the floor.
	// src/neutrino.cpp:1269 src/gui/osd_setup.cpp:1519
	{
		"infoClockFontSize", ValueType::Int, "osd",
		"clock_size_height", "menu.hint_clock_size",
		30, 120, NULL, 0, 30, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(infoClockFontSize)
	},
	// src/neutrino.cpp:1273 src/gui/osd_setup.cpp:1524
	{
		"infoClockSeconds", ValueType::Bool, "osd",
		"clock_seconds", "menu.hint_clock_seconds",
		0, 1, NULL, 0, 0, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(infoClockSeconds)
	},
	// src/neutrino.cpp:1268 src/gui/osd_setup.cpp:1531
	{
		"infoClockBackground", ValueType::Bool, "osd",
		"clock_background", "menu.hint_clock_background",
		0, 1, NULL, 0, 0, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(infoClockBackground)
	},

	// screen
	// src/neutrino.cpp:1073 src/gui/osd_setup.cpp:1782
	{
		"screen_preset", ValueType::Enum, "osd",
		"colormenu.osd_preset", "menu.hint_osd_preset",
		0, 0, COREAPI_VALUES(kScreenPreset), 0, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(screen_preset)
	},

	// screenshot
	// src/neutrino.cpp:1036 src/gui/osd_setup.cpp:1810
	{
		"screenshot_dir", ValueType::String, "osd",
		"screenshot.defdir", "menu.hint_screenshot_dir",
		0, 0, NULL, 0, 0, TARGET_ROOT "/media/sda1/movies", false, false, COREAPI_ALWAYS,
		COREAPI_TEXT_FIELD(screenshot_dir)
	},
	// src/neutrino.cpp:1037 src/gui/osd_setup.cpp:1814
	{
		"screenshot_count", ValueType::Int, "osd",
		"screenshot.count", "menu.hint_screenshot_count",
		1, 5, NULL, 0, 1, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(screenshot_count)
	},
	/* A number and not a choice. The screen names its three formats in words
	   of its own rather than by a locale, so a choice here would offer names
	   nothing else in the program has, and the range is the enumeration the
	   driver takes. */
	// src/neutrino.cpp:1039 src/gui/osd_setup.cpp:1818 src/driver/screenshot.h:40
	{
		"screenshot_format", ValueType::Int, "osd",
		"screenshot.format", "menu.hint_screenshot_format",
		0, 2, NULL, 0, 1, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(screenshot_format)
	},
	// src/neutrino.cpp:1040 src/gui/osd_setup.cpp:1822
	{
		"screenshot_mode", ValueType::Enum, "osd",
		"screenshot.res", "menu.hint_screenshot_res",
		0, 0, COREAPI_VALUES(kScreenshotMode), 0, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(screenshot_mode)
	},
	// src/neutrino.cpp:1042 src/gui/osd_setup.cpp:1826
	{
		"screenshot_video", ValueType::Bool, "osd",
		"screenshot.video", "menu.hint_screenshot_video",
		0, 1, NULL, 0, 1, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(screenshot_video)
	},
	// src/neutrino.cpp:1041 src/gui/osd_setup.cpp:1830
	{
		"screenshot_scale", ValueType::Bool, "osd",
		"screenshot.scale", "menu.hint_screenshot_scale",
		0, 1, NULL, 0, 0, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(screenshot_scale)
	},
	// src/neutrino.cpp:1038 src/gui/osd_setup.cpp:1834
	{
		"screenshot_cover", ValueType::Bool, "osd",
		"screenshot.cover", "menu.hint_screenshot_cover",
		0, 1, NULL, 0, 0, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(screenshot_cover)
	},

	// screensaver
	// Minutes, and nought is what the screen offers as off.
	// src/neutrino.cpp:730 src/gui/osd_setup.cpp:1857
	{
		"screensaver_delay", ValueType::Int, "osd",
		"screensaver.delay", "menu.hint_screensaver_delay",
		0, 999, NULL, 0, 1, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(screensaver_delay)
	},
	// src/neutrino.cpp:732 src/gui/osd_setup.cpp:1863
	{
		"screensaver_mode", ValueType::Enum, "osd",
		"screensaver.mode", "menu.hint_screensaver_mode",
		0, 0, COREAPI_VALUES(kScreensaverMode), 1, NULL, false, false, COREAPI_CONDITIONS(kScreensaverOn),
		COREAPI_NUMBER_FIELD(screensaver_mode)
	},
	// Seconds, and nought is off again.
	// src/neutrino.cpp:735 src/gui/osd_setup.cpp:1869
	{
		"screensaver_timeout", ValueType::Int, "osd",
		"screensaver.timeout", "menu.hint_screensaver_timeout",
		0, 60, NULL, 0, 10, NULL, false, false, COREAPI_CONDITIONS(kScreensaverOn),
		COREAPI_NUMBER_FIELD(screensaver_timeout)
	},
	// src/neutrino.cpp:731 src/gui/osd_setup.cpp:1876
	{
		"screensaver_dir", ValueType::String, "osd",
		"screensaver.dir", "menu.hint_screensaver_dir",
		0, 0, NULL, 0, 0, ICONSDIR "/screensaver", false, false, COREAPI_CONDITIONS(kScreensaverImage),
		COREAPI_TEXT_FIELD(screensaver_dir)
	},
	// src/neutrino.cpp:734 src/gui/osd_setup.cpp:1882
	{
		"screensaver_random", ValueType::Bool, "osd",
		"screensaver.random", "menu.hint_screensaver_random",
		0, 1, NULL, 0, 0, NULL, false, false, COREAPI_CONDITIONS(kScreensaverImage),
		COREAPI_NUMBER_FIELD(screensaver_random)
	},
	// The item for this one sits in an arm the screen never compiles as well,
	// and the screensaver reads the setting either way.
	// src/neutrino.cpp:733 src/gui/osd_setup.cpp:1889 src/gui/screensaver.cpp:402
	{
		"screensaver_mode_text", ValueType::Bool, "osd",
		"screensaver.enable_text_info", "menu.hint_screensaver_enable_text_info",
		0, 1, NULL, 0, 0, NULL, false, false, COREAPI_CONDITIONS(kScreensaverOn),
		COREAPI_NUMBER_FIELD(screensaver_mode_text)
	},
	// infoicons
	// src/neutrino.cpp:449 src/gui/infoicons_setup.cpp:156
	{
		"mode_icons_skin", ValueType::Enum, "osd",
		"infoicons_skin", "menu.hint_infoicons_skin",
		0, 0, COREAPI_ENUM(kInfoiconsSkin), 0, NULL, false, false,
		COREAPI_CONDITIONS(kIconsOff),
		COREAPI_NUMBER_FIELD(mode_icons_skin)
	},
	// src/neutrino.cpp:447 src/gui/infoicons_setup.cpp:160
	{
		"mode_icons", ValueType::Enum, "osd",
		"infoicons_modeicon", "menu.hint_infoicons_modeicon",
		0, 0, COREAPI_ENUM(kInfoiconsMode), 0, NULL, false, false,
		COREAPI_CONDITIONS(kSkinNotInfoviewer),
		COREAPI_NUMBER_FIELD(mode_icons)
	},
	// src/neutrino.cpp:448 src/gui/infoicons_setup.cpp:204
	{
		"mode_icons_background", ValueType::Bool, "osd",
		"infoicons_background", "menu.hint_infoicons_background",
		0, 1, NULL, 0, 0, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(mode_icons_background)
	},
	/* The corners of the drawn area, four of them for each pairing of an OSD
	   resolution and a preset. The screen is a widget the viewer drags rather
	   than a menu, src/gui/screensetup.cpp:198-249, so all sixteen share the
	   two words it draws beside the corners and the key is what tells them
	   apart: a names the full pixel preset and b the other,
	   src/gui/osd_setup.h:105, while 0 names the 720 line OSD and 1 the 1080
	   line one, src/gui/osd_helpers.h:8.

	   Each needs a restart. The box copies the quartet its resolution and
	   preset name into the four values it draws with,
	   src/neutrino.cpp:1414-1462, and that runs at start and where the
	   resolution or the preset changes, not where one of these does.

	   The upper left corner is held to 200 in both directions and the lower
	   right to at least 400, src/gui/screensetup.cpp:201-249; the ceiling of
	   the lower right is the OSD's own size, which the suffix names. */
	/* Which of the two sizes the box draws its own screen at, as the mode the
	   program stores and not as a place in the list the screen builds:
	   OSDMODE_720 and OSDMODE_1080, src/gui/osd_helpers.h:7. Every driver
	   builds its list in that order and what reads the value reads it as a
	   mode, src/neutrino.cpp:1431.

	   A number and not a choice: the screen names its two with the sizes in
	   pixels it reads off the framebuffer, src/gui/osd_setup.cpp:731, and the
	   program has no wording for either.

	   The default is the one the pass that loads the settings falls back to,
	   src/neutrino.cpp:1060. The pass that sets the framebuffer up reads the
	   same key with a different fallback, :2752, which is a disagreement in the
	   program and not in this row: whoever changes one belongs at the other. */
	// src/neutrino.cpp:1064 src/gui/osd_setup.cpp:744
	{
		"osd_resolution", ValueType::Int, "osd",
		"colormenu.osd_resolution", "menu.hint_osd_resolution",
		0, 1, NULL, 0,
#if HAVE_ARM_HARDWARE || (HAVE_CST_HARDWARE && defined(BOXMODEL_CST_HD2))
		1,
#else
		0,
#endif
		NULL, false, false, COREAPI_ALWAYS,
		COREAPI_SERVICE_FIELD(osd_resolution, askOsdResolution, tellOsdResolution)
	},
	// src/neutrino.cpp:1054 src/gui/screensetup.cpp:225
	{
		"screen_StartX_a_0", ValueType::Int, "osd",
		"screensetup.upperleft", NULL,
		0, 200, NULL, 0, 0, NULL, true, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(screen_StartX_a_0)
	},
	// src/neutrino.cpp:1055 src/gui/screensetup.cpp:201
	{
		"screen_StartY_a_0", ValueType::Int, "osd",
		"screensetup.upperleft", NULL,
		0, 200, NULL, 0, 0, NULL, true, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(screen_StartY_a_0)
	},
	// src/neutrino.cpp:1056 src/gui/screensetup.cpp:239
	{
		"screen_EndX_a_0", ValueType::Int, "osd",
		"screensetup.lowerright", NULL,
		400, 1279, NULL, 0, 1279, NULL, true, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(screen_EndX_a_0)
	},
	// src/neutrino.cpp:1057 src/gui/screensetup.cpp:212
	{
		"screen_EndY_a_0", ValueType::Int, "osd",
		"screensetup.lowerright", NULL,
		400, 719, NULL, 0, 719, NULL, true, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(screen_EndY_a_0)
	},
	// src/neutrino.cpp:1058 src/gui/screensetup.cpp:225
	{
		"screen_StartX_a_1", ValueType::Int, "osd",
		"screensetup.upperleft", NULL,
		0, 200, NULL, 0, 0, NULL, true, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(screen_StartX_a_1)
	},
	// src/neutrino.cpp:1059 src/gui/screensetup.cpp:201
	{
		"screen_StartY_a_1", ValueType::Int, "osd",
		"screensetup.upperleft", NULL,
		0, 200, NULL, 0, 0, NULL, true, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(screen_StartY_a_1)
	},
	// src/neutrino.cpp:1060 src/gui/screensetup.cpp:239
	{
		"screen_EndX_a_1", ValueType::Int, "osd",
		"screensetup.lowerright", NULL,
		400, 1919, NULL, 0, 1919, NULL, true, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(screen_EndX_a_1)
	},
	// src/neutrino.cpp:1061 src/gui/screensetup.cpp:212
	{
		"screen_EndY_a_1", ValueType::Int, "osd",
		"screensetup.lowerright", NULL,
		400, 1079, NULL, 0, 1079, NULL, true, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(screen_EndY_a_1)
	},
	// src/neutrino.cpp:1064 src/gui/screensetup.cpp:225
	{
		"screen_StartX_b_0", ValueType::Int, "osd",
		"screensetup.upperleft", NULL,
		0, 200, NULL, 0, 22, NULL, true, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(screen_StartX_b_0)
	},
	// src/neutrino.cpp:1065 src/gui/screensetup.cpp:201
	{
		"screen_StartY_b_0", ValueType::Int, "osd",
		"screensetup.upperleft", NULL,
		0, 200, NULL, 0, 12, NULL, true, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(screen_StartY_b_0)
	},
	// src/neutrino.cpp:1066 src/gui/screensetup.cpp:239
	{
		"screen_EndX_b_0", ValueType::Int, "osd",
		"screensetup.lowerright", NULL,
		400, 1279, NULL, 0, 1236, NULL, true, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(screen_EndX_b_0)
	},
	// src/neutrino.cpp:1067 src/gui/screensetup.cpp:212
	{
		"screen_EndY_b_0", ValueType::Int, "osd",
		"screensetup.lowerright", NULL,
		400, 719, NULL, 0, 695, NULL, true, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(screen_EndY_b_0)
	},
	// src/neutrino.cpp:1068 src/gui/screensetup.cpp:225
	{
		"screen_StartX_b_1", ValueType::Int, "osd",
		"screensetup.upperleft", NULL,
		0, 200, NULL, 0, 33, NULL, true, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(screen_StartX_b_1)
	},
	// src/neutrino.cpp:1069 src/gui/screensetup.cpp:201
	{
		"screen_StartY_b_1", ValueType::Int, "osd",
		"screensetup.upperleft", NULL,
		0, 200, NULL, 0, 18, NULL, true, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(screen_StartY_b_1)
	},
	// src/neutrino.cpp:1070 src/gui/screensetup.cpp:239
	{
		"screen_EndX_b_1", ValueType::Int, "osd",
		"screensetup.lowerright", NULL,
		400, 1919, NULL, 0, 1854, NULL, true, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(screen_EndX_b_1)
	},
	// src/neutrino.cpp:1071 src/gui/screensetup.cpp:212
	{
		"screen_EndY_b_1", ValueType::Int, "osd",
		"screensetup.lowerright", NULL,
		400, 1079, NULL, 0, 1043, NULL, true, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(screen_EndY_b_1)
	},

	/* Whether the infobar shows the module line at all. The screen has no item
	   for it and derives it from the position beside it,
	   src/gui/osd_setup.cpp:1665, so writing the position alone leaves this one
	   as it was and the line does not follow. The infobar reads it,
	   src/gui/infoviewer_bb.cpp. */
	// src/neutrino.cpp:468
	{
		"show_ecm", ValueType::Bool, "osd",
		NULL, NULL,
		0, 1, NULL, 0, 0, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(show_ecm)
	},
	/* Whether the infobar carries the channel description,
	   src/gui/infoviewer.cpp. No item names it. */
	// src/neutrino.cpp:1318
	{
		"infobar_show_channeldesc", ValueType::Bool, "osd",
		NULL, NULL,
		0, 1, NULL, 0, 0, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(infobar_show_channeldesc)
	},
	/* The name of the colour theme, picked from a file list rather than typed,
	   src/gui/themes.cpp, and read once at start. Its default is a migration
	   name where the box has no settings file yet and empty otherwise, which is
	   what every box that has saved once gets and what the row states.
	   src/neutrino.cpp:408 */
	// src/neutrino.cpp:408
	{
		"theme_name", ValueType::String, "osd",
		NULL, NULL,
		0, 0, NULL, 0, 0, "", true, false, COREAPI_ALWAYS,
		COREAPI_TEXT_FIELD(theme_name)
	},
};

} // anonymous namespace

const Descriptor *settingsTableOsd(size_t &count)
{
	count = sizeof(kOsd) / sizeof(kOsd[0]);
	return kOsd;
}

} // namespace coreapi
