/*
 * settingstable_player.cpp - player settings, one row per field
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

/* What the three players offer: the audio player and its internet radio, the
   movie player, and the picture viewer. Eight of the flags are offered under a
   yes and a no rather than an on and an off,
   src/gui/widget/menue_options.h:22, so what a frontend draws them as is its
   own choice: the values are the two a flag carries either way. */

// src/gui/audioplayer_setup.cpp:99
const EnumValue kDisplayOrder[] =
{
	{ 0, "audioplayer.artist_title" },
	{ 1, "audioplayer.title_artist" }
};

// The order is the screen's and so is the gap: none is last and carries nought.
// src/gui/pictureviewer_setup.cpp:88
const EnumValue kPicviewerScaling[] =
{
	{ 1, "pictureviewer.resize.simple" },
	{ 2, "pictureviewer.resize.color_average" },
	{ 0, "pictureviewer.resize.none" }
};

const Descriptor kSettings[] =
{
	// src/neutrino.cpp:1239 src/gui/audioplayer_setup.cpp:117
	{
		"audioplayer_display", ValueType::Enum, "player",
		"audioplayer.display_order", "menu.hint_audioplayer_order",
		0, 0, COREAPI_ENUM(kDisplayOrder), 0, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(audioplayer_display)
	},
	// src/neutrino.cpp:1241 src/gui/audioplayer_setup.cpp:121
	{
		"audioplayer_follow", ValueType::Bool, "player",
		"audioplayer.follow", "menu.hint_audioplayer_follow",
		0, 1, NULL, 0, 0, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(audioplayer_follow)
	},
	// src/neutrino.cpp:1244 src/gui/audioplayer_setup.cpp:125
	{
		"audioplayer_select_title_by_name", ValueType::Bool, "player",
		"audioplayer.select_title_by_name", "menu.hint_audioplayer_title",
		0, 1, NULL, 0, 0, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(audioplayer_select_title_by_name)
	},
	// src/neutrino.cpp:1243 src/gui/audioplayer_setup.cpp:129
	{
		"audioplayer_repeat_on", ValueType::Bool, "player",
		"audioplayer.repeat_on", "menu.hint_audioplayer_repeat",
		0, 1, NULL, 0, 0, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(audioplayer_repeat_on)
	},
	// src/neutrino.cpp:1245 src/gui/audioplayer_setup.cpp:133
	{
		"audioplayer_show_playlist", ValueType::Bool, "player",
		"audioplayer.show_playlist", "menu.hint_audioplayer_playlist",
		0, 1, NULL, 0, 1, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(audioplayer_show_playlist)
	},
	// src/neutrino.cpp:1238 src/gui/audioplayer_setup.cpp:137
	{
		"audioplayer_cover_as_screensaver", ValueType::Bool, "player",
		"audioplayer.cover_as_screensaver", "menu.hint_audioplayer_cover_as_screensaver",
		0, 1, NULL, 0, 1, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(audioplayer_cover_as_screensaver)
	},
	// src/neutrino.cpp:1242 src/gui/audioplayer_setup.cpp:141
	{
		"audioplayer_highprio", ValueType::Bool, "player",
		"audioplayer.highprio", "menu.hint_audioplayer_highprio",
		0, 1, NULL, 0, 0, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(audioplayer_highprio)
	},
	/* The item is in an arm the build never takes, src/gui/audioplayer_setup.cpp:145,
	   and the value is read while a track plays all the same,
	   src/driver/audiodec/mp3dec.cpp:393. The label and the words come off that
	   dead item, which is the only statement of them there is. */
	// src/neutrino.cpp:1247 src/gui/audioplayer_setup.cpp:146
	{
		"spectrum", ValueType::Bool, "player",
		"audioplayer.spectrum", NULL,
		0, 1, NULL, 0, 0, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(spectrum)
	},
	// src/neutrino.cpp:833 src/gui/audioplayer_setup.cpp:148
	{
		"network_nfs_audioplayerdir", ValueType::String, "player",
		"audioplayer.defdir", "menu.hint_audioplayer_defdir",
		0, 0, NULL, 0, 0, TARGET_ROOT "/media/sda1/music", false, false, COREAPI_ALWAYS,
		COREAPI_TEXT_FIELD(network_nfs_audioplayerdir)
	},
	// src/neutrino.cpp:1246 src/gui/audioplayer_setup.cpp:155
	{
		"inetradio_autostart", ValueType::Bool, "player",
		"inetradio.autostart", "menu.hint_inetradio_autostart",
		0, 1, NULL, 0, 0, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(inetradio_autostart)
	},
	// src/neutrino.cpp:1240 src/gui/audioplayer_setup.cpp:159
	{
		"audioplayer_enable_sc_metadata", ValueType::Bool, "player",
		"audioplayer.enable_sc_metadata", "menu.hint_audioplayer_sc_metadata",
		0, 1, NULL, 0, 1, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(audioplayer_enable_sc_metadata)
	},
	// src/neutrino.cpp:837 src/gui/audioplayer_setup.cpp:163
	{
		"network_nfs_streamripperdir", ValueType::String, "player",
		"audioplayer.streamripper_dir", "menu.hint_audioplayer_streamripper_dir",
		0, 0, NULL, 0, 0, TARGET_ROOT "/media/sda1/music/streamripper", false, false, COREAPI_ALWAYS,
		COREAPI_TEXT_FIELD(network_nfs_streamripperdir)
	},
	/* The default is the box's own panel and no constant states it: the loader
	   asks the hardware whether the display is a numeric one. Nought is what
	   every other box gets, and the screen offers the item only where the panel
	   is at least eight characters wide,
	   src/gui/mediaplayer_setup.cpp:128, which is hardware and not a setting. */
	// src/neutrino.cpp:1251 src/gui/mediaplayer_setup.cpp:128
	{
		"movieplayer_display_playtime", ValueType::Bool, "player",
		"movieplayer.display_playtime", "menu.hint_movieplayer_display_playtime",
		0, 1, NULL, 0, 0, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(movieplayer_display_playtime)
	},
	// src/neutrino.cpp:1257 src/gui/mediaplayer_setup.cpp:132
	{
		"movieplayer_timeosd_while_searching", ValueType::Bool, "player",
		"movieplayer.timeosd_while_searching", "menu.hint_movieplayer_timeosd_while_searching",
		0, 1, NULL, 0, 1, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(movieplayer_timeosd_while_searching)
	},
	// How many times the end of a file has to be seen before the player stops.
	// src/neutrino.cpp:1252 src/gui/mediaplayer_setup.cpp:138
	{
		"movieplayer_eof_cnt", ValueType::Int, "player",
		"movieplayer.eof_cnt", NULL,
		1, 10, NULL, 0, 1, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(movieplayer_eof_cnt)
	},
	// src/neutrino.cpp:1234 src/gui/pictureviewer_setup.cpp:103
	{
		"picviewer_scaling", ValueType::Enum, "player",
		"pictureviewer.scaling", "menu.hint_pictureviewer_scaling",
		0, 0, COREAPI_ENUM(kPicviewerScaling), 1, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(picviewer_scaling)
	},
	// Seconds a slide is shown.
	// src/neutrino.cpp:1235 src/gui/pictureviewer_setup.cpp:107
	{
		"picviewer_slide_time", ValueType::Int, "player",
		"pictureviewer.slide_time", "menu.hint_pictureviewer_slide_time",
		0, 999, NULL, 0, 10, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(picviewer_slide_time)
	},
	// src/neutrino.cpp:835 src/gui/pictureviewer_setup.cpp:112
	{
		"network_nfs_picturedir", ValueType::String, "player",
		"pictureviewer.defdir", "menu.hint_pictureviewer_defdir",
		0, 0, NULL, 0, 0, TARGET_ROOT "/media/sda1/pictures", false, false, COREAPI_ALWAYS,
		COREAPI_TEXT_FIELD(network_nfs_picturedir)
	},

	/* The character set the box reads a subtitle file in. The item is in the
	   player's own audio and subtitle chooser rather than a settings screen,
	   and it fills its list at run time, src/gui/movieplayer.cpp:4323, so this
	   is text and not a choice. */
	// src/neutrino.cpp:770 src/gui/movieplayer.cpp:4323
	{
		"subs_charset", ValueType::String, "player",
		"subtitles.charset", NULL,
		0, 0, NULL, 0, 0, "CP1252", false, false, COREAPI_ALWAYS,
		COREAPI_TEXT_FIELD(subs_charset)
	},
	/* What the player repeats, which the player writes itself when the repeat
	   key is pressed, src/gui/movieplayer.cpp:3205. No item names it. A number
	   and not a choice: the three modes have no locales of their own.
	   src/gui/movieplayer.h:89 */
	// src/neutrino.cpp:1253
	{
		"movieplayer_repeat_on", ValueType::Int, "player",
		NULL, NULL,
		0, 2, NULL, 0, 0, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(movieplayer_repeat_on)
	},
#if HAVE_CST_HARDWARE
	/* Behind the arm the settings struct puts the field behind. Whether the
	   player treats a track of no stated kind as AC3, which the audio chooser
	   turns over rather than offering as an item, src/gui/movieplayer.cpp:3862,
	   so the program states no name for it. */
	// src/neutrino.cpp:1255
	{
		"movieplayer_select_ac3_atype0", ValueType::Bool, "player",
		NULL, NULL,
		0, 1, NULL, 0, 0, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(movieplayer_select_ac3_atype0)
	},
#endif
};

} // anonymous namespace

const Descriptor *settingsTablePlayer(size_t &count)
{
	count = sizeof(kSettings) / sizeof(kSettings[0]);
	return kSettings;
}

} // namespace coreapi
