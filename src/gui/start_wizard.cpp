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
#include <zapit/zapit.h>
#include <system/helpers.h>

#include <gui/widget/messagebox.h>

#include <video.h>

extern cVideo * videoDecoder;
using namespace std;


CStartUpWizard::CStartUpWizard()
{
	
}

CStartUpWizard::~CStartUpWizard()
{
	
}

int CStartUpWizard::exec(CMenuTarget* parent, const string & /*actionKey*/)
{
	int res = menu_return::RETURN_REPAINT;
	showBackgroundLogo();

	if (parent)
		parent->hide();
	
	COsdLangSetup languageSettings(COsdLangSetup::OSDLANG_SETUP_MODE_WIZARD);
	
	languageSettings.exec(NULL, "");

#if 0
	if(ShowMsgUTF (LOCALE_WIZARD_WELCOME_HEAD, g_Locale->getText(LOCALE_WIZARD_WELCOME_TEXT), CMessageBox::mbrYes, CMessageBox::mbYes | CMessageBox::mbrCancel) == CMessageBox::mbrYes) 
#endif
	{
		//open video settings in wizardmode
		g_videoSettings->setWizardMode(CVideoSettings::V_SETUP_MODE_WIZARD);
		
		COsdSetup osdSettings(COsdSetup::OSD_SETUP_MODE_WIZARD);
		
		res = g_videoSettings->exec(NULL, "");
		g_videoSettings->setWizardMode(CVideoSettings::V_SETUP_MODE_WIZARD_NO);

		if(res != menu_return::RETURN_EXIT_ALL) 
		{
			res = osdSettings.exec(NULL, "");
		}
		if(res != menu_return::RETURN_EXIT_ALL) 
		{
			CNetworkSetup::getInstance()->setWizardMode(CNetworkSetup::N_SETUP_MODE_WIZARD);
			res = CNetworkSetup::getInstance()->exec(NULL, "");
			CNetworkSetup::getInstance()->setWizardMode(CNetworkSetup::N_SETUP_MODE_WIZARD_NO);
		}
		bool init_settings = false;
		if (CFEManager::getInstance()->haveSat())
			init_settings = file_exists(CONFIGDIR "/initial/");

		if(init_settings && (res != menu_return::RETURN_EXIT_ALL))
		{
			if (ShowMsgUTF(LOCALE_WIZARD_INITIAL_SETTINGS, g_Locale->getText(LOCALE_WIZARD_INSTALL_SETTINGS),
				CMessageBox::mbrYes, CMessageBox::mbYes | CMessageBox::mbNo, NULL, 450, 30, false) == CMessageBox::mbrYes) {
				system("/bin/cp " CONFIGDIR "/initial/* " CONFIGDIR "/zapit/");
				CFEManager::getInstance()->loadSettings();
				CFEManager::getInstance()->saveSettings();
				CZapit::getInstance()->PrepareChannels();
			}
		}
		if(res != menu_return::RETURN_EXIT_ALL) 
		{
			CScanSetup::getInstance()->setWizardMode(CScanSetup::SCAN_SETUP_MODE_WIZARD);
			res = CScanSetup::getInstance()->exec(NULL, "");
			CScanSetup::getInstance()->setWizardMode(CScanSetup::SCAN_SETUP_MODE_WIZARD_NO);
		}
	}
	
	killBackgroundLogo();
	return res;
}


inline void CStartUpWizard::showBackgroundLogo()
{
	videoDecoder->ShowPicture(DATADIR "/neutrino/icons/start.jpg");
}

inline void CStartUpWizard::killBackgroundLogo()
{
	videoDecoder->StopPicture();
}

