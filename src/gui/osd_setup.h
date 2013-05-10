/*
	$port: osd_setup.h,v 1.1 2010/08/28 22:47:09 tuxbox-cvs Exp $

	osd_setup implementation - Neutrino-GUI

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

#ifndef __osd_setup__
#define __osd_setup__

#include <gui/widget/menue.h>

#include <driver/framebuffer.h>

#include <system/setting_helpers.h>
#include <system/settings.h>

#include <string>

class COsdSetup : public CMenuTarget, public CChangeObserver
{	
	private:
		CColorSetupNotifier *colorSetupNotifier;
		CFontSizeNotifier *fontsizenotifier;
		CMenuWidget *osd_menu;
		CMenuWidget *submenu_menus;

		int width;
		bool is_wizard;
		int show_menu_hints;
		int show_tuner_icon;

		int showOsdSetup();
 		void showOsdMenueColorSetup(CMenuWidget *menu_colors);
		void showOsdFontSizeSetup(CMenuWidget *menu_fonts);
		void showOsdTimeoutSetup(CMenuWidget *menu_timeout);
		void showOsdMenusSetup(CMenuWidget *menu_menus);
		void showOsdInfobarSetup(CMenuWidget *menu_infobar);
		void showOsdChanlistSetup(CMenuWidget *menu_chanlist);
		void showOsdEventlistSetup(CMenuWidget *menu_eventlist);
		void showOsdVolumeSetup(CMenuWidget *menu_volume);
		void showOsdScreenShotSetup(CMenuWidget *menu_screenshot);

 		void AddFontSettingItem(CMenuWidget &font_Settings, const SNeutrinoSettings::FONT_TYPES number_of_fontsize_entry);
 
	public:
		enum INFOBAR_CHANNEL_LOGO_POS_OPTIONS	
		{
			INFOBAR_NO_LOGO,
			INFOBAR_LOGO_AS_CHANNELLUM,
			INFOBAR_LOGO_AS_CHANNELNAME,
			INFOBAR_LOGO_BESIDE_CHANNELNAME
		};

		enum INFOBAR_CHANNEL_LOGO_BACKROUND_OPTIONS	
		{
			INFOBAR_NO_BACKGROUND,
			INFOBAR_LOGO_FRAMED,
			INFOBAR_LOGO_SHADED
		};
		
		enum OSD_SETUP_MODE
		{
			OSD_SETUP_MODE_WIZARD_NO   = 0,
			OSD_SETUP_MODE_WIZARD   = 1
		};
		
		COsdSetup(bool wizard_mode = OSD_SETUP_MODE_WIZARD_NO);
		~COsdSetup();
		int exec(CMenuTarget* parent, const std::string & actionKey);
		bool changeNotify(const neutrino_locale_t OptionName, void * /*data*/);
		int showContextChanlistMenu();
};

#endif
