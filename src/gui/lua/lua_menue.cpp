/*
 * lua menue
 *
 * (C) 2014 by martii
 * (C) 2014-2015 M. Liebmann (micha-bbg)
 * (C) 2016 Sven Hoefer (svenhoefer)
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

#include <config.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <unistd.h>

#include <global.h>
#include <gui/widget/keyboard_input.h>
#include <gui/widget/msgbox.h>
#include <gui/filebrowser.h>
#include <system/debug.h>
#include <system/helpers.h>
#include <neutrino.h>

#include "luainstance.h"
#include "lua_menue.h"

CLuaInstMenu* CLuaInstMenu::getInstance()
{
	static CLuaInstMenu* LuaInstMenu = NULL;

	if(!LuaInstMenu)
		LuaInstMenu = new CLuaInstMenu();
	return LuaInstMenu;
}

bool CLuaMenuChangeObserver::changeNotify(lua_State *L, const std::string &luaAction, const std::string &luaId, void *Data)
{
	const char *optionValue = (const char *) Data;
	lua_pushglobaltable(L);
	lua_getfield(L, -1, luaAction.c_str());
	lua_remove(L, -2);
	lua_pushstring(L, luaId.c_str());
	lua_pushstring(L, optionValue);
	int status = lua_pcall(L, 2 /* two args */, 1 /* one result */, 0);
	if (status) {
		bool isString = lua_isstring(L,-1);
		const char *null = "NULL";
		fprintf(stderr, "[CLuaMenuChangeObserver::%s:%d] error in script: %s\n", __func__, __LINE__, isString ? lua_tostring(L, -1): null);
		DisplayErrorMessage(isString ? lua_tostring(L, -1):null, "Lua Script Error:");
	}
	double res = lua_isnumber(L, -1) ? lua_tonumber(L, -1) : 0;
	return (((int)res == menu_return::RETURN_REPAINT) || ((int)res == menu_return::RETURN_EXIT_REPAINT));
}

void CLuaInstMenu::MenuRegister(lua_State *L)
{
	luaL_Reg meth[] = {
		{ "new",       CLuaInstMenu::MenuNew },
		{ "addKey",    CLuaInstMenu::MenuAddKey },
		{ "addItem",   CLuaInstMenu::MenuAddItem },
		{ "exec",      CLuaInstMenu::MenuExec },
		{ "hide",      CLuaInstMenu::MenuHide },
		{ "setActive", CLuaInstMenu::MenuSetActive },
		{ "setName",   CLuaInstMenu::MenuSetName },
		{ "__gc",      CLuaInstMenu::MenuDelete },
		{ NULL, NULL }
	};

	luaL_newmetatable(L, "menu");
	luaL_setfuncs(L, meth, 0);
	lua_pushvalue(L, -1);
	lua_setfield(L, -1, "__index");
	lua_setglobal(L, "menu");

	// keep misspelled "menue" for backwards-compatibility
	luaL_newmetatable(L, "menu");
	luaL_setfuncs(L, meth, 0);
	lua_pushvalue(L, -1);
	lua_setfield(L, -1, "__index");
	lua_setglobal(L, "menue");
}

CLuaMenu *CLuaInstMenu::MenuCheck(lua_State *L, int n)
{
	return *(CLuaMenu **) luaL_checkudata(L, n, "menu");
}

CLuaMenu::CLuaMenu()
{
	m      = NULL;
	observ = new CLuaMenuChangeObserver();
}

CLuaMenu::~CLuaMenu()
{
	itemmap.clear();
	delete m;
	delete observ;
}

CLuaMenuForwarder::CLuaMenuForwarder(lua_State *_L, std::string _luaAction, std::string _luaId)
{
	Init(_L, _luaAction, _luaId);
}

void CLuaMenuForwarder::Init(lua_State *_L, std::string _luaAction, std::string _luaId)
{
	L         = _L;
	luaAction = _luaAction;
	luaId     = _luaId;
}

CLuaMenuForwarder::~CLuaMenuForwarder()
{
}

int CLuaMenuForwarder::exec(CMenuTarget* /*parent*/, const std::string & /*actionKey*/)
{
	int res = menu_return::RETURN_REPAINT;
	if (!luaAction.empty()) {
		lua_pushglobaltable(L);
		lua_getfield(L, -1, luaAction.c_str());
		lua_remove(L, -2);
		lua_pushstring(L, luaId.c_str());
		int status = lua_pcall(L, 1 /* one arg */, 1 /* one result */, 0);
		if (status) {
			bool isString = lua_isstring(L,-1);
			const char *null = "NULL";
			fprintf(stderr, "[CLuaMenuForwarder::%s:%d] error in script: %s\n", __func__, __LINE__, isString ? lua_tostring(L, -1):null);
			DisplayErrorMessage(isString ? lua_tostring(L, -1):null, "Lua Script Error:");
		}
		if (lua_isnumber(L, -1))
			res = (int) lua_tonumber(L, -1);
		lua_pop(L, 1);
	}
	return res;
}

CLuaMenuFilebrowser::CLuaMenuFilebrowser(lua_State *_L, std::string _luaAction, std::string _luaId, std::string *_value, bool _dirMode) : CLuaMenuForwarder(_L, _luaAction, _luaId)
{
	Init(_value, _dirMode);
}

void CLuaMenuFilebrowser::Init(std::string *_value, bool _dirMode)
{
	value   = _value;
	dirMode = _dirMode;
}

int CLuaMenuFilebrowser::exec(CMenuTarget* /*parent*/, const std::string& /*actionKey*/)
{
	int res = menu_return::RETURN_REPAINT;
	CFileBrowser fileBrowser;
	fileBrowser.Dir_Mode = dirMode;

	CFileFilter fileFilter;
	for (std::vector<std::string>::iterator it = filter.begin(); it != filter.end(); ++it)
		fileFilter.addFilter(*it);
	if (!filter.empty())
		fileBrowser.Filter = &fileFilter;

	std::string tmpValue = (dirMode) ? *value : getPathName(*value);
	if (fileBrowser.exec(tmpValue.c_str()) == true)
		*value = fileBrowser.getSelectedFile()->Name;

	if (!luaAction.empty()) {
		lua_pushglobaltable(L);
		lua_getfield(L, -1, luaAction.c_str());
		lua_remove(L, -2);
		lua_pushstring(L, luaId.c_str());
		lua_pushstring(L, value->c_str());
		int status = lua_pcall(L, 2 /* two arg */, 1 /* one result */, 0);
		if (status) {
			bool isString = lua_isstring(L,-1);
			const char *null = "NULL";
			fprintf(stderr, "[CLuaMenuFilebrowser::%s:%d] error in script: %s\n", __func__, __LINE__, isString ? lua_tostring(L, -1):null);
			DisplayErrorMessage(isString ? lua_tostring(L, -1):null, "Lua Script Error:");
		}
		if (lua_isnumber(L, -1))
			res = (int) lua_tonumber(L, -1);
		lua_pop(L, 1);
	}
	return res;
}

CLuaMenuStringinput::CLuaMenuStringinput(lua_State *_L, std::string _luaAction, std::string _luaId, const char *_name, std::string *_value, int _size, std::string _valid_chars, CChangeObserver *_observ, const char *_icon, bool _sms) : CLuaMenuForwarder(_L, _luaAction, _luaId)
{
	Init(_name, _value, _size, _valid_chars, _observ, _icon, _sms);
}

void CLuaMenuStringinput::Init(const char *_name, std::string *_value, int _size, std::string _valid_chars, CChangeObserver *_observ, const char *_icon, bool _sms)
{
	name        = _name;
	value       = _value;
	size        = _size;
	valid_chars = _valid_chars;
	icon        = _icon;
	observ      = _observ;
	sms         = _sms;
}

int CLuaMenuStringinput::exec(CMenuTarget* /*parent*/, const std::string & /*actionKey*/)
{
	int res = menu_return::RETURN_REPAINT;
	CStringInput *i;
	if (sms)
		i = new CStringInputSMS((char *)name, value, size,
					NONEXISTANT_LOCALE, NONEXISTANT_LOCALE, valid_chars.c_str(), observ, icon);
	else
		i = new CStringInput((char *)name, value, size,
				     NONEXISTANT_LOCALE, NONEXISTANT_LOCALE, valid_chars.c_str(), observ, icon);
	i->exec(NULL, "");
	delete i;
	if (!luaAction.empty()) {
		lua_pushglobaltable(L);
		lua_getfield(L, -1, luaAction.c_str());
		lua_remove(L, -2);
		lua_pushstring(L, luaId.c_str());
		lua_pushstring(L, value->c_str());
		int status = lua_pcall(L, 2 /* two arg */, 1 /* one result */, 0);
		if (status) {
			bool isString = lua_isstring(L,-1);
			const char *null = "NULL";
			fprintf(stderr, "[CLuaMenuStringinput::%s:%d] error in script: %s\n", __func__, __LINE__, isString ? lua_tostring(L, -1):null);
			DisplayErrorMessage(isString ? lua_tostring(L, -1):null, "Lua Script Error:");
		}
		if (lua_isnumber(L, -1))
			res = (int) lua_tonumber(L, -1);
		lua_pop(L, 2);
	}
	return res;
}

CLuaMenuKeyboardinput::CLuaMenuKeyboardinput(lua_State *_L, std::string _luaAction, std::string _luaId, const char *_name, std::string *_value, int _size, CChangeObserver *_observ, const char *_icon, std::string _help, std::string _help2) : CLuaMenuForwarder(_L, _luaAction, _luaId)
{
	Init(_name, _value, _size, _observ, _icon, _help, _help2);
}

void CLuaMenuKeyboardinput::Init(const char *_name, std::string *_value, int _size, CChangeObserver *_observ, const char *_icon, std::string _help, std::string _help2)
{
	name   = _name;
	value  = _value;
	size   = _size;
	icon   = _icon;
	observ = _observ;
	help   = _help;
	help2  = _help2;
}

int CLuaMenuKeyboardinput::exec(CMenuTarget* /*parent*/, const std::string & /*actionKey*/)
{
	int res = menu_return::RETURN_REPAINT;
	CKeyboardInput *i;
	i = new CKeyboardInput((char *)name, value, size, observ, icon, help, help2);
	i->exec(NULL, "");
	delete i;
	if (!luaAction.empty()) {
		lua_pushglobaltable(L);
		lua_getfield(L, -1, luaAction.c_str());
		lua_remove(L, -2);
		lua_pushstring(L, luaId.c_str());
		lua_pushstring(L, value->c_str());
		int status = lua_pcall(L, 2 /* two arg */, 1 /* one result */, 0);
		if (status) {
			bool isString = lua_isstring(L,-1);
			const char *null = "NULL";
			fprintf(stderr, "[CLuaMenuKeyboardinput::%s:%d] error in script: %s\n", __func__, __LINE__, isString ? lua_tostring(L, -1):null);
			DisplayErrorMessage(isString ? lua_tostring(L, -1):null, "Lua Script Error:");
		}
		if (lua_isnumber(L, -1))
			res = (int) lua_tonumber(L, -1);
		lua_pop(L, 2);
	}
	return res;
}

int CLuaInstMenu::MenuNew(lua_State *L)
{
	CMenuWidget *m;

	if (lua_istable(L, 1)) {
		std::string name; tableLookup(L, "name", name) || tableLookup(L, "title", name);
		std::string icon; tableLookup(L, "icon", icon);
		lua_Integer mwidth;
		if (tableLookup(L, "mwidth", mwidth))
			m = new CMenuWidget(name.c_str(), icon.c_str(), mwidth);
		else
			m = new CMenuWidget(name.c_str(), icon.c_str());
	} else
		m = new CMenuWidget();

	CLuaMenu **udata = (CLuaMenu **) lua_newuserdata(L, sizeof(CLuaMenu *));
	*udata = new CLuaMenu();
	(*udata)->m = m;
	luaL_getmetatable(L, "menu");
	lua_setmetatable(L, -2);
	return 1;
}

int CLuaInstMenu::MenuAddKey(lua_State *L)
{
	CLuaMenu *D = MenuCheck(L, 1);
	if (!D) return 0;
	lua_assert(lua_istable(L, 2));

	std::string action;				tableLookup(L, "action", action);
	std::string id;					tableLookup(L, "id", id);
	lua_Unsigned directkey = CRCInput::RC_nokey;	tableLookup(L, "directkey", directkey);
	if ((!action.empty()) && (directkey != CRCInput::RC_nokey)) {
		CLuaMenuForwarder *forwarder = new CLuaMenuForwarder(L, action, id);
		D->m->addKey(directkey, forwarder, action);
		D->targets.push_back(forwarder);
	}
	return 0;
}

int CLuaInstMenu::MenuAddItem(lua_State *L)
{
	CLuaMenu *D = MenuCheck(L, 1);
	if (!D) {
		lua_pushnil(L);
		return 1;
	}
	lua_assert(lua_istable(L, 2));

	CMenuItem *mi = NULL;
	CLuaMenuItem i;
	D->items.push_back(i);
	CLuaMenuItem *b = &D->items.back();

	tableLookup(L, "name", b->name);
	std::string type; tableLookup(L, "type", type);
	if (type == "back") {
		D->m->addItem(GenericMenuBack);
	} else if (type == "next") {
		D->m->addItem(GenericMenuNext);
	} else if (type == "cancel") {
		D->m->addItem(GenericMenuCancel);
	} else if (type == "separator") {
		D->m->addItem(GenericMenuSeparator);
	} else if ((type == "separatorline") || (type == "subhead")) {
		if (!b->name.empty()) {
			int flag = (type == "separatorline") ? CMenuSeparator::LINE : CMenuSeparator::SUB_HEAD;
			D->m->addItem(new CMenuSeparator(CMenuSeparator::STRING | flag, b->name.c_str(), NONEXISTANT_LOCALE));
		} else
			D->m->addItem(GenericMenuSeparatorLine);
	} else {
		std::string right_icon_str;	tableLookup(L, "right_icon", right_icon_str);
		std::string action;		tableLookup(L, "action", action);
		std::string value;		tableLookup(L, "value", value);
		std::string hint;		tableLookup(L, "hint", hint);
		std::string hint_icon_str;	tableLookup(L, "hint_icon", hint_icon_str);
		std::string icon_str;		tableLookup(L, "icon", icon_str);
		std::string id;			tableLookup(L, "id", id);
		std::string tmp;
		char *right_icon = NULL;
		if (!right_icon_str.empty()) {
			right_icon = strdup(right_icon_str.c_str());
			D->tofree.push_back(right_icon);
		}
		char *hint_icon = NULL;
		if (!hint_icon_str.empty()) {
			hint_icon = strdup(hint_icon_str.c_str());
			D->tofree.push_back(hint_icon);
		}
		char *icon = NULL;
		if (!icon_str.empty()) {
			icon = strdup(icon_str.c_str());
			D->tofree.push_back(icon);
		}

		lua_Unsigned directkey = CRCInput::RC_nokey;	tableLookup(L, "directkey", directkey);
		lua_Integer pulldown = false;			tableLookup(L, "pulldown", pulldown);

		bool enabled = true;
		if (!(tableLookup(L, "enabled", enabled) || tableLookup(L, "active", enabled))) {
			tmp = "true";
			if (tableLookup(L, "enabled", tmp) || tableLookup(L, "active", tmp))
				paramBoolDeprecated(L, tmp.c_str());
			enabled = (tmp == "true" || tmp == "1" || tmp == "yes");
		}

		tableLookup(L, "range", tmp);
		int range_from = 0, range_to = 99;
		sscanf(tmp.c_str(), "%d,%d", &range_from, &range_to);

		if (type == "forwarder") {
			b->str_val = value;
			CLuaMenuForwarder *forwarder = new CLuaMenuForwarder(L, action, id);
			mi = new CMenuForwarder(b->name, enabled, b->str_val, forwarder, NULL/*ActionKey*/, directkey, icon, right_icon);
			if (!hint.empty() || hint_icon)
				mi->setHint(hint_icon, hint);
			D->targets.push_back(forwarder);
		} else if (type == "chooser") {
			int options_count = 0;
			lua_pushstring(L, "options");
			lua_gettable(L, -2);
			if (lua_istable(L, -1))
				for (lua_pushnil(L); lua_next(L, -2); lua_pop(L, 2)) {
					lua_pushvalue(L, -2);
					options_count++;
				}
			lua_pop(L, 1);
			if (options_count == 0) {
				D->m->addItem(new CMenuSeparator(CMenuSeparator::STRING | CMenuSeparator::LINE, "ERROR! (options_count)", NONEXISTANT_LOCALE));
				lua_pushnil(L);
				return 1;
			}

			CMenuOptionChooser::keyval_ext *kext = (CMenuOptionChooser::keyval_ext *)calloc(options_count, sizeof(CMenuOptionChooser::keyval_ext));
			D->tofree.push_back(kext);
			lua_pushstring(L, "options");
			lua_gettable(L, -2);
			b->int_val = 0;
			if (lua_istable(L, -1)) {
				int j = 0;
				for (lua_pushnil(L); lua_next(L, -2); lua_pop(L, 2)) {
					lua_pushvalue(L, -2);
					const char *key = lua_tostring(L, -1);
					const char *val = lua_tostring(L, -2);
					kext[j].key = atoi(key);
					kext[j].value = NONEXISTANT_LOCALE;
					kext[j].valname = strdup(val?val:"ERROR");
					D->tofree.push_back((void *)kext[j].valname);
					if (!strcmp(value.c_str(), kext[j].valname))
						b->int_val = kext[j].key;
					j++;
				}
			}
			lua_pop(L, 1);
			mi = new CMenuOptionChooser(b->name.c_str(), &b->int_val, kext, options_count, enabled, D->observ, directkey, icon, pulldown);
		} else if (type == "numeric") {
			b->int_val = range_from;
			sscanf(value.c_str(), "%d", &b->int_val);
			mi = new CMenuOptionNumberChooser(b->name, &b->int_val, enabled, range_from, range_to, D->observ, 0, 0, NONEXISTANT_LOCALE, pulldown);
		} else if (type == "string") {
			b->str_val = value;
			mi = new CMenuOptionStringChooser(b->name.c_str(), &b->str_val, enabled, D->observ, directkey, icon, pulldown);
		} else if (type == "stringinput") {
			b->str_val = value;
			std::string valid_chars = "abcdefghijklmnopqrstuvwxyz0123456789!\"§$%&/()=?-.@,_: ";
			tableLookup(L, "valid_chars", valid_chars);
			lua_Integer sms = 0;	tableLookup(L, "sms", sms);
			lua_Integer size = 30;	tableLookup(L, "size", size);
			CLuaMenuStringinput *stringinput = new CLuaMenuStringinput(L, action, id, b->name.c_str(), &b->str_val, size, valid_chars, D->observ, icon, sms);
			mi = new CMenuForwarder(b->name, enabled, b->str_val, stringinput, NULL/*ActionKey*/, directkey, icon, right_icon);
			D->targets.push_back(stringinput);
		} else if (type == "keyboardinput") {
			b->str_val = value;
			lua_Integer size = 0;	tableLookup(L, "size", size);
			std::string help = "";	tableLookup(L, "help", help);
			std::string help2 = "";	tableLookup(L, "help2", help2);
			CLuaMenuKeyboardinput *keyboardinput = new CLuaMenuKeyboardinput(L, action, id, b->name.c_str(), &b->str_val, size, D->observ, icon, help, help2);
			mi = new CMenuForwarder(b->name, enabled, b->str_val, keyboardinput, NULL/*ActionKey*/, directkey, icon, right_icon);
			D->targets.push_back(keyboardinput);
		} else if (type == "filebrowser") {
			b->str_val = value;
			lua_Integer dirMode = 0; tableLookup(L, "dir_mode", dirMode);
			CLuaMenuFilebrowser *filebrowser = new CLuaMenuFilebrowser(L, action, id, &b->str_val, dirMode);
			lua_pushstring(L, "filter");
			lua_gettable(L, -2);
			if (lua_istable(L, -1))
				for (lua_pushnil(L); lua_next(L, -2); lua_pop(L, 2)) {
					lua_pushvalue(L, -2);
					const char *val = lua_tostring(L, -2);
					filebrowser->addFilter(val);
				}
			lua_pop(L, 1);

			mi = new CMenuForwarder(b->name, enabled, b->str_val, filebrowser, NULL/*ActionKey*/, directkey, icon, right_icon);
			D->targets.push_back(filebrowser);
		}
		if (mi) {
			mi->setLua(L, action, id);
			if (!hint.empty() || hint_icon)
				mi->setHint(hint_icon, hint);
			D->m->addItem(mi);
		}
	}

	if (mi) {
		lua_Integer id = D->itemmap.size() + 1;
		D->itemmap.insert(itemmap_pair_t(id, mi));
		lua_pushinteger(L, id);
	} else
		lua_pushnil(L);

	return 1;
}

int CLuaInstMenu::MenuExec(lua_State *L)
{
	CLuaMenu *D = MenuCheck(L, 1);
	if (!D) return 0;
	D->m->exec(NULL, "");
	D->m->hide();
	return 0;
}

int CLuaInstMenu::MenuHide(lua_State *L)
{
	CLuaMenu *D = MenuCheck(L, 1);
	if (!D) return 0;
	D->m->hide();
	return 0;
}

int CLuaInstMenu::MenuSetActive(lua_State *L)
{
	lua_assert(lua_istable(L, 2));
	CLuaMenu *D = MenuCheck(L, 1);
	if (!D) return 0;

	lua_Integer id;	tableLookup(L, "item", id);
	bool activ;	tableLookup(L, "activ", activ);

	CMenuItem* item = NULL;
	for (itemmap_iterator_t it = D->itemmap.begin(); it != D->itemmap.end(); ++it) {
		if (it->first == id) {
			item = it->second;
			break;
		}
	}
	if (item)
		item->setActive(activ);
	return 0;
}

int CLuaInstMenu::MenuSetName(lua_State *L)
{
	lua_assert(lua_istable(L, 2));
	CLuaMenu *D = MenuCheck(L, 1);
	if (!D) return 0;

	lua_Integer id;		tableLookup(L, "item", id);
	std::string name;	tableLookup(L, "name", name);

	CMenuItem* item = NULL;
	for (itemmap_iterator_t it = D->itemmap.begin(); it != D->itemmap.end(); ++it) {
		if (it->first == id) {
			item = it->second;
			break;
		}
	}
	if (item)
		item->setName(name);
	return 0;
}

int CLuaInstMenu::MenuDelete(lua_State *L)
{
	CLuaMenu *D = MenuCheck(L, 1);
	if (!D) return 0;

	while (!D->targets.empty()) {
		delete D->targets.back();
		D->targets.pop_back();
	}
	while (!D->tofree.empty()) {
		free(D->tofree.back());
		D->tofree.pop_back();
	}

	delete D;
	return 0;
}
