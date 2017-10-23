/*
 * simple lua curl functions
 *
 * (C) 2015 M. Liebmann (micha-bbg)
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

#ifndef _LUACURL_H
#define _LUACURL_H

class CLuaCurl
{
	public:
		CLuaCurl() {};
		~CLuaCurl() {};
};

struct progressData {
	CURL *curl;
	curl_off_t last_dlnow;
};

class CLuaInstCurl
{
	public:
		enum {
			LUA_CURL_OK              = 0,
			LUA_CURL_ERR_HANDLE      = 1,
			LUA_CURL_ERR_NO_URL      = 2,
			LUA_CURL_ERR_CREATE_FILE = 3,
			LUA_CURL_ERR_CURL        = 4
		};

		CLuaInstCurl() {};
		~CLuaInstCurl() {};
		static CLuaInstCurl* getInstance();
		static void LuaCurlRegister(lua_State *L);
		static std::string CurlUriInternal(std::string data, bool decode);

	private:
		static CLuaCurl *CurlCheckData(lua_State *L, int n);
		static int CurlNew(lua_State *L);
		static size_t CurlWriteToString(void *ptr, size_t size, size_t nmemb, void *data);
#if LIBCURL_VERSION_NUM < 0x072000
		static int CurlProgressFunc_old(void *p, double dltotal, double dlnow, double ultotal, double ulnow);
#endif
		static int CurlProgressFunc(void *p, curl_off_t dltotal, curl_off_t dlnow, curl_off_t ultotal, curl_off_t ulnow);
		static int CurlDownload(lua_State *L);
		static int CurlEncodeUri(lua_State *L);
		static int CurlDecodeUri(lua_State *L);
		static int CurlSetUriData(lua_State *L);
		static int CurlDelete(lua_State *L);
};

#endif //_LUACURL_H
