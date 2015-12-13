/*
 * lua components signalbox
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
#include <gui/components/cc.h>
#include <neutrino.h>

#include "luainstance.h"
#include "lua_cc_window.h"
#include "lua_cc_signalbox.h"

CLuaInstCCSignalbox* CLuaInstCCSignalbox::getInstance()
{
	static CLuaInstCCSignalbox* LuaInstCCSignalbox = NULL;

	if(!LuaInstCCSignalbox)
		LuaInstCCSignalbox = new CLuaInstCCSignalbox();
	return LuaInstCCSignalbox;
}

CLuaCCSignalBox *CLuaInstCCSignalbox::CCSignalBoxCheck(lua_State *L, int n)
{
	return *(CLuaCCSignalBox **) luaL_checkudata(L, n, "signalbox");
}

void CLuaInstCCSignalbox::CCSignalBoxRegister(lua_State *L)
{
	luaL_Reg meth[] = {
		{ "new",          CLuaInstCCSignalbox::CCSignalBoxNew },
		{ "paint",        CLuaInstCCSignalbox::CCSignalBoxPaint },
		{ "setCenterPos", CLuaInstCCSignalbox::CCSignalBoxSetCenterPos },
		{ "__gc",         CLuaInstCCSignalbox::CCSignalBoxDelete },
		{ NULL, NULL }
	};

	luaL_newmetatable(L, "signalbox");
	luaL_setfuncs(L, meth, 0);
	lua_pushvalue(L, -1);
	lua_setfield(L, -1, "__index");
	lua_setglobal(L, "signalbox");
}

int CLuaInstCCSignalbox::CCSignalBoxNew(lua_State *L)
{
	lua_assert(lua_istable(L,1));

	lua_Integer x = 110, y = 150, dx = 430, dy = 150;
	lua_Integer vertical = true;
	CLuaCCWindow* parent = NULL;
	tableLookup(L, "x", x);
	tableLookup(L, "y", y);
	tableLookup(L, "dx", dx);
	tableLookup(L, "dy", dy);
	tableLookup(L, "vertical", vertical);
	tableLookup(L, "parent", (void**)&parent);

	CComponentsForm* pw = (parent && parent->w) ? parent->w->getBodyObject() : NULL;
	CLuaCCSignalBox **udata = (CLuaCCSignalBox **) lua_newuserdata(L, sizeof(CLuaCCSignalBox *));
	*udata = new CLuaCCSignalBox();
	(*udata)->s = new CSignalBox(x, y, dx, dy, NULL, (vertical!=0)?true:false, pw);
	(*udata)->parent = pw;
	luaL_getmetatable(L, "signalbox");
	lua_setmetatable(L, -2);
	return 1;
}

int CLuaInstCCSignalbox::CCSignalBoxPaint(lua_State *L)
{
	lua_assert(lua_istable(L,1));
	CLuaCCSignalBox *m = CCSignalBoxCheck(L, 1);
	if (!m) return 0;

	bool do_save_bg = true;
	if (!tableLookup(L, "do_save_bg", do_save_bg)) {
		std::string tmp = "true";
		if (tableLookup(L, "do_save_bg", tmp))
			paramBoolDeprecated(L, tmp.c_str());
		do_save_bg = (tmp == "true" || tmp == "1" || tmp == "yes");
	}
	m->s->paint(do_save_bg);
	return 0;
}

int CLuaInstCCSignalbox::CCSignalBoxSetCenterPos(lua_State *L)
{
	lua_assert(lua_istable(L,1));
	CLuaCCSignalBox *m = CCSignalBoxCheck(L, 1);
	if (!m) return 0;
	lua_Integer  tmp_along_mode, along_mode = CC_ALONG_X | CC_ALONG_Y;
	tableLookup(L, "along_mode", tmp_along_mode);

	if (tmp_along_mode & CC_ALONG_X || tmp_along_mode & CC_ALONG_Y)
		along_mode=tmp_along_mode;

	m->s->setCenterPos(along_mode);
	return 0;
}

int CLuaInstCCSignalbox::CCSignalBoxDelete(lua_State *L)
{
	CLuaCCSignalBox *m = CCSignalBoxCheck(L, 1);
	if (!m)
		return 0;

	delete m;
	return 0;
}
