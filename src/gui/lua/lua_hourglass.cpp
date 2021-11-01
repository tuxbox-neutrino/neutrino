/*
 * lua hourglass/loader
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
#include "lua_hourglass.h"
#include "lua_cc_window.h"

CLuaInstHourGlass* CLuaInstHourGlass::getInstance()
{
	static CLuaInstHourGlass* LuaInstHourGlass = NULL;

	if(!LuaInstHourGlass)
		      LuaInstHourGlass = new CLuaInstHourGlass();
	return LuaInstHourGlass;
}

void CLuaInstHourGlass::HourGlassRegister(lua_State *L)
{
	luaL_Reg meth[] = {
		{ "new",             CLuaInstHourGlass::HourGlassNew },
		{ "paint",           CLuaInstHourGlass::HourGlassPaint },
		{ "hide",            CLuaInstHourGlass::HourGlassHide },
		{ "__gc",            CLuaInstHourGlass::HourGlassDelete },
		{ NULL,              NULL }
	};

	luaL_newmetatable(L, "hourglass");
	luaL_setfuncs(L, meth, 0);
	lua_pushvalue(L, -1);
	lua_setfield(L, -1, "__index");
	lua_setglobal(L, "hourglass");
}

int CLuaInstHourGlass::HourGlassNew(lua_State *L)
{
	lua_assert(lua_istable(L,1));

	lua_Integer x = 20, y = 20, dx = 48, dy = 48;
	tableLookup(L, "x", x);
	tableLookup(L, "y", y);
	tableLookup(L, "dx", dx);
	tableLookup(L, "dy", dy);

	std::string image_basename = "hourglass";
	tableLookup(L, "image_basename", image_basename);

	lua_Integer interval = HG_AUTO_PAINT_INTERVAL;
	tableLookup(L, "interval", interval);

	CLuaCCWindow* parent = NULL;
	tableLookup(L, "parent", (void**)&parent);

	lua_Integer shadow_mode = CC_SHADOW_OFF;
	tableLookup(L, "shadow_mode", shadow_mode);

	lua_Unsigned color_frame      = (lua_Unsigned)COL_FRAME_PLUS_0;
	lua_Unsigned color_background = (lua_Unsigned)COL_MENUCONTENT_PLUS_0;
	lua_Unsigned color_shadow     = (lua_Unsigned)COL_SHADOW_PLUS_0;
	tableLookup(L, "color_frame",      color_frame);
	tableLookup(L, "color_background", color_background);
	tableLookup(L, "color_shadow",     color_shadow);

	CComponentsForm* parent_container = (parent && parent->w) ? parent->w->getBodyObject() : NULL;

	CLuaHourGlass **udata = (CLuaHourGlass **) lua_newuserdata(L, sizeof(CLuaHourGlass *));
	*udata = new CLuaHourGlass();

	(*udata)->h = new CHourGlass(x, y,  dx, dy, image_basename, (int64_t)interval, parent_container, shadow_mode, (fb_pixel_t)color_frame, (fb_pixel_t)color_background, (fb_pixel_t)color_shadow);

	luaL_getmetatable(L, "hourglass");
	lua_setmetatable(L, -2);
	return 1;
}

CLuaHourGlass *CLuaInstHourGlass::HourGlassCheck(lua_State *L, int n)
{
	return *(CLuaHourGlass **) luaL_checkudata(L, n, "hourglass");
}

int CLuaInstHourGlass::HourGlassPaint(lua_State *L)
{
	lua_assert(lua_istable(L,1));
	CLuaHourGlass *D = HourGlassCheck(L, 1);
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

int CLuaInstHourGlass::HourGlassHide(lua_State *L)
{
	lua_assert(lua_istable(L,1));
	CLuaHourGlass *D = HourGlassCheck(L, 1);
	if (!D) return 0;

	bool tmp1 = false;
	std::string tmp2 = "false";
	if ((tableLookup(L, "no_restore", tmp1)) || (tableLookup(L, "no_restore", tmp2)))
		obsoleteHideParameter(L);

	D->h->hide();
	return 0;
}

int CLuaInstHourGlass::HourGlassDelete(lua_State *L)
{
	LUA_DEBUG("CLuaInstHourGlass::%s %d\n", __func__, lua_gettop(L));
	CLuaHourGlass *D = HourGlassCheck(L, 1);
	if (!D) return 0;
	delete D;
	return 0;
}
