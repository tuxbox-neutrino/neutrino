/*
 * lua file helpers functions
 *
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

#ifndef _LUAFILEHELPERS_H
#define _LUAFILEHELPERS_H

class CLuaFileHelpers
{
	public:
		CLuaFileHelpers() {};
		~CLuaFileHelpers() {};
};

class CLuaInstFileHelpers
{
	public:

		CLuaInstFileHelpers() {};
		~CLuaInstFileHelpers() {};
		static CLuaInstFileHelpers* getInstance();
		static void LuaFileHelpersRegister(lua_State *L);

	private:
		static CLuaFileHelpers *FileHelpersCheckData(lua_State *L, int n);
		static int FileHelpersNew(lua_State *L);
		static int FileHelpersCp(lua_State *L);
		static int FileHelpersChmod(lua_State *L);
		static int FileHelpersTouch(lua_State *L);
		static int FileHelpersRmdir(lua_State *L);
		static int FileHelpersMkdir(lua_State *L);
		static int FileHelpersReadlink(lua_State *L);
		static int FileHelpersLn(lua_State *L);
		static int FileHelpersExist(lua_State *L);
		static int FileHelpersDelete(lua_State *L);
};

#endif //_LUAFILEHELPERS_H
