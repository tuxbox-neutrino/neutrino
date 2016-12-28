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

/* these are more descriptive... */
#define _LOCALE_PROGRESSBAR_COLOR_MATRIX        LOCALE_MISCSETTINGS_PROGRESSBAR_DESIGN_0
#define _LOCALE_PROGRESSBAR_COLOR_VERTICAL      LOCALE_MISCSETTINGS_PROGRESSBAR_DESIGN_1
#define _LOCALE_PROGRESSBAR_COLOR_HORIZONTAL    LOCALE_MISCSETTINGS_PROGRESSBAR_DESIGN_2
#define _LOCALE_PROGRESSBAR_COLOR_FULL          LOCALE_MISCSETTINGS_PROGRESSBAR_DESIGN_3
#define _LOCALE_PROGRESSBAR_COLOR_MONO          LOCALE_MISCSETTINGS_PROGRESSBAR_DESIGN_4

#define PROGRESSBAR_COLOR_OPTION_COUNT 6
const CMenuOptionChooser::keyval PROGRESSBAR_COLOR_OPTIONS[PROGRESSBAR_COLOR_OPTION_COUNT] =
{
	{ CProgressBar::PB_OFF,         LOCALE_OPTIONS_OFF },
	{ CProgressBar::PB_MONO,        _LOCALE_PROGRESSBAR_COLOR_MONO },
	{ CProgressBar::PB_MATRIX,      _LOCALE_PROGRESSBAR_COLOR_MATRIX },
	{ CProgressBar::PB_LINES_V,     _LOCALE_PROGRESSBAR_COLOR_VERTICAL },
	{ CProgressBar::PB_LINES_H,     _LOCALE_PROGRESSBAR_COLOR_HORIZONTAL },
	{ CProgressBar::PB_COLOR,       _LOCALE_PROGRESSBAR_COLOR_FULL },
};

#define PROGRESSBAR_TIMESCALE_INVERT_OPTION_COUNT 2
const CMenuOptionChooser::keyval PROGRESSBAR_TIMESCALE_INVERT_OPTIONS[PROGRESSBAR_TIMESCALE_INVERT_OPTION_COUNT] =
{
	{ 0, LOCALE_MISCSETTINGS_PROGRESSBAR_TIMESCALE_RED_GREEN },
	{ 1, LOCALE_MISCSETTINGS_PROGRESSBAR_TIMESCALE_GREEN_RED }
};

CProgressbarSetup::CProgressbarSetup()
{
	width = 40;
}

CProgressbarSetup::~CProgressbarSetup()
{

}

bool CProgressbarSetup::changeNotify(const neutrino_locale_t /* OptionName */, void * /* data */)
{
	return true; // repaint
}

int CProgressbarSetup::exec(CMenuTarget* parent, const std::string &actionKey)
{
	printf("[neutrino] init progressbar menu setup...\n");

	if (actionKey == "reset") {
		g_settings.theme.progressbar_timescale_red = 0;
		g_settings.theme.progressbar_timescale_green = 100;
		g_settings.theme.progressbar_timescale_yellow = 70;
		g_settings.theme.progressbar_timescale_invert = false;
		return menu_return::RETURN_REPAINT;
	}

	if (parent)
		parent->hide();

	return showMenu();
}

int CProgressbarSetup::showMenu()
{
	CMenuWidget m(LOCALE_MAINMENU_SETTINGS, NEUTRINO_ICON_SETTINGS, width, MN_WIDGET_ID_OSDSETUP_PROGRESSBAR);

	m.addIntroItems(LOCALE_MISCSETTINGS_PROGRESSBAR /*, LOCALE_MISCSETTINGS_GENERAL*/);

	// general progress bar design
	CMenuOptionChooser *mc = new CMenuOptionChooser(LOCALE_MISCSETTINGS_PROGRESSBAR_DESIGN_LONG,
			&g_settings.theme.progressbar_design, PROGRESSBAR_COLOR_OPTIONS + 1, PROGRESSBAR_COLOR_OPTION_COUNT - 1, true, this);
	mc->setHint("", LOCALE_MENU_HINT_PROGRESSBAR_COLOR);
	m.addItem(mc);

	// progress bar gradient
	mc = new CMenuOptionChooser(LOCALE_MISCSETTINGS_PROGRESSBAR_GRADIENT, &g_settings.theme.progressbar_gradient, OPTIONS_OFF0_ON1_OPTIONS, OPTIONS_OFF0_ON1_OPTION_COUNT, true, this);
	mc->setHint("", LOCALE_MENU_HINT_PROGRESSBAR_GRADIENT);
	m.addItem(mc);

	// preview
	CMenuProgressbar *mb = new CMenuProgressbar(LOCALE_MISCSETTINGS_PROGRESSBAR_PREVIEW);
	mb->setHint("", LOCALE_MENU_HINT_PROGRESSBAR_PREVIEW);
	m.addItem(mb);
	m.addItem(new CMenuSeparator(CMenuSeparator::LINE | CMenuSeparator::STRING, LOCALE_MISCSETTINGS_PROGRESSBAR_TIMESCALE));

	CMenuOptionNumberChooser *nc;

	nc = new CMenuOptionNumberChooser(LOCALE_MISCSETTINGS_PROGRESSBAR_TIMESCALE_RED, &g_settings.theme.progressbar_timescale_red, true, 0, 100, this);
	nc->setNumericInput(true);
	nc->setNumberFormat("%d %%");
	nc->setHint("", LOCALE_MENU_HINT_PROGRESSBAR_TIMESCALE_RED);
	m.addItem(nc);

	nc = new CMenuOptionNumberChooser(LOCALE_MISCSETTINGS_PROGRESSBAR_TIMESCALE_YELLOW, &g_settings.theme.progressbar_timescale_yellow, true, 0, 100, this);
	nc->setNumericInput(true);
	nc->setNumberFormat("%d %%");
	nc->setHint("", LOCALE_MENU_HINT_PROGRESSBAR_TIMESCALE_YELLOW);
	m.addItem(nc);

	nc = new CMenuOptionNumberChooser(LOCALE_MISCSETTINGS_PROGRESSBAR_TIMESCALE_GREEN, &g_settings.theme.progressbar_timescale_green, true, 0, 100, this);
	nc->setNumericInput(true);
	nc->setNumberFormat("%d %%");
	nc->setHint("", LOCALE_MENU_HINT_PROGRESSBAR_TIMESCALE_GREEN);
	m.addItem(nc);

	mc = new CMenuOptionChooser(LOCALE_MISCSETTINGS_PROGRESSBAR_TIMESCALE_INVERT, &g_settings.theme.progressbar_timescale_invert, PROGRESSBAR_TIMESCALE_INVERT_OPTIONS, PROGRESSBAR_TIMESCALE_INVERT_OPTION_COUNT, true, this);
	mc->setHint("", LOCALE_MENU_HINT_PROGRESSBAR_TIMESCALE_INVERT);
	m.addItem(mc);

	mb = new CMenuProgressbar(LOCALE_MISCSETTINGS_PROGRESSBAR_PREVIEW);
	mb->setHint("", LOCALE_MENU_HINT_PROGRESSBAR_PREVIEW);
	mb->getScale()->setType(CProgressBar::PB_TIMESCALE);
	m.addItem(mb);

	CMenuForwarder* mf = new CMenuForwarder(LOCALE_OPTIONS_DEFAULT, true, NULL, this, "reset", CRCInput::RC_red);
	mf->setHint("", LOCALE_OPTIONS_HINT_DEFAULT);
	m.addItem(mf);

	// extended channel list (progressbars)
	m.addItem(new CMenuSeparator(CMenuSeparator::LINE | CMenuSeparator::STRING, LOCALE_MAINMENU_CHANNELS));

	mc = new CMenuOptionChooser(LOCALE_CHANNELLIST_EXTENDED, &g_settings.theme.progressbar_design_channellist, PROGRESSBAR_COLOR_OPTIONS, PROGRESSBAR_COLOR_OPTION_COUNT, true, this);
	mc->setHint("", LOCALE_MENU_HINT_CHANNELLIST_EXTENDED);
	m.addItem(mc);

	mb = new CMenuProgressbar(LOCALE_MISCSETTINGS_PROGRESSBAR_PREVIEW);
	mb->setHint("", LOCALE_MENU_HINT_PROGRESSBAR_PREVIEW);
	mb->getScale()->setType(CProgressBar::PB_TIMESCALE);
	mb->getScale()->setDesign(g_settings.theme.progressbar_design_channellist);
	mb->getScale()->doPaintBg(false);
	m.addItem(mb);

	return m.exec(NULL, "");
}
