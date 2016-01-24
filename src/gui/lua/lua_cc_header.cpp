/*
 * lua components header
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

#include <global.h>
#include <system/debug.h>
#include <neutrino.h>

#include "luainstance.h"
#include "lua_cc_window.h"
#include "lua_cc_header.h"


CLuaInstCCHeader* CLuaInstCCHeader::getInstance()
{
	static CLuaInstCCHeader* LuaInstCCHeader = NULL;

	if(!LuaInstCCHeader)
		LuaInstCCHeader = new CLuaInstCCHeader();
	return LuaInstCCHeader;
}

CLuaCCHeader *CLuaInstCCHeader::CCHeaderCheck(lua_State *L, int n)
{
	return *(CLuaCCHeader **) luaL_checkudata(L, n, LUA_HEADER_CLASSNAME);
}

void CLuaInstCCHeader::CCHeaderRegister(lua_State *L)
{
	luaL_Reg meth[] = {
		{ "new",     CLuaInstCCHeader::CCHeaderNew },
		{ "paint",   CLuaInstCCHeader::CCHeaderPaint },
		{ "hide",    CLuaInstCCHeader::CCHeaderHide },
		{ "__gc",    CLuaInstCCHeader::CCHeaderDelete },
		{ NULL, NULL }
	};

	luaL_newmetatable(L, LUA_HEADER_CLASSNAME);
	luaL_setfuncs(L, meth, 0);
	lua_pushvalue(L, -1);
	lua_setfield(L, -1, "__index");
	lua_setglobal(L, LUA_HEADER_CLASSNAME);
}


int CLuaInstCCHeader::CCHeaderNew(lua_State *L)
{
	lua_assert(lua_istable(L,1));

	CLuaCCWindow* parent = NULL;
	lua_Integer x=10, y=10, dx=100, dy=100;
	lua_Integer buttons = 0;
	lua_Integer shadow_mode = CC_SHADOW_OFF;

	std::string caption, icon;
	lua_Unsigned color_frame  = (lua_Unsigned)COL_MENUCONTENT_PLUS_6;
	lua_Unsigned color_body   = (lua_Unsigned)COL_MENUCONTENT_PLUS_0;
	lua_Unsigned color_shadow = (lua_Unsigned)COL_MENUCONTENTDARK_PLUS_0;

	tableLookup(L, "parent",       (void**)&parent);
	tableLookup(L, "x",            x);
	tableLookup(L, "y",            y);
	tableLookup(L, "dx",           dx);
	tableLookup(L, "dy",           dy);
	tableLookup(L, "caption",      caption);
	tableLookup(L, "icon",         icon);
	tableLookup(L, "shadow_mode",  shadow_mode);
	tableLookup(L, "color_frame",  color_frame);
	tableLookup(L, "color_body" ,  color_body);
	tableLookup(L, "color_shadow", color_shadow);

	color_frame  = checkMagicMask(color_frame);
	color_body   = checkMagicMask(color_body);
	color_shadow = checkMagicMask(color_shadow);

	CComponentsForm* pw = (parent && parent->w) ? parent->w->getBodyObject() : NULL;
	CLuaCCHeader **udata = (CLuaCCHeader **) lua_newuserdata(L, sizeof(CLuaCCHeader *));
	*udata = new CLuaCCHeader();
	(*udata)->ch = new CComponentsHeader((const int&)x, (const int&)y, (const int&)dx, (const int&)dy,
					     caption, (std::string)"", (const int&)buttons,
					     (CComponentsForm*)parent, (int)shadow_mode,
					     (fb_pixel_t)color_frame, (fb_pixel_t)color_body, (fb_pixel_t)color_shadow);
	(*udata)->parent = pw;
	luaL_getmetatable(L, LUA_HEADER_CLASSNAME);
	lua_setmetatable(L, -2);
	return 1;
}

int CLuaInstCCHeader::CCHeaderPaint(lua_State *L)
{
	lua_assert(lua_istable(L,1));
	CLuaCCHeader *D = CCHeaderCheck(L, 1);
	if (!D) return 0;

	bool do_save_bg = true;
	tableLookup(L, "do_save_bg", do_save_bg);

	D->ch->paint(do_save_bg);
	return 0;
}

int CLuaInstCCHeader::CCHeaderHide(lua_State *L)
{
	CLuaCCHeader *D = CCHeaderCheck(L, 1);
	if (!D) return 0;

	D->ch->hide();
	return 0;
}

int CLuaInstCCHeader::CCHeaderDelete(lua_State *L)
{
	CLuaCCHeader *D = CCHeaderCheck(L, 1);
	if (!D) return 0;
	delete D;
	return 0;
}
