/*
	$port: user_menue_setup.cpp,v 1.1 2010/07/30 20:52:31 tuxbox-cvs Exp $

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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif


#include "gui/user_menue_setup.h"

#include <global.h>
#include <neutrino.h>
#include <neutrino_menue.h>
#include <gui/widget/icons.h>
#include <gui/widget/stringinput.h>
#include <gui/widget/keychooser.h>

#include <driver/screen_max.h>

#include <system/debug.h>

CUserMenuSetup::CUserMenuSetup(neutrino_locale_t menue_title, int menue_button)
{
	local = menue_title;
	button = menue_button;

	width = w_max (40, 10);
}

CUserMenuSetup::~CUserMenuSetup()
{

}


#define USERMENU_ITEM_OPTION_COUNT SNeutrinoSettings::ITEM_MAX
const CMenuOptionChooser::keyval USERMENU_ITEM_OPTIONS[USERMENU_ITEM_OPTION_COUNT] =
{
	{ SNeutrinoSettings::ITEM_NONE,			LOCALE_USERMENU_ITEM_NONE },
	{ SNeutrinoSettings::ITEM_BAR,			LOCALE_USERMENU_ITEM_BAR },
	{ SNeutrinoSettings::ITEM_EPG_LIST,		LOCALE_EPGMENU_EVENTLIST },
	{ SNeutrinoSettings::ITEM_EPG_SUPER,		LOCALE_EPGMENU_EPGPLUS },
	{ SNeutrinoSettings::ITEM_EPG_INFO,		LOCALE_EPGMENU_EVENTINFO },
	{ SNeutrinoSettings::ITEM_EPG_MISC,		LOCALE_USERMENU_ITEM_EPG_MISC },
	{ SNeutrinoSettings::ITEM_AUDIO_SELECT,		LOCALE_AUDIOSELECTMENUE_HEAD },
	{ SNeutrinoSettings::ITEM_SUBCHANNEL,		LOCALE_INFOVIEWER_SUBSERVICE },
	{ SNeutrinoSettings::ITEM_RECORD,		LOCALE_TIMERLIST_TYPE_RECORD },
	{ SNeutrinoSettings::ITEM_MOVIEPLAYER_MB,	LOCALE_MOVIEBROWSER_HEAD },
	{ SNeutrinoSettings::ITEM_TIMERLIST,		LOCALE_TIMERLIST_NAME },
	{ SNeutrinoSettings::ITEM_REMOTE,		LOCALE_RCLOCK_MENUEADD },
	{ SNeutrinoSettings::ITEM_FAVORITS,		LOCALE_FAVORITES_MENUEADD },
	{ SNeutrinoSettings::ITEM_TECHINFO,		LOCALE_EPGMENU_STREAMINFO },
	{ SNeutrinoSettings::ITEM_PLUGIN,		LOCALE_TIMERLIST_PLUGIN },
	{ SNeutrinoSettings::ITEM_VTXT,			LOCALE_USERMENU_ITEM_VTXT },
	{ SNeutrinoSettings::ITEM_IMAGEINFO,		LOCALE_SERVICEMENU_IMAGEINFO },
	{ SNeutrinoSettings::ITEM_BOXINFO,		LOCALE_EXTRA_DBOXINFO },
	{ SNeutrinoSettings::ITEM_CAM,			LOCALE_CI_SETTINGS }
};

int CUserMenuSetup::exec(CMenuTarget* parent, const std::string &)
{
	if(parent != NULL)
		parent->hide();

	return showSetup();
}

int CUserMenuSetup::showSetup()
{
	mn_widget_id_t widget_id = MN_WIDGET_ID_USERMENU_RED + button; //add up ''button'' and becomes to MN_WIDGET_ID_USERMENU_ GREEN, MN_WIDGET_ID_USERMENU_ YELLOW, MN_WIDGET_ID_USERMENU_BLUE
	CMenuWidget * ums = new CMenuWidget(local, NEUTRINO_ICON_KEYBINDING, width, widget_id);
	
	//CUserMenuNotifier *notify = new CUserMenuNotifier();
	CStringInputSMS name(LOCALE_USERMENU_NAME, &g_settings.usermenu_text[button], 11, NONEXISTANT_LOCALE, NONEXISTANT_LOCALE, "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyzäöüß/- "/*, notify*/);

	CMenuForwarder *mf = new CMenuForwarder(LOCALE_USERMENU_NAME, true, g_settings.usermenu_text[button],&name);
// 	notify->setItem(mf);

	//-------------------------------------
	ums->addIntroItems();
	//-------------------------------------
	ums->addItem(mf);
	ums->addItem(GenericMenuSeparatorLine);
	//-------------------------------------
	char text[10];
	for(int item = 0; item < SNeutrinoSettings::ITEM_MAX && item <13; item++) // Do not show more than 13 items
	{
		snprintf(text,10,"%d:",item);
		text[9]=0;// terminate for sure
		ums->addItem( new CMenuOptionChooser(text, &g_settings.usermenu[button][item], USERMENU_ITEM_OPTIONS, USERMENU_ITEM_OPTION_COUNT, true, NULL, CRCInput::RC_nokey, "", true));
	}

	int res = ums->exec(NULL, "");
	ums->hide();
	delete ums;
	return res;
}

