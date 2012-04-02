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
	{"personalize_redbutton"		, CPersonalizeGui::PERSONALIZE_ACTIVE_MODE_ENABLED}, // epg/info
	
	//main menu
	{"personalize_tv_mode"			, CPersonalizeGui::PERSONALIZE_MODE_VISIBLE}, 
	{"personalize_tv_radio_mode"		, CPersonalizeGui::PERSONALIZE_MODE_NOTVISIBLE}, //toggle
	{"personalize_radio_mode"		, CPersonalizeGui::PERSONALIZE_MODE_VISIBLE}, 
	{"personalize_timer"			, CPersonalizeGui::PERSONALIZE_MODE_VISIBLE}, 
	{"personalize_media"			, CPersonalizeGui::PERSONALIZE_MODE_VISIBLE}, 
	
	{"personalize_games"			, CPersonalizeGui::PERSONALIZE_MODE_NOTVISIBLE},	
	{"personalize_scripts"			, CPersonalizeGui::PERSONALIZE_MODE_NOTVISIBLE},
	{"personalize_settings"			, CPersonalizeGui::PERSONALIZE_PROTECT_MODE_NOT_PROTECTED},
	{"personalize_service"			, CPersonalizeGui::PERSONALIZE_PROTECT_MODE_NOT_PROTECTED},
	{"personalize_sleeptimer"		, CPersonalizeGui::PERSONALIZE_MODE_VISIBLE}, 
	{"personalize_reboot"			, CPersonalizeGui::PERSONALIZE_MODE_VISIBLE},
	{"personalize_shutdown"			, CPersonalizeGui::PERSONALIZE_MODE_VISIBLE},
	{"personalize_infomenu_main"		, CPersonalizeGui::PERSONALIZE_MODE_VISIBLE},
	{"personalize_cisettings_main"		, CPersonalizeGui::PERSONALIZE_MODE_VISIBLE},
	
	//main menu->settings menu
	{"personalize_settingsmager"		, CPersonalizeGui::PERSONALIZE_PROTECT_MODE_NOT_PROTECTED},
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
	{"personalize_media_movieplayer"	, CPersonalizeGui::PERSONALIZE_PROTECT_MODE_NOT_PROTECTED},
	{"personalize_media_pviewer"		, CPersonalizeGui::PERSONALIZE_MODE_VISIBLE},
	{"personalize_media_upnp"		, CPersonalizeGui::PERSONALIZE_MODE_VISIBLE},
	
	//media menu->movieplayer
	{"personalize_mplayer_mbrowswer"	, CPersonalizeGui::PERSONALIZE_MODE_VISIBLE},
	{"personalize_mplayer_fileplay"		, CPersonalizeGui::PERSONALIZE_MODE_VISIBLE},
	
	//key
	{"personalize_feat_key_fav"		, CPersonalizeGui::PERSONALIZE_FEAT_KEY_GREEN},
	{"personalize_feat_key_timerlist"	, CPersonalizeGui::PERSONALIZE_FEAT_KEY_YELLOW},
	{"personalize_feat_key_vtxt"		, CPersonalizeGui::PERSONALIZE_FEAT_KEY_BLUE},
	{"personalize_feat_key_rclock"		, CPersonalizeGui::PERSONALIZE_FEAT_KEY_AUTO},
	
	//user menu
	{"personalize_usermenu_show_cancel"	, true},
};

CScanSettings::CScanSettings(void)
	: configfile('\t')
{
	delivery_system = DVB_S;
	bouquetMode     = CZapitClient::BM_UPDATEBOUQUETS;
	scanType = CServiceScan::SCAN_TVRADIO;
	strcpy(satNameNoDiseqc, "none");
}

bool CScanSettings::loadSettings(const char * const fileName, const delivery_system_t dsys)
{
	bool ret = configfile.loadConfig(fileName);

	if (configfile.getInt32("delivery_system", -1) != dsys)
	{
		// configfile is not for this delivery system
		configfile.clear();
		delivery_system = dsys;
		ret = false;
	}

	bouquetMode = (CZapitClient::bouquetMode) configfile.getInt32("bouquetMode" , bouquetMode);
	scanType = (CZapitClient::scanType) configfile.getInt32("scanType", scanType);
	strcpy(satNameNoDiseqc, configfile.getString("satNameNoDiseqc", satNameNoDiseqc).c_str());

	scan_fta_flag = configfile.getInt32("scan_fta_flag", 0);
	scan_nit = configfile.getInt32("scan_nit", 1);
	scan_nit_manual = configfile.getInt32("scan_nit_manual", 0);
	scan_bat = configfile.getInt32("scan_bat", 0);
	scan_reset_numbers = configfile.getInt32("scan_reset_numbers", 0);
	scan_logical_numbers = configfile.getInt32("scan_logical_numbers", 0);
	TP_fec = configfile.getInt32("TP_fec", 1);
	TP_pol = configfile.getInt32("TP_pol", 0);
	TP_mod = configfile.getInt32("TP_mod", 3);

	strcpy(TP_freq, configfile.getString("TP_freq", "10100000").c_str());
	strcpy(TP_rate, configfile.getString("TP_rate", "27500000").c_str());
#if 1
	if(TP_fec == 4) TP_fec = 5;
#endif
	fast_type = configfile.getInt32("fast_type", 1);
	fast_op = configfile.getInt32("fast_op", 0);
	cable_nid = configfile.getInt32("cable_nid", 0);

	return ret;
}

bool CScanSettings::saveSettings(const char * const fileName)
{
	configfile.setInt32("delivery_system", delivery_system);
	configfile.setInt32("bouquetMode", bouquetMode);
	configfile.setInt32("scanType", scanType);
	configfile.setString("satNameNoDiseqc", satNameNoDiseqc);

	configfile.setInt32("scan_fta_flag", scan_fta_flag);
	configfile.setInt32("scan_nit", scan_nit);
	configfile.setInt32("scan_nit_manual", scan_nit_manual);
	configfile.setInt32("scan_bat", scan_bat);
	configfile.setInt32("scan_reset_numbers", scan_reset_numbers);
	configfile.setInt32("scan_logical_numbers", scan_logical_numbers);
	configfile.setInt32("TP_fec", TP_fec);
	configfile.setInt32("TP_pol", TP_pol);
	configfile.setInt32("TP_mod", TP_mod);
	configfile.setString("TP_freq", TP_freq);
	configfile.setString("TP_rate", TP_rate);
	configfile.setInt32("fast_type", fast_type);
	configfile.setInt32("fast_op", fast_op);
	configfile.setInt32("cable_nid", fast_op);

	if(configfile.getModifiedFlag())
		configfile.saveConfig(fileName);

	return true;
}
