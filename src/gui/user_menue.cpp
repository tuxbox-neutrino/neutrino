/*

	user_menue setup implementation - Neutrino-GUI
	based up implementation by Günther

	Copyright (C) 2001 Steffen Hehn 'McClean'
	and some other guys
	Homepage: http://dbox.cyberphoria.org/

	Rework
	Copyright (C) 2011 T. Graf 'dbt'
	Homepage: http://www.dbox2-tuning.net/


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

#include "user_menue.h"
#include "user_menue_setup.h"
#include "subchannel_select.h"
#include "favorites.h"
#include "audio_select.h"
#include "streaminfo2.h"
#include "epgplus.h"
#include "movieplayer.h"
#include "timerlist.h"
#include "plugins.h"
#include "imageinfo.h"
#include "dboxinfo.h"
#include "cam_menu.h"
#include "pluginlist.h"
#include "infoclock.h"

#include <global.h>
#include <neutrino.h>
#include <mymenu.h>

#include <gui/widget/icons.h>

#include <driver/record.h>
#include <driver/screen_max.h>

#include <daemonc/remotecontrol.h>
extern CRemoteControl * g_RemoteControl; /* neutrino.cpp */
// extern CPlugins * g_PluginList;
extern CCAMMenuHandler * g_CamHandler;
extern CInfoClock * InfoClock;
// 
#include <system/debug.h>

CUserMenu::CUserMenu()
{
	width = 0;
}

CUserMenu::~CUserMenu()
{
	
}

// USERMENU
bool CUserMenu::showUserMenu(int button)
{
	InfoClock->enableInfoClock(false);

	// set width
	width = w_max (40, 10);
	
	if (button < 0 || button >= COL_BUTTONMAX)
		return false;

	CMenuItem* menu_item = NULL;
	CColorKeyHelper keyhelper;
	
	//set default feature key
	neutrino_msg_t key = feat_key[CPersonalizeGui::PERSONALIZE_FEAT_KEY_AUTO].key; //CRCInput::RC_nokey
	
	const char * icon = NULL;
	int dummy;

	int menu_items = 0;
	int menu_prev = -1;

	// define classes
	CFavorites* tmpFavorites                                = NULL;
	CAudioSelectMenuHandler* tmpAudioSelectMenuHandler      = NULL;
	CMenuWidget* tmpNVODSelector                            = NULL;
	CSubChannelSelectMenu subchanselect;
	CStreamInfo2 * streamInfo				= NULL;
	CEventListHandler* tmpEventListHandler                  = NULL;
	CEPGplusHandler* tmpEPGplusHandler                      = NULL;
	CEPGDataHandler* tmpEPGDataHandler                      = NULL;
	CTimerList* Timerlist					= NULL;
	CRCLock *rcLock						= NULL;
	CStreamFeaturesChangeExec *StreamFeaturesChanger	= NULL;
	CImageInfo *imageinfo					= NULL;
	CDBoxInfoWidget *boxinfo				= NULL;
	CNeutrinoApp * neutrino					= NULL;
	CPluginList * games					= NULL;
	CPluginList * tools					= NULL;
	CPluginList * scripts					= NULL;
#if ENABLE_LUA
	CPluginList * lua					= NULL;
#endif
	
	std::string txt = g_settings.usermenu_text[button];
	neutrino_locale_t caption = user_menu[button].caption;
	
	//ensure correct caption for yellow menue
	if ( button == SNeutrinoSettings::BUTTON_YELLOW) 
		caption = g_RemoteControl->are_subchannels ? LOCALE_INFOVIEWER_SUBSERVICE : LOCALE_INFOVIEWER_SELECTTIME;
	
	//ensure no empty caption
	if ( txt.empty() )
		txt = g_Locale->getText(caption);
	
	CMenuWidget *menu = new CMenuWidget(txt.c_str() , user_menu[button].menu_icon_def, width);
	if (menu == NULL)
		return 0;
	
	menu->setSelected(user_menu[button].selected);
	
	//show cancel button if configured
	if (g_settings.personalize[SNeutrinoSettings::P_UMENU_SHOW_CANCEL])
		menu->addIntroItems(NONEXISTANT_LOCALE, NONEXISTANT_LOCALE, CMenuWidget::BTN_TYPE_CANCEL);
	else
		menu->addItem(GenericMenuSeparator);
	
	// go through any postition number
	for (int pos = 0; pos < SNeutrinoSettings::ITEM_MAX ; pos++) {
		// now compare pos with the position of any item. Add this item if position is the same
		switch (g_settings.usermenu[button][pos]) {
		case SNeutrinoSettings::ITEM_NONE:
			// do nothing
			break;
		case SNeutrinoSettings::ITEM_BAR:
			if (menu_prev == -1 || menu_prev == SNeutrinoSettings::ITEM_BAR )
				break;

			menu->addItem(GenericMenuSeparatorLine);
			menu_prev = SNeutrinoSettings::ITEM_BAR;
			break;

		case SNeutrinoSettings::ITEM_FAVORITS:
		{
			menu_items++;
			menu_prev = SNeutrinoSettings::ITEM_FAVORITS;
			tmpFavorites = new CFavorites;
			keyhelper.get(&key,&icon,feat_key[g_settings.personalize[SNeutrinoSettings::P_FEAT_KEY_FAVORIT]].key); //CRCInput::RC_green
			menu_item = new CMenuForwarder(LOCALE_FAVORITES_MENUEADD, true, NULL, tmpFavorites, "-1", key, icon);
			menu->addItem(menu_item, false);
			break;
		}
		case SNeutrinoSettings::ITEM_RECORD:
		{
			if (g_settings.recording_type == RECORDING_OFF)
				break;

			menu_items++;
			menu_prev = SNeutrinoSettings::ITEM_RECORD;
			keyhelper.get(&key,&icon,CRCInput::RC_red);
			menu_item = new CMenuForwarder(LOCALE_MAINMENU_RECORDING, true, NULL, CRecordManager::getInstance(), "-1", key, icon);
			menu->addItem(menu_item, false);
			break;
		}
		case SNeutrinoSettings::ITEM_MOVIEPLAYER_MB:
			menu_items++;
			menu_prev = SNeutrinoSettings::ITEM_MOVIEPLAYER_MB;
			keyhelper.get(&key,&icon,CRCInput::RC_green);
			menu_item = new CMenuForwarder(LOCALE_MOVIEBROWSER_HEAD, true, NULL, &CMoviePlayerGui::getInstance(), "tsmoviebrowser", key, icon);
			menu->addItem(menu_item, false);
			break;

		case SNeutrinoSettings::ITEM_TIMERLIST:
			menu_items++;
			menu_prev = SNeutrinoSettings::ITEM_TIMERLIST;
			keyhelper.get(&key,&icon,feat_key[g_settings.personalize[SNeutrinoSettings::P_FEAT_KEY_TIMERLIST]].key); //CRCInput::RC_yellow
			Timerlist = new CTimerList();
			menu_item = new CMenuForwarder(LOCALE_TIMERLIST_NAME, true, NULL, Timerlist, "-1", key, icon);
			menu->addItem(menu_item, false);
			break;

		case SNeutrinoSettings::ITEM_REMOTE:
			menu_items++;
			menu_prev = SNeutrinoSettings::ITEM_REMOTE;
			keyhelper.get(&key,&icon,feat_key[g_settings.personalize[SNeutrinoSettings::P_FEAT_KEY_RC_LOCK]].key); //CRCInput::RC_nokey);
			rcLock = new CRCLock();
			menu_item = new CMenuForwarder(LOCALE_RCLOCK_MENUEADD, true, NULL, rcLock, "-1" , key, icon );
			menu->addItem(menu_item, false);
			break;

		case SNeutrinoSettings::ITEM_EPG_SUPER:
			menu_items++;
			menu_prev = SNeutrinoSettings::ITEM_EPG_SUPER;
			tmpEPGplusHandler = new CEPGplusHandler();
			keyhelper.get(&key,&icon,CRCInput::RC_green);
			menu_item = new CMenuForwarder(LOCALE_EPGMENU_EPGPLUS   , true, NULL, tmpEPGplusHandler  ,  "-1", key, icon);
			menu->addItem(menu_item, false);
			break;

		case SNeutrinoSettings::ITEM_EPG_LIST:
			menu_items++;
			menu_prev = SNeutrinoSettings::ITEM_EPG_LIST;
			tmpEventListHandler = new CEventListHandler();
			keyhelper.get(&key,&icon,CRCInput::RC_red);
			menu_item = new CMenuForwarder(LOCALE_EPGMENU_EVENTLIST , true, NULL, tmpEventListHandler,  "-1", key, icon);
			menu->addItem(menu_item, false);
			break;

		case SNeutrinoSettings::ITEM_EPG_INFO:
			menu_items++;
			menu_prev = SNeutrinoSettings::ITEM_EPG_INFO;
			tmpEPGDataHandler = new CEPGDataHandler();
			keyhelper.get(&key,&icon,CRCInput::RC_yellow);
			menu_item = new CMenuForwarder(LOCALE_EPGMENU_EVENTINFO , true, NULL, tmpEPGDataHandler ,  "-1", key, icon);
			menu->addItem(menu_item, false);
			break;

		case SNeutrinoSettings::ITEM_EPG_MISC:
			menu_items++;
			menu_prev = SNeutrinoSettings::ITEM_EPG_MISC;
			dummy = g_Sectionsd->getIsScanningActive();
			//dummy = sectionsd_scanning;
			keyhelper.get(&key,&icon);
			menu_item = new CMenuOptionChooser(LOCALE_MAINMENU_PAUSESECTIONSD, &dummy, OPTIONS_OFF0_ON1_OPTIONS, OPTIONS_OFF0_ON1_OPTION_COUNT, true, this , key, icon );
			menu->addItem(menu_item, false);
			menu_items++;
			keyhelper.get(&key,&icon);
			menu_item = new CMenuForwarder(LOCALE_MAINMENU_CLEARSECTIONSD, true, NULL, CNeutrinoApp::getInstance(), "clearSectionsd", key,icon);
			menu->addItem(menu_item, false);
			break;

		case SNeutrinoSettings::ITEM_AUDIO_SELECT:
			//g_settings.audio_left_right_selectable || g_RemoteControl->current_PIDs.APIDs.size() > 1)
			if (1) {
				menu_items++;
				menu_prev = SNeutrinoSettings::ITEM_AUDIO_SELECT;
				tmpAudioSelectMenuHandler = new CAudioSelectMenuHandler;
				keyhelper.get(&key,&icon);
				menu_item = new CMenuForwarder(LOCALE_AUDIOSELECTMENUE_HEAD, true, NULL, tmpAudioSelectMenuHandler, "-1", key,icon);
				menu->addItem(menu_item, false);
			}
			break;

		case SNeutrinoSettings::ITEM_SUBCHANNEL:
			if (!(g_RemoteControl->subChannels.empty())) {
				// NVOD/SubService- Kanal!
				tmpNVODSelector = new CMenuWidget(g_RemoteControl->are_subchannels ? LOCALE_NVODSELECTOR_SUBSERVICE : LOCALE_NVODSELECTOR_HEAD, NEUTRINO_ICON_VIDEO);
				if (subchanselect.getNVODMenu(tmpNVODSelector)) {
					menu_items++;
					menu_prev = SNeutrinoSettings::ITEM_SUBCHANNEL;
					keyhelper.get(&key,&icon);
					menu_item = new CMenuForwarder(g_RemoteControl->are_subchannels ? LOCALE_NVODSELECTOR_SUBSERVICE : LOCALE_NVODSELECTOR_HEAD, true, NULL, tmpNVODSelector, "-1", key,icon);
					menu->addItem(menu_item, false);
				}
			}
			break;

		case SNeutrinoSettings::ITEM_TECHINFO:
			menu_items++;
			menu_prev = SNeutrinoSettings::ITEM_TECHINFO;
			streamInfo = new CStreamInfo2();
			keyhelper.get(&key,&icon,CRCInput::RC_blue);
			menu_item = new CMenuForwarder(LOCALE_EPGMENU_STREAMINFO, !CNeutrinoApp::getInstance()->channelList->isEmpty(), NULL, streamInfo, "-1", key, icon );
			menu->addItem(menu_item, false);
			break;
		case SNeutrinoSettings::ITEM_GAMES:
			menu_items++;
			menu_prev = SNeutrinoSettings::ITEM_GAMES;
			games = new CPluginList(LOCALE_MAINMENU_GAMES,CPlugins::P_TYPE_GAME);
			keyhelper.get(&key,&icon);
			menu_item = new CMenuForwarder(LOCALE_MAINMENU_GAMES, g_PluginList->hasPlugin(CPlugins::P_TYPE_GAME), NULL, games, "-1", key, icon );
			menu->addItem(menu_item, false);
			break;
		case SNeutrinoSettings::ITEM_TOOLS:
			menu_items++;
			menu_prev = SNeutrinoSettings::ITEM_TOOLS;
			tools = new CPluginList(LOCALE_MAINMENU_TOOLS,CPlugins::P_TYPE_TOOL);
			keyhelper.get(&key,&icon);
			menu_item = new CMenuForwarder(LOCALE_MAINMENU_TOOLS, g_PluginList->hasPlugin(CPlugins::P_TYPE_TOOL), NULL, tools, "-1", key, icon );
			menu->addItem(menu_item, false);
			break;
		case SNeutrinoSettings::ITEM_SCRIPTS:
			menu_items++;
			menu_prev = SNeutrinoSettings::ITEM_SCRIPTS;
			scripts = new CPluginList(LOCALE_MAINMENU_SCRIPTS,CPlugins::P_TYPE_SCRIPT);
			keyhelper.get(&key,&icon);
			menu_item = new CMenuForwarder(LOCALE_MAINMENU_SCRIPTS, g_PluginList->hasPlugin(CPlugins::P_TYPE_SCRIPT), NULL, scripts, "-1", key, icon );
			menu->addItem(menu_item, false);
			break;
#if ENABLE_LUA
		case SNeutrinoSettings::ITEM_LUA:
			menu_items++;
			menu_prev = SNeutrinoSettings::ITEM_LUA;
			lua = new CPluginList(LOCALE_MAINMENU_LUA,CPlugins::P_TYPE_LUA);
			keyhelper.get(&key,&icon);
			menu_item = new CMenuForwarder(LOCALE_MAINMENU_LUA, g_PluginList->hasPlugin(CPlugins::P_TYPE_LUA), NULL, lua, "-1", key, icon );
			menu->addItem(menu_item, false);
			break;
#endif
		case SNeutrinoSettings::ITEM_PLUGIN_TYPES:
		{
			char id[5];
			int cnt = 0;
			for (unsigned int count = 0; count < (unsigned int) g_PluginList->getNumberOfPlugins(); count++)
			{
				bool show = false;
				if (g_settings.personalize[SNeutrinoSettings::P_UMENU_PLUGIN_TYPE_GAMES])
					show = show || g_PluginList->getType(count) == CPlugins::P_TYPE_GAME;
				if (g_settings.personalize[SNeutrinoSettings::P_UMENU_PLUGIN_TYPE_TOOLS])
					show = show || g_PluginList->getType(count) == CPlugins::P_TYPE_TOOL;
				if (g_settings.personalize[SNeutrinoSettings::P_UMENU_PLUGIN_TYPE_SCRIPTS])
					show = show || g_PluginList->getType(count) == CPlugins::P_TYPE_SCRIPT;
#if ENABLE_LUA
				if (g_settings.personalize[SNeutrinoSettings::P_UMENU_PLUGIN_TYPE_LUA])
					show = show || g_PluginList->getType(count) == CPlugins::P_TYPE_LUA;
#endif

				if (show && !g_PluginList->isHidden(count))
				{
					sprintf(id, "%d", count);
					menu_items++;
					menu_prev = SNeutrinoSettings::ITEM_PLUGIN_TYPES;
					neutrino_msg_t d_key = g_PluginList->getKey(count);
					//printf("[neutrino usermenu] plugin %d, set key %d...\n", count, g_PluginList->getKey(count));
					StreamFeaturesChanger     = new CStreamFeaturesChangeExec();
					keyhelper.get(&key,&icon, d_key);
					menu_item = new CMenuForwarder(g_PluginList->getName(count), true, NULL, StreamFeaturesChanger, id, key, icon);
					menu->addItem(menu_item, 0);
					cnt++;
				}
			}
		}
		break;

		case SNeutrinoSettings::ITEM_VTXT:
			menu_items++;
			menu_prev = SNeutrinoSettings::ITEM_VTXT;
			keyhelper.get(&key,&icon, feat_key[g_settings.personalize[SNeutrinoSettings::P_FEAT_KEY_VTXT]].key); //CRCInput::RC_blue
			StreamFeaturesChanger     = new CStreamFeaturesChangeExec();
			menu_item = new CMenuForwarder(LOCALE_USERMENU_ITEM_VTXT, true, NULL, StreamFeaturesChanger, "teletext", key, icon);
			menu->addItem(menu_item, 0);
			break;
		case SNeutrinoSettings::ITEM_IMAGEINFO:
			menu_items++;
			menu_prev = SNeutrinoSettings::ITEM_IMAGEINFO;
			imageinfo = new CImageInfo();
			keyhelper.get(&key,&icon);
			menu->addItem(new CMenuForwarder(LOCALE_SERVICEMENU_IMAGEINFO,  true, NULL, imageinfo, NULL, key, icon ), false);
			break;
		case SNeutrinoSettings::ITEM_BOXINFO:
			menu_items++;
			menu_prev = SNeutrinoSettings::ITEM_BOXINFO;
			boxinfo = new CDBoxInfoWidget();
			keyhelper.get(&key,&icon);
			menu->addItem( new CMenuForwarder(LOCALE_EXTRA_DBOXINFO, true, NULL, boxinfo, NULL, key, icon));
			break;
		case SNeutrinoSettings::ITEM_CAM:
			//if(cs_get_revision() != 10)
			{
				menu_items++;
				menu_prev = SNeutrinoSettings::ITEM_CAM;
				keyhelper.get(&key,&icon);
				menu->addItem(new CMenuForwarder(LOCALE_CI_SETTINGS, true, NULL, g_CamHandler, NULL, key, icon));
			}
			break;
		case SNeutrinoSettings::ITEM_CLOCK:
			{
				menu_items++;
				menu_prev = SNeutrinoSettings::ITEM_CLOCK;
				keyhelper.get(&key,&icon); 
				neutrino = CNeutrinoApp::getInstance();
				menu_item = new CMenuForwarder(!g_settings.mode_clock ? LOCALE_CLOCK_SWITCH_ON:LOCALE_CLOCK_SWITCH_OFF, true, NULL, neutrino, "clock_switch", key, icon);
				menu->addItem(menu_item, false);
			}
			break;
		case SNeutrinoSettings::ITEM_YOUTUBE:
			{
				menu_items++;
				menu_prev = SNeutrinoSettings::ITEM_YOUTUBE;
				keyhelper.get(&key,&icon);
				menu_item = new CMenuForwarder(LOCALE_MOVIEPLAYER_YTPLAYBACK, true, NULL, &CMoviePlayerGui::getInstance(), "ytplayback", key, icon);
				menu_item->setHint(NEUTRINO_ICON_HINT_YTPLAY, LOCALE_MENU_HINT_YTPLAY);
				menu->addItem(menu_item, 0);
			}
			break;
		case SNeutrinoSettings::ITEM_FILEPLAY:
			{
				menu_items++;
				menu_prev = SNeutrinoSettings::ITEM_FILEPLAY;
				keyhelper.get(&key,&icon);
				menu_item = new CMenuForwarder(LOCALE_MOVIEPLAYER_FILEPLAYBACK, true, NULL, &CMoviePlayerGui::getInstance(), "fileplayback", key, icon);
				menu_item->setHint(NEUTRINO_ICON_HINT_FILEPLAY, LOCALE_MENU_HINT_FILEPLAY);
				menu->addItem(menu_item, 0);
			}
			break;
		default:
			printf("[neutrino] WARNING! menu wrong item!!\n");
			break;
		}
	}

#if 0
	// Allow some tailoring for privat image bakers ;)
	if (button == SNeutrinoSettings::BUTTON_RED) {
	}
	else if ( button == SNeutrinoSettings::BUTTON_GREEN) {
	}
	else if ( button == SNeutrinoSettings::BUTTON_YELLOW) {
	}
	else if ( button == SNeutrinoSettings::BUTTON_BLUE) {
	}
#endif

	// show menu if there are more than 2 items only
	// otherwise, we start the item directly (must be the last one)
	if (menu_items > 1 ) {
		menu->exec(NULL,"");
	}
	else if (menu_item != NULL)
		menu_item->exec( NULL );
	
	user_menu[button].selected = menu->getSelected();

	// clear the heap
	if (tmpFavorites)                delete tmpFavorites;
	if (tmpAudioSelectMenuHandler)   delete tmpAudioSelectMenuHandler;
	if (tmpNVODSelector)             delete tmpNVODSelector;
	if (streamInfo)                  delete streamInfo;
	if (tmpEventListHandler)         delete tmpEventListHandler;
	if (tmpEPGplusHandler)           delete tmpEPGplusHandler;
	if (tmpEPGDataHandler)           delete tmpEPGDataHandler;
	if (Timerlist)			 delete Timerlist;
	if (rcLock)			 delete rcLock;
	if (StreamFeaturesChanger)	 delete StreamFeaturesChanger;
	if (imageinfo)			 delete imageinfo;
	if (boxinfo)			 delete boxinfo;
	if (games)                       delete games;
	if (scripts)                     delete scripts;
	if (menu)                        delete menu;

	InfoClock->enableInfoClock(true);

 	return 0;
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
