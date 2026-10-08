/*
 * settingstable_channel.cpp - channel settings, one row per field
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

/* The channel section. Two screens, and the descriptor carries no sub-section,
   so that structure is not in the table: the start channel and the list mode
   come from src/gui/zapit_setup.cpp, the web channel rows from
   src/gui/webchannels_setup.cpp. The scan screen offers nothing here: it binds
   CScanSettings and Zapit_config, which are objects of their own with files of
   their own.

   The start channel is four rows and two settings. What the box zaps to is
   startchanneltv_id and startchannelradio_id, src/neutrino.cpp:3121, each sixty
   four bits wide and so carried as the text a channel is named by everywhere
   else in this layer. The two beside them hold the name a person reads.

   The screen writes the pair together, src/gui/zapit_setup.cpp:152, and a write
   of several settings keeps it together too, settings/couple.cpp: an identifier
   written alone brings the name of its channel and a name written alone is
   refused. The name is read through channels::get, which takes the channel
   stack's lock for one lookup the way GET /api/v1/channels/{id} does from the
   same thread. */

/* The list the screen offers, which is not the whole of the enum: LIST_MODE_WEB
   is 2 and is not among them. Nought below is the value the program falls back
   to, which the enum spells LIST_MODE_FAV. src/gui/channellist.h:51
   src/gui/zapit_setup.cpp:85 */
constexpr EnumValue kChannelListMode[] =
{
	option(-1).label("channellist.remember"),
	option(0).label("channellist.favs"),
	option(1).label("channellist.provs"),
	option(3).label("channellist.sats"),
	option(4).label("channellist.head")
};

/* The four start channel rows are offered only while the box is not told to
   come up on the channel it was left on, which is what greys the two items out.
   src/gui/zapit_setup.cpp:78 */
constexpr Condition kNoLastChannel[] =
{
	when("uselastchannel").is(0)
};

/* The loader writes the initial mode over the mode the box was left in
   wherever one is chosen, so the pair below matters only while none is.
   src/neutrino.cpp:521 */
constexpr Condition kNoInitialMode[] =
{
	when("channel_mode_initial").below(0)
};

constexpr Descriptor kChannel[] =
{
	/* The three rows below reach the box only at start: the first is handed to
	   zapit once, src/neutrino.cpp:3072, and the two list modes are read by the
	   pass that loads them and nowhere else, src/neutrino.cpp:521. */
	// src/neutrino.cpp:1201 src/gui/zapit_setup.cpp:72
	boolRow("uselastchannel")
		.section("channel")
		.label("zapitsetup.last_use")
		.hint("menu.hint_last_use")
		.defaultValue(1)
		.needsRestart()
		.field(COREAPI_NUMBER_FIELD(uselastchannel)),
	// src/neutrino.cpp:519 src/gui/zapit_setup.cpp:94
	enumRow("channel_mode_initial")
		.section("channel")
		.label("zapitsetup.channelmode")
		.hint("menu.hint_channellist_mode")
		.defaultValue(0)
		.values(kChannelListMode)
		.needsRestart()
		.field(COREAPI_NUMBER_FIELD(channel_mode_initial)),
	// src/neutrino.cpp:520 src/gui/zapit_setup.cpp:98
	enumRow("channel_mode_initial_radio")
		.section("channel")
		.label("zapitsetup.channelmode_radio")
		.hint("menu.hint_channellist_mode_radio")
		.defaultValue(0)
		.values(kChannelListMode)
		.needsRestart()
		.field(COREAPI_NUMBER_FIELD(channel_mode_initial_radio)),

	/* The two identifiers, which is what a start channel really is. The program
	   has no name for either: what the screen names is the pair, and the name
	   beside each carries that. Handed to zapit once at start, so a written one
	   takes effect at the next. */
	// src/neutrino.cpp:1213 src/neutrino.cpp:3121
	textRow("startchanneltv_id")
		.section("channel")
		.defaultValue("0")
		.text(kRuleChannelId)
		.needsRestart()
		.changeableWhen(kNoLastChannel)
		.field(COREAPI_CHANNEL_ID_FIELD(startchanneltv_id)),
	// src/neutrino.cpp:1214 src/neutrino.cpp:3122
	textRow("startchannelradio_id")
		.section("channel")
		.defaultValue("0")
		.text(kRuleChannelId)
		.needsRestart()
		.changeableWhen(kNoLastChannel)
		.field(COREAPI_CHANNEL_ID_FIELD(startchannelradio_id)),
	/* The name beside each, which is what the screen shows on the item and what
	   a person reads. Not what the box zaps to. */
	// src/neutrino.cpp:1211 src/gui/zapit_setup.cpp:78
	textRow("startchanneltv")
		.section("channel")
		.label("zapitsetup.last_tv")
		.hint("menu.hint_last_tv")
		.defaultValue("")
		.needsRestart()
		.changeableWhen(kNoLastChannel)
		.field(COREAPI_TEXT_FIELD(StartChannelTV)),
	// src/neutrino.cpp:1212 src/gui/zapit_setup.cpp:81
	textRow("startchannelradio")
		.section("channel")
		.label("zapitsetup.last_radio")
		.hint("menu.hint_last_radio")
		.defaultValue("")
		.needsRestart()
		.changeableWhen(kNoLastChannel)
		.field(COREAPI_TEXT_FIELD(StartChannelRadio)),

	/* Web channels. The screen offers the three below only in its television
	   arm, src/gui/webchannels_setup.cpp:244, which is which menu was opened
	   and not a setting, so none of them carries a condition.

	   A number of pixels across, and a number rather than a choice: the screen
	   names its steps with literal sizes rather than locales, so every value
	   between the offered ones is one a frontend can write and the box has no
	   step for. Bound: the first and the last of the table, which the same
	   condition shortens as the screen does.
	   src/gui/webchannels_setup.cpp:52 */
	// src/neutrino.cpp:1021 src/gui/webchannels_setup.cpp:538
	intRow("livestreamResolution")
		.section("channel")
		.label("livestream.resolution")
#if HAVE_CST_HARDWARE
		.range(480, 1920)
#else
		.range(480, 3840)
#endif
		.defaultValue(1920)
		.field(COREAPI_NUMBER_FIELD(livestreamResolution)),
	// src/neutrino.cpp:942 src/gui/webchannels_setup.cpp:250
	intRow("webtv_stream_restart_attempts")
		.section("channel")
		.label("webtv.stream_restart_attempts")
		.range(0, 3)
		.defaultValue(1)
		.field(COREAPI_NUMBER_FIELD(webtv_stream_restart_attempts)),
	// src/neutrino.cpp:947 src/gui/webchannels_setup.cpp:252
	boolRow("webtv_dns_diagnostics")
		.section("channel")
		.label("webtv.dns.diagnostics")
		.defaultValue(1)
		.field(COREAPI_NUMBER_FIELD(webtv_dns_diagnostics)),
	/* The two below carry no hint: the screen builds one at run time out of the
	   directories it reads, which is text and not the name of one.
	   src/gui/webchannels_setup.cpp:259 */
	// src/neutrino.cpp:941 src/gui/webchannels_setup.cpp:268
	boolRow("webtv_xml_auto")
		.section("channel")
		.label("webtv.xml.auto")
		.defaultValue(1)
		.field(COREAPI_NUMBER_FIELD(webtv_xml_auto)),
	// src/neutrino.cpp:973 src/gui/webchannels_setup.cpp:263
	boolRow("webradio_xml_auto")
		.section("channel")
		.label("webradio.xml.auto")
		.defaultValue(1)
		.field(COREAPI_NUMBER_FIELD(webradio_xml_auto)),
	/* The item for this one is inside an arm the build does not take, and the
	   value is live anyway: the movie player still reads it,
	   src/gui/movieplayer.cpp:2252. Its label and its position come from that
	   dead item, which is the only statement of them there is.
	   src/gui/webchannels_setup.cpp:238 */
	// src/neutrino.cpp:1022 src/gui/webchannels_setup.cpp:238
	textRow("livestreamScriptPath")
		.section("channel")
		.label("livestream.scriptpath")
		.defaultValue(WEBTVDIR)
		.text(kRuleDirectory)
		.field(COREAPI_TEXT_FIELD(livestreamScriptPath)),

	/* The two below are the list the box was left in, which it writes whenever
	   the list is changed, src/neutrino.cpp:2541 and :2545, and reads once at
	   the next start, :5335 and :5631. The loader writes the initial mode over
	   both where one is chosen, :521, which is the condition each carries. No
	   item names either, and the ceiling is the last of the five list modes,
	   src/gui/channellist.h:51. */
	// src/neutrino.cpp:517
	intRow("channel_mode")
		.section("channel")
		.range(0, 4)
		.defaultValue(0)
		.needsRestart()
		.changeableWhen(kNoInitialMode)
		.field(COREAPI_NUMBER_FIELD(channel_mode)),
	// src/neutrino.cpp:518
	intRow("channel_mode_radio")
		.section("channel")
		.range(0, 4)
		.defaultValue(0)
		.needsRestart()
		.changeableWhen(kNoInitialMode)
		.field(COREAPI_NUMBER_FIELD(channel_mode_radio)),
	/* How the channel list is sorted, which the list itself steps through and
	   wraps at the last sort there is, src/gui/channellist.cpp:939. That count
	   is the ceiling here, src/gui/channellist.h:265. No item names it. */
	// src/neutrino.cpp:1307
	intRow("channellist_sort_mode")
		.section("channel")
		.range(0, 3)
		.defaultValue(0)
		.field(COREAPI_NUMBER_FIELD(channellist_sort_mode)),
	/* The two below are where the file browser last stood when a web channel
	   list was picked, written by the browser itself,
	   src/gui/webchannels_setup.cpp:139 and :161, and read the next time it
	   opens, :154. No item names either. */
	// src/neutrino.cpp:967
	textRow("last_webtv_dir")
		.section("channel")
		.defaultValue(WEBTVDIR_VAR)
		.text(kRuleDirectory)
		.field(COREAPI_TEXT_FIELD(last_webtv_dir)),
	// src/neutrino.cpp:999
	textRow("last_webradio_dir")
		.section("channel")
		.defaultValue(WEBRADIODIR_VAR)
		.text(kRuleDirectory)
		.field(COREAPI_TEXT_FIELD(last_webradio_dir)),
};

} // anonymous namespace

const Descriptor *settingsTableChannel(size_t &count)
{
	count = sizeof(kChannel) / sizeof(kChannel[0]);
	return kChannel;
}

} // namespace coreapi
