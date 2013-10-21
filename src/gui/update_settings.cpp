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

	You should have received a copy of the GNU General Public
	License along with this program; if not, write to the
	Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
	Boston, MA  02110-1301, USA.
*/


#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <global.h>
#include <neutrino.h>
#include <mymenu.h>
#include <neutrino_menue.h>
#include <gui/filebrowser.h>
#include <gui/update_ext.h>
#include <gui/update_settings.h>
#include <gui/widget/icons.h>
#include <driver/screen_max.h>
#include <system/debug.h>


CUpdateSettings::CUpdateSettings(CMenuForwarder * update_item)
{
	width = w_max (40, 10);
	updateItem = update_item;
#ifdef USE_SMS_INPUT
	input_url_file = new CStringInputSMS(LOCALE_FLASHUPDATE_URL_FILE, g_settings.softupdate_url_file, 30, NONEXISTANT_LOCALE, NONEXISTANT_LOCALE, "abcdefghijklmnopqrstuvwxyz0123456789!""$%&/()=?-. ");
#endif
}

CUpdateSettings::~CUpdateSettings()
{
#ifdef USE_SMS_INPUT
	delete input_url_file;
#endif
}

#define FLASHUPDATE_UPDATEMODE_OPTION_COUNT 2
const CMenuOptionChooser::keyval FLASHUPDATE_UPDATEMODE_OPTIONS[FLASHUPDATE_UPDATEMODE_OPTION_COUNT] =
{
	{ 0, LOCALE_FLASHUPDATE_UPDATEMODE_MANUAL   },
	{ 1, LOCALE_FLASHUPDATE_UPDATEMODE_INTERNET }
};

#if ENABLE_EXTUPDATE
#define SOFTUPDATE_NAME_MODE1_OPTION_COUNT 3
const CMenuOptionChooser::keyval SOFTUPDATE_NAME_MODE1_OPTIONS[SOFTUPDATE_NAME_MODE1_OPTION_COUNT] =
{
	{ CExtUpdate::SOFTUPDATE_NAME_DEFAULT,       LOCALE_FLASHUPDATE_NAMEMODE1_DEFAULT       },
	{ CExtUpdate::SOFTUPDATE_NAME_HOSTNAME_TIME, LOCALE_FLASHUPDATE_NAMEMODE1_HOSTNAME_TIME },
	{ CExtUpdate::SOFTUPDATE_NAME_ORGNAME_TIME,  LOCALE_FLASHUPDATE_NAMEMODE1_ORGNAME_TIME  }
};

#define SOFTUPDATE_NAME_MODE2_OPTION_COUNT 2
const CMenuOptionChooser::keyval SOFTUPDATE_NAME_MODE2_OPTIONS[SOFTUPDATE_NAME_MODE2_OPTION_COUNT] =
{
	{ CExtUpdate::SOFTUPDATE_NAME_DEFAULT,       LOCALE_FLASHUPDATE_NAMEMODE2_DEFAULT       },
	{ CExtUpdate::SOFTUPDATE_NAME_HOSTNAME_TIME, LOCALE_FLASHUPDATE_NAMEMODE2_HOSTNAME_TIME }
};
#endif

int CUpdateSettings::exec(CMenuTarget* parent, const std::string &actionKey)
{
	dprintf(DEBUG_DEBUG, "init software-update settings\n");
	int   res = menu_return::RETURN_REPAINT;

	if (parent)
		parent->hide();

	if(actionKey == "update_dir") {
		const char *action_str = "update";
		if(chooserDir(g_settings.update_dir, true, action_str, sizeof(g_settings.update_dir)-1,true))
			printf("[neutrino] new %s dir %s\n", action_str, g_settings.update_dir);

		return res;
	}
#ifndef USE_SMS_INPUT
	else if(actionKey == "select_url_config_file"){
		CFileBrowser 	fileBrowser;
		CFileFilter 	fileFilter;

		fileFilter.addFilter("conf");
		fileFilter.addFilter("urls");
		fileBrowser.Filter = &fileFilter;
		if (fileBrowser.exec("/var/etc") == true)
			strncpy(g_settings.softupdate_url_file, fileBrowser.getSelectedFile()->Name.c_str(), 30);

		return res;
	}
#endif

	res = initMenu();
	return res;
}

/* init options for software update */
int CUpdateSettings::initMenu()
{
	COnOffNotifier* OnOffNotifier = new COnOffNotifier(0);

	CMenuWidget w_upsettings(LOCALE_SERVICEMENU_UPDATE, NEUTRINO_ICON_UPDATE, width, MN_WIDGET_ID_SOFTWAREUPDATE_SETTINGS);
	w_upsettings.addIntroItems(LOCALE_FLASHUPDATE_SETTINGS);

	CMenuForwarder * fw_url 	= new CMenuForwarder(LOCALE_FLASHUPDATE_URL_FILE, true, g_settings.softupdate_url_file, this, "select_url_config_file", CRCInput::RC_green, NEUTRINO_ICON_BUTTON_GREEN);
//	fw_url->setHint("", LOCALE_MENU_HINT_XXX);
	CMenuForwarder * fw_update_dir 	= new CMenuForwarder(LOCALE_EXTRA_UPDATE_DIR, true, g_settings.update_dir , this, "update_dir", CRCInput::RC_red, NEUTRINO_ICON_BUTTON_RED);
//	fw_update_dir->setHint("", LOCALE_MENU_HINT_XXX);
#if ENABLE_EXTUPDATE
	CMenuOptionChooser *name_backup = new CMenuOptionChooser(LOCALE_FLASHUPDATE_NAMEMODE2, &g_settings.softupdate_name_mode_backup, SOFTUPDATE_NAME_MODE2_OPTIONS, SOFTUPDATE_NAME_MODE2_OPTION_COUNT, true);
//	name_backup->setHint("", LOCALE_MENU_HINT_XXX);

#ifndef BOXMODEL_APOLLO
	CMenuOptionChooser *apply_settings = new CMenuOptionChooser(LOCALE_FLASHUPDATE_MENU_APPLY_SETTINGS, &g_settings.apply_settings, OPTIONS_OFF0_ON1_OPTIONS, OPTIONS_OFF0_ON1_OPTION_COUNT, true, OnOffNotifier);
//	apply_settings->setHint("", LOCALE_MENU_HINT_XXX);

	CMenuOptionChooser *name_apply = new CMenuOptionChooser(LOCALE_FLASHUPDATE_NAMEMODE1, &g_settings.softupdate_name_mode_apply, SOFTUPDATE_NAME_MODE1_OPTIONS, SOFTUPDATE_NAME_MODE1_OPTION_COUNT, g_settings.apply_settings);
//	name_apply->setHint("", LOCALE_MENU_HINT_XXX);
	OnOffNotifier->addItem(name_apply);
#endif
#endif

#if 0
	CMenuOptionChooser *apply_kernel = new CMenuOptionChooser(LOCALE_FLASHUPDATE_MENU_APPLY_KERNEL, &g_settings.apply_kernel, OPTIONS_OFF0_ON1_OPTIONS, OPTIONS_OFF0_ON1_OPTION_COUNT, g_settings.apply_settings);
//	apply_kernel->setHint("", LOCALE_MENU_HINT_XXX);
	OnOffNotifier->addItem(apply_kernel);
#endif

	w_upsettings.addItem(fw_update_dir);
	w_upsettings.addItem(fw_url);
#if ENABLE_EXTUPDATE
	w_upsettings.addItem(name_backup);
#ifndef BOXMODEL_APOLLO
	w_upsettings.addItem(GenericMenuSeparatorLine);
	w_upsettings.addItem(apply_settings);
	w_upsettings.addItem(name_apply);
#endif
#endif

#if 0
	w_upsettings.addItem(apply_kernel);
#endif

	int res = w_upsettings.exec (NULL, "");
	delete OnOffNotifier;

	return res;
}
