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
		/* The window a widget's table, on top of the stack, names under
		   "parent": a cwindow handle, NULL where there is none or the value
		   is no userdata, and a Lua error for a userdata of another class.
		   The value, or nil, is left below the table at slot: on the stack
		   the window cannot be collected while the widget is made, whatever
		   the table's other fields run. */
		static CLuaCCWindow *ParentLookup(lua_State *L, int &slot);
		/* The window deletes the widgets in its body when it goes, so a
		   widget's handle holds the window's handle, the one at slot, in the
		   registry for as long as it lives. Taken once the widget's userdata
		   has its metatable, so that whatever fails before leaves no hold
		   behind, and given back by the widget's __gc with ParentRelease(). */
		static int ParentHold(lua_State *L, int slot, CLuaCCWindow *parent);
		static void ParentRelease(lua_State *L, int &ref);

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
