/*
	(C)2012 by martii

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

#ifndef __psisetup__
#define __psisetup__

#include <gui/widget/menue.h>
#include <gui/components/cc_item_progressbar.h>
#include <driver/framebuffer.h>
#include <system/localize.h>
#include <string>

class CPSISetup : public CMenuTarget, public CChangeObserver
{
	private:
		CFrameBuffer * frameBuffer;
		int x;
		int y;
		int dx;
		int dy;
		int width;
		int locWidth;
		int locHeight;
		int lineHeight;
		int sliderOffset;
		int selected;
		bool needsBlit;
		neutrino_locale_t name;

		void paint ();
		void setPSI ();
		void paintSlider (int i);
		// unsigned char readProcPSI(int);
		CPSISetup (const neutrino_locale_t Name);
		void writeProcPSI ();
		void writeProcPSI (int);

	public:
		int exec (CMenuTarget * parent, const std::string & actionKey);
		void hide ();
		void blankScreen (bool b = true);
		bool changeNotify(const neutrino_locale_t, void *);
		static CPSISetup *getInstance();
};
#endif
