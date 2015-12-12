/*
 * lua misc functions
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

#ifndef _LUAMISCFUNCS_H
#define _LUAMISCFUNCS_H

/*
class CLuaMisc
{
	public:
		CLuaMisc() {};
		~CLuaMisc() {};
};
*/

class CLuaInstMisc
{
	static CLuaData *CheckData(lua_State *L, int narg);
	public:
		CLuaInstMisc() {};
		~CLuaInstMisc() {};
		static CLuaInstMisc* getInstance();
//		static void MiscRegister(lua_State *L);

		static int strFind_old(lua_State *L);
		static int strSub_old(lua_State *L);
		static int createChannelIDfromUrl_old(lua_State *L);
		static int enableInfoClock_old(lua_State *L);
		static int runScriptExt_old(lua_State *L);
		static int GetRevision_old(lua_State *L);
		static int checkVersion_old(lua_State *L);

//	private:
//		static CLuaMisc *MiscCheck(lua_State *L, int n);
};

#endif //_LUAMISCFUNCS_H
