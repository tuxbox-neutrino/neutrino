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

/* inspired by Steve Kemp http://www.steve.org.uk/ */
class CLUAInstance
{
	static const char className[];
	static const luaL_Reg methods[];
	static CFBWindow *CheckWindow(lua_State *L, int narg);
public:
	CLUAInstance();
	~CLUAInstance();
	void runScript(const char *fileName);

private:
	lua_State* lua;
	void registerFunctions();

	static int NewWindow(lua_State *L);
	static int PaintBox(lua_State *L);
	static int PaintIcon(lua_State *L);
	static int RenderString(lua_State *L);
	static int GCWindow(lua_State *L);
};

#endif /* _LUAINSTANCE_H */
