/*
 * lua components text
 *
 * (C) 2014-2015 M. Liebmann (micha-bbg)
 * (C) 2014 Thilo Graf (dbt)
 * (C) 2014 Sven Hoefer (svenhoefer)
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

#ifndef _LUACCTEXT_H
#define _LUACCTEXT_H

class CLuaCCText
{
	public:
		CComponentsText *ct;
		CComponentsForm *parent;
		int mode, font_text;
		CLuaCCText() { ct = NULL; parent = NULL; mode = 0; font_text = 0;}
		~CLuaCCText() { if (parent == NULL) delete ct; }
};

class CLuaInstCCText
{
	public:
		CLuaInstCCText() {};
		~CLuaInstCCText() {};
		static CLuaInstCCText* getInstance();
		static void CCTextRegister(lua_State *L);

	private:
		static CLuaCCText *CCTextCheck(lua_State *L, int n);
		static int CCTextNew(lua_State *L);
		static int CCTextPaint(lua_State *L);
		static int CCTextHide(lua_State *L);
		static int CCTextSetText(lua_State *L);
		static int CCTextGetLines(lua_State *L);
		static int CCTextScroll(lua_State *L);
		static int CCTextSetCenterPos(lua_State *L);
		static int CCTextEnableUTF8(lua_State *L);
		static int CCTextSetDimensionsAll(lua_State *L);
		static int CCTextDelete(lua_State *L);
};

#endif //_LUACCTEXT_H
