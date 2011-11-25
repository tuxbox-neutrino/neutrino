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
#include "gui/widget/menue.h"

//#define CPU_FREQ

class CMiscMenue : public CMenuTarget
{	
	private:
		CFanControlNotifier *fanNotifier;
		int width;

		int showMiscSettingsMenu();
		void showMiscSettingsMenuGeneral(CMenuWidget *ms_general);
		void showMiscSettingsMenuEnergy(CMenuWidget *ms_energy);
		void showMiscSettingsMenuEpg(CMenuWidget *ms_epg);
		void showMiscSettingsMenuFBrowser(CMenuWidget *ms_fbrowser);
		void showMiscSettingsMenuChanlist(CMenuWidget *ms_chanlist);
#ifdef CPU_FREQ
		void showMiscSettingsMenuCPUFreq(CMenuWidget *ms_cpu);
#endif /*CPU_FREQ*/
#if 0
		void showMiscSettingsMenuEmLog(CMenuWidget *ms_emlog);
#endif
	public:
		CMiscMenue();
		~CMiscMenue();
		int exec(CMenuTarget* parent, const std::string & actionKey);
		
};


#endif
