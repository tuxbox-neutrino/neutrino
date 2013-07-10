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


#ifndef __colorchooser__
#define __colorchooser__

#include <driver/framebuffer.h>
#include <system/localize.h>
#include <gui/widget/menue.h>

#include <string>

class CColorChooser : public CMenuTarget
{
	private:
		CFrameBuffer	*frameBuffer;
		int x;
		int y;
		int width;
		int height;
		int hheight,mheight; // head/menu font height
		int offset;
		int font_info;

		unsigned char * value[4]; // r, g, b, alpha

		neutrino_locale_t name;

		CChangeObserver* observer;

		void paint();
		void setColor();
		void paintSlider(int x, int y, unsigned char *spos, const neutrino_locale_t text, const char * const iconname, const bool selected);

	public:

		CColorChooser(const neutrino_locale_t Name, unsigned char *R, unsigned char *G, unsigned char *B, unsigned char* Alpha, CChangeObserver* Observer = NULL); // UTF-8

		void hide();
		int exec(CMenuTarget* parent, const std::string & actionKey);
		fb_pixel_t getColor(void);

};


#endif

