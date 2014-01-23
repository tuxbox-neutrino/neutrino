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


#ifndef __hintboxext__
#define __hintboxext__

#include <driver/fb_window.h>
#include <system/localize.h>

#include <gui/widget/drawable.h>
#include <gui/widget/icons.h>

#include <string>
#include <vector>

class CHintBoxExt
{
 protected:

	CFBWindow *              m_window;

	unsigned int             m_currentPage;
	std::vector<int>         m_startEntryOfPage;
	int                      m_maxEntriesPerPage;
	int                      m_pages;

	int                      m_width;
	int                      m_height;
	int                      m_bbheight; /* a button bar at the bottom? */
	int                      textStartX;

	int                      m_fheight;
	int                      m_theight;
	neutrino_locale_t        m_caption;
	std::string              m_captionString;
	char *                   m_message;
	ContentLines             m_lines;
	std::string              m_iconfile;
	bool			 bgPainted;
	
	void refresh(bool toround = 1);

 public:
	CHintBoxExt(const neutrino_locale_t Caption, const char * const Text, const int Width, const char * const Icon);
	CHintBoxExt(const neutrino_locale_t Caption, ContentLines& lines, const int Width = 450, const char * const Icon = NEUTRINO_ICON_INFO);
	CHintBoxExt(const std::string &Caption, const char * const Text, const int Width, const char * const Icon);
	CHintBoxExt(const std::string &Caption, ContentLines& lines, const int Width = 450, const char * const Icon = NEUTRINO_ICON_INFO);

	~CHintBoxExt(void);
	
	void init(const neutrino_locale_t Caption, const std::string &CaptionString, const int Width, const char * const Icon);

	bool has_scrollbar(void);
	void scroll_up(void);
	void scroll_down(void);

	void paint(bool toround = 1);
	void hide(void);
};

#endif
