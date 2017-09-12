/*
	Neutrino-GUI  -  Tuxbox-Project
	Copyright (C) 2001 Steffen Hehn 'McClean'
	http://www.tuxbox.org
		
	Startup wizard
	based upon an implementation created by 
	Copyright (C) 2009 CoolStream International Ltd.
      
	Reworked by dbt (Thilo Graf)
	Copyright (C) 2012 dbt
	http://www.dbox2-tuning.net

	License: GPL

	This software is free software; you can redistribute it and/or
	modify it under the terms of the GNU General Public
	License as published by the Free Software Foundation; either
	version 2 of the License, or (at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
	Library General Public License for more details.

	You should have received a copy of the GNU General Public
	License along with this program; if not, write to the
	Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
	Boston, MA  02110-1301, USA.
	
	NOTE for ignorant distributors:
	It's not allowed to distribute any compiled parts of this code, if you don't accept the terms of GPL.
	Please read it and understand it right!
	This means for you: Hold it, if not, leave it! You could face legal action! 
	Otherwise ask the copyright owners, anything else would be theft!
*/


#include "start_wizard.h"

#include <global.h>
#include <neutrino.h>
#include <neutrino_menue.h>

#include "network_setup.h"
#include "osd_setup.h"
#include "osdlang_setup.h"
#include "scan_setup.h"
#include "settings_manager.h"
#include "videosettings.h"
#include <zapit/zapit.h>
#include <system/helpers.h>

#include <gui/widget/msgbox.h>

#include <video.h>

extern cVideo * videoDecoder;
extern Zapit_config zapitCfg;
using namespace std;

CStartUpWizard::CStartUpWizard()
{
}

CStartUpWizard::~CStartUpWizard()
{
}

const CMenuOptionChooser::keyval WIZARD_SETUP_TYPE[] =
{
	{ 0, LOCALE_WIZARD_SETUP_EASY },
	{ 1, LOCALE_WIZARD_SETUP_ADVANCED },
};

int CStartUpWizard::exec(CMenuTarget* parent, const string & /*actionKey*/)
{
	int res = menu_return::RETURN_REPAINT;
	showBackgroundLogo();

	if (parent)
		parent->hide();

        //language setup
	COsdLangSetup languageSettings(SNeutrinoSettings::WIZARD_START);
	CMenuWidget osdl_setup(LOCALE_LANGUAGESETUP_OSD, NEUTRINO_ICON_LANGUAGE, 45, MN_WIDGET_ID_LANGUAGESETUP_LOCALE);
	osdl_setup.setWizardMode(true);
	languageSettings.showLanguageSetup(&osdl_setup);
	osdl_setup.exec(NULL, "");

	//restore backup
	CSettingsManager settingsManager(SNeutrinoSettings::WIZARD_START);
	settingsManager.exec(NULL, "");

	if(ShowMsg (LOCALE_WIZARD_WELCOME_HEAD, g_Locale->getText(LOCALE_WIZARD_WELCOME_TEXT), CMsgBox::mbrYes, CMsgBox::mbYes | CMsgBox::mbrCancel) == CMsgBox::mbrYes)
	{
		int advanced = 1;
#ifdef ENABLE_FASTSCAN
		std::string lang = g_settings.language;
		if (lang == "nederlands") {
			advanced = 0;
			CMenuWidget wtype(LOCALE_WIZARD_SETUP);
			wtype.setWizardMode(SNeutrinoSettings::WIZARD_ON);
			wtype.addIntroItems();
			CMenuOptionChooser * mc = new CMenuOptionChooser(LOCALE_WIZARD_SETUP_TYPE, &advanced, WIZARD_SETUP_TYPE, 2, true, NULL);
			mc->setHint("", LOCALE_WIZARD_SETUP_TYPE_HINT);
			wtype.addItem(mc);
			wtype.exec(NULL, "");
		}
#endif
		//open video settings in wizardmode
		if(advanced && res != menu_return::RETURN_EXIT_ALL) {
			g_videoSettings->setWizardMode(SNeutrinoSettings::WIZARD_ON);
			res = g_videoSettings->exec(NULL, "");
			g_videoSettings->setWizardMode(SNeutrinoSettings::WIZARD_OFF);
		}
		if(advanced && res != menu_return::RETURN_EXIT_ALL)
		{
			COsdSetup osdSettings(SNeutrinoSettings::WIZARD_ON);
			res = osdSettings.exec(NULL, "");
		}
		if(advanced && res != menu_return::RETURN_EXIT_ALL)
		{
			CNetworkSetup::getInstance()->setWizardMode(SNeutrinoSettings::WIZARD_ON);
			res = CNetworkSetup::getInstance()->exec(NULL, "");
			CNetworkSetup::getInstance()->setWizardMode(SNeutrinoSettings::WIZARD_OFF);
		}
		bool init_settings = false;
		if (CFEManager::getInstance()->haveSat())
			init_settings = file_exists(CONFIGDIR "/initial/");

		if(advanced && init_settings && (res != menu_return::RETURN_EXIT_ALL))
		{
			if (ShowMsg(LOCALE_WIZARD_INITIAL_SETTINGS, g_Locale->getText(LOCALE_WIZARD_INSTALL_SETTINGS),
				CMsgBox::mbrYes, CMsgBox::mbYes | CMsgBox::mbNo, NULL, 450, 30) == CMsgBox::mbrYes) {
				system("/bin/cp " CONFIGDIR "/initial/* " CONFIGDIR "/zapit/");
				CFEManager::getInstance()->loadSettings();
				CFEManager::getInstance()->saveSettings();
				CZapit::getInstance()->PrepareChannels();
			}
		}
		if(res != menu_return::RETURN_EXIT_ALL)
		{
			CScanSetup::getInstance()->setWizardMode(SNeutrinoSettings::WIZARD_ON);
			if (advanced) {
				res = CScanSetup::getInstance()->exec(NULL, "setup_frontend");
				if(res != menu_return::RETURN_EXIT_ALL)
					res = CScanSetup::getInstance()->exec(NULL, "");
			} else {
				CZapit::getInstance()->GetConfig(zapitCfg);
#ifdef ENABLE_FASTSCAN
				if (CFEManager::getInstance()->haveSat()) {
					CMenuWidget fastScanMenu(LOCALE_SATSETUP_FASTSCAN_HEAD, NEUTRINO_ICON_SETTINGS, 45, MN_WIDGET_ID_SCAN_FAST_SCAN);
					fastScanMenu.setWizardMode(SNeutrinoSettings::WIZARD_ON);
					CScanSetup::getInstance()->addScanMenuFastScan(&fastScanMenu);
					res = fastScanMenu.exec(NULL, "");
				}
#endif
				if (CFEManager::getInstance()->haveCable()) {
					CMenuWidget cableScan(LOCALE_SATSETUP_CABLE, NEUTRINO_ICON_SETTINGS, 45, MN_WIDGET_ID_SCAN_CABLE_SCAN);
					cableScan.setWizardMode(SNeutrinoSettings::WIZARD_ON);
					CScanSetup::getInstance()->addScanMenuCable(&cableScan);
					res = cableScan.exec(NULL, "");
				}
			}
			CScanSetup::getInstance()->setWizardMode(SNeutrinoSettings::WIZARD_OFF);
		}
	}

	killBackgroundLogo();
	return res;
}

inline void CStartUpWizard::showBackgroundLogo()
{
	CFrameBuffer::getInstance()->showFrame("start.jpg");
}

inline void CStartUpWizard::killBackgroundLogo()
{
	CFrameBuffer::getInstance()->stopFrame();
}

