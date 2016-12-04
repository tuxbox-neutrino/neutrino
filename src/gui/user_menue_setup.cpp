/*
	$port: user_menue_setup.cpp,v 1.1 2010/07/30 20:52:31 tuxbox-cvs Exp $

	user_menue setup implementation - Neutrino-GUI
	based up implementation by Günther

	Copyright (C) 2001 Steffen Hehn 'McClean'
	and some other guys
	Homepage: http://dbox.cyberphoria.org/

	Copyright (C) 2010 T. Graf 'dbt'
	Homepage: http://www.dbox2-tuning.net/

	Copyright (C) 2014 Stefan Seyfried

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

#include "user_menue_setup.h"

#include <global.h>
#include <neutrino.h>
#include <neutrino_menue.h>
#include <gui/plugins.h>
#include <gui/widget/icons.h>
#include <gui/widget/stringinput.h>
#include <gui/widget/keyboard_input.h>
#include <gui/widget/keychooser.h>
#include <gui/widget/msgbox.h>

#include <driver/screen_max.h>

#include <system/helpers.h>
#include <system/debug.h>

static bool usermenu_show = true;
#if 0
#if HAVE_SPARK_HARDWARE
static bool usermenu_show_three_d_mode = true;
#else
static bool usermenu_show_three_d_mode = false;
#endif
#endif
#if HAVE_SPARK_HARDWARE
static bool usermenu_show_cam = false; // FIXME -- use hwcaps?
#else
static bool usermenu_show_cam = true; // FIXME -- use hwcaps?
#endif
struct keyvals
{
	const int key;
	const neutrino_locale_t value;
	bool &show;
};

static keyvals usermenu_items[] =
{
	{ SNeutrinoSettings::ITEM_NONE,			LOCALE_USERMENU_ITEM_NONE,		usermenu_show },
	{ SNeutrinoSettings::ITEM_BAR,			LOCALE_USERMENU_ITEM_BAR,		usermenu_show },
	{ SNeutrinoSettings::ITEM_EPG_LIST,		LOCALE_EPGMENU_EVENTLIST,		usermenu_show },
	{ SNeutrinoSettings::ITEM_EPG_SUPER,		LOCALE_EPGMENU_EPGPLUS,			usermenu_show },
	{ SNeutrinoSettings::ITEM_EPG_INFO,		LOCALE_EPGMENU_EVENTINFO,		usermenu_show },
	{ SNeutrinoSettings::ITEM_EPG_MISC,		LOCALE_USERMENU_ITEM_EPG_MISC,		usermenu_show },
	{ SNeutrinoSettings::ITEM_AUDIO_SELECT,		LOCALE_AUDIOSELECTMENUE_HEAD,		usermenu_show },
	{ SNeutrinoSettings::ITEM_SUBCHANNEL,		LOCALE_NVODSELECTOR_HEAD,		usermenu_show },
	{ SNeutrinoSettings::ITEM_FILEPLAY,		LOCALE_MOVIEPLAYER_FILEPLAYBACK,	usermenu_show },
	{ SNeutrinoSettings::ITEM_AUDIOPLAY,		LOCALE_AUDIOPLAYER_NAME,		usermenu_show },
	{ SNeutrinoSettings::ITEM_INETPLAY,		LOCALE_INETRADIO_NAME,			usermenu_show },
	{ SNeutrinoSettings::ITEM_MOVIEPLAYER_MB,	LOCALE_MOVIEBROWSER_HEAD,		usermenu_show },
	{ SNeutrinoSettings::ITEM_TIMERLIST,		LOCALE_TIMERLIST_NAME,			usermenu_show },
	{ SNeutrinoSettings::ITEM_REMOTE,		LOCALE_RCLOCK_TITLE,			usermenu_show },
	{ SNeutrinoSettings::ITEM_FAVORITS,		LOCALE_FAVORITES_MENUEADD,		usermenu_show },
	{ SNeutrinoSettings::ITEM_TECHINFO,		LOCALE_EPGMENU_STREAMINFO,		usermenu_show },
	{ SNeutrinoSettings::ITEM_PLUGIN_TYPES,		LOCALE_USERMENU_ITEM_PLUGIN_TYPES,	usermenu_show },
	{ SNeutrinoSettings::ITEM_VTXT,			LOCALE_USERMENU_ITEM_VTXT,		usermenu_show },
	{ SNeutrinoSettings::ITEM_IMAGEINFO,		LOCALE_SERVICEMENU_IMAGEINFO,		usermenu_show },
	{ SNeutrinoSettings::ITEM_BOXINFO,		LOCALE_EXTRA_DBOXINFO,			usermenu_show },
	{ SNeutrinoSettings::ITEM_CAM,			LOCALE_CI_SETTINGS,			usermenu_show_cam },
	{ SNeutrinoSettings::ITEM_CLOCK,		LOCALE_CLOCK_SWITCH_ON,			usermenu_show },
	{ SNeutrinoSettings::ITEM_GAMES,		LOCALE_MAINMENU_GAMES,			usermenu_show },
	{ SNeutrinoSettings::ITEM_TOOLS,		LOCALE_MAINMENU_TOOLS,			usermenu_show },
	{ SNeutrinoSettings::ITEM_SCRIPTS,		LOCALE_MAINMENU_SCRIPTS,		usermenu_show },
	{ SNeutrinoSettings::ITEM_LUA,                  LOCALE_MAINMENU_LUA,			usermenu_show },
#if 0
	{ SNeutrinoSettings::ITEM_TUNER_RESTART,	LOCALE_SERVICEMENU_RESTART_TUNER,	usermenu_show },
	{ SNeutrinoSettings::ITEM_THREE_D_MODE,		LOCALE_THREE_D_SETTINGS,		usermenu_show_three_d_mode },
	{ SNeutrinoSettings::ITEM_RASS,			LOCALE_RASS_HEAD,			usermenu_show },
	{ SNeutrinoSettings::ITEM_NETZKINO,		LOCALE_MOVIEPLAYER_NKPLAYBACK,		usermenu_show },
#endif
	{ SNeutrinoSettings::ITEM_YOUTUBE,		LOCALE_MOVIEPLAYER_YTPLAYBACK,		usermenu_show },
	{ SNeutrinoSettings::ITEM_RECORD,		LOCALE_TIMERLIST_TYPE_RECORD,		usermenu_show },
	{ SNeutrinoSettings::ITEM_HDDMENU,		LOCALE_HDD_SETTINGS,			usermenu_show },
	{ SNeutrinoSettings::ITEM_NETSETTINGS,		LOCALE_MAINSETTINGS_NETWORK,		usermenu_show },
	{ SNeutrinoSettings::ITEM_SWUPDATE,		LOCALE_SERVICEMENU_UPDATE,		usermenu_show },
	{ SNeutrinoSettings::ITEM_LIVESTREAM_RESOLUTION,LOCALE_LIVESTREAM_RESOLUTION,		usermenu_show },
	{ SNeutrinoSettings::ITEM_ADZAP,		LOCALE_USERMENU_ITEM_ADZAP,		usermenu_show },
	{ SNeutrinoSettings::ITEM_MAX,			NONEXISTANT_LOCALE,			usermenu_show }
};

CUserMenuSetup::CUserMenuSetup(neutrino_locale_t menue_title, int menue_button)
{
	local = menue_title;
	button = menue_button;
	max_char = 24;
	width = 40;
	if (menue_button < (int) g_settings.usermenu.size())
		pref_name = g_settings.usermenu[button]->title; //set current button name as prefered name
	forwarder = NULL;

	for (int i = 0; usermenu_items[i].key != SNeutrinoSettings::ITEM_MAX; i++) {
		const char *loc = g_Locale->getText(usermenu_items[i].value);
		if (usermenu_items[i].show)
			options.push_back(loc);
		keys[loc] = to_string(usermenu_items[i].key);
		vals[keys[loc]] = loc;
	}

	int number_of_plugins = g_PluginList->getNumberOfPlugins();
	for (int count = 0; count < number_of_plugins; count++) {
		const char *loc = g_PluginList->getName(count);
		const char *key = g_PluginList->getFileName(count);
		if (loc && *loc && key && *key) {
			options.push_back(loc);
			keys[loc] = key;
			vals[keys[loc]] = loc;
		}
	}
	std::sort(options.begin(), options.end());
}

CUserMenuSetup::~CUserMenuSetup()
{
}

int CUserMenuSetup::exec(CMenuTarget* parent, const std::string &actionKey)
{
	if (actionKey == ">d") {
		int selected = ums->getSelected();
		if (selected >= item_offset) {
			if(parent)
				parent->hide();
			ums->removeItem(selected);
			ums->hide();
			return menu_return::RETURN_REPAINT;
		}
		return menu_return::RETURN_NONE;
	}

	if(parent)
		parent->hide();

	if (actionKey == ">a") {
		int selected = ums->getSelected();
		CMenuOptionStringChooser *c = new CMenuOptionStringChooser(std::string(""), NULL, true, NULL, CRCInput::RC_nokey, NULL, true);
		c->setOptions(options);
		std::string n(g_Locale->getText(LOCALE_USERMENU_ITEM_NONE));
		c->setOptionValue(n);
		if (selected >= item_offset)
			ums->insertItem(selected, c);
		else
			ums->addItem(c);
		ums->hide();
		return menu_return::RETURN_REPAINT;
	}

	int res = showSetup();
	checkButtonName();
	
	return res; 
}

static neutrino_locale_t locals[SNeutrinoSettings::ITEM_MAX];
neutrino_locale_t CUserMenuSetup::getLocale(unsigned int key)
{
	if(key >= SNeutrinoSettings::ITEM_MAX){
		key = SNeutrinoSettings::ITEM_NONE;
	}

	static bool initialized = false;
	if (!initialized) {
		initialized = true;
		for (int i = 0; i < SNeutrinoSettings::ITEM_MAX; i++)
			locals[i] = NONEXISTANT_LOCALE;
		for (int i = 0; usermenu_items[i].key != SNeutrinoSettings::ITEM_MAX; i++) 
			locals[usermenu_items[i].key] = usermenu_items[i].value;
	}
	return locals[key];
}

int CUserMenuSetup::showSetup()
{
	mn_widget_id_t widget_id = (button < SNeutrinoSettings::BUTTON_MAX) ? MN_WIDGET_ID_USERMENU_RED + button : NO_WIDGET_ID;
	ums = new CMenuWidget(local, NEUTRINO_ICON_KEYBINDING, width, widget_id);

	ums->addIntroItems();

	int old_key = g_settings.usermenu[button]->key;
	CKeyboardInput name(LOCALE_USERMENU_NAME, &g_settings.usermenu[button]->title);
	CMenuForwarder * mf = new CMenuForwarder(LOCALE_USERMENU_NAME, true, NULL, &name);

	ums->addItem(mf);

	if (button >= SNeutrinoSettings::BUTTON_MAX) {
		CKeyChooser *kc = new CKeyChooser(&g_settings.usermenu[button]->key, LOCALE_USERMENU_KEY_SELECT, NEUTRINO_ICON_SETTINGS);
		CMenuDForwarder *kf = new CMenuDForwarder(LOCALE_USERMENU_KEY, true, kc->getKeyName(), kc);
		ums->addItem(kf);
	}

	ums->addItem(new CMenuSeparator(CMenuSeparator::STRING | CMenuSeparator::LINE, LOCALE_USERMENU_ITEMS));

	std::vector<std::string> items = ::split(g_settings.usermenu[button]->items, ',');
	item_offset = ums->getItemsCount();
	for (std::vector<std::string>::iterator it = items.begin(); it != items.end(); ++it) {
		CMenuOptionStringChooser *c = new CMenuOptionStringChooser(std::string(""), NULL, true, NULL, CRCInput::RC_nokey, NULL, true);
		c->setTitle(LOCALE_USERMENU_ITEMS);
		c->setOptions(options);
		c->setOptionValue(vals[*it]);
		ums->addItem(c);
	}

	const struct button_label footerButtons[2] = {
		{ NEUTRINO_ICON_BUTTON_RED, LOCALE_BOUQUETEDITOR_DELETE },
		{ NEUTRINO_ICON_BUTTON_GREEN, LOCALE_BOUQUETEDITOR_ADD }
	};
	ums->setFooter(footerButtons, 2);
	ums->addKey(CRCInput::RC_red, this, ">d");
	ums->addKey(CRCInput::RC_green, this, ">a");

	int res = ums->exec(NULL, "");
	int items_end = ums->getItemsCount();

	const char *delim = "";
	g_settings.usermenu[button]->items = "";
	std::string none = to_string(SNeutrinoSettings::ITEM_NONE);
	for (int count = item_offset; count < items_end; count++) {
		std::string lk = keys[static_cast<CMenuOptionStringChooser*>(ums->getItem(count))->getOptionValue()];
		if (lk == none)
			continue;
		g_settings.usermenu[button]->items += delim + lk;
		delim = ",";
	}

	delete ums;

	if (forwarder && (old_key != (int) g_settings.usermenu[button]->key))
		forwarder->setName(CRCInput::getKeyName(g_settings.usermenu[button]->key));

	return res;
}


//check button name for details like empty string and show an user message on issue
void CUserMenuSetup::checkButtonName()
{
	//count of configured items
	int used_items = getUsedItemsCount();

	//warn if no items defined and reset menu name, if empty
	if (used_items == 0)
	{
		if (!g_settings.usermenu[button]->title.empty()){
			// DisplayInfoMessage(g_Locale->getText(LOCALE_USERMENU_MSG_WARNING_NO_ITEMS));
			g_settings.usermenu[button]->title = "";
		}
		//exit function
		return;
	}

#if 0
	//if found only 1 configured item, ensure that the caption of usermenu is the same like this
	if (used_items == 1) {
		bool dummy;
		g_settings.usermenu[button]->title =  CUserMenu::getUserMenuButtonName(button, dummy);
	}
#endif

	if (button < USERMENU_ITEMS_COUNT && g_settings.usermenu[button]->title.empty())
	{
		std::string msg(g_Locale->getText(LOCALE_USERMENU_MSG_INFO_IS_EMPTY));
		msg += g_Locale->getText(usermenu[button].def_name);
		DisplayInfoMessage(msg.c_str());

		g_settings.usermenu[button]->title = g_Locale->getText(usermenu[button].def_name);
	}
}

//get count of used items
int CUserMenuSetup::getUsedItemsCount()
{
	return ::split(g_settings.usermenu[button]->items, ',').size();
}
