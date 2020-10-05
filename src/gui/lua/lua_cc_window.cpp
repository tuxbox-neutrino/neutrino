/*
 * lua components window
 *
 * (C) 2014-2015 M. Liebmann (micha-bbg)
 * (C) 2014 Thilo Graf (dbt)
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

CLuaInstCCWindow* CLuaInstCCWindow::getInstance()
{
	static CLuaInstCCWindow* LuaInstCCWindow = NULL;

	if(!LuaInstCCWindow)
		LuaInstCCWindow = new CLuaInstCCWindow();
	return LuaInstCCWindow;
}

void CLuaInstCCWindow::CCWindowRegister(lua_State *L)
{
	luaL_Reg meth[] = {
		{ "new",            CLuaInstCCWindow::CCWindowNew },
		{ "paint",          CLuaInstCCWindow::CCWindowPaint },
		{ "hide",           CLuaInstCCWindow::CCWindowHide },
		{ "setCaption",     CLuaInstCCWindow::CCWindowSetCaption },
		{ "setWindowColor", CLuaInstCCWindow::CCWindowSetWindowColor },
		{ "paintHeader",    CLuaInstCCWindow::CCWindowPaintHeader },
		{ "headerHeight",   CLuaInstCCWindow::CCWindowGetHeaderHeight },
		{ "footerHeight",   CLuaInstCCWindow::CCWindowGetFooterHeight },
		{ "header_height",  CLuaInstCCWindow::CCWindowGetHeaderHeight_dep }, /* function 'header_height' is deprecated */
		{ "footer_height",  CLuaInstCCWindow::CCWindowGetFooterHeight_dep }, /* function 'footer_height' is deprecated */
		{ "setCenterPos",   CLuaInstCCWindow::CCWindowSetCenterPos },
		{ "setDimensionsAll", CLuaInstCCWindow::CCWindowSetDimensionsAll },
		{ "setBodyImage",   CCWindowSetBodyImage },
		{ "__gc",           CLuaInstCCWindow::CCWindowDelete },
		{ NULL, NULL }
	};

	luaL_newmetatable(L, "cwindow");
	luaL_setfuncs(L, meth, 0);
	lua_pushvalue(L, -1);
	lua_setfield(L, -1, "__index");
	lua_setglobal(L, "cwindow");
}

int CLuaInstCCWindow::CCWindowNew(lua_State *L)
{
	lua_assert(lua_istable(L,1));

	std::string name, icon    = std::string(NEUTRINO_ICON_INFO);
	lua_Unsigned color_frame  = (lua_Unsigned)COL_FRAME_PLUS_0;
	lua_Unsigned color_body   = (lua_Unsigned)COL_MENUCONTENT_PLUS_0;
	lua_Unsigned color_shadow = (lua_Unsigned)COL_SHADOW_PLUS_0;
	std::string tmp1          = "false";
	std::string btnRed        = "";
	std::string btnGreen      = "";
	std::string btnYellow     = "";
	std::string btnBlue       = "";
	lua_Integer x = 100, y = 100, dx = 450, dy = 250;
	tableLookup(L, "x", x);
	tableLookup(L, "y", y);
	tableLookup(L, "dx", dx);
	tableLookup(L, "dy", dy);
	tableLookup(L, "name", name) || tableLookup(L, "title", name) || tableLookup(L, "caption", name);
	tableLookup(L, "icon", icon);

	lua_Integer has_shadow = CC_SHADOW_OFF;
	if (!tableLookup(L, "has_shadow", has_shadow)) {
		tmp1 = "false";
		if (tableLookup(L, "has_shadow", tmp1))
			paramBoolDeprecated(L, tmp1.c_str());
		if ((tmp1 == "true" || tmp1 == "1" || tmp1 == "yes"))
			has_shadow = CC_SHADOW_ON;
	}

	tableLookup(L, "color_frame" , color_frame);
	tableLookup(L, "color_body"  , color_body);
	tableLookup(L, "color_shadow", color_shadow);
	tableLookup(L, "btnRed", btnRed);
	tableLookup(L, "btnGreen", btnGreen);
	tableLookup(L, "btnYellow", btnYellow);
	tableLookup(L, "btnBlue", btnBlue);

	color_frame  = checkMagicMask(color_frame);
	color_body   = checkMagicMask(color_body);
	color_shadow = checkMagicMask(color_shadow);

	bool show_header = true;
	if (!tableLookup(L, "show_header", show_header)) {
		tmp1 = "true";
		if (tableLookup(L, "show_header", tmp1))
			paramBoolDeprecated(L, tmp1.c_str());
		show_header = (tmp1 == "true" || tmp1 == "1" || tmp1 == "yes");
	}
	bool show_footer = true;
	if (!tableLookup(L, "show_footer", show_footer)) {
		tmp1 = "true";
		if (tableLookup(L, "show_footer", tmp1))
			paramBoolDeprecated(L, tmp1.c_str());
		show_footer = (tmp1 == "true" || tmp1 == "1" || tmp1 == "yes");
	}

	CLuaCCWindow **udata = (CLuaCCWindow **) lua_newuserdata(L, sizeof(CLuaCCWindow *));
	*udata = new CLuaCCWindow();
	(*udata)->w = new CComponentsWindow(x, y, dx, dy, name.c_str(), icon.c_str(), NULL, has_shadow, (fb_pixel_t)color_frame, (fb_pixel_t)color_body, (fb_pixel_t)color_shadow);
	/* Ignore percent conversion of width and height
	   to remain compatible with the Lua API */
	(*udata)->w->setWidth(dx);
	(*udata)->w->setHeight(dy);

	if (!show_header)
		(*udata)->w->showHeader(false);
	if (!show_footer)
		(*udata)->w->showFooter(false);
	else {
		CComponentsFooter* footer = (*udata)->w->getFooterObject();
		if (footer) {
			std::vector<button_label_cc> buttons;
			if (!btnRed.empty()) {
				button_label_cc btnSred;
				btnSred.button 		= NEUTRINO_ICON_BUTTON_RED;
				btnSred.text 		= btnRed;
				buttons.push_back(btnSred);
			}
			if (!btnGreen.empty()) {
				button_label_cc btnSgreen;
				btnSgreen.button 	= NEUTRINO_ICON_BUTTON_GREEN;
				btnSgreen.text		= btnGreen;
				buttons.push_back(btnSgreen);
			}
			if (!btnYellow.empty()) {
				button_label_cc btnSyellow;
				btnSyellow.button 	= NEUTRINO_ICON_BUTTON_YELLOW;
				btnSyellow.text 	= btnYellow;
				buttons.push_back(btnSyellow);
			}
			if (!btnBlue.empty()) {
				button_label_cc btnSblue;
				btnSblue.button 	= NEUTRINO_ICON_BUTTON_BLUE;
				btnSblue.text 		= btnBlue;
				buttons.push_back(btnSblue);
			}
			if (!buttons.empty())
				footer->setButtonLabels(buttons, footer->getWidth(), footer->getWidth() / buttons.size());
		}
	}

	luaL_getmetatable(L, "cwindow");
	lua_setmetatable(L, -2);
	return 1;
}

CLuaCCWindow *CLuaInstCCWindow::CCWindowCheck(lua_State *L, int n)
{
	return *(CLuaCCWindow **) luaL_checkudata(L, n, "cwindow");
}

int CLuaInstCCWindow::CCWindowPaint(lua_State *L)
{
	lua_assert(lua_istable(L,1));
	CLuaCCWindow *D = CCWindowCheck(L, 1);
	if (!D) return 0;

	bool do_save_bg = true;
	if (!tableLookup(L, "do_save_bg", do_save_bg)) {
		std::string tmp = "true";
		if (tableLookup(L, "do_save_bg", tmp))
			paramBoolDeprecated(L, tmp.c_str());
		do_save_bg = (tmp == "true" || tmp == "1" || tmp == "yes");
	}
	D->w->paint(do_save_bg);
	return 0;
}

int CLuaInstCCWindow::CCWindowHide(lua_State *L)
{
	lua_assert(lua_istable(L,1));
	CLuaCCWindow *D = CCWindowCheck(L, 1);
	if (!D) return 0;

	bool tmp1 = false;
	std::string tmp2 = "false";
	if ((tableLookup(L, "no_restore", tmp1)) || (tableLookup(L, "no_restore", tmp2)))
		obsoleteHideParameter(L);

	D->w->hide();
	return 0;
}

int CLuaInstCCWindow::CCWindowSetCaption(lua_State *L)
{
	lua_assert(lua_istable(L,1));
	CLuaCCWindow *D = CCWindowCheck(L, 1);
	if (!D) return 0;

	std::string name = "";
	tableLookup(L, "name", name) || tableLookup(L, "title", name) || tableLookup(L, "caption", name);

	lua_Integer alignment = (lua_Integer)DEFAULT_TITLE_ALIGN;
	tableLookup(L, "alignment", alignment);

	D->w->setWindowCaption(name, (cc_title_alignment_t)alignment);
	return 0;
}

int CLuaInstCCWindow::CCWindowSetWindowColor(lua_State *L)
{
	lua_assert(lua_istable(L,1));
	CLuaCCWindow *D = CCWindowCheck(L, 1);
	if (!D) return 0;

	lua_Unsigned color;
	if (tableLookup(L, "color_frame"  , color)) {
		color = checkMagicMask(color);
		D->w->setColorFrame(color);
	}
	if (tableLookup(L, "color_body"  , color)) {
		color = checkMagicMask(color);
		D->w->setColorBody(color);
	}
	if (tableLookup(L, "color_shadow"  , color)) {
		color = checkMagicMask(color);
		D->w->setColorShadow(color);
	}

	return 0;
}

int CLuaInstCCWindow::CCWindowPaintHeader(lua_State *L)
{
	CLuaCCWindow *D = CCWindowCheck(L, 1);
	if (!D) return 0;

	CComponentsHeader* header = D->w->getHeaderObject();
	if (header){
		D->w->showHeader();
		header->paint();
	}
	return 0;
}

// function 'header_height' is deprecated
int CLuaInstCCWindow::CCWindowGetHeaderHeight_dep(lua_State *L)
{
	functionDeprecated(L, "header_height", "headerHeight");
	return CCWindowGetHeaderHeight(L);
}

// function 'footer_height' is deprecated
int CLuaInstCCWindow::CCWindowGetFooterHeight_dep(lua_State *L)
{
	functionDeprecated(L, "footer_height", "footerHeight");
	return CCWindowGetFooterHeight(L);
}

int CLuaInstCCWindow::CCWindowGetHeaderHeight(lua_State *L)
{
	CLuaCCWindow *D = CCWindowCheck(L, 1);
	if (!D) return 0;

	CComponentsHeader* header = D->w->getHeaderObject();
	int hh = 0;
	if (header)
		hh = header->getHeight();
	lua_pushinteger(L, hh);
	return 1;
}

int CLuaInstCCWindow::CCWindowGetFooterHeight(lua_State *L)
{
	CLuaCCWindow *D = CCWindowCheck(L, 1);
	if (!D) return 0;

	CComponentsFooter* footer = D->w->getFooterObject();
	int fh = 0;
	if (footer)
		fh = footer->getHeight();
	lua_pushinteger(L, fh);
	return 1;
}

int CLuaInstCCWindow::CCWindowSetDimensionsAll(lua_State *L)
{
	CLuaCCWindow *D = CCWindowCheck(L, 1);
	if (!D) return 0;
	lua_Integer x = luaL_checkint(L, 2);
	lua_Integer y = luaL_checkint(L, 3);
	lua_Integer w = luaL_checkint(L, 4);
	lua_Integer h = luaL_checkint(L, 5);
	if(x>-1 && y > -1 && w > 1 && h > 1){
		if (h > (lua_Integer)CFrameBuffer::getInstance()->getScreenHeight())
			h = (lua_Integer)CFrameBuffer::getInstance()->getScreenHeight();
		if (w > (lua_Integer)CFrameBuffer::getInstance()->getScreenWidth())
			w = (lua_Integer)CFrameBuffer::getInstance()->getScreenWidth();
		if(x > w)
			x = 0;
		if(y > h)
			y = 0;
		D->w->setDimensionsAll(x,y,w,h);
	}
	return 0;
}

int CLuaInstCCWindow::CCWindowSetCenterPos(lua_State *L)
{
	lua_assert(lua_istable(L,1));
	CLuaCCWindow *D = CCWindowCheck(L, 1);
	if (!D) return 0;
	lua_Integer  tmp_along_mode, along_mode = CC_ALONG_X | CC_ALONG_Y;
	tableLookup(L, "along_mode", tmp_along_mode);

	if (tmp_along_mode & CC_ALONG_X || tmp_along_mode & CC_ALONG_Y)
		along_mode=tmp_along_mode;

	D->w->setCenterPos(along_mode);
	return 0;
}

int CLuaInstCCWindow::CCWindowSetBodyImage(lua_State *L)
{
	lua_assert(lua_istable(L,1));
	CLuaCCWindow *D = CCWindowCheck(L, 1);
	if (!D) return 0;

	std::string image = "";
	if (lua_istable(L, -1))
		tableLookup(L, "image_path", image);

	D->w->setBodyBGImage(image);
	return 0;
}

int CLuaInstCCWindow::CCWindowDelete(lua_State *L)
{
	LUA_DEBUG("CLuaInstCCWindow::%s %d\n", __func__, lua_gettop(L));
	CLuaCCWindow *D = CCWindowCheck(L, 1);
	if (!D) return 0;
	delete D;
	return 0;
}
