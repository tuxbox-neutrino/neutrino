/*
 * lua messagebox
 *
 * (C) 2014 by martii
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

#ifndef _LUAMESSAGEBOX_H
#define _LUAMESSAGEBOX_H

class CLuaMessagebox
{
	public:
		CMsgBox *b;
		CLuaMessagebox();
		~CLuaMessagebox();
};

class CLuaInstMessagebox
{
	public:
		CLuaInstMessagebox() {};
		~CLuaInstMessagebox() {};
		static CLuaInstMessagebox* getInstance();
		static void MessageboxRegister(lua_State *L);

	private:
//		static CLuaMessagebox *MessageboxCheck(lua_State *L, int n);
		static int MessageboxExec(lua_State *L);
};

#endif //_LUAMESSAGEBOX_H
