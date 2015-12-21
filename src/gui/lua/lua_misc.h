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

class CLuaMisc
{
	public:
		CLuaMisc() {};
		~CLuaMisc() {};
};

class CLuaInstMisc
{
	public:
		enum {
			POSTMSG_STANDBY_ON = 1
		};

		CLuaInstMisc() {};
		~CLuaInstMisc() {};
		static CLuaInstMisc* getInstance();
		static void LuaMiscRegister(lua_State *L);

		/* deprecated functions */
		static int strFind_old(lua_State *L);
		static int strSub_old(lua_State *L);
		static int enableInfoClock_old(lua_State *L);
		static int runScriptExt_old(lua_State *L);
		static int GetRevision_old(lua_State *L);
		static int checkVersion_old(lua_State *L);

	private:
		static CLuaMisc *MiscCheckData(lua_State *L, int n);
		static int MiscNew(lua_State *L);
		static int strFind(lua_State *L);
		static int strSub(lua_State *L);
		static int enableInfoClock(lua_State *L);
		static int runScriptExt(lua_State *L);
		static int GetRevision(lua_State *L);
		static int checkVersion(lua_State *L);
		static int postMsg(lua_State *L);
		static int MiscDelete(lua_State *L);

		static void miscFunctionDeprecated(lua_State *L, std::string oldFunc);
};

#endif //_LUAMISCFUNCS_H
