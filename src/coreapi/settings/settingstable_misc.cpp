/*
 * settingstable_misc.cpp - miscellaneous settings, one row per field
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

/* The misc section, as the program states it: the key, the type and the default
   off the line that loads the setting, the bounds, the choices and the labels
   off the screen that offers it. Every row cites both, because a bound that is
   nearly right reads as correct on the page and most of the numbers below are
   ones no extractor holds to. The screen has nine submenus and the descriptor
   carries no sub-section, so that structure is not in the table. Nothing here
   needs a restart: each of these is read again by whatever acts on it. */

/* The one inverted flag in the tree. Its screen reads nought as on, so a Bool
   row would turn a frontend's true into the opposite and nothing would say so.
   src/gui/widget/menue_options.h:8 */
const EnumValue kShutdownReal[] =
{
	{ 0, "options.on" },
	{ 1, "options.off" }
};

// src/gui/miscsettings_menu.cpp:194
const EnumValue kFilesystemUtf8[] =
{
	{ 0, "filesystem.is.utf8.option.iso8859.1" },
	{ 1, "filesystem.is.utf8.option.utf8" }
};

// src/gui/miscsettings_menu.cpp:201
const EnumValue kNewZapMode[] =
{
	{ 0, "channellist.new_zap_mode_off" },
	{ 1, "channellist.new_zap_mode_allow" },
	{ 2, "channellist.new_zap_mode_active" }
};

// src/gui/miscsettings_menu.cpp:209
const EnumValue kEnableSdt[] =
{
	{ 0, "channellist.enablesdt_off" },
	{ 1, "channellist.enablesdt_on" },
	{ 2, "channellist.enablesdt_on_extended" }
};

/* The off value the enum carries is not offered here: the loader turns a stored
   nought into the first of the three below and turns the scan off through the
   mode beside it. src/neutrino.cpp:791 src/gui/miscsettings_menu.cpp:235 */
const EnumValue kEpgScan[] =
{
	{ 1, "miscsettings.epg_scan_bq" },
	{ 2, "miscsettings.epg_scan_fav" },
	{ 3, "miscsettings.epg_scan_sel" }
};

// The screen makes the two below editable only while the box is left to switch
// off for real. src/gui/miscsettings_menu.cpp:422
const Condition kShutdownRealOff[] =
{
	{ "shutdown_real", CompareOp::Eq, 0, NULL, 0 }
};

// src/gui/miscsettings_menu.cpp:478
const Condition kEpgSaveOn[] =
{
	{ "epg_save", CompareOp::Ne, 0, NULL, 0 }
};

// src/gui/miscsettings_menu.cpp:487
const Condition kEpgReadOn[] =
{
	{ "epg_read", CompareOp::Ne, 0, NULL, 0 }
};

/* Where the box goes when an advert break is spotted, src/gui/adzap.cpp:346.
   The numbers are the enum the settings struct declares,
   src/system/settings.h:981. */
const EnumValue kAdzapZap[] =
{
	{ 0, "adzap.zap_off" },
	{ 1, "adzap.zap_to_last_channel" },
	{ 2, "adzap.zap_to_start_channel" }
};

const Descriptor kMisc[] =
{
	// general
	// src/neutrino.cpp:724 src/gui/miscsettings_menu.cpp:391
	{
		"power_standby", ValueType::Bool, "misc",
		"extra.start_tostandby", "menu.hint_start_tostandby",
		0, 1, NULL, 0, 0, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(power_standby)
	},
	// src/neutrino.cpp:703 src/gui/miscsettings_menu.cpp:395
	{
		"cacheTXT", ValueType::Bool, "misc",
		"extra.cache_txt", "menu.hint_cache_txt",
		0, 1, NULL, 0, 1, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(cacheTXT)
	},
	/* Offered only where the box has a fan, a capability read at run time.
	   Nought names the fan off and the chooser cannot reach it: it clamps to
	   the floor, src/gui/widget/menue.cpp:1921. */
	// src/neutrino.cpp:526 src/gui/miscsettings_menu.cpp:404
	{
		"fan_speed", ValueType::Int, "misc",
		"fan_speed", "menu.hint_fan_speed",
		1, 14, NULL, 0, 1, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(fan_speed)
	},

	// energy and shutdown
	// src/neutrino.cpp:714 src/gui/miscsettings_menu.cpp:436
	{
		"shutdown_real", ValueType::Enum, "misc",
		"miscsettings.shutdown_real", "menu.hint_shutdown_real",
		0, 0, COREAPI_VALUES(kShutdownReal), 0, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(shutdown_real)
	},
	// src/neutrino.cpp:721 src/gui/miscsettings_menu.cpp:422
	{
		"shutdown_real_rcdelay", ValueType::Bool, "misc",
		"miscsettings.shutdown_real_rcdelay", "menu.hint_shutdown_rcdelay",
		0, 1, NULL, 0, 0, NULL, false, false, COREAPI_CONDITIONS(kShutdownRealOff),
		COREAPI_NUMBER_FIELD(shutdown_real_rcdelay)
	},
	// The screen offers this one only where the box can switch off, which is a
	// capability read at run time. src/gui/miscsettings_menu.cpp:444
	// src/neutrino.cpp:722 src/gui/miscsettings_menu.cpp:446
	{
		"shutdown_block_while_recording", ValueType::Bool, "misc",
		"miscsettings.shutdown_block_recording", "menu.hint_shutdown_block_recording",
		0, 1, NULL, 0, 0, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(shutdown_block_while_recording)
	},
	/* Minutes, and nought says the box takes the length off the programme it is
	   showing. A number and not a choice, because the screen names its six
	   under literal words rather than locales and no scan can read that table.
	   Bound: the last of the six, src/gui/miscsettings_menu.cpp:253. */
	// src/neutrino.cpp:723 src/gui/miscsettings_menu.cpp:455
	{
		"sleeptimer_min", ValueType::Int, "misc",
		"miscsettings.sleeptimer_min", "menu.hint_sleeptimer_min",
		0, 150, NULL, 0, 0, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(sleeptimer_min)
	},

	// epg
	// src/neutrino.cpp:784 src/gui/miscsettings_menu.cpp:475
	{
		"epg_save", ValueType::Bool, "misc",
		"miscsettings.epg_save", "menu.hint_epg_save",
		0, 1, NULL, 0, 0, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(epg_save)
	},
	// src/neutrino.cpp:787 src/gui/miscsettings_menu.cpp:478
	{
		"epg_save_standby", ValueType::Bool, "misc",
		"miscsettings.epg_save_standby", "menu.hint_epg_save_standby",
		0, 1, NULL, 0, 1, NULL, false, false, COREAPI_CONDITIONS(kEpgSaveOn),
		COREAPI_NUMBER_FIELD(epg_save_standby)
	},
	// src/neutrino.cpp:785 src/gui/miscsettings_menu.cpp:481
	{
		"epg_save_frequently", ValueType::Bool, "misc",
		"miscsettings.epg_save_frequently", "menu.hint_epg_save_frequently",
		0, 1, NULL, 0, 0, NULL, false, false, COREAPI_CONDITIONS(kEpgSaveOn),
		COREAPI_NUMBER_FIELD(epg_save_frequently)
	},
	// src/neutrino.cpp:786 src/gui/miscsettings_menu.cpp:531
	{
		"epg_save_mode", ValueType::Bool, "misc",
		"miscsettings.epg_save_mode", "menu.hint_epg_save_mode",
		0, 1, NULL, 0, 0, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(epg_save_mode)
	},
	// src/neutrino.cpp:782 src/gui/miscsettings_menu.cpp:484
	{
		"epg_read", ValueType::Bool, "misc",
		"miscsettings.epg_read", "menu.hint_epg_read",
		0, 1, NULL, 0, 1, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(epg_read)
	},
	// src/neutrino.cpp:783 src/gui/miscsettings_menu.cpp:487
	{
		"epg_read_frequently", ValueType::Bool, "misc",
		"miscsettings.epg_read_frequently", "menu.hint_epg_read_frequently",
		0, 1, NULL, 0, 1, NULL, false, false, COREAPI_CONDITIONS(kEpgReadOn),
		COREAPI_NUMBER_FIELD(epg_read_frequently)
	},
	/* Offered while either of the two above is on, a disjunction across two
	   keys that a conjunction of comparisons cannot say, so it carries no
	   condition and is always shown. */
	// src/neutrino.cpp:777 src/gui/miscsettings_menu.cpp:493
	{
		"epg_dir", ValueType::String, "misc",
		"miscsettings.epg_dir", "menu.hint_epg_dir",
		0, 0, NULL, 0, 0, TARGET_ROOT "/media/sda1/epg", false, false, COREAPI_ALWAYS,
		COREAPI_TEXT_FIELD(epg_dir)
	},
	// src/neutrino.cpp:788 src/gui/miscsettings_menu.cpp:524
	{
		"epg_scan", ValueType::Enum, "misc",
		"miscsettings.epg_scan_bouquets", "menu.hint_epg_scan",
		0, 0, COREAPI_VALUES(kEpgScan), 2, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(epg_scan)
	},
	/* A number and not a choice: one of the four words the screen puts beside a
	   value stands for a number two headers state differently, so no scan can
	   read that table. The value is a pair of bits, live and standby, so every
	   number in the range is a mode. Bound: src/driver/scanepg.h:39. The two
	   standby modes are offered only where more than one tuner is enabled. */
	// src/neutrino.cpp:789 src/gui/miscsettings_menu.cpp:527
	{
		"epg_scan_mode", ValueType::Int, "misc",
		"miscsettings.epg_scan", "menu.hint_epg_scan_mode",
		0, 3, NULL, 0, 2, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(epg_scan_mode)
	},

	// file browser
	// src/neutrino.cpp:707 src/gui/miscsettings_menu.cpp:561
	{
		"filesystem_is_utf8", ValueType::Enum, "misc",
		"filesystem.is.utf8", "menu.hint_filesystem_is_utf8",
		0, 0, COREAPI_VALUES(kFilesystemUtf8), 1, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(filesystem_is_utf8)
	},
	// src/neutrino.cpp:1261 src/gui/miscsettings_menu.cpp:565
	{
		"filebrowser_showrights", ValueType::Bool, "misc",
		"filebrowser.showrights", "menu.hint_filebrowser_showrights",
		0, 1, NULL, 0, 1, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(filebrowser_showrights)
	},
	// src/neutrino.cpp:1260 src/gui/miscsettings_menu.cpp:568
	{
		"filebrowser_denydirectoryleave", ValueType::Bool, "misc",
		"filebrowser.denydirectoryleave", "menu.hint_filebrowser_denydirectoryleave",
		0, 1, NULL, 0, 0, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(filebrowser_denydirectoryleave)
	},

	// channel list
	// src/neutrino.cpp:622 src/gui/miscsettings_menu.cpp:585
	{
		"make_hd_list", ValueType::Bool, "misc",
		"channellist.make_hdlist", "menu.hint_make_hdlist",
		0, 1, NULL, 0, 0, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(make_hd_list)
	},
	// src/neutrino.cpp:626 src/gui/miscsettings_menu.cpp:589
	{
		"make_webtv_list", ValueType::Bool, "misc",
		"channellist.make_webtvlist", "menu.hint_make_webtvlist",
		0, 1, NULL, 0, 1, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(make_webtv_list)
	},
	// src/neutrino.cpp:625 src/gui/miscsettings_menu.cpp:593
	{
		"make_webradio_list", ValueType::Bool, "misc",
		"channellist.make_webradiolist", "menu.hint_make_webradiolist",
		0, 1, NULL, 0, 1, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(make_webradio_list)
	},
	// src/neutrino.cpp:623 src/gui/miscsettings_menu.cpp:597
	{
		"make_new_list", ValueType::Bool, "misc",
		"channellist.make_newlist", "menu.hint_make_newlist",
		0, 1, NULL, 0, 1, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(make_new_list)
	},
	// src/neutrino.cpp:624 src/gui/miscsettings_menu.cpp:601
	{
		"make_removed_list", ValueType::Bool, "misc",
		"channellist.make_removedlist", "menu.hint_make_removedlist",
		0, 1, NULL, 0, 1, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(make_removed_list)
	},
	// src/neutrino.cpp:628 src/gui/miscsettings_menu.cpp:605
	{
		"keep_channel_numbers", ValueType::Bool, "misc",
		"channellist.keep_numbers", "menu.hint_keep_numbers",
		0, 1, NULL, 0, 0, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(keep_channel_numbers)
	},
	// src/neutrino.cpp:699 src/gui/miscsettings_menu.cpp:609
	{
		"zap_cycle", ValueType::Bool, "misc",
		"extra.zap_cycle", "menu.hint_zap_cycle",
		0, 1, NULL, 0, 1, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(zap_cycle)
	},
	// src/neutrino.cpp:1300 src/gui/miscsettings_menu.cpp:613
	{
		"channellist_new_zap_mode", ValueType::Enum, "misc",
		"channellist.new_zap_mode", "menu.hint_new_zap_mode",
		0, 0, COREAPI_VALUES(kNewZapMode), 0, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(channellist_new_zap_mode)
	},
	// src/neutrino.cpp:1301 src/gui/miscsettings_menu.cpp:617
	{
		"channellist_numeric_adjust", ValueType::Bool, "misc",
		"channellist.numeric_adjust", "menu.hint_numeric_adjust",
		0, 1, NULL, 0, 0, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(channellist_numeric_adjust)
	},
	// src/neutrino.cpp:629 src/gui/miscsettings_menu.cpp:621
	{
		"show_empty_favorites", ValueType::Bool, "misc",
		"channellist.show_empty_favs", "menu.hint_channellist_show_empty_favs",
		0, 1, NULL, 0, 0, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(show_empty_favorites)
	},
	// src/neutrino.cpp:701 src/gui/miscsettings_menu.cpp:625
	{
		"enable_sdt", ValueType::Enum, "misc",
		"miscsettings.channellist_enablesdt", "menu.hint_channellist_enablesdt",
		0, 0, COREAPI_VALUES(kEnableSdt), 1, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(enable_sdt)
	},

	/* Online services. Each of the four flags is editable only where the key
	   beside it passes a check that reads the key itself, which is a function
	   call and not a comparison against another setting, so none of them
	   carries a condition. Each key defaults to the placeholder below only
	   where the build carries none of its own; one configured with a key falls
	   back to that key instead, which is not a constant this can carry.
	   src/neutrino.cpp:1168, :1173

	   The four keys are credentials and are declared secret. The flags beside
	   them are not: which service a box uses is not a secret, and marking them
	   would hide the state of a switch a frontend has to draw. */
	// src/neutrino.cpp:1179 src/gui/miscsettings_menu.cpp:655
	{
		"tmdb_enabled", ValueType::Bool, "misc",
		"tmdb.enabled", "menu.hint_tmdb_enabled",
		0, 1, NULL, 0, 1, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(tmdb_enabled)
	},
	// src/neutrino.cpp:1177 src/gui/miscsettings_menu.cpp:661
	{
		"tmdb_api_key", ValueType::String, "misc",
		"tmdb.api_key", "menu.hint_tmdb_api_key",
		0, 0, NULL, 0, 0, "XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX", false, true, COREAPI_ALWAYS,
		COREAPI_TEXT_FIELD(tmdb_api_key)
	},
	// src/neutrino.cpp:1186 src/gui/miscsettings_menu.cpp:670
	{
		"omdb_enabled", ValueType::Bool, "misc",
		"omdb.enabled", "menu.hint_omdb_enabled",
		0, 1, NULL, 0, 1, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(omdb_enabled)
	},
	// src/neutrino.cpp:1184 src/gui/miscsettings_menu.cpp:676
	{
		"omdb_api_key", ValueType::String, "misc",
		"omdb.api_key", "menu.hint_omdb_api_key",
		0, 0, NULL, 0, 0, "XXXXXXXX", false, true, COREAPI_ALWAYS,
		COREAPI_TEXT_FIELD(omdb_api_key)
	},
	// src/neutrino.cpp:1193 src/gui/miscsettings_menu.cpp:685
	{
		"shoutcast_enabled", ValueType::Bool, "misc",
		"shoutcast.enabled", "menu.hint_shoutcast_enabled",
		0, 1, NULL, 0, 1, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(shoutcast_enabled)
	},
	// src/neutrino.cpp:1191 src/gui/miscsettings_menu.cpp:691
	{
		"shoutcast_dev_id", ValueType::String, "misc",
		"shoutcast.dev_id", "menu.hint_shoutcast_dev_id",
		0, 0, NULL, 0, 0, "XXXXXXXXXXXXXXXX", false, true, COREAPI_ALWAYS,
		COREAPI_TEXT_FIELD(shoutcast_dev_id)
	},
	// src/neutrino.cpp:1172 src/gui/miscsettings_menu.cpp:700
	{
		"youtube_enabled", ValueType::Bool, "misc",
		"youtube.enabled", "menu.hint_youtube_enabled",
		0, 1, NULL, 0, 1, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(youtube_enabled)
	},
	// src/neutrino.cpp:1170 src/gui/miscsettings_menu.cpp:706
	{
		"youtube_api_key", ValueType::String, "misc",
		"youtube.api_key", "menu.hint_youtube_api_key",
		0, 0, NULL, 0, 0, "XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX", false, true, COREAPI_ALWAYS,
		COREAPI_TEXT_FIELD(youtube_api_key)
	},

	// plugins
	// src/neutrino.cpp:1025 src/gui/miscsettings_menu.cpp:722
	{
		"plugin_hdd_dir", ValueType::String, "misc",
		"plugins.hdd_dir", "menu.hint_plugins_hdd_dir",
		0, 0, NULL, 0, 0, TARGET_ROOT "/media/sda1/plugins", false, false, COREAPI_ALWAYS,
		COREAPI_TEXT_FIELD(plugin_hdd_dir)
	},
	// The name of a plugin, and the default is what the screen offers as none.
	// src/neutrino.cpp:1033 src/gui/miscsettings_menu.cpp:726
	{
		"movieplayer_plugin", ValueType::String, "misc",
		"mpkey.plugin", "menu.hint_movieplayer_plugin",
		0, 0, NULL, 0, 0, "---", false, false, COREAPI_ALWAYS,
		COREAPI_TEXT_FIELD(movieplayer_plugin)
	},

	// streaming
	// Bound: the five digits the screen takes.
	// src/neutrino.cpp:870 src/gui/miscsettings_menu.cpp:750
	{
		"streaming_port", ValueType::Int, "misc",
		"streaming.port", NULL,
		0, 99999, NULL, 0, 31339, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(streaming_port)
	},
	// src/neutrino.cpp:868 src/gui/miscsettings_menu.cpp:755
	{
		"streaming_ecmmode", ValueType::Bool, "misc",
		"streaming.ecmmode", NULL,
		0, 1, NULL, 0, 0, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(streaming_ecmmode)
	},
	// src/neutrino.cpp:869 src/gui/miscsettings_menu.cpp:760
	{
		"streaming_decryptmode", ValueType::Bool, "misc",
		"streaming.decryptmode", NULL,
		0, 1, NULL, 0, 1, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(streaming_decryptmode)
	},

	/* The two below are numbers and not choices, because the screen names its
	   thirteen steps under literal words rather than locales. Bound: nought and
	   the last of the thirteen, src/gui/miscsettings_menu.cpp:218. Both are
	   offered only where the box can change its clock, and the loader forces a
	   value where it cannot, nought for the first and fifty for the second. */
	// src/neutrino.cpp:578 src/gui/miscsettings_menu.cpp:774
	{
		"cpufreq", ValueType::Int, "misc",
		"cpu.freq_normal", NULL,
		0, 600, NULL, 0, 0, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(cpufreq)
	},
	// src/neutrino.cpp:579 src/gui/miscsettings_menu.cpp:775
	{
		"standby_cpufreq", ValueType::Int, "misc",
		"cpu.freq_standby", NULL,
		0, 600, NULL, 0, 100, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(standby_cpufreq)
	},

	/* What follows has no item on any screen, or an item a screen offers
	   outside the settings menus. Where the program states no name for a
	   setting the row says so rather than borrowing the name of a neighbouring
	   item, which reads as right on the page and is wrong on the screen. */
	/* The small channel list, which the lists ask for while they draw.
	   src/neutrino.cpp:3553 */
	// src/neutrino.cpp:705
	{
		"minimode", ValueType::Bool, "misc",
		NULL, NULL,
		0, 1, NULL, 0, 0, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(minimode)
	},
	/* Three digits on the screen, src/gui/miscsettings_menu.cpp:428, the only
	   bound stated anywhere: the value is kept as text there and turned into a
	   number on the way out, :461. */
	// src/neutrino.cpp:710 src/gui/miscsettings_menu.cpp:429
	{
		"shutdown_count", ValueType::Int, "misc",
		"miscsettings.shutdown_count", "menu.hint_shutdown_count",
		0, 999, NULL, 0, 0, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(shutdown_count)
	},
	/* The standing sleep timer in minutes. Its item is the sleep timer box and
	   holds the value as three digits of text, src/gui/sleeptimer.cpp:68. The
	   loader reads it only where the box can switch itself off. */
	// src/neutrino.cpp:713 src/gui/sleeptimer.cpp:68
	{
		"shutdown_min", ValueType::Int, "misc",
		"sleeptimerbox.title2", NULL,
		0, 999, NULL, 0, 0, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(shutdown_min)
	},
	/* Which of the three the power menu did last, standby, off or restart,
	   src/gui/poweroff_menu.cpp:56, :62 and :68. The menu writes it and reads
	   it back to mark the entry, :119; no item names it. Its default is the
	   switch beside it, and the literal below is what that one falls back to. */
	// src/neutrino.cpp:727
	{
		"power_off_selected", ValueType::Int, "misc",
		NULL, NULL,
		0, 2, NULL, 0, 0, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(power_off_selected)
	},
	/* Whether the box follows a recorder onto its own input. Read once a
	   message arrives from one, src/neutrino.cpp:4468, and offered by no screen
	   at all; the web interface offers it instead. */
	// src/neutrino.cpp:738
	{
		"vcr_AutoSwitch", ValueType::Bool, "misc",
		NULL, NULL,
		0, 1, NULL, 0, 1, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(vcr_AutoSwitch)
	},
	/* Whether the up and down keys walk the audio tracks while the infobar
	   stands, src/daemonc/remotecontrol.cpp. No screen offers it. */
	// src/neutrino.cpp:744
	{
		"audiochannel_up_down_enable", ValueType::Bool, "misc",
		NULL, NULL,
		0, 1, NULL, 0, 0, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(audiochannel_up_down_enable)
	},
	/* The four below are the section daemon's cache sizes. Each has an item
	   that keeps the value as a fixed number of digits of text, which is the
	   only bound stated anywhere and is what the bounds here are. They reach
	   the daemon through the same call as the time settings,
	   src/neutrino.cpp:2779. */
	// src/neutrino.cpp:778 src/gui/miscsettings_menu.cpp:500
	{
		"epg_cache_time", ValueType::Int, "misc",
		"miscsettings.epg_cache", "menu.hint_epg_cache",
		0, 99, NULL, 0, 7, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(epg_cache)
	},
	// src/neutrino.cpp:779 src/gui/miscsettings_menu.cpp:507
	{
		"epg_extendedcache_time", ValueType::Int, "misc",
		"miscsettings.epg_extendedcache", "menu.hint_epg_extendedcache",
		0, 999, NULL, 0, 168, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(epg_extendedcache)
	},
	// src/neutrino.cpp:780 src/gui/miscsettings_menu.cpp:521
	{
		"epg_max_events", ValueType::Int, "misc",
		"miscsettings.epg_max_events", "menu.hint_epg_max_events",
		0, 999999, NULL, 0, 30000, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(epg_max_events)
	},
	// src/neutrino.cpp:781 src/gui/miscsettings_menu.cpp:514
	{
		"epg_old_events", ValueType::Int, "misc",
		"miscsettings.epg_old_events", "menu.hint_epg_old_events",
		0, 999, NULL, 0, 1, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(epg_old_events)
	},
	/* How many hours pass before the standby scan runs again,
	   src/driver/scanepg.cpp:325. Nothing states a bound for it, so the row
	   states the widest the field holds and a floor of nought. A bound no
	   screen governs and no check can compare. */
	// src/neutrino.cpp:796
	{
		"epg_scan_rescan", ValueType::Int, "misc",
		NULL, NULL,
		0, 2147483647, NULL, 0, 24, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(epg_scan_rescan)
	},
	// src/neutrino.cpp:799 src/gui/eventlist.cpp:1482
	{
		"epg_search_history_max", ValueType::Int, "misc",
		"eventfinder.max_history", NULL,
		0, 50, NULL, 0, 10, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(epg_search_history_max)
	},
	/* How many searches the box actually kept, which the search list writes
	   itself and holds to the count above, src/gui/eventlist.cpp:1243. Writing
	   it does not add a search; the ceiling is the one the list enforces. */
	// src/neutrino.cpp:800
	{
		"epg_search_history_size", ValueType::Int, "misc",
		NULL, NULL,
		0, 50, NULL, 0, 0, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(epg_search_history_size)
	},
	/* Which keyboard the on screen one comes up as, written by the keyboard
	   itself when it is changed, src/gui/widget/keyboard_input.cpp:345, and
	   read the next time it opens, :337. Empty means the box picks by
	   language. */
	// src/neutrino.cpp:773
	{
		"keyboard_layout", ValueType::String, "misc",
		NULL, NULL,
		0, 0, NULL, 0, 0, "", false, false, COREAPI_ALWAYS,
		COREAPI_TEXT_FIELD(keyboard_layout)
	},
	/* How the file browser sorts, which it writes itself. The loader holds it
	   to the number of sorts there are and turns anything else into nought,
	   src/neutrino.cpp:1263, and that count is the ceiling here. */
	// src/neutrino.cpp:1262
	{
		"filebrowser_sortmethod", ValueType::Int, "misc",
		NULL, NULL,
		0, 4, NULL, 0, 1, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(filebrowser_sortmethod)
	},
	/* Whether the event view opens in the larger window,
	   src/gui/epgview.cpp:752. Read as a flag and offered by no screen. */
	// src/neutrino.cpp:1287
	{
		"bigFonts", ValueType::Bool, "misc",
		NULL, NULL,
		0, 1, NULL, 0, 1, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(bigFonts)
	},
	/* The code that guards the personalisation menu. A credential, so a read
	   answers nothing and an empty write is refused; the declared default is
	   the program's own fallback and not a box's value. Nothing here asks for
	   the old code before taking a new one. */
	// src/neutrino.cpp:934 src/gui/personalize.cpp:445
	{
		"personalize_pincode", ValueType::String, "misc",
		"personalize.pincode", "personalize.pinhint",
		0, 0, NULL, 0, 0, "0000", false, true, COREAPI_ALWAYS,
		COREAPI_TEXT_FIELD(personalize_pincode)
	},
	/* The five below are lists of plugin file names joined with commas, which
	   the personalisation screen rebuilds whole whenever it is left,
	   src/gui/personalize.cpp:388. A name this layer cannot check is what a
	   frontend would be writing. */
	// src/neutrino.cpp:1026
	{
		"plugins_disabled", ValueType::String, "misc",
		NULL, NULL,
		0, 0, NULL, 0, 0, "", false, false, COREAPI_ALWAYS,
		COREAPI_TEXT_FIELD(plugins_disabled)
	},
	// src/neutrino.cpp:1027
	{
		"plugins_game", ValueType::String, "misc",
		NULL, NULL,
		0, 0, NULL, 0, 0, "", false, false, COREAPI_ALWAYS,
		COREAPI_TEXT_FIELD(plugins_game)
	},
	// src/neutrino.cpp:1028
	{
		"plugins_lua", ValueType::String, "misc",
		NULL, NULL,
		0, 0, NULL, 0, 0, "", false, false, COREAPI_ALWAYS,
		COREAPI_TEXT_FIELD(plugins_lua)
	},
	// src/neutrino.cpp:1029
	{
		"plugins_script", ValueType::String, "misc",
		NULL, NULL,
		0, 0, NULL, 0, 0, "", false, false, COREAPI_ALWAYS,
		COREAPI_TEXT_FIELD(plugins_script)
	},
	// src/neutrino.cpp:1030
	{
		"plugins_tool", ValueType::String, "misc",
		NULL, NULL,
		0, 0, NULL, 0, 0, "", false, false, COREAPI_ALWAYS,
		COREAPI_TEXT_FIELD(plugins_tool)
	},
	/* Where the box goes back to after an advert break, in seconds. The menu
	   offers nine fixed minutes and then a chooser over ten to a hundred and
	   twenty, src/gui/adzap.cpp:357 and :369, so the floor is the smallest of
	   the nine and the ceiling the chooser's own, both in seconds. That chooser
	   binds a local in minutes rather than the field, so its label names
	   minutes and this row is seconds; the row therefore states no label. */
	// src/neutrino.cpp:1204 src/gui/adzap.cpp:369
	{
		"adzap_zapBackPeriod", ValueType::Int, "misc",
		NULL, NULL,
		60, 7200, NULL, 0, 180, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(adzap_zapBackPeriod)
	},
	// src/neutrino.cpp:1205 src/gui/adzap.cpp:346
	{
		"adzap_zapOnActivation", ValueType::Enum, "misc",
		"adzap.zap", "menu.hint_adzap_zap",
		0, 0, COREAPI_VALUES(kAdzapZap), 0, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(adzap_zapOnActivation)
	},
	// src/neutrino.cpp:1206 src/gui/adzap.cpp:342
	{
		"adzap_writeData", ValueType::Bool, "misc",
		"adzap.writedata", "menu.hint_adzap_writedata",
		0, 1, NULL, 0, 0, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(adzap_writeData)
	},
	/* Where the settings backup is written. Two file browsers write it,
	   src/gui/keybind_setup.cpp:126 and src/gui/settings_manager.cpp:79, and
	   neither names it: the one locale that carries the words is a question
	   with the directory in it and not the name of a field. */
	// src/neutrino.cpp:1122
	{
		"backup_dir", ValueType::String, "misc",
		NULL, NULL,
		0, 0, NULL, 0, 0, TARGET_ROOT "/media", false, false, COREAPI_ALWAYS,
		COREAPI_TEXT_FIELD(backup_dir)
	},
};

} // anonymous namespace

const Descriptor *settingsTableMisc(size_t &count)
{
	count = sizeof(kMisc) / sizeof(kMisc[0]);
	return kMisc;
}

} // namespace coreapi
