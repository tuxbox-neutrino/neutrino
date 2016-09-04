/*
 * lua file helpers functions
 *
 * (C) 2016 M. Liebmann (micha-bbg)
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
#include <math.h>

#include <global.h>
#include <system/debug.h>
#include <system/helpers.h>
#include <neutrino.h>

#include "luainstance.h"
#include "lua_filehelpers.h"

CLuaInstFileHelpers* CLuaInstFileHelpers::getInstance()
{
	static CLuaInstFileHelpers* LuaInstFileHelpers = NULL;

	if (!LuaInstFileHelpers)
		LuaInstFileHelpers = new CLuaInstFileHelpers();
	return LuaInstFileHelpers;
}

CLuaFileHelpers *CLuaInstFileHelpers::FileHelpersCheckData(lua_State *L, int n)
{
	return *(CLuaFileHelpers **) luaL_checkudata(L, n, LUA_FILEHELPER_CLASSNAME);
}

void CLuaInstFileHelpers::LuaFileHelpersRegister(lua_State *L)
{
	luaL_Reg meth[] = {
		{ "new",        CLuaInstFileHelpers::FileHelpersNew      },
		{ "cp",         CLuaInstFileHelpers::FileHelpersCp       },
		{ "__gc",       CLuaInstFileHelpers::FileHelpersDelete   },
		{ NULL, NULL }
	};

	luaL_newmetatable(L, LUA_FILEHELPER_CLASSNAME);
	luaL_setfuncs(L, meth, 0);
	lua_pushvalue(L, -1);
	lua_setfield(L, -1, "__index");
	lua_setglobal(L, LUA_FILEHELPER_CLASSNAME);
}

int CLuaInstFileHelpers::FileHelpersNew(lua_State *L)
{
	CLuaFileHelpers **udata = (CLuaFileHelpers **) lua_newuserdata(L, sizeof(CLuaFileHelpers *));
	*udata = new CLuaFileHelpers();
	luaL_getmetatable(L, LUA_FILEHELPER_CLASSNAME);
	lua_setmetatable(L, -2);
	return 1;
}

int CLuaInstFileHelpers::FileHelpersCp(lua_State *L)
{
	CLuaFileHelpers *D = FileHelpersCheckData(L, 1);
	if (!D) return 0;

	int numargs = lua_gettop(L) - 1;
	int min_numargs = 2;
	if (numargs < min_numargs) {
		printf("luascript cp: not enough arguments (%d, expected %d)\n", numargs, min_numargs);
		lua_pushboolean(L, false);
		return 1;
	}

	if (!lua_isstring(L, 2)) {
		printf("luascript cp: argument 1 is not a string.\n");
		lua_pushboolean(L, false);
		return 1;
	}
	const char *from = "";
	from = luaL_checkstring(L, 2);

	if (!lua_isstring(L, 3)) {
		printf("luascript cp: argument 2 is not a string.\n");
		lua_pushboolean(L, false);
		return 1;
	}
	const char *to = "";
	to = luaL_checkstring(L, 3);

	const char *flags = "";
	if (numargs > min_numargs)
		flags = luaL_checkstring(L, 4);

	bool ret = false;
	CFileHelpers fh;
	fh.setConsoleQuiet(true);
	ret = fh.cp(from, to, flags);
	if (ret == false) {
		helpersDebugInfo di;
		fh.readDebugInfo(&di);
		lua_Debug ar;
		lua_getstack(L, 1, &ar);
		lua_getinfo(L, "Sl", &ar);
		printf(">>> Lua script error [%s:%d] %s\n    (error from neutrino: [%s:%d])\n",
		       ar.short_src, ar.currentline, di.msg.c_str(), di.file.c_str(), di.line);
	}

	lua_pushboolean(L, ret);
	return 1;
}




int CLuaInstFileHelpers::FileHelpersDelete(lua_State *L)
{
	CLuaFileHelpers *D = FileHelpersCheckData(L, 1);
	if (!D) return 0;
	delete D;
	return 0;
}
