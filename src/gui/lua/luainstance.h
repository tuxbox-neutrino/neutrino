/*
 * neutrino-mp lua to c++ bridge
 *
 * (C) 2013 Stefan Seyfried <seife@tuxboxcvs.slipkontur.de>
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
#ifndef _LUAINSTANCE_H
#define _LUAINSTANCE_H

/* LUA Is C */
extern "C" {
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
}
#include <driver/fb_window.h>
#include <vector>

#include "luainstance_helpers.h"

void LuaInstRegisterFunctions(lua_State *L, bool fromThreads=false);

/* inspired by Steve Kemp http://www.steve.org.uk/ */
class CLuaInstance
{
	static const char className[];
	static CLuaData *CheckData(lua_State *L, int narg);
public:
	CLuaInstance();
	~CLuaInstance();
	void runScript(const char *fileName, std::vector<std::string> *argv = NULL, std::string *result_code = NULL, std::string *result_string = NULL, std::string *error_string = NULL);
	void abortScript();

	enum {
		DYNFONT_NO_ERROR      = 0,
		DYNFONT_MAXIMUM_FONTS = 1,
		DYNFONT_TOO_WIDE      = 2,
		DYNFONT_TOO_HIGH      = 3
	};

	// Example: runScript(fileName, "Arg1", "Arg2", "Arg3", ..., NULL);
	//	Type of all parameters: const char*
	//	The last parameter to NULL is imperative.
	void runScript(const char *fileName, const char *arg0, ...);

	static int NewWindow(lua_State *L);
	static int GCWindow(lua_State *L);
	static int GetInput(lua_State *L);
	static int Blit(lua_State *L);
	static int GetLanguage(lua_State *L);
	static int PaintBox(lua_State *L);
	static int paintHLineRel(lua_State *L);
	static int paintVLineRel(lua_State *L);
	static int RenderString(lua_State *L);
	static int getRenderWidth(lua_State *L);
	static int FontHeight(lua_State *L);
	static int getDynFont(lua_State *L);
	static int PaintIcon(lua_State *L);
	static int DisplayImage(lua_State *L);
	static int GetSize(lua_State *L);
	static int saveScreen(lua_State *L);
	static int restoreScreen(lua_State *L);
	static int deleteSavedScreen(lua_State *L);
	static int scale2Res(lua_State *L);

private:
	lua_State* lua;

};

#endif /* _LUAINSTANCE_H */
