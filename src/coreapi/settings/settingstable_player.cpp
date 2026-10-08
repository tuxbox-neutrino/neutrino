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
#include "predicates.h"
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
constexpr EnumValue kDisplayOrder[] =
{
	option(0).label("audioplayer.artist_title"),
	option(1).label("audioplayer.title_artist")
};

// The order is the screen's and so is the gap: none is last and carries nought.
// src/gui/pictureviewer_setup.cpp:88
constexpr EnumValue kPicviewerScaling[] =
{
	option(1).label("pictureviewer.resize.simple"),
	option(2).label("pictureviewer.resize.color_average"),
	option(0).label("pictureviewer.resize.none")
};

long displayPlaytimeDefault()
{
	return hasNumericPanel() ? 1 : 0;
}

constexpr Descriptor kSettings[] =
{
	// src/neutrino.cpp:1239 src/gui/audioplayer_setup.cpp:117
	enumRow("audioplayer_display")
		.section("player")
		.label("audioplayer.display_order")
		.hint("menu.hint_audioplayer_order")
		.defaultValue(0)
		.values(kDisplayOrder)
		.field(COREAPI_NUMBER_FIELD(audioplayer_display)),
	// src/neutrino.cpp:1241 src/gui/audioplayer_setup.cpp:121
	boolRow("audioplayer_follow")
		.section("player")
		.label("audioplayer.follow")
		.hint("menu.hint_audioplayer_follow")
		.defaultValue(0)
		.field(COREAPI_NUMBER_FIELD(audioplayer_follow)),
	// src/neutrino.cpp:1244 src/gui/audioplayer_setup.cpp:125
	boolRow("audioplayer_select_title_by_name")
		.section("player")
		.label("audioplayer.select_title_by_name")
		.hint("menu.hint_audioplayer_title")
		.defaultValue(0)
		.field(COREAPI_NUMBER_FIELD(audioplayer_select_title_by_name)),
	// src/neutrino.cpp:1243 src/gui/audioplayer_setup.cpp:129
	boolRow("audioplayer_repeat_on")
		.section("player")
		.label("audioplayer.repeat_on")
		.hint("menu.hint_audioplayer_repeat")
		.defaultValue(0)
		.field(COREAPI_NUMBER_FIELD(audioplayer_repeat_on)),
	// src/neutrino.cpp:1245 src/gui/audioplayer_setup.cpp:133
	boolRow("audioplayer_show_playlist")
		.section("player")
		.label("audioplayer.show_playlist")
		.hint("menu.hint_audioplayer_playlist")
		.defaultValue(1)
		.field(COREAPI_NUMBER_FIELD(audioplayer_show_playlist)),
	// src/neutrino.cpp:1238 src/gui/audioplayer_setup.cpp:137
	boolRow("audioplayer_cover_as_screensaver")
		.section("player")
		.label("audioplayer.cover_as_screensaver")
		.hint("menu.hint_audioplayer_cover_as_screensaver")
		.defaultValue(1)
		.field(COREAPI_NUMBER_FIELD(audioplayer_cover_as_screensaver)),
	// src/neutrino.cpp:1242 src/gui/audioplayer_setup.cpp:141
	boolRow("audioplayer_highprio")
		.section("player")
		.label("audioplayer.highprio")
		.hint("menu.hint_audioplayer_highprio")
		.defaultValue(0)
		.field(COREAPI_NUMBER_FIELD(audioplayer_highprio)),
	/* The item is in an arm the build never takes, src/gui/audioplayer_setup.cpp:145,
	   and the value is read while a track plays all the same,
	   src/driver/audiodec/mp3dec.cpp:393. The label and the words come off that
	   dead item, which is the only statement of them there is. */
	// src/neutrino.cpp:1247 src/gui/audioplayer_setup.cpp:146
	boolRow("spectrum")
		.section("player")
		.label("audioplayer.spectrum")
		.defaultValue(0)
		.field(COREAPI_NUMBER_FIELD(spectrum)),
	// src/neutrino.cpp:833 src/gui/audioplayer_setup.cpp:148
	textRow("network_nfs_audioplayerdir")
		.section("player")
		.label("audioplayer.defdir")
		.hint("menu.hint_audioplayer_defdir")
		.defaultValue(TARGET_ROOT "/media/sda1/music")
		.field(COREAPI_TEXT_FIELD(network_nfs_audioplayerdir)),
	// src/neutrino.cpp:1246 src/gui/audioplayer_setup.cpp:155
	boolRow("inetradio_autostart")
		.section("player")
		.label("inetradio.autostart")
		.hint("menu.hint_inetradio_autostart")
		.defaultValue(0)
		.field(COREAPI_NUMBER_FIELD(inetradio_autostart)),
	// src/neutrino.cpp:1240 src/gui/audioplayer_setup.cpp:159
	boolRow("audioplayer_enable_sc_metadata")
		.section("player")
		.label("audioplayer.enable_sc_metadata")
		.hint("menu.hint_audioplayer_sc_metadata")
		.defaultValue(1)
		.field(COREAPI_NUMBER_FIELD(audioplayer_enable_sc_metadata)),
	// src/neutrino.cpp:837 src/gui/audioplayer_setup.cpp:163
	textRow("network_nfs_streamripperdir")
		.section("player")
		.label("audioplayer.streamripper_dir")
		.hint("menu.hint_audioplayer_streamripper_dir")
		.defaultValue(TARGET_ROOT "/media/sda1/music/streamripper")
		.field(COREAPI_TEXT_FIELD(network_nfs_streamripperdir)),
	/* The default is the box's own panel and no constant states it: the loader
	   asks the hardware whether the display is a numeric one. Nought is what
	   every other box gets, and the screen offers the item only where the panel
	   is at least eight characters wide,
	   src/gui/mediaplayer_setup.cpp:128, which is hardware and not a setting. */
	// src/neutrino.cpp:1251 src/gui/mediaplayer_setup.cpp:128
	boolRow("movieplayer_display_playtime")
		.section("player")
		.label("movieplayer.display_playtime")
		.hint("menu.hint_movieplayer_display_playtime")
		.defaultValue(0)
		.defaultFrom(displayPlaytimeDefault)
		.field(COREAPI_NUMBER_FIELD_ON(movieplayer_display_playtime, displayFitsPlaytime, NULL)),
	/* Local resolution: the source declares movieplayer_timeosd_while_searching
	   here. In this fork the key is loaded and saved but nothing reads the
	   member - the player has no use of it and the screen's item sits in
	   #if 0 (src/gui/mediaplayer_setup.cpp:131) - so a row would offer a
	   setting the box does nothing with. The member is carried in
	   settingsundeclared.cpp and is a port candidate if the feature returns
	   (WORK-114). */
	// How many times the end of a file has to be seen before the player stops.
	// src/neutrino.cpp:1252 src/gui/mediaplayer_setup.cpp:138
	intRow("movieplayer_eof_cnt")
		.section("player")
		.label("movieplayer.eof_cnt")
		.range(1, 10)
		.defaultValue(1)
		.field(COREAPI_NUMBER_FIELD(movieplayer_eof_cnt)),
	// src/neutrino.cpp:1234 src/gui/pictureviewer_setup.cpp:103
	enumRow("picviewer_scaling")
		.section("player")
		.label("pictureviewer.scaling")
		.hint("menu.hint_pictureviewer_scaling")
		.defaultValue(1)
		.values(kPicviewerScaling)
		.field(COREAPI_NUMBER_FIELD(picviewer_scaling)),
	// Seconds a slide is shown.
	// src/neutrino.cpp:1235 src/gui/pictureviewer_setup.cpp:107
	intRow("picviewer_slide_time")
		.section("player")
		.label("pictureviewer.slide_time")
		.hint("menu.hint_pictureviewer_slide_time")
		.range(0, 999)
		.defaultValue(10)
		.unit("unit.short.second")
		.field(COREAPI_NUMBER_FIELD(picviewer_slide_time)),
	// src/neutrino.cpp:835 src/gui/pictureviewer_setup.cpp:112
	textRow("network_nfs_picturedir")
		.section("player")
		.label("pictureviewer.defdir")
		.hint("menu.hint_pictureviewer_defdir")
		.defaultValue(TARGET_ROOT "/media/sda1/pictures")
		.field(COREAPI_TEXT_FIELD(network_nfs_picturedir)),

	/* The character set the box reads a subtitle file in. The item is in the
	   player's own audio and subtitle chooser rather than a settings screen,
	   and it fills its list at run time, src/gui/movieplayer.cpp:4323, so this
	   is text and not a choice. */
	// src/neutrino.cpp:770 src/gui/movieplayer.cpp:4323
	textRow("subs_charset")
		.section("player")
		.label("subtitles.charset")
		.defaultValue("CP1252")
		.field(COREAPI_TEXT_FIELD(subs_charset)),
	/* What the player repeats, which the player writes itself when the repeat
	   key is pressed, src/gui/movieplayer.cpp:3205. No item names it. A number
	   and not a choice: the three modes have no locales of their own.
	   src/gui/movieplayer.h:89 */
	// src/neutrino.cpp:1253
	intRow("movieplayer_repeat_on")
		.section("player")
		.range(0, 2)
		.defaultValue(0)
		.field(COREAPI_NUMBER_FIELD(movieplayer_repeat_on)),
#if HAVE_CST_HARDWARE
	/* Behind the arm the settings struct puts the field behind. Whether the
	   player treats a track of no stated kind as AC3, which the audio chooser
	   turns over rather than offering as an item, src/gui/movieplayer.cpp:3862,
	   so the program states no name for it. */
	// src/neutrino.cpp:1255
	boolRow("movieplayer_select_ac3_atype0")
		.section("player")
		.defaultValue(0)
		.field(COREAPI_NUMBER_FIELD(movieplayer_select_ac3_atype0)),
#endif
};

} // anonymous namespace

const Descriptor *settingsTablePlayer(size_t &count)
{
	count = sizeof(kSettings) / sizeof(kSettings[0]);
	return kSettings;
}

} // namespace coreapi
