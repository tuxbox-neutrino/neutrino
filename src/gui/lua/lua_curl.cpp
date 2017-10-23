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

#include <config.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <unistd.h>
#include <math.h>

#include <global.h>
#include <system/debug.h>
#include <system/helpers.h>
#include <neutrino.h>

#include <curl/curl.h>
#include <curl/easy.h>

#include "luainstance.h"
#include "lua_curl.h"

CLuaInstCurl* CLuaInstCurl::getInstance()
{
	static CLuaInstCurl* LuaInstCurl = NULL;

	if (!LuaInstCurl)
		LuaInstCurl = new CLuaInstCurl();
	return LuaInstCurl;
}

CLuaCurl *CLuaInstCurl::CurlCheckData(lua_State *L, int n)
{
	return *(CLuaCurl **) luaL_checkudata(L, n, LUA_CURL_CLASSNAME);
}

void CLuaInstCurl::LuaCurlRegister(lua_State *L)
{
	luaL_Reg meth[] = {
		{ "new",      CLuaInstCurl::CurlNew },
		{ "download", CLuaInstCurl::CurlDownload },
		{ "__gc",     CLuaInstCurl::CurlDelete },
		{ NULL, NULL }
	};

	luaL_newmetatable(L, LUA_CURL_CLASSNAME);
	luaL_setfuncs(L, meth, 0);
	lua_pushvalue(L, -1);
	lua_setfield(L, -1, "__index");
	lua_setglobal(L, LUA_CURL_CLASSNAME);
}

int CLuaInstCurl::CurlNew(lua_State *L)
{
	CLuaCurl **udata = (CLuaCurl **) lua_newuserdata(L, sizeof(CLuaCurl *));
	*udata = new CLuaCurl();
	luaL_getmetatable(L, LUA_CURL_CLASSNAME);
	lua_setmetatable(L, -2);
	return 1;
}

size_t CLuaInstCurl::CurlWriteToString(void *ptr, size_t size, size_t nmemb, void *data)
{
	if (size * nmemb > 0) {
		std::string* pStr = (std::string*) data;
		pStr->append((char*) ptr, nmemb);
	}
	return size*nmemb;
}

int CLuaInstCurl::CurlProgressFunc(void *p, curl_off_t dltotal, curl_off_t dlnow, curl_off_t /*ultotal*/, curl_off_t /*ulnow*/)
{
	if (dltotal == 0)
		return 0;

	struct progressData *_pgd = static_cast<struct progressData*>(p);
	if (_pgd->last_dlnow == dlnow)
		return 0;
	_pgd->last_dlnow = dlnow;

	double dlSpeed;
	long responseCode = 0;
	curl_easy_getinfo(_pgd->curl, CURLINFO_SPEED_DOWNLOAD, &dlSpeed);
	curl_easy_getinfo(_pgd->curl, CURLINFO_RESPONSE_CODE, &responseCode);

	uint32_t MUL = 0x7FFF;
	uint32_t dlFragment = (uint32_t)((dlnow * MUL) / dltotal);
	if (responseCode != 200) {
		dlFragment = 0;
		dlSpeed    = 0;
	}

	int showDots = 50;
	int dots = (dlFragment * showDots) / MUL;
	printf(" %d%% [", (dlFragment * 100) / MUL);
	int i = 0;
	for (; i < dots-1; i++)
		printf("=");
	if (i < showDots) {
		printf(">");
		i++;
	}
	for (; i < showDots; i++)
		printf(" ");
	printf("] speed: %.03f KB/sec     \r", dlSpeed/1024); fflush(stdout);
	return 0;
}

#if LIBCURL_VERSION_NUM < 0x072000
int CLuaInstCurl::CurlProgressFunc_old(void *p, double dltotal, double dlnow, double ultotal, double ulnow)
{
	return CurlProgressFunc(p, (curl_off_t)dltotal, (curl_off_t)dlnow, (curl_off_t)ultotal, (curl_off_t)ulnow);
}
#endif

int CLuaInstCurl::CurlDownload(lua_State *L)
{
/*
	parameter	typ		default
	----------------------------------------
	url		string		required
	o, outputfile	string		when empty then save to string
					as secund return value
	A, userAgent	string		empty
	P, postfields	string		empty
	v, verbose	bool		false
	s, silent	bool		false
	h, header	bool		false
	connectTimeout	number		20
	ipv4		bool		false
	ipv6		bool		false
	useProxy	bool		true (default)
	followRedir	bool		true
	maxRedirs	number		20
*/

/*
Example:
	-- simplest program call:
	-- ----------------------
	local curl = curl.new()
	local ret, data = curl:download{url="http://example.com", o="/tmp/test.txt"}
	if ret ~= CURL.OK then
		print("Error: " .. data)
	end

	-- or --

	local curl = curl.new()
	local ret, data = curl:download{url="http://example.com"}
	if ret == CURL.OK then
		-- downloaded data
		print(data)
		..
	else
		print("Error: " .. data)
	end
*/

#define CURL_MSG_ERROR "[curl:download \33[1;31mERROR!\33[0m]"

	lua_assert(lua_istable(L,1));
	CLuaCurl *D = CurlCheckData(L, 1);
	if (!D) return 0;

	char errMsg[1024]={0};
	CURL *curl_handle = curl_easy_init();
	if (!curl_handle) {
		memset(errMsg, '\0', sizeof(errMsg));
		snprintf(errMsg, sizeof(errMsg)-1, "error creating cUrl handle.");
		printf("%s %s\n", CURL_MSG_ERROR, errMsg);
		lua_pushinteger(L, LUA_CURL_ERR_HANDLE);
		lua_pushstring(L, errMsg);
		return 2;
	}

	std::string url = "";
	tableLookup(L, "url", url);
	if (url.empty()) {
		curl_easy_cleanup(curl_handle);
		memset(errMsg, '\0', sizeof(errMsg));
		snprintf(errMsg, sizeof(errMsg)-1, "no url given.");
		printf("%s %s\n", CURL_MSG_ERROR, errMsg);
		lua_pushinteger(L, LUA_CURL_ERR_NO_URL);
		lua_pushstring(L, errMsg);
		return 2;
	}

	std::string outputfile = "";
	bool toFile = tableLookup(L, "o", outputfile) || tableLookup(L, "outputfile", outputfile);
	std::string retString = "";
	FILE *fp = NULL;
	if (toFile) {
		fp = fopen(outputfile.c_str(), "wb");
		if (fp == NULL) {
			memset(errMsg, '\0', sizeof(errMsg));
			snprintf(errMsg, sizeof(errMsg)-1, "Can't create %s", outputfile.c_str());
			printf("%s %s\n", CURL_MSG_ERROR, errMsg);
			curl_easy_cleanup(curl_handle);
			lua_pushinteger(L, LUA_CURL_ERR_CREATE_FILE);
			lua_pushstring(L, errMsg);
			return 2;
		}
	}

	std::string userAgent = "";
	tableLookup(L, "A", userAgent) || tableLookup(L, "userAgent", userAgent);

	std::string postfields = "";//specify data to POST to server
	tableLookup(L, "P", postfields) || tableLookup(L, "postfields", postfields);

	bool verbose = false;
	tableLookup(L, "v", verbose) || tableLookup(L, "verbose", verbose);

	bool silent = false;
	if (!verbose)
		tableLookup(L, "s", silent) || tableLookup(L, "silent", silent);

	bool pass_header = false;//pass headers to the data stream
	tableLookup(L, "header", pass_header);

	lua_Integer connectTimeout = 20;
	tableLookup(L, "connectTimeout", connectTimeout);

	bool ipv4 = false;
	tableLookup(L, "ipv4", ipv4);
	bool ipv6 = false;
	tableLookup(L, "ipv6", ipv6);

	bool useProxy = true;
	tableLookup(L, "useProxy", useProxy);

	bool followRedir = true;
	tableLookup(L, "followRedir", followRedir);

	lua_Integer maxRedirs = 20;
	tableLookup(L, "maxRedirs", maxRedirs);

	curl_easy_setopt(curl_handle, CURLOPT_URL, url.c_str());
	if (toFile) {
		curl_easy_setopt(curl_handle, CURLOPT_WRITEFUNCTION, NULL);
		curl_easy_setopt(curl_handle, CURLOPT_WRITEDATA, fp);
	}
	else {
		curl_easy_setopt(curl_handle, CURLOPT_WRITEFUNCTION, &CLuaInstCurl::CurlWriteToString);
		curl_easy_setopt(curl_handle, CURLOPT_WRITEDATA, (void*)&retString);
	}
	curl_easy_setopt(curl_handle, CURLOPT_FAILONERROR, 1L);
	curl_easy_setopt(curl_handle, CURLOPT_TIMEOUT, (long)(4*connectTimeout));
	curl_easy_setopt(curl_handle, CURLOPT_CONNECTTIMEOUT, (long)connectTimeout);
	curl_easy_setopt(curl_handle, CURLOPT_NOSIGNAL, 1L);
	curl_easy_setopt(curl_handle, CURLOPT_SSL_VERIFYPEER, 0L);
	/* enable all supported built-in compressions */
	curl_easy_setopt(curl_handle, CURLOPT_ACCEPT_ENCODING, "");
	curl_easy_setopt(curl_handle, CURLOPT_COOKIE, "");
	if (!userAgent.empty())
		curl_easy_setopt(curl_handle, CURLOPT_USERAGENT, userAgent.c_str());

	if (!postfields.empty()) {
		curl_easy_setopt(curl_handle, CURLOPT_POSTFIELDSIZE, static_cast<long>(postfields.length()));
		curl_easy_setopt(curl_handle, CURLOPT_POSTFIELDS, postfields.c_str());
	}

	if (ipv4 && ipv6)
		curl_easy_setopt(curl_handle, CURLOPT_IPRESOLVE, CURL_IPRESOLVE_WHATEVER);
	else if (ipv4)
		curl_easy_setopt(curl_handle, CURLOPT_IPRESOLVE, CURL_IPRESOLVE_V4);
	else if (ipv6)
		curl_easy_setopt(curl_handle, CURLOPT_IPRESOLVE, CURL_IPRESOLVE_V6);

	if (!g_settings.softupdate_proxyserver.empty() && useProxy) {
		curl_easy_setopt(curl_handle, CURLOPT_PROXY, g_settings.softupdate_proxyserver.c_str());
		if (!g_settings.softupdate_proxyusername.empty()) {
			std::string tmp = g_settings.softupdate_proxyusername + ":" + g_settings.softupdate_proxypassword;
			curl_easy_setopt(curl_handle, CURLOPT_PROXYUSERPWD, tmp.c_str());
		}
	}

	curl_easy_setopt(curl_handle, CURLOPT_FOLLOWLOCATION, (followRedir)?1L:0L);
	curl_easy_setopt(curl_handle, CURLOPT_MAXREDIRS, (long)maxRedirs);
	curl_easy_setopt(curl_handle, CURLOPT_NOPROGRESS, (silent)?1L:0L);
	curl_easy_setopt(curl_handle, CURLOPT_VERBOSE, (verbose)?1L:0L);
	curl_easy_setopt(curl_handle, CURLOPT_HEADER, (pass_header)?1L:0L);

	progressData pgd;

	if (!silent) {
		pgd.curl = curl_handle;
		pgd.last_dlnow = -1;
#if LIBCURL_VERSION_NUM >= 0x072000
		curl_easy_setopt(curl_handle, CURLOPT_XFERINFOFUNCTION, CLuaInstCurl::CurlProgressFunc);
		curl_easy_setopt(curl_handle, CURLOPT_XFERINFODATA, &pgd);
#else
		curl_easy_setopt(curl_handle, CURLOPT_PROGRESSFUNCTION, CLuaInstCurl::CurlProgressFunc_old);
		curl_easy_setopt(curl_handle, CURLOPT_PROGRESSDATA, &pgd);
#endif
	}

	char cerror[CURL_ERROR_SIZE]={0};
	curl_easy_setopt(curl_handle, CURLOPT_ERRORBUFFER, cerror);

	printf("\n[curl:download] download %s => %s\n", url.c_str(), (toFile)?outputfile.c_str():"return string");
	if (!silent)
		printf("\e[?25l"); /* cursor off */
	printf("\n");
	CURLcode ret = curl_easy_perform(curl_handle);
	if (!silent)
		printf("\e[?25h"); /* cursor on */
	printf("\n");

	std::string msg;
	if (!silent) {
		double dsize, dtime;
		char *dredirect=NULL, *deffektive=NULL;
		curl_easy_getinfo(curl_handle, CURLINFO_SIZE_DOWNLOAD, &dsize);
		curl_easy_getinfo(curl_handle, CURLINFO_TOTAL_TIME, &dtime);
		CURLcode res1 = curl_easy_getinfo(curl_handle, CURLINFO_EFFECTIVE_URL, &deffektive);
		CURLcode res2 = curl_easy_getinfo(curl_handle, CURLINFO_REDIRECT_URL, &dredirect);

		char msg1[1024]={0};
		memset(msg1, '\0', sizeof(msg1));
		snprintf(msg1, sizeof(msg1)-1, "\n[curl:download] O.K. size: %.0f bytes, time: %.02f sec.", dsize, dtime);
		msg = msg1;
		if (toFile)
			msg += std::string("\n		     file: ") + outputfile;
		else
			msg += std::string("\n		   output: return string");
		msg += std::string("\n		      url: ") + url;
		if ((res1 == CURLE_OK) && deffektive) {
			std::string tmp1 = std::string(deffektive);
			std::string tmp2 = url;
			if (trim(tmp1, " /") != trim(tmp2, " /"))
				msg += std::string("\n	    effektive url: ") + deffektive;
		}
		if ((res2 == CURLE_OK) && dredirect)
			msg += std::string("\n	      redirect to: ") + dredirect;
	}

	curl_easy_cleanup(curl_handle);
	if (toFile)
		fclose(fp);

	if (ret != 0) {
		memset(errMsg, '\0', sizeof(errMsg));
		snprintf(errMsg, sizeof(errMsg)-1, "%s", cerror);
		printf("%s curl error: %s\n", CURL_MSG_ERROR, errMsg);
		if (toFile)
			unlink(outputfile.c_str());
		lua_pushinteger(L, LUA_CURL_ERR_CURL);
		lua_pushstring(L, errMsg);
		return 2;
	}

	if (verbose == true)
		printf("%s\n \n", msg.c_str());

	lua_pushinteger(L, LUA_CURL_OK);
	lua_pushstring(L, retString.c_str());
	return 2;
}

int CLuaInstCurl::CurlDelete(lua_State *L)
{
	CLuaCurl *D = CurlCheckData(L, 1);
	if (!D) return 0;
	delete D;
	return 0;
}
