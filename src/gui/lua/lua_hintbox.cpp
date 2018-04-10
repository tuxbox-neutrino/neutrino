/*
 * lua hintbox
 *
 * (C) 2014 by martii
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

#include <config.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <unistd.h>

#include <global.h>
#include <system/debug.h>
#include <gui/widget/hintbox.h>
#include <neutrino.h>

#include "luainstance.h"
#include "lua_hintbox.h"

CLuaInstHintbox* CLuaInstHintbox::getInstance()
{
	static CLuaInstHintbox* LuaInstHintbox = NULL;

	if(!LuaInstHintbox)
		LuaInstHintbox = new CLuaInstHintbox();
	return LuaInstHintbox;
}

void CLuaInstHintbox::HintboxRegister(lua_State *L)
{
	luaL_Reg meth[] = {
		{ "new", CLuaInstHintbox::HintboxNew },
		{ "exec", CLuaInstHintbox::HintboxExec },
		{ "paint", CLuaInstHintbox::HintboxPaint },
		{ "hide", CLuaInstHintbox::HintboxHide },
		{ "__gc", CLuaInstHintbox::HintboxDelete },
		{ NULL, NULL }
	};

	luaL_newmetatable(L, "hintbox");
	luaL_setfuncs(L, meth, 0);
	lua_pushvalue(L, -1);
	lua_setfield(L, -1, "__index");
	lua_setglobal(L, "hintbox");
}

CLuaHintbox *CLuaInstHintbox::HintboxCheck(lua_State *L, int n)
{
	return *(CLuaHintbox **) luaL_checkudata(L, n, "hintbox");
}

CLuaHintbox::CLuaHintbox()
{
	caption = NULL;
	b = NULL;
}

CLuaHintbox::~CLuaHintbox()
{
	if (caption)
		free(caption);
	delete b;
}

int CLuaInstHintbox::HintboxNew(lua_State *L)
{
	lua_assert(lua_istable(L,1));

	std::string name, text, icon = std::string(NEUTRINO_ICON_INFO);
	tableLookup(L, "name", name) || tableLookup(L, "title", name) || tableLookup(L, "caption", name);
	tableLookup(L, "text", text);
	tableLookup(L, "icon", icon);
	lua_Integer width = 450;
	tableLookup(L, "width", width);

	CLuaHintbox **udata = (CLuaHintbox **) lua_newuserdata(L, sizeof(CLuaHintbox *));
	*udata = new CLuaHintbox();
	(*udata)->caption = strdup(name.c_str());
	(*udata)->b = new CHintBox((*udata)->caption, text.c_str(), width, icon.c_str());
	luaL_getmetatable(L, "hintbox");
	lua_setmetatable(L, -2);
	return 1;
}

int CLuaInstHintbox::HintboxDelete(lua_State *L)
{
	CLuaHintbox *D = HintboxCheck(L, 1);
	if (!D) return 0;
	delete D;
	return 0;
}

int CLuaInstHintbox::HintboxPaint(lua_State *L)
{
	CLuaHintbox *D = HintboxCheck(L, 1);
	if (!D) return 0;
	D->b->paint();
	return 0;
}

int CLuaInstHintbox::HintboxHide(lua_State *L)
{
	CLuaHintbox *D = HintboxCheck(L, 1);
	if (!D) return 0;
	D->b->hide();
	return 0;
}

int CLuaInstHintbox::HintboxExec(lua_State *L)
{
	CLuaHintbox *D = HintboxCheck(L, 1);
	if (!D) return 0;
	int timeout = -1;
	if (lua_isnumber(L, -1))
		timeout = (int) lua_tonumber(L, -1);
	D->b->paint();

	// copied from gui/widget/hintbox.cpp
	neutrino_msg_t msg;
	neutrino_msg_data_t data;
	if ( timeout == -1 )
		timeout = 5; /// default timeout 5 sec
	//timeout = g_settings.handling_infobar[SNeutrinoSettings::HANDLING_INFOBAR];

	uint64_t timeoutEnd = CRCInput::calcTimeoutEnd(timeout);

	int res = messages_return::none;

	while ( ! ( res & ( messages_return::cancel_info | messages_return::cancel_all ) ) ) {
		g_RCInput->getMsgAbsoluteTimeout( &msg, &data, &timeoutEnd );

		if ((msg == CRCInput::RC_timeout) || (msg == CRCInput::RC_ok))
			res = messages_return::cancel_info;
		else if (msg == CRCInput::RC_home)
			res = messages_return::cancel_all;
		else if (/*(D->b->has_scrollbar()) &&*/ ((msg == CRCInput::RC_up) || (msg == CRCInput::RC_down))) {
			if (msg == CRCInput::RC_up)
				D->b->scroll_up();
			else
				D->b->scroll_down();
		} else if (CNeutrinoApp::getInstance()->listModeKey(msg)) {
			// do nothing
		} else if (msg == CRCInput::RC_mode) {
			break;
		} else if ((msg == CRCInput::RC_next) || (msg == CRCInput::RC_prev)) {
			res = messages_return::cancel_all;
			g_RCInput->postMsg(msg, data);
		} else {
			res = CNeutrinoApp::getInstance()->handleMsg(msg, data);
			if (res & messages_return::unhandled) {

				// leave here and handle above...
				g_RCInput->postMsg(msg, data);
				res = messages_return::cancel_all;
			}
		}
	}
	D->b->hide();
	return 0;
}
