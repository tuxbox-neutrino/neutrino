/*
	scan_setup menue - Neutrino-GUI

	Copyright (C) 2001 Steffen Hehn 'McClean'
	and some other guys
	Homepage: http://dbox.cyberphoria.org/

	Reworked as similar to $Id: scan_setup.cpp,v 1.10 2010/12/05 22:32:12 tuxbox-cvs Exp $
	Copyright (C) 2011 T. Graf 'dbt'
	Homepage: http://www.dbox2-tuning.net/

	Copyright (C) 2011-2015 Stefan Seyfried
	Copyright (C) 2011-2014 CoolStream International Ltd

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

#include <unistd.h>

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
#include <driver/fontrenderer.h>
#include <driver/record.h>
#include <system/debug.h>
#include <system/helpers.h>

#include <zapit/femanager.h>
#include <zapit/getservices.h>
#include <zapit/scan.h>
#include <zapit/zapit.h>
#include <zapit/debug.h>
#include <set>

//extern std::map<transponder_id_t, transponder> select_transponders;
extern Zapit_config zapitCfg;
extern char zapit_lat[20];
extern char zapit_long[20];

//static int all_usals = 1;
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

#define SATSETUP_DISEQC_OPTION_COUNT 7
const CMenuOptionChooser::keyval SATSETUP_DISEQC_OPTIONS[SATSETUP_DISEQC_OPTION_COUNT] =
{
	{ NO_DISEQC,		LOCALE_SATSETUP_NODISEQC },
	{ MINI_DISEQC,		LOCALE_SATSETUP_MINIDISEQC },
	{ DISEQC_1_0,		LOCALE_SATSETUP_DISEQC10 },
	{ DISEQC_1_1,		LOCALE_SATSETUP_DISEQC11 },
	/*{ DISEQC_1_2,	LOCALE_SATSETUP_DISEQC12 },*/
	{ DISEQC_ADVANCED,	LOCALE_SATSETUP_DISEQC_ADVANCED },
	{ DISEQC_UNICABLE,	LOCALE_SATSETUP_UNICABLE },
	{ DISEQC_UNICABLE2,	LOCALE_SATSETUP_UNICABLE2 }
//	{ SMATV_REMOTE_TUNING,	LOCALE_SATSETUP_SMATVREMOTE }
};

#define SATSETUP_SCANTP_DELSYS_COUNT	5
const CMenuOptionChooser::keyval SATSETUP_SCANTP_DELSYS[SATSETUP_SCANTP_DELSYS_COUNT] =
{
	{ DVB_S,		LOCALE_EXTRA_TP_DELSYS_DVBS },
	{ DVB_S2,		LOCALE_EXTRA_TP_DELSYS_DVBS2 },
	{ DSS,			LOCALE_EXTRA_TP_DELSYS_DSS },
	{ ISDBS,		LOCALE_EXTRA_TP_DELSYS_ISDBS },
	{ TURBO,		LOCALE_EXTRA_TP_DELSYS_TURBO }
};

#define CABLESETUP_SCANTP_DELSYS_COUNT	2
const CMenuOptionChooser::keyval CABLESETUP_SCANTP_DELSYS[CABLESETUP_SCANTP_DELSYS_COUNT] =
{
	{ DVB_C,		LOCALE_EXTRA_TP_DELSYS_DVBC  },
	{ ISDBC,		LOCALE_EXTRA_TP_DELSYS_ISDBC }
};

#define TERRSETUP_SCANTP_DELSYS_COUNT	4
const CMenuOptionChooser::keyval TERRSETUP_SCANTP_DELSYS[TERRSETUP_SCANTP_DELSYS_COUNT] =
{
	{ DVB_T,		LOCALE_EXTRA_TP_DELSYS_DVBT  },
	{ DVB_T2,		LOCALE_EXTRA_TP_DELSYS_DVBT2 },
	{ DTMB,			LOCALE_EXTRA_TP_DELSYS_DTMB  },
	{ ISDBT,		LOCALE_EXTRA_TP_DELSYS_ISDBT }
};

#define SATSETUP_SCANTP_FEC_COUNT 10
//#define CABLESETUP_SCANTP_FEC_COUNT 5
const CMenuOptionChooser::keyval SATSETUP_SCANTP_FEC[SATSETUP_SCANTP_FEC_COUNT] =
{
	// S
	{ FEC_1_2, LOCALE_EXTRA_FEC_1_2   },
	{ FEC_2_3, LOCALE_EXTRA_FEC_2_3   },
	{ FEC_3_4, LOCALE_EXTRA_FEC_3_4   },
	{ FEC_5_6, LOCALE_EXTRA_FEC_5_6   },
	{ FEC_7_8, LOCALE_EXTRA_FEC_7_8   },
	{ FEC_AUTO, LOCALE_EXTRA_FEC_AUTO },
	// S2
	{ FEC_3_5, LOCALE_EXTRA_FEC_3_5   },
	{ FEC_4_5, LOCALE_EXTRA_FEC_4_5   },
	{ FEC_8_9, LOCALE_EXTRA_FEC_8_9   },
	{ FEC_9_10, LOCALE_EXTRA_FEC_9_10 }
};

#if 0
#define CABLESETUP_SCANTP_FEC_COUNT	6
const CMenuOptionChooser::keyval CABLESETUP_SCANTP_FEC[CABLESETUP_SCANTP_FEC_COUNT] =
{
	{ FEC_1_2, LOCALE_EXTRA_FEC_1_2   },
	{ FEC_2_3, LOCALE_EXTRA_FEC_2_3   },
	{ FEC_3_4, LOCALE_EXTRA_FEC_3_4   },
	{ FEC_5_6, LOCALE_EXTRA_FEC_5_6   },
	{ FEC_7_8, LOCALE_EXTRA_FEC_7_8   },
	{ FEC_AUTO, LOCALE_EXTRA_FEC_AUTO },
};
#endif

#if _HAVE_DVB57
#define TERRSETUP_SCANTP_FEC_COUNT	9
#else
#define TERRSETUP_SCANTP_FEC_COUNT	8
#endif
const CMenuOptionChooser::keyval TERRSETUP_SCANTP_FEC[TERRSETUP_SCANTP_FEC_COUNT] =
{
	// DVB-T
	{ FEC_1_2,  LOCALE_EXTRA_FEC_1_2  },
	{ FEC_2_3,  LOCALE_EXTRA_FEC_2_3  },
	{ FEC_3_4,  LOCALE_EXTRA_FEC_3_4  },
	{ FEC_5_6,  LOCALE_EXTRA_FEC_5_6  },
	{ FEC_7_8,  LOCALE_EXTRA_FEC_7_8  },
	{ FEC_AUTO, LOCALE_EXTRA_FEC_AUTO },
	// DTMB ie
#if _HAVE_DVB57
	{ FEC_2_5,  LOCALE_EXTRA_FEC_2_5  }, 
#endif
	{ FEC_3_5,  LOCALE_EXTRA_FEC_3_5  }, 
	{ FEC_4_5,  LOCALE_EXTRA_FEC_4_5  }, 

};

#if _HAVE_DVB57
#define TERRSETUP_SCANTP_BW_COUNT	7
#else
#define TERRSETUP_SCANTP_BW_COUNT	4
#endif
const CMenuOptionChooser::keyval TERRSETUP_SCANTP_BW[TERRSETUP_SCANTP_BW_COUNT] =
{
	{ BANDWIDTH_8_MHZ,     LOCALE_EXTRA_TP_BANDWIDTH_8MHZ     },
	{ BANDWIDTH_7_MHZ,     LOCALE_EXTRA_TP_BANDWIDTH_7MHZ     },
	{ BANDWIDTH_6_MHZ,     LOCALE_EXTRA_TP_BANDWIDTH_6MHZ     },
#if _HAVE_DVB57
	{ BANDWIDTH_5_MHZ,     LOCALE_EXTRA_TP_BANDWIDTH_5MHZ     },
	{ BANDWIDTH_10_MHZ,    LOCALE_EXTRA_TP_BANDWIDTH_10MHZ    },
	{ BANDWIDTH_1_712_MHZ, LOCALE_EXTRA_TP_BANDWIDTH_1_712MHZ },
#endif
	{ BANDWIDTH_AUTO,      LOCALE_EXTRA_TP_BANDWIDTH_AUTO     }
};

#if _HAVE_DVB57
#define TERRSETUP_SCANTP_GI_COUNT	11
#else
#define TERRSETUP_SCANTP_GI_COUNT	5
#endif
const CMenuOptionChooser::keyval TERRSETUP_SCANTP_GI[TERRSETUP_SCANTP_GI_COUNT] =
{
	{ GUARD_INTERVAL_1_4,    LOCALE_EXTRA_TP_GI_1_4    },
	{ GUARD_INTERVAL_1_8,    LOCALE_EXTRA_TP_GI_1_8    },
	{ GUARD_INTERVAL_1_16,   LOCALE_EXTRA_TP_GI_1_16   },
	{ GUARD_INTERVAL_1_32,   LOCALE_EXTRA_TP_GI_1_32   },
#if _HAVE_DVB57
	{ GUARD_INTERVAL_1_128,  LOCALE_EXTRA_TP_GI_1_128  },
	{ GUARD_INTERVAL_19_128, LOCALE_EXTRA_TP_GI_19_128 },
	{ GUARD_INTERVAL_19_256, LOCALE_EXTRA_TP_GI_19_256 },
	{ GUARD_INTERVAL_PN420,  LOCALE_EXTRA_TP_GI_PN420  },
	{ GUARD_INTERVAL_PN595,  LOCALE_EXTRA_TP_GI_PN595  },
	{ GUARD_INTERVAL_PN945,  LOCALE_EXTRA_TP_GI_PN945  },
#endif
	{ GUARD_INTERVAL_AUTO,   LOCALE_EXTRA_TP_GI_AUTO   }
};

#define TERRSETUP_SCANTP_HIERARCHY_COUNT	5
const CMenuOptionChooser::keyval TERRSETUP_SCANTP_HIERARCHY[TERRSETUP_SCANTP_HIERARCHY_COUNT] =
{
	{ HIERARCHY_NONE, LOCALE_EXTRA_TP_HIERARCHY_NONE },
	{ HIERARCHY_1,    LOCALE_EXTRA_TP_HIERARCHY_1    },
	{ HIERARCHY_2,    LOCALE_EXTRA_TP_HIERARCHY_2    },
	{ HIERARCHY_4,    LOCALE_EXTRA_TP_HIERARCHY_4    },
	{ HIERARCHY_AUTO, LOCALE_EXTRA_TP_HIERARCHY_AUTO }
};

#if _HAVE_DVB57
#define TERRSETUP_SCANTP_TRANSMIT_MODE_COUNT	9
#else
#define TERRSETUP_SCANTP_TRANSMIT_MODE_COUNT	4
#endif
const CMenuOptionChooser::keyval TERRSETUP_SCANTP_TRANSMIT_MODE[TERRSETUP_SCANTP_TRANSMIT_MODE_COUNT] =
{
#if _HAVE_DVB57
	{ TRANSMISSION_MODE_1K,    LOCALE_EXTRA_TP_TRANSMIT_MODE_1K    },
#endif
	{ TRANSMISSION_MODE_2K,    LOCALE_EXTRA_TP_TRANSMIT_MODE_2K    },
	{ TRANSMISSION_MODE_4K,    LOCALE_EXTRA_TP_TRANSMIT_MODE_4K    },
	{ TRANSMISSION_MODE_8K,    LOCALE_EXTRA_TP_TRANSMIT_MODE_8K    },
#if _HAVE_DVB57
	{ TRANSMISSION_MODE_16K,   LOCALE_EXTRA_TP_TRANSMIT_MODE_16K   },
	{ TRANSMISSION_MODE_32K,   LOCALE_EXTRA_TP_TRANSMIT_MODE_32K   },
	{ TRANSMISSION_MODE_C1,    LOCALE_EXTRA_TP_TRANSMIT_MODE_C1    },
	{ TRANSMISSION_MODE_C3780, LOCALE_EXTRA_TP_TRANSMIT_MODE_C3780 },
#endif
	{ TRANSMISSION_MODE_AUTO,  LOCALE_EXTRA_TP_TRANSMIT_MODE_AUTO  }
};

#define SATSETUP_SCANTP_MOD_COUNT 3
const CMenuOptionChooser::keyval SATSETUP_SCANTP_MOD[SATSETUP_SCANTP_MOD_COUNT] =
{
	{ QPSK,     LOCALE_EXTRA_TP_MOD_4    },
	{ PSK_8,    LOCALE_EXTRA_TP_MOD_8    },
	{ QAM_AUTO, LOCALE_EXTRA_TP_MOD_AUTO }
};

#define CABLESETUP_SCANTP_MOD_COUNT 6
const CMenuOptionChooser::keyval CABLESETUP_SCANTP_MOD[CABLESETUP_SCANTP_MOD_COUNT] =
{
	{ QAM_16,   LOCALE_EXTRA_TP_MOD_16   },
	{ QAM_32,   LOCALE_EXTRA_TP_MOD_32   },
	{ QAM_64,   LOCALE_EXTRA_TP_MOD_64   },
	{ QAM_128,  LOCALE_EXTRA_TP_MOD_128  },
	{ QAM_256,  LOCALE_EXTRA_TP_MOD_256  },
	{ QAM_AUTO, LOCALE_EXTRA_TP_MOD_AUTO }
};

#if _HAVE_DVB57
#define TERRSETUP_SCANTP_MOD_COUNT 6
#else
#define TERRSETUP_SCANTP_MOD_COUNT 5
#endif
const CMenuOptionChooser::keyval TERRSETUP_SCANTP_MOD[TERRSETUP_SCANTP_MOD_COUNT] =
{
	{ QPSK,     LOCALE_EXTRA_TP_MOD_4    },
	{ QAM_16,   LOCALE_EXTRA_TP_MOD_16   },
	{ QAM_32,   LOCALE_EXTRA_TP_MOD_32   },
	{ QAM_64,   LOCALE_EXTRA_TP_MOD_64   },
#if _HAVE_DVB57
	{ QAM_4_NR, LOCALE_EXTRA_TP_MOD_4_NR },
#endif
	{ QAM_AUTO, LOCALE_EXTRA_TP_MOD_AUTO }
};

#define SATSETUP_SCANTP_POL_COUNT 4
const CMenuOptionChooser::keyval SATSETUP_SCANTP_POL[SATSETUP_SCANTP_POL_COUNT] =
{
	{ 0, LOCALE_EXTRA_TP_POL_H },
	{ 1, LOCALE_EXTRA_TP_POL_V },
	{ 2, LOCALE_EXTRA_TP_POL_L },
	{ 3, LOCALE_EXTRA_TP_POL_R }
};

#define SATSETUP_SCANTP_PILOT_COUNT 4
const CMenuOptionChooser::keyval SATSETUP_SCANTP_PILOT[SATSETUP_SCANTP_PILOT_COUNT] =
{
	{ ZPILOT_ON,      LOCALE_OPTIONS_ON },
	{ ZPILOT_OFF,     LOCALE_OPTIONS_OFF },
	{ ZPILOT_AUTO,    LOCALE_EXTRA_TP_PILOT_AUTO },
	{ ZPILOT_AUTO_SW, LOCALE_EXTRA_TP_PILOT_AUTO_SW }
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

#define FRONTEND_FORCE_MODE_COUNT 3
const CMenuOptionChooser::keyval FRONTEND_FORCE_MODE[FRONTEND_FORCE_MODE_COUNT] =
{
	{ 0, LOCALE_TUNERSETUP_HYBRID },
	{ 1, LOCALE_TUNERSETUP_CABLE },
	{ 2, LOCALE_TUNERSETUP_TERR }
};

#define SATSETUP_SCANTP_PLM_COUNT 4
const CMenuOptionChooser::keyval SATSETUP_SCANTP_PLM[SATSETUP_SCANTP_PLM_COUNT] =
{
	{ 0, LOCALE_EXTRA_TP_PLM_ROOT },
	{ 1, LOCALE_EXTRA_TP_PLM_GOLD },
	{ 2, LOCALE_EXTRA_TP_PLM_COMBO },
	{ 3, LOCALE_EXTRA_TP_PLM_UNK }
};

CScanSetup::CScanSetup(int wizard_mode)
{
	width = 40;
	is_wizard = wizard_mode;

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
	in_menu		= false;
	allow_start	= true;
	tsp			= NULL;
	if (CFEManager::getInstance()->haveCable())
		nid = new CIntInput(LOCALE_SATSETUP_CABLE_NID, (int*) &scansettings.cable_nid, 5, NONEXISTANT_LOCALE, NONEXISTANT_LOCALE);
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
	delete nid;
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
		CHintBox chb(LOCALE_MESSAGEBOX_INFO, g_Locale->getText(LOCALE_SERVICEMENU_RELOAD_HINT));
		chb.paint();
		/* save if changed, to make sure NEW/REMOVED/... flags are updated */
		CServiceManager::getInstance()->SaveServices(true, true);
		/* Z->reinitChannels triggers EVT_SERVICESCHANGED and this triggers channelsInit() */
		g_Zapit->reinitChannels();
		chb.hide();
		CNeutrinoApp::getInstance ()->SDTreloadChannels = false;
		if(file_exists(CURRENTSERVICES_XML)){
			unlink(CURRENTSERVICES_XML);
		}
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
			//g_settings.keep_channel_numbers = 1;
			CServiceManager::getInstance()->KeepNumbers(g_settings.keep_channel_numbers);
		}
		CScanTs scanTs(ALL_CABLE);
		scanTs.exec(NULL, "manual");
		if (is_wizard)
			return menu_return::RETURN_EXIT_ALL;
		return res;
	}
	if (actionKey == "fastdiseqc") {
		printf("[neutrino] CScanSetup::%s: showFastscanDiseqcSetup()\n", __FUNCTION__);
#ifdef ENABLE_FASTSCAN
		return showFastscanDiseqcSetup();
#else
		return res;
#endif
	}
	std::string scants_key[] = {"all", "manual", "test", "fast", "auto"/*doesn't exists in CScanTs!*/};

	if (actionKey.size() > 1) {
		delivery_system_t delsys;
		switch (actionKey[0]) {
			case 's': delsys = ALL_SAT; break;
			case 't': delsys = ALL_TERR;  break;
			default:  delsys = ALL_CABLE; break;
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
#if 0
				/* FIXME save fst version. other than fast scan will reset it to 0
				   to disable fast scan updates */
				scansettings.fst_version = CServiceScan::getInstance()->GetFstVersion();
#endif
				if (as == "fast") {
					//scansettings.fst_update = 1;
					if (is_wizard)
						return menu_return::RETURN_EXIT_ALL;
				}
				return res;
			}
		}
	}


	printf("[neutrino] CScanSetup %s: init scan setup (Mode: %d)...\n",__FUNCTION__ , is_wizard);
	CZapit::getInstance()->GetConfig(zapitCfg);
	in_menu = true;
	res = showScanMenu();
	in_menu = false;

	return res;
}

//scale to max sat/cable name lenght
unsigned int CScanSetup::getSatMenuListWidth()
{
	printf("[neutrino] CScanSetup call %s...\n", __FUNCTION__);
	unsigned int sat_txt_w = 0, max_txt_w = 0;

	satellite_map_t satmap = CServiceManager::getInstance()->SatelliteList();
	for(sat_iterator_t it = satmap.begin(); it != satmap.end(); ++it) {
		sat_txt_w = g_Font[SNeutrinoSettings::FONT_TYPE_MENU]->getRenderWidth(it->second.name);
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
	CMenuWidget * settings = new CMenuWidget(/*is_wizard ? LOCALE_SERVICEMENU_SCANTS :*/ LOCALE_SERVICEMENU_HEAD, NEUTRINO_ICON_SETTINGS, width, MN_WIDGET_ID_SCAN_MAIN);
	settings->setWizardMode(is_wizard);

	//back
	settings->addIntroItems(/*is_wizard ? NONEXISTANT_LOCALE : */ LOCALE_SERVICEMENU_SCANTS);
	//----------------------------------------------------------------------
#if 0
	//save scan settings
	mf = new CMenuForwarder(LOCALE_MAINSETTINGS_SAVESETTINGSNOW, true, NULL, this, "save_scansettings", CRCInput::RC_red);
	mf->setHint("", LOCALE_MENU_HINT_SCAN_SAVESETTINGS);
	settings->addItem(mf);
	//----------------------------------------------------------------------
	settings->addItem(GenericMenuSeparatorLine);
	//----------------------------------------------------------------------
#endif
	//sat/provider selector
#if 0
	if(CFEManager::getInstance()->haveSat() || CFEManager::getInstance()->getFrontendCount() > 1) {
		mf = new CMenuForwarder(LOCALE_SATSETUP_FE_SETUP, allow_start, NULL, this, "setup_frontend", CRCInput::convertDigitToKey(shortcut++));
		mf->setHint("", LOCALE_MENU_HINT_SCAN_FESETUP);
		settings->addItem(mf);
	}
#endif
	if (CFEManager::getInstance()->haveSat()) {
		r_system = ALL_SAT;

		//settings->addItem(new CMenuSeparator(CMenuSeparator::LINE | CMenuSeparator::STRING, LOCALE_SCANTS_PREVERENCES_RECEIVING_SYSTEM ));

		satOnOff = new CMenuWidget(LOCALE_SATSETUP_SATELLITE, NEUTRINO_ICON_SETTINGS, width);

		//auto scan
		char autoscan[64];
		std::string s_capt_part = g_Locale->getText(LOCALE_SATSETUP_SATELLITE);
		snprintf(autoscan, 64, g_Locale->getText(LOCALE_SATSETUP_AUTO_SCAN), s_capt_part.c_str());

		CMenuWidget * autoScan = new CMenuWidget(LOCALE_SERVICEMENU_SCANTS, NEUTRINO_ICON_SETTINGS, w/*width*/, MN_WIDGET_ID_SCAN_AUTO_SCAN);
		addScanMenuAutoScan(autoScan);
		mf = new CMenuDForwarder(autoscan, true, NULL, autoScan, "", CRCInput::RC_red);
		mf->setHint("", LOCALE_MENU_HINT_SCAN_AUTO);
		settings->addItem(mf);

		//manual scan
		CMenuWidget * manualScan = new CMenuWidget(LOCALE_SATSETUP_MANUAL_SCAN, NEUTRINO_ICON_SETTINGS, w/*width*/, MN_WIDGET_ID_SCAN_MANUAL_SCAN);
		addScanMenuManualScan(manualScan);
		mf = new CMenuDForwarder(LOCALE_SATSETUP_MANUAL_SCAN, true, NULL, manualScan, "", CRCInput::RC_yellow);
		mf->setHint("", LOCALE_MENU_HINT_SCAN_MANUAL);
		settings->addItem(mf);
		//auto scan all
		CMenuWidget * autoScanAll = new CMenuWidget(LOCALE_SATSETUP_AUTO_SCAN_ALL, NEUTRINO_ICON_SETTINGS, w/*width*/, MN_WIDGET_ID_SCAN_AUTO_SCAN_ALL);
		addScanMenuAutoScanAll(autoScanAll);
		fautoScanAll = new CMenuDForwarder(LOCALE_SATSETUP_AUTO_SCAN_ALL, true /*(dmode != NO_DISEQC)*/, NULL, autoScanAll, "", CRCInput::RC_blue);
		fautoScanAll->setHint("", LOCALE_MENU_HINT_SCAN_AUTOALL);
		settings->addItem(fautoScanAll);
#ifdef ENABLE_FASTSCAN
		//fast scan
		CMenuWidget * fastScanMenu = new CMenuWidget(LOCALE_SATSETUP_FASTSCAN_HEAD, NEUTRINO_ICON_SETTINGS, w, MN_WIDGET_ID_SCAN_FAST_SCAN);
		addScanMenuFastScan(fastScanMenu);
		mf = new CMenuDForwarder(LOCALE_SATSETUP_FASTSCAN_HEAD, true, NULL, fastScanMenu, "", CRCInput::RC_blue);
		mf->setHint("", LOCALE_MENU_HINT_SCAN_FAST);
		settings->addItem(mf);
#endif
		//signal test
		CMenuWidget * sTest = new CMenuWidget(LOCALE_SCANTS_TEST, NEUTRINO_ICON_SETTINGS, w/*width*/, MN_WIDGET_ID_SCAN_MANUAL_SCAN);
		addScanMenuManualScan(sTest, true);
		mf = new CMenuDForwarder(LOCALE_SCANTS_TEST, true, NULL, sTest, "", CRCInput::RC_green);
		mf->setHint("", LOCALE_MENU_HINT_SCAN_TEST);
		settings->addItem(mf);
		settings->addItem(GenericMenuSeparatorLine);
	}

	if (CFEManager::getInstance()->haveCable()) {
		r_system = ALL_CABLE;

		//tune timeout
		if(CFEManager::getInstance()->getFrontendCount() <= 1) {
			CMenuOptionNumberChooser * nc = new CMenuOptionNumberChooser(LOCALE_EXTRA_ZAPIT_FE_TIMEOUT, (int *)&zapitCfg.feTimeout, true, 6, 100);
			nc->setNumberFormat(std::string("%d00 ") + g_Locale->getText(LOCALE_UNIT_SHORT_MILLISECOND));
			nc->setHint("", LOCALE_MENU_HINT_SCAN_FETIMEOUT);
			settings->addItem(nc);
		}
		//nid = new CIntInput(LOCALE_SATSETUP_CABLE_NID, (int&) scansettings.cable_nid, 5, NONEXISTANT_LOCALE, NONEXISTANT_LOCALE);

		//auto scan
		char autoscan[64];
		std::string s_capt_part = g_Locale->getText(LOCALE_CABLESETUP_PROVIDER);
		snprintf(autoscan, 64, g_Locale->getText(LOCALE_SATSETUP_AUTO_SCAN), s_capt_part.c_str());
		bool have_sat = CFEManager::getInstance()->haveSat();

		CMenuWidget * autoScan = new CMenuWidget(LOCALE_SERVICEMENU_SCANTS, NEUTRINO_ICON_SETTINGS, w/*width*/, MN_WIDGET_ID_SCAN_AUTO_SCAN);
		addScanMenuAutoScan(autoScan);
		mf = new CMenuDForwarder(autoscan, true, NULL, autoScan, "", have_sat ? CRCInput::convertDigitToKey(shortcut++) : CRCInput::RC_red);
		mf->setHint("", LOCALE_MENU_HINT_SCAN_AUTO);
		settings->addItem(mf);

		//manual scan
		CMenuWidget * manualScan = new CMenuWidget(LOCALE_SATSETUP_MANUAL_SCAN, NEUTRINO_ICON_SETTINGS, w/*width*/, MN_WIDGET_ID_SCAN_MANUAL_SCAN);
		addScanMenuManualScan(manualScan);
		mf = new CMenuDForwarder(LOCALE_SATSETUP_MANUAL_SCAN, true, NULL, manualScan, "", have_sat ? CRCInput::convertDigitToKey(shortcut++) : CRCInput::RC_yellow);
		mf->setHint("", LOCALE_MENU_HINT_SCAN_MANUAL);
		settings->addItem(mf);
		//simple cable scan
		CMenuWidget * cableScan = new CMenuWidget(LOCALE_SATSETUP_CABLE, NEUTRINO_ICON_SETTINGS, w/*width*/, MN_WIDGET_ID_SCAN_CABLE_SCAN);
		addScanMenuCable(cableScan);
		CMenuForwarder * fcableScan = new CMenuDForwarder(LOCALE_SATSETUP_CABLE, true, NULL, cableScan, "", have_sat ? CRCInput::convertDigitToKey(shortcut++) : CRCInput::RC_blue);
		fcableScan->setHint("", LOCALE_MENU_HINT_SCAN_CABLE_SIMPLE);
		settings->addItem(fcableScan);
		settings->addItem(GenericMenuSeparatorLine);
	}

	if (CFEManager::getInstance()->haveTerr()) {
		r_system = ALL_TERR;
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
		mf = new CMenuDForwarder(autoscan, true, NULL, autoScan, "", have_other ? CRCInput::convertDigitToKey(shortcut++) : CRCInput::RC_red);
		mf->setHint("", LOCALE_MENU_HINT_SCAN_AUTO);
		settings->addItem(mf);

		//manual scan
		CMenuWidget * manualScan = new CMenuWidget(LOCALE_SATSETUP_MANUAL_SCAN, NEUTRINO_ICON_SETTINGS, w, MN_WIDGET_ID_SCAN_MANUAL_SCAN);
		addScanMenuManualScan(manualScan);
		mf = new CMenuForwarder(LOCALE_SATSETUP_MANUAL_SCAN, true, NULL, manualScan, "", have_other ? CRCInput::convertDigitToKey(shortcut++) : CRCInput::RC_yellow);
		mf->setHint("", LOCALE_MENU_HINT_SCAN_MANUAL);
		settings->addItem(mf);
		settings->addItem(GenericMenuSeparatorLine);
	}
	//service select mode
	mc = new CMenuOptionChooser(LOCALE_ZAPIT_SCANTYPE, (int *)&scansettings.scanType, SCANTS_ZAPIT_SCANTYPE, SCANTS_ZAPIT_SCANTYPE_COUNT, true, NULL, CRCInput::convertDigitToKey(shortcut++), "", true);
	mc->setHint("", LOCALE_MENU_HINT_SCAN_SCANTYPE);
	settings->addItem(mc);

	//bouquet generate mode
	mc = new CMenuOptionChooser(LOCALE_SCANTS_BOUQUET, (int *)&scansettings.bouquetMode, SCANTS_BOUQUET_OPTIONS, SCANTS_BOUQUET_OPTION_COUNT, true, NULL, CRCInput::convertDigitToKey(shortcut++), "", true);
	mc->setHint("", LOCALE_MENU_HINT_SCAN_BOUQUET);
	settings->addItem(mc);

	//bouquet write_names selection
	const short SCANTS_BOUQUET_WRITENAMES_COUNT = 4;
	const CMenuOptionChooser::keyval SCANTS_BOUQUET_WRITENAMES[SCANTS_BOUQUET_WRITENAMES_COUNT] =
	{
		{ CBouquetManager::BWN_NEVER		, LOCALE_SCANTS_BOUQUET_WRITENAMES_NEVER     	},
		{ CBouquetManager::BWN_UBOUQUETS    	, LOCALE_SCANTS_BOUQUET_WRITENAMES_UBOUQUETS   	},
		{ CBouquetManager::BWN_BOUQUETS     	, LOCALE_SCANTS_BOUQUET_WRITENAMES_BOUQUETS    	},
		{ CBouquetManager::BWN_EVER        	, LOCALE_SCANTS_BOUQUET_WRITENAMES_EVER    	}
	};

	int tmp_writeChannelsNames = zapitCfg.writeChannelsNames;
	mc = new CMenuOptionChooser(LOCALE_SCANTS_BOUQUET_WRITENAMES, (int *)&zapitCfg.writeChannelsNames, SCANTS_BOUQUET_WRITENAMES, SCANTS_BOUQUET_WRITENAMES_COUNT, true, NULL, CRCInput::convertDigitToKey(shortcut++), "", true);
	mc->setHint("", LOCALE_MENU_HINT_SCAN_BOUQUET_WRITENAMES);
	settings->addItem(mc);

	int res = settings->exec(NULL, "");
	//set write_names if changed
	if(zapitCfg.writeChannelsNames != tmp_writeChannelsNames){
		CZapit::getInstance()->SetConfig(&zapitCfg);
		g_Zapit->saveBouquets();
	}

	delete satOnOff;
	delete settings;
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

	fe_restart = false;
	allow_start = !CRecordManager::getInstance()->RecordingStatus() || CRecordManager::getInstance()->TimeshiftOnly();

	CZapit::getInstance()->GetConfig(zapitCfg);
	CMenuWidget * setupMenu = new CMenuWidget(LOCALE_SATSETUP_FE_SETUP, NEUTRINO_ICON_SETTINGS, width, MN_WIDGET_ID_SCAN_FE_SETUP);
	setupMenu->addIntroItems();
	setupMenu->setWizardMode(is_wizard);

	int count = CFEManager::getInstance()->getFrontendCount();

	for(int i = 0; i < count; i++) {
		CFrontend * fe = CFEManager::getInstance()->getFE(i);

		char tmp[32];
		snprintf(tmp, sizeof(tmp), "config_frontend%d", i);

		char name[255];
		snprintf(name, sizeof(name), "%s %d: %s %s", g_Locale->getText(LOCALE_SATSETUP_FE_SETUP), i+1,
				fe->isHybrid() ? g_Locale->getText(LOCALE_SCANTS_ACTHYBRID)
				: fe->hasSat() ? g_Locale->getText(LOCALE_SCANTS_ACTSATELLITE)
				: fe->hasTerr()? g_Locale->getText(LOCALE_SCANTS_ACTTERRESTRIAL)
				: g_Locale->getText(LOCALE_SCANTS_ACTCABLE),
				fe->getName());

		neutrino_msg_t key = CRCInput::RC_nokey;
		if (i == 0)
			key = CRCInput::RC_red;
		else if (i == 1)
			key = CRCInput::RC_green;
		else if (i == 2)
			key = CRCInput::RC_yellow;
		else if (i == 3)
			key = CRCInput::RC_blue;

		modestr[i] = g_Locale->getText(getModeLocale(fe->getMode()));

		mf = new CMenuForwarder(name, allow_start, modestr[i], this, tmp, key);
		mf->setHint("", LOCALE_MENU_HINT_SCAN_SETUP_FE);
		setupMenu->addItem(mf);
		if(i != 0)
			frontendSetup = mf;
	}
	CMenuOptionNumberChooser * nc = new CMenuOptionNumberChooser(LOCALE_EXTRA_ZAPIT_FE_TIMEOUT, (int *)&zapitCfg.feTimeout, true, 6, 100);
	nc->setNumberFormat(std::string("%d00 ") + g_Locale->getText(LOCALE_UNIT_SHORT_MILLISECOND));
	nc->setHint("", LOCALE_MENU_HINT_SCAN_FETIMEOUT);
	setupMenu->addItem(nc);

	std::string zapit_lat_str;
	std::string zapit_long_str;

	if (CFEManager::getInstance()->haveSat()) {
		sprintf(zapit_lat, "%02.6f", zapitCfg.gotoXXLatitude);
		sprintf(zapit_long, "%02.6f", zapitCfg.gotoXXLongitude);
		zapit_lat_str = std::string(zapit_lat);
		zapit_long_str = std::string(zapit_long);

		CMenuWidget * rotorMenu = new CMenuWidget(LOCALE_SATSETUP_EXTENDED_MOTOR, NEUTRINO_ICON_SETTINGS, width);
		rotorMenu->addIntroItems();

		int shortcut = 1;
		CMenuOptionChooser * mc = new CMenuOptionChooser(LOCALE_EXTRA_LADIRECTION,  (int *)&zapitCfg.gotoXXLaDirection, OPTIONS_SOUTH0_NORTH1_OPTIONS, OPTIONS_SOUTH0_NORTH1_OPTION_COUNT, true, NULL, CRCInput::convertDigitToKey(shortcut++));
		mc->setHint("", LOCALE_MENU_HINT_SCAN_LADIRECTION);
		rotorMenu->addItem(mc);

		CStringInput * toff1 = new CStringInput(LOCALE_EXTRA_LATITUDE, &zapit_lat_str, 10, NONEXISTANT_LOCALE, NONEXISTANT_LOCALE, "0123456789.");
		mf = new CMenuDForwarder(LOCALE_EXTRA_LATITUDE, true, zapit_lat_str, toff1, "", CRCInput::convertDigitToKey(shortcut++));
		mf->setHint("", LOCALE_MENU_HINT_SCAN_LATITUDE);
		rotorMenu->addItem(mf);

		mc = new CMenuOptionChooser(LOCALE_EXTRA_LODIRECTION,  (int *)&zapitCfg.gotoXXLoDirection, OPTIONS_EAST0_WEST1_OPTIONS, OPTIONS_EAST0_WEST1_OPTION_COUNT, true, NULL, CRCInput::convertDigitToKey(shortcut++));
		mc->setHint("", LOCALE_MENU_HINT_SCAN_LODIRECTION);
		rotorMenu->addItem(mc);

		CStringInput * toff2 = new CStringInput(LOCALE_EXTRA_LONGITUDE, &zapit_long_str, 10, NONEXISTANT_LOCALE, NONEXISTANT_LOCALE, "0123456789.");
		mf = new CMenuDForwarder(LOCALE_EXTRA_LONGITUDE, true, zapit_long_str, toff2, "", CRCInput::convertDigitToKey(shortcut++));
		mf->setHint("", LOCALE_MENU_HINT_SCAN_LONGITUDE);
		rotorMenu->addItem(mf);

		nc = new CMenuOptionNumberChooser(LOCALE_SATSETUP_USALS_REPEAT, (int *)&zapitCfg.repeatUsals, true, 0, 10, NULL, CRCInput::convertDigitToKey(shortcut++), NULL, 0, 0, LOCALE_OPTIONS_OFF);
		nc->setHint("", LOCALE_MENU_HINT_SCAN_USALS_REPEAT);
		rotorMenu->addItem(nc);

		mf = new CMenuDForwarder(LOCALE_SATSETUP_EXTENDED_MOTOR, true, NULL, rotorMenu);
		mf->setHint("", LOCALE_MENU_HINT_SCAN_USALS);
		setupMenu->addItem(mf);
	}

	int res = setupMenu->exec(NULL, "");

	strncpy(zapit_lat, zapit_lat_str.c_str(), sizeof(zapit_lat));
	strncpy(zapit_long, zapit_long_str.c_str(), sizeof(zapit_long));

	delete setupMenu;
	if (fe_restart) {
		fe_restart = false;
		CFEManager::getInstance()->linkFrontends(true);
		t_channel_id live_channel_id = CZapit::getInstance()->GetCurrentChannelID();
		if (live_channel_id && CNeutrinoApp::getInstance()->channelList)
			CNeutrinoApp::getInstance()->channelList->zapTo_ChannelID(live_channel_id, true);
	}
	saveScanSetup();
	return res;
}

static std::string rotationSpeed2str(int i)
{
	return to_string(i/10) + g_Locale->getText(LOCALE_UNIT_DECIMAL) + to_string(i%10) + "°/" + g_Locale->getText(LOCALE_UNIT_SHORT_SECOND);
}

static struct CMenuOptionChooser::keyval_ext twin_doptions[2];
void CScanSetup::setDiseqcOptions(int number)
{
	CFrontend * fe = CFEManager::getInstance()->getFE(number);

	int mode = fe->getMode();
	if (CFrontend::linked(femode)) {
		printf("CScanSetup::setDiseqcOptions: set options for linked\n");
		CFrontend * mfe = CFEManager::getInstance()->getFE(femaster);
		frontend_config_t & mfe_config = mfe->getConfig();

		int count = 1;
		twin_doptions[0].key = mfe_config.diseqcType;
		twin_doptions[0].value = (mfe_config.diseqcType == NO_DISEQC ? LOCALE_SATSETUP_NODISEQC :
			mfe_config.diseqcType == MINI_DISEQC ? LOCALE_SATSETUP_MINIDISEQC :
			mfe_config.diseqcType == DISEQC_1_0 ? LOCALE_SATSETUP_DISEQC10 :
			mfe_config.diseqcType == DISEQC_1_1 ? LOCALE_SATSETUP_DISEQC11 :
			mfe_config.diseqcType == DISEQC_ADVANCED ? LOCALE_SATSETUP_DISEQC_ADVANCED :
			mfe_config.diseqcType == DISEQC_UNICABLE ? LOCALE_SATSETUP_UNICABLE :
			LOCALE_SATSETUP_UNICABLE2);

		if (mode == CFrontend::FE_MODE_LINK_TWIN && mfe_config.diseqcType != DISEQC_UNICABLE) {
			count++;
			twin_doptions[1].key = DISEQC_UNICABLE;
			twin_doptions[1].value = LOCALE_SATSETUP_UNICABLE;
		}
		dtype->setOptions(twin_doptions, count);
	} else if( mode != CFrontend::FE_MODE_UNUSED) {
		printf("CScanSetup::setDiseqcOptions: set default options\n");
		dtype->setOptions(SATSETUP_DISEQC_OPTIONS, SATSETUP_DISEQC_OPTION_COUNT);
	}
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
	snprintf(name, sizeof(name), "%s %d: %s", g_Locale->getText(LOCALE_SATSETUP_FE_SETUP), number+1, fe->getName());

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
	if (fe->hasSat()) {
		/* enable master option, check if we have masters to enable link options */
		mode_count = 3;
		for (int i = 0; i < fecount; i++) {
			CFrontend * testfe = CFEManager::getInstance()->getFE(i);
			if (i != fenumber && (fe->getSupportedDeliverySystems() == testfe->getSupportedDeliverySystems()) && (testfe->getMode() == CFrontend::FE_MODE_MASTER)) {
				int num = testfe->getNumber();
				snprintf(fename[select_count], sizeof(fename[select_count]), "%d: %s", num+1, testfe->getName());
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
			enable, this, CRCInput::RC_red, NULL, true);
	mc->setHint("", LOCALE_MENU_HINT_SCAN_FEMODE);
	setupMenu->addItem(mc);

	msettings.Clear();

	if (fe->hasCable() && fe->hasTerr()) {
		mc = new CMenuOptionChooser(LOCALE_TUNERSETUP_MODE,  (int *)&fe_config.force_mode, FRONTEND_FORCE_MODE, FRONTEND_FORCE_MODE_COUNT, true, this);
		mc->setHint("", LOCALE_MENU_HINT_SCAN_FEMODE);
		setupMenu->addItem(mc);
	}

	if (fe->hasTerr()) {
		tsp = new CMenuOptionChooser(LOCALE_TUNERSETUP_POWER,  (int *)&fe_config.powered, OPTIONS_OFF0_ON1_OPTIONS, OPTIONS_OFF0_ON1_OPTION_COUNT, fe_config.force_mode != 1, this);
		tsp->setHint("", LOCALE_MENU_HINT_SCAN_FEMODE);
		setupMenu->addItem(tsp);
	}

	if (fe->hasSat()) {
		/* disable all but mode option for linked frontends */
		bool allow_moptions = !CFrontend::linked(femode) && femode != CFrontend::FE_MODE_UNUSED;

		if (fecount > 1) {
			/* link to master select */
			linkfe = new CMenuOptionChooser(LOCALE_SATSETUP_FE_MODE_MASTER, &femaster, feselect, select_count, CFrontend::linked(femode), this, CRCInput::RC_green, NULL, true);
			linkfe->setHint("", LOCALE_MENU_HINT_SCAN_FELINK);
			setupMenu->addItem(linkfe);
		}
		/* diseqc type select */
		dtype = new CMenuOptionChooser(LOCALE_SATSETUP_DISEQC, (int *)&dmode, SATSETUP_DISEQC_OPTIONS, SATSETUP_DISEQC_OPTION_COUNT,
				femode != CFrontend::FE_MODE_UNUSED && femode != CFrontend::FE_MODE_LINK_LOOP,
				this, CRCInput::convertDigitToKey(shortcut++), "", true);
		dtype->setHint("", LOCALE_MENU_HINT_SCAN_DISEQCTYPE);
		setupMenu->addItem(dtype);

		if (fecount > 1)
			setDiseqcOptions(fenumber);

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

		fsatSelect = new CMenuDForwarder(LOCALE_SATSETUP_SELECT_SAT, allow_moptions, NULL, satToSelect, "", CRCInput::convertDigitToKey(shortcut++));
		setOptionSatSelect(fenumber, fsatSelect);
		fsatSelect->setHint("", LOCALE_MENU_HINT_SCAN_SATADD);
		setupMenu->addItem(fsatSelect);
		if (satToSelect->OnAfterHide.empty())
			satToSelect->OnAfterHide.connect(sigc::bind(sigc::mem_fun(this, &CScanSetup::setOptionSatSelect), fenumber, fsatSelect));

		fsatSetup 	= new CMenuForwarder(LOCALE_SATSETUP_SAT_SETUP, allow_moptions, NULL, this, "satsetup", CRCInput::convertDigitToKey(shortcut++));
		fsatSetup->setHint("", LOCALE_MENU_HINT_SCAN_SATSETUP);
		setupMenu->addItem(fsatSetup);

		uniSetup	= new CMenuForwarder(LOCALE_SATSETUP_UNI_SETTINGS, (dmode == DISEQC_UNICABLE ? true : dmode == DISEQC_UNICABLE2), NULL, this, "unisetup", CRCInput::convertDigitToKey(shortcut++));
		setupMenu->addItem(uniSetup);

		CMenuWidget * rotorMenu = new CMenuWidget(LOCALE_SATSETUP_EXTENDED_MOTOR, NEUTRINO_ICON_SETTINGS, width);
		rotorMenu->addIntroItems();

		CMenuOptionNumberChooser * nc = new CMenuOptionNumberChooser(LOCALE_EXTRA_ZAPIT_MOTOR_SPEED, (int *)&fe_config.motorRotationSpeed, allow_moptions, 0, 64, NULL);
		nc->setNumberFormat(rotationSpeed2str);
		nc->setHint("", LOCALE_MENU_HINT_SCAN_MOTOR_SPEED);
		rotorMenu->addItem(nc);
		msettings.Add(nc);

		mc = new CMenuOptionChooser(LOCALE_EXTRA_ZAPIT_HVOLTAGE,  (int *)&fe_config.highVoltage, OPTIONS_OFF0_ON1_OPTIONS, OPTIONS_OFF0_ON1_OPTION_COUNT, allow_moptions);
		mc->setHint("", LOCALE_MENU_HINT_SCAN_MOTOR_18V);
		rotorMenu->addItem(mc);
		msettings.Add(mc);

		mc = new CMenuOptionChooser(LOCALE_SATSETUP_USE_USALS,  &fe_config.use_usals, OPTIONS_OFF0_ON1_OPTIONS, OPTIONS_OFF0_ON1_OPTION_COUNT, allow_moptions, this);
		mc->setHint("", LOCALE_MENU_HINT_SCAN_USALSALL);
		rotorMenu->addItem(mc);
		msettings.Add(mc);

		mc = new CMenuOptionChooser(LOCALE_EXTRA_ROTOR_SWAP,  &fe_config.rotor_swap, OPTIONS_OFF0_ON1_OPTIONS, OPTIONS_OFF0_ON1_OPTION_COUNT, allow_moptions, this);
		mc->setHint("", LOCALE_MENU_HINT_ROTOR_SWAP);
		rotorMenu->addItem(mc);
		msettings.Add(mc);

		CMenuForwarder * mf = new CMenuForwarder(LOCALE_MOTORCONTROL_HEAD, allow_moptions, NULL, this, "satfind", CRCInput::RC_blue);
		mf->setHint("", LOCALE_MENU_HINT_SCAN_SATFIND);
		rotorMenu->addItem(mf);
		msettings.Add(mf);

		mf = new CMenuDForwarder(LOCALE_SATSETUP_EXTENDED_MOTOR, allow_moptions, NULL, rotorMenu, "", CRCInput::RC_blue);
		mf->setHint("", LOCALE_MENU_HINT_SCAN_MOTOR);
		setupMenu->addItem(mf);
	}

	int res = setupMenu->exec(NULL, "");
	feselected = setupMenu->getSelected();
	msettings.Clear();

#if 0
	/* add configured satellites to satSelect in case they changed */
	if (fe->isSat())
		fillSatSelect(satSelect);
#endif

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
	int unicable_pin = fe_config.uni_pin;

	CMenuOptionNumberChooser *uniscr = new CMenuOptionNumberChooser(LOCALE_UNICABLE_SCR, &unicable_scr, true, 0, dmode == DISEQC_UNICABLE ? 7 : 31);
	CIntInput		 *uniqrg = new CIntInput(LOCALE_UNICABLE_QRG, &unicable_qrg, 4, NONEXISTANT_LOCALE, NONEXISTANT_LOCALE);
	CMenuOptionNumberChooser *unipin = new CMenuOptionNumberChooser(LOCALE_UNICABLE_PIN, &unicable_pin, true, 0, 255, NULL, CRCInput::RC_nokey, NULL, 0, 0, LOCALE_OPTIONS_OFF);
	unipin->setNumericInput(true);
	unipin->setHint("", LOCALE_UNICABLE_PIN_HINT);

	CMenuWidget *uni_setup = new CMenuWidget(LOCALE_SATSETUP_UNI_SETTINGS, NEUTRINO_ICON_SETTINGS, width);
	uni_setup->addIntroItems();

	uni_setup->addItem(uniscr);
	CMenuForwarder *mf = new CMenuDForwarder(LOCALE_UNICABLE_QRG, true, uniqrg->getValue(), uniqrg);
	uni_setup->addItem(mf);
	uni_setup->addItem(unipin);
	res = uni_setup->exec(NULL, "");
	delete uni_setup;
	if (res) {
		fe_config.uni_scr = unicable_scr;
		fe_config.uni_qrg = unicable_qrg;
		fe_config.uni_pin = unicable_pin;
		printf("%s: scr: %d qrg: %d pin: %d\n", __func__, unicable_scr, unicable_qrg, unicable_pin);
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
	snprintf(name, sizeof(name), "%s %d: %s", g_Locale->getText(LOCALE_SATSETUP_SATELLITE), fenumber+1, fe->getName());

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

		char opt[100];
		sprintf(opt, "diseqc %2d / rotor %2d", sit->second.diseqc+1, sit->second.motor_position);
		satoptions.push_back(opt);
		CMenuForwarder * mf = new CMenuForwarder(satname.c_str(), true, satoptions[count].c_str(), tempsat);
		mf->setHint("", LOCALE_MENU_HINT_SCAN_LNBCONFIG);
		sat_setup->addItem(mf);
		satmf.push_back(mf);
		tmp[count] = tempsat;
		count++;
	}
	if(count) {
		res = sat_setup->exec(NULL, "");
		for (int i = 0; i < count; i++)
			delete tmp[i];
	}
	delete sat_setup;
	satmf.clear();
	satoptions.clear();
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

		if (!fe->hasSat() || (fe->getMode() == CFrontend::FE_MODE_UNUSED))
			continue;

		satellite_map_t & satmap = fe->getSatellites();
		for (sat_iterator_t sit = satmap.begin(); sit != satmap.end(); ++sit) {
			tmpit = satpos.find(sit->first);
			if(sit->second.configured && tmpit == satpos.end()) {
				std::string satname = CServiceManager::getInstance()->GetSatelliteName(sit->first);
				select->addOption(satname.c_str());
				satpos.insert(sit->first);

				if (!sfound && (scansettings.satName == satname))
					sfound = true;
			}
		}
	}
	if(!sfound && !satpos.empty()) {
		tmpit = satpos.begin();
		std::string satname = CServiceManager::getInstance()->GetSatelliteName(*tmpit);
		scansettings.satName = satname;
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
	const char *what = (r_system & ALL_CABLE) ? "cable" : "terrestrial";
	printf("[neutrino] CScanSetup call %s (%s)...\n", __func__, what);
	//don't misunderstand the name "satSelect", in this context it's actually for cable providers
	satellite_map_t satmap = CServiceManager::getInstance()->SatelliteList();
	bool sfound = false;
	std::string fname;
	for (sat_iterator_t sit = satmap.begin(); sit != satmap.end(); sit++)
	{
		if (!(r_system & sit->second.delsys))
			continue;

		printf("Adding %s menu for %s position %d\n", what, sit->second.name.c_str(), sit->first);
		select->addOption(sit->second.name.c_str());

		if (fname.empty())
			fname = sit->second.name;

		if (!sfound && (scansettings.cableName == sit->second.name))
			sfound = true;

		dprintf(DEBUG_DEBUG, "got scanprovider (%s): %s\n", what, sit->second.name.c_str());
	}

	if (!sfound && !fname.empty()) {
		if (r_system & ALL_CABLE)
			scansettings.cableName = fname;
	}
}

//init terrestrial provider menu
void CScanSetup::fillTerrSelect(CMenuOptionStringChooser * select)
{
	const char *what = (r_system & ALL_TERR) ? "terrestrial" : "cable";
	printf("[neutrino] CScanSetup call %s (%s)...\n", __func__, what);
	//don't misunderstand the name "satSelect", in this context it's actually for cable providers
	satellite_map_t satmap = CServiceManager::getInstance()->SatelliteList();
	bool sfound = false;
	std::string fname;
	for (sat_iterator_t sit = satmap.begin(); sit != satmap.end(); sit++)
	{
		if (!(r_system & sit->second.delsys))
			continue;

		printf("Adding %s menu for %s position %d\n", what, sit->second.name.c_str(), sit->first);
		select->addOption(sit->second.name.c_str());

		if (fname.empty())
			fname = sit->second.name;

		if (!sfound && (scansettings.terrestrialName == sit->second.name))
			sfound = true;

		dprintf(DEBUG_DEBUG, "got scanprovider (%s): %s\n", what, sit->second.name.c_str());
	}

	if (!sfound && !fname.empty()) {
		if (r_system & ALL_TERR)
			scansettings.terrestrialName = fname;
	}
}

int CScanSetup::showScanMenuSatFind()
{
	printf("[neutrino] CScanSetup call %s fe %d\n", __FUNCTION__, fenumber);
	int count = 0;
	CFrontend * fe = CFEManager::getInstance()->getFE(fenumber);
	char name[255];
	static int selected = 0;

	r_system = ALL_SAT;

	snprintf(name, sizeof(name), "%s %d: %s", g_Locale->getText(LOCALE_MOTORCONTROL_HEAD), fenumber+1, fe->getName());

	CMenuWidget* sat_findMenu = new CMenuWidget(name /*LOCALE_MOTORCONTROL_HEAD*/, NEUTRINO_ICON_SETTINGS, width);
	sat_findMenu->setSelected(selected);
	sat_findMenu->addIntroItems();

	CMenuOptionStringChooser * feSatSelect = new CMenuOptionStringChooser(LOCALE_SATSETUP_SATELLITE, &scansettings.satName, true, NULL, CRCInput::RC_red, NULL, true);
	feSatSelect->setHint("", LOCALE_MENU_HINT_SCAN_SATSELECT);

	satellite_map_t & satmap = fe->getSatellites();
	bool sfound = false;
	std::string firstname;
	for (sat_iterator_t sit = satmap.begin(); sit != satmap.end(); ++sit) {
		if(!sit->second.configured)
			continue;
		std::string satname = CServiceManager::getInstance()->GetSatelliteName(sit->first);
		feSatSelect->addOption(satname.c_str());
		if (!sfound && (scansettings.satName == satname))
			sfound = true;
		if (!sfound && firstname.empty())
			firstname = satname;
		count++;
	}
	if(count && !sfound)
		scansettings.satName = firstname;

	sat_findMenu->addItem(feSatSelect);

	CTPSelectHandler tpSelect;
	CMenuForwarder * mf = new CMenuForwarder(LOCALE_SCANTS_SELECT_TP, true, NULL, &tpSelect, "sat", CRCInput::RC_green);
	mf->setHint("", LOCALE_MENU_HINT_SCAN_TPSELECT);
	sat_findMenu->addItem(mf);
	sat_findMenu->addItem(GenericMenuSeparatorLine);
	//--------------------------------------------------------------
	addScanOptionsItems(sat_findMenu);
	//--------------------------------------------------------------
	sat_findMenu->addItem(GenericMenuSeparatorLine);
	CMotorControl mcontrol(fenumber);
	mf = new CMenuForwarder(LOCALE_MOTORCONTROL_HEAD, allow_start, NULL, &mcontrol, "", CRCInput::RC_yellow);
	mf->setHint("", LOCALE_MENU_HINT_SCAN_SATFIND_START);
	sat_findMenu->addItem(mf);

	int res = sat_findMenu->exec(NULL, "");
	selected = sat_findMenu->getSelected();
	delete sat_findMenu;
	return res;
}

//init tempsat menu
void CScanSetup::addScanMenuTempSat(CMenuWidget *temp_sat, sat_config_t & satconfig)
{
	temp_sat->addIntroItems();

	CMenuOptionNumberChooser	*diseqc = NULL;
	CMenuOptionNumberChooser	*comm   = NULL;
	CMenuOptionNumberChooser	*uncomm = NULL;
	CMenuOptionNumberChooser	*unilnb = NULL;
	CMenuOptionNumberChooser	*motor  = NULL;
	CMenuOptionChooser		*usals  = NULL;
	CMenuForwarder			*mf;
	bool unicable = (dmode == DISEQC_UNICABLE ? true : dmode == DISEQC_UNICABLE2);

	if (!unicable) {
		diseqc	= new CMenuOptionNumberChooser(LOCALE_SATSETUP_DISEQC_INPUT, &satconfig.diseqc, ((dmode != NO_DISEQC) && (dmode != DISEQC_ADVANCED)), -1, 15, this, CRCInput::RC_nokey, NULL, 1, -1, LOCALE_OPTIONS_OFF);
		diseqc->setHint("", LOCALE_MENU_HINT_SCAN_DISEQC);
		comm 	= new CMenuOptionNumberChooser(LOCALE_SATSETUP_COMM_INPUT, &satconfig.commited, dmode == DISEQC_ADVANCED, -1, 15, NULL, CRCInput::RC_nokey, NULL, 1, -1, LOCALE_OPTIONS_OFF);
		comm->setHint("", LOCALE_MENU_HINT_SCAN_COMMITED);
		uncomm = new CMenuOptionNumberChooser(LOCALE_SATSETUP_UNCOMM_INPUT, &satconfig.uncommited, dmode == DISEQC_ADVANCED, -1, 15, NULL, CRCInput::RC_nokey, NULL, 1, -1, LOCALE_OPTIONS_OFF);
		uncomm->setHint("", LOCALE_MENU_HINT_SCAN_UNCOMMITED);
		motor 	= new CMenuOptionNumberChooser(LOCALE_SATSETUP_MOTOR_POS, &satconfig.motor_position, true /*dmode == DISEQC_ADVANCED*/, 0, 64, this, CRCInput::RC_nokey, NULL, 0, 0, LOCALE_OPTIONS_OFF);
		motor->setHint("", LOCALE_MENU_HINT_SCAN_MOTORPOS);
		usals	= new CMenuOptionChooser(LOCALE_EXTRA_USE_GOTOXX,  &satconfig.use_usals, OPTIONS_OFF0_ON1_OPTIONS, OPTIONS_OFF0_ON1_OPTION_COUNT, true /*dmode == DISEQC_ADVANCED*/);
		usals->setHint("", LOCALE_MENU_HINT_SCAN_USEUSALS);
	} else {
		if (satconfig.diseqc < 0)
			satconfig.diseqc = 0;
		unilnb = new CMenuOptionNumberChooser(LOCALE_UNICABLE_LNB, &satconfig.diseqc, true, 0, dmode == DISEQC_UNICABLE ? 1 : 3);
	}

	CIntInput* lofL = new CIntInput(LOCALE_SATSETUP_LOFL, (int*) &satconfig.lnbOffsetLow, 5, NONEXISTANT_LOCALE, NONEXISTANT_LOCALE);
	CIntInput* lofH = new CIntInput(LOCALE_SATSETUP_LOFH, (int*) &satconfig.lnbOffsetHigh, 5, NONEXISTANT_LOCALE, NONEXISTANT_LOCALE);
	CIntInput* lofS = new CIntInput(LOCALE_SATSETUP_LOFS, (int*) &satconfig.lnbSwitch, 5, NONEXISTANT_LOCALE, NONEXISTANT_LOCALE);

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
void CScanSetup::addScanMenuManualScan(CMenuWidget *manual_Scan, bool stest)
{
	printf("[neutrino] CScanSetup call %s...\n", __FUNCTION__);
	int shortCut = 1;
	CMenuForwarder * mf;

	manual_Scan->addIntroItems();
	const char *act_test, *act_manual;
	//----------------------------------------------------------------------
	if (r_system == ALL_CABLE)  {
		act_test = "ctest"; act_manual = "cmanual";
		CMenuOptionStringChooser * cableSelect = new CMenuOptionStringChooser(LOCALE_CABLESETUP_PROVIDER, &scansettings.cableName, true, this, CRCInput::RC_red, NULL, true);
		cableSelect->setHint("", LOCALE_MENU_HINT_SCAN_CABLE);
		fillCableSelect(cableSelect);
		manual_Scan->addItem(cableSelect);
		mf = new CMenuForwarder(LOCALE_SATSETUP_CABLE_NID, true, nid->getValue(), nid);
		mf->setHint("", LOCALE_MENU_HINT_SCAN_NID);
		manual_Scan->addItem(mf);
		mf = new CMenuDForwarder(LOCALE_SCANTS_SELECT_TP, true, NULL, new CTPSelectHandler(), "cable", CRCInput::RC_blue);
	} else if (r_system == ALL_TERR) {
		act_test = "ttest"; act_manual = "tmanual";
		CMenuOptionStringChooser * terrSelect = new CMenuOptionStringChooser(LOCALE_TERRESTRIALSETUP_PROVIDER, &scansettings.terrestrialName, true, this, CRCInput::RC_red, NULL, true);
		//terrSelect->setHint("", LOCALE_MENU_HINT_SCAN_CABLE);
		fillTerrSelect(terrSelect);
		manual_Scan->addItem(terrSelect);
		mf = new CMenuDForwarder(LOCALE_SCANTS_SELECT_TP, true, NULL, new CTPSelectHandler(), "terrestrial", CRCInput::RC_blue);
	} else if (r_system == ALL_SAT) {
		act_test = "stest"; act_manual = "smanual";
		CMenuOptionStringChooser * satSelect = new CMenuOptionStringChooser(LOCALE_SATSETUP_SATELLITE, &scansettings.satName, true, this, CRCInput::RC_red, NULL, true);
		satSelect->setHint("", LOCALE_MENU_HINT_SCAN_SATELLITE);
		/* add configured satellites to satSelect */
		fillSatSelect(satSelect);
		manual_Scan->addItem(satSelect);
		mf = new CMenuDForwarder(LOCALE_SCANTS_SELECT_TP, true, NULL, new CTPSelectHandler(), "sat", CRCInput::RC_blue);
	} else
		return;

	mf->setHint("", LOCALE_MENU_HINT_SCAN_TPSELECT);
	manual_Scan->addItem(mf);

	manual_Scan->addItem(GenericMenuSeparatorLine);
	//----------------------------------------------------------------------
	shortCut = addScanOptionsItems(manual_Scan, shortCut);
	addListFlagsItems(manual_Scan, shortCut, true);
	//----------------------------------------------------------------------
	manual_Scan->addItem(GenericMenuSeparatorLine);

	mf = new CMenuForwarder(LOCALE_SCANTS_TEST, allow_start, NULL, this, act_test, CRCInput::RC_yellow);
	mf->setHint("", LOCALE_MENU_HINT_SCAN_TEST);
	manual_Scan->addItem(mf);

	if (!stest) {
		mf = new CMenuForwarder(LOCALE_SCANTS_STARTNOW, allow_start, NULL, this, act_manual, CRCInput::RC_green);
		mf->setHint("", LOCALE_MENU_HINT_SCAN_START);
		manual_Scan->addItem(mf);
	}
}

//init auto scan all menu
void CScanSetup::addScanMenuAutoScanAll(CMenuWidget *auto_ScanAll)
{
	printf("[neutrino] CScanSetup call %s...\n", __FUNCTION__);
	auto_ScanAll->addIntroItems();
	//----------------------------------------------------------------------
	CMenuForwarder * mf = new CMenuForwarder(LOCALE_SATSETUP_SATELLITE, true, NULL, satOnOff, "", CRCInput::RC_red);
	mf->setHint("", LOCALE_MENU_HINT_SCAN_AUTOALL_SELECT);
	auto_ScanAll->addItem(mf);

	auto_ScanAll->addItem(GenericMenuSeparatorLine);
	//----------------------------------------------------------------------
	addListFlagsItems(auto_ScanAll, 1);
	//----------------------------------------------------------------------
	auto_ScanAll->addItem(GenericMenuSeparatorLine);
	mf = new CMenuForwarder(LOCALE_SCANTS_STARTNOW, allow_start, NULL, this, "sall", CRCInput::RC_green);
	mf->setHint("", LOCALE_MENU_HINT_SCAN_START);
	auto_ScanAll->addItem(mf);
}

#ifdef ENABLE_FASTSCAN
#if 0
#define FAST_SCAN_OPTIONS_COUNT 2
const CMenuOptionChooser::keyval FAST_SCAN_OPTIONS[FAST_SCAN_OPTIONS_COUNT] =
{
	{ FAST_SCAN_SD, LOCALE_SATSETUP_FASTSCAN_SD },
	{ FAST_SCAN_HD, LOCALE_SATSETUP_FASTSCAN_HD  }
	/*{ FAST_SCAN_ALL, LOCALE_SATSETUP_FASTSCAN_ALL  }*/
};
#endif

const CMenuOptionChooser::keyval FAST_SCAN_PROV_OPTIONS[] =
{
	{ OPERATOR_CD_SD, LOCALE_SATSETUP_FASTSCAN_PROV_CD_SD },
	{ OPERATOR_CD_HD, LOCALE_SATSETUP_FASTSCAN_PROV_CD_HD },
	{ OPERATOR_TVV_SD, LOCALE_SATSETUP_FASTSCAN_PROV_TVV_SD },
	{ OPERATOR_TVV_HD, LOCALE_SATSETUP_FASTSCAN_PROV_TVV_HD },
	{ OPERATOR_TELESAT_B, LOCALE_SATSETUP_FASTSCAN_PROV_TELESAT_B },
	{ OPERATOR_TELESAT_L, LOCALE_SATSETUP_FASTSCAN_PROV_TELESAT_L },
	{ OPERATOR_HD_AUSTRIA, LOCALE_SATSETUP_FASTSCAN_PROV_HDA },
	{ OPERATOR_SKYLINK_C, LOCALE_SATSETUP_FASTSCAN_PROV_SKYLINK_C },
	{ OPERATOR_SKYLINK_S, LOCALE_SATSETUP_FASTSCAN_PROV_SKYLINK_S },
	{ OPERATOR_HELLO, LOCALE_SATSETUP_FASTSCAN_PROV_HELLO }
};
#define FAST_SCAN_PROV_OPTIONS_COUNT (sizeof(FAST_SCAN_PROV_OPTIONS)/sizeof(CMenuOptionChooser::keyval))

//init fast scan menu
void CScanSetup::addScanMenuFastScan(CMenuWidget *fast_ScanMenu)
{
	printf("[neutrino] CScanSetup call %s...\n", __FUNCTION__);
	fast_ScanMenu->addIntroItems();

	CMenuOptionChooser* fastProv = new CMenuOptionChooser(LOCALE_SATSETUP_FASTSCAN_PROV, (int *)&scansettings.fast_op, FAST_SCAN_PROV_OPTIONS, FAST_SCAN_PROV_OPTIONS_COUNT, true, NULL, CRCInput::RC_red, NULL, true);
	fastProv->setHint("", LOCALE_MENU_HINT_SCAN_FASTPROV);
	fast_ScanMenu->addItem(fastProv);
#if 0
	CMenuOptionChooser* fastType = new CMenuOptionChooser(LOCALE_SATSETUP_FASTSCAN_TYPE, (int *)&scansettings.fast_type, FAST_SCAN_OPTIONS, FAST_SCAN_OPTIONS_COUNT, true, NULL, CRCInput::RC_green, NULL, true);
	fastType->setHint("", LOCALE_MENU_HINT_SCAN_FASTTYPE);
	fast_ScanMenu->addItem(fastType);
#endif
	CMenuOptionChooser* fastUp = new CMenuOptionChooser(LOCALE_SATSETUP_FASTSCAN_UPDATE, (int *)&scansettings.fst_update, OPTIONS_OFF0_ON1_OPTIONS, OPTIONS_OFF0_ON1_OPTION_COUNT, true, NULL, CRCInput::RC_green, NULL, true);
	fastUp->setHint("", LOCALE_MENU_HINT_SCAN_FASTUPDATE);
	fast_ScanMenu->addItem(fastUp);
	fast_ScanMenu->addItem(GenericMenuSeparatorLine);

	CMenuForwarder * mf = new CMenuForwarder(LOCALE_SATSETUP_FASTSCAN_AUTO_DISEQC, allow_start, NULL, this, "fastdiseqc", CRCInput::RC_yellow);
	mf->setHint("", LOCALE_MENU_HINT_SCAN_FASTDISEQC);
	fast_ScanMenu->addItem(mf);

	mf = new CMenuForwarder(LOCALE_SCANTS_STARTNOW, allow_start, NULL, this, "sfast", CRCInput::RC_blue);
	mf->setHint("", LOCALE_MENU_HINT_SCAN_START);
	fast_ScanMenu->addItem(mf);
}

int CScanSetup::showFastscanDiseqcSetup()
{
	CHintBox hintbox(LOCALE_MESSAGEBOX_INFO, g_Locale->getText(LOCALE_SATSETUP_FASTSCAN_AUTO_DISEQC_WAIT));
	hintbox.paint();

	CServiceScan::getInstance()->TestDiseqcConfig(scansettings.fast_op);
	hintbox.hide();

	CMenuWidget * sat_setup = new CMenuWidget(LOCALE_SATSETUP_DISEQC_INPUT, NEUTRINO_ICON_SETTINGS, width);
	sat_setup->addIntroItems();

	CFrontend * fe = CFEManager::getInstance()->getFE(0);
	satellite_map_t & satmap = fe->getSatellites();
	INFO("satmap size = %d", (int)satmap.size());
	for (sat_iterator_t sit = satmap.begin(); sit != satmap.end(); ++sit)
	{
		if(!sit->second.configured)
			continue;

		std::string satname = CServiceManager::getInstance()->GetSatelliteName(sit->first);
		CMenuOptionNumberChooser *diseqc = new CMenuOptionNumberChooser(satname, &sit->second.diseqc, true, -1, 15, this, CRCInput::RC_nokey, NULL, 1, -1, LOCALE_OPTIONS_OFF);
		sat_setup->addItem(diseqc);
	}
	sat_setup->addItem(GenericMenuSeparatorLine);
	CMenuForwarder * mf = new CMenuForwarder(LOCALE_SCANTS_STARTNOW, allow_start, NULL, this, "sfast", CRCInput::RC_yellow);
	mf->setHint("", LOCALE_MENU_HINT_SCAN_START);
	sat_setup->addItem(mf);

	int res = sat_setup->exec(NULL, "");
	delete sat_setup;
	return res;
}
#endif /*ENABLE_FASTSCAN*/

//init auto scan menu
void CScanSetup::addScanMenuAutoScan(CMenuWidget *auto_Scan)
{
	printf("[neutrino] CScanSetup call %s...\n", __FUNCTION__);
	CMenuForwarder * mf;
	auto_Scan->addIntroItems();

	const char *action;
	if (r_system == ALL_CABLE)  { //cable
		CMenuOptionStringChooser * cableSelect = new CMenuOptionStringChooser(LOCALE_CABLESETUP_PROVIDER, &scansettings.cableName, true, this, CRCInput::RC_red, NULL, true);
		cableSelect->setHint("", LOCALE_MENU_HINT_SCAN_CABLE);
		fillCableSelect(cableSelect);
		auto_Scan->addItem(cableSelect);
		mf = new CMenuForwarder(LOCALE_SATSETUP_CABLE_NID, true, nid->getValue(), nid);
		mf->setHint("", LOCALE_MENU_HINT_SCAN_NID);
		auto_Scan->addItem(mf);
		action = "cauto";
	} else if (r_system == ALL_TERR) {
		CMenuOptionStringChooser * terrSelect = new CMenuOptionStringChooser(LOCALE_TERRESTRIALSETUP_PROVIDER, &scansettings.terrestrialName, true, this, CRCInput::RC_red, NULL, true);
		//terrSelect->setHint("", LOCALE_MENU_HINT_SCAN_CABLE);
		fillCableSelect(terrSelect);
		auto_Scan->addItem(terrSelect);
		action = "tauto";
	} else if (r_system == ALL_SAT) {
		CMenuOptionStringChooser * satSelect = new CMenuOptionStringChooser(LOCALE_SATSETUP_SATELLITE, &scansettings.satName, true, this, CRCInput::RC_red, NULL, true);
		satSelect->setHint("", LOCALE_MENU_HINT_SCAN_SATELLITE);
		/* add configured satellites to satSelect */
		fillSatSelect(satSelect);
		auto_Scan->addItem(satSelect);
		action = "sauto";
	} else {
		return;
	}

	auto_Scan->addItem(GenericMenuSeparatorLine);
	//----------------------------------------------------------------------
	addListFlagsItems(auto_Scan, 1);
	//----------------------------------------------------------------------
	auto_Scan->addItem(GenericMenuSeparatorLine);
	mf = new CMenuForwarder(LOCALE_SCANTS_STARTNOW, allow_start, NULL, this, action, CRCInput::RC_green);
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
	CMenuOptionStringChooser * select = new CMenuOptionStringChooser(LOCALE_CABLESETUP_PROVIDER, &scansettings.cableName, true, this, CRCInput::RC_red, NULL, true);
	fillCableSelect(select);
	select->setHint("", LOCALE_MENU_HINT_SCAN_CABLE);
	menu->addItem(select);

	mf = new CMenuForwarder(LOCALE_SATSETUP_CABLE_NID, true, nid->getValue(), nid);
	mf->setHint("", LOCALE_MENU_HINT_SCAN_NID);
	menu->addItem(mf);

	mf = new CMenuDForwarder(LOCALE_SCANTS_SELECT_TP, true, NULL, new CTPSelectHandler(), "cable", CRCInput::RC_blue);
	mf->setHint("", LOCALE_MENU_HINT_SCAN_TPSELECT);
	menu->addItem(mf);

	menu->addItem(GenericMenuSeparatorLine);

	CMenuOptionChooser *delsys = new CMenuOptionChooser(LOCALE_EXTRA_TP_DELSYS, (int *)&scansettings.cable_TP_delsys, CABLESETUP_SCANTP_DELSYS, CABLESETUP_SCANTP_DELSYS_COUNT, true, NULL, CRCInput::convertDigitToKey(shortCut++), "", true);
	delsys->setHint("", LOCALE_MENU_HINT_SCAN_DELSYS);

	CStringInput		*freq	= new CStringInput(LOCALE_EXTRA_TP_FREQ, &scansettings.cable_TP_freq, 6, NONEXISTANT_LOCALE, NONEXISTANT_LOCALE, "0123456789");
	CMenuForwarder		*Freq 	= new CMenuDForwarder(LOCALE_EXTRA_TP_FREQ, true, scansettings.cable_TP_freq, freq, "", CRCInput::convertDigitToKey(shortCut++));
	Freq->setHint("", LOCALE_MENU_HINT_SCAN_FREQ);

	CStringInput		*rate 	= new CStringInput(LOCALE_EXTRA_TP_RATE, &scansettings.cable_TP_rate, 8, NONEXISTANT_LOCALE, NONEXISTANT_LOCALE, "0123456789");
	CMenuForwarder		*Rate 	= new CMenuDForwarder(LOCALE_EXTRA_TP_RATE, true, scansettings.cable_TP_rate, rate, "", CRCInput::convertDigitToKey(shortCut++));
	Rate->setHint("", LOCALE_MENU_HINT_SCAN_RATE);


	CMenuOptionChooser *mod = new CMenuOptionChooser(LOCALE_EXTRA_TP_MOD, (int *)&scansettings.cable_TP_mod, CABLESETUP_SCANTP_MOD, CABLESETUP_SCANTP_MOD_COUNT, true, NULL, CRCInput::convertDigitToKey(shortCut++));
	mod->setHint("", LOCALE_MENU_HINT_SCAN_MOD);

	menu->addItem(delsys);
	menu->addItem(Freq);
	menu->addItem(Rate);
	menu->addItem(mod);

	CMenuOptionChooser *lcn  = new CMenuOptionChooser(LOCALE_SATSETUP_LOGICAL_NUMBERS, (int *)&scansettings.scan_logical_numbers, OPTIONS_OFF0_ON1_OPTIONS, OPTIONS_OFF0_ON1_OPTION_COUNT, g_settings.keep_channel_numbers, this, CRCInput::convertDigitToKey(shortCut++));
	lcn->setHint("", LOCALE_MENU_HINT_SCAN_LOGICAL);
	menu->addItem(lcn);

	lcnhd  = new CMenuOptionChooser(LOCALE_SATSETUP_LOGICAL_HD, (int *)&scansettings.scan_logical_hd, OPTIONS_OFF0_ON1_OPTIONS, OPTIONS_OFF0_ON1_OPTION_COUNT, scansettings.scan_logical_numbers, NULL, CRCInput::convertDigitToKey(shortCut++));
	lcnhd->setHint("", LOCALE_MENU_HINT_SCAN_LOGICAL_HD);
	menu->addItem(lcnhd);

	menu->addItem(GenericMenuSeparatorLine);
#if 0
	mf = new CMenuForwarder(LOCALE_SCANTS_TEST, allow_start, NULL, this, "test", CRCInput::RC_yellow);
	mf->setHint("", LOCALE_MENU_HINT_SCAN_TEST);
	menu->addItem(mf);
#endif
	mf = new CMenuForwarder(LOCALE_SCANTS_STARTNOW, allow_start, NULL, this, "cable", CRCInput::RC_green);
	mf->setHint("", LOCALE_MENU_HINT_SCAN_START);
	menu->addItem(mf);
}

//create scan options items
int CScanSetup::addScanOptionsItems(CMenuWidget *options_menu, const int &shortcut)
{
	printf("[neutrino] CScanSetup call %s...\n", __FUNCTION__);
	int shortCut = shortcut;

	CMenuOptionChooser	*fec = NULL;
	CMenuOptionChooser	*mod = NULL;
	CMenuOptionChooser	*pol = NULL;
	CMenuOptionChooser	*bw = NULL;
	CMenuOptionChooser	*gi = NULL;
	CMenuOptionChooser	*hi = NULL;
	CMenuOptionChooser	*delsys = NULL;
	CMenuOptionChooser	*coderateHP = NULL;
	CMenuOptionChooser	*coderateLP = NULL;
	CMenuOptionChooser	*tm = NULL;
	CMenuForwarder		*Freq = NULL;
	CMenuForwarder		*Rate = NULL;
	CMenuOptionChooser	*pilot = NULL;
	CMenuForwarder		*Pli = NULL;
	CMenuForwarder		*Plc = NULL;
	CMenuOptionChooser	*Plm = NULL;
	if (r_system == ALL_SAT) {
		delsys = new CMenuOptionChooser(LOCALE_EXTRA_TP_DELSYS, (int *)&scansettings.sat_TP_delsys, SATSETUP_SCANTP_DELSYS, SATSETUP_SCANTP_DELSYS_COUNT, true, NULL, CRCInput::convertDigitToKey(shortCut++), "", true);
		delsys->setHint("", LOCALE_MENU_HINT_SCAN_DELSYS);
		CStringInput		*freq	= new CStringInput(LOCALE_EXTRA_TP_FREQ, &scansettings.sat_TP_freq, 8, NONEXISTANT_LOCALE, NONEXISTANT_LOCALE, "0123456789");
		Freq 	= new CMenuDForwarder(LOCALE_EXTRA_TP_FREQ, true, scansettings.sat_TP_freq, freq, "", CRCInput::convertDigitToKey(shortCut++));
		Freq->setHint("", LOCALE_MENU_HINT_SCAN_FREQ);
		CStringInput		*rate 	= new CStringInput(LOCALE_EXTRA_TP_RATE, &scansettings.sat_TP_rate, 8, NONEXISTANT_LOCALE, NONEXISTANT_LOCALE, "0123456789");
		Rate 	= new CMenuDForwarder(LOCALE_EXTRA_TP_RATE, true, scansettings.sat_TP_rate, rate, "", CRCInput::convertDigitToKey(shortCut++));
		Rate->setHint("", LOCALE_MENU_HINT_SCAN_RATE);
		mod = new CMenuOptionChooser(LOCALE_EXTRA_TP_MOD, (int *)&scansettings.sat_TP_mod, SATSETUP_SCANTP_MOD, SATSETUP_SCANTP_MOD_COUNT, true, NULL, CRCInput::convertDigitToKey(shortCut++), "", true);
		mod->setHint("", LOCALE_MENU_HINT_SCAN_MOD);
		fec 	= new CMenuOptionChooser(LOCALE_EXTRA_TP_FEC, (int *)&scansettings.sat_TP_fec, SATSETUP_SCANTP_FEC, SATSETUP_SCANTP_FEC_COUNT, true, NULL, CRCInput::convertDigitToKey(shortCut++), "", true);
		fec->setHint("", LOCALE_MENU_HINT_SCAN_FEC);
		pol = new CMenuOptionChooser(LOCALE_EXTRA_TP_POL, (int *)&scansettings.sat_TP_pol, SATSETUP_SCANTP_POL, SATSETUP_SCANTP_POL_COUNT, true, NULL, CRCInput::convertDigitToKey(shortCut++));
		pol->setHint("", LOCALE_MENU_HINT_SCAN_POL);
		pilot = new CMenuOptionChooser(LOCALE_EXTRA_TP_PILOT, (int *)&scansettings.sat_TP_pilot, SATSETUP_SCANTP_PILOT, SATSETUP_SCANTP_PILOT_COUNT, true, NULL, CRCInput::convertDigitToKey(shortCut++));
		pilot->setHint("", LOCALE_MENU_HINT_SCAN_PILOT);
		CStringInput		*pli 	= new CStringInput(LOCALE_EXTRA_TP_PLI, &scansettings.sat_TP_pli, 3, NONEXISTANT_LOCALE, NONEXISTANT_LOCALE, "0123456789");
		Pli 	= new CMenuDForwarder(LOCALE_EXTRA_TP_PLI, true, scansettings.sat_TP_pli, pli, "", CRCInput::convertDigitToKey(shortCut++));
		CStringInput		*plc 	= new CStringInput(LOCALE_EXTRA_TP_PLC, &scansettings.sat_TP_plc, 6, NONEXISTANT_LOCALE, NONEXISTANT_LOCALE, "0123456789");
		Plc 	= new CMenuDForwarder(LOCALE_EXTRA_TP_PLC, true, scansettings.sat_TP_plc, plc, "", CRCInput::convertDigitToKey(shortCut++));
		Plm 	= new CMenuOptionChooser(LOCALE_EXTRA_TP_PLM, (int *)&scansettings.sat_TP_plm, SATSETUP_SCANTP_PLM, SATSETUP_SCANTP_PLM_COUNT, true, NULL, CRCInput::convertDigitToKey(shortCut++));
	} else if (r_system == ALL_CABLE) {
		delsys = new CMenuOptionChooser(LOCALE_EXTRA_TP_DELSYS, (int *)&scansettings.cable_TP_delsys, CABLESETUP_SCANTP_DELSYS, CABLESETUP_SCANTP_DELSYS_COUNT, true, NULL, CRCInput::convertDigitToKey(shortCut++), "", true);
		delsys->setHint("", LOCALE_MENU_HINT_SCAN_DELSYS);
		CStringInput		*freq	= new CStringInput(LOCALE_EXTRA_TP_FREQ, &scansettings.cable_TP_freq, 6, NONEXISTANT_LOCALE, NONEXISTANT_LOCALE, "0123456789");
		Freq 	= new CMenuDForwarder(LOCALE_EXTRA_TP_FREQ, true, scansettings.cable_TP_freq, freq, "", CRCInput::convertDigitToKey(shortCut++));
		Freq->setHint("", LOCALE_MENU_HINT_SCAN_FREQ);
		CStringInput		*rate 	= new CStringInput(LOCALE_EXTRA_TP_RATE, &scansettings.cable_TP_rate, 8, NONEXISTANT_LOCALE, NONEXISTANT_LOCALE, "0123456789");
		Rate 	= new CMenuDForwarder(LOCALE_EXTRA_TP_RATE, true, scansettings.cable_TP_rate, rate, "", CRCInput::convertDigitToKey(shortCut++));
		Rate->setHint("", LOCALE_MENU_HINT_SCAN_RATE);
		mod = new CMenuOptionChooser(LOCALE_EXTRA_TP_MOD, (int *)&scansettings.cable_TP_mod, CABLESETUP_SCANTP_MOD, CABLESETUP_SCANTP_MOD_COUNT, true, NULL, CRCInput::convertDigitToKey(shortCut++));
		mod->setHint("", LOCALE_MENU_HINT_SCAN_MOD);
	} else if (r_system == ALL_TERR) {
		// TODO: more params ?
		delsys = new CMenuOptionChooser(LOCALE_EXTRA_TP_DELSYS, (int *)&scansettings.terrestrial_TP_delsys, TERRSETUP_SCANTP_DELSYS, TERRSETUP_SCANTP_DELSYS_COUNT, true, NULL, CRCInput::convertDigitToKey(shortCut++), "", true);
		delsys->setHint("", LOCALE_MENU_HINT_SCAN_DELSYS);
		CStringInput *freq = new CStringInput(LOCALE_EXTRA_TP_FREQ, &scansettings.terrestrial_TP_freq, 6, NONEXISTANT_LOCALE, NONEXISTANT_LOCALE, "0123456789");
		Freq = new CMenuDForwarder(LOCALE_EXTRA_TP_FREQ, true, scansettings.terrestrial_TP_freq, freq, "", CRCInput::convertDigitToKey(shortCut++));
		Freq->setHint("", LOCALE_MENU_HINT_SCAN_FREQ);
		bw = new CMenuOptionChooser(LOCALE_EXTRA_TP_BANDWIDTH, (int *)&scansettings.terrestrial_TP_bw, TERRSETUP_SCANTP_BW, TERRSETUP_SCANTP_BW_COUNT, true, NULL, CRCInput::convertDigitToKey(shortCut++));
		bw->setHint("", LOCALE_MENU_HINT_SCAN_BW);
		mod = new CMenuOptionChooser(LOCALE_EXTRA_TP_CONSTELLATION, (int *)&scansettings.terrestrial_TP_constel, TERRSETUP_SCANTP_MOD, TERRSETUP_SCANTP_MOD_COUNT, true, NULL, CRCInput::convertDigitToKey(shortCut++));
		mod->setHint("", LOCALE_MENU_HINT_SCAN_MOD);
		gi = new CMenuOptionChooser(LOCALE_EXTRA_TP_GI, (int *)&scansettings.terrestrial_TP_guard, TERRSETUP_SCANTP_GI, TERRSETUP_SCANTP_GI_COUNT, true, NULL, CRCInput::convertDigitToKey(shortCut++));
		gi->setHint("", LOCALE_MENU_HINT_SCAN_GI);
		hi = new CMenuOptionChooser(LOCALE_EXTRA_TP_HIERARCHY, (int *)&scansettings.terrestrial_TP_hierarchy, TERRSETUP_SCANTP_HIERARCHY, TERRSETUP_SCANTP_HIERARCHY_COUNT, true, NULL, CRCInput::convertDigitToKey(shortCut++));
		hi->setHint("", LOCALE_MENU_HINT_SCAN_HIERARCHY);
		coderateHP = new CMenuOptionChooser(LOCALE_EXTRA_TP_CODERATE_HP, (int *)&scansettings.terrestrial_TP_coderate_HP, TERRSETUP_SCANTP_FEC, TERRSETUP_SCANTP_FEC_COUNT, true, NULL, CRCInput::convertDigitToKey(shortCut++), "", true);
		coderateHP->setHint("", LOCALE_MENU_HINT_SCAN_FEC);
		coderateLP = new CMenuOptionChooser(LOCALE_EXTRA_TP_CODERATE_LP, (int *)&scansettings.terrestrial_TP_coderate_LP, TERRSETUP_SCANTP_FEC, TERRSETUP_SCANTP_FEC_COUNT, true, NULL, CRCInput::convertDigitToKey(shortCut++), "", true);
		coderateLP->setHint("", LOCALE_MENU_HINT_SCAN_FEC);
		tm = new CMenuOptionChooser(LOCALE_EXTRA_TP_TRANSMIT_MODE, (int *)&scansettings.terrestrial_TP_transmit_mode, TERRSETUP_SCANTP_TRANSMIT_MODE, TERRSETUP_SCANTP_TRANSMIT_MODE_COUNT, true, NULL, CRCInput::convertDigitToKey(shortCut++));
		tm->setHint("", LOCALE_MENU_HINT_SCAN_TRANSMIT_MODE);
		CStringInput		*pli 	= new CStringInput(LOCALE_EXTRA_TP_PLI, &scansettings.terrestrial_TP_pli, 1, NONEXISTANT_LOCALE, NONEXISTANT_LOCALE, "0123456789");
		Pli 	= new CMenuDForwarder(LOCALE_EXTRA_TP_PLI, true, scansettings.terrestrial_TP_pli, pli, "", CRCInput::convertDigitToKey(shortCut++));
	}

	if (delsys)
		options_menu->addItem(delsys);
	if (Freq)
		options_menu->addItem(Freq);
	if (Rate)
		options_menu->addItem(Rate);
	if (bw)
		options_menu->addItem(bw);
	if (mod)
		options_menu->addItem(mod);
	if (fec)
		options_menu->addItem(fec);
	if (gi)
		options_menu->addItem(gi);
	if (hi)
		options_menu->addItem(hi);
	if (coderateHP)
		options_menu->addItem(coderateHP);
	if (coderateLP)
		options_menu->addItem(coderateLP);
	if (tm)
		options_menu->addItem(tm);
	if (pol)
		options_menu->addItem(pol);
	if (pilot)
		options_menu->addItem(pilot);

	if (Pli)
		options_menu->addItem(Pli);
	if (Plc)
		options_menu->addItem(Plc);
	if (Plm)
		options_menu->addItem(Plm);

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

//save current settings
void CScanSetup::saveScanSetup()
{
	printf("[neutrino] CScanSetup call %s...\n", __FUNCTION__);
	if(!scansettings.saveSettings(NEUTRINO_SCAN_SETTINGS_FILE))
		dprintf(DEBUG_NORMAL, "error while saving scan-settings!\n");

	//CServiceManager::getInstance()->SaveMotorPositions();
	if(zapit_lat[0] != '#')//check if var ok
		zapitCfg.gotoXXLatitude = strtod(zapit_lat, NULL);
	if(zapit_long[0] != '#')//check if var ok
		zapitCfg.gotoXXLongitude = strtod(zapit_long, NULL);

	CZapit::getInstance()->SetConfig(&zapitCfg);
	CFEManager::getInstance()->saveSettings(true);
	//CFEManager::getInstance()->linkFrontends();
}

bool CScanSetup::changeNotify(const neutrino_locale_t OptionName, void * /*data*/)
{
	bool ret = false;

	if(ARE_LOCALES_EQUAL(OptionName, LOCALE_SATSETUP_USE_USALS)) {
	}
	else if(ARE_LOCALES_EQUAL(OptionName, LOCALE_SATSETUP_DISEQC)) {
		printf("[neutrino] CScanSetup::%s: diseqc %d fenumber %d\n", __FUNCTION__, dmode, fenumber);
		CFrontend * fe = CFEManager::getInstance()->getFE(fenumber);
		if (fe->getDiseqcType() == (diseqc_t)dmode)
			return ret;

		fe_restart = true;
		fe->setDiseqcType((diseqc_t) dmode);
		fe->setTsidOnid(0);

		uniSetup->setActive(dmode == DISEQC_UNICABLE ? true : dmode == DISEQC_UNICABLE2);
		bool enable = (dmode < DISEQC_ADVANCED) && (dmode != NO_DISEQC);
		ojDiseqcRepeats->setActive(enable && !CFrontend::linked(femode) && femode != CFrontend::FE_MODE_UNUSED);
		dorder->setActive(!CFrontend::linked(femode) && femode != CFrontend::FE_MODE_UNUSED && dmode == DISEQC_ADVANCED);
	}
	else if(ARE_LOCALES_EQUAL(OptionName, LOCALE_SATSETUP_FE_MODE)) {
		printf("[neutrino] CScanSetup::%s: fe%d mode %d master %d\n", __FUNCTION__, fenumber, femode, femaster);
		CFrontend * fe = CFEManager::getInstance()->getFE(fenumber);

		if (fe && fe->getMode() == femode)
			return ret;

		modestr[fenumber] = g_Locale->getText(getModeLocale(femode));
		fe_restart = true;
		if (fe)
			fe->setMode(femode);

		if (fe && fe->hasSat()) {
			if (linkfe)
				linkfe->setActive(CFrontend::linked(femode));
			/* leave diseqc type enabled for TWIN in case user need different unicable setup */
			dtype->setActive(femode != CFrontend::FE_MODE_UNUSED && femode != CFrontend::FE_MODE_LINK_LOOP);
			uniSetup->setActive(dmode == DISEQC_UNICABLE ? true : dmode == DISEQC_UNICABLE2 && femode != CFrontend::FE_MODE_UNUSED && femode != CFrontend::FE_MODE_LINK_LOOP);
			dorder->setActive(!CFrontend::linked(femode) && femode != CFrontend::FE_MODE_UNUSED && dmode == DISEQC_ADVANCED);
			fsatSelect->setActive(!CFrontend::linked(femode) && femode != CFrontend::FE_MODE_UNUSED);
			fsatSetup->setActive(!CFrontend::linked(femode) && femode != CFrontend::FE_MODE_UNUSED);
			bool enable = (dmode < DISEQC_ADVANCED) && (dmode != NO_DISEQC);
			ojDiseqcRepeats->setActive(!CFrontend::linked(femode) && femode != CFrontend::FE_MODE_UNUSED && enable);

			msettings.Activate(!CFrontend::linked(femode) && femode != CFrontend::FE_MODE_UNUSED);

			/* if mode changed, set current master too */
			if (femaster >= 0)
				fe->setMaster(femaster);
			setDiseqcOptions(fenumber);
		}
	}
	else if(ARE_LOCALES_EQUAL(OptionName, LOCALE_SATSETUP_FE_MODE_MASTER)) {
		printf("[neutrino] CScanSetup::%s: fe%d link %d \n", __FUNCTION__, fenumber, femaster);
		fe_restart = true;
		CFrontend * fe = CFEManager::getInstance()->getFE(fenumber);
		if (fe)
			fe->setMaster(femaster);
		setDiseqcOptions(fenumber);
	}
	else if(ARE_LOCALES_EQUAL(OptionName, LOCALE_CABLESETUP_PROVIDER)) {
		printf("[neutrino] CScanSetup::%s: new provider: [%s]\n", __FUNCTION__, scansettings.cableName.c_str());
		satellite_map_t & satmap = CServiceManager::getInstance()->SatelliteList();
		for (sat_iterator_t sit = satmap.begin(); sit != satmap.end(); sit++)
		{
			if (scansettings.cableName == sit->second.name) {
				if(sit->second.cable_nid > 0) {
					scansettings.cable_nid = sit->second.cable_nid;
					nid->updateValue();
					ret = true;
				}

				break;
			}
		}
	}
	else if(ARE_LOCALES_EQUAL(OptionName, LOCALE_SATSETUP_MOTOR_POS) ||
			ARE_LOCALES_EQUAL(OptionName, LOCALE_SATSETUP_DISEQC_INPUT)) {
		CFrontend * fe = CFEManager::getInstance()->getFE(fenumber);
		satellite_map_t & satmap = fe->getSatellites();
		int count = 0;
		for (sat_iterator_t sit = satmap.begin(); sit != satmap.end(); ++sit) {
			if(!sit->second.configured)
				continue;
			char opt[100];
			sprintf(opt, "diseqc %d / rotor %d", sit->second.diseqc+1, sit->second.motor_position);
			satoptions[count] = opt;
			satmf[count]->setOption(satoptions[count]);
			count++;
		}
	}
	else if(ARE_LOCALES_EQUAL(OptionName, LOCALE_SATSETUP_LOGICAL_NUMBERS)) {
printf("[neutrino] CScanSetup::%s: logical numbers %d\n", __FUNCTION__, scansettings.scan_logical_numbers);
		lcnhd->setActive(scansettings.scan_logical_numbers);
	}
	else if(ARE_LOCALES_EQUAL(OptionName, LOCALE_TUNERSETUP_MODE)) {
		CFrontend * fe = CFEManager::getInstance()->getFE(fenumber);
		frontend_config_t & fe_config = fe->getConfig();
		if (fe->hasCable() && fe->hasTerr())
			fe->forceDelSys(fe_config.force_mode);
		if (fe_config.force_mode == 1)
			fe_config.powered = 0;
		tsp->setActive(fe_config.force_mode != 1);
		ret = true;
	}
	return ret;
}

void CScanSetup::updateManualSettings()
{
	if (in_menu)
		return;

	CZapitChannel * channel = CZapit::getInstance()->GetCurrentChannel();
	if(channel) {
		transponder_list_t::iterator tI;
		tI = transponders.find(channel->getTransponderId());
		if(tI != transponders.end()) {
			// CFrontend *frontend = CFEManager::getInstance()->getLiveFE();
			// FIXME: should this be live fe current delsys or delsys from tuned TP
			if (CFrontend::isSat(tI->second.feparams.delsys)) {
				scansettings.sat_TP_freq = to_string(tI->second.feparams.frequency);
				scansettings.sat_TP_rate = to_string(tI->second.feparams.symbol_rate);
				scansettings.sat_TP_fec = tI->second.feparams.fec_inner;
				scansettings.sat_TP_pol = tI->second.feparams.polarization;
				scansettings.sat_TP_delsys = tI->second.feparams.delsys;
				scansettings.sat_TP_mod = tI->second.feparams.modulation;
				scansettings.satName = CServiceManager::getInstance()->GetSatelliteName(channel->getSatellitePosition());
				scansettings.sat_TP_pli = to_string(tI->second.feparams.plp_id);
				scansettings.sat_TP_plc = to_string(tI->second.feparams.pls_code);
				scansettings.sat_TP_plm = tI->second.feparams.pls_mode;
			} else if (CFrontend::isCable(tI->second.feparams.delsys)) {
				scansettings.cable_TP_freq = to_string(tI->second.feparams.frequency);
				scansettings.cable_TP_rate = to_string(tI->second.feparams.symbol_rate);
				scansettings.cable_TP_fec = tI->second.feparams.fec_inner;
				scansettings.cable_TP_mod = tI->second.feparams.modulation;
				scansettings.cable_TP_delsys = tI->second.feparams.delsys;
				scansettings.cableName = CServiceManager::getInstance()->GetSatelliteName(channel->getSatellitePosition());
			} else if (CFrontend::isTerr(tI->second.feparams.delsys)) {
				scansettings.terrestrial_TP_freq = to_string(tI->second.feparams.frequency);
				scansettings.terrestrial_TP_bw = tI->second.feparams.bandwidth;
				scansettings.terrestrial_TP_constel = tI->second.feparams.modulation;
				scansettings.terrestrial_TP_hierarchy = tI->second.feparams.hierarchy;
				scansettings.terrestrial_TP_delsys = tI->second.feparams.delsys;
				scansettings.terrestrial_TP_transmit_mode = tI->second.feparams.transmission_mode;
				scansettings.terrestrial_TP_coderate_HP = tI->second.feparams.code_rate_HP;
				scansettings.terrestrial_TP_coderate_LP = tI->second.feparams.code_rate_LP;
				scansettings.terrestrial_TP_pli = to_string(tI->second.feparams.plp_id);

				scansettings.terrestrialName = CServiceManager::getInstance()->GetSatelliteName(channel->getSatellitePosition());
			}
		}
	}
}

void CScanSetup::setOptionSatSelect(int fe_number, CMenuForwarder* menu_item)
{
	satellite_map_t & satmap = CFEManager::getInstance()->getFE(fe_number)->getSatellites();
	int count = 0;
	for (sat_iterator_t sit = satmap.begin(); sit != satmap.end(); ++sit) {
		if(!sit->second.configured)
			continue;
		count++;
	}
	std::string count_of = to_string(count) + char(0x2f) + to_string(satmap.size());
	menu_item->setOption(count_of);
}

int CTPSelectHandler::exec(CMenuTarget* parent, const std::string &actionkey)
{
	std::map<int, transponder> tmplist;
	std::map<int, transponder>::iterator tmpI;
	char cnt[5];
	int select = -1;
	static int old_selected = 0;
	static t_satellite_position old_position = 0;

	int width = 40;

	if (parent)
		parent->hide();

	t_satellite_position position;
	std::string name;
	if (actionkey == "sat")
		name = scansettings.satName;
	else if (actionkey == "terrestrial")
		name = scansettings.terrestrialName;
	else 
		name = scansettings.cableName;

	position = CServiceManager::getInstance()->GetSatellitePosition(name.c_str());
	INFO("%s: %s\n", actionkey.c_str(), name.c_str());

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

	int i = menu.getItemsCount();
	transponder_list_t &select_transponders = CServiceManager::getInstance()->GetSatelliteTransponders(position);
	for (transponder_list_t::iterator tI = select_transponders.begin(); tI != select_transponders.end(); ++tI) {
		snprintf(cnt, sizeof(cnt), "%d", i);
		transponder & t = tI->second;

		if(!old_selected && ct == t)
			old_selected = i;

		std::string tname = t.description();
		CMenuForwarder * ts_item = new CMenuForwarder(tname.c_str(), true, NULL, selector, cnt, CRCInput::RC_nokey, NULL)/*, false*/;

		ts_item->setItemButton(NEUTRINO_ICON_BUTTON_OKAY, true);
		menu.addItem(ts_item, old_selected == i);

		tmplist.insert(std::pair <int, transponder>(i, tI->second));
		i++;
	}
	if (i == 0) {
		std::string text = "No transponders found for ";
		text += name;
		ShowHint(LOCALE_MESSAGEBOX_ERROR, text.c_str(), 450, 2);
		delete selector;
		return menu_return::RETURN_REPAINT;
	}

	menu.setSelected(old_selected);
	int retval = menu.exec(NULL, "");

	delete selector;

	if (select >= 0) {
		old_selected = select;

		tmpI = tmplist.find(select);

		tmpI->second.dump("CTPSelectHandler::exec: selected TP:");
		if (CFrontend::isSat(tmpI->second.feparams.delsys)) {
			scansettings.sat_TP_freq = to_string(tmpI->second.feparams.frequency);
			scansettings.sat_TP_rate = to_string(tmpI->second.feparams.symbol_rate);
			scansettings.sat_TP_fec = tmpI->second.feparams.fec_inner;
			scansettings.sat_TP_pol = tmpI->second.feparams.polarization;
			scansettings.sat_TP_delsys = tmpI->second.feparams.delsys;
			scansettings.sat_TP_mod = tmpI->second.feparams.modulation;
			scansettings.sat_TP_pilot = tmpI->second.feparams.pilot;
			scansettings.sat_TP_pli = to_string(tmpI->second.feparams.plp_id);
			scansettings.sat_TP_plc = to_string(tmpI->second.feparams.pls_code);
			scansettings.sat_TP_plm = tmpI->second.feparams.pls_mode;
		}
		else if (CFrontend::isCable(tmpI->second.feparams.delsys)) {
			scansettings.cable_TP_freq = to_string(tmpI->second.feparams.frequency);
			scansettings.cable_TP_rate = to_string(tmpI->second.feparams.symbol_rate);
			scansettings.cable_TP_fec = tmpI->second.feparams.fec_inner;
			scansettings.cable_TP_mod = tmpI->second.feparams.modulation;
		}
		else if (CFrontend::isTerr(tmpI->second.feparams.delsys)) {
			scansettings.terrestrial_TP_freq = to_string(tmpI->second.feparams.frequency);
			scansettings.terrestrial_TP_bw = tmpI->second.feparams.bandwidth;
			scansettings.terrestrial_TP_constel = tmpI->second.feparams.modulation;
			scansettings.terrestrial_TP_hierarchy = tmpI->second.feparams.hierarchy;
			scansettings.terrestrial_TP_delsys = tmpI->second.feparams.delsys;
			scansettings.terrestrial_TP_transmit_mode = tmpI->second.feparams.transmission_mode;
			scansettings.terrestrial_TP_coderate_HP = tmpI->second.feparams.code_rate_HP;
			scansettings.terrestrial_TP_coderate_LP = tmpI->second.feparams.code_rate_LP;
			scansettings.terrestrial_TP_pli = to_string(tmpI->second.feparams.plp_id);
			//scansettings.terrestrialName = CServiceManager::getInstance()->GetSatelliteName(channel->getSatellitePosition());
		}
	}

	if (retval == menu_return::RETURN_EXIT_ALL)
		return menu_return::RETURN_EXIT_ALL;

	return menu_return::RETURN_REPAINT;
}
