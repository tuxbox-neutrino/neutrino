/*
 * lua progress window
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

#ifndef _LUAPROGRESSWINDOW_H_
#define _LUAPROGRESSWINDOW_H_

class CLuaCProgressWindow
{
	public:
		CProgressWindow *w;
		CLuaCProgressWindow() { w = NULL; }
		~CLuaCProgressWindow() { delete w; }
};

class CLuaInstProgressWindow
{
	public:
		CLuaInstProgressWindow() {};
		~CLuaInstProgressWindow() {};
		static CLuaInstProgressWindow* getInstance();
		static void ProgressWindowRegister(lua_State *L);

	private:
		static int CProgressWindowNew(lua_State *L);
		static CLuaCProgressWindow *CProgressWindowCheck(lua_State *L, int n);
		static int CProgressWindowPaint(lua_State *L);
		static int CProgressWindowHide(lua_State *L);
		static int CProgressWindowSetTitle(lua_State *L);
		static int CProgressWindowDelete(lua_State *L);
		static int CProgressWindowShowStatusInternal(lua_State *L, bool local);
		static int CProgressWindowShowLocalStatus(lua_State *L);
		static int CProgressWindowShowGlobalStatus(lua_State *L);
};

#endif //_LUAPROGRESSWINDOW_H_
