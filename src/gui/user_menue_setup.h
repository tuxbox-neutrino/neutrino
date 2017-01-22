/*
	$port: user_menue_setup.h,v 1.1 2010/07/30 20:52:31 tuxbox-cvs Exp $

	user_menue setup implementation - Neutrino-GUI
	based up implementation by Günther

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

#ifndef __user_menue_setup__
#define __user_menue_setup__

#include <gui/widget/menue.h>

#include <system/settings.h>

#include <string>
#include <vector>
#include <map>

typedef struct usermenu_props_t
{
	neutrino_locale_t menue_title;
	const int menue_button;
	const neutrino_msg_t DirectKey;
	const char* IconName;
	neutrino_locale_t def_name;

}usermenu_props_struct_t;

#define USERMENU_ITEMS_COUNT SNeutrinoSettings::BUTTON_MAX
const struct usermenu_props_t usermenu[USERMENU_ITEMS_COUNT] =
{
	{LOCALE_USERMENU_BUTTON_RED	, SNeutrinoSettings::BUTTON_RED		, CRCInput::RC_red	,NEUTRINO_ICON_BUTTON_RED	,LOCALE_USERMENU_TITLE_RED	},
	{LOCALE_USERMENU_BUTTON_GREEN	, SNeutrinoSettings::BUTTON_GREEN	, CRCInput::RC_green	,NEUTRINO_ICON_BUTTON_GREEN	,LOCALE_USERMENU_TITLE_GREEN	},
	{LOCALE_USERMENU_BUTTON_YELLOW	, SNeutrinoSettings::BUTTON_YELLOW	, CRCInput::RC_yellow	,NEUTRINO_ICON_BUTTON_YELLOW	,LOCALE_USERMENU_TITLE_YELLOW	},
	{LOCALE_USERMENU_BUTTON_BLUE	, SNeutrinoSettings::BUTTON_BLUE	, CRCInput::RC_blue	,NEUTRINO_ICON_BUTTON_BLUE	,LOCALE_USERMENU_TITLE_BLUE	}
};

class CUserMenuSetup : public CMenuTarget
{	
	private:
		int width;
		int max_char;
		int button;
		int item_offset;
		std::string pref_name;
		neutrino_locale_t local;
		CMenuForwarder *forwarder;
		CMenuWidget * ums;
		std::vector<std::string> options;
		std::map<std::string,std::string> keys;
		std::map<std::string,std::string> vals;

		int showSetup();
		void checkButtonName();

	public:
		CUserMenuSetup(neutrino_locale_t menue_title, int menue_button);
		~CUserMenuSetup();
		void setCaller(CMenuForwarder *fw) { forwarder = fw ; }
		int getUsedItemsCount();
		int exec(CMenuTarget* parent, const std::string & actionKey);
		static neutrino_locale_t getLocale(unsigned int i);
};

#endif
