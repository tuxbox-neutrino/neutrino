/*
	Neutrino-GUI  -   DBoxII-Project

	Copyright (C) 2001 Steffen Hehn 'McClean'
	Homepage: http://dbox.cyberphoria.org/

	Kommentar:

	Diese GUI wurde von Grund auf neu programmiert und sollte nun vom
	Aufbau und auch den Ausbaumoeglichkeiten gut aussehen. Neutrino basiert
	auf der Client-Server Idee, diese GUI ist also von der direkten DBox-
	Steuerung getrennt. Diese wird dann von Daemons uebernommen.


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
	Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/


#ifndef __httptool__
#define __httptool__

#include <curl/curl.h>

#include <map>
#include <string>
#include <vector>

/* Named rather than included: the window is only pointed at from here, and its
   header carries the whole screen behind it into everything that wants an HTTP
   client. */
class CProgressWindow;

class CHTTPTool
{
	private:
		/* Defined in the implementation: what one transfer needs is an
		   argument list long enough that the two callers would otherwise
		   have to repeat it, and none of it is anyone else's business. */
		struct Request;

		std::string userAgent;
		std::string extraHeader;
		int	iGlobalProgressEnd;
		int	iGlobalProgressBegin;
		long	lastHttpCode;
		std::map<std::string, std::string> cookies;

		CProgressWindow*	statusViewer;
#if !CURL_AT_LEAST_VERSION( 7,32,0 )
		static int show_progress_old(void *clientp, double dltotal, double dlnow, double ultotal, double ulnow);
#endif
		static int show_progress(void *clientp, curl_off_t dltotal, curl_off_t dlnow, curl_off_t ultotal, curl_off_t ulnow);
		static size_t CurlWriteToString(void *ptr, size_t size, size_t nmemb, void *data);
		static size_t CurlReadHeader(void *ptr, size_t size, size_t nmemb, void *data);

		void takeSetCookie(const std::string &value);
		std::string cookieHeader() const;
		bool perform(Request &req);

	public:
		CHTTPTool();
		void setStatusViewer( CProgressWindow* statusview );
		/* Set an extra HTTP header sent with the next download call, e.g.
		 * "X-Tuxbox-Service-Key: <value>". Pass an empty string to clear. */
		void setExtraHeader(const std::string& header);
		/* HTTP response code of the last request (0 if none). */
		long getLastHttpCode() const { return lastHttpCode; }

		bool downloadFile( const std::string & URL, const char * const downloadTarget, int globalProgressEnd=-1, int connecttimeout=10000, int timeout=1800);
		std::string downloadString(const std::string & URL, int globalProgressEnd=-1, int connecttimeout=10000, int timeout=1800);

		/* One REST call against another box's API. method is a token such as
		   GET, POST, PATCH or DELETE; headers are whole lines ("Name: value"),
		   which is how the class already takes its one extra header. A body is
		   sent for every method but GET, with contentType as its Content-Type
		   when that is given.

		   response takes the answer's body whatever the status line said, so a
		   refusal can be read. The return value is about the transport alone:
		   false means no answer arrived, and a 4xx or a 5xx comes back true with
		   getLastHttpCode() carrying the code.

		   Redirects are not followed here. Replaying a body under a method the
		   server picked, or carrying the session to whatever origin it names, is
		   not a decision this can make for the caller; a 3xx is handed back as
		   the status it is. */
		bool sendRequest(const std::string & method,
				const std::string & URL,
				std::string & response,
				const std::string & body = std::string(),
				const std::string & contentType = std::string(),
				const std::vector<std::string> & headers = std::vector<std::string>(),
				int connecttimeout=10000, int timeout=1800);

		/* Cookies the answers set are kept on the object and sent back with
		   every later request it makes, so one instance is one session. */
		void clearCookies();

};


#endif
