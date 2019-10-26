/*
	Neutrino graphlcd menue

	(c) 2012 by martii


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

#define __USE_FILE_OFFSET64 1
#include "filebrowser.h"
#include <stdio.h>
#include <global.h>
#include <neutrino.h>
#include <zapit/channel.h>
#include <driver/fontrenderer.h>
#include <driver/rcinput.h>
#include <daemonc/remotecontrol.h>
#include <driver/nglcd.h>
#include <driver/screen_max.h>
#include "glcdsetup.h"
#include <mymenu.h>

#define KEY_GLCD_BLACK			0
#define KEY_GLCD_WHITE			1
#define KEY_GLCD_RED			2
#define KEY_GLCD_GREEN			3
#define KEY_GLCD_BLUE			4
#define KEY_GLCD_MAGENTA		5
#define KEY_GLCD_CYAN			6
#define KEY_GLCD_YELLOW			7
#define KEY_GLCD_ORANGE			8
#define KEY_GLCD_LIGHT_GRAY		9
#define KEY_GLCD_GRAY			10
#define KEY_GLCD_DARK_GRAY		11
#define KEY_GLCD_DARK_RED		12
#define KEY_GLCD_DARK_GREEN		13
#define KEY_GLCD_DARK_BLUE		14
#define KEY_GLCD_PURPLE			15
#define KEY_GLCD_MINT			16
#define KEY_GLCD_GOLDEN			17
#define GLCD_COLOR_OPTION_COUNT		18
static const CMenuOptionChooser::keyval GLCD_COLOR_OPTIONS[GLCD_COLOR_OPTION_COUNT] =
{
	{ KEY_GLCD_BLACK,	LOCALE_GLCD_COLOR_BLACK },
	{ KEY_GLCD_WHITE,	LOCALE_GLCD_COLOR_WHITE },
	{ KEY_GLCD_RED,		LOCALE_GLCD_COLOR_RED },
	{ KEY_GLCD_GREEN,	LOCALE_GLCD_COLOR_GREEN },
	{ KEY_GLCD_BLUE,	LOCALE_GLCD_COLOR_BLUE },
	{ KEY_GLCD_MAGENTA,	LOCALE_GLCD_COLOR_MAGENTA },
	{ KEY_GLCD_CYAN,	LOCALE_GLCD_COLOR_CYAN },
	{ KEY_GLCD_YELLOW,	LOCALE_GLCD_COLOR_YELLOW },
	{ KEY_GLCD_ORANGE,	LOCALE_GLCD_COLOR_ORANGE },
	{ KEY_GLCD_LIGHT_GRAY,	LOCALE_GLCD_COLOR_LIGHT_GRAY },
	{ KEY_GLCD_GRAY,	LOCALE_GLCD_COLOR_GRAY },
	{ KEY_GLCD_DARK_GRAY,	LOCALE_GLCD_COLOR_DARK_GRAY },
	{ KEY_GLCD_DARK_RED,	LOCALE_GLCD_COLOR_DARK_RED },
	{ KEY_GLCD_DARK_GREEN,	LOCALE_GLCD_COLOR_DARK_GREEN },
	{ KEY_GLCD_DARK_BLUE,	LOCALE_GLCD_COLOR_DARK_BLUE },
	{ KEY_GLCD_PURPLE,	LOCALE_GLCD_COLOR_PURPLE },
	{ KEY_GLCD_MINT,	LOCALE_GLCD_COLOR_MINT },
	{ KEY_GLCD_GOLDEN,	LOCALE_GLCD_COLOR_GOLDEN }
};

static const uint32_t colormap[GLCD_COLOR_OPTION_COUNT] =
{
	GLCD::cColor::Black,
	GLCD::cColor::White,
	GLCD::cColor::Red,
	GLCD::cColor::Green,
	GLCD::cColor::Blue,
	GLCD::cColor::Magenta,
	GLCD::cColor::Cyan,
	GLCD::cColor::Yellow,
	GLCD::cColor::Orange,
	GLCD::cColor::Light_Gray,
	GLCD::cColor::Gray,
	GLCD::cColor::Dark_Gray,
	GLCD::cColor::Dark_Red,
	GLCD::cColor::Dark_Green,
	GLCD::cColor::Dark_Blue,
	GLCD::cColor::Purple,
	GLCD::cColor::Mint,
	GLCD::cColor::Golden
};

int GLCD_Menu::color2index(uint32_t color) {
	for (int i = 0; i < GLCD_COLOR_OPTION_COUNT; i++)
		if (colormap[i] == color)
			return i;
	return KEY_GLCD_BLACK;
}

uint32_t GLCD_Menu::index2color(int i) {
	return (i < GLCD_COLOR_OPTION_COUNT) ? colormap[i] : GLCD::cColor::ERRCOL;
}

GLCD_Menu::GLCD_Menu()
{
	width = 40;
	selected = -1;
}

int GLCD_Menu::exec(CMenuTarget* parent, const std::string & actionKey)
{
	int res = menu_return::RETURN_REPAINT;
	nGLCD *nglcd = nGLCD::getInstance();
	if(actionKey == "rescan") {
		nglcd->Rescan();
		return res;
	}
	if(actionKey == "select_font") {
		if(parent)
			parent->hide();
		CFileBrowser fileBrowser;
		CFileFilter fileFilter;
		fileFilter.addFilter("ttf");
		fileBrowser.Filter = &fileFilter;
		if (fileBrowser.exec(FONTDIR) == true) {
			g_settings.glcd_font = fileBrowser.getSelectedFile()->Name;
			nglcd->Rescan();
		}
		return res;
	}

	if (parent)
		parent->hide();

	GLCD_Menu_Settings();

	return res;
}

void GLCD_Menu::hide()
{
}

bool GLCD_Menu::changeNotify (const neutrino_locale_t OptionName, void *Data)
{
	if (!Data)
		return false;
	nGLCD *nglcd = nGLCD::getInstance();
	switch(OptionName) {
	case LOCALE_GLCD_SELECT_FG:
		g_settings.glcd_color_fg = GLCD_Menu::index2color(*((int *) Data));
		break;
	case LOCALE_GLCD_SELECT_BG:
		g_settings.glcd_color_bg = GLCD_Menu::index2color(*((int *) Data));
		break;
	case LOCALE_GLCD_SELECT_BAR:
		g_settings.glcd_color_bar = GLCD_Menu::index2color(*((int *) Data));
		break;
	case LOCALE_GLCD_ENABLE:
		if (g_settings.glcd_enable)
			nglcd->Resume();
		else
			nglcd->Suspend();
		return true;
	case LOCALE_GLCD_MIRROR_OSD:
		nglcd->MirrorOSD(*((int *) Data));
		break;
	case LOCALE_GLCD_MIRROR_VIDEO:
		nglcd->Update();
		break;
	case LOCALE_GLCD_BRIGHTNESS:
	case LOCALE_GLCD_BRIGHTNESS_STANDBY:
	case LOCALE_GLCD_SHOW_LOGO:
	case LOCALE_GLCD_SIZE_BAR:
	case LOCALE_GLCD_SIZE_CHANNEL:
	case LOCALE_GLCD_SIZE_EPG:
	case LOCALE_GLCD_SIZE_LOGO:
	case LOCALE_GLCD_SIZE_TIME:
	case LOCALE_GLCD_SIZE_TIME_STANDBY:
	case LOCALE_GLCD_TIME_IN_STANDBY:
	case LOCALE_GLCD_SCROLL_SPEED:
		break;
	default:
		return false;
	}
	if (((OptionName == LOCALE_GLCD_TIME_IN_STANDBY || OptionName == LOCALE_GLCD_BRIGHTNESS_STANDBY) && g_settings.glcd_percent_time_standby) || OptionName == LOCALE_GLCD_SIZE_TIME_STANDBY)
		nglcd->StandbyMode(true);
	else
		nglcd->StandbyMode(false);

	nglcd->Update();
	return true;
}

#define ONOFFSEC_OPTION_COUNT 3
static const CMenuOptionChooser::keyval ONOFFSEC_OPTIONS[ONOFFSEC_OPTION_COUNT] = {
	{ 0, LOCALE_OPTIONS_OFF },
	{ 1, LOCALE_OPTIONS_ON },
//	{ 2, LOCALE_CLOCK_SECONDS },
	{ 3, LOCALE_OPTIONS_ON } // FIXME
};

void GLCD_Menu::GLCD_Menu_Settings()
{
	int color_bg = color2index(g_settings.glcd_color_bg);
	int color_fg = color2index(g_settings.glcd_color_fg);
	int color_bar = color2index(g_settings.glcd_color_bar);

	CMenuWidget m(LOCALE_GLCD_HEAD, NEUTRINO_ICON_SETTINGS, width);
	m.addIntroItems();
	m.setSelected(selected);

	m.addItem(new CMenuOptionChooser(LOCALE_GLCD_ENABLE, &g_settings.glcd_enable,
				OPTIONS_OFF0_ON1_OPTIONS, OPTIONS_OFF0_ON1_OPTION_COUNT, true, this));
	int shortcut = 1;
	m.addItem(GenericMenuSeparatorLine);
	m.addItem(new CMenuOptionChooser(LOCALE_GLCD_SELECT_FG, &color_fg,
				GLCD_COLOR_OPTIONS, GLCD_COLOR_OPTION_COUNT, true, this,
				CRCInput::convertDigitToKey(shortcut++)));
	m.addItem(new CMenuOptionChooser(LOCALE_GLCD_SELECT_BG, &color_bg,
				GLCD_COLOR_OPTIONS, GLCD_COLOR_OPTION_COUNT, true, this,
				CRCInput::convertDigitToKey(shortcut++)));
	m.addItem(new CMenuOptionChooser(LOCALE_GLCD_SELECT_BAR, &color_bar,
				GLCD_COLOR_OPTIONS, GLCD_COLOR_OPTION_COUNT, true, this,
				CRCInput::convertDigitToKey(shortcut++)));
	m.addItem(new CMenuForwarder(LOCALE_GLCD_FONT, true, g_settings.glcd_font, this, "select_font",
				CRCInput::convertDigitToKey(shortcut++)));
	m.addItem(new CMenuOptionNumberChooser(LOCALE_GLCD_SIZE_CHANNEL,
				&g_settings.glcd_percent_channel, true, 0, 100, this));
	m.addItem(new CMenuOptionNumberChooser(LOCALE_GLCD_SIZE_EPG,
				&g_settings.glcd_percent_epg, true, 0, 100, this));
	m.addItem(new CMenuOptionNumberChooser(LOCALE_GLCD_SIZE_BAR,
				&g_settings.glcd_percent_bar, true, 0, 100, this));
	m.addItem(new CMenuOptionNumberChooser(LOCALE_GLCD_SIZE_TIME,
				&g_settings.glcd_percent_time, true, 0, 100, this));
	m.addItem(new CMenuOptionChooser(LOCALE_GLCD_SHOW_LOGO, &g_settings.glcd_show_logo,
				OPTIONS_OFF0_ON1_OPTIONS, OPTIONS_OFF0_ON1_OPTION_COUNT, true, this,
				CRCInput::convertDigitToKey(shortcut++)));
	m.addItem(new CMenuOptionNumberChooser(LOCALE_GLCD_SIZE_LOGO,
				&g_settings.glcd_percent_logo, true, 0, 100, this));
	m.addItem(new CMenuOptionNumberChooser(LOCALE_GLCD_BRIGHTNESS,
				&g_settings.glcd_brightness, true, 0, 100, this));
	m.addItem(GenericMenuSeparatorLine);
	m.addItem(new CMenuOptionChooser(LOCALE_GLCD_TIME_IN_STANDBY, &g_settings.glcd_time_in_standby,
				ONOFFSEC_OPTIONS, ONOFFSEC_OPTION_COUNT, true, this,
				CRCInput::convertDigitToKey(shortcut++)));
	m.addItem(new CMenuOptionNumberChooser(LOCALE_GLCD_SIZE_TIME_STANDBY,
				&g_settings.glcd_percent_time_standby, true, 0, 100, this));
	m.addItem(new CMenuOptionNumberChooser(LOCALE_GLCD_BRIGHTNESS_STANDBY,
				&g_settings.glcd_brightness_standby, true, 0, 100, this));
	m.addItem(GenericMenuSeparatorLine);
	m.addItem(new CMenuOptionNumberChooser(LOCALE_GLCD_SCROLL_SPEED,
				&g_settings.glcd_scroll_speed, true, 1, 63, this));
	m.addItem(GenericMenuSeparatorLine);
	m.addItem(new CMenuOptionChooser(LOCALE_GLCD_MIRROR_OSD, &g_settings.glcd_mirror_osd,
				OPTIONS_OFF0_ON1_OPTIONS, OPTIONS_OFF0_ON1_OPTION_COUNT, true, this, CRCInput::RC_green));
	m.addItem(new CMenuOptionChooser(LOCALE_GLCD_MIRROR_VIDEO, &g_settings.glcd_mirror_video,
				OPTIONS_OFF0_ON1_OPTIONS, OPTIONS_OFF0_ON1_OPTION_COUNT, true, this, CRCInput::RC_yellow));
	m.addItem(GenericMenuSeparatorLine);
	m.addItem(new CMenuForwarder(LOCALE_GLCD_RESTART, true, NULL, this, "rescan", CRCInput::RC_red));
	m.exec(NULL, "");
	selected = m.getSelected();
	nGLCD::getInstance()->StandbyMode(false);
	m.hide();
}
