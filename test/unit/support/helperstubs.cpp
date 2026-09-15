/*
 * helperstubs.cpp - stubs the tests link in place of the box
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

// helpers.cpp is linked whole for the cases that drive my_system(). The same
// object carries the box's disk wakeup and its text shortening, which draw on
// the screen and which nothing here calls, so what those two need is defined
// rather than linked. Declared here rather than included, so that no screen
// header reaches this binary: only the names have to match, and a signature
// that changes breaks the link rather than a case.

#include <cstddef>
#include <string>

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
