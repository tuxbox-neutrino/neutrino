/*
 * lua misc functions
 *
 * (C) 2014-2015 M. Liebmann (micha-bbg)
 * (C) 2014 Sven Hoefer (svenhoefer)
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
#include <system/debug.h>
#include <gui/infoclock.h>
#include <cs_api.h>
#include <neutrino.h>

#include "luainstance.h"
#include "lua_misc.h"

CLuaInstMisc* CLuaInstMisc::getInstance()
{
	static CLuaInstMisc* LuaInstMisc = NULL;

	if(!LuaInstMisc)
		LuaInstMisc = new CLuaInstMisc();
	return LuaInstMisc;
}

CLuaData *CLuaInstMisc::CheckData(lua_State *L, int narg)
{
	luaL_checktype(L, narg, LUA_TUSERDATA);
	void *ud = luaL_checkudata(L, narg, LUA_CLASSNAME);
	if (!ud)
		fprintf(stderr, "[CLuaInstMisc::%s] wrong type %p, %d, %s\n", __func__, L, narg, LUA_CLASSNAME);
	return *(CLuaData **)ud;  // unbox pointer
}

int CLuaInstMisc::strFind_old(lua_State *L)
{
	int numargs = lua_gettop(L);
	if (numargs < 3) {
		printf("CLuaInstMisc::%s: not enough arguments (%d, expected 2 (or 3 or 4))\n", __func__, numargs);
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

int CLuaInstMisc::strSub_old(lua_State *L)
{
	int numargs = lua_gettop(L);
	if (numargs < 3) {
		printf("CLuaInstMisc::%s: not enough arguments (%d, expected 2 (or 3))\n", __func__, numargs);
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

int CLuaInstMisc::createChannelIDfromUrl_old(lua_State *L)
{
	int numargs = lua_gettop(L);
	if (numargs < 2) {
		printf("CLuaInstMisc::%s: no arguments\n", __func__);
		lua_pushnil(L);
		return 1;
	}

	const char *url = luaL_checkstring(L, 2);
	if (strlen(url) < 1 ) {
		lua_pushnil(L);
		return 1;
	}

	t_channel_id id = CREATE_CHANNEL_ID(0, 0, 0, url);
	char id_str[17];
	snprintf(id_str, sizeof(id_str), "%" PRIx64, id);

	lua_pushstring(L, id_str);
	return 1;
}

int CLuaInstMisc::enableInfoClock_old(lua_State *L)
{
	bool enable = true;
	int numargs = lua_gettop(L);
	if (numargs > 1)
		enable = _luaL_checkbool(L, 2);
	CInfoClock::getInstance()->enableInfoClock(enable);
	return 0;
}

int CLuaInstMisc::runScriptExt_old(lua_State *L)
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

int CLuaInstMisc::GetRevision_old(lua_State *L)
{
	unsigned int rev = 0;
	std::string hw   = "";
#if HAVE_COOL_HARDWARE
	hw = "Coolstream";
#endif
	rev = cs_get_revision();
	lua_pushinteger(L, rev);
	lua_pushstring(L, hw.c_str());
	return 2;
}

int CLuaInstMisc::checkVersion_old(lua_State *L)
{
	int numargs = lua_gettop(L);
	if (numargs < 3) {
		printf("CLuaInstMisc::%s: not enough arguments (%d, expected 2)\n", __func__, numargs);
		lua_pushnil(L);
		return 1;
	}
	int major=0, minor=0;
	major = luaL_checkint(L, 2);
	minor = luaL_checkint(L, 3);
	if ((major > LUA_API_VERSION_MAJOR) || ((major == LUA_API_VERSION_MAJOR) && (minor > LUA_API_VERSION_MINOR))) {
		char msg[1024];
		snprintf(msg, sizeof(msg)-1, "%s (v%d.%d)\n%s v%d.%d",
				g_Locale->getText(LOCALE_LUA_VERSIONSCHECK1),
				LUA_API_VERSION_MAJOR, LUA_API_VERSION_MINOR,
				g_Locale->getText(LOCALE_LUA_VERSIONSCHECK2),
				major, minor);
		luaL_error(L, msg);
	}
	lua_pushinteger(L, 1); /* for backward compatibility */
	return 1;
}
