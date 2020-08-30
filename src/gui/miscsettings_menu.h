/*
	$port: miscsettings_menu.h,v 1.2 2010/12/05 22:32:12 tuxbox-cvs Exp $

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

#ifndef __miscsettings_menu__
#define __miscsettings_menu__

#include <gui/widget/menue.h>

#include <system/settings.h>

#include <string>

class CMiscMenue : public CMenuTarget, CChangeObserver
{
	private:
		enum {
			DEUTSCHLAND = 0,
			NORWAY = 1,
		};
		CFanControlNotifier *fanNotifier;
		CSectionsdConfigNotifier* sectionsdConfigNotifier;
		//COnOffNotifier* miscNotifier;
		CMenuOptionChooser * epg_save;
		CMenuOptionChooser * epg_save_standby;
		CMenuOptionChooser * epg_save_frequently;
		CMenuOptionChooser * epg_read;
		CMenuOptionChooser * epg_read_frequently;
		CMenuOptionChooser * epg_scan;
		CMenuOptionChooser * weather_onoff;
		CMenuOptionChooser * tmdb_onoff;
		CMenuOptionChooser * omdb_onoff;
		CMenuOptionChooser * youtube_onoff;
		CMenuOptionChooser * shoutcast_onoff;
		CMenuForwarder * epg_dir;
		CMenuForwarder * epg_read_now;
		int width;
		std::string epg_cache;
		std::string epg_extendedcache;
		std::string epg_old_events;
		std::string epg_max_events;

		std::string weather_api_key_short;
		std::string tmdb_api_key_short;
		std::string omdb_api_key_short;
		std::string youtube_dev_id_short;
		std::string shoutcast_dev_id_short;

		int showMiscSettingsMenu();
		void showMiscSettingsMenuGeneral(CMenuWidget *ms_general);
		void showMiscSettingsMenuEpg(CMenuWidget *ms_epg);
		void showMiscSettingsMenuFBrowser(CMenuWidget *ms_fbrowser);
		int showMiscSettingsMenuEnergy();
		int showMiscSettingsMenuChanlist();
		int showMiscSettingsMenuOnlineServices();
		int showMiscSettingsSelectWeatherLocation();
		int showMiscSettingsMenuPlugins();
		void showMiscSettingsMenuCPUFreq(CMenuWidget *ms_cpu);
	public:
		CMiscMenue();
		~CMiscMenue();
		int exec(CMenuTarget* parent, const std::string & actionKey);
		bool changeNotify(const neutrino_locale_t OptionName, void * /*data*/);
};

#endif
