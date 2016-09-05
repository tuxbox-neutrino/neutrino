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

#include <config.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <unistd.h>
#include <math.h>
#include <errno.h>
#include <utime.h>

#include <global.h>
#include <system/debug.h>
#include <system/helpers.h>
#include <neutrino.h>

#include "luainstance.h"
#include "lua_filehelpers.h"

CLuaInstFileHelpers* CLuaInstFileHelpers::getInstance()
{
	static CLuaInstFileHelpers* LuaInstFileHelpers = NULL;

	if (!LuaInstFileHelpers)
		LuaInstFileHelpers = new CLuaInstFileHelpers();
	return LuaInstFileHelpers;
}

CLuaFileHelpers *CLuaInstFileHelpers::FileHelpersCheckData(lua_State *L, int n)
{
	return *(CLuaFileHelpers **) luaL_checkudata(L, n, LUA_FILEHELPER_CLASSNAME);
}

void CLuaInstFileHelpers::LuaFileHelpersRegister(lua_State *L)
{
	luaL_Reg meth[] = {
		{ "new",        CLuaInstFileHelpers::FileHelpersNew      },
		{ "cp",         CLuaInstFileHelpers::FileHelpersCp       },
		{ "chmod",      CLuaInstFileHelpers::FileHelpersChmod    },
		{ "touch",      CLuaInstFileHelpers::FileHelpersTouch    },
		{ "rmdir",      CLuaInstFileHelpers::FileHelpersRmdir    },
		{ "mkdir",      CLuaInstFileHelpers::FileHelpersMkdir    },
		{ "readlink",   CLuaInstFileHelpers::FileHelpersReadlink },
		{ "ln",         CLuaInstFileHelpers::FileHelpersLn       },
		{ "exist",      CLuaInstFileHelpers::FileHelpersExist    },
		{ "__gc",       CLuaInstFileHelpers::FileHelpersDelete   },
		{ NULL, NULL }
	};

	luaL_newmetatable(L, LUA_FILEHELPER_CLASSNAME);
	luaL_setfuncs(L, meth, 0);
	lua_pushvalue(L, -1);
	lua_setfield(L, -1, "__index");
	lua_setglobal(L, LUA_FILEHELPER_CLASSNAME);
}

int CLuaInstFileHelpers::FileHelpersNew(lua_State *L)
{
	CLuaFileHelpers **udata = (CLuaFileHelpers **) lua_newuserdata(L, sizeof(CLuaFileHelpers *));
	*udata = new CLuaFileHelpers();
	luaL_getmetatable(L, LUA_FILEHELPER_CLASSNAME);
	lua_setmetatable(L, -2);
	return 1;
}

int CLuaInstFileHelpers::FileHelpersCp(lua_State *L)
{
	CLuaFileHelpers *D = FileHelpersCheckData(L, 1);
	if (!D) return 0;

	int numargs = lua_gettop(L) - 1;
	int min_numargs = 2;
	if (numargs < min_numargs) {
		printf("luascript cp: not enough arguments (%d, expected %d)\n", numargs, min_numargs);
		lua_pushboolean(L, false);
		return 1;
	}

	if (!lua_isstring(L, 2)) {
		printf("luascript cp: argument 1 is not a string.\n");
		lua_pushboolean(L, false);
		return 1;
	}
	const char *from = "";
	from = luaL_checkstring(L, 2);

	if (!lua_isstring(L, 3)) {
		printf("luascript cp: argument 2 is not a string.\n");
		lua_pushboolean(L, false);
		return 1;
	}
	const char *to = "";
	to = luaL_checkstring(L, 3);

	const char *flags = "";
	if (numargs > min_numargs)
		flags = luaL_checkstring(L, 4);

	bool ret = false;
	CFileHelpers fh;
	fh.setConsoleQuiet(true);
	ret = fh.cp(from, to, flags);
	if (ret == false) {
		helpersDebugInfo di;
		fh.readDebugInfo(&di);
		lua_Debug ar;
		lua_getstack(L, 1, &ar);
		lua_getinfo(L, "Sl", &ar);
		printf(">>> Lua script error [%s:%d] %s\n    (error from neutrino: [%s:%d])\n",
		       ar.short_src, ar.currentline, di.msg.c_str(), di.file.c_str(), di.line);
	}

	lua_pushboolean(L, ret);
	return 1;
}

int CLuaInstFileHelpers::FileHelpersChmod(lua_State *L)
{
	CLuaFileHelpers *D = FileHelpersCheckData(L, 1);
	if (!D) return 0;

	int numargs = lua_gettop(L) - 1;
	int min_numargs = 2;
	if (numargs < min_numargs) {
		printf("luascript chmod: not enough arguments (%d, expected %d)\n", numargs, min_numargs);
		lua_pushboolean(L, false);
		return 1;
	}

	const char *file = "";
	file = luaL_checkstring(L, 2);

	int mode_i = luaL_checkint(L, 3);
	/* Hack for convert lua number to octal */
	std::string mode_s = itoa(mode_i, 10);
	mode_t mode = (mode_t)(strtol(mode_s.c_str(), (char **)NULL, 8) & 0x0FFF);
	//printf("\n##### [%s:%d] str: %s, okt: %o \n \n", __func__, __LINE__, mode_s.c_str(), (int)mode);

	bool ret = true;
	if (chmod(file, mode) != 0) {
		ret = false;
		lua_Debug ar;
		lua_getstack(L, 1, &ar);
		lua_getinfo(L, "Sl", &ar);
		const char* s = strerror(errno);
		printf(">>> Lua script error [%s:%d] %s\n    (error from neutrino: [%s:%d])\n",
		       ar.short_src, ar.currentline, s, __path_file__, __LINE__);
	}

	lua_pushboolean(L, ret);
	return 1;
}

int CLuaInstFileHelpers::FileHelpersTouch(lua_State *L)
{
	CLuaFileHelpers *D = FileHelpersCheckData(L, 1);
	if (!D) return 0;

	int numargs = lua_gettop(L) - 1;
	int min_numargs = 1;
	if (numargs < min_numargs) {
		printf("luascript touch: not enough arguments (%d, expected %d)\n", numargs, min_numargs);
		lua_pushboolean(L, false);
		return 1;
	}

	const char *file = "";
	file = luaL_checkstring(L, 2);

	bool ret = true;
	lua_Debug ar;

	if (!file_exists(file)) {
		FILE *f = fopen(file, "w");
		if (f == NULL) {
			ret = false;
			lua_getstack(L, 1, &ar);
			lua_getinfo(L, "Sl", &ar);
			const char* s = strerror(errno);
			printf(">>> Lua script error [%s:%d] %s\n    (error from neutrino: [%s:%d])\n",
			       ar.short_src, ar.currentline, s, __path_file__, __LINE__);
			lua_pushboolean(L, ret);
			return 1;
		}
		fclose(f);
		if (numargs == min_numargs) {
			lua_pushboolean(L, ret);
			return 1;
		}
	}

	time_t modTime;
	if (numargs == min_numargs)
		/* current time */
		modTime = time(NULL);
	else
		/* new time */
		modTime = (time_t)luaL_checkint(L, 3);

	utimbuf utb;
	utb.actime  = modTime;
	utb.modtime = modTime;
	if (utime(file, &utb) != 0) {
		ret = false;
		lua_getstack(L, 1, &ar);
		lua_getinfo(L, "Sl", &ar);
		const char* s = strerror(errno);
		printf(">>> Lua script error [%s:%d] %s\n    (error from neutrino: [%s:%d])\n",
		       ar.short_src, ar.currentline, s, __path_file__, __LINE__);
	}

	lua_pushboolean(L, ret);
	return 1;
}

int CLuaInstFileHelpers::FileHelpersRmdir(lua_State *L)
{
	CLuaFileHelpers *D = FileHelpersCheckData(L, 1);
	if (!D) return 0;

	int numargs = lua_gettop(L) - 1;
	int min_numargs = 1;
	if (numargs < min_numargs) {
		printf("luascript rmdir: not enough arguments (%d, expected %d)\n", numargs, min_numargs);
		lua_pushboolean(L, false);
		return 1;
	}

	const char *dir = "";
	dir = luaL_checkstring(L, 2);

	bool ret = false;
	CFileHelpers* fh = CFileHelpers::getInstance();
	fh->setConsoleQuiet(true);
	ret = fh->removeDir(dir);
	if (ret == false) {
		helpersDebugInfo di;
		fh->readDebugInfo(&di);
		lua_Debug ar;
		lua_getstack(L, 1, &ar);
		lua_getinfo(L, "Sl", &ar);
		printf(">>> Lua script error [%s:%d] %s\n    (error from neutrino: [%s:%d])\n",
		       ar.short_src, ar.currentline, di.msg.c_str(), di.file.c_str(), di.line);
	}

	lua_pushboolean(L, ret);
	return 1;
}

int CLuaInstFileHelpers::FileHelpersMkdir(lua_State *L)
{
	CLuaFileHelpers *D = FileHelpersCheckData(L, 1);
	if (!D) return 0;

	int numargs = lua_gettop(L) - 1;
	int min_numargs = 1;
	if (numargs < min_numargs) {
		printf("luascript mkdir: not enough arguments (%d, expected %d)\n", numargs, min_numargs);
		lua_pushboolean(L, false);
		return 1;
	}

	const char *dir = "";
	dir = luaL_checkstring(L, 2);

	mode_t mode = 0755;
	if (numargs > min_numargs) {
		int mode_i = luaL_checkint(L, 3);
		/* Hack for convert lua number to octal */
		std::string mode_s = itoa(mode_i, 10);
		mode = (mode_t)(strtol(mode_s.c_str(), (char **)NULL, 8) & 0x0FFF);
		//printf("\n##### [%s:%d] str: %s, okt: %o \n \n", __func__, __LINE__, mode_s.c_str(), (int)mode);
	}

	bool ret = false;
	CFileHelpers* fh = CFileHelpers::getInstance();
	fh->setConsoleQuiet(true);
	ret = fh->createDir(dir, mode);
	if (ret == false) {
		helpersDebugInfo di;
		fh->readDebugInfo(&di);
		lua_Debug ar;
		lua_getstack(L, 1, &ar);
		lua_getinfo(L, "Sl", &ar);
		printf(">>> Lua script error [%s:%d] %s\n    (error from neutrino: [%s:%d])\n",
		       ar.short_src, ar.currentline, di.msg.c_str(), di.file.c_str(), di.line);
	}

	lua_pushboolean(L, ret);
	return 1;
}

int CLuaInstFileHelpers::FileHelpersReadlink(lua_State *L)
{
	CLuaFileHelpers *D = FileHelpersCheckData(L, 1);
	if (!D) return 0;

	int numargs = lua_gettop(L) - 1;
	int min_numargs = 1;
	if (numargs < min_numargs) {
		printf("luascript readlink: not enough arguments (%d, expected %d)\n", numargs, min_numargs);
		lua_pushnil(L);
		return 1;
	}

	const char *link = "";
	link = luaL_checkstring(L, 2);

	char buf[PATH_MAX];
	memset(buf, '\0', sizeof(buf));
	if (readlink(link, buf, sizeof(buf)-1) == -1) {
		lua_Debug ar;
		lua_getstack(L, 1, &ar);
		lua_getinfo(L, "Sl", &ar);
		const char* s = strerror(errno);
		printf(">>> Lua script error [%s:%d] %s\n    (error from neutrino: [%s:%d])\n",
		       ar.short_src, ar.currentline, s, __path_file__, __LINE__);

		lua_pushnil(L);
		return 1;
	}

	lua_pushstring(L, buf);
	return 1;
}

int CLuaInstFileHelpers::FileHelpersLn(lua_State *L)
{
	CLuaFileHelpers *D = FileHelpersCheckData(L, 1);
	if (!D) return 0;

	int numargs = lua_gettop(L) - 1;
	int min_numargs = 2;
	if (numargs < min_numargs) {
		printf("luascript ln: not enough arguments (%d, expected %d)\n", numargs, min_numargs);
		lua_pushboolean(L, false);
		return 1;
	}

	const char *src = "";
	src = luaL_checkstring(L, 2);
	const char *link = "";
	link = luaL_checkstring(L, 3);

	const char *flags = "";
	if (numargs > min_numargs)
		flags = luaL_checkstring(L, 4);

	bool symlnk = (strchr(flags, 's') != NULL);
	bool force  = (strchr(flags, 'f') != NULL);
	lua_Debug ar;

	if (!symlnk) {
		lua_getstack(L, 1, &ar);
		lua_getinfo(L, "Sl", &ar);
		const char* s = "Currently only supports symlinks.";
		printf(">>> Lua script error [%s:%d] %s\n    (error from neutrino: [%s:%d])\n",
		       ar.short_src, ar.currentline, s, __path_file__, __LINE__);
		lua_pushboolean(L, false);
		return 1;
	}

	bool ret = true;
	if (symlink(src, link) != 0) {
		if (force && (errno == EEXIST)) {
			if (unlink(link) == 0) {
				if (symlink(src, link) == 0) {
					lua_pushboolean(L, ret);
					return 1;
				}
			}
		}
		ret = false;
		lua_getstack(L, 1, &ar);
		lua_getinfo(L, "Sl", &ar);
		const char* s = strerror(errno);
		printf(">>> Lua script error [%s:%d] %s\n    (error from neutrino: [%s:%d])\n",
		       ar.short_src, ar.currentline, s, __path_file__, __LINE__);

	}

	lua_pushboolean(L, ret);
	return 1;
}

int CLuaInstFileHelpers::FileHelpersExist(lua_State *L)
{
	CLuaFileHelpers *D = FileHelpersCheckData(L, 1);
	if (!D) return 0;

	int numargs = lua_gettop(L) - 1;
	int min_numargs = 2;
	if (numargs < min_numargs) {
		printf("luascript exist: not enough arguments (%d, expected %d)\n", numargs, min_numargs);
		lua_pushnil(L);
		return 1;
	}

	bool ret = false;
	bool err = false;
	int errLine = 0;
	std::string errMsg = "";

	const char *file = "";
	file = luaL_checkstring(L, 2);
	const char *flag = "";
	flag = luaL_checkstring(L, 3);

	if (file_exists(file)) {
		struct stat FileInfo;
		if (lstat(file, &FileInfo) == -1) {
			err = true;
			errLine = __LINE__;
			errMsg = (std::string)strerror(errno);
		}
		else if (strchr(flag, 'f') != NULL) {
			if (S_ISREG(FileInfo.st_mode))
				ret = true;
		}
		else if (strchr(flag, 'l') != NULL) {
			if (S_ISLNK(FileInfo.st_mode))
				ret = true;
		}
		else if (strchr(flag, 'd') != NULL) {
			if (S_ISDIR(FileInfo.st_mode))
				ret = true;
		}
		else {
			err = true;
			errLine = __LINE__;
			errMsg = (strlen(flag) == 0) ? "no" : "unknown";
			errMsg += " flag given.";
		}
	}

	if (err) {
		lua_Debug ar;
		lua_getstack(L, 1, &ar);
		lua_getinfo(L, "Sl", &ar);
		printf(">>> Lua script error [%s:%d] %s\n    (error from neutrino: [%s:%d])\n",
		       ar.short_src, ar.currentline, errMsg.c_str(), __path_file__, errLine);
		lua_pushnil(L);
		return 1;
	}

	lua_pushboolean(L, ret);
	return 1;
}


int CLuaInstFileHelpers::FileHelpersDelete(lua_State *L)
{
	CLuaFileHelpers *D = FileHelpersCheckData(L, 1);
	if (!D) return 0;
	delete D;
	return 0;
}
