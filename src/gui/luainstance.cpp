/*
 * neutrino-mp lua to c++ bridge
 *
 * (C) 2013 Stefan Seyfried <seife@tuxboxcvs.slipkontur.de>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <config.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <unistd.h>

#include <global.h>
#include <system/helpers.h>
#include <system/settings.h>
#include <gui/widget/msgbox.h>
#include <gui/widget/messagebox.h>
#include <gui/filebrowser.h>
#include <gui/movieplayer.h>
#include <driver/pictureviewer/pictureviewer.h>
#include <neutrino.h>
#include <system/debug.h>
#include "luainstance.h"
#include <video.h>

/* the magic color that tells us we are using one of the palette colors */
#define MAGIC_COLOR 0x42424200
#define MAGIC_MASK  0xFFFFFF00

struct table_key {
	const char *name;
	lua_Integer code;
};

struct table_key_u {
	const char *name;
	lua_Unsigned code;
};

struct lua_envexport {
	const char *name;
	table_key *t;
};

struct lua_envexport_u {
	const char *name;
	table_key_u *t;
};

static void set_lua_variables(lua_State *L)
{
	/* keyname table created with
	 * sed -n '/^[[:space:]]*RC_0/,/^[[:space:]]*RC_analog_off/ {
	 * 	s#^[[:space:]]*RC_\([^[:space:]]*\).*#	{ "\1",	CRCInput::RC_\1 },#p
	 * }' driver/rcinput.h
	 */
	static table_key keyname[] =
	{
		{ "0",			CRCInput::RC_0 },
		{ "1",			CRCInput::RC_1 },
		{ "2",			CRCInput::RC_2 },
		{ "3",			CRCInput::RC_3 },
		{ "4",			CRCInput::RC_4 },
		{ "5",			CRCInput::RC_5 },
		{ "6",			CRCInput::RC_6 },
		{ "7",			CRCInput::RC_7 },
		{ "8",			CRCInput::RC_8 },
		{ "9",			CRCInput::RC_9 },
		{ "backspace",		CRCInput::RC_backspace },
		{ "up",			CRCInput::RC_up },
		{ "left",		CRCInput::RC_left },
		{ "right",		CRCInput::RC_right },
		{ "down",		CRCInput::RC_down },
		{ "spkr",		CRCInput::RC_spkr },
		{ "minus",		CRCInput::RC_minus },
		{ "plus",		CRCInput::RC_plus },
		{ "standby",		CRCInput::RC_standby },
		{ "help",		CRCInput::RC_help },
		{ "home",		CRCInput::RC_home },
		{ "setup",		CRCInput::RC_setup },
		{ "topleft",		CRCInput::RC_topleft },
		{ "topright",		CRCInput::RC_topright },
		{ "page_up",		CRCInput::RC_page_up },
		{ "page_down",		CRCInput::RC_page_down },
		{ "ok",			CRCInput::RC_ok },
		{ "red",		CRCInput::RC_red },
		{ "green",		CRCInput::RC_green },
		{ "yellow",		CRCInput::RC_yellow },
		{ "blue",		CRCInput::RC_blue },
		{ "top_left",		CRCInput::RC_top_left },
		{ "top_right",		CRCInput::RC_top_right },
		{ "bottom_left",	CRCInput::RC_bottom_left },
		{ "bottom_right",	CRCInput::RC_bottom_right },
		{ "audio",		CRCInput::RC_audio },
		{ "video",		CRCInput::RC_video },
		{ "tv",			CRCInput::RC_tv },
		{ "radio",		CRCInput::RC_radio },
		{ "text",		CRCInput::RC_text },
		{ "info",		CRCInput::RC_info },
		{ "epg",		CRCInput::RC_epg },
		{ "recall",		CRCInput::RC_recall },
		{ "favorites",		CRCInput::RC_favorites },
		{ "sat",		CRCInput::RC_sat },
		{ "sat2",		CRCInput::RC_sat2 },
		{ "record",		CRCInput::RC_record },
		{ "play",		CRCInput::RC_play },
		{ "pause",		CRCInput::RC_pause },
		{ "forward",		CRCInput::RC_forward },
		{ "rewind",		CRCInput::RC_rewind },
		{ "stop",		CRCInput::RC_stop },
		{ "timeshift",		CRCInput::RC_timeshift },
		{ "mode",		CRCInput::RC_mode },
		{ "games",		CRCInput::RC_games },
		{ "next",		CRCInput::RC_next },
		{ "prev",		CRCInput::RC_prev },
		{ "www",		CRCInput::RC_www },
		{ "power_on",		CRCInput::RC_power_on },
		{ "power_off",		CRCInput::RC_power_off },
		{ "standby_on",		CRCInput::RC_standby_on },
		{ "standby_off",	CRCInput::RC_standby_off },
		{ "mute_on",		CRCInput::RC_mute_on },
		{ "mute_off",		CRCInput::RC_mute_off },
		{ "analog_on",		CRCInput::RC_analog_on },
		{ "analog_off",		CRCInput::RC_analog_off },
#if 0
		{ "find",		CRCInput::RC_find },
		{ "pip",		CRCInput::RC_pip },
		{ "folder",		CRCInput::RC_archive },
		{ "forward",		CRCInput::RC_fastforward },
		{ "slow",		CRCInput::RC_slow },
		{ "playmode",		CRCInput::RC_playmode },
		{ "usb",		CRCInput::RC_usb },
		{ "f1",			CRCInput::RC_f1 },
		{ "f2",			CRCInput::RC_f2 },
		{ "f3",			CRCInput::RC_f3 },
		{ "f4",			CRCInput::RC_f4 },
		{ "prog1",		CRCInput::RC_prog1 },
		{ "prog2",		CRCInput::RC_prog2 },
		{ "prog3",		CRCInput::RC_prog3 },
		{ "prog4",		CRCInput::RC_prog4 },
#endif
		/* to check if it is in our range */
		{ "MaxRC",		CRCInput::RC_MaxRC },
		{ NULL, 0 }
	};

	/* list of colors, exported e.g. as COL['INFOBAR_SHADOW'] */
	static table_key_u colorlist[] =
	{
		{ "COLORED_EVENTS_CHANNELLIST",	MAGIC_COLOR | (COL_COLORED_EVENTS_CHANNELLIST) },
		{ "COLORED_EVENTS_INFOBAR",	MAGIC_COLOR | (COL_COLORED_EVENTS_INFOBAR) },
		{ "INFOBAR_SHADOW",		MAGIC_COLOR | (COL_INFOBAR_SHADOW) },
		{ "INFOBAR",			MAGIC_COLOR | (COL_INFOBAR) },
		{ "MENUHEAD",			MAGIC_COLOR | (COL_MENUHEAD) },
		{ "MENUCONTENT",		MAGIC_COLOR | (COL_MENUCONTENT) },
		{ "MENUCONTENTDARK",		MAGIC_COLOR | (COL_MENUCONTENTDARK) },
		{ "MENUCONTENTSELECTED",	MAGIC_COLOR | (COL_MENUCONTENTSELECTED) },
		{ "MENUCONTENTINACTIVE",	MAGIC_COLOR | (COL_MENUCONTENTINACTIVE) },
		{ "BACKGROUND",			MAGIC_COLOR | (COL_BACKGROUND) },
		{ "DARK_RED",			MAGIC_COLOR | (COL_DARK_RED0) },
		{ "DARK_GREEN",			MAGIC_COLOR | (COL_DARK_GREEN0) },
		{ "DARK_BLUE",			MAGIC_COLOR | (COL_DARK_BLUE0) },
		{ "LIGHT_GRAY",			MAGIC_COLOR | (COL_LIGHT_GRAY0) },
		{ "DARK_GRAY",			MAGIC_COLOR | (COL_DARK_GRAY0) },
		{ "RED",			MAGIC_COLOR | (COL_RED0) },
		{ "GREEN",			MAGIC_COLOR | (COL_GREEN0) },
		{ "YELLOW",			MAGIC_COLOR | (COL_YELLOW0) },
		{ "BLUE",			MAGIC_COLOR | (COL_BLUE0) },
		{ "PURP",			MAGIC_COLOR | (COL_PURP0) },
		{ "LIGHT_BLUE",			MAGIC_COLOR | (COL_LIGHT_BLUE0) },
		{ "WHITE",			MAGIC_COLOR | (COL_WHITE0) },
		{ "BLACK",			MAGIC_COLOR | (COL_BLACK0) },
		{ "COLORED_EVENTS_TEXT",	(lua_Unsigned) (COL_COLORED_EVENTS_TEXT) },
		{ "INFOBAR_TEXT",		(lua_Unsigned) (COL_INFOBAR_TEXT) },
		{ "INFOBAR_SHADOW_TEXT",	(lua_Unsigned) (COL_INFOBAR_SHADOW_TEXT) },
		{ "MENUHEAD_TEXT",		(lua_Unsigned) (COL_MENUHEAD_TEXT) },
		{ "MENUCONTENT_TEXT",		(lua_Unsigned) (COL_MENUCONTENT_TEXT) },
		{ "MENUCONTENT_TEXT_PLUS_1",	(lua_Unsigned) (COL_MENUCONTENT_TEXT_PLUS_1) },
		{ "MENUCONTENT_TEXT_PLUS_2",	(lua_Unsigned) (COL_MENUCONTENT_TEXT_PLUS_2) },
		{ "MENUCONTENT_TEXT_PLUS_3",	(lua_Unsigned) (COL_MENUCONTENT_TEXT_PLUS_3) },
		{ "MENUCONTENTDARK_TEXT",	(lua_Unsigned) (COL_MENUCONTENTDARK_TEXT) },
		{ "MENUCONTENTDARK_TEXT_PLUS_1",	(lua_Unsigned) (COL_MENUCONTENTDARK_TEXT_PLUS_1) },
		{ "MENUCONTENTDARK_TEXT_PLUS_2",	(lua_Unsigned) (COL_MENUCONTENTDARK_TEXT_PLUS_2) },
		{ "MENUCONTENTSELECTED_TEXT",		(lua_Unsigned) (COL_MENUCONTENTSELECTED_TEXT) },
		{ "MENUCONTENTSELECTED_TEXT_PLUS_1",	(lua_Unsigned) (COL_MENUCONTENTSELECTED_TEXT_PLUS_1) },
		{ "MENUCONTENTSELECTED_TEXT_PLUS_2",	(lua_Unsigned) (COL_MENUCONTENTSELECTED_TEXT_PLUS_2) },
		{ "MENUCONTENTINACTIVE_TEXT",		(lua_Unsigned) (COL_MENUCONTENTINACTIVE_TEXT) },
		{ "MENUHEAD_PLUS_0",			(lua_Unsigned) (COL_MENUHEAD_PLUS_0) },
		{ "MENUCONTENT_PLUS_0",			(lua_Unsigned) (COL_MENUCONTENT_PLUS_0) },
		{ "MENUCONTENT_PLUS_1",			(lua_Unsigned) (COL_MENUCONTENT_PLUS_1) },
		{ "MENUCONTENT_PLUS_2",			(lua_Unsigned) (COL_MENUCONTENT_PLUS_2) },
		{ "MENUCONTENT_PLUS_3",			(lua_Unsigned) (COL_MENUCONTENT_PLUS_3) },
		{ "MENUCONTENT_PLUS_4",			(lua_Unsigned) (COL_MENUCONTENT_PLUS_4) },
		{ "MENUCONTENT_PLUS_5",			(lua_Unsigned) (COL_MENUCONTENT_PLUS_5) },
		{ "MENUCONTENT_PLUS_6",			(lua_Unsigned) (COL_MENUCONTENT_PLUS_6) },
		{ "MENUCONTENT_PLUS_7",			(lua_Unsigned) (COL_MENUCONTENT_PLUS_7) },
		{ "MENUCONTENTDARK_PLUS_0",		(lua_Unsigned) (COL_MENUCONTENTDARK_PLUS_0) },
		{ "MENUCONTENTDARK_PLUS_2",		(lua_Unsigned) (COL_MENUCONTENTDARK_PLUS_2) },
		{ "MENUCONTENTSELECTED_PLUS_0",		(lua_Unsigned) (COL_MENUCONTENTSELECTED_PLUS_0) },
		{ "MENUCONTENTSELECTED_PLUS_2",		(lua_Unsigned) (COL_MENUCONTENTSELECTED_PLUS_2) },
		{ "MENUCONTENTINACTIVE_PLUS_0",		(lua_Unsigned) (COL_MENUCONTENTINACTIVE_PLUS_0) },
		{ NULL, 0 }
	};

	/* font list, the _TYPE_ is redundant, exported e.g. as FONT['MENU'] */
	static table_key fontlist[] =
	{
		{ "MENU",		SNeutrinoSettings::FONT_TYPE_MENU },
		{ "MENU_TITLE",		SNeutrinoSettings::FONT_TYPE_MENU_TITLE },
		{ "MENU_INFO",		SNeutrinoSettings::FONT_TYPE_MENU_INFO },
		{ "EPG_TITLE",		SNeutrinoSettings::FONT_TYPE_EPG_TITLE },
		{ "EPG_INFO1",		SNeutrinoSettings::FONT_TYPE_EPG_INFO1 },
		{ "EPG_INFO2",		SNeutrinoSettings::FONT_TYPE_EPG_INFO2 },
		{ "EPG_DATE",		SNeutrinoSettings::FONT_TYPE_EPG_DATE },
		{ "EVENTLIST_TITLE",	SNeutrinoSettings::FONT_TYPE_EVENTLIST_TITLE },
		{ "EVENTLIST_ITEMLARGE",SNeutrinoSettings::FONT_TYPE_EVENTLIST_ITEMLARGE },
		{ "EVENTLIST_ITEMSMALL",SNeutrinoSettings::FONT_TYPE_EVENTLIST_ITEMSMALL },
		{ "EVENTLIST_DATETIME",	SNeutrinoSettings::FONT_TYPE_EVENTLIST_DATETIME },
		{ "CHANNELLIST",	SNeutrinoSettings::FONT_TYPE_CHANNELLIST },
		{ "CHANNELLIST_DESCR",	SNeutrinoSettings::FONT_TYPE_CHANNELLIST_DESCR },
		{ "CHANNELLIST_NUMBER",	SNeutrinoSettings::FONT_TYPE_CHANNELLIST_NUMBER },
		{ "CHANNELLIST_EVENT",	SNeutrinoSettings::FONT_TYPE_CHANNELLIST_EVENT },
		{ "CHANNEL_NUM_ZAP",	SNeutrinoSettings::FONT_TYPE_CHANNEL_NUM_ZAP },
		{ "INFOBAR_NUMBER",	SNeutrinoSettings::FONT_TYPE_INFOBAR_NUMBER },
		{ "INFOBAR_CHANNAME",	SNeutrinoSettings::FONT_TYPE_INFOBAR_CHANNAME },
		{ "INFOBAR_INFO",	SNeutrinoSettings::FONT_TYPE_INFOBAR_INFO },
		{ "INFOBAR_SMALL",	SNeutrinoSettings::FONT_TYPE_INFOBAR_SMALL },
		{ "FILEBROWSER_ITEM",	SNeutrinoSettings::FONT_TYPE_FILEBROWSER_ITEM },
		{ "MENU_HINT",		SNeutrinoSettings::FONT_TYPE_MENU_HINT },
		{ NULL, 0 }
	};

	table_key corners[] =
	{
		{ "TOP_LEFT",		CORNER_TOP_LEFT },
		{ "TOP_RIGHT",		CORNER_TOP_RIGHT },
		{ "BOTTOM_LEFT",	CORNER_BOTTOM_LEFT },
		{ "BOTTOM_RIGHT",	CORNER_BOTTOM_RIGHT },
		{ "RADIUS_LARGE",	RADIUS_LARGE },	/* those depend on g_settings.rounded_corners */
		{ "RADIUS_MID",		RADIUS_MID },
		{ "RADIUS_SMALL",	RADIUS_SMALL },
		{ "RADIUS_MIN",		RADIUS_MIN },
		{ NULL, 0 }
	};

	/* screen offsets, exported as e.g. SCREEN['END_Y'] */
	table_key screenopts[] =
	{
		{ "OFF_X", g_settings.screen_StartX },
		{ "OFF_Y", g_settings.screen_StartY },
		{ "END_X", g_settings.screen_EndX },
		{ "END_Y", g_settings.screen_EndY },
		{ NULL, 0 }
	};
	table_key menureturn[] =
	{
		{ "NONE", menu_return::RETURN_NONE },
		{ "REPAINT", menu_return::RETURN_REPAINT },
		{ "EXIT", menu_return::RETURN_EXIT },
		{ "EXIT_ALL", menu_return::RETURN_EXIT_ALL },
		{ "EXIT_REPAINT", menu_return::RETURN_EXIT_REPAINT },
		{ NULL, 0 }
	};

	/* list of environment variable arrays to be exported */
	lua_envexport e[] =
	{
		{ "RC",		keyname },
		{ "SCREEN",	screenopts },
		{ "FONT",	fontlist },
		{ "CORNER",	corners },
		{ "MENU_RETURN", menureturn },
		{ NULL, NULL }
	};

	int i = 0;
	while (e[i].name) {
		int j = 0;
		lua_newtable(L);
		while (e[i].t[j].name) {
			lua_pushstring(L, e[i].t[j].name);
			lua_pushinteger(L, e[i].t[j].code);
			lua_settable(L, -3);
			j++;
		}
		lua_setglobal(L, e[i].name);
		i++;
	}

	lua_envexport_u e_u[] =
	{
		{ "COL",	colorlist },
		{ NULL, NULL }
	};

	i = 0;
	while (e_u[i].name) {
		int j = 0;
		lua_newtable(L);
		while (e_u[i].t[j].name) {
			lua_pushstring(L, e_u[i].t[j].name);
			lua_pushunsigned(L, e_u[i].t[j].code);
			lua_settable(L, -3);
			j++;
		}
		lua_setglobal(L, e_u[i].name);
		i++;
	}
}

//#define DBG printf
#define DBG(...)

#define lua_boxpointer(L, u) \
	(*(void **)(lua_newuserdata(L, sizeof(void *))) = (u))

#define lua_unboxpointer(L, i) \
	(*(void **)(lua_touserdata(L, i)))

const char CLuaInstance::className[] = "neutrino";

CLuaInstance::CLuaInstance()
{
	/* Create the intepreter object.  */
	lua = luaL_newstate();

	/* register standard + custom functions. */
	registerFunctions();
}

CLuaInstance::~CLuaInstance()
{
	if (lua != NULL)
	{
		lua_close(lua);
		lua = NULL;
	}
}

/* Ported by Benny Chen, http://www.bennychen.cn/tag/lual_checkbool/ */
bool CLuaInstance::_luaL_checkbool(lua_State *L, int numArg)
{
	bool b = false;
	if (lua_isboolean(L, numArg))
		b = lua_toboolean(L, numArg);
	else {
		lua_Debug ar;
		lua_getstack(L, 0, &ar);
		lua_getinfo(L, "n", &ar);
		luaL_error(L, "bad argument #%d to '%s' (%s expected, got %s)\n", 
				numArg-1, ar.name,
				lua_typename(L, LUA_TBOOLEAN),
				lua_typename(L, lua_type(L, numArg)));
	}
	return b;
}

void CLuaInstance::functionDeprecated(lua_State *L, const char* oldFunc, const char* newFunc)
{
	lua_Debug ar;
	lua_getstack(L, 1, &ar);
	lua_getinfo(L, "Sl", &ar);
	printf("[Lua Script] \33[1;31m%s\33[0m %s \33[33m%s\33[0m %s \33[1;33m%s\33[0m.\n                      (%s:%d)\n", 
					g_Locale->getText(LOCALE_LUA_FUNCTION_DEPRECATED1), 
					g_Locale->getText(LOCALE_LUA_FUNCTION_DEPRECATED2), oldFunc,
					g_Locale->getText(LOCALE_LUA_FUNCTION_DEPRECATED3), newFunc,
					ar.short_src, ar.currentline);
}

lua_Unsigned CLuaInstance::checkMagicMask(lua_Unsigned &col)
{
	if ((col & MAGIC_MASK) == MAGIC_COLOR)
		/* use the color constants */
		col = CFrameBuffer::getInstance()->realcolor[col & 0x000000ff];
	return col;
}

#define SET_VAR1(NAME) \
	lua_pushinteger(lua, NAME); \
	lua_setglobal(lua, #NAME);
#define SET_VAR2(NAME, VALUE) \
	lua_pushinteger(lua, VALUE); \
	lua_setglobal(lua, #NAME);

/* Run the given script. */
void CLuaInstance::runScript(const char *fileName, std::vector<std::string> *argv, std::string *result_code, std::string *result_string, std::string *error_string)
{
	// luaL_dofile(lua, fileName);
	/* run the script */
	int status = luaL_loadfile(lua, fileName);
	if (status) {
		fprintf(stderr, "[CLuaInstance::%s] Can't load file: %s\n", __func__, lua_tostring(lua, -1));
		ShowMsg2UTF("Lua script error:", lua_tostring(lua, -1), CMsgBox::mbrBack, CMsgBox::mbBack);
		if (error_string)
			*error_string = std::string(lua_tostring(lua, -1));
		return;
	}
	int argvSize = 1;
	int n = 0;
	set_lua_variables(lua);
	if (argv && (!argv->empty()))
		argvSize += argv->size();
	lua_createtable(lua, argvSize, 0);

	// arg0 is scriptname
	lua_pushstring(lua, fileName);
	lua_rawseti(lua, -2, n++);

	if (argv && (!argv->empty())) {
		for(std::vector<std::string>::iterator it = argv->begin(); it != argv->end(); ++it) {
			lua_pushstring(lua, it->c_str());
			lua_rawseti(lua, -2, n++);
		}
	}
	lua_setglobal(lua, "arg");
	status = lua_pcall(lua, 0, LUA_MULTRET, 0);
	if (result_code)
		*result_code = to_string(status);
	if (result_string && lua_isstring(lua, -1))
		*result_string = std::string(lua_tostring(lua, -1));
	if (status)
	{
		fprintf(stderr, "[CLuaInstance::%s] error in script: %s\n", __func__, lua_tostring(lua, -1));
		ShowMsg2UTF("Lua script error:", lua_tostring(lua, -1), CMsgBox::mbrBack, CMsgBox::mbBack);
		if (error_string)
			*error_string = std::string(lua_tostring(lua, -1));
	}
}

// Example: runScript(fileName, "Arg1", "Arg2", "Arg3", ..., NULL);
//	Type of all parameters: const char*
//	The last parameter to NULL is imperative.
void CLuaInstance::runScript(const char *fileName, const char *arg0, ...)
{
	int i = 0;
	std::vector<std::string> args;
	args.push_back(arg0);
	va_list list;
	va_start(list, arg0);
	const char* temp = va_arg(list, const char*);
	while (temp != NULL) {
		if (i >= 64) {
			fprintf(stderr, "CLuaInstance::runScript: too many arguments!\n");
			args.clear();
			return;
		}
		args.push_back(temp);
		temp = va_arg(list, const char*);
		i++;
	}
	va_end(list);
	runScript(fileName, &args);
	args.clear();
}

const luaL_Reg CLuaInstance::methods[] =
{
	{ "PaintBox", CLuaInstance::PaintBox },
	{ "RenderString", CLuaInstance::RenderString },
	{ "PaintIcon", CLuaInstance::PaintIcon },
	{ "GetInput", CLuaInstance::GetInput },
	{ "FontHeight", CLuaInstance::FontHeight },
	{ "getRenderWidth", CLuaInstance::getRenderWidth },
	{ "GetSize", CLuaInstance::GetSize },
	{ "DisplayImage", CLuaInstance::DisplayImage },
	{ "setBlank", CLuaInstance::setBlank },
	{ "ShowPicture", CLuaInstance::ShowPicture },
	{ "StopPicture", CLuaInstance::StopPicture },
	{ "Blit", CLuaInstance::Blit },
	{ "GetLanguage", CLuaInstance::GetLanguage },
	{ "runScript", CLuaInstance::runScriptExt },
	{ "PlayFile", CLuaInstance::PlayFile },
	{ "strFind", CLuaInstance::strFind },
	{ "strSub", CLuaInstance::strSub },
	{ NULL, NULL }
};

#ifdef STATIC_LUAPOSIX
/* hack: we link against luaposix, which is included in our
 * custom built lualib */
extern "C" { LUAMOD_API int (luaopen_posix_c) (lua_State *L); }
#endif

/* load basic functions and register our own C callbacks */
void CLuaInstance::registerFunctions()
{
	luaL_openlibs(lua);
	luaopen_table(lua);
	luaopen_io(lua);
	luaopen_string(lua);
	luaopen_math(lua);
	lua_newtable(lua);
	int methodtable = lua_gettop(lua);
	luaL_newmetatable(lua, className);
	int metatable = lua_gettop(lua);
	lua_pushliteral(lua, "__metatable");
	lua_pushvalue(lua, methodtable);
	lua_settable(lua, metatable);

	lua_pushliteral(lua, "__index");
	lua_pushvalue(lua, methodtable);
	lua_settable(lua, metatable);

	lua_pushliteral(lua, "__gc");
	lua_pushcfunction(lua, GCWindow);
	lua_settable(lua, metatable);

	lua_pop(lua, 1);

	luaL_setfuncs(lua, methods, 0);
	lua_pop(lua, 1);

	lua_register(lua, className, NewWindow);
	MenuRegister(lua);
	HintboxRegister(lua);
	MessageboxRegister(lua);
	CWindowRegister(lua);
	ComponentsTextRegister(lua);
	SignalBoxRegister(lua);
	CPictureRegister(lua);
	LuaConfigFileRegister(lua);
}

CLuaData *CLuaInstance::CheckData(lua_State *L, int narg)
{
	luaL_checktype(L, narg, LUA_TUSERDATA);
	void *ud = luaL_checkudata(L, narg, className);
	if (!ud)
		fprintf(stderr, "[CLuaInstance::%s] wrong type %p, %d, %s\n", __func__, L, narg, className);
	return *(CLuaData **)ud;  // unbox pointer
}

int CLuaInstance::NewWindow(lua_State *L)
{
	int count = lua_gettop(L);
	int x = g_settings.screen_StartX;
	int y = g_settings.screen_StartY;
	int w = g_settings.screen_EndX - x;
	int h = g_settings.screen_EndY - y;
	CLuaData *D = new CLuaData();
	if (count > 0)
		x = luaL_checkint(L, 1);
	if (count > 1)
		y = luaL_checkint(L, 2);
	if (count > 2)
		w = luaL_checkint(L, 3);
	if (count > 3)
		h = luaL_checkint(L, 4);
	CFBWindow *W = new CFBWindow(x, y, w, h);
	D->fbwin = W;
	D->rcinput = g_RCInput;
	lua_boxpointer(L, D);
	luaL_getmetatable(L, className);
	lua_setmetatable(L, -2);
	return 1;
}

int CLuaInstance::PaintBox(lua_State *L)
{
	int count = lua_gettop(L);
	DBG("CLuaInstance::%s %d\n", __func__, count);
	int x, y, w, h, radius = 0, corner = CORNER_ALL;
	unsigned int c;

	CLuaData *W = CheckData(L, 1);
	if (!W || !W->fbwin)
		return 0;
	x = luaL_checkint(L, 2);
	y = luaL_checkint(L, 3);
	w = luaL_checkint(L, 4);
	h = luaL_checkint(L, 5);
	/* luaL_checkint does not like e.g. 0xffcc0000 on powerpc (returns INT_MAX) instead */
	c = (unsigned int)luaL_checknumber(L, 6);
	if (count > 6)
		radius = luaL_checkint(L, 7);
	if (count > 7)
		corner = luaL_checkint(L, 8);
	/* those checks should be done in CFBWindow instead... */
	if (x < 0)
		x = 0;
	if (y < 0)
		y = 0;
	if (w < 0 || x + w > W->fbwin->dx)
		w = W->fbwin->dx - x;
	if (h < 0 || y + h > W->fbwin->dy)
		h = W->fbwin->dy - y;
	checkMagicMask(c);
	W->fbwin->paintBoxRel(x, y, w, h, c, radius, corner);
	return 0;
}

int CLuaInstance::PaintIcon(lua_State *L)
{
	DBG("CLuaInstance::%s %d\n", __func__, lua_gettop(L));
	int x, y, h;
	unsigned int o;
	const char *fname;

	CLuaData *W = CheckData(L, 1);
	if (!W || !W->fbwin)
		return 0;
	fname = luaL_checkstring(L, 2);
	x = luaL_checkint(L, 3);
	y = luaL_checkint(L, 4);
	h = luaL_checkint(L, 5);
	o = luaL_checkint(L, 6);
	W->fbwin->paintIcon(fname, x, y, h, o);
	return 0;
}

extern CPictureViewer * g_PicViewer;

int CLuaInstance::DisplayImage(lua_State *L)
{
	DBG("CLuaInstance::%s %d\n", __func__, lua_gettop(L));
	int x, y, w, h;
	const char *fname;

	fname = luaL_checkstring(L, 2);
	x = luaL_checkint(L, 3);
	y = luaL_checkint(L, 4);
	w = luaL_checkint(L, 5);
	h = luaL_checkint(L, 6);
	int trans = 0;
	if (lua_isnumber(L, 7))
		trans = luaL_checkint(L, 7);
	g_PicViewer->DisplayImage(fname, x, y, w, h, trans);
	return 0;
}

extern cVideo * videoDecoder;

int CLuaInstance::setBlank(lua_State *L)
{
	int enable = 1;
	enable = luaL_checkint(L, 2);
	videoDecoder->setBlank((enable==1)?true:false);
	return 0;
}

int CLuaInstance::ShowPicture(lua_State *L)
{
	const char *fname = luaL_checkstring(L, 2);
	videoDecoder->ShowPicture(fname);
	return 0;
}

int CLuaInstance::StopPicture(lua_State */*L*/)
{
	videoDecoder->StopPicture();
	return 0;
}

int CLuaInstance::PlayFile(lua_State *L)
{
	printf("CLuaInstance::%s %d\n", __func__, lua_gettop(L));
	int numargs = lua_gettop(L);

	if (numargs < 3) {
		printf("CLuaInstance::%s: not enough arguments (%d, expected 3)\n", __func__, numargs);
		return 0;
	}
	const char *title;
	const char *info1 = "";
	const char *info2 = "";
	const char *fname;

	title = luaL_checkstring(L, 2);
	fname = luaL_checkstring(L, 3);
	if (numargs > 3)
		info1 = luaL_checkstring(L, 4);
	if (numargs > 4)
		info2 = luaL_checkstring(L, 5);
	printf("CLuaInstance::%s: title %s file %s\n", __func__, title, fname);
	std::string st(title);
	std::string si1(info1);
	std::string si2(info2);
	std::string sf(fname);
	CMoviePlayerGui::getInstance().SetFile(st, sf, si1, si2);
	CMoviePlayerGui::getInstance().exec(NULL, "http");
	return 0;
}

int CLuaInstance::strFind(lua_State *L)
{
	int numargs = lua_gettop(L);
	if (numargs < 3) {
		printf("CLuaInstance::%s: not enough arguments (%d, expected 2 (or 3 or 4))\n", __func__, numargs);
		lua_pushnil(L);
		return 1;
	}
	const char *s1;
	const char *s2;
	int pos=0, n=0, ret=0;
	s1 = luaL_checkstring(L, 2);
	s2 = luaL_checkstring(L, 3);
	if (numargs > 3)
		pos = luaL_checkint(L, 4);
	if (numargs > 4)
		n = luaL_checkint(L, 5);

	std::string str(s1);
	if (numargs > 4)
		ret = str.find(s2, pos, n);
	else
		ret = str.find(s2, pos);

//	printf("####[%s:%d] str_len: %d, s2: %s, pos: %d, n: %d, ret: %d\n", __func__, __LINE__, str.length(), s2, pos, n, ret);
	if (ret == (int)std::string::npos)
		lua_pushnil(L);
	else
		lua_pushinteger(L, ret);
	return 1;
}

int CLuaInstance::strSub(lua_State *L)
{
	int numargs = lua_gettop(L);
	if (numargs < 3) {
		printf("CLuaInstance::%s: not enough arguments (%d, expected 2 (or 3))\n", __func__, numargs);
		lua_pushstring(L, "");
		return 1;
	}
	const char *s1;
	int pos=0, len=std::string::npos;
	std::string ret="";
	s1 = luaL_checkstring(L, 2);
	pos = luaL_checkint(L, 3);
	if (numargs > 3)
		len = luaL_checkint(L, 4);

	std::string str(s1);
	ret = str.substr(pos, len);

//	printf("####[%s:%d] str_len: %d, pos: %d, len: %d, ret_len: %d\n", __func__, __LINE__, str.length(), pos, len, ret.length());
	lua_pushstring(L, ret.c_str());
	return 1;
}

int CLuaInstance::GetSize(lua_State *L)
{
	DBG("CLuaInstance::%s %d\n", __func__, lua_gettop(L));
	int w = 0, h = 0;
	const char *fname;

	fname = luaL_checkstring(L, 2);
	g_PicViewer->getSize(fname, &w, &h);
	lua_pushinteger(L, w);
	lua_pushinteger(L, h);
	return 2;
}

int CLuaInstance::RenderString(lua_State *L)
{
	int x, y, w, boxh, f, center;
	unsigned int c;
	const char *text;
	int numargs = lua_gettop(L);
	DBG("CLuaInstance::%s %d\n", __func__, numargs);
	c = COL_MENUCONTENT_TEXT;
	boxh = 0;
	center = 0;

	CLuaData *W = CheckData(L, 1);
	if (!W || !W->fbwin)
		return 0;
	f = luaL_checkint(L, 2);	/* font number, use FONT_TYPE_XXX in the script */
	text = luaL_checkstring(L, 3);	/* text */
	x = luaL_checkint(L, 4);
	y = luaL_checkint(L, 5);
	if (numargs > 5)
		c = luaL_checkint(L, 6);
	if (numargs > 6)
		w = luaL_checkint(L, 7);
	else
		w = W->fbwin->dx - x;
	if (numargs > 7)
		boxh = luaL_checkint(L, 8);
	if (numargs > 8)
		center = luaL_checkint(L, 9);
	if (f >= SNeutrinoSettings::FONT_TYPE_COUNT || f < 0)
		f = SNeutrinoSettings::FONT_TYPE_MENU;
	int rwidth = g_Font[f]->getRenderWidth(text);
	if (center) { /* center the text inside the box */
		if (rwidth < w)
			x += (w - rwidth) / 2;
	}
	checkMagicMask(c);
	if (boxh > -1) /* if boxh < 0, don't paint string */
		W->fbwin->RenderString(g_Font[f], x, y, w, text, c, boxh);
	lua_pushinteger(L, rwidth); /* return renderwidth */
	return 1;
}

int CLuaInstance::getRenderWidth(lua_State *L)
{
	int f;
	const char *text;
	DBG("CLuaInstance::%s %d\n", __func__, lua_gettop(L));

	CLuaData *W = CheckData(L, 1);
	if (!W)
		return 0;
	f = luaL_checkint(L, 2);	/* font number, use FONT['xxx'] for FONT_TYPE_xxx in the script */
	text = luaL_checkstring(L, 3);	/* text */
	if (f >= SNeutrinoSettings::FONT_TYPE_COUNT || f < 0)
		f = SNeutrinoSettings::FONT_TYPE_MENU;

	lua_pushinteger(L, (int)g_Font[f]->getRenderWidth(text));
	return 1;
}

int CLuaInstance::GetInput(lua_State *L)
{
	int numargs = lua_gettop(L);
	int timeout = 0;
	neutrino_msg_t msg;
	neutrino_msg_data_t data;
	CLuaData *W = CheckData(L, 1);
	if (!W)
		return 0;
	if (numargs > 1)
		timeout = luaL_checkint(L, 2);
	W->rcinput->getMsg_ms(&msg, &data, timeout);
	/* TODO: I'm not sure if this works... */
	if (msg != CRCInput::RC_timeout && msg > CRCInput::RC_MaxRC)
	{
		DBG("CLuaInstance::%s: msg 0x%08" PRIx32 " data 0x%08" PRIx32 "\n", __func__, msg, data);
		CNeutrinoApp::getInstance()->handleMsg(msg, data);
	}
	/* signed int is debatable, but the "big" messages can't yet be handled
	 * inside lua scripts anyway. RC_timeout == -1, RC_nokey == -2 */
	lua_pushinteger(L, (int)msg);
	lua_pushunsigned(L, data);
	return 2;
}

int CLuaInstance::FontHeight(lua_State *L)
{
	int f;
	DBG("CLuaInstance::%s %d\n", __func__, lua_gettop(L));

	CLuaData *W = CheckData(L, 1);
	if (!W)
		return 0;
	f = luaL_checkint(L, 2);	/* font number, use FONT['xxx'] for FONT_TYPE_xxx in the script */
	if (f >= SNeutrinoSettings::FONT_TYPE_COUNT || f < 0)
		f = SNeutrinoSettings::FONT_TYPE_MENU;
	lua_pushinteger(L, (int)g_Font[f]->getHeight());
	return 1;
}

int CLuaInstance::GCWindow(lua_State *L)
{
	DBG("CLuaInstance::%s %d\n", __func__, lua_gettop(L));
	CLuaData *w = (CLuaData *)lua_unboxpointer(L, 1);
	delete w->fbwin;
	w->rcinput = NULL;
	delete w;
	return 0;
}

#if 1
int CLuaInstance::Blit(lua_State *)
{
	return 0;
}
#else
int CLuaInstance::Blit(lua_State *L)
{
	CLuaData *W = CheckData(L, 1);
	if (W && W->fbwin) {
		if (lua_isnumber(L, 2))
			W->fbwin->blit((int)lua_tonumber(L, 2)); // enable/disable automatic blit
		else
			W->fbwin->blit();
	}
	return 0;
}
#endif

int CLuaInstance::GetLanguage(lua_State *L)
{
	// FIXME -- should conform to ISO 639-1/ISO 3166-1
	lua_pushstring(L, g_settings.language.c_str());

	return 1;
}

int CLuaInstance::runScriptExt(lua_State *L)
{
	CLuaData *W = CheckData(L, 1);
	if (!W) return 0;

	int numargs = lua_gettop(L);
	const char *script = luaL_checkstring(L, 2);
	std::vector<std::string> args;
	for (int i = 3; i <= numargs; i++) {
		std::string arg = luaL_checkstring(L, i);
		if (!arg.empty())
			args.push_back(arg);
	}

	CLuaInstance *lua = new CLuaInstance();
	lua->runScript(script, &args);
	args.clear();
	delete lua;
	return 0;
}

bool CLuaInstance::tableLookup(lua_State *L, const char *what, std::string &value)
{
	bool res = false;
	lua_pushstring(L, what);
	lua_gettable(L, -2);
	res = lua_isstring(L, -1);
	if (res)
		value = lua_tostring(L, -1);
	lua_pop(L, 1);
	return res;
}

bool CLuaInstance::tableLookup(lua_State *L, const char *what, lua_Integer &value)
{
	bool res = false;
	lua_pushstring(L, what);
	lua_gettable(L, -2);
	res = lua_isnumber(L, -1);
	if (res)
		value = (lua_Integer) lua_tonumber(L, -1);
	lua_pop(L, 1);
	return res;
}

bool CLuaInstance::tableLookup(lua_State *L, const char *what, lua_Unsigned &value)
{
	bool res = false;
	lua_pushstring(L, what);
	lua_gettable(L, -2);
	res = lua_isnumber(L, -1);
	if (res)
		value = (lua_Unsigned) lua_tonumber(L, -1);
	lua_pop(L, 1);
	return res;
}

bool CLuaInstance::tableLookup(lua_State *L, const char *what, void** value)
{
	bool res = false;
	lua_pushstring(L, what);
	lua_gettable(L, -2);
	res = lua_isuserdata(L, -1);
	if (res)
		*value = lua_unboxpointer(L, -1);
	lua_pop(L, 1);
	return res;
}

bool CLuaMenuChangeObserver::changeNotify(lua_State *L, const std::string &luaAction, const std::string &luaId, void *Data)
{
	const char *optionValue = (const char *) Data;
	lua_pushglobaltable(L);
	lua_getfield(L, -1, luaAction.c_str());
	lua_remove(L, -2);
	lua_pushstring(L, luaId.c_str());
	lua_pushstring(L, optionValue);
	lua_pcall(L, 2 /* two args */, 1 /* one result */, 0);
	double res = lua_isnumber(L, -1) ? lua_tonumber(L, -1) : 0;
	lua_pop(L, 2);
	return (((int)res == menu_return::RETURN_REPAINT) || ((int)res == menu_return::RETURN_EXIT_REPAINT));
}

void CLuaInstance::MenuRegister(lua_State *L)
{
	luaL_Reg meth[] = {
		{ "new", CLuaInstance::MenuNew },
		{ "addKey", CLuaInstance::MenuAddKey },
		{ "addItem", CLuaInstance::MenuAddItem },
		{ "exec", CLuaInstance::MenuExec },
		{ "hide", CLuaInstance::MenuHide },
		{ "__gc", CLuaInstance::MenuDelete },
		{ NULL, NULL }
	};

	luaL_newmetatable(L, "menu");
	luaL_setfuncs(L, meth, 0);
	lua_pushvalue(L, -1);
	lua_setfield(L, -1, "__index");
	lua_setglobal(L, "menu");

	// keep misspelled "menue" for backwards-compatibility
	luaL_newmetatable(L, "menu");
	luaL_setfuncs(L, meth, 0);
	lua_pushvalue(L, -1);
	lua_setfield(L, -1, "__index");
	lua_setglobal(L, "menue");
}

CLuaMenu *CLuaInstance::MenuCheck(lua_State *L, int n)
{
	return *(CLuaMenu **) luaL_checkudata(L, n, "menu");
}

CLuaMenu::CLuaMenu()
{
	m = NULL;
	observ = new CLuaMenuChangeObserver();
}

CLuaMenu::~CLuaMenu()
{
	delete m;
	delete observ;
}

CLuaMenuForwarder::CLuaMenuForwarder(lua_State *_L, std::string _luaAction, std::string _luaId)
{
	L = _L;
	luaAction = _luaAction;
	luaId = _luaId;
}

CLuaMenuForwarder::~CLuaMenuForwarder()
{
}

int CLuaMenuForwarder::exec(CMenuTarget* /*parent*/, const std::string & /*actionKey*/)
{
	int res = menu_return::RETURN_REPAINT;
	if (!luaAction.empty()){
		lua_pushglobaltable(L);
		lua_getfield(L, -1, luaAction.c_str());
		lua_remove(L, -2);
		lua_pushstring(L, luaId.c_str());
		int status = lua_pcall(L, 1 /* one arg */, 1 /* one result */, 0);
		if (status) {
			fprintf(stderr, "[CLuaMenuForwarder::%s] error in script: %s\n", __func__, lua_tostring(L, -1));
			ShowMsg2UTF("Lua script error:", lua_tostring(L, -1), CMsgBox::mbrBack, CMsgBox::mbBack);
		}
		if (lua_isnumber(L, -1))
			res = (int) lua_tonumber(L, -1);
		lua_pop(L, 1);
	}
	return res;
}

CLuaMenuFilebrowser::CLuaMenuFilebrowser(lua_State *_L, std::string _luaAction, std::string _luaId, std::string *_value, bool _dirMode) : CLuaMenuForwarder(_L, _luaAction, _luaId)
{
	value = _value;
	dirMode = _dirMode;
}

int CLuaMenuFilebrowser::exec(CMenuTarget* /*parent*/, const std::string& /*actionKey*/)
{
	CFileBrowser fileBrowser;
	fileBrowser.Dir_Mode = dirMode;

	CFileFilter fileFilter;
	for (std::vector<std::string>::iterator it = filter.begin(); it != filter.end(); ++it)
		fileFilter.addFilter(*it);
	if (!filter.empty())
		fileBrowser.Filter = &fileFilter;

	if (fileBrowser.exec(value->c_str()) == true)
		*value = fileBrowser.getSelectedFile()->Name;

	if (!luaAction.empty()){
		lua_pushglobaltable(L);
		lua_getfield(L, -1, luaAction.c_str());
		lua_remove(L, -2);
		lua_pushstring(L, value->c_str());
		lua_pcall(L, 1 /* one arg */, 1 /* one result */, 0);
		lua_pop(L, 1);
	}
	return menu_return::RETURN_REPAINT;
}

CLuaMenuStringinput::CLuaMenuStringinput(lua_State *_L, std::string _luaAction, std::string _luaId, const char *_name, std::string *_value, int _size, std::string _valid_chars, CChangeObserver *_observ, const char *_icon, bool _sms) : CLuaMenuForwarder(_L, _luaAction, _luaId)
{
	name = _name;
	value = _value;
	size = _size;
	valid_chars = _valid_chars;
	icon = _icon;
	observ = _observ;
	sms = _sms;
}

int CLuaMenuStringinput::exec(CMenuTarget* /*parent*/, const std::string & /*actionKey*/)
{
	CStringInput *i;
	if (sms)
		i = new CStringInputSMS((char *)name, value, size,
			NONEXISTANT_LOCALE, NONEXISTANT_LOCALE, valid_chars.c_str(), observ, icon);
	else
		i = new CStringInput((char *)name, value, size,
			NONEXISTANT_LOCALE, NONEXISTANT_LOCALE, valid_chars.c_str(), observ, icon);
	i->exec(NULL, "");
	delete i;
	if (!luaAction.empty()){
		lua_pushglobaltable(L);
		lua_getfield(L, -1, luaAction.c_str());
		lua_remove(L, -2);
		lua_pushstring(L, luaId.c_str());
		lua_pushstring(L, value->c_str());
		lua_pcall(L, 2 /* two args */, 1 /* one result */, 0);
		lua_pop(L, 2);
	}
	return menu_return::RETURN_REPAINT;
}

int CLuaInstance::MenuNew(lua_State *L)
{
	CMenuWidget *m;

	if (lua_istable(L, 1)) {
		std::string name, icon;
		tableLookup(L, "name", name) || tableLookup(L, "title", name);
		tableLookup(L, "icon", icon);
		int mwidth;
		if(tableLookup(L, "mwidth", mwidth))
			m = new CMenuWidget(name.c_str(), icon.c_str(), mwidth);
		else
			m = new CMenuWidget(name.c_str(), icon.c_str());
	} else
		m = new CMenuWidget();

	CLuaMenu **udata = (CLuaMenu **) lua_newuserdata(L, sizeof(CLuaMenu *));
	*udata = new CLuaMenu();
	(*udata)->m = m;
	luaL_getmetatable(L, "menu");
	lua_setmetatable(L, -2);
	return 1;
}

int CLuaInstance::MenuDelete(lua_State *L)
{
	CLuaMenu *m = MenuCheck(L, 1);
	if (!m)
		return 0;

	while(!m->targets.empty()) {
		delete m->targets.back();
		m->targets.pop_back();
	}
	while(!m->tofree.empty()) {
		free(m->tofree.back());
		m->tofree.pop_back();
	}

	delete m;
	return 0;
}

int CLuaInstance::MenuAddKey(lua_State *L)
{
	CLuaMenu *m = MenuCheck(L, 1);
	if (!m)
		return 0;
	lua_assert(lua_istable(L, 2));

	std::string action;	tableLookup(L, "action", action);
	std::string id;		tableLookup(L, "id", id);
	int directkey = CRCInput::RC_nokey; tableLookup(L, "directkey", directkey);
	if (action != "" && directkey != (int) CRCInput::RC_nokey) {
		CLuaMenuForwarder *forwarder = new CLuaMenuForwarder(L, action, id);
		m->m->addKey(directkey, forwarder, action);
		m->targets.push_back(forwarder);
	}
	return 0;
}

int CLuaInstance::MenuAddItem(lua_State *L)
{
	CLuaMenu *m = MenuCheck(L, 1);
	if (!m)
		return 0;
	lua_assert(lua_istable(L, 2));

	CLuaMenuItem i;
	m->items.push_back(i);
	CLuaMenuItem *b = &m->items.back();

	tableLookup(L, "name", b->name);
	std::string type;	tableLookup(L, "type", type);
	if (type == "back") {
		m->m->addItem(GenericMenuBack);
	} else if (type == "next") {
		m->m->addItem(GenericMenuNext);
	} else if (type == "cancel") {
		m->m->addItem(GenericMenuCancel);
	} else if (type == "separator") {
		m->m->addItem(GenericMenuSeparator);
	} else if ((type == "separatorline") || (type == "subhead")) {
		if (!b->name.empty()) {
			int flag = (type == "separatorline") ? CMenuSeparator::LINE : CMenuSeparator::SUB_HEAD;
			m->m->addItem(new CMenuSeparator(CMenuSeparator::STRING | flag, b->name.c_str(), NONEXISTANT_LOCALE));
		} else
			m->m->addItem(GenericMenuSeparatorLine);
	} else {
		std::string right_icon_str;	tableLookup(L, "right_icon", right_icon_str);
		std::string action;	tableLookup(L, "action", action);
		std::string value;	tableLookup(L, "value", value);
		std::string hint;	tableLookup(L, "hint", hint);
		std::string hint_icon_str;	tableLookup(L, "hint_icon", hint_icon_str);
		std::string icon_str;	tableLookup(L, "icon", icon_str);
		std::string id;		tableLookup(L, "id", id);
		std::string tmp;
                char *right_icon = NULL;
                if (!right_icon_str.empty()) {
                        right_icon = strdup(right_icon_str.c_str());
                        m->tofree.push_back(right_icon);
                }
                char *hint_icon = NULL;
                if (!hint_icon_str.empty()) {
                        hint_icon = strdup(hint_icon_str.c_str());
                        m->tofree.push_back(hint_icon);
                }
                char *icon = NULL;
                if (!icon_str.empty()) {
                        icon = strdup(icon_str.c_str());
                        m->tofree.push_back(icon);
                }

		int directkey = CRCInput::RC_nokey; tableLookup(L, "directkey", directkey);
		int pulldown = false; 	tableLookup(L, "pulldown", pulldown);
		tmp = "true";
		tableLookup(L, "enabled", tmp) || tableLookup(L, "active", tmp);
		bool enabled = (tmp == "true" || tmp == "1" || tmp == "yes");
		tableLookup(L, "range", tmp);
		int range_from = 0, range_to = 99;
		sscanf(tmp.c_str(), "%d,%d", &range_from, &range_to);

		CMenuItem *mi = NULL;

		if (type == "forwarder") {
			b->str_val = value;
			CLuaMenuForwarder *forwarder = new CLuaMenuForwarder(L, action, id);
			mi = new CMenuForwarder(b->name, enabled, b->str_val, forwarder, NULL/*ActionKey*/, directkey, icon, right_icon);
			if (!hint.empty() || hint_icon)
				mi->setHint(hint_icon, hint);
			m->targets.push_back(forwarder);
		} else if (type == "chooser") {
			int options_count = 0;
			lua_pushstring(L, "options");
			lua_gettable(L, -2);
			if (lua_istable(L, -1))
				for (lua_pushnil(L); lua_next(L, -2); lua_pop(L, 2)) {
					lua_pushvalue(L, -2);
					options_count++;
				}
			lua_pop(L, 1);
			if (options_count == 0) {
				m->m->addItem(new CMenuSeparator(CMenuSeparator::STRING | CMenuSeparator::LINE, "ERROR! (options_count)", NONEXISTANT_LOCALE));
				return 0;
			}

			CMenuOptionChooser::keyval_ext *kext = (CMenuOptionChooser::keyval_ext *)calloc(options_count, sizeof(CMenuOptionChooser::keyval_ext));
			m->tofree.push_back(kext);
			lua_pushstring(L, "options");
			lua_gettable(L, -2);
			b->int_val = 0;
			int j = 0;
			if (lua_istable(L, -1))
				for (lua_pushnil(L); lua_next(L, -2); lua_pop(L, 2)) {
					lua_pushvalue(L, -2);
					const char *key = lua_tostring(L, -1);
					const char *val = lua_tostring(L, -2);
					kext[j].key = atoi(key);
					kext[j].value = NONEXISTANT_LOCALE;
					kext[j].valname = strdup(val);
					m->tofree.push_back((void *)kext[j].valname);
					if (!strcmp(value.c_str(), kext[j].valname))
						b->int_val = kext[j].key;
					j++;
				}
			lua_pop(L, 1);
			mi = new CMenuOptionChooser(b->name.c_str(), &b->int_val, kext, options_count, enabled, m->observ, directkey, icon, pulldown);
		} else if (type == "numeric") {
			b->int_val = range_from;
			sscanf(value.c_str(), "%d", &b->int_val);
			mi = new CMenuOptionNumberChooser(b->name, &b->int_val, enabled, range_from, range_to, m->observ, 0, 0, NONEXISTANT_LOCALE, pulldown);
		} else if (type == "string") {
			b->str_val = value;
			mi = new CMenuOptionStringChooser(b->name.c_str(), &b->str_val, enabled, m->observ, directkey, icon, pulldown);
		} else if (type == "stringinput") {
			b->str_val = value;
			std::string valid_chars = "abcdefghijklmnopqrstuvwxyz0123456789!\"§$%&/()=?-. ";
			tableLookup(L, "valid_chars", valid_chars);
			int sms = 0;	tableLookup(L, "sms", sms);
			int size = 30;	tableLookup(L, "size", size);
			CLuaMenuStringinput *stringinput = new CLuaMenuStringinput(L, action, id, b->name.c_str(), &b->str_val, size, valid_chars, m->observ, icon, sms);
			mi = new CMenuForwarder(b->name, enabled, b->str_val, stringinput, NULL/*ActionKey*/, directkey, icon, right_icon);
			m->targets.push_back(stringinput);
		} else if (type == "filebrowser") {
			b->str_val = value;
			int dirMode = 0; tableLookup(L, "dir_mode", dirMode);
			CLuaMenuFilebrowser *filebrowser = new CLuaMenuFilebrowser(L, action, id, &b->str_val, dirMode);
			lua_pushstring(L, "filter");
			lua_gettable(L, -2);
			if (lua_istable(L, -1))
				for (lua_pushnil(L); lua_next(L, -2); lua_pop(L, 2)) {
					lua_pushvalue(L, -2);
					const char *val = lua_tostring(L, -2);
					filebrowser->addFilter(val);
				}
			lua_pop(L, 1);

			mi = new CMenuForwarder(b->name, enabled, b->str_val, filebrowser, NULL/*ActionKey*/, directkey, icon, right_icon);
			m->targets.push_back(filebrowser);
		}
		if (mi) {
			mi->setLua(L, action, id);
			if (!hint.empty() || hint_icon)
				mi->setHint(hint_icon, hint);
			m->m->addItem(mi);
		}
	}
	return 0;
}

int CLuaInstance::MenuHide(lua_State *L)
{
	CLuaMenu *m = MenuCheck(L, 1);
	if (!m)
		return 0;
	m->m->hide();
	return 0;
}

int CLuaInstance::MenuExec(lua_State *L)
{
	CLuaMenu *m = MenuCheck(L, 1);
	if (!m)
		return 0;
	m->m->exec(NULL, "");
	m->m->hide();
	return 0;
}

void CLuaInstance::HintboxRegister(lua_State *L)
{
	luaL_Reg meth[] = {
		{ "new", CLuaInstance::HintboxNew },
		{ "exec", CLuaInstance::HintboxExec },
		{ "paint", CLuaInstance::HintboxPaint },
		{ "hide", CLuaInstance::HintboxHide },
		{ "__gc", CLuaInstance::HintboxDelete },
		{ NULL, NULL }
	};

	luaL_newmetatable(L, "hintbox");
	luaL_setfuncs(L, meth, 0);
	lua_pushvalue(L, -1);
	lua_setfield(L, -1, "__index");
	lua_setglobal(L, "hintbox");
}

CLuaHintbox *CLuaInstance::HintboxCheck(lua_State *L, int n)
{
	return *(CLuaHintbox **) luaL_checkudata(L, n, "hintbox");
}

CLuaHintbox::CLuaHintbox()
{
	caption = NULL;
	b = NULL;
}

CLuaHintbox::~CLuaHintbox()
{
	if (caption)
		free(caption);
	delete b;
}

int CLuaInstance::HintboxNew(lua_State *L)
{
	lua_assert(lua_istable(L,1));

	std::string name, text, icon = std::string(NEUTRINO_ICON_INFO);
	tableLookup(L, "name", name) || tableLookup(L, "title", name) || tableLookup(L, "caption", name);
	tableLookup(L, "text", text);
	tableLookup(L, "icon", icon);
	int width = 450;
	tableLookup(L, "width", width);

	CLuaHintbox **udata = (CLuaHintbox **) lua_newuserdata(L, sizeof(CLuaHintbox *));
	*udata = new CLuaHintbox();
	(*udata)->caption = strdup(name.c_str());
	(*udata)->b = new CHintBox((*udata)->caption, text.c_str(), width, icon.c_str());
	luaL_getmetatable(L, "hintbox");
	lua_setmetatable(L, -2);
	return 1;
}

int CLuaInstance::HintboxDelete(lua_State *L)
{
	CLuaHintbox *m = HintboxCheck(L, 1);
	delete m;
	return 0;
}

int CLuaInstance::HintboxPaint(lua_State *L)
{
	CLuaHintbox *m = HintboxCheck(L, 1);
	if (!m)
		return 0;
	m->b->paint();
	return 0;
}

int CLuaInstance::HintboxHide(lua_State *L)
{
	CLuaHintbox *m = HintboxCheck(L, 1);
	m->b->hide();
	return 0;
}

int CLuaInstance::HintboxExec(lua_State *L)
{
	CLuaHintbox *m = HintboxCheck(L, 1);
	if (!m)
		return 0;
	int timeout = -1;
	if (lua_isnumber(L, -1))
		timeout = (int) lua_tonumber(L, -1);
	m->b->paint();

	// copied from gui/widget/hintbox.cpp

	neutrino_msg_t msg;
	neutrino_msg_data_t data;
	if ( timeout == -1 )
		timeout = 5; /// default timeout 5 sec
		//timeout = g_settings.timing[SNeutrinoSettings::TIMING_INFOBAR];

	uint64_t timeoutEnd = CRCInput::calcTimeoutEnd( timeout );

	int res = messages_return::none;

	while ( ! ( res & ( messages_return::cancel_info | messages_return::cancel_all ) ) )
	{
		g_RCInput->getMsgAbsoluteTimeout( &msg, &data, &timeoutEnd );

		if ((msg == CRCInput::RC_timeout) || (msg == CRCInput::RC_ok))
			res = messages_return::cancel_info;
		else if(msg == CRCInput::RC_home)
			res = messages_return::cancel_all;
		else if ((m->b->has_scrollbar()) && ((msg == CRCInput::RC_up) || (msg == CRCInput::RC_down)))
		{
			if (msg == CRCInput::RC_up)
				m->b->scroll_up();
			else
				m->b->scroll_down();
		}
		else if((msg == CRCInput::RC_sat) || (msg == CRCInput::RC_favorites)) {
		}
		else if(msg == CRCInput::RC_mode) {
			res = messages_return::handled;
			break;
		}
		else if((msg == CRCInput::RC_next) || (msg == CRCInput::RC_prev)) {
			res = messages_return::cancel_all;
			g_RCInput->postMsg(msg, data);
		}
		else
		{
			res = CNeutrinoApp::getInstance()->handleMsg(msg, data);
			if (res & messages_return::unhandled)
			{

				// leave here and handle above...
				g_RCInput->postMsg(msg, data);
				res = messages_return::cancel_all;
			}
		}
	}
	m->b->hide();
	return 0;
}

void CLuaInstance::MessageboxRegister(lua_State *L)
{
	luaL_Reg meth[] = {
		{ "exec", CLuaInstance::MessageboxExec },
		{ NULL, NULL }
	};

	luaL_newmetatable(L, "messagebox");
	luaL_setfuncs(L, meth, 0);
	lua_pushvalue(L, -1);
	lua_setfield(L, -1, "__index");
	lua_setglobal(L, "messagebox");
}

// messagebox.exec{caption="Title", text="text", icon="settings", width=500,timeout=-1,return_default_on_timeout=0,
//	default = "yes", buttons = { "yes", "no", "cancel", "all", "back", "ok" }, align="center1|center2|left|right" }
int CLuaInstance::MessageboxExec(lua_State *L)
{
	lua_assert(lua_istable(L,1));

	std::string name, text, icon = std::string(NEUTRINO_ICON_INFO);
	tableLookup(L, "name", name) || tableLookup(L, "title", name) || tableLookup(L, "caption", name);
	tableLookup(L, "text", text);
	tableLookup(L, "icon", icon);
	int timeout = -1, width = 450, return_default_on_timeout = 0, show_buttons = 0, default_button = 0;
	tableLookup(L, "timeout", timeout);
	tableLookup(L, "width", width);
	tableLookup(L, "return_default_on_timeout", return_default_on_timeout);

	std::string tmp;
	if (tableLookup(L, "align", tmp)) {
		lua_pushvalue(L, -2);
		const char *val = lua_tostring(L, -2);
		table_key mb[] = {
			{ "center1",	CMessageBox::mbBtnAlignCenter1 },
			{ "center2",	CMessageBox::mbBtnAlignCenter2 },
			{ "left",	CMessageBox::mbBtnAlignLeft },
			{ "right",	CMessageBox::mbBtnAlignRight },
			{ NULL,		0 }
		};
		for (int i = 0; mb[i].name; i++)
			if (!strcmp(mb[i].name, val)) {
				show_buttons |= mb[i].code;
				break;
			}
	}
	lua_pushstring(L, "buttons");
	lua_gettable(L, -2);
	for (lua_pushnil(L); lua_next(L, -2); lua_pop(L, 2)) {
		lua_pushvalue(L, -2);
		const char *val = lua_tostring(L, -2);
		table_key mb[] = {
			{ "yes",	CMessageBox::mbYes },
			{ "no",		CMessageBox::mbNo },
			{ "cancel",	CMessageBox::mbCancel },
			{ "all",	CMessageBox::mbAll },
			{ "back",	CMessageBox::mbBack },
			{ "ok",		CMessageBox::mbOk },
			{ NULL,		0 }
		};
		for (int i = 0; mb[i].name; i++)
			if (!strcmp(mb[i].name, val)) {
				show_buttons |= mb[i].code;
				break;
			}
	}
	lua_pop(L, 1);

	table_key mbr[] = {
		{ "yes",	CMessageBox::mbrYes },
		{ "no",		CMessageBox::mbrNo },
		{ "cancel",	CMessageBox::mbrCancel },
		{ "back",	CMessageBox::mbrBack },
		{ "ok",		CMessageBox::mbrOk },
		{ NULL,		0 }
	};
	if (tableLookup(L, "default", tmp)) {
		lua_pushvalue(L, -2);
		const char *val = lua_tostring(L, -2);
		for (int i = 0; mbr[i].name; i++)
			if (!strcmp(mbr[i].name, val)) {
				default_button = mbr[i].code;
				break;
			}
	}

	int res = ShowMsg(name, text, (CMessageBox::result_) default_button, (CMessageBox::buttons_) show_buttons, icon.empty() ? NULL : icon.c_str(), width, timeout, return_default_on_timeout);

	tmp = "cancel";
	for (int i = 0; mbr[i].name; i++)
		if (res == mbr[i].code) {
			tmp = mbr[i].name;
			break;
		}
	lua_pushstring(L, tmp.c_str());

	return 1;
}

// --------------------------------------------------------------------------------

void CLuaInstance::CWindowRegister(lua_State *L)
{
	luaL_Reg meth[] = {
		{ "new", CLuaInstance::CWindowNew },
		{ "paint", CLuaInstance::CWindowPaint },
		{ "hide", CLuaInstance::CWindowHide },
		{ "setCaption", CLuaInstance::CWindowSetCaption },
		{ "paintHeader", CLuaInstance::CWindowPaintHeader },
		{ "headerHeight", CLuaInstance::CWindowGetHeaderHeight },
		{ "footerHeight", CLuaInstance::CWindowGetFooterHeight },
		{ "header_height", CLuaInstance::CWindowGetHeaderHeight_dep }, /* function 'header_height' is deprecated */
		{ "footer_height", CLuaInstance::CWindowGetFooterHeight_dep }, /* function 'footer_height' is deprecated */
		{ "__gc", CLuaInstance::CWindowDelete },
		{ NULL, NULL }
	};

	luaL_newmetatable(L, "cwindow");
	luaL_setfuncs(L, meth, 0);
	lua_pushvalue(L, -1);
	lua_setfield(L, -1, "__index");
	lua_setglobal(L, "cwindow");
}

int CLuaInstance::CWindowNew(lua_State *L)
{
	lua_assert(lua_istable(L,1));

	std::string name, icon   = std::string(NEUTRINO_ICON_INFO);
	lua_Unsigned color_frame  = (lua_Unsigned)COL_MENUCONTENT_PLUS_6;
	lua_Unsigned color_body   = (lua_Unsigned)COL_MENUCONTENT_PLUS_0;
	lua_Unsigned color_shadow = (lua_Unsigned)COL_MENUCONTENTDARK_PLUS_0;
	std::string tmp1         = "false";
	std::string btnRed       = "";
	std::string btnGreen     = "";
	std::string btnYellow    = "";
	std::string btnBlue      = "";
	int x = 100, y = 100, dx = 450, dy = 250;
	tableLookup(L, "x", x);
	tableLookup(L, "y", y);
	tableLookup(L, "dx", dx);
	tableLookup(L, "dy", dy);
	tableLookup(L, "name", name) || tableLookup(L, "title", name) || tableLookup(L, "caption", name);
	tableLookup(L, "icon", icon);
	tableLookup(L, "has_shadow"  , tmp1);
	bool has_shadow = (tmp1 == "true" || tmp1 == "1" || tmp1 == "yes");
	tableLookup(L, "color_frame" , color_frame);
	tableLookup(L, "color_body"  , color_body);
	tableLookup(L, "color_shadow", color_shadow);
	tableLookup(L, "btnRed", btnRed);
	tableLookup(L, "btnGreen", btnGreen);
	tableLookup(L, "btnYellow", btnYellow);
	tableLookup(L, "btnBlue", btnBlue);

	checkMagicMask(color_frame);
	checkMagicMask(color_body);
	checkMagicMask(color_shadow);

	tmp1 = "true";
	tableLookup(L, "show_header"  , tmp1);
	bool show_header = (tmp1 == "true" || tmp1 == "show" || tmp1 == "yes");
	tmp1 = "true";
	tableLookup(L, "show_footer"  , tmp1);
	bool show_footer = (tmp1 == "true" || tmp1 == "show" || tmp1 == "yes");

	CLuaCWindow **udata = (CLuaCWindow **) lua_newuserdata(L, sizeof(CLuaCWindow *));
	*udata = new CLuaCWindow();
	(*udata)->w = new CComponentsWindow(x, y, dx, dy, name.c_str(), icon.c_str(), 0, has_shadow, (fb_pixel_t)color_frame, (fb_pixel_t)color_body, (fb_pixel_t)color_shadow);

	if (!show_header)
		(*udata)->w->showHeader(false);
	if (!show_footer)
		(*udata)->w->showFooter(false);
	else {
		CComponentsFooter* footer = (*udata)->w->getFooterObject();
		if (footer) {
			int btnCount = 0;
			if (btnRed    != "") btnCount++;
			if (btnGreen  != "") btnCount++;
			if (btnYellow != "") btnCount++;
			if (btnBlue   != "") btnCount++;
			if (btnCount) {
				fb_pixel_t col = footer->getColorBody();
				int btnw = (dx-20) / btnCount;
				int btnh = footer->getHeight();
				int start = 10;
				if (btnRed != "")
					footer->addCCItem(new CComponentsButtonRed(start, CC_CENTERED, btnw, btnh, btnRed, 0, false , true, false, col, col));
				if (btnGreen != "")
					footer->addCCItem(new CComponentsButtonGreen(start+=btnw, CC_CENTERED, btnw, btnh, btnGreen, 0, false , true, false, col, col));
				if (btnYellow != "")
					footer->addCCItem(new CComponentsButtonYellow(start+=btnw, CC_CENTERED, btnw, btnh, btnYellow, 0, false , true, false, col, col));
				if (btnBlue != "")
					footer->addCCItem(new CComponentsButtonBlue(start+=btnw, CC_CENTERED, btnw, btnh, btnBlue, 0, false , true, false, col, col));
			}
		}
	}

	luaL_getmetatable(L, "cwindow");
	lua_setmetatable(L, -2);
	return 1;
}

CLuaCWindow *CLuaInstance::CWindowCheck(lua_State *L, int n)
{
	return *(CLuaCWindow **) luaL_checkudata(L, n, "cwindow");
}

int CLuaInstance::CWindowPaint(lua_State *L)
{
	lua_assert(lua_istable(L,1));
	std::string tmp = "true";
	tableLookup(L, "do_save_bg", tmp);
	bool do_save_bg = (tmp == "true" || tmp == "1" || tmp == "yes");

	CLuaCWindow *m = CWindowCheck(L, 1);
	if (!m)
		return 0;

	m->w->paint(do_save_bg);
	return 0;
}

int CLuaInstance::CWindowHide(lua_State *L)
{
	lua_assert(lua_istable(L,1));
	std::string tmp = "false";
	tableLookup(L, "no_restore", tmp);
	bool no_restore = (tmp == "true" || tmp == "1" || tmp == "yes");

	CLuaCWindow *m = CWindowCheck(L, 1);
	if (!m)
		return 0;

	m->w->hide(no_restore);
	return 0;
}

int CLuaInstance::CWindowSetCaption(lua_State *L)
{
	lua_assert(lua_istable(L,1));
	CLuaCWindow *m = CWindowCheck(L, 1);
	if (!m) return 0;

	std::string name = "";
	tableLookup(L, "name", name) || tableLookup(L, "title", name) || tableLookup(L, "caption", name);

	m->w->setWindowCaption(name);
	return 0;
}

int CLuaInstance::CWindowPaintHeader(lua_State *L)
{
	CLuaCWindow *m = CWindowCheck(L, 1);
	if (!m) return 0;

	CComponentsHeader* header = m->w->getHeaderObject();
	if (header)
		m->w->showHeader();
		header->paint();

	return 0;
}

// function 'header_height' is deprecated
int CLuaInstance::CWindowGetHeaderHeight_dep(lua_State *L)
{
	functionDeprecated(L, "header_height", "headerHeight");
	return CWindowGetHeaderHeight(L);
}

// function 'footer_height' is deprecated
int CLuaInstance::CWindowGetFooterHeight_dep(lua_State *L)
{
	functionDeprecated(L, "footer_height", "footerHeight");
	return CWindowGetFooterHeight(L);
}

int CLuaInstance::CWindowGetHeaderHeight(lua_State *L)
{
	CLuaCWindow *m = CWindowCheck(L, 1);
	if (!m)
		return 0;

	CComponentsHeader* header = m->w->getHeaderObject();
	int hh = 0;
	if (header)
		hh = header->getHeight();
	lua_pushinteger(L, hh);
	return 1;
}

int CLuaInstance::CWindowGetFooterHeight(lua_State *L)
{
	CLuaCWindow *m = CWindowCheck(L, 1);
	if (!m)
		return 0;

	CComponentsFooter* footer = m->w->getFooterObject();
	int fh = 0;
	if (footer)
		fh = footer->getHeight();
	lua_pushinteger(L, fh);
	return 1;
}

int CLuaInstance::CWindowDelete(lua_State *L)
{
	DBG("CLuaInstance::%s %d\n", __func__, lua_gettop(L));
	CLuaCWindow *m = CWindowCheck(L, 1);
	if (!m)
		return 0;

	m->w->hide();
	delete m;
	return 0;
}

// --------------------------------------------------------------------------------

CLuaSignalBox *CLuaInstance::SignalBoxCheck(lua_State *L, int n)
{
	return *(CLuaSignalBox **) luaL_checkudata(L, n, "signalbox");
}

void CLuaInstance::SignalBoxRegister(lua_State *L)
{
	luaL_Reg meth[] = {
		{ "new", CLuaInstance::SignalBoxNew },
		{ "paint", CLuaInstance::SignalBoxPaint },
		{ "__gc", CLuaInstance::SignalBoxDelete },
		{ NULL, NULL }
	};

	luaL_newmetatable(L, "signalbox");
	luaL_setfuncs(L, meth, 0);
	lua_pushvalue(L, -1);
	lua_setfield(L, -1, "__index");
	lua_setglobal(L, "signalbox");
}

int CLuaInstance::SignalBoxNew(lua_State *L)
{
	lua_assert(lua_istable(L,1));

	std::string name, icon = std::string(NEUTRINO_ICON_INFO);
	int x = 110, y = 150, dx = 430, dy = 150;
	int vertical = true;
	CLuaCWindow* parent = NULL;
	tableLookup(L, "x", x);
	tableLookup(L, "y", y);
	tableLookup(L, "dx", dx);
	tableLookup(L, "dy", dy);
	tableLookup(L, "vertical", vertical);
	tableLookup(L, "parent", (void**)&parent);

	CComponentsForm* pw = (parent && parent->w) ? parent->w->getBodyObject() : NULL;
	CLuaSignalBox **udata = (CLuaSignalBox **) lua_newuserdata(L, sizeof(CLuaSignalBox *));
	*udata = new CLuaSignalBox();
	(*udata)->s = new CSignalBox(x, y, dx, dy, NULL, (vertical!=0)?true:false, pw);
	(*udata)->parent = pw;
	luaL_getmetatable(L, "signalbox");
	lua_setmetatable(L, -2);
	return 1;
}

int CLuaInstance::SignalBoxPaint(lua_State *L)
{
	lua_assert(lua_istable(L,1));
	std::string tmp = "true";
	tableLookup(L, "do_save_bg", tmp);
	bool do_save_bg = (tmp == "true" || tmp == "1" || tmp == "yes");

	CLuaSignalBox *m = SignalBoxCheck(L, 1);
	if (!m)
		return 0;

	m->s->paint(do_save_bg);
	return 0;
}

int CLuaInstance::SignalBoxDelete(lua_State *L)
{
	CLuaSignalBox *m = SignalBoxCheck(L, 1);
	if (!m)
		return 0;

	m->s->kill();
	delete m;
	return 0;
}

// --------------------------------------------------------------------------------

CLuaComponentsText *CLuaInstance::ComponentsTextCheck(lua_State *L, int n)
{
	return *(CLuaComponentsText **) luaL_checkudata(L, n, "ctext");
}

void CLuaInstance::ComponentsTextRegister(lua_State *L)
{
	luaL_Reg meth[] = {
		{ "new", CLuaInstance::ComponentsTextNew },
		{ "paint", CLuaInstance::ComponentsTextPaint },
		{ "hide", CLuaInstance::ComponentsTextHide },
		{ "setText", CLuaInstance::ComponentsTextSetText },
		{ "scroll", CLuaInstance::ComponentsTextScroll },
		{ "__gc", CLuaInstance::ComponentsTextDelete },
		{ NULL, NULL }
	};

	luaL_newmetatable(L, "ctext");
	luaL_setfuncs(L, meth, 0);
	lua_pushvalue(L, -1);
	lua_setfield(L, -1, "__index");
	lua_setglobal(L, "ctext");
}

int CLuaInstance::ComponentsTextNew(lua_State *L)
{
	lua_assert(lua_istable(L,1));

	CLuaCWindow* parent = NULL;
	int x=10, y=10, dx=100, dy=100;
	std::string text         = "";
	std::string tmpMode      = "";
	int         mode         = CTextBox::AUTO_WIDTH;
	int         font_text    = SNeutrinoSettings::FONT_TYPE_MENU;
	lua_Unsigned color_text   = (lua_Unsigned)COL_MENUCONTENT_TEXT;
	lua_Unsigned color_frame  = (lua_Unsigned)COL_MENUCONTENT_PLUS_6;
	lua_Unsigned color_body   = (lua_Unsigned)COL_MENUCONTENT_PLUS_0;
	lua_Unsigned color_shadow = (lua_Unsigned)COL_MENUCONTENTDARK_PLUS_0;
	std::string tmp1         = "false";

	tableLookup(L, "parent"      , (void**)&parent);
	tableLookup(L, "x"           , x);
	tableLookup(L, "y"           , y);
	tableLookup(L, "dx"          , dx);
	tableLookup(L, "dy"          , dy);
	tableLookup(L, "text"        , text);
	tableLookup(L, "mode"        , tmpMode);
	tableLookup(L, "font_text"   , font_text);
	if (font_text >= SNeutrinoSettings::FONT_TYPE_COUNT || font_text < 0)
		font_text = SNeutrinoSettings::FONT_TYPE_MENU;
	tableLookup(L, "has_shadow"  , tmp1);
	bool has_shadow = (tmp1 == "true" || tmp1 == "1" || tmp1 == "yes");
	tableLookup(L, "color_text"  , color_text);
	tableLookup(L, "color_frame" , color_frame);
	tableLookup(L, "color_body"  , color_body);
	tableLookup(L, "color_shadow", color_shadow);

	checkMagicMask(color_text);
	checkMagicMask(color_frame);
	checkMagicMask(color_body);
	checkMagicMask(color_shadow);

	if (!tmpMode.empty()) {
		table_key txt_align[] = {
			{ "ALIGN_AUTO_WIDTH",		CTextBox::AUTO_WIDTH },
			{ "ALIGN_AUTO_HIGH",		CTextBox::AUTO_HIGH },
			{ "ALIGN_SCROLL",		CTextBox::SCROLL },
			{ "ALIGN_CENTER",		CTextBox::CENTER },
			{ "ALIGN_RIGHT",		CTextBox::RIGHT },
			{ "ALIGN_TOP",			CTextBox::TOP },
			{ "ALIGN_BOTTOM",		CTextBox::BOTTOM },
			{ "ALIGN_NO_AUTO_LINEBREAK",	CTextBox::NO_AUTO_LINEBREAK },
			{ "DECODE_HTML",		0 },
			{ NULL,				0 }
		};
		mode = 0;
		for (int i = 0; txt_align[i].name; i++) {
			if (tmpMode.find(txt_align[i].name) != std::string::npos)
				mode |= txt_align[i].code;
		}
		if (tmpMode.find("DECODE_HTML") != std::string::npos)
			htmlEntityDecode(text);
	}

	CComponentsForm* pw = (parent && parent->w) ? parent->w->getBodyObject() : NULL;

	CLuaComponentsText **udata = (CLuaComponentsText **) lua_newuserdata(L, sizeof(CLuaComponentsText *));
	*udata = new CLuaComponentsText();
	(*udata)->ct = new CComponentsText(x, y, dx, dy, text, mode, g_Font[font_text], pw, has_shadow, (fb_pixel_t)color_text, (fb_pixel_t)color_frame, (fb_pixel_t)color_body, (fb_pixel_t)color_shadow);
	(*udata)->parent = pw;
	(*udata)->mode = mode;
	(*udata)->font_text = font_text;
	luaL_getmetatable(L, "ctext");
	lua_setmetatable(L, -2);
	return 1;
}

int CLuaInstance::ComponentsTextPaint(lua_State *L)
{
	lua_assert(lua_istable(L,1));
	CLuaComponentsText *m = ComponentsTextCheck(L, 1);
	if (!m) return 0;

	std::string tmp = "true";
	tableLookup(L, "do_save_bg", tmp);
	bool do_save_bg = (tmp == "true" || tmp == "1" || tmp == "yes");

	m->ct->paint(do_save_bg);
	return 0;
}

int CLuaInstance::ComponentsTextHide(lua_State *L)
{
	lua_assert(lua_istable(L,1));
	CLuaComponentsText *m = ComponentsTextCheck(L, 1);
	if (!m) return 0;

	std::string tmp = "false";
	tableLookup(L, "no_restore", tmp);
	bool no_restore = (tmp == "true" || tmp == "1" || tmp == "yes");

	if (m->parent) {
		m->ct->setText("", m->mode, g_Font[m->font_text]);
		m->ct->paint();
	} else
		m->ct->hide(no_restore);
	return 0;
}

int CLuaInstance::ComponentsTextSetText(lua_State *L)
{
	lua_assert(lua_istable(L,1));
	CLuaComponentsText *m = ComponentsTextCheck(L, 1);
	if (!m) return 0;

	std::string text = "";
	lua_Integer mode = m->mode;
	lua_Integer font_text = m->font_text;
	tableLookup(L, "text", text);
	tableLookup(L, "mode", mode);
	tableLookup(L, "font_text", font_text);

	m->ct->setText(text, mode, g_Font[font_text]);
	return 0;
}

int CLuaInstance::ComponentsTextScroll(lua_State *L)
{
	lua_assert(lua_istable(L,1));
	CLuaComponentsText *m = ComponentsTextCheck(L, 1);
	if (!m) return 0;

	std::string tmp = "true";
	tableLookup(L, "dir", tmp);
	bool scrollDown = (tmp == "down" || tmp == "1");

	//get the textbox instance from lua object and use CTexBbox scroll methods
	CTextBox* ctb = m->ct->getCTextBoxObject();
	if (ctb) {
		ctb->enableBackgroundPaint(true);
		if (scrollDown)
			ctb->scrollPageDown(1);
		else
			ctb->scrollPageUp(1);
		ctb->enableBackgroundPaint(false);
	}
	return 0;
}

int CLuaInstance::ComponentsTextDelete(lua_State *L)
{
	DBG("CLuaInstance::%s %d\n", __func__, lua_gettop(L));
	CLuaComponentsText *m = ComponentsTextCheck(L, 1);
	if (!m)
		return 0;

	m->ct->hide();
	delete m;
	return 0;
}

// --------------------------------------------------------------------------------

CLuaPicture *CLuaInstance::CPictureCheck(lua_State *L, int n)
{
	return *(CLuaPicture **) luaL_checkudata(L, n, "cpicture");
}

void CLuaInstance::CPictureRegister(lua_State *L)
{
	luaL_Reg meth[] = {
		{ "new", CLuaInstance::CPictureNew },
		{ "paint", CLuaInstance::CPicturePaint },
		{ "hide", CLuaInstance::CPictureHide },
		{ "setPicture", CLuaInstance::CPictureSetPicture },
		{ "__gc", CLuaInstance::CPictureDelete },
		{ NULL, NULL }
	};

	luaL_newmetatable(L, "cpicture");
	luaL_setfuncs(L, meth, 0);
	lua_pushvalue(L, -1);
	lua_setfield(L, -1, "__index");
	lua_setglobal(L, "cpicture");
}

int CLuaInstance::CPictureNew(lua_State *L)
{
	lua_assert(lua_istable(L,1));

	CLuaCWindow* parent = NULL;
	lua_Integer x=10, y=10, dx=100, dy=100;
	std::string image_name         = "";
	lua_Integer alignment          = 0;
	std::string tmp1             = "false";	// has_shadow
	lua_Unsigned color_frame      = (lua_Unsigned)COL_MENUCONTENT_PLUS_6;
	lua_Unsigned color_background = (lua_Unsigned)COL_MENUCONTENT_PLUS_0;
	lua_Unsigned color_shadow     = (lua_Unsigned)COL_MENUCONTENTDARK_PLUS_0;

	/*
	transparency = CFrameBuffer::TM_BLACK (2): Transparency when black content ('pseudo' transparency)
	transparency = CFrameBuffer::TM_NONE  (1): No 'pseudo' transparency
	*/
	lua_Integer transparency     = CFrameBuffer::TM_NONE;

	tableLookup(L, "parent"           , (void**)&parent);
	tableLookup(L, "x"                , x);
	tableLookup(L, "y"                , y);
	tableLookup(L, "dx"               , dx);
	tableLookup(L, "dy"               , dy);
	tableLookup(L, "image"            , image_name);

	tableLookup(L, "alignment"        , alignment); //invalid argumet, for compatibility
	if (alignment)
		dprintf(DEBUG_NORMAL, "[CLuaInstance][%s - %d] invalid argument: 'alignment' has no effect!\n", __func__, __LINE__);

	tableLookup(L, "has_shadow"       , tmp1);
	bool has_shadow = (tmp1 == "true" || tmp1 == "1" || tmp1 == "yes");
	tableLookup(L, "color_frame"      , color_frame);
	tableLookup(L, "color_background" , color_background);
	tableLookup(L, "color_shadow"     , color_shadow);
	tableLookup(L, "transparency"     , transparency);

	checkMagicMask(color_frame);
	checkMagicMask(color_background);
	checkMagicMask(color_shadow);

	CComponentsForm* pw = (parent && parent->w) ? parent->w->getBodyObject() : NULL;

	CLuaPicture **udata = (CLuaPicture **) lua_newuserdata(L, sizeof(CLuaPicture *));
	*udata = new CLuaPicture();
	(*udata)->cp = new CComponentsPicture(x, y, dx, dy, image_name, pw, has_shadow, (fb_pixel_t)color_frame, (fb_pixel_t)color_background, (fb_pixel_t)color_shadow, transparency);
	(*udata)->parent = pw;
	luaL_getmetatable(L, "cpicture");
	lua_setmetatable(L, -2);
	return 1;
}

int CLuaInstance::CPicturePaint(lua_State *L)
{
	lua_assert(lua_istable(L,1));
	CLuaPicture *m = CPictureCheck(L, 1);
	if (!m) return 0;

	std::string tmp = "true";
	tableLookup(L, "do_save_bg", tmp);
	bool do_save_bg = (tmp == "true" || tmp == "1" || tmp == "yes");

	m->cp->paint(do_save_bg);
	return 0;
}

int CLuaInstance::CPictureHide(lua_State *L)
{
	lua_assert(lua_istable(L,1));
	CLuaPicture *m = CPictureCheck(L, 1);
	if (!m) return 0;

	std::string tmp = "false";
	tableLookup(L, "no_restore", tmp);
	bool no_restore = (tmp == "true" || tmp == "1" || tmp == "yes");

	if (m->parent) {
		m->cp->setPicture("");
		m->cp->paint();
	} else
		m->cp->hide(no_restore);
	return 0;
}

int CLuaInstance::CPictureSetPicture(lua_State *L)
{
	lua_assert(lua_istable(L,1));
	CLuaPicture *m = CPictureCheck(L, 1);
	if (!m) return 0;

	std::string image_name = "";
	tableLookup(L, "image", image_name);

	m->cp->setPicture(image_name);
	return 0;
}

int CLuaInstance::CPictureDelete(lua_State *L)
{
	DBG("CLuaInstance::%s %d\n", __func__, lua_gettop(L));
	CLuaPicture *m = CPictureCheck(L, 1);
	if (!m) return 0;

//	m->cp->hide();
	delete m;
	return 0;
}

// --------------------------------------------------------------------------------

CLuaConfigFile *CLuaInstance::LuaConfigFileCheck(lua_State *L, int n)
{
	return *(CLuaConfigFile **) luaL_checkudata(L, n, "configfile");
}

void CLuaInstance::LuaConfigFileRegister(lua_State *L)
{
	luaL_Reg meth[] = {
		{ "new", CLuaInstance::LuaConfigFileNew },
		{ "loadConfig", CLuaInstance::LuaConfigFileLoadConfig },
		{ "saveConfig", CLuaInstance::LuaConfigFileSaveConfig },
		{ "clear", CLuaInstance::LuaConfigFileClear },
		{ "getString", CLuaInstance::LuaConfigFileGetString },
		{ "setString", CLuaInstance::LuaConfigFileSetString },
		{ "getInt32", CLuaInstance::LuaConfigFileGetInt32 },
		{ "setInt32", CLuaInstance::LuaConfigFileSetInt32 },
		{ "getBool", CLuaInstance::LuaConfigFileGetBool },
		{ "setBool", CLuaInstance::LuaConfigFileSetBool },
		{ "__gc", CLuaInstance::LuaConfigFileDelete },
		{ NULL, NULL }
	};

	luaL_newmetatable(L, "configfile");
	luaL_setfuncs(L, meth, 0);
	lua_pushvalue(L, -1);
	lua_setfield(L, -1, "__index");
	lua_setglobal(L, "configfile");
}

int CLuaInstance::LuaConfigFileNew(lua_State *L)
{
	CLuaConfigFile **udata = (CLuaConfigFile **) lua_newuserdata(L, sizeof(CLuaConfigFile *));
	*udata = new CLuaConfigFile();
	(*udata)->c = new CConfigFile('\t');
	luaL_getmetatable(L, "configfile");
	lua_setmetatable(L, -2);
	return 1;
}

int CLuaInstance::LuaConfigFileLoadConfig(lua_State *L)
{
	CLuaConfigFile *c = LuaConfigFileCheck(L, 1);
	if (!c) return 0;

	const char *fname = luaL_checkstring(L, 2);
	bool ret = c->c->loadConfig(fname);
	lua_pushboolean(L, ret);
	return 1;
}

int CLuaInstance::LuaConfigFileSaveConfig(lua_State *L)
{
	CLuaConfigFile *c = LuaConfigFileCheck(L, 1);
	if (!c) return 0;

	const char *fname = luaL_checkstring(L, 2);
	bool ret = c->c->saveConfig(fname);
	lua_pushboolean(L, ret);
	return 1;
}

int CLuaInstance::LuaConfigFileClear(lua_State *L)
{
	CLuaConfigFile *c = LuaConfigFileCheck(L, 1);
	if (!c) return 0;

	c->c->clear();
	return 0;
}

int CLuaInstance::LuaConfigFileGetString(lua_State *L)
{
	CLuaConfigFile *c = LuaConfigFileCheck(L, 1);
	if (!c) return 0;

	std::string ret;
	const char *key = luaL_checkstring(L, 2);
	const char *defaultVal = luaL_checkstring(L, 3);
	ret = c->c->getString(key, defaultVal);
	lua_pushstring(L, ret.c_str());
	return 1;
}

int CLuaInstance::LuaConfigFileSetString(lua_State *L)
{
	CLuaConfigFile *c = LuaConfigFileCheck(L, 1);
	if (!c) return 0;

	const char *key = luaL_checkstring(L, 2);
	const char *val = luaL_checkstring(L, 3);
	c->c->setString(key, val);
	return 0;
}

int CLuaInstance::LuaConfigFileGetInt32(lua_State *L)
{
	CLuaConfigFile *c = LuaConfigFileCheck(L, 1);
	if (!c) return 0;

	int ret;
	const char *key = luaL_checkstring(L, 2);
	int defaultVal = luaL_checkint(L, 3);
	ret = c->c->getInt32(key, defaultVal);
	lua_pushinteger(L, ret);
	return 1;
}

int CLuaInstance::LuaConfigFileSetInt32(lua_State *L)
{
	CLuaConfigFile *c = LuaConfigFileCheck(L, 1);
	if (!c) return 0;

	const char *key = luaL_checkstring(L, 2);
	int val = luaL_checkint(L, 3);
	c->c->setInt32(key, val);
	return 0;
}

int CLuaInstance::LuaConfigFileGetBool(lua_State *L)
{
	CLuaConfigFile *c = LuaConfigFileCheck(L, 1);
	if (!c) return 0;

	bool ret;
	const char *key = luaL_checkstring(L, 2);
	bool defaultVal = _luaL_checkbool(L, 3);
	ret = c->c->getBool(key, defaultVal);
	lua_pushboolean(L, ret);
	return 1;
}

int CLuaInstance::LuaConfigFileSetBool(lua_State *L)
{
	CLuaConfigFile *c = LuaConfigFileCheck(L, 1);
	if (!c) return 0;

	const char *key = luaL_checkstring(L, 2);
	bool val = _luaL_checkbool(L, 3);
	c->c->setBool(key, val);
	return 0;
}

int CLuaInstance::LuaConfigFileDelete(lua_State *L)
{
	CLuaConfigFile *c = LuaConfigFileCheck(L, 1);
	if (!c) return 0;
	delete c;
	return 0;
}

// --------------------------------------------------------------------------------
