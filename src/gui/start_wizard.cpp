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
#if ENABLE_PKG_MANAGEMENT
#include "opkg_manager.h"
#endif
#include <zapit/zapit.h>
#include <system/helpers.h>

#include "widget/msgbox.h"

#include <hardware/video.h>

extern cVideo *videoDecoder;
extern Zapit_config zapitCfg;
using namespace std;

CStartUpWizard::CStartUpWizard()
{
}

CStartUpWizard::~CStartUpWizard()
{
}

int CStartUpWizard::exec(CMenuTarget *parent, const string &/*actionKey*/)
{
	// remove menu-timeout during wizard
	int default_timing_menu = g_settings.timing[SNeutrinoSettings::TIMING_MENU];
	g_settings.timing[SNeutrinoSettings::TIMING_MENU] = 0;
	//printf("[neutrino] Removing 'timing.menu' during wizard ...\n");

	int res = menu_return::RETURN_REPAINT;
	showBackgroundLogo();

	if (parent)
		parent->hide();

	// language setup
	COsdLangSetup languageSettings(SNeutrinoSettings::WIZARD_START);
	CMenuWidget osdl_setup(LOCALE_LANGUAGESETUP_OSD, NEUTRINO_ICON_LANGUAGE, 45, MN_WIDGET_ID_LANGUAGESETUP_LOCALE);
	osdl_setup.setWizardMode(true);
	languageSettings.showLanguageSetup(&osdl_setup);
	osdl_setup.exec(NULL, "");

	// hack to ensure system's view of timezone is the same as neutrino's
	CTZChangeNotifier tzn;
	tzn.changeNotify(NONEXISTANT_LOCALE, (void *)"dummy");

	// restore backup
	CSettingsManager settingsManager(SNeutrinoSettings::WIZARD_START);
	settingsManager.exec(NULL, "");

	//if (ShowMsg(LOCALE_WIZARD_WELCOME_HEAD, g_Locale->getText(LOCALE_WIZARD_WELCOME_TEXT), CMsgBox::mbrYes, CMsgBox::mbYes | CMsgBox::mbrCancel) == CMsgBox::mbrYes)
	{
		// open video settings in wizardmode
		if (res != menu_return::RETURN_EXIT_ALL)
		{
			g_videoSettings->setWizardMode(SNeutrinoSettings::WIZARD_ON);
			res = g_videoSettings->exec(NULL, "");
			g_videoSettings->setWizardMode(SNeutrinoSettings::WIZARD_OFF);
		}

		if (res != menu_return::RETURN_EXIT_ALL)
		{
			COsdSetup osdSettings(SNeutrinoSettings::WIZARD_ON);
			res = osdSettings.exec(NULL, "");
		}

		if (res != menu_return::RETURN_EXIT_ALL)
		{
			CNetworkSetup::getInstance()->setWizardMode(SNeutrinoSettings::WIZARD_ON);
			res = CNetworkSetup::getInstance()->exec(NULL, "");
			CNetworkSetup::getInstance()->setWizardMode(SNeutrinoSettings::WIZARD_OFF);
		}

#if ENABLE_PKG_MANAGEMENT
		// package update check
		if (res != menu_return::RETURN_EXIT_ALL)
		{
			COPKGManager man(SNeutrinoSettings::WIZARD_START);
			if (man.hasOpkgSupport())
			{
				int msg = ShowMsg(LOCALE_WIZARD_MESSAGE_CHECK_FOR_UPDATES, LOCALE_WIZARD_MESSAGE_CHECK_FOR_UPDATES_ASK_NOW, CMsgBox::mbrYes, CMsgBox::mbYes | CMsgBox::mbNo, NULL, 450);
				if (msg == CMsgBox::mbrYes)
					res = man.exec(NULL, "");
				msg = ShowMsg(LOCALE_WIZARD_MESSAGE_CHECK_FOR_UPDATES, LOCALE_WIZARD_MESSAGE_CHECK_FOR_UPDATES_ASK_ENABLE, CMsgBox::mbrYes, CMsgBox::mbYes | CMsgBox::mbNo, NULL, 450);
				if (msg == CMsgBox::mbrYes)
					g_settings.softupdate_autocheck_packages = true;
			}
		}
#endif

		bool init_settings = false;
		if (CFEManager::getInstance()->haveSat())
			init_settings = file_exists(CONFIGDIR "/initial/");

		if (init_settings && res != menu_return::RETURN_EXIT_ALL)
		{
			if (ShowMsg(LOCALE_WIZARD_INITIAL_SETTINGS, g_Locale->getText(LOCALE_WIZARD_INSTALL_SETTINGS),
					CMsgBox::mbrYes, CMsgBox::mbYes | CMsgBox::mbNo, NULL, 450, 30) == CMsgBox::mbrYes)
			{
				system("/bin/cp " CONFIGDIR "/initial/* " ZAPITDIR);
				CFEManager::getInstance()->loadSettings();
				CFEManager::getInstance()->saveSettings();
				CZapit::getInstance()->PrepareChannels();
			}
		}
		if (res != menu_return::RETURN_EXIT_ALL)
		{
			CScanSetup::getInstance()->setWizardMode(SNeutrinoSettings::WIZARD_ON);
			res = CScanSetup::getInstance()->exec(NULL, "setup_frontend");
			if (res != menu_return::RETURN_EXIT_ALL)
				res = CScanSetup::getInstance()->exec(NULL, "");
			CScanSetup::getInstance()->setWizardMode(SNeutrinoSettings::WIZARD_OFF);
		}
	}

	// reset menu-timeout to our default if user doesn't change the value
	if (g_settings.timing[SNeutrinoSettings::TIMING_MENU] == 0)
	{
		g_settings.timing[SNeutrinoSettings::TIMING_MENU] = default_timing_menu;
		//printf("[neutrino] Set 'timing.menu' to default...\n");
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
