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

//#include "gui/widget/stringinput.h"
//#include "gui/widget/stringinput_ext.h"

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

	mainMenu.addItem(new CMenuForwarder(LOCALE_MAINMENU_TVMODE, true, NULL, this, "tv", CRCInput::RC_red, NEUTRINO_ICON_BUTTON_RED), true);
	mainMenu.addItem(new CMenuForwarder(LOCALE_MAINMENU_RADIOMODE, true, NULL, this, "radio", CRCInput::RC_green, NEUTRINO_ICON_BUTTON_GREEN));
	//mainMenu.addItem(new CMenuForwarder(LOCALE_MAINMENU_SCARTMODE, true, NULL, this, "scart", CRCInput::RC_yellow, NEUTRINO_ICON_BUTTON_YELLOW));
	if (g_PluginList->hasPlugin(CPlugins::P_TYPE_GAME))
		mainMenu.addItem(new CMenuForwarder(LOCALE_MAINMENU_GAMES, true, NULL, new CPluginList(LOCALE_MAINMENU_GAMES,CPlugins::P_TYPE_GAME), "", CRCInput::RC_yellow, NEUTRINO_ICON_BUTTON_YELLOW));

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
		mainMenu.addItem(new CMenuForwarder(LOCALE_MESSAGEBOX_INFO, true, NULL, new CInfoMenu(), NULL, CRCInput::RC_help, NEUTRINO_ICON_BUTTON_HELP_SMALL ));
	
	// end of infomenu
	/*if(system_rev != 10)*/
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
 		service.addItem(new CMenuForwarder(LOCALE_MESSAGEBOX_INFO, true, NULL, new CInfoMenu(), NULL, CRCInput::RC_help, NEUTRINO_ICON_BUTTON_HELP_SMALL ));
	
	// end of infomenu in service
	//softupdate
	service.addItem(new CMenuForwarder(LOCALE_SERVICEMENU_UPDATE, true, NULL, new CSoftwareUpdate()));
}


// USERMENU
// leave this functions, somebody might want to use it in the future again
void CNeutrinoApp::SelectNVOD()
{
	if (!(g_RemoteControl->subChannels.empty()))
	{
		// NVOD/SubService- Kanal!
		CMenuWidget NVODSelector(g_RemoteControl->are_subchannels ? LOCALE_NVODSELECTOR_SUBSERVICE : LOCALE_NVODSELECTOR_HEAD, NEUTRINO_ICON_VIDEO);
		if (getNVODMenu(&NVODSelector))
			NVODSelector.exec(NULL, "");
	}
}

bool CNeutrinoApp::getNVODMenu(CMenuWidget* menu)
{
	if (menu == NULL)
		return false;
	if (g_RemoteControl->subChannels.empty())
		return false;

	menu->addItem(GenericMenuSeparator);

	int count = 0;
	char nvod_id[5];

	for ( CSubServiceListSorted::iterator e=g_RemoteControl->subChannels.begin(); e!=g_RemoteControl->subChannels.end(); ++e)
	{
		sprintf(nvod_id, "%d", count);

		t_channel_id subid = e->getChannelID();
		bool enabled = CRecordManager::getInstance()->SameTransponder(subid);

		if ( !g_RemoteControl->are_subchannels ) {
			char nvod_time_a[50], nvod_time_e[50], nvod_time_x[50];
			char nvod_s[100];
			struct  tm *tmZeit;

			tmZeit= localtime(&e->startzeit);
			sprintf(nvod_time_a, "%02d:%02d", tmZeit->tm_hour, tmZeit->tm_min);

			time_t endtime = e->startzeit+ e->dauer;
			tmZeit= localtime(&endtime);
			sprintf(nvod_time_e, "%02d:%02d", tmZeit->tm_hour, tmZeit->tm_min);

			time_t jetzt=time(NULL);
			if (e->startzeit > jetzt) {
				int mins=(e->startzeit- jetzt)/ 60;
				sprintf(nvod_time_x, g_Locale->getText(LOCALE_NVOD_STARTING), mins);
			}
			else if ( (e->startzeit<= jetzt) && (jetzt < endtime) ) {
				int proz=(jetzt- e->startzeit)*100/ e->dauer;
				sprintf(nvod_time_x, g_Locale->getText(LOCALE_NVOD_PERCENTAGE), proz);
			}
			else
				nvod_time_x[0]= 0;

			sprintf(nvod_s, "%s - %s %s", nvod_time_a, nvod_time_e, nvod_time_x);
			menu->addItem(new CMenuForwarderNonLocalized(nvod_s, enabled, NULL, NVODChanger, nvod_id), (count == g_RemoteControl->selected_subchannel));
		} else {
			menu->addItem(new CMenuForwarderNonLocalized(e->subservice_name.c_str(), enabled, NULL, NVODChanger, nvod_id, CRCInput::convertDigitToKey(count)), (count == g_RemoteControl->selected_subchannel));
		}

		count++;
	}

	if ( g_RemoteControl->are_subchannels ) {
		menu->addItem(GenericMenuSeparatorLine);
		CMenuOptionChooser* oj = new CMenuOptionChooser(LOCALE_NVODSELECTOR_DIRECTORMODE, &g_RemoteControl->director_mode, OPTIONS_OFF0_ON1_OPTIONS, OPTIONS_OFF0_ON1_OPTION_COUNT, true, NULL, CRCInput::RC_yellow, NEUTRINO_ICON_BUTTON_YELLOW);
		menu->addItem(oj);
	}
	return true;
}

void CNeutrinoApp::SelectAPID()
{
#if 0
	if ( g_RemoteControl->current_PIDs.APIDs.size()> 1 )
	{
		// we have APIDs for this channel!

		CMenuWidget APIDSelector(LOCALE_APIDSELECTOR_HEAD, NEUTRINO_ICON_AUDIO);
		APIDSelector.addItem(GenericMenuSeparator);

		for ( unsigned int count=0; count<g_RemoteControl->current_PIDs.APIDs.size(); count++ )
		{
			char apid[5];
			sprintf(apid, "%d", count);
			APIDSelector.addItem(new CMenuForwarderNonLocalized(g_RemoteControl->current_PIDs.APIDs[count].desc, true, NULL, APIDChanger, apid, CRCInput::convertDigitToKey(count + 1)), (count == g_RemoteControl->current_PIDs.PIDs.selected_apid));
		}
		APIDSelector.exec(NULL, "");
	}
#endif
}

#define MAINMENU_RECORDING_OPTION_COUNT 2
const CMenuOptionChooser::keyval MAINMENU_RECORDING_OPTIONS[MAINMENU_RECORDING_OPTION_COUNT] =
{
	{ 0, LOCALE_MAINMENU_RECORDING_START },
	{ 1, LOCALE_MAINMENU_RECORDING_STOP  }
};

// USERMENU
typedef struct user_menu_data_t
{
	neutrino_locale_t caption;
	const neutrino_msg_t key_helper_msg_def;
	const char * key_helper_icon_def;
	const char * menu_icon_def;
	int selected;
} user_menu_data_struct;

#define BUTTONMAX SNeutrinoSettings::BUTTON_MAX
static user_menu_data_t user_menu[BUTTONMAX]=
{
	{LOCALE_INFOVIEWER_EVENTLIST	, CRCInput::RC_red	, NEUTRINO_ICON_BUTTON_RED	, NEUTRINO_ICON_RED,	-1},
	{LOCALE_INFOVIEWER_LANGUAGES	, CRCInput::RC_green	, NEUTRINO_ICON_BUTTON_GREEN	, NEUTRINO_ICON_GREEN, 	-1},
	{NONEXISTANT_LOCALE		, CRCInput::RC_yellow	, NEUTRINO_ICON_BUTTON_YELLOW	, NEUTRINO_ICON_YELLOW,	-1},
	{LOCALE_INFOVIEWER_STREAMINFO	, CRCInput::RC_blue	, NEUTRINO_ICON_BUTTON_BLUE	, NEUTRINO_ICON_FEATURES, -1}
};

// This is just a quick helper for the usermenu only. I already made it a class for future use.
class CKeyHelper
{
private:
	int number_key;
	bool color_key_used[BUTTONMAX];
public:
	CKeyHelper() {
		reset();
	};
	void reset(void)
	{
		number_key = 1;
		for (int i= 0; i < BUTTONMAX; i++ )
			color_key_used[i] = false;
	};

	/* Returns the next available button, to be used in menu as 'direct' keys. Appropriate
	 * definitions are returnd in msp and icon
	 * A color button could be requested as prefered button (other buttons are not supported yet).
	 * If the appropriate button is already in used, the next number_key button is returned instead
	 * (first 1-9 and than 0). */
	bool get(neutrino_msg_t* msg, const char** icon, neutrino_msg_t prefered_key = CRCInput::RC_nokey)
	{
		bool result = false;
		int button = -1;
		if (prefered_key == CRCInput::RC_red)
			button = 0;
		if (prefered_key == CRCInput::RC_green)
			button = 1;
		if (prefered_key == CRCInput::RC_yellow)
			button = 2;
		if (prefered_key == CRCInput::RC_blue)
			button = 3;

		*msg = CRCInput::RC_nokey;
		*icon = "";
		if (button >= 0 && button < BUTTONMAX)
		{ // try to get color button
			if ( color_key_used[button] == false)
			{
				color_key_used[button] = true;
				*msg = user_menu[button].key_helper_msg_def;
				*icon = user_menu[button].key_helper_icon_def;
				result = true;
			}
		}

		if ( result == false && number_key < 10) // no key defined yet, at least try to get a numbered key
		{
			// there is still a available number_key
			*msg = CRCInput::convertDigitToKey(number_key);
			*icon = "";
			if (number_key == 9)
				number_key = 0;
			else if (number_key == 0)
				number_key = 10;
			else
				number_key++;
			result = true;
		}
		return (result);
	};
};

// USERMENU
bool CNeutrinoApp::showUserMenu(int button)
{
	if (button < 0 || button >= BUTTONMAX)
		return false;

	CMenuItem* menu_item = NULL;
	CKeyHelper keyhelper;
	neutrino_msg_t key = CRCInput::RC_nokey;
	const char * icon = NULL;
	int dummy;

	int menu_items = 0;
	int menu_prev = -1;

	// define classes
	CFavorites* tmpFavorites                                = NULL;
	CPauseSectionsdNotifier* tmpPauseSectionsdNotifier      = NULL;
	CAudioSelectMenuHandler* tmpAudioSelectMenuHandler      = NULL;
	CMenuWidget* tmpNVODSelector                            = NULL;
	CStreamInfo2Handler*    tmpStreamInfo2Handler           = NULL;
	CEventListHandler* tmpEventListHandler                  = NULL;
	CEPGplusHandler* tmpEPGplusHandler                      = NULL;
	CEPGDataHandler* tmpEPGDataHandler                      = NULL;
	
	std::string txt = g_settings.usermenu_text[button];
	neutrino_locale_t caption = user_menu[button].caption;
	
	//ensure correct caption for yellow menue
	if ( button == SNeutrinoSettings::BUTTON_YELLOW) 
		caption = g_RemoteControl->are_subchannels ? LOCALE_INFOVIEWER_SUBSERVICE : LOCALE_INFOVIEWER_SELECTTIME;
	
	if ( txt.empty() )
		txt = g_Locale->getText(caption);
	
	CMenuWidget *menu = new CMenuWidget(txt.c_str() , user_menu[button].menu_icon_def, 35);
	if (menu == NULL)
		return 0;
	
	menu->setSelected(user_menu[button].selected);
	
	//menu->addIntroItems(NONEXISTANT_LOCALE, NONEXISTANT_LOCALE, CMenuWidget::BTN_TYPE_CANCEL);
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
			menu_items++;
			menu_prev = SNeutrinoSettings::ITEM_FAVORITS;
			tmpFavorites = new CFavorites;
			keyhelper.get(&key,&icon,CRCInput::RC_green);
			menu_item = new CMenuForwarder(LOCALE_FAVORITES_MENUEADD, true, NULL, tmpFavorites, "-1", key, icon);
			menu->addItem(menu_item, false);
			break;

		case SNeutrinoSettings::ITEM_RECORD:
			if (g_settings.recording_type == RECORDING_OFF)
				break;

			menu_items++;
			menu_prev = SNeutrinoSettings::ITEM_RECORD;
			keyhelper.get(&key,&icon,CRCInput::RC_red);
#if 0 //NEW, show menu, how better ?
			menu_item = new CMenuForwarder(LOCALE_MAINMENU_RECORDING, true, NULL, CRecordManager::getInstance(), "-1", key, icon);
#else //OLD, show start/stop chooser
			menu_item = new CMenuOptionChooser(LOCALE_MAINMENU_RECORDING, &CRecordManager::getInstance()->recordingstatus, 
					MAINMENU_RECORDING_OPTIONS, MAINMENU_RECORDING_OPTION_COUNT, true, 
					CRecordManager::getInstance(), key, icon);
#endif
			menu->addItem(menu_item, false);
			//if(has_hdd)
			//	menu->addItem(new CMenuForwarder(LOCALE_EXTRA_AUTO_TO_RECORD, autoshift, NULL, this, "autolink"), false);
			break;

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
			keyhelper.get(&key,&icon,CRCInput::RC_yellow);
			menu_item = new CMenuForwarder(LOCALE_TIMERLIST_NAME, true, NULL, Timerlist, "-1", key, icon);
			menu->addItem(menu_item, false);
			break;

		case SNeutrinoSettings::ITEM_REMOTE:
			menu_items++;
			menu_prev = SNeutrinoSettings::ITEM_REMOTE;
			keyhelper.get(&key,&icon);
			menu_item = new CMenuForwarder(LOCALE_RCLOCK_MENUEADD, true, NULL, this->rcLock, "-1" , key, icon );
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
			tmpPauseSectionsdNotifier = new CPauseSectionsdNotifier;
			keyhelper.get(&key,&icon);
			menu_item = new CMenuOptionChooser(LOCALE_MAINMENU_PAUSESECTIONSD, &dummy, OPTIONS_OFF0_ON1_OPTIONS, OPTIONS_OFF0_ON1_OPTION_COUNT, true, tmpPauseSectionsdNotifier , key, icon );
			menu->addItem(menu_item, false);
			menu_items++;
			keyhelper.get(&key,&icon);
			menu_item = new CMenuForwarder(LOCALE_MAINMENU_CLEARSECTIONSD, true, NULL, this, "clearSectionsd", key,icon);
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
				if (getNVODMenu(tmpNVODSelector)) {
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
			tmpStreamInfo2Handler = new CStreamInfo2Handler();
			keyhelper.get(&key,&icon,CRCInput::RC_blue);
			menu_item = new CMenuForwarder(LOCALE_EPGMENU_STREAMINFO, true, NULL, tmpStreamInfo2Handler    , "-1", key, icon );
			menu->addItem(menu_item, false);
			break;
		case SNeutrinoSettings::ITEM_PLUGIN:
		{
			char id[5];
			int cnt = 0;
			for (unsigned int count = 0; count < (unsigned int) g_PluginList->getNumberOfPlugins(); count++)
			{
				std::string tmp = g_PluginList->getName(count);
				if (g_PluginList->getType(count)== CPlugins::P_TYPE_TOOL && !g_PluginList->isHidden(count) && tmp.find("Teletext") == std::string::npos)
				{
					sprintf(id, "%d", count);
					menu_items++;
					menu_prev = SNeutrinoSettings::ITEM_PLUGIN;

					//keyhelper.get(&key,&icon,CRCInput::RC_blue);
					keyhelper.get(&key,&icon);
					menu_item = new CMenuForwarderNonLocalized(g_PluginList->getName(count), true, NULL, StreamFeaturesChanger, id, key, icon);
					//menu->addItem(menu_item, (cnt == 0));
					menu->addItem(menu_item, 0);
					cnt++;
				}
			}
		}
		break;

		case SNeutrinoSettings::ITEM_VTXT:
			menu_items++;
			menu_prev = SNeutrinoSettings::ITEM_VTXT;
			keyhelper.get(&key,&icon,CRCInput::RC_blue);
			menu_item = new CMenuForwarder(LOCALE_USERMENU_ITEM_VTXT, true, NULL, StreamFeaturesChanger, "teletext", key, icon);
			menu->addItem(menu_item, 0);
			break;
		case SNeutrinoSettings::ITEM_IMAGEINFO:
			menu_items++;
			menu_prev = SNeutrinoSettings::ITEM_IMAGEINFO;
			keyhelper.get(&key,&icon);
			menu->addItem(new CMenuForwarder(LOCALE_SERVICEMENU_IMAGEINFO,  true, NULL, new CImageInfo(), NULL, key, icon ), false);
			break;
		case SNeutrinoSettings::ITEM_BOXINFO:
			menu_items++;
			menu_prev = SNeutrinoSettings::ITEM_BOXINFO;
			keyhelper.get(&key,&icon);
			menu->addItem( new CMenuForwarder(LOCALE_EXTRA_DBOXINFO, true, NULL, new CDBoxInfoWidget, NULL, key, icon));
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

#if 0 // FIXME not supported yet
		case SNeutrinoSettings::ITEM_MOVIEPLAYER_TS:
			menu_items++;
			menu_prev = SNeutrinoSettings::ITEM_MOVIEPLAYER_TS;
			keyhelper.get(&key,&icon,CRCInput::RC_green);
			menu_item = new CMenuForwarder(LOCALE_MOVIEPLAYER_TSPLAYBACK, true, NULL, moviePlayerGui, "tsplayback", key, icon);
			menu->addItem(menu_item, false);
			break;

		case SNeutrinoSettings::ITEM_VTXT:
			for (unsigned int count = 0; count < (unsigned int) g_PluginList->getNumberOfPlugins(); count++)
			{
				std::string tmp = g_PluginList->getName(count);
				if (g_PluginList->getType(count)== CPlugins::P_TYPE_TOOL && !g_PluginList->isHidden(count) && tmp.find("Teletext") != std::string::npos)
				{
					sprintf(id, "%d", count);
					menu_items++;
					menu_prev = SNeutrinoSettings::ITEM_VTXT;

					//keyhelper.get(&key,&icon,CRCInput::RC_blue);
					keyhelper.get(&key,&icon);
					menu_item = new CMenuForwarderNonLocalized(g_PluginList->getName(count), true, NULL, StreamFeaturesChanger, id, key, icon);
					menu->addItem(menu_item, 0);
				}
			}
			break;
#endif
		default:
			printf("[neutrino] WARNING! menu wrong item!!\n");
			break;
		}
	}

	// Allow some tailoring for privat image bakers ;)
	if (button == SNeutrinoSettings::BUTTON_RED) {
	}
	else if ( button == SNeutrinoSettings::BUTTON_GREEN) {
	}
	else if ( button == SNeutrinoSettings::BUTTON_YELLOW) {
	}
	else if ( button == SNeutrinoSettings::BUTTON_BLUE) {
#ifdef _EXPERIMENTAL_SETTINGS_
		//Experimental Settings
		if (menu_prev != -1)
			menu->addItem(GenericMenuSeparatorLine);
		menu_items ++;
		menu_key++;
		// FYI: there is a memory leak with 'new CExperimentalSettingsMenuHandler()
		menu_item = new CMenuForwarder(LOCALE_EXPERIMENTALSETTINGS, true, NULL, new CExperimentalSettingsMenuHandler(), "-1", CRCInput::convertDigitToKey(menu_key));
		menu->addItem(menu_item, false);
#endif
	}

	// show menu if there are more than 2 items only
	// otherwise, we start the item directly (must be the last one)
	if (menu_items > 1 ) {
		menu->exec(NULL,"");
	}
	else if (menu_item != NULL)
		menu_item->exec( NULL );
	
	user_menu[button].selected = menu->getSelected();

	// restore mute symbol
	//AudioMute(current_muted, true);

	// clear the heap
	if (tmpFavorites)                delete tmpFavorites;
	if (tmpPauseSectionsdNotifier)   delete tmpPauseSectionsdNotifier;
	if (tmpAudioSelectMenuHandler)   delete tmpAudioSelectMenuHandler;
	if (tmpNVODSelector)             delete tmpNVODSelector;
	if (tmpStreamInfo2Handler)       delete tmpStreamInfo2Handler;
	if (tmpEventListHandler)         delete tmpEventListHandler;
	if (tmpEPGplusHandler)           delete tmpEPGplusHandler;
	if (tmpEPGDataHandler)           delete tmpEPGDataHandler;
	if (menu)                        delete menu;
	return 0;
}

