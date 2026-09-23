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
#include <gui/widget/progresswindow.h>

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

/* What a transfer does with an answer that names another place. The download
   calls have always let curl follow on its own while there was no extra header
   to hand to whatever the answer names, and taken over by hand as soon as there
   was one, which is why this is decided again at every hop rather than once. */
enum RedirectPolicy
{
	kFollowGuarded,
	kFollowNever
};

std::string toLowerAscii(const std::string &value)
{
	std::string result;
	result.reserve(value.length());
	for (size_t i = 0; i < value.length(); ++i)
		result.push_back((char)std::tolower((unsigned char)value[i]));
	return result;
}

std::string trimAscii(const std::string &value)
{
	const size_t first = value.find_first_not_of(" \t\r\n");
	if (first == std::string::npos)
		return "";

	const size_t last = value.find_last_not_of(" \t\r\n");
	return value.substr(first, last - first + 1);
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

bool isMethodToken(const std::string &method)
{
	if (method.empty())
		return false;

	for (size_t i = 0; i < method.length(); ++i)
	{
		if (method[i] < 'A' || method[i] > 'Z')
			return false;
	}
	return true;
}

/* A header carrying a line break would put the rest of itself into the request
   as a header of its own, and the empty value is left alone because that is how
   curl is told to drop one of the headers it adds by itself. */
bool isHeaderLine(const std::string &line)
{
	if (line.find_first_of("\r\n") != std::string::npos)
		return false;

	const size_t colon = line.find(':');
	return colon != std::string::npos && colon > 0;
}

} // namespace

/* One transfer, with everything the two kinds of caller differ in. It is a type
   of its own because the redirect loop has to keep the url and the extra header
   it rewrites between hops, and an argument list that long is easier to read
   wrong than to read. */
struct CHTTPTool::Request
{
	std::string url;
	std::string extraHeader;
	std::string method;
	std::string body;
	std::string contentType;
	std::vector<std::string> headers;
	const char *target;
	std::string *text;
	RedirectPolicy redirects;
	bool sendBody;
	bool failOnError;
	bool progress;
	int globalProgressEnd;
	int connecttimeout;
	int timeout;

	Request()
		: target(NULL)
		, text(NULL)
		, redirects(kFollowGuarded)
		, sendBody(false)
		, failOnError(true)
		, progress(true)
		, globalProgressEnd(-1)
		, connecttimeout(10000)
		, timeout(1800)
	{
	}
};

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

void CHTTPTool::clearCookies()
{
	cookies.clear();
}

size_t CHTTPTool::CurlWriteToString(void *ptr, size_t size, size_t nmemb, void *data)
{
	if (size * nmemb > 0) {
		std::string* pStr = (std::string*) data;
		pStr->append((char*) ptr, nmemb);
	}
	return size*nmemb;
}

/* The session is kept here rather than in curl's own cookie engine, because
   that engine lives on the easy handle and every call below opens and closes
   one of its own, so the jar would go with it. What the engine offers besides
   is domain, path and expiry, and these answers come from the one box that was
   just asked a question. */
size_t CHTTPTool::CurlReadHeader(void *ptr, size_t size, size_t nmemb, void *data)
{
	const size_t bytes = size * nmemb;
	CHTTPTool *hTool = (CHTTPTool *) data;
	static const char name[] = "set-cookie:";
	const size_t nameLength = sizeof(name) - 1;

	if (hTool != NULL && bytes > nameLength)
	{
		const std::string line((char *) ptr, bytes);
		if (toLowerAscii(line.substr(0, nameLength)) == name)
			hTool->takeSetCookie(line.substr(nameLength));
	}

	/* Anything but the whole length tells curl the transfer went wrong. */
	return bytes;
}

void CHTTPTool::takeSetCookie(const std::string &value)
{
	const std::string pair = trimAscii(value.substr(0, value.find(';')));
	const size_t eq = pair.find('=');
	if (eq == std::string::npos || eq == 0)
		return;

	const std::string cookieName = trimAscii(pair.substr(0, eq));
	const std::string cookieValue = trimAscii(pair.substr(eq + 1));
	if (cookieName.empty())
		return;

	/* A server ends a session by handing the cookie back empty, so an empty
	   value is a removal and not something to keep sending. */
	if (cookieValue.empty())
		cookies.erase(cookieName);
	else
		cookies[cookieName] = cookieValue;
}

std::string CHTTPTool::cookieHeader() const
{
	if (cookies.empty())
		return "";

	std::string header = "Cookie: ";
	for (std::map<std::string, std::string>::const_iterator it = cookies.begin(); it != cookies.end(); ++it)
	{
		if (it != cookies.begin())
			header += "; ";
		header += it->first;
		header += "=";
		header += it->second;
	}
	return header;
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
bool CHTTPTool::perform(Request &req)
{
	const int maxRedirects = 10;
	lastHttpCode = 0;

	for (int redirectCount = 0; redirectCount <= maxRedirects; ++redirectCount)
	{
		CURL *curl;
		CURLcode res = (CURLcode) 1;
		FILE *targetFile = NULL;
		struct curl_slist *headerList = NULL;
		std::string redirectUrl;

		const bool curlFollows = (req.redirects == kFollowGuarded) && req.extraHeader.empty();
		const bool followByHand = (req.redirects == kFollowGuarded) && !req.extraHeader.empty();

		/* The answer of the hop before is not part of this one. */
		if (req.text)
			req.text->clear();

		if (req.target)
		{
#ifdef DEBUG
printf("open file %s\n", req.target);
#endif
			targetFile = fopen(req.target, "w");
			if (!targetFile)
				return false;
#ifdef DEBUG
printf("open file ok\n");
#endif
		}
#ifdef DEBUG
printf("url is %s\n", req.url.c_str());
#endif
		curl = curl_easy_init();
		if(curl)
		{
			if (req.progress)
			{
				iGlobalProgressEnd = req.globalProgressEnd;
				if(statusViewer)
				{
					iGlobalProgressBegin = statusViewer->getGlobalStatus();
				}
			}
			curl_easy_setopt(curl, CURLOPT_URL, req.url.c_str() );
			if (targetFile)
			{
				curl_easy_setopt(curl, CURLOPT_FILE, targetFile);
			}
			else
			{
				curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, &CHTTPTool::CurlWriteToString);
				curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void*)req.text);
			}
			curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, &CHTTPTool::CurlReadHeader);
			curl_easy_setopt(curl, CURLOPT_HEADERDATA, this);
			curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, curlFollows ? 1L : 0L);
			if (req.progress)
			{
#if CURL_AT_LEAST_VERSION( 7,32,0 )
				curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, show_progress);
#else
				curl_easy_setopt(curl, CURLOPT_PROGRESSFUNCTION, show_progress_old);
#endif
				curl_easy_setopt(curl, CURLOPT_PROGRESSDATA, this);
				curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0);
			}
			curl_easy_setopt(curl, CURLOPT_USERAGENT, userAgent.c_str());
			curl_easy_setopt(curl, CURLOPT_NOSIGNAL, (long)1);
			curl_easy_setopt(curl, CURLOPT_TIMEOUT, req.timeout);
			curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT_MS, req.connecttimeout);
			/* A long rather than a bool, because curl reads the value off the
			   stack as a long and this is the option that decides whether a
			   refusal keeps its body. */
			curl_easy_setopt(curl, CURLOPT_FAILONERROR, req.failOnError ? 1L : 0L);
			curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, false);
#ifdef DEBUG
			curl_easy_setopt(curl, CURLOPT_VERBOSE, 1);
#endif

			if (!req.method.empty() && req.method != "GET")
				curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, req.method.c_str());

			if (req.sendBody)
			{
				/* curl keeps the pointer instead of copying, so what it points
				   at has to outlive the transfer; the request owns it. */
				curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, (long)req.body.length());
				curl_easy_setopt(curl, CURLOPT_POSTFIELDS, req.body.c_str());
			}

			if (!req.extraHeader.empty()) {
				headerList = curl_slist_append(headerList, req.extraHeader.c_str());
			}
			if (req.sendBody && !req.contentType.empty()) {
				/* Ahead of what the caller passed, so a caller who names the
				   type itself still has the last word on it. */
				const std::string type = "Content-Type: " + req.contentType;
				headerList = curl_slist_append(headerList, type.c_str());
			}
			const std::string cookie = cookieHeader();
			if (!cookie.empty()) {
				headerList = curl_slist_append(headerList, cookie.c_str());
			}
			for (size_t i = 0; i < req.headers.size(); ++i) {
				headerList = curl_slist_append(headerList, req.headers[i].c_str());
			}
			if (headerList) {
				curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headerList);
			}

			/* Copied under the lock: the compatibility layer reaches this on
			   one of the web server's threads and the box's loop assigns to the
			   same members from a screen. */
			const std::string proxy = settingsText(g_settings.softupdate_proxyserver);
			if (!proxy.empty()) {//use proxyserver
#ifdef DEBUG
printf("use proxyserver : %s\n", proxy.c_str());
#endif
				curl_easy_setopt(curl, CURLOPT_PROXY, proxy.c_str());

				const std::string proxyuser = settingsText(g_settings.softupdate_proxyusername);
				if (!proxyuser.empty()) {//use auth
					//printf("use proxyauth\n");
					std::string tmp = proxyuser;
					tmp += ":";
					tmp += settingsText(g_settings.softupdate_proxypassword);
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
		if (targetFile)
		{
			fflush(targetFile);
			fclose(targetFile);
		}

		if (!curl)
			return false;

		if (res == CURLE_OK && followByHand && isRedirectHttpCode(lastHttpCode) && !redirectUrl.empty())
		{
			const std::string nextUrl = resolveRedirectUrl(req.url, redirectUrl);
			if (nextUrl.empty())
				return false;
			if (!sameOrigin(req.url, nextUrl))
				req.extraHeader.clear();
			req.url = nextUrl;
			continue;
		}

		return res == CURLE_OK;
	}

	return false;
}

bool CHTTPTool::downloadFile(const std::string & URL, const char * const downloadTarget, int globalProgressEnd, int connecttimeout/*=10000*/, int timeout/*=1800*/)
{
	Request req;
	req.url = URL;
	req.extraHeader = extraHeader;
	req.target = downloadTarget;
	req.globalProgressEnd = globalProgressEnd;
	req.connecttimeout = connecttimeout;
	req.timeout = timeout;

	return perform(req);
}

std::string CHTTPTool::downloadString(const std::string & URL, int globalProgressEnd, int connecttimeout/*=10000*/, int timeout/*=1800*/)
{
	std::string retString = "";

	Request req;
	req.url = URL;
	req.extraHeader = extraHeader;
	req.text = &retString;
	req.globalProgressEnd = globalProgressEnd;
	req.connecttimeout = connecttimeout;
	req.timeout = timeout;

	return perform(req) ? retString : "";
}

bool CHTTPTool::sendRequest(const std::string & method, const std::string & URL, std::string & response,
	const std::string & body/*=""*/, const std::string & contentType/*=""*/,
	const std::vector<std::string> & headers/*=empty*/,
	int connecttimeout/*=10000*/, int timeout/*=1800*/)
{
	response.clear();
	lastHttpCode = 0;

	/* A method or a header line that cannot go out as written would go out as
	   something else, and a request nobody asked for is worse than none. */
	if (!isMethodToken(method))
		return false;
	for (size_t i = 0; i < headers.size(); ++i)
	{
		if (!isHeaderLine(headers[i]))
			return false;
	}

	Request req;
	req.url = URL;
	req.extraHeader = extraHeader;
	req.method = method;
	req.body = body;
	req.contentType = contentType;
	req.headers = headers;
	req.text = &response;
	req.redirects = kFollowNever;
	req.sendBody = (method != "GET");
	/* The body of a refusal is the part that says what was wrong with the call,
	   and curl hands it over only while it is not treating the status itself as
	   the failure. The status is read back either way. */
	req.failOnError = false;
	/* Nothing here has a length worth drawing, and the window that draws it
	   divides by the total it is handed. */
	req.progress = false;
	req.connecttimeout = connecttimeout;
	req.timeout = timeout;

	return perform(req);
}
