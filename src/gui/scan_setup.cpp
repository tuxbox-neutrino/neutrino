/*
	scan_setup menue - Neutrino-GUI

	Copyright (C) 2001 Steffen Hehn 'McClean'
	and some other guys
	Homepage: http://dbox.cyberphoria.org/

	Reworked as similar to $Id: scan_setup.cpp,v 1.10 2010/12/05 22:32:12 tuxbox-cvs Exp $
	Copyright (C) 2011 T. Graf 'dbt'
	Homepage: http://www.dbox2-tuning.net/

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


#include "gui/scan.h"
#include "gui/scan_setup.h"
#include "gui/motorcontrol.h"
#include "gui/bedit/bouqueteditor_bouquets.h"

#include "gui/widget/hintbox.h"
#include "gui/widget/stringinput.h"
#include "gui/widget/stringinput_ext.h"

#include <driver/screen_max.h>
#include <driver/framebuffer.h>
#include <driver/record.h>
#include <system/debug.h>

#include <zapit/femanager.h>
#include <zapit/getservices.h>
#include <zapit/satconfig.h>
#include <zapit/scan.h>
#include <zapit/zapit.h>
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
	{ SMATV_REMOTE_TUNING,	LOCALE_SATSETUP_SMATVREMOTE }
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

#if 0
/*Cable*/
#define CABLESETUP_SCANTP_MOD_COUNT 7
const CMenuOptionChooser::keyval CABLESETUP_SCANTP_MOD[CABLESETUP_SCANTP_MOD_COUNT] =
{
	{0, LOCALE_EXTRA_TP_MOD_QPSK     } ,
	{1, LOCALE_EXTRA_TP_MOD_QAM_16   } ,
	{2, LOCALE_EXTRA_TP_MOD_QAM_32   } ,
	{3, LOCALE_EXTRA_TP_MOD_QAM_64   } ,
	{4, LOCALE_EXTRA_TP_MOD_QAM_128  } ,
	{5, LOCALE_EXTRA_TP_MOD_QAM_256  } ,
	{6, LOCALE_EXTRA_TP_MOD_QAM_AUTO }
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

#define SATSETUP_FRONTEND_MODE_COUNT 4
const CMenuOptionChooser::keyval SATSETUP_FRONTEND_MODE[SATSETUP_FRONTEND_MODE_COUNT] =
{
	{ CFEManager::FE_MODE_SINGLE, LOCALE_SATSETUP_FE_MODE_SINGLE },
	{ CFEManager::FE_MODE_LOOP,   LOCALE_SATSETUP_FE_MODE_LOOP   },
	{ CFEManager::FE_MODE_TWIN,   LOCALE_SATSETUP_FE_MODE_TWIN   },
	{ CFEManager::FE_MODE_ALONE,  LOCALE_SATSETUP_FE_MODE_ALONE  }
};

CScanSetup::CScanSetup(bool wizard_mode)
{
	width = w_max (40, 10);
	r_system = g_info.delivery_system;
	fec_count = (r_system == DVB_S) ? SATSETUP_SCANTP_FEC_COUNT : CABLESETUP_SCANTP_FEC_COUNT;
	freq_length = (r_system == DVB_S) ? 8 : 6;

	is_wizard = wizard_mode;

	//define caption of some forwarders and widgets depends of current receiver type
	satprov_locale = (r_system == DVB_S) ? LOCALE_SATSETUP_SATELLITE : LOCALE_CABLESETUP_PROVIDER;

	satSelect 	= NULL;
	frontendSetup	= NULL;
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
	if(actionKey=="save_scansettings")
	{
		printf("[neutrino] CScanSetup::%s save_scansettings...\n", __FUNCTION__);
		saveScanSetup();
		return res;
	}
	else if(actionKey=="reloadchannels")
	{
		printf("[neutrino] CScanSetup::%s reloadchannels...\n", __FUNCTION__);
		if (reloadhintBox)
			reloadhintBox->paint();
		g_Zapit->reinitChannels();
		if (reloadhintBox)
			reloadhintBox->hide();
		CNeutrinoApp::getInstance ()->SDTreloadChannels = false;
		return res;
	}
	else if(actionKey=="satsetup")
	{
		return showScanMenuLnbSetup();
	}
	else if(actionKey=="satfind")
	{
		return showScanMenuSatFind();
	}
	else if((loc = actionKey.find("config_frontend", 0)) != std::string::npos)
	{
		int number = actionKey.at(15) - '0';
		printf("[neutrino] CScanSetup::%s %s, fe %d\n", __FUNCTION__, actionKey.c_str(), number);
		return showFrontendSetup(number);
	}

	//starting scan
	std::string scants_key[] = {"all", "manual", "test", "fast", "auto"/*doesn't exists in CScanTs!*/};

	for (uint i=0; i< (sizeof(scants_key)/sizeof(scants_key[0])); i++)
	{
		if (actionKey == scants_key[i])
		{
			printf("[neutrino] CScanSetup::%s %s...\n", __FUNCTION__, scants_key[i].c_str());
			//ensure that be saved all settings before scan...
			saveScanSetup();
			//...then start scan
			CScanTs scanTs;
			scanTs.exec(NULL, scants_key[i]);
			return res;
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

	allow_start = !CRecordManager::getInstance()->RecordingStatus() || CRecordManager::getInstance()->TimeshiftOnly();

	//main
	CMenuWidget * settings = new CMenuWidget(is_wizard ? LOCALE_SERVICEMENU_SCANTS : LOCALE_SERVICEMENU_HEAD, NEUTRINO_ICON_SETTINGS, width, MN_WIDGET_ID_SCAN_MAIN);
	settings->setWizardMode(is_wizard);

	//back
	settings->addIntroItems(is_wizard ? NONEXISTANT_LOCALE : LOCALE_SERVICEMENU_SCANTS);
	//----------------------------------------------------------------------
	//save scan settings
	settings->addItem(new CMenuForwarder(LOCALE_MAINSETTINGS_SAVESETTINGSNOW, true, NULL, this, "save_scansettings", CRCInput::RC_red, NEUTRINO_ICON_BUTTON_RED));
	//----------------------------------------------------------------------
	settings->addItem(GenericMenuSeparatorLine);
	//----------------------------------------------------------------------
	//service select mode
	settings->addItem(new CMenuOptionChooser(LOCALE_ZAPIT_SCANTYPE, (int *)&scansettings.scanType, SCANTS_ZAPIT_SCANTYPE, SCANTS_ZAPIT_SCANTYPE_COUNT, true, NULL, CRCInput::convertDigitToKey(shortcut++), "", true));

	//bouquet generate mode
	settings->addItem(new CMenuOptionChooser(LOCALE_SCANTS_BOUQUET, (int *)&scansettings.bouquetMode, SCANTS_BOUQUET_OPTIONS, SCANTS_BOUQUET_OPTION_COUNT, true, NULL, CRCInput::convertDigitToKey(shortcut++), "", true));

	//sat/provider selector
	satSelect = new CMenuOptionStringChooser(satprov_locale, scansettings.satNameNoDiseqc, true, NULL, CRCInput::RC_red, NEUTRINO_ICON_BUTTON_RED, true);
	satOnOff = new CMenuWidget(satprov_locale, NEUTRINO_ICON_SETTINGS, width);

	if (r_system == DVB_S) //sat
	{
		//settings->addItem(new CMenuSeparator(CMenuSeparator::LINE | CMenuSeparator::STRING, LOCALE_SCANTS_PREVERENCES_RECEIVING_SYSTEM));
		CMenuWidget * setupMenu = new CMenuWidget(LOCALE_SATSETUP_FE_SETUP, NEUTRINO_ICON_SETTINGS, width, MN_WIDGET_ID_SCAN_FE_SETUP);
		addScanMenuFrontendSetup(setupMenu);
		CMenuForwarder  * fsetupMenu = new CMenuDForwarder(LOCALE_SATSETUP_FE_SETUP, true, NULL, setupMenu, "", CRCInput::convertDigitToKey(shortcut++));
		settings->addItem(fsetupMenu);
		/* add configured satellites to satSelect */
		fillSatSelect();
	}
	else if (r_system == DVB_C) //cable
	{
		//--------------------------------------------------------------
		settings->addItem(GenericMenuSeparatorLine);
		//--------------------------------------------------------------
		fillCableSelect();
		//tune timeout
		settings->addItem(new CMenuOptionNumberChooser(LOCALE_EXTRA_ZAPIT_FE_TIMEOUT, (int *)&zapitCfg.feTimeout, true, 6, 100) );
		if(CFEManager::getInstance()->getFrontendCount() > 1) {
			femode = CFEManager::getInstance()->getMode();
			CMenuOptionChooser * mode = new CMenuOptionChooser(LOCALE_SATSETUP_FE_MODE, (int *)&femode, SATSETUP_FRONTEND_MODE, 2, true, this);
			settings->addItem(mode);
		}
#if 0
		CIntInput* nid = new CIntInput(LOCALE_SATSETUP_CABLE_NID, (int&) scansettings.cable_nid, 5, NONEXISTANT_LOCALE, NONEXISTANT_LOCALE);
		settings->addItem(new CMenuDForwarder(LOCALE_SATSETUP_CABLE_NID, true, nid->getValue(), nid));
#endif
	}
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
	settings->addItem(new CMenuForwarderNonLocalized(autoscan, true, NULL, autoScan, "", CRCInput::RC_green, NEUTRINO_ICON_BUTTON_GREEN));

	//manual scan
	CMenuWidget manualScan(LOCALE_SATSETUP_MANUAL_SCAN, NEUTRINO_ICON_SETTINGS, w/*width*/, MN_WIDGET_ID_SCAN_MANUAL_SCAN);
	addScanMenuManualScan(&manualScan);
	settings->addItem(new CMenuForwarder(LOCALE_SATSETUP_MANUAL_SCAN, true, NULL, &manualScan, "", CRCInput::RC_yellow, NEUTRINO_ICON_BUTTON_YELLOW));

	if (r_system == DVB_S)
	{
		//auto scan all
		CMenuWidget * autoScanAll = new CMenuWidget(LOCALE_SATSETUP_AUTO_SCAN_ALL, NEUTRINO_ICON_SETTINGS, w/*width*/, MN_WIDGET_ID_SCAN_AUTO_SCAN_ALL);
		addScanMenuAutoScanAll(autoScanAll);
		fautoScanAll	= new CMenuDForwarder(LOCALE_SATSETUP_AUTO_SCAN_ALL, true /*(dmode != NO_DISEQC)*/, NULL, autoScanAll, "", CRCInput::RC_blue, NEUTRINO_ICON_BUTTON_BLUE);
		settings->addItem(fautoScanAll);
#ifdef ENABLE_FASTSCAN
		//fast scan
		CMenuWidget * fastScanMenu = new CMenuWidget(LOCALE_SATSETUP_FASTSCAN_HEAD, NEUTRINO_ICON_SETTINGS, MN_WIDGET_ID_SCAN_FAST_SCAN);
		addScanMenuFastScan(fastScanMenu);
		settings->addItem(new CMenuDForwarder(LOCALE_SATSETUP_FASTSCAN_HEAD, true, NULL, fastScanMenu, "", CRCInput::convertDigitToKey(shortcut++)));
#endif /*ENABLE_FASTSCAN*/
	}

	int res = settings->exec(NULL, "");

	//delete satSelect;
	delete satOnOff;
	delete settings;
	return res;
}

void CScanSetup::addScanMenuFrontendSetup(CMenuWidget * setupMenu)
{
	int shortcut = 1;
	femode = CFEManager::getInstance()->getMode();

	setupMenu->addIntroItems();

	int count = CFEManager::getInstance()->getFrontendCount();

	CMenuOptionChooser * mode = new CMenuOptionChooser(LOCALE_SATSETUP_FE_MODE, (int *)&femode, SATSETUP_FRONTEND_MODE, SATSETUP_FRONTEND_MODE_COUNT, allow_start && (count > 1), this, CRCInput::RC_red, NEUTRINO_ICON_BUTTON_RED, true);
	setupMenu->addItem(mode);
	setupMenu->addItem(new CMenuOptionNumberChooser(LOCALE_EXTRA_ZAPIT_FE_TIMEOUT, (int *)&zapitCfg.feTimeout, true, 6, 100) );

	for(int i = 0; i < count; i++) {
		CFrontend * fe = CFEManager::getInstance()->getFE(i);

		char tmp[32];
		snprintf(tmp, sizeof(tmp), "config_frontend%d", i);
		char name[255];
		snprintf(name, sizeof(name), "%s %d: %s", g_Locale->getText(LOCALE_SATSETUP_FE_SETUP), i+1, fe->getInfo()->name);

		CMenuForwarder  * fSetup = new CMenuForwarderNonLocalized(name, ((i == 0) || (femode == CFEManager::FE_MODE_ALONE)), NULL, this, tmp,
			(i == 0) ? CRCInput::RC_green : CRCInput::RC_yellow, (i == 0) ? NEUTRINO_ICON_BUTTON_GREEN : NEUTRINO_ICON_BUTTON_YELLOW);
		setupMenu->addItem(fSetup);
		if(i != 0)
			frontendSetup = fSetup;
	}
	sprintf(zapit_lat, "%02.6f", zapitCfg.gotoXXLatitude);
	sprintf(zapit_long, "%02.6f", zapitCfg.gotoXXLongitude);

	setupMenu->addItem(new CMenuSeparator(CMenuSeparator::LINE | CMenuSeparator::STRING, LOCALE_SATSETUP_EXTENDED_MOTOR));
	setupMenu->addItem(new CMenuOptionChooser(LOCALE_EXTRA_LADIRECTION,  (int *)&zapitCfg.gotoXXLaDirection, OPTIONS_SOUTH0_NORTH1_OPTIONS, OPTIONS_SOUTH0_NORTH1_OPTION_COUNT, true, NULL, CRCInput::convertDigitToKey(shortcut++)));

	CStringInput toff1(LOCALE_EXTRA_LATITUDE, (char *) zapit_lat, 10, NONEXISTANT_LOCALE, NONEXISTANT_LOCALE, "0123456789.");
	setupMenu->addItem(new CMenuForwarder(LOCALE_EXTRA_LATITUDE, true, zapit_lat, &toff1, "", CRCInput::convertDigitToKey(shortcut++)));

	setupMenu->addItem(new CMenuOptionChooser(LOCALE_EXTRA_LODIRECTION,  (int *)&zapitCfg.gotoXXLoDirection, OPTIONS_EAST0_WEST1_OPTIONS, OPTIONS_EAST0_WEST1_OPTION_COUNT, true, NULL, CRCInput::convertDigitToKey(shortcut++)));

	CStringInput toff2(LOCALE_EXTRA_LONGITUDE, (char *) zapit_long, 10, NONEXISTANT_LOCALE, NONEXISTANT_LOCALE, "0123456789.");
	setupMenu->addItem(new CMenuForwarder(LOCALE_EXTRA_LONGITUDE, true, zapit_long, &toff2, "", CRCInput::convertDigitToKey(shortcut++)));

	setupMenu->addItem(new CMenuOptionNumberChooser(LOCALE_SATSETUP_USALS_REPEAT, (int *)&zapitCfg.repeatUsals, true, 0, 10, NULL, 0, 0, LOCALE_OPTIONS_OFF) );
}

int CScanSetup::showFrontendSetup(int number)
{
	int shortcut = 1;
	static int feselected = 0;

	printf("[neutrino] CScanSetup::%s fe %d\n", __FUNCTION__, number);

	fenumber = number;

#if 0
	itemsForAnyDiseqc.Clear();
	itemsForAdvancedDiseqc.Clear();
	itemsForNonAdvancedDiseqc.Clear();
#endif

	CFrontend * fe = CFEManager::getInstance()->getFE(number);
	frontend_config_t & fe_config = fe->getConfig();
	dmode = fe_config.diseqcType;

	char name[255];
	snprintf(name, sizeof(name), "%s %d: %s", g_Locale->getText(LOCALE_SATSETUP_FE_SETUP), number+1, fe->getInfo()->name);

	CMenuWidget * setupMenu = new CMenuWidget(name, NEUTRINO_ICON_SETTINGS, width);
	setupMenu->setSelected(feselected);
	setupMenu->addIntroItems();

	CMenuOptionChooser * ojDiseqc	= new CMenuOptionChooser(LOCALE_SATSETUP_DISEQC, (int *)&dmode, SATSETUP_DISEQC_OPTIONS, SATSETUP_DISEQC_OPTION_COUNT, allow_start, this, CRCInput::convertDigitToKey(shortcut++), "", true);
	/*CMenuOptionNumberChooser * */ ojDiseqcRepeats = new CMenuOptionNumberChooser(LOCALE_SATSETUP_DISEQCREPEAT, (int *)&fe_config.diseqcRepeats, (dmode != NO_DISEQC) && (dmode != DISEQC_ADVANCED), 0, 2, NULL);

	//itemsForAnyDiseqc.Add(ojDiseqcRepeats);

	setupMenu->addItem(ojDiseqc);
	setupMenu->addItem(ojDiseqcRepeats);

	CMenuWidget satToSelect(LOCALE_SATSETUP_SELECT_SAT, NEUTRINO_ICON_SETTINGS, width);
	satToSelect.addIntroItems();

	satellite_map_t & satmap = fe->getSatellites();
	for (sat_iterator_t sit = satmap.begin(); sit != satmap.end(); ++sit)
	{
		std::string satname = CServiceManager::getInstance()->GetSatelliteName(sit->first);
		satToSelect.addItem(new CMenuOptionChooser(satname.c_str(),  &sit->second.configured, OPTIONS_OFF0_ON1_OPTIONS, OPTIONS_OFF0_ON1_OPTION_COUNT, true));
	}
	setupMenu->addItem(new CMenuForwarder(LOCALE_SATSETUP_SELECT_SAT, true, NULL, &satToSelect, "", CRCInput::convertDigitToKey(shortcut++)));

	fsatSetup 	= new CMenuForwarder(LOCALE_SATSETUP_SAT_SETUP, true, NULL, this, "satsetup", CRCInput::convertDigitToKey(shortcut++));
	setupMenu->addItem(fsatSetup);

	setupMenu->addItem(new CMenuSeparator(CMenuSeparator::LINE | CMenuSeparator::STRING, LOCALE_SATSETUP_EXTENDED_MOTOR));
	setupMenu->addItem(new CMenuOptionNumberChooser(LOCALE_EXTRA_ZAPIT_MOTOR_SPEED, (int *)&fe_config.motorRotationSpeed, true, 0, 64, NULL) );
	setupMenu->addItem(new CMenuOptionChooser(LOCALE_EXTRA_ZAPIT_HVOLTAGE,  (int *)&fe_config.highVoltage, OPTIONS_OFF0_ON1_OPTIONS, OPTIONS_OFF0_ON1_OPTION_COUNT, true));

	CMenuOptionChooser * allusals = new CMenuOptionChooser(LOCALE_SATSETUP_USE_USALS,  &all_usals, OPTIONS_OFF0_ON1_OPTIONS, OPTIONS_OFF0_ON1_OPTION_COUNT, true, this);
	setupMenu->addItem(allusals);

	setupMenu->addItem(new CMenuForwarder(LOCALE_MOTORCONTROL_HEAD, true, NULL, this, "satfind", CRCInput::RC_red, NEUTRINO_ICON_BUTTON_RED));

	int res = setupMenu->exec(NULL, "");
	feselected = setupMenu->getSelected();

	/* add configured satellites to satSelect in case they changed */
	fillSatSelect();

	delete setupMenu;
	return res;
}

int CScanSetup::showScanMenuLnbSetup()
{
	printf("[neutrino] CScanSetup call %s fe %d\n", __FUNCTION__, fenumber);

	int count = 0;
	int res = menu_return::RETURN_REPAINT;
	CFrontend * fe = CFEManager::getInstance()->getFE(fenumber);

	char name[255];
	snprintf(name, sizeof(name), "%s %d: %s", g_Locale->getText(satprov_locale), fenumber+1, fe->getInfo()->name);

	CMenuWidget * sat_setup = new CMenuWidget(name /*satprov_locale*/, NEUTRINO_ICON_SETTINGS, width);
	sat_setup->addIntroItems();

	satellite_map_t & satmap = fe->getSatellites();
	INFO("satmap size = %d", satmap.size());

	CMenuWidget *tmp[satmap.size()];
	for (sat_iterator_t sit = satmap.begin(); sit != satmap.end(); ++sit)
	{
		if(!sit->second.configured)
			continue;

		//std::string satname = it->second.name.c_str();
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
			sat_setup->addItem(new CMenuForwarderNonLocalized(satname.c_str(), true, NULL, tempsat));
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

void CScanSetup::fillSatSelect()
{
	std::set<int> satpos;
	std::set<int>::iterator tmpit;

	satSelect->removeOptions();

	satOnOff->resetWidget(true);
	satOnOff->addIntroItems();

	bool sfound = false;
	int count = CFEManager::getInstance()->getFrontendCount();
	for(int i = 0; i < count; i++) {
		CFrontend * fe = CFEManager::getInstance()->getFE(i);
		satellite_map_t & satmap = fe->getSatellites();
		for (sat_iterator_t sit = satmap.begin(); sit != satmap.end(); ++sit) {
			tmpit = satpos.find(sit->first);
			if(sit->second.configured && tmpit == satpos.end()) {
				std::string satname = CServiceManager::getInstance()->GetSatelliteName(sit->first);
				satSelect->addOption(satname.c_str());
				satpos.insert(sit->first);

				if (!sfound && strcmp(scansettings.satNameNoDiseqc, satname.c_str()) == 0)
					sfound = true;
			}
		}
		if(CFEManager::getInstance()->getMode() != CFEManager::FE_MODE_ALONE)
			break;
	}
	if(!sfound && satpos.size()) {
		tmpit = satpos.begin();
		std::string satname = CServiceManager::getInstance()->GetSatelliteName(*tmpit);
		snprintf(scansettings.satNameNoDiseqc, sizeof(scansettings.satNameNoDiseqc), "%s", satname.c_str());
	}
	satellite_map_t & satmap = CServiceManager::getInstance()->SatelliteList();
	for (sat_iterator_t sit = satmap.begin(); sit != satmap.end(); sit++) {
		if(satpos.find(sit->first) != satpos.end())
			satOnOff->addItem(new CMenuOptionChooser(sit->second.name.c_str(),  &sit->second.use_in_scan, OPTIONS_OFF0_ON1_OPTIONS, OPTIONS_OFF0_ON1_OPTION_COUNT, true));
		else
			sit->second.use_in_scan = false;
	}
}

//init cable provider menu
void CScanSetup::fillCableSelect()
{
	printf("[neutrino] CScanSetup call %s...\n", __FUNCTION__);
	//don't misunderstand the name "satSelect", in this context it's actually for cable providers
	satellite_map_t satmap = CServiceManager::getInstance()->SatelliteList();
	bool sfound = false;
	for (sat_iterator_t sit = satmap.begin(); sit != satmap.end(); sit++)
	{
		printf("Adding cable menu for %s position %d\n", sit->second.name.c_str(), sit->first);
		satSelect->addOption(sit->second.name.c_str());

		if (!sfound && strcmp(scansettings.satNameNoDiseqc, sit->second.name.c_str()) == 0)
			sfound = true;

		dprintf(DEBUG_DEBUG, "got scanprovider (cable): %s\n", sit->second.name.c_str());
	}
	if (!sfound && satmap.size()) {
		sat_iterator_t sit = satmap.begin();
		snprintf(scansettings.satNameNoDiseqc, sizeof(scansettings.satNameNoDiseqc), "%s", sit->second.name.c_str());
	}
}

int CScanSetup::showScanMenuSatFind()
{
	printf("[neutrino] CScanSetup call %s fe %d\n", __FUNCTION__, fenumber);
	int count = 0;
	CFrontend * fe = CFEManager::getInstance()->getFE(fenumber);
	char name[255];
	snprintf(name, sizeof(name), "%s %d: %s", g_Locale->getText(LOCALE_MOTORCONTROL_HEAD), fenumber+1, fe->getInfo()->name);

	CMenuWidget* sat_findMenu = new CMenuWidget(name /*LOCALE_MOTORCONTROL_HEAD*/, NEUTRINO_ICON_SETTINGS, width);

	sat_findMenu->addIntroItems();

	CMenuOptionStringChooser * feSatSelect = new CMenuOptionStringChooser(satprov_locale, scansettings.satNameNoDiseqc, true, NULL, CRCInput::RC_red, NEUTRINO_ICON_BUTTON_RED, true);
	satellite_map_t & satmap = fe->getSatellites();
	bool sfound = false;
	std::string firstname;
	for (sat_iterator_t sit = satmap.begin(); sit != satmap.end(); ++sit) {
		if(!sit->second.configured)
			continue;
		std::string satname = CServiceManager::getInstance()->GetSatelliteName(sit->first);
		feSatSelect->addOption(satname.c_str());
		if (!sfound && strcmp(scansettings.satNameNoDiseqc, satname.c_str()) == 0)
			sfound = true;
		if (!sfound && !firstname.size())
			firstname = satname;
		count++;
	}
	if(count && !sfound)
		snprintf(scansettings.satNameNoDiseqc, sizeof(scansettings.satNameNoDiseqc), "%s", firstname.c_str());

	sat_findMenu->addItem(feSatSelect);

	CTPSelectHandler tpSelect;
	sat_findMenu->addItem(new CMenuForwarder(LOCALE_SCANTS_SELECT_TP, true, NULL, &tpSelect, "test", CRCInput::RC_green, NEUTRINO_ICON_BUTTON_GREEN));
	sat_findMenu->addItem(GenericMenuSeparatorLine);
	//--------------------------------------------------------------
	addScanOptionsItems(sat_findMenu);
	//--------------------------------------------------------------
	sat_findMenu->addItem(GenericMenuSeparatorLine);
	CMotorControl mcontrol(fenumber);
	sat_findMenu->addItem(new CMenuForwarder(LOCALE_MOTORCONTROL_HEAD, allow_start, NULL, &mcontrol, "", CRCInput::RC_yellow, NEUTRINO_ICON_BUTTON_YELLOW));

	int res = sat_findMenu->exec(NULL, "");
	delete sat_findMenu;
	return res;
}

//init tempsat menu
void CScanSetup::addScanMenuTempSat(CMenuWidget *temp_sat, sat_config_t & satconfig)
{
	temp_sat->addIntroItems();

	CMenuOptionNumberChooser	*diseqc	= new CMenuOptionNumberChooser(LOCALE_SATSETUP_DISEQC_INPUT, &satconfig.diseqc, ((dmode != NO_DISEQC) && (dmode != DISEQC_ADVANCED)), -1, 15, NULL, 1, -1, LOCALE_OPTIONS_OFF);
	CMenuOptionNumberChooser	*comm 	= new CMenuOptionNumberChooser(LOCALE_SATSETUP_COMM_INPUT, &satconfig.commited, dmode == DISEQC_ADVANCED, -1, 15, NULL, 1, -1, LOCALE_OPTIONS_OFF);
	CMenuOptionNumberChooser	*uncomm = new CMenuOptionNumberChooser(LOCALE_SATSETUP_UNCOMM_INPUT, &satconfig.uncommited, dmode == DISEQC_ADVANCED, -1, 15, NULL, 1, -1, LOCALE_OPTIONS_OFF);
	CMenuOptionNumberChooser	*motor 	= new CMenuOptionNumberChooser(LOCALE_SATSETUP_MOTOR_POS, &satconfig.motor_position, true /*dmode == DISEQC_ADVANCED*/, 0, 64, NULL, 0, 0, LOCALE_OPTIONS_OFF);
	CMenuOptionChooser		*usals 	= new CMenuOptionChooser(LOCALE_EXTRA_USE_GOTOXX,  &satconfig.use_usals, OPTIONS_OFF0_ON1_OPTIONS, OPTIONS_OFF0_ON1_OPTION_COUNT, true /*dmode == DISEQC_ADVANCED*/);

	if(!satconfig.use_usals)
		all_usals = 0;

#if 0
	itemsForAnyDiseqc.Add(diseqc);
	itemsForAdvancedDiseqc.Add(comm);
	itemsForAdvancedDiseqc.Add(uncomm);
#endif
	//FIXME testing motor without DISEQC_ADVANCED
	//itemsForAdvancedDiseqc.Add(motor);
	//itemsForAdvancedDiseqc.Add(usals);

	CIntInput* lofL = new CIntInput(LOCALE_SATSETUP_LOFL, (int&) satconfig.lnbOffsetLow, 5, NONEXISTANT_LOCALE, NONEXISTANT_LOCALE);
	CIntInput* lofH = new CIntInput(LOCALE_SATSETUP_LOFH, (int&) satconfig.lnbOffsetHigh, 5, NONEXISTANT_LOCALE, NONEXISTANT_LOCALE);
	CIntInput* lofS = new CIntInput(LOCALE_SATSETUP_LOFS, (int&) satconfig.lnbSwitch, 5, NONEXISTANT_LOCALE, NONEXISTANT_LOCALE);

	temp_sat->addItem(diseqc);
	temp_sat->addItem(comm);
	temp_sat->addItem(uncomm);
	temp_sat->addItem(motor);
	temp_sat->addItem(usals);
	temp_sat->addItem(new CMenuDForwarder(LOCALE_SATSETUP_LOFL, true, lofL->getValue(), lofL));
	temp_sat->addItem(new CMenuDForwarder(LOCALE_SATSETUP_LOFH, true, lofH->getValue(), lofH));
	temp_sat->addItem(new CMenuDForwarder(LOCALE_SATSETUP_LOFS, true, lofS->getValue(), lofS));
}

//init manual scan menu
void CScanSetup::addScanMenuManualScan(CMenuWidget *manual_Scan)
{
	printf("[neutrino] CScanSetup call %s...\n", __FUNCTION__);
	int shortCut = 1;

	manual_Scan->addIntroItems();
	//----------------------------------------------------------------------
	manual_Scan->addItem(satSelect);
	if (r_system == DVB_C)  { //cable
		CIntInput* nid = new CIntInput(LOCALE_SATSETUP_CABLE_NID, (int&) scansettings.cable_nid, 5, NONEXISTANT_LOCALE, NONEXISTANT_LOCALE);
		manual_Scan->addItem(new CMenuDForwarder(LOCALE_SATSETUP_CABLE_NID, true, nid->getValue(), nid));
	}
	manual_Scan->addItem(new CMenuDForwarder(LOCALE_SCANTS_SELECT_TP, true, NULL, new CTPSelectHandler()/*tpSelect*/, "test", CRCInput::RC_green, NEUTRINO_ICON_BUTTON_GREEN));

	manual_Scan->addItem(GenericMenuSeparatorLine);
	//----------------------------------------------------------------------
	shortCut = addScanOptionsItems(manual_Scan, shortCut);
	addListFlagsItems(manual_Scan, shortCut, true);
	//----------------------------------------------------------------------
	manual_Scan->addItem(GenericMenuSeparatorLine);

	manual_Scan->addItem(new CMenuForwarder(LOCALE_SCANTS_TEST, allow_start, NULL, this, "test", CRCInput::RC_yellow, NEUTRINO_ICON_BUTTON_YELLOW));
	manual_Scan->addItem(new CMenuForwarder(LOCALE_SCANTS_STARTNOW, allow_start, NULL, this, "manual", CRCInput::RC_blue, NEUTRINO_ICON_BUTTON_BLUE));
}

//init auto scan all menu
void CScanSetup::addScanMenuAutoScanAll(CMenuWidget *auto_ScanAll)
{
	printf("[neutrino] CScanSetup call %s...\n", __FUNCTION__);
	auto_ScanAll->addIntroItems();
	//----------------------------------------------------------------------
	auto_ScanAll->addItem(new CMenuForwarder(satprov_locale, true, NULL, satOnOff, "", CRCInput::RC_red, NEUTRINO_ICON_BUTTON_RED));

	auto_ScanAll->addItem(GenericMenuSeparatorLine);
	//----------------------------------------------------------------------
	addListFlagsItems(auto_ScanAll, 1);
	//----------------------------------------------------------------------
	auto_ScanAll->addItem(GenericMenuSeparatorLine);
	auto_ScanAll->addItem(new CMenuForwarder(LOCALE_SCANTS_STARTNOW, allow_start, NULL, this, "all", CRCInput::RC_green, NEUTRINO_ICON_BUTTON_GREEN));
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
	CMenuOptionChooser* fastType = new CMenuOptionChooser(LOCALE_SATSETUP_FASTSCAN_TYPE, (int *)&scansettings.fast_type, FAST_SCAN_OPTIONS, FAST_SCAN_OPTIONS_COUNT, true, NULL, CRCInput::RC_green, NEUTRINO_ICON_BUTTON_GREEN, true);

	//----------------------------------------------------------------------
	fast_ScanMenu->addItem(fastProv);
	fast_ScanMenu->addItem(fastType);
	//----------------------------------------------------------------------
	fast_ScanMenu->addItem(GenericMenuSeparatorLine);
	fast_ScanMenu->addItem(new CMenuForwarder(LOCALE_SCANTS_STARTNOW, allow_start, NULL, this, "fast", CRCInput::RC_yellow, NEUTRINO_ICON_BUTTON_YELLOW));
}
#endif /*ENABLE_FASTSCAN*/

//init auto scan menu
void CScanSetup::addScanMenuAutoScan(CMenuWidget *auto_Scan)
{
	printf("[neutrino] CScanSetup call %s...\n", __FUNCTION__);
	auto_Scan->addIntroItems();

	auto_Scan->addItem(satSelect);
	if (r_system == DVB_C)  { //cable
		CIntInput* nid = new CIntInput(LOCALE_SATSETUP_CABLE_NID, (int&) scansettings.cable_nid, 5, NONEXISTANT_LOCALE, NONEXISTANT_LOCALE);
		auto_Scan->addItem(new CMenuDForwarder(LOCALE_SATSETUP_CABLE_NID, true, nid->getValue(), nid));
	}
	auto_Scan->addItem(GenericMenuSeparatorLine);
	//----------------------------------------------------------------------
	addListFlagsItems(auto_Scan, 1);
	//----------------------------------------------------------------------
	auto_Scan->addItem(GenericMenuSeparatorLine);
	auto_Scan->addItem(new CMenuForwarder(LOCALE_SCANTS_STARTNOW, allow_start, NULL, this, "auto", CRCInput::RC_green, NEUTRINO_ICON_BUTTON_GREEN));
}

//create scan options items
int CScanSetup::addScanOptionsItems(CMenuWidget *options_menu, const int &shortcut)
{
	printf("[neutrino] CScanSetup call %s...\n", __FUNCTION__);
	int shortCut = shortcut;

	CStringInput		*freq	= new CStringInput(LOCALE_EXTRA_TP_FREQ, (char *) scansettings.TP_freq, freq_length, NONEXISTANT_LOCALE, NONEXISTANT_LOCALE, "0123456789");
	CMenuForwarder		*Freq 	= new CMenuDForwarder(LOCALE_EXTRA_TP_FREQ, true, scansettings.TP_freq, freq, "", CRCInput::convertDigitToKey(shortCut++));

	CStringInput		*rate 	= new CStringInput(LOCALE_EXTRA_TP_RATE, (char *) scansettings.TP_rate, 8, NONEXISTANT_LOCALE, NONEXISTANT_LOCALE, "0123456789");
	CMenuForwarder		*Rate 	= new CMenuDForwarder(LOCALE_EXTRA_TP_RATE, true, scansettings.TP_rate, rate, "", CRCInput::convertDigitToKey(shortCut++));

	CMenuOptionChooser	*fec = NULL;

	CMenuOptionChooser	*mod_pol= NULL;
	if (r_system == DVB_S) {
		fec 	= new CMenuOptionChooser(LOCALE_EXTRA_TP_FEC, (int *)&scansettings.TP_fec, SATSETUP_SCANTP_FEC, fec_count, true, NULL, CRCInput::convertDigitToKey(shortCut++), "", true);
		mod_pol = new CMenuOptionChooser(LOCALE_EXTRA_TP_POL, (int *)&scansettings.TP_pol, SATSETUP_SCANTP_POL, SATSETUP_SCANTP_POL_COUNT, true, NULL, CRCInput::convertDigitToKey(shortCut++));
	} else if (r_system == DVB_C) {
		mod_pol = new CMenuOptionChooser(LOCALE_EXTRA_TP_MOD, (int *)&scansettings.TP_mod, SATSETUP_SCANTP_MOD, SATSETUP_SCANTP_MOD_COUNT, true, NULL, CRCInput::convertDigitToKey(shortCut++));
	}

	options_menu->addItem(Freq);
	options_menu->addItem(Rate);
	if (r_system == DVB_S)
		options_menu->addItem(fec);
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

	CMenuOptionChooser *ftaFlag = new CMenuOptionChooser(LOCALE_SATSETUP_USE_FTA_FLAG, (int *)&scansettings.scan_fta_flag, OPTIONS_OFF0_ON1_OPTIONS, OPTIONS_OFF0_ON1_OPTION_COUNT, true, NULL, CRCInput::convertDigitToKey(shortCut++));
	CMenuOptionChooser *scanPid = new CMenuOptionChooser(LOCALE_EXTRA_ZAPIT_SCANPIDS,  &zapitCfg.scanPids, OPTIONS_OFF0_ON1_OPTIONS, OPTIONS_OFF0_ON1_OPTION_COUNT, true, NULL, CRCInput::convertDigitToKey(shortCut++));

	listflags_menu->addItem(useNit);
	listflags_menu->addItem(ftaFlag);
	listflags_menu->addItem(scanPid);

	CMenuOptionChooser *resetNum  = new CMenuOptionChooser(LOCALE_SATSETUP_RESET_NUMBERS, (int *)&scansettings.scan_reset_numbers, OPTIONS_OFF0_ON1_OPTIONS, OPTIONS_OFF0_ON1_OPTION_COUNT, true, NULL, CRCInput::convertDigitToKey(shortCut++));
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

	CServiceManager::getInstance()->SaveMotorPositions();
	zapitCfg.gotoXXLatitude = strtod(zapit_lat, NULL);
	zapitCfg.gotoXXLongitude = strtod(zapit_long, NULL);

	CZapit::getInstance()->SetConfig(&zapitCfg);
	CFEManager::getInstance()->saveSettings(true);
}

bool CScanSetup::changeNotify(const neutrino_locale_t OptionName, void * /*data*/)
{
	if(ARE_LOCALES_EQUAL(OptionName, LOCALE_SATSETUP_USE_USALS)) {
		CFrontend * fe = CFEManager::getInstance()->getFE(fenumber);
		printf("[neutrino] CScanSetup::%s: all usals %d \n", __FUNCTION__, all_usals);
		satellite_map_t & satmap = fe->getSatellites();
		for (sat_iterator_t sit = satmap.begin(); sit != satmap.end(); sit++) {
			sit->second.use_usals = all_usals;
		}
	}
	else if(ARE_LOCALES_EQUAL(OptionName, LOCALE_SATSETUP_DISEQC)) {
		printf("[neutrino] CScanSetup::%s: diseqc %d \n", __FUNCTION__, dmode);
		//FIXME 2 frontends ??
		fautoScanAll->setActive(dmode != NO_DISEQC);
		if(dmode == NO_DISEQC) {
			ojDiseqcRepeats->setActive(false);
#if 0
			itemsForAnyDiseqc.Activate(false);
			itemsForAdvancedDiseqc.Activate(false);
			itemsForNonAdvancedDiseqc.Activate(false);
#endif
		}
		else if(dmode < DISEQC_ADVANCED) {
			ojDiseqcRepeats->setActive(true);
#if 0
			itemsForAnyDiseqc.Activate(true);
			itemsForAdvancedDiseqc.Activate(false);
			itemsForNonAdvancedDiseqc.Activate(true);
#endif
		}
		else if(dmode == DISEQC_ADVANCED) {
			ojDiseqcRepeats->setActive(true);
#if 0
			itemsForAnyDiseqc.Activate(true);
			itemsForAdvancedDiseqc.Activate(true);
			itemsForNonAdvancedDiseqc.Activate(false);
#endif
		}
		CFrontend * fe = CFEManager::getInstance()->getFE(fenumber);
		fe->setDiseqcType((diseqc_t) dmode);
	}
	else if(ARE_LOCALES_EQUAL(OptionName, LOCALE_SATSETUP_FE_MODE)) {
		printf("[neutrino] CScanSetup::%s: fe mode %d \n", __FUNCTION__, femode);
		/* cable dont have this menu */
		if(frontendSetup)
			frontendSetup->setActive(femode ==  CFEManager::FE_MODE_ALONE);
		CFEManager::getInstance()->setMode((CFEManager::fe_mode_t) femode);
		/* to copy settings from fe0 */
		if(femode !=  CFEManager::FE_MODE_ALONE)
			CFEManager::getInstance()->saveSettings(true);
		if (r_system == DVB_S) //sat
			fillSatSelect();
	}
	return false;
}

void CScanSetup::updateManualSettings()
{
	CZapitChannel * channel = CZapit::getInstance()->GetCurrentChannel();
	if(channel) {
		strncpy(scansettings.satNameNoDiseqc,
			CServiceManager::getInstance()->GetSatelliteName(channel->getSatellitePosition()).c_str(), 50);
		transponder_list_t::iterator tI;
		tI = transponders.find(channel->getTransponderId());
		if(tI != transponders.end()) {
			sprintf(scansettings.TP_freq, "%d", tI->second.feparams.frequency);
			CFrontend * frontend = CFEManager::getInstance()->getLiveFE();
			switch (frontend->getInfo()->type) {
				case FE_QPSK:
					sprintf(scansettings.TP_rate, "%d", tI->second.feparams.u.qpsk.symbol_rate);
					scansettings.TP_fec = tI->second.feparams.u.qpsk.fec_inner;
					scansettings.TP_pol = tI->second.polarization;
					break;
				case FE_QAM:
					sprintf(scansettings.TP_rate, "%d", tI->second.feparams.u.qam.symbol_rate);
					scansettings.TP_fec = tI->second.feparams.u.qam.fec_inner;
					scansettings.TP_mod = tI->second.feparams.u.qam.modulation;
					break;
				case FE_OFDM:
				case FE_ATSC:
					break;
			}
		}
	}
}

int CTPSelectHandler::exec(CMenuTarget* parent, const std::string &/*actionkey*/)
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

	t_satellite_position position = CServiceManager::getInstance()->GetSatellitePosition(scansettings.satNameNoDiseqc);

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

	CFrontend * frontend = CFEManager::getInstance()->getLiveFE();
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
		text += scansettings.satNameNoDiseqc;
		ShowHintUTF(LOCALE_MESSAGEBOX_ERROR, text.c_str(), 450, 2);
		return menu_return::RETURN_REPAINT;
	}

	menu.setSelected(old_selected);
	int retval = menu.exec(NULL, "");

	delete selector;

	if (select >= 0) {
		old_selected = select;

		tmpI = tmplist.find(select);

		printf("CTPSelectHandler::exec: selected TP: freq %d pol %d SR %d\n", tmpI->second.feparams.frequency,
		       tmpI->second.polarization, tmpI->second.feparams.u.qpsk.symbol_rate);

		sprintf(scansettings.TP_freq, "%d", tmpI->second.feparams.frequency);

		switch (frontend->getInfo()->type) {
			case FE_QPSK:
				sprintf(scansettings.TP_rate, "%d", tmpI->second.feparams.u.qpsk.symbol_rate);
				scansettings.TP_fec = tmpI->second.feparams.u.qpsk.fec_inner;
				scansettings.TP_pol = tmpI->second.polarization;
				break;
			case FE_QAM:
				sprintf(scansettings.TP_rate, "%d", tmpI->second.feparams.u.qam.symbol_rate);
				scansettings.TP_fec = tmpI->second.feparams.u.qam.fec_inner;
				scansettings.TP_mod = tmpI->second.feparams.u.qam.modulation;
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
