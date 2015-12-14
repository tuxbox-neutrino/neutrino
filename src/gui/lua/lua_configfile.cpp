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

#include <config.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <unistd.h>

#include <global.h>
#include <system/debug.h>
#include <neutrino.h>

#include "luainstance.h"
#include "lua_configfile.h"

CLuaInstConfigFile* CLuaInstConfigFile::getInstance()
{
	static CLuaInstConfigFile* LuaInstConfigFile = NULL;

	if(!LuaInstConfigFile)
		LuaInstConfigFile = new CLuaInstConfigFile();
	return LuaInstConfigFile;
}

CLuaConfigFile *CLuaInstConfigFile::LuaConfigFileCheck(lua_State *L, int n)
{
	return *(CLuaConfigFile **) luaL_checkudata(L, n, "configfile");
}

void CLuaInstConfigFile::LuaConfigFileRegister(lua_State *L)
{
	luaL_Reg meth[] = {
		{ "new",        CLuaInstConfigFile::LuaConfigFileNew },
		{ "loadConfig", CLuaInstConfigFile::LuaConfigFileLoadConfig },
		{ "saveConfig", CLuaInstConfigFile::LuaConfigFileSaveConfig },
		{ "clear",      CLuaInstConfigFile::LuaConfigFileClear },
		{ "getString",  CLuaInstConfigFile::LuaConfigFileGetString },
		{ "setString",  CLuaInstConfigFile::LuaConfigFileSetString },
		{ "getInt32",   CLuaInstConfigFile::LuaConfigFileGetInt32 },
		{ "setInt32",   CLuaInstConfigFile::LuaConfigFileSetInt32 },
		{ "getBool",    CLuaInstConfigFile::LuaConfigFileGetBool },
		{ "setBool",    CLuaInstConfigFile::LuaConfigFileSetBool },
		{ "deleteKey",  CLuaInstConfigFile::LuaConfigFileDeleteKey },
		{ "__gc",       CLuaInstConfigFile::LuaConfigFileDelete },
		{ NULL, NULL }
	};

	luaL_newmetatable(L, "configfile");
	luaL_setfuncs(L, meth, 0);
	lua_pushvalue(L, -1);
	lua_setfield(L, -1, "__index");
	lua_setglobal(L, "configfile");
}

int CLuaInstConfigFile::LuaConfigFileNew(lua_State *L)
{
	int numargs = lua_gettop(L);
	const char *delimiter = "\t";
	if (numargs > 0)
		delimiter = luaL_checkstring(L, 1);
	bool saveDefaults = true;
	if (numargs > 1)
		saveDefaults = _luaL_checkbool(L, 2);

	CLuaConfigFile **udata = (CLuaConfigFile **) lua_newuserdata(L, sizeof(CLuaConfigFile *));
	*udata = new CLuaConfigFile();
	(*udata)->c = new CConfigFile(delimiter[0], saveDefaults);
	luaL_getmetatable(L, "configfile");
	lua_setmetatable(L, -2);
	return 1;
}

int CLuaInstConfigFile::LuaConfigFileLoadConfig(lua_State *L)
{
	CLuaConfigFile *c = LuaConfigFileCheck(L, 1);
	if (!c) return 0;

	const char *fname = luaL_checkstring(L, 2);
	bool ret = c->c->loadConfig(fname);
	lua_pushboolean(L, ret);
	return 1;
}

int CLuaInstConfigFile::LuaConfigFileSaveConfig(lua_State *L)
{
	CLuaConfigFile *c = LuaConfigFileCheck(L, 1);
	if (!c) return 0;

	const char *fname = luaL_checkstring(L, 2);
	bool ret = c->c->saveConfig(fname);
	lua_pushboolean(L, ret);
	return 1;
}

int CLuaInstConfigFile::LuaConfigFileClear(lua_State *L)
{
	CLuaConfigFile *c = LuaConfigFileCheck(L, 1);
	if (!c) return 0;

	c->c->clear();
	return 0;
}

int CLuaInstConfigFile::LuaConfigFileGetString(lua_State *L)
{
	CLuaConfigFile *c = LuaConfigFileCheck(L, 1);
	if (!c) return 0;
	int numargs = lua_gettop(L);

	std::string ret;
	const char *key = luaL_checkstring(L, 2);
	const char *defaultVal = "";
	if (numargs > 2)
		defaultVal = luaL_checkstring(L, 3);
	ret = c->c->getString(key, defaultVal);
	lua_pushstring(L, ret.c_str());
	return 1;
}

int CLuaInstConfigFile::LuaConfigFileSetString(lua_State *L)
{
	CLuaConfigFile *c = LuaConfigFileCheck(L, 1);
	if (!c) return 0;

	const char *key = luaL_checkstring(L, 2);
	const char *val = luaL_checkstring(L, 3);
	c->c->setString(key, val);
	return 0;
}

int CLuaInstConfigFile::LuaConfigFileGetInt32(lua_State *L)
{
	CLuaConfigFile *c = LuaConfigFileCheck(L, 1);
	if (!c) return 0;
	int numargs = lua_gettop(L);

	int ret;
	const char *key = luaL_checkstring(L, 2);
	int defaultVal = 0;
	if (numargs > 2)
		defaultVal = luaL_checkint(L, 3);
	ret = c->c->getInt32(key, defaultVal);
	lua_pushinteger(L, ret);
	return 1;
}

int CLuaInstConfigFile::LuaConfigFileSetInt32(lua_State *L)
{
	CLuaConfigFile *c = LuaConfigFileCheck(L, 1);
	if (!c) return 0;

	const char *key = luaL_checkstring(L, 2);
	int val = luaL_checkint(L, 3);
	c->c->setInt32(key, val);
	return 0;
}

int CLuaInstConfigFile::LuaConfigFileGetBool(lua_State *L)
{
	CLuaConfigFile *c = LuaConfigFileCheck(L, 1);
	if (!c) return 0;
	int numargs = lua_gettop(L);

	bool ret;
	const char *key = luaL_checkstring(L, 2);
	bool defaultVal = false;
	if (numargs > 2)
		defaultVal = _luaL_checkbool(L, 3);
	ret = c->c->getBool(key, defaultVal);
	lua_pushboolean(L, ret);
	return 1;
}

int CLuaInstConfigFile::LuaConfigFileSetBool(lua_State *L)
{
	CLuaConfigFile *c = LuaConfigFileCheck(L, 1);
	if (!c) return 0;

	const char *key = luaL_checkstring(L, 2);
	bool val = _luaL_checkbool(L, 3);
	c->c->setBool(key, val);
	return 0;
}

int CLuaInstConfigFile::LuaConfigFileDeleteKey(lua_State *L)
{
	CLuaConfigFile *c = LuaConfigFileCheck(L, 1);
	if (!c) return 0;

	const char *s1 = luaL_checkstring(L, 2);
	std::string key(s1);
	c->c->deleteKey(key);
	return 0;
}

int CLuaInstConfigFile::LuaConfigFileDelete(lua_State *L)
{
	CLuaConfigFile *c = LuaConfigFileCheck(L, 1);
	if (!c) return 0;
	delete c;
	return 0;
}
