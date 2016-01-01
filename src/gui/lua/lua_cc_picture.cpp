/*
 * lua components picture
 *
 * (C) 2014-2015 M. Liebmann (micha-bbg)
 * (C) 2014 Thilo Graf (dbt)
 * (C) 2015 Jacek Jendrzej (SatBaby)
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
#include "lua_cc_picture.h"

CLuaInstCCPicture* CLuaInstCCPicture::getInstance()
{
	static CLuaInstCCPicture* LuaInstCCPicture = NULL;

	if(!LuaInstCCPicture)
		LuaInstCCPicture = new CLuaInstCCPicture();
	return LuaInstCCPicture;
}

CLuaCCPicture *CLuaInstCCPicture::CCPictureCheck(lua_State *L, int n)
{
	return *(CLuaCCPicture **) luaL_checkudata(L, n, "cpicture");
}

void CLuaInstCCPicture::CCPictureRegister(lua_State *L)
{
	luaL_Reg meth[] = {
		{ "new",          CLuaInstCCPicture::CCPictureNew },
		{ "paint",        CLuaInstCCPicture::CCPicturePaint },
		{ "hide",         CLuaInstCCPicture::CCPictureHide },
		{ "setPicture",   CLuaInstCCPicture::CCPictureSetPicture },
		{ "setCenterPos", CLuaInstCCPicture::CCPictureSetCenterPos },
		{ "__gc",         CLuaInstCCPicture::CCPictureDelete },
		{ NULL, NULL }
	};

	luaL_newmetatable(L, "cpicture");
	luaL_setfuncs(L, meth, 0);
	lua_pushvalue(L, -1);
	lua_setfield(L, -1, "__index");
	lua_setglobal(L, "cpicture");
}

int CLuaInstCCPicture::CCPictureNew(lua_State *L)
{
	lua_assert(lua_istable(L,1));

	CLuaCCWindow* parent = NULL;
	lua_Integer x=10, y=10, dx=100, dy=100;
	std::string image_name        = "";
	lua_Integer alignment         = 0;
	lua_Unsigned color_frame      = (lua_Unsigned)COL_MENUCONTENT_PLUS_6;
	lua_Unsigned color_background = (lua_Unsigned)COL_MENUCONTENT_PLUS_0;
	lua_Unsigned color_shadow     = (lua_Unsigned)COL_MENUCONTENTDARK_PLUS_0;

	/*
	transparency = CFrameBuffer::TM_BLACK (2): Transparency when black content ('pseudo' transparency)
	transparency = CFrameBuffer::TM_NONE  (1): No 'pseudo' transparency
	*/
	lua_Integer transparency     = CFrameBuffer::TM_NONE;

	tableLookup(L, "parent",    (void**)&parent);
	tableLookup(L, "x",         x);
	tableLookup(L, "y",         y);
	tableLookup(L, "dx",        dx);
	tableLookup(L, "dy",        dy);
	tableLookup(L, "image",     image_name);
	tableLookup(L, "alignment", alignment); //invalid argumet, for compatibility
	if (alignment)
		dprintf(DEBUG_NORMAL, "[CLuaInstance][%s - %d] invalid argument: 'alignment' has no effect!\n", __func__, __LINE__);

	bool has_shadow = false;
	if (!tableLookup(L, "has_shadow", has_shadow)) {
		std::string tmp1 = "false";
		if (tableLookup(L, "has_shadow", tmp1))
			paramBoolDeprecated(L, tmp1.c_str());
		has_shadow = (tmp1 == "true" || tmp1 == "1" || tmp1 == "yes");
	}
	tableLookup(L, "color_frame",      color_frame);
	tableLookup(L, "color_background", color_background);
	tableLookup(L, "color_shadow",     color_shadow);
	tableLookup(L, "transparency",     transparency);

	color_frame      = checkMagicMask(color_frame);
	color_background = checkMagicMask(color_background);
	color_shadow     = checkMagicMask(color_shadow);

	CComponentsForm* pw = (parent && parent->w) ? parent->w->getBodyObject() : NULL;

	CLuaCCPicture **udata = (CLuaCCPicture **) lua_newuserdata(L, sizeof(CLuaCCPicture *));
	*udata = new CLuaCCPicture();
	if (dx == 0 && dy == 0) /* NO_SCALE */
		(*udata)->cp = new CComponentsPicture(x, y, image_name, pw, has_shadow, (fb_pixel_t)color_frame, (fb_pixel_t)color_background, (fb_pixel_t)color_shadow, transparency);
	else
		(*udata)->cp = new CComponentsPicture(x, y, dx, dy, image_name, pw, has_shadow, (fb_pixel_t)color_frame, (fb_pixel_t)color_background, (fb_pixel_t)color_shadow, transparency);
	(*udata)->parent = pw;
	luaL_getmetatable(L, "cpicture");
	lua_setmetatable(L, -2);
	return 1;
}

int CLuaInstCCPicture::CCPicturePaint(lua_State *L)
{
	lua_assert(lua_istable(L,1));
	CLuaCCPicture *D = CCPictureCheck(L, 1);
	if (!D) return 0;

	bool do_save_bg = true;
	if (!tableLookup(L, "do_save_bg", do_save_bg)) {
		std::string tmp = "true";
		if (tableLookup(L, "do_save_bg", tmp))
			paramBoolDeprecated(L, tmp.c_str());
		do_save_bg = (tmp == "true" || tmp == "1" || tmp == "yes");
	}
	D->cp->paint(do_save_bg);
	return 0;
}

int CLuaInstCCPicture::CCPictureHide(lua_State *L)
{
	lua_assert(lua_istable(L,1));
	CLuaCCPicture *D = CCPictureCheck(L, 1);
	if (!D) return 0;

	bool tmp1 = false;
	std::string tmp2 = "false";
	if ((tableLookup(L, "no_restore", tmp1)) || (tableLookup(L, "no_restore", tmp2)))
		obsoleteHideParameter(L);

	if (D->parent) {
		D->cp->setPicture("");
		D->cp->paint();
	} else
		D->cp->hide();
	return 0;
}

int CLuaInstCCPicture::CCPictureSetPicture(lua_State *L)
{
	lua_assert(lua_istable(L,1));
	CLuaCCPicture *D = CCPictureCheck(L, 1);
	if (!D) return 0;

	std::string image_name = "";
	tableLookup(L, "image", image_name);

	D->cp->setPicture(image_name);
	return 0;
}

int CLuaInstCCPicture::CCPictureSetCenterPos(lua_State *L)
{
	lua_assert(lua_istable(L,1));
	CLuaCCPicture *D = CCPictureCheck(L, 1);
	if (!D) return 0;
	lua_Integer  tmp_along_mode, along_mode = CC_ALONG_X | CC_ALONG_Y;
	tableLookup(L, "along_mode", tmp_along_mode);

	if (tmp_along_mode & CC_ALONG_X || tmp_along_mode & CC_ALONG_Y)
		along_mode=tmp_along_mode;

	D->cp->setCenterPos(along_mode);
	return 0;
}

int CLuaInstCCPicture::CCPictureDelete(lua_State *L)
{
	LUA_DEBUG("CLuaInstCCPicture::%s %d\n", __func__, lua_gettop(L));
	CLuaCCPicture *D = CCPictureCheck(L, 1);
	if (!D) return 0;
	delete D;
	return 0;
}
