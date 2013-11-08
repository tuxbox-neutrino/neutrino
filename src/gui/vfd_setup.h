/*
	$similar port: lcd_setup.h,v 1.1 2010/07/30 20:52:16 tuxbox-cvs Exp $

	vfd setup implementation, similar to lcd_setup.h of tuxbox-cvs - Neutrino-GUI

	Copyright (C) 2001 Steffen Hehn 'McClean'
	and some other guys
	Homepage: http://dbox.cyberphoria.org/

	Copyright (C) 2010 T. Graf 'dbt'
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

#ifndef __lcd_setup__
#define __lcd_setup__

#include <gui/widget/menue.h>
#include <gui/widget/stringinput.h>

#include <string>

 class CVfdSetup : public CMenuTarget, CChangeObserver
{	
	private:
		int width;
		
		int showSetup();
		void showBrightnessSetup(CMenuWidget *mn_widget);
		void showLedSetup(CMenuWidget *mn_led_widget);
		void showBacklightSetup(CMenuWidget *mn_led_widget);
		virtual bool changeNotify(const neutrino_locale_t OptionName, void *data);
		int brightness;
		int brightnessstandby;
		int brightnessdeepstandby;
		
		bool vfd_enabled; 
		
		CStringInput * dim_time;

	public:
		CVfdSetup();
		~CVfdSetup();
		int exec(CMenuTarget* parent, const std::string & actionKey);
};


#endif
