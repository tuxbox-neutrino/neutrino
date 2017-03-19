/*
	Neutrino-GUI  -   DBoxII-Project

	Copyright (C) 2001 Steffen Hehn 'McClean'
							 and some other guys
	Homepage: http://dbox.cyberphoria.org/

	Kommentar:

	Diese GUI wurde von Grund auf neu programmiert und sollte nun vom
	Aufbau und auch den Ausbaumoeglichkeiten gut aussehen. Neutrino basiert
	auf der Client-Server Idee, diese GUI ist also von der direkten DBox-
	Steuerung getrennt. Diese wird dann von Daemons uebernommen.


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
	Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA
*/

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "global.h"
#include "neutrino.h"
#include "mymenu.h"

#include <system/debug.h>

#include <cs_api.h>

#include "gui/audio_setup.h"
#include "gui/audio_select.h"
#include "gui/bedit/bouqueteditor_bouquets.h"
#include "gui/bouquetlist.h"
#include "gui/cam_menu.h"
#include "gui/dboxinfo.h"
#include "gui/epgplus.h"
#include "gui/favorites.h"
#include "gui/hdd_menu.h"
#include "gui/imageinfo.h"
#include "gui/info_menue.h"
#include "gui/keybind_setup.h"
#include "gui/mediaplayer.h"
#include "gui/mediaplayer_setup.h"
#include "gui/miscsettings_menu.h"
#include "gui/motorcontrol.h"
#include "gui/movieplayer.h"
#include "gui/network_setup.h"
#include "gui/osd_setup.h"
#include "gui/osdlang_setup.h"
#include "gui/parentallock_setup.h"
#include "gui/pluginlist.h"
#include "gui/plugins.h"
#include "gui/record_setup.h"
#include "gui/scan_setup.h"
#include "gui/settings_manager.h"
#include "gui/sleeptimer.h"
#include "gui/timerlist.h"
#include "gui/update_menue.h"
#include "gui/streaminfo2.h"
#ifdef ENABLE_TESTING
#include "gui/test_menu.h"
#endif
#include "gui/update.h"
#include "gui/vfd_setup.h"
#include "gui/videosettings.h"
#include "driver/record.h"
#include "driver/display.h"


extern CPlugins       * g_Plugins;
extern CRemoteControl * g_RemoteControl;
extern CCAMMenuHandler * g_CamHandler;
// extern bool has_hdd;
// extern char current_timezone[50];
// extern bool autoshift;

#define MENU_WIDTH 35

const mn_widget_struct_t menu_widgets[MENU_MAX] =
{
	{LOCALE_MAINMENU_HEAD, 		NEUTRINO_ICON_MAINMENU, 	MENU_WIDTH},	/* 0 = MENU_MAIN*/
	{LOCALE_MAINSETTINGS_HEAD, 	NEUTRINO_ICON_SETTINGS, 	MENU_WIDTH},	/* 1 = MENU_SETTINGS*/
	{LOCALE_SERVICEMENU_HEAD,	NEUTRINO_ICON_SETTINGS, 	MENU_WIDTH}, 	/* 2 = MENU_SERVICE*/
	{LOCALE_MAINMENU_SHUTDOWN_MENU,	NEUTRINO_ICON_BUTTON_POWER, 	MENU_WIDTH}, 	/* 3 = MENU_SHUTDOWN*/
};

//init all menues
void CNeutrinoApp::InitMenu()
{
	printf("[neutrino] init menus...\n");

	//personalize: neutrino.h, neutrino.cpp
	personalize.enableUsermenu();
	personalize.enablePluginMenu();
	personalize.enablePinSetup();
	personalize.addWidgets(menu_widgets, g_settings.easymenu ? MENU_MAX : 3);

	InitMenuMain();
 	InitMenuSettings();
 	InitMenuService();
	//add submenu for media
	CMediaPlayerMenu::getInstance()->initMenuMedia(new CMenuWidget(LOCALE_MAINMENU_MEDIA, NEUTRINO_ICON_MULTIMEDIA, MENU_WIDTH), &personalize);

	personalize.addPersonalizedItems();

	//add I_TYPE_SETTING plugins
	unsigned int nextShortcut;
	CMenuWidget &menuSettings = personalize.getWidget(MENU_SETTINGS);
	nextShortcut = (unsigned int)menuSettings.getNextShortcut();
	menuSettings.integratePlugins(CPlugins::I_TYPE_SETTING, nextShortcut);

	//add I_TYPE_SERVICE plugins
	CMenuWidget &menuService = personalize.getWidget(MENU_SERVICE);
	nextShortcut = (unsigned int)menuService.getNextShortcut();
	menuService.integratePlugins(CPlugins::I_TYPE_SERVICE, nextShortcut);
}

//init main menu
void CNeutrinoApp::InitMenuMain()
{
	dprintf(DEBUG_DEBUG, "init mainmenue\n");

	// Dynamic renumbering
	personalize.setShortcut();

	CMenuWidget &menu = personalize.getWidget(MENU_MAIN);
	menu.addKey(CRCInput::RC_stop, this, "easyswitch");

	//top
	personalize.addItem(MENU_MAIN, GenericMenuSeparator, NULL, false, CPersonalizeGui::PERSONALIZE_SHOW_NO);

	//1st section***************************************************************************************************

	if (g_settings.easymenu) {
		if (g_settings.recording_type != CNeutrinoApp::RECORDING_OFF) {
			CMenuForwarder *mb = new CMenuForwarder(LOCALE_MOVIEBROWSER_HEAD, true, NULL, CMediaPlayerMenu::getInstance(), "movieplayer", CRCInput::RC_red);
			mb->setHint(NEUTRINO_ICON_HINT_MB, LOCALE_MENU_HINT_MB);
			personalize.addItem(MENU_MAIN, mb, &g_settings.personalize[SNeutrinoSettings::P_MPLAYER_MBROWSER], false, CPersonalizeGui::PERSONALIZE_SHOW_AS_ITEM_OPTION, NULL, DCOND_MODE_TS);
		}
#if 0
		CMenuForwarder *cl = new CMenuForwarder(LOCALE_MAINMENU_CHANNELS, true, NULL, this, "channels", CRCInput::RC_green);
		cl->setHint(NEUTRINO_ICON_HINT_TVMODE, LOCALE_MENU_HINT_CHANNELS);
		personalize.addItem(MENU_MAIN, cl);
#endif
		CMenuForwarder * mf = new CMenuForwarder(LOCALE_BOUQUETEDITOR_NAME    , true, NULL, new CBEBouquetWidget(), NULL, CRCInput::RC_green);
		mf->setHint(NEUTRINO_ICON_HINT_BEDIT, LOCALE_MENU_HINT_BEDIT);
		personalize.addItem(MENU_MAIN, mf, &g_settings.personalize[SNeutrinoSettings::P_MSER_BOUQUET_EDIT], false, CPersonalizeGui::PERSONALIZE_SHOW_AS_ACCESS_OPTION);
	} else {
		//tv <-> radio toggle
		CMenuForwarder *tvradio_switch = new CMenuForwarder(LOCALE_MAINMENU_TVRADIO_SWITCH, true, NULL, this, "tv_radio_switch", CRCInput::RC_red);
		tvradio_switch->setHint(NEUTRINO_ICON_HINT_TVRADIO_SWITCH, LOCALE_MENU_HINT_TVRADIO_SWITCH);
		personalize.addItem(MENU_MAIN, tvradio_switch, &g_settings.personalize[SNeutrinoSettings::P_MAIN_TV_RADIO_MODE], false, CPersonalizeGui::PERSONALIZE_SHOW_AS_ITEM_OPTION, NULL, DCOND_MODE_TS);

		//tv-mode
		CMenuForwarder *tvswitch = new CMenuForwarder(LOCALE_MAINMENU_TVMODE, true, NULL, this, "tv", CRCInput::RC_red);
		tvswitch->setHint(NEUTRINO_ICON_HINT_TVMODE, LOCALE_MENU_HINT_TVMODE);
		personalize.addItem(MENU_MAIN, tvswitch, &g_settings.personalize[SNeutrinoSettings::P_MAIN_TV_MODE], false, CPersonalizeGui::PERSONALIZE_SHOW_AS_ITEM_OPTION, tvradio_switch, DCOND_MODE_TV | DCOND_MODE_TS); //observed

		//radio-mode
		CMenuForwarder *radioswitch = new CMenuForwarder(LOCALE_MAINMENU_RADIOMODE, true, NULL, this, "radio", CRCInput::RC_green);
		radioswitch->setHint(NEUTRINO_ICON_HINT_RADIOMODE, LOCALE_MENU_HINT_RADIOMODE);
		personalize.addItem(MENU_MAIN, radioswitch, &g_settings.personalize[SNeutrinoSettings::P_MAIN_RADIO_MODE], false, CPersonalizeGui::PERSONALIZE_SHOW_AS_ITEM_OPTION, tvradio_switch, DCOND_MODE_RADIO | DCOND_MODE_TS); //observed
	}

	//timer
	CMenuForwarder *timerlist = new CMenuForwarder(LOCALE_TIMERLIST_NAME, true, NULL, new CTimerList(), NULL, CRCInput::RC_yellow);
	timerlist->setHint(NEUTRINO_ICON_HINT_TIMERS, LOCALE_MENU_HINT_TIMERS);
	personalize.addItem(MENU_MAIN, timerlist, &g_settings.personalize[SNeutrinoSettings::P_MAIN_TIMER]);

	//multimedia menu
	CMenuForwarder *media = new CMenuForwarder(LOCALE_MAINMENU_MEDIA, true, NULL, CMediaPlayerMenu::getInstance(), NULL, CRCInput::RC_blue);
	media->setHint(NEUTRINO_ICON_HINT_MEDIA, LOCALE_MENU_HINT_MEDIA);
	if (g_settings.easymenu)
		personalize.addItem(MENU_MAIN, media, &g_settings.personalize[SNeutrinoSettings::P_MAIN_MEDIA], false, CPersonalizeGui::PERSONALIZE_SHOW_AS_ACCESS_OPTION);
	else
		personalize.addItem(MENU_MAIN, media, &g_settings.personalize[SNeutrinoSettings::P_MAIN_MEDIA]);

	CMenuForwarder * mf;
	if (!g_settings.easymenu) {
		//games
		bool show_games = g_Plugins->hasPlugin(CPlugins::P_TYPE_GAME);
		mf = new CMenuForwarder(LOCALE_MAINMENU_GAMES, show_games, NULL, new CPluginList(LOCALE_MAINMENU_GAMES,CPlugins::P_TYPE_GAME));
		mf->setHint(NEUTRINO_ICON_HINT_GAMES, LOCALE_MENU_HINT_GAMES);
		personalize.addItem(MENU_MAIN, mf, &g_settings.personalize[SNeutrinoSettings::P_MAIN_GAMES]);

		//tools
		bool show_tools = g_Plugins->hasPlugin(CPlugins::P_TYPE_TOOL);
		mf = new CMenuForwarder(LOCALE_MAINMENU_TOOLS, show_tools, NULL, new CPluginList(LOCALE_MAINMENU_TOOLS,CPlugins::P_TYPE_TOOL));
		mf->setHint(NEUTRINO_ICON_HINT_SCRIPTS, LOCALE_MENU_HINT_TOOLS);
		personalize.addItem(MENU_MAIN, mf, &g_settings.personalize[SNeutrinoSettings::P_MAIN_TOOLS]);

		//scripts
		bool show_scripts = g_Plugins->hasPlugin(CPlugins::P_TYPE_SCRIPT);
		mf = new CMenuForwarder(LOCALE_MAINMENU_SCRIPTS, show_scripts, NULL, new CPluginList(LOCALE_MAINMENU_SCRIPTS,CPlugins::P_TYPE_SCRIPT));
		mf->setHint(NEUTRINO_ICON_HINT_SCRIPTS, LOCALE_MENU_HINT_SCRIPTS);
		personalize.addItem(MENU_MAIN, mf, &g_settings.personalize[SNeutrinoSettings::P_MAIN_SCRIPTS]);

		//lua
		bool show_lua = g_Plugins->hasPlugin(CPlugins::P_TYPE_LUA);
		mf = new CMenuForwarder(LOCALE_MAINMENU_LUA, show_lua, NULL, new CPluginList(LOCALE_MAINMENU_LUA,CPlugins::P_TYPE_LUA));
		mf->setHint(NEUTRINO_ICON_HINT_SCRIPTS, LOCALE_MENU_HINT_LUA);
		personalize.addItem(MENU_MAIN, mf, &g_settings.personalize[SNeutrinoSettings::P_MAIN_LUA]);
	}

	//separator
	personalize.addSeparator(MENU_MAIN);

	//2nd section***************************************************************************************************

	// settings, also as pin protected option in personalize menu, as a result of parameter value CPersonalizeGui::PERSONALIZE_SHOW_AS_ACCESS_OPTION
	mf = new CMenuForwarder(LOCALE_MAINMENU_SETTINGS, true, NULL, &personalize.getWidget(MENU_SETTINGS));
	mf->setHint(NEUTRINO_ICON_HINT_SETTINGS, LOCALE_MENU_HINT_SETTINGS);
	personalize.addItem(MENU_MAIN, mf, &g_settings.personalize[SNeutrinoSettings::P_MAIN_SETTINGS], false, CPersonalizeGui::PERSONALIZE_SHOW_AS_ACCESS_OPTION);

	// service, also as pin protected option in personalize menu, as a result of parameter value CPersonalizeGui::PERSONALIZE_SHOW_AS_ACCESS_OPTION
	mf = new CMenuForwarder(LOCALE_MAINMENU_SERVICE, true, NULL, &personalize.getWidget(MENU_SERVICE));
	mf->setHint(NEUTRINO_ICON_HINT_SERVICE, LOCALE_MENU_HINT_SERVICE);
	personalize.addItem(MENU_MAIN, mf, &g_settings.personalize[SNeutrinoSettings::P_MAIN_SERVICE], false, CPersonalizeGui::PERSONALIZE_SHOW_AS_ACCESS_OPTION);

	//separator
	personalize.addSeparator(MENU_MAIN);

	//3rd section***************************************************************************************************

	if (!g_settings.easymenu) {
		//10. -- only 10 shortcuts (1-9, 0), the next could be the last also!(10. => 0)
		//sleeptimer
		mf = new CMenuForwarder(LOCALE_MAINMENU_SLEEPTIMER, true, NULL, new CSleepTimerWidget);
		mf->setHint(NEUTRINO_ICON_HINT_SLEEPTIMER, LOCALE_MENU_HINT_SLEEPTIMER);
		personalize.addItem(MENU_MAIN, mf, &g_settings.personalize[SNeutrinoSettings::P_MAIN_SLEEPTIMER]);

		//reboot
		mf = new CMenuForwarder(LOCALE_MAINMENU_REBOOT, true, NULL, this, "reboot");
		mf->setHint(NEUTRINO_ICON_HINT_REBOOT, LOCALE_MENU_HINT_REBOOT);
		personalize.addItem(MENU_MAIN, mf, &g_settings.personalize[SNeutrinoSettings::P_MAIN_REBOOT]);

		//shutdown
		if (g_info.hw_caps->can_shutdown) {
			mf = new CMenuForwarder(LOCALE_MAINMENU_SHUTDOWN, true, NULL, this, "shutdown", CRCInput::RC_standby);
			mf->setHint(NEUTRINO_ICON_HINT_SHUTDOWN, LOCALE_MENU_HINT_SHUTDOWN);
			personalize.addItem(MENU_MAIN, mf, &g_settings.personalize[SNeutrinoSettings::P_MAIN_SHUTDOWN]);
		}
	}

	//separator
	personalize.addSeparator(MENU_MAIN);

	//4th section***************************************************************************************************

	//infomenu
	mf = new CMenuForwarder(LOCALE_MESSAGEBOX_INFO, true, NULL, new CInfoMenu(), NULL, CRCInput::RC_info);
	mf->setHint(NEUTRINO_ICON_HINT_INFO, LOCALE_MENU_HINT_INFO);
	personalize.addItem(MENU_MAIN, mf, &g_settings.personalize[SNeutrinoSettings::P_MAIN_INFOMENU]);

	//cisettings
	if (!g_settings.easymenu && (cCA::GetInstance()->GetNumberCISlots() > 0 || cCA::GetInstance()->GetNumberSmartCardSlots() > 0)) {
		mf = new CMenuForwarder(LOCALE_CI_SETTINGS, true, NULL, g_CamHandler);
		mf->setHint(NEUTRINO_ICON_HINT_CI, LOCALE_MENU_HINT_CI);
		personalize.addItem(MENU_MAIN, mf, &g_settings.personalize[SNeutrinoSettings::P_MAIN_CISETTINGS]);
	}
	if (g_settings.easymenu) {
		personalize.addIntroItems(MENU_SHUTDOWN);

  		mf = new CMenuForwarder(LOCALE_MAINMENU_SHUTDOWN_MENU, true, NULL, &personalize.getWidget(MENU_SHUTDOWN), NULL, CRCInput::RC_standby);
		mf->setHint(NEUTRINO_ICON_HINT_SHUTDOWN, LOCALE_MENU_HINT_SHUTDOWN_MENU);
		personalize.addItem(MENU_MAIN, mf);

		// standby
		mf = new CMenuForwarder(LOCALE_MAINMENU_STANDBY, true, NULL, this, "standby", CRCInput::RC_red);
		mf->setHint(NEUTRINO_ICON_HINT_SLEEPTIMER, LOCALE_MENU_HINT_STANDBY);
		personalize.addItem(MENU_SHUTDOWN, mf, &g_settings.personalize[SNeutrinoSettings::P_MAIN_STANDBY]);

		//sleeptimer
		mf = new CMenuForwarder(LOCALE_MAINMENU_SLEEPTIMER, true, NULL, new CSleepTimerWidget, NULL, CRCInput::RC_green);
		mf->setHint(NEUTRINO_ICON_HINT_SLEEPTIMER, LOCALE_MENU_HINT_SLEEPTIMER);
		personalize.addItem(MENU_SHUTDOWN, mf, &g_settings.personalize[SNeutrinoSettings::P_MAIN_SLEEPTIMER]);

		//reboot
		mf = new CMenuForwarder(LOCALE_MAINMENU_REBOOT, true, NULL, this, "reboot", CRCInput::RC_yellow);
		mf->setHint(NEUTRINO_ICON_HINT_REBOOT, LOCALE_MENU_HINT_REBOOT);
		personalize.addItem(MENU_SHUTDOWN, mf, &g_settings.personalize[SNeutrinoSettings::P_MAIN_REBOOT]);

		//shutdown
		if (g_info.hw_caps->can_shutdown) {
			mf = new CMenuForwarder(LOCALE_MAINMENU_SHUTDOWN, true, NULL, this, "shutdown", CRCInput::RC_blue);
			mf->setHint(NEUTRINO_ICON_HINT_SHUTDOWN, LOCALE_MENU_HINT_SHUTDOWN);
			personalize.addItem(MENU_SHUTDOWN, mf, &g_settings.personalize[SNeutrinoSettings::P_MAIN_SHUTDOWN]);
		}
	}

#ifdef ENABLE_TESTING
	personalize.addItem(MENU_MAIN, new CMenuForwarder("Test menu", true, NULL, new CTestMenu()), NULL, false, CPersonalizeGui::PERSONALIZE_SHOW_NO);
#endif
}

//settings menue
void CNeutrinoApp::InitMenuSettings()
{
	dprintf(DEBUG_DEBUG, "init settings menue...\n");

	//CMenuWidget &menu = personalize.getWidget(MENU_SETTINGS);

	// Dynamic renumbering
	personalize.setShortcut();

	// back button, no personalized
	personalize.addIntroItems(MENU_SETTINGS);

	static int show = CPersonalizeGui::PERSONALIZE_MODE_VISIBLE;
	//***************************************************************************************************
	// save
	CMenuForwarder * mf;
	if (!g_settings.easymenu) {
		mf = new CMenuForwarder(LOCALE_MAINSETTINGS_SAVESETTINGSNOW, true, NULL, this, "savesettings", CRCInput::RC_red);
		mf->setHint(NEUTRINO_ICON_HINT_SAVE_SETTINGS, LOCALE_MENU_HINT_SAVE_SETTINGS);
		personalize.addItem(MENU_SETTINGS, mf, &show, false, CPersonalizeGui::PERSONALIZE_SHOW_NO);
	}

	// settings manager
	if (g_settings.easymenu) {
		mf = new CMenuForwarder(LOCALE_MAINSETTINGS_MANAGE, true, NULL, new CSettingsManager(), NULL, CRCInput::RC_red);
		personalize.addItem(MENU_SETTINGS, mf, &g_settings.personalize[SNeutrinoSettings::P_MSET_SETTINGS_MANAGER]);
	} else {
		mf = new CMenuForwarder(LOCALE_MAINSETTINGS_MANAGE, true, NULL, new CSettingsManager(), NULL, CRCInput::RC_green);
		personalize.addItem(MENU_SETTINGS, mf, &g_settings.personalize[SNeutrinoSettings::P_MSET_SETTINGS_MANAGER], false, CPersonalizeGui::PERSONALIZE_SHOW_AS_ACCESS_OPTION);
	}
	mf->setHint(NEUTRINO_ICON_HINT_MANAGE_SETTINGS, LOCALE_MENU_HINT_MANAGE_SETTINGS);

	// personalize
	if (!g_settings.easymenu) {
		mf = new CMenuForwarder(LOCALE_PERSONALIZE_HEAD, true, NULL, &personalize, NULL, CRCInput::RC_yellow, NULL, NEUTRINO_ICON_LOCK);
		mf->setHint(NEUTRINO_ICON_HINT_PERSONALIZE, LOCALE_MENU_HINT_PERSONALIZE);
		personalize.addItem(MENU_SETTINGS, mf, &show, false, CPersonalizeGui::PERSONALIZE_SHOW_NO);
	}

	// miscSettings
	if (!g_settings.easymenu) {
		mf = new CMenuForwarder(LOCALE_MAINSETTINGS_MISC, true, NULL, new CMiscMenue() , NULL, CRCInput::RC_blue);
		mf->setHint(NEUTRINO_ICON_HINT_EXTENDED, LOCALE_MENU_HINT_EXTENDED);
		personalize.addItem(MENU_SETTINGS, mf, &g_settings.personalize[SNeutrinoSettings::P_MSET_MISC]);

		//separator
		personalize.addSeparator(MENU_SETTINGS);
	}

	//1st section***************************************************************************************************

	//use only 10 shortcuts (1-9, 0), >9 means -> no shortcut

	// video.
	if (g_settings.easymenu)
		mf = new CMenuForwarder(LOCALE_MAINSETTINGS_VIDEO, true, NULL, g_videoSettings, NULL, CRCInput::RC_green);
	else
		mf = new CMenuForwarder(LOCALE_MAINSETTINGS_VIDEO, true, NULL, g_videoSettings);
	mf->setHint(NEUTRINO_ICON_HINT_VIDEO, LOCALE_MENU_HINT_VIDEO);
	personalize.addItem(MENU_SETTINGS, mf, &g_settings.personalize[SNeutrinoSettings::P_MSET_VIDEO]);

	// audio
	if (g_settings.easymenu)
		mf = new CMenuForwarder(LOCALE_MAINSETTINGS_AUDIO, true, NULL, new CAudioSetup(), NULL, CRCInput::RC_yellow);
	else
		mf = new CMenuForwarder(LOCALE_MAINSETTINGS_AUDIO, true, NULL, new CAudioSetup());
	mf->setHint(NEUTRINO_ICON_HINT_AUDIO, LOCALE_MENU_HINT_AUDIO);
	personalize.addItem(MENU_SETTINGS, mf, &g_settings.personalize[SNeutrinoSettings::P_MSET_AUDIO]);

	// channel list
	if (g_settings.easymenu) {
		mf = new CMenuForwarder(LOCALE_MISCSETTINGS_CHANNELLIST, true, NULL, new CMiscMenue(), "channellist", CRCInput::RC_blue);
		mf->setHint("", LOCALE_MENU_HINT_MISC_CHANNELLIST);
		personalize.addItem(MENU_SETTINGS, mf, &g_settings.personalize[SNeutrinoSettings::P_MSET_MISC]);
	}

	// parental lock
	mf = new CMenuForwarder(LOCALE_PARENTALLOCK_PARENTALLOCK, true, NULL, new CParentalSetup());
	mf->setHint(NEUTRINO_ICON_HINT_PROTECTION, LOCALE_MENU_HINT_PROTECTION);
	personalize.addItem(MENU_SETTINGS, mf, &show, false, CPersonalizeGui::PERSONALIZE_SHOW_NO);

	// network
	mf = new CMenuForwarder(LOCALE_MAINSETTINGS_NETWORK, true, NULL, CNetworkSetup::getInstance());
	mf->setHint(NEUTRINO_ICON_HINT_NETWORK, LOCALE_MENU_HINT_NETWORK);
	personalize.addItem(MENU_SETTINGS, mf, &g_settings.personalize[SNeutrinoSettings::P_MSET_NETWORK]);

	// record settings
	if (g_settings.recording_type != CNeutrinoApp::RECORDING_OFF) {
		mf = new CMenuForwarder(LOCALE_MAINSETTINGS_RECORDING, true, NULL, new CRecordSetup());
		mf->setHint(NEUTRINO_ICON_HINT_RECORDING, LOCALE_MENU_HINT_RECORDING);
		personalize.addItem(MENU_SETTINGS, mf, &g_settings.personalize[SNeutrinoSettings::P_MSET_RECORDING]);
	}

	// osdlang
	mf = new CMenuForwarder(LOCALE_MAINSETTINGS_LANGUAGE, true, NULL, new COsdLangSetup());
	mf->setHint(NEUTRINO_ICON_HINT_LANGUAGE, LOCALE_MENU_HINT_LANGUAGE);
	personalize.addItem(MENU_SETTINGS, mf, &g_settings.personalize[SNeutrinoSettings::P_MSET_OSDLANG]);

	// osd
	if (!g_settings.easymenu) {
		mf = new CMenuForwarder(LOCALE_MAINSETTINGS_OSD, true, NULL, new COsdSetup());
		mf->setHint(NEUTRINO_ICON_HINT_OSD, LOCALE_MENU_HINT_OSD);
		personalize.addItem(MENU_SETTINGS, mf, &g_settings.personalize[SNeutrinoSettings::P_MSET_OSD]);
	}

	// lcd
	mf = new CMenuForwarder(LOCALE_MAINSETTINGS_LCD, true, NULL, new CVfdSetup());
	mf->setHint(NEUTRINO_ICON_HINT_VFD, LOCALE_MENU_HINT_VFD);
	personalize.addItem(MENU_SETTINGS, mf, &g_settings.personalize[SNeutrinoSettings::P_MSET_VFD]);

	// drive settings
	if (g_settings.recording_type != CNeutrinoApp::RECORDING_OFF) {
		mf = new CMenuForwarder(LOCALE_HDD_SETTINGS, true, NULL, CHDDMenuHandler::getInstance());
		mf->setHint(NEUTRINO_ICON_HINT_HDD, LOCALE_MENU_HINT_HDD);
		personalize.addItem(MENU_SETTINGS, mf, &g_settings.personalize[SNeutrinoSettings::P_MSET_DRIVES]);
	}

	// energy
	if (g_settings.easymenu) {
		mf = new CMenuForwarder(LOCALE_MISCSETTINGS_ENERGY, true, NULL, new CMiscMenue(), "energy");
		mf->setHint("", LOCALE_MENU_HINT_MISC_ENERGY);
		personalize.addItem(MENU_SETTINGS, mf, &g_settings.personalize[SNeutrinoSettings::P_MSET_MISC]);
	}

	if (!g_settings.easymenu) {
		// cisettings
		mf = new CMenuForwarder(LOCALE_CI_SETTINGS, true, NULL, g_CamHandler);
		mf->setHint(NEUTRINO_ICON_HINT_CI, LOCALE_MENU_HINT_CI);
		personalize.addItem(MENU_SETTINGS, mf, &g_settings.personalize[SNeutrinoSettings::P_MSET_CISETTINGS]);

		// keybindings
		mf = new CMenuForwarder(LOCALE_MAINSETTINGS_KEYBINDING, true, NULL, new CKeybindSetup());
		mf->setHint(NEUTRINO_ICON_HINT_KEYS, LOCALE_MENU_HINT_KEYS);
		personalize.addItem(MENU_SETTINGS, mf, &g_settings.personalize[SNeutrinoSettings::P_MSET_KEYBINDING]);

		// multimedia settings
		mf = new CMenuForwarder(LOCALE_MAINSETTINGS_MULTIMEDIA, true, NULL, new CMediaPlayerSetup());
		mf->setHint(NEUTRINO_ICON_HINT_A_PIC, LOCALE_MENU_HINT_A_PIC);
		personalize.addItem(MENU_SETTINGS, mf, &g_settings.personalize[SNeutrinoSettings::P_MSET_MEDIAPLAYER]);
	}
}


/* service menu*/
void CNeutrinoApp::InitMenuService()
{
	dprintf(DEBUG_DEBUG, "init service menu...\n");

	//CMenuWidget &menu = personalize.getWidget(MENU_SERVICE);

	// Dynamic renumbering
	personalize.setShortcut();

	// back button, no personalized
	personalize.addIntroItems(MENU_SERVICE);

	//1st section***************************************************************************************************

	CMenuForwarder * mf;
	// tuner setup
	if(CFEManager::getInstance()->haveSat() || CFEManager::getInstance()->getFrontendCount() > 1) {
		mf = new CMenuForwarder(LOCALE_SATSETUP_FE_SETUP, true, NULL, CScanSetup::getInstance(), "setup_frontend", CRCInput::RC_red);
		mf->setHint(NEUTRINO_ICON_HINT_SETTINGS, LOCALE_MENU_HINT_SCAN_FESETUP);
		personalize.addItem(MENU_SERVICE, mf, &g_settings.personalize[SNeutrinoSettings::P_MSER_TUNER]);
	}

	// channel scan
	mf = new CMenuForwarder(LOCALE_SERVICEMENU_SCANTS    , true, NULL, CScanSetup::getInstance(), "", CRCInput::RC_green);
	mf->setHint(NEUTRINO_ICON_HINT_SERVICE_SCAN, LOCALE_MENU_HINT_SERVICE_SCAN);
	personalize.addItem(MENU_SERVICE, mf, &g_settings.personalize[SNeutrinoSettings::P_MSER_SCANTS], false, CPersonalizeGui::PERSONALIZE_SHOW_AS_ITEM_OPTION, NULL, DCOND_MODE_TS);

	if (!g_settings.easymenu) {
		//reload channels
		mf = new CMenuForwarder(LOCALE_SERVICEMENU_RELOAD    , true, NULL, CScanSetup::getInstance(), "reloadchannels", CRCInput::RC_yellow);
		mf->setHint(NEUTRINO_ICON_HINT_RELOAD_CHANNELS, LOCALE_MENU_HINT_RELOAD_CHANNELS);
		personalize.addItem(MENU_SERVICE, mf, &g_settings.personalize[SNeutrinoSettings::P_MSER_RELOAD_CHANNELS]);
	}

	//bouquet edit
	if (!g_settings.easymenu) {
		mf = new CMenuForwarder(LOCALE_BOUQUETEDITOR_NAME    , true, NULL, new CBEBouquetWidget(), NULL, CRCInput::RC_blue);

		mf->setHint(NEUTRINO_ICON_HINT_BEDIT, LOCALE_MENU_HINT_BEDIT);
		personalize.addItem(MENU_SERVICE, mf, &g_settings.personalize[SNeutrinoSettings::P_MSER_BOUQUET_EDIT]);
	}

	//channel reset
	CDataResetNotifier *resetNotifier = new CDataResetNotifier();
	if (g_settings.easymenu)
		mf = new CMenuForwarder(LOCALE_RESET_CHANNELS    , true, NULL, resetNotifier, "channels", CRCInput::RC_yellow);
	else
		mf = new CMenuForwarder(LOCALE_RESET_CHANNELS    , true, NULL, resetNotifier, "channels");

	mf->setHint(NEUTRINO_ICON_HINT_DELETE_CHANNELS, LOCALE_MENU_HINT_DELETE_CHANNELS);
	personalize.addItem(MENU_SERVICE, mf, &g_settings.personalize[SNeutrinoSettings::P_MSER_RESET_CHANNELS]);

	if (!g_settings.easymenu) {
		/* todo: only show if (g_settings.make_removed_list) */
		mf = new CMenuForwarder(LOCALE_RESET_REMOVED, true, NULL, resetNotifier, "delete_removed");
		mf->setHint(NEUTRINO_ICON_HINT_DELETE_CHANNELS, LOCALE_MENU_HINT_DELETE_REMOVED);
		personalize.addItem(MENU_SERVICE, mf, &g_settings.personalize[SNeutrinoSettings::P_MSER_RESET_CHANNELS]);
	}

	//separator
	personalize.addSeparator(MENU_SERVICE);


	if (!g_settings.easymenu) {
		personalize.addSeparator(MENU_SERVICE);

		//restart neutrino
		mf = new CMenuForwarder(LOCALE_SERVICEMENU_RESTART   , true, NULL, this, "restart", CRCInput::RC_standby);
		mf->setHint(NEUTRINO_ICON_HINT_SOFT_RESTART, LOCALE_MENU_HINT_SOFT_RESTART);
		personalize.addItem(MENU_SERVICE, mf, &g_settings.personalize[SNeutrinoSettings::P_MSER_RESTART]);

		//reload plugins
		mf = new CMenuForwarder(LOCALE_SERVICEMENU_GETPLUGINS, true, NULL, this, "reloadplugins");
		mf->setHint(NEUTRINO_ICON_HINT_RELOAD_CHANNELS, LOCALE_MENU_HINT_RELOAD_PLUGINS);
		personalize.addItem(MENU_SERVICE, mf, &g_settings.personalize[SNeutrinoSettings::P_MSER_RELOAD_PLUGINS]);

		//separator
		personalize.addSeparator(MENU_SERVICE);
	}

	//2nd section***************************************************************************************************

	//infomenu
	if (!g_settings.easymenu) {
		mf = new CMenuForwarder(LOCALE_MESSAGEBOX_INFO, true, NULL, new CInfoMenu(), NULL, CRCInput::RC_info);
		mf->setHint(NEUTRINO_ICON_HINT_INFO, LOCALE_MENU_HINT_INFO);
		personalize.addItem(MENU_SERVICE, mf, &g_settings.personalize[SNeutrinoSettings::P_MSER_SERVICE_INFOMENU]);
	}

	//firmware update
	if (g_settings.easymenu)
		mf = new CMenuForwarder(LOCALE_SERVICEMENU_UPDATE, true, NULL, new CSoftwareUpdate(), NULL, CRCInput::RC_blue);
	else
		mf = new CMenuForwarder(LOCALE_SERVICEMENU_UPDATE, true, NULL, new CSoftwareUpdate());

	mf->setHint(NEUTRINO_ICON_HINT_SW_UPDATE, LOCALE_MENU_HINT_SW_UPDATE);
	personalize.addItem(MENU_SERVICE, mf, &g_settings.personalize[SNeutrinoSettings::P_MSER_SOFTUPDATE], false, CPersonalizeGui::PERSONALIZE_SHOW_AS_ITEM_OPTION, NULL, DCOND_MODE_TS);
}
