/*
 * lua hourglass/loader
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

#ifndef _LUAHOURGLASS_H_
#define _LUAHOURGLASS_H_

#include <gui/widget/hourglass.h>

class CLuaHourGlass
{
	public:
		CHourGlass *h;
		CLuaHourGlass() { h = NULL; }
		~CLuaHourGlass() { delete h; }
};

class CLuaInstHourGlass
{
	public:
		CLuaInstHourGlass() {};
		~CLuaInstHourGlass() {};
		static CLuaInstHourGlass* getInstance();
		static void HourGlassRegister(lua_State *L);

	private:
		static CLuaHourGlass *HourGlassCheck(lua_State *L, int n);

		static int HourGlassNew(lua_State *L);
		static int HourGlassPaint(lua_State *L);
		static int HourGlassHide(lua_State *L);
		static int HourGlassDelete(lua_State *L);
};

#endif //_LUAHOURGLASS_H_
