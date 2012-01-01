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

//#define TEST_MENU

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
#include "gui/epg_menu.h"
#include <gui/epgplus.h>
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
#include "gui/osd_setup.h"
#include "gui/osdlang_setup.h"
#include "gui/parentallock_setup.h"
#include "gui/pluginlist.h"
#include "gui/plugins.h"
#include "gui/record_setup.h"
#include "gui/scan_setup.h"
#include "gui/settings_manager.h"
#include "gui/sleeptimer.h"
#include "gui/software_update.h"
#include <gui/streaminfo2.h>
#ifdef TEST_MENU
#include "gui/test_menu.h"
#endif /*TEST_MENU*/
#include "gui/update.h"
#include "gui/vfd_setup.h"
#include <driver/record.h>


extern CPlugins       * g_PluginList;
extern CRemoteControl * g_RemoteControl;
extern CCAMMenuHandler * g_CamHandler;
// extern bool has_hdd;
// extern char current_timezone[50];
// extern bool autoshift;

enum
{
	MENU_MAIN,
	MENU_SETTINGS,
	MENU_SERVICE,
	
	MENU_MAX //3
};

#define MENU_WIDTH 35

const mn_widget_struct_t menu_widgets[MENU_MAX] =
{
	{LOCALE_MAINMENU_HEAD, 		NEUTRINO_ICON_MAINMENU, 	MENU_WIDTH},	/* 0 = MENU_MAIN*/
	{LOCALE_MAINSETTINGS_HEAD, 	NEUTRINO_ICON_SETTINGS, 	MENU_WIDTH},	/* 1 = MENU_SETTINGS*/
	{LOCALE_SERVICEMENU_HEAD,	NEUTRINO_ICON_SETTINGS, 	MENU_WIDTH}, 	/* 2 = MENU_SERVICE*/
};

//init all menues
void CNeutrinoApp::InitMenu()
{
	printf("[neutrino] init menus...\n");
	
	//personalize: neutrino.h, neutrino.cpp
	personalize.enableUsermenu();
	personalize.enablePinSetup();
	personalize.addWidgets(menu_widgets, MENU_MAX);
		
	InitMenuMain();
 	InitMenuSettings();
 	InitMenuService();
	//add submenu for media
	CMediaPlayerMenu::getInstance()->initMenuMedia(new CMenuWidget(LOCALE_MAINMENU_MEDIA, NEUTRINO_ICON_MULTIMEDIA, MENU_WIDTH), &personalize);
	
	personalize.addPersonalizedItems();
}

//init main menu
void CNeutrinoApp::InitMenuMain()
{
	dprintf(DEBUG_DEBUG, "init mainmenue\n");
	
	unsigned int system_rev = cs_get_revision();
	
	// Dynamic renumbering
	personalize.setShortcut();
	
	//CMenuWidget &menu = personalize.getWidget(MENU_MAIN)/**main**/;
	
	//top
	personalize.addItem(MENU_MAIN, GenericMenuSeparator, NULL, false, CPersonalizeGui::PERSONALIZE_SHOW_NO);
	
	//1st section*************************************************************************************************** 
	
	//tv-mode
	CMenuItem *tvswitch = new CMenuForwarder(LOCALE_MAINMENU_TVMODE, true, NULL, this, "tv", CRCInput::RC_red, NEUTRINO_ICON_BUTTON_RED);
	personalize.addItem(MENU_MAIN, tvswitch, &g_settings.personalize[SNeutrinoSettings::P_MAIN_TV_MODE]);
	
	//radio-mode
	CMenuItem *radioswitch = new CMenuForwarder(LOCALE_MAINMENU_RADIOMODE, true, NULL, this, "radio", CRCInput::RC_green, NEUTRINO_ICON_BUTTON_GREEN);
	personalize.addItem(MENU_MAIN, radioswitch, &g_settings.personalize[SNeutrinoSettings::P_MAIN_RADIO_MODE]);
	
	//tv <-> radio toggle
	CMenuItem *tvradio_switch = new CMenuForwarder(LOCALE_MAINMENU_TVRADIO_SWITCH, true, NULL, this, "tv_radio_switch", CRCInput::RC_red, NEUTRINO_ICON_BUTTON_RED);
	personalize.addItem(MENU_MAIN, tvradio_switch, &g_settings.personalize[SNeutrinoSettings::P_MAIN_TV_RADIO_MODE]);
	
	//timer
	CMenuItem *timerlist = new CMenuForwarder(LOCALE_TIMERLIST_NAME, true, NULL, new CTimerList(), NULL, CRCInput::RC_yellow, NEUTRINO_ICON_BUTTON_YELLOW);
	personalize.addItem(MENU_MAIN, timerlist, &g_settings.personalize[SNeutrinoSettings::P_MAIN_TIMER]);
		
	//multimedia menu
	CMenuItem *media = new CMenuForwarder(LOCALE_MAINMENU_MEDIA, true, NULL, CMediaPlayerMenu::getInstance(), NULL, CRCInput::RC_blue, NEUTRINO_ICON_BUTTON_BLUE);
 	personalize.addItem(MENU_MAIN, media, &g_settings.personalize[SNeutrinoSettings::P_MAIN_MEDIA]);
		
	//separator
 	personalize.addSeparator(MENU_MAIN);
	
	
	//2nd section***************************************************************************************************
	
	//games
	bool show_games = g_PluginList->hasPlugin(CPlugins::P_TYPE_GAME);
	personalize.addItem(MENU_MAIN, new CMenuForwarder(LOCALE_MAINMENU_GAMES, show_games, NULL, new CPluginList(LOCALE_MAINMENU_GAMES,CPlugins::P_TYPE_GAME)), &g_settings.personalize[SNeutrinoSettings::P_MAIN_GAMES]);

	//scripts 
	bool show_scripts = g_PluginList->hasPlugin(CPlugins::P_TYPE_SCRIPT);
	personalize.addItem(MENU_MAIN, new CMenuForwarder(LOCALE_MAINMENU_SCRIPTS, show_scripts, NULL, new CPluginList(LOCALE_MAINMENU_SCRIPTS,CPlugins::P_TYPE_SCRIPT)), &g_settings.personalize[SNeutrinoSettings::P_MAIN_SCRIPTS]);
	
	// settings, also as pin protected option in personalize menu, as a result of parameter value CPersonalizeGui::PERSONALIZE_SHOW_AS_ACCESS_OPTION
	personalize.addItem(MENU_MAIN, new CMenuForwarder(LOCALE_MAINMENU_SETTINGS, true, NULL, &personalize.getWidget(MENU_SETTINGS)/**settings**/), &g_settings.personalize[SNeutrinoSettings::P_MAIN_SETTINGS], false, CPersonalizeGui::PERSONALIZE_SHOW_AS_ACCESS_OPTION);

	// service, also as pin protected option in personalize menu, as a result of parameter value CPersonalizeGui::PERSONALIZE_SHOW_AS_ACCESS_OPTION
	personalize.addItem(MENU_MAIN, new CMenuForwarder(LOCALE_MAINMENU_SERVICE, true, NULL, &personalize.getWidget(MENU_SERVICE)/**service**/), &g_settings.personalize[SNeutrinoSettings::P_MAIN_SERVICE], false, CPersonalizeGui::PERSONALIZE_SHOW_AS_ACCESS_OPTION);
	
	//separator
	personalize.addSeparator(MENU_MAIN);
	
	//3rd section***************************************************************************************************
	
	//10. -- only 10 shortcuts (1-9, 0), the next could be the last also!(10. => 0)
	//sleeptimer
	personalize.addItem(MENU_MAIN, new CMenuForwarder(LOCALE_MAINMENU_SLEEPTIMER, true, NULL, new CSleepTimerWidget), &g_settings.personalize[SNeutrinoSettings::P_MAIN_SLEEPTIMER]);

	//reboot
	personalize.addItem(MENU_MAIN, new CMenuForwarder(LOCALE_MAINMENU_REBOOT, true, NULL, this, "reboot"), &g_settings.personalize[SNeutrinoSettings::P_MAIN_REBOOT]);

	//shutdown
	if(system_rev >= 8)
		personalize.addItem(MENU_MAIN, new CMenuForwarder(LOCALE_MAINMENU_SHUTDOWN, true, NULL, this, "shutdown", CRCInput::RC_standby, NEUTRINO_ICON_BUTTON_POWER), &g_settings.personalize[SNeutrinoSettings::P_MAIN_SHUTDOWN]);

	//separator
	personalize.addSeparator(MENU_MAIN);
	
	//4th section***************************************************************************************************

	//infomenu
	personalize.addItem(MENU_MAIN, new CMenuForwarder(LOCALE_MESSAGEBOX_INFO, true, NULL, new CInfoMenu(), NULL, CRCInput::RC_info, NEUTRINO_ICON_BUTTON_INFO_SMALL), &g_settings.personalize[SNeutrinoSettings::P_MAIN_INFOMENU]);
	
	//cisettings
	if (cCA::GetInstance()->GetNumberCISlots() > 0 || cCA::GetInstance()->GetNumberSmartCardSlots() > 0)
		personalize.addItem(MENU_MAIN, new CMenuForwarder(LOCALE_CI_SETTINGS, true, NULL, g_CamHandler), &g_settings.personalize[SNeutrinoSettings::P_MAIN_CISETTINGS]);
	
#ifdef TEST_MENU
	personalize.addItem(MENU_MAIN, new CMenuForwarderNonLocalized("Test menu", true, NULL, new CTestMenu(), CPersonalizeGui::PERSONALIZE_SHOW_NO));
#endif
}

//settings menue
void CNeutrinoApp::InitMenuSettings()
{
	dprintf(DEBUG_DEBUG, "init settings menue...\n");
	
	//CMenuWidget &menu = personalize.getWidget(MENU_SETTINGS)/**settings**/;
	
	// Dynamic renumbering
	personalize.setShortcut();
	
	// back button, no personalized
	personalize.addIntroItems(MENU_SETTINGS);
	
	//***************************************************************************************************
	// save
	int show_save = CPersonalizeGui::PERSONALIZE_MODE_VISIBLE;
	personalize.addItem(MENU_SETTINGS, new CMenuForwarder(LOCALE_MAINSETTINGS_SAVESETTINGSNOW, true, NULL, this, "savesettings", CRCInput::RC_red, NEUTRINO_ICON_BUTTON_RED), &show_save, false, CPersonalizeGui::PERSONALIZE_SHOW_NO);
	
	// separator line
	personalize.addItem(MENU_SETTINGS, GenericMenuSeparatorLine, NULL, false, CPersonalizeGui::PERSONALIZE_SHOW_NO);
	
	//1st section***************************************************************************************************
	
	// video.
	personalize.addItem(MENU_SETTINGS, new CMenuForwarder(LOCALE_MAINSETTINGS_VIDEO, true, NULL, g_videoSettings), &g_settings.personalize[SNeutrinoSettings::P_MSET_VIDEO]);	

	// audio
	personalize.addItem(MENU_SETTINGS, new CMenuForwarder(LOCALE_MAINSETTINGS_AUDIO, true, NULL, new CAudioSetup()), &g_settings.personalize[SNeutrinoSettings::P_MSET_AUDIO]);
	
	// parental lock
	personalize.addItem(MENU_SETTINGS, new CMenuForwarder(LOCALE_PARENTALLOCK_PARENTALLOCK, true, NULL, new CParentalSetup()), &g_settings.personalize[SNeutrinoSettings::P_MSET_YOUTH]);

	// network
	if(networksetup == NULL)
		networksetup = new CNetworkSetup();
	personalize.addItem(MENU_SETTINGS, new CMenuForwarder(LOCALE_MAINSETTINGS_NETWORK, true, NULL, networksetup), &g_settings.personalize[SNeutrinoSettings::P_MSET_NETWORK]);

	// record settings
	personalize.addItem(MENU_SETTINGS, new CMenuForwarder(LOCALE_MAINSETTINGS_RECORDING, true, NULL, new CRecordSetup()), &g_settings.personalize[SNeutrinoSettings::P_MSET_RECORDING]);

	// osdlang
	personalize.addItem(MENU_SETTINGS, new CMenuForwarder(LOCALE_MAINSETTINGS_LANGUAGE, true, NULL, new COsdLangSetup()), &g_settings.personalize[SNeutrinoSettings::P_MSET_OSDLANG]);
	
	// osd
	personalize.addItem(MENU_SETTINGS, new CMenuForwarder(LOCALE_MAINSETTINGS_OSD, true, NULL, new COsdSetup()), &g_settings.personalize[SNeutrinoSettings::P_MSET_OSD]);
	
	// lcd
	if (CVFD::getInstance()->has_lcd)
		personalize.addItem(MENU_SETTINGS, new CMenuForwarder(LOCALE_MAINSETTINGS_LCD, true, NULL, new CVfdSetup()), &g_settings.personalize[SNeutrinoSettings::P_MSET_VFD]);
	
	// drive settings
	personalize.addItem(MENU_SETTINGS, new CMenuForwarder(LOCALE_HDD_SETTINGS, true, NULL, new CHDDMenuHandler()), &g_settings.personalize[SNeutrinoSettings::P_MSET_DRIVES]);
	
	// cisettings
	personalize.addItem(MENU_SETTINGS, new CMenuForwarder(LOCALE_CI_SETTINGS, true, NULL, g_CamHandler), &g_settings.personalize[SNeutrinoSettings::P_MSET_CISETTINGS]);
	
	//separator
	personalize.addSeparator(MENU_SETTINGS);
		
	//***************************************************************************************************	
	//10. -- only 10 shortcuts (1-9, 0), the next could be the last also!(10. => 0)
	// settings manager
	personalize.addItem(MENU_SETTINGS, new CMenuForwarder(LOCALE_MAINSETTINGS_MANAGE, true, NULL, new CSettingsManager()), &g_settings.personalize[SNeutrinoSettings::P_MSET_SETTINGS_MANAGER], false, CPersonalizeGui::PERSONALIZE_SHOW_AS_ACCESS_OPTION);

	// personalize
	personalize.addItem(MENU_SETTINGS, new CMenuForwarder(LOCALE_PERSONALIZE_HEAD, true, NULL, &personalize), &g_settings.personalize[SNeutrinoSettings::P_MAIN_PINSTATUS], false, CPersonalizeGui::PERSONALIZE_SHOW_AS_ACCESS_OPTION);
	
	// separator
	personalize.addSeparator(MENU_SETTINGS);
		
	//***************************************************************************************************
		
	// keybindings
	personalize.addItem(MENU_SETTINGS, new CMenuForwarder(LOCALE_MAINSETTINGS_KEYBINDING, true, NULL, new CKeybindSetup(), NULL, CRCInput::RC_green, NEUTRINO_ICON_BUTTON_GREEN), &g_settings.personalize[SNeutrinoSettings::P_MSET_KEYBINDING]);
	
	// audioplayer/pictureviewer settings
	personalize.addItem(MENU_SETTINGS, new CMenuForwarder(LOCALE_AUDIOPLAYERPICSETTINGS_GENERAL, true, NULL, new CMediaPlayerSetup(), NULL, CRCInput::RC_yellow, NEUTRINO_ICON_BUTTON_YELLOW), &g_settings.personalize[SNeutrinoSettings::P_MSET_MEDIAPLAYER]);

	// miscSettings
	personalize.addItem(MENU_SETTINGS, new CMenuForwarder(LOCALE_MAINSETTINGS_MISC, true, NULL, new CMiscMenue() , NULL, CRCInput::RC_blue , NEUTRINO_ICON_BUTTON_BLUE), &g_settings.personalize[SNeutrinoSettings::P_MSET_MISC]);
	
}
	

/* service menu*/
void CNeutrinoApp::InitMenuService()
{
	dprintf(DEBUG_DEBUG, "init service menu...\n");
	
	//CMenuWidget &menu = personalize.getWidget(MENU_SERVICE)/**service**/;
	
	// Dynamic renumbering
	personalize.setShortcut();

	// back button, no personalized
	personalize.addIntroItems(MENU_SERVICE);
	
	//1st section***************************************************************************************************
	
	// channel scan
	personalize.addItem(MENU_SERVICE, new CMenuForwarder(LOCALE_SERVICEMENU_SCANTS    , true, NULL, CScanSetup::getInstance(), "", CRCInput::RC_red, NEUTRINO_ICON_BUTTON_RED) , &g_settings.personalize[SNeutrinoSettings::P_MSER_SCANTS]);
	
	//reload channels
	personalize.addItem(MENU_SERVICE, new CMenuForwarder(LOCALE_SERVICEMENU_RELOAD    , true, NULL, CScanSetup::getInstance(), "reloadchannels", CRCInput::RC_green, NEUTRINO_ICON_BUTTON_GREEN) , &g_settings.personalize[SNeutrinoSettings::P_MSER_RELOAD_CHANNELS]);
	
	//bouquet edit
	personalize.addItem(MENU_SERVICE, new CMenuForwarder(LOCALE_BOUQUETEDITOR_NAME    , true, NULL, new CBEBouquetWidget(), NULL, CRCInput::RC_yellow, NEUTRINO_ICON_BUTTON_YELLOW) , &g_settings.personalize[SNeutrinoSettings::P_MSER_BOUQUET_EDIT]);
	
	//channel reset
	CDataResetNotifier *resetNotifier = new CDataResetNotifier();
	personalize.addItem(MENU_SERVICE, new CMenuForwarder(LOCALE_RESET_CHANNELS    , true, NULL, resetNotifier, "channels", CRCInput::RC_blue, NEUTRINO_ICON_BUTTON_BLUE) , &g_settings.personalize[SNeutrinoSettings::P_MSER_RESET_CHANNELS]);
	
	//restart neutrino
	personalize.addItem(MENU_SERVICE, new CMenuForwarder(LOCALE_SERVICEMENU_RESTART   , true, NULL, this, "restart", CRCInput::RC_standby, NEUTRINO_ICON_BUTTON_POWER) , &g_settings.personalize[SNeutrinoSettings::P_MSER_RESTART]);
	
	//reload plugins
	personalize.addItem(MENU_SERVICE, new CMenuForwarder(LOCALE_SERVICEMENU_GETPLUGINS, true, NULL, this, "reloadplugins") , &g_settings.personalize[SNeutrinoSettings::P_MSER_RELOAD_PLUGINS]);
	
 	//separator
	personalize.addSeparator(MENU_SERVICE);
 	
	//2nd section***************************************************************************************************
	
	//infomenu
	personalize.addItem(MENU_SERVICE, new CMenuForwarder(LOCALE_MESSAGEBOX_INFO, true, NULL, new CInfoMenu(), NULL, CRCInput::RC_info, NEUTRINO_ICON_BUTTON_INFO_SMALL) , &g_settings.personalize[SNeutrinoSettings::P_MSER_SERVICE_INFOMENU]);
	
	//firmware update
	personalize.addItem(MENU_SERVICE, new CMenuForwarder(LOCALE_SERVICEMENU_UPDATE, true, NULL, new CSoftwareUpdate()) , &g_settings.personalize[SNeutrinoSettings::P_MSER_SOFTUPDATE]);
	
}

