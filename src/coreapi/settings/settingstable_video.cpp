/*
 * settingstable_video.cpp - video settings, one row per field
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

/* The video section. Five of this screen's number choosers sit in an arm the
   preprocessor drops, and the fields behind them are loaded, saved and edited
   by another screen that carries no bound of its own. Those rows cite the
   dropped call, because it is the only statement of a bound there is. */

// src/gui/videosettings.cpp:342 lib/hardware/coolstream/hd1/libcoolstream/video_cs.h:66
const EnumValue kVideoFormat[] =
{
	{ 1, "videomenu.videoformat_43" },
	{ 2, "videomenu.videoformat_149" },
	{ 3, "videomenu.videoformat_169" }
};

// src/gui/videosettings.cpp:350
const EnumValue kDbDr[] =
{
	{ 0, "videomenu.dbdr_none" },
	{ 1, "videomenu.dbdr_deblock" },
	{ 2, "videomenu.dbdr_both" }
};

#if HAVE_ARM_HARDWARE || HAVE_MIPS_HARDWARE
// src/gui/videosettings.cpp:359
const EnumValue kZappingMode[] =
{
	{ 0, "videomenu.zappingmode_mute" },
	{ 1, "videomenu.zappingmode_hold" },
	{ 2, "videomenu.zappingmode_mutetilllock" },
	{ 3, "videomenu.zappingmode_holdtilllock" }
};
#endif

// src/gui/cec_setup.cpp:79
const EnumValue kCecMode[] =
{
	{ 0, "videomenu.hdmi_cec_mode_off" },
	{ 1, "videomenu.hdmi_cec_mode_tuner" },
	{ 2, "videomenu.hdmi_cec_mode_recorder" }
};

// src/gui/cec_setup.cpp:88
const EnumValue kCecVolume[] =
{
	{ 0, "videomenu.hdmi_cec_vol_off" },
	{ 1, "videomenu.hdmi_cec_vol_audiosystem" },
	{ 2, "videomenu.hdmi_cec_vol_tv" }
};

// The three below the mode are offered only while the link is on.
// src/gui/cec_setup.cpp:105
const Condition kCecOn[] =
{
	{ "hdmi_cec_mode", CompareOp::Ne, 0, NULL, 0 }
};

const Descriptor kVideo[] =
{
	/* The screen drops the third choice where the box cannot draw 14:9, which
	   is a capability read at run time and not a setting to condition on.
	   src/gui/videosettings.cpp:441 */
	// src/neutrino.cpp:489 src/gui/videosettings.cpp:441
	{
		"video_Format", ValueType::Enum, "video",
		"videomenu.videoformat", "menu.hint_video_format",
		0, 0, COREAPI_VALUES(kVideoFormat), 3, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(video_Format)
	},
	/* A number and not a choice: the screen offers it out of a list it builds
	   at run time, which no scan can read, so a choice here would be one no
	   case could hold to the screen. Every value in the range is a mode.
	   Bound: DISPLAY_AR_MODE_PANSCAN through DISPLAY_AR_MODE_PANSCAN2, the four
	   the screen's own table names, src/gui/videosettings.cpp:118. The screen
	   drops the fourth where the box cannot pan and scan 14:9, at :897. */
	// src/neutrino.cpp:490 src/gui/videosettings.cpp:437
	{
		"video_43mode", ValueType::Int, "video",
		"videomenu.43mode", "menu.hint_video_43mode",
		0, 3, NULL, 0, 1, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(video_43mode)
	},
	// The screen offers this one only off Coolstream, which is a revision read
	// at run time. src/gui/videosettings.cpp:457
	// src/neutrino.cpp:544 src/gui/videosettings.cpp:459
	{
		"video_dbdr", ValueType::Enum, "video",
		"videomenu.dbdr", "menu.hint_video_dbdr",
		0, 0, COREAPI_VALUES(kDbDr), 0, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(video_dbdr)
	},
#if HAVE_ARM_HARDWARE || HAVE_MIPS_HARDWARE
	// One box model falls back to 2 instead, which is a model read at run time
	// and not a constant this can carry.
	// src/neutrino.cpp:574 src/gui/videosettings.cpp:549
	{
		"zappingmode", ValueType::Enum, "video",
		"videomenu.zappingmode", "menu.hint_video_zappingmode",
		0, 0, COREAPI_VALUES(kZappingMode), 0, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(zappingmode)
	},
	/* A number and not a choice, and the ceiling differs by box: the same
	   stored number names a different colorimetry on the two arms below, so no
	   one list of values describes both.
	   src/gui/videosettings.cpp:369 offers three, :377 offers four. */
	// src/neutrino.cpp:575 src/gui/videosettings.cpp:560
	{
		"hdmi_colorimetry", ValueType::Int, "video",
		"videomenu.hdmi_colorimetry", "menu.hint_video_hdmi_colorimetry",
#if BOXMODEL_VUPLUS_ARM
		0, 2,
#else
		0, 3,
#endif
		NULL, 0, 0, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(hdmi_colorimetry)
	},
	/* The five below are edited by src/gui/psisetup.cpp, which holds each of
	   the four values to 0..255 at :206 and :215 and states no bound for the
	   step. The dropped call cited beside each is the only place the bound is
	   written as one. */
	// src/neutrino.cpp:493 src/gui/videosettings.cpp:592
	{
		"video_psi_brightness", ValueType::Int, "video",
		"videomenu.psi.brightness", "menu.hint_video_brightness",
		0, 255, NULL, 0, 128, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(psi_brightness)
	},
	// src/neutrino.cpp:494 src/gui/videosettings.cpp:584
	{
		"video_psi_contrast", ValueType::Int, "video",
		"videomenu.psi.contrast", "menu.hint_video_contrast",
		0, 255, NULL, 0, 128, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(psi_contrast)
	},
	// src/neutrino.cpp:495 src/gui/videosettings.cpp:588
	{
		"video_psi_saturation", ValueType::Int, "video",
		"videomenu.psi.saturation", "menu.hint_video_saturation",
		0, 255, NULL, 0, 128, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(psi_saturation)
	},
	// src/neutrino.cpp:497 src/gui/videosettings.cpp:596
	{
		"video_psi_tint", ValueType::Int, "video",
		"videomenu.psi.tint", "menu.hint_video_tint",
		0, 255, NULL, 0, 128, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(psi_tint)
	},
	// How far one press moves the four above, not a value of its own.
	// src/neutrino.cpp:496 src/gui/videosettings.cpp:572
	{
		"video_psi_step", ValueType::Int, "video",
		"videomenu.psi.step", "menu.hint_video_psi_step",
		1, 100, NULL, 0, 2, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(psi_step)
	},
#endif
	/* The HDMI link, which the misc menu opens and only where the box can
	   drive it, src/gui/miscsettings_menu.cpp:308. That is hardware and not a
	   setting, so no row carries it as a condition. */
	// src/neutrino.cpp:501 src/gui/cec_setup.cpp:103
	{
		"hdmi_cec_mode", ValueType::Enum, "video",
		"videomenu.hdmi_cec_mode", "menu.hint_cec_mode",
		0, 0, COREAPI_ENUM(kCecMode), 0, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(hdmi_cec_mode)
	},
	// src/neutrino.cpp:502 src/gui/cec_setup.cpp:105
	{
		"hdmi_cec_view_on", ValueType::Bool, "video",
		"videomenu.hdmi_cec_view_on", "menu.hint_cec_view_on",
		0, 1, NULL, 0, 0, NULL, false, false, COREAPI_CONDITIONS(kCecOn),
		COREAPI_NUMBER_FIELD(hdmi_cec_view_on)
	},
	// src/neutrino.cpp:503 src/gui/cec_setup.cpp:107
	{
		"hdmi_cec_standby", ValueType::Bool, "video",
		"videomenu.hdmi_cec_standby", "menu.hint_cec_standby",
		0, 1, NULL, 0, 0, NULL, false, false, COREAPI_CONDITIONS(kCecOn),
		COREAPI_NUMBER_FIELD(hdmi_cec_standby)
	},
	// src/neutrino.cpp:504 src/gui/cec_setup.cpp:110
	{
		"hdmi_cec_volume", ValueType::Enum, "video",
		"videomenu.hdmi_cec_volume", "menu.hint_cec_volume",
		0, 0, COREAPI_ENUM(kCecVolume), 0, NULL, false, false, COREAPI_CONDITIONS(kCecOn),
		COREAPI_NUMBER_FIELD(hdmi_cec_volume)
	},
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
	{
		"pip_x", ValueType::Int, "video",
		"videomenu.pip", "menu.hint_video_pip",
		0, 1920, NULL, 0, 50, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(pip_x)
	},
	// src/neutrino.cpp:1211 src/gui/pipsetup.cpp:61
	{
		"pip_y", ValueType::Int, "video",
		"videomenu.pip", "menu.hint_video_pip",
		0, 1080, NULL, 0, 50, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(pip_y)
	},
	// src/neutrino.cpp:1212 src/gui/pipsetup.cpp:132
	{
		"pip_width", ValueType::Int, "video",
		"videomenu.pip", "menu.hint_video_pip",
		0, 1920, NULL, 0, 365, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(pip_width)
	},
	// src/neutrino.cpp:1213 src/gui/pipsetup.cpp:133
	{
		"pip_height", ValueType::Int, "video",
		"videomenu.pip", "menu.hint_video_pip",
		0, 1080, NULL, 0, 200, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(pip_height)
	},
	// The radio pair falls back to the television one rather than to a number.
	// src/neutrino.cpp:1215 src/gui/pipsetup.cpp:60
	{
		"pip_radio_x", ValueType::Int, "video",
		"videomenu.pip", "menu.hint_video_pip",
		0, 1920, NULL, 0, 50, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(pip_radio_x)
	},
	// src/neutrino.cpp:1216 src/gui/pipsetup.cpp:61
	{
		"pip_radio_y", ValueType::Int, "video",
		"videomenu.pip", "menu.hint_video_pip",
		0, 1080, NULL, 0, 50, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(pip_radio_y)
	},
	// src/neutrino.cpp:1217 src/gui/pipsetup.cpp:132
	{
		"pip_radio_width", ValueType::Int, "video",
		"videomenu.pip", "menu.hint_video_pip",
		0, 1920, NULL, 0, 365, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(pip_radio_width)
	},
	// src/neutrino.cpp:1218 src/gui/pipsetup.cpp:133
	{
		"pip_radio_height", ValueType::Int, "video",
		"videomenu.pip", "menu.hint_video_pip",
		0, 1080, NULL, 0, 200, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(pip_radio_height)
	},
	/* Which corner the small picture was last moved to. The floor is the value
	   that means no corner at all and the rest are the four,
	   src/system/settings.h:1363-1369. */
	// src/neutrino.cpp:1220 src/gui/pipsetup.cpp:99
	{
		"pip_rotate_lastpos", ValueType::Int, "video",
		"videomenu.pip", "menu.hint_video_pip",
		-1, 3, NULL, 0, -1, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(pip_rotate_lastpos)
	},
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
	{
		"video_Mode", ValueType::Enum, "video",
		"videomenu.videomode", "menu.hint_video_mode",
		0, 0, NULL, 0,
#if HAVE_ARM_HARDWARE
		// VIDEO_STD_1080P50, the thirteenth of libarmbox/video_lib.h
		12,
#elif HAVE_CST_HARDWARE && defined(BOXMODEL_CST_HD2)
		// VIDEO_STD_1080P24, the eleventh of hd2/libcoolstream/video_cs.h
		10,
#else
		// VIDEO_STD_720P50, the eighth, which every family numbers alike
		7,
#endif
		NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD_ASKED(video_Mode, "video_mode")
	},
	// src/neutrino.cpp:497 src/gui/videosettings.cpp:509
	{
		"analog_mode1", ValueType::Enum, "video",
		"videomenu.analog_mode", "menu.hint_video_analog_mode",
		0, 0, NULL, 0,
#ifdef ANALOG_MODE
		// ANALOG_MODE(BOTH, SD, RGB), which is ANALOG_SD_RGB together with
		// ANALOG_xD_BOTH. hd2/libcoolstream/video_cs.h:27 and :36
		770,
#else
		// ANALOG_SD_RGB_SCART. hd1/libcoolstream/video_cs.h:30
		0x10,
#endif
		NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD_ASKED(analog_mode1, "analog_mode1")
	},
	// src/neutrino.cpp:498 src/gui/videosettings.cpp:525
	{
		"analog_mode2", ValueType::Enum, "video",
		"videomenu.cinch", "menu.hint_video_cinch_mode",
		0, 0, NULL, 0,
#ifdef ANALOG_MODE
		// ANALOG_MODE(CINCH, SD, YPRPB), which is ANALOG_SD_YPRPB together with
		// ANALOG_xD_CINCH. hd2/libcoolstream/video_cs.h:28 and :35
		516,
#else
		// ANALOG_SD_YPRPB_CINCH. hd1/libcoolstream/video_cs.h:27
		0x01,
#endif
		NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD_ASKED(analog_mode2, "analog_mode2")
	},
#ifdef BOXMODEL_CST_HD2
	/* The driver takes -128..127 for the three below and the screen offers a
	   narrower range because the picture stops changing outside it, so the
	   bound here is the screen's and not the driver's.
	   The screen multiplies contrast and saturation by three on the way out,
	   src/gui/videosettings.cpp:727 and :731, so the stored number is the one
	   the screen shows rather than the one the driver is given. */
	// src/neutrino.cpp:651 src/gui/videosettings.cpp:519
	{
		"brightness", ValueType::Int, "video",
		"videomenu.brightness", "menu.hint_video_brightness",
		-42, 42, NULL, 0, 0, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(brightness)
	},
	// src/neutrino.cpp:652 src/gui/videosettings.cpp:521
	{
		"contrast", ValueType::Int, "video",
		"videomenu.contrast", "menu.hint_video_contrast",
		-42, 42, NULL, 0, 0, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(contrast)
	},
	// src/neutrino.cpp:653 src/gui/videosettings.cpp:523
	{
		"saturation", ValueType::Int, "video",
		"videomenu.saturation", "menu.hint_video_saturation",
		-42, 42, NULL, 0, 0, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(saturation)
	},
	// src/neutrino.cpp:1337 src/gui/videosettings.cpp:529
	{
		"enable_sd_osd", ValueType::Bool, "video",
		"videomenu.sdosd", "menu.hint_video_sdosd",
		0, 1, NULL, 0, 1, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(enable_sd_osd)
	},
#endif
};

} // anonymous namespace

const Descriptor *settingsTableVideo(size_t &count)
{
	count = sizeof(kVideo) / sizeof(kVideo[0]);
	return kVideo;
}

} // namespace coreapi
