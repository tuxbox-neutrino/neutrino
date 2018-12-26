/*
	lcd4l setup

	Copyright (C) 2012 'defans'
	Homepage: http://www.bluepeercrew.us/

	Copyright (C) 2012-2016 'vanhofen'
	Homepage: http://www.neutrino-images.de/

	Modded    (C) 2016 'TangoCash'

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


#ifndef __lcd4l_setup__
#define __lcd4l_setup__

#include <gui/widget/menue.h>

class CLCD4lSetup : public CMenuTarget, CChangeObserver
{
	private:
		CMenuOptionNumberChooser *nc;
		CMenuOptionChooser *mc;
		CMenuForwarder * mf;

		int width;
		int show();

	public:
		CLCD4lSetup();
		~CLCD4lSetup();
		int exec(CMenuTarget* parent, const std::string &actionkey);
		virtual bool changeNotify(const neutrino_locale_t OptionName, void * /*data*/);
};

#endif
