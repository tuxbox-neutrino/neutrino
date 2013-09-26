/*
	$Id: port of software_update.cpp,v 1.8 2011/04/03 21:56:13 tuxbox-cvs Exp $
	
	Neutrino-GUI  -   DBoxII-Project
	
	Software update implementation - Neutrino-GUI
	
	Copyright (C) 2001 Steffen Hehn 'McClean'
	and some other guys
	Homepage: http://dbox.cyberphoria.org/
	
	Rework Copyright (C) 2011 T. Graf 'dbt'
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
#include <neutrino_menue.h>

#include "update_menue.h"
#include "update_settings.h"

#include <gui/widget/icons.h>
#include <driver/screen_max.h>
#include <system/debug.h>
#include <system/flashtool.h>

CSoftwareUpdate::CSoftwareUpdate()
{
	width = w_max (40, 10);
	fe = new CFlashExpert();
	update_item = NULL;
}

CSoftwareUpdate::~CSoftwareUpdate()
{
	delete fe;
	delete update_item;
}

int CSoftwareUpdate::exec(CMenuTarget* parent, const std::string &/*actionKey*/)
{
	dprintf(DEBUG_DEBUG, "init software-update\n");
	int   res = menu_return::RETURN_REPAINT;
	
	if (parent)
		parent->hide();
	
	res = showSoftwareUpdate();
	return res;
}

int CSoftwareUpdate::showSoftwareUpdate()
/* shows the menue and options for software update */
{
	CMenuForwarder * mf;
	CMenuWidget softUpdate(LOCALE_MAINMENU_SERVICE, NEUTRINO_ICON_UPDATE, width, MN_WIDGET_ID_SOFTWAREUPDATE);
	
	softUpdate.addIntroItems(LOCALE_SERVICEMENU_UPDATE);
	
	//flashing
	CFlashUpdate flash;
#if 0
	neutrino_locale_t up_text = (g_settings.softupdate_mode == 0) ? LOCALE_FLASHUPDATE_CHECKUPDATE_LOCAL : LOCALE_FLASHUPDATE_CHECKUPDATE_INTERNET;
	update_item = new CMenuForwarder(up_text, true, NULL, &flash, NULL, CRCInput::RC_red, NEUTRINO_ICON_BUTTON_RED);
	update_item->setHint("", LOCALE_MENU_HINT_SOFTUPDATE_CHECK);
	softUpdate.addItem(update_item);
#endif

	update_item = new CMenuForwarder(LOCALE_FLASHUPDATE_CHECKUPDATE_INTERNET, true, NULL, &flash, "inet", CRCInput::RC_red, NEUTRINO_ICON_BUTTON_RED);
	update_item->setHint("", LOCALE_MENU_HINT_SOFTUPDATE_CHECK);
	softUpdate.addItem(update_item);

	update_item = new CMenuForwarder(LOCALE_FLASHUPDATE_CHECKUPDATE_LOCAL, true, NULL, &flash, "local", CRCInput::RC_green, NEUTRINO_ICON_BUTTON_GREEN);
	update_item->setHint("", LOCALE_MENU_HINT_SOFTUPDATE_CHECK_LOCAL);
	softUpdate.addItem(update_item);

	//settings
	CUpdateSettings update_settings(update_item);
	mf = new CMenuForwarder(LOCALE_FLASHUPDATE_SETTINGS, true, NULL, &update_settings, NULL, CRCInput::RC_yellow, NEUTRINO_ICON_BUTTON_YELLOW);
	mf->setHint("", LOCALE_MENU_HINT_SOFTUPDATE_SETTINGS);
	softUpdate.addItem(mf);

	softUpdate.addItem(GenericMenuSeparatorLine);

	//expert-functions
	CMenuWidget mtdexpert(LOCALE_FLASHUPDATE_EXPERTFUNCTIONS, NEUTRINO_ICON_UPDATE, width, MN_WIDGET_ID_MTDEXPERT);
	showSoftwareUpdateExpert(&mtdexpert); 
	mf = new CMenuForwarder(LOCALE_FLASHUPDATE_EXPERTFUNCTIONS, true, NULL, &mtdexpert, NULL, CRCInput::RC_blue, NEUTRINO_ICON_BUTTON_BLUE);
	mf->setHint("", LOCALE_MENU_HINT_SOFTUPDATE_EXPERT);
	softUpdate.addItem(mf);

#ifdef BOXMODEL_APOLLO
	softUpdate.addItem(GenericMenuSeparatorLine);

	mf = new CMenuForwarder(LOCALE_FLASHUPDATE_CREATEIMAGE_MENU, true, NULL, new CFlashExpertSetup(), NULL, CRCInput::convertDigitToKey(1));
	mf->setHint("", LOCALE_MENU_HINT_SOFTUPDATE_CREATEIMAGE_MENU);
	softUpdate.addItem(mf);
#endif

	int res = softUpdate.exec (NULL, "");
	return res;
}

/* shows experts-functions to read/write to the mtd */
void CSoftwareUpdate::showSoftwareUpdateExpert(CMenuWidget *w_mtd_expert)
{
	CMenuForwarder * mf;
	w_mtd_expert->addIntroItems();
		
	mf = new CMenuForwarder(LOCALE_FLASHUPDATE_READFLASHMTD , true, NULL, fe, "readflashmtd" , CRCInput::RC_red  , NEUTRINO_ICON_BUTTON_RED);
	mf->setHint("", LOCALE_MENU_HINT_SOFTUPDATE_EXPERT_READ);
	w_mtd_expert->addItem(mf);
	mf = new CMenuForwarder(LOCALE_FLASHUPDATE_WRITEFLASHMTD, true, NULL, fe, "writeflashmtd", CRCInput::RC_green, NEUTRINO_ICON_BUTTON_GREEN);
	mf->setHint("", LOCALE_MENU_HINT_SOFTUPDATE_EXPERT_WRITE);
	w_mtd_expert->addItem(mf);
}
