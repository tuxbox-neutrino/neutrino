/*
	Neutrino-GUI  -   DBoxII-Project

	License: GPL

	This program is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation; either version 2 of the License, or
	(at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program; if not, write to the Free Software
	Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA
*/

#include <config.h>
#include <cctype>
#include <cstdlib>
#include <cstring>
#include <system/httptool.h>

#include <curl/curl.h>
#include <curl/easy.h>

#if LIBCURL_VERSION_NUM < 0x071507
#include <curl/types.h>
#endif

#include <global.h>

namespace {

struct UrlOrigin
{
	std::string scheme;
	std::string host;
	int port;
	bool valid;

	UrlOrigin()
		: port(-1)
		, valid(false)
	{
	}
};

std::string toLowerAscii(const std::string &value)
{
	std::string result;
	result.reserve(value.length());
	for (size_t i = 0; i < value.length(); ++i)
		result.push_back((char)std::tolower((unsigned char)value[i]));
	return result;
}

bool hasUrlScheme(const std::string &url)
{
	const size_t pos = url.find(':');
	if (pos == std::string::npos || pos == 0)
		return false;

	for (size_t i = 0; i < pos; ++i)
	{
		const unsigned char c = (unsigned char)url[i];
		if (!std::isalpha(c) && !std::isdigit(c) && c != '+' && c != '-' && c != '.')
			return false;
	}
	return true;
}

int defaultPortForScheme(const std::string &scheme)
{
	if (scheme == "http")
		return 80;
	if (scheme == "https")
		return 443;
	return -1;
}

int parsePort(const std::string &value)
{
	if (value.empty())
		return -1;

	char *endptr = NULL;
	const long port = std::strtol(value.c_str(), &endptr, 10);
	if (endptr == value.c_str() || *endptr != '\0' || port < 1 || port > 65535)
		return -1;

	return (int)port;
}

UrlOrigin parseUrlOrigin(const std::string &url)
{
	UrlOrigin origin;
	const size_t schemeEnd = url.find("://");
	if (schemeEnd == std::string::npos || schemeEnd == 0)
		return origin;

	origin.scheme = toLowerAscii(url.substr(0, schemeEnd));
	const size_t authorityStart = schemeEnd + 3;
	const size_t authorityEnd = url.find_first_of("/?#", authorityStart);
	std::string authority = url.substr(authorityStart, authorityEnd == std::string::npos ? std::string::npos : authorityEnd - authorityStart);
	if (authority.empty())
		return origin;

	const size_t userInfoEnd = authority.rfind('@');
	if (userInfoEnd != std::string::npos)
		authority.erase(0, userInfoEnd + 1);

	if (authority.empty())
		return origin;

	std::string host;
	int port = -1;
	if (authority[0] == '[')
	{
		const size_t close = authority.find(']');
		if (close == std::string::npos)
			return origin;
		host = authority.substr(1, close - 1);
		if (close + 1 < authority.length())
		{
			if (authority[close + 1] != ':')
				return origin;
			port = parsePort(authority.substr(close + 2));
		}
	}
	else
	{
		const size_t firstColon = authority.find(':');
		const size_t lastColon = authority.rfind(':');
		if (firstColon != std::string::npos && firstColon == lastColon)
		{
			host = authority.substr(0, firstColon);
			port = parsePort(authority.substr(firstColon + 1));
		}
		else
		{
			host = authority;
		}
	}

	if (host.empty())
		return origin;

	if (port < 0)
		port = defaultPortForScheme(origin.scheme);

	origin.host = toLowerAscii(host);
	origin.port = port;
	origin.valid = true;
	return origin;
}

std::string resolveRedirectUrl(const std::string &baseUrl, const std::string &location)
{
	if (location.empty())
		return "";

	if (hasUrlScheme(location))
		return location;

	const size_t schemeEnd = baseUrl.find("://");
	if (schemeEnd == std::string::npos)
		return location;

	if (location.compare(0, 2, "//") == 0)
		return baseUrl.substr(0, schemeEnd) + ":" + location;

	const size_t authorityStart = schemeEnd + 3;
	const size_t authorityEnd = baseUrl.find_first_of("/?#", authorityStart);
	const std::string origin = baseUrl.substr(0, authorityEnd == std::string::npos ? std::string::npos : authorityEnd);
	if (location[0] == '/')
		return origin + location;

	std::string pathBase = baseUrl.substr(0, baseUrl.find_first_of("?#"));
	const size_t slash = pathBase.rfind('/');
	if (slash == std::string::npos || slash < authorityStart)
		return origin + "/" + location;

	return pathBase.substr(0, slash + 1) + location;
}

bool sameOrigin(const std::string &leftUrl, const std::string &rightUrl)
{
	const UrlOrigin left = parseUrlOrigin(leftUrl);
	const UrlOrigin right = parseUrlOrigin(rightUrl);
	return left.valid && right.valid
		&& left.scheme == right.scheme
		&& left.host == right.host
		&& left.port == right.port;
}

bool isRedirectHttpCode(long code)
{
	return code == 301 || code == 302 || code == 303 || code == 307 || code == 308;
}

} // namespace

CHTTPTool::CHTTPTool()
{
	statusViewer = NULL;
	userAgent = "neutrino/httpdownloader";
	extraHeader = "";
	lastHttpCode = 0;
	iGlobalProgressEnd = -1;
	iGlobalProgressBegin = 0;
}

void CHTTPTool::setStatusViewer( CProgressWindow* statusview )
{
	statusViewer = statusview;
}

void CHTTPTool::setExtraHeader(const std::string& header)
{
	extraHeader = header;
}

size_t CHTTPTool::CurlWriteToString(void *ptr, size_t size, size_t nmemb, void *data)
{
	if (size * nmemb > 0) {
		std::string* pStr = (std::string*) data;
		pStr->append((char*) ptr, nmemb);
	}
	return size*nmemb;
}

int CHTTPTool::show_progress(void *clientp, curl_off_t dltotal, curl_off_t dlnow, curl_off_t /*ultotal*/, curl_off_t /*ulnow*/)
{
	CHTTPTool* hTool = ((CHTTPTool*)clientp);
	if(hTool->statusViewer)
	{
		int progress = int( dlnow*100.0/dltotal);
		hTool->statusViewer->showLocalStatus(progress);
		if(hTool->iGlobalProgressEnd!=-1)
		{
			int globalProg = hTool->iGlobalProgressBegin + int((hTool->iGlobalProgressEnd-hTool->iGlobalProgressBegin) * progress/100. );
			hTool->statusViewer->showGlobalStatus(globalProg);
		}
	}
	return 0;
}
#if !CURL_AT_LEAST_VERSION( 7,32,0 )
int CHTTPTool::show_progress_old(void *clientp, double dltotal, double dlnow, double ultotal, double ulnow)
{
	return show_progress(clientp, (curl_off_t)dltotal, (curl_off_t)dlnow, (curl_off_t)ultotal , (curl_off_t)ulnow);
}
#endif

//#define DEBUG
bool CHTTPTool::downloadFile(const std::string & URL, const char * const downloadTarget, int globalProgressEnd, int connecttimeout/*=10000*/, int timeout/*=1800*/)
{
	const int maxRedirects = 10;
	std::string currentUrl = URL;
	std::string currentExtraHeader = extraHeader;
	lastHttpCode = 0;

	for (int redirectCount = 0; redirectCount <= maxRedirects; ++redirectCount)
	{
		CURL *curl;
		CURLcode res = (CURLcode) 1;
		FILE *headerfile;
		struct curl_slist *headerList = NULL;
		std::string redirectUrl;
#ifdef DEBUG
printf("open file %s\n", downloadTarget);
#endif
		headerfile = fopen(downloadTarget, "w");
		if (!headerfile)
			return false;
#ifdef DEBUG
printf("open file ok\n");
printf("url is %s\n", currentUrl.c_str());
#endif
		curl = curl_easy_init();
		if(curl)
		{
			iGlobalProgressEnd = globalProgressEnd;
			if(statusViewer)
			{
				iGlobalProgressBegin = statusViewer->getGlobalStatus();
			}
			curl_easy_setopt(curl, CURLOPT_URL, currentUrl.c_str() );
			curl_easy_setopt(curl, CURLOPT_FILE, headerfile);
			curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, currentExtraHeader.empty() ? 1L : 0L);
#if CURL_AT_LEAST_VERSION( 7,32,0 )
			curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, show_progress);
#else
			curl_easy_setopt(curl, CURLOPT_PROGRESSFUNCTION, show_progress_old);
#endif
			curl_easy_setopt(curl, CURLOPT_PROGRESSDATA, this);
			curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0);
			curl_easy_setopt(curl, CURLOPT_USERAGENT, userAgent.c_str());
			curl_easy_setopt(curl, CURLOPT_NOSIGNAL, (long)1);
			curl_easy_setopt(curl, CURLOPT_TIMEOUT, timeout);
			curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT_MS, connecttimeout);
			curl_easy_setopt(curl, CURLOPT_FAILONERROR, true);
			curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, false);
#ifdef DEBUG
			curl_easy_setopt(curl, CURLOPT_VERBOSE, 1);
#endif

			if (!currentExtraHeader.empty()) {
				headerList = curl_slist_append(headerList, currentExtraHeader.c_str());
				curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headerList);
			}

			if (!g_settings.softupdate_proxyserver.empty()) {//use proxyserver
#ifdef DEBUG
printf("use proxyserver : %s\n", g_settings.softupdate_proxyserver.c_str());
#endif
				curl_easy_setopt(curl, CURLOPT_PROXY, g_settings.softupdate_proxyserver.c_str());

				if (!g_settings.softupdate_proxyusername.empty()) {//use auth
					//printf("use proxyauth\n");
					std::string tmp = g_settings.softupdate_proxyusername;
					tmp += ":";
					tmp += g_settings.softupdate_proxypassword;
					curl_easy_setopt(curl, CURLOPT_PROXYUSERPWD, tmp.c_str());
				}
			}
#ifdef DEBUG
printf("going to download\n");
#endif
			res = curl_easy_perform(curl);
			curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &lastHttpCode);
			char *curlRedirectUrl = NULL;
			curl_easy_getinfo(curl, CURLINFO_REDIRECT_URL, &curlRedirectUrl);
			if (curlRedirectUrl && *curlRedirectUrl)
				redirectUrl = curlRedirectUrl;
			curl_easy_cleanup(curl);
			if (headerList)
				curl_slist_free_all(headerList);
		}
#ifdef DEBUG
printf("download code %d\n", res);
#endif
		if (headerfile)
		{
			fflush(headerfile);
			fclose(headerfile);
		}

		if (!curl)
			return false;

		if (res == CURLE_OK && !currentExtraHeader.empty() && isRedirectHttpCode(lastHttpCode) && !redirectUrl.empty())
		{
			const std::string nextUrl = resolveRedirectUrl(currentUrl, redirectUrl);
			if (nextUrl.empty())
				return false;
			if (!sameOrigin(currentUrl, nextUrl))
				currentExtraHeader.clear();
			currentUrl = nextUrl;
			continue;
		}

		return res == CURLE_OK;
	}

	return false;
}

std::string CHTTPTool::downloadString(const std::string & URL, int globalProgressEnd, int connecttimeout/*=10000*/, int timeout/*=1800*/)
{
	const int maxRedirects = 10;
	std::string currentUrl = URL;
	std::string currentExtraHeader = extraHeader;
	lastHttpCode = 0;

	for (int redirectCount = 0; redirectCount <= maxRedirects; ++redirectCount)
	{
		CURL *curl;
		CURLcode res = (CURLcode) 1;
		std::string retString = "";
		struct curl_slist *headerList = NULL;
		std::string redirectUrl;
#ifdef DEBUG
printf("url is %s\n", currentUrl.c_str());
#endif
		curl = curl_easy_init();
		if(curl)
		{
			iGlobalProgressEnd = globalProgressEnd;
			if(statusViewer)
			{
				iGlobalProgressBegin = statusViewer->getGlobalStatus();
			}
			curl_easy_setopt(curl, CURLOPT_URL, currentUrl.c_str() );
			curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, &CHTTPTool::CurlWriteToString);
			curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void*)&retString);
			curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, currentExtraHeader.empty() ? 1L : 0L);
#if CURL_AT_LEAST_VERSION( 7,32,0 )
			curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, show_progress);
#else
			curl_easy_setopt(curl, CURLOPT_PROGRESSFUNCTION, show_progress_old);
#endif
			curl_easy_setopt(curl, CURLOPT_PROGRESSDATA, this);
			curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0);
			curl_easy_setopt(curl, CURLOPT_USERAGENT, userAgent.c_str());
			curl_easy_setopt(curl, CURLOPT_NOSIGNAL, (long)1);
			curl_easy_setopt(curl, CURLOPT_TIMEOUT, timeout);
			curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT_MS, connecttimeout);
			curl_easy_setopt(curl, CURLOPT_FAILONERROR, true);
			curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, false);
#ifdef DEBUG
			curl_easy_setopt(curl, CURLOPT_VERBOSE, 1);
#endif

			if (!currentExtraHeader.empty()) {
				headerList = curl_slist_append(headerList, currentExtraHeader.c_str());
				curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headerList);
			}

			if (!g_settings.softupdate_proxyserver.empty()) {//use proxyserver
#ifdef DEBUG
printf("use proxyserver : %s\n", g_settings.softupdate_proxyserver.c_str());
#endif
				curl_easy_setopt(curl, CURLOPT_PROXY, g_settings.softupdate_proxyserver.c_str());

				if (!g_settings.softupdate_proxyusername.empty()) {//use auth
					//printf("use proxyauth\n");
					std::string tmp = g_settings.softupdate_proxyusername;
					tmp += ":";
					tmp += g_settings.softupdate_proxypassword;
					curl_easy_setopt(curl, CURLOPT_PROXYUSERPWD, tmp.c_str());
				}
			}
#ifdef DEBUG
printf("going to download\n");
#endif
			res = curl_easy_perform(curl);
			curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &lastHttpCode);
			char *curlRedirectUrl = NULL;
			curl_easy_getinfo(curl, CURLINFO_REDIRECT_URL, &curlRedirectUrl);
			if (curlRedirectUrl && *curlRedirectUrl)
				redirectUrl = curlRedirectUrl;
			curl_easy_cleanup(curl);
			if (headerList)
				curl_slist_free_all(headerList);
		}
#ifdef DEBUG
printf("download code %d\n", res);
#endif
		if (!curl)
			return "";

		if (res == CURLE_OK && !currentExtraHeader.empty() && isRedirectHttpCode(lastHttpCode) && !redirectUrl.empty())
		{
			const std::string nextUrl = resolveRedirectUrl(currentUrl, redirectUrl);
			if (nextUrl.empty())
				return "";
			if (!sameOrigin(currentUrl, nextUrl))
				currentExtraHeader.clear();
			currentUrl = nextUrl;
			continue;
		}

		return (res==CURLE_OK) ? retString : "";
	}

	return "";
}
