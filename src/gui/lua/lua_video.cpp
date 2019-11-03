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
#include <gui/widget/msgbox.h>
#include <zapit/zapit.h>
#include <hardware/video.h>
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
	void* ret = luaL_testudata(L, n, LUA_VIDEO_CLASSNAME);
	if (ret == NULL)
		return NULL;
	else
		return *(CLuaVideo **) ret;
}

void CLuaInstVideo::LuaVideoRegister(lua_State *L)
{
	luaL_Reg meth[] = {
		{ "new",                    CLuaInstVideo::VideoNew },
		{ "setBlank",               CLuaInstVideo::setBlank },
		{ "ShowPicture",            CLuaInstVideo::ShowPicture },
		{ "StopPicture",            CLuaInstVideo::StopPicture },
		{ "PlayFile",               CLuaInstVideo::PlayFile },
		{ "setInfoFunc",            CLuaInstVideo::setInfoFunc },
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
	/* workaround for deprecated functions */
	CLuaVideo *D;
	if (luaL_testudata(L, 1, LUA_CLASSNAME) == NULL) {
		D = VideoCheckData(L, 1);
		if (!D) return 0;
	}
	/* CLuaVideo *D = VideoCheckData(L, 1);
	if (!D) return 0; */
	bool enable = true;
	int numargs = lua_gettop(L);
	if (numargs > 1)
		enable = _luaL_checkbool(L, 2);
	videoDecoder->setBlank(enable);
	return 0;
}

int CLuaInstVideo::ShowPicture(lua_State *L)
{
	/* workaround for deprecated functions */
	CLuaVideo *D;
	if (luaL_testudata(L, 1, LUA_CLASSNAME) == NULL) {
		D = VideoCheckData(L, 1);
		if (!D) return 0;
	}
	/* CLuaVideo *D = VideoCheckData(L, 1);
	if (!D) return 0; */
	const char *fname = luaL_checkstring(L, 2);
	CFrameBuffer::getInstance()->showFrame(fname);
	return 0;
}

int CLuaInstVideo::StopPicture(lua_State *L)
{
	/* workaround for deprecated functions */
	CLuaVideo *D;
	if (luaL_testudata(L, 1, LUA_CLASSNAME) == NULL) {
		D = VideoCheckData(L, 1);
		if (!D) return 0;
	}
	/* CLuaVideo *D = VideoCheckData(L, 1);
	if (!D) return 0; */
	CFrameBuffer::getInstance()->stopFrame();
	return 0;
}

int CLuaInstVideo::PlayFile(lua_State *L)
{
	/* workaround for deprecated functions */
	CLuaVideo *D = NULL;
	if (luaL_testudata(L, 1, LUA_CLASSNAME) == NULL) {
		D = VideoCheckData(L, 1);
		if (!D) return 0;
	}
	/* CLuaVideo *D = VideoCheckData(L, 1);
	if (!D) return 0; */
	LUA_DEBUG("CLuaInstVideo::%s %d\n", __func__, lua_gettop(L));
	int numargs = lua_gettop(L);

	if (numargs < 3) {
		printf("CLuaInstVideo::%s: not enough arguments (%d, expected 3)\n", __func__, numargs);
		return 0;
	}
	const char *errmsg = "is not a string.";
	if(!lua_isstring(L,2)){
		printf("CLuaInstVideo::%s: argument 1 %s\n", __func__, errmsg);
			return 0;
	}
	if(!lua_isstring(L,3)){
		printf("CLuaInstVideo::%s: argument 2 %s\n", __func__, errmsg);
			return 0;
	}
	if(numargs > 3 && !lua_isstring(L,4)){
		printf("CLuaInstVideo::%s: argument 3 %s\n", __func__, errmsg);
			return 0;
	}
	if(numargs > 4 && !lua_isstring(L,5)){
		printf("CLuaInstVideo::%s: argument 4 %s\n", __func__, errmsg);
			return 0;
	}

	bool sp = false;
	if (luaL_testudata(L, 1, LUA_CLASSNAME) == NULL)
		if (D)
			sp = D->singlePlay;
	if ((sp == false) && (CMoviePlayerGui::getInstance().getBlockedFromPlugin() == false))
		CMoviePlayerGui::getInstance().setBlockedFromPlugin(true);

	const char *title;
	const char *info1 = "";
	const char *info2 = "";
	const char *fname;
	const char *fname2 = "";

	title = luaL_checkstring(L, 2);
	fname = luaL_checkstring(L, 3);
	if (numargs > 3)
		info1 = luaL_checkstring(L, 4);
	if (numargs > 4)
		info2 = luaL_checkstring(L, 5);
	if (numargs > 5)
		fname2 = luaL_checkstring(L, 6);
	printf("CLuaInstVideo::%s: title %s file %s\n", __func__, title, fname);
	std::string st(title);
	std::string si1(info1);
	std::string si2(info2);
	std::string sf(fname);
	std::string sf2(fname2);
	if (D != NULL && !D->infoFunc.empty())
		CMoviePlayerGui::getInstance().setLuaInfoFunc(L, true);
	CMoviePlayerGui::getInstance().SetFile(st, sf, si1, si2,sf2);
	CMoviePlayerGui::getInstance().exec(NULL, "http_lua");
	CMoviePlayerGui::getInstance().setLuaInfoFunc(L, false);
	if (D != NULL && !D->infoFunc.empty())
		D->infoFunc = "";
	int ret = CMoviePlayerGui::getInstance().getKeyPressed();
	lua_pushinteger(L, ret);
	return 1;
}

int CLuaInstVideo::setInfoFunc(lua_State *L)
{
	CLuaVideo *D = VideoCheckData(L, 1);
	if (!D) return 0;

	int numargs = lua_gettop(L);
	if (numargs < 2) {
		printf("CLuaInstVideo::%s: not enough arguments (%d, expected 1)\n", __func__, numargs-1);
		return 0;
	}

	D->infoFunc = luaL_checkstring(L, 2);
	return 0;
}

bool CLuaInstVideo::execLuaInfoFunc(lua_State *L, int xres, int yres, int aspectRatio, int framerate)
{
	CLuaVideo *D = VideoCheckData(L, 1);
	if (!D) return false;

	lua_getglobal(L, D->infoFunc.c_str());
	lua_pushinteger(L, (lua_Integer)xres);
	lua_pushinteger(L, (lua_Integer)yres);
	lua_pushinteger(L, (lua_Integer)aspectRatio);
	lua_pushinteger(L, (lua_Integer)framerate);
	int status = lua_pcall(L, 4, 0, 0);
	if (status) {
		char msg[1024];
		lua_Debug ar;
		lua_getstack(L, 1, &ar);
		lua_getinfo(L, "Sl", &ar);
		memset(msg, '\0', sizeof(msg));
		bool isString = lua_isstring(L,-1);
		const char *null = "NULL";
		snprintf(msg, sizeof(msg)-1, "[%s:%d] error running function '%s': %s", ar.short_src, ar.currentline, D->infoFunc.c_str(), isString ? lua_tostring(L, -1):null);
		fprintf(stderr, "[CLuaInstVideo::%s:%d] %s\n", __func__, __LINE__, msg);
		DisplayErrorMessage(msg);
		return false;
	}
	return true;
}

int CLuaInstVideo::zapitStopPlayBack(lua_State *L)
{
	/* workaround for deprecated functions */
	CLuaVideo *D;
	if (luaL_testudata(L, 1, LUA_CLASSNAME) == NULL) {
		D = VideoCheckData(L, 1);
		if (!D) return 0;
	}
	/* CLuaVideo *D = VideoCheckData(L, 1);
	if (!D) return 0; */
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
	/* workaround for deprecated functions */
	CLuaVideo *D;
	if (luaL_testudata(L, 1, LUA_CLASSNAME) == NULL) {
		D = VideoCheckData(L, 1);
		if (!D) return 0;
	}
	/* CLuaVideo *D = VideoCheckData(L, 1);
	if (!D) return 0; */
	CNeutrinoApp::getInstance()->channelRezap();
	if (CNeutrinoApp::getInstance()->getMode() == NeutrinoModes::mode_radio)
		CFrameBuffer::getInstance()->showFrame("radiomode.jpg");
	return 0;
}

int CLuaInstVideo::createChannelIDfromUrl(lua_State *L)
{
	/* workaround for deprecated functions */
	CLuaVideo *D;
	if (luaL_testudata(L, 1, LUA_CLASSNAME) == NULL) {
		D = VideoCheckData(L, 1);
		if (!D) return 0;
	}
	/* CLuaVideo *D = VideoCheckData(L, 1);
	if (!D) return 0; */
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

#define VIDEO_FUNC_DEPRECATED videoFunctionDeprecated

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
