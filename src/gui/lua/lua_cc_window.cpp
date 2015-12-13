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
	lua_Unsigned color_frame  = (lua_Unsigned)COL_MENUCONTENT_PLUS_6;
	lua_Unsigned color_body   = (lua_Unsigned)COL_MENUCONTENT_PLUS_0;
	lua_Unsigned color_shadow = (lua_Unsigned)COL_MENUCONTENTDARK_PLUS_0;
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

	bool has_shadow = false;
	if (!tableLookup(L, "has_shadow", has_shadow)) {
		tmp1 = "false";
		if (tableLookup(L, "has_shadow", tmp1))
			paramBoolDeprecated(L, tmp1.c_str());
		has_shadow = (tmp1 == "true" || tmp1 == "1" || tmp1 == "yes");
	}

	tableLookup(L, "color_frame" , color_frame);
	tableLookup(L, "color_body"  , color_body);
	tableLookup(L, "color_shadow", color_shadow);
	tableLookup(L, "btnRed", btnRed);
	tableLookup(L, "btnGreen", btnGreen);
	tableLookup(L, "btnYellow", btnYellow);
	tableLookup(L, "btnBlue", btnBlue);

	checkMagicMask(color_frame);
	checkMagicMask(color_body);
	checkMagicMask(color_shadow);

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
	(*udata)->w = new CComponentsWindow(x, y, dx, dy, name.c_str(), icon.c_str(), 0, has_shadow, (fb_pixel_t)color_frame, (fb_pixel_t)color_body, (fb_pixel_t)color_shadow);

	if (!show_header)
		(*udata)->w->showHeader(false);
	if (!show_footer)
		(*udata)->w->showFooter(false);
	else {
		CComponentsFooter* footer = (*udata)->w->getFooterObject();
		if (footer) {
			vector<button_label_s> buttons;
			if (!btnRed.empty()) {
				button_label_s btnSred;
				btnSred.button 		= NEUTRINO_ICON_BUTTON_RED;
				btnSred.text 		= btnRed;
				buttons.push_back(btnSred);
			}
			if (!btnGreen.empty()) {
				button_label_s btnSgreen;
				btnSgreen.button 	= NEUTRINO_ICON_BUTTON_GREEN;
				btnSgreen.text		= btnGreen;
				buttons.push_back(btnSgreen);
			}
			if (!btnYellow.empty()) {
				button_label_s btnSyellow;
				btnSyellow.button 	= NEUTRINO_ICON_BUTTON_YELLOW;
				btnSyellow.text 	= btnYellow;
				buttons.push_back(btnSyellow);
			}
			if (!btnBlue.empty()) {
				button_label_s btnSblue;
				btnSblue.button 	= NEUTRINO_ICON_BUTTON_BLUE;
				btnSblue.text 		= btnBlue;
				buttons.push_back(btnSblue);
			}
			if (!buttons.empty())
				footer->setButtonLabels(buttons, dx-20, (dx-20) / (buttons.size()+1));
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
	CLuaCCWindow *m = CCWindowCheck(L, 1);
	if (!m) return 0;

	bool do_save_bg = true;
	if (!tableLookup(L, "do_save_bg", do_save_bg)) {
		std::string tmp = "true";
		if (tableLookup(L, "do_save_bg", tmp))
			paramBoolDeprecated(L, tmp.c_str());
		do_save_bg = (tmp == "true" || tmp == "1" || tmp == "yes");
	}
	m->w->paint(do_save_bg);
	return 0;
}

int CLuaInstCCWindow::CCWindowHide(lua_State *L)
{
	lua_assert(lua_istable(L,1));
	CLuaCCWindow *m = CCWindowCheck(L, 1);
	if (!m) return 0;

	bool no_restore = false;
	if (!tableLookup(L, "no_restore", no_restore)) {
		std::string tmp = "false";
		if (tableLookup(L, "no_restore", tmp))
			paramBoolDeprecated(L, tmp.c_str());
		no_restore = (tmp == "true" || tmp == "1" || tmp == "yes");
	}
	m->w->hide(no_restore);
	return 0;
}

int CLuaInstCCWindow::CCWindowSetCaption(lua_State *L)
{
	lua_assert(lua_istable(L,1));
	CLuaCCWindow *m = CCWindowCheck(L, 1);
	if (!m) return 0;

	std::string name = "";
	tableLookup(L, "name", name) || tableLookup(L, "title", name) || tableLookup(L, "caption", name);

	m->w->setWindowCaption(name);
	return 0;
}

int CLuaInstCCWindow::CCWindowSetWindowColor(lua_State *L)
{
	lua_assert(lua_istable(L,1));
	CLuaCCWindow *m = CCWindowCheck(L, 1);
	if (!m) return 0;

	lua_Unsigned color;
	if (tableLookup(L, "color_frame"  , color)) {
		checkMagicMask(color);
		m->w->setColorFrame(color);
	}
	if (tableLookup(L, "color_body"  , color)) {
		checkMagicMask(color);
		m->w->setColorBody(color);
	}
	if (tableLookup(L, "color_shadow"  , color)) {
		checkMagicMask(color);
		m->w->setColorShadow(color);
	}

	return 0;
}

int CLuaInstCCWindow::CCWindowPaintHeader(lua_State *L)
{
	CLuaCCWindow *m = CCWindowCheck(L, 1);
	if (!m) return 0;

	CComponentsHeader* header = m->w->getHeaderObject();
	if (header)
		m->w->showHeader();
	header->paint();

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
	CLuaCCWindow *m = CCWindowCheck(L, 1);
	if (!m)
		return 0;

	CComponentsHeader* header = m->w->getHeaderObject();
	int hh = 0;
	if (header)
		hh = header->getHeight();
	lua_pushinteger(L, hh);
	return 1;
}

int CLuaInstCCWindow::CCWindowGetFooterHeight(lua_State *L)
{
	CLuaCCWindow *m = CCWindowCheck(L, 1);
	if (!m)
		return 0;

	CComponentsFooter* footer = m->w->getFooterObject();
	int fh = 0;
	if (footer)
		fh = footer->getHeight();
	lua_pushinteger(L, fh);
	return 1;
}

int CLuaInstCCWindow::CCWindowSetCenterPos(lua_State *L)
{
	lua_assert(lua_istable(L,1));
	CLuaCCWindow *m = CCWindowCheck(L, 1);
	if (!m) return 0;
	lua_Integer  tmp_along_mode, along_mode = CC_ALONG_X | CC_ALONG_Y;
	tableLookup(L, "along_mode", tmp_along_mode);

	if (tmp_along_mode & CC_ALONG_X || tmp_along_mode & CC_ALONG_Y)
		along_mode=tmp_along_mode;

	m->w->setCenterPos(along_mode);
	return 0;
}

int CLuaInstCCWindow::CCWindowDelete(lua_State *L)
{
	LUA_DEBUG("CLuaInstCCWindow::%s %d\n", __func__, lua_gettop(L));
	CLuaCCWindow *m = CCWindowCheck(L, 1);
	if (!m)
		return 0;

	delete m;
	return 0;
}
