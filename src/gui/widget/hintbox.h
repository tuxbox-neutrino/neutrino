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


#ifndef __hintbox__
#define __hintbox__

#include <driver/fb_window.h>
#include <system/localize.h>
#include <gui/widget/icons.h>

#include <string>
#include <vector>

class CHintBox
{
 protected:

	CFBWindow *              window;

	unsigned int             entries_per_page;
	unsigned int             current_page;

	int                      width;
	int                      height;
	int                      textStartX;

	int                      fheight;
	int                      theight;
	const char *		 caption;
	char *                   message;
	std::vector<char *>      line;
	std::string              iconfile;
	void init(const char * const Caption, const char * const Text, const int Width, const char * const Icon);
	void refresh(void);

 public:
	// Text is UTF-8 encoded
	CHintBox(const neutrino_locale_t Caption, const char * const Text, const int Width = 450, const char * const Icon = NEUTRINO_ICON_INFO);
	CHintBox(const char * const Caption, const char * const Text, const int Width = 450, const char * const Icon = NEUTRINO_ICON_INFO);
	~CHintBox(void);

	bool has_scrollbar(void);
	void scroll_up(void);
	void scroll_down(void);

	void paint(void);
	void hide(void);
};

// Text is UTF-8 encoded
int ShowHint(const neutrino_locale_t Caption, const char * const Text, const int Width = 450, int timeout = -1, const char * const Icon = NEUTRINO_ICON_INFO);
int ShowHint(const neutrino_locale_t Caption, const neutrino_locale_t Text, const int Width = 450, int timeout = -1, const char * const Icon = NEUTRINO_ICON_INFO);
int ShowHint(const char * const Caption, const char * const Text, const int Width = 450, int timeout = -1, const char * const Icon = NEUTRINO_ICON_INFO);
int ShowHint(const char * const Caption, const neutrino_locale_t Text, const int Width = 450, int timeout = -1, const char * const Icon = NEUTRINO_ICON_INFO);


#endif
