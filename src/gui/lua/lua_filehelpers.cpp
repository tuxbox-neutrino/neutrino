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
		{ "new",      CLuaInstFileHelpers::FileHelpersNew },
		{ "__gc",     CLuaInstFileHelpers::FileHelpersDelete },
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





int CLuaInstFileHelpers::FileHelpersDelete(lua_State *L)
{
	CLuaFileHelpers *D = FileHelpersCheckData(L, 1);
	if (!D) return 0;
	delete D;
	return 0;
}
