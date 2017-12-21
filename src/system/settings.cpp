/*

        $Id: settings.cpp,v 1.38 2005/03/03 19:59:41 diemade Exp $

	Neutrino-GUI  -   DBoxII-Project

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

#include <cstring>
#include <system/settings.h>

#include <zapit/settings.h>
#include <zapit/scan.h>
#include <gui/personalize.h>

//enum PERSONALIZE_SETTINGS to find in settings.h
const struct personalize_settings_t personalize_settings[SNeutrinoSettings::P_SETTINGS_MAX] =
{
	{"personalize_pinstatus"		, CPersonalizeGui::PERSONALIZE_PROTECT_MODE_NOT_PROTECTED},
	
	//user menu
	{"personalize_bluebutton"		, CPersonalizeGui::PERSONALIZE_ACTIVE_MODE_ENABLED}, // features
	{"personalize_yellowbutton"		, CPersonalizeGui::PERSONALIZE_ACTIVE_MODE_ENABLED}, // features
	{"personalize_greenbutton"		, CPersonalizeGui::PERSONALIZE_ACTIVE_MODE_ENABLED}, // features
	{"personalize_redbutton"		, CPersonalizeGui::PERSONALIZE_ACTIVE_MODE_ENABLED}, // epg/info
	
	//main menu
	{"personalize_tv_mode"			, CPersonalizeGui::PERSONALIZE_MODE_NOTVISIBLE},
	{"personalize_tv_radio_mode"		, CPersonalizeGui::PERSONALIZE_MODE_VISIBLE}, //toggle
	{"personalize_radio_mode"		, CPersonalizeGui::PERSONALIZE_MODE_NOTVISIBLE},
	{"personalize_timer"			, CPersonalizeGui::PERSONALIZE_MODE_VISIBLE}, 
	{"personalize_media"			, CPersonalizeGui::PERSONALIZE_MODE_VISIBLE}, 
	
	{"personalize_games"			, CPersonalizeGui::PERSONALIZE_MODE_NOTVISIBLE},	
	{"personalize_tools"			, CPersonalizeGui::PERSONALIZE_MODE_NOTVISIBLE},
	{"personalize_scripts"			, CPersonalizeGui::PERSONALIZE_MODE_NOTVISIBLE},
	{"personalize_lua"			, CPersonalizeGui::PERSONALIZE_MODE_NOTVISIBLE},
	{"personalize_settings"			, CPersonalizeGui::PERSONALIZE_PROTECT_MODE_NOT_PROTECTED},
	{"personalize_service"			, CPersonalizeGui::PERSONALIZE_PROTECT_MODE_NOT_PROTECTED},
	{"personalize_sleeptimer"		, CPersonalizeGui::PERSONALIZE_MODE_VISIBLE}, 
	{"personalize_standby"			, CPersonalizeGui::PERSONALIZE_MODE_VISIBLE},
	{"personalize_reboot"			, CPersonalizeGui::PERSONALIZE_MODE_VISIBLE},
	{"personalize_shutdown"			, CPersonalizeGui::PERSONALIZE_MODE_VISIBLE},
	{"personalize_infomenu_main"		, CPersonalizeGui::PERSONALIZE_MODE_VISIBLE},
	{"personalize_cisettings_main"		, CPersonalizeGui::PERSONALIZE_MODE_VISIBLE},
	
	//main menu->settings menu
	{"personalize_settingsmager"		, CPersonalizeGui::PERSONALIZE_MODE_VISIBLE},
	{"personalize_video"			, CPersonalizeGui::PERSONALIZE_MODE_VISIBLE},
	{"personalize_audio"			, CPersonalizeGui::PERSONALIZE_MODE_VISIBLE},
	{"personalize_network"			, CPersonalizeGui::PERSONALIZE_MODE_VISIBLE},
	{"personalize_recording"		, CPersonalizeGui::PERSONALIZE_MODE_VISIBLE},
	{"personalize_osdlang"			, CPersonalizeGui::PERSONALIZE_MODE_VISIBLE},
	{"personalize_osd"			, CPersonalizeGui::PERSONALIZE_MODE_VISIBLE},
	{"personalize_vfd"			, CPersonalizeGui::PERSONALIZE_MODE_VISIBLE},
	{"personalize_drives"			, CPersonalizeGui::PERSONALIZE_MODE_VISIBLE},
	{"personalize_cisettings_settings"	, CPersonalizeGui::PERSONALIZE_MODE_NOTVISIBLE},
	{"personalize_keybindings"		, CPersonalizeGui::PERSONALIZE_MODE_VISIBLE},
	{"personalize_mediaplayer"		, CPersonalizeGui::PERSONALIZE_MODE_VISIBLE},
	{"personalize_misc"			, CPersonalizeGui::PERSONALIZE_MODE_VISIBLE},
	
	//main menu->service menu
	{"personalize_tuner"			, CPersonalizeGui::PERSONALIZE_MODE_VISIBLE},
	{"personalize_scants"			, CPersonalizeGui::PERSONALIZE_MODE_VISIBLE},
	{"personalize_reload_channels"		, CPersonalizeGui::PERSONALIZE_MODE_VISIBLE},
	{"personalize_bouquet_edit"		, CPersonalizeGui::PERSONALIZE_MODE_VISIBLE},
	{"personalize_reset_channels"		, CPersonalizeGui::PERSONALIZE_MODE_VISIBLE}, 
	{"personalize_restart"			, CPersonalizeGui::PERSONALIZE_MODE_VISIBLE},
	{"personalize_reload_plugins"		, CPersonalizeGui::PERSONALIZE_MODE_VISIBLE},
	{"personalize_infomenu_service"		, CPersonalizeGui::PERSONALIZE_MODE_NOTVISIBLE},
	{"personalize_softupdate"		, CPersonalizeGui::PERSONALIZE_MODE_VISIBLE},
	
	//media menu
	{"personalize_media_menu"		, CPersonalizeGui::PERSONALIZE_PROTECT_MODE_NOT_PROTECTED},
	{"personalize_media_audio"		, CPersonalizeGui::PERSONALIZE_MODE_VISIBLE},
	{"personalize_media_intetplay"		, CPersonalizeGui::PERSONALIZE_MODE_VISIBLE},
	{"personalize_media_movieplayer"	, CPersonalizeGui::PERSONALIZE_MODE_VISIBLE},
	{"personalize_media_pviewer"		, CPersonalizeGui::PERSONALIZE_MODE_VISIBLE},
	{"personalize_media_upnp"		, CPersonalizeGui::PERSONALIZE_MODE_VISIBLE},
	
	//media menu->movieplayer
	{"personalize_mplayer_mbrowswer"	, CPersonalizeGui::PERSONALIZE_MODE_VISIBLE},
	{"personalize_mplayer_fileplay_video"	, CPersonalizeGui::PERSONALIZE_MODE_VISIBLE},
	{"personalize_mplayer_fileplay_audio"	, CPersonalizeGui::PERSONALIZE_MODE_VISIBLE},
	{"personalize_mplayer_ytplay"		, CPersonalizeGui::PERSONALIZE_MODE_VISIBLE},
	
	//key
	{"personalize_feat_key_fav"		, CPersonalizeGui::PERSONALIZE_FEAT_KEY_GREEN},
	{"personalize_feat_key_timerlist"	, CPersonalizeGui::PERSONALIZE_FEAT_KEY_YELLOW},
	{"personalize_feat_key_vtxt"		, CPersonalizeGui::PERSONALIZE_FEAT_KEY_BLUE},
	{"personalize_feat_key_rclock"		, CPersonalizeGui::PERSONALIZE_FEAT_KEY_AUTO},
	
	//user menu
	{"personalize_usermenu_show_cancel"	, true},

	//plugin types
	{"personalize_usermenu_plugin_type_games"	, false},
	{"personalize_usermenu_plugin_type_tools"	, true},
	{"personalize_usermenu_plugin_type_scripts"	, false},
	{"personalize_usermenu_plugin_type_lua"		, true},
};

CScanSettings::CScanSettings(void)
	: configfile('\t')
{
	bouquetMode     = CZapitClient::BM_UPDATEBOUQUETS;
	scanType = CServiceScan::SCAN_TVRADIO;
	satName = "none";
	cableName ="none";
}

bool CScanSettings::loadSettings(const char * const fileName)
{
	bool ret = configfile.loadConfig(fileName);

	bouquetMode = (CZapitClient::bouquetMode) configfile.getInt32("bouquetMode" , bouquetMode);
	scanType = (CZapitClient::scanType) configfile.getInt32("scanType", scanType);

	scan_fta_flag = configfile.getInt32("scan_fta_flag", 0);
	scan_nit = configfile.getInt32("scan_nit", 1);
	scan_nit_manual = configfile.getInt32("scan_nit_manual", 0);
	scan_bat = configfile.getInt32("scan_bat", 0);
	scan_reset_numbers = configfile.getInt32("scan_reset_numbers", 0);
	scan_logical_numbers = configfile.getInt32("scan_logical_numbers", 0);
	scan_logical_hd = configfile.getInt32("scan_logical_hd", 1);

	satName     = configfile.getString("satName", satName);
	sat_TP_fec  = configfile.getInt32("sat_TP_fec", 1);
	sat_TP_pol  = configfile.getInt32("sat_TP_pol", 0);
	sat_TP_freq = configfile.getString("sat_TP_freq", "10700000");
	sat_TP_rate = configfile.getString("sat_TP_rate", "27500000");
	sat_TP_mod = configfile.getInt32("sat_TP_mod", QPSK);
	sat_TP_delsys = configfile.getInt32("sat_TP_delsys", DVB_S);
	sat_TP_pilot = configfile.getInt32("sat_TP_pilot", ZPILOT_AUTO_SW);
	sat_TP_pli  = configfile.getString("sat_TP_pli", "0");
	sat_TP_plc  = configfile.getString("sat_TP_plc", "1");
	sat_TP_plm  = configfile.getInt32("sat_TP_plm", 0);

	cableName     = configfile.getString("cableName", cableName);
	cable_TP_mod  = configfile.getInt32("cable_TP_mod", QAM_64);
	cable_TP_fec  = configfile.getInt32("cable_TP_fec", 1);
	cable_TP_freq = configfile.getString("cable_TP_freq", "369000");
	cable_TP_rate = configfile.getString("cable_TP_rate", "6875000");
	cable_TP_delsys = configfile.getInt32("cable_TP_delsys", DVB_C);

	terrestrialName			= configfile.getString("terrestrialName", terrestrialName);
	terrestrial_TP_constel		= configfile.getInt32("terrestrial_TP_constel", QAM_AUTO);
	terrestrial_TP_bw		= configfile.getInt32("terrestrial_TP_bw", BANDWIDTH_AUTO);
	terrestrial_TP_coderate_HP	= configfile.getInt32("terrestrial_TP_coderate_HP", FEC_AUTO);
	terrestrial_TP_coderate_LP	= configfile.getInt32("terrestrial_TP_coderate_LP", FEC_AUTO);
	terrestrial_TP_freq		= configfile.getString("terrestrial_TP_freq", "369000");
	terrestrial_TP_guard		= configfile.getInt32("terrestrial_TP_guard", GUARD_INTERVAL_AUTO);
	terrestrial_TP_hierarchy	= configfile.getInt32("terrestrial_TP_hierarchy", HIERARCHY_AUTO);
	terrestrial_TP_transmit_mode	= configfile.getInt32("terrestrial_TP_transmit_mode", TRANSMISSION_MODE_AUTO);
	terrestrial_TP_delsys		= configfile.getInt32("terrestrial_TP_delsys", DVB_T);
	terrestrial_TP_pli		= configfile.getString("terrestrial_TP_pli", "0");

#if 1
	if(sat_TP_fec == 4) sat_TP_fec = 5;
#endif

	fast_type = configfile.getInt32("fast_type", 2);
	fast_op = configfile.getInt32("fast_op", 0);
	fst_version = configfile.getInt32("fst_version", 0);
	fst_update = configfile.getInt32("fst_update", 0);
	cable_nid = configfile.getInt32("cable_nid", 0);

	return ret;
}

bool CScanSettings::saveSettings(const char * const fileName)
{
	configfile.setInt32("bouquetMode", bouquetMode);
	configfile.setInt32("scanType", scanType);

	configfile.setInt32("scan_fta_flag", scan_fta_flag);
	configfile.setInt32("scan_nit", scan_nit);
	configfile.setInt32("scan_nit_manual", scan_nit_manual);
	configfile.setInt32("scan_bat", scan_bat);
	configfile.setInt32("scan_reset_numbers", scan_reset_numbers);
	configfile.setInt32("scan_logical_numbers", scan_logical_numbers);
	configfile.setInt32("scan_logical_hd", scan_logical_hd);
	configfile.setInt32("fast_type", fast_type);
	configfile.setInt32("fast_op", fast_op);
	configfile.setInt32("fst_version", fst_version);
	configfile.setInt32("fst_update", fst_update);
	configfile.setInt32("cable_nid", cable_nid);

	configfile.setString("satName", satName);
	configfile.setInt32("sat_TP_fec", sat_TP_fec);
	configfile.setInt32("sat_TP_pol", sat_TP_pol);
	configfile.setString("sat_TP_freq", sat_TP_freq);
	configfile.setString("sat_TP_rate", sat_TP_rate);
	configfile.setInt32("sat_TP_delsys", sat_TP_delsys);
	configfile.setInt32("sat_TP_mod", sat_TP_mod);
	configfile.setInt32("sat_TP_pilot", sat_TP_pilot);
	configfile.setString("sat_TP_pli", sat_TP_pli);
	configfile.setString("sat_TP_plc", sat_TP_plc);
	configfile.setInt32("sat_TP_plm", sat_TP_plm);

	configfile.setString("cableName", cableName);
	configfile.setInt32("cable_TP_fec", cable_TP_fec);
	configfile.setInt32("cable_TP_mod", cable_TP_mod);
	configfile.setString("cable_TP_freq", cable_TP_freq);
	configfile.setString("cable_TP_rate", cable_TP_rate);
	configfile.setInt32("cable_TP_delsys", cable_TP_delsys);

	configfile.setString("terrestrialName", terrestrialName);
	configfile.setInt32("terrestrial_TP_constel", terrestrial_TP_constel);
	configfile.setInt32("terrestrial_TP_bw", terrestrial_TP_bw);
	configfile.setInt32("terrestrial_TP_coderate_HP", terrestrial_TP_coderate_HP);
	configfile.setInt32("terrestrial_TP_coderate_LP", terrestrial_TP_coderate_LP);
	configfile.setString("terrestrial_TP_freq", terrestrial_TP_freq);
	configfile.setInt32("terrestrial_TP_hierarchy", terrestrial_TP_hierarchy);
	configfile.setInt32("terrestrial_TP_guard", terrestrial_TP_guard);
	configfile.setInt32("terrestrial_TP_transmit_mode", terrestrial_TP_transmit_mode);
	configfile.setInt32("terrestrial_TP_delsys", terrestrial_TP_delsys);
	configfile.setString("terrestrial_TP_pli", terrestrial_TP_pli);

	if(configfile.getModifiedFlag())
		configfile.saveConfig(fileName);

	return true;
}
