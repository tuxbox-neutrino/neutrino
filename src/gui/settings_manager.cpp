/*
	settings mangaer menue - Neutrino-GUI

	Copyright (C) 2001 Steffen Hehn 'McClean'
	and some other guys
	Homepage: http://dbox.cyberphoria.org/

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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <global.h>
#include <neutrino.h>
#include <neutrino_menue.h>

#include <gui/settings_manager.h>
#include <gui/filebrowser.h>
#include <gui/widget/msgbox.h>
#include <gui/widget/stringinput.h>
#include <gui/widget/keyboard_input.h>

#include <driver/display.h>
#include <driver/screen_max.h>
#include <driver/display.h>
#include <system/helpers.h>
#include <system/debug.h>

#include <sys/vfs.h>


CSettingsManager::CSettingsManager(int wizard_mode)
{
	width = 40;
	is_wizard = wizard_mode;
}


CSettingsManager::~CSettingsManager()
{
}

int CSettingsManager::exec(CMenuTarget* parent, const std::string &actionKey)
{
	dprintf(DEBUG_NORMAL, "[CSettingsManager]\t[%s - %d] actionKey = [%s]\n", __func__, __LINE__, actionKey.c_str());
	int   res = menu_return::RETURN_REPAINT;

	if (parent)
		parent->hide();

	CFileBrowser fileBrowser;
	CFileFilter fileFilter;

	if(actionKey == "loadconfig")
	{
		fileFilter.addFilter("conf");
		fileBrowser.Filter = &fileFilter;
		if (fileBrowser.exec(g_settings.backup_dir.c_str()) == true)
		{
			g_settings.backup_dir = fileBrowser.getCurrentDir();
			std::string new_config = fileBrowser.getSelectedFile()->Name.c_str();
			CNeutrinoApp::getInstance()->loadSetup(new_config.c_str());
			CColorSetupNotifier *colorSetupNotifier = new CColorSetupNotifier;
			colorSetupNotifier->changeNotify(NONEXISTANT_LOCALE, NULL);
			CNeutrinoApp::getInstance()->SetupFonts(CNeutrinoFonts::FONTSETUP_ALL);
			CVFD::getInstance()->setlcdparameter();
			dprintf(DEBUG_NORMAL, "[CSettingsManager]\t[%s - %d] load config from %s\n", __func__, __LINE__, new_config.c_str());
			delete colorSetupNotifier;
		}
		return res;
	}
	else if(actionKey == "saveconfig")
	{
		char msgtxt[1024];
		snprintf(msgtxt, sizeof(msgtxt), g_Locale->getText(LOCALE_SETTINGS_BACKUP_DIR), g_settings.backup_dir.c_str());

		int result = ShowMsg(LOCALE_EXTRA_SAVECONFIG, msgtxt, CMsgBox::mbrYes, CMsgBox::mbYes | CMsgBox::mbNo);
		if (result == CMsgBox::mbrNo)
		{
			fileBrowser.Dir_Mode = true;
			if (fileBrowser.exec(g_settings.backup_dir.c_str()) == true)
				g_settings.backup_dir = fileBrowser.getSelectedFile()->Name;
			else
				return res;
		}

		std::string fname = "neutrino.conf";
		CKeyboardInput * sms = new CKeyboardInput(LOCALE_EXTRA_SAVECONFIG, &fname);
		sms->exec(NULL, "");
		delete sms;

		std::string sname = g_settings.backup_dir + "/" + fname;
		dprintf(DEBUG_NORMAL, "[CSettingsManager]\t[%s - %d] save neutrino settings to %s\n", __func__, __LINE__, sname.c_str());

		CNeutrinoApp::getInstance()->saveSetup(sname.c_str());

		return res;
	}
	else if(actionKey == "backup")
	{
		char msgtxt[1024];
		snprintf(msgtxt, sizeof(msgtxt), g_Locale->getText(LOCALE_SETTINGS_BACKUP_DIR), g_settings.backup_dir.c_str());

		int result = ShowMsg(LOCALE_SETTINGS_BACKUP, msgtxt, CMsgBox::mbrYes, CMsgBox::mbYes | CMsgBox::mbNo);
		if (result == CMsgBox::mbrNo)
		{
			fileBrowser.Dir_Mode = true;
			if (fileBrowser.exec(g_settings.backup_dir.c_str()) == true)
				g_settings.backup_dir = fileBrowser.getSelectedFile()->Name;
			else
				return res;
		}

		struct statfs s;
		int ret = ::statfs(g_settings.backup_dir.c_str(), &s);
		if (ret == 0 && s.f_type != 0x72b6L) /*jffs2*/
		{
			CHintBox * hintBox = new CHintBox(LOCALE_MESSAGEBOX_INFO, g_Locale->getText(LOCALE_SETTINGS_BACKUP));
			hintBox->paint();

			const char backup_sh[] = TARGET_PREFIX "/bin/backup.sh";
			dprintf(DEBUG_NORMAL, "[CSettingsManager]\t[%s - %d] executing [%s %s]\n", __func__, __LINE__, backup_sh, g_settings.backup_dir.c_str());
			my_system(2, backup_sh, g_settings.backup_dir.c_str());

			hintBox->hide();
			delete hintBox;
		}
		else
			ShowMsg(LOCALE_MESSAGEBOX_ERROR, g_Locale->getText(LOCALE_SETTINGS_BACKUP_FAILED),CMsgBox::mbrBack, CMsgBox::mbBack, NEUTRINO_ICON_ERROR);

		return res;
	}
	else if(actionKey == "restore")
	{
		fileFilter.addFilter("tar");
		fileBrowser.Filter = &fileFilter;
		if (fileBrowser.exec(g_settings.backup_dir.c_str()) == true)
		{
			g_settings.backup_dir = fileBrowser.getCurrentDir();
			int result = ShowMsg(LOCALE_SETTINGS_RESTORE, g_Locale->getText(LOCALE_SETTINGS_RESTORE_WARN), CMsgBox::mbrNo, CMsgBox::mbYes | CMsgBox::mbNo);
			if(result == CMsgBox::mbrYes)
			{
				const char restore_sh[] = TARGET_PREFIX "/bin/restore.sh";
				std::string restore_file = fileBrowser.getSelectedFile()->Name;
				dprintf(DEBUG_NORMAL, "[CSettingsManager]\t[%s - %d] executing [%s %s]\n", __func__, __LINE__, restore_sh, restore_file.c_str());
				my_system(2, restore_sh, restore_file.c_str());
			}
		}
		return res;
	}

	res = is_wizard ? showMenu_wizard() : showMenu();

	return res;
}

//use a own small menu for start_wizard, because i don't want to fiddle around the easymenu code
int CSettingsManager::showMenu_wizard()
{
	printf("[neutrino] CSettingsManager call %s...\n", __FUNCTION__);

	CMenuWidget * mset = new CMenuWidget(LOCALE_MAINSETTINGS_HEAD, NEUTRINO_ICON_SETTINGS, width, MN_WIDGET_ID_SETTINGS_MNGR);
	mset->setWizardMode(is_wizard);
	mset->addIntroItems(LOCALE_MAINSETTINGS_MANAGE);

	CMenuForwarder * mf;

	mf = new CMenuForwarder(LOCALE_EXTRA_LOADCONFIG, true, NULL, this, "loadconfig", CRCInput::RC_red);
	mf->setHint(NEUTRINO_ICON_HINT_LOAD, LOCALE_MENU_HINT_LOAD);
	mset->addItem(mf);

	mf = new CMenuForwarder(LOCALE_SETTINGS_RESTORE, true, NULL, this, "restore", CRCInput::RC_green);
	mf->setHint(NEUTRINO_ICON_HINT_BACKUP, LOCALE_MENU_HINT_BACKUP);
	mset->addItem(mf);

	int res = mset->exec(NULL, "");
	delete mset;
	return res;
}

int CSettingsManager::showMenu()
{
	printf("[neutrino] CSettingsManager call %s...\n", __FUNCTION__);

	CDataResetNotifier * resetNotifier = new CDataResetNotifier();

	CMenuWidget * mset = new CMenuWidget(LOCALE_MAINSETTINGS_HEAD, NEUTRINO_ICON_SETTINGS, width, MN_WIDGET_ID_SETTINGS_MNGR);
	mset->addIntroItems(LOCALE_MAINSETTINGS_MANAGE);

	CMenuForwarder * mf;
	mf = new CMenuForwarder(LOCALE_RESET_SETTINGS,   true, NULL, resetNotifier,    "settings",     CRCInput::RC_recall);

	mf->setHint(NEUTRINO_ICON_HINT_RESET, LOCALE_MENU_HINT_RESET); // FIXME: RC-button RECALL is broken
	mset->addItem(mf);

	mset->addItem(GenericMenuSeparatorLine);

	mf = new CMenuForwarder(LOCALE_EXTRA_SAVECONFIG, true, NULL, this, "saveconfig", CRCInput::RC_red);
	mf->setHint(NEUTRINO_ICON_HINT_SAVEAS, LOCALE_MENU_HINT_SAVEAS);
	mset->addItem(mf);

	mf = new CMenuForwarder(LOCALE_EXTRA_LOADCONFIG, true, NULL, this, "loadconfig", CRCInput::RC_green);
	mf->setHint(NEUTRINO_ICON_HINT_LOAD, LOCALE_MENU_HINT_LOAD);
	mset->addItem(mf);

	mset->addItem(GenericMenuSeparatorLine);

	mf = new CMenuForwarder(LOCALE_SETTINGS_BACKUP, true, NULL, this, "backup",  CRCInput::RC_yellow);

	mf->setHint(NEUTRINO_ICON_HINT_BACKUP, LOCALE_MENU_HINT_BACKUP);
	mset->addItem(mf);

	mf = new CMenuForwarder(LOCALE_SETTINGS_RESTORE, true, NULL, this, "restore", CRCInput::RC_blue);

	mf->setHint(NEUTRINO_ICON_HINT_RESTORE, LOCALE_MENU_HINT_RESTORE);
	mset->addItem(mf);

	mset->addItem(GenericMenuSeparatorLine);
	mf = new CMenuForwarder(LOCALE_RESET_ALL, true, NULL, resetNotifier, "all", CRCInput::RC_standby);
	mf->setHint(NEUTRINO_ICON_HINT_FACTORY, LOCALE_MENU_HINT_FACTORY);
	mset->addItem(mf);

	int res = mset->exec(NULL, "");
	delete resetNotifier;
	delete mset;
	return res;
}
