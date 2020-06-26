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
#include <driver/glcd/glcd.h>
#include <driver/screen_max.h>
#include <system/helpers.h>
#include "glcdsetup.h"
#include <mymenu.h>
#include <gui/widget/colorchooser.h>
#include <neutrino_menue.h>
#include "glcdthemes.h"

#define ONOFFSEC_OPTION_COUNT 6
static const CMenuOptionChooser::keyval ONOFFSEC_OPTIONS[ONOFFSEC_OPTION_COUNT] = {
	{ cGLCD::CLOCK_OFF,	LOCALE_OPTIONS_OFF },
	{ cGLCD::CLOCK_SIMPLE,	LOCALE_GLCD_STANDBY_SIMPLE_CLOCK },
	{ cGLCD::CLOCK_LED,	LOCALE_GLCD_STANDBY_LED_CLOCK },
	{ cGLCD::CLOCK_LCD,	LOCALE_GLCD_STANDBY_LCD_CLOCK },
	{ cGLCD::CLOCK_DIGITAL,	LOCALE_GLCD_STANDBY_DIGITAL_CLOCK },
	{ cGLCD::CLOCK_ANALOG,	LOCALE_GLCD_STANDBY_ANALOG_CLOCK }
};

#define ONOFFPRI_OPTION_COUNT 4
static const CMenuOptionChooser::keyval ONOFFPRI_OPTIONS[ONOFFPRI_OPTION_COUNT] = {
	{ 0, LOCALE_GLCD_ALIGN_NONE },
	{ 1, LOCALE_GLCD_ALIGN_LEFT },
	{ 2, LOCALE_GLCD_ALIGN_CENTER },
	{ 3, LOCALE_GLCD_ALIGN_RIGHT },
};

#if 0
#define KEY_GLCD_BLACK			0
#define KEY_GLCD_WHITE			1
#define KEY_GLCD_RED			2
#define KEY_GLCD_PINK			3
#define KEY_GLCD_PURPLE			4
#define KEY_GLCD_DEEPPURPLE		5
#define KEY_GLCD_INDIGO			6
#define KEY_GLCD_BLUE			7
#define KEY_GLCD_LIGHTBLUE		8
#define KEY_GLCD_CYAN			9
#define KEY_GLCD_TEAL			10
#define KEY_GLCD_GREEN			11
#define KEY_GLCD_LIGHTGREEN		12
#define KEY_GLCD_LIME			13
#define KEY_GLCD_YELLOW			14
#define KEY_GLCD_AMBER			15
#define KEY_GLCD_ORANGE			16
#define KEY_GLCD_DEEPORANGE		17
#define KEY_GLCD_BROWN			18
#define KEY_GLCD_GRAY			19
#define KEY_GLCD_BLUEGRAY		20

#define GLCD_COLOR_OPTION_COUNT		21

static const CMenuOptionChooser::keyval GLCD_COLOR_OPTIONS[GLCD_COLOR_OPTION_COUNT] =
{
	{ KEY_GLCD_BLACK,	LOCALE_GLCD_COLOR_BLACK },
	{ KEY_GLCD_WHITE,	LOCALE_GLCD_COLOR_WHITE },
	{ KEY_GLCD_RED,		LOCALE_GLCD_COLOR_RED },
	{ KEY_GLCD_PINK,	LOCALE_GLCD_COLOR_PINK },
	{ KEY_GLCD_PURPLE,	LOCALE_GLCD_COLOR_PURPLE },
	{ KEY_GLCD_DEEPPURPLE,	LOCALE_GLCD_COLOR_DEEPPURPLE },
	{ KEY_GLCD_INDIGO,	LOCALE_GLCD_COLOR_INDIGO },
	{ KEY_GLCD_BLUE,	LOCALE_GLCD_COLOR_BLUE },
	{ KEY_GLCD_LIGHTBLUE,	LOCALE_GLCD_COLOR_LIGHTBLUE },
	{ KEY_GLCD_CYAN,	LOCALE_GLCD_COLOR_CYAN },
	{ KEY_GLCD_TEAL,	LOCALE_GLCD_COLOR_TEAL },
	{ KEY_GLCD_GREEN,	LOCALE_GLCD_COLOR_GREEN },
	{ KEY_GLCD_LIGHTGREEN,	LOCALE_GLCD_COLOR_LIGHTGREEN },
	{ KEY_GLCD_LIME,	LOCALE_GLCD_COLOR_LIME },
	{ KEY_GLCD_YELLOW,	LOCALE_GLCD_COLOR_YELLOW },
	{ KEY_GLCD_AMBER,	LOCALE_GLCD_COLOR_AMBER },
	{ KEY_GLCD_ORANGE,	LOCALE_GLCD_COLOR_ORANGE },
	{ KEY_GLCD_DEEPORANGE,	LOCALE_GLCD_COLOR_DEEPORANGE },
	{ KEY_GLCD_BROWN,	LOCALE_GLCD_COLOR_BROWN },
	{ KEY_GLCD_GRAY,	LOCALE_GLCD_COLOR_GRAY },
	{ KEY_GLCD_BLUEGRAY,	LOCALE_GLCD_COLOR_BLUEGRAY },
};

static const uint32_t colormap[GLCD_COLOR_OPTION_COUNT] =
{
	GLCD::cColor::Black,
	GLCD::cColor::White,
	GLCD::cColor::Red,
	GLCD::cColor::Pink,
	GLCD::cColor::Purple,
	GLCD::cColor::DeepPurple,
	GLCD::cColor::Indigo,
	GLCD::cColor::Blue,
	GLCD::cColor::LightBlue,
	GLCD::cColor::Cyan,
	GLCD::cColor::Teal,
	GLCD::cColor::Green,
	GLCD::cColor::LightGreen,
	GLCD::cColor::Lime,
	GLCD::cColor::Yellow,
	GLCD::cColor::Amber,
	GLCD::cColor::Orange,
	GLCD::cColor::DeepOrange,
	GLCD::cColor::Brown,
	GLCD::cColor::Gray,
	GLCD::cColor::BlueGray
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
#endif

GLCD_Menu::GLCD_Menu()
{
	width = 40;
	selected = -1;

	select_driver = NULL;
}

int GLCD_Menu::exec(CMenuTarget* parent, const std::string & actionKey)
{
	int res = menu_return::RETURN_REPAINT;
	cGLCD *cglcd = cGLCD::getInstance();
	SNeutrinoGlcdTheme &t = g_settings.glcd_theme;

	if (parent)
		parent->hide();

	if (actionKey == "rescan")
	{
		cglcd->Rescan();
		return res;
	}
	else if (actionKey == "select_font")
	{
		CFileBrowser fileBrowser;
		CFileFilter fileFilter;
		fileFilter.addFilter("ttf");
		fileBrowser.Filter = &fileFilter;
		if (fileBrowser.exec(FONTDIR) == true)
		{
			t.glcd_font = fileBrowser.getSelectedFile()->Name;
			cglcd->Rescan();
		}
		return res;
	}
	else if (actionKey == "brightness_default")
	{
		g_settings.glcd_brightness = GLCD_DEFAULT_BRIGHTNESS;
		g_settings.glcd_brightness_standby = GLCD_DEFAULT_BRIGHTNESS_STANDBY;
		g_settings.glcd_brightness_dim = GLCD_DEFAULT_BRIGHTNESS_DIM;
		g_settings.glcd_brightness_dim_time = GLCD_DEFAULT_BRIGHTNESS_DIM_TIME;
		cglcd->UpdateBrightness();
		return res;
	}
	else if (actionKey == "select_driver")
	{
		return GLCD_Menu_Select_Driver();
	}
	else if (actionKey == "theme_settings")
	{
		return GLCD_Theme_Settings();
	}
	else if (actionKey == "brightness_settings")
	{
		return GLCD_Brightness_Settings();
	}
	else if (actionKey == "standby_settings")
	{
		return GLCD_Standby_Settings();
        }
	else if (actionKey == "position_settings")
	{
		return GLCD_Theme_Position_Settings();
	}
	else
	{
		return GLCD_Menu_Settings();
	}

	return res;
}

void GLCD_Menu::hide()
{
}

bool GLCD_Menu::changeNotify (const neutrino_locale_t OptionName, void *Data)
{
	if (!Data)
		return false;
	cGLCD *cglcd = cGLCD::getInstance();
	switch(OptionName) {
	case LOCALE_GLCD_SELECT_FG:
	case LOCALE_GLCD_SELECT_BG:
	case LOCALE_GLCD_SELECT_BAR:
		cglcd->Update();
		break;
	case LOCALE_GLCD_ENABLE:
		if (g_settings.glcd_enable)
			cglcd->Resume();
		else
			cglcd->Suspend();
		return true;
	case LOCALE_GLCD_MIRROR_OSD:
		cglcd->MirrorOSD(*((int *) Data));
		break;
	case LOCALE_GLCD_MIRROR_VIDEO:
		cglcd->Update();
		break;
	case LOCALE_GLCD_BRIGHTNESS:
	case LOCALE_GLCD_BRIGHTNESS_STANDBY:
	case LOCALE_GLCD_BRIGHTNESS_DIM:
	case LOCALE_GLCD_BRIGHTNESS_DIM_TIME:
		cglcd->Update();
		break;
	case LOCALE_GLCD_SHOW_PROGRESSBAR:
	case LOCALE_GLCD_SHOW_DURATION:
	case LOCALE_GLCD_SHOW_START:
	case LOCALE_GLCD_SHOW_END:
	case LOCALE_GLCD_SHOW_TIME:
	case LOCALE_GLCD_SHOW_WEATHER:
	case LOCALE_GLCD_SHOW_LOGO:
	case LOCALE_GLCD_SIZE_BAR:
	case LOCALE_GLCD_BAR_X_POSITION:
	case LOCALE_GLCD_BAR_Y_POSITION:
	case LOCALE_GLCD_BAR_WIDTH:
	case LOCALE_GLCD_SIZE_CHANNEL:
	case LOCALE_GLCD_CHANNEL_X_POSITION:
	case LOCALE_GLCD_CHANNEL_Y_POSITION:
	case LOCALE_GLCD_SIZE_EPG:
	case LOCALE_GLCD_EPG_X_POSITION:
	case LOCALE_GLCD_EPG_Y_POSITION:
	case LOCALE_GLCD_SIZE_DURATION:
	case LOCALE_GLCD_DURATION_X_POSITION:
	case LOCALE_GLCD_DURATION_Y_POSITION:
	case LOCALE_GLCD_SIZE_START:
	case LOCALE_GLCD_START_X_POSITION:
	case LOCALE_GLCD_START_Y_POSITION:
	case LOCALE_GLCD_SIZE_END:
	case LOCALE_GLCD_END_X_POSITION:
	case LOCALE_GLCD_END_Y_POSITION:
	case LOCALE_GLCD_SIZE_LOGO:
	case LOCALE_GLCD_LOGO_X_POSITION:
	case LOCALE_GLCD_LOGO_Y_POSITION:
	case LOCALE_GLCD_SIZE_TIME:
	case LOCALE_GLCD_TIME_X_POSITION:
	case LOCALE_GLCD_TIME_Y_POSITION:
	case LOCALE_GLCD_TIME_IN_STANDBY:
	case LOCALE_GLCD_SCROLL_SPEED:
	case LOCALE_GLCD_THEME_POSITION_SETTINGS:
		break;
	default:
		return false;
	}

	if (OptionName == LOCALE_GLCD_TIME_IN_STANDBY || OptionName == LOCALE_GLCD_BRIGHTNESS_STANDBY || OptionName == LOCALE_GLCD_STANDBY_LED_CLOCK || OptionName == LOCALE_GLCD_STANDBY_LCD_CLOCK
	 || OptionName == LOCALE_GLCD_STANDBY_DIGITAL_CLOCK || OptionName == LOCALE_GLCD_STANDBY_DIGITAL_CLOCK || OptionName == LOCALE_GLCD_STANDBY_ANALOG_CLOCK || OptionName == LOCALE_GLCD_STANDBY_WEATHER
	 || OptionName == LOCALE_GLCD_DIGITAL_CLOCK_Y_POSITION || OptionName == LOCALE_GLCD_SIZE_SIMPLE_CLOCK || OptionName == LOCALE_GLCD_SIMPLE_CLOCK_Y_POSITION)
		cglcd->StandbyMode(true);
	else
		cglcd->StandbyMode(false);

	cglcd->Update();
	return true;
}

int GLCD_Menu::GLCD_Menu_Settings()
{
	CMenuWidget m(LOCALE_GLCD_HEAD, NEUTRINO_ICON_SETTINGS, width);
	m.addIntroItems();
	m.setSelected(selected);

	sigc::slot0<void> slot_repaint = sigc::mem_fun(m, &CMenuWidget::paint); //we want to repaint after changed Option

	m.addItem(new CMenuOptionChooser(LOCALE_GLCD_ENABLE, &g_settings.glcd_enable,
				OPTIONS_OFF0_ON1_OPTIONS, OPTIONS_OFF0_ON1_OPTION_COUNT, true, this, CRCInput::RC_red));

	select_driver = new CMenuForwarder(LOCALE_GLCD_DISPLAY, (cGLCD::getInstance()->GetConfigSize() > 1),
				cGLCD::getInstance()->GetConfigName(g_settings.glcd_selected_config).c_str(), this, "select_driver");
	m.addItem(select_driver);

	m.addItem(GenericMenuSeparatorLine);

	int shortcut = 1;
	m.addItem(new CMenuForwarder(LOCALE_GLCD_THEME_SETTINGS, true, NULL, this, "theme_settings",
				CRCInput::convertDigitToKey(shortcut++)));
	m.addItem(new CMenuForwarder(LOCALE_GLCD_BRIGHTNESS_SETTINGS, true, NULL, this, "brightness_settings",
				CRCInput::convertDigitToKey(shortcut++)));
	m.addItem(new CMenuForwarder(LOCALE_GLCD_STANDBY_SETTINGS, true, NULL, this, "standby_settings",
				CRCInput::convertDigitToKey(shortcut++)));
	m.addItem(new CMenuOptionChooser(LOCALE_GLCD_SHOW_LOGO, &g_settings.glcd_show_logo,
				OPTIONS_OFF0_ON1_OPTIONS, OPTIONS_OFF0_ON1_OPTION_COUNT, true, this,
				CRCInput::convertDigitToKey(shortcut++)));
	m.addItem(new CMenuOptionNumberChooser(LOCALE_GLCD_SCROLL_SPEED,
				&g_settings.glcd_scroll_speed, true, 1, 63, this));
	m.addItem(GenericMenuSeparatorLine);

	m.addItem(new CMenuOptionChooser(LOCALE_GLCD_MIRROR_OSD, &g_settings.glcd_mirror_osd,
				OPTIONS_OFF0_ON1_OPTIONS, OPTIONS_OFF0_ON1_OPTION_COUNT, true, this, CRCInput::RC_green));
	m.addItem(new CMenuOptionChooser(LOCALE_GLCD_MIRROR_VIDEO, &g_settings.glcd_mirror_video,
				OPTIONS_OFF0_ON1_OPTIONS, OPTIONS_OFF0_ON1_OPTION_COUNT, true, this, CRCInput::RC_yellow));
	m.addItem(GenericMenuSeparatorLine);

	m.addItem(new CMenuForwarder(LOCALE_GLCD_RESTART, true, NULL, this, "rescan", CRCInput::RC_blue));

	int res = m.exec(NULL, "");
	selected = m.getSelected();
	cGLCD::getInstance()->StandbyMode(false);
	m.hide();
	return res;
}

int GLCD_Menu::GLCD_Standby_Settings()
{
	CMenuWidget m(LOCALE_GLCD_STANDBY_SETTINGS, NEUTRINO_ICON_SETTINGS, width);
	m.addIntroItems();
	m.setSelected(selected);

	SNeutrinoGlcdTheme &t = g_settings.glcd_theme;
	sigc::slot0<void> slot_repaint = sigc::mem_fun(m, &CMenuWidget::paint); //we want to repaint after changed Option

	int shortcut = 1;
	m.addItem(new CMenuOptionChooser(LOCALE_GLCD_TIME_IN_STANDBY, &g_settings.glcd_time_in_standby,
				ONOFFSEC_OPTIONS, ONOFFSEC_OPTION_COUNT, true, this,
				CRCInput::convertDigitToKey(shortcut++)));
	m.addItem(new CMenuOptionChooser(LOCALE_GLCD_STANDBY_WEATHER, &g_settings.glcd_standby_weather,
				OPTIONS_OFF0_ON1_OPTIONS, OPTIONS_OFF0_ON1_OPTION_COUNT, true, this));
	if (t.glcd_position_settings) {
		m.addItem(new CMenuOptionNumberChooser(LOCALE_GLCD_DIGITAL_CLOCK_Y_POSITION,
					&t.glcd_digital_clock_y_position, true, 0, 500, this));
		m.addItem(new CMenuOptionNumberChooser(LOCALE_GLCD_SIZE_SIMPLE_CLOCK,
					&t.glcd_size_simple_clock, true, 0, 100, this));
		m.addItem(new CMenuOptionNumberChooser(LOCALE_GLCD_SIMPLE_CLOCK_Y_POSITION,
					&t.glcd_simple_clock_y_position, true, 0, 500, this));
	}
	int res = m.exec(NULL, "");
	selected = m.getSelected();
	cGLCD::getInstance()->StandbyMode(false);
	m.hide();
	return res;
}

int GLCD_Menu::GLCD_Brightness_Settings()
{
	CMenuWidget m(LOCALE_GLCD_BRIGHTNESS_SETTINGS, NEUTRINO_ICON_SETTINGS, width);
	m.addIntroItems();
	m.setSelected(selected);

	sigc::slot0<void> slot_repaint = sigc::mem_fun(m, &CMenuWidget::paint); //we want to repaint after changed Option

	m.addItem(new CMenuOptionNumberChooser(LOCALE_GLCD_BRIGHTNESS,
				&g_settings.glcd_brightness, true, 0, 10, this, CRCInput::RC_nokey, NULL, 0, 0, NONEXISTANT_LOCALE, true));
	m.addItem(new CMenuOptionNumberChooser(LOCALE_GLCD_BRIGHTNESS_STANDBY,
				&g_settings.glcd_brightness_standby, true, 0, 10, this, CRCInput::RC_nokey, NULL, 0, 0, NONEXISTANT_LOCALE, true));
	m.addItem(GenericMenuSeparatorLine);
	m.addItem(new CMenuOptionNumberChooser(LOCALE_GLCD_BRIGHTNESS_DIM,
				&g_settings.glcd_brightness_dim, true, 0, 10, this, CRCInput::RC_nokey, NULL, 0, 0, NONEXISTANT_LOCALE, true));
	CStringInput *dim_time = new CStringInput(LOCALE_GLCD_BRIGHTNESS_DIM_TIME, &g_settings.glcd_brightness_dim_time, 5, NONEXISTANT_LOCALE, NONEXISTANT_LOCALE,"0123456789 ");
	m.addItem(new CMenuForwarder(LOCALE_GLCD_BRIGHTNESS_DIM_TIME, true, g_settings.glcd_brightness_dim_time, dim_time));

	m.addItem(GenericMenuSeparatorLine);
	m.addItem(new CMenuForwarder(LOCALE_OPTIONS_DEFAULT, true, NULL, this, "brightness_default", CRCInput::RC_nokey));

	int res = m.exec(NULL, "");
	selected = m.getSelected();
	cGLCD::getInstance()->StandbyMode(false);
	m.hide();
	return res;
}

int GLCD_Menu::GLCD_Theme_Settings()
{
	CMenuWidget m(LOCALE_GLCD_THEME_SETTINGS, NEUTRINO_ICON_SETTINGS, width);
	m.addIntroItems();
	m.setSelected(selected);

	int shortcut = 1;
	SNeutrinoGlcdTheme &t = g_settings.glcd_theme;
	sigc::slot0<void> slot_repaint = sigc::mem_fun(m, &CMenuWidget::paint); //we want to repaint after changed Option

	m.addItem(new CMenuForwarder(LOCALE_GLCD_THEME, true, NULL, CGLCDThemes::getInstance(), NULL, CRCInput::convertDigitToKey(shortcut++)));

	m.addItem(new CMenuForwarder(LOCALE_GLCD_FONT, true, t.glcd_font, this, "select_font",
				CRCInput::convertDigitToKey(shortcut++)));

	m.addItem(GenericMenuSeparatorLine);
	CColorSetupNotifier *colorSetupNotifier = new CColorSetupNotifier();

	CColorChooser* fg = new CColorChooser(LOCALE_GLCD_SELECT_FG, &t.glcd_color_fg_red, &t.glcd_color_fg_green, &t.glcd_color_fg_blue, NULL, colorSetupNotifier);
	m.addItem(new CMenuDForwarder(LOCALE_GLCD_SELECT_FG, true, NULL, fg));

	CColorChooser* bg = new CColorChooser(LOCALE_GLCD_SELECT_BG, &t.glcd_color_bg_red, &t.glcd_color_bg_green, &t.glcd_color_bg_blue, NULL, colorSetupNotifier);
	m.addItem(new CMenuDForwarder(LOCALE_GLCD_SELECT_BG, true, NULL, bg));

	CColorChooser* bar = new CColorChooser(LOCALE_GLCD_SELECT_BAR, &t.glcd_color_bar_red, &t.glcd_color_bar_green, &t.glcd_color_bar_blue, NULL, colorSetupNotifier);
	m.addItem(new CMenuDForwarder(LOCALE_GLCD_SELECT_BAR, true, NULL, bar));

	if (t.glcd_position_settings)
		m.addItem(new CMenuForwarder(LOCALE_GLCD_POSITION_SETTINGS, true, NULL, this, "position_settings",
					CRCInput::convertDigitToKey(shortcut++)));

	//delete colorSetupNotifier;
	int res = m.exec(NULL, "");
	selected = m.getSelected();
	cGLCD::getInstance()->StandbyMode(false);
	m.hide();
	return res;
}

int GLCD_Menu::GLCD_Theme_Position_Settings()
{
	CMenuWidget m(LOCALE_GLCD_THEME_POSITION_SETTINGS, NEUTRINO_ICON_SETTINGS, width);
	m.addIntroItems();
	m.setSelected(selected);

	SNeutrinoGlcdTheme &t = g_settings.glcd_theme;
	sigc::slot0<void> slot_repaint = sigc::mem_fun(m, &CMenuWidget::paint); //we want to repaint after changed Option

	m.addItem(new CMenuOptionNumberChooser(LOCALE_GLCD_SIZE_CHANNEL,
				&t.glcd_percent_channel, !g_settings.glcd_show_logo, 0, 100, this));
	m.addItem(new CMenuOptionChooser(LOCALE_GLCD_ALIGN_CHANNEL, &t.glcd_align_channel,
				ONOFFPRI_OPTIONS, ONOFFPRI_OPTION_COUNT, !g_settings.glcd_show_logo, NULL));
	m.addItem(new CMenuOptionNumberChooser(LOCALE_GLCD_CHANNEL_X_POSITION,
				&t.glcd_channel_x_position, !g_settings.glcd_show_logo, 0, 500, this));
	m.addItem(new CMenuOptionNumberChooser(LOCALE_GLCD_CHANNEL_Y_POSITION,
				&t.glcd_channel_y_position, !g_settings.glcd_show_logo, 0, 500, this));

	m.addItem(GenericMenuSeparatorLine);

	m.addItem(new CMenuOptionNumberChooser(LOCALE_GLCD_SIZE_LOGO,
				&t.glcd_percent_logo, g_settings.glcd_show_logo, 0, 100, this));
	m.addItem(new CMenuOptionNumberChooser(LOCALE_GLCD_LOGO_X_POSITION,
				&t.glcd_logo_x_position, g_settings.glcd_show_logo, 0, 500, this));
	m.addItem(new CMenuOptionNumberChooser(LOCALE_GLCD_LOGO_Y_POSITION,
				&t.glcd_logo_y_position, g_settings.glcd_show_logo, 0, 500, this));

	m.addItem(GenericMenuSeparatorLine);

	m.addItem(new CMenuOptionNumberChooser(LOCALE_GLCD_SIZE_EPG,
				&t.glcd_percent_epg, true, 0, 100, this));
	m.addItem(new CMenuOptionChooser(LOCALE_GLCD_ALIGN_EPG, &t.glcd_align_epg,
				ONOFFPRI_OPTIONS, ONOFFPRI_OPTION_COUNT, true, NULL));
	m.addItem(new CMenuOptionNumberChooser(LOCALE_GLCD_EPG_X_POSITION,
				&t.glcd_epg_x_position, true, 0, 500, this));
	m.addItem(new CMenuOptionNumberChooser(LOCALE_GLCD_EPG_Y_POSITION,
				&t.glcd_epg_y_position, true, 0, 500, this));

	m.addItem(GenericMenuSeparatorLine);

	m.addItem(new CMenuOptionChooser(LOCALE_GLCD_SHOW_DURATION, &t.glcd_show_duration,
				OPTIONS_OFF0_ON1_OPTIONS, OPTIONS_OFF0_ON1_OPTION_COUNT, true, this));
	m.addItem(new CMenuOptionNumberChooser(LOCALE_GLCD_SIZE_DURATION,
				&t.glcd_percent_duration, true, 0, 100, this));
	m.addItem(new CMenuOptionChooser(LOCALE_GLCD_ALIGN_DURATION, &t.glcd_align_duration,
				ONOFFPRI_OPTIONS, ONOFFPRI_OPTION_COUNT, true, NULL));
	m.addItem(new CMenuOptionNumberChooser(LOCALE_GLCD_DURATION_X_POSITION,
				&t.glcd_duration_x_position, true, 0, 500, this));
	m.addItem(new CMenuOptionNumberChooser(LOCALE_GLCD_DURATION_Y_POSITION,
				&t.glcd_duration_y_position, true, 0, 500, this));

	m.addItem(GenericMenuSeparatorLine);

	m.addItem(new CMenuOptionChooser(LOCALE_GLCD_SHOW_START, &t.glcd_show_start,
				OPTIONS_OFF0_ON1_OPTIONS, OPTIONS_OFF0_ON1_OPTION_COUNT, true, this));
	m.addItem(new CMenuOptionNumberChooser(LOCALE_GLCD_SIZE_START,
				&t.glcd_percent_start, true, 0, 100, this));
	m.addItem(new CMenuOptionChooser(LOCALE_GLCD_ALIGN_START, &t.glcd_align_start,
				ONOFFPRI_OPTIONS, ONOFFPRI_OPTION_COUNT, true, NULL));
	m.addItem(new CMenuOptionNumberChooser(LOCALE_GLCD_START_X_POSITION,
				&t.glcd_start_x_position, true, 0, 500, this));
	m.addItem(new CMenuOptionNumberChooser(LOCALE_GLCD_START_Y_POSITION,
				&t.glcd_start_y_position, true, 0, 500, this));

	m.addItem(GenericMenuSeparatorLine);

	m.addItem(new CMenuOptionChooser(LOCALE_GLCD_SHOW_END, &t.glcd_show_end,
				OPTIONS_OFF0_ON1_OPTIONS, OPTIONS_OFF0_ON1_OPTION_COUNT, true, this));
	m.addItem(new CMenuOptionNumberChooser(LOCALE_GLCD_SIZE_END,
				&t.glcd_percent_end, true, 0, 100, this));
	m.addItem(new CMenuOptionChooser(LOCALE_GLCD_ALIGN_END, &t.glcd_align_end,
				ONOFFPRI_OPTIONS, ONOFFPRI_OPTION_COUNT, true, NULL));
	m.addItem(new CMenuOptionNumberChooser(LOCALE_GLCD_END_X_POSITION,
				&t.glcd_end_x_position, true, 0, 500, this));
	m.addItem(new CMenuOptionNumberChooser(LOCALE_GLCD_END_Y_POSITION,
				&t.glcd_end_y_position, true, 0, 500, this));

	m.addItem(GenericMenuSeparatorLine);

	m.addItem(new CMenuOptionChooser(LOCALE_GLCD_SHOW_TIME, &t.glcd_show_time,
				OPTIONS_OFF0_ON1_OPTIONS, OPTIONS_OFF0_ON1_OPTION_COUNT, true, this));
	m.addItem(new CMenuOptionNumberChooser(LOCALE_GLCD_SIZE_TIME,
				&t.glcd_percent_time, true, 0, 100, this));
	m.addItem(new CMenuOptionChooser(LOCALE_GLCD_ALIGN_TIME, &t.glcd_align_time,
				ONOFFPRI_OPTIONS, ONOFFPRI_OPTION_COUNT, true, NULL));
	m.addItem(new CMenuOptionNumberChooser(LOCALE_GLCD_TIME_X_POSITION,
				&t.glcd_time_x_position, true, 0, 500, this));
	m.addItem(new CMenuOptionNumberChooser(LOCALE_GLCD_TIME_Y_POSITION,
				&t.glcd_time_y_position, true, 0, 500, this));

	m.addItem(GenericMenuSeparatorLine);


	m.addItem(new CMenuOptionChooser(LOCALE_GLCD_SHOW_PROGRESSBAR, &t.glcd_show_progressbar,
				OPTIONS_OFF0_ON1_OPTIONS, OPTIONS_OFF0_ON1_OPTION_COUNT, true, this));
	m.addItem(new CMenuOptionNumberChooser(LOCALE_GLCD_SIZE_BAR,
				&t.glcd_percent_bar, true, 0, 100, this));
	m.addItem(new CMenuOptionNumberChooser(LOCALE_GLCD_BAR_WIDTH,
				&t.glcd_bar_width, true, 0, 500, this));
	m.addItem(new CMenuOptionNumberChooser(LOCALE_GLCD_BAR_X_POSITION,
				&t.glcd_bar_x_position, true, 0, 500, this));
	m.addItem(new CMenuOptionNumberChooser(LOCALE_GLCD_BAR_Y_POSITION,
				&t.glcd_bar_y_position, true, 0, 500, this));

	m.addItem(GenericMenuSeparatorLine);

	m.addItem(new CMenuOptionChooser(LOCALE_GLCD_SHOW_WEATHER, &t.glcd_show_weather,
				OPTIONS_OFF0_ON1_OPTIONS, OPTIONS_OFF0_ON1_OPTION_COUNT, true, this));

	int res = m.exec(NULL, "");
	selected = m.getSelected();
	cGLCD::getInstance()->StandbyMode(false);
	m.hide();
	return res;
}

int GLCD_Menu::GLCD_Menu_Select_Driver()
{
	int select = 0;
	int res = menu_return::RETURN_NONE;

	if (cGLCD::getInstance()->GetConfigSize() > 1)
	{
		CMenuWidget *m = new CMenuWidget(LOCALE_GLCD_HEAD, NEUTRINO_ICON_SETTINGS);
		CMenuSelectorTarget * selector = new CMenuSelectorTarget(&select);

		// we don't show introitems, so we add a separator for a smoother view
		m->addItem(GenericMenuSeparator);

		CMenuForwarder* mf;
		for (int i = 0; i < cGLCD::getInstance()->GetConfigSize(); i++)
		{
			mf = new CMenuForwarder(cGLCD::getInstance()->GetConfigName(i), true, NULL, selector, to_string(i).c_str());
			mf->setInfoIconRight(i == g_settings.glcd_selected_config ? NEUTRINO_ICON_MARKER_DIALOG_OK : NULL);
			m->addItem(mf);
		}

		m->enableSaveScreen();
		res = m->exec(NULL, "");

		delete selector;

		if (!m->gotAction() || g_settings.glcd_selected_config == select)
			return res;
	}
	g_settings.glcd_selected_config = select;
	select_driver->setOption(cGLCD::getInstance()->GetConfigName(g_settings.glcd_selected_config).c_str());
	cGLCD::getInstance()->Respawn();
	return menu_return::RETURN_REPAINT;
}
