/*
 * settingstable_misc.cpp - miscellaneous settings, one row per field
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
constexpr EnumValue kShutdownReal[] =
{
	option(0).label("options.on"),
	option(1).label("options.off")
};

// src/gui/miscsettings_menu.cpp:194
constexpr EnumValue kFilesystemUtf8[] =
{
	option(0).label("filesystem.is.utf8.option.iso8859.1"),
	option(1).label("filesystem.is.utf8.option.utf8")
};

// src/gui/miscsettings_menu.cpp:201
constexpr EnumValue kNewZapMode[] =
{
	option(0).label("channellist.new_zap_mode_off"),
	option(1).label("channellist.new_zap_mode_allow"),
	option(2).label("channellist.new_zap_mode_active")
};

// src/gui/miscsettings_menu.cpp:209
constexpr EnumValue kEnableSdt[] =
{
	option(0).label("channellist.enablesdt_off"),
	option(1).label("channellist.enablesdt_on"),
	option(2).label("channellist.enablesdt_on_extended")
};

/* The off value the enum carries is not offered here: the loader turns a stored
   nought into the first of the three below and turns the scan off through the
   mode beside it. src/neutrino.cpp:791 src/gui/miscsettings_menu.cpp:235 */
constexpr EnumValue kEpgScan[] =
{
	option(1).label("miscsettings.epg_scan_bq"),
	option(2).label("miscsettings.epg_scan_fav"),
	option(3).label("miscsettings.epg_scan_sel")
};

// The screen makes the two below editable only while the box is left to switch
// off for real. src/gui/miscsettings_menu.cpp:422
constexpr Condition kShutdownRealOff[] =
{
	when("shutdown_real").is(0)
};

// src/gui/miscsettings_menu.cpp:478
constexpr Condition kEpgSaveOn[] =
{
	when("epg_save").isNot(0)
};

// src/gui/miscsettings_menu.cpp:487
constexpr Condition kEpgReadOn[] =
{
	when("epg_read").isNot(0)
};

/* Where the box goes when an advert break is spotted, src/gui/adzap.cpp:346.
   The numbers are the enum the settings struct declares,
   src/system/settings.h:981. */
constexpr EnumValue kAdzapZap[] =
{
	option(0).label("adzap.zap_off"),
	option(1).label("adzap.zap_to_last_channel"),
	option(2).label("adzap.zap_to_start_channel")
};

constexpr Descriptor kMisc[] =
{
	// general
	// src/neutrino.cpp:724 src/gui/miscsettings_menu.cpp:391
	boolRow("power_standby")
		.section("misc")
		.label("extra.start_tostandby")
		.hint("menu.hint_start_tostandby")
		.defaultValue(0)
		.field(COREAPI_NUMBER_FIELD(power_standby)),
	// src/neutrino.cpp:703 src/gui/miscsettings_menu.cpp:395
	boolRow("cacheTXT")
		.section("misc")
		.label("extra.cache_txt")
		.hint("menu.hint_cache_txt")
		.defaultValue(1)
		.field(COREAPI_NUMBER_FIELD(cacheTXT)),
	/* Offered only where the box has a fan, a capability read at run time.
	   Nought names the fan off and the chooser cannot reach it: it clamps to
	   the floor, src/gui/widget/menue.cpp:1921. */
	// src/neutrino.cpp:526 src/gui/miscsettings_menu.cpp:404
	intRow("fan_speed")
		.section("misc")
		.label("fan_speed")
		.hint("menu.hint_fan_speed")
		.range(1, 14)
		.defaultValue(1)
		.field(COREAPI_NUMBER_FIELD_ON(fan_speed, hasFan, NULL)),

	// energy and shutdown
	// src/neutrino.cpp:714 src/gui/miscsettings_menu.cpp:436
	enumRow("shutdown_real")
		.section("misc")
		.label("miscsettings.shutdown_real")
		.hint("menu.hint_shutdown_real")
		.defaultValue(0)
		.values(kShutdownReal)
		.field(COREAPI_NUMBER_FIELD(shutdown_real)),
	// src/neutrino.cpp:721 src/gui/miscsettings_menu.cpp:422
	boolRow("shutdown_real_rcdelay")
		.section("misc")
		.label("miscsettings.shutdown_real_rcdelay")
		.hint("menu.hint_shutdown_rcdelay")
		.defaultValue(0)
		.changeableWhen(kShutdownRealOff)
		.field(COREAPI_NUMBER_FIELD(shutdown_real_rcdelay)),
	// The screen offers this one only where the box can switch off, which is a
	// capability read at run time. src/gui/miscsettings_menu.cpp:444
	// src/neutrino.cpp:722 src/gui/miscsettings_menu.cpp:446
	boolRow("shutdown_block_while_recording")
		.section("misc")
		.label("miscsettings.shutdown_block_recording")
		.hint("menu.hint_shutdown_block_recording")
		.defaultValue(0)
		.field(COREAPI_NUMBER_FIELD_ON(shutdown_block_while_recording, canShutdown, NULL)),
	/* Minutes, and nought says the box takes the length off the programme it is
	   showing. A number and not a choice, because the screen names its six
	   under literal words rather than locales and no scan can read that table.
	   Bound: the last of the six, src/gui/miscsettings_menu.cpp:253. */
	// src/neutrino.cpp:723 src/gui/miscsettings_menu.cpp:455
	intRow("sleeptimer_min")
		.section("misc")
		.label("miscsettings.sleeptimer_min")
		.hint("menu.hint_sleeptimer_min")
		.range(0, 150)
		.defaultValue(0)
		.field(COREAPI_NUMBER_FIELD(sleeptimer_min)),

	// epg
	// src/neutrino.cpp:784 src/gui/miscsettings_menu.cpp:475
	boolRow("epg_save")
		.section("misc")
		.label("miscsettings.epg_save")
		.hint("menu.hint_epg_save")
		.defaultValue(0)
		.field(COREAPI_NUMBER_FIELD(epg_save)),
	// src/neutrino.cpp:787 src/gui/miscsettings_menu.cpp:478
	boolRow("epg_save_standby")
		.section("misc")
		.label("miscsettings.epg_save_standby")
		.hint("menu.hint_epg_save_standby")
		.defaultValue(1)
		.changeableWhen(kEpgSaveOn)
		.field(COREAPI_NUMBER_FIELD(epg_save_standby)),
	// src/neutrino.cpp:785 src/gui/miscsettings_menu.cpp:481
	boolRow("epg_save_frequently")
		.section("misc")
		.label("miscsettings.epg_save_frequently")
		.hint("menu.hint_epg_save_frequently")
		.defaultValue(0)
		.changeableWhen(kEpgSaveOn)
		.field(COREAPI_NUMBER_FIELD(epg_save_frequently)),
	// src/neutrino.cpp:786 src/gui/miscsettings_menu.cpp:531
	boolRow("epg_save_mode")
		.section("misc")
		.label("miscsettings.epg_save_mode")
		.hint("menu.hint_epg_save_mode")
		.defaultValue(0)
		.field(COREAPI_NUMBER_FIELD(epg_save_mode)),
	// src/neutrino.cpp:782 src/gui/miscsettings_menu.cpp:484
	boolRow("epg_read")
		.section("misc")
		.label("miscsettings.epg_read")
		.hint("menu.hint_epg_read")
		.defaultValue(1)
		.field(COREAPI_NUMBER_FIELD(epg_read)),
	// src/neutrino.cpp:783 src/gui/miscsettings_menu.cpp:487
	boolRow("epg_read_frequently")
		.section("misc")
		.label("miscsettings.epg_read_frequently")
		.hint("menu.hint_epg_read_frequently")
		.defaultValue(1)
		.changeableWhen(kEpgReadOn)
		.field(COREAPI_NUMBER_FIELD(epg_read_frequently)),
	/* Offered while either of the two above is on, a disjunction across two
	   keys that a conjunction of comparisons cannot say, so it carries no
	   condition and is always shown. */
	// src/neutrino.cpp:777 src/gui/miscsettings_menu.cpp:493
	textRow("epg_dir")
		.section("misc")
		.label("miscsettings.epg_dir")
		.hint("menu.hint_epg_dir")
		.defaultValue(TARGET_ROOT "/media/sda1/epg")
		.text(kRuleDirectoryDurable)
		.field(COREAPI_TEXT_FIELD(epg_dir)),
	// src/neutrino.cpp:788 src/gui/miscsettings_menu.cpp:524
	enumRow("epg_scan")
		.section("misc")
		.label("miscsettings.epg_scan_bouquets")
		.hint("menu.hint_epg_scan")
		.defaultValue(2)
		.values(kEpgScan)
		.field(COREAPI_NUMBER_FIELD(epg_scan)),
	/* A number and not a choice: one of the four words the screen puts beside a
	   value stands for a number two headers state differently, so no scan can
	   read that table. The value is a pair of bits, live and standby, so every
	   number in the range is a mode. Bound: src/driver/scanepg.h:39. The two
	   standby modes are offered only where more than one tuner is enabled. */
	// src/neutrino.cpp:789 src/gui/miscsettings_menu.cpp:527
	intRow("epg_scan_mode")
		.section("misc")
		.label("miscsettings.epg_scan")
		.hint("menu.hint_epg_scan_mode")
		.range(0, 3)
		.defaultValue(2)
		.field(COREAPI_NUMBER_FIELD(epg_scan_mode)),

	// file browser
	// src/neutrino.cpp:707 src/gui/miscsettings_menu.cpp:561
	enumRow("filesystem_is_utf8")
		.section("misc")
		.label("filesystem.is.utf8")
		.hint("menu.hint_filesystem_is_utf8")
		.defaultValue(1)
		.values(kFilesystemUtf8)
		.field(COREAPI_NUMBER_FIELD(filesystem_is_utf8)),
	// src/neutrino.cpp:1261 src/gui/miscsettings_menu.cpp:565
	boolRow("filebrowser_showrights")
		.section("misc")
		.label("filebrowser.showrights")
		.hint("menu.hint_filebrowser_showrights")
		.defaultValue(1)
		.field(COREAPI_NUMBER_FIELD(filebrowser_showrights)),
	// src/neutrino.cpp:1260 src/gui/miscsettings_menu.cpp:568
	boolRow("filebrowser_denydirectoryleave")
		.section("misc")
		.label("filebrowser.denydirectoryleave")
		.hint("menu.hint_filebrowser_denydirectoryleave")
		.defaultValue(0)
		.field(COREAPI_NUMBER_FIELD(filebrowser_denydirectoryleave)),

	// channel list
	// src/neutrino.cpp:622 src/gui/miscsettings_menu.cpp:585
	boolRow("make_hd_list")
		.section("misc")
		.label("channellist.make_hdlist")
		.hint("menu.hint_make_hdlist")
		.defaultValue(0)
		.field(COREAPI_NUMBER_FIELD(make_hd_list)),
	// src/neutrino.cpp:626 src/gui/miscsettings_menu.cpp:589
	boolRow("make_webtv_list")
		.section("misc")
		.label("channellist.make_webtvlist")
		.hint("menu.hint_make_webtvlist")
		.defaultValue(1)
		.field(COREAPI_NUMBER_FIELD(make_webtv_list)),
	// src/neutrino.cpp:625 src/gui/miscsettings_menu.cpp:593
	boolRow("make_webradio_list")
		.section("misc")
		.label("channellist.make_webradiolist")
		.hint("menu.hint_make_webradiolist")
		.defaultValue(1)
		.field(COREAPI_NUMBER_FIELD(make_webradio_list)),
	// src/neutrino.cpp:623 src/gui/miscsettings_menu.cpp:597
	boolRow("make_new_list")
		.section("misc")
		.label("channellist.make_newlist")
		.hint("menu.hint_make_newlist")
		.defaultValue(1)
		.field(COREAPI_NUMBER_FIELD(make_new_list)),
	// src/neutrino.cpp:624 src/gui/miscsettings_menu.cpp:601
	boolRow("make_removed_list")
		.section("misc")
		.label("channellist.make_removedlist")
		.hint("menu.hint_make_removedlist")
		.defaultValue(1)
		.field(COREAPI_NUMBER_FIELD(make_removed_list)),
	// src/neutrino.cpp:628 src/gui/miscsettings_menu.cpp:605
	boolRow("keep_channel_numbers")
		.section("misc")
		.label("channellist.keep_numbers")
		.hint("menu.hint_keep_numbers")
		.defaultValue(0)
		.field(COREAPI_NUMBER_FIELD(keep_channel_numbers)),
	// src/neutrino.cpp:699 src/gui/miscsettings_menu.cpp:609
	boolRow("zap_cycle")
		.section("misc")
		.label("extra.zap_cycle")
		.hint("menu.hint_zap_cycle")
		.defaultValue(1)
		.field(COREAPI_NUMBER_FIELD(zap_cycle)),
	// src/neutrino.cpp:1300 src/gui/miscsettings_menu.cpp:613
	enumRow("channellist_new_zap_mode")
		.section("misc")
		.label("channellist.new_zap_mode")
		.hint("menu.hint_new_zap_mode")
		.defaultValue(0)
		.values(kNewZapMode)
		.field(COREAPI_NUMBER_FIELD(channellist_new_zap_mode)),
	// src/neutrino.cpp:1301 src/gui/miscsettings_menu.cpp:617
	boolRow("channellist_numeric_adjust")
		.section("misc")
		.label("channellist.numeric_adjust")
		.hint("menu.hint_numeric_adjust")
		.defaultValue(0)
		.field(COREAPI_NUMBER_FIELD(channellist_numeric_adjust)),
	// src/neutrino.cpp:629 src/gui/miscsettings_menu.cpp:621
	boolRow("show_empty_favorites")
		.section("misc")
		.label("channellist.show_empty_favs")
		.hint("menu.hint_channellist_show_empty_favs")
		.defaultValue(0)
		.field(COREAPI_NUMBER_FIELD(show_empty_favorites)),
	// src/neutrino.cpp:701 src/gui/miscsettings_menu.cpp:625
	enumRow("enable_sdt")
		.section("misc")
		.label("miscsettings.channellist_enablesdt")
		.hint("menu.hint_channellist_enablesdt")
		.defaultValue(1)
		.values(kEnableSdt)
		.field(COREAPI_NUMBER_FIELD(enable_sdt)),

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
	boolRow("tmdb_enabled")
		.section("misc")
		.label("tmdb.enabled")
		.hint("menu.hint_tmdb_enabled")
		.defaultValue(1)
		.field(COREAPI_NUMBER_FIELD(tmdb_enabled)),
	// src/neutrino.cpp:1177 src/gui/miscsettings_menu.cpp:661
	textRow("tmdb_api_key")
		.section("misc")
		.label("tmdb.api_key")
		.hint("menu.hint_tmdb_api_key")
		.defaultValue("XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX")
		.text(kRuleKey32)
		.secret()
		.field(COREAPI_TEXT_FIELD(tmdb_api_key)),
	// src/neutrino.cpp:1186 src/gui/miscsettings_menu.cpp:670
	boolRow("omdb_enabled")
		.section("misc")
		.label("omdb.enabled")
		.hint("menu.hint_omdb_enabled")
		.defaultValue(1)
		.field(COREAPI_NUMBER_FIELD(omdb_enabled)),
	// src/neutrino.cpp:1184 src/gui/miscsettings_menu.cpp:676
	textRow("omdb_api_key")
		.section("misc")
		.label("omdb.api_key")
		.hint("menu.hint_omdb_api_key")
		.defaultValue("XXXXXXXX")
		.text(kRuleKey8)
		.secret()
		.field(COREAPI_TEXT_FIELD(omdb_api_key)),
	// src/neutrino.cpp:1193 src/gui/miscsettings_menu.cpp:685
	boolRow("shoutcast_enabled")
		.section("misc")
		.label("shoutcast.enabled")
		.hint("menu.hint_shoutcast_enabled")
		.defaultValue(1)
		.field(COREAPI_NUMBER_FIELD(shoutcast_enabled)),
	// src/neutrino.cpp:1191 src/gui/miscsettings_menu.cpp:691
	textRow("shoutcast_dev_id")
		.section("misc")
		.label("shoutcast.dev_id")
		.hint("menu.hint_shoutcast_dev_id")
		.defaultValue("XXXXXXXXXXXXXXXX")
		.text(kRuleKey16)
		.secret()
		.field(COREAPI_TEXT_FIELD(shoutcast_dev_id)),
	// src/neutrino.cpp:1172 src/gui/miscsettings_menu.cpp:700
	boolRow("youtube_enabled")
		.section("misc")
		.label("youtube.enabled")
		.hint("menu.hint_youtube_enabled")
		.defaultValue(1)
		.field(COREAPI_NUMBER_FIELD(youtube_enabled)),
	// src/neutrino.cpp:1170 src/gui/miscsettings_menu.cpp:706
	textRow("youtube_api_key")
		.section("misc")
		.label("youtube.api_key")
		.hint("menu.hint_youtube_api_key")
		.defaultValue("XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX")
		.text(kRuleKey39)
		.secret()
		.field(COREAPI_TEXT_FIELD(youtube_api_key)),

	// plugins
	// src/neutrino.cpp:1025 src/gui/miscsettings_menu.cpp:722
	textRow("plugin_hdd_dir")
		.section("misc")
		.label("plugins.hdd_dir")
		.hint("menu.hint_plugins_hdd_dir")
		.defaultValue(TARGET_ROOT "/media/sda1/plugins")
		.text(kRuleDirectoryExists)
		.field(COREAPI_TEXT_FIELD(plugin_hdd_dir)),
	// The name of a plugin, and the default is what the screen offers as none.
	// src/neutrino.cpp:1033 src/gui/miscsettings_menu.cpp:726
	textRow("movieplayer_plugin")
		.section("misc")
		.label("mpkey.plugin")
		.hint("menu.hint_movieplayer_plugin")
		.defaultValue("---")
		.text(kRuleNameFromList)
		.field(COREAPI_TEXT_FIELD(movieplayer_plugin)),

	// streaming
	// Bound: the five digits the screen takes.
	// src/neutrino.cpp:870 src/gui/miscsettings_menu.cpp:750
	intRow("streaming_port")
		.section("misc")
		.label("streaming.port")
		.range(0, 99999)
		.defaultValue(31339)
		.field(COREAPI_NUMBER_FIELD(streaming_port)),
	// src/neutrino.cpp:868 src/gui/miscsettings_menu.cpp:755
	boolRow("streaming_ecmmode")
		.section("misc")
		.label("streaming.ecmmode")
		.defaultValue(0)
		.field(COREAPI_NUMBER_FIELD(streaming_ecmmode)),
	// src/neutrino.cpp:869 src/gui/miscsettings_menu.cpp:760
	boolRow("streaming_decryptmode")
		.section("misc")
		.label("streaming.decryptmode")
		.defaultValue(1)
		.field(COREAPI_NUMBER_FIELD(streaming_decryptmode)),

	/* The two below are numbers and not choices, because the screen names its
	   thirteen steps under literal words rather than locales. Bound: nought and
	   the last of the thirteen, src/gui/miscsettings_menu.cpp:218. Both are
	   offered only where the box can change its clock, and the loader forces a
	   value where it cannot, nought for the first and fifty for the second. */
	// src/neutrino.cpp:578 src/gui/miscsettings_menu.cpp:774
	intRow("cpufreq")
		.section("misc")
		.label("cpu.freq_normal")
		.range(0, 600)
		.defaultValue(0)
		.field(COREAPI_NUMBER_FIELD_ON(cpufreq, canCpufreq, NULL)),
	// src/neutrino.cpp:579 src/gui/miscsettings_menu.cpp:775
	intRow("standby_cpufreq")
		.section("misc")
		.label("cpu.freq_standby")
		.range(0, 600)
		.defaultValue(100)
		.field(COREAPI_NUMBER_FIELD_ON(standby_cpufreq, canCpufreq, NULL)),

	/* What follows has no item on any screen, or an item a screen offers
	   outside the settings menus. Where the program states no name for a
	   setting the row says so rather than borrowing the name of a neighbouring
	   item, which reads as right on the page and is wrong on the screen. */
	/* The small channel list, which the lists ask for while they draw.
	   src/neutrino.cpp:3553 */
	// src/neutrino.cpp:705
	boolRow("minimode")
		.section("misc")
		.defaultValue(0)
		.field(COREAPI_NUMBER_FIELD(minimode)),
	/* Three digits on the screen, src/gui/miscsettings_menu.cpp:428, the only
	   bound stated anywhere: the value is kept as text there and turned into a
	   number on the way out, :461. */
	// src/neutrino.cpp:710 src/gui/miscsettings_menu.cpp:429
	intRow("shutdown_count")
		.section("misc")
		.label("miscsettings.shutdown_count")
		.hint("menu.hint_shutdown_count")
		.range(0, 999)
		.defaultValue(0)
		.unit("unit.short.minute")
		.field(COREAPI_NUMBER_FIELD(shutdown_count)),
	/* The standing sleep timer in minutes. Its item is the sleep timer box and
	   holds the value as three digits of text, src/gui/sleeptimer.cpp:68. The
	   loader reads it only where the box can switch itself off. */
	// src/neutrino.cpp:713 src/gui/sleeptimer.cpp:68
	/* Local resolution: this fork falls back to 180 minutes of inactivity
	   before the box switches itself off, not the source's 0 (never). */
	intRow("shutdown_min")
		.section("misc")
		.label("sleeptimerbox.title2")
		.range(0, 999)
		.defaultValue(180)
		.unit("unit.short.minute")
		.field(COREAPI_NUMBER_FIELD(shutdown_min)),
	/* Which of the three the power menu did last, standby, off or restart,
	   src/gui/poweroff_menu.cpp:56, :62 and :68. The menu writes it and reads
	   it back to mark the entry, :119; no item names it. Its default is the
	   switch beside it, and the literal below is what that one falls back to. */
	// src/neutrino.cpp:727
	intRow("power_off_selected")
		.section("misc")
		.range(0, 2)
		.defaultValue(0)
		.field(COREAPI_NUMBER_FIELD(power_off_selected)),
	/* Whether the box follows a recorder onto its own input. Read once a
	   message arrives from one, src/neutrino.cpp:4468, and offered by no screen
	   at all; the web interface offers it instead. */
	// src/neutrino.cpp:738
	boolRow("vcr_AutoSwitch")
		.section("misc")
		.defaultValue(1)
		.field(COREAPI_NUMBER_FIELD(vcr_AutoSwitch)),
	/* Whether the up and down keys walk the audio tracks while the infobar
	   stands, src/daemonc/remotecontrol.cpp. No screen offers it. */
	// src/neutrino.cpp:744
	boolRow("audiochannel_up_down_enable")
		.section("misc")
		.defaultValue(0)
		.field(COREAPI_NUMBER_FIELD(audiochannel_up_down_enable)),
	/* The four below are the section daemon's cache sizes. Each has an item
	   that keeps the value as a fixed number of digits of text, which is the
	   only bound stated anywhere and is what the bounds here are. They reach
	   the daemon through the same call as the time settings,
	   src/neutrino.cpp:2779. */
	// src/neutrino.cpp:778 src/gui/miscsettings_menu.cpp:500
	intRow("epg_cache_time")
		.section("misc")
		.label("miscsettings.epg_cache")
		.hint("menu.hint_epg_cache")
		.range(0, 99)
		.defaultValue(7)
		.field(COREAPI_NUMBER_FIELD(epg_cache)),
	// src/neutrino.cpp:779 src/gui/miscsettings_menu.cpp:507
	intRow("epg_extendedcache_time")
		.section("misc")
		.label("miscsettings.epg_extendedcache")
		.hint("menu.hint_epg_extendedcache")
		.range(0, 999)
		.defaultValue(168)
		.unit("unit.short.hour")
		.field(COREAPI_NUMBER_FIELD(epg_extendedcache)),
	// src/neutrino.cpp:780 src/gui/miscsettings_menu.cpp:521
	intRow("epg_max_events")
		.section("misc")
		.label("miscsettings.epg_max_events")
		.hint("menu.hint_epg_max_events")
		.range(0, 999999)
		.defaultValue(30000)
		.field(COREAPI_NUMBER_FIELD(epg_max_events)),
	// src/neutrino.cpp:781 src/gui/miscsettings_menu.cpp:514
	intRow("epg_old_events")
		.section("misc")
		.label("miscsettings.epg_old_events")
		.hint("menu.hint_epg_old_events")
		.range(0, 999)
		.defaultValue(1)
		.unit("unit.short.hour")
		.field(COREAPI_NUMBER_FIELD(epg_old_events)),
	/* How many hours pass before the standby scan runs again,
	   src/driver/scanepg.cpp:325. Nothing states a bound for it, so the row
	   states the widest the field holds and a floor of nought. A bound no
	   screen governs and no check can compare. */
	// src/neutrino.cpp:796
	intRow("epg_scan_rescan")
		.section("misc")
		.range(0, 2147483647)
		.defaultValue(24)
		.field(COREAPI_NUMBER_FIELD(epg_scan_rescan)),
	// src/neutrino.cpp:799 src/gui/eventlist.cpp:1482
	intRow("epg_search_history_max")
		.section("misc")
		.label("eventfinder.max_history")
		.range(0, 50)
		.defaultValue(10)
		.field(COREAPI_NUMBER_FIELD(epg_search_history_max)),
	/* How many searches the box actually kept, which the search list writes
	   itself and holds to the count above, src/gui/eventlist.cpp:1243. Writing
	   it does not add a search; the ceiling is the one the list enforces. */
	// src/neutrino.cpp:800
	intRow("epg_search_history_size")
		.section("misc")
		.range(0, 50)
		.defaultValue(0)
		.field(COREAPI_NUMBER_FIELD(epg_search_history_size)),
	/* Which keyboard the on screen one comes up as, written by the keyboard
	   itself when it is changed, src/gui/widget/keyboard_input.cpp:345, and
	   read the next time it opens, :337. Empty means the box picks by
	   language. */
	// src/neutrino.cpp:773
	textRow("keyboard_layout")
		.section("misc")
		.defaultValue("")
		.field(COREAPI_TEXT_FIELD(keyboard_layout)),
	/* How the file browser sorts, which it writes itself. The loader holds it
	   to the number of sorts there are and turns anything else into nought,
	   src/neutrino.cpp:1263, and that count is the ceiling here. */
	// src/neutrino.cpp:1262
	intRow("filebrowser_sortmethod")
		.section("misc")
		.range(0, 4)
		.defaultValue(1)
		.field(COREAPI_NUMBER_FIELD(filebrowser_sortmethod)),
	/* Whether the event view opens in the larger window,
	   src/gui/epgview.cpp:752. Read as a flag and offered by no screen. */
	// src/neutrino.cpp:1287
	boolRow("bigFonts")
		.section("misc")
		.defaultValue(1)
		.field(COREAPI_NUMBER_FIELD(bigFonts)),
	/* The code that guards the personalisation menu. A credential, so a read
	   answers nothing and an empty write is refused; the declared default is
	   the program's own fallback and not a box's value. Nothing here asks for
	   the old code before taking a new one. */
	// src/neutrino.cpp:934 src/gui/personalize.cpp:445
	textRow("personalize_pincode")
		.section("misc")
		.label("personalize.pincode")
		.hint("personalize.pinhint")
		.defaultValue("0000")
		.text(kRulePin)
		.secret()
		.field(COREAPI_TEXT_FIELD(personalize_pincode)),
	/* The five below are lists of plugin file names joined with commas, which
	   the personalisation screen rebuilds whole whenever it is left,
	   src/gui/personalize.cpp:388. A name this layer cannot check is what a
	   frontend would be writing. */
	// src/neutrino.cpp:1026
	textRow("plugins_disabled")
		.section("misc")
		.defaultValue("")
		.text(kRulePathList)
		.field(COREAPI_TEXT_FIELD(plugins_disabled)),
	// src/neutrino.cpp:1027
	textRow("plugins_game")
		.section("misc")
		.defaultValue("")
		.text(kRulePathList)
		.field(COREAPI_TEXT_FIELD(plugins_game)),
	// src/neutrino.cpp:1028
	textRow("plugins_lua")
		.section("misc")
		.defaultValue("")
		.text(kRulePathList)
		.field(COREAPI_TEXT_FIELD(plugins_lua)),
	// src/neutrino.cpp:1029
	textRow("plugins_script")
		.section("misc")
		.defaultValue("")
		.text(kRulePathList)
		.field(COREAPI_TEXT_FIELD(plugins_script)),
	// src/neutrino.cpp:1030
	textRow("plugins_tool")
		.section("misc")
		.defaultValue("")
		.text(kRulePathList)
		.field(COREAPI_TEXT_FIELD(plugins_tool)),
	/* Where the box goes back to after an advert break, in seconds. The menu
	   offers nine fixed minutes and then a chooser over ten to a hundred and
	   twenty, src/gui/adzap.cpp:357 and :369, so the floor is the smallest of
	   the nine and the ceiling the chooser's own, both in seconds. That chooser
	   binds a local in minutes rather than the field, so its label names
	   minutes and this row is seconds; the row therefore states no label. */
	// src/neutrino.cpp:1204 src/gui/adzap.cpp:369
	intRow("adzap_zapBackPeriod")
		.section("misc")
		.range(60, 7200)
		.defaultValue(180)
		.unit("unit.short.second")
		.field(COREAPI_NUMBER_FIELD(adzap_zapBackPeriod)),
	// src/neutrino.cpp:1205 src/gui/adzap.cpp:346
	enumRow("adzap_zapOnActivation")
		.section("misc")
		.label("adzap.zap")
		.hint("menu.hint_adzap_zap")
		.defaultValue(0)
		.values(kAdzapZap)
		.field(COREAPI_NUMBER_FIELD(adzap_zapOnActivation)),
	// src/neutrino.cpp:1206 src/gui/adzap.cpp:342
	boolRow("adzap_writeData")
		.section("misc")
		.label("adzap.writedata")
		.hint("menu.hint_adzap_writedata")
		.defaultValue(0)
		.field(COREAPI_NUMBER_FIELD(adzap_writeData)),
	/* Where the settings backup is written. Two file browsers write it,
	   src/gui/keybind_setup.cpp:126 and src/gui/settings_manager.cpp:79, and
	   neither names it: the one locale that carries the words is a question
	   with the directory in it and not the name of a field. */
	// src/neutrino.cpp:1122
	textRow("backup_dir")
		.section("misc")
		.defaultValue(TARGET_ROOT "/media")
		.text(kRuleDirectoryExists)
		.field(COREAPI_TEXT_FIELD(backup_dir)),
};

} // anonymous namespace

const Descriptor *settingsTableMisc(size_t &count)
{
	count = sizeof(kMisc) / sizeof(kMisc[0]);
	return kMisc;
}

} // namespace coreapi
