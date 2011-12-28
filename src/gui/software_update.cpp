/*
	$Id: port of software_update.cpp,v 1.8 2011/04/03 21:56:13 tuxbox-cvs Exp $

	Neutrino-GUI  -   DBoxII-Project

	Software update implementation - Neutrino-GUI

	Copyright (C) 2001 Steffen Hehn 'McClean'
	and some other guys
	Homepage: http://dbox.cyberphoria.org/

	rework Copyright (C) 2011 T. Graf 'dbt'
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
#include "software_update.h"

#include "gui/filebrowser.h"
#include <gui/widget/icons.h>
#include <driver/screen_max.h>
#include <driver/screen_max.h>
#include <system/debug.h>
#include <system/flashtool.h>

CSoftwareUpdate::CSoftwareUpdate()
{
	width = w_max (40, 10);
	fe = new CFlashExpert();
	input_url_file = new CStringInputSMS(LOCALE_FLASHUPDATE_URL_FILE, g_settings.softupdate_url_file, 30, NONEXISTANT_LOCALE, NONEXISTANT_LOCALE, "abcdefghijklmnopqrstuvwxyz0123456789!""�$%&/()=?-. ");
}

CSoftwareUpdate::~CSoftwareUpdate()
{
	delete fe;
	delete input_url_file;
}

#define FLASHUPDATE_UPDATEMODE_OPTION_COUNT 2
const CMenuOptionChooser::keyval FLASHUPDATE_UPDATEMODE_OPTIONS[FLASHUPDATE_UPDATEMODE_OPTION_COUNT] =
{
	{ 0, LOCALE_FLASHUPDATE_UPDATEMODE_MANUAL   },
	{ 1, LOCALE_FLASHUPDATE_UPDATEMODE_INTERNET }
};

int CSoftwareUpdate::exec(CMenuTarget* parent, const std::string &actionKey)
{
	dprintf(DEBUG_DEBUG, "init software-update\n");
	int   res = menu_return::RETURN_REPAINT;

	if (parent)
	{
		parent->hide();
	}

	if(actionKey == "update_dir") {

		const char *action_str = "update";
		if(chooserDir(g_settings.update_dir, true, action_str, sizeof(g_settings.update_dir)-1))
			printf("[neutrino] new %s dir %s\n", action_str, g_settings.update_dir);
		
		return res;
	}

	res = showSoftwareUpdate();
	return res;
}

int CSoftwareUpdate::showSoftwareUpdate()
/* shows the menue and options for software update */
{
	CMenuWidget* softUpdate = new CMenuWidget(LOCALE_MAINMENU_SERVICE, NEUTRINO_ICON_UPDATE, width, MN_WIDGET_ID_SOFTWAREUPDATE);
	
	softUpdate->addIntroItems(LOCALE_SERVICEMENU_UPDATE);
	
 	//expert-functions
 	CMenuWidget *mtdexpert = new CMenuWidget(LOCALE_FLASHUPDATE_EXPERTFUNCTIONS, NEUTRINO_ICON_UPDATE, width, MN_WIDGET_ID_MTDEXPERT);
	showSoftwareUpdateExpert(mtdexpert); 
 	softUpdate->addItem(new CMenuForwarder(LOCALE_FLASHUPDATE_EXPERTFUNCTIONS, true, NULL, mtdexpert, "", CRCInput::RC_red, NEUTRINO_ICON_BUTTON_RED));
	
 	softUpdate->addItem(GenericMenuSeparatorLine);
	
 	CMenuOptionChooser *oj = new CMenuOptionChooser(LOCALE_FLASHUPDATE_UPDATEMODE, &g_settings.softupdate_mode, FLASHUPDATE_UPDATEMODE_OPTIONS, FLASHUPDATE_UPDATEMODE_OPTION_COUNT, true, NULL);
 	softUpdate->addItem( oj );
 	
 	softUpdate->addItem( new CMenuForwarder(LOCALE_EXTRA_UPDATE_DIR, true, g_settings.update_dir , this, "update_dir", CRCInput::RC_green, NEUTRINO_ICON_BUTTON_GREEN));
 
	softUpdate->addItem(GenericMenuSeparatorLine);
	softUpdate->addItem(new CMenuForwarder(LOCALE_FLASHUPDATE_CHECKUPDATE, true, NULL, new CFlashUpdate(), "", CRCInput::RC_yellow, NEUTRINO_ICON_BUTTON_YELLOW ));

	int res = softUpdate->exec (NULL, "");
	softUpdate->hide ();
	delete softUpdate;
	return res;
}

/* shows experts-functions to read/write to the mtd */
void CSoftwareUpdate::showSoftwareUpdateExpert(CMenuWidget *mtd_expert)
{
 	mtd_expert->addIntroItems();
		
	mtd_expert->addItem(new CMenuForwarder(LOCALE_FLASHUPDATE_READFLASHMTD , true, NULL, fe, "readflashmtd" ));
	mtd_expert->addItem(new CMenuForwarder(LOCALE_FLASHUPDATE_WRITEFLASHMTD, true, NULL, fe, "writeflashmtd"));
	
	mtd_expert->addItem(GenericMenuSeparatorLine);

	mtd_expert->addItem(new CMenuForwarder(LOCALE_FLASHUPDATE_URL_FILE, true, g_settings.softupdate_url_file, input_url_file));
}
