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


/**************************************************************************************
*          CNeutrinoApp -  init main menu                                             *
**************************************************************************************/
void CNeutrinoApp::InitMainMenu(CMenuWidget &mainMenu, CMenuWidget &mainSettings, CMenuWidget &service)
{
	unsigned int system_rev = cs_get_revision();

	int shortcut = 1;

	dprintf(DEBUG_DEBUG, "init mainmenue\n");
	mainMenu.addItem(GenericMenuSeparator);

	mainMenu.addItem(new CMenuForwarder(LOCALE_MAINMENU_TVMODE, true, NULL, this, "tv_radio_switch", CRCInput::RC_red, NEUTRINO_ICON_BUTTON_RED), true);

	//games	
	if (g_PluginList->hasPlugin(CPlugins::P_TYPE_GAME))
		mainMenu.addItem(new CMenuForwarder(LOCALE_MAINMENU_GAMES, true, NULL, new CPluginList(LOCALE_MAINMENU_GAMES,CPlugins::P_TYPE_GAME), "", CRCInput::RC_green, NEUTRINO_ICON_BUTTON_GREEN));
	
	//timer
	mainMenu.addItem(new CMenuForwarder(LOCALE_TIMERLIST_NAME, true, NULL, new CTimerList(), NULL, CRCInput::RC_yellow, NEUTRINO_ICON_BUTTON_YELLOW));

	//multimedia menu
 	mainMenu.addItem(new CMenuForwarder(LOCALE_MAINMENU_MEDIA, true, NULL, CMediaPlayerMenu::getInstance(), NULL, CRCInput::RC_blue, NEUTRINO_ICON_BUTTON_BLUE));

	if (g_PluginList->hasPlugin(CPlugins::P_TYPE_SCRIPT))
		mainMenu.addItem(new CMenuForwarder(LOCALE_MAINMENU_SCRIPTS, true, NULL, new CPluginList(LOCALE_MAINMENU_SCRIPTS,CPlugins::P_TYPE_SCRIPT), "",
						    CRCInput::convertDigitToKey(shortcut++)));
	mainMenu.addItem(GenericMenuSeparatorLine);

	mainMenu.addItem(new CMenuForwarder(LOCALE_MAINMENU_SETTINGS, true, NULL, &mainSettings, NULL,
					    CRCInput::convertDigitToKey(shortcut++)));
	mainMenu.addItem(new CLockedMenuForwarder(LOCALE_MAINMENU_SERVICE, g_settings.parentallock_pincode, false, true, NULL, &service, NULL, CRCInput::convertDigitToKey(shortcut++)));
	mainMenu.addItem(GenericMenuSeparatorLine);

	mainMenu.addItem(new CMenuForwarder(LOCALE_MAINMENU_SLEEPTIMER, true, NULL, new CSleepTimerWidget, "",
					    CRCInput::convertDigitToKey(shortcut++)));
	mainMenu.addItem(new CMenuForwarder(LOCALE_MAINMENU_REBOOT, true, NULL, this, "reboot",
					    CRCInput::convertDigitToKey(shortcut++)));

	if(system_rev >= 8)
		mainMenu.addItem(new CMenuForwarder(LOCALE_MAINMENU_SHUTDOWN, true, NULL, this, "shutdown", CRCInput::RC_standby, NEUTRINO_ICON_BUTTON_POWER));

	mainMenu.addItem( new CMenuSeparator(CMenuSeparator::LINE) );
	// start of infomenu
	if (g_settings.show_infomenu == 0)
		mainMenu.addItem(new CMenuForwarder(LOCALE_MESSAGEBOX_INFO, true, NULL, new CInfoMenu(), NULL, CRCInput::RC_info, NEUTRINO_ICON_BUTTON_INFO_SMALL ));
	
	// end of infomenu
	if (cCA::GetInstance()->GetNumberCISlots() > 0 || cCA::GetInstance()->GetNumberSmartCardSlots() > 0)
		mainMenu.addItem( new CMenuForwarder(LOCALE_CI_SETTINGS, true, NULL, g_CamHandler, NULL, CRCInput::convertDigitToKey(0)));

	//settings menu
	int sett_count =1;
	mainSettings.addIntroItems();

	mainSettings.addItem(new CMenuForwarder(LOCALE_MAINSETTINGS_SAVESETTINGSNOW, true, NULL, this, "savesettings", CRCInput::RC_red, NEUTRINO_ICON_BUTTON_RED));

	mainSettings.addItem(new CMenuForwarder(LOCALE_MAINSETTINGS_MANAGE, true, NULL, new CSettingsManager()));

	mainSettings.addItem(GenericMenuSeparatorLine);
	
	mainSettings.addItem(new CMenuForwarder(LOCALE_MAINSETTINGS_VIDEO     , true, NULL, g_videoSettings, NULL, CRCInput::convertDigitToKey(sett_count++)));
	mainSettings.addItem(new CMenuForwarder(LOCALE_MAINSETTINGS_AUDIO     , true, NULL, new CAudioSetup() , NULL, CRCInput::convertDigitToKey(sett_count++)));
	mainSettings.addItem(new CLockedMenuForwarder(LOCALE_PARENTALLOCK_PARENTALLOCK, g_settings.parentallock_pincode, true, true, NULL, new CParentalSetup()/*&parentallockSettings*/, NULL, CRCInput::convertDigitToKey(sett_count++)));

#if 0
	if (g_settings.parentallock_prompt)
		mainSettings.addItem(new CLockedMenuForwarder(LOCALE_PARENTALLOCK_PARENTALLOCK, g_settings.parentallock_pincode, true, true, NULL, &parentallockSettings, NULL, CRCInput::convertDigitToKey(sett_count++)));
	else
		mainSettings.addItem(new CMenuForwarder(LOCALE_PARENTALLOCK_PARENTALLOCK, true, NULL, &parentallockSettings, NULL, CRCInput::convertDigitToKey(sett_count++)));
#endif
        if(networksetup == NULL)
		networksetup = new CNetworkSetup();
	mainSettings.addItem(new CMenuForwarder(LOCALE_MAINSETTINGS_NETWORK   , true, NULL, networksetup  , NULL, CRCInput::convertDigitToKey(sett_count++)));
	mainSettings.addItem(new CMenuForwarder(LOCALE_MAINSETTINGS_RECORDING , true, NULL, new CRecordSetup(), NULL, CRCInput::convertDigitToKey(sett_count++)));
	mainSettings.addItem(new CMenuForwarder(LOCALE_MAINSETTINGS_LANGUAGE  , true, NULL, new COsdLangSetup(), NULL, CRCInput::convertDigitToKey(sett_count++)));

	mainSettings.addItem(new CMenuForwarder(LOCALE_MAINSETTINGS_OSD    , true, NULL, new COsdSetup()  , NULL, CRCInput::convertDigitToKey(sett_count++)));

	if (CVFD::getInstance()->has_lcd)
		mainSettings.addItem(new CMenuForwarder(LOCALE_MAINSETTINGS_LCD       , true, NULL, new CVfdSetup(), NULL, CRCInput::convertDigitToKey(sett_count++)));
	
	mainSettings.addItem(new CMenuForwarder(LOCALE_HDD_SETTINGS, true, NULL, new CHDDMenuHandler(), NULL, CRCInput::convertDigitToKey(sett_count++)));
	mainSettings.addItem(new CMenuForwarder(LOCALE_MAINSETTINGS_KEYBINDING, true, NULL, new CKeybindSetup(), NULL, CRCInput::RC_green  , NEUTRINO_ICON_BUTTON_GREEN  ));
	mainSettings.addItem(new CMenuForwarder(LOCALE_AUDIOPLAYERPICSETTINGS_GENERAL , true, NULL, new CMediaPlayerSetup(), NULL, CRCInput::RC_yellow, NEUTRINO_ICON_BUTTON_YELLOW));
	mainSettings.addItem(new CMenuForwarder(LOCALE_MAINSETTINGS_MISC      , true, NULL, new CMiscMenue() , NULL, CRCInput::RC_blue , NEUTRINO_ICON_BUTTON_BLUE ));
	
	//mainSettings.addItem(new CMenuForwarder(LOCALE_CAM_SETTINGS, true, NULL, g_CamHandler));

#ifdef TEST_MENU
	mainMenu.addItem(new CMenuForwarderNonLocalized("Test menu", true, NULL, new CTestMenu()));
#endif
#if 0
	g_RFmod = new RFmod();
	if (g_RFmod->rfmodfd >=0) {
		g_RFmod->init();
		CMenuWidget * rfmenu  = new CMenuWidget(LOCALE_RFMOD, NEUTRINO_ICON_SETTINGS);
		CRfNotifier * rfnot = new CRfNotifier();
		CRfExec * rfexec = new CRfExec();

		rfmenu->addItem( GenericMenuBack );
		rfmenu->addItem( new CMenuForwarder(LOCALE_RECORDINGMENU_SETUPNOW, true, NULL, rfexec, "savesettings", CRCInput::RC_red, NEUTRINO_ICON_BUTTON_RED));
		rfmenu->addItem( GenericMenuSeparatorLine );

		rfmenu->addItem( new CMenuOptionChooser(LOCALE_RF_CARRIER, &g_settings.rf_subcarrier, RF_CARRIER_OPTIONS, RF_CARRIER_OPTION_COUNT, true, rfnot));
		rfmenu->addItem( new CMenuOptionChooser(LOCALE_RF_ENABLE, &g_settings.rf_soundenable, OPTIONS_OFF1_ON0_OPTIONS, OPTIONS_OFF0_ON1_OPTION_COUNT, true, rfnot));
		rfmenu->addItem( new CMenuOptionNumberChooser(LOCALE_RF_CHANNEL, &g_settings.rf_channel, true, 21, 69) );
		rfmenu->addItem( new CMenuOptionNumberChooser(LOCALE_RF_FINETUNE, &g_settings.rf_finetune, true, 0, 64) );

		rfmenu->addItem( new CMenuOptionChooser(LOCALE_RF_STANDBY, &g_settings.rf_standby, OPTIONS_OFF1_ON0_OPTIONS, OPTIONS_OFF0_ON1_OPTION_COUNT, true, rfnot));
		rf_dummy = 0;
		rfmenu->addItem( new CMenuOptionChooser(LOCALE_RF_TEST, &rf_dummy, OPTIONS_OFF0_ON1_OPTIONS, OPTIONS_OFF0_ON1_OPTION_COUNT, true, rfnot));
		mainSettings.addItem(new CMenuForwarder(LOCALE_RFMOD, true, NULL, rfmenu));
	}
#endif
}

#define FLASHUPDATE_UPDATEMODE_OPTION_COUNT 2
const CMenuOptionChooser::keyval FLASHUPDATE_UPDATEMODE_OPTIONS[FLASHUPDATE_UPDATEMODE_OPTION_COUNT] =
{
	{ 0, LOCALE_FLASHUPDATE_UPDATEMODE_MANUAL   },
	{ 1, LOCALE_FLASHUPDATE_UPDATEMODE_INTERNET }
};


void CNeutrinoApp::InitServiceSettings(CMenuWidget &service)
{
	dprintf(DEBUG_DEBUG, "init serviceSettings\n");


#if 0
	CZapitDestExec * zexec = new CZapitDestExec();

	CMenuWidget* zapit_menu = new CMenuWidget(LOCALE_EXTRA_ZAPIT_MENU, NEUTRINO_ICON_SETTINGS);
	zapit_menu->addItem( GenericMenuBack );
	//zapit_menu->addItem(new CMenuForwarder(LOCALE_EXTRA_SAVESETTINGS, true, "", new CZapitDestExec, "savesettings", CRCInput::RC_red, NEUTRINO_ICON_BUTTON_RED));
	zapit_menu->addItem(new CMenuForwarder(LOCALE_EXTRA_SAVESETTINGS, true, "", this, "savesettings", CRCInput::RC_red, NEUTRINO_ICON_BUTTON_RED));
	zapit_menu->addItem( GenericMenuSeparatorLine );

	zapit_menu->addItem(new CMenuOptionChooser(LOCALE_EXTRA_ZAPIT_MAKE_BOUQUET, (int *)&zapitCfg.makeRemainingChannelsBouquet, OPTIONS_OFF0_ON1_OPTIONS, OPTIONS_OFF0_ON1_OPTION_COUNT, true));
	zapit_menu->addItem(new CMenuOptionChooser(LOCALE_EXTRA_ZAPIT_SAVE_LAST, (int *)&zapitCfg.saveLastChannel, OPTIONS_OFF0_ON1_OPTIONS, OPTIONS_OFF0_ON1_OPTION_COUNT, true));
	zapit_menu->addItem(new CMenuOptionChooser(LOCALE_EXTRA_ZAPIT_WRITE_NAMES, (int *)&zapitCfg.writeChannelsNames, OPTIONS_OFF0_ON1_OPTIONS, OPTIONS_OFF0_ON1_OPTION_COUNT, true));
	zapit_menu->addItem(new CMenuOptionChooser(LOCALE_EXTRA_ZAPIT_SORTNAMES,  (int *)&zapitCfg.sortNames, OPTIONS_OFF0_ON1_OPTIONS, OPTIONS_OFF0_ON1_OPTION_COUNT, true));

#if 0
	zapit_menu->addItem(new CMenuOptionNumberChooser(LOCALE_EXTRA_ZAPIT_TIMEOUT, (int *)&zapitCfg.feTimeout, true, 0, 100) );
#else
	zapit_menu->addItem(new CMenuOptionChooser(LOCALE_EXTRA_ZAPIT_FASTZAP,  (int *)&zapitCfg.fastZap, OPTIONS_OFF0_ON1_OPTIONS, OPTIONS_OFF0_ON1_OPTION_COUNT, true));
#endif
	zapit_menu->addItem( new CMenuOptionChooser(LOCALE_ZAPIT_SCANSDT, (int *)&zapitCfg.scanSDT, SECTIONSD_SCAN_OPTIONS, SECTIONSD_SCAN_OPTIONS_COUNT, true));

	zapit_menu->addItem(new CMenuOptionChooser(LOCALE_EXTRA_ZAPIT_SCANPIDS,  (int *)&zapitCfg.scanPids, OPTIONS_OFF0_ON1_OPTIONS, OPTIONS_OFF0_ON1_OPTION_COUNT, true));
	zapit_menu->addItem(new CMenuOptionChooser(LOCALE_EXTRA_ZAPIT_HVOLTAGE,  (int *)&zapitCfg.highVoltage, OPTIONS_OFF0_ON1_OPTIONS, OPTIONS_OFF0_ON1_OPTION_COUNT, true));

	zapit_menu->addItem( GenericMenuSeparatorLine );
	zapit_menu->addItem(new CMenuForwarder(LOCALE_EXTRA_ZAPIT_DELETE, true, "", zexec /*new CZapitDestExec*/, "delete"));
	zapit_menu->addItem(new CMenuForwarder(LOCALE_EXTRA_ZAPIT_BACKUP, true, "", zexec /*new CZapitDestExec*/, "backup"));
	zapit_menu->addItem(new CMenuForwarder(LOCALE_EXTRA_ZAPIT_RESTORE, true, "", zexec /*new CZapitDestExec*/, "restore"));
#endif

	service.addIntroItems();
	
	service.addItem(new CMenuForwarder(LOCALE_SERVICEMENU_SCANTS    , true, NULL, CScanSetup::getInstance(), "", CRCInput::RC_red, NEUTRINO_ICON_BUTTON_RED) );
	// service.addItem(new CMenuForwarder(LOCALE_EXTRA_ZAPIT_MENU      , true, NULL, zapit_menu, NULL, CRCInput::RC_green, NEUTRINO_ICON_BUTTON_GREEN));
	service.addItem(new CMenuForwarder(LOCALE_SERVICEMENU_RELOAD    , true, NULL, CScanSetup::getInstance(), "reloadchannels", CRCInput::RC_green, NEUTRINO_ICON_BUTTON_GREEN ));
	service.addItem(new CMenuForwarder(LOCALE_BOUQUETEDITOR_NAME    , true, NULL, new CBEBouquetWidget(), NULL, CRCInput::RC_yellow, NEUTRINO_ICON_BUTTON_YELLOW ));

	CDataResetNotifier * resetNotifier = new CDataResetNotifier();
	service.addItem(new CMenuForwarder(LOCALE_RESET_CHANNELS    , true, NULL, resetNotifier, "channels", CRCInput::RC_blue, NEUTRINO_ICON_BUTTON_BLUE));

	service.addItem(GenericMenuSeparatorLine);
	service.addItem(new CMenuForwarder(LOCALE_SERVICEMENU_RESTART   , true, NULL, this, "restart", CRCInput::RC_standby, NEUTRINO_ICON_BUTTON_POWER));
	service.addItem(new CMenuForwarder(LOCALE_SERVICEMENU_GETPLUGINS, true, NULL, this, "reloadplugins"));
	
// 	// start infomenu in service
 	if (g_settings.show_infomenu == 1) 
 		service.addItem(new CMenuForwarder(LOCALE_MESSAGEBOX_INFO, true, NULL, new CInfoMenu(), NULL, CRCInput::RC_info, NEUTRINO_ICON_BUTTON_INFO_SMALL ));
	
	// end of infomenu in service
	//softupdate
	service.addItem(new CMenuForwarder(LOCALE_SERVICEMENU_UPDATE, true, NULL, new CSoftwareUpdate()));
}



