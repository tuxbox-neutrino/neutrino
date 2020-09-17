/*
 * lua instance helper functions
 *
 * (C) 2013 Stefan Seyfried (seife)
 * (C) 2014-2016 M. Liebmann (micha-bbg)
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

#ifndef _LUAINSTANCEHELPERS_H
#define _LUAINSTANCEHELPERS_H

#include <map>

#if LUA_COMPAT_5_2

#include <stdint.h>
#include <math.h>

typedef uint32_t lua_Unsigned;
int lua_absindex (lua_State *L, int i);
void lua_rawgetp (lua_State *L, int i, const void *p);
void lua_rawsetp (lua_State *L, int i, const void *p);
void lua_pushunsigned (lua_State *L, lua_Unsigned n);
lua_Unsigned luaL_checkunsigned (lua_State *L, int i);
#define lua_pushglobaltable(L) \
	lua_pushvalue(L, LUA_GLOBALSINDEX)

#define LUA_SUPUNSIGNED \
	((lua_Number)(~(lua_Unsigned)0) + 1)

#define lua_number2unsigned(i,n) \
	((i) = (lua_Unsigned)(n))

#define lua_unsigned2number(u) \
	(((u) <= (lua_Unsigned)INT_MAX) ? (lua_Number)(int)(u) : (lua_Number)(u))

#endif

//#define LUA_DEBUG printf
#define LUA_DEBUG(...)

#define LUA_CLASSNAME       "neutrino"
#define LUA_VIDEO_CLASSNAME "video"
#define LUA_MISC_CLASSNAME  "misc"
#define LUA_CURL_CLASSNAME  "curl"
#define LUA_HEADER_CLASSNAME  "header"
#define LUA_FILEHELPER_CLASSNAME  "filehelpers"

#define LUA_WIKI "https://wiki.tuxbox-neutrino.org/wiki"
//#define LUA_WIKI "https://wiki.slknet.de/wiki"

/* the magic color that tells us we are using one of the palette colors */
#define MAGIC_COLOR 0x42424200
#define MAGIC_MASK  0xFFFFFF00

#define lua_boxpointer(L, u) \
	(*(void **)(lua_newuserdata(L, sizeof(void *))) = (u))

#define lua_unboxpointer(L, i) \
	(*(void **)(lua_touserdata(L, i)))

class Font;

typedef std::pair<lua_Integer, Font*> fontmap_pair_t;
typedef std::map<lua_Integer, Font*> fontmap_t;
typedef fontmap_t::iterator fontmap_iterator_t;

typedef std::pair<lua_Integer, fb_pixel_t*> screenmap_pair_t;
typedef std::map<lua_Integer, fb_pixel_t*> screenmap_t;
typedef screenmap_t::iterator screenmap_iterator_t;

struct table_key {
	const char *name;
	lua_Integer code;
};

struct table_key_u {
	const char *name;
	lua_Unsigned code;
};

struct lua_envexport {
	const char *name;
	table_key *t;
};

struct lua_envexport_u {
	const char *name;
	table_key_u *t;
};

/* this is stored as userdata in the lua_State */
struct CLuaData
{
	CFBWindow *fbwin;
	CRCInput *rcinput;
	fontmap_t fontmap;
	screenmap_t screenmap;
};

bool _luaL_checkbool(lua_State *L, int numArg);

void paramBoolDeprecated(lua_State *L, const char* val);
void paramDeprecated(lua_State *L, const char* oldParam, const char* newParam);
void functionDeprecated(lua_State *L, const char* oldFunc, const char* newFunc);
void obsoleteHideParameter(lua_State *L);
lua_Unsigned checkMagicMask(lua_Unsigned col);

bool tableLookup(lua_State*, const char*, std::string&);
bool tableLookup(lua_State*, const char*, lua_Integer&);
bool tableLookup(lua_State*, const char*, lua_Unsigned&);
bool tableLookup(lua_State*, const char*, void**);
bool tableLookup(lua_State*, const char*, bool &value);

#endif // _LUAINSTANCEHELPERS_H
