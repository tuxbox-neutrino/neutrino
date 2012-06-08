/*
	Neutrino-GUI  -   DBoxII-Project
	
	Update settings  implementation - Neutrino-GUI
	
	Copyright (C) 2001 Steffen Hehn 'McClean'
	and some other guys
	Homepage: http://dbox.cyberphoria.org/
	
	Copyright (C) 2012 T. Graf 'dbt'
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
	
	You should have received a copy of the GNU Library General Public
	License along with this program; if not, write to the
	Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
	Boston, MA  02110-1301, USA.
*/


#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <global.h>
#include <neutrino.h>
#include <neutrino_menue.h>
#include "update_settings.h"
#include "gui/filebrowser.h"
#include <gui/widget/icons.h>
#include <driver/screen_max.h>
#include <system/debug.h>


CUpdateSettings::CUpdateSettings()
{
	width = w_max (40, 10);
	input_url_file = new CStringInputSMS(LOCALE_FLASHUPDATE_URL_FILE, g_settings.softupdate_url_file, 30, NONEXISTANT_LOCALE, NONEXISTANT_LOCALE, "abcdefghijklmnopqrstuvwxyz0123456789!""�$%&/()=?-. ");
}

CUpdateSettings::~CUpdateSettings()
{
	delete input_url_file;
}

#define FLASHUPDATE_UPDATEMODE_OPTION_COUNT 2
const CMenuOptionChooser::keyval FLASHUPDATE_UPDATEMODE_OPTIONS[FLASHUPDATE_UPDATEMODE_OPTION_COUNT] =
{
	{ 0, LOCALE_FLASHUPDATE_UPDATEMODE_MANUAL   },
	{ 1, LOCALE_FLASHUPDATE_UPDATEMODE_INTERNET }
};

int CUpdateSettings::exec(CMenuTarget* parent, const std::string &actionKey)
{
	dprintf(DEBUG_DEBUG, "init software-update settings\n");
	int   res = menu_return::RETURN_REPAINT;
	
	if (parent)
		parent->hide();
	
	if(actionKey == "update_dir") {
		const char *action_str = "update";
		if(chooserDir(g_settings.update_dir, true, action_str, sizeof(g_settings.update_dir)-1))
			printf("[neutrino] new %s dir %s\n", action_str, g_settings.update_dir);
		
		return res;
	}
	
	res = initMenu();
	return res;
}

/* init options for software update */
int CUpdateSettings::initMenu()
{
	CMenuWidget w_upsettings(LOCALE_SERVICEMENU_UPDATE, NEUTRINO_ICON_UPDATE, width, MN_WIDGET_ID_SOFTWAREUPDATE_SETTINGS);
	
	w_upsettings.addIntroItems(LOCALE_FLASHUPDATE_SETTINGS);
	
	CMenuForwarder * fw_url_input = new CMenuForwarder(LOCALE_FLASHUPDATE_URL_FILE, g_settings.softupdate_mode, g_settings.softupdate_url_file, input_url_file, NULL, CRCInput::RC_red, NEUTRINO_ICON_BUTTON_RED);
	CMenuForwarder * fw_update_dir = new CMenuForwarder(LOCALE_EXTRA_UPDATE_DIR, !g_settings.softupdate_mode, g_settings.update_dir , this, "update_dir", CRCInput::RC_green, NEUTRINO_ICON_BUTTON_GREEN);
	
	CUrlConfigSetupNotifier url_setup_notifier(fw_url_input, fw_update_dir);
	
	CMenuOptionChooser *oj_mode = new CMenuOptionChooser(LOCALE_FLASHUPDATE_UPDATEMODE, &g_settings.softupdate_mode, FLASHUPDATE_UPDATEMODE_OPTIONS, FLASHUPDATE_UPDATEMODE_OPTION_COUNT, true, &url_setup_notifier);
	
	w_upsettings.addItem(oj_mode);
	w_upsettings.addItem(GenericMenuSeparatorLine);
	w_upsettings.addItem(fw_update_dir);
	w_upsettings.addItem(fw_url_input);
	
	int res = w_upsettings.exec (NULL, "");
	
	return res;
}


CUrlConfigSetupNotifier::CUrlConfigSetupNotifier( CMenuItem* i1, CMenuItem* i2)
{
	toDisable[0] = i1;
	toDisable[1] = i2;
}

bool CUrlConfigSetupNotifier::changeNotify(const neutrino_locale_t, void *)
{
	if (g_settings.softupdate_mode){
		toDisable[0]->setActive(true);
		toDisable[1]->setActive(false);
	}else{
		toDisable[0]->setActive(false);
		toDisable[1]->setActive(true);
	}
	return false;
}
