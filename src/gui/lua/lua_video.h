/*
 * lua video functions
 *
 * (C) 2014 [CST ]Focus
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
#ifndef _LUAVIDEO_H
#define _LUAVIDEO_H

#if LUA_COMPAT_5_2
void lua_rawsetp (lua_State *L, int i, const void *p);
#endif

class CLuaVideo
{
	public:
		bool singlePlay;
		std::string infoFunc;
		CLuaVideo() { singlePlay=false; infoFunc=""; };
		~CLuaVideo() {};
};

class CLuaInstVideo
{
	public:
		CLuaInstVideo() {};
		~CLuaInstVideo() {};
		static CLuaInstVideo* getInstance();
		static void LuaVideoRegister(lua_State *L);
		static int channelRezap(lua_State *L);
		static bool execLuaInfoFunc(lua_State *L, int xres, int yres, int aspectRatio, int framerate);

		/* deprecated functions */
		static int setBlank_old(lua_State *L);
		static int ShowPicture_old(lua_State *L);
		static int StopPicture_old(lua_State *L);
		static int PlayFile_old(lua_State *L);
		static int zapitStopPlayBack_old(lua_State *L);
		static int channelRezap_old(lua_State *L);
		static int createChannelIDfromUrl_old(lua_State *L);

	private:
		static CLuaVideo *VideoCheckData(lua_State *L, int n);
		static int VideoNew(lua_State *L);
		static int setBlank(lua_State *L);
		static int ShowPicture(lua_State *L);
		static int StopPicture(lua_State *L);
		static int PlayFile(lua_State *L);
		static int setInfoFunc(lua_State *L);
		static int zapitStopPlayBack(lua_State *L);
		static int createChannelIDfromUrl(lua_State *L);
		static int getNeutrinoMode(lua_State *L);
		static int setSinglePlay(lua_State *L);
		static int VideoDelete(lua_State *L);

		static void videoFunctionDeprecated(lua_State *L, std::string oldFunc);
};

#endif //_LUAVIDEO_H
