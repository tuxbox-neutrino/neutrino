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

#include <config.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <unistd.h>

#include <global.h>
#include <system/debug.h>
#include <gui/movieplayer.h>
#include <zapit/zapit.h>
#include <video.h>
#include <neutrino.h>

#include "luainstance.h"
#include "lua_video.h"

extern cVideo * videoDecoder;

CLuaInstVideo* CLuaInstVideo::getInstance()
{
	static CLuaInstVideo* LuaInstVideo = NULL;

	if(!LuaInstVideo)
		LuaInstVideo = new CLuaInstVideo();
	return LuaInstVideo;
}

CLuaData *CLuaInstVideo::CheckData(lua_State *L, int narg)
{
	luaL_checktype(L, narg, LUA_TUSERDATA);
	void *ud = luaL_checkudata(L, narg, LUA_CLASSNAME);
	if (!ud)
		fprintf(stderr, "[CLuaInstVideo::%s] wrong type %p, %d, %s\n", __func__, L, narg, LUA_CLASSNAME);
	return *(CLuaData **)ud;  // unbox pointer
}

int CLuaInstVideo::setBlank_old(lua_State *L)
{
	bool enable = true;
	int numargs = lua_gettop(L);
	if (numargs > 1)
		enable = _luaL_checkbool(L, 2);
	videoDecoder->setBlank(enable);
	return 0;
}

int CLuaInstVideo::ShowPicture_old(lua_State *L)
{
	const char *fname = luaL_checkstring(L, 2);
	CFrameBuffer::getInstance()->showFrame(fname);
	return 0;
}

int CLuaInstVideo::StopPicture_old(lua_State */*L*/)
{
	CFrameBuffer::getInstance()->stopFrame();
	return 0;
}

int CLuaInstVideo::PlayFile_old(lua_State *L)
{
	printf("CLuaInstVideo::%s %d\n", __func__, lua_gettop(L));
	int numargs = lua_gettop(L);

	if (numargs < 3) {
		printf("CLuaInstVideo::%s: not enough arguments (%d, expected 3)\n", __func__, numargs);
		return 0;
	}

	CLuaData *W = CheckData(L, 1);
	if (!W)
		return 0;
	if (W->moviePlayerBlocked == false) {
		CMoviePlayerGui::getInstance().setBlockedFromPlugin(true);
		W->moviePlayerBlocked = true;
	}

	const char *title;
	const char *info1 = "";
	const char *info2 = "";
	const char *fname;

	title = luaL_checkstring(L, 2);
	fname = luaL_checkstring(L, 3);
	if (numargs > 3)
		info1 = luaL_checkstring(L, 4);
	if (numargs > 4)
		info2 = luaL_checkstring(L, 5);
	printf("CLuaInstVideo::%s: title %s file %s\n", __func__, title, fname);
	std::string st(title);
	std::string si1(info1);
	std::string si2(info2);
	std::string sf(fname);
	CMoviePlayerGui::getInstance().SetFile(st, sf, si1, si2);
	CMoviePlayerGui::getInstance().exec(NULL, "http_lua");
	int ret = CMoviePlayerGui::getInstance().getKeyPressed();
	lua_pushinteger(L, ret);
	return 1;
}

int CLuaInstVideo::zapitStopPlayBack_old(lua_State *L)
{
	bool stop = true;
	int numargs = lua_gettop(L);
	if (numargs > 1)
		stop = _luaL_checkbool(L, 2);
	if (stop) {
		CMoviePlayerGui::getInstance().stopPlayBack();
		g_Zapit->stopPlayBack();
	}
	else
		g_Zapit->startPlayBack();
	return 0;
}

int CLuaInstVideo::channelRezap_old(lua_State */*L*/)
{
	CNeutrinoApp::getInstance()->channelRezap();
	if (CNeutrinoApp::getInstance()->getMode() == CNeutrinoApp::mode_radio)
		CFrameBuffer::getInstance()->showFrame("radiomode.jpg");
	return 0;
}
