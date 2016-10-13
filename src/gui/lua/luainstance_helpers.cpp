/*
 * lua instance helper functions
 *
 * (C) 2013 Stefan Seyfried (seife)
 * (C) 2014-2015 M. Liebmann (micha-bbg)
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
#include <neutrino.h>

#include "luainstance.h"

/* Ported by Benny Chen, http://www.bennychen.cn/tag/lual_checkbool/ */
bool _luaL_checkbool(lua_State *L, int numArg)
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

void paramBoolDeprecated(lua_State *L, const char* val)
{
	lua_Debug ar;
	lua_getstack(L, 1, &ar);
	lua_getinfo(L, "Sl", &ar);
	printf("[Lua Script] \33[1;31m%s\33[0m %s (\33[31m\"%s\"\33[0m)\n                      %s \33[32mtrue\33[0m.\n                      (%s:%d)\n",
					g_Locale->getText(LOCALE_LUA_BOOLPARAM_DEPRECATED1),
					g_Locale->getText(LOCALE_LUA_BOOLPARAM_DEPRECATED2), val,
					g_Locale->getText(LOCALE_LUA_BOOLPARAM_DEPRECATED3),
					ar.short_src, ar.currentline);
}

void paramDeprecated(lua_State *L, const char* oldParam, const char* newParam)
{
	lua_Debug ar;
	lua_getstack(L, 1, &ar);
	lua_getinfo(L, "Sl", &ar);
	printf("[Lua Script] \33[1;31m%s\33[0m %s \33[33m%s\33[0m %s \33[1;33m%s\33[0m.\n                      (%s:%d)\n",
					g_Locale->getText(LOCALE_LUA_FUNCTION_DEPRECATED1),
					g_Locale->getText(LOCALE_LUA_PARAMETER_DEPRECATED2), oldParam,
					g_Locale->getText(LOCALE_LUA_FUNCTION_DEPRECATED3), newParam,
					ar.short_src, ar.currentline);
}

void functionDeprecated(lua_State *L, const char* oldFunc, const char* newFunc)
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

void obsoleteHideParameter(lua_State *L)
{
	lua_Debug ar;
	lua_getstack(L, 1, &ar);
	lua_getinfo(L, "Sl", &ar);
	printf("\33[1;31m[Lua script warning]\33[0m %s:%d: Obsolete parameter for hide() in use, please remove!\n", ar.short_src, ar.currentline);
}

lua_Unsigned checkMagicMask(lua_Unsigned col)
{
	if ((col & MAGIC_MASK) == MAGIC_COLOR)
		col = CFrameBuffer::getInstance()->realcolor[col & 0x000000ff];
	return col;
}

bool tableLookup(lua_State *L, const char *what, std::string &value)
{
	lua_pushstring(L, what);
	lua_gettable(L, -2);
	bool res = lua_isstring(L, -1);
	if (res)
		value = lua_tostring(L, -1);
	lua_pop(L, 1);
	return res;
}

bool tableLookup(lua_State *L, const char *what, lua_Integer &value)
{
	lua_pushstring(L, what);
	lua_gettable(L, -2);
	bool res = lua_isnumber(L, -1);
	if (res)
		value = (lua_Integer) lua_tonumber(L, -1);
	lua_pop(L, 1);
	return res;
}

bool tableLookup(lua_State *L, const char *what, lua_Unsigned &value)
{
	lua_pushstring(L, what);
	lua_gettable(L, -2);
	bool res = lua_isnumber(L, -1);
	if (res)
		value = (lua_Unsigned) lua_tonumber(L, -1);
	lua_pop(L, 1);
	return res;
}

bool tableLookup(lua_State *L, const char *what, void** value)
{
	lua_pushstring(L, what);
	lua_gettable(L, -2);
	bool res = lua_isuserdata(L, -1);
	if (res)
		*value = lua_unboxpointer(L, -1);
	lua_pop(L, 1);
	return res;
}

bool tableLookup(lua_State *L, const char *what, bool &value)
{
	lua_pushstring(L, what);
	lua_gettable(L, -2);
	bool res = lua_isboolean(L, -1);
	if (res)
		value = lua_toboolean(L, -1);
	lua_pop(L, 1);
	return res;
}
