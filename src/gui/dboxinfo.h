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


#ifndef __dboxinfo__
#define __dboxinfo__

#include <gui/widget/menue.h>
#include <driver/framebuffer.h>
#include <string>

class CDBoxInfoWidget : public CMenuTarget
{
	private:

		CFrameBuffer	*frameBuffer;
		int x;
		int y;
		int width;
		int height;
		int hheight,mheight; // head/menu font height
		int fontWidth;
		int sizeWidth;
		int percWidth;
		int nameWidth;

		void paint();

	public:

		CDBoxInfoWidget();

		void hide();
		int exec(CMenuTarget* parent, const std::string & actionKey);

};


#endif
