/*
	scan_setup menue - Neutrino-GUI

	Copyright (C) 2001 Steffen Hehn 'McClean'
	and some other guys
	Homepage: http://dbox.cyberphoria.org/

	Reworked as similar to $Id: scan_setup.h,v 1.6 2010/12/05 22:32:12 tuxbox-cvs Exp $
	Copyright (C) 2011 T. Graf 'dbt'
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

#ifndef __SCAN_SETUP__
#define __SCAN_SETUP__

#include <gui/widget/menue.h>
#include <gui/widget/icons.h>

#include <system/setting_helpers.h>
#include <system/settings.h>


#include <string>

#define scansettings CNeutrinoApp::getInstance()->getScanSettings()

//#define ENABLE_FASTSCAN //don't define this to remove fast scan menu

class CScanSetup : public CMenuTarget
{
	protected:
		int width;
	
	private:
		CSatelliteSetupNotifier 	*satNotify;
		CAllUsalsNotifier 		*usalsNotify;
		
		CMenuOptionStringChooser 	*satSelect;
		
		bool is_wizard;
		
		int selected;
		int dmode;
		int sfound;
		int fec_count;
		int freq_length;
		int r_system;
		bool allow_start;
		
		neutrino_locale_t satprov_locale;
		
		uint getSatMenuListWidth();
		
		int showScanMenu();
		
		void addScanMenuLnbSetup(CMenuWidget *sat_setup);
		void addScanMenuProvider(CMenuWidget *provider_setup);
		void addScanMenuSatFind(CMenuWidget *sat_findMenu);
 		void addScanMenuSatOnOff(CMenuWidget *sat_OnOff);
 		void addScanMenuTempSat(CMenuWidget *temp_sat);
 		void addScanMenuMotorMenu(CMenuWidget *motor_Menu);
 		void addScanMenuManualScan(CMenuWidget *manual_Scan);
 		void addScanMenuAutoScanAll(CMenuWidget *auto_ScanAll);
#ifdef ENABLE_FASTSCAN
 		void addScanMenuFastScan(CMenuWidget *fast_ScanMenu);
#endif
 		void addScanMenuAutoScan(CMenuWidget *auto_Scan);

		int addScanOptionsItems(CMenuWidget *options_menu, const int &shortcut = 1);
		int addListFlagsItems(CMenuWidget *listflags_menu, const int &shortcut = 1);
		
		void saveScanSetup();

	public:	
		enum SCAN_SETUP_MODE
		{
			SCAN_SETUP_MODE_WIZARD_NO   = 0,
			SCAN_SETUP_MODE_WIZARD   = 1
		};
		
		CScanSetup(bool wizard_mode = SCAN_SETUP_MODE_WIZARD_NO);
		~CScanSetup();
		
		static CScanSetup* getInstance();
		
		bool getWizardMode() {return is_wizard;};
		void setWizardMode(bool mode);

		int exec(CMenuTarget* parent, const std::string & actionKey = "");
};

class CTPSelectHandler : public CScanSetup
{
	public:
		int exec(CMenuTarget* parent,  const std::string &actionkey);
};


#endif
