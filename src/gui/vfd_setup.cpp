/*
	$similar port: lcd_setup.cpp,v 1.1 2010/07/30 20:52:16 tuxbox-cvs Exp $

	vfd setup implementation, similar to lcd_setup.cpp of tuxbox-cvs - Neutrino-GUI

	Copyright (C) 2001 Steffen Hehn 'McClean'
	and some other guys
	Homepage: http://dbox.cyberphoria.org/

	Copyright (C) 2010 T. Graf 'dbt'
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


#include "vfd_setup.h"

#include <global.h>
#include <neutrino.h>
#include <mymenu.h>
#include <neutrino_menue.h>

#include <gui/widget/icons.h>

#include <driver/display.h>
#include <driver/screen_max.h>
#include <driver/display.h>

#include <system/debug.h>
#include <cs_api.h>


CVfdSetup::CVfdSetup()
{
	width = 40;
	vfd_enabled = (cs_get_revision() != 10) && (cs_get_revision() != 11);
}

CVfdSetup::~CVfdSetup()
{
}


int CVfdSetup::exec(CMenuTarget* parent, const std::string &actionKey)
{
	dprintf(DEBUG_DEBUG, "init lcd setup\n");
	if(parent != NULL)
		parent->hide();

	if(actionKey=="def") {
		brightness		= DEFAULT_VFD_BRIGHTNESS;
		brightnessstandby	= DEFAULT_VFD_STANDBYBRIGHTNESS;
		brightnessdeepstandby   = DEFAULT_VFD_STANDBYBRIGHTNESS;
		g_settings.lcd_setting_dim_brightness = 3;
		CVFD::getInstance()->setBrightness(brightness);
		CVFD::getInstance()->setBrightnessStandby(brightnessstandby);
		CVFD::getInstance()->setBrightnessDeepStandby(brightnessdeepstandby);
		return menu_return::RETURN_REPAINT;
	} else if (actionKey == "brightness") {
		return showBrightnessSetup();
	}

	int res = showSetup();

	return res;
}

#define LCDMENU_STATUSLINE_OPTION_COUNT 2
const CMenuOptionChooser::keyval LCDMENU_STATUSLINE_OPTIONS[LCDMENU_STATUSLINE_OPTION_COUNT] =
{
	{ 0, LOCALE_LCDMENU_STATUSLINE_PLAYTIME },
	{ 1, LOCALE_LCDMENU_STATUSLINE_VOLUME   }
	//,{ 2, LOCALE_LCDMENU_STATUSLINE_BOTH     }
};

#define LEDMENU_OPTION_COUNT 4
const CMenuOptionChooser::keyval LEDMENU_OPTIONS[LEDMENU_OPTION_COUNT] =
{
	{ 0, LOCALE_LEDCONTROLER_OFF },
	{ 1, LOCALE_LEDCONTROLER_ON_ALL },
	{ 2, LOCALE_LEDCONTROLER_ON_LED1 },
	{ 3, LOCALE_LEDCONTROLER_ON_LED2   }
};

#define LCD_INFO_OPTION_COUNT 2
const CMenuOptionChooser::keyval LCD_INFO_OPTIONS[LCD_INFO_OPTION_COUNT] =
{
	{ 0, LOCALE_LCD_INFO_LINE_CHANNEL },
	{ 1, LOCALE_LCD_INFO_LINE_CLOCK }
};

int CVfdSetup::showSetup()
{
	CMenuWidget *vfds = new CMenuWidget(LOCALE_MAINMENU_SETTINGS, NEUTRINO_ICON_LCD, width, MN_WIDGET_ID_VFDSETUP);
	vfds->addIntroItems(LOCALE_LCDMENU_HEAD);

	CMenuForwarder * mf;

	//led menu
	if (cs_get_revision() > 7) // not HD1 and BSE
	{
 		CMenuWidget * ledMenu = new CMenuWidget(LOCALE_LCDMENU_HEAD, NEUTRINO_ICON_LCD, width, MN_WIDGET_ID_VFDSETUP_LED_SETUP);
		showLedSetup(ledMenu);
		mf = new CMenuDForwarder(LOCALE_LEDCONTROLER_MENU, true, NULL, ledMenu, NULL, CRCInput::RC_red);
		mf->setHint("", LOCALE_MENU_HINT_POWER_LEDS);
		vfds->addItem(mf);
	}

	if (g_info.hw_caps->can_set_display_brightness)
	{
		//vfd brightness menu
		mf = new CMenuForwarder(LOCALE_LCDMENU_LCDCONTROLER, vfd_enabled, NULL, this, "brightness", CRCInput::RC_green);
		mf->setHint("", LOCALE_MENU_HINT_VFD_BRIGHTNESS_SETUP);
		vfds->addItem(mf);
	}

	if (CVFD::getInstance()->has_lcd)
	{
		if (cs_get_revision() == 9) // Tank only
		{
			//backlight menu
			CMenuWidget * blMenu = new CMenuWidget(LOCALE_LCDMENU_HEAD, NEUTRINO_ICON_LCD, width, MN_WIDGET_ID_VFDSETUP_BACKLIGHT);
			showBacklightSetup(blMenu);
			mf = new CMenuDForwarder(LOCALE_LEDCONTROLER_BACKLIGHT, true, NULL, blMenu, NULL, CRCInput::RC_yellow);
			mf->setHint("", LOCALE_MENU_HINT_BACKLIGHT);
			vfds->addItem(mf);
		}

		vfds->addItem(GenericMenuSeparatorLine);

		//status and info line options
		CMenuOptionChooser* oj = new CMenuOptionChooser(LOCALE_LCDMENU_STATUSLINE, &g_settings.lcd_setting[SNeutrinoSettings::LCD_SHOW_VOLUME], LCDMENU_STATUSLINE_OPTIONS, LCDMENU_STATUSLINE_OPTION_COUNT, vfd_enabled);
		oj->setHint("", LOCALE_MENU_HINT_VFD_STATUSLINE);
		CMenuOptionChooser* lcd_clock_channelname_menu = new CMenuOptionChooser(LOCALE_LCD_INFO_LINE, &g_settings.lcd_info_line, LCD_INFO_OPTIONS, LCD_INFO_OPTION_COUNT, vfd_enabled);
		lcd_clock_channelname_menu->setHint("", LOCALE_MENU_HINT_VFD_INFOLINE);
		vfds->addItem(oj);
		vfds->addItem(lcd_clock_channelname_menu);

		oj = new CMenuOptionChooser(LOCALE_LCDMENU_SCROLL, &g_settings.lcd_scroll, OPTIONS_OFF0_ON1_OPTIONS, OPTIONS_OFF0_ON1_OPTION_COUNT, vfd_enabled);
		oj->setHint("", LOCALE_MENU_HINT_VFD_SCROLL);
		vfds->addItem(oj);

		oj = new CMenuOptionChooser(LOCALE_LCDMENU_NOTIFY_RCLOCK, &g_settings.lcd_notify_rclock, OPTIONS_OFF0_ON1_OPTIONS, OPTIONS_OFF0_ON1_OPTION_COUNT, vfd_enabled);
		oj->setHint("", LOCALE_MENU_HINT_VFD_NOTIFY_RCLOCK);
		vfds->addItem(oj);
	}

	int res = vfds->exec(NULL, "");

	delete vfds;
	return res;
}

int CVfdSetup::showBrightnessSetup()
{
	CMenuOptionNumberChooser * nc;
	CMenuForwarder * mf;

	CMenuWidget *mn_widget = new CMenuWidget(LOCALE_LCDMENU_HEAD, NEUTRINO_ICON_LCD,width, MN_WIDGET_ID_VFDSETUP_LCD_SLIDERS);

	mn_widget->addItem(new CMenuSeparator(CMenuSeparator::ALIGN_LEFT | CMenuSeparator::SUB_HEAD | CMenuSeparator::STRING, LOCALE_LCDMENU_LCDCONTROLER));
	mn_widget->addItem(GenericMenuSeparator);

	mf = new CMenuForwarder(LOCALE_MENU_BACK, true);
	mf->setItemButton(!g_settings.menu_left_exit ? NEUTRINO_ICON_BUTTON_HOME : NEUTRINO_ICON_BUTTON_LEFT);
	mf->setActivateObserver(this);
	mn_widget->addItem(mf);

	mn_widget->addItem(GenericMenuSeparatorLine);

	brightness = CVFD::getInstance()->getBrightness();
	brightnessstandby = CVFD::getInstance()->getBrightnessStandby();
	brightnessdeepstandby = CVFD::getInstance()->getBrightnessDeepStandby();

	nc = new CMenuOptionNumberChooser(LOCALE_LCDCONTROLER_BRIGHTNESS, &brightness, true, 0, 15, this, CRCInput::RC_nokey, NULL, 0, 0, NONEXISTANT_LOCALE, true);
	nc->setHint("", LOCALE_MENU_HINT_VFD_BRIGHTNESS);
	nc->setActivateObserver(this);
	mn_widget->addItem(nc);

	nc = new CMenuOptionNumberChooser(LOCALE_LCDCONTROLER_BRIGHTNESSSTANDBY, &brightnessstandby, true, 0, 15, this, CRCInput::RC_nokey, NULL, 0, 0, NONEXISTANT_LOCALE, true);
	nc->setHint("", LOCALE_MENU_HINT_VFD_BRIGHTNESSSTANDBY);
	nc->setActivateObserver(this);
	mn_widget->addItem(nc);

	if(cs_get_revision() > 7) {
		nc = new CMenuOptionNumberChooser(LOCALE_LCDCONTROLER_BRIGHTNESSDEEPSTANDBY, &brightnessdeepstandby, true, 0, 15, this, CRCInput::RC_nokey, NULL, 0, 0, NONEXISTANT_LOCALE, true);
		nc->setHint("", LOCALE_MENU_HINT_VFD_BRIGHTNESSDEEPSTANDBY);
		nc->setActivateObserver(this);
		mn_widget->addItem(nc);
	}
	nc = new CMenuOptionNumberChooser(LOCALE_LCDMENU_DIM_BRIGHTNESS, &g_settings.lcd_setting_dim_brightness, vfd_enabled, -1, 15, NULL, CRCInput::RC_nokey, NULL, 0, -1, LOCALE_OPTIONS_OFF, true);
	nc->setHint("", LOCALE_MENU_HINT_VFD_BRIGHTNESSDIM);
	nc->setActivateObserver(this);
	mn_widget->addItem(nc);

	mn_widget->addItem(GenericMenuSeparatorLine);
	CStringInput *dim_time = new CStringInput(LOCALE_LCDMENU_DIM_TIME, &g_settings.lcd_setting_dim_time, 3, NONEXISTANT_LOCALE, NONEXISTANT_LOCALE,"0123456789 ");

	mf = new CMenuForwarder(LOCALE_LCDMENU_DIM_TIME, vfd_enabled, g_settings.lcd_setting_dim_time, dim_time);
	mf->setHint("", LOCALE_MENU_HINT_VFD_DIMTIME);
	mf->setActivateObserver(this);
	mn_widget->addItem(mf);

	mn_widget->addItem(GenericMenuSeparatorLine);
	mf = new CMenuForwarder(LOCALE_OPTIONS_DEFAULT, true, NULL, this, "def", CRCInput::RC_red);
	mf->setHint("", LOCALE_MENU_HINT_VFD_DEFAULTS);
	mf->setActivateObserver(this);
	mn_widget->addItem(mf);

	int res = mn_widget->exec(this, "");
	delete mn_widget;

	g_settings.lcd_setting[SNeutrinoSettings::LCD_BRIGHTNESS] = brightness;
	g_settings.lcd_setting[SNeutrinoSettings::LCD_STANDBY_BRIGHTNESS] = brightnessstandby;
	return res;
}

void CVfdSetup::showLedSetup(CMenuWidget *mn_led_widget)
{
	CMenuOptionChooser * mc;
	mn_led_widget->addIntroItems(LOCALE_LEDCONTROLER_MENU);

	mc = new CMenuOptionChooser(LOCALE_LEDCONTROLER_MODE_TV, &g_settings.led_tv_mode, LEDMENU_OPTIONS, LEDMENU_OPTION_COUNT, true, this);
	mc->setHint("", LOCALE_MENU_HINT_LEDS_TV);
	mn_led_widget->addItem(mc);

	mc = new CMenuOptionChooser(LOCALE_LEDCONTROLER_MODE_STANDBY, &g_settings.led_standby_mode, LEDMENU_OPTIONS, LEDMENU_OPTION_COUNT, true);
	mc->setHint("", LOCALE_MENU_HINT_LEDS_STANDBY);
	mn_led_widget->addItem(mc);

	mc = new CMenuOptionChooser(LOCALE_LEDCONTROLER_MODE_DEEPSTANDBY, &g_settings.led_deep_mode, LEDMENU_OPTIONS, LEDMENU_OPTION_COUNT, true);
	mc->setHint("", LOCALE_MENU_HINT_LEDS_DEEPSTANDBY);
	mn_led_widget->addItem(mc);

	mc = new CMenuOptionChooser(LOCALE_LEDCONTROLER_MODE_RECORD, &g_settings.led_rec_mode, LEDMENU_OPTIONS, LEDMENU_OPTION_COUNT, true);
	mc->setHint("", LOCALE_MENU_HINT_LEDS_RECORD);
	mn_led_widget->addItem(mc);

	mc = new CMenuOptionChooser(LOCALE_LEDCONTROLER_BLINK, &g_settings.led_blink, OPTIONS_OFF0_ON1_OPTIONS, OPTIONS_OFF0_ON1_OPTION_COUNT, true);
	mc->setHint("", LOCALE_MENU_HINT_LEDS_BLINK);
	mn_led_widget->addItem(mc);
}

void CVfdSetup::showBacklightSetup(CMenuWidget *mn_led_widget)
{
	CMenuOptionChooser * mc;
	mn_led_widget->addIntroItems(LOCALE_LEDCONTROLER_BACKLIGHT);

	mc = new CMenuOptionChooser(LOCALE_LEDCONTROLER_BACKLIGHT_TV, &g_settings.backlight_tv, OPTIONS_OFF0_ON1_OPTIONS, OPTIONS_OFF0_ON1_OPTION_COUNT, true, this);
	mc->setHint("", LOCALE_MENU_HINT_LEDS_TV);
	mn_led_widget->addItem(mc);

	mc = new CMenuOptionChooser(LOCALE_LEDCONTROLER_MODE_STANDBY, &g_settings.backlight_standby, OPTIONS_OFF0_ON1_OPTIONS, OPTIONS_OFF0_ON1_OPTION_COUNT, true);
	mc->setHint("", LOCALE_MENU_HINT_LEDS_STANDBY);
	mn_led_widget->addItem(mc);

	mc = new CMenuOptionChooser(LOCALE_LEDCONTROLER_MODE_DEEPSTANDBY, &g_settings.backlight_deepstandby, OPTIONS_OFF0_ON1_OPTIONS, OPTIONS_OFF0_ON1_OPTION_COUNT, true);
	mc->setHint("", LOCALE_MENU_HINT_LEDS_DEEPSTANDBY);
	mn_led_widget->addItem(mc);
}

bool CVfdSetup::changeNotify(const neutrino_locale_t OptionName, void * /* data */)
{
	if (ARE_LOCALES_EQUAL(OptionName, LOCALE_LCDCONTROLER_BRIGHTNESS)) {
		CVFD::getInstance()->setBrightness(brightness);
	} else if (ARE_LOCALES_EQUAL(OptionName, LOCALE_LCDCONTROLER_BRIGHTNESSSTANDBY)) {
		CVFD::getInstance()->setBrightnessStandby(brightnessstandby);
	} else if (ARE_LOCALES_EQUAL(OptionName, LOCALE_LCDCONTROLER_BRIGHTNESSDEEPSTANDBY)) {
		CVFD::getInstance()->setBrightnessStandby(brightnessdeepstandby);
		CVFD::getInstance()->setBrightnessDeepStandby(brightnessdeepstandby);
	} else if (ARE_LOCALES_EQUAL(OptionName, LOCALE_LCDMENU_DIM_BRIGHTNESS)) {
		CVFD::getInstance()->setBrightness(g_settings.lcd_setting_dim_brightness);
	} else if (ARE_LOCALES_EQUAL(OptionName, LOCALE_LEDCONTROLER_MODE_TV)) {
		CVFD::getInstance()->setled();
	} else if (ARE_LOCALES_EQUAL(OptionName, LOCALE_LEDCONTROLER_BACKLIGHT_TV)) {
		CVFD::getInstance()->setBacklight(g_settings.backlight_tv);
	}

	return false;
}

void CVfdSetup::activateNotify(const neutrino_locale_t OptionName)
{
	if (ARE_LOCALES_EQUAL(OptionName, LOCALE_LCDCONTROLER_BRIGHTNESSSTANDBY)) {
		g_settings.lcd_setting[SNeutrinoSettings::LCD_STANDBY_BRIGHTNESS] = brightnessstandby;
		CVFD::getInstance()->setMode(CVFD::MODE_STANDBY);
	} else if (ARE_LOCALES_EQUAL(OptionName, LOCALE_LCDCONTROLER_BRIGHTNESSDEEPSTANDBY)) {
		g_settings.lcd_setting[SNeutrinoSettings::LCD_STANDBY_BRIGHTNESS] = brightnessdeepstandby;
		CVFD::getInstance()->setMode(CVFD::MODE_STANDBY);
	} else if (ARE_LOCALES_EQUAL(OptionName, LOCALE_LCDCONTROLER_BRIGHTNESS)) {
		g_settings.lcd_setting[SNeutrinoSettings::LCD_BRIGHTNESS] = brightness;
		CVFD::getInstance()->setMode(CVFD::MODE_TVRADIO);
	} else if (ARE_LOCALES_EQUAL(OptionName, LOCALE_LCDMENU_DIM_BRIGHTNESS)) {
		g_settings.lcd_setting[SNeutrinoSettings::LCD_BRIGHTNESS] = g_settings.lcd_setting_dim_brightness;
		CVFD::getInstance()->setMode(CVFD::MODE_TVRADIO);
	} else {
		g_settings.lcd_setting[SNeutrinoSettings::LCD_BRIGHTNESS] = brightness;
		CVFD::getInstance()->setMode(CVFD::MODE_MENU_UTF8);
	}
}
