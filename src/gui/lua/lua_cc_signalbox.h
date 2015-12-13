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

#ifndef _LUACCSIGNALBOX_H
#define _LUACCSIGNALBOX_H

class CLuaCCSignalBox
{
	public:
		CSignalBox *s;
		CComponentsForm *parent;
		CLuaCCSignalBox() { s = NULL; parent = NULL;}
		~CLuaCCSignalBox() { if (parent == NULL) delete s; }
};

class CLuaInstCCSignalbox
{
	public:
		CLuaInstCCSignalbox() {};
		~CLuaInstCCSignalbox() {};
		static CLuaInstCCSignalbox* getInstance();
		static void CCSignalBoxRegister(lua_State *L);

	private:
		static CLuaCCSignalBox *CCSignalBoxCheck(lua_State *L, int n);
		static int CCSignalBoxNew(lua_State *L);
		static int CCSignalBoxPaint(lua_State *L);
		static int CCSignalBoxSetCenterPos(lua_State *L);
		static int CCSignalBoxDelete(lua_State *L);
};

#endif //_LUACCSIGNALBOX_H
