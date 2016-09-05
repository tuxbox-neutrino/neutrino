/*
 * lua stringinput
 *
 * (C) 2016 Sven Hoefer (svenhoefer)
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

#ifndef _LUASTRINGINPUT_H
#define _LUASTRINGINPUT_H

class CLuaStringInput
{
	public:
		CStringInput *b;
		CLuaStringInput();
		~CLuaStringInput();
};

class CLuaInstStringInput
{
	public:
		CLuaInstStringInput() {};
		~CLuaInstStringInput() {};
		static CLuaInstStringInput* getInstance();
		static void StringInputRegister(lua_State *L);

	private:
		static int StringInputExec(lua_State *L);
};

#endif //_LUASTRINGINPUT_H
