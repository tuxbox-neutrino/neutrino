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
#include <gui/widget/hintbox.h>
#include <gui/widget/messagebox.h>
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
	union //value
	{
		int i;
		char s[255];
	};
	std::string name;
};

class CLuaMenueChangeObserver : public CChangeObserver
{
	public:
		bool changeNotify(lua_State *, const std::string &, const std::string &, void *);
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
	public:
		lua_State *L;
		std::string luaAction;
		std::string luaId;
		CLuaMenueForwarder(lua_State *L, std::string _luaAction, std::string _luaId);
		~CLuaMenueForwarder();
		int exec(CMenuTarget* parent, const std::string & actionKey);
};

class CLuaMenueFilebrowser : public CLuaMenueForwarder
{
	private:
		char *value;
		bool dirMode;
		std::vector<std::string> filter;
	public:
		CLuaMenueFilebrowser(lua_State *_L, std::string _luaAction, std::string _luaId, char *_value, bool _dirMode);
		int exec(CMenuTarget* parent, const std::string & actionKey);
		void addFilter(std::string s) { filter.push_back(s); };
};

class CLuaMenueStringinput : public CLuaMenueForwarder
{
	private:
		char *value;
		std::string valid_chars;
		const char *name;
		const char *icon;
		bool sms;
		int size;
		CChangeObserver *observ;
	public:
		CLuaMenueStringinput(lua_State *_L, std::string _luaAction, std::string _luaId, const char *_name, char *_value, int _size, std::string _valid_chars, CChangeObserver *_observ, const char *_icon, bool _sms);
		int exec(CMenuTarget* parent, const std::string & actionKey);
};

class CLuaHintbox
{
	public:
		CHintBox *b;
		char *caption;
		CLuaHintbox();
		~CLuaHintbox();
};

class CLuaMessagebox
{
	public:
		CMessageBox *b;
		CLuaMessagebox();
		~CLuaMessagebox();
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
	static int Blit(lua_State *L);
	static int GetLanguage(lua_State *L);

	void MenueRegister(lua_State *L);
	static int MenueNew(lua_State *L);
	static int MenueDelete(lua_State *L);
	static int MenueAddKey(lua_State *L);
	static int MenueAddItem(lua_State *L);
	static int MenueHide(lua_State *L);
	static int MenueExec(lua_State *L);
	static CLuaMenue *MenueCheck(lua_State *L, int n);

	void HintboxRegister(lua_State *L);
	static int HintboxNew(lua_State *L);
	static int HintboxDelete(lua_State *L);
	static int HintboxExec(lua_State *L);
	static int HintboxPaint(lua_State *L);
	static int HintboxHide(lua_State *L);
	static CLuaHintbox *HintboxCheck(lua_State *L, int n);

	void MessageboxRegister(lua_State *L);
	static int MessageboxExec(lua_State *L);
	static CLuaMessagebox *MessageboxCheck(lua_State *L, int n);

	static bool tableLookup(lua_State*, const char*, std::string&);
	static bool tableLookup(lua_State*, const char*, int&);
#endif
};

#endif /* _LUAINSTANCE_H */
