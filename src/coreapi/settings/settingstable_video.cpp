/*
 * settingstable_video.cpp - video settings, one row per field
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
#include "boxdefaults.h"
#include "predicates.h"
#include "settingsfield.h"

namespace coreapi
{

namespace
{

/* The video section. Five of this screen's number choosers sit in an arm the
   preprocessor drops, and the fields behind them are loaded, saved and edited
   by another screen that carries no bound of its own. Those rows cite the
   dropped call, because it is the only statement of a bound there is. */

// src/gui/videosettings.cpp:342 lib/hardware/coolstream/hd1/libcoolstream/video_cs.h:66
constexpr EnumValue kVideoFormat[] =
{
	option(1).label("videomenu.videoformat_43"),
	option(2).label("videomenu.videoformat_149"),
	option(3).label("videomenu.videoformat_169")
};

// src/gui/videosettings.cpp:350
constexpr EnumValue kDbDr[] =
{
	option(0).label("videomenu.dbdr_none"),
	option(1).label("videomenu.dbdr_deblock"),
	option(2).label("videomenu.dbdr_both")
};

#if HAVE_ARM_HARDWARE || HAVE_MIPS_HARDWARE
// src/gui/videosettings.cpp:359
constexpr EnumValue kZappingMode[] =
{
	option(0).label("videomenu.zappingmode_mute"),
	option(1).label("videomenu.zappingmode_hold"),
	option(2).label("videomenu.zappingmode_mutetilllock"),
	option(3).label("videomenu.zappingmode_holdtilllock")
};
#endif

// src/gui/cec_setup.cpp:79
constexpr EnumValue kCecMode[] =
{
	option(0).label("videomenu.hdmi_cec_mode_off"),
	option(1).label("videomenu.hdmi_cec_mode_tuner"),
	option(2).label("videomenu.hdmi_cec_mode_recorder")
};

// src/gui/cec_setup.cpp:88
constexpr EnumValue kCecVolume[] =
{
	option(0).label("videomenu.hdmi_cec_vol_off"),
	option(1).label("videomenu.hdmi_cec_vol_audiosystem"),
	option(2).label("videomenu.hdmi_cec_vol_tv")
};

// The three below the mode are offered only while the link is on.
// src/gui/cec_setup.cpp:105
constexpr Condition kCecOn[] =
{
	when("hdmi_cec_mode").isNot(0)
};

#if HAVE_ARM_HARDWARE || HAVE_MIPS_HARDWARE
long zappingModeDefault()
{
	return boxdefault::kZappingModeTwo ? 2 : 0;
}
#endif

constexpr EnumValue kHdmiColorimetry[] =
{
#if BOXMODEL_VUPLUS_ARM
	option(0).label("videomenu.hdmi_colorimetry_auto"),
	option(1).label("videomenu.hdmi_colorimetry_bt709"),
	option(2).label("videomenu.hdmi_colorimetry_bt470")
#else
	option(0).label("videomenu.hdmi_colorimetry_auto"),
	option(1).label("videomenu.hdmi_colorimetry_bt2020ncl"),
	option(2).label("videomenu.hdmi_colorimetry_bt2020cl"),
	option(3).label("videomenu.hdmi_colorimetry_bt709")
#endif
};

constexpr Descriptor kVideo[] =
{
	/* The screen drops the third choice where the box cannot draw 14:9, which
	   is a capability read at run time and not a setting to condition on.
	   src/gui/videosettings.cpp:441 */
	// src/neutrino.cpp:489 src/gui/videosettings.cpp:441
	enumRow("video_Format")
		.section("video")
		.label("videomenu.videoformat")
		.hint("menu.hint_video_format")
		.defaultValue(3)
		.values(kVideoFormat)
		.field(COREAPI_NUMBER_FIELD(video_Format)),
	/* A number and not a choice: the screen offers it out of a list it builds
	   at run time, which no scan can read, so a choice here would be one no
	   case could hold to the screen. Every value in the range is a mode.
	   Bound: DISPLAY_AR_MODE_PANSCAN through DISPLAY_AR_MODE_PANSCAN2, the four
	   the screen's own table names, src/gui/videosettings.cpp:118. The screen
	   drops the fourth where the box cannot pan and scan 14:9, at :897. */
	// src/neutrino.cpp:490 src/gui/videosettings.cpp:437
	intRow("video_43mode")
		.section("video")
		.label("videomenu.43mode")
		.hint("menu.hint_video_43mode")
		.range(0, 3)
		.defaultValue(1)
		.field(COREAPI_NUMBER_FIELD(video_43mode)),
	// The screen offers this one only off Coolstream, which is a revision read
	// at run time. src/gui/videosettings.cpp:457
	// src/neutrino.cpp:544 src/gui/videosettings.cpp:459
	enumRow("video_dbdr")
		.section("video")
		.label("videomenu.dbdr")
		.hint("menu.hint_video_dbdr")
		.defaultValue(0)
		.values(kDbDr)
		.field(COREAPI_NUMBER_FIELD_ON(video_dbdr, hasDbdr, NULL)),
#if HAVE_ARM_HARDWARE || HAVE_MIPS_HARDWARE
	// One box model falls back to 2 instead, which is a model read at run time
	// and not a constant this can carry.
	// src/neutrino.cpp:574 src/gui/videosettings.cpp:549
	enumRow("zappingmode")
		.section("video")
		.label("videomenu.zappingmode")
		.hint("menu.hint_video_zappingmode")
		.defaultValue(0)
		.defaultFrom(zappingModeDefault)
		.values(kZappingMode)
		.field(COREAPI_NUMBER_FIELD_ON(zappingmode, takesZappingMode, NULL)),
	/* The same stored number names a different colorimetry on the two arms of
	   the list: src/gui/videosettings.cpp:369 offers three, :377 offers four. */
	// src/neutrino.cpp:575 src/gui/videosettings.cpp:560
	enumRow("hdmi_colorimetry")
		.section("video")
		.label("videomenu.hdmi_colorimetry")
		.hint("menu.hint_video_hdmi_colorimetry")
		.defaultValue(0)
		.values(kHdmiColorimetry)
		.field(COREAPI_NUMBER_FIELD_ON(hdmi_colorimetry, takesHdmiColorimetry, NULL)),
	/* The five below are edited by src/gui/psisetup.cpp, which holds each of
	   the four values to 0..255 at :206 and :215 and states no bound for the
	   step. The dropped call cited beside each is the only place the bound is
	   written as one. */
	// src/neutrino.cpp:493 src/gui/videosettings.cpp:592
	intRow("video_psi_brightness")
		.section("video")
		.label("videomenu.psi.brightness")
		.hint("menu.hint_video_brightness")
		.range(0, 255)
		.defaultValue(128)
		.field(COREAPI_NUMBER_FIELD(psi_brightness)),
	// src/neutrino.cpp:494 src/gui/videosettings.cpp:584
	intRow("video_psi_contrast")
		.section("video")
		.label("videomenu.psi.contrast")
		.hint("menu.hint_video_contrast")
		.range(0, 255)
		.defaultValue(128)
		.field(COREAPI_NUMBER_FIELD(psi_contrast)),
	// src/neutrino.cpp:495 src/gui/videosettings.cpp:588
	intRow("video_psi_saturation")
		.section("video")
		.label("videomenu.psi.saturation")
		.hint("menu.hint_video_saturation")
		.range(0, 255)
		.defaultValue(128)
		.field(COREAPI_NUMBER_FIELD(psi_saturation)),
	// src/neutrino.cpp:497 src/gui/videosettings.cpp:596
	intRow("video_psi_tint")
		.section("video")
		.label("videomenu.psi.tint")
		.hint("menu.hint_video_tint")
		.range(0, 255)
		.defaultValue(128)
		.field(COREAPI_NUMBER_FIELD(psi_tint)),
	// How far one press moves the four above, not a value of its own.
	// src/neutrino.cpp:496 src/gui/videosettings.cpp:572
	intRow("video_psi_step")
		.section("video")
		.label("videomenu.psi.step")
		.hint("menu.hint_video_psi_step")
		.range(1, 100)
		.defaultValue(2)
		.field(COREAPI_NUMBER_FIELD(psi_step)),
#endif
	/* The HDMI link, which the misc menu opens and only where the box can
	   drive it, src/gui/miscsettings_menu.cpp:308. That is hardware and not a
	   setting, so no row carries it as a condition. */
	// src/neutrino.cpp:501 src/gui/cec_setup.cpp:103
	enumRow("hdmi_cec_mode")
		.section("video")
		.label("videomenu.hdmi_cec_mode")
		.hint("menu.hint_cec_mode")
		.defaultValue(0)
		.values(kCecMode)
		.field(COREAPI_NUMBER_FIELD_ON(hdmi_cec_mode, canCec, NULL)),
	// src/neutrino.cpp:502 src/gui/cec_setup.cpp:105
	boolRow("hdmi_cec_view_on")
		.section("video")
		.label("videomenu.hdmi_cec_view_on")
		.hint("menu.hint_cec_view_on")
		.defaultValue(0)
		.changeableWhen(kCecOn)
		.field(COREAPI_NUMBER_FIELD_ON(hdmi_cec_view_on, canCec, NULL)),
	// src/neutrino.cpp:503 src/gui/cec_setup.cpp:107
	boolRow("hdmi_cec_standby")
		.section("video")
		.label("videomenu.hdmi_cec_standby")
		.hint("menu.hint_cec_standby")
		.defaultValue(0)
		.changeableWhen(kCecOn)
		.field(COREAPI_NUMBER_FIELD_ON(hdmi_cec_standby, canCec, NULL)),
	// src/neutrino.cpp:504 src/gui/cec_setup.cpp:110
	enumRow("hdmi_cec_volume")
		.section("video")
		.label("videomenu.hdmi_cec_volume")
		.hint("menu.hint_cec_volume")
		.defaultValue(0)
		.values(kCecVolume)
		.changeableWhen(kCecOn)
		.field(COREAPI_NUMBER_FIELD_ON(hdmi_cec_volume, canCec, NULL)),
#if ENABLE_PIP
	/* The small picture's place and size, which the video menu opens as a
	   widget the viewer drags rather than as items,
	   src/gui/pipsetup.cpp:36-135. So the nine rows below share the label of
	   the item that opens it, and the pair for radio carries the same one
	   again: the widget edits whichever pair the box is in, :29-42.

	   The ceiling of each is the OSD's own width or height, which the box asks
	   the framebuffer for. Every backend in the tree offers 1280 by 720 and at
	   most 1920 by 1080, src/driver/fb_accel_arm.cpp:306-317, so those are the
	   widest a value can be. */
	// src/neutrino.cpp:1210 src/gui/pipsetup.cpp:60
	intRow("pip_x")
		.section("video")
		.label("videomenu.pip")
		.hint("menu.hint_video_pip")
		.range(0, 1920)
		.defaultValue(50)
		.field(COREAPI_NUMBER_FIELD_ON(pip_x, pipUsable, NULL)),
	// src/neutrino.cpp:1211 src/gui/pipsetup.cpp:61
	intRow("pip_y")
		.section("video")
		.label("videomenu.pip")
		.hint("menu.hint_video_pip")
		.range(0, 1080)
		.defaultValue(50)
		.field(COREAPI_NUMBER_FIELD_ON(pip_y, pipUsable, NULL)),
	// src/neutrino.cpp:1212 src/gui/pipsetup.cpp:132
	intRow("pip_width")
		.section("video")
		.label("videomenu.pip")
		.hint("menu.hint_video_pip")
		.range(0, 1920)
		.defaultValue(365)
		.field(COREAPI_NUMBER_FIELD_ON(pip_width, pipUsable, NULL)),
	// src/neutrino.cpp:1213 src/gui/pipsetup.cpp:133
	intRow("pip_height")
		.section("video")
		.label("videomenu.pip")
		.hint("menu.hint_video_pip")
		.range(0, 1080)
		.defaultValue(200)
		.field(COREAPI_NUMBER_FIELD_ON(pip_height, pipUsable, NULL)),
	// The radio pair falls back to the television one rather than to a number.
	// src/neutrino.cpp:1215 src/gui/pipsetup.cpp:60
	intRow("pip_radio_x")
		.section("video")
		.label("videomenu.pip")
		.hint("menu.hint_video_pip")
		.range(0, 1920)
		.defaultValue(50)
		.field(COREAPI_NUMBER_FIELD_ON(pip_radio_x, pipUsable, NULL)),
	// src/neutrino.cpp:1216 src/gui/pipsetup.cpp:61
	intRow("pip_radio_y")
		.section("video")
		.label("videomenu.pip")
		.hint("menu.hint_video_pip")
		.range(0, 1080)
		.defaultValue(50)
		.field(COREAPI_NUMBER_FIELD_ON(pip_radio_y, pipUsable, NULL)),
	// src/neutrino.cpp:1217 src/gui/pipsetup.cpp:132
	intRow("pip_radio_width")
		.section("video")
		.label("videomenu.pip")
		.hint("menu.hint_video_pip")
		.range(0, 1920)
		.defaultValue(365)
		.field(COREAPI_NUMBER_FIELD_ON(pip_radio_width, pipUsable, NULL)),
	// src/neutrino.cpp:1218 src/gui/pipsetup.cpp:133
	intRow("pip_radio_height")
		.section("video")
		.label("videomenu.pip")
		.hint("menu.hint_video_pip")
		.range(0, 1080)
		.defaultValue(200)
		.field(COREAPI_NUMBER_FIELD_ON(pip_radio_height, pipUsable, NULL)),
	/* Which corner the small picture was last moved to. The floor is the value
	   that means no corner at all and the rest are the four,
	   src/system/settings.h:1363-1369. */
	// src/neutrino.cpp:1220 src/gui/pipsetup.cpp:99
	intRow("pip_rotate_lastpos")
		.section("video")
		.label("videomenu.pip")
		.hint("menu.hint_video_pip")
		.range(-1, 3)
		.defaultValue(-1)
		.field(COREAPI_NUMBER_FIELD_ON(pip_rotate_lastpos, pipUsable, NULL)),
#endif
	/* The three whose set of values the box decides. The numbering is the
	   backend's and the screen builds each list as it opens: which video modes
	   a model can draw is a table per model with the rest marked out,
	   src/gui/videosettings.cpp:218, and which table an analog output is
	   offered from is a revision read at run time, :389. A list written here
	   would be right for one box model and quietly wrong for the next, so each
	   row names the set instead.

	   analog_mode1 carries two labels for the same reason: the box offers it as
	   an analog mode or as a SCART socket depending on the revision.

	   The numbering is the driver's own enum and the families do not agree past
	   its twelfth entry: 1080p 50Hz is twelve on an ARM box and thirteen on a
	   PC build, so each arm here is right for the family its condition picks and
	   for no other. Widening an arm moves a mode without moving a number. One
	   more arm decides video_Mode at run time out of the environment,
	   src/neutrino.cpp:492, which no row states. */
	enumRow("video_Mode")
		.section("video")
		.label("videomenu.videomode")
		.hint("menu.hint_video_mode")
#if HAVE_ARM_HARDWARE
		// VIDEO_STD_1080P50, the thirteenth of libarmbox/video_lib.h
		.defaultValue(12)
#elif HAVE_CST_HARDWARE && defined(BOXMODEL_CST_HD2)
		// VIDEO_STD_1080P24, the eleventh of hd2/libcoolstream/video_cs.h
		.defaultValue(10)
#else
		// VIDEO_STD_720P50, the eighth, which every family numbers alike
		.defaultValue(7)
#endif
		.field(COREAPI_NUMBER_FIELD_ASKED(video_Mode, "video_mode")),
	// src/neutrino.cpp:497 src/gui/videosettings.cpp:509
	enumRow("analog_mode1")
		.section("video")
		.label("videomenu.analog_mode")
		.hint("menu.hint_video_analog_mode")
#ifdef ANALOG_MODE
		// ANALOG_MODE(BOTH, SD, RGB), which is ANALOG_SD_RGB together with
		// ANALOG_xD_BOTH. hd2/libcoolstream/video_cs.h:27 and :36
		.defaultValue(770)
#else
		// ANALOG_SD_RGB_SCART. hd1/libcoolstream/video_cs.h:30
		.defaultValue(0x10)
#endif
		.field(COREAPI_NUMBER_FIELD_ASKED(analog_mode1, "analog_mode1")),
	// src/neutrino.cpp:498 src/gui/videosettings.cpp:525
	enumRow("analog_mode2")
		.section("video")
		.label("videomenu.cinch")
		.hint("menu.hint_video_cinch_mode")
#ifdef ANALOG_MODE
		// ANALOG_MODE(CINCH, SD, YPRPB), which is ANALOG_SD_YPRPB together with
		// ANALOG_xD_CINCH. hd2/libcoolstream/video_cs.h:28 and :35
		.defaultValue(516)
#else
		// ANALOG_SD_YPRPB_CINCH. hd1/libcoolstream/video_cs.h:27
		.defaultValue(0x01)
#endif
		.field(COREAPI_NUMBER_FIELD_ASKED_ON(analog_mode2, "analog_mode2", hasAnalogCinch)),
#ifdef BOXMODEL_CST_HD2
	/* The driver takes -128..127 for the three below and the screen offers a
	   narrower range because the picture stops changing outside it, so the
	   bound here is the screen's and not the driver's.
	   The screen multiplies contrast and saturation by three on the way out,
	   src/gui/videosettings.cpp:727 and :731, so the stored number is the one
	   the screen shows rather than the one the driver is given. */
	// src/neutrino.cpp:651 src/gui/videosettings.cpp:519
	intRow("brightness")
		.section("video")
		.label("videomenu.brightness")
		.hint("menu.hint_video_brightness")
		.range(-42, 42)
		.defaultValue(0)
		.field(COREAPI_NUMBER_FIELD(brightness)),
	// src/neutrino.cpp:652 src/gui/videosettings.cpp:521
	intRow("contrast")
		.section("video")
		.label("videomenu.contrast")
		.hint("menu.hint_video_contrast")
		.range(-42, 42)
		.defaultValue(0)
		.field(COREAPI_NUMBER_FIELD(contrast)),
	// src/neutrino.cpp:653 src/gui/videosettings.cpp:523
	intRow("saturation")
		.section("video")
		.label("videomenu.saturation")
		.hint("menu.hint_video_saturation")
		.range(-42, 42)
		.defaultValue(0)
		.field(COREAPI_NUMBER_FIELD(saturation)),
	// src/neutrino.cpp:1337 src/gui/videosettings.cpp:529
	boolRow("enable_sd_osd")
		.section("video")
		.label("videomenu.sdosd")
		.hint("menu.hint_video_sdosd")
		.defaultValue(1)
		.field(COREAPI_NUMBER_FIELD(enable_sd_osd)),
#endif
};

} // anonymous namespace

const Descriptor *settingsTableVideo(size_t &count)
{
	count = sizeof(kVideo) / sizeof(kVideo[0]);
	return kVideo;
}

} // namespace coreapi
