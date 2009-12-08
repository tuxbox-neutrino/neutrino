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


#ifndef __alphasetup__
#define __alphasetup__


#include "widget/menue.h"

#include <driver/framebuffer.h>
#include <system/localize.h>

#include <string>


class CAlphaSetup : public CMenuTarget
{
	private:
		CFrameBuffer	*frameBuffer;
		int x;
		int y;
		int width;
		int height;
		int hheight,mheight; // head/menu font height

		unsigned char *alpha1;
		unsigned char *alpha2;


		neutrino_locale_t name;

		CChangeObserver* observer;

		void paint();
		void setAlpha();
		void paintSlider(const int x, const int y, const unsigned char * const spos, const neutrino_locale_t text, const char * const iconname, const bool selected);

	public:

		CAlphaSetup(const neutrino_locale_t Name, unsigned char* Alpha1, unsigned char* Alpha2, CChangeObserver* Observer = NULL);

		void hide();
		int exec(CMenuTarget* parent, const std::string & actionKey);

};


#endif

