/*
	scan_setup menue - Neutrino-GUI

	Copyright (C) 2001 Steffen Hehn 'McClean'
	and some other guys
	Homepage: http://dbox.cyberphoria.org/

	Reworked as similar to $Id: scan_setup.cpp,v 1.10 2010/12/05 22:32:12 tuxbox-cvs Exp $
	Copyright (C) 2011 T. Graf 'dbt'
	Homepage: http://www.dbox2-tuning.net/

	Copyright (C) 2011-2012 Stefan Seyfried
	Copyright (C) 2011 CoolStream International Ltd

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

#include <global.h>
#include <neutrino.h>
#include <mymenu.h>
#include <neutrino_menue.h>

#include <gui/scan.h>
#include <gui/scan_setup.h>
#include <gui/motorcontrol.h>
#include <gui/bedit/bouqueteditor_bouquets.h>

#include <gui/widget/hintbox.h>
#include <gui/widget/stringinput.h>

#include <driver/screen_max.h>
#include <driver/framebuffer.h>
#include <driver/record.h>
#include <system/debug.h>

#include <zapit/femanager.h>
#include <zapit/getservices.h>
#include <zapit/scan.h>
#include <zapit/zapit.h>
#define __NFILE__ 1
#include <zapit/debug.h>
#include <set>

//extern std::map<transponder_id_t, transponder> select_transponders;
extern Zapit_config zapitCfg;
extern char zapit_lat[20];
extern char zapit_long[20];
/* ugly */
extern CHintBox *reloadhintBox;

static int all_usals = 1;
//sat_iterator_t sit;

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
	{  CServiceScan::SCAN_TVRADIO	, LOCALE_ZAPIT_SCANTYPE_TVRADIO	},
	{  CServiceScan::SCAN_TV	, LOCALE_ZAPIT_SCANTYPE_TV      },
	{  CServiceScan::SCAN_RADIO	, LOCALE_ZAPIT_SCANTYPE_RADIO   },
	{  CServiceScan::SCAN_ALL	, LOCALE_ZAPIT_SCANTYPE_ALL     }
};

#define SATSETUP_DISEQC_OPTION_COUNT 6
const CMenuOptionChooser::keyval SATSETUP_DISEQC_OPTIONS[SATSETUP_DISEQC_OPTION_COUNT] =
{
	{ NO_DISEQC,		LOCALE_SATSETUP_NODISEQC },
	{ MINI_DISEQC,		LOCALE_SATSETUP_MINIDISEQC },
	{ DISEQC_1_0,		LOCALE_SATSETUP_DISEQC10 },
	{ DISEQC_1_1,		LOCALE_SATSETUP_DISEQC11 },
	/*{ DISEQC_1_2,	LOCALE_SATSETUP_DISEQC12 },*/
	{ DISEQC_ADVANCED,	LOCALE_SATSETUP_DISEQC_ADVANCED },
	{ DISEQC_UNICABLE,	LOCALE_SATSETUP_UNICABLE }
//	{ SMATV_REMOTE_TUNING,	LOCALE_SATSETUP_SMATVREMOTE }
};

#define SATSETUP_SCANTP_FEC_COUNT 21
#define CABLESETUP_SCANTP_FEC_COUNT 5
const CMenuOptionChooser::keyval SATSETUP_SCANTP_FEC[SATSETUP_SCANTP_FEC_COUNT] =
{
	{ FEC_AUTO, LOCALE_EXTRA_FEC_AUTO },
	{ FEC_S2_AUTO, LOCALE_EXTRA_FEC_AUTO_S2 },

	{ FEC_1_2, LOCALE_EXTRA_FEC_1_2 },
	{ FEC_2_3, LOCALE_EXTRA_FEC_2_3 },
	{ FEC_3_4, LOCALE_EXTRA_FEC_3_4 },
	{ FEC_5_6, LOCALE_EXTRA_FEC_5_6 },
	{ FEC_7_8, LOCALE_EXTRA_FEC_7_8 },

	{ FEC_S2_QPSK_1_2, LOCALE_EXTRA_FEC_S2_QPSK_1_2 },
	{ FEC_S2_QPSK_2_3, LOCALE_EXTRA_FEC_S2_QPSK_2_3 },
	{ FEC_S2_QPSK_3_4, LOCALE_EXTRA_FEC_S2_QPSK_3_4 },
	{ FEC_S2_QPSK_5_6, LOCALE_EXTRA_FEC_S2_QPSK_5_6 },
	//{ FEC_S2_QPSK_7_8, LOCALE_EXTRA_FEC_S2_QPSK_7_8 },
	{ FEC_S2_QPSK_8_9, LOCALE_EXTRA_FEC_S2_QPSK_8_9 },
	{ FEC_S2_QPSK_3_5, LOCALE_EXTRA_FEC_S2_QPSK_3_5 },
	{ FEC_S2_QPSK_4_5, LOCALE_EXTRA_FEC_S2_QPSK_4_5 },
	{ FEC_S2_QPSK_9_10, LOCALE_EXTRA_FEC_S2_QPSK_9_10 },

	//{ FEC_S2_8PSK_1_2, LOCALE_EXTRA_FEC_S2_8PSK_1_2 },
	{ FEC_S2_8PSK_2_3, LOCALE_EXTRA_FEC_S2_8PSK_2_3 },
	{ FEC_S2_8PSK_3_4, LOCALE_EXTRA_FEC_S2_8PSK_3_4 },
	{ FEC_S2_8PSK_3_5, LOCALE_EXTRA_FEC_S2_8PSK_3_5 },
	{ FEC_S2_8PSK_5_6, LOCALE_EXTRA_FEC_S2_8PSK_5_6 },
	//{ FEC_S2_8PSK_7_8, LOCALE_EXTRA_FEC_S2_8PSK_7_8 },
	{ FEC_S2_8PSK_8_9, LOCALE_EXTRA_FEC_S2_8PSK_8_9 },
	//{ FEC_S2_8PSK_4_5, LOCALE_EXTRA_FEC_S2_8PSK_4_5 },
	{ FEC_S2_8PSK_9_10, LOCALE_EXTRA_FEC_S2_8PSK_9_10 }
};

#define SATSETUP_SCANTP_MOD_COUNT 5
const CMenuOptionChooser::keyval SATSETUP_SCANTP_MOD[SATSETUP_SCANTP_MOD_COUNT] =
{
	{ 1, LOCALE_EXTRA_TP_MOD_16 },
	{ 2, LOCALE_EXTRA_TP_MOD_32 },
	{ 3, LOCALE_EXTRA_TP_MOD_64 },
	{ 4, LOCALE_EXTRA_TP_MOD_128},
	{ 5, LOCALE_EXTRA_TP_MOD_256}
};

#define SATSETUP_SCANTP_POL_COUNT 4
const CMenuOptionChooser::keyval SATSETUP_SCANTP_POL[SATSETUP_SCANTP_POL_COUNT] =
{
	{ 0, LOCALE_EXTRA_TP_POL_H },
	{ 1, LOCALE_EXTRA_TP_POL_V },
	{ 2, LOCALE_EXTRA_TP_POL_L },
	{ 3, LOCALE_EXTRA_TP_POL_R }
};

#define OPTIONS_SOUTH0_NORTH1_OPTION_COUNT 2
const CMenuOptionChooser::keyval OPTIONS_SOUTH0_NORTH1_OPTIONS[OPTIONS_SOUTH0_NORTH1_OPTION_COUNT] =
{
	{ 0, LOCALE_EXTRA_SOUTH },
	{ 1, LOCALE_EXTRA_NORTH }
};

#define OPTIONS_EAST0_WEST1_OPTION_COUNT 2
const CMenuOptionChooser::keyval OPTIONS_EAST0_WEST1_OPTIONS[OPTIONS_EAST0_WEST1_OPTION_COUNT] =
{
	{ 0, LOCALE_EXTRA_EAST },
	{ 1, LOCALE_EXTRA_WEST }
};

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

#define SATSETUP_FRONTEND_MODE_COUNT 5
const CMenuOptionChooser::keyval SATSETUP_FRONTEND_MODE[SATSETUP_FRONTEND_MODE_COUNT] =
{
	{ CFrontend::FE_MODE_UNUSED,      LOCALE_SATSETUP_FE_MODE_UNUSED      },
	{ CFrontend::FE_MODE_INDEPENDENT, LOCALE_SATSETUP_FE_MODE_INDEPENDENT },
	{ CFrontend::FE_MODE_MASTER,      LOCALE_SATSETUP_FE_MODE_MASTER      },
	{ CFrontend::FE_MODE_LINK_LOOP,   LOCALE_SATSETUP_FE_MODE_LINK_LOOP   },
	{ CFrontend::FE_MODE_LINK_TWIN,   LOCALE_SATSETUP_FE_MODE_LINK_TWIN   },
};

CScanSetup::CScanSetup(bool wizard_mode)
{
	width = w_max (40, 10);
	r_system = g_info.delivery_system;
	fec_count = (r_system == DVB_S) ? SATSETUP_SCANTP_FEC_COUNT : CABLESETUP_SCANTP_FEC_COUNT;
	freq_length = (r_system == DVB_S) ? 8 : 6;

	is_wizard = wizard_mode;

	//define caption of some forwarders and widgets depends of current receiver type
	switch (r_system)
	{
		case DVB_S:
			satprov_locale = LOCALE_SATSETUP_SATELLITE;
			break;
		case DVB_T:
			satprov_locale = LOCALE_TERRESTRIALSETUP_PROVIDER;
			break;
		case DVB_C:
		default:
			satprov_locale = LOCALE_CABLESETUP_PROVIDER;
			break;
	}

	satSelect 	= NULL;
	cableSelect 	= NULL;
	terrSelect	= NULL;
	satOnOff	= NULL;
	fautoScanAll	= NULL;
	frontendSetup	= NULL;
	fsatSetup	= NULL;
	fsatSelect	= NULL;
	dtype		= NULL;
	uniSetup	= NULL;
	ojDiseqcRepeats	= NULL;
	nid		= NULL;
	lcnhd		= NULL;
	linkfe		= NULL;
}

CScanSetup* CScanSetup::getInstance()
{
	static CScanSetup* scs = NULL;

	if(!scs)
	{
		scs = new CScanSetup();
		printf("[neutrino] ScanSetup Instance created\n");
	}
	return scs;
}

CScanSetup::~CScanSetup()
{
}

int CScanSetup::exec(CMenuTarget* parent, const std::string &actionKey)
{
	int   res = menu_return::RETURN_REPAINT;
	std::string::size_type loc;

	if (parent)
		parent->hide();

	printf("[neutrino] CScanSetup::%s: %s \n", __FUNCTION__, actionKey.c_str());
	if(actionKey == "save_scansettings")
	{
		printf("[neutrino] CScanSetup::%s save_scansettings...\n", __FUNCTION__);
		saveScanSetup();
		return res;
	}
	else if(actionKey == "reloadchannels")
	{
		printf("[neutrino] CScanSetup::%s reloadchannels...\n", __FUNCTION__);
		if (reloadhintBox)
			reloadhintBox->paint();
		/* save if changed, to make sure NEW/REMOVED/... flags are updated */
		CServiceManager::getInstance()->SaveServices(true, true);
		/* Z->reinitChannels triggers EVT_SERVICESCHANGED and this triggers channelsInit() */
		g_Zapit->reinitChannels();
		if (reloadhintBox)
			reloadhintBox->hide();
		CNeutrinoApp::getInstance ()->SDTreloadChannels = false;
		return res;
	}
	else if(actionKey == "satsetup")
	{
		return showScanMenuLnbSetup();
	}
	else if(actionKey == "unisetup")
	{
		return showUnicableSetup();
	}
	else if(actionKey == "satfind")
	{
		return showScanMenuSatFind();
	}
	else if(actionKey == "setup_frontend")
	{
		return showScanMenuFrontendSetup();
	}
	else if((loc = actionKey.find("config_frontend", 0)) != std::string::npos)
	{
		int number = actionKey.at(15) - '0';
		printf("[neutrino] CScanSetup::%s %s, fe %d\n", __FUNCTION__, actionKey.c_str(), number);
		return showFrontendSetup(number);
	}

	//starting scan
	if(actionKey == "cable") {
		printf("[neutrino] CScanSetup::%s: simple cable scan\n", __FUNCTION__);
		saveScanSetup();
		/* for simple cable scan, force some options */
		scansettings.scan_reset_numbers = 0;
		scansettings.scan_nit_manual = 1;
		scansettings.scan_fta_flag = 0;
		if (scansettings.scan_logical_numbers) {
			g_settings.keep_channel_numbers = 1;
			CServiceManager::getInstance()->KeepNumbers(g_settings.keep_channel_numbers);
		}
		CScanTs scanTs(FE_QAM);
		scanTs.exec(NULL, "manual");
		return res;
	}
	std::string scants_key[] = {"all", "manual", "test", "fast", "auto"/*doesn't exists in CScanTs!*/};

	if (actionKey.size() > 1) {
		int delsys;
		switch (actionKey[0]) {
			case 's': delsys = FE_QPSK; break;
			case 't': delsys = FE_OFDM; break;
			default:  delsys = FE_QAM;  break;
		}
		std::string as = actionKey.substr(1);
		printf("[neutrino] CScanSetup::%s scan %c in %s mode...\n", __FUNCTION__, actionKey[0], as.c_str());
		for (uint i=0; i< (sizeof(scants_key)/sizeof(scants_key[0])); i++)
		{
			if (as == scants_key[i])
			{
				printf("[neutrino] CScanSetup::%s %s...\n", __FUNCTION__, scants_key[i].c_str());
				//ensure that be saved all settings before scan...
				saveScanSetup();
				//...then start scan
				CScanTs scanTs(delsys);
				scanTs.exec(NULL, scants_key[i]);
				return res;
			}
		}
	}


	printf("[neutrino] CScanSetup %s: init scan setup (Mode: %d)...\n",__FUNCTION__ , is_wizard);
	CZapit::getInstance()->GetConfig(zapitCfg);
	res = showScanMenu();

	return res;
}

//scale to max sat/cable name lenght
unsigned int CScanSetup::getSatMenuListWidth()
{
	printf("[neutrino] CScanSetup call %s...\n", __FUNCTION__);
	unsigned int sat_txt_w = 0, max_txt_w = 0;

	satellite_map_t satmap = CServiceManager::getInstance()->SatelliteList();
	for(sat_iterator_t it = satmap.begin(); it != satmap.end(); ++it) {
		sat_txt_w = g_Font[SNeutrinoSettings::FONT_TYPE_MENU]->getRenderWidth(it->second.name.c_str(), true);
		max_txt_w = std::max(max_txt_w, sat_txt_w);
	}

	int w, h;
	const unsigned int mini_w = width;//mini width
	CFrameBuffer::getInstance()->getIconSize(NEUTRINO_ICON_BUTTON_RED, &w, &h);
	max_txt_w += (w*2) +30;
	max_txt_w = std::min(max_txt_w, CFrameBuffer::getInstance()->getScreenWidth());
	max_txt_w = max_txt_w*100/CFrameBuffer::getInstance()->getScreenWidth();
	max_txt_w = std::max(max_txt_w,mini_w);

	return max_txt_w;
}

int CScanSetup::showScanMenu()
{
	printf("[neutrino] CScanSetup call %s...\n", __FUNCTION__);
	int shortcut = 1;
	int w = getSatMenuListWidth();
printf("C: %d S: %d T: %d\n", CFEManager::getInstance()->haveCable(),CFEManager::getInstance()->haveSat(), CFEManager::getInstance()->haveTerr());

	CMenuForwarder  * mf;
	CMenuOptionChooser * mc;

	allow_start = !CRecordManager::getInstance()->RecordingStatus() || CRecordManager::getInstance()->TimeshiftOnly();

	//main
	CMenuWidget * settings = new CMenuWidget(is_wizard ? LOCALE_SERVICEMENU_SCANTS : LOCALE_SERVICEMENU_HEAD, NEUTRINO_ICON_SETTINGS, width, MN_WIDGET_ID_SCAN_MAIN);
	settings->setWizardMode(is_wizard);

	//back
	settings->addIntroItems(is_wizard ? NONEXISTANT_LOCALE : LOCALE_SERVICEMENU_SCANTS);
	//----------------------------------------------------------------------
	//save scan settings
	mf = new CMenuForwarder(LOCALE_MAINSETTINGS_SAVESETTINGSNOW, true, NULL, this, "save_scansettings", CRCInput::RC_red, NEUTRINO_ICON_BUTTON_RED);
	mf->setHint("", LOCALE_MENU_HINT_SCAN_SAVESETTINGS);
	settings->addItem(mf);
	//----------------------------------------------------------------------
	settings->addItem(GenericMenuSeparatorLine);
	//----------------------------------------------------------------------
	//service select mode
	mc = new CMenuOptionChooser(LOCALE_ZAPIT_SCANTYPE, (int *)&scansettings.scanType, SCANTS_ZAPIT_SCANTYPE, SCANTS_ZAPIT_SCANTYPE_COUNT, true, NULL, CRCInput::convertDigitToKey(shortcut++), "", true);
	mc->setHint("", LOCALE_MENU_HINT_SCAN_SCANTYPE);
	settings->addItem(mc);

	//bouquet generate mode
	mc = new CMenuOptionChooser(LOCALE_SCANTS_BOUQUET, (int *)&scansettings.bouquetMode, SCANTS_BOUQUET_OPTIONS, SCANTS_BOUQUET_OPTION_COUNT, true, NULL, CRCInput::convertDigitToKey(shortcut++), "", true);
	mc->setHint("", LOCALE_MENU_HINT_SCAN_BOUQUET);
	settings->addItem(mc);

	//sat/provider selector

	if(CFEManager::getInstance()->haveSat() || CFEManager::getInstance()->getFrontendCount() > 1) {
#if 0
		CMenuWidget * setupMenu = new CMenuWidget(LOCALE_SATSETUP_FE_SETUP, NEUTRINO_ICON_SETTINGS, width, MN_WIDGET_ID_SCAN_FE_SETUP);
		addScanMenuFrontendSetup(setupMenu);
		mf = new CMenuDForwarder(LOCALE_SATSETUP_FE_SETUP, allow_start, NULL, setupMenu, "", CRCInput::convertDigitToKey(shortcut++));
#endif
		mf = new CMenuForwarder(LOCALE_SATSETUP_FE_SETUP, allow_start, NULL, this, "setup_frontend", CRCInput::convertDigitToKey(shortcut++));
		mf->setHint("", LOCALE_MENU_HINT_SCAN_FESETUP);
		settings->addItem(mf);
	}

	if (CFEManager::getInstance()->haveSat()) {
		r_system = DVB_S;

		settings->addItem(new CMenuSeparator(CMenuSeparator::LINE | CMenuSeparator::STRING, LOCALE_SCANTS_PREVERENCES_RECEIVING_SYSTEM));

		satSelect = new CMenuOptionStringChooser(LOCALE_SATSETUP_SATELLITE, scansettings.satName, true, this, CRCInput::RC_red, NEUTRINO_ICON_BUTTON_RED, true);
		satSelect->setHint("", LOCALE_MENU_HINT_SCAN_SATELLITE);

		satOnOff = new CMenuWidget(LOCALE_SATSETUP_SATELLITE, NEUTRINO_ICON_SETTINGS, width);

#if 0
		CMenuWidget * setupMenu = new CMenuWidget(LOCALE_SATSETUP_FE_SETUP, NEUTRINO_ICON_SETTINGS, width, MN_WIDGET_ID_SCAN_FE_SETUP);
		addScanMenuFrontendSetup(setupMenu);
		mf = new CMenuDForwarder(LOCALE_SATSETUP_FE_SETUP, allow_start, NULL, setupMenu, "", CRCInput::convertDigitToKey(shortcut++));
		mf->setHint("", LOCALE_MENU_HINT_SCAN_FESETUP);
		settings->addItem(mf);
#endif
		/* add configured satellites to satSelect */
		fillSatSelect(satSelect);

		//auto scan
		char autoscan[64];
		std::string s_capt_part = g_Locale->getText(LOCALE_SATSETUP_SATELLITE);
		snprintf(autoscan, 64, g_Locale->getText(LOCALE_SATSETUP_AUTO_SCAN), s_capt_part.c_str());

		/* FIXME leak, satSelect added to both auto and manual scan, so one of them cannot be deleted */
		CMenuWidget * autoScan = new CMenuWidget(LOCALE_SERVICEMENU_SCANTS, NEUTRINO_ICON_SETTINGS, w/*width*/, MN_WIDGET_ID_SCAN_AUTO_SCAN);
		addScanMenuAutoScan(autoScan);
		mf = new CMenuForwarderNonLocalized(autoscan, true, NULL, autoScan, "", CRCInput::RC_green, NEUTRINO_ICON_BUTTON_GREEN);
		mf->setHint("", LOCALE_MENU_HINT_SCAN_AUTO);
		settings->addItem(mf);

		//manual scan
		CMenuWidget * manualScan = new CMenuWidget(LOCALE_SATSETUP_MANUAL_SCAN, NEUTRINO_ICON_SETTINGS, w/*width*/, MN_WIDGET_ID_SCAN_MANUAL_SCAN);
		addScanMenuManualScan(manualScan);
		mf = new CMenuForwarder(LOCALE_SATSETUP_MANUAL_SCAN, true, NULL, manualScan, "", CRCInput::RC_yellow, NEUTRINO_ICON_BUTTON_YELLOW);
		mf->setHint("", LOCALE_MENU_HINT_SCAN_MANUAL);
		settings->addItem(mf);
		//auto scan all
		CMenuWidget * autoScanAll = new CMenuWidget(LOCALE_SATSETUP_AUTO_SCAN_ALL, NEUTRINO_ICON_SETTINGS, w/*width*/, MN_WIDGET_ID_SCAN_AUTO_SCAN_ALL);
		addScanMenuAutoScanAll(autoScanAll);
		fautoScanAll = new CMenuDForwarder(LOCALE_SATSETUP_AUTO_SCAN_ALL, true /*(dmode != NO_DISEQC)*/, NULL, autoScanAll, "", CRCInput::RC_blue, NEUTRINO_ICON_BUTTON_BLUE);
		fautoScanAll->setHint("", LOCALE_MENU_HINT_SCAN_AUTOALL);
		settings->addItem(fautoScanAll);
#ifdef ENABLE_FASTSCAN
		//fast scan
		CMenuWidget * fastScanMenu = new CMenuWidget(LOCALE_SATSETUP_FASTSCAN_HEAD, NEUTRINO_ICON_SETTINGS, MN_WIDGET_ID_SCAN_FAST_SCAN);
		addScanMenuFastScan(fastScanMenu);
		mf = new CMenuDForwarder(LOCALE_SATSETUP_FASTSCAN_HEAD, true, NULL, fastScanMenu, "", CRCInput::convertDigitToKey(shortcut++));
		mf->setHint("", LOCALE_MENU_HINT_SCAN_FAST);
		settings->addItem(mf);
#endif
	}
	if (CFEManager::getInstance()->haveCable()) {
		r_system = DVB_C;

		cableSelect = new CMenuOptionStringChooser(LOCALE_CABLESETUP_PROVIDER, scansettings.cableName, true, this, CRCInput::RC_red, NEUTRINO_ICON_BUTTON_RED, true);
		cableSelect->setHint("", LOCALE_MENU_HINT_SCAN_CABLE);
		//--------------------------------------------------------------
		settings->addItem(GenericMenuSeparatorLine);
		//--------------------------------------------------------------
		fillCableSelect(cableSelect);
		//tune timeout
		if(CFEManager::getInstance()->getFrontendCount() <= 1) {
			CMenuOptionNumberChooser * nc = new CMenuOptionNumberChooser(LOCALE_EXTRA_ZAPIT_FE_TIMEOUT, (int *)&zapitCfg.feTimeout, true, 6, 100);
			nc->setHint("", LOCALE_MENU_HINT_SCAN_FETIMEOUT);
			settings->addItem(nc);
		}
		nid = new CIntInput(LOCALE_SATSETUP_CABLE_NID, (int&) scansettings.cable_nid, 5, NONEXISTANT_LOCALE, NONEXISTANT_LOCALE);

		//auto scan
		char autoscan[64];
		std::string s_capt_part = g_Locale->getText(LOCALE_CABLESETUP_PROVIDER);
		snprintf(autoscan, 64, g_Locale->getText(LOCALE_SATSETUP_AUTO_SCAN), s_capt_part.c_str());
		bool have_sat = CFEManager::getInstance()->haveSat();

		/* FIXME leak, satSelect added to both auto and manual scan, so one of them cannot be deleted */
		CMenuWidget * autoScan = new CMenuWidget(LOCALE_SERVICEMENU_SCANTS, NEUTRINO_ICON_SETTINGS, w/*width*/, MN_WIDGET_ID_SCAN_AUTO_SCAN);
		addScanMenuAutoScan(autoScan);
		mf = new CMenuForwarderNonLocalized(autoscan, true, NULL, autoScan, "", have_sat ? CRCInput::RC_nokey : CRCInput::RC_green, have_sat ? NULL : NEUTRINO_ICON_BUTTON_GREEN);
		mf->setHint("", LOCALE_MENU_HINT_SCAN_AUTO);
		settings->addItem(mf);

		//manual scan
		CMenuWidget * manualScan = new CMenuWidget(LOCALE_SATSETUP_MANUAL_SCAN, NEUTRINO_ICON_SETTINGS, w/*width*/, MN_WIDGET_ID_SCAN_MANUAL_SCAN);
		addScanMenuManualScan(manualScan);
		mf = new CMenuForwarder(LOCALE_SATSETUP_MANUAL_SCAN, true, NULL, manualScan, "", have_sat ? CRCInput::RC_nokey : CRCInput::RC_yellow, have_sat ? NULL : NEUTRINO_ICON_BUTTON_YELLOW);
		mf->setHint("", LOCALE_MENU_HINT_SCAN_MANUAL);
		settings->addItem(mf);
		//simple cable scan
		CMenuWidget * cableScan = new CMenuWidget(LOCALE_SATSETUP_CABLE, NEUTRINO_ICON_SETTINGS, w/*width*/, MN_WIDGET_ID_SCAN_CABLE_SCAN);
		addScanMenuCable(cableScan);
		CMenuForwarder * fcableScan = new CMenuDForwarder(LOCALE_SATSETUP_CABLE, true, NULL, cableScan, "", have_sat ? CRCInput::RC_nokey : CRCInput::RC_blue, have_sat ? NULL : NEUTRINO_ICON_BUTTON_BLUE);
		fcableScan->setHint("", LOCALE_MENU_HINT_SCAN_CABLE_SIMPLE);
		settings->addItem(fcableScan);
	}
	if (CFEManager::getInstance()->haveTerr()) {
		r_system = DVB_T;

		terrSelect = new CMenuOptionStringChooser(LOCALE_TERRESTRIALSETUP_PROVIDER, scansettings.terrName, true, this, CRCInput::RC_red, NEUTRINO_ICON_BUTTON_RED, true);
		//terrSelect->setHint("", LOCALE_MENU_HINT_SCAN_CABLE);
		//--------------------------------------------------------------
		settings->addItem(GenericMenuSeparatorLine);
		//--------------------------------------------------------------
		fillCableSelect(terrSelect);
		// tune timeout, "Setup tuner" is not shown for only one non-sat tuner
		if (CFEManager::getInstance()->getFrontendCount() <= 1) {
			CMenuOptionNumberChooser * nc = new CMenuOptionNumberChooser(LOCALE_EXTRA_ZAPIT_FE_TIMEOUT, (int *)&zapitCfg.feTimeout, true, 6, 100);
			nc->setHint("", LOCALE_MENU_HINT_SCAN_FETIMEOUT);
			settings->addItem(nc);
		}

		//auto scan
		char autoscan[64];
		std::string s_capt_part = g_Locale->getText(LOCALE_TERRESTRIALSETUP_PROVIDER);
		snprintf(autoscan, 64, g_Locale->getText(LOCALE_SATSETUP_AUTO_SCAN), s_capt_part.c_str());
		bool have_other = ! CFEManager::getInstance()->terrOnly();

		/* FIXME leak, satSelect added to both auto and manual scan, so one of them cannot be deleted */
		CMenuWidget * autoScan = new CMenuWidget(LOCALE_SERVICEMENU_SCANTS, NEUTRINO_ICON_SETTINGS, w, MN_WIDGET_ID_SCAN_AUTO_SCAN);
		addScanMenuAutoScan(autoScan);
		mf = new CMenuForwarderNonLocalized(autoscan, true, NULL, autoScan, "", have_other ? CRCInput::RC_nokey : CRCInput::RC_green, have_other ? NULL : NEUTRINO_ICON_BUTTON_GREEN);
		mf->setHint("", LOCALE_MENU_HINT_SCAN_AUTO);
		settings->addItem(mf);

		//manual scan
		CMenuWidget * manualScan = new CMenuWidget(LOCALE_SATSETUP_MANUAL_SCAN, NEUTRINO_ICON_SETTINGS, w, MN_WIDGET_ID_SCAN_MANUAL_SCAN);
		addScanMenuManualScan(manualScan);
		mf = new CMenuForwarder(LOCALE_SATSETUP_MANUAL_SCAN, true, NULL, manualScan, "", have_other ? CRCInput::RC_nokey : CRCInput::RC_yellow, have_other ? NULL : NEUTRINO_ICON_BUTTON_YELLOW);
		mf->setHint("", LOCALE_MENU_HINT_SCAN_MANUAL);
		settings->addItem(mf);
	}
#if 0
	//--------------------------------------------------------------
	settings->addItem(new CMenuSeparator(CMenuSeparator::LINE | CMenuSeparator::STRING, LOCALE_SCANTS_PREVERENCES_SCAN));
	//--------------------------------------------------------------

	int w = getSatMenuListWidth();

	//auto scan
	char autoscan[64];
	std::string s_capt_part = g_Locale->getText(satprov_locale);
	snprintf(autoscan, 64, g_Locale->getText(LOCALE_SATSETUP_AUTO_SCAN), s_capt_part.c_str());

	/* FIXME leak, satSelect added to both auto and manual scan, so one of them cannot be deleted */
	CMenuWidget * autoScan = new CMenuWidget(LOCALE_SERVICEMENU_SCANTS, NEUTRINO_ICON_SETTINGS, w/*width*/, MN_WIDGET_ID_SCAN_AUTO_SCAN);
	addScanMenuAutoScan(autoScan);
	mf = new CMenuForwarderNonLocalized(autoscan, true, NULL, autoScan, "", CRCInput::RC_green, NEUTRINO_ICON_BUTTON_GREEN);
	mf->setHint("", LOCALE_MENU_HINT_SCAN_AUTO);
	settings->addItem(mf);

	//manual scan
	CMenuWidget manualScan(LOCALE_SATSETUP_MANUAL_SCAN, NEUTRINO_ICON_SETTINGS, w/*width*/, MN_WIDGET_ID_SCAN_MANUAL_SCAN);
	addScanMenuManualScan(&manualScan);
	mf = new CMenuForwarder(LOCALE_SATSETUP_MANUAL_SCAN, true, NULL, &manualScan, "", CRCInput::RC_yellow, NEUTRINO_ICON_BUTTON_YELLOW);
	mf->setHint("", LOCALE_MENU_HINT_SCAN_MANUAL);
	settings->addItem(mf);

	if (r_system == DVB_S)
	{
		//auto scan all
		CMenuWidget * autoScanAll = new CMenuWidget(LOCALE_SATSETUP_AUTO_SCAN_ALL, NEUTRINO_ICON_SETTINGS, w/*width*/, MN_WIDGET_ID_SCAN_AUTO_SCAN_ALL);
		addScanMenuAutoScanAll(autoScanAll);
		fautoScanAll = new CMenuDForwarder(LOCALE_SATSETUP_AUTO_SCAN_ALL, true /*(dmode != NO_DISEQC)*/, NULL, autoScanAll, "", CRCInput::RC_blue, NEUTRINO_ICON_BUTTON_BLUE);
		fautoScanAll->setHint("", LOCALE_MENU_HINT_SCAN_AUTOALL);
		settings->addItem(fautoScanAll);
#ifdef ENABLE_FASTSCAN
		//fast scan
		CMenuWidget * fastScanMenu = new CMenuWidget(LOCALE_SATSETUP_FASTSCAN_HEAD, NEUTRINO_ICON_SETTINGS, MN_WIDGET_ID_SCAN_FAST_SCAN);
		addScanMenuFastScan(fastScanMenu);
		mf = new CMenuDForwarder(LOCALE_SATSETUP_FASTSCAN_HEAD, true, NULL, fastScanMenu, "", CRCInput::convertDigitToKey(shortcut++));
		mf->setHint("", LOCALE_MENU_HINT_SCAN_FAST);
		settings->addItem(mf);
#endif /*ENABLE_FASTSCAN*/
	}
	else if (r_system == DVB_C) //cable
	{
		CMenuWidget * cableScan = new CMenuWidget(LOCALE_SATSETUP_CABLE, NEUTRINO_ICON_SETTINGS, w/*width*/, MN_WIDGET_ID_SCAN_CABLE_SCAN);
		addScanMenuCable(cableScan);
		CMenuForwarder * fcableScan = new CMenuDForwarder(LOCALE_SATSETUP_CABLE, true, NULL, cableScan, "", CRCInput::RC_blue, NEUTRINO_ICON_BUTTON_BLUE);
		fcableScan->setHint("", LOCALE_MENU_HINT_SCAN_CABLE_SIMPLE);
		settings->addItem(fcableScan);
	}
#endif
	int res = settings->exec(NULL, "");

	//delete satSelect;
	delete satOnOff;
	delete settings;
	delete nid;
	return res;
}

neutrino_locale_t CScanSetup::getModeLocale(int mode)
{
	neutrino_locale_t lmode = LOCALE_SATSETUP_FE_MODE_UNUSED;
	if (mode == CFrontend::FE_MODE_INDEPENDENT)
		lmode =  LOCALE_SATSETUP_FE_MODE_INDEPENDENT;
	else if (mode == CFrontend::FE_MODE_MASTER)
		lmode =  LOCALE_SATSETUP_FE_MODE_MASTER;
	else if (mode == CFrontend::FE_MODE_LINK_LOOP)
		lmode =  LOCALE_SATSETUP_FE_MODE_LINK_LOOP;
	else if (mode == CFrontend::FE_MODE_LINK_TWIN)
		lmode =  LOCALE_SATSETUP_FE_MODE_LINK_TWIN;

	return lmode;
}

int CScanSetup::showScanMenuFrontendSetup()
{
	CMenuForwarder * mf;
	int shortcut = 1;

	fe_restart = false;

	CMenuWidget * setupMenu = new CMenuWidget(LOCALE_SATSETUP_FE_SETUP, NEUTRINO_ICON_SETTINGS, width, MN_WIDGET_ID_SCAN_FE_SETUP);
	setupMenu->addIntroItems();

	int count = CFEManager::getInstance()->getFrontendCount();

	for(int i = 0; i < count; i++) {
		CFrontend * fe = CFEManager::getInstance()->getFE(i);

		char tmp[32];
		snprintf(tmp, sizeof(tmp), "config_frontend%d", i);

		char name[255];
		snprintf(name, sizeof(name), "%s %d: %s", g_Locale->getText(LOCALE_SATSETUP_FE_SETUP), i+1, fe->getInfo()->name);

		const char * icon = NULL;
		neutrino_msg_t key = CRCInput::RC_nokey;
		if (i == 0) {
			key = CRCInput::RC_red;
			icon = NEUTRINO_ICON_BUTTON_RED;
		} else if (i == 1) {
			key = CRCInput::RC_green;
			icon = NEUTRINO_ICON_BUTTON_GREEN;
		} else if (i == 2) {
			key = CRCInput::RC_yellow;
			icon = NEUTRINO_ICON_BUTTON_YELLOW;
		} else if (i == 3) {
			key = CRCInput::RC_blue;
			icon = NEUTRINO_ICON_BUTTON_BLUE;
		}

		modestr[i] = g_Locale->getText(getModeLocale(fe->getMode()));
		mf = new CMenuForwarderNonLocalized(name, true, modestr[i], this, tmp, key, icon);
		mf->setHint("", LOCALE_MENU_HINT_SCAN_SETUP_FE);
		setupMenu->addItem(mf);
		if(i != 0)
			frontendSetup = mf;
	}
	CMenuOptionNumberChooser * nc = new CMenuOptionNumberChooser(LOCALE_EXTRA_ZAPIT_FE_TIMEOUT, (int *)&zapitCfg.feTimeout, true, 6, 100);
	nc->setHint("", LOCALE_MENU_HINT_SCAN_FETIMEOUT);
	setupMenu->addItem(nc);

	sprintf(zapit_lat, "%02.6f", zapitCfg.gotoXXLatitude);
	sprintf(zapit_long, "%02.6f", zapitCfg.gotoXXLongitude);

	setupMenu->addItem(new CMenuSeparator(CMenuSeparator::LINE | CMenuSeparator::STRING, LOCALE_SATSETUP_EXTENDED_MOTOR));
	CMenuOptionChooser * mc = new CMenuOptionChooser(LOCALE_EXTRA_LADIRECTION,  (int *)&zapitCfg.gotoXXLaDirection, OPTIONS_SOUTH0_NORTH1_OPTIONS, OPTIONS_SOUTH0_NORTH1_OPTION_COUNT, true, NULL, CRCInput::convertDigitToKey(shortcut++));
	mc->setHint("", LOCALE_MENU_HINT_SCAN_LADIRECTION);
	setupMenu->addItem(mc);

	CStringInput * toff1 = new CStringInput(LOCALE_EXTRA_LATITUDE, (char *) zapit_lat, 10, NONEXISTANT_LOCALE, NONEXISTANT_LOCALE, "0123456789.");
	mf = new CMenuDForwarder(LOCALE_EXTRA_LATITUDE, true, zapit_lat, toff1, "", CRCInput::convertDigitToKey(shortcut++));
	mf->setHint("", LOCALE_MENU_HINT_SCAN_LATITUDE);
	setupMenu->addItem(mf);

	mc = new CMenuOptionChooser(LOCALE_EXTRA_LODIRECTION,  (int *)&zapitCfg.gotoXXLoDirection, OPTIONS_EAST0_WEST1_OPTIONS, OPTIONS_EAST0_WEST1_OPTION_COUNT, true, NULL, CRCInput::convertDigitToKey(shortcut++));
	mc->setHint("", LOCALE_MENU_HINT_SCAN_LODIRECTION);
	setupMenu->addItem(mc);

	CStringInput * toff2 = new CStringInput(LOCALE_EXTRA_LONGITUDE, (char *) zapit_long, 10, NONEXISTANT_LOCALE, NONEXISTANT_LOCALE, "0123456789.");
	mf = new CMenuDForwarder(LOCALE_EXTRA_LONGITUDE, true, zapit_long, toff2, "", CRCInput::convertDigitToKey(shortcut++));
	mf->setHint("", LOCALE_MENU_HINT_SCAN_LONGITUDE);
	setupMenu->addItem(mf);

	nc = new CMenuOptionNumberChooser(LOCALE_SATSETUP_USALS_REPEAT, (int *)&zapitCfg.repeatUsals, true, 0, 10, NULL, 0, 0, LOCALE_OPTIONS_OFF);
	nc->setHint("", LOCALE_MENU_HINT_SCAN_USALS_REPEAT);
	setupMenu->addItem(nc);

	int res = setupMenu->exec(NULL, "");
	delete setupMenu;
	if (fe_restart) {
		fe_restart = false;
		CFEManager::getInstance()->linkFrontends(true);
		t_channel_id live_channel_id = CZapit::getInstance()->GetCurrentChannelID();
		if (live_channel_id)
			CNeutrinoApp::getInstance()->channelList->zapTo_ChannelID(live_channel_id, true);
	}
	return res;
}

int CScanSetup::showFrontendSetup(int number)
{
	int shortcut = 1;
	static int feselected = 0;

	printf("[neutrino] CScanSetup::%s fe %d\n", __FUNCTION__, number);

	fenumber = number;

	CFrontend * fe = CFEManager::getInstance()->getFE(number);
	frontend_config_t & fe_config = fe->getConfig();
	dmode = fe_config.diseqcType;

	char name[255];
	snprintf(name, sizeof(name), "%s %d: %s", g_Locale->getText(LOCALE_SATSETUP_FE_SETUP), number+1, fe->getInfo()->name);

	CMenuWidget * setupMenu = new CMenuWidget(name, NEUTRINO_ICON_SETTINGS, width);
	setupMenu->setSelected(feselected);
	setupMenu->addIntroItems();

	int fecount = CFEManager::getInstance()->getFrontendCount();

	femode = fe->getMode();

	char fename[fecount][255];
	CMenuOptionChooser::keyval_ext feselect[fecount];
	feselect[0].key = -1;
	feselect[0].value = NONEXISTANT_LOCALE;
	feselect[0].valname = "--";

	femaster = -1;
	int select_count = 0;

	/* by default, enable first 2 options */
	int mode_count = 2;
	if (fe->getInfo()->type == FE_QPSK) {
		/* enable master option, check if we have masters to enable link options */
		mode_count = 3;
		for (int i = 0; i < fecount; i++) {
			CFrontend * testfe = CFEManager::getInstance()->getFE(i);
			if (i != fenumber && (fe->getType() == testfe->getType()) && (testfe->getMode() == CFrontend::FE_MODE_MASTER)) {
				int num = testfe->getNumber();
				snprintf(fename[select_count], sizeof(fename[select_count]), "%d: %s", num+1, testfe->getInfo()->name);
				feselect[select_count].key = num;
				feselect[select_count].value = NONEXISTANT_LOCALE;
				feselect[select_count].valname = fename[select_count];
				select_count++;
				if (femaster < 0)
					femaster = num;
			}
		}
	}
	/* if there are master(s), enable all options */
	if (select_count)
		mode_count = SATSETUP_FRONTEND_MODE_COUNT;
	else
		select_count = 1;

	bool enable = true;
	/* disable mode option, if fe is master and there are links - prevent master mode change
	   and hence linked in undefined state */
	if (fe->hasLinks() || fecount == 1)
		enable = false;

	if (CFrontend::linked(femode))
		femaster = fe->getMaster();

	CMenuOptionChooser * mc = new CMenuOptionChooser(LOCALE_SATSETUP_FE_MODE, (int *)&femode, SATSETUP_FRONTEND_MODE, mode_count,
			enable, this, CRCInput::RC_red, NEUTRINO_ICON_BUTTON_RED, true);
	mc->setHint("", LOCALE_MENU_HINT_SCAN_FEMODE);
	setupMenu->addItem(mc);

	if (fe->getInfo()->type == FE_QPSK) {
		/* disable all but mode option for linked frontends */
		bool allow_moptions = !CFrontend::linked(femode) && femode != CFrontend::FE_MODE_UNUSED;

		if (fecount > 1) {
			/* link to master select */
			linkfe = new CMenuOptionChooser(LOCALE_SATSETUP_FE_MODE_MASTER, &femaster, feselect, select_count, CFrontend::linked(femode), this, CRCInput::RC_green, NEUTRINO_ICON_BUTTON_GREEN, true);
			linkfe->setHint("", LOCALE_MENU_HINT_SCAN_FELINK);
			setupMenu->addItem(linkfe);
		}
		/* diseqc type select */
		dtype = new CMenuOptionChooser(LOCALE_SATSETUP_DISEQC, (int *)&dmode, SATSETUP_DISEQC_OPTIONS, SATSETUP_DISEQC_OPTION_COUNT,
				femode != CFrontend::FE_MODE_UNUSED && femode != CFrontend::FE_MODE_LINK_LOOP,
				this, CRCInput::convertDigitToKey(shortcut++), "", true);
		dtype->setHint("", LOCALE_MENU_HINT_SCAN_DISEQCTYPE);
		setupMenu->addItem(dtype);

		/* diseqc repeats */
		ojDiseqcRepeats = new CMenuOptionNumberChooser(LOCALE_SATSETUP_DISEQCREPEAT, (int *)&fe_config.diseqcRepeats, allow_moptions && (dmode != NO_DISEQC) && (dmode != DISEQC_ADVANCED), 0, 2, NULL);
		ojDiseqcRepeats->setHint("", LOCALE_MENU_HINT_SCAN_DISEQCREPEAT);
		setupMenu->addItem(ojDiseqcRepeats);

		/* diseqc cmd order select */
		dorder = new CMenuOptionChooser(LOCALE_SATSETUP_DISEQC_ORDER, (int *)&fe_config.diseqc_order, DISEQC_ORDER_OPTIONS, DISEQC_ORDER_OPTION_COUNT,
				allow_moptions && (dmode == DISEQC_ADVANCED),
				this, CRCInput::convertDigitToKey(shortcut++), "", true);
		dorder->setHint("", LOCALE_MENU_HINT_SCAN_DISEQCORDER);
		setupMenu->addItem(dorder);

		CMenuWidget * satToSelect = new CMenuWidget(LOCALE_SATSETUP_SELECT_SAT, NEUTRINO_ICON_SETTINGS, width);
		satToSelect->addIntroItems();

		satellite_map_t & satmap = fe->getSatellites();
		for (sat_iterator_t sit = satmap.begin(); sit != satmap.end(); ++sit)
		{
			std::string satname = CServiceManager::getInstance()->GetSatelliteName(sit->first);
			mc = new CMenuOptionChooser(satname.c_str(),  &sit->second.configured, OPTIONS_OFF0_ON1_OPTIONS, OPTIONS_OFF0_ON1_OPTION_COUNT, true);
			mc->setHint("", LOCALE_MENU_HINT_SCAN_SATENABLE);
			satToSelect->addItem(mc);
		}
		fsatSelect = new CMenuForwarder(LOCALE_SATSETUP_SELECT_SAT, allow_moptions, NULL, satToSelect, "", CRCInput::convertDigitToKey(shortcut++));
		fsatSelect->setHint("", LOCALE_MENU_HINT_SCAN_SATADD);
		setupMenu->addItem(fsatSelect);

		fsatSetup 	= new CMenuForwarder(LOCALE_SATSETUP_SAT_SETUP, allow_moptions, NULL, this, "satsetup", CRCInput::convertDigitToKey(shortcut++));
		fsatSetup->setHint("", LOCALE_MENU_HINT_SCAN_SATSETUP);
		setupMenu->addItem(fsatSetup);

		uniSetup	= new CMenuForwarder(LOCALE_SATSETUP_UNI_SETTINGS, (dmode == DISEQC_UNICABLE), NULL, this, "unisetup", CRCInput::convertDigitToKey(shortcut++));
		setupMenu->addItem(uniSetup);

		setupMenu->addItem(new CMenuSeparator(CMenuSeparator::LINE | CMenuSeparator::STRING, LOCALE_SATSETUP_EXTENDED_MOTOR));
		CMenuOptionNumberChooser * nc = new CMenuOptionNumberChooser(LOCALE_EXTRA_ZAPIT_MOTOR_SPEED, (int *)&fe_config.motorRotationSpeed, true, 0, 64, NULL);
		nc->setHint("", LOCALE_MENU_HINT_SCAN_MOTOR_SPEED);
		setupMenu->addItem(nc);

		mc = new CMenuOptionChooser(LOCALE_EXTRA_ZAPIT_HVOLTAGE,  (int *)&fe_config.highVoltage, OPTIONS_OFF0_ON1_OPTIONS, OPTIONS_OFF0_ON1_OPTION_COUNT, true);
		mc->setHint("", LOCALE_MENU_HINT_SCAN_MOTOR_18V);
		setupMenu->addItem(mc);

		mc = new CMenuOptionChooser(LOCALE_SATSETUP_USE_USALS,  &all_usals, OPTIONS_OFF0_ON1_OPTIONS, OPTIONS_OFF0_ON1_OPTION_COUNT, true, this);
		mc->setHint("", LOCALE_MENU_HINT_SCAN_USALSALL);
		setupMenu->addItem(mc);

		CMenuForwarder * mf = new CMenuForwarder(LOCALE_MOTORCONTROL_HEAD, true, NULL, this, "satfind", CRCInput::RC_blue, NEUTRINO_ICON_BUTTON_BLUE);
		mf->setHint("", LOCALE_MENU_HINT_SCAN_SATFIND);
		setupMenu->addItem(mf);
	}

	int res = setupMenu->exec(NULL, "");
	feselected = setupMenu->getSelected();

	/* add configured satellites to satSelect in case they changed */
	if (fe->isSat())
		fillSatSelect(satSelect);

	delete setupMenu;
	linkfe = NULL;
	/* re-link in case mode changed, without fe init */
	CFEManager::getInstance()->linkFrontends(false);
	/* copy settings from master, if it set and fe linked */
	CFEManager::getInstance()->copySettings(fe);
	/* apply settings without write file */
	CFEManager::getInstance()->saveSettings(false);
	return res;
}

int CScanSetup::showUnicableSetup()
{
	printf("[neutrino] CScanSetup call %s fe %d\n", __func__, fenumber);

	int res = menu_return::RETURN_REPAINT;
	CFrontend *fe = CFEManager::getInstance()->getFE(fenumber);
	frontend_config_t &fe_config = fe->getConfig();
	int unicable_scr = fe_config.uni_scr;
	int unicable_qrg = fe_config.uni_qrg;

	CMenuOptionNumberChooser *uniscr = new CMenuOptionNumberChooser(LOCALE_UNICABLE_SCR, &unicable_scr, true, 0, 7);
	CIntInput		 *uniqrg = new CIntInput(LOCALE_UNICABLE_QRG, unicable_qrg, 4, NONEXISTANT_LOCALE, NONEXISTANT_LOCALE);

	CMenuWidget *uni_setup = new CMenuWidget(LOCALE_SATSETUP_UNI_SETTINGS, NEUTRINO_ICON_SETTINGS, width);
	uni_setup->addIntroItems();

	uni_setup->addItem(uniscr);
	CMenuForwarder *mf = new CMenuDForwarder(LOCALE_UNICABLE_QRG, true, uniqrg->getValue(), uniqrg);
	uni_setup->addItem(mf);
	res = uni_setup->exec(NULL, "");
	delete uni_setup;
	if (res) {
		fe_config.uni_scr = unicable_scr;
		fe_config.uni_qrg = unicable_qrg;
		printf("%s: scr: %d qrg: %d\n", __func__, unicable_scr, unicable_qrg);
	}
	return res;
}

int CScanSetup::showScanMenuLnbSetup()
{
	printf("[neutrino] CScanSetup call %s fe %d\n", __FUNCTION__, fenumber);

	int count = 0;
	int res = menu_return::RETURN_REPAINT;
	CFrontend * fe = CFEManager::getInstance()->getFE(fenumber);

	char name[255];
	snprintf(name, sizeof(name), "%s %d: %s", g_Locale->getText(LOCALE_SATSETUP_SATELLITE), fenumber+1, fe->getInfo()->name);

	CMenuWidget * sat_setup = new CMenuWidget(name, NEUTRINO_ICON_SETTINGS, width);
	sat_setup->addIntroItems();

	satellite_map_t & satmap = fe->getSatellites();
	INFO("satmap size = %d", (int)satmap.size());

	CMenuWidget *tmp[satmap.size()];
	for (sat_iterator_t sit = satmap.begin(); sit != satmap.end(); ++sit)
	{
		if(!sit->second.configured)
			continue;

		std::string satname = CServiceManager::getInstance()->GetSatelliteName(sit->first);

		//sub menu for sat settings to selectable sat item
 		CMenuWidget *tempsat = new CMenuWidget(satname.c_str(), NEUTRINO_ICON_SETTINGS, width);
 		addScanMenuTempSat(tempsat, sit->second);
#if 0 // option not refreshed.
		if(sit->second.motor_position > 0) {
			char mpos[10];
			sprintf(mpos, "%d", sit->second.motor_position);
			sat_setup->addItem(new CMenuForwarderNonLocalized(satname.c_str(), true, mpos, tempsat));
		} else
#endif
		{
			CMenuForwarder * mf = new CMenuForwarderNonLocalized(satname.c_str(), true, NULL, tempsat);
			mf->setHint("", LOCALE_MENU_HINT_SCAN_LNBCONFIG);
			sat_setup->addItem(mf);
		}
		tmp[count] = tempsat;
		count++;
	}
	if(count) {
		res = sat_setup->exec(NULL, "");
		for (int i = 0; i < count; i++)
			delete tmp[i];
	}
	delete sat_setup;
	return res;
}

void CScanSetup::fillSatSelect(CMenuOptionStringChooser * select)
{
	std::set<int> satpos;
	std::set<int>::iterator tmpit;

	select->removeOptions();

	satOnOff->resetWidget(true);
	satOnOff->addIntroItems();

	bool sfound = false;
	int count = CFEManager::getInstance()->getFrontendCount();
	for(int i = 0; i < count; i++) {
		CFrontend * fe = CFEManager::getInstance()->getFE(i);

		if (fe->getInfo()->type != FE_QPSK)
			continue;

		satellite_map_t & satmap = fe->getSatellites();
		for (sat_iterator_t sit = satmap.begin(); sit != satmap.end(); ++sit) {
			tmpit = satpos.find(sit->first);
			if(sit->second.configured && tmpit == satpos.end()) {
				std::string satname = CServiceManager::getInstance()->GetSatelliteName(sit->first);
				select->addOption(satname.c_str());
				satpos.insert(sit->first);

				if (!sfound && strcmp(scansettings.satName, satname.c_str()) == 0)
					sfound = true;
			}
		}
#if 0
		if(CFEManager::getInstance()->getMode() != CFEManager::FE_MODE_ALONE)
			break;
#endif
	}
	if(!sfound && !satpos.empty()) {
		tmpit = satpos.begin();
		std::string satname = CServiceManager::getInstance()->GetSatelliteName(*tmpit);
		snprintf(scansettings.satName, sizeof(scansettings.satName), "%s", satname.c_str());
	}
	satellite_map_t & satmap = CServiceManager::getInstance()->SatelliteList();
	for (sat_iterator_t sit = satmap.begin(); sit != satmap.end(); sit++) {
		if(satpos.find(sit->first) != satpos.end()) {
			CMenuOptionChooser * mc = new CMenuOptionChooser(sit->second.name.c_str(),  &sit->second.use_in_scan, OPTIONS_OFF0_ON1_OPTIONS, OPTIONS_OFF0_ON1_OPTION_COUNT, true);
			mc->setHint("", LOCALE_MENU_HINT_SCAN_SATSCAN);
			satOnOff->addItem(mc);
		} else
			sit->second.use_in_scan = false;
	}
}

//init cable provider menu
void CScanSetup::fillCableSelect(CMenuOptionStringChooser * select)
{
	const char *what = r_system == DVB_C ? "cable" : "terrestrial";
	printf("[neutrino] CScanSetup call %s (%s)...\n", __func__, what);
	//don't misunderstand the name "satSelect", in this context it's actually for cable providers
	satellite_map_t satmap = CServiceManager::getInstance()->SatelliteList();
	bool sfound = false;
	std::string fname;
	for (sat_iterator_t sit = satmap.begin(); sit != satmap.end(); sit++)
	{
		if (r_system == DVB_C && sit->second.deltype != FE_QAM)
			continue;
		if (r_system == DVB_T && sit->second.deltype != FE_OFDM)
			continue;

		printf("Adding %s menu for %s position %d\n", what, sit->second.name.c_str(), sit->first);
		select->addOption(sit->second.name.c_str());

		if (fname.empty())
			fname = sit->second.name;

		if (!sfound && strcmp(scansettings.cableName, sit->second.name.c_str()) == 0)
			sfound = true;

		dprintf(DEBUG_DEBUG, "got scanprovider (%s): %s\n", what, sit->second.name.c_str());
	}
	if (!sfound && !fname.empty()) {
		if (r_system == DVB_C)
			snprintf(scansettings.cableName, sizeof(scansettings.cableName), "%s", fname.c_str());
		if (r_system == DVB_T)
			snprintf(scansettings.terrName, sizeof(scansettings.terrName), "%s", fname.c_str());
	}
}

int CScanSetup::showScanMenuSatFind()
{
	printf("[neutrino] CScanSetup call %s fe %d\n", __FUNCTION__, fenumber);
	int count = 0;
	CFrontend * fe = CFEManager::getInstance()->getFE(fenumber);
	char name[255];

	r_system = DVB_S;

	snprintf(name, sizeof(name), "%s %d: %s", g_Locale->getText(LOCALE_MOTORCONTROL_HEAD), fenumber+1, fe->getInfo()->name);

	CMenuWidget* sat_findMenu = new CMenuWidget(name /*LOCALE_MOTORCONTROL_HEAD*/, NEUTRINO_ICON_SETTINGS, width);

	sat_findMenu->addIntroItems();

	CMenuOptionStringChooser * feSatSelect = new CMenuOptionStringChooser(LOCALE_SATSETUP_SATELLITE, scansettings.satName, true, NULL, CRCInput::RC_red, NEUTRINO_ICON_BUTTON_RED, true);
	feSatSelect->setHint("", LOCALE_MENU_HINT_SCAN_SATSELECT);

	satellite_map_t & satmap = fe->getSatellites();
	bool sfound = false;
	std::string firstname;
	for (sat_iterator_t sit = satmap.begin(); sit != satmap.end(); ++sit) {
		if(!sit->second.configured)
			continue;
		std::string satname = CServiceManager::getInstance()->GetSatelliteName(sit->first);
		feSatSelect->addOption(satname.c_str());
		if (!sfound && strcmp(scansettings.satName, satname.c_str()) == 0)
			sfound = true;
		if (!sfound && firstname.empty())
			firstname = satname;
		count++;
	}
	if(count && !sfound)
		snprintf(scansettings.satName, sizeof(scansettings.satName), "%s", firstname.c_str());

	sat_findMenu->addItem(feSatSelect);

	CTPSelectHandler tpSelect;
	CMenuForwarder * mf = new CMenuForwarder(LOCALE_SCANTS_SELECT_TP, true, NULL, &tpSelect, "sat", CRCInput::RC_green, NEUTRINO_ICON_BUTTON_GREEN);
	mf->setHint("", LOCALE_MENU_HINT_SCAN_TPSELECT);
	sat_findMenu->addItem(mf);
	sat_findMenu->addItem(GenericMenuSeparatorLine);
	//--------------------------------------------------------------
	addScanOptionsItems(sat_findMenu);
	//--------------------------------------------------------------
	sat_findMenu->addItem(GenericMenuSeparatorLine);
	CMotorControl mcontrol(fenumber);
	mf = new CMenuForwarder(LOCALE_MOTORCONTROL_HEAD, allow_start, NULL, &mcontrol, "", CRCInput::RC_yellow, NEUTRINO_ICON_BUTTON_YELLOW);
	mf->setHint("", LOCALE_MENU_HINT_SCAN_SATFIND_START);
	sat_findMenu->addItem(mf);

	int res = sat_findMenu->exec(NULL, "");
	delete sat_findMenu;
	return res;
}

//init tempsat menu
void CScanSetup::addScanMenuTempSat(CMenuWidget *temp_sat, sat_config_t & satconfig)
{
	bool unicable = (dmode == DISEQC_UNICABLE);
	temp_sat->addIntroItems();

	CMenuOptionNumberChooser	*diseqc = NULL;
	CMenuOptionNumberChooser	*comm   = NULL;
	CMenuOptionNumberChooser	*uncomm = NULL;
	CMenuOptionNumberChooser	*unilnb = NULL;
	CMenuOptionNumberChooser	*motor  = NULL;
	CMenuOptionChooser		*usals  = NULL;
	CMenuForwarder			*mf;

	if (!unicable) {
		diseqc	= new CMenuOptionNumberChooser(LOCALE_SATSETUP_DISEQC_INPUT, &satconfig.diseqc, (dmode != NO_DISEQC) && (dmode != DISEQC_ADVANCED), -1, 15, NULL, 1, -1, LOCALE_OPTIONS_OFF);
		diseqc->setHint("", LOCALE_MENU_HINT_SCAN_DISEQC);
		comm	= new CMenuOptionNumberChooser(LOCALE_SATSETUP_COMM_INPUT, &satconfig.commited, dmode == DISEQC_ADVANCED, -1, 15, NULL, 1, -1, LOCALE_OPTIONS_OFF);
		comm->setHint("", LOCALE_MENU_HINT_SCAN_COMMITED);
		uncomm	= new CMenuOptionNumberChooser(LOCALE_SATSETUP_UNCOMM_INPUT, &satconfig.uncommited, dmode == DISEQC_ADVANCED, -1, 15, NULL, 1, -1, LOCALE_OPTIONS_OFF);
		uncomm->setHint("", LOCALE_MENU_HINT_SCAN_UNCOMMITED);
		motor	= new CMenuOptionNumberChooser(LOCALE_SATSETUP_MOTOR_POS, &satconfig.motor_position, true /*dmode == DISEQC_ADVANCED*/, 0, 64, NULL, 0, 0, LOCALE_OPTIONS_OFF);
		motor->setHint("", LOCALE_MENU_HINT_SCAN_MOTORPOS);
		usals	= new CMenuOptionChooser(LOCALE_EXTRA_USE_GOTOXX,  &satconfig.use_usals, OPTIONS_OFF0_ON1_OPTIONS, OPTIONS_OFF0_ON1_OPTION_COUNT, true /*dmode == DISEQC_ADVANCED*/);
		usals->setHint("", LOCALE_MENU_HINT_SCAN_USEUSALS);
	} else {
		if (satconfig.diseqc < 0)
			satconfig.diseqc = 0;
		unilnb = new CMenuOptionNumberChooser(LOCALE_UNICABLE_LNB, &satconfig.diseqc, true, 0, 1);
	}

	if(!satconfig.use_usals)
		all_usals = 0;

	CIntInput* lofL = new CIntInput(LOCALE_SATSETUP_LOFL, (int&) satconfig.lnbOffsetLow, 5, NONEXISTANT_LOCALE, NONEXISTANT_LOCALE);
	CIntInput* lofH = new CIntInput(LOCALE_SATSETUP_LOFH, (int&) satconfig.lnbOffsetHigh, 5, NONEXISTANT_LOCALE, NONEXISTANT_LOCALE);
	CIntInput* lofS = new CIntInput(LOCALE_SATSETUP_LOFS, (int&) satconfig.lnbSwitch, 5, NONEXISTANT_LOCALE, NONEXISTANT_LOCALE);

	if (!unicable) {
		temp_sat->addItem(diseqc);
		temp_sat->addItem(comm);
		temp_sat->addItem(uncomm);
		temp_sat->addItem(motor);
		temp_sat->addItem(usals);
	} else {
		temp_sat->addItem(unilnb);
	}

	mf = new CMenuDForwarder(LOCALE_SATSETUP_LOFL, true, lofL->getValue(), lofL);
	mf->setHint("", LOCALE_MENU_HINT_SCAN_LOFL);
	temp_sat->addItem(mf);
	mf = new CMenuDForwarder(LOCALE_SATSETUP_LOFH, true, lofH->getValue(), lofH);
	mf->setHint("", LOCALE_MENU_HINT_SCAN_LOFH);
	temp_sat->addItem(mf);

	mf = new CMenuDForwarder(LOCALE_SATSETUP_LOFS, true, lofS->getValue(), lofS);
	mf->setHint("", LOCALE_MENU_HINT_SCAN_LOFS);
	temp_sat->addItem(mf);
}

//init manual scan menu
void CScanSetup::addScanMenuManualScan(CMenuWidget *manual_Scan)
{
	printf("[neutrino] CScanSetup call %s...\n", __FUNCTION__);
	int shortCut = 1;
	CMenuForwarder * mf;

	manual_Scan->addIntroItems();
	//----------------------------------------------------------------------
	if (r_system == DVB_C)  {
		manual_Scan->addItem(cableSelect);
		mf = new CMenuForwarder(LOCALE_SATSETUP_CABLE_NID, true, nid->getValue(), nid);
		mf->setHint("", LOCALE_MENU_HINT_SCAN_NID);
		manual_Scan->addItem(mf);
		mf = new CMenuDForwarder(LOCALE_SCANTS_SELECT_TP, true, NULL, new CTPSelectHandler(), "cable", CRCInput::RC_green, NEUTRINO_ICON_BUTTON_GREEN);
	} else if (r_system == DVB_T) {
		manual_Scan->addItem(terrSelect);
		mf = new CMenuDForwarder(LOCALE_SCANTS_SELECT_TP, true, NULL, new CTPSelectHandler(), "terrestrial", CRCInput::RC_green, NEUTRINO_ICON_BUTTON_GREEN);
	} else {
		manual_Scan->addItem(satSelect);
		mf = new CMenuDForwarder(LOCALE_SCANTS_SELECT_TP, true, NULL, new CTPSelectHandler(), "sat", CRCInput::RC_green, NEUTRINO_ICON_BUTTON_GREEN);
	}

	mf->setHint("", LOCALE_MENU_HINT_SCAN_TPSELECT);
	manual_Scan->addItem(mf);

	manual_Scan->addItem(GenericMenuSeparatorLine);
	//----------------------------------------------------------------------
	shortCut = addScanOptionsItems(manual_Scan, shortCut);
	addListFlagsItems(manual_Scan, shortCut, true);
	//----------------------------------------------------------------------
	manual_Scan->addItem(GenericMenuSeparatorLine);

	mf = new CMenuForwarder(LOCALE_SCANTS_TEST, allow_start, NULL, this, r_system == DVB_C ? "ctest" : "stest", CRCInput::RC_yellow, NEUTRINO_ICON_BUTTON_YELLOW);
	mf->setHint("", LOCALE_MENU_HINT_SCAN_TEST);
	manual_Scan->addItem(mf);

	mf = new CMenuForwarder(LOCALE_SCANTS_STARTNOW, allow_start, NULL, this, r_system == DVB_C ? "cmanual" : "smanual", CRCInput::RC_blue, NEUTRINO_ICON_BUTTON_BLUE);
	mf->setHint("", LOCALE_MENU_HINT_SCAN_START);
	manual_Scan->addItem(mf);
}

//init auto scan all menu
void CScanSetup::addScanMenuAutoScanAll(CMenuWidget *auto_ScanAll)
{
	printf("[neutrino] CScanSetup call %s...\n", __FUNCTION__);
	auto_ScanAll->addIntroItems();
	//----------------------------------------------------------------------
	CMenuForwarder * mf = new CMenuForwarder(LOCALE_SATSETUP_SATELLITE, true, NULL, satOnOff, "", CRCInput::RC_red, NEUTRINO_ICON_BUTTON_RED);
	mf->setHint("", LOCALE_MENU_HINT_SCAN_AUTOALL_SELECT);
	auto_ScanAll->addItem(mf);

	auto_ScanAll->addItem(GenericMenuSeparatorLine);
	//----------------------------------------------------------------------
	addListFlagsItems(auto_ScanAll, 1);
	//----------------------------------------------------------------------
	auto_ScanAll->addItem(GenericMenuSeparatorLine);
	mf = new CMenuForwarder(LOCALE_SCANTS_STARTNOW, allow_start, NULL, this, "sall", CRCInput::RC_green, NEUTRINO_ICON_BUTTON_GREEN);
	mf->setHint("", LOCALE_MENU_HINT_SCAN_START);
	auto_ScanAll->addItem(mf);
}

#ifdef ENABLE_FASTSCAN
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

//init fast scan menu
void CScanSetup::addScanMenuFastScan(CMenuWidget *fast_ScanMenu)
{
	printf("[neutrino] CScanSetup call %s...\n", __FUNCTION__);
	fast_ScanMenu->addIntroItems();

	CMenuOptionChooser* fastProv = new CMenuOptionChooser(LOCALE_SATSETUP_FASTSCAN_PROV, (int *)&scansettings.fast_op, FAST_SCAN_PROV_OPTIONS, FAST_SCAN_PROV_OPTIONS_COUNT, true, NULL, CRCInput::RC_red, NEUTRINO_ICON_BUTTON_RED, true);
	fastProv->setHint("", LOCALE_MENU_HINT_SCAN_FASTPROV);
	CMenuOptionChooser* fastType = new CMenuOptionChooser(LOCALE_SATSETUP_FASTSCAN_TYPE, (int *)&scansettings.fast_type, FAST_SCAN_OPTIONS, FAST_SCAN_OPTIONS_COUNT, true, NULL, CRCInput::RC_green, NEUTRINO_ICON_BUTTON_GREEN, true);
	fastType->setHint("", LOCALE_MENU_HINT_SCAN_FASTTYPE);

	//----------------------------------------------------------------------
	fast_ScanMenu->addItem(fastProv);
	fast_ScanMenu->addItem(fastType);
	//----------------------------------------------------------------------
	fast_ScanMenu->addItem(GenericMenuSeparatorLine);
	CMenuForwarder * mf = new CMenuForwarder(LOCALE_SCANTS_STARTNOW, allow_start, NULL, this, "sfast", CRCInput::RC_yellow, NEUTRINO_ICON_BUTTON_YELLOW);
	mf->setHint("", LOCALE_MENU_HINT_SCAN_START);
	fast_ScanMenu->addItem(mf);
}
#endif /*ENABLE_FASTSCAN*/

//init auto scan menu
void CScanSetup::addScanMenuAutoScan(CMenuWidget *auto_Scan)
{
	printf("[neutrino] CScanSetup call %s...\n", __FUNCTION__);
	CMenuForwarder * mf;
	auto_Scan->addIntroItems();

	const char *action;
	if (r_system == DVB_C)  { //cable
		auto_Scan->addItem(cableSelect);
		mf = new CMenuForwarder(LOCALE_SATSETUP_CABLE_NID, true, nid->getValue(), nid);
		mf->setHint("", LOCALE_MENU_HINT_SCAN_NID);
		auto_Scan->addItem(mf);
		action = "cauto";
	} else if (r_system == DVB_T) {
		auto_Scan->addItem(terrSelect);
		action = "tauto";
	} else {
		auto_Scan->addItem(satSelect);
		action = "sauto";
	}

	auto_Scan->addItem(GenericMenuSeparatorLine);
	//----------------------------------------------------------------------
	addListFlagsItems(auto_Scan, 1);
	//----------------------------------------------------------------------
	auto_Scan->addItem(GenericMenuSeparatorLine);
	mf = new CMenuForwarder(LOCALE_SCANTS_STARTNOW, allow_start, NULL, this, action, CRCInput::RC_green, NEUTRINO_ICON_BUTTON_GREEN);
	mf->setHint("", LOCALE_MENU_HINT_SCAN_START);
	auto_Scan->addItem(mf);
}

//init simple cable scan menu
void CScanSetup::addScanMenuCable(CMenuWidget *menu)
{
	printf("[neutrino] CScanSetup call %s...\n", __FUNCTION__);
	int shortCut = 1;
	CMenuForwarder * mf;

	menu->addIntroItems();
	//----------------------------------------------------------------------
	CMenuOptionStringChooser * select = new CMenuOptionStringChooser(LOCALE_CABLESETUP_PROVIDER, scansettings.cableName, true, this, CRCInput::RC_red, NEUTRINO_ICON_BUTTON_RED, true);
	fillCableSelect(select);
	select->setHint("", LOCALE_MENU_HINT_SCAN_CABLE);
	menu->addItem(select);

	mf = new CMenuForwarder(LOCALE_SATSETUP_CABLE_NID, true, nid->getValue(), nid);
	mf->setHint("", LOCALE_MENU_HINT_SCAN_NID);
	menu->addItem(mf);

	mf = new CMenuDForwarder(LOCALE_SCANTS_SELECT_TP, true, NULL, new CTPSelectHandler(), "cable", CRCInput::RC_green, NEUTRINO_ICON_BUTTON_GREEN);
	mf->setHint("", LOCALE_MENU_HINT_SCAN_TPSELECT);
	menu->addItem(mf);

	menu->addItem(GenericMenuSeparatorLine);

	CStringInput		*freq	= new CStringInput(LOCALE_EXTRA_TP_FREQ, (char *) scansettings.cable_TP_freq, freq_length, NONEXISTANT_LOCALE, NONEXISTANT_LOCALE, "0123456789");
	CMenuForwarder		*Freq 	= new CMenuDForwarder(LOCALE_EXTRA_TP_FREQ, true, scansettings.cable_TP_freq, freq, "", CRCInput::convertDigitToKey(shortCut++));
	Freq->setHint("", LOCALE_MENU_HINT_SCAN_FREQ);

	CStringInput		*rate 	= new CStringInput(LOCALE_EXTRA_TP_RATE, (char *) scansettings.cable_TP_rate, 8, NONEXISTANT_LOCALE, NONEXISTANT_LOCALE, "0123456789");
	CMenuForwarder		*Rate 	= new CMenuDForwarder(LOCALE_EXTRA_TP_RATE, true, scansettings.cable_TP_rate, rate, "", CRCInput::convertDigitToKey(shortCut++));
	Rate->setHint("", LOCALE_MENU_HINT_SCAN_RATE);

	CMenuOptionChooser * mod_pol = new CMenuOptionChooser(LOCALE_EXTRA_TP_MOD, (int *)&scansettings.cable_TP_mod, SATSETUP_SCANTP_MOD, SATSETUP_SCANTP_MOD_COUNT, true, NULL, CRCInput::convertDigitToKey(shortCut++));
	mod_pol->setHint("", LOCALE_MENU_HINT_SCAN_MOD);

	menu->addItem(Freq);
	menu->addItem(Rate);
	menu->addItem(mod_pol);

	CMenuOptionChooser *lcn  = new CMenuOptionChooser(LOCALE_SATSETUP_LOGICAL_NUMBERS, (int *)&scansettings.scan_logical_numbers, OPTIONS_OFF0_ON1_OPTIONS, OPTIONS_OFF0_ON1_OPTION_COUNT, true, this, CRCInput::convertDigitToKey(shortCut++));
	lcn->setHint("", LOCALE_MENU_HINT_SCAN_LOGICAL);
	menu->addItem(lcn);

	lcnhd  = new CMenuOptionChooser(LOCALE_SATSETUP_LOGICAL_HD, (int *)&scansettings.scan_logical_hd, OPTIONS_OFF0_ON1_OPTIONS, OPTIONS_OFF0_ON1_OPTION_COUNT, scansettings.scan_logical_numbers, NULL, CRCInput::convertDigitToKey(shortCut++));
	lcnhd->setHint("", LOCALE_MENU_HINT_SCAN_LOGICAL_HD);
	menu->addItem(lcnhd);

	menu->addItem(GenericMenuSeparatorLine);
#if 0
	mf = new CMenuForwarder(LOCALE_SCANTS_TEST, allow_start, NULL, this, "test", CRCInput::RC_yellow, NEUTRINO_ICON_BUTTON_YELLOW);
	mf->setHint("", LOCALE_MENU_HINT_SCAN_TEST);
	menu->addItem(mf);
#endif
	mf = new CMenuForwarder(LOCALE_SCANTS_STARTNOW, allow_start, NULL, this, "cable", CRCInput::RC_blue, NEUTRINO_ICON_BUTTON_BLUE);
	mf->setHint("", LOCALE_MENU_HINT_SCAN_START);
	menu->addItem(mf);
}

//create scan options items
int CScanSetup::addScanOptionsItems(CMenuWidget *options_menu, const int &shortcut)
{
	printf("[neutrino] CScanSetup call %s...\n", __FUNCTION__);
	int shortCut = shortcut;
	fec_count = (r_system == DVB_S) ? SATSETUP_SCANTP_FEC_COUNT : CABLESETUP_SCANTP_FEC_COUNT;
	freq_length = (r_system == DVB_S) ? 8 : 6;

	CMenuOptionChooser	*fec = NULL;
	CMenuOptionChooser	*mod_pol = NULL;
	CMenuForwarder		*Freq = NULL;
	CMenuForwarder		*Rate = NULL;
	if (r_system == DVB_S) {
		CStringInput		*freq	= new CStringInput(LOCALE_EXTRA_TP_FREQ, (char *) scansettings.sat_TP_freq, freq_length, NONEXISTANT_LOCALE, NONEXISTANT_LOCALE, "0123456789");
		Freq 	= new CMenuDForwarder(LOCALE_EXTRA_TP_FREQ, true, scansettings.sat_TP_freq, freq, "", CRCInput::convertDigitToKey(shortCut++));
		Freq->setHint("", LOCALE_MENU_HINT_SCAN_FREQ);

		CStringInput		*rate 	= new CStringInput(LOCALE_EXTRA_TP_RATE, (char *) scansettings.sat_TP_rate, 8, NONEXISTANT_LOCALE, NONEXISTANT_LOCALE, "0123456789");
		Rate 	= new CMenuDForwarder(LOCALE_EXTRA_TP_RATE, true, scansettings.sat_TP_rate, rate, "", CRCInput::convertDigitToKey(shortCut++));
		Rate->setHint("", LOCALE_MENU_HINT_SCAN_RATE);
		fec 	= new CMenuOptionChooser(LOCALE_EXTRA_TP_FEC, (int *)&scansettings.sat_TP_fec, SATSETUP_SCANTP_FEC, fec_count, true, NULL, CRCInput::convertDigitToKey(shortCut++), "", true);
		fec->setHint("", LOCALE_MENU_HINT_SCAN_FEC);
		mod_pol = new CMenuOptionChooser(LOCALE_EXTRA_TP_POL, (int *)&scansettings.sat_TP_pol, SATSETUP_SCANTP_POL, SATSETUP_SCANTP_POL_COUNT, true, NULL, CRCInput::convertDigitToKey(shortCut++));
		mod_pol->setHint("", LOCALE_MENU_HINT_SCAN_POL);
	} else if (r_system == DVB_C) {
		CStringInput		*freq	= new CStringInput(LOCALE_EXTRA_TP_FREQ, (char *) scansettings.cable_TP_freq, freq_length, NONEXISTANT_LOCALE, NONEXISTANT_LOCALE, "0123456789");
		Freq 	= new CMenuDForwarder(LOCALE_EXTRA_TP_FREQ, true, scansettings.cable_TP_freq, freq, "", CRCInput::convertDigitToKey(shortCut++));
		Freq->setHint("", LOCALE_MENU_HINT_SCAN_FREQ);

		CStringInput		*rate 	= new CStringInput(LOCALE_EXTRA_TP_RATE, (char *) scansettings.cable_TP_rate, 8, NONEXISTANT_LOCALE, NONEXISTANT_LOCALE, "0123456789");
		Rate 	= new CMenuDForwarder(LOCALE_EXTRA_TP_RATE, true, scansettings.cable_TP_rate, rate, "", CRCInput::convertDigitToKey(shortCut++));
		Rate->setHint("", LOCALE_MENU_HINT_SCAN_RATE);
		mod_pol = new CMenuOptionChooser(LOCALE_EXTRA_TP_MOD, (int *)&scansettings.cable_TP_mod, SATSETUP_SCANTP_MOD, SATSETUP_SCANTP_MOD_COUNT, true, NULL, CRCInput::convertDigitToKey(shortCut++));
		mod_pol->setHint("", LOCALE_MENU_HINT_SCAN_MOD);
	} else if (r_system == DVB_T) {
		CStringInput *freq = new CStringInput(LOCALE_EXTRA_TP_FREQ, (char *)scansettings.terr_TP_freq, freq_length, NONEXISTANT_LOCALE, NONEXISTANT_LOCALE, "0123456789");
		Freq = new CMenuDForwarder(LOCALE_EXTRA_TP_FREQ, true, scansettings.terr_TP_freq, freq, "", CRCInput::convertDigitToKey(shortCut++));
		Freq->setHint("", LOCALE_MENU_HINT_SCAN_FREQ);
	}

	options_menu->addItem(Freq);
	if (Rate)
		options_menu->addItem(Rate);
	if (fec)
		options_menu->addItem(fec);
	if (mod_pol)
		options_menu->addItem(mod_pol);

	return shortCut;
}

//create list flag items
int CScanSetup::addListFlagsItems(CMenuWidget *listflags_menu, const int &shortcut, bool manual)
{
	printf("[neutrino] CScanSetup call %s...\n", __FUNCTION__);
	int shortCut = shortcut;

	CMenuOptionChooser *useNit;
	if (manual)
		useNit = new CMenuOptionChooser(LOCALE_SATSETUP_USE_NIT, (int *)&scansettings.scan_nit_manual, OPTIONS_OFF0_ON1_OPTIONS, OPTIONS_OFF0_ON1_OPTION_COUNT, true, NULL, CRCInput::convertDigitToKey(shortCut++));
	else
		useNit = new CMenuOptionChooser(LOCALE_SATSETUP_USE_NIT, (int *)&scansettings.scan_nit, OPTIONS_OFF0_ON1_OPTIONS, OPTIONS_OFF0_ON1_OPTION_COUNT, true, NULL, CRCInput::convertDigitToKey(shortCut++));

	useNit->setHint("", LOCALE_MENU_HINT_SCAN_NIT);

	CMenuOptionChooser *ftaFlag = new CMenuOptionChooser(LOCALE_SATSETUP_USE_FTA_FLAG, (int *)&scansettings.scan_fta_flag, OPTIONS_OFF0_ON1_OPTIONS, OPTIONS_OFF0_ON1_OPTION_COUNT, true, NULL, CRCInput::convertDigitToKey(shortCut++));
	ftaFlag->setHint("", LOCALE_MENU_HINT_SCAN_FTA);

	CMenuOptionChooser *scanPid = new CMenuOptionChooser(LOCALE_EXTRA_ZAPIT_SCANPIDS,  &zapitCfg.scanPids, OPTIONS_OFF0_ON1_OPTIONS, OPTIONS_OFF0_ON1_OPTION_COUNT, true, NULL, CRCInput::convertDigitToKey(shortCut++));
	scanPid->setHint("", LOCALE_MENU_HINT_SCAN_PIDS);

	listflags_menu->addItem(useNit);
	listflags_menu->addItem(ftaFlag);
	listflags_menu->addItem(scanPid);

	CMenuOptionChooser *resetNum  = new CMenuOptionChooser(LOCALE_SATSETUP_RESET_NUMBERS, (int *)&scansettings.scan_reset_numbers, OPTIONS_OFF0_ON1_OPTIONS, OPTIONS_OFF0_ON1_OPTION_COUNT, true, NULL, CRCInput::convertDigitToKey(shortCut++));
	resetNum->setHint("", LOCALE_MENU_HINT_SCAN_RESET_NUMBERS);
	listflags_menu->addItem(resetNum);
#if 0 // testing
	CMenuOptionChooser *useBat  = new CMenuOptionChooser(LOCALE_SATSETUP_USE_BAT, (int *)&scansettings.scan_bat, OPTIONS_OFF0_ON1_OPTIONS, OPTIONS_OFF0_ON1_OPTION_COUNT, true, NULL, CRCInput::convertDigitToKey(shortCut++));
	listflags_menu->addItem(useBat);
	CMenuOptionChooser *lcn  = new CMenuOptionChooser(LOCALE_SATSETUP_LOGICAL_NUMBERS, (int *)&scansettings.scan_logical_numbers, OPTIONS_OFF0_ON1_OPTIONS, OPTIONS_OFF0_ON1_OPTION_COUNT, true, NULL, CRCInput::convertDigitToKey(shortCut++));
	listflags_menu->addItem(lcn);
#endif
	return shortCut;
}

//sets menu mode to "wizard" or "default"
void CScanSetup::setWizardMode(bool mode)
{
	printf("[neutrino] CScanSetup %s set scan-setup to mode %d...\n", __FUNCTION__, mode);
	is_wizard = mode;
}

//save current settings
void CScanSetup::saveScanSetup()
{
	printf("[neutrino] CScanSetup call %s...\n", __FUNCTION__);
	if(!scansettings.saveSettings(NEUTRINO_SCAN_SETTINGS_FILE))
		dprintf(DEBUG_NORMAL, "error while saving scan-settings!\n");

	//CServiceManager::getInstance()->SaveMotorPositions();
	zapitCfg.gotoXXLatitude = strtod(zapit_lat, NULL);
	zapitCfg.gotoXXLongitude = strtod(zapit_long, NULL);

	CZapit::getInstance()->SetConfig(&zapitCfg);
	CFEManager::getInstance()->saveSettings(true);
	//CFEManager::getInstance()->linkFrontends();
}

bool CScanSetup::changeNotify(const neutrino_locale_t OptionName, void * /*data*/)
{
	bool ret = false;

	if(ARE_LOCALES_EQUAL(OptionName, LOCALE_SATSETUP_USE_USALS)) {
		CFrontend * fe = CFEManager::getInstance()->getFE(fenumber);
		printf("[neutrino] CScanSetup::%s: all usals %d \n", __FUNCTION__, all_usals);
		satellite_map_t & satmap = fe->getSatellites();
		for (sat_iterator_t sit = satmap.begin(); sit != satmap.end(); sit++) {
			sit->second.use_usals = all_usals;
		}
	}
	else if(ARE_LOCALES_EQUAL(OptionName, LOCALE_SATSETUP_DISEQC)) {
		printf("[neutrino] CScanSetup::%s: diseqc %d fenumber %d\n", __FUNCTION__, dmode, fenumber);
		CFrontend * fe = CFEManager::getInstance()->getFE(fenumber);
		if (fe->getDiseqcType() == (diseqc_t)dmode)
			return ret;

		fe_restart = true;
		fe->setDiseqcType((diseqc_t) dmode);
		fe->setTsidOnid(0);

#if 0
		if(femode !=  CFEManager::FE_MODE_ALONE)
			CFEManager::getInstance()->saveSettings(true);

		fautoScanAll->setActive(dmode != NO_DISEQC);
		uniSetup->setActive(dmode == DISEQC_UNICABLE);
		if (dmode == NO_DISEQC || dmode == DISEQC_UNICABLE) {
			ojDiseqcRepeats->setActive(false);
		}
		else if(dmode < DISEQC_ADVANCED) {
			ojDiseqcRepeats->setActive(true);
		}
		else if(dmode == DISEQC_ADVANCED) {
			ojDiseqcRepeats->setActive(true);
		}
#endif
		uniSetup->setActive(dmode == DISEQC_UNICABLE);
		bool enable = (dmode < DISEQC_ADVANCED) && (dmode != NO_DISEQC);
		ojDiseqcRepeats->setActive(enable && !CFrontend::linked(femode) && femode != CFrontend::FE_MODE_UNUSED);
		dorder->setActive(!CFrontend::linked(femode) && femode != CFrontend::FE_MODE_UNUSED && dmode == DISEQC_ADVANCED);

	}
	else if(ARE_LOCALES_EQUAL(OptionName, LOCALE_SATSETUP_FE_MODE)) {
		printf("[neutrino] CScanSetup::%s: fe%d mode %d master %d\n", __FUNCTION__, fenumber, femode, femaster);
#if 0
		/* cable dont have this menu */
		if(frontendSetup)
			frontendSetup->setActive(femode ==  CFEManager::FE_MODE_ALONE);
		CFEManager::getInstance()->setMode((CFEManager::fe_mode_t) femode);
		/* to copy settings from fe0 */
		if(femode !=  CFEManager::FE_MODE_ALONE)
			CFEManager::getInstance()->saveSettings(true);
		if (r_system == DVB_S) //sat
			fillSatSelect(satSelect);
#endif
		CFrontend * fe = CFEManager::getInstance()->getFE(fenumber);
		if (fe->getMode() == femode)
			return ret;

		modestr[fenumber] = g_Locale->getText(getModeLocale(femode));
		fe_restart = true;
		fe->setMode(femode);
		if (fe && fe->getType() == FE_QPSK) {
			if (linkfe)
				linkfe->setActive(CFrontend::linked(femode));
			/* leave diseqc type enabled for TWIN in case user need different unicable setup */
			dtype->setActive(femode != CFrontend::FE_MODE_UNUSED && femode != CFrontend::FE_MODE_LINK_LOOP);
			uniSetup->setActive(dmode == DISEQC_UNICABLE && femode != CFrontend::FE_MODE_UNUSED && femode != CFrontend::FE_MODE_LINK_LOOP);
			dorder->setActive(!CFrontend::linked(femode) && femode != CFrontend::FE_MODE_UNUSED && dmode == DISEQC_ADVANCED);
			fsatSelect->setActive(!CFrontend::linked(femode) && femode != CFrontend::FE_MODE_UNUSED);
			fsatSetup->setActive(!CFrontend::linked(femode) && femode != CFrontend::FE_MODE_UNUSED);
			bool enable = (dmode < DISEQC_ADVANCED) && (dmode != NO_DISEQC);
			ojDiseqcRepeats->setActive(!CFrontend::linked(femode) && femode != CFrontend::FE_MODE_UNUSED && enable);

			/* if mode changed, set current master too */
			if (femaster >= 0)
				fe->setMaster(femaster);
		}
	}
	else if(ARE_LOCALES_EQUAL(OptionName, LOCALE_SATSETUP_FE_MODE_MASTER)) {
		printf("[neutrino] CScanSetup::%s: fe%d link %d \n", __FUNCTION__, fenumber, femaster);
		CFrontend * fe = CFEManager::getInstance()->getFE(fenumber);
		if (fe)
			fe->setMaster(femaster);
	}
	else if(ARE_LOCALES_EQUAL(OptionName, LOCALE_CABLESETUP_PROVIDER)) {
		printf("[neutrino] CScanSetup::%s: new provider: [%s]\n", __FUNCTION__, scansettings.cableName);
		satellite_map_t & satmap = CServiceManager::getInstance()->SatelliteList();
		for (sat_iterator_t sit = satmap.begin(); sit != satmap.end(); sit++)
		{
			if (strcmp(scansettings.cableName, sit->second.name.c_str()) == 0) {
				if(sit->second.cable_nid > 0) {
					scansettings.cable_nid = sit->second.cable_nid;
					nid->updateValue();
					ret = true;
				}

				break;
			}
		}
	}
	else if(ARE_LOCALES_EQUAL(OptionName, LOCALE_SATSETUP_LOGICAL_NUMBERS)) {
printf("[neutrino] CScanSetup::%s: logical numbers %d\n", __FUNCTION__, scansettings.scan_logical_numbers);
		lcnhd->setActive(scansettings.scan_logical_numbers);
	}
	return ret;
}

void CScanSetup::updateManualSettings()
{
	CZapitChannel * channel = CZapit::getInstance()->GetCurrentChannel();
	if(channel) {
		transponder_list_t::iterator tI;
		tI = transponders.find(channel->getTransponderId());
		if(tI != transponders.end()) {
			CFrontend * frontend = CFEManager::getInstance()->getLiveFE();
			switch (frontend->getType()) {
				case FE_QPSK:
					sprintf(scansettings.sat_TP_freq, "%d", tI->second.feparams.dvb_feparams.frequency);
					sprintf(scansettings.sat_TP_rate, "%d", tI->second.feparams.dvb_feparams.u.qpsk.symbol_rate);
					scansettings.sat_TP_fec = tI->second.feparams.dvb_feparams.u.qpsk.fec_inner;
					scansettings.sat_TP_pol = tI->second.polarization;
					strncpy(scansettings.satName,
							CServiceManager::getInstance()->GetSatelliteName(channel->getSatellitePosition()).c_str(), 50);
					break;
				case FE_QAM:
					sprintf(scansettings.cable_TP_freq, "%d", tI->second.feparams.dvb_feparams.frequency);
					sprintf(scansettings.cable_TP_rate, "%d", tI->second.feparams.dvb_feparams.u.qam.symbol_rate);
					scansettings.cable_TP_fec = tI->second.feparams.dvb_feparams.u.qam.fec_inner;
					scansettings.cable_TP_mod = tI->second.feparams.dvb_feparams.u.qam.modulation;
					strncpy(scansettings.cableName,
							CServiceManager::getInstance()->GetSatelliteName(channel->getSatellitePosition()).c_str(), 50);
					break;
				case FE_OFDM:
				case FE_ATSC:
					break;
			}
		}
	}
}

int CTPSelectHandler::exec(CMenuTarget* parent, const std::string &actionkey)
{
	std::map<int, transponder> tmplist;
	std::map<int, transponder>::iterator tmpI;
	char cnt[5];
	int select = -1;
	static int old_selected = 0;
	static t_satellite_position old_position = 0;

	int width = w_max (40, 10);

	if (parent)
		parent->hide();

	t_satellite_position position;
	char * name;
	if (actionkey == "sat")
		name = scansettings.satName;
	else if (actionkey == "terrestrial")
		name = scansettings.terrName;
	else 
		name = scansettings.cableName;

	position = CServiceManager::getInstance()->GetSatellitePosition(name);
	INFO("%s: %s\n", actionkey.c_str(), name);

	if (old_position != position) {
		old_selected = 0;
		old_position = position;
	}

	CMenuWidget menu(LOCALE_SCANTS_SELECT_TP, NEUTRINO_ICON_SETTINGS, width);
	CMenuSelectorTarget * selector = new CMenuSelectorTarget(&select);
	menu.addIntroItems(NONEXISTANT_LOCALE, NONEXISTANT_LOCALE, CMenuWidget::BTN_TYPE_CANCEL); //add cancel button, ensures that we have enought space left from item caption

	transponder ct;
	CZapitChannel * channel = CZapit::getInstance()->GetCurrentChannel();
	if(channel)
		CServiceManager::getInstance()->GetTransponder(channel->getTransponderId(), ct);

	int i = 0;
	transponder_list_t &select_transponders = CServiceManager::getInstance()->GetSatelliteTransponders(position);
	for (transponder_list_t::iterator tI = select_transponders.begin(); tI != select_transponders.end(); ++tI) {
		sprintf(cnt, "%d", i);
		transponder & t = tI->second;

		if(!old_selected && ct == t)
			old_selected = i;

		std::string tname = t.description();
		CMenuForwarderNonLocalized * ts_item = new CMenuForwarderNonLocalized(tname.c_str(), true, NULL, selector, cnt, CRCInput::RC_nokey, NULL)/*, false*/;

		ts_item->setItemButton(NEUTRINO_ICON_BUTTON_OKAY, true);
		menu.addItem(ts_item, old_selected == i);

		tmplist.insert(std::pair <int, transponder>(i, tI->second));
		i++;
	}
	if (i == 0) {
		std::string text = "No transponders found for ";
		text += name;
		ShowHintUTF(LOCALE_MESSAGEBOX_ERROR, text.c_str(), 450, 2);
		return menu_return::RETURN_REPAINT;
	}

	menu.setSelected(old_selected);
	int retval = menu.exec(NULL, "");

	delete selector;

	if (select >= 0) {
		old_selected = select;

		tmpI = tmplist.find(select);

		printf("CTPSelectHandler::exec: selected TP: freq %d pol %d SR %d\n", tmpI->second.feparams.dvb_feparams.frequency,
		       tmpI->second.polarization, tmpI->second.feparams.dvb_feparams.u.qpsk.symbol_rate);

		switch (tmpI->second.deltype) {
			case FE_QPSK:
				sprintf(scansettings.sat_TP_freq, "%d", tmpI->second.feparams.dvb_feparams.frequency);
				sprintf(scansettings.sat_TP_rate, "%d", tmpI->second.feparams.dvb_feparams.u.qpsk.symbol_rate);
				scansettings.sat_TP_fec = tmpI->second.feparams.dvb_feparams.u.qpsk.fec_inner;
				scansettings.sat_TP_pol = tmpI->second.polarization;
				break;
			case FE_QAM:
				sprintf(scansettings.cable_TP_freq, "%d", tmpI->second.feparams.dvb_feparams.frequency);
				sprintf(scansettings.cable_TP_rate, "%d", tmpI->second.feparams.dvb_feparams.u.qam.symbol_rate);
				scansettings.cable_TP_fec = tmpI->second.feparams.dvb_feparams.u.qam.fec_inner;
				scansettings.cable_TP_mod = tmpI->second.feparams.dvb_feparams.u.qam.modulation;
				break;
			case FE_OFDM:
				sprintf(scansettings.terr_TP_freq, "%d", tmpI->second.feparams.dvb_feparams.frequency);
				break;
			case FE_ATSC:
				break;
		}

	}

	if (retval == menu_return::RETURN_EXIT_ALL)
		return menu_return::RETURN_EXIT_ALL;

	return menu_return::RETURN_REPAINT;
}
