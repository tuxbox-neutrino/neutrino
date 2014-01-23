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

#include <cstring>
#include <system/httptool.h>

#include <curl/curl.h>
#include <curl/easy.h>

#if LIBCURL_VERSION_NUM < 0x071507
#include <curl/types.h>
#endif

#include <global.h>

CHTTPTool::CHTTPTool()
{
	statusViewer = NULL;
	userAgent = "neutrino/httpdownloader";
}

void CHTTPTool::setStatusViewer( CProgress_StatusViewer* statusview )
{
	statusViewer = statusview;
}


int CHTTPTool::show_progress( void *clientp, double dltotal, double dlnow, double /*ultotal*/, double /*ulnow*/ )
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
//#define DEBUG
bool CHTTPTool::downloadFile(const std::string & URL, const char * const downloadTarget, int globalProgressEnd)
{
	CURL *curl;
	CURLcode res;
	FILE *headerfile;
#ifdef DEBUG
printf("open file %s\n", downloadTarget);
#endif
	headerfile = fopen(downloadTarget, "w");
	if (!headerfile)
		return false;
#ifdef DEBUG
printf("open file ok\n");
printf("url is %s\n", URL.c_str());
#endif
	res = (CURLcode) 1;
	curl = curl_easy_init();
	if(curl)
	{
		iGlobalProgressEnd = globalProgressEnd;
		if(statusViewer)
		{
			iGlobalProgressBegin = statusViewer->getGlobalStatus();
		}
		curl_easy_setopt(curl, CURLOPT_URL, URL.c_str() );
		curl_easy_setopt(curl, CURLOPT_FILE, headerfile);
		curl_easy_setopt(curl, CURLOPT_PROGRESSFUNCTION, show_progress);
		curl_easy_setopt(curl, CURLOPT_PROGRESSDATA, this);
		curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0);
		curl_easy_setopt(curl, CURLOPT_USERAGENT, userAgent.c_str());
		curl_easy_setopt(curl, CURLOPT_NOSIGNAL, (long)1);
		curl_easy_setopt(curl, CURLOPT_TIMEOUT, 1800);
		curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 10);
#ifdef DEBUG
		curl_easy_setopt(curl, CURLOPT_VERBOSE, 1);
#endif

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
		curl_easy_cleanup(curl);
	}
#ifdef DEBUG
printf("download code %d\n", res);
#endif
	if (headerfile)
	{
		fflush(headerfile);
		fclose(headerfile);
	}

	return res==0;
}
