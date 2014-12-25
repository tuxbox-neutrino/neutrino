/*

	user_menue setup implementation - Neutrino-GUI
	based up implementation by Günther

	Copyright (C) 2001 Steffen Hehn 'McClean'
	and some other guys
	Homepage: http://dbox.cyberphoria.org/

	Rework
	Copyright (C) 2011 T. Graf 'dbt'
	Homepage: http://www.dbox2-tuning.net/

	Copyright (C) 2013-2014 Stefan Seyfried

        License: GPL

        This program is free software; you can redistribute it and/or
	modify it under the terms of the GNU General Public
	License as published by the Free Software Foundation; either
	version 2 of the License, or (at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
	Library General Public License for more details.

	You should have received a copy of the GNU General Public
	License along with this program; if not, write to the
	Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
	Boston, MA  02110-1301, USA.

		
	NOTE for ignorant distributors:
	It's not allowed to distribute any compiled parts of this code, if you don't accept the terms of GPL.
	Please read it and understand it right!
	This means for you: Hold it, if not, leave it! You could face legal action! 
	Otherwise ask the copyright owners, anything else would be theft!
*/

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include <unistd.h>

#include "user_menue.h"
#include "user_menue_setup.h"
#include "subchannel_select.h"
#include "favorites.h"
#include "audio_select.h"
#include "streaminfo2.h"
#include "epgplus.h"
#include "epgview.h"
#include "eventlist.h"
#include "movieplayer.h"
#include "timerlist.h"
#include "plugins.h"
#include "imageinfo.h"
#include "dboxinfo.h"
#if !HAVE_SPARK_HARDWARE
#include "cam_menu.h"
#endif
#include "pluginlist.h"
#include "infoclock.h"

#include <global.h>
#include <neutrino.h>
#include <mymenu.h>

#include <gui/widget/icons.h>
#include <gui/network_setup.h>
#include <gui/update_menue.h>
#include <gui/hdd_menu.h>

#include <driver/radiotext.h>
#include <driver/record.h>
#include <driver/screen_max.h>

#include <system/helpers.h>

#include <daemonc/remotecontrol.h>
extern CRemoteControl * g_RemoteControl;	/* neutrino.cpp */
extern CPlugins * g_PluginList;			/* neutrino.cpp */
#if !HAVE_SPARK_HARDWARE
extern CCAMMenuHandler * g_CamHandler;
#endif

// 
#include <system/debug.h>

CUserMenu::CUserMenu()
{
	width = 0;
}

CUserMenu::~CUserMenu()
{
}

bool CUserMenu::showUserMenu(neutrino_msg_t msg)
{
	int button = -1;
	unsigned ums = g_settings.usermenu.size();
	for (unsigned int i = 0; i < ums; i++)
		if (g_settings.usermenu[i]->key == msg) {
			button = i;
			break;
		}

	if (button < 0)
		return false;

	int pers = -1;
	switch(msg) {
		case CRCInput::RC_red:
			pers = SNeutrinoSettings::P_MAIN_RED_BUTTON;
			button = SNeutrinoSettings::BUTTON_RED;
			break;
		case CRCInput::RC_green:
			pers = SNeutrinoSettings::P_MAIN_GREEN_BUTTON;
			button = SNeutrinoSettings::BUTTON_GREEN;
			break;
		case CRCInput::RC_yellow:
			pers = SNeutrinoSettings::P_MAIN_YELLOW_BUTTON;
			button = SNeutrinoSettings::BUTTON_YELLOW;
			break;
		case CRCInput::RC_blue:
			pers = SNeutrinoSettings::P_MAIN_BLUE_BUTTON;
			button = SNeutrinoSettings::BUTTON_BLUE;
			break;
	}

	CNeutrinoApp::getInstance()->StopSubtitles();

	if (pers > -1 && (g_settings.personalize[pers] != CPersonalizeGui::PERSONALIZE_ACTIVE_MODE_ENABLED)) {
		ShowHint(LOCALE_MESSAGEBOX_INFO, g_Locale->getText(LOCALE_PERSONALIZE_MENUDISABLEDHINT),450, 10);
		CNeutrinoApp::getInstance()->StartSubtitles();
		return true;
	}

	width = w_max (40, 10);

	CMenuItem* menu_item = NULL;
	CMenuItem* last_menu_item = NULL;
	CColorKeyHelper keyhelper;
	
	//set default feature key
	neutrino_msg_t key = feat_key[CPersonalizeGui::PERSONALIZE_FEAT_KEY_AUTO].key; //CRCInput::RC_nokey

	const char * icon = NULL;
	int menu_items = 0;

	// define classes
	CSubChannelSelectMenu subchanselect;
	CPluginsExec plugins;
	CNeutrinoApp * neutrino	= CNeutrinoApp::getInstance();
	
	std::string txt = g_settings.usermenu[button]->title;
	if (button < COL_BUTTONMAX && txt.empty())
		txt = g_Locale->getText(user_menu[button].caption);

	CMenuWidget *menu = new CMenuWidget(txt, (button < COL_BUTTONMAX) ? user_menu[button].menu_icon_def : "", width);
	if (menu == NULL)
		return true;

	if (button < COL_BUTTONMAX)	
		menu->setSelected(user_menu[button].selected);
	
	//show cancel button if configured
	if (g_settings.personalize[SNeutrinoSettings::P_UMENU_SHOW_CANCEL])
		menu->addIntroItems(NONEXISTANT_LOCALE, NONEXISTANT_LOCALE, CMenuWidget::BTN_TYPE_CANCEL);
	else
		menu->addItem(GenericMenuSeparator);
	
	std::string itemstr_last("1");

	std::vector<std::string> items = ::split(g_settings.usermenu[button]->items, ',');
	for (std::vector<std::string>::iterator it = items.begin(); it != items.end(); ++it) {
		if (it->empty())
			continue;
		if (*it == itemstr_last)
			continue;
		int item = -1;
		if (it->find_first_not_of("0123456789") == std::string::npos)
			item = atoi(*it);
		menu_item = NULL;
		switch (item) {
		case SNeutrinoSettings::ITEM_NONE:
			continue;
		case SNeutrinoSettings::ITEM_BAR:
			menu->addItem(GenericMenuSeparatorLine);
			break;
		case SNeutrinoSettings::ITEM_FAVORITS:
			keyhelper.get(&key,&icon,feat_key[g_settings.personalize[SNeutrinoSettings::P_FEAT_KEY_FAVORIT]].key); //CRCInput::RC_green
			menu_item = new CMenuDForwarder(LOCALE_FAVORITES_MENUEADD, true, NULL, new CFavorites, "-1", key, icon);
			// FIXME menu_item->setHint("", NONEXISTANT_LOCALE);
			break;
		case SNeutrinoSettings::ITEM_RECORD:
			if (g_settings.recording_type == RECORDING_OFF)
				break;
			keyhelper.get(&key,&icon,CRCInput::RC_red);
			menu_item = new CMenuForwarder(LOCALE_MAINMENU_RECORDING, true, NULL, CRecordManager::getInstance(), "-1", key, icon);
			// FIXME menu_item->setHint("", NONEXISTANT_LOCALE);
			break;
		case SNeutrinoSettings::ITEM_MOVIEPLAYER_MB:
			if (g_settings.recording_type == RECORDING_OFF)
				break;
			keyhelper.get(&key,&icon,CRCInput::RC_green);
			menu_item = new CMenuForwarder(LOCALE_MOVIEBROWSER_HEAD, true, NULL, neutrino, "tsmoviebrowser", key, icon);
			menu_item->setHint(NEUTRINO_ICON_HINT_MB, LOCALE_MENU_HINT_MB);
			break;
		case SNeutrinoSettings::ITEM_TIMERLIST:
			keyhelper.get(&key,&icon,feat_key[g_settings.personalize[SNeutrinoSettings::P_FEAT_KEY_TIMERLIST]].key); //CRCInput::RC_yellow
			menu_item = new CMenuDForwarder(LOCALE_TIMERLIST_NAME, true, NULL, new CTimerList, "-1", key, icon);
			menu_item->setHint(NEUTRINO_ICON_HINT_TIMERS, LOCALE_MENU_HINT_TIMERS);
			break;
		case SNeutrinoSettings::ITEM_REMOTE:
			keyhelper.get(&key,&icon,feat_key[g_settings.personalize[SNeutrinoSettings::P_FEAT_KEY_RC_LOCK]].key); //CRCInput::RC_nokey);
			menu_item = new CMenuDForwarder(LOCALE_RCLOCK_MENUEADD, true, NULL, new CRCLock, "-1" , key, icon );
			// FIXME menu_item->setHint("", NONEXISTANT_LOCALE);
			break;
		case SNeutrinoSettings::ITEM_EPG_SUPER:
			keyhelper.get(&key,&icon,CRCInput::RC_green);
			menu_item = new CMenuDForwarder(LOCALE_EPGMENU_EPGPLUS   , true, NULL, new CEPGplusHandler,  "-1", key, icon);
			// FIXME menu_item->setHint("", NONEXISTANT_LOCALE);
			break;
		case SNeutrinoSettings::ITEM_EPG_LIST:
			keyhelper.get(&key,&icon,CRCInput::RC_red);
			menu_item = new CMenuDForwarder(LOCALE_EPGMENU_EVENTLIST , true, NULL, new CEventListHandler,  "-1", key, icon);
			// FIXME menu_item->setHint("", NONEXISTANT_LOCALE);
			break;
		case SNeutrinoSettings::ITEM_EPG_INFO:
			keyhelper.get(&key,&icon,CRCInput::RC_yellow);
			menu_item = new CMenuDForwarder(LOCALE_EPGMENU_EVENTINFO , true, NULL, new CEPGDataHandler,  "-1", key, icon);
			// FIXME menu_item->setHint("", NONEXISTANT_LOCALE);
			break;
		case SNeutrinoSettings::ITEM_EPG_MISC:
		{
			int dummy = g_Sectionsd->getIsScanningActive();
			keyhelper.get(&key,&icon);
			//          new CMenuOptionChooser(LOCALE_VIDEOMENU_VIDEOMODE, &g_settings.video_Mode, VIDEOMENU_VIDEOMODE_OPTIONS, VIDEOMENU_VIDEOMODE_OPTION_COUNT, true, this, CRCInput::RC_nokey, "", true);
			menu_item = new CMenuOptionChooser(LOCALE_MAINMENU_PAUSESECTIONSD, &dummy, OPTIONS_OFF0_ON1_OPTIONS, OPTIONS_OFF0_ON1_OPTION_COUNT, true, this , key, icon );
			menu->addItem(menu_item, false);

			keyhelper.get(&key,&icon);
			menu_item = new CMenuForwarder(LOCALE_MAINMENU_CLEARSECTIONSD, true, NULL, neutrino, "clearSectionsd", key,icon);
			// FIXME menu_item->setHint("", NONEXISTANT_LOCALE);
			break;
		}
		case SNeutrinoSettings::ITEM_AUDIO_SELECT:
			keyhelper.get(&key,&icon);
			menu_item = new CMenuDForwarder(LOCALE_AUDIOSELECTMENUE_HEAD, true, NULL, new CAudioSelectMenuHandler, "-1", key,icon);
			// FIXME menu_item->setHint("", NONEXISTANT_LOCALE);
			break;
		case SNeutrinoSettings::ITEM_SUBCHANNEL:
		{
			if (g_RemoteControl->subChannels.empty())
				break;
			// NVOD/SubService- Kanal!
			CMenuWidget *tmpNVODSelector = new CMenuWidget(g_RemoteControl->are_subchannels ? LOCALE_NVODSELECTOR_SUBSERVICE : LOCALE_NVODSELECTOR_HEAD, NEUTRINO_ICON_VIDEO);
			if (!subchanselect.getNVODMenu(tmpNVODSelector)) {
				delete tmpNVODSelector;
				break;
			}
			keyhelper.get(&key,&icon);
			menu_item = new CMenuDForwarder(g_RemoteControl->are_subchannels ? LOCALE_NVODSELECTOR_SUBSERVICE : LOCALE_NVODSELECTOR_HEAD, true, NULL, tmpNVODSelector, "-1", key,icon);
			// FIXME menu_item->setHint("", NONEXISTANT_LOCALE);
			break;
		}
		case SNeutrinoSettings::ITEM_TECHINFO:
			keyhelper.get(&key,&icon,CRCInput::RC_blue);
			menu_item = new CMenuDForwarder(LOCALE_EPGMENU_STREAMINFO, !neutrino->channelList->isEmpty(), NULL, new CStreamInfo2, "-1", key, icon );
			menu_item->setHint(NEUTRINO_ICON_HINT_STREAMINFO, LOCALE_MENU_HINT_STREAMINFO);
			break;
		case SNeutrinoSettings::ITEM_GAMES:
			keyhelper.get(&key,&icon);
			menu_item = new CMenuDForwarder(LOCALE_MAINMENU_GAMES, g_PluginList->hasPlugin(CPlugins::P_TYPE_GAME), NULL, new CPluginList(LOCALE_MAINMENU_GAMES,CPlugins::P_TYPE_GAME), "-1", key, icon );
			menu_item->setHint(NEUTRINO_ICON_HINT_GAMES, LOCALE_MENU_HINT_GAMES);
			break;
                case SNeutrinoSettings::ITEM_TOOLS:
                        keyhelper.get(&key,&icon);
                        menu_item = new CMenuDForwarder(LOCALE_MAINMENU_TOOLS, g_PluginList->hasPlugin(CPlugins::P_TYPE_TOOL), NULL, new CPluginList(LOCALE_MAINMENU_TOOLS,CPlugins::P_TYPE_TOOL), "-1", key, icon );
                        break;
		case SNeutrinoSettings::ITEM_SCRIPTS:
			keyhelper.get(&key,&icon);
			menu_item = new CMenuDForwarder(LOCALE_MAINMENU_SCRIPTS, g_PluginList->hasPlugin(CPlugins::P_TYPE_SCRIPT), NULL, new CPluginList(LOCALE_MAINMENU_SCRIPTS,CPlugins::P_TYPE_SCRIPT), "-1", key, icon );
			menu_item->setHint(NEUTRINO_ICON_HINT_SCRIPTS, LOCALE_MENU_HINT_SCRIPTS);
			break;
		case SNeutrinoSettings::ITEM_LUA:
			keyhelper.get(&key,&icon);
			menu_item = new CMenuDForwarder(LOCALE_MAINMENU_LUA, g_PluginList->hasPlugin(CPlugins::P_TYPE_LUA), NULL, new CPluginList(LOCALE_MAINMENU_LUA,CPlugins::P_TYPE_LUA), "-1", key, icon );
			break;
		case SNeutrinoSettings::ITEM_PLUGIN_TYPES:
		{
			unsigned int number_of_plugins = (unsigned int) g_PluginList->getNumberOfPlugins();
			if (!number_of_plugins)
				continue;
			char id[5];
			int cnt = 0;
			for (unsigned int count = 0; count < number_of_plugins; count++)
			{
#if 0
				bool show = g_PluginList->getType(count) == CPlugins::P_TYPE_TOOL ||
					g_PluginList->getType(count) == CPlugins::P_TYPE_LUA;
#endif
				bool show = false;
				if (g_settings.personalize[SNeutrinoSettings::P_UMENU_PLUGIN_TYPE_GAMES])
					show = show || g_PluginList->getType(count) == CPlugins::P_TYPE_GAME;
				if (g_settings.personalize[SNeutrinoSettings::P_UMENU_PLUGIN_TYPE_TOOLS])
					show = show || g_PluginList->getType(count) == CPlugins::P_TYPE_TOOL;
				if (g_settings.personalize[SNeutrinoSettings::P_UMENU_PLUGIN_TYPE_SCRIPTS])
					show = show || g_PluginList->getType(count) == CPlugins::P_TYPE_SCRIPT;
				if (g_settings.personalize[SNeutrinoSettings::P_UMENU_PLUGIN_TYPE_LUA])
					show = show || g_PluginList->getType(count) == CPlugins::P_TYPE_LUA;

				if (show && !g_PluginList->isHidden(count) && (g_PluginList->getIntegration(count) == CPlugins::I_TYPE_DISABLED))
				{
					sprintf(id, "%d", count);
					menu_items++;
					neutrino_msg_t d_key = g_PluginList->getKey(count);
					//printf("[neutrino usermenu] plugin %d, set key %d...\n", count, g_PluginList->getKey(count));
					keyhelper.get(&key,&icon, d_key);
					menu_item = new CMenuForwarder(g_PluginList->getName(count), true, NULL, &plugins, id, key, icon);
					menu_item->setHint(g_PluginList->getHintIcon(count), g_PluginList->getDescription(count));

					menu->addItem(menu_item, false);
					cnt++;
				}
			}
			menu_item = NULL;
			break;
		}
		case SNeutrinoSettings::ITEM_VTXT:
			keyhelper.get(&key,&icon, feat_key[g_settings.personalize[SNeutrinoSettings::P_FEAT_KEY_VTXT]].key); //CRCInput::RC_blue
			menu_item = new CMenuForwarder(LOCALE_USERMENU_ITEM_VTXT, true, NULL, &plugins, "teletext", key, icon);
			// FIXME menu_item->setHint("", NONEXISTANT_LOCALE);
			break;
		case SNeutrinoSettings::ITEM_IMAGEINFO:
			keyhelper.get(&key,&icon);
			menu_item = new CMenuDForwarder(LOCALE_SERVICEMENU_IMAGEINFO,  true, NULL, new CImageInfo, NULL, key, icon);
			menu_item->setHint(NEUTRINO_ICON_HINT_IMAGEINFO, LOCALE_MENU_HINT_IMAGEINFO);
			break;
		case SNeutrinoSettings::ITEM_BOXINFO:
			keyhelper.get(&key,&icon);
			menu_item = new CMenuDForwarder(LOCALE_EXTRA_DBOXINFO, true, NULL, new CDBoxInfoWidget, NULL, key, icon);
			menu_item->setHint(NEUTRINO_ICON_HINT_DBOXINFO, LOCALE_MENU_HINT_DBOXINFO);
			break;
#if !HAVE_SPARK_HARDWARE
		case SNeutrinoSettings::ITEM_CAM:
			//if(cs_get_revision() == 10) continue;
			keyhelper.get(&key,&icon);
			menu_item = new CMenuForwarder(LOCALE_CI_SETTINGS, true, NULL, g_CamHandler, NULL, key, icon);
			break;
#endif
		case SNeutrinoSettings::ITEM_CLOCK:
			keyhelper.get(&key,&icon); 
			menu_item = new CMenuForwarder(!g_settings.mode_clock ? LOCALE_CLOCK_SWITCH_ON:LOCALE_CLOCK_SWITCH_OFF, true, NULL, neutrino, "clock_switch", key, icon);
			menu_item->setHint("", LOCALE_MENU_HINT_CLOCK_MODE);
			break;
#if 0
		case SNeutrinoSettings::ITEM_ADZAP:
			keyhelper.get(&key,&icon,CRCInput::RC_blue);
			menu_item = new CMenuForwarder(LOCALE_USERMENU_ITEM_ADZAP, true, NULL, neutrino, "adzap", key, icon);
			menu_item->setHint("", LOCALE_MENU_HINT_ADZAP);
			break;
		case SNeutrinoSettings::ITEM_TUNER_RESTART:
			keyhelper.get(&key,&icon);
			menu_item = new CMenuForwarder(LOCALE_SERVICEMENU_RESTART_TUNER, true, NULL, neutrino, "restarttuner", key, icon);
			menu_item->setHint(NEUTRINO_ICON_HINT_RELOAD_CHANNELS, LOCALE_MENU_HINT_RESTART_TUNER);
			break;
		case SNeutrinoSettings::ITEM_RASS:
			if (!(neutrino->getMode() == CNeutrinoApp::mode_radio && g_Radiotext && g_Radiotext->haveRASS()))
				continue;
			keyhelper.get(&key,&icon);
			menu_item = new CMenuForwarder(LOCALE_RASS_HEAD, true, NULL, neutrino, "rass", key, icon);
			menu_item->setHint(NEUTRINO_ICON_HINT_RASS, LOCALE_MENU_HINT_RASS);
			break;
		case SNeutrinoSettings::ITEM_NETZKINO:
			keyhelper.get(&key,&icon);
			menu_item = new CMenuForwarder(LOCALE_MOVIEPLAYER_NKPLAYBACK, true, NULL, neutrino, "nkplayback", key, icon);
			menu_item->setHint(NEUTRINO_ICON_HINT_NKPLAY, LOCALE_MENU_HINT_NKPLAY);
			break;
#endif
#if HAVE_SPARK_HARDWARE
		case SNeutrinoSettings::ITEM_THREE_D_MODE:
			keyhelper.get(&key,&icon);
			menu_item = new CMenuForwarder(LOCALE_THREE_D_SETTINGS, true, NULL, neutrino, "3dmode", key, icon);
			menu_item->setHint("", LOCALE_MENU_HINT_VIDEO_THREE_D);
			break;
#endif
		case SNeutrinoSettings::ITEM_YOUTUBE:
			keyhelper.get(&key,&icon);
			menu_item = new CMenuForwarder(LOCALE_MOVIEPLAYER_YTPLAYBACK, true, NULL, neutrino, "ytplayback", key, icon);
			menu_item->setHint(NEUTRINO_ICON_HINT_YTPLAY, LOCALE_MENU_HINT_YTPLAY);
			break;
		case SNeutrinoSettings::ITEM_FILEPLAY:
			keyhelper.get(&key,&icon);
			menu_item = new CMenuForwarder(LOCALE_MOVIEPLAYER_FILEPLAYBACK, true, NULL, neutrino, "fileplayback", key, icon);
			menu_item->setHint(NEUTRINO_ICON_HINT_FILEPLAY, LOCALE_MENU_HINT_FILEPLAY);
			break;
		case SNeutrinoSettings::ITEM_AUDIOPLAY:
			keyhelper.get(&key,&icon);
			menu_item = new CMenuForwarder(LOCALE_AUDIOPLAYER_NAME, true, NULL, neutrino, "audioplayer", key, icon);
			menu_item->setHint(NEUTRINO_ICON_HINT_APLAY, LOCALE_MENU_HINT_APLAY);
			break;
		case SNeutrinoSettings::ITEM_INETPLAY:
			keyhelper.get(&key,&icon);
			menu_item = new CMenuForwarder(LOCALE_INETRADIO_NAME, true, NULL, neutrino, "inetplayer", key, icon);
			menu_item->setHint(NEUTRINO_ICON_HINT_INET_RADIO, LOCALE_MENU_HINT_INET_RADIO);
			break;
		case SNeutrinoSettings::ITEM_HDDMENU:
			keyhelper.get(&key,&icon);
			menu_item = new CMenuForwarder(LOCALE_HDD_SETTINGS, true, NULL, CHDDMenuHandler::getInstance(), NULL, key, icon);
			menu_item->setHint(NEUTRINO_ICON_HINT_HDD, LOCALE_MENU_HINT_HDD);
			break;
		case SNeutrinoSettings::ITEM_NETSETTINGS:
			keyhelper.get(&key,&icon);
			menu_item = new CMenuForwarder(LOCALE_MAINSETTINGS_NETWORK, true, NULL, CNetworkSetup::getInstance(), NULL, key, icon);
			menu_item->setHint(NEUTRINO_ICON_HINT_NETWORK, LOCALE_MENU_HINT_NETWORK);
			break;
		case SNeutrinoSettings::ITEM_SWUPDATE:
			keyhelper.get(&key,&icon);
			menu_item = new CMenuDForwarder(LOCALE_SERVICEMENU_UPDATE, true, NULL, new CSoftwareUpdate(), NULL, key, icon);
			menu_item->setHint(NEUTRINO_ICON_HINT_SW_UPDATE, LOCALE_MENU_HINT_SW_UPDATE);
			break;
		case -1: // plugin
		    {
			int number_of_plugins = g_PluginList->getNumberOfPlugins();
			if (!number_of_plugins)
				continue;
			int count = 0;
			for(; count < number_of_plugins; count++) {
				const char *pname = g_PluginList->getFileName(count);
				if (pname && (std::string(pname) == *it)) {
					keyhelper.get(&key,&icon);
					menu_item = new CMenuForwarder(g_PluginList->getName(count), true, NULL, this, pname, key, icon);
					const std::string hint = g_PluginList->getDescription(count);
					if (hint != "") {
						const char *hint_icon = NULL;
						switch(g_PluginList->getType(count)) {
							case CPlugins::P_TYPE_GAME:
								hint_icon = NEUTRINO_ICON_HINT_GAMES;
							break;
							case CPlugins::P_TYPE_SCRIPT:
								hint_icon = NEUTRINO_ICON_HINT_SCRIPTS;
							break;
						}
						menu_item->setHint(hint_icon, hint);
					}
					break;
				}
			}
			if (count == number_of_plugins)
				continue;
		    }
		}
			
		itemstr_last = *it;
		if (menu_item) {
			menu_items++;
			menu->addItem(menu_item, false);
			last_menu_item = menu_item;
		}
	}

	extern CInfoClock *InfoClock;
	InfoClock->enableInfoClock(false);

	// show menu if there are more than 2 items only
	// otherwise, we start the item directly (must be the last one)
	if (menu_items > 1 )
		menu->exec(NULL, "");
	else if (last_menu_item)
		last_menu_item->exec( NULL );
	
	InfoClock->enableInfoClock(true);
	CNeutrinoApp::getInstance()->StartSubtitles();

	if (button < COL_BUTTONMAX)
		user_menu[button].selected = menu->getSelected();

	delete menu;

	return true;
}

const char *CUserMenu::getUserMenuButtonName(int button, bool &active)
{
	active = false;
	if(button < 0 || button >= (int) g_settings.usermenu.size())
		return "";

	bool return_title = false;
	neutrino_locale_t loc = NONEXISTANT_LOCALE;
	const char *text = NULL;

	std::vector<std::string> items = ::split(g_settings.usermenu[button]->items, ',');
	for (std::vector<std::string>::iterator it = items.begin(); it != items.end(); ++it) {
		int item = -1;
		if (it->find_first_not_of("0123456789") == std::string::npos)
			item = atoi(*it);

		switch(item) {
			case -1:
				if(loc != NONEXISTANT_LOCALE || text)
					return_title = true;
				else {
					int nop = g_PluginList->getNumberOfPlugins();
					for(int count = 0; count < nop; count++) {
						if (std::string(g_PluginList->getFileName(count)) == *it) {
							text = g_PluginList->getName(count);
							active = true;
							break;
						}
					}
				}
				continue;
			case SNeutrinoSettings::ITEM_NONE:
			case SNeutrinoSettings::ITEM_BAR:
				continue;
			case SNeutrinoSettings::ITEM_EPG_MISC:
				return_title = true;
				active = true;
				continue;
			case SNeutrinoSettings::ITEM_SUBCHANNEL:
				if (!g_RemoteControl->subChannels.empty()) {
					if(loc == NONEXISTANT_LOCALE && !text)
						loc = g_RemoteControl->are_subchannels ? LOCALE_NVODSELECTOR_SUBSERVICE : LOCALE_NVODSELECTOR_HEAD;
					else
						return_title = true;
					active = true;
				}
				continue;
			case SNeutrinoSettings::ITEM_PLUGIN_TYPES:
				return_title = true;
				continue;
			case SNeutrinoSettings::ITEM_CLOCK:
				if(loc == NONEXISTANT_LOCALE && !text)
					loc = g_settings.mode_clock ? LOCALE_CLOCK_SWITCH_OFF : LOCALE_CLOCK_SWITCH_ON;
				else
					return_title = true;
				active = true;
				continue;
			case SNeutrinoSettings::ITEM_AUDIO_SELECT:
				if(loc == NONEXISTANT_LOCALE && !text) {
					if (g_RemoteControl->current_PIDs.APIDs.size() > 0)
						text = g_RemoteControl->current_PIDs.APIDs[
							g_RemoteControl->current_PIDs.PIDs.selected_apid].desc;
				} else
					return_title = true;
				active = true;
				continue;
#if 0
			case SNeutrinoSettings::ITEM_RASS:
				if (!(CNeutrinoApp::getInstance()->getMode() == CNeutrinoApp::mode_radio && g_Radiotext && g_Radiotext->haveRASS()))
					continue;
#endif
			default:
				if(loc == NONEXISTANT_LOCALE && !text)
					loc = CUserMenuSetup::getLocale(item);
				else
					return_title = true;
				active = true;
				continue;
		}
		if (return_title)
			break;
	}

	if (return_title) {
		if (!g_settings.usermenu[button]->title.empty())
			return g_settings.usermenu[button]->title.c_str();
		if (button < USERMENU_ITEMS_COUNT)
			return g_Locale->getText(usermenu[button].def_name);
	} else {
		if (text)
			return text;
		if (loc != NONEXISTANT_LOCALE)
			return g_Locale->getText(loc);
	}

	return "";
}

/**************************************************************************************
*          changeNotify - features menu recording start / stop                        *
**************************************************************************************/
bool CUserMenu::changeNotify(const neutrino_locale_t OptionName, void * Data)
{
	if (ARE_LOCALES_EQUAL(OptionName, LOCALE_MAINMENU_PAUSESECTIONSD)) {
		g_Sectionsd->setPauseScanning((*((int *)Data)) == 0);
	}
	
	return false;
}

int CUserMenu::exec(CMenuTarget* /*parent*/, const std::string & actionKey)
{
	if (actionKey == "")
		return menu_return::RETURN_NONE;
	g_PluginList->startPlugin(actionKey.c_str());
	return menu_return::RETURN_EXIT;
}
