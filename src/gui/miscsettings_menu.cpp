/*
	$port: miscsettings_menu.cpp,v 1.3 2010/12/05 22:32:12 tuxbox-cvs Exp $

	miscsettings_menu implementation - Neutrino-GUI

	Copyright (C) 2010 T. Graf 'dbt'
	Homepage: http://www.dbox2-tuning.net/


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
#include <system/setting_helpers.h>
#include <system/helpers.h>

#include <gui/miscsettings_menu.h>
#include <gui/cec_setup.h>
#include <gui/filebrowser.h>
#include <gui/keybind_setup.h>
#include <gui/plugins.h>
#include <gui/sleeptimer.h>
#include <gui/zapit_setup.h>

#include <gui/widget/icons.h>
#include <gui/widget/stringinput.h>
#include <gui/widget/messagebox.h>

#include <driver/screen_max.h>

#include <system/debug.h>
#include <zapit/femanager.h>
#include <eitd/sectionsd.h>

#include <cs_api.h>
#include <video.h>

//#define ONE_KEY_PLUGIN

extern CPlugins       * g_PluginList;
extern cVideo *videoDecoder;

CMiscMenue::CMiscMenue()
{
	width = w_max (40, 10);
}

CMiscMenue::~CMiscMenue()
{

}

int CMiscMenue::exec(CMenuTarget* parent, const std::string &actionKey)
{
	printf("init extended settings menu...\n");

	if(parent != NULL)
		parent->hide();

	if(actionKey == "epgdir")
	{
		const char *action_str = "epg";
		if(chooserDir(g_settings.epg_dir, true, action_str))
			CNeutrinoApp::getInstance()->SendSectionsdConfig();

		return menu_return::RETURN_REPAINT;
	}
	else if(actionKey == "plugin_dir")
	{
		const char *action_str = "plugin";
		if(chooserDir(g_settings.plugin_hdd_dir, false, action_str))
			g_PluginList->loadPlugins();

		return menu_return::RETURN_REPAINT;
	}
#ifdef ONE_KEY_PLUGIN
	else if(actionKey == "onekeyplugin")
	{
		CMenuWidget MoviePluginSelector(LOCALE_EXTRA_KEY_PLUGIN, NEUTRINO_ICON_FEATURES);
		MoviePluginSelector.addItem(GenericMenuSeparator);

		char id[5];
		int cnt = 0;
		int enabled_count = 0;
		for(unsigned int count=0;count < (unsigned int) g_PluginList->getNumberOfPlugins();count++)
		{
			if (g_PluginList->getType(count)== CPlugins::P_TYPE_TOOL && !g_PluginList->isHidden(count))
			{
				// e.g. vtxt-plugins
				sprintf(id, "%d", count);
				enabled_count++;
				MoviePluginSelector.addItem(new CMenuForwarder(g_PluginList->getName(count), true, NULL, new COnekeyPluginChangeExec(), id, CRCInput::convertDigitToKey(count)), (cnt == 0));
				cnt++;
			}
		}

		MoviePluginSelector.exec(NULL, "");
		return menu_return::RETURN_REPAINT;
	}
#endif /*ONE_KEY_PLUGIN*/
	else if(actionKey == "info")
	{
		unsigned num = CEitManager::getInstance()->getEventsCount();
		char str[128];
		sprintf(str, "Event count: %d", num);
		ShowMsgUTF(LOCALE_MESSAGEBOX_INFO, str, CMessageBox::mbrBack, CMessageBox::mbBack);
		return menu_return::RETURN_REPAINT;
	}
	else if(actionKey == "energy")
	{
		return showMiscSettingsMenuEnergy();
	}
	else if(actionKey == "channellist")
	{
		return showMiscSettingsMenuChanlist();
	}

	return showMiscSettingsMenu();
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

#define CHANNELLIST_NEW_ZAP_MODE_OPTION_COUNT 3
const CMenuOptionChooser::keyval CHANNELLIST_NEW_ZAP_MODE_OPTIONS[CHANNELLIST_NEW_ZAP_MODE_OPTION_COUNT] =
{
	{ 0, LOCALE_CHANNELLIST_NEW_ZAP_MODE_OFF	},
	{ 1, LOCALE_CHANNELLIST_NEW_ZAP_MODE_ALLOW	},
	{ 2, LOCALE_CHANNELLIST_NEW_ZAP_MODE_ACTIVE	}
};

#ifdef CPU_FREQ
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
	{ 600, NONEXISTANT_LOCALE, "600 Mhz"}
};
#endif /*CPU_FREQ*/

#define EPG_SCAN_OPTION_COUNT 3
const CMenuOptionChooser::keyval EPG_SCAN_OPTIONS[EPG_SCAN_OPTION_COUNT] =
{
	{ 0, LOCALE_OPTIONS_OFF },
	{ 1, LOCALE_MISCSETTINGS_EPG_SCAN_BQ },
	{ 2, LOCALE_MISCSETTINGS_EPG_SCAN_FAV },
};

#define SLEEPTIMER_MIN_OPTION_COUNT 7
const CMenuOptionChooser::keyval_ext SLEEPTIMER_MIN_OPTIONS[SLEEPTIMER_MIN_OPTION_COUNT] =
{
	{ 0,	NONEXISTANT_LOCALE, "EPG"	},
	{ 30,	NONEXISTANT_LOCALE, "30 min"	},
	{ 60,	NONEXISTANT_LOCALE, "60 min"	},
	{ 90,	NONEXISTANT_LOCALE, "90 min"	},
	{ 120,	NONEXISTANT_LOCALE, "120 min"	},
	{ 150,	NONEXISTANT_LOCALE, "150 min"	}
};

//show misc settings menue
int CMiscMenue::showMiscSettingsMenu()
{
	//misc settings
	fanNotifier = new CFanControlNotifier();
	sectionsdConfigNotifier = new CSectionsdConfigNotifier();
	CMenuWidget misc_menue(LOCALE_MAINSETTINGS_HEAD, NEUTRINO_ICON_SETTINGS, width, MN_WIDGET_ID_MISCSETUP);

	misc_menue.addIntroItems(LOCALE_MISCSETTINGS_HEAD);

	//general
	CMenuWidget misc_menue_general(LOCALE_MISCSETTINGS_HEAD, NEUTRINO_ICON_SETTINGS, width, MN_WIDGET_ID_MISCSETUP_GENERAL);
	showMiscSettingsMenuGeneral(&misc_menue_general);
	CMenuForwarder * mf = new CMenuForwarder(LOCALE_MISCSETTINGS_GENERAL, true, NULL, &misc_menue_general, NULL, CRCInput::RC_red, NEUTRINO_ICON_BUTTON_RED);
	mf->setHint("", LOCALE_MENU_HINT_MISC_GENERAL);
	misc_menue.addItem(mf);

	//energy, shutdown
	if(cs_get_revision() > 7)
	{
		mf = new CMenuForwarder(LOCALE_MISCSETTINGS_ENERGY, true, NULL, this, "energy", CRCInput::RC_green, NEUTRINO_ICON_BUTTON_GREEN);
		mf->setHint("", LOCALE_MENU_HINT_MISC_ENERGY);
		misc_menue.addItem(mf);
	}

	//epg
	CMenuWidget misc_menue_epg(LOCALE_MISCSETTINGS_HEAD, NEUTRINO_ICON_SETTINGS, width, MN_WIDGET_ID_MISCSETUP_EPG);
	showMiscSettingsMenuEpg(&misc_menue_epg);
	mf = new CMenuForwarder(LOCALE_MISCSETTINGS_EPG_HEAD, true, NULL, &misc_menue_epg, NULL, CRCInput::RC_yellow,NEUTRINO_ICON_BUTTON_YELLOW);
	mf->setHint("", LOCALE_MENU_HINT_MISC_EPG);
	misc_menue.addItem(mf);

	//filebrowser settings
	CMenuWidget misc_menue_fbrowser(LOCALE_MISCSETTINGS_HEAD, NEUTRINO_ICON_SETTINGS, width, MN_WIDGET_ID_MISCSETUP_FILEBROWSER);
	showMiscSettingsMenuFBrowser(&misc_menue_fbrowser);
	mf = new CMenuForwarder(LOCALE_FILEBROWSER_HEAD, true, NULL, &misc_menue_fbrowser, NULL, CRCInput::RC_blue, NEUTRINO_ICON_BUTTON_BLUE);
	mf->setHint("", LOCALE_MENU_HINT_MISC_FILEBROWSER);
	misc_menue.addItem(mf);

	misc_menue.addItem(GenericMenuSeparatorLine);

	//cec settings
	CCECSetup cecsetup;
	mf = new CMenuForwarder(LOCALE_VIDEOMENU_HDMI_CEC, true, NULL, &cecsetup, NULL, CRCInput::RC_1);
	mf->setHint("", LOCALE_MENU_HINT_MISC_CEC);
	misc_menue.addItem(mf);

	//channellist
	mf = new CMenuForwarder(LOCALE_MISCSETTINGS_CHANNELLIST, true, NULL, this, "channellist", CRCInput::RC_2);
	mf->setHint("", LOCALE_MENU_HINT_MISC_CHANNELLIST);
	misc_menue.addItem(mf);

	//start channels
	CZapitSetup zapitsetup;
	mf = new CMenuForwarder(LOCALE_ZAPITSETUP_HEAD, true, NULL, &zapitsetup, NULL, CRCInput::RC_3);
	mf->setHint("", LOCALE_MENU_HINT_MISC_ZAPIT);
	misc_menue.addItem(mf);

#ifdef CPU_FREQ
	//CPU
	CMenuWidget misc_menue_cpu("CPU", NEUTRINO_ICON_SETTINGS, width);
	showMiscSettingsMenuCPUFreq(&misc_menue_cpu);
	misc_menue.addItem( new CMenuForwarder("CPU", true, NULL, &misc_menue_cpu, NULL, CRCInput::RC_4));
#endif /*CPU_FREQ*/

	int res = misc_menue.exec(NULL, "");

	g_settings.epg_cache = atoi(epg_cache.c_str());
	g_settings.epg_extendedcache = atoi(epg_extendedcache.c_str());
	g_settings.epg_old_events = atoi(epg_old_events.c_str());
	g_settings.epg_max_events = atoi(epg_max_events.c_str());

	delete fanNotifier;
	delete sectionsdConfigNotifier;
#if 0
	if(cs_get_revision() > 7)
		delete miscNotifier;
#endif
	delete miscEpgNotifier;
	return res;
}

//general settings
void CMiscMenue::showMiscSettingsMenuGeneral(CMenuWidget *ms_general)
{
	ms_general->addIntroItems(LOCALE_MISCSETTINGS_GENERAL);

	//standby after boot
	CMenuOptionChooser * mc = new CMenuOptionChooser(LOCALE_EXTRA_START_TOSTANDBY, &g_settings.power_standby, OPTIONS_OFF0_ON1_OPTIONS, OPTIONS_OFF0_ON1_OPTION_COUNT, true);
	mc->setHint("", LOCALE_MENU_HINT_START_TOSTANDBY);
	ms_general->addItem(mc);

	mc = new CMenuOptionChooser(LOCALE_EXTRA_CACHE_TXT,  (int *)&g_settings.cacheTXT, OPTIONS_OFF0_ON1_OPTIONS, OPTIONS_OFF0_ON1_OPTION_COUNT, true);
	mc->setHint("", LOCALE_MENU_HINT_CACHE_TXT);
	ms_general->addItem(mc);

	//fan speed
	if (g_info.has_fan)
	{
		CMenuOptionNumberChooser * mn = new CMenuOptionNumberChooser(LOCALE_FAN_SPEED, &g_settings.fan_speed, true, 1, 14, fanNotifier, 0, 0, LOCALE_OPTIONS_OFF);
		mn->setHint("", LOCALE_MENU_HINT_FAN_SPEED);
		ms_general->addItem(mn);
	}

	//rotor
	//don't show rotor settings on cable box
	if (CFEManager::getInstance()->haveSat()) {
		mc = new CMenuOptionChooser(LOCALE_EXTRA_ROTOR_SWAP, &g_settings.rotor_swap, OPTIONS_OFF0_ON1_OPTIONS, OPTIONS_OFF0_ON1_OPTION_COUNT, true);
		mc->setHint("", LOCALE_MENU_HINT_ROTOR_SWAP);
		ms_general->addItem(mc);
	}

	CMenuForwarder * mf = new CMenuForwarder(LOCALE_PLUGINS_HDD_DIR, true, g_settings.plugin_hdd_dir, this, "plugin_dir");
	mf->setHint("", LOCALE_MENU_HINT_PLUGINS_HDD_DIR);
	ms_general->addItem(mf);
#ifdef ONE_KEY_PLUGIN
	ms_general->addItem(new CMenuForwarder(LOCALE_EXTRA_KEY_PLUGIN, true, g_settings.onekey_plugin,this,"onekeyplugin"));
#endif /*ONE_KEY_PLUGIN*/
}

#define VIDEOMENU_HDMI_CEC_MODE_OPTION_COUNT 2
const CMenuOptionChooser::keyval VIDEOMENU_HDMI_CEC_MODE_OPTIONS[VIDEOMENU_HDMI_CEC_MODE_OPTION_COUNT] =
{
	{ VIDEO_HDMI_CEC_MODE_OFF       , LOCALE_OPTIONS_OFF   },
	{ VIDEO_HDMI_CEC_MODE_TUNER     , LOCALE_OPTIONS_ON    }
};

//energy and shutdown settings
int CMiscMenue::showMiscSettingsMenuEnergy()
{
	CMenuWidget *ms_energy = new CMenuWidget(LOCALE_MISCSETTINGS_HEAD, NEUTRINO_ICON_SETTINGS, width, MN_WIDGET_ID_MISCSETUP_ENERGY);
	ms_energy->addIntroItems(LOCALE_MISCSETTINGS_ENERGY);

	CMenuOptionChooser *m1 = new CMenuOptionChooser(LOCALE_MISCSETTINGS_SHUTDOWN_REAL_RCDELAY, &g_settings.shutdown_real_rcdelay, OPTIONS_OFF0_ON1_OPTIONS, OPTIONS_OFF0_ON1_OPTION_COUNT, !g_settings.shutdown_real);
	m1->setHint("", LOCALE_MENU_HINT_SHUTDOWN_RCDELAY);

	std::string shutdown_count = to_string(g_settings.shutdown_count);
	if (shutdown_count.length() < 3)
		shutdown_count.insert(0, 3 - shutdown_count.length(), ' ');
	CStringInput * miscSettings_shutdown_count = new CStringInput(LOCALE_MISCSETTINGS_SHUTDOWN_COUNT, &shutdown_count, 3, LOCALE_MISCSETTINGS_SHUTDOWN_COUNT_HINT1, LOCALE_MISCSETTINGS_SHUTDOWN_COUNT_HINT2, "0123456789 ");
	CMenuForwarder *m2 = new CMenuDForwarder(LOCALE_MISCSETTINGS_SHUTDOWN_COUNT, !g_settings.shutdown_real, shutdown_count.c_str(), miscSettings_shutdown_count);
	m2->setHint("", LOCALE_MENU_HINT_SHUTDOWN_COUNT);

	COnOffNotifier * miscNotifier = new COnOffNotifier(1);
	miscNotifier->addItem(m1);
	miscNotifier->addItem(m2);

	CMenuOptionChooser * mc = new CMenuOptionChooser(LOCALE_MISCSETTINGS_SHUTDOWN_REAL, &g_settings.shutdown_real, OPTIONS_OFF1_ON0_OPTIONS, OPTIONS_OFF1_ON0_OPTION_COUNT, true, miscNotifier);
	mc->setHint("", LOCALE_MENU_HINT_SHUTDOWN_REAL);

	ms_energy->addItem(mc);
	ms_energy->addItem(m1);
	ms_energy->addItem(m2);

	m2 = new CMenuDForwarder(LOCALE_MISCSETTINGS_SLEEPTIMER, true, NULL, new CSleepTimerWidget(true));
	m2->setHint("", LOCALE_MENU_HINT_INACT_TIMER);
	ms_energy->addItem(m2);

	CMenuOptionChooser * m4 = new CMenuOptionChooser(LOCALE_MISCSETTINGS_SLEEPTIMER_MIN, &g_settings.sleeptimer_min, SLEEPTIMER_MIN_OPTIONS, SLEEPTIMER_MIN_OPTION_COUNT, true);
	m4->setHint("", LOCALE_MENU_HINT_SLEEPTIMER_MIN);
	ms_energy->addItem(m4);

	if (g_settings.easymenu) {
		CMenuOptionChooser *cec_ch = new CMenuOptionChooser(LOCALE_VIDEOMENU_HDMI_CEC, &g_settings.hdmi_cec_mode, VIDEOMENU_HDMI_CEC_MODE_OPTIONS, VIDEOMENU_HDMI_CEC_MODE_OPTION_COUNT, true, this);
		cec_ch->setHint("", LOCALE_MENU_HINT_CEC_MODE);
		ms_energy->addItem(cec_ch);
	}

	int res = ms_energy->exec(NULL, "");

	g_settings.shutdown_count = atoi(shutdown_count.c_str());

	delete ms_energy;
	delete miscNotifier;
	return res;
}

//EPG settings
void CMiscMenue::showMiscSettingsMenuEpg(CMenuWidget *ms_epg)
{
	ms_epg->addIntroItems(LOCALE_MISCSETTINGS_EPG_HEAD);
	ms_epg->addKey(CRCInput::RC_info, this, "info");

	CMenuOptionChooser * mc1 = new CMenuOptionChooser(LOCALE_MISCSETTINGS_EPG_SAVE_STANDBY, &g_settings.epg_save_standby, OPTIONS_OFF0_ON1_OPTIONS, OPTIONS_OFF0_ON1_OPTION_COUNT, g_settings.epg_save);
	mc1->setHint("", LOCALE_MENU_HINT_EPG_SAVE_STANDBY);

	epg_cache = to_string(g_settings.epg_cache);
	if (epg_cache.length() < 2)
		epg_cache.insert(0, 2 - epg_cache.length(), ' ');
	CStringInput * miscSettings_epg_cache = new CStringInput(LOCALE_MISCSETTINGS_EPG_CACHE, &epg_cache, 2,LOCALE_MISCSETTINGS_EPG_CACHE_HINT1, LOCALE_MISCSETTINGS_EPG_CACHE_HINT2 , "0123456789 ", sectionsdConfigNotifier);
	CMenuForwarder * mf = new CMenuDForwarder(LOCALE_MISCSETTINGS_EPG_CACHE, true, epg_cache, miscSettings_epg_cache);
	mf->setHint("", LOCALE_MENU_HINT_EPG_CACHE);

	epg_extendedcache = to_string(g_settings.epg_extendedcache);
	if (epg_extendedcache.length() < 3)
		epg_extendedcache.insert(0, 3 - epg_extendedcache.length(), ' ');
	CStringInput * miscSettings_epg_cache_e = new CStringInput(LOCALE_MISCSETTINGS_EPG_EXTENDEDCACHE, &epg_extendedcache, 3,LOCALE_MISCSETTINGS_EPG_EXTENDEDCACHE_HINT1, LOCALE_MISCSETTINGS_EPG_EXTENDEDCACHE_HINT2 , "0123456789 ", sectionsdConfigNotifier);
	CMenuForwarder * mf1  = new CMenuDForwarder(LOCALE_MISCSETTINGS_EPG_EXTENDEDCACHE, true, epg_extendedcache.c_str(), miscSettings_epg_cache_e);
	mf1->setHint("", LOCALE_MENU_HINT_EPG_EXTENDEDCACHE);

	epg_old_events = to_string(g_settings.epg_old_events);
	if (epg_old_events.length() < 3)
		epg_old_events.insert(0, 3 - epg_old_events.length(), ' ');
	CStringInput * miscSettings_epg_old_events = new CStringInput(LOCALE_MISCSETTINGS_EPG_OLD_EVENTS, &epg_old_events, 3,LOCALE_MISCSETTINGS_EPG_OLD_EVENTS_HINT1, LOCALE_MISCSETTINGS_EPG_OLD_EVENTS_HINT2 , "0123456789 ", sectionsdConfigNotifier);
	CMenuForwarder * mf2 = new CMenuDForwarder(LOCALE_MISCSETTINGS_EPG_OLD_EVENTS, true, epg_old_events.c_str(), miscSettings_epg_old_events);
	mf2->setHint("", LOCALE_MENU_HINT_EPG_OLD_EVENTS);

	epg_max_events = to_string(g_settings.epg_max_events);
	if (epg_max_events.length() < 6)
		epg_max_events.insert(0, 6 - epg_max_events.length(), ' ');
	CStringInput * miscSettings_epg_max_events = new CStringInput(LOCALE_MISCSETTINGS_EPG_MAX_EVENTS, &epg_max_events, 6,LOCALE_MISCSETTINGS_EPG_MAX_EVENTS_HINT1, LOCALE_MISCSETTINGS_EPG_MAX_EVENTS_HINT2 , "0123456789 ", sectionsdConfigNotifier);
	CMenuForwarder * mf3 = new CMenuDForwarder(LOCALE_MISCSETTINGS_EPG_MAX_EVENTS, true, epg_max_events.c_str(), miscSettings_epg_max_events);
	mf3->setHint("", LOCALE_MENU_HINT_EPG_MAX_EVENTS);

	CMenuForwarder * mf4 = new CMenuForwarder(LOCALE_MISCSETTINGS_EPG_DIR, g_settings.epg_save, g_settings.epg_dir, this, "epgdir");
	mf4->setHint("", LOCALE_MENU_HINT_EPG_DIR);

	miscEpgNotifier = new COnOffNotifier();
	miscEpgNotifier->addItem(mc1);
	//miscEpgNotifier->addItem(mf);
	//miscEpgNotifier->addItem(mf1);
	//miscEpgNotifier->addItem(mf2);
	//miscEpgNotifier->addItem(mf3);
	miscEpgNotifier->addItem(mf4);

	CMenuOptionChooser * mc = new CMenuOptionChooser(LOCALE_MISCSETTINGS_EPG_SAVE, &g_settings.epg_save, OPTIONS_OFF0_ON1_OPTIONS, OPTIONS_OFF0_ON1_OPTION_COUNT, true,miscEpgNotifier);
	mc->setHint("", LOCALE_MENU_HINT_EPG_SAVE);

	CMenuOptionChooser * mc2 = new CMenuOptionChooser(LOCALE_MISCSETTINGS_EPG_SCAN, &g_settings.epg_scan, EPG_SCAN_OPTIONS, EPG_SCAN_OPTION_COUNT,
		true /*CFEManager::getInstance()->getEnabledCount() > 1*/);
	mc2->setHint("", LOCALE_MENU_HINT_EPG_SCAN);

	ms_epg->addItem(mc);
	ms_epg->addItem(mc1);
	ms_epg->addItem(mf);
	ms_epg->addItem(mf1);
	ms_epg->addItem(mf2);
	ms_epg->addItem(mf3);
	ms_epg->addItem(mf4);
	ms_epg->addItem(mc2);
}

//filebrowser settings
void CMiscMenue::showMiscSettingsMenuFBrowser(CMenuWidget *ms_fbrowser)
{
	ms_fbrowser->addIntroItems(LOCALE_FILEBROWSER_HEAD);

	CMenuOptionChooser * mc;
	mc = new CMenuOptionChooser(LOCALE_FILESYSTEM_IS_UTF8            , &g_settings.filesystem_is_utf8            , MISCSETTINGS_FILESYSTEM_IS_UTF8_OPTIONS, MISCSETTINGS_FILESYSTEM_IS_UTF8_OPTION_COUNT, true );
	mc->setHint("", LOCALE_MENU_HINT_FILESYSTEM_IS_UTF8);
	ms_fbrowser->addItem(mc);

	mc = new CMenuOptionChooser(LOCALE_FILEBROWSER_SHOWRIGHTS        , &g_settings.filebrowser_showrights        , MESSAGEBOX_NO_YES_OPTIONS              , MESSAGEBOX_NO_YES_OPTION_COUNT              , true );
	mc->setHint("", LOCALE_MENU_HINT_FILEBROWSER_SHOWRIGHTS);
	ms_fbrowser->addItem(mc);
	mc = new CMenuOptionChooser(LOCALE_FILEBROWSER_DENYDIRECTORYLEAVE, &g_settings.filebrowser_denydirectoryleave, MESSAGEBOX_NO_YES_OPTIONS              , MESSAGEBOX_NO_YES_OPTION_COUNT              , true );
	mc->setHint("", LOCALE_MENU_HINT_FILEBROWSER_DENYDIRECTORYLEAVE);
	ms_fbrowser->addItem(mc);
}

//channellist
int CMiscMenue::showMiscSettingsMenuChanlist()
{
	CMenuWidget * ms_chanlist = new CMenuWidget(LOCALE_MISCSETTINGS_HEAD, NEUTRINO_ICON_SETTINGS, width, MN_WIDGET_ID_MISCSETUP_CHANNELLIST);
	ms_chanlist->addIntroItems(LOCALE_MISCSETTINGS_CHANNELLIST);

	CMenuOptionChooser * mc;
	mc = new CMenuOptionChooser(LOCALE_CHANNELLIST_MAKE_HDLIST ,     &g_settings.make_hd_list            , OPTIONS_OFF0_ON1_OPTIONS, OPTIONS_OFF0_ON1_OPTION_COUNT, true);
	mc->setHint("", LOCALE_MENU_HINT_MAKE_HDLIST);
	ms_chanlist->addItem(mc);

	mc = new CMenuOptionChooser(LOCALE_CHANNELLIST_MAKE_NEWLIST,     &g_settings.make_new_list           , OPTIONS_OFF0_ON1_OPTIONS, OPTIONS_OFF0_ON1_OPTION_COUNT, true);
	mc->setHint("", LOCALE_MENU_HINT_MAKE_NEWLIST);
	ms_chanlist->addItem(mc);

	mc = new CMenuOptionChooser(LOCALE_CHANNELLIST_MAKE_REMOVEDLIST, &g_settings.make_removed_list       , OPTIONS_OFF0_ON1_OPTIONS, OPTIONS_OFF0_ON1_OPTION_COUNT, true);
	mc->setHint("", LOCALE_MENU_HINT_MAKE_REMOVEDLIST);
	ms_chanlist->addItem(mc);

	mc = new CMenuOptionChooser(LOCALE_CHANNELLIST_KEEP_NUMBERS,     &g_settings.keep_channel_numbers    , OPTIONS_OFF0_ON1_OPTIONS, OPTIONS_OFF0_ON1_OPTION_COUNT, true);
	mc->setHint("", LOCALE_MENU_HINT_KEEP_NUMBERS);
	ms_chanlist->addItem(mc);

	mc = new CMenuOptionChooser(LOCALE_EXTRA_ZAP_CYCLE         ,     &g_settings.zap_cycle               , OPTIONS_OFF0_ON1_OPTIONS, OPTIONS_OFF0_ON1_OPTION_COUNT, true);
	mc->setHint("", LOCALE_MENU_HINT_ZAP_CYCLE);
	ms_chanlist->addItem(mc);

	mc = new CMenuOptionChooser(LOCALE_CHANNELLIST_NEW_ZAP_MODE,     &g_settings.channellist_new_zap_mode, CHANNELLIST_NEW_ZAP_MODE_OPTIONS, CHANNELLIST_NEW_ZAP_MODE_OPTION_COUNT, true );
	mc->setHint("", LOCALE_MENU_HINT_NEW_ZAP_MODE);
	ms_chanlist->addItem(mc);

	mc = new CMenuOptionChooser(LOCALE_CHANNELLIST_NUMERIC_ADJUST,   &g_settings.channellist_numeric_adjust, OPTIONS_OFF0_ON1_OPTIONS, OPTIONS_OFF0_ON1_OPTION_COUNT, true);
	mc->setHint("", LOCALE_MENU_HINT_NUMERIC_ADJUST);
	ms_chanlist->addItem(mc);
	int res = ms_chanlist->exec(NULL, "");
	delete ms_chanlist;
	return res;
}

#ifdef CPU_FREQ
//CPU
void CMiscMenue::showMiscSettingsMenuCPUFreq(CMenuWidget *ms_cpu)
{
	ms_cpu->addIntroItems();

	CCpuFreqNotifier * cpuNotifier = new CCpuFreqNotifier();
	ms_cpu->addItem(new CMenuOptionChooser(LOCALE_CPU_FREQ_NORMAL, &g_settings.cpufreq, CPU_FREQ_OPTIONS, CPU_FREQ_OPTION_COUNT, true, cpuNotifier));
	ms_cpu->addItem(new CMenuOptionChooser(LOCALE_CPU_FREQ_STANDBY, &g_settings.standby_cpufreq, CPU_FREQ_OPTIONS, CPU_FREQ_OPTION_COUNT, true));
}
#endif /*CPU_FREQ*/

bool CMiscMenue::changeNotify(const neutrino_locale_t OptionName, void * /*data*/)
{
	if (ARE_LOCALES_EQUAL(OptionName, LOCALE_VIDEOMENU_HDMI_CEC))
	{
		printf("[neutrino CEC Settings] %s set CEC settings...\n", __FUNCTION__);
		g_settings.hdmi_cec_standby = 0;
		g_settings.hdmi_cec_view_on = 0;
		if (g_settings.hdmi_cec_mode != VIDEO_HDMI_CEC_MODE_OFF) {
			g_settings.hdmi_cec_standby = 1;
			g_settings.hdmi_cec_view_on = 1;
			g_settings.hdmi_cec_mode = VIDEO_HDMI_CEC_MODE_TUNER;
		}
		videoDecoder->SetCECAutoStandby(g_settings.hdmi_cec_standby == 1);
		videoDecoder->SetCECAutoView(g_settings.hdmi_cec_view_on == 1);
		videoDecoder->SetCECMode((VIDEO_HDMI_CEC_MODE)g_settings.hdmi_cec_mode);
	}

	return false;
}
