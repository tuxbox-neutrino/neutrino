/*
 * lua config file
 *
 * (C) 2014-2015 M. Liebmann (micha-bbg)
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

#ifndef _LUACONFIGFILE_H
#define _LUACONFIGFILE_H

#include <configfile.h>

class CLuaConfigFile
{
	public:
		CConfigFile *c;
		CLuaConfigFile() { c = NULL; }
		~CLuaConfigFile() { delete c; }
};

class CLuaInstConfigFile
{
	public:
		CLuaInstConfigFile() {};
		~CLuaInstConfigFile() {};
		static CLuaInstConfigFile* getInstance();
		static void LuaConfigFileRegister(lua_State *L);

	private:
		static CLuaConfigFile *LuaConfigFileCheck(lua_State *L, int n);
		static int LuaConfigFileNew(lua_State *L);
		static int LuaConfigFileLoadConfig(lua_State *L);
		static int LuaConfigFileSaveConfig(lua_State *L);
		static int LuaConfigFileClear(lua_State *L);
		static int LuaConfigFileGetString(lua_State *L);
		static int LuaConfigFileSetString(lua_State *L);
		static int LuaConfigFileGetInt32(lua_State *L);
		static int LuaConfigFileSetInt32(lua_State *L);
		static int LuaConfigFileGetBool(lua_State *L);
		static int LuaConfigFileSetBool(lua_State *L);
		static int LuaConfigFileDeleteKey(lua_State *L);
		static int LuaConfigFileDelete(lua_State *L);
};

#endif //_LUACONFIGFILE_H
