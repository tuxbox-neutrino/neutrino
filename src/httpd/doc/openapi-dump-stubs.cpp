/*
 * openapi-dump-stubs.cpp - stubs that let the document dump link without the box
 *
 * Copyright (C) 2026 NI-Team
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

// openapi_dump links libhttpd.a whole, and since the mount point in
// server.cpp builds a CyhookHandler on every call, that pulls in
// helpers.cpp for the same reason test/unit/support/helperstubs.cpp already names:
// the disk-wakeup and text-shortening code that object also carries reaches a
// few GUI classes that this offline documentation dump never runs and has no
// reason to link for real. Same approach as that file: declared here rather
// than included, so no screen header reaches this binary, and only the
// mangled names have to match. g_settings, the other thing helpers.cpp
// reads, is the real struct in openapi-dump-globals.cpp instead, because a
// fake one here would collide with global.h the moment anything in this
// binary needs the real Font or CLocaleManager it also declares.

#include <cstddef>
#include <string>
#include <vector>

enum neutrino_locale_t { LOCALE_LINK_STUB };

class CFrameBuffer
{
	public:
		static CFrameBuffer *getInstance();
};
CFrameBuffer *CFrameBuffer::getInstance() { return NULL; }

class CLocaleManager
{
	public:
		const char *getText(neutrino_locale_t) const;
};
const char *CLocaleManager::getText(neutrino_locale_t) const { return ""; }
CLocaleManager *g_Locale = NULL;

class Font
{
	public:
		int getRenderWidth(const std::string &, bool);
};
int Font::getRenderWidth(const std::string &, bool) { return 0; }
Font *g_Font[64];

class CCDraw
{
	public:
		void hide();
};
void CCDraw::hide() {}

class CComponentsForm
{
	public:
		void hideCCItems();
};
void CComponentsForm::hideCCItems() {}

class CComponentsWindow
{
	public:
		void paint(const bool &);
};
void CComponentsWindow::paint(const bool &) {}

class CHintBox
{
	public:
		CHintBox(neutrino_locale_t, const char *, int, const char *, const char *,
			 const int &, const int &, const int &, const unsigned int &,
			 const unsigned int &, const unsigned int &, const int &);
		~CHintBox();
};
CHintBox::CHintBox(neutrino_locale_t, const char *, int, const char *, const char *,
		   const int &, const int &, const int &, const unsigned int &,
		   const unsigned int &, const unsigned int &, const int &) {}
CHintBox::~CHintBox() {}

void netGetHostname(std::string &);
void netGetHostname(std::string &) {}
bool netDefaultRoute(std::string &, std::string &);
bool netDefaultRoute(std::string &, std::string &) { return false; }

/* The head of a carried answer is written with nhttpd's own writer
   (carry.cpp), which the routes that carry their answers reach through the
   tables; the dump carries none. */
class CyhookHandler
{
	public:
		CyhookHandler();
		~CyhookHandler();
		std::string BuildHeader(bool);
};
CyhookHandler::CyhookHandler() {}
CyhookHandler::~CyhookHandler() {}
std::string CyhookHandler::BuildHeader(bool) { return std::string(); }

/* PUT /api/v1/system/webserver holds the web folders it saves to the old server's
   rule for them (ystatic.cpp); the dump answers no request and links no nhttpd. */
namespace ystatic
{
std::vector<std::string> keptPlaces();
std::vector<std::string> keptPlaces() { return std::vector<std::string>(); }
std::string rootRefused(const std::string &, const std::vector<std::string> &);
std::string rootRefused(const std::string &, const std::vector<std::string> &) { return std::string(); }
}

/* The page that asks before a command of the old interface runs hides a value by the
   rule the web log reads parameter names by (ylogging.cpp); the dump builds no such
   page, and here every name counts as one that holds a secret. */
namespace ylog
{
bool secretParam(const std::string &);
bool secretParam(const std::string &) { return true; }
}
