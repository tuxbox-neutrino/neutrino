/*
	Based up Neutrino-GUI - Tuxbox-Project
	Copyright (C) 2001 by Steffen Hehn 'McClean'

	progressbar_setup menu
	Suggested by tomworld

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
	Foundation, Inc., 51 Franklin Street, Fifth Floor, 
	Boston, MA  02110-1301, USA.

*/

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "osd_progressbar_setup.h"

#include <global.h>
#include <neutrino.h>
#include <mymenu.h>
#include <neutrino_menue.h>

#include <driver/screen_max.h>

#include <system/debug.h>

#define PROGRESSBAR_INFOBAR_POSITION_COUNT 4
const CMenuOptionChooser::keyval PROGRESSBAR_INFOBAR_POSITION_OPTIONS[PROGRESSBAR_INFOBAR_POSITION_COUNT]=
{
   { 0 , LOCALE_MISCSETTINGS_PROGRESSBAR_INFOBAR_POSITION_0 },
   { 1 , LOCALE_MISCSETTINGS_PROGRESSBAR_INFOBAR_POSITION_1 },
   { 2 , LOCALE_MISCSETTINGS_PROGRESSBAR_INFOBAR_POSITION_2 },
   { 3 , LOCALE_MISCSETTINGS_PROGRESSBAR_INFOBAR_POSITION_3 }
};

#define PROGRESSBAR_DESIGN_COUNT 4
const CMenuOptionChooser::keyval PROGRESSBAR_DESIGN_OPTIONS[PROGRESSBAR_DESIGN_COUNT]=
{
   { 0 , LOCALE_MISCSETTINGS_PROGRESSBAR_DESIGN_0 },
   { 1 , LOCALE_MISCSETTINGS_PROGRESSBAR_DESIGN_1 },
   { 2 , LOCALE_MISCSETTINGS_PROGRESSBAR_DESIGN_2 },
   { 3 , LOCALE_MISCSETTINGS_PROGRESSBAR_DESIGN_3 }
};

CProgressbarSetup::CProgressbarSetup()
{
	width = w_max (40, 10); //%
}

CProgressbarSetup::~CProgressbarSetup()
{

}

int CProgressbarSetup::exec(CMenuTarget* parent, const std::string &)
{
	printf("[neutrino] init progressbar menu setup...\n");

	if (parent)
		parent->hide();

	return showMenu();
}

int CProgressbarSetup::showMenu()
{
	//menue init
	CMenuWidget *progress = new CMenuWidget(LOCALE_MAINMENU_SETTINGS, NEUTRINO_ICON_SETTINGS, width, MN_WIDGET_ID_PROGRESSBAR);

	//intros: back ande save
	progress->addIntroItems(LOCALE_MISCSETTINGS_PROGRESSBAR);

	COnOffNotifier* miscProgressNotifier = new COnOffNotifier(0);

	//color on/off
	CMenuOptionChooser *color;
	color = new CMenuOptionChooser(LOCALE_MISCSETTINGS_PROGRESSBAR_COLOR, &g_settings.progressbar_color, OPTIONS_OFF0_ON1_OPTIONS, OPTIONS_OFF0_ON1_OPTION_COUNT, true, miscProgressNotifier);
	color->setHint("", LOCALE_MENU_HINT_PROGRESSBAR_COLOR);

	//design
	CMenuOptionChooser *design;
	design = new CMenuOptionChooser(LOCALE_MISCSETTINGS_PROGRESSBAR_DESIGN, &g_settings.progressbar_design, PROGRESSBAR_DESIGN_OPTIONS, PROGRESSBAR_DESIGN_COUNT, g_settings.progressbar_color);
	design->setHint("", LOCALE_MENU_HINT_PROGRESSBAR_DESIGN);

	//infobar position
	CMenuOptionChooser *infobar_position;
	infobar_position = new CMenuOptionChooser(LOCALE_MISCSETTINGS_PROGRESSBAR_INFOBAR_POSITION, &g_settings.infobar_progressbar, PROGRESSBAR_INFOBAR_POSITION_OPTIONS, PROGRESSBAR_INFOBAR_POSITION_COUNT, true);
	infobar_position->setHint("", LOCALE_MENU_HINT_PROGRESSBAR_INFOBAR_POSITION);

	miscProgressNotifier->addItem(design);

	//paint items
	progress->addItem(color);
	progress->addItem(design);
	progress->addItem(new CMenuSeparator(CMenuSeparator::LINE | CMenuSeparator::STRING, LOCALE_MISCSETTINGS_INFOBAR));
	progress->addItem(infobar_position);

	int res = progress->exec (NULL, "");
	delete miscProgressNotifier;
	delete progress;

	return res;
}
