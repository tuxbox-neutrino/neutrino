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
#include <gui/widget/stringinput_ext.h>

#include <system/setting_helpers.h>
#include <system/settings.h>
#include <zapit/satconfig.h>

#include <string>

#define scansettings CNeutrinoApp::getInstance()->getScanSettings()

class CScanSetup : public CMenuTarget, public CChangeObserver
{
	protected:
		int width;
	
	private:
		CMenuWidget			*satOnOff;

		/* global items to be enabled/disabled in notify */
		CMenuForwarder  *fautoScanAll;
		CMenuForwarder  *frontendSetup;
		CMenuForwarder  *fsatSetup;
		CMenuForwarder  *fsatSelect;
		CMenuOptionChooser * dtype;
		CMenuOptionChooser * dorder;
		CMenuOptionChooser * tsp;
		CMenuForwarder  *uniSetup;
		CMenuOptionNumberChooser * ojDiseqcRepeats;
		CIntInput * nid;
		CMenuOptionChooser * lcnhd;
		/* items active for master/independent fe mode */
		CGenericMenuActivate msettings;

		CMenuOptionChooser * linkfe;
#if BOXMODEL_VUULTIMO4K
		std::string modestr[24];
#else
#if BOXMODEL_VUSOLO4K || BOXMODEL_VUDUO4K || BOXMODEL_VUUNO4KSE
		std::string modestr[16];
#else
		std::string modestr[4];
#endif
#endif

		/* variables for selected frontend */
		/* diseqc mode */
		int dmode;
		/* frontend setup menu current fe number */
		int fenumber;
		/* frontend mode */
		int femode;
		/* frontend link to number */
		int femaster;

		std::vector<std::string> satoptions;
		std::vector<CMenuForwarder*> satmf;

		/* flag to allow any operations which can damage recordings */
		bool allow_start;
		/* flag to re-init frontends */
		bool fe_restart;
		/* flag to skip manual params update while in menu */
		bool in_menu;

		int is_wizard;
		
		int r_system;

		neutrino_locale_t satprov_locale;

		uint getSatMenuListWidth();

		int showScanMenu();

		int showFrontendSetup(int number);
		int showFrontendSelect(int number);
		int showScanMenuLnbSetup();
		int showUnicableSetup();
		int showScanMenuSatFind();
		void fillSatSelect(CMenuOptionStringChooser *select);
		void fillCableSelect(CMenuOptionStringChooser *select);
		void fillTerrSelect(CMenuOptionStringChooser *select);

		neutrino_locale_t getModeLocale(int mode);
		int showScanMenuFrontendSetup();
 		void addScanMenuTempSat(CMenuWidget *temp_sat, sat_config_t &satconfig);
 		void addScanMenuManualScan(CMenuWidget *manual_Scan, bool stest = false);
 		void addScanMenuAutoScanAll(CMenuWidget *auto_ScanAll);
 		void addScanMenuAutoScan(CMenuWidget *auto_Scan);

		int addScanOptionsItems(CMenuWidget *options_menu, const int &shortcut = 1);
		int addListFlagsItems(CMenuWidget *listflags_menu, const int &shortcut = 1, bool manual = false);
#ifdef ENABLE_FASTSCAN
		int showFastscanDiseqcSetup();
#endif
		void setDiseqcOptions(int number);

		void saveScanSetup();

		CScanSetup(int wizard_mode = SNeutrinoSettings::WIZARD_OFF);

		/* required to set display count of selected satellites, see: showFrontendSetup() */
		void setOptionSatSelect(int fe_number, CMenuForwarder* menu_item);
	public:	
		~CScanSetup();

		static CScanSetup* getInstance();

		void setWizardMode(int mode) {is_wizard = mode;};
		void updateManualSettings();

		int exec(CMenuTarget* parent, const std::string & actionKey = "");
		bool changeNotify(const neutrino_locale_t OptionName, void * /*data*/);
#ifdef ENABLE_FASTSCAN
 		void addScanMenuFastScan(CMenuWidget *fast_ScanMenu);
#endif
		void addScanMenuCable(CMenuWidget *menu);
};

class CTPSelectHandler : public CMenuTarget //CScanSetup
{
	public:
		int exec(CMenuTarget* parent,  const std::string &actionkey);
};
#endif
