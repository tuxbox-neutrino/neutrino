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

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <dlfcn.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/statvfs.h>
#include <sys/vfs.h>

#include <sys/socket.h>

#include <iostream>
#include <fstream>
#include <string>


#include "global.h"
#include "neutrino.h"

#include <daemonc/remotecontrol.h>

#include <driver/encoding.h>
#include <driver/framebuffer.h>
#include <driver/fontrenderer.h>
#include <driver/rcinput.h>
#include <driver/stream2file.h>
#include <driver/vcrcontrol.h>
#include <driver/shutdown_count.h>

#include <gui/epgplus.h>
#include <gui/streaminfo2.h>

#include "gui/alphasetup.h"
#include "gui/audio_setup.h"
#include "gui/audioplayer.h"
#include "gui/bedit/bouqueteditor_bouquets.h"
#include "gui/bouquetlist.h"
#include "gui/channellist.h"
#include "gui/cec_setup.h"
#include "gui/color.h"
#include "gui/customcolor.h"
#include "gui/epg_menu.h"
#include "gui/epgview.h"
#include "gui/eventlist.h"
#include "gui/favorites.h"
#include "gui/filebrowser.h"
#include "gui/imageinfo.h"
#include "gui/infoviewer.h"
#include "gui/keybind_setup.h"
#include "gui/mediaplayer.h"
#include "gui/motorcontrol.h"
#include "gui/movieplayer.h"
#include "gui/network_setup.h"
#include "gui/osd_setup.h"
#include "gui/osdlang_setup.h"
#include "gui/parentallock_setup.h"
#include "gui/pictureviewer.h"
#include "gui/pluginlist.h"
#include "gui/plugins.h"
#include "gui/rc_lock.h"
#include "gui/record_setup.h"
#include "gui/scan.h"
#include "gui/sleeptimer.h"
#ifdef TEST_MENU
#include "gui/test_menu.h"
#endif /*TEST_MENU*/
#include "gui/timerlist.h"
#include "gui/update.h"
#include "gui/user_menue_setup.h"
#include "gui/user_menue_setup.h"
#include "gui/vfd_setup.h"
#include "gui/videosettings.h"

#include "gui/widget/colorchooser.h"
#include "gui/widget/hintbox.h"
#include "gui/widget/icons.h"
#include "gui/widget/keychooser.h"
#include "gui/widget/menue.h"
#include "gui/widget/messagebox.h"
#include "gui/widget/mountchooser.h"
#include "gui/widget/stringinput.h"
#include "gui/widget/stringinput_ext.h"

#include <system/setting_helpers.h>
#include <system/settings.h>
#include <system/debug.h>
#include <system/flashtool.h>
#include <system/fsmounter.h>

#include <timerdclient/timerdmsg.h>
#include <video.h>
#include <audio.h>
#include <zapit/frontend_c.h>
#include <zapit/getservices.h>

#include <string.h>
#include "gui/infoclock.h"
#include "mymenu.h"
#include "gui/dboxinfo.h"
#include "gui/hdd_menu.h"
#include "gui/audio_select.h"
#include "gui/cam_menu.h"

#include <zapit/getservices.h>
#include <zapit/satconfig.h>
#include <zapit/fastscan.h>
#include <cs_api.h>



extern CFrontend * frontend;
extern CAudioPlayerGui * audioPlayer;
extern CPlugins       * g_PluginList;
extern bool has_hdd;
extern CZapitClient::SatelliteList satList;
extern Zapit_config zapitCfg;
extern char zapit_lat[20];
extern char zapit_long[20];
extern char current_timezone[50];
extern cVideo *videoDecoder;
extern cAudio *audioDecoder;
// extern bool parentallocked;
//extern const char * locale_real_names[];
//extern CFontSizeNotifier fontsizenotifier;
extern CFanControlNotifier * funNotifier;
extern CRemoteControl * g_RemoteControl;
extern CCAMMenuHandler * g_CamHandler;
extern bool autoshift;

//extern int sectionsd_scanning;

//static CTimingSettingsNotifier timingsettingsnotifier;

//CMenuOptionStringChooser* tzSelect;
/**************************************************************************************
*          CNeutrinoApp -  init main menu                                             *
**************************************************************************************/
void CNeutrinoApp::InitMainMenu(CMenuWidget &mainMenu, CMenuWidget &mainSettings, CMenuWidget &miscSettings,
				CMenuWidget &service, CMenuWidget &audiopl_picSettings, CMenuWidget &/*streamingSettings*/)
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
 	mainMenu.addItem(new CMenuForwarder(LOCALE_MAINMENU_MEDIA, true, NULL, new CMediaPlayerMenu(), NULL, CRCInput::RC_blue, NEUTRINO_ICON_BUTTON_BLUE));


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
	if (g_settings.show_infomenu == 0) {
		CMenuWidget *info = new CMenuWidget(LOCALE_MESSAGEBOX_INFO, NEUTRINO_ICON_INFO);
		info->addIntroItems();
		info->addItem(new CMenuForwarder(LOCALE_SERVICEMENU_IMAGEINFO,  true, NULL, new CImageInfo(), NULL, CRCInput::RC_red, NEUTRINO_ICON_BUTTON_RED ), false);
		info->addItem( new CMenuForwarder(LOCALE_DBOXINFO, true, NULL, new CDBoxInfoWidget, NULL, CRCInput::RC_green, NEUTRINO_ICON_BUTTON_GREEN));
		info->addItem(new CMenuForwarder(LOCALE_STREAMINFO_HEAD, true, NULL, new CStreamInfo2Handler(), NULL, CRCInput::RC_yellow, NEUTRINO_ICON_BUTTON_YELLOW));
		mainMenu.addItem(new CMenuForwarder(LOCALE_MESSAGEBOX_INFO, true, NULL, info, NULL, CRCInput::RC_help, NEUTRINO_ICON_BUTTON_HELP_SMALL ));
	}
	// end of infomenu
	/*if(system_rev != 10)*/
		mainMenu.addItem( new CMenuForwarder(LOCALE_CI_SETTINGS, true, NULL, g_CamHandler, NULL, CRCInput::convertDigitToKey(0)));

	//settings menu
	int sett_count =1;
	mainSettings.addIntroItems();

	mainSettings.addItem(new CMenuForwarder(LOCALE_MAINSETTINGS_SAVESETTINGSNOW, true, NULL, this, "savesettings", CRCInput::RC_red, NEUTRINO_ICON_BUTTON_RED));

	CDataResetNotifier * resetNotifier = new CDataResetNotifier();
	CMenuWidget * mset = new CMenuWidget(LOCALE_MAINSETTINGS_MANAGE, NEUTRINO_ICON_SETTINGS);
	mset->addIntroItems();

	mset->addItem(new CMenuForwarder(LOCALE_RESET_SETTINGS,   true, NULL, resetNotifier, "settings"));
	mset->addItem(new CMenuForwarder(LOCALE_EXTRA_LOADCONFIG, true, NULL, this, "loadconfig"));
	mset->addItem(new CMenuForwarder(LOCALE_EXTRA_SAVECONFIG, true, NULL, this, "saveconfig"));
	mset->addItem(new CMenuForwarder(LOCALE_SETTINGS_BACKUP,  true, NULL, this, "backup"));
	mset->addItem(new CMenuForwarder(LOCALE_SETTINGS_RESTORE, true, NULL, this, "restore"));
	mset->addItem(new CMenuForwarder(LOCALE_RESET_ALL,   true, NULL, resetNotifier, "all"));

	mainSettings.addItem(new CMenuForwarder(LOCALE_MAINSETTINGS_MANAGE, true, NULL, mset));

	//mainSettings.addItem(new CMenuForwarder(LOCALE_RESET_SETTINGS    , true, NULL, resetNotifier, "settings"));

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
	mainSettings.addItem(new CMenuForwarder(LOCALE_AUDIOPLAYERPICSETTINGS_GENERAL , true, NULL, &audiopl_picSettings   , NULL, CRCInput::RC_yellow, NEUTRINO_ICON_BUTTON_YELLOW));
	mainSettings.addItem(new CMenuForwarder(LOCALE_MAINSETTINGS_MISC      , true, NULL, &miscSettings     , NULL, CRCInput::RC_blue , NEUTRINO_ICON_BUTTON_BLUE ));
	
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

#define SCANTS_BOUQUET_OPTION_COUNT 3
const CMenuOptionChooser::keyval SCANTS_BOUQUET_OPTIONS[SCANTS_BOUQUET_OPTION_COUNT] =
{
	{ CZapitClient::BM_DELETEBOUQUETS        , LOCALE_SCANTS_BOUQUET_ERASE     },
	/*{ CZapitClient::BM_CREATEBOUQUETS        , LOCALE_SCANTS_BOUQUET_CREATE    },*/
	{ CZapitClient::BM_DONTTOUCHBOUQUETS     , LOCALE_SCANTS_BOUQUET_LEAVE     },
	{ CZapitClient::BM_UPDATEBOUQUETS        , LOCALE_SCANTS_BOUQUET_UPDATE    }
	/*{ CZapitClient::BM_CREATESATELLITEBOUQUET, LOCALE_SCANTS_BOUQUET_SATELLITE }*/
};

#define SCANTS_ZAPIT_SCANTYPE_COUNT 4
const CMenuOptionChooser::keyval SCANTS_ZAPIT_SCANTYPE[SCANTS_ZAPIT_SCANTYPE_COUNT] =
{
	{  CZapitClient::ST_TVRADIO	, LOCALE_ZAPIT_SCANTYPE_TVRADIO     },
	{  CZapitClient::ST_TV		, LOCALE_ZAPIT_SCANTYPE_TV    },
	{  CZapitClient::ST_RADIO	, LOCALE_ZAPIT_SCANTYPE_RADIO     },
	{  CZapitClient::ST_ALL		, LOCALE_ZAPIT_SCANTYPE_ALL }
};

#define SATSETUP_DISEQC_OPTION_COUNT 6
const CMenuOptionChooser::keyval SATSETUP_DISEQC_OPTIONS[SATSETUP_DISEQC_OPTION_COUNT] =
{
	{ NO_DISEQC,		LOCALE_SATSETUP_NODISEQC },
	{ MINI_DISEQC,		LOCALE_SATSETUP_MINIDISEQC },
	{ DISEQC_1_0,		LOCALE_SATSETUP_DISEQC10 },
	{ DISEQC_1_1,		LOCALE_SATSETUP_DISEQC11 },
	/*{ DISEQC_1_2,	LOCALE_SATSETUP_DISEQC12 },*/
	{ DISEQC_ADVANCED,	LOCALE_SATSETUP_DISEQ_ADVANCED },
	{ SMATV_REMOTE_TUNING,	LOCALE_SATSETUP_SMATVREMOTE }
};

#define SATSETUP_SCANTP_FEC_COUNT 21
#define CABLESETUP_SCANTP_FEC_COUNT 5
const CMenuOptionChooser::keyval SATSETUP_SCANTP_FEC[SATSETUP_SCANTP_FEC_COUNT] =
{
	{ FEC_AUTO, LOCALE_SCANTP_FEC_AUTO },
	{ FEC_S2_AUTO, LOCALE_SCANTP_FEC_AUTO_S2 },

	{ FEC_1_2, LOCALE_SCANTP_FEC_1_2 },
	{ FEC_2_3, LOCALE_SCANTP_FEC_2_3 },
	{ FEC_3_4, LOCALE_SCANTP_FEC_3_4 },
	{ FEC_5_6, LOCALE_SCANTP_FEC_5_6 },
	{ FEC_7_8, LOCALE_SCANTP_FEC_7_8 },

	{ FEC_S2_QPSK_1_2, LOCALE_FEC_S2_QPSK_1_2 },
	{ FEC_S2_QPSK_2_3, LOCALE_FEC_S2_QPSK_2_3 },
	{ FEC_S2_QPSK_3_4, LOCALE_FEC_S2_QPSK_3_4 },
	{ FEC_S2_QPSK_5_6, LOCALE_FEC_S2_QPSK_5_6 },
	//{ FEC_S2_QPSK_7_8, LOCALE_FEC_S2_QPSK_7_8 },
	{ FEC_S2_QPSK_8_9, LOCALE_FEC_S2_QPSK_8_9 },
	{ FEC_S2_QPSK_3_5, LOCALE_FEC_S2_QPSK_3_5 },
	{ FEC_S2_QPSK_4_5, LOCALE_FEC_S2_QPSK_4_5 },
	{ FEC_S2_QPSK_9_10, LOCALE_FEC_S2_QPSK_9_10 },

	//{ FEC_S2_8PSK_1_2, LOCALE_FEC_S2_8PSK_1_2 },
	{ FEC_S2_8PSK_2_3, LOCALE_FEC_S2_8PSK_2_3 },
	{ FEC_S2_8PSK_3_4, LOCALE_FEC_S2_8PSK_3_4 },
	{ FEC_S2_8PSK_3_5, LOCALE_FEC_S2_8PSK_3_5 },
	{ FEC_S2_8PSK_5_6, LOCALE_FEC_S2_8PSK_5_6 },
	//{ FEC_S2_8PSK_7_8, LOCALE_FEC_S2_8PSK_7_8 },
	{ FEC_S2_8PSK_8_9, LOCALE_FEC_S2_8PSK_8_9 },
	//{ FEC_S2_8PSK_4_5, LOCALE_FEC_S2_8PSK_4_5 },
	{ FEC_S2_8PSK_9_10, LOCALE_FEC_S2_8PSK_9_10 }
};

#define SATSETUP_SCANTP_MOD_COUNT 5
const CMenuOptionChooser::keyval SATSETUP_SCANTP_MOD[SATSETUP_SCANTP_MOD_COUNT] =
{
	{ 1, LOCALE_SCANTP_MOD_16 },
	{ 2, LOCALE_SCANTP_MOD_32 },
	{ 3, LOCALE_SCANTP_MOD_64 },
	{ 4, LOCALE_SCANTP_MOD_128},
	{ 5, LOCALE_SCANTP_MOD_256}
};
#define SATSETUP_SCANTP_POL_COUNT 2
const CMenuOptionChooser::keyval SATSETUP_SCANTP_POL[SATSETUP_SCANTP_POL_COUNT] =
{
	{ 0, LOCALE_EXTRA_POL_H },
	{ 1, LOCALE_EXTRA_POL_V }
};

#if 0
/*Cable*/
#define CABLESETUP_SCANTP_MOD_COUNT 7
const CMenuOptionChooser::keyval CABLESETUP_SCANTP_MOD[CABLESETUP_SCANTP_MOD_COUNT] =
{
	{0, LOCALE_SCANTP_MOD_QPSK     } ,
	{1, LOCALE_SCANTP_MOD_QAM_16   } ,
	{2, LOCALE_SCANTP_MOD_QAM_32   } ,
	{3, LOCALE_SCANTP_MOD_QAM_64   } ,
	{4, LOCALE_SCANTP_MOD_QAM_128  } ,
	{5, LOCALE_SCANTP_MOD_QAM_256  } ,
	{6, LOCALE_SCANTP_MOD_QAM_AUTO }
};
#endif

#define SECTIONSD_SCAN_OPTIONS_COUNT 3
const CMenuOptionChooser::keyval SECTIONSD_SCAN_OPTIONS[SECTIONSD_SCAN_OPTIONS_COUNT] =
{
	{ 0, LOCALE_OPTIONS_OFF },
	{ 1, LOCALE_OPTIONS_ON  },
	{ 2, LOCALE_OPTIONS_ON_WITHOUT_MESSAGES  }
};
#define DISEQC_ORDER_OPTION_COUNT 2
const CMenuOptionChooser::keyval DISEQC_ORDER_OPTIONS[DISEQC_ORDER_OPTION_COUNT] =
{
	{ COMMITED_FIRST, LOCALE_SATSETUP_DISEQC_COM_UNCOM },
	{ UNCOMMITED_FIRST, LOCALE_SATSETUP_DISEQC_UNCOM_COM  }
};

class CTPSelectHandler : public CMenuTarget
{
public:
	int exec(CMenuTarget* parent,  const std::string &actionkey);
};

extern CZapitChannel *g_current_channel;
extern std::map<transponder_id_t, transponder> select_transponders;
int CTPSelectHandler::exec(CMenuTarget* parent, const std::string &/*actionkey*/)
{
	transponder_list_t::iterator tI;
	sat_iterator_t sit;
	t_satellite_position position = 0;
	std::map<int, transponder> tmplist;
	std::map<int, transponder>::iterator tmpI;
	int i;
	char cnt[5];
	int select = -1;
	static int old_selected = 0;
	static t_satellite_position old_position = 0;

	if (parent)
		parent->hide();

	for (sit = satellitePositions.begin(); sit != satellitePositions.end(); sit++) {
		if (!strcmp(sit->second.name.c_str(), CNeutrinoApp::getInstance()->getScanSettings().satNameNoDiseqc)) {
			position = sit->first;
			break;
		}
	}
	if (old_position != position) {
		old_selected = 0;
		old_position = position;
	}

	CMenuWidget* menu = new CMenuWidget(LOCALE_SCANTS_SELECT_TP, NEUTRINO_ICON_SETTINGS);
	CMenuSelectorTarget * selector = new CMenuSelectorTarget(&select);
	menu->addIntroItems(NONEXISTANT_LOCALE, NONEXISTANT_LOCALE, CMenuWidget::BTN_TYPE_CANCEL); //add cancel button, ensures that we have enought space left from item caption
	i = 0;
	for (tI = select_transponders.begin(); tI != select_transponders.end(); tI++) {
		t_satellite_position satpos = GET_SATELLITEPOSITION_FROM_TRANSPONDER_ID(tI->first) & 0xFFF;
		if (GET_SATELLITEPOSITION_FROM_TRANSPONDER_ID(tI->first) & 0xF000)
			satpos = -satpos;
		if (satpos != position)
			continue;

		char buf[128];
		sprintf(cnt, "%d", i);
		char * f, *s, *m;
		switch (frontend->getInfo()->type) {
		case FE_QPSK:
			frontend->getDelSys(tI->second.feparams.u.qpsk.fec_inner, dvbs_get_modulation(tI->second.feparams.u.qpsk.fec_inner),  f, s, m);
			snprintf(buf, sizeof(buf), "%d %c %d %s %s %s ", tI->second.feparams.frequency/1000, tI->second.polarization ? 'V' : 'H', tI->second.feparams.u.qpsk.symbol_rate/1000, f, s, m);
			break;
		case FE_QAM:
			frontend->getDelSys(tI->second.feparams.u.qam.fec_inner, tI->second.feparams.u.qam.modulation, f, s, m);
			snprintf(buf, sizeof(buf), "%d %d %s %s %s ", tI->second.feparams.frequency/1000, tI->second.feparams.u.qam.symbol_rate/1000, f, s, m);
			break;
		case FE_OFDM:
		case FE_ATSC:
			break;
		}
		if(!old_selected && g_current_channel && g_current_channel->getSatellitePosition() == position) {
			if(g_current_channel->getFreqId() == GET_FREQ_FROM_TPID(tI->first)) {
				old_selected = i;
			}
		}

		//menu->addItem(new CMenuForwarderNonLocalized(buf, true, NULL, selector, cnt), old_selected == i);

		CMenuForwarderNonLocalized * ts_item = new CMenuForwarderNonLocalized(buf, true, NULL, selector, NULL, CRCInput::RC_nokey, NULL)/*, false*/;
		ts_item->setItemButton(NEUTRINO_ICON_BUTTON_OKAY, true);
		menu->addItem(ts_item);

		tmplist.insert(std::pair <int, transponder>(i, tI->second));
		i++;
	}
	if (i == 0) {
		std::string text = "No transponders found for ";
		text += CNeutrinoApp::getInstance()->getScanSettings().satNameNoDiseqc;
		ShowHintUTF(LOCALE_MESSAGEBOX_ERROR, text.c_str(), 450, 2);
		return menu_return::RETURN_REPAINT;
	}
	menu->setSelected(old_selected);
	int retval = menu->exec(NULL, "");
	delete menu;
	delete selector;
	if (select >= 0) {
		old_selected = select;

		tmpI = tmplist.find(select);
		printf("CTPSelectHandler::exec: selected TP: freq %d pol %d SR %d\n", tmpI->second.feparams.frequency,
		       tmpI->second.polarization, tmpI->second.feparams.u.qpsk.symbol_rate);
		sprintf(get_set.TP_freq, "%d", tmpI->second.feparams.frequency);
		switch (frontend->getInfo()->type) {
		case FE_QPSK:
			sprintf(get_set.TP_rate, "%d", tmpI->second.feparams.u.qpsk.symbol_rate);
			get_set.TP_fec = tmpI->second.feparams.u.qpsk.fec_inner;
			get_set.TP_pol = tmpI->second.polarization;
			break;
		case FE_QAM:
			sprintf(get_set.TP_rate, "%d", tmpI->second.feparams.u.qam.symbol_rate);
			get_set.TP_fec = tmpI->second.feparams.u.qam.fec_inner;
			get_set.TP_mod = tmpI->second.feparams.u.qam.modulation;
			break;
		case FE_OFDM:
		case FE_ATSC:
			break;
		}

	}
	if (retval == menu_return::RETURN_EXIT_ALL)
		return menu_return::RETURN_EXIT_ALL;

	return menu_return::RETURN_REPAINT;
}

extern int scan_pids;

#define FAST_SCAN_OPTIONS_COUNT 2
const CMenuOptionChooser::keyval FAST_SCAN_OPTIONS[FAST_SCAN_OPTIONS_COUNT] =
{
	{ FAST_SCAN_SD, LOCALE_SATSETUP_FASTSCAN_SD },
        { FAST_SCAN_HD, LOCALE_SATSETUP_FASTSCAN_HD  }
        /*{ FAST_SCAN_ALL, LOCALE_SATSETUP_FASTSCAN_ALL  }*/
};

#define FAST_SCAN_PROV_OPTIONS_COUNT 3
const CMenuOptionChooser::keyval FAST_SCAN_PROV_OPTIONS[FAST_SCAN_PROV_OPTIONS_COUNT] =
{
	{ OPERATOR_CD, LOCALE_SATSETUP_FASTSCAN_PROV_CD },
        { OPERATOR_TVV, LOCALE_SATSETUP_FASTSCAN_PROV_TVV  },
        { OPERATOR_TELESAT, LOCALE_SATSETUP_FASTSCAN_PROV_TELESAT  }
};

void CNeutrinoApp::InitScanSettings(CMenuWidget &settings)
{
	dprintf(DEBUG_DEBUG, "init scansettings\n");
	int sfound = 0;
	int dmode = scanSettings.diseqcMode;
	int shortcut = 1;
	static int all_usals = 1;
	
	//scale to max sat/cable name lenght
	unsigned int sat_txt_w = 0, max_txt_w = 0;
	sat_iterator_t sit;
	if (g_info.delivery_system == DVB_S) {
		for (sit = satellitePositions.begin(); sit != satellitePositions.end(); sit++) {
			sat_txt_w = g_Font[SNeutrinoSettings::FONT_TYPE_MENU]->getRenderWidth(sit->second.name.c_str(), true);
			max_txt_w = std::max(max_txt_w,sat_txt_w);
		}
	} else if (g_info.delivery_system == DVB_C) {
		for (sit = satellitePositions.begin(); sit != satellitePositions.end(); sit++) {
			sat_txt_w = g_Font[SNeutrinoSettings::FONT_TYPE_MENU]->getRenderWidth(sit->second.name.c_str(), true);
			max_txt_w = std::max(max_txt_w,sat_txt_w);
		}
	}
	int w, h;
	const unsigned int mini_w = 30;//mini width 30%
	frameBuffer->getIconSize(NEUTRINO_ICON_BUTTON_RED, &w, &h);
	max_txt_w += (w*2) +30;
	max_txt_w = std::min(max_txt_w, frameBuffer->getScreenWidth());
	max_txt_w = max_txt_w*100/frameBuffer->getScreenWidth();
	max_txt_w = std::max(max_txt_w,mini_w);

	CTPSelectHandler * tpSelect = new CTPSelectHandler();

	CSatelliteSetupNotifier * satNotify = new CSatelliteSetupNotifier();

	CMenuOptionChooser* ojScantype = new CMenuOptionChooser(LOCALE_ZAPIT_SCANTYPE, (int *)&scanSettings.scanType, SCANTS_ZAPIT_SCANTYPE, SCANTS_ZAPIT_SCANTYPE_COUNT, true, NULL, CRCInput::convertDigitToKey(shortcut++), "", true);
	CMenuOptionChooser* ojBouquets = new CMenuOptionChooser(LOCALE_SCANTS_BOUQUET, (int *)&scanSettings.bouquetMode, SCANTS_BOUQUET_OPTIONS, SCANTS_BOUQUET_OPTION_COUNT, true, NULL, CRCInput::convertDigitToKey(shortcut++), "", true);

	CMenuOptionChooser* useNit = new CMenuOptionChooser(LOCALE_SATSETUP_USE_NIT, (int *)&scanSettings.scan_mode, OPTIONS_OFF1_ON0_OPTIONS, OPTIONS_OFF1_ON0_OPTION_COUNT, true, NULL, CRCInput::RC_green, NEUTRINO_ICON_BUTTON_GREEN);
	CMenuOptionChooser* scanPids = new CMenuOptionChooser(LOCALE_EXTRA_ZAPIT_SCANPIDS,  &scan_pids, OPTIONS_OFF0_ON1_OPTIONS, OPTIONS_OFF0_ON1_OPTION_COUNT, true, NULL, CRCInput::RC_yellow, NEUTRINO_ICON_BUTTON_YELLOW);

	// Please lets keep shortcuts which used for a long time - unchanged. -- focus
	//CMenuOptionChooser* ftaFlag = new CMenuOptionChooser(LOCALE_SATSETUP_USE_FTA_FLAG, (int *)&scanSettings.scan_fta_flag, OPTIONS_OFF0_ON1_OPTIONS, OPTIONS_OFF1_ON0_OPTION_COUNT, true, NULL, CRCInput::RC_blue, NEUTRINO_ICON_BUTTON_BLUE);
	CMenuOptionChooser* ftaFlag = new CMenuOptionChooser(LOCALE_SATSETUP_USE_FTA_FLAG, (int *)&scanSettings.scan_fta_flag, OPTIONS_OFF0_ON1_OPTIONS, OPTIONS_OFF1_ON0_OPTION_COUNT, true, NULL, CRCInput::convertDigitToKey(0));

	CMenuWidget* satSetup = new CMenuWidget(LOCALE_SATSETUP_SAT_SETUP, NEUTRINO_ICON_SETTINGS);
	satSetup->addItem(GenericMenuSeparator);
	satSetup->addItem(GenericMenuBack);

	satSetup->addItem(new CMenuForwarder(LOCALE_MAINSETTINGS_SAVESETTINGSNOW, true, NULL, this, "savescansettings", CRCInput::RC_red, NEUTRINO_ICON_BUTTON_RED));
	satSetup->addItem(GenericMenuSeparatorLine);

	CMenuWidget* satfindMenu = new CMenuWidget(LOCALE_MOTORCONTROL_HEAD, NEUTRINO_ICON_SETTINGS);

	satfindMenu->addIntroItems();

	CMenuOptionStringChooser *satSelect = NULL;
	CMenuWidget* satOnOff = NULL;

	//t_satellite_position currentSatellitePosition = frontend->getCurrentSatellitePosition();
	if (g_info.delivery_system == DVB_S) {
		satSelect = new CMenuOptionStringChooser(LOCALE_SATSETUP_SATELLITE, scanSettings.satNameNoDiseqc, true, NULL, CRCInput::RC_red, NEUTRINO_ICON_BUTTON_RED, true);
		satOnOff = new CMenuWidget(LOCALE_SATSETUP_SATELLITE, NEUTRINO_ICON_SETTINGS);
		satOnOff->addIntroItems();

		for (sit = satellitePositions.begin(); sit != satellitePositions.end(); sit++) {
			/* printf("Adding sat menu for %s position %d\n", sit->second.name.c_str(), sit->first); */

			satSelect->addOption(sit->second.name.c_str());
			if (strcmp(scanSettings.satNameNoDiseqc,sit->second.name.c_str()) == 0) {
				sfound = 1;
			}

			CMenuWidget* tempsat = new CMenuWidget(sit->second.name.c_str(), NEUTRINO_ICON_SETTINGS);
			tempsat->addIntroItems();

			CMenuOptionChooser * inuse = new CMenuOptionChooser(sit->second.name.c_str(),  &sit->second.use_in_scan, OPTIONS_OFF0_ON1_OPTIONS, OPTIONS_OFF0_ON1_OPTION_COUNT, true);
			CMenuOptionNumberChooser * diseqc = new CMenuOptionNumberChooser(LOCALE_SATSETUP_DISEQC_INPUT, &sit->second.diseqc, ((dmode != NO_DISEQC) && (dmode != DISEQC_ADVANCED)), -1, 15, NULL, 1, -1, LOCALE_OPTIONS_OFF);
			CMenuOptionNumberChooser * comm = new CMenuOptionNumberChooser(LOCALE_SATSETUP_COMM_INPUT, &sit->second.commited, dmode == DISEQC_ADVANCED, -1, 15, NULL, 1, -1, LOCALE_OPTIONS_OFF);
			CMenuOptionNumberChooser * uncomm = new CMenuOptionNumberChooser(LOCALE_SATSETUP_UNCOMM_INPUT, &sit->second.uncommited, dmode == DISEQC_ADVANCED, -1, 15, NULL, 1, -1, LOCALE_OPTIONS_OFF);
			//CMenuOptionNumberChooser * motor = new CMenuOptionNumberChooser(LOCALE_SATSETUP_MOTOR_POS, &sit->second.motor_position, dmode == DISEQC_ADVANCED, 0, 64, NULL, 0, 0, LOCALE_OPTIONS_OFF);
			CMenuOptionNumberChooser * motor = new CMenuOptionNumberChooser(LOCALE_SATSETUP_MOTOR_POS, &sit->second.motor_position, true, 0, 64, NULL, 0, 0, LOCALE_OPTIONS_OFF);
			//CMenuOptionChooser * usals = new CMenuOptionChooser(LOCALE_EXTRA_USE_GOTOXX,  &sit->second.use_usals, OPTIONS_OFF0_ON1_OPTIONS, OPTIONS_OFF0_ON1_OPTION_COUNT, dmode == DISEQC_ADVANCED);
			CMenuOptionChooser * usals = new CMenuOptionChooser(LOCALE_EXTRA_USE_GOTOXX,  &sit->second.use_usals, OPTIONS_OFF0_ON1_OPTIONS, OPTIONS_OFF0_ON1_OPTION_COUNT, true);
			if(!sit->second.use_usals)
				all_usals = 0;

			satNotify->addItem(1, diseqc);
			satNotify->addItem(0, comm);
			satNotify->addItem(0, uncomm);
			//satNotify->addItem(0, motor); //FIXME testing motor with not DISEQC_ADVANCED
			//satNotify->addItem(0, usals);

			CIntInput* lofL = new CIntInput(LOCALE_SATSETUP_LOFL, (int&) sit->second.lnbOffsetLow, 5, NONEXISTANT_LOCALE, NONEXISTANT_LOCALE);
			CIntInput* lofH = new CIntInput(LOCALE_SATSETUP_LOFH, (int&) sit->second.lnbOffsetHigh, 5, NONEXISTANT_LOCALE, NONEXISTANT_LOCALE);
			CIntInput* lofS = new CIntInput(LOCALE_SATSETUP_LOFS, (int&) sit->second.lnbSwitch, 5, NONEXISTANT_LOCALE, NONEXISTANT_LOCALE);

			satOnOff->addItem(inuse);
			tempsat->addItem(diseqc);
			tempsat->addItem(comm);
			tempsat->addItem(uncomm);
			tempsat->addItem(motor);
			tempsat->addItem(usals);
			tempsat->addItem(new CMenuForwarder(LOCALE_SATSETUP_LOFL, true, lofL->getValue(), lofL));
			tempsat->addItem(new CMenuForwarder(LOCALE_SATSETUP_LOFH, true, lofH->getValue(), lofH));
			tempsat->addItem(new CMenuForwarder(LOCALE_SATSETUP_LOFS, true, lofS->getValue(), lofS));
			satSetup->addItem(new CMenuForwarderNonLocalized(sit->second.name.c_str(), true, NULL, tempsat));

		}
	} else if (g_info.delivery_system == DVB_C) {
		satSelect = new CMenuOptionStringChooser(LOCALE_CABLESETUP_PROVIDER, (char*)scanSettings.satNameNoDiseqc, true);
		for (sit = satellitePositions.begin(); sit != satellitePositions.end(); sit++) {
			printf("Adding cable menu for %s position %d\n", sit->second.name.c_str(), sit->first);
			satSelect->addOption(sit->second.name.c_str());
			if (strcmp(scanSettings.satNameNoDiseqc,sit->second.name.c_str()) == 0) {
				sfound = 1;
			}
			dprintf(DEBUG_DEBUG, "got scanprovider (cable): %s\n", sit->second.name.c_str());
		}
	}
	satfindMenu->addItem(satSelect);
	satfindMenu->addItem(new CMenuForwarder(LOCALE_SCANTS_SELECT_TP, true, NULL, tpSelect, "test"));

	int freq_length = (g_info.delivery_system == DVB_S) ? 8 : 6;

	CStringInput*		freq = new CStringInput(LOCALE_EXTRA_FREQ, (char *) scanSettings.TP_freq, freq_length, NONEXISTANT_LOCALE, NONEXISTANT_LOCALE, "0123456789");
	CStringInput*		rate = new CStringInput(LOCALE_EXTRA_RATE, (char *) scanSettings.TP_rate, 8, NONEXISTANT_LOCALE, NONEXISTANT_LOCALE, "0123456789");
	CMenuForwarder *	Freq = new CMenuForwarder(LOCALE_EXTRA_FREQ, true, scanSettings.TP_freq, freq, "", CRCInput::convertDigitToKey(1));
	CMenuForwarder *	Rate = new CMenuForwarder(LOCALE_EXTRA_RATE, true, scanSettings.TP_rate, rate, "", CRCInput::convertDigitToKey(2));

	int fec_count = (g_info.delivery_system == DVB_S) ? SATSETUP_SCANTP_FEC_COUNT : CABLESETUP_SCANTP_FEC_COUNT;

	CMenuOptionChooser*	fec = new CMenuOptionChooser(LOCALE_EXTRA_FEC, (int *)&scanSettings.TP_fec, SATSETUP_SCANTP_FEC, fec_count, true, NULL, CRCInput::convertDigitToKey(3), "", true);
	CMenuOptionChooser*	mod_pol = NULL;

	if (g_info.delivery_system == DVB_S)
		mod_pol = new CMenuOptionChooser(LOCALE_EXTRA_POL, (int *)&scanSettings.TP_pol, SATSETUP_SCANTP_POL, SATSETUP_SCANTP_POL_COUNT, true, NULL, CRCInput::convertDigitToKey(4));
	else if (g_info.delivery_system == DVB_C)
		mod_pol = new CMenuOptionChooser(LOCALE_EXTRA_MOD, (int *)&scanSettings.TP_mod, SATSETUP_SCANTP_MOD, SATSETUP_SCANTP_MOD_COUNT, true, NULL, CRCInput::convertDigitToKey(4));

	satfindMenu->addItem(Freq);
	satfindMenu->addItem(Rate);
	satfindMenu->addItem(fec);
	satfindMenu->addItem(mod_pol);

	CMenuWidget* motorMenu = NULL;
	if (g_info.delivery_system == DVB_S) {
		satfindMenu->addItem(new CMenuForwarder(LOCALE_MOTORCONTROL_HEAD, true, NULL, new CMotorControl(), "", CRCInput::RC_blue, NEUTRINO_ICON_BUTTON_BLUE));

		motorMenu = new CMenuWidget(LOCALE_SATSETUP_EXTENDED_MOTOR, NEUTRINO_ICON_SETTINGS);
		motorMenu->addItem(GenericMenuSeparator);
		motorMenu->addItem(GenericMenuBack);
		motorMenu->addItem(new CMenuForwarder(LOCALE_SATSETUP_SAVESETTINGSNOW, true, NULL, this, "savesettings", CRCInput::RC_red, NEUTRINO_ICON_BUTTON_RED));
		//motorMenu->addItem(new CMenuForwarder(LOCALE_SATSETUP_MOTORCONTROL   , true, NULL, new CMotorControl()));
		motorMenu->addItem(new CMenuForwarder(LOCALE_MOTORCONTROL_HEAD, true, NULL, satfindMenu, "", CRCInput::RC_green, NEUTRINO_ICON_BUTTON_GREEN));
		motorMenu->addItem(GenericMenuSeparatorLine);

		motorMenu->addItem(new CMenuOptionNumberChooser(LOCALE_EXTRA_ZAPIT_ROTATION_SPEED, (int *)&zapitCfg.motorRotationSpeed, true, 0, 64, NULL) );
		motorMenu->addItem(new CMenuOptionChooser(LOCALE_EXTRA_ZAPIT_HVOLTAGE,  (int *)&zapitCfg.highVoltage, OPTIONS_OFF0_ON1_OPTIONS, OPTIONS_OFF0_ON1_OPTION_COUNT, true));
		//motorMenu->addItem(new CMenuOptionChooser(LOCALE_EXTRA_USE_GOTOXX,  (int *)&zapitCfg.useGotoXX, OPTIONS_OFF0_ON1_OPTIONS, OPTIONS_OFF0_ON1_OPTION_COUNT, true));

		CStringInput * toff;
		sprintf(zapit_lat, "%02.6f", zapitCfg.gotoXXLatitude);
		sprintf(zapit_long, "%02.6f", zapitCfg.gotoXXLongitude);

		motorMenu->addItem(new CMenuOptionChooser(LOCALE_EXTRA_LADIR,  (int *)&zapitCfg.gotoXXLaDirection, OPTIONS_SOUTH0_NORTH1_OPTIONS, OPTIONS_SOUTH0_NORTH1_OPTION_COUNT, true));
		toff = new CStringInput(LOCALE_EXTRA_LAT, (char *) zapit_lat, 10, NONEXISTANT_LOCALE, NONEXISTANT_LOCALE, "0123456789.");
		motorMenu->addItem(new CMenuForwarder(LOCALE_EXTRA_LAT, true, zapit_lat, toff));

		motorMenu->addItem(new CMenuOptionChooser(LOCALE_EXTRA_LODIR,  (int *)&zapitCfg.gotoXXLoDirection, OPTIONS_EAST0_WEST1_OPTIONS, OPTIONS_EAST0_WEST1_OPTION_COUNT, true));
		toff = new CStringInput(LOCALE_EXTRA_LONG, (char *) zapit_long, 10, NONEXISTANT_LOCALE, NONEXISTANT_LOCALE, "0123456789.");
		motorMenu->addItem(new CMenuForwarder(LOCALE_EXTRA_LONG, true, zapit_long, toff));
		motorMenu->addItem(new CMenuOptionNumberChooser(LOCALE_SATSETUP_USALS_REPEAT, (int *)&zapitCfg.repeatUsals, true, 0, 10, NULL, 0, 0, LOCALE_OPTIONS_OFF) );
		CAllUsalsNotifier * usalsNotify = new CAllUsalsNotifier();
		CMenuOptionChooser * allusals = new CMenuOptionChooser(LOCALE_SATSETUP_USE_USALS,  &all_usals, OPTIONS_OFF0_ON1_OPTIONS, OPTIONS_OFF0_ON1_OPTION_COUNT, true, usalsNotify);
		motorMenu->addItem(allusals);
	}

	if (!sfound && satellitePositions.size()) {
		sit = satellitePositions.begin();
		strcpy(scanSettings.satNameNoDiseqc, sit->second.name.c_str());
	}

	CMenuWidget* manualScan = new CMenuWidget(LOCALE_SATSETUP_MANUAL_SCAN, NEUTRINO_ICON_SETTINGS, max_txt_w);

	CScanTs * scanTs = new CScanTs();

	manualScan->addIntroItems();

	manualScan->addItem(satSelect);
	manualScan->addItem(new CMenuForwarder(LOCALE_SCANTS_SELECT_TP, true, NULL, tpSelect, "test"));
	manualScan->addItem(Freq);
	manualScan->addItem(Rate);
	manualScan->addItem(fec);
	manualScan->addItem(mod_pol);
	manualScan->addItem(useNit);
	manualScan->addItem(ftaFlag);
	manualScan->addItem(GenericMenuSeparatorLine);
	manualScan->addItem(new CMenuForwarder(LOCALE_SCANTS_TEST, true, NULL, scanTs, "test", CRCInput::RC_yellow, NEUTRINO_ICON_BUTTON_YELLOW));
	manualScan->addItem(new CMenuForwarder(LOCALE_SCANTS_STARTNOW, true, NULL, scanTs, "manual", CRCInput::RC_blue, NEUTRINO_ICON_BUTTON_BLUE));

	CMenuWidget* autoScan = new CMenuWidget(LOCALE_SATSETUP_AUTO_SCAN, NEUTRINO_ICON_SETTINGS, max_txt_w);
	autoScan->addIntroItems();
	autoScan->addItem(satSelect);
	autoScan->addItem(useNit);
	autoScan->addItem(scanPids);
	autoScan->addItem(ftaFlag);
	autoScan->addItem(new CMenuForwarder(LOCALE_SCANTS_STARTNOW, true, NULL, scanTs, "auto", CRCInput::RC_blue, NEUTRINO_ICON_BUTTON_BLUE));

	CMenuOptionChooser* ojDiseqc = NULL;
	CMenuOptionNumberChooser * ojDiseqcRepeats = NULL;
	CMenuForwarder *fsatSetup = NULL;
	CMenuForwarder *fmotorMenu = NULL;
	CMenuForwarder *fautoScanAll = NULL;

	if (g_info.delivery_system == DVB_S) {
		ojDiseqc	= new CMenuOptionChooser(LOCALE_SATSETUP_DISEQC, (int *)&scanSettings.diseqcMode, SATSETUP_DISEQC_OPTIONS, SATSETUP_DISEQC_OPTION_COUNT, true, satNotify, CRCInput::convertDigitToKey(shortcut++), "", true);
		ojDiseqcRepeats	= new CMenuOptionNumberChooser(LOCALE_SATSETUP_DISEQCREPEAT, (int *)&scanSettings.diseqcRepeat, (dmode != NO_DISEQC) && (dmode != DISEQC_ADVANCED), 0, 2, NULL);

		satNotify->addItem(1, ojDiseqcRepeats);

		fsatSetup	= new CMenuForwarder(LOCALE_SATSETUP_SAT_SETUP, true, NULL, satSetup, "", CRCInput::convertDigitToKey(shortcut++));
		//fmotorMenu	= new CMenuForwarder(LOCALE_SATSETUP_EXTENDED_MOTOR, (dmode == DISEQC_ADVANCED), NULL, motorMenu, "", CRCInput::convertDigitToKey(shortcut++));
		//satNotify->addItem(0, fmotorMenu); //FIXME testing motor with not DISEQC_ADVANCED
		fmotorMenu	= new CMenuForwarder(LOCALE_SATSETUP_EXTENDED_MOTOR, true, NULL, motorMenu, "", CRCInput::convertDigitToKey(shortcut++));

		CMenuWidget* autoScanAll = new CMenuWidget(LOCALE_SATSETUP_AUTO_SCAN_ALL, NEUTRINO_ICON_SETTINGS);
		fautoScanAll	= new CMenuForwarder(LOCALE_SATSETUP_AUTO_SCAN_ALL, (dmode != NO_DISEQC), NULL, autoScanAll, "", CRCInput::RC_blue, NEUTRINO_ICON_BUTTON_BLUE);
		satNotify->addItem(2, fautoScanAll);


		autoScanAll->addIntroItems();
		autoScanAll->addItem(new CMenuForwarder(LOCALE_SATSETUP_SATELLITE, true, NULL, satOnOff, "", CRCInput::RC_red, NEUTRINO_ICON_BUTTON_RED));
		autoScanAll->addItem(useNit);
		autoScanAll->addItem(scanPids);
		autoScanAll->addItem(ftaFlag);
		autoScanAll->addItem(new CMenuForwarder(LOCALE_SCANTS_STARTNOW, true, NULL, scanTs, "all", CRCInput::RC_blue, NEUTRINO_ICON_BUTTON_BLUE));
	}

	settings.addItem(GenericMenuSeparator);
	settings.addItem(GenericMenuBack);
	settings.addItem(new CMenuForwarder(LOCALE_MAINSETTINGS_SAVESETTINGSNOW, true, NULL, this, "savesettings", CRCInput::RC_red, NEUTRINO_ICON_BUTTON_RED));
	settings.addItem(GenericMenuSeparatorLine);

	settings.addItem(ojScantype);
	settings.addItem(ojBouquets);

	if (g_info.delivery_system == DVB_S) {
		settings.addItem(ojDiseqc);
		settings.addItem(ojDiseqcRepeats);
		settings.addItem(fsatSetup);
		settings.addItem(fmotorMenu);
	}
	settings.addItem(new CMenuOptionNumberChooser(LOCALE_EXTRA_ZAPIT_TIMEOUT, (int *)&zapitCfg.feTimeout, true, 6, 100) );

	settings.addItem(new CMenuForwarder(LOCALE_SATSETUP_MANUAL_SCAN, true, NULL, manualScan, "", CRCInput::RC_green, NEUTRINO_ICON_BUTTON_GREEN));
	settings.addItem(new CMenuForwarder(LOCALE_SATSETUP_AUTO_SCAN, true, NULL, autoScan, "", CRCInput::RC_yellow, NEUTRINO_ICON_BUTTON_YELLOW));

	if (g_info.delivery_system == DVB_S) {
		settings.addItem(fautoScanAll);

		CMenuWidget* fastScanMenu = new CMenuWidget(LOCALE_SATSETUP_FASTSCAN_HEAD, NEUTRINO_ICON_SETTINGS);
		fastScanMenu->addIntroItems();

		CMenuOptionChooser* fastProv = new CMenuOptionChooser(LOCALE_SATSETUP_FASTSCAN_PROV, (int *)&scanSettings.fast_op, FAST_SCAN_PROV_OPTIONS, FAST_SCAN_PROV_OPTIONS_COUNT, true, NULL, CRCInput::RC_red, NEUTRINO_ICON_BUTTON_RED, true);
		CMenuOptionChooser* fastType = new CMenuOptionChooser(LOCALE_SATSETUP_FASTSCAN_TYPE, (int *)&scanSettings.fast_type, FAST_SCAN_OPTIONS, FAST_SCAN_OPTIONS_COUNT, true, NULL, CRCInput::RC_green, NEUTRINO_ICON_BUTTON_GREEN, true);

		fastScanMenu->addItem(fastProv);
		fastScanMenu->addItem(fastType);
		fastScanMenu->addItem(new CMenuForwarder(LOCALE_SCANTS_STARTNOW, true, NULL, scanTs, "fast", CRCInput::RC_blue, NEUTRINO_ICON_BUTTON_BLUE));
		settings.addItem(new CMenuForwarder(LOCALE_SATSETUP_FASTSCAN_HEAD, true, NULL, fastScanMenu, "", CRCInput::convertDigitToKey(0)));
	}
}

#define FLASHUPDATE_UPDATEMODE_OPTION_COUNT 2
const CMenuOptionChooser::keyval FLASHUPDATE_UPDATEMODE_OPTIONS[FLASHUPDATE_UPDATEMODE_OPTION_COUNT] =
{
	{ 0, LOCALE_FLASHUPDATE_UPDATEMODE_MANUAL   },
	{ 1, LOCALE_FLASHUPDATE_UPDATEMODE_INTERNET }
};

void getZapitConfig(Zapit_config *Cfg);
void CNeutrinoApp::InitServiceSettings(CMenuWidget &service, CMenuWidget &_scanSettings)
{
	dprintf(DEBUG_DEBUG, "init serviceSettings\n");
	getZapitConfig(&zapitCfg);

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
	service.addItem(new CMenuForwarder(LOCALE_SERVICEMENU_SCANTS    , true, NULL, &_scanSettings, "savesettings", CRCInput::RC_red, NEUTRINO_ICON_BUTTON_RED) );
	// service.addItem(new CMenuForwarder(LOCALE_EXTRA_ZAPIT_MENU      , true, NULL, zapit_menu, NULL, CRCInput::RC_green, NEUTRINO_ICON_BUTTON_GREEN));
	service.addItem(new CMenuForwarder(LOCALE_SERVICEMENU_RELOAD    , true, NULL, this, "reloadchannels", CRCInput::RC_green, NEUTRINO_ICON_BUTTON_GREEN ));
	service.addItem(new CMenuForwarder(LOCALE_BOUQUETEDITOR_NAME    , true, NULL, new CBEBouquetWidget(), NULL, CRCInput::RC_yellow, NEUTRINO_ICON_BUTTON_YELLOW ));

	CDataResetNotifier * resetNotifier = new CDataResetNotifier();
	service.addItem(new CMenuForwarder(LOCALE_RESET_CHANNELS    , true, NULL, resetNotifier, "channels", CRCInput::RC_blue, NEUTRINO_ICON_BUTTON_BLUE ));

	service.addItem(GenericMenuSeparatorLine);
	service.addItem(new CMenuForwarder(LOCALE_SERVICEMENU_RESTART   , true, NULL, this, "restart", CRCInput::RC_standby, NEUTRINO_ICON_BUTTON_POWER));
	service.addItem(new CMenuForwarder(LOCALE_SERVICEMENU_GETPLUGINS, true, NULL, this, "reloadplugins"));
	// start infomenu in service
	if (g_settings.show_infomenu == 1) {
		CMenuWidget *info = new CMenuWidget(LOCALE_MESSAGEBOX_INFO, NEUTRINO_ICON_INFO);
		info->addIntroItems();
		info->addItem(new CMenuForwarder(LOCALE_SERVICEMENU_IMAGEINFO,  true, NULL, new CImageInfo(), NULL, CRCInput::RC_red, NEUTRINO_ICON_BUTTON_RED ), false);
		info->addItem( new CMenuForwarder(LOCALE_DBOXINFO, true, NULL, new CDBoxInfoWidget, NULL, CRCInput::RC_green, NEUTRINO_ICON_BUTTON_GREEN));
		info->addItem(new CMenuForwarder(LOCALE_STREAMINFO_HEAD, true, NULL, new CStreamInfo2Handler(), NULL, CRCInput::RC_yellow, NEUTRINO_ICON_BUTTON_YELLOW));
		service.addItem(new CMenuForwarder(LOCALE_MESSAGEBOX_INFO, true, NULL, info, NULL, CRCInput::RC_help, NEUTRINO_ICON_BUTTON_HELP_SMALL ));
	}
	// end of infomenu in service
	//softupdate
	//if(softupdate)
	{
		dprintf(DEBUG_DEBUG, "init soft-update-stuff\n");
		CMenuWidget* updateSettings = new CMenuWidget(LOCALE_SERVICEMENU_UPDATE, NEUTRINO_ICON_UPDATE);
		updateSettings->addIntroItems();

		// expert-functions to read/write mtd
		CMenuWidget* mtdexpert = new CMenuWidget(LOCALE_FLASHUPDATE_EXPERTFUNCTIONS, NEUTRINO_ICON_UPDATE);
		mtdexpert->addIntroItems();
		CFlashExpert* fe = new CFlashExpert();

		//mtdexpert->addItem(new CMenuForwarder(LOCALE_FLASHUPDATE_READFLASH    , true, NULL, fe, "readflash"    ));
		//if(softupdate)
		//	mtdexpert->addItem(new CMenuForwarder(LOCALE_FLASHUPDATE_WRITEFLASH   , true, NULL, fe, "writeflash"   ));
		//mtdexpert->addItem(GenericMenuSeparatorLine);
		mtdexpert->addItem(new CMenuForwarder(LOCALE_FLASHUPDATE_READFLASHMTD , true, NULL, fe, "readflashmtd" ));
		if (softupdate)
			mtdexpert->addItem(new CMenuForwarder(LOCALE_FLASHUPDATE_WRITEFLASHMTD, true, NULL, fe, "writeflashmtd"));
		mtdexpert->addItem(GenericMenuSeparatorLine);

		CStringInputSMS * updateSettings_url_file = new CStringInputSMS(LOCALE_FLASHUPDATE_URL_FILE, g_settings.softupdate_url_file, 30, NONEXISTANT_LOCALE, NONEXISTANT_LOCALE, "abcdefghijklmnopqrstuvwxyz0123456789!""�$%&/()=?-. ");
		mtdexpert->addItem(new CMenuForwarder(LOCALE_FLASHUPDATE_URL_FILE, true, g_settings.softupdate_url_file, updateSettings_url_file));

		updateSettings->addItem(new CMenuForwarder(LOCALE_FLASHUPDATE_EXPERTFUNCTIONS, true, NULL, mtdexpert, "", CRCInput::RC_red, NEUTRINO_ICON_BUTTON_RED));

		updateSettings->addItem(GenericMenuSeparatorLine);
		CMenuOptionChooser *oj = new CMenuOptionChooser(LOCALE_FLASHUPDATE_UPDATEMODE, &g_settings.softupdate_mode, FLASHUPDATE_UPDATEMODE_OPTIONS, FLASHUPDATE_UPDATEMODE_OPTION_COUNT, true, NULL, CRCInput::RC_green, NEUTRINO_ICON_BUTTON_GREEN);
		updateSettings->addItem( oj );
		updateSettings->addItem( new CMenuForwarder(LOCALE_EXTRA_UPDATE_DIR, true, g_settings.update_dir , this, "update_dir", CRCInput::RC_yellow, NEUTRINO_ICON_BUTTON_YELLOW) );

#if 0 // image info and update list show current version, probably ehough
		/* show current version */
		updateSettings->addItem(new CMenuSeparator(CMenuSeparator::LINE | CMenuSeparator::STRING, LOCALE_FLASHUPDATE_CURRENTVERSION_SEP));

		/* get current version SBBBYYYYMMTTHHMM -- formatsting */
		CConfigFile _configfile('\t');

		const char * versionString = (_configfile.loadConfig("/.version")) ? (_configfile.getString( "version", "????????????????").c_str()) : "????????????????";

		dprintf(DEBUG_INFO, "current flash-version: %s\n", versionString);

		static CFlashVersionInfo versionInfo(versionString);

		updateSettings->addItem(new CMenuForwarder(LOCALE_FLASHUPDATE_CURRENTVERSIONDATE    , false, versionInfo.getDate()));
		updateSettings->addItem(new CMenuForwarder(LOCALE_FLASHUPDATE_CURRENTVERSIONTIME    , false, versionInfo.getTime()));
		updateSettings->addItem(new CMenuForwarder(LOCALE_FLASHUPDATE_CURRENTRELEASECYCLE   , false, versionInfo.getReleaseCycle()));
		/* versionInfo.getType() returns const char * which is never deallocated */
		updateSettings->addItem(new CMenuForwarder(LOCALE_FLASHUPDATE_CURRENTVERSIONSNAPSHOT, false, versionInfo.getType()));
#endif
		updateSettings->addItem(GenericMenuSeparatorLine);
		updateSettings->addItem(new CMenuForwarder(LOCALE_FLASHUPDATE_CHECKUPDATE, true, NULL, new CFlashUpdate(), "", CRCInput::RC_blue, NEUTRINO_ICON_BUTTON_BLUE));
		service.addItem(new CMenuForwarder(LOCALE_SERVICEMENU_UPDATE, true, NULL, updateSettings));
	}
}

#define MESSAGEBOX_NO_YES_OPTION_COUNT 2
const CMenuOptionChooser::keyval MESSAGEBOX_NO_YES_OPTIONS[MESSAGEBOX_NO_YES_OPTION_COUNT] =
{
	{ 0, LOCALE_MESSAGEBOX_NO  },
	{ 1, LOCALE_MESSAGEBOX_YES }
};

#define PICTUREVIEWER_SCALING_OPTION_COUNT 3
const CMenuOptionChooser::keyval PICTUREVIEWER_SCALING_OPTIONS[PICTUREVIEWER_SCALING_OPTION_COUNT] =
{
	{ CPictureViewer::SIMPLE, LOCALE_PICTUREVIEWER_RESIZE_SIMPLE        },
	{ CPictureViewer::COLOR , LOCALE_PICTUREVIEWER_RESIZE_COLOR_AVERAGE },
	{ CPictureViewer::NONE  , LOCALE_PICTUREVIEWER_RESIZE_NONE          }
};

#define AUDIOPLAYER_DISPLAY_ORDER_OPTION_COUNT 2
const CMenuOptionChooser::keyval AUDIOPLAYER_DISPLAY_ORDER_OPTIONS[AUDIOPLAYER_DISPLAY_ORDER_OPTION_COUNT] =
{
	{ CAudioPlayerGui::ARTIST_TITLE, LOCALE_AUDIOPLAYER_ARTIST_TITLE },
	{ CAudioPlayerGui::TITLE_ARTIST, LOCALE_AUDIOPLAYER_TITLE_ARTIST }
};

void CNeutrinoApp::InitAudioplPicSettings(CMenuWidget &audioplPicSettings)
{
	audioplPicSettings.addItem(GenericMenuSeparator);
	audioplPicSettings.addItem(GenericMenuBack);

	audioplPicSettings.addItem(new CMenuSeparator(CMenuSeparator::LINE | CMenuSeparator::STRING, LOCALE_PICTUREVIEWER_HEAD));
	audioplPicSettings.addItem(new CMenuOptionChooser(LOCALE_PICTUREVIEWER_SCALING  , &g_settings.picviewer_scaling     , PICTUREVIEWER_SCALING_OPTIONS  , PICTUREVIEWER_SCALING_OPTION_COUNT  , true ));
	CStringInput * pic_timeout= new CStringInput(LOCALE_PICTUREVIEWER_SLIDE_TIME, g_settings.picviewer_slide_time, 2, NONEXISTANT_LOCALE, NONEXISTANT_LOCALE, "0123456789 ");
	audioplPicSettings.addItem(new CMenuForwarder(LOCALE_PICTUREVIEWER_SLIDE_TIME, true, g_settings.picviewer_slide_time, pic_timeout));
	audioplPicSettings.addItem(new CMenuForwarder(LOCALE_PICTUREVIEWER_DEFDIR, true, g_settings.network_nfs_picturedir, this, "picturedir"));

	audioplPicSettings.addItem(new CMenuSeparator(CMenuSeparator::LINE | CMenuSeparator::STRING, LOCALE_AUDIOPLAYER_NAME));
	audioplPicSettings.addItem(new CMenuOptionChooser(LOCALE_AUDIOPLAYER_DISPLAY_ORDER, &g_settings.audioplayer_display     , AUDIOPLAYER_DISPLAY_ORDER_OPTIONS, AUDIOPLAYER_DISPLAY_ORDER_OPTION_COUNT, true ));
	audioplPicSettings.addItem(new CMenuOptionChooser(LOCALE_AUDIOPLAYER_FOLLOW       , &g_settings.audioplayer_follow      , MESSAGEBOX_NO_YES_OPTIONS      , MESSAGEBOX_NO_YES_OPTION_COUNT      , true ));
	audioplPicSettings.addItem(new CMenuOptionChooser(LOCALE_AUDIOPLAYER_SELECT_TITLE_BY_NAME       , &g_settings.audioplayer_select_title_by_name      , MESSAGEBOX_NO_YES_OPTIONS      , MESSAGEBOX_NO_YES_OPTION_COUNT      , true ));
	audioplPicSettings.addItem(new CMenuOptionChooser(LOCALE_AUDIOPLAYER_REPEAT_ON       , &g_settings.audioplayer_repeat_on      , MESSAGEBOX_NO_YES_OPTIONS      , MESSAGEBOX_NO_YES_OPTION_COUNT      , true ));
	audioplPicSettings.addItem(new CMenuOptionChooser(LOCALE_AUDIOPLAYER_SHOW_PLAYLIST, &g_settings.audioplayer_show_playlist, MESSAGEBOX_NO_YES_OPTIONS, MESSAGEBOX_NO_YES_OPTION_COUNT, true ));

	CStringInput * audio_screensaver= new CStringInput(LOCALE_AUDIOPLAYER_SCREENSAVER_TIMEOUT, g_settings.audioplayer_screensaver, 2, NONEXISTANT_LOCALE, NONEXISTANT_LOCALE, "0123456789 ");
	audioplPicSettings.addItem(new CMenuForwarder(LOCALE_AUDIOPLAYER_SCREENSAVER_TIMEOUT, true, g_settings.audioplayer_screensaver, audio_screensaver));

	audioplPicSettings.addItem(new CMenuOptionChooser(LOCALE_AUDIOPLAYER_HIGHPRIO     , &g_settings.audioplayer_highprio    , MESSAGEBOX_NO_YES_OPTIONS      , MESSAGEBOX_NO_YES_OPTION_COUNT      , true ));

	if (CVFD::getInstance()->has_lcd) //FIXME
		audioplPicSettings.addItem(new CMenuOptionChooser(LOCALE_AUDIOPLAYER_SPECTRUM     , &g_settings.spectrum    , MESSAGEBOX_NO_YES_OPTIONS      , MESSAGEBOX_NO_YES_OPTION_COUNT      , true ));
	audioplPicSettings.addItem(new CMenuForwarder(LOCALE_AUDIOPLAYER_DEFDIR, true, g_settings.network_nfs_audioplayerdir, this, "audioplayerdir"));
	audioplPicSettings.addItem(new CMenuOptionChooser(LOCALE_AUDIOPLAYER_ENABLE_SC_METADATA, &g_settings.audioplayer_enable_sc_metadata, MESSAGEBOX_NO_YES_OPTIONS, MESSAGEBOX_NO_YES_OPTION_COUNT, true ));

}

#define MISCSETTINGS_FB_DESTINATION_OPTION_COUNT 3
const CMenuOptionChooser::keyval MISCSETTINGS_FB_DESTINATION_OPTIONS[MISCSETTINGS_FB_DESTINATION_OPTION_COUNT] =
{
	{ 0, LOCALE_OPTIONS_NULL   },
	{ 1, LOCALE_OPTIONS_SERIAL },
	{ 2, LOCALE_OPTIONS_FB     }
};

#define MISCSETTINGS_FILESYSTEM_IS_UTF8_OPTION_COUNT 2
const CMenuOptionChooser::keyval MISCSETTINGS_FILESYSTEM_IS_UTF8_OPTIONS[MISCSETTINGS_FILESYSTEM_IS_UTF8_OPTION_COUNT] =
{
	{ 0, LOCALE_FILESYSTEM_IS_UTF8_OPTION_ISO8859_1 },
	{ 1, LOCALE_FILESYSTEM_IS_UTF8_OPTION_UTF8      }
};

// #define INFOBAR_SUBCHAN_DISP_POS_OPTIONS_COUNT 5
// const CMenuOptionChooser::keyval  INFOBAR_SUBCHAN_DISP_POS_OPTIONS[INFOBAR_SUBCHAN_DISP_POS_OPTIONS_COUNT]=
// {
// 	{ 0 , LOCALE_SETTINGS_POS_TOP_RIGHT },
// 	{ 1 , LOCALE_SETTINGS_POS_TOP_LEFT },
// 	{ 2 , LOCALE_SETTINGS_POS_BOTTOM_LEFT },
// 	{ 3 , LOCALE_SETTINGS_POS_BOTTOM_RIGHT },
// 	{ 4 , LOCALE_INFOVIEWER_SUBCHAN_INFOBAR }
// };

#define CHANNELLIST_EPGTEXT_ALIGN_RIGHT_OPTIONS_COUNT 2
const CMenuOptionChooser::keyval  CHANNELLIST_EPGTEXT_ALIGN_RIGHT_OPTIONS[CHANNELLIST_EPGTEXT_ALIGN_RIGHT_OPTIONS_COUNT]=
{
	{ 0 , LOCALE_CHANNELLIST_EPGTEXT_ALIGN_LEFT },
	{ 1 , LOCALE_CHANNELLIST_EPGTEXT_ALIGN_RIGHT }
};

#define CHANNELLIST_FOOT_OPTIONS_COUNT 3
const CMenuOptionChooser::keyval  CHANNELLIST_FOOT_OPTIONS[CHANNELLIST_FOOT_OPTIONS_COUNT]=
{
	{ 0 , LOCALE_CHANNELLIST_FOOT_FREQ },
	{ 1 , LOCALE_CHANNELLIST_FOOT_NEXT },
	{ 2 , LOCALE_CHANNELLIST_FOOT_OFF }
};

#define CPU_FREQ_OPTION_COUNT 13
const CMenuOptionChooser::keyval_ext CPU_FREQ_OPTIONS[CPU_FREQ_OPTION_COUNT] =
{
	{ 0, LOCALE_CPU_FREQ_DEFAULT, NULL },
	{ 50, NONEXISTANT_LOCALE,  "50 Mhz"},
	{ 100, NONEXISTANT_LOCALE, "100 Mhz"},
	{ 150, NONEXISTANT_LOCALE, "150 Mhz"},
	{ 200, NONEXISTANT_LOCALE, "200 Mhz"},
	{ 250, NONEXISTANT_LOCALE, "250 Mhz"},
	{ 300, NONEXISTANT_LOCALE, "300 Mhz"},
	{ 350, NONEXISTANT_LOCALE, "350 Mhz"},
	{ 400, NONEXISTANT_LOCALE, "400 Mhz"},
	{ 450, NONEXISTANT_LOCALE, "450 Mhz"},
	{ 500, NONEXISTANT_LOCALE, "500 Mhz"},
	{ 550, NONEXISTANT_LOCALE, "550 Mhz"},
	{ 600, NONEXISTANT_LOCALE, "600 Mhz"},
};


void CNeutrinoApp::InitMiscSettings(CMenuWidget &miscSettings)
{
	dprintf(DEBUG_DEBUG, "init miscsettings\n");
	miscSettings.addIntroItems();
//	miscSettings.addItem(new CMenuForwarder(LOCALE_MAINSETTINGS_SAVESETTINGSNOW, true, NULL, this, "savesettings", CRCInput::RC_red, NEUTRINO_ICON_BUTTON_RED));
	CMenuWidget *miscSettingsGeneral = new CMenuWidget(LOCALE_MISCSETTINGS_GENERAL, NEUTRINO_ICON_SETTINGS);
	miscSettingsGeneral->addIntroItems();
#if 0
	CMenuOptionChooser *m1 = new CMenuOptionChooser(LOCALE_MISCSETTINGS_SHUTDOWN_REAL_RCDELAY, &g_settings.shutdown_real_rcdelay, OPTIONS_OFF0_ON1_OPTIONS, OPTIONS_OFF0_ON1_OPTION_COUNT, !g_settings.shutdown_real);
	CMiscNotifier* miscNotifier = new CMiscNotifier( m1 );
	miscSettings.addItem(new CMenuOptionChooser(LOCALE_MISCSETTINGS_SHUTDOWN_REAL, &g_settings.shutdown_real, OPTIONS_OFF1_ON0_OPTIONS, OPTIONS_OFF1_ON0_OPTION_COUNT, true, miscNotifier));
#endif
	miscSettingsGeneral->addItem(new CMenuOptionChooser(LOCALE_EXTRA_STARTSTANDBY, &g_settings.power_standby, OPTIONS_OFF0_ON1_OPTIONS, OPTIONS_OFF0_ON1_OPTION_COUNT, true));
	miscSettingsGeneral->addItem(new CMenuOptionChooser(LOCALE_EXTRA_CACHE_TXT,  (int *)&g_settings.cacheTXT, OPTIONS_OFF0_ON1_OPTIONS, OPTIONS_OFF0_ON1_OPTION_COUNT, true));
	//miscSettings.addItem(new CMenuForwarder(LOCALE_EXTRA_KEY_PLUGIN, true, g_settings.onekey_plugin,this,"onekeyplugin"));
	
	//don't show fan speed settings on cable box and NEO
	funNotifier = new CFanControlNotifier();
	if (g_info.delivery_system == DVB_S && (cs_get_revision() < 8)) {
		miscSettingsGeneral->addItem(new CMenuOptionNumberChooser(LOCALE_FAN_SPEED, &g_settings.fan_speed, true, 1, 14, funNotifier, 0, 0, LOCALE_OPTIONS_OFF) );
		funNotifier->changeNotify(NONEXISTANT_LOCALE, (void*) &g_settings.fan_speed);
	}
	//don't show rotor settings on cable box
	if (g_info.delivery_system == DVB_S) {
		miscSettingsGeneral->addItem(new CMenuOptionChooser(LOCALE_EXTRA_ROTORSWAP, &g_settings.rotor_swap, OPTIONS_OFF0_ON1_OPTIONS, OPTIONS_OFF0_ON1_OPTION_COUNT, true));
	}
	miscSettingsGeneral->addItem(new CMenuForwarder(LOCALE_PLUGINS_HDD_DIR, true, g_settings.plugin_hdd_dir, this, "plugin_dir"));
	miscSettingsGeneral->addItem(new CMenuForwarder(LOCALE_LOGO_HDD_DIR, true, g_settings.logo_hdd_dir, this, "logo_dir"));


	if(cs_get_revision() > 7){
		miscSettingsGeneral->addItem(new CMenuSeparator(CMenuSeparator::LINE | CMenuSeparator::STRING, LOCALE_MAINMENU_SHUTDOWN));
		CMenuOptionChooser *m1 = new CMenuOptionChooser(LOCALE_MISCSETTINGS_SHUTDOWN_REAL_RCDELAY, &g_settings.shutdown_real_rcdelay, OPTIONS_OFF0_ON1_OPTIONS, OPTIONS_OFF0_ON1_OPTION_COUNT, !g_settings.shutdown_real);

		CStringInput * miscSettings_shutdown_count = new CStringInput(LOCALE_MISCSETTINGS_SHUTDOWN_COUNT, g_settings.shutdown_count, 3, LOCALE_MISCSETTINGS_SHUTDOWN_COUNT_HINT1, LOCALE_MISCSETTINGS_SHUTDOWN_COUNT_HINT2, "0123456789 ");
		CMenuForwarder *m2 = new CMenuForwarder(LOCALE_MISCSETTINGS_SHUTDOWN_COUNT, !g_settings.shutdown_real, g_settings.shutdown_count, miscSettings_shutdown_count);                

		CMiscNotifier* miscNotifier = new CMiscNotifier( m1, m2 );

		miscSettingsGeneral->addItem(new CMenuOptionChooser(LOCALE_MISCSETTINGS_SHUTDOWN_REAL, &g_settings.shutdown_real, OPTIONS_OFF1_ON0_OPTIONS, OPTIONS_OFF1_ON0_OPTION_COUNT, true, miscNotifier));
		miscSettingsGeneral->addItem(m1);
		miscSettingsGeneral->addItem(m2);
		miscSettingsGeneral->addItem(new CMenuForwarder(LOCALE_MISCSETTINGS_SLEEPTIMER, true, NULL, new CSleepTimerWidget, "permanent"));
	}

#if 0
	miscSettingsGeneral->addItem(new CMenuOptionChooser(LOCALE_EXTRA_SCRAMBLED_MESSAGE, &g_settings.scrambled_message, OPTIONS_OFF0_ON1_OPTIONS, OPTIONS_OFF0_ON1_OPTION_COUNT, true));
	miscSettingsGeneral->addItem(new CMenuOptionChooser(LOCALE_INFOVIEWER_SUBCHAN_DISP_POS, &g_settings.infobar_subchan_disp_pos, INFOBAR_SUBCHAN_DISP_POS_OPTIONS, INFOBAR_SUBCHAN_DISP_POS_OPTIONS_COUNT, true));
	miscSettingsGeneral->addItem(new CMenuOptionChooser(LOCALE_EXTRA_VOLUME_POS, &g_settings.volume_pos, VOLUMEBAR_DISP_POS_OPTIONS, VOLUMEBAR_DISP_POS_OPTIONS_COUNT, true));
	miscSettingsGeneral->addItem(new CMenuOptionChooser(LOCALE_SETTINGS_MENU_POS, &g_settings.menu_pos, MENU_DISP_POS_OPTIONS, MENU_DISP_POS_OPTIONS_COUNT, true));
	miscSettingsGeneral->addItem(new CMenuOptionChooser(LOCALE_EXTRA_ROUNDED_CORNERS, &g_settings.rounded_corners, MENU_CORNERSETTINGS_TYPE_OPTIONS, MENU_CORNERSETTINGS_TYPE_OPTION_COUNT, true));
#endif
	miscSettings.addItem( new CMenuForwarder(LOCALE_MISCSETTINGS_GENERAL, true, NULL, miscSettingsGeneral, NULL, CRCInput::RC_red, NEUTRINO_ICON_BUTTON_RED) );

	//channellist
	CMenuWidget *miscSettingsChannellist = new CMenuWidget(LOCALE_MISCSETTINGS_CHANNELLIST, NEUTRINO_ICON_SETTINGS);
	miscSettingsChannellist->addIntroItems();
	miscSettingsChannellist->addItem(new CMenuOptionChooser(LOCALE_MISCSETTINGS_CHANNELLIST_EPGTEXT_ALIGN, &g_settings.channellist_epgtext_align_right, CHANNELLIST_EPGTEXT_ALIGN_RIGHT_OPTIONS, CHANNELLIST_EPGTEXT_ALIGN_RIGHT_OPTIONS_COUNT, true));
	miscSettingsChannellist->addItem(new CMenuOptionChooser(LOCALE_CHANNELLIST_EXTENDED, &g_settings.channellist_extended, OPTIONS_OFF0_ON1_OPTIONS, OPTIONS_OFF0_ON1_OPTION_COUNT, true));
	miscSettingsChannellist->addItem(new CMenuOptionChooser(LOCALE_CHANNELLIST_FOOT, &g_settings.channellist_foot, CHANNELLIST_FOOT_OPTIONS, CHANNELLIST_FOOT_OPTIONS_COUNT, true));
	miscSettingsChannellist->addItem(new CMenuOptionChooser(LOCALE_CHANNELLIST_MAKE_HDLIST, &g_settings.make_hd_list, OPTIONS_OFF0_ON1_OPTIONS, OPTIONS_OFF0_ON1_OPTION_COUNT, true));
	miscSettingsChannellist->addItem(new CMenuOptionChooser(LOCALE_EXTRA_ZAP_CYCLE, &g_settings.zap_cycle, OPTIONS_OFF0_ON1_OPTIONS, OPTIONS_OFF0_ON1_OPTION_COUNT, true));
	miscSettings.addItem( new CMenuForwarder(LOCALE_MISCSETTINGS_CHANNELLIST, true, NULL, miscSettingsChannellist, NULL, CRCInput::RC_green, NEUTRINO_ICON_BUTTON_GREEN) );

	//EPGSettings
	CMenuWidget *miscSettingsEPGSettings = new CMenuWidget(LOCALE_MISCSETTINGS_EPG_HEAD, NEUTRINO_ICON_SETTINGS);
	miscSettingsEPGSettings->addIntroItems();
	CSectionsdConfigNotifier* sectionsdConfigNotifier = new CSectionsdConfigNotifier;
	miscSettingsEPGSettings->addItem(new CMenuOptionChooser(LOCALE_MISCSETTINGS_EPG_SAVE, &g_settings.epg_save, OPTIONS_OFF0_ON1_OPTIONS, OPTIONS_OFF0_ON1_OPTION_COUNT, true));
	CStringInput * miscSettings_epg_cache = new CStringInput(LOCALE_MISCSETTINGS_EPG_CACHE, &g_settings.epg_cache, 2,LOCALE_MISCSETTINGS_EPG_CACHE_HINT1, LOCALE_MISCSETTINGS_EPG_CACHE_HINT2 , "0123456789 ", sectionsdConfigNotifier);
	miscSettingsEPGSettings->addItem(new CMenuForwarder(LOCALE_MISCSETTINGS_EPG_CACHE, true, g_settings.epg_cache, miscSettings_epg_cache));
	CStringInput * miscSettings_epg_cache_e = new CStringInput(LOCALE_MISCSETTINGS_EPG_EXTENDEDCACHE, &g_settings.epg_extendedcache, 3,LOCALE_MISCSETTINGS_EPG_EXTENDEDCACHE_HINT1, LOCALE_MISCSETTINGS_EPG_EXTENDEDCACHE_HINT2 , "0123456789 ", sectionsdConfigNotifier);
	miscSettingsEPGSettings->addItem(new CMenuForwarder(LOCALE_MISCSETTINGS_EPG_EXTENDEDCACHE, true, g_settings.epg_extendedcache, miscSettings_epg_cache_e));
	CStringInput * miscSettings_epg_old_events = new CStringInput(LOCALE_MISCSETTINGS_EPG_OLD_EVENTS, &g_settings.epg_old_events, 2,LOCALE_MISCSETTINGS_EPG_OLD_EVENTS_HINT1, LOCALE_MISCSETTINGS_EPG_OLD_EVENTS_HINT2 , "0123456789 ", sectionsdConfigNotifier);
	miscSettingsEPGSettings->addItem(new CMenuForwarder(LOCALE_MISCSETTINGS_EPG_OLD_EVENTS, true, g_settings.epg_old_events, miscSettings_epg_old_events));
	CStringInput * miscSettings_epg_max_events = new CStringInput(LOCALE_MISCSETTINGS_EPG_MAX_EVENTS, &g_settings.epg_max_events, 5,LOCALE_MISCSETTINGS_EPG_MAX_EVENTS_HINT1, LOCALE_MISCSETTINGS_EPG_MAX_EVENTS_HINT2 , "0123456789 ", sectionsdConfigNotifier);
	miscSettingsEPGSettings->addItem(new CMenuForwarder(LOCALE_MISCSETTINGS_EPG_MAX_EVENTS, true, g_settings.epg_max_events, miscSettings_epg_max_events));
	miscSettingsEPGSettings->addItem(new CMenuForwarder(LOCALE_MISCSETTINGS_EPG_DIR, true, g_settings.epg_dir, this, "epgdir"));
	miscSettings.addItem( new CMenuForwarder(LOCALE_MISCSETTINGS_EPG_HEAD, true, NULL, miscSettingsEPGSettings, NULL, CRCInput::RC_yellow,NEUTRINO_ICON_BUTTON_YELLOW ) );
	//end of EPGSettings

#if 0
	miscSettings.addItem(new CMenuSeparator(CMenuSeparator::LINE | CMenuSeparator::STRING, LOCALE_MISCSETTINGS_DRIVER_BOOT));
	if (access("/var/tuxbox/emlog", F_OK) == 0)
		g_settings.emlog = 1;
	miscSettings.addItem(new CMenuOptionChooser(LOCALE_EXTRA_USELOG, &g_settings.emlog, OPTIONS_OFF0_ON1_OPTIONS, OPTIONS_OFF0_ON1_OPTION_COUNT, true, new CLogChangeNotifier));
#endif

	//filebrowsersettings
	CMenuWidget *miscSettingsFilebrowser = new CMenuWidget(LOCALE_FILEBROWSER_HEAD, NEUTRINO_ICON_SETTINGS);
	miscSettingsFilebrowser->addIntroItems();
	miscSettingsFilebrowser->addItem(new CMenuOptionChooser(LOCALE_FILESYSTEM_IS_UTF8            , &g_settings.filesystem_is_utf8            , MISCSETTINGS_FILESYSTEM_IS_UTF8_OPTIONS, MISCSETTINGS_FILESYSTEM_IS_UTF8_OPTION_COUNT, true ));
	miscSettingsFilebrowser->addItem(new CMenuOptionChooser(LOCALE_FILEBROWSER_SHOWRIGHTS        , &g_settings.filebrowser_showrights        , MESSAGEBOX_NO_YES_OPTIONS              , MESSAGEBOX_NO_YES_OPTION_COUNT              , true ));
	miscSettingsFilebrowser->addItem(new CMenuOptionChooser(LOCALE_FILEBROWSER_DENYDIRECTORYLEAVE, &g_settings.filebrowser_denydirectoryleave, MESSAGEBOX_NO_YES_OPTIONS              , MESSAGEBOX_NO_YES_OPTION_COUNT              , true ));
	miscSettings.addItem( new CMenuForwarder(LOCALE_FILEBROWSER_HEAD, true, NULL, miscSettingsFilebrowser, NULL, CRCInput::RC_blue, NEUTRINO_ICON_BUTTON_BLUE) );
	miscSettings.addItem(new CMenuForwarder(LOCALE_VIDEOMENU_HDMI_CEC, true, NULL, new CCECSetup() , NULL, CRCInput::RC_0));

#if 0
	//infobar
	CMenuWidget *miscSettingsInfobar = new CMenuWidget(LOCALE_MISCSETTINGS_INFOBAR, NEUTRINO_ICON_SETTINGS);
	addIntroItems(*miscSettingsInfobar);
	miscSettingsInfobar->addItem(new CMenuOptionChooser(LOCALE_PROGRESSBAR_COLOR, &g_settings.progressbar_color, OPTIONS_OFF0_ON1_OPTIONS, OPTIONS_OFF0_ON1_OPTION_COUNT, true));
	miscSettingsInfobar->addItem(new CMenuOptionChooser(LOCALE_MISCSETTINGS_INFOBAR_CASYSTEM_DISPLAY, &g_settings.casystem_display, OPTIONS_OFF0_ON1_OPTIONS, OPTIONS_OFF0_ON1_OPTION_COUNT, true));
	miscSettingsInfobar->addItem(new CMenuOptionChooser(LOCALE_MISCSETTINGS_INFOBAR_DISP_LOG, &g_settings.infobar_show_channellogo, LOCALE_MISCSETTINGS_INFOBAR_DISP_OPTIONS, LOCALE_MISCSETTINGS_INFOBAR_DISP_OPTIONS_COUNT, true));
	miscSettingsInfobar->addItem(new CMenuOptionChooser(LOCALE_MISCSETTINGS_VIRTUAL_ZAP_MODE, &g_settings.virtual_zap_mode, OPTIONS_OFF0_ON1_OPTIONS, OPTIONS_OFF0_ON1_OPTION_COUNT, true));
	miscSettingsInfobar->addItem(new CMenuOptionChooser(LOCALE_MISCSETTINGS_INFOBAR_SAT_DISPLAY, &g_settings.infobar_sat_display, OPTIONS_OFF0_ON1_OPTIONS, OPTIONS_OFF0_ON1_OPTION_COUNT, true));
	miscSettings.addItem(new CMenuForwarder(LOCALE_MISCSETTINGS_INFOBAR, true, NULL, miscSettingsInfobar, NULL, CRCInput::RC_1));
#endif
#if 0
	CCpuFreqNotifier * cpuNotifier = new CCpuFreqNotifier();
	miscSettings.addItem(new CMenuOptionChooser(LOCALE_CPU_FREQ_NORMAL, &g_settings.cpufreq, CPU_FREQ_OPTIONS, CPU_FREQ_OPTION_COUNT, true, cpuNotifier));
	miscSettings.addItem(new CMenuOptionChooser(LOCALE_CPU_FREQ_STANDBY, &g_settings.standby_cpufreq, CPU_FREQ_OPTIONS, CPU_FREQ_OPTION_COUNT, true));
#endif
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
			menu->addItem(new CMenuForwarderNonLocalized(nvod_s, true, NULL, NVODChanger, nvod_id), (count == g_RemoteControl->selected_subchannel));
		} else {
			menu->addItem(new CMenuForwarderNonLocalized(e->subservice_name.c_str(), true, NULL, NVODChanger, nvod_id, CRCInput::convertDigitToKey(count)), (count == g_RemoteControl->selected_subchannel));
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
// This is just a quick helper for the usermenu only. I already made it a class for future use.
#define BUTTONMAX 4
const neutrino_msg_t key_helper_msg_def[BUTTONMAX]={CRCInput::RC_red,CRCInput::RC_green,CRCInput::RC_yellow,CRCInput::RC_blue};
const char * key_helper_icon_def[BUTTONMAX]={NEUTRINO_ICON_BUTTON_RED,NEUTRINO_ICON_BUTTON_GREEN,NEUTRINO_ICON_BUTTON_YELLOW,NEUTRINO_ICON_BUTTON_BLUE};
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
				*msg = key_helper_msg_def[button];
				*icon = key_helper_icon_def[button];
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
	if (button < 0 || button >= SNeutrinoSettings::BUTTON_MAX)
		return false;

	CMenuItem* menu_item = NULL;
	CKeyHelper keyhelper;
	neutrino_msg_t key = CRCInput::RC_nokey;
	const char * icon = NULL;
	int dummy;

	int menu_items = 0;
	int menu_prev = -1;
	static int selected[SNeutrinoSettings::BUTTON_MAX] = {-1, -1, -1, -1};

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
	if (button == SNeutrinoSettings::BUTTON_RED) {
		if ( txt.empty() )
			txt = g_Locale->getText(LOCALE_INFOVIEWER_EVENTLIST);
	}
	else if ( button == SNeutrinoSettings::BUTTON_GREEN) {
		if ( txt.empty() )
			txt = g_Locale->getText(LOCALE_INFOVIEWER_LANGUAGES);
	}
	else if ( button == SNeutrinoSettings::BUTTON_YELLOW) {
		if ( txt.empty() )
			txt = g_Locale->getText((g_RemoteControl->are_subchannels) ? LOCALE_INFOVIEWER_SUBSERVICE : LOCALE_INFOVIEWER_SELECTTIME);
		//txt = g_Locale->getText(LOCALE_NVODSELECTOR_DIRECTORMODE);
	}
	else if ( button == SNeutrinoSettings::BUTTON_BLUE) {
		if ( txt.empty() )
			txt = g_Locale->getText(LOCALE_INFOVIEWER_STREAMINFO);
	}
	CMenuWidget *menu = new CMenuWidget(txt.c_str() , NEUTRINO_ICON_FEATURES);
	if (menu == NULL)
		return 0;
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
			menu_item = new CMenuOptionChooser(LOCALE_MAINMENU_RECORDING, &recordingstatus, MAINMENU_RECORDING_OPTIONS, MAINMENU_RECORDING_OPTION_COUNT, true, this, key, icon);
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
			menu->addItem( new CMenuForwarder(LOCALE_DBOXINFO, true, NULL, new CDBoxInfoWidget, NULL, key, icon));
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
		menu->setSelected(selected[button]);
		menu->exec(NULL,"");
		selected[button] = menu->getSelected();
	}
	else if (menu_item != NULL)
		menu_item->exec( NULL );

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

