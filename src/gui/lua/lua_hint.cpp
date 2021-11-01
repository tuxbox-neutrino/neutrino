/*
 * lua simple hint window
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
#include "lua_hint.h"

CLuaInstHint* CLuaInstHint::getInstance()
{
	static CLuaInstHint* LuaInstHint = NULL;

	if(!LuaInstHint)
		      LuaInstHint = new CLuaInstHint();
	return LuaInstHint;
}

void CLuaInstHint::HintRegister(lua_State *L)
{
	luaL_Reg meth[] = {
		{ "new",             CLuaInstHint::HintNew },
		{ "paint",           CLuaInstHint::HintPaint },
		{ "hide",            CLuaInstHint::HintHide },
		{ "__gc",            CLuaInstHint::HintDelete },
		{ NULL,              NULL }
	};

	luaL_newmetatable(L, "hint");
	luaL_setfuncs(L, meth, 0);
	lua_pushvalue(L, -1);
	lua_setfield(L, -1, "__index");
	lua_setglobal(L, "hint");
}

int CLuaInstHint::HintNew(lua_State *L)
{
	lua_assert(lua_istable(L,1));

	std::string text = "";
	tableLookup(L, "text", text);

	bool show_background = true;
	if (!tableLookup(L, "show_background", show_background))
	{
		std::string tmp = "true";
		if (tableLookup(L, "show_background", tmp))
			paramBoolDeprecated(L, tmp.c_str());
		show_background = (tmp == "true" || tmp == "1" || tmp == "yes");
	}

	CLuaHint **udata = (CLuaHint **) lua_newuserdata(L, sizeof(CLuaHint *));
	*udata = new CLuaHint();

	(*udata)->h = new CHint(text.c_str(), show_background);

	luaL_getmetatable(L, "hint");
	lua_setmetatable(L, -2);
	return 1;
}

CLuaHint *CLuaInstHint::HintCheck(lua_State *L, int n)
{
	return *(CLuaHint **) luaL_checkudata(L, n, "hint");
}

int CLuaInstHint::HintPaint(lua_State *L)
{
	lua_assert(lua_istable(L,1));
	CLuaHint *D = HintCheck(L, 1);
	if (!D) return 0;

	bool do_save_bg = true;
	if (!tableLookup(L, "do_save_bg", do_save_bg)) {
		std::string tmp = "true";
		if (tableLookup(L, "do_save_bg", tmp))
			paramBoolDeprecated(L, tmp.c_str());
		do_save_bg = (tmp == "true" || tmp == "1" || tmp == "yes");
	}
	D->h->paint(do_save_bg);
	return 0;
}

int CLuaInstHint::HintHide(lua_State *L)
{
	lua_assert(lua_istable(L,1));
	CLuaHint *D = HintCheck(L, 1);
	if (!D) return 0;

	bool tmp1 = false;
	std::string tmp2 = "false";
	if ((tableLookup(L, "no_restore", tmp1)) || (tableLookup(L, "no_restore", tmp2)))
		obsoleteHideParameter(L);

	D->h->hide();
	return 0;
}

int CLuaInstHint::HintDelete(lua_State *L)
{
	LUA_DEBUG("CLuaInstHint::%s %d\n", __func__, lua_gettop(L));
	CLuaHint *D = HintCheck(L, 1);
	if (!D) return 0;
	delete D;
	return 0;
}
