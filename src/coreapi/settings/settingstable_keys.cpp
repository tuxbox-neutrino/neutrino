/*
 * settingstable_keys.cpp - key binding settings, one row per field
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

#include "coreapi/base/deps.h"

// The remote control codes a keybinding stores, as the program names them, so
// that no default here is a number copied by hand.
#include <driver/rcinput.h>

namespace coreapi
{

namespace
{

/* The keybinding section: the key, the type and the default off the line that
   loads the setting, the label and the hint off the table the screen builds its
   items from.

   Almost no default here is comparable against the program, which writes them
   as names of the remote control code. They are not copied as numbers either:
   the header that declares them is included above and each row names the same
   constant the loader names.

   Six defaults are decided by the box model and the row states the arm every
   other box takes. src/neutrino.cpp:6213 gives key_favorites RC_video on the
   HD51, :6247 and :6249 give key_timeshift RC_nokey or RC_playpause, :6255
   gives key_tvradio_mode RC_tv, :6280 and :6283 give mpkey_pause and mpkey_play
   RC_playpause. */

// What a keybinding may hold. The chooser takes anything the box can send,
// src/gui/widget/keychooser.cpp:91, and the remover writes the code that means
// no key at all, :106. A long carries the second as the settings file does,
// which is what a signed read of it gives.
const long kKeyNone = (int32_t) CRCInput::RC_nokey;
const long kKeyMax = (long) CRCInput::RC_MaxRC;

// src/gui/keybind_setup.cpp:149
constexpr EnumValue kBouquetlistMode[] =
{
	option(0).label("keybindingmenu.channellist"),
	option(1).label("keybindingmenu.favorites")
};

// src/gui/keybind_setup.cpp:157
constexpr EnumValue kRemoteHardware[] =
{
	option(0).label("keybindingmenu.remotecontrol_hardware_coolstream"),
	option(1).label("keybindingmenu.remotecontrol_hardware_dbox"),
	option(2).label("keybindingmenu.remotecontrol_hardware_philips")
};

// src/gui/keybind_setup.cpp:165
constexpr EnumValue kLeftRightKeyTv[] =
{
	option(0).label("keybindingmenu.mode_left_right_key_tv_zap"),
	option(1).label("keybindingmenu.mode_left_right_key_tv_vzap"),
	option(2).label("keybindingmenu.mode_left_right_key_tv_volume"),
	option(3).label("keybindingmenu.mode_left_right_key_tv_infobar")
};

long keyFavoritesDefault()
{
	return boxdefault::kFavoritesIsVideo ? (int32_t) CRCInput::RC_video : (int32_t) CRCInput::RC_favorites;
}

/* The movie browser's cover key falls back to the favourites key as it is set,
   which the loader reads first, and only without one to that key's own default. */
long mbKeyCoverDefault()
{
	long favorites = 0;
	if (settingsSource().readInt("key_favorites", favorites) == Status::Ok)
		return favorites;
	return keyFavoritesDefault();
}

long keyTimeshiftDefault()
{
	if (boxdefault::kTimeshiftIsNone)
		return (int32_t) CRCInput::RC_nokey;
	if (boxdefault::kVuPlus)
		return (int32_t) CRCInput::RC_playpause;
	return (int32_t) CRCInput::RC_pause;
}

long keyTvRadioModeDefault()
{
	return boxdefault::kTvRadioKeyIsTv ? (int32_t) CRCInput::RC_tv : (int32_t) CRCInput::RC_nokey;
}

long mpKeyPauseDefault()
{
	if (boxdefault::kPlayPauseKey || boxdefault::kVuPlus)
		return (int32_t) CRCInput::RC_playpause;
	return (int32_t) CRCInput::RC_pause;
}

long mpKeyPlayDefault()
{
	return boxdefault::kPlayPauseKey ? (int32_t) CRCInput::RC_playpause : (int32_t) CRCInput::RC_play;
}

constexpr Descriptor kSettings[] =
{
	// src/neutrino.cpp:6257 src/gui/keybind_setup.cpp:183
	keyRow("key_tvradio_mode")
		.section("keybindings")
		.label("keybindingmenu.tvradiomode")
		.hint("menu.hint_key_tvradiomode")
		.range(kKeyNone, kKeyMax)
		.defaultValue((int32_t) CRCInput::RC_nokey)
		.defaultFrom(keyTvRadioModeDefault)
		.field(COREAPI_NUMBER_FIELD(key_tvradio_mode)),
	// src/neutrino.cpp:6237 src/gui/keybind_setup.cpp:184
	keyRow("key_power_off")
		.section("keybindings")
		.label("keybindingmenu.poweroff")
		.hint("menu.hint_key_poweroff")
		.range(kKeyNone, kKeyMax)
		.defaultValue((int32_t) CRCInput::RC_standby)
		.field(COREAPI_NUMBER_FIELD(key_power_off)),
	// src/neutrino.cpp:6243 src/gui/keybind_setup.cpp:185
	keyRow("key_standby_off_add")
		.section("keybindings")
		.label("keybindingmenu.standbyoff_add")
		.hint("menu.hint_key_standbyoff_add")
		.range(kKeyNone, kKeyMax)
		.defaultValue((int32_t) CRCInput::RC_ok)
		.field(COREAPI_NUMBER_FIELD(key_standby_off_add)),
	// src/neutrino.cpp:6216 src/gui/keybind_setup.cpp:186
	keyRow("key_favorites")
		.section("keybindings")
		.label("keybindingmenu.favorites")
		.hint("menu.hint_key_favorites")
		.range(kKeyNone, kKeyMax)
		.defaultValue((int32_t) CRCInput::RC_favorites)
		.defaultFrom(keyFavoritesDefault)
		.field(COREAPI_NUMBER_FIELD(key_favorites)),
	// src/neutrino.cpp:6226 src/gui/keybind_setup.cpp:187
	keyRow("key_channelList_pageup")
		.section("keybindings")
		.label("keybindingmenu.pageup")
		.hint("menu.hint_key_pageup")
		.range(kKeyNone, kKeyMax)
		.defaultValue((int32_t) CRCInput::RC_page_up)
		.field(COREAPI_NUMBER_FIELD(key_pageup)),
	// src/neutrino.cpp:6225 src/gui/keybind_setup.cpp:188
	keyRow("key_channelList_pagedown")
		.section("keybindings")
		.label("keybindingmenu.pagedown")
		.hint("menu.hint_key_pagedown")
		.range(kKeyNone, kKeyMax)
		.defaultValue((int32_t) CRCInput::RC_page_down)
		.field(COREAPI_NUMBER_FIELD(key_pagedown)),
	// src/neutrino.cpp:6261 src/gui/keybind_setup.cpp:189
	keyRow("key_volumeup")
		.section("keybindings")
		.label("keybindingmenu.volumeup")
		.hint("menu.hint_key_volumeup")
		.range(kKeyNone, kKeyMax)
		.defaultValue((int32_t) CRCInput::RC_plus)
		.field(COREAPI_NUMBER_FIELD(key_volumeup)),
	// src/neutrino.cpp:6260 src/gui/keybind_setup.cpp:190
	keyRow("key_volumedown")
		.section("keybindings")
		.label("keybindingmenu.volumedown")
		.hint("menu.hint_key_volumedown")
		.range(kKeyNone, kKeyMax)
		.defaultValue((int32_t) CRCInput::RC_minus)
		.field(COREAPI_NUMBER_FIELD(key_volumedown)),
	// src/neutrino.cpp:6223 src/gui/keybind_setup.cpp:191
	keyRow("key_list_start")
		.section("keybindings")
		.label("extra.key_list_start")
		.hint("menu.hint_key_list_start")
		.range(kKeyNone, kKeyMax)
		.defaultValue((int32_t) CRCInput::RC_nokey)
		.field(COREAPI_NUMBER_FIELD(key_list_start)),
	// src/neutrino.cpp:6222 src/gui/keybind_setup.cpp:192
	keyRow("key_list_end")
		.section("keybindings")
		.label("extra.key_list_end")
		.hint("menu.hint_key_list_end")
		.range(kKeyNone, kKeyMax)
		.defaultValue((int32_t) CRCInput::RC_nokey)
		.field(COREAPI_NUMBER_FIELD(key_list_end)),
	// src/neutrino.cpp:6210 src/gui/keybind_setup.cpp:193
	keyRow("key_channelList_cancel")
		.section("keybindings")
		.label("keybindingmenu.cancel")
		.hint("menu.hint_key_cancel")
		.range(kKeyNone, kKeyMax)
		.defaultValue((int32_t) CRCInput::RC_home)
		.field(COREAPI_NUMBER_FIELD(key_channelList_cancel)),
	// src/neutrino.cpp:6211 src/gui/keybind_setup.cpp:194
	keyRow("key_channelList_sort")
		.section("keybindings")
		.label("keybindingmenu.sort")
		.hint("menu.hint_key_sort")
		.range(kKeyNone, kKeyMax)
		.defaultValue((int32_t) CRCInput::RC_blue)
		.field(COREAPI_NUMBER_FIELD(key_channelList_sort)),
	// src/neutrino.cpp:6208 src/gui/keybind_setup.cpp:195
	keyRow("key_channelList_addrecord")
		.section("keybindings")
		.label("keybindingmenu.addrecord")
		.hint("menu.hint_key_addrecord")
		.range(kKeyNone, kKeyMax)
		.defaultValue((int32_t) CRCInput::RC_red)
		.field(COREAPI_NUMBER_FIELD(key_channelList_addrecord)),
	// src/neutrino.cpp:6209 src/gui/keybind_setup.cpp:196
	keyRow("key_channelList_addremind")
		.section("keybindings")
		.label("keybindingmenu.addremind")
		.hint("menu.hint_key_addremind")
		.range(kKeyNone, kKeyMax)
		.defaultValue((int32_t) CRCInput::RC_yellow)
		.field(COREAPI_NUMBER_FIELD(key_channelList_addremind)),
	// src/neutrino.cpp:6207 src/gui/keybind_setup.cpp:197
	keyRow("key_bouquet_up")
		.section("keybindings")
		.label("keybindingmenu.bouquetup")
		.hint("menu.hint_key_bouquetup")
		.range(kKeyNone, kKeyMax)
		.defaultValue((int32_t) CRCInput::RC_right)
		.field(COREAPI_NUMBER_FIELD(key_bouquet_up)),
	// src/neutrino.cpp:6206 src/gui/keybind_setup.cpp:198
	keyRow("key_bouquet_down")
		.section("keybindings")
		.label("keybindingmenu.bouquetdown")
		.hint("menu.hint_key_bouquetdown")
		.range(kKeyNone, kKeyMax)
		.defaultValue((int32_t) CRCInput::RC_left)
		.field(COREAPI_NUMBER_FIELD(key_bouquet_down)),
	// src/neutrino.cpp:6212 src/gui/keybind_setup.cpp:199
	keyRow("key_current_transponder")
		.section("keybindings")
		.label("extra.key_current_transponder")
		.hint("menu.hint_key_transponder")
		.range(kKeyNone, kKeyMax)
		.defaultValue((int32_t) CRCInput::RC_nokey)
		.field(COREAPI_NUMBER_FIELD(key_current_transponder)),
	// src/neutrino.cpp:6239 src/gui/keybind_setup.cpp:200
	keyRow("key_quickzap_up")
		.section("keybindings")
		.label("keybindingmenu.channelup")
		.hint("menu.hint_key_channelup")
		.range(kKeyNone, kKeyMax)
		.defaultValue((int32_t) CRCInput::RC_up)
		.field(COREAPI_NUMBER_FIELD(key_quickzap_up)),
	// src/neutrino.cpp:6238 src/gui/keybind_setup.cpp:201
	keyRow("key_quickzap_down")
		.section("keybindings")
		.label("keybindingmenu.channeldown")
		.hint("menu.hint_key_channeldown")
		.range(kKeyNone, kKeyMax)
		.defaultValue((int32_t) CRCInput::RC_down)
		.field(COREAPI_NUMBER_FIELD(key_quickzap_down)),
	// src/neutrino.cpp:6245 src/gui/keybind_setup.cpp:202
	keyRow("key_subchannel_up")
		.section("keybindings")
		.label("keybindingmenu.subchannelup")
		.hint("menu.hint_key_subchannelup")
		.range(kKeyNone, kKeyMax)
		.defaultValue((int32_t) CRCInput::RC_right)
		.field(COREAPI_NUMBER_FIELD(key_subchannel_up)),
	// src/neutrino.cpp:6244 src/gui/keybind_setup.cpp:203
	keyRow("key_subchannel_down")
		.section("keybindings")
		.label("keybindingmenu.subchanneldown")
		.hint("menu.hint_key_subchanneldown")
		.range(kKeyNone, kKeyMax)
		.defaultValue((int32_t) CRCInput::RC_left)
		.field(COREAPI_NUMBER_FIELD(key_subchannel_down)),
	// src/neutrino.cpp:6262 src/gui/keybind_setup.cpp:204
	keyRow("key_zaphistory")
		.section("keybindings")
		.label("keybindingmenu.zaphistory")
		.hint("menu.hint_key_history")
		.range(kKeyNone, kKeyMax)
		.defaultValue((int32_t) CRCInput::RC_home)
		.field(COREAPI_NUMBER_FIELD(key_zaphistory)),
	// src/neutrino.cpp:6221 src/gui/keybind_setup.cpp:205
	keyRow("key_lastchannel")
		.section("keybindings")
		.label("keybindingmenu.lastchannel")
		.hint("menu.hint_key_lastchannel")
		.range(kKeyNone, kKeyMax)
		.defaultValue((int32_t) CRCInput::RC_0)
		.field(COREAPI_NUMBER_FIELD(key_lastchannel)),
	// src/neutrino.cpp:6285 src/gui/keybind_setup.cpp:206
	keyRow("mpkey.play")
		.section("keybindings")
		.label("mpkey.play")
		.hint("menu.hint_key_mpplay")
		.range(kKeyNone, kKeyMax)
		.defaultValue((int32_t) CRCInput::RC_play)
		.defaultFrom(mpKeyPlayDefault)
		.field(COREAPI_NUMBER_FIELD(mpkey_play)),
	// src/neutrino.cpp:6284 src/gui/keybind_setup.cpp:207
	keyRow("mpkey.pause")
		.section("keybindings")
		.label("mpkey.pause")
		.hint("menu.hint_key_mppause")
		.range(kKeyNone, kKeyMax)
		.defaultValue((int32_t) CRCInput::RC_pause)
		.defaultFrom(mpKeyPauseDefault)
		.field(COREAPI_NUMBER_FIELD(mpkey_pause)),
	// src/neutrino.cpp:6289 src/gui/keybind_setup.cpp:208
	keyRow("mpkey.stop")
		.section("keybindings")
		.label("mpkey.stop")
		.hint("menu.hint_key_mpstop")
		.range(kKeyNone, kKeyMax)
		.defaultValue((int32_t) CRCInput::RC_stop)
		.field(COREAPI_NUMBER_FIELD(mpkey_stop)),
	// src/neutrino.cpp:6274 src/gui/keybind_setup.cpp:209
	keyRow("mpkey.forward")
		.section("keybindings")
		.label("mpkey.forward")
		.hint("menu.hint_key_mpforward")
		.range(kKeyNone, kKeyMax)
		.defaultValue((int32_t) CRCInput::RC_forward)
		.field(COREAPI_NUMBER_FIELD(mpkey_forward)),
	// src/neutrino.cpp:6288 src/gui/keybind_setup.cpp:210
	keyRow("mpkey.rewind")
		.section("keybindings")
		.label("mpkey.rewind")
		.hint("menu.hint_key_mprewind")
		.range(kKeyNone, kKeyMax)
		.defaultValue((int32_t) CRCInput::RC_rewind)
		.field(COREAPI_NUMBER_FIELD(mpkey_rewind)),
	// src/neutrino.cpp:6272 src/gui/keybind_setup.cpp:211
	keyRow("mpkey.audio")
		.section("keybindings")
		.label("mpkey.audio")
		.hint("menu.hint_key_mpaudio")
		.range(kKeyNone, kKeyMax)
		.defaultValue((int32_t) CRCInput::RC_green)
		.field(COREAPI_NUMBER_FIELD(mpkey_audio)),
	// src/neutrino.cpp:6290 src/gui/keybind_setup.cpp:212
	keyRow("mpkey.subtitle")
		.section("keybindings")
		.label("mpkey.subtitle")
		.hint("menu.hint_key_mpsubtitle")
		.range(kKeyNone, kKeyMax)
		.defaultValue((int32_t) CRCInput::RC_sub)
		.field(COREAPI_NUMBER_FIELD(mpkey_subtitle)),
	// src/neutrino.cpp:6291 src/gui/keybind_setup.cpp:213
	keyRow("mpkey.time")
		.section("keybindings")
		.label("mpkey.time")
		.hint("menu.hint_key_mptime")
		.range(kKeyNone, kKeyMax)
		.defaultValue((int32_t) CRCInput::RC_timeshift)
		.field(COREAPI_NUMBER_FIELD(mpkey_time)),
	// src/neutrino.cpp:6273 src/gui/keybind_setup.cpp:214
	keyRow("mpkey.bookmark")
		.section("keybindings")
		.label("mpkey.bookmark")
		.hint("menu.hint_key_mpbookmark")
		.range(kKeyNone, kKeyMax)
		.defaultValue((int32_t) CRCInput::RC_yellow)
		.field(COREAPI_NUMBER_FIELD(mpkey_bookmark)),
	// src/neutrino.cpp:6275 src/gui/keybind_setup.cpp:215
	keyRow("mpkey.goto")
		.section("keybindings")
		.label("mpkey.goto")
		.range(kKeyNone, kKeyMax)
		.defaultValue((int32_t) CRCInput::RC_nokey)
		.field(COREAPI_NUMBER_FIELD(mpkey_goto)),
	// src/neutrino.cpp:6276 src/gui/keybind_setup.cpp:216
	keyRow("mpkey.next_repeat_mode")
		.section("keybindings")
		.label("mpkey.next_repeat_mode")
		.range(kKeyNone, kKeyMax)
		.defaultValue((int32_t) CRCInput::RC_nokey)
		.field(COREAPI_NUMBER_FIELD(mpkey_next_repeat_mode)),
	// src/neutrino.cpp:6287 src/gui/keybind_setup.cpp:217
	keyRow("mpkey.plugin")
		.section("keybindings")
		.label("mpkey.plugin")
		.hint("menu.hint_key_mpplugin")
		.range(kKeyNone, kKeyMax)
		.defaultValue((int32_t) CRCInput::RC_nokey)
		.field(COREAPI_NUMBER_FIELD(mpkey_plugin)),
	// src/neutrino.cpp:6252 src/gui/keybind_setup.cpp:218
	keyRow("key_timeshift")
		.section("keybindings")
		.label("extra.key_timeshift")
		.hint("menu.hint_key_timeshift")
		.range(kKeyNone, kKeyMax)
		.defaultValue((int32_t) CRCInput::RC_pause)
		.defaultFrom(keyTimeshiftDefault)
		.field(COREAPI_NUMBER_FIELD(key_timeshift)),
	// src/neutrino.cpp:6259 src/gui/keybind_setup.cpp:219
	keyRow("key_unlock")
		.section("keybindings")
		.label("extra.key_unlock")
		.hint("menu.hint_key_unlock")
		.range(kKeyNone, kKeyMax)
		.defaultValue((int32_t) CRCInput::RC_setup)
		.field(COREAPI_NUMBER_FIELD(key_unlock)),
	// src/neutrino.cpp:6220 src/gui/keybind_setup.cpp:220
	keyRow("key_help")
		.section("keybindings")
		.label("extra.key_help")
		.range(kKeyNone, kKeyMax)
		.defaultValue((int32_t) CRCInput::RC_help)
		.field(COREAPI_NUMBER_FIELD(key_help)),
	// src/neutrino.cpp:6224 src/gui/keybind_setup.cpp:221
	keyRow("key_next43mode")
		.section("keybindings")
		.label("extra.key_next43mode")
		.range(kKeyNone, kKeyMax)
		.defaultValue((int32_t) CRCInput::RC_nokey)
		.field(COREAPI_NUMBER_FIELD(key_next43mode)),
	// src/neutrino.cpp:6246 src/gui/keybind_setup.cpp:222
	keyRow("key_switchformat")
		.section("keybindings")
		.label("extra.key_switchformat")
		.range(kKeyNone, kKeyMax)
		.defaultValue((int32_t) CRCInput::RC_nokey)
		.field(COREAPI_NUMBER_FIELD(key_switchformat)),
	// src/neutrino.cpp:6241 src/gui/keybind_setup.cpp:223
	keyRow("key_screenshot")
		.section("keybindings")
		.label("extra.key_screenshot")
		.hint("menu.hint_key_screenshot")
		.range(kKeyNone, kKeyMax)
		.defaultValue((int32_t) CRCInput::RC_games)
		.field(COREAPI_NUMBER_FIELD(key_screenshot)),
	// src/neutrino.cpp:6242 src/gui/keybind_setup.cpp:224
	keyRow("key_sleep")
		.section("keybindings")
		.label("extra.key_sleep")
		.hint("menu.hint_key_sleep")
		.range(kKeyNone, kKeyMax)
		.defaultValue((int32_t) CRCInput::RC_sleep)
		.field(COREAPI_NUMBER_FIELD(key_sleep)),
#if ENABLE_PIP
	// src/neutrino.cpp:6230 src/gui/keybind_setup.cpp:226
	keyRow("key_pip_close")
		.section("keybindings")
		.label("extra.key_pip_close")
		.hint("menu.hint_key_pip_close")
		.range(kKeyNone, kKeyMax)
		.defaultValue((int32_t) CRCInput::RC_prev)
		.field(COREAPI_NUMBER_FIELD(key_pip_close)),
	// src/neutrino.cpp:6231 src/gui/keybind_setup.cpp:227
	keyRow("key_pip_close_avinput")
		.section("keybindings")
		.label("extra.key_pip_close_avinput")
		.range(kKeyNone, kKeyMax)
		.defaultValue((int32_t) CRCInput::RC_nokey)
		.field(COREAPI_NUMBER_FIELD(key_pip_close_avinput)),
	// src/neutrino.cpp:6232 src/gui/keybind_setup.cpp:228
	keyRow("key_pip_rotate_cw")
		.section("keybindings")
		.label("extra.key_pip_rotate_cw")
		.hint("menu.hint_key_pip_rotate_cw")
		.range(kKeyNone, kKeyMax)
		.defaultValue((int32_t) CRCInput::RC_nokey)
		.field(COREAPI_NUMBER_FIELD(key_pip_rotate_cw)),
	// src/neutrino.cpp:6233 src/gui/keybind_setup.cpp:229
	keyRow("key_pip_rotate_ccw")
		.section("keybindings")
		.label("extra.key_pip_rotate_ccw")
		.hint("menu.hint_key_pip_rotate_ccw")
		.range(kKeyNone, kKeyMax)
		.defaultValue((int32_t) CRCInput::RC_nokey)
		.field(COREAPI_NUMBER_FIELD(key_pip_rotate_ccw)),
	// src/neutrino.cpp:6234 src/gui/keybind_setup.cpp:230
	keyRow("key_pip_setup")
		.section("keybindings")
		.label("extra.key_pip_setup")
		.hint("menu.hint_key_pip_setup")
		.range(kKeyNone, kKeyMax)
		.defaultValue((int32_t) CRCInput::RC_nokey)
		.field(COREAPI_NUMBER_FIELD(key_pip_setup)),
	// src/neutrino.cpp:6235 src/gui/keybind_setup.cpp:231
	keyRow("key_pip_swap")
		.section("keybindings")
		.label("extra.key_pip_swap")
		.hint("menu.hint_key_pip_close")
		.range(kKeyNone, kKeyMax)
		.defaultValue((int32_t) CRCInput::RC_next)
		.field(COREAPI_NUMBER_FIELD(key_pip_swap)),
#endif
	// src/neutrino.cpp:6219 src/gui/keybind_setup.cpp:233
	boolRow("key_format_mode_active")
		.section("keybindings")
		.label("extra.key_format_mode")
		.hint("menu.hint_key_format_mode_active")
		.defaultValue(1)
		.field(COREAPI_NUMBER_FIELD_ON(key_format_mode_active, hasFormatButton, NULL)),
	// src/neutrino.cpp:6227 src/gui/keybind_setup.cpp:234
	boolRow("key_pic_mode_active")
		.section("keybindings")
		.label("extra.key_pic_mode")
		.hint("menu.hint_key_pic_mode_active")
		.defaultValue(1)
		.field(COREAPI_NUMBER_FIELD(key_pic_mode_active)),
	// src/neutrino.cpp:6228 src/gui/keybind_setup.cpp:235
	boolRow("key_pic_size_active")
		.section("keybindings")
		.label("extra.key_pic_size")
		.hint("menu.hint_key_pic_size_active")
		.defaultValue(1)
		.field(COREAPI_NUMBER_FIELD(key_pic_size_active)),
	// src/neutrino.cpp:6240 src/gui/keybind_setup.cpp:236
	keyRow("key_record")
		.section("keybindings")
		.label("extra.key_record")
		.hint("menu.hint_key_record")
		.range(kKeyNone, kKeyMax)
		.defaultValue((int32_t) CRCInput::RC_record)
		.field(COREAPI_NUMBER_FIELD(key_record)),
	// src/neutrino.cpp:6264 src/gui/keybind_setup.cpp:237
	keyRow("mbkey.copy_onefile")
		.section("keybindings")
		.label("mbkey.copy_onefile")
		.range(kKeyNone, kKeyMax)
		.defaultValue((int32_t) CRCInput::RC_nokey)
		.field(COREAPI_NUMBER_FIELD(mbkey_copy_onefile)),
	// src/neutrino.cpp:6265 src/gui/keybind_setup.cpp:238
	keyRow("mbkey.copy_several")
		.section("keybindings")
		.label("mbkey.copy_several")
		.range(kKeyNone, kKeyMax)
		.defaultValue((int32_t) CRCInput::RC_nokey)
		.field(COREAPI_NUMBER_FIELD(mbkey_copy_several)),
	// src/neutrino.cpp:6267 src/gui/keybind_setup.cpp:239
	keyRow("mbkey.cut")
		.section("keybindings")
		.label("mbkey.cut")
		.range(kKeyNone, kKeyMax)
		.defaultValue((int32_t) CRCInput::RC_nokey)
		.field(COREAPI_NUMBER_FIELD(mbkey_cut)),
	// src/neutrino.cpp:6268 src/gui/keybind_setup.cpp:240
	keyRow("mbkey.truncate")
		.section("keybindings")
		.label("mbkey.truncate")
		.range(kKeyNone, kKeyMax)
		.defaultValue((int32_t) CRCInput::RC_nokey)
		.field(COREAPI_NUMBER_FIELD(mbkey_truncate)),
	// src/neutrino.cpp:6269 src/gui/keybind_setup.cpp:241
	keyRow("mbkey.toggle_view_cw")
		.section("keybindings")
		.label("mbkey.toggle_view_cw")
		.range(kKeyNone, kKeyMax)
		.defaultValue((int32_t) CRCInput::RC_right)
		.field(COREAPI_NUMBER_FIELD(mbkey_toggle_view_cw)),
	// src/neutrino.cpp:6270 src/gui/keybind_setup.cpp:242
	keyRow("mbkey.toggle_view_ccw")
		.section("keybindings")
		.label("mbkey.toggle_view_ccw")
		.range(kKeyNone, kKeyMax)
		.defaultValue((int32_t) CRCInput::RC_left)
		.field(COREAPI_NUMBER_FIELD(mbkey_toggle_view_ccw)),
	// src/neutrino.cpp:6266 src/gui/keybind_setup.cpp:243
	keyRow("mbkey.cover")
		.section("keybindings")
		.label("mbkey.cover")
		.hint("menu.hint_mbkey_cover")
		.range(kKeyNone, kKeyMax)
		.defaultValue((int32_t) CRCInput::RC_favorites)
		.defaultFrom(mbKeyCoverDefault)
		.field(COREAPI_NUMBER_FIELD(mbkey_cover)),
	// src/neutrino.cpp:6294 src/gui/keybind_setup.cpp:526
	enumRow("bouquetlist_mode")
		.section("keybindings")
		.label("keybindingmenu.bouquetlist_mode")
		.defaultValue(0)
		.values(kBouquetlistMode)
		.field(COREAPI_NUMBER_FIELD(bouquetlist_mode)),
	/* The running box reads this once, when the input driver is built,
	   src/driver/rcinput.cpp:159. The screen calls the same function on its way
	   out, :347, and nothing else does. */
	// src/neutrino.cpp:700 src/gui/keybind_setup.cpp:320
	enumRow("remote_control_hardware")
		.section("keybindings")
		.label("keybindingmenu.remotecontrol_hardware")
		.hint("menu.hint_key_hardware")
		.defaultValue(0)
		.values(kRemoteHardware)
		.needsRestart()
		.field(COREAPI_NUMBER_FIELD_ON(remote_control_hardware, canSelectRemote, NULL)),
	// Milliseconds, and zero is what the screen offers as off.
	// src/neutrino.cpp:6297 src/gui/keybind_setup.cpp:335
	intRow("repeat_genericblocker")
		.section("keybindings")
		.label("keybindingmenu.repeatblockgeneric")
		.hint("menu.hint_key_repeatblockgeneric")
		.range(0, 999)
		.defaultValue(100)
		.unit("unit.short.millisecond")
		.field(COREAPI_NUMBER_FIELD(repeat_genericblocker)),
	// src/neutrino.cpp:6298 src/gui/keybind_setup.cpp:536
	boolRow("sms_channel")
		.section("keybindings")
		.label("extra.sms_channel")
		.hint("menu.hint_sms_channel")
		.defaultValue(0)
		.field(COREAPI_NUMBER_FIELD(sms_channel)),
	// src/neutrino.cpp:6299 src/gui/keybind_setup.cpp:584
	boolRow("sms_movie")
		.section("keybindings")
		.label("extra.sms_movie")
		.hint("menu.hint_sms_movie")
		.defaultValue(1)
		.field(COREAPI_NUMBER_FIELD(sms_movie)),
	// src/neutrino.cpp:6303 src/gui/keybind_setup.cpp:498
	enumRow("mode_left_right_key_tv")
		.section("keybindings")
		.label("keybindingmenu.mode_left_right_key_tv")
		.hint("menu.hint_key_right")
		.defaultValue(1)
		.values(kLeftRightKeyTv)
		.field(COREAPI_NUMBER_FIELD(mode_left_right_key_tv)),
	/* Local resolution: the source declares movieplayer_bisection_jump here
	   (minutes, zero for off). This fork stilled the whole feature - the
	   player's jump code and the screen's item both sit in #if 0 and load and
	   save are commented out in src/neutrino.cpp - so a row would declare a
	   key nothing reads. The member is carried in settingsundeclared.cpp and
	   is a port candidate if the feature returns (WORK-114). */
	/* The three below sit on the same screen and are loaded and saved beside
	   the keybindings, which is why they are here and not with the other
	   options: the program loads them in the same pass. */
	// src/neutrino.cpp:6295 src/gui/keybind_setup.cpp:426
	boolRow("menu_left_exit")
		.section("keybindings")
		.label("extra.menu_left_exit")
		.hint("menu.hint_key_left_exit")
		.defaultValue(0)
		.field(COREAPI_NUMBER_FIELD(menu_left_exit)),
	// Milliseconds, and zero is what the screen offers as off.
	// src/neutrino.cpp:6296 src/gui/keybind_setup.cpp:327
	intRow("repeat_blocker")
		.section("keybindings")
		.label("keybindingmenu.repeatblock")
		.hint("menu.hint_key_repeatblock")
		.range(0, 999)
		.defaultValue(450)
		.unit("unit.short.millisecond")
		.field(COREAPI_NUMBER_FIELD(repeat_blocker)),
	/* Milliseconds again, and here the floor is what off means rather than
	   zero: anything above it is a duration and the value itself is the only
	   one that is not. */
	// src/neutrino.cpp:6301 src/gui/keybind_setup.cpp:310
	intRow("longkeypress_duration")
		.section("keybindings")
		.label("keybindingmenu.longkeypress_duration")
		.hint("menu.hint_longkeypress_duration")
		.range(LONGKEYPRESS_OFF, 9999)
		.defaultValue(LONGKEYPRESS_OFF)
		.unit("unit.short.millisecond")
		.field(COREAPI_NUMBER_FIELD(longkeypress_duration)),
};

} // anonymous namespace

const Descriptor *settingsTableKeys(size_t &count)
{
	count = sizeof(kSettings) / sizeof(kSettings[0]);
	return kSettings;
}

} // namespace coreapi
