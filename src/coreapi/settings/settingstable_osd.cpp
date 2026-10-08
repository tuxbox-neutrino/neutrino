/*
 * settingstable_osd.cpp - on screen display settings, one row per field
 *
 * Copyright (C) 2026 NI-Team
 * Copyright (C) 2026 Tuxbox-Project (additions, fixes)
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
#include "predicates.h"
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
constexpr EnumValue kSubchanPos[] =
{
	option(0).label("settings.pos_top_right"),
	option(1).label("settings.pos_top_left"),
	option(2).label("settings.pos_bottom_left"),
	option(3).label("settings.pos_bottom_right"),
	option(4).label("infoviewer.subchan_infobar")
};

// src/gui/osd_setup.cpp:498
constexpr EnumValue kMenuPos[] =
{
	option(0).label("settings.pos_center"),
	option(1).label("settings.pos_top_left"),
	option(2).label("settings.pos_top_right"),
	option(3).label("settings.pos_bottom_left"),
	option(4).label("settings.pos_bottom_right")
};

// The gap is the program's: the three positions are the bits of a mask.
// src/gui/osd_setup.cpp:586 src/gui/components/cc_frm_header.h:44
constexpr EnumValue kChannelLogoPos[] =
{
	option(0).label("options.off"),
	option(1).label("settings.pos_right"),
	option(2).label("settings.pos_left"),
	option(4).label("settings.pos_center")
};

// src/gui/osd_setup.cpp:1203
constexpr EnumValue kInfobarDisp[] =
{
	option(0).label("miscsettings.infobar_disp_0"),
	option(1).label("miscsettings.infobar_disp_1"),
	option(2).label("miscsettings.infobar_disp_2"),
	option(3).label("miscsettings.infobar_disp_3"),
	option(4).label("miscsettings.infobar_disp_4"),
	option(5).label("miscsettings.infobar_disp_5"),
	option(6).label("miscsettings.infobar_disp_6")
};

// Nought is on here and the last value is off, which is why this is a choice
// and not a flag.
// src/gui/osd_setup.cpp:460
constexpr EnumValue kCaSystem[] =
{
	option(0).label("options.on"),
	option(1).label("miscsettings.infobar_casystem_mode"),
	option(2).label("miscsettings.infobar_casystem_mini"),
	option(3).label("options.off")
};

// src/gui/osd_setup.cpp:1258
constexpr EnumValue kEcmPos[] =
{
	option(0).label("options.off"),
	option(1).label("settings.pos_top_left"),
	option(2).label("settings.pos_top_center"),
	option(3).label("settings.pos_top_right")
};

// src/gui/osd_setup.cpp:1250 src/system/settings.h:487
constexpr EnumValue kHddStatfs[] =
{
	option(0).label("options.off"),
	option(1).label("hdd_statfs_always"),
	option(2).label("hdd_statfs_recording")
};

// Nought is on here as well.
// src/gui/osd_setup.cpp:508
constexpr EnumValue kInfobarShowRes[] =
{
	option(0).label("options.on"),
	option(1).label("miscsettings.infobar_show_res_simple"),
	option(2).label("options.off")
};

// src/gui/osd_setup.cpp:1215
constexpr EnumValue kProgressbarInfobarPos[] =
{
	option(0).label("miscsettings.progressbar_infobar_position_0"),
	option(1).label("miscsettings.progressbar_infobar_position_1"),
	option(2).label("miscsettings.progressbar_infobar_position_2"),
	option(3).label("miscsettings.progressbar_infobar_position_3")
};

// src/gui/osd_setup.cpp:516
constexpr EnumValue kChannellistAdditional[] =
{
	option(0).label("channellist.additional_off"),
	option(1).label("channellist.additional_on"),
	option(2).label("channellist.additional_on_minitv")
};

// src/gui/osd_setup.cpp:532 src/gui/channellist.h:68
constexpr EnumValue kEpgtextAlignment[] =
{
	option(0).label("channellist.epgtext_align_left_middle"),
	option(1).label("channellist.epgtext_align_left_bottom"),
	option(2).label("channellist.epgtext_align_right_middle"),
	option(3).label("channellist.epgtext_align_right_bottom")
};

// src/gui/osd_setup.cpp:524
constexpr EnumValue kChannellistFoot[] =
{
	option(0).label("channellist.foot_freq"),
	option(1).label("channellist.foot_next"),
	option(2).label("channellist.foot_off")
};

// src/gui/osd_setup.cpp:486 src/gui/volumebar.h:78
constexpr EnumValue kVolumePos[] =
{
	option(0).label("settings.pos_top_right"),
	option(1).label("settings.pos_top_left"),
	option(2).label("settings.pos_bottom_left"),
	option(3).label("settings.pos_bottom_right"),
	option(4).label("settings.pos_top_center"),
	option(5).label("settings.pos_bottom_center"),
	option(6).label("settings.pos_higher_center")
};

// src/gui/osd_setup.cpp:453 src/gui/osd_setup.h:105
constexpr EnumValue kScreenPreset[] =
{
	option(0).label("osd.preset_screen_a"),
	option(1).label("osd.preset_screen_b")
};

// src/gui/osd_setup.cpp:1797
constexpr EnumValue kScreenshotMode[] =
{
	option(0).label("screenshot.tv"),
	option(1).label("screenshot.osd")
};

// src/gui/osd_setup.cpp:1840 src/gui/screensaver.h:85
constexpr EnumValue kScreensaverMode[] =
{
	option(0).label("screensaver.mode_image"),
	option(1).label("screensaver.mode_clock"),
	option(2).label("screensaver.mode_clock_color")
};

// The screen hands each of these in as the flag that makes the item editable.
constexpr Condition kChannelLogoOn[] =
{
	when("channellist_show_channellogo").isNot(0)
};

// Two and three are the mini bar and off, and neither has a frame to draw.
constexpr Condition kCaSystemDrawn[] =
{
	when("infobar_casystem_display").below(2)
};

constexpr Condition kSysfsHddOn[] =
{
	when("infobar_show_sysfs_hdd").isNot(0)
};

constexpr Condition kInfoboxOn[] =
{
	when("channellist_show_infobox").isNot(0)
};

constexpr Condition kScreensaverOn[] =
{
	when("screensaver_delay").isNot(0)
};

// The image mode is the only one that reads a directory of its own.
constexpr Condition kScreensaverImage[] =
{
	when("screensaver_delay").isNot(0),
	when("screensaver_mode").is(0)
};

// src/gui/infoicons_setup.cpp:62
constexpr EnumValue kInfoiconsSkin[] =
{
	option(0).label("infoicons_static"),
	option(1).label("infoicons_infoviewer"),
	option(2).label("infoicons_popup")
};

/* A choice and not a flag, because the screen names the two by what pressing
   the item does rather than by an on and an off, and a flag row would carry the
   values while nothing held the words to the screen.
   src/gui/widget/menue_options.h:29 */
constexpr EnumValue kInfoiconsMode[] =
{
	option(0).label("options.start"),
	option(1).label("options.stop")
};

// The skin is offered only while the icons are off.
// src/gui/infoicons_setup.cpp:156
constexpr Condition kIconsOff[] =
{
	when("mode_icons").is(0)
};

// And the icons only where the skin is not the one the infobar draws.
// src/gui/infoicons_setup.cpp:160
constexpr Condition kSkinNotInfoviewer[] =
{
	when("mode_icons_skin").isNot(1)
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

/* VIDEO_STD_AUTO of the video library this build has, the last of its list of
   standards, which each family numbers differently. Written out because the
   library's header is not one every build of this table can include. */
#if HAVE_CST_HARDWARE && defined(BOXMODEL_CST_HD2)
constexpr long kVideoStdAuto = 16;	// hd2/libcoolstream/video_cs.h
#elif HAVE_ARM_HARDWARE || HAVE_MIPS_HARDWARE
constexpr long kVideoStdAuto = 20;	// libarmbox and libmipsbox video_lib.h
#else
constexpr long kVideoStdAuto = 12;	// hd1/libcoolstream/video_cs.h, libgeneric-pc and libraspi video_lib.h
#endif

// The larger size is not offered while the box picks the video standard itself.
constexpr Condition kVideoModeNotAuto[] =
{
	when("video_Mode").isNot(kVideoStdAuto)
};

constexpr Descriptor kOsd[] =
{
	// src/neutrino.cpp:702 src/gui/osd_setup.cpp:711
	boolRow("radiotext_enable")
		.section("osd")
		.label("miscsettings.radiotext")
		.hint("menu.hint_infobar_radiotext")
		.defaultValue(1)
		.field(COREAPI_NUMBER_FIELD(radiotext_enable)),
	// src/neutrino.cpp:1327 src/gui/osd_setup.cpp:716
	// Local resolution: this fork falls back to the message being on, not the source's off.
	boolRow("scrambled_message")
		.section("osd")
		.label("extra.scrambled_message")
		.hint("menu.hint_scrambled_message")
		.defaultValue(1)
		.field(COREAPI_NUMBER_FIELD(scrambled_message)),
	// src/neutrino.cpp:938 src/gui/osd_setup.cpp:762
	boolRow("widget_fade")
		.section("osd")
		.label("colormenu.fade")
		.hint("menu.hint_fade")
		.defaultValue(0)
		.field(COREAPI_NUMBER_FIELD(widget_fade)),
	/* The two below are one item on the screen, edited by a key loop rather
	   than by a chooser, so the range is the one that loop enforces and the
	   label is the one item's. The program falls back to whatever window_size
	   holds, and the value below is what that key falls back to in turn, which
	   is the only part of it a constant can carry. */
	// src/neutrino.cpp:1330 src/neutrino.cpp:1331 src/gui/osd_setup.cpp:770
	// src/gui/osd_setup.cpp:1915 src/driver/fb_generic.h:80
	intRow("window_width")
		.section("osd")
		.label("window_size")
		.hint("menu.hint_window_size")
		.range(50, 100)
		.defaultValue(100)
		.field(COREAPI_NUMBER_FIELD(window_width)),
	// src/neutrino.cpp:1330 src/neutrino.cpp:1332 src/gui/osd_setup.cpp:770
	// src/gui/osd_setup.cpp:1919 src/driver/fb_generic.h:80
	intRow("window_height")
		.section("osd")
		.label("window_size")
		.hint("menu.hint_window_size")
		.range(50, 100)
		.defaultValue(100)
		.field(COREAPI_NUMBER_FIELD(window_height)),
	// src/neutrino.cpp:1324 src/gui/osd_setup.cpp:774
	enumRow("infobar_subchan_disp_pos")
		.section("osd")
		.label("infoviewer.subchan_disp_pos")
		.hint("menu.hint_subchannel_pos")
		.defaultValue(4)
		.values(kSubchanPos)
		.field(COREAPI_NUMBER_FIELD(infobar_subchan_disp_pos)),

	// fonts
	// src/neutrino.cpp:1142 src/gui/osd_setup.cpp:1127
	textRow("font_file")
		.section("osd")
		.label("colormenu.font")
		.hint("menu.hint_font_gui")
		.defaultValue(FONTDIR "/neutrino.ttf")
		.text(kRuleFontFile)
		.field(COREAPI_TEXT_FIELD(font_file)),
	// src/neutrino.cpp:1143 src/gui/osd_setup.cpp:1134
	textRow("font_file_monospace")
		.section("osd")
		.label("colormenu.font_ttx")
		.hint("menu.hint_font_ttx")
		.defaultValue(FONTDIR "/tuxtxt.ttf")
		.text(kRuleFontFile)
		.field(COREAPI_TEXT_FIELD(font_file_monospace)),
	// Per cent of the size each font is configured at.
	// src/neutrino.cpp:1151 src/gui/osd_setup.cpp:307
	// Local resolution: this fork falls back to 100 per cent, not the source's 105.
	intRow("font_scaling_x")
		.section("osd")
		.label("fontmenu.scaling_x")
		.hint("fontmenu.scaling_x_hint2")
		.range(50, 200)
		.defaultValue(100)
		.unit("unit.short.percent")
		.field(COREAPI_NUMBER_FIELD(font_scaling_x)),
	// src/neutrino.cpp:1152 src/gui/osd_setup.cpp:313
	// Local resolution: this fork falls back to 100 per cent, not the source's 105.
	intRow("font_scaling_y")
		.section("osd")
		.label("fontmenu.scaling_y")
		.hint("fontmenu.scaling_y_hint2")
		.range(50, 200)
		.defaultValue(100)
		.unit("unit.short.percent")
		.field(COREAPI_NUMBER_FIELD(font_scaling_y)),

	// menus
	// src/neutrino.cpp:1282 src/gui/osd_setup.cpp:1232 src/gui/widget/menue.h:675
	enumRow("menu_pos")
		.section("osd")
		.label("settings.menu_pos")
		.hint("menu.hint_menu_pos")
		.defaultValue(0)
		.values(kMenuPos)
		.field(COREAPI_NUMBER_FIELD(menu_pos)),
	// src/neutrino.cpp:1283 src/gui/osd_setup.cpp:1238
	boolRow("show_menu_hints")
		.section("osd")
		.label("settings.menu_hints")
		.hint("menu.hint_menu_hints")
		.defaultValue(1)
		.field(COREAPI_NUMBER_FIELD(show_menu_hints)),
	// src/neutrino.cpp:1284 src/gui/osd_setup.cpp:1244

	// channel logos
	// src/neutrino.cpp:840 src/gui/osd_setup.cpp:1276
	textRow("logo_hdd_dir")
		.section("osd")
		.label("miscsettings.infobar_logo_hdd_dir")
		.hint("menu.hint_infobar_logo_dir")
		.defaultValue(TARGET_ROOT "/media/sda1/logos")
		.text(kRuleDirectoryExists)
		.field(COREAPI_TEXT_FIELD(logo_hdd_dir)),
	// src/neutrino.cpp:1302 src/gui/osd_setup.cpp:1283
	enumRow("channellist_show_channellogo")
		.section("osd")
		.label("channellist.show_channellogo")
		.hint("menu.hint_channellist_show_channellogo")
		.defaultValue(1)
		.values(kChannelLogoPos)
		.field(COREAPI_NUMBER_FIELD(channellist_show_channellogo)),
	// src/neutrino.cpp:1303 src/gui/osd_setup.cpp:1288
	boolRow("channellist_show_eventlogo")
		.section("osd")
		.label("channellist.show_eventlogo")
		.hint("menu.hint_channellist_show_eventlogo")
		.defaultValue(1)
		.changeableWhen(kChannelLogoOn)
		.field(COREAPI_NUMBER_FIELD(channellist_show_eventlogo)),

	// infobar
	// src/neutrino.cpp:1317 src/gui/osd_setup.cpp:1304
	boolRow("infobar_show")
		.section("osd")
		.label("miscsettings.infobar_show")
		.hint("menu.hint_infobar_on_epg")
		.defaultValue(0)
		.field(COREAPI_NUMBER_FIELD(infobar_show)),
	// src/neutrino.cpp:1311 src/gui/osd_setup.cpp:1309
	boolRow("infobar_buttons_usertitle")
		.section("osd")
		.label("miscsettings.infobar_buttons_usertitle")
		.hint("menu.hint_infobar_buttons_usertitle")
		.defaultValue(0)
		.field(COREAPI_NUMBER_FIELD(infobar_buttons_usertitle)),
	// src/neutrino.cpp:1310 src/gui/osd_setup.cpp:1314
	// The screen offers this one only where the weather is switched on, which
	// is a key another section declares and a condition cannot name yet.
	// src/neutrino.cpp:1325 src/gui/osd_setup.cpp:1319
	// src/neutrino.cpp:1319 src/gui/osd_setup.cpp:1326
	// Local resolution: this fork falls back to mode 3, not the source's 5.
	enumRow("infobar_show_channellogo")
		.section("osd")
		.label("miscsettings.infobar_disp")
		.hint("menu.hint_infobar_logo")
		.defaultValue(3)
		.values(kInfobarDisp)
		.field(COREAPI_NUMBER_FIELD(infobar_show_channellogo)),
	// src/neutrino.cpp:1316 src/gui/osd_setup.cpp:1332
	boolRow("infobar_sat_display")
		.section("osd")
		.label("miscsettings.infobar_sat_display")
		.hint("menu.hint_infobar_sat")
		.defaultValue(1)
		.field(COREAPI_NUMBER_FIELD(infobar_sat_display)),
	// src/neutrino.cpp:1312 src/gui/osd_setup.cpp:1341
	// Local resolution: this fork falls back to the mini display, not the source's full one.
	enumRow("infobar_casystem_display")
		.section("osd")
		.label("miscsettings.infobar_casystem_display")
		.hint("menu.hint_infobar_casys")
		.defaultValue(2)
		.values(kCaSystem)
		.field(COREAPI_NUMBER_FIELD(infobar_casystem_display)),
	// Both halves of this one are behind #if 0: the menu item that would set it
	// (src/gui/osd_setup.cpp:1346) and the drawing that would read it
	// (src/gui/infoviewer_bb.cpp:971). Written and stored, it reaches nothing
	// until one of those comes back, and the row stays so that it works again
	// when they do.
	// src/neutrino.cpp:1313
	boolRow("infobar_casystem_dotmatrix")
		.section("osd")
		.label("miscsettings.infobar_casystem_dotmatrix")
		.hint("menu.hint_infobar_casys_dotmatrix")
		.defaultValue(0)
		.changeableWhen(kCaSystemDrawn)
		.field(COREAPI_NUMBER_FIELD(infobar_casystem_dotmatrix)),
	// src/neutrino.cpp:1314 src/gui/osd_setup.cpp:1355
	// Local resolution: this fork falls back to the frame being on, not the source's off.
	boolRow("infobar_casystem_frame")
		.section("osd")
		.label("miscsettings.infobar_casystem_frame")
		.hint("menu.hint_infobar_casys_frame")
		.defaultValue(1)
		.changeableWhen(kCaSystemDrawn)
		.field(COREAPI_NUMBER_FIELD(infobar_casystem_frame)),
	// src/neutrino.cpp:469 src/gui/osd_setup.cpp:1362
	// src/neutrino.cpp:1322 src/gui/osd_setup.cpp:1369
	// Local resolution: this fork falls back to the gauge being shown, not the source's off.
	boolRow("infobar_show_sysfs_hdd")
		.section("osd")
		.label("miscsettings.infobar_show_sysfs_hdd")
		.hint("menu.hint_infobar_filesys")
		.defaultValue(1)
		.field(COREAPI_NUMBER_FIELD(infobar_show_sysfs_hdd)),
	// src/neutrino.cpp:660 src/gui/osd_setup.cpp:1374
	enumRow("hdd_statfs_mode")
		.section("osd")
		.label("hdd_statfs")
		.hint("menu.hint_hdd_statfs")
		.defaultValue(2)
		.values(kHddStatfs)
		.changeableWhen(kSysfsHddOn)
		.field(COREAPI_NUMBER_FIELD(hdd_statfs_mode)),
	// The screen offers this one only where the box has a second tuner, which
	// is counted at run time and not a setting to condition on.
	// src/neutrino.cpp:1323 src/gui/osd_setup.cpp:1391
	boolRow("infobar_show_tuner")
		.section("osd")
		.label("miscsettings.infobar_show_tuner")
		.hint("menu.hint_infobar_tuner")
		.defaultValue(1)
		.field(COREAPI_NUMBER_FIELD_ON(infobar_show_tuner, severalTunersFitted, NULL)),
	// src/neutrino.cpp:1321 src/gui/osd_setup.cpp:1396
	enumRow("infobar_show_res")
		.section("osd")
		.label("miscsettings.infobar_show_res")
		.hint("menu.hint_infobar_res")
		.defaultValue(0)
		.values(kInfobarShowRes)
		.field(COREAPI_NUMBER_FIELD(infobar_show_res)),
	// src/neutrino.cpp:1320 src/gui/osd_setup.cpp:1401
	boolRow("infobar_show_dd_available")
		.section("osd")
		.label("miscsettings.infobar_show_dd_available")
		.hint("menu.hint_infobar_dd")
		.defaultValue(1)
		.field(COREAPI_NUMBER_FIELD(infobar_show_dd_available)),
	// src/neutrino.cpp:1315 src/gui/osd_setup.cpp:1408
	// Local resolution: this fork falls back to position 1, not the source's 2.
	enumRow("infobar_progressbar")
		.section("osd")
		.label("miscsettings.progressbar_infobar_position")
		.hint("menu.hint_progressbar_infobar_position")
		.defaultValue(1)
		.values(kProgressbarInfobarPos)
		.field(COREAPI_NUMBER_FIELD(infobar_progressbar)),

	// channel list
	// src/neutrino.cpp:1297 src/gui/osd_setup.cpp:1423
	enumRow("channellist_additional")
		.section("osd")
		.label("channellist.additional")
		.hint("menu.hint_channellist_additional")
		.defaultValue(1)
		.values(kChannellistAdditional)
		.field(COREAPI_NUMBER_FIELD(channellist_additional)),
	// src/neutrino.cpp:1298 src/gui/osd_setup.cpp:1428
	enumRow("channellist_epgtext_alignment")
		.section("osd")
		.label("miscsettings.channellist_epgtext_alignment")
		.hint("menu.hint_channellist_epg_align")
		.defaultValue(0)
		.values(kEpgtextAlignment)
		.field(COREAPI_NUMBER_FIELD(channellist_epgtext_alignment)),
	// src/neutrino.cpp:1306 src/gui/osd_setup.cpp:1433
	boolRow("channellist_show_res_icon")
		.section("osd")
		.label("channellist.show_res_icon")
		.hint("menu.hint_channellist_show_res_icon")
		.defaultValue(0)
		.field(COREAPI_NUMBER_FIELD(channellist_show_res_icon)),
	// src/neutrino.cpp:1304 src/gui/osd_setup.cpp:1443
	boolRow("channellist_show_infobox")
		.section("osd")
		.label("channellist.show_infobox")
		.hint("menu.hint_channellist_show_infobox")
		.defaultValue(1)
		.field(COREAPI_NUMBER_FIELD(channellist_show_infobox)),
	// src/neutrino.cpp:1299 src/gui/osd_setup.cpp:1448
	enumRow("channellist_foot")
		.section("osd")
		.label("channellist.foot")
		.hint("menu.hint_channellist_foot")
		.defaultValue(1)
		.values(kChannellistFoot)
		.changeableWhen(kInfoboxOn)
		.field(COREAPI_NUMBER_FIELD(channellist_foot)),
	// src/neutrino.cpp:1305 src/gui/osd_setup.cpp:1454
	boolRow("channellist_show_numbers")
		.section("osd")
		.label("channellist.show_channelnumber")
		.hint("menu.hint_channellist_show_channelnumber")
		.defaultValue(1)
		.field(COREAPI_NUMBER_FIELD(channellist_show_numbers)),

	// event list
	// src/neutrino.cpp:1290 src/gui/osd_setup.cpp:1467
	boolRow("eventlist_additional")
		.section("osd")
		.label("eventlist.additional")
		.hint("menu.hint_eventlist_additional")
		.defaultValue(1)
		.field(COREAPI_NUMBER_FIELD(eventlist_additional)),
	// src/neutrino.cpp:1291 src/gui/osd_setup.cpp:1472
	boolRow("eventlist_epgplus")
		.section("osd")
		.label("eventlist.epgplus")
		.hint("menu.hint_eventlist_epgplus")
		.defaultValue(1)
		.field(COREAPI_NUMBER_FIELD(eventlist_epgplus)),

	// volume
	// src/neutrino.cpp:1278 src/gui/osd_setup.cpp:1485
	enumRow("volume_pos")
		.section("osd")
		.label("extra.volume_pos")
		.hint("menu.hint_volume_pos")
		.defaultValue(5)
		.values(kVolumePos)
		.field(COREAPI_NUMBER_FIELD(volume_pos)),
	/* A height in pixels. The floor the screen enforces is the height of the
	   volume icon the running box loaded, so the floor below is the widest one
	   that holds every value the screen could offer rather than the screen's
	   own. */
	// src/neutrino.cpp:1279 src/gui/osd_setup.cpp:1490
	intRow("volume_size")
		.section("osd")
		.label("extra.volume_size")
		.hint("menu.hint_volume_size")
		.range(0, 50)
		.defaultValue(26)
		.field(COREAPI_NUMBER_FIELD(volume_size)),
	// src/neutrino.cpp:1277 src/gui/osd_setup.cpp:1497
	boolRow("volume_digits")
		.section("osd")
		.label("extra.volume_digits")
		.hint("menu.hint_volume_digits")
		.defaultValue(1)
		.field(COREAPI_NUMBER_FIELD(volume_digits)),
	// src/neutrino.cpp:1276 src/gui/osd_setup.cpp:1502
	boolRow("show_mute_icon")
		.section("osd")
		.label("extra.show_mute_icon")
		.hint("menu.hint_show_mute_icon")
		.defaultValue(0)
		.field(COREAPI_NUMBER_FIELD(show_mute_icon)),

	// info clock
	// src/neutrino.cpp:1267 src/gui/osd_setup.cpp:1512
	boolRow("mode_clock")
		.section("osd")
		.label("miscsettings.infoclock")
		.hint("menu.hint_clock_mode")
		.defaultValue(0)
		.field(COREAPI_NUMBER_FIELD(mode_clock)),
	// A height in pixels again. The program refuses nought and below and puts
	// the default back, which is the same number as the floor.
	// src/neutrino.cpp:1269 src/gui/osd_setup.cpp:1519
	intRow("infoClockFontSize")
		.section("osd")
		.label("clock_size_height")
		.hint("menu.hint_clock_size")
		.range(30, 120)
		.defaultValue(30)
		.field(COREAPI_NUMBER_FIELD(infoClockFontSize)),
	// src/neutrino.cpp:1273 src/gui/osd_setup.cpp:1524
	boolRow("infoClockSeconds")
		.section("osd")
		.label("clock_seconds")
		.hint("menu.hint_clock_seconds")
		.defaultValue(0)
		.field(COREAPI_NUMBER_FIELD(infoClockSeconds)),
	// src/neutrino.cpp:1268 src/gui/osd_setup.cpp:1531
	boolRow("infoClockBackground")
		.section("osd")
		.label("clock_background")
		.hint("menu.hint_clock_background")
		.defaultValue(0)
		.field(COREAPI_NUMBER_FIELD(infoClockBackground)),

	// screen
	// src/neutrino.cpp:1073 src/gui/osd_setup.cpp:1782
	enumRow("screen_preset")
		.section("osd")
		.label("colormenu.osd_preset")
		.hint("menu.hint_osd_preset")
		.defaultValue(0)
		.values(kScreenPreset)
		.field(COREAPI_NUMBER_FIELD(screen_preset)),

	// screenshot
	// src/neutrino.cpp:1036 src/gui/osd_setup.cpp:1810
	textRow("screenshot_dir")
		.section("osd")
		.label("screenshot.defdir")
		.hint("menu.hint_screenshot_dir")
		.defaultValue(TARGET_ROOT "/media/sda1/movies")
		.text(kRuleDirectoryDurable)
		.field(COREAPI_TEXT_FIELD(screenshot_dir)),
	// src/neutrino.cpp:1037 src/gui/osd_setup.cpp:1814
	intRow("screenshot_count")
		.section("osd")
		.label("screenshot.count")
		.hint("menu.hint_screenshot_count")
		.range(1, 5)
		.defaultValue(1)
		.field(COREAPI_NUMBER_FIELD(screenshot_count)),
	/* A number and not a choice. The screen names its three formats in words
	   of its own rather than by a locale, so a choice here would offer names
	   nothing else in the program has, and the range is the enumeration the
	   driver takes. */
	// src/neutrino.cpp:1039 src/gui/osd_setup.cpp:1818 src/driver/screenshot.h:40
	intRow("screenshot_format")
		.section("osd")
		.label("screenshot.format")
		.hint("menu.hint_screenshot_format")
		.range(0, 2)
		.defaultValue(1)
		.field(COREAPI_NUMBER_FIELD(screenshot_format)),
	// src/neutrino.cpp:1040 src/gui/osd_setup.cpp:1822
	enumRow("screenshot_mode")
		.section("osd")
		.label("screenshot.res")
		.hint("menu.hint_screenshot_res")
		.defaultValue(0)
		.values(kScreenshotMode)
		.field(COREAPI_NUMBER_FIELD(screenshot_mode)),
	// src/neutrino.cpp:1042 src/gui/osd_setup.cpp:1826
	boolRow("screenshot_video")
		.section("osd")
		.label("screenshot.video")
		.hint("menu.hint_screenshot_video")
		.defaultValue(1)
		.field(COREAPI_NUMBER_FIELD(screenshot_video)),
	// src/neutrino.cpp:1041 src/gui/osd_setup.cpp:1830
	boolRow("screenshot_scale")
		.section("osd")
		.label("screenshot.scale")
		.hint("menu.hint_screenshot_scale")
		.defaultValue(0)
		.field(COREAPI_NUMBER_FIELD(screenshot_scale)),
	// src/neutrino.cpp:1038 src/gui/osd_setup.cpp:1834
	boolRow("screenshot_cover")
		.section("osd")
		.label("screenshot.cover")
		.hint("menu.hint_screenshot_cover")
		.defaultValue(0)
		.field(COREAPI_NUMBER_FIELD(screenshot_cover)),

	// screensaver
	// Minutes, and nought is what the screen offers as off.
	// src/neutrino.cpp:730 src/gui/osd_setup.cpp:1857
	intRow("screensaver_delay")
		.section("osd")
		.label("screensaver.delay")
		.hint("menu.hint_screensaver_delay")
		.range(0, 999)
		.defaultValue(1)
		.unit("unit.short.minute")
		.field(COREAPI_NUMBER_FIELD(screensaver_delay)),
	// src/neutrino.cpp:732 src/gui/osd_setup.cpp:1863
	enumRow("screensaver_mode")
		.section("osd")
		.label("screensaver.mode")
		.hint("menu.hint_screensaver_mode")
		.defaultValue(1)
		.values(kScreensaverMode)
		.changeableWhen(kScreensaverOn)
		.field(COREAPI_NUMBER_FIELD(screensaver_mode)),
	// Seconds, and nought is off again.
	// src/neutrino.cpp:735 src/gui/osd_setup.cpp:1869
	intRow("screensaver_timeout")
		.section("osd")
		.label("screensaver.timeout")
		.hint("menu.hint_screensaver_timeout")
		.range(0, 60)
		.defaultValue(10)
		.unit("unit.short.second")
		.changeableWhen(kScreensaverOn)
		.field(COREAPI_NUMBER_FIELD(screensaver_timeout)),
	// src/neutrino.cpp:731 src/gui/osd_setup.cpp:1876
	textRow("screensaver_dir")
		.section("osd")
		.label("screensaver.dir")
		.hint("menu.hint_screensaver_dir")
		.defaultValue(ICONSDIR "/screensaver")
		.text(kRuleDirectoryExists)
		.changeableWhen(kScreensaverImage)
		.field(COREAPI_TEXT_FIELD(screensaver_dir)),
	// src/neutrino.cpp:734 src/gui/osd_setup.cpp:1882
	boolRow("screensaver_random")
		.section("osd")
		.label("screensaver.random")
		.hint("menu.hint_screensaver_random")
		.defaultValue(0)
		.changeableWhen(kScreensaverImage)
		.field(COREAPI_NUMBER_FIELD(screensaver_random)),
	// The item for this one sits in an arm the screen never compiles as well,
	// and the screensaver reads the setting either way.
	// src/neutrino.cpp:733 src/gui/osd_setup.cpp:1889 src/gui/screensaver.cpp:402
	boolRow("screensaver_mode_text")
		.section("osd")
		.label("screensaver.enable_text_info")
		.hint("menu.hint_screensaver_enable_text_info")
		.defaultValue(0)
		.changeableWhen(kScreensaverOn)
		.field(COREAPI_NUMBER_FIELD(screensaver_mode_text)),
	// infoicons
	// src/neutrino.cpp:449 src/gui/infoicons_setup.cpp:156
	enumRow("mode_icons_skin")
		.section("osd")
		.label("infoicons_skin")
		.hint("menu.hint_infoicons_skin")
		.defaultValue(0)
		.values(kInfoiconsSkin)
		.changeableWhen(kIconsOff)
		.field(COREAPI_NUMBER_FIELD(mode_icons_skin)),
	// src/neutrino.cpp:447 src/gui/infoicons_setup.cpp:160
	enumRow("mode_icons")
		.section("osd")
		.label("infoicons_modeicon")
		.hint("menu.hint_infoicons_modeicon")
		.defaultValue(0)
		.values(kInfoiconsMode)
		.changeableWhen(kSkinNotInfoviewer)
		.field(COREAPI_NUMBER_FIELD(mode_icons)),
	/* Local resolution: the source declares mode_icons_background here. In this
	   fork the icons' drawing code is stilled (#if 0) and the osd module
	   reads only mode and skin (src/coreapi/osd.cpp), so nothing reads this
	   member and a row would offer a switch that changes nothing. The two
	   rows above stay: the osd module is their reader, and the shipped
	   /api/v1/osd/infoicons route acts on them. The member is carried in
	   settingsundeclared.cpp and is a port candidate if the feature returns
	   (WORK-114). */
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
	intRow("osd_resolution")
		.section("osd")
		.label("colormenu.osd_resolution")
		.hint("menu.hint_osd_resolution")
		.range(0, 1)
#if HAVE_ARM_HARDWARE || (HAVE_CST_HARDWARE && defined(BOXMODEL_CST_HD2))
		.defaultValue(1)
#else
		.defaultValue(0)
#endif
		.changeableWhen(kVideoModeNotAuto)
		.field(COREAPI_SERVICE_FIELD(osd_resolution, askOsdResolution, tellOsdResolution)),
	// src/neutrino.cpp:1054 src/gui/screensetup.cpp:225
	intRow("screen_StartX_a_0")
		.section("osd")
		.label("screensetup.upperleft")
		.range(0, 200)
		.defaultValue(0)
		.needsRestart()
		.field(COREAPI_NUMBER_FIELD(screen_StartX_a_0)),
	// src/neutrino.cpp:1055 src/gui/screensetup.cpp:201
	intRow("screen_StartY_a_0")
		.section("osd")
		.label("screensetup.upperleft")
		.range(0, 200)
		.defaultValue(0)
		.needsRestart()
		.field(COREAPI_NUMBER_FIELD(screen_StartY_a_0)),
	// src/neutrino.cpp:1056 src/gui/screensetup.cpp:239
	intRow("screen_EndX_a_0")
		.section("osd")
		.label("screensetup.lowerright")
		.range(400, 1279)
		.defaultValue(1279)
		.needsRestart()
		.field(COREAPI_NUMBER_FIELD(screen_EndX_a_0)),
	// src/neutrino.cpp:1057 src/gui/screensetup.cpp:212
	intRow("screen_EndY_a_0")
		.section("osd")
		.label("screensetup.lowerright")
		.range(400, 719)
		.defaultValue(719)
		.needsRestart()
		.field(COREAPI_NUMBER_FIELD(screen_EndY_a_0)),
	// src/neutrino.cpp:1058 src/gui/screensetup.cpp:225
	intRow("screen_StartX_a_1")
		.section("osd")
		.label("screensetup.upperleft")
		.range(0, 200)
		.defaultValue(0)
		.needsRestart()
		.field(COREAPI_NUMBER_FIELD(screen_StartX_a_1)),
	// src/neutrino.cpp:1059 src/gui/screensetup.cpp:201
	intRow("screen_StartY_a_1")
		.section("osd")
		.label("screensetup.upperleft")
		.range(0, 200)
		.defaultValue(0)
		.needsRestart()
		.field(COREAPI_NUMBER_FIELD(screen_StartY_a_1)),
	// src/neutrino.cpp:1060 src/gui/screensetup.cpp:239
	intRow("screen_EndX_a_1")
		.section("osd")
		.label("screensetup.lowerright")
		.range(400, 1919)
		.defaultValue(1919)
		.needsRestart()
		.field(COREAPI_NUMBER_FIELD(screen_EndX_a_1)),
	// src/neutrino.cpp:1061 src/gui/screensetup.cpp:212
	intRow("screen_EndY_a_1")
		.section("osd")
		.label("screensetup.lowerright")
		.range(400, 1079)
		.defaultValue(1079)
		.needsRestart()
		.field(COREAPI_NUMBER_FIELD(screen_EndY_a_1)),
	// src/neutrino.cpp:1064 src/gui/screensetup.cpp:225
	intRow("screen_StartX_b_0")
		.section("osd")
		.label("screensetup.upperleft")
		.range(0, 200)
		.defaultValue(22)
		.needsRestart()
		.field(COREAPI_NUMBER_FIELD(screen_StartX_b_0)),
	// src/neutrino.cpp:1065 src/gui/screensetup.cpp:201
	intRow("screen_StartY_b_0")
		.section("osd")
		.label("screensetup.upperleft")
		.range(0, 200)
		.defaultValue(12)
		.needsRestart()
		.field(COREAPI_NUMBER_FIELD(screen_StartY_b_0)),
	// src/neutrino.cpp:1066 src/gui/screensetup.cpp:239
	intRow("screen_EndX_b_0")
		.section("osd")
		.label("screensetup.lowerright")
		.range(400, 1279)
		.defaultValue(1236)
		.needsRestart()
		.field(COREAPI_NUMBER_FIELD(screen_EndX_b_0)),
	// src/neutrino.cpp:1067 src/gui/screensetup.cpp:212
	intRow("screen_EndY_b_0")
		.section("osd")
		.label("screensetup.lowerright")
		.range(400, 719)
		.defaultValue(695)
		.needsRestart()
		.field(COREAPI_NUMBER_FIELD(screen_EndY_b_0)),
	// src/neutrino.cpp:1068 src/gui/screensetup.cpp:225
	intRow("screen_StartX_b_1")
		.section("osd")
		.label("screensetup.upperleft")
		.range(0, 200)
		.defaultValue(33)
		.needsRestart()
		.field(COREAPI_NUMBER_FIELD(screen_StartX_b_1)),
	// src/neutrino.cpp:1069 src/gui/screensetup.cpp:201
	intRow("screen_StartY_b_1")
		.section("osd")
		.label("screensetup.upperleft")
		.range(0, 200)
		.defaultValue(18)
		.needsRestart()
		.field(COREAPI_NUMBER_FIELD(screen_StartY_b_1)),
	// src/neutrino.cpp:1070 src/gui/screensetup.cpp:239
	intRow("screen_EndX_b_1")
		.section("osd")
		.label("screensetup.lowerright")
		.range(400, 1919)
		.defaultValue(1854)
		.needsRestart()
		.field(COREAPI_NUMBER_FIELD(screen_EndX_b_1)),
	// src/neutrino.cpp:1071 src/gui/screensetup.cpp:212
	intRow("screen_EndY_b_1")
		.section("osd")
		.label("screensetup.lowerright")
		.range(400, 1079)
		.defaultValue(1043)
		.needsRestart()
		.field(COREAPI_NUMBER_FIELD(screen_EndY_b_1)),

	/* Whether the infobar shows the module line at all. The screen has no item
	   for it and derives it from the position beside it,
	   src/gui/osd_setup.cpp:1665, so writing the position alone leaves this one
	   as it was and the line does not follow. The infobar reads it,
	   src/gui/infoviewer_bb.cpp. */
	// src/neutrino.cpp:468
	/* Whether the infobar carries the channel description,
	   src/gui/infoviewer.cpp. No item names it. */
	// src/neutrino.cpp:1318
	boolRow("infobar_show_channeldesc")
		.section("osd")
		.defaultValue(0)
		.field(COREAPI_NUMBER_FIELD(infobar_show_channeldesc)),
	/* The name of the colour theme, picked from a file list rather than typed,
	   src/gui/themes.cpp, and read once at start. Its default is a migration
	   name where the box has no settings file yet and empty otherwise, which is
	   what every box that has saved once gets and what the row states.
	   src/neutrino.cpp:408 */
	// src/neutrino.cpp:408
	textRow("theme_name")
		.section("osd")
		.defaultValue("")
		.text(kRuleNameFromList)
		.needsRestart()
		.field(COREAPI_TEXT_FIELD(theme_name)),
};

} // anonymous namespace

const Descriptor *settingsTableOsd(size_t &count)
{
	count = sizeof(kOsd) / sizeof(kOsd[0]);
	return kOsd;
}

} // namespace coreapi
