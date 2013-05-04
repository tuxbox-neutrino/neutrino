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
#ifdef MARTII
#include <gui/widget/menue.h>
#endif

/* this is stored as userdata in the lua_State */
struct CLuaData
{
	CFBWindow *fbwin;
	CRCInput *rcinput;
};
#ifdef MARTII
struct CLuaMenueItem
{
	union
	{
		int i;
		char s[255];
	};
	std::string name;
	std::string Icon;
	std::string right_Icon;
};

class CLuaMenueChangeObserver : public CChangeObserver
{
	public:
		bool changeNotify(lua_State *, const std::string &, void *);
};

class CLuaMenue
{
	public:
		CMenuWidget *m;
		CLuaMenueChangeObserver *observ;
		std::list<CLuaMenueItem> items;
		std::list<CMenuTarget *> targets;
		std::list<void *> tofree;
		CLuaMenue();
		~CLuaMenue();
};

class CLuaMenueForwarder : public CMenuTarget
{
	private:
		lua_State *L;
		std::string luaAction;
	public:
		CLuaMenueForwarder(lua_State *L, std::string _luaAction);
		~CLuaMenueForwarder();
		int exec(CMenuTarget* parent, const std::string & actionKey);
};

class CLuaMenueFilebrowser : public CMenuTarget
{
	private:
		lua_State *L;
		std::string luaAction;
		char *value;
	public:
		CLuaMenueFilebrowser(lua_State *L, std::string _luaAction, char *_value);
		~CLuaMenueFilebrowser();
		int exec(CMenuTarget* parent, const std::string & actionKey);
};

class CLuaMenueStringinput : public CMenuTarget
{
	private:
		lua_State *L;
		std::string luaAction;
		const char *name;
		char *value;
		int size;
		std::string valid_chars;
		CChangeObserver *observ;
		const char *icon;
	public:
		CLuaMenueStringinput(lua_State *_L, std::string _luaAction, const char *_name, char *_value, int _size, std::string _valid_chars, CChangeObserver *_observ, const char *_icon);
		~CLuaMenueStringinput();
		int exec(CMenuTarget* /*parent*/, const std::string & /*actionKey*/);
};
#endif

/* inspired by Steve Kemp http://www.steve.org.uk/ */
class CLuaInstance
{
	static const char className[];
	static const luaL_Reg methods[];
#ifdef MARTII
	static const luaL_Reg menue_methods[];
#endif
	static CLuaData *CheckData(lua_State *L, int narg);
public:
	CLuaInstance();
	~CLuaInstance();
	void runScript(const char *fileName);

private:
	lua_State* lua;
	void registerFunctions();

	static int NewWindow(lua_State *L);
	static int PaintBox(lua_State *L);
	static int PaintIcon(lua_State *L);
	static int RenderString(lua_State *L);
	static int FontHeight(lua_State *L);
	static int GetInput(lua_State *L);
	static int GCWindow(lua_State *L);
#ifdef MARTII
	void MenueRegister(lua_State *L);
	static int MenueNew(lua_State *L);
	static int MenueDelete(lua_State *L);
	static int MenueAddItem(lua_State *L);
	static int MenueHide(lua_State *L);
	static int MenueExec(lua_State *L);
	static CLuaMenue *MenueCheck(lua_State *L, int n);
	static bool tableLookupString(lua_State*, const char*, std::string&);
	static bool tableLookupInt(lua_State*, const char*, int&);
#endif
};

#endif /* _LUAINSTANCE_H */
