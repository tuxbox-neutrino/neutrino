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

#include "gui/settings_manager.h"

#include "gui/filebrowser.h"

#include "gui/widget/messagebox.h"
#include "gui/widget/stringinput.h"

#include <driver/screen_max.h>
#include <system/setting_helpers.h>

#include <sys/vfs.h>



CSettingsManager::CSettingsManager()
{
	width = w_max (40, 10);
}


CSettingsManager::~CSettingsManager()
{
}

int CSettingsManager::exec(CMenuTarget* parent, const std::string &actionKey)
{
	printf("[neutrino] CSettingsManager %s: init...\n",__FUNCTION__);
	int   res = menu_return::RETURN_REPAINT;
		
	if (parent)
		parent->hide();
	
	CFileBrowser fileBrowser;
	CFileFilter fileFilter;
	
	if(actionKey == "loadconfig") 
	{
		fileFilter.addFilter("conf");
		fileBrowser.Filter = &fileFilter;
		if (fileBrowser.exec("/var/tuxbox/config") == true) 
		{
			CNeutrinoApp::getInstance()->loadSetup(fileBrowser.getSelectedFile()->Name.c_str());
			CColorSetupNotifier *colorSetupNotifier = new CColorSetupNotifier;
			colorSetupNotifier->changeNotify(NONEXISTANT_LOCALE, NULL);
			CVFD::getInstance()->setlcdparameter();
			printf("[neutrino] new settings: %s\n", fileBrowser.getSelectedFile()->Name.c_str());
			delete colorSetupNotifier;
		}
		return res;
	}
	else if(actionKey == "saveconfig") 
	{
		fileBrowser.Dir_Mode = true;
		if (fileBrowser.exec("/var/tuxbox") == true) 
		{
			char  fname[256] = "neutrino.conf", sname[256];
			CStringInputSMS * sms = new CStringInputSMS(LOCALE_EXTRA_SAVECONFIG, fname, 30, NONEXISTANT_LOCALE, NONEXISTANT_LOCALE, "abcdefghijklmnopqrstuvwxyz0123456789. ");
			sms->exec(NULL, "");
			sprintf(sname, "%s/%s", fileBrowser.getSelectedFile()->Name.c_str(), fname);
			printf("[neutrino] save settings: %s\n", sname);
			CNeutrinoApp::getInstance()->saveSetup(sname);
			delete sms;
		}
		return res;
	}
	else if(actionKey == "backup") 
	{
		fileBrowser.Dir_Mode = true;
		if (fileBrowser.exec("/media") == true) 
		{
			char  fname[256];
			struct statfs s;
			int ret = ::statfs(fileBrowser.getSelectedFile()->Name.c_str(), &s);
			if(ret == 0 && s.f_type != 0x72b6L) /*jffs2*/
			{ 
				sprintf(fname, "/bin/backup.sh %s", fileBrowser.getSelectedFile()->Name.c_str());
				printf("backup: executing [%s]\n", fname);
				system(fname);
			} 
			else
				ShowMsgUTF(LOCALE_MESSAGEBOX_ERROR, g_Locale->getText(LOCALE_SETTINGS_BACKUP_FAILED),CMessageBox::mbrBack, CMessageBox::mbBack, NEUTRINO_ICON_ERROR);
		}
		return res;
	}
	else if(actionKey == "restore") 
	{
		fileFilter.addFilter("tar");
		fileBrowser.Filter = &fileFilter;
		if (fileBrowser.exec("/media") == true) 
		{
			int result = ShowMsgUTF(LOCALE_SETTINGS_RESTORE, g_Locale->getText(LOCALE_SETTINGS_RESTORE_WARN), CMessageBox::mbrNo, CMessageBox::mbYes | CMessageBox::mbNo);
			if(result == CMessageBox::mbrYes) 
			{
				char  fname[256];
				sprintf(fname, "/bin/restore.sh %s", fileBrowser.getSelectedFile()->Name.c_str());
				printf("restore: executing [%s]\n", fname);
				system(fname);
			}
		}
		return res;
	}
	
	res = showMenu();

	return res;
}

int CSettingsManager::showMenu()
{
	printf("[neutrino] CSettingsManager call %s...\n", __FUNCTION__);

	CDataResetNotifier * resetNotifier = new CDataResetNotifier();

	CMenuWidget * mset = new CMenuWidget(LOCALE_MAINSETTINGS_MANAGE, NEUTRINO_ICON_SETTINGS, width, MN_WIDGET_ID_SETTINGS_MNGR);
	mset->addIntroItems();

	mset->addItem(new CMenuForwarder(LOCALE_RESET_SETTINGS,   true, NULL, resetNotifier, 	"settings", 	CRCInput::RC_recall));// FIXME: RC-button RECALL is broken
	mset->addItem(GenericMenuSeparatorLine);
	mset->addItem(new CMenuForwarder(LOCALE_EXTRA_SAVECONFIG, true, NULL, this, 	"saveconfig", 	CRCInput::RC_red, NEUTRINO_ICON_BUTTON_RED));
	mset->addItem(new CMenuForwarder(LOCALE_EXTRA_LOADCONFIG, true, NULL, this, 	"loadconfig", 	CRCInput::RC_green, NEUTRINO_ICON_BUTTON_GREEN));
	mset->addItem(GenericMenuSeparatorLine);
	mset->addItem(new CMenuForwarder(LOCALE_SETTINGS_BACKUP,  true, NULL, this, 	"backup", 	CRCInput::RC_yellow, NEUTRINO_ICON_BUTTON_YELLOW));
	mset->addItem(new CMenuForwarder(LOCALE_SETTINGS_RESTORE, true, NULL, this, 	"restore", 	CRCInput::RC_blue, NEUTRINO_ICON_BUTTON_BLUE));
	mset->addItem(GenericMenuSeparatorLine);
	mset->addItem(new CMenuForwarder(LOCALE_RESET_ALL,   true, NULL, resetNotifier, 	"all", 		CRCInput::RC_standby, NEUTRINO_ICON_BUTTON_POWER));
		
	int res = mset->exec(NULL, "");
	mset->hide();
	delete resetNotifier;
	delete mset;
	return res;
}
