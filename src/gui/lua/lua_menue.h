/*
 * lua menue
 *
 * (C) 2014 by martii
 * (C) 2014-2015 M. Liebmann (micha-bbg)
 * (C) 2014 Sven Hoefer (svenhoefer)
 * (C) 2015 Jacek Jendrzej (SatBaby)
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

#ifndef _LUAMENUE_H
#define _LUAMENUE_H

struct CLuaMenuItem {
	int int_val;
	std::string str_val;
	std::string name;
};

class CLuaMenuChangeObserver : public CChangeObserver
{
	public:
		bool changeNotify(lua_State *, const std::string &, const std::string &, void *);
};

typedef std::pair<lua_Integer, CMenuItem*> itemmap_pair_t;
typedef std::map<lua_Integer, CMenuItem*> itemmap_t;
typedef itemmap_t::iterator itemmap_iterator_t;

class CLuaMenu
{
	public:
		CMenuWidget *m;
		CLuaMenuChangeObserver *observ;
		std::list<CLuaMenuItem> items;
		std::list<CMenuTarget *> targets;
		std::list<void *> tofree;
		itemmap_t itemmap;
		CLuaMenu();
		~CLuaMenu();
};

class CLuaMenuForwarder : public CMenuTarget
{
	private:
		void Init(lua_State *_L, std::string _luaAction, std::string _luaId);
	public:
		lua_State *L;
		std::string luaAction;
		std::string luaId;
		CLuaMenuForwarder(lua_State *L, std::string _luaAction, std::string _luaId);
		~CLuaMenuForwarder();
		int exec(CMenuTarget* parent=NULL, const std::string & actionKey="");
};

class CLuaMenuFilebrowser : public CLuaMenuForwarder
{
	private:
		std::string *value;
		bool dirMode;
		std::vector<std::string> filter;
		void Init(std::string *_value, bool _dirMode);
	public:
		CLuaMenuFilebrowser(lua_State *_L, std::string _luaAction, std::string _luaId, std::string *_value, bool _dirMode);
		int exec(CMenuTarget* parent=NULL, const std::string & actionKey="");
		void addFilter(std::string s) { filter.push_back(s); };
};

class CLuaMenuStringinput : public CLuaMenuForwarder
{
	private:
		std::string *value;
		std::string valid_chars;
		const char *name;
		const char *icon;
		bool sms;
		int size;
		CChangeObserver *observ;
		void Init(const char *_name, std::string *_value, int _size, std::string _valid_chars, CChangeObserver *_observ, const char *_icon, bool _sms);
	public:
		CLuaMenuStringinput(lua_State *_L, std::string _luaAction, std::string _luaId, const char *_name, std::string *_value, int _size, std::string _valid_chars, CChangeObserver *_observ, const char *_icon, bool _sms);
		int exec(CMenuTarget* parent=NULL, const std::string & actionKey="");
};

class CLuaMenuKeyboardinput : public CLuaMenuForwarder
{
	private:
		std::string *value;
		const char *name;
		const char *icon;
		int size;
		CChangeObserver *observ;
		std::string help, help2;
		void Init(const char *_name, std::string *_value, int _size, CChangeObserver *_observ, const char *_icon, std::string _help, std::string _help2);
	public:
		CLuaMenuKeyboardinput(lua_State *_L, std::string _luaAction, std::string _luaId, const char *_name, std::string *_value, int _size, CChangeObserver *_observ, const char *_icon, std::string _help, std::string _help2);
		int exec(CMenuTarget* parent=NULL, const std::string & actionKey="");
};

class CLuaInstMenu
{
	public:
		CLuaInstMenu() {};
		~CLuaInstMenu() {};
		static CLuaInstMenu* getInstance();
		static void MenuRegister(lua_State *L);

	private:
		static CLuaMenu *MenuCheck(lua_State *L, int n);
		static int MenuNew(lua_State *L);
		static int MenuAddKey(lua_State *L);
		static int MenuAddItem(lua_State *L);
		static int MenuExec(lua_State *L);
		static int MenuHide(lua_State *L);
		static int MenuSetActive(lua_State *L);
		static int MenuSetName(lua_State *L);
		static int MenuDelete(lua_State *L);
		static int MenuSetSelected(lua_State *L);
};

#endif //_LUAMENUE_H
