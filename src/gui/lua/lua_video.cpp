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

CLuaVideo *CLuaInstVideo::VideoCheckData(lua_State *L, int n)
{
	return *(CLuaVideo **) luaL_checkudata(L, n, LUA_VIDEO_CLASSNAME);
}

void CLuaInstVideo::LuaVideoRegister(lua_State *L)
{
	luaL_Reg meth[] = {
		{ "new",                    CLuaInstVideo::VideoNew },
		{ "setBlank",               CLuaInstVideo::setBlank },
		{ "ShowPicture",            CLuaInstVideo::ShowPicture },
		{ "StopPicture",            CLuaInstVideo::StopPicture },
		{ "PlayFile",               CLuaInstVideo::PlayFile },
		{ "zapitStopPlayBack",      CLuaInstVideo::zapitStopPlayBack },
		{ "channelRezap",           CLuaInstVideo::channelRezap },
		{ "createChannelIDfromUrl", CLuaInstVideo::createChannelIDfromUrl },
		{ "getNeutrinoMode",        CLuaInstVideo::getNeutrinoMode },
		{ "setSinglePlay",          CLuaInstVideo::setSinglePlay },
		{ "__gc",                   CLuaInstVideo::VideoDelete },
		{ NULL, NULL }
	};

	luaL_newmetatable(L, LUA_VIDEO_CLASSNAME);
	luaL_setfuncs(L, meth, 0);
	lua_pushvalue(L, -1);
	lua_setfield(L, -1, "__index");
	lua_setglobal(L, LUA_VIDEO_CLASSNAME);
}

int CLuaInstVideo::VideoNew(lua_State *L)
{
	CLuaVideo **udata = (CLuaVideo **) lua_newuserdata(L, sizeof(CLuaVideo *));
	*udata = new CLuaVideo();
	luaL_getmetatable(L, LUA_VIDEO_CLASSNAME);
	lua_setmetatable(L, -2);
	return 1;
}

int CLuaInstVideo::setBlank(lua_State *L)
{
	CLuaVideo *D = VideoCheckData(L, 1);
	if (!D) return 0;
	bool enable = true;
	int numargs = lua_gettop(L);
	if (numargs > 1)
		enable = _luaL_checkbool(L, 2);
	videoDecoder->setBlank(enable);
	return 0;
}

int CLuaInstVideo::ShowPicture(lua_State *L)
{
	CLuaVideo *D = VideoCheckData(L, 1);
	if (!D) return 0;
	const char *fname = luaL_checkstring(L, 2);
	CFrameBuffer::getInstance()->showFrame(fname);
	return 0;
}

int CLuaInstVideo::StopPicture(lua_State *L)
{
	CLuaVideo *D = VideoCheckData(L, 1);
	if (!D) return 0;
	CFrameBuffer::getInstance()->stopFrame();
	return 0;
}

int CLuaInstVideo::PlayFile(lua_State *L)
{
	CLuaVideo *D = VideoCheckData(L, 1);
	if (!D) return 0;
	LUA_DEBUG("CLuaInstVideo::%s %d\n", __func__, lua_gettop(L));
	int numargs = lua_gettop(L);

	if (numargs < 3) {
		printf("CLuaInstVideo::%s: not enough arguments (%d, expected 3)\n", __func__, numargs);
		return 0;
	}

	bool sp = false;
	if (luaL_testudata(L, 1, LUA_CLASSNAME) == NULL)
		sp = D->singlePlay;
	if ((sp == false) && (CMoviePlayerGui::getInstance().getBlockedFromPlugin() == false))
		CMoviePlayerGui::getInstance().setBlockedFromPlugin(true);

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

int CLuaInstVideo::zapitStopPlayBack(lua_State *L)
{
	CLuaVideo *D = VideoCheckData(L, 1);
	if (!D) return 0;
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

int CLuaInstVideo::channelRezap(lua_State *L)
{
	CLuaVideo *D = VideoCheckData(L, 1);
	if (!D) return 0;
	CNeutrinoApp::getInstance()->channelRezap();
	if (CNeutrinoApp::getInstance()->getMode() == CNeutrinoApp::mode_radio)
		CFrameBuffer::getInstance()->showFrame("radiomode.jpg");
	return 0;
}

int CLuaInstVideo::createChannelIDfromUrl(lua_State *L)
{
	CLuaVideo *D = VideoCheckData(L, 1);
	if (!D) return 0;
	int numargs = lua_gettop(L);
	if (numargs < 2) {
		printf("CLuaInstVideo::%s: no arguments\n", __func__);
		lua_pushnil(L);
		return 1;
	}

	const char *url = luaL_checkstring(L, 2);
	if (strlen(url) < 1 ) {
		lua_pushnil(L);
		return 1;
	}

	t_channel_id id = CREATE_CHANNEL_ID(0, 0, 0, url);
	char id_str[17];
	snprintf(id_str, sizeof(id_str), "%" PRIx64, id);

	lua_pushstring(L, id_str);
	return 1;
}

int CLuaInstVideo::getNeutrinoMode(lua_State *L)
{
	CLuaVideo *D = VideoCheckData(L, 1);
	if (!D) return 0;
	lua_pushinteger(L, (lua_Integer)CNeutrinoApp::getInstance()->getMode());
	return 1;
}

int CLuaInstVideo::setSinglePlay(lua_State *L)
{
	CLuaVideo *D = VideoCheckData(L, 1);
	if (!D) return 0;
	bool mode = true;
	int numargs = lua_gettop(L);
	if (numargs > 1)
		mode = _luaL_checkbool(L, 2);

	D->singlePlay = mode;
	return 0;
}

int CLuaInstVideo::VideoDelete(lua_State *L)
{
	CLuaVideo *D = VideoCheckData(L, 1);
	if (!D) return 0;
	delete D;
	return 0;
}


/* --------------------------------------------------------------
  deprecated functions
  --------------------------------------------------------------- */

//#define VIDEO_FUNC_DEPRECATED videoFunctionDeprecated
#define VIDEO_FUNC_DEPRECATED(...)

void CLuaInstVideo::videoFunctionDeprecated(lua_State *L, std::string oldFunc)
{
	std::string of = std::string("n:") + oldFunc + "()";
	std::string nf = std::string("video = video.new(); video:") + oldFunc + "()";
	functionDeprecated(L, of.c_str(), nf.c_str());
	printf("  [see also] \33[33m%s\33[0m\n", LUA_WIKI "/Kategorie:Lua:Neutrino-API:Videofunktionen:de");
}

int CLuaInstVideo::setBlank_old(lua_State *L)
{
	VIDEO_FUNC_DEPRECATED(L, "setBlank");
	return setBlank(L);
}

int CLuaInstVideo::ShowPicture_old(lua_State *L)
{
	VIDEO_FUNC_DEPRECATED(L, "ShowPicture");
	return ShowPicture(L);
}

int CLuaInstVideo::StopPicture_old(lua_State *L)
{
	VIDEO_FUNC_DEPRECATED(L, "StopPicture");
	return StopPicture(L);
}

int CLuaInstVideo::PlayFile_old(lua_State *L)
{
	VIDEO_FUNC_DEPRECATED(L, "PlayFile");
	return PlayFile(L);
}

int CLuaInstVideo::zapitStopPlayBack_old(lua_State *L)
{
	VIDEO_FUNC_DEPRECATED(L, "zapitStopPlayBack");
	return zapitStopPlayBack(L);
}

int CLuaInstVideo::channelRezap_old(lua_State *L)
{
	VIDEO_FUNC_DEPRECATED(L, "channelRezap");
	return channelRezap(L);
}

int CLuaInstVideo::createChannelIDfromUrl_old(lua_State *L)
{
	VIDEO_FUNC_DEPRECATED(L, "createChannelIDfromUrl");
	return createChannelIDfromUrl(L);
}

/* --------------------------------------------------------------- */
