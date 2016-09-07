/*
 * lua stringinput
 *
 * (C) 2016 Sven Hoefer (svenhoefer)
 * (C) 2016 M. Liebmann (micha-bbg)
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
#include <gui/widget/stringinput.h>
#include <neutrino.h>

#include "luainstance.h"
#include "lua_stringinput.h"

CLuaInstStringInput* CLuaInstStringInput::getInstance()
{
	static CLuaInstStringInput* LuaInstStringInput = NULL;

	if (!LuaInstStringInput)
		LuaInstStringInput = new CLuaInstStringInput();
	return LuaInstStringInput;
}

void CLuaInstStringInput::StringInputRegister(lua_State *L)
{
	luaL_Reg meth[] = {
		{ "exec", CLuaInstStringInput::StringInputExec },
		{ NULL, NULL }
	};

	luaL_newmetatable(L, "stringinput");
	luaL_setfuncs(L, meth, 0);
	lua_pushvalue(L, -1);
	lua_setfield(L, -1, "__index");
	lua_setglobal(L, "stringinput");
}

/*
	local return_value = stringinput.exec{
		caption="Title",
		value="value",
		icon="settings",
		valid_chars="0123456789",
		size=4
	}
*/
int CLuaInstStringInput::StringInputExec(lua_State *L)
{
	lua_assert(lua_istable(L,1));

	std::string name;
	tableLookup(L, "name", name) || tableLookup(L, "title", name) || tableLookup(L, "caption", name);

	std::string value;
	tableLookup(L, "value", value);

	lua_Integer size = 30;
	tableLookup(L, "size", size);

	// TODO: Locales?

	std::string valid_chars = "abcdefghijklmnopqrstuvwxyz0123456789!\"§$%&/()=?-.@,_: ";
	tableLookup(L, "valid_chars", valid_chars);

	// TODO: CChangeObserver?

	std::string icon = std::string(NEUTRINO_ICON_INFO);
	tableLookup(L, "icon", icon);

	lua_Integer sms = 0;
	tableLookup(L, "sms", sms);

	lua_Integer pin = 0;
	tableLookup(L, "pin", pin);

	if (sms && pin)
		dprintf(DEBUG_NORMAL, "[CLuaInstance][%s - %d]: 'sms' AND 'pin' is defined! 'pin' will be prefered.\n", __func__, __LINE__);

	CStringInput *i;
	if (pin)
		i = new CPINInput(name, &value, size,
				     NONEXISTANT_LOCALE, NONEXISTANT_LOCALE, valid_chars.c_str(), NULL);
	else if (sms)
		i = new CStringInputSMS(name, &value, size,
				     NONEXISTANT_LOCALE, NONEXISTANT_LOCALE, valid_chars.c_str(), NULL, icon.c_str());
	else
		i = new CStringInput(name, &value, size,
				     NONEXISTANT_LOCALE, NONEXISTANT_LOCALE, valid_chars.c_str(), NULL, icon.c_str());
	i->exec(NULL, "");
	delete i;

	lua_pushstring(L, value.c_str());

	return 1;
}
