/*
	lcd4l setup

	Copyright (C) 2012 'defans'
	Homepage: http://www.bluepeercrew.us/

	Copyright (C) 2012-2018 'vanhofen'
	Homepage: http://www.neutrino-images.de/

	Modded	  (C) 2016 'TangoCash'
		  (C) 2021, Thilo Graf 'dbt'

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
	along with this program. If not, see <http://www.gnu.org/licenses/>.
*/

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include <iostream>
#include <fstream>
#include <sstream>
#include <signal.h>
#include <unistd.h>

#include <global.h>
#include <neutrino.h>
#include <neutrino_menue.h>

#include <gui/filebrowser.h>
#include <gui/widget/hintbox.h>
#include <gui/widget/icons.h>
#include <gui/widget/menue.h>
#include <gui/widget/menue_options.h>

#include <gui/lcd4l_setup.h>

#include <system/helpers.h>

#include <driver/screen_max.h>

#include "driver/lcd4l.h"

const CMenuOptionChooser::keyval LCD4L_SUPPORT_OPTIONS[] =
{
	{ 0, LOCALE_LCD4L_SUPPORT_OFF },
	{ 1, LOCALE_LCD4L_SUPPORT_AUTO },
	{ 2, LOCALE_LCD4L_SUPPORT_ON }
};
#define LCD4L_SUPPORT_OPTION_COUNT (sizeof(LCD4L_SUPPORT_OPTIONS)/sizeof(CMenuOptionChooser::keyval))

const CMenuOptionChooser::keyval_ext LCD4L_DISPLAY_TYPE_OPTIONS[] =
{
	{ CLCD4l::PEARL320x240,    NONEXISTANT_LOCALE, "320x240 Pearl DPF"},
	{ CLCD4l::SAMSUNG800x480,  NONEXISTANT_LOCALE, "800x480 Samsung SPF"},
	{ CLCD4l::SAMSUNG800x600,  NONEXISTANT_LOCALE, "800x600 Samsung SPF"},
	{ CLCD4l::SAMSUNG1024x600, NONEXISTANT_LOCALE, "1024x600 Samsung SPF"},
#if defined BOXMODEL_VUSOLO4K
	{ CLCD4l::VUSOLO4K480x320, NONEXISTANT_LOCALE, "480x320 VUSolo4K"},
#endif
	{ CLCD4l::PNG800x480,      NONEXISTANT_LOCALE, "800x480 PNG"},
	{ CLCD4l::PNG800x600,      NONEXISTANT_LOCALE, "800x600 PNG"},
	{ CLCD4l::PNG1024x600,     NONEXISTANT_LOCALE, "1024x600 PNG"}
};
#define LCD4L_DISPLAY_TYPE_OPTION_COUNT (sizeof(LCD4L_DISPLAY_TYPE_OPTIONS)/sizeof(CMenuOptionChooser::keyval_ext))

const CMenuOptionChooser::keyval LCD4L_SKIN_OPTIONS[] =
{
	{ 0, LOCALE_LCD4L_SKIN_0 },
	{ 1, LOCALE_LCD4L_SKIN_1 },
	{ 2, LOCALE_LCD4L_SKIN_2 },
	{ 3, LOCALE_LCD4L_SKIN_3 },
	{ 4, LOCALE_LCD4L_SKIN_4 }
};
#define LCD4L_SKIN_OPTION_COUNT (sizeof(LCD4L_SKIN_OPTIONS)/sizeof(CMenuOptionChooser::keyval))

using namespace sigc;

CLCD4lSetup::CLCD4lSetup()
{
	width = 40;
	hint = NULL;
	lcd4l_display_type_changed = false;

	sl_start = bind(mem_fun(*this, &CLCD4lSetup::showHint), "Starting lcd service...");
	sl_stop = bind(mem_fun(*this, &CLCD4lSetup::showHint), "Stopping lcd service...");
	sl_restart = bind(mem_fun(*this, &CLCD4lSetup::showHint), "Restarting lcd service...");
	sl_remove = mem_fun(*this, &CLCD4lSetup::removeHint);
	connectSlots();
}

CLCD4lSetup::~CLCD4lSetup()
{
	removeHint();
}

CLCD4lSetup* CLCD4lSetup::getInstance()
{
	static CLCD4lSetup* me = NULL;

	if(!me)
		me = new CLCD4lSetup();

	return me;
}

int CLCD4lSetup::exec(CMenuTarget *parent, const std::string &actionkey)
{
	printf("CLCD4lSetup::exec: actionkey %s\n", actionkey.c_str());
	int res = menu_return::RETURN_REPAINT;

	if (parent)
		parent->hide();

	if (actionkey == "lcd4l_logodir")
	{
		const char *action_str = "lcd4l_logodir";
		chooserDir(g_settings.lcd4l_logodir, false, action_str);
		return menu_return::RETURN_REPAINT;
	}
	else if (actionkey == "typeSetup")
	{
		return showTypeSetup();
	}

	res = show();

	return res;
}

bool CLCD4lSetup::changeNotify(const neutrino_locale_t OptionName, void * /*data*/)
{
#if 0
	int value = 0;

	if (data)
		value = (*(int *)data);
#endif

	if (ARE_LOCALES_EQUAL(OptionName, LOCALE_LCD4L_SUPPORT))
	{
		CLCD4l::getInstance()->StopLCD4l();
		if (g_settings.lcd4l_support)
			CLCD4l::getInstance()->StartLCD4l();
	}
	else if (ARE_LOCALES_EQUAL(OptionName, LOCALE_LCD4L_DISPLAY_TYPE))
	{
		g_settings.lcd4l_display_type = temp_lcd4l_display_type;
		lcd4l_display_type_changed = true;
	}

	return false;
}

int CLCD4lSetup::show()
{
	int shortcut = 1;

	temp_lcd4l_display_type = g_settings.lcd4l_display_type;
	temp_lcd4l_skin = g_settings.lcd4l_skin;
	temp_lcd4l_brightness = g_settings.lcd4l_brightness;

	CMenuOptionChooser *mc;
	CMenuForwarder *mf;

	// lcd4l setup
	CMenuWidget *lcd4lSetup = new CMenuWidget(LOCALE_MISCSETTINGS_HEAD, NEUTRINO_ICON_SETTINGS, width, MN_WIDGET_ID_LCD4L_SETUP);
	lcd4lSetup->addIntroItems(LOCALE_LCD4L_SUPPORT);

	mc = new CMenuOptionChooser(LOCALE_LCD4L_SUPPORT, &g_settings.lcd4l_support, LCD4L_SUPPORT_OPTIONS, LCD4L_SUPPORT_OPTION_COUNT, true, this, CRCInput::RC_red);
	mc->setHint(NEUTRINO_ICON_HINT_LCD4LINUX, LOCALE_MENU_HINT_LCD4L_SUPPORT);
	lcd4lSetup->addItem(mc);

	lcd4lSetup->addItem(GenericMenuSeparatorLine);

	mc = new CMenuOptionChooser(LOCALE_LCD4L_DISPLAY_TYPE, &temp_lcd4l_display_type, LCD4L_DISPLAY_TYPE_OPTIONS, LCD4L_DISPLAY_TYPE_OPTION_COUNT, true, this, CRCInput::RC_green);
	mc->setHint(NEUTRINO_ICON_HINT_LCD4LINUX, LOCALE_MENU_HINT_LCD4L_DISPLAY_TYPE);
	lcd4lSetup->addItem(mc);

	mf = new CMenuForwarder(LOCALE_LCD4L_DISPLAY_TYPE_SETUP, true, NULL, this, "typeSetup", CRCInput::RC_yellow);
	mf->setHint(NEUTRINO_ICON_HINT_LCD4LINUX, LOCALE_MENU_HINT_LCD4L_DISPLAY_TYPE_SETUP);
	lcd4lSetup->addItem(mf);

	lcd4lSetup->addItem(GenericMenuSeparatorLine);

	mf = new CMenuForwarder(LOCALE_LCD4L_LOGODIR, true, g_settings.lcd4l_logodir, this, "lcd4l_logodir", CRCInput::convertDigitToKey(shortcut++));
	mf->setHint(NEUTRINO_ICON_HINT_LCD4LINUX, LOCALE_MENU_HINT_LCD4L_LOGODIR);
	lcd4lSetup->addItem(mf);

	lcd4lSetup->addItem(GenericMenuSeparator);

	const char *flag_lcd4l_weather = FLAGDIR "/.lcd-weather";
	int fake_lcd4l_weather = file_exists(flag_lcd4l_weather);
	CTouchFileNotifier *lcd_weather = new CTouchFileNotifier(flag_lcd4l_weather);
	mc = new CMenuOptionChooser(LOCALE_LCD4L_WEATHER, &fake_lcd4l_weather, OPTIONS_OFF0_ON1_OPTIONS, OPTIONS_OFF0_ON1_OPTION_COUNT, g_settings.weather_enabled, lcd_weather, CRCInput::convertDigitToKey(shortcut++));
	mc->setHint(NEUTRINO_ICON_HINT_LCD4LINUX, LOCALE_MENU_HINT_LCD4L_WEATHER);
	lcd4lSetup->addItem(mc);

	const char *flag_lcd4l_clock_a = FLAGDIR "/.lcd-clock_a";
	int fake_lcd4l_clock_a = file_exists(flag_lcd4l_clock_a);
	CTouchFileNotifier *lcd_clock_a = new CTouchFileNotifier(flag_lcd4l_clock_a);
	mc = new CMenuOptionChooser(LOCALE_LCD4L_CLOCK_A, &fake_lcd4l_clock_a, OPTIONS_OFF0_ON1_OPTIONS, OPTIONS_OFF0_ON1_OPTION_COUNT, ((file_exists(LCD4L_ICONSDIR "/clock/analog")) || (file_exists(LCD4L_ICONSDIR_VAR "/clock/analog"))), lcd_clock_a, CRCInput::convertDigitToKey(shortcut++));
	mc->setHint(NEUTRINO_ICON_HINT_LCD4LINUX, LOCALE_MENU_HINT_LCD4L_CLOCK_A);
	lcd4lSetup->addItem(mc);

	lcd4lSetup->addItem(GenericMenuSeparator);

	mc = new CMenuOptionChooser(LOCALE_LCD4L_CONVERT, &g_settings.lcd4l_convert, OPTIONS_OFF0_ON1_OPTIONS, OPTIONS_OFF0_ON1_OPTION_COUNT, true, NULL, CRCInput::convertDigitToKey(shortcut++));
	mc->setHint(NEUTRINO_ICON_HINT_LCD4LINUX, LOCALE_MENU_HINT_LCD4L_CONVERT);
	lcd4lSetup->addItem(mc);

	int res = lcd4lSetup->exec(NULL, "");

	lcd4lSetup->hide();
	if (lcd_clock_a)
		delete lcd_clock_a;
	if (lcd4lSetup)
		delete lcd4lSetup;
	if(lcd_weather)
		delete lcd_weather;

	// the things to do on exit

	bool initlcd4l = false;

	if ((g_settings.lcd4l_display_type != temp_lcd4l_display_type) || lcd4l_display_type_changed)
	{
		g_settings.lcd4l_display_type = temp_lcd4l_display_type;
		lcd4l_display_type_changed = false;
		initlcd4l = true;
	}

	if (g_settings.lcd4l_skin != temp_lcd4l_skin)
	{
		g_settings.lcd4l_skin = temp_lcd4l_skin;
		initlcd4l = true;
	}

	if (g_settings.lcd4l_brightness != temp_lcd4l_brightness)
	{
		g_settings.lcd4l_brightness = temp_lcd4l_brightness;
		initlcd4l = true;
	}

	if (initlcd4l)
		CLCD4l::getInstance()->InitLCD4l();

	if (g_settings.lcd4l_support == 1 ) // automatic
		CLCD4l::getInstance()->ForceRun();

	return res;
}

int CLCD4lSetup::showTypeSetup()
{
	if (temp_lcd4l_display_type == CLCD4l::PEARL320x240)
	{
		// fix brightness values for Pearl DPF
		if (temp_lcd4l_brightness > 7)
			temp_lcd4l_brightness = 7;
		if (g_settings.lcd4l_brightness_standby > 7)
			g_settings.lcd4l_brightness_standby = 7;
	}

	int shortcut = 1;

	CMenuOptionChooser *mc;
	CMenuOptionNumberChooser *nc;

	CMenuWidget *typeSetup = new CMenuWidget(LOCALE_LCD4L_DISPLAY_TYPE_SETUP, NEUTRINO_ICON_SETTINGS, width);
	typeSetup->addIntroItems(); //FIXME: show lcd4l display type

	mc = new CMenuOptionChooser(LOCALE_LCD4L_SKIN, &temp_lcd4l_skin, LCD4L_SKIN_OPTIONS, LCD4L_SKIN_OPTION_COUNT, true, NULL, CRCInput::convertDigitToKey(shortcut++));
	mc->setHint(NEUTRINO_ICON_HINT_LCD4LINUX, LOCALE_MENU_HINT_LCD4L_SKIN);
	typeSetup->addItem(mc);

	mc = new CMenuOptionChooser(LOCALE_LCD4L_SKIN_RADIO, &g_settings.lcd4l_skin_radio, OPTIONS_OFF0_ON1_OPTIONS, OPTIONS_OFF0_ON1_OPTION_COUNT, true, NULL, CRCInput::convertDigitToKey(shortcut++));
	mc->setHint(NEUTRINO_ICON_HINT_LCD4LINUX, LOCALE_MENU_HINT_LCD4L_SKIN_RADIO);
	typeSetup->addItem(mc);

	nc = new CMenuOptionNumberChooser(LOCALE_LCD4L_BRIGHTNESS, (int *)&temp_lcd4l_brightness, true, 1, CLCD4l::getInstance()->GetMaxBrightness(), this);
	nc->setHint(NEUTRINO_ICON_HINT_LCD4LINUX, LOCALE_MENU_HINT_LCD4L_BRIGHTNESS);
	typeSetup->addItem(nc);

	nc = new CMenuOptionNumberChooser(LOCALE_LCD4L_BRIGHTNESS_STANDBY, (int *)&g_settings.lcd4l_brightness_standby, !g_settings.shutdown_real, 1, CLCD4l::getInstance()->GetMaxBrightness(), this);
	nc->setHint(NEUTRINO_ICON_HINT_LCD4LINUX, LOCALE_MENU_HINT_LCD4L_BRIGHTNESS_STANDBY);
	typeSetup->addItem(nc);

	return typeSetup->exec(NULL, "");
}

void CLCD4lSetup::showHint(const std::string &text)
{
	removeHint();
	hint = new CHint(text.c_str());
	hint->paint();

}

void CLCD4lSetup::removeHint()
{
	if (hint)
	{
		hint->hide();
		delete hint;
		hint = NULL;
	}
}

void CLCD4lSetup::connectSlots()
{
	CLCD4l::getInstance()->OnBeforeStart.connect(sl_start);
	CLCD4l::getInstance()->OnBeforeStop.connect(sl_stop);
	CLCD4l::getInstance()->OnBeforeRestart.connect(sl_restart);

	CLCD4l::getInstance()->OnAfterStart.connect(sl_remove);
	CLCD4l::getInstance()->OnAfterStop.connect(sl_remove);
	CLCD4l::getInstance()->OnAfterRestart.connect(sl_remove);

	CLCD4l::getInstance()->OnError.connect(sl_remove);
}
