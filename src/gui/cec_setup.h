/*
	Mediaplayer selection menu - Neutrino-GUI

	Copyright (C) 2001 Steffen Hehn 'McClean'
	and some other guys
	Homepage: http://dbox.cyberphoria.org/

	Copyright (C) 2011 T. Graf 'dbt'
	Homepage: http://www.dbox2-tuning.net/

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

#ifndef __CEC_SETUP__
#define __CEC_SETUP__

#include <gui/widget/menue.h>

#include <string>

class CCECSetup : public CMenuTarget, CChangeObserver
{
	private:
		CMenuOptionChooser 	*cec1, *cec2, *cec3;
				
		int width;

		int showMenu();
 		

	public:	
		CCECSetup();
		~CCECSetup();
#if HAVE_SH4_HARDWARE
		void setCECSettings(bool);
#else
		void setCECSettings();
#endif
		int exec(CMenuTarget* parent, const std::string & actionKey);
		virtual bool changeNotify(const neutrino_locale_t OptionName, void * data);
};


#endif
