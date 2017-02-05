/*
	Neutrino-GUI  -   DBoxII-Project

	Copyright (C) 2001 Steffen Hehn 'McClean'
	Homepage: http://dbox.cyberphoria.org/

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


#ifndef __listbox__
#define __listbox__

#include "menue.h"
#include "listhelpers.h"

#include <string>

class CFrameBuffer;
class CListBox : public CMenuTarget, public CListHelpers
{
	protected:
		CFrameBuffer*	frameBuffer;
		bool            modified;
		std::string	caption;
		int		width;
		int		height;
		int		x;
		int		y;

		int		fheight;
		int		theight;
		int		iheight;
		int		iconoffset;

		unsigned int	selected;
		unsigned int	liststart;
		unsigned int	listmaxshow;

		int 		ButtonHeight;

		//----------------------------

		virtual void paintItem(int pos);
		virtual void paint();
		virtual	void paintHead();
		virtual void paintFoot();
		virtual void hide();


		//------hier Methoden überschreiben-------

		//------Fernbedienungsevents--------------
		virtual void onRedKeyPressed(){};
		virtual void onGreenKeyPressed(){};
		virtual void onYellowKeyPressed(){};
		virtual void onBlueKeyPressed(){};
		virtual void onOkKeyPressed(){};
		virtual void onOtherKeyPressed( int /*key*/ ){};

		//------gibt die Anzahl der Listenitems---
		virtual unsigned int getItemCount();

		//------malen der Items-------------------
		virtual int getItemHeight();
		virtual void paintItem(uint32_t itemNr, int paintNr, bool selected);
		void updateSelection(unsigned int newpos);

		//------Benutzung von setModified---------
		void setModified(void);

	public:
		CListBox(const char * const Caption);
		virtual int exec(CMenuTarget* parent, const std::string & actionKey);
};


#endif
