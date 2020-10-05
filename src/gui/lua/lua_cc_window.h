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

#ifndef _LUACCWINDOW_H
#define _LUACCWINDOW_H

class CLuaCCWindow
{
	public:
		CComponentsWindow *w;
		CLuaCCWindow() { w = NULL; }
		~CLuaCCWindow() { delete w; }
};

class CLuaInstCCWindow : CCHeaderTypes
{
	public:
		CLuaInstCCWindow() {};
		~CLuaInstCCWindow() {};
		static CLuaInstCCWindow* getInstance();
		static void CCWindowRegister(lua_State *L);

	private:
		static int CCWindowNew(lua_State *L);
		static CLuaCCWindow *CCWindowCheck(lua_State *L, int n);
		static int CCWindowPaint(lua_State *L);
		static int CCWindowHide(lua_State *L);
		static int CCWindowSetCaption(lua_State *L);
		static int CCWindowSetWindowColor(lua_State *L);
		static int CCWindowPaintHeader(lua_State *L);
		static int CCWindowGetHeaderHeight(lua_State *L);
		static int CCWindowGetFooterHeight(lua_State *L);
		static int CCWindowGetHeaderHeight_dep(lua_State *L); // function 'header_height' is deprecated
		static int CCWindowGetFooterHeight_dep(lua_State *L); // function 'footer_height' is deprecated
		static int CCWindowSetCenterPos(lua_State *L);
		static int CCWindowDelete(lua_State *L);
		static int CCWindowSetDimensionsAll(lua_State *L);
		static int CCWindowSetBodyImage(lua_State *L);
};

#endif //_LUACCWINDOW_H
