/*
 * lua progress window
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
#include <gui/widget/progresswindow.h>
#include <neutrino.h>

#include "luainstance.h"
#include "lua_progresswindow.h"

CLuaInstProgressWindow* CLuaInstProgressWindow::getInstance()
{
	static CLuaInstProgressWindow* LuaInstProgressWindow = NULL;

	if(!LuaInstProgressWindow)
		      LuaInstProgressWindow = new CLuaInstProgressWindow();
	return LuaInstProgressWindow;
}

void CLuaInstProgressWindow::ProgressWindowRegister(lua_State *L)
{
	luaL_Reg meth[] = {
		{ "new",             CLuaInstProgressWindow::CProgressWindowNew },
		{ "paint",           CLuaInstProgressWindow::CProgressWindowPaint },
		{ "hide",            CLuaInstProgressWindow::CProgressWindowHide },
		{ "showStatus",      CLuaInstProgressWindow::CProgressWindowShowLocalStatus},
		{ "showLocalStatus", CLuaInstProgressWindow::CProgressWindowShowLocalStatus},
		{ "showGlobalStatus",CLuaInstProgressWindow::CProgressWindowShowGlobalStatus },
		{ "setTitle",        CLuaInstProgressWindow::CProgressWindowSetTitle},
		{ "__gc",            CLuaInstProgressWindow::CProgressWindowDelete },
		{ NULL,              NULL }
	};

	luaL_newmetatable(L, "cprogresswindow");
	luaL_setfuncs(L, meth, 0);
	lua_pushvalue(L, -1);
	lua_setfield(L, -1, "__index");
	lua_setglobal(L, "cprogresswindow");
}


int CLuaInstProgressWindow::CProgressWindowNew(lua_State *L)
{
	lua_assert(lua_istable(L,1));

	std::string name = "";
	tableLookup(L, "name", name) || tableLookup(L, "title", name) || tableLookup(L, "caption", name);

	CLuaCProgressWindow **udata = (CLuaCProgressWindow **) lua_newuserdata(L, sizeof(CLuaCProgressWindow *));
	*udata = new CLuaCProgressWindow();

#if 0
	/* Enable when pu/fb-setmode branch is merged in master */
	(*udata)->w = new CProgressWindow(name);
#else
	(*udata)->w = new CProgressWindow();
	(*udata)->w->setTitle(name);
#endif

	luaL_getmetatable(L, "cprogresswindow");
	lua_setmetatable(L, -2);
	return 1;
}

CLuaCProgressWindow *CLuaInstProgressWindow::CProgressWindowCheck(lua_State *L, int n)
{
	return *(CLuaCProgressWindow **) luaL_checkudata(L, n, "cprogresswindow");
}

int CLuaInstProgressWindow::CProgressWindowPaint(lua_State *L)
{
	lua_assert(lua_istable(L,1));
	CLuaCProgressWindow *D = CProgressWindowCheck(L, 1);
	if (!D) return 0;

	bool do_save_bg = true;
	if (!tableLookup(L, "do_save_bg", do_save_bg)) {
		std::string tmp = "true";
		if (tableLookup(L, "do_save_bg", tmp))
			paramBoolDeprecated(L, tmp.c_str());
		do_save_bg = (tmp == "true" || tmp == "1" || tmp == "yes");
	}
	D->w->paint(do_save_bg);
	return 0;
}

int CLuaInstProgressWindow::CProgressWindowHide(lua_State *L)
{
	lua_assert(lua_istable(L,1));
	CLuaCProgressWindow *D = CProgressWindowCheck(L, 1);
	if (!D) return 0;

	bool tmp1 = false;
	std::string tmp2 = "false";
	if ((tableLookup(L, "no_restore", tmp1)) || (tableLookup(L, "no_restore", tmp2)))
		obsoleteHideParameter(L);

	D->w->hide();
	return 0;
}

int CLuaInstProgressWindow::CProgressWindowSetTitle(lua_State *L)
{
	lua_assert(lua_istable(L,1));
	CLuaCProgressWindow *D = CProgressWindowCheck(L, 1);
	if (!D) return 0;

	std::string name = "";
	tableLookup(L, "name", name) || tableLookup(L, "title", name) || tableLookup(L, "caption", name);

	D->w->setTitle(name);
	return 0;
}

int CLuaInstProgressWindow::CProgressWindowShowStatusInternal(lua_State *L, bool local)
{
	lua_assert(lua_istable(L,1));
	CLuaCProgressWindow *D = CProgressWindowCheck(L, 1);
	if (!D) return 0;

	lua_Unsigned prog;
	std::string statusText  = std::string();
	lua_Integer max 	= 100 ;

	tableLookup(L, "prog", prog);
	tableLookup(L, "statusText", statusText);
	tableLookup(L, "max", max);

	if (local)
		D->w->showLocalStatus(prog, max, statusText);
	else
		D->w->showGlobalStatus(prog, max, statusText);

	return 0;
}

int CLuaInstProgressWindow::CProgressWindowShowLocalStatus(lua_State *L)
{
	return CProgressWindowShowStatusInternal(L, true);
}

int CLuaInstProgressWindow::CProgressWindowShowGlobalStatus(lua_State *L)
{
	return CProgressWindowShowStatusInternal(L, false);
}

int CLuaInstProgressWindow::CProgressWindowDelete(lua_State *L)
{
	LUA_DEBUG("CLuaInstProgressWindow::%s %d\n", __func__, lua_gettop(L));
	CLuaCProgressWindow *D = CProgressWindowCheck(L, 1);
	if (!D) return 0;
	delete D;
	return 0;
}
