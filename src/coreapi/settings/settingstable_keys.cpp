/*
 * settingstable_keys.cpp - key binding settings, one row per field
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
const EnumValue kBouquetlistMode[] =
{
	{ 0, "keybindingmenu.channellist" },
	{ 1, "keybindingmenu.favorites" }
};

// src/gui/keybind_setup.cpp:157
const EnumValue kRemoteHardware[] =
{
	{ 0, "keybindingmenu.remotecontrol_hardware_coolstream" },
	{ 1, "keybindingmenu.remotecontrol_hardware_dbox" },
	{ 2, "keybindingmenu.remotecontrol_hardware_philips" }
};

// src/gui/keybind_setup.cpp:165
const EnumValue kLeftRightKeyTv[] =
{
	{ 0, "keybindingmenu.mode_left_right_key_tv_zap" },
	{ 1, "keybindingmenu.mode_left_right_key_tv_vzap" },
	{ 2, "keybindingmenu.mode_left_right_key_tv_volume" },
	{ 3, "keybindingmenu.mode_left_right_key_tv_infobar" }
};

const Descriptor kSettings[] =
{
	// src/neutrino.cpp:6257 src/gui/keybind_setup.cpp:183
	{
		"key_tvradio_mode", ValueType::Int, "keybindings",
		"keybindingmenu.tvradiomode", "menu.hint_key_tvradiomode",
		kKeyNone, kKeyMax, NULL, 0, (int32_t) CRCInput::RC_nokey, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(key_tvradio_mode)
	},
	// src/neutrino.cpp:6237 src/gui/keybind_setup.cpp:184
	{
		"key_power_off", ValueType::Int, "keybindings",
		"keybindingmenu.poweroff", "menu.hint_key_poweroff",
		kKeyNone, kKeyMax, NULL, 0, (int32_t) CRCInput::RC_standby, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(key_power_off)
	},
	// src/neutrino.cpp:6243 src/gui/keybind_setup.cpp:185
	{
		"key_standby_off_add", ValueType::Int, "keybindings",
		"keybindingmenu.standbyoff_add", "menu.hint_key_standbyoff_add",
		kKeyNone, kKeyMax, NULL, 0, (int32_t) CRCInput::RC_ok, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(key_standby_off_add)
	},
	// src/neutrino.cpp:6216 src/gui/keybind_setup.cpp:186
	{
		"key_favorites", ValueType::Int, "keybindings",
		"keybindingmenu.favorites", "menu.hint_key_favorites",
		kKeyNone, kKeyMax, NULL, 0, (int32_t) CRCInput::RC_favorites, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(key_favorites)
	},
	// src/neutrino.cpp:6226 src/gui/keybind_setup.cpp:187
	{
		"key_channelList_pageup", ValueType::Int, "keybindings",
		"keybindingmenu.pageup", "menu.hint_key_pageup",
		kKeyNone, kKeyMax, NULL, 0, (int32_t) CRCInput::RC_page_up, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(key_pageup)
	},
	// src/neutrino.cpp:6225 src/gui/keybind_setup.cpp:188
	{
		"key_channelList_pagedown", ValueType::Int, "keybindings",
		"keybindingmenu.pagedown", "menu.hint_key_pagedown",
		kKeyNone, kKeyMax, NULL, 0, (int32_t) CRCInput::RC_page_down, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(key_pagedown)
	},
	// src/neutrino.cpp:6261 src/gui/keybind_setup.cpp:189
	{
		"key_volumeup", ValueType::Int, "keybindings",
		"keybindingmenu.volumeup", "menu.hint_key_volumeup",
		kKeyNone, kKeyMax, NULL, 0, (int32_t) CRCInput::RC_plus, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(key_volumeup)
	},
	// src/neutrino.cpp:6260 src/gui/keybind_setup.cpp:190
	{
		"key_volumedown", ValueType::Int, "keybindings",
		"keybindingmenu.volumedown", "menu.hint_key_volumedown",
		kKeyNone, kKeyMax, NULL, 0, (int32_t) CRCInput::RC_minus, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(key_volumedown)
	},
	// src/neutrino.cpp:6223 src/gui/keybind_setup.cpp:191
	{
		"key_list_start", ValueType::Int, "keybindings",
		"extra.key_list_start", "menu.hint_key_list_start",
		kKeyNone, kKeyMax, NULL, 0, (int32_t) CRCInput::RC_nokey, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(key_list_start)
	},
	// src/neutrino.cpp:6222 src/gui/keybind_setup.cpp:192
	{
		"key_list_end", ValueType::Int, "keybindings",
		"extra.key_list_end", "menu.hint_key_list_end",
		kKeyNone, kKeyMax, NULL, 0, (int32_t) CRCInput::RC_nokey, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(key_list_end)
	},
	// src/neutrino.cpp:6210 src/gui/keybind_setup.cpp:193
	{
		"key_channelList_cancel", ValueType::Int, "keybindings",
		"keybindingmenu.cancel", "menu.hint_key_cancel",
		kKeyNone, kKeyMax, NULL, 0, (int32_t) CRCInput::RC_home, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(key_channelList_cancel)
	},
	// src/neutrino.cpp:6211 src/gui/keybind_setup.cpp:194
	{
		"key_channelList_sort", ValueType::Int, "keybindings",
		"keybindingmenu.sort", "menu.hint_key_sort",
		kKeyNone, kKeyMax, NULL, 0, (int32_t) CRCInput::RC_blue, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(key_channelList_sort)
	},
	// src/neutrino.cpp:6208 src/gui/keybind_setup.cpp:195
	{
		"key_channelList_addrecord", ValueType::Int, "keybindings",
		"keybindingmenu.addrecord", "menu.hint_key_addrecord",
		kKeyNone, kKeyMax, NULL, 0, (int32_t) CRCInput::RC_red, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(key_channelList_addrecord)
	},
	// src/neutrino.cpp:6209 src/gui/keybind_setup.cpp:196
	{
		"key_channelList_addremind", ValueType::Int, "keybindings",
		"keybindingmenu.addremind", "menu.hint_key_addremind",
		kKeyNone, kKeyMax, NULL, 0, (int32_t) CRCInput::RC_yellow, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(key_channelList_addremind)
	},
	// src/neutrino.cpp:6207 src/gui/keybind_setup.cpp:197
	{
		"key_bouquet_up", ValueType::Int, "keybindings",
		"keybindingmenu.bouquetup", "menu.hint_key_bouquetup",
		kKeyNone, kKeyMax, NULL, 0, (int32_t) CRCInput::RC_right, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(key_bouquet_up)
	},
	// src/neutrino.cpp:6206 src/gui/keybind_setup.cpp:198
	{
		"key_bouquet_down", ValueType::Int, "keybindings",
		"keybindingmenu.bouquetdown", "menu.hint_key_bouquetdown",
		kKeyNone, kKeyMax, NULL, 0, (int32_t) CRCInput::RC_left, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(key_bouquet_down)
	},
	// src/neutrino.cpp:6212 src/gui/keybind_setup.cpp:199
	{
		"key_current_transponder", ValueType::Int, "keybindings",
		"extra.key_current_transponder", "menu.hint_key_transponder",
		kKeyNone, kKeyMax, NULL, 0, (int32_t) CRCInput::RC_nokey, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(key_current_transponder)
	},
	// src/neutrino.cpp:6239 src/gui/keybind_setup.cpp:200
	{
		"key_quickzap_up", ValueType::Int, "keybindings",
		"keybindingmenu.channelup", "menu.hint_key_channelup",
		kKeyNone, kKeyMax, NULL, 0, (int32_t) CRCInput::RC_up, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(key_quickzap_up)
	},
	// src/neutrino.cpp:6238 src/gui/keybind_setup.cpp:201
	{
		"key_quickzap_down", ValueType::Int, "keybindings",
		"keybindingmenu.channeldown", "menu.hint_key_channeldown",
		kKeyNone, kKeyMax, NULL, 0, (int32_t) CRCInput::RC_down, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(key_quickzap_down)
	},
	// src/neutrino.cpp:6245 src/gui/keybind_setup.cpp:202
	{
		"key_subchannel_up", ValueType::Int, "keybindings",
		"keybindingmenu.subchannelup", "menu.hint_key_subchannelup",
		kKeyNone, kKeyMax, NULL, 0, (int32_t) CRCInput::RC_right, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(key_subchannel_up)
	},
	// src/neutrino.cpp:6244 src/gui/keybind_setup.cpp:203
	{
		"key_subchannel_down", ValueType::Int, "keybindings",
		"keybindingmenu.subchanneldown", "menu.hint_key_subchanneldown",
		kKeyNone, kKeyMax, NULL, 0, (int32_t) CRCInput::RC_left, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(key_subchannel_down)
	},
	// src/neutrino.cpp:6262 src/gui/keybind_setup.cpp:204
	{
		"key_zaphistory", ValueType::Int, "keybindings",
		"keybindingmenu.zaphistory", "menu.hint_key_history",
		kKeyNone, kKeyMax, NULL, 0, (int32_t) CRCInput::RC_home, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(key_zaphistory)
	},
	// src/neutrino.cpp:6221 src/gui/keybind_setup.cpp:205
	{
		"key_lastchannel", ValueType::Int, "keybindings",
		"keybindingmenu.lastchannel", "menu.hint_key_lastchannel",
		kKeyNone, kKeyMax, NULL, 0, (int32_t) CRCInput::RC_0, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(key_lastchannel)
	},
	// src/neutrino.cpp:6285 src/gui/keybind_setup.cpp:206
	{
		"mpkey.play", ValueType::Int, "keybindings",
		"mpkey.play", "menu.hint_key_mpplay",
		kKeyNone, kKeyMax, NULL, 0, (int32_t) CRCInput::RC_play, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(mpkey_play)
	},
	// src/neutrino.cpp:6284 src/gui/keybind_setup.cpp:207
	{
		"mpkey.pause", ValueType::Int, "keybindings",
		"mpkey.pause", "menu.hint_key_mppause",
		kKeyNone, kKeyMax, NULL, 0, (int32_t) CRCInput::RC_pause, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(mpkey_pause)
	},
	// src/neutrino.cpp:6289 src/gui/keybind_setup.cpp:208
	{
		"mpkey.stop", ValueType::Int, "keybindings",
		"mpkey.stop", "menu.hint_key_mpstop",
		kKeyNone, kKeyMax, NULL, 0, (int32_t) CRCInput::RC_stop, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(mpkey_stop)
	},
	// src/neutrino.cpp:6274 src/gui/keybind_setup.cpp:209
	{
		"mpkey.forward", ValueType::Int, "keybindings",
		"mpkey.forward", "menu.hint_key_mpforward",
		kKeyNone, kKeyMax, NULL, 0, (int32_t) CRCInput::RC_forward, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(mpkey_forward)
	},
	// src/neutrino.cpp:6288 src/gui/keybind_setup.cpp:210
	{
		"mpkey.rewind", ValueType::Int, "keybindings",
		"mpkey.rewind", "menu.hint_key_mprewind",
		kKeyNone, kKeyMax, NULL, 0, (int32_t) CRCInput::RC_rewind, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(mpkey_rewind)
	},
	// src/neutrino.cpp:6272 src/gui/keybind_setup.cpp:211
	{
		"mpkey.audio", ValueType::Int, "keybindings",
		"mpkey.audio", "menu.hint_key_mpaudio",
		kKeyNone, kKeyMax, NULL, 0, (int32_t) CRCInput::RC_green, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(mpkey_audio)
	},
	// src/neutrino.cpp:6290 src/gui/keybind_setup.cpp:212
	{
		"mpkey.subtitle", ValueType::Int, "keybindings",
		"mpkey.subtitle", "menu.hint_key_mpsubtitle",
		kKeyNone, kKeyMax, NULL, 0, (int32_t) CRCInput::RC_sub, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(mpkey_subtitle)
	},
	// src/neutrino.cpp:6291 src/gui/keybind_setup.cpp:213
	{
		"mpkey.time", ValueType::Int, "keybindings",
		"mpkey.time", "menu.hint_key_mptime",
		kKeyNone, kKeyMax, NULL, 0, (int32_t) CRCInput::RC_timeshift, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(mpkey_time)
	},
	// src/neutrino.cpp:6273 src/gui/keybind_setup.cpp:214
	{
		"mpkey.bookmark", ValueType::Int, "keybindings",
		"mpkey.bookmark", "menu.hint_key_mpbookmark",
		kKeyNone, kKeyMax, NULL, 0, (int32_t) CRCInput::RC_yellow, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(mpkey_bookmark)
	},
	// src/neutrino.cpp:6275 src/gui/keybind_setup.cpp:215
	{
		"mpkey.goto", ValueType::Int, "keybindings",
		"mpkey.goto", NULL,
		kKeyNone, kKeyMax, NULL, 0, (int32_t) CRCInput::RC_nokey, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(mpkey_goto)
	},
	// src/neutrino.cpp:6276 src/gui/keybind_setup.cpp:216
	{
		"mpkey.next_repeat_mode", ValueType::Int, "keybindings",
		"mpkey.next_repeat_mode", NULL,
		kKeyNone, kKeyMax, NULL, 0, (int32_t) CRCInput::RC_nokey, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(mpkey_next_repeat_mode)
	},
	// src/neutrino.cpp:6287 src/gui/keybind_setup.cpp:217
	{
		"mpkey.plugin", ValueType::Int, "keybindings",
		"mpkey.plugin", "menu.hint_key_mpplugin",
		kKeyNone, kKeyMax, NULL, 0, (int32_t) CRCInput::RC_nokey, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(mpkey_plugin)
	},
	// src/neutrino.cpp:6252 src/gui/keybind_setup.cpp:218
	{
		"key_timeshift", ValueType::Int, "keybindings",
		"extra.key_timeshift", "menu.hint_key_timeshift",
		kKeyNone, kKeyMax, NULL, 0, (int32_t) CRCInput::RC_pause, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(key_timeshift)
	},
	// src/neutrino.cpp:6259 src/gui/keybind_setup.cpp:219
	{
		"key_unlock", ValueType::Int, "keybindings",
		"extra.key_unlock", "menu.hint_key_unlock",
		kKeyNone, kKeyMax, NULL, 0, (int32_t) CRCInput::RC_setup, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(key_unlock)
	},
	// src/neutrino.cpp:6220 src/gui/keybind_setup.cpp:220
	{
		"key_help", ValueType::Int, "keybindings",
		"extra.key_help", NULL,
		kKeyNone, kKeyMax, NULL, 0, (int32_t) CRCInput::RC_help, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(key_help)
	},
	// src/neutrino.cpp:6224 src/gui/keybind_setup.cpp:221
	{
		"key_next43mode", ValueType::Int, "keybindings",
		"extra.key_next43mode", NULL,
		kKeyNone, kKeyMax, NULL, 0, (int32_t) CRCInput::RC_nokey, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(key_next43mode)
	},
	// src/neutrino.cpp:6246 src/gui/keybind_setup.cpp:222
	{
		"key_switchformat", ValueType::Int, "keybindings",
		"extra.key_switchformat", NULL,
		kKeyNone, kKeyMax, NULL, 0, (int32_t) CRCInput::RC_nokey, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(key_switchformat)
	},
	// src/neutrino.cpp:6241 src/gui/keybind_setup.cpp:223
	{
		"key_screenshot", ValueType::Int, "keybindings",
		"extra.key_screenshot", "menu.hint_key_screenshot",
		kKeyNone, kKeyMax, NULL, 0, (int32_t) CRCInput::RC_games, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(key_screenshot)
	},
	// src/neutrino.cpp:6242 src/gui/keybind_setup.cpp:224
	{
		"key_sleep", ValueType::Int, "keybindings",
		"extra.key_sleep", "menu.hint_key_sleep",
		kKeyNone, kKeyMax, NULL, 0, (int32_t) CRCInput::RC_sleep, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(key_sleep)
	},
#if ENABLE_PIP
	// src/neutrino.cpp:6230 src/gui/keybind_setup.cpp:226
	{
		"key_pip_close", ValueType::Int, "keybindings",
		"extra.key_pip_close", "menu.hint_key_pip_close",
		kKeyNone, kKeyMax, NULL, 0, (int32_t) CRCInput::RC_prev, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(key_pip_close)
	},
	// src/neutrino.cpp:6231 src/gui/keybind_setup.cpp:227
	{
		"key_pip_close_avinput", ValueType::Int, "keybindings",
		"extra.key_pip_close_avinput", NULL,
		kKeyNone, kKeyMax, NULL, 0, (int32_t) CRCInput::RC_nokey, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(key_pip_close_avinput)
	},
	// src/neutrino.cpp:6232 src/gui/keybind_setup.cpp:228
	{
		"key_pip_rotate_cw", ValueType::Int, "keybindings",
		"extra.key_pip_rotate_cw", "menu.hint_key_pip_rotate_cw",
		kKeyNone, kKeyMax, NULL, 0, (int32_t) CRCInput::RC_nokey, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(key_pip_rotate_cw)
	},
	// src/neutrino.cpp:6233 src/gui/keybind_setup.cpp:229
	{
		"key_pip_rotate_ccw", ValueType::Int, "keybindings",
		"extra.key_pip_rotate_ccw", "menu.hint_key_pip_rotate_ccw",
		kKeyNone, kKeyMax, NULL, 0, (int32_t) CRCInput::RC_nokey, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(key_pip_rotate_ccw)
	},
	// src/neutrino.cpp:6234 src/gui/keybind_setup.cpp:230
	{
		"key_pip_setup", ValueType::Int, "keybindings",
		"extra.key_pip_setup", "menu.hint_key_pip_setup",
		kKeyNone, kKeyMax, NULL, 0, (int32_t) CRCInput::RC_nokey, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(key_pip_setup)
	},
	// src/neutrino.cpp:6235 src/gui/keybind_setup.cpp:231
	{
		"key_pip_swap", ValueType::Int, "keybindings",
		"extra.key_pip_swap", "menu.hint_key_pip_close",
		kKeyNone, kKeyMax, NULL, 0, (int32_t) CRCInput::RC_next, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(key_pip_swap)
	},
#endif
	// src/neutrino.cpp:6219 src/gui/keybind_setup.cpp:233
	{
		"key_format_mode_active", ValueType::Bool, "keybindings",
		"extra.key_format_mode", "menu.hint_key_format_mode_active",
		0, 1, NULL, 0, 1, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(key_format_mode_active)
	},
	// src/neutrino.cpp:6227 src/gui/keybind_setup.cpp:234
	{
		"key_pic_mode_active", ValueType::Bool, "keybindings",
		"extra.key_pic_mode", "menu.hint_key_pic_mode_active",
		0, 1, NULL, 0, 1, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(key_pic_mode_active)
	},
	// src/neutrino.cpp:6228 src/gui/keybind_setup.cpp:235
	{
		"key_pic_size_active", ValueType::Bool, "keybindings",
		"extra.key_pic_size", "menu.hint_key_pic_size_active",
		0, 1, NULL, 0, 1, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(key_pic_size_active)
	},
	// src/neutrino.cpp:6240 src/gui/keybind_setup.cpp:236
	{
		"key_record", ValueType::Int, "keybindings",
		"extra.key_record", "menu.hint_key_record",
		kKeyNone, kKeyMax, NULL, 0, (int32_t) CRCInput::RC_record, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(key_record)
	},
	// src/neutrino.cpp:6264 src/gui/keybind_setup.cpp:237
	{
		"mbkey.copy_onefile", ValueType::Int, "keybindings",
		"mbkey.copy_onefile", NULL,
		kKeyNone, kKeyMax, NULL, 0, (int32_t) CRCInput::RC_nokey, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(mbkey_copy_onefile)
	},
	// src/neutrino.cpp:6265 src/gui/keybind_setup.cpp:238
	{
		"mbkey.copy_several", ValueType::Int, "keybindings",
		"mbkey.copy_several", NULL,
		kKeyNone, kKeyMax, NULL, 0, (int32_t) CRCInput::RC_nokey, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(mbkey_copy_several)
	},
	// src/neutrino.cpp:6267 src/gui/keybind_setup.cpp:239
	{
		"mbkey.cut", ValueType::Int, "keybindings",
		"mbkey.cut", NULL,
		kKeyNone, kKeyMax, NULL, 0, (int32_t) CRCInput::RC_nokey, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(mbkey_cut)
	},
	// src/neutrino.cpp:6268 src/gui/keybind_setup.cpp:240
	{
		"mbkey.truncate", ValueType::Int, "keybindings",
		"mbkey.truncate", NULL,
		kKeyNone, kKeyMax, NULL, 0, (int32_t) CRCInput::RC_nokey, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(mbkey_truncate)
	},
	// src/neutrino.cpp:6269 src/gui/keybind_setup.cpp:241
	{
		"mbkey.toggle_view_cw", ValueType::Int, "keybindings",
		"mbkey.toggle_view_cw", NULL,
		kKeyNone, kKeyMax, NULL, 0, (int32_t) CRCInput::RC_right, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(mbkey_toggle_view_cw)
	},
	// src/neutrino.cpp:6270 src/gui/keybind_setup.cpp:242
	{
		"mbkey.toggle_view_ccw", ValueType::Int, "keybindings",
		"mbkey.toggle_view_ccw", NULL,
		kKeyNone, kKeyMax, NULL, 0, (int32_t) CRCInput::RC_left, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(mbkey_toggle_view_ccw)
	},
	// src/neutrino.cpp:6266 src/gui/keybind_setup.cpp:243
	{
		"mbkey.cover", ValueType::Int, "keybindings",
		"mbkey.cover", "menu.hint_mbkey_cover",
		kKeyNone, kKeyMax, NULL, 0, (int32_t) CRCInput::RC_favorites, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(mbkey_cover)
	},
	// src/neutrino.cpp:6294 src/gui/keybind_setup.cpp:526
	{
		"bouquetlist_mode", ValueType::Enum, "keybindings",
		"keybindingmenu.bouquetlist_mode", NULL,
		0, 0, COREAPI_ENUM(kBouquetlistMode), 0, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(bouquetlist_mode)
	},
	/* The running box reads this once, when the input driver is built,
	   src/driver/rcinput.cpp:159. The screen calls the same function on its way
	   out, :347, and nothing else does. */
	// src/neutrino.cpp:700 src/gui/keybind_setup.cpp:320
	{
		"remote_control_hardware", ValueType::Enum, "keybindings",
		"keybindingmenu.remotecontrol_hardware", "menu.hint_key_hardware",
		0, 0, COREAPI_ENUM(kRemoteHardware), 0, NULL, true, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(remote_control_hardware)
	},
	// Milliseconds, and zero is what the screen offers as off.
	// src/neutrino.cpp:6297 src/gui/keybind_setup.cpp:335
	{
		"repeat_genericblocker", ValueType::Int, "keybindings",
		"keybindingmenu.repeatblockgeneric", "menu.hint_key_repeatblockgeneric",
		0, 999, NULL, 0, 100, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(repeat_genericblocker)
	},
	// src/neutrino.cpp:6298 src/gui/keybind_setup.cpp:536
	{
		"sms_channel", ValueType::Bool, "keybindings",
		"extra.sms_channel", "menu.hint_sms_channel",
		0, 1, NULL, 0, 0, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(sms_channel)
	},
	// src/neutrino.cpp:6299 src/gui/keybind_setup.cpp:584
	{
		"sms_movie", ValueType::Bool, "keybindings",
		"extra.sms_movie", "menu.hint_sms_movie",
		0, 1, NULL, 0, 1, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(sms_movie)
	},
	// src/neutrino.cpp:6303 src/gui/keybind_setup.cpp:498
	{
		"mode_left_right_key_tv", ValueType::Enum, "keybindings",
		"keybindingmenu.mode_left_right_key_tv", "menu.hint_key_right",
		0, 0, COREAPI_ENUM(kLeftRightKeyTv), 1, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(mode_left_right_key_tv)
	},
	// Minutes, and zero is what the screen offers as off.
	// src/neutrino.cpp:1250 src/gui/keybind_setup.cpp:567
	{
		"movieplayer_bisection_jump", ValueType::Int, "keybindings",
		"movieplayer.bisection_jump", "menu.hint_movieplayer_bisection_jump",
		0, 10, NULL, 0, 5, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(movieplayer_bisection_jump)
	},
	/* The three below sit on the same screen and are loaded and saved beside
	   the keybindings, which is why they are here and not with the other
	   options: the program loads them in the same pass. */
	// src/neutrino.cpp:6295 src/gui/keybind_setup.cpp:426
	{
		"menu_left_exit", ValueType::Bool, "keybindings",
		"extra.menu_left_exit", "menu.hint_key_left_exit",
		0, 1, NULL, 0, 0, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(menu_left_exit)
	},
	// Milliseconds, and zero is what the screen offers as off.
	// src/neutrino.cpp:6296 src/gui/keybind_setup.cpp:327
	{
		"repeat_blocker", ValueType::Int, "keybindings",
		"keybindingmenu.repeatblock", "menu.hint_key_repeatblock",
		0, 999, NULL, 0, 450, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(repeat_blocker)
	},
	/* Milliseconds again, and here the floor is what off means rather than
	   zero: anything above it is a duration and the value itself is the only
	   one that is not. */
	// src/neutrino.cpp:6301 src/gui/keybind_setup.cpp:310
	{
		"longkeypress_duration", ValueType::Int, "keybindings",
		"keybindingmenu.longkeypress_duration", "menu.hint_longkeypress_duration",
		LONGKEYPRESS_OFF, 9999, NULL, 0, LONGKEYPRESS_OFF, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(longkeypress_duration)
	},
};

} // anonymous namespace

const Descriptor *settingsTableKeys(size_t &count)
{
	count = sizeof(kSettings) / sizeof(kSettings[0]);
	return kSettings;
}

} // namespace coreapi
