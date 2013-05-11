/*
	$port: osd_setup.cpp,v 1.6 2010/09/30 20:13:59 tuxbox-cvs Exp $

	osd_setup implementation - Neutrino-GUI

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

#include <global.h>
#include <neutrino.h>
#include <mymenu.h>
#include <neutrino_menue.h>

#include "osd_setup.h"
#include "themes.h"
#include "screensetup.h"
#include "osdlang_setup.h"
#include "filebrowser.h"
#include "osd_progressbar_setup.h"

#include <gui/widget/icons.h>
#include <gui/widget/colorchooser.h>
#include <gui/widget/stringinput.h>

#include <driver/screen_max.h>
#include <driver/screenshot.h>
#include <driver/volume.h>

#include <zapit/femanager.h>
#include <system/debug.h>

extern CRemoteControl * g_RemoteControl;

static CTimingSettingsNotifier timingsettingsnotifier;

extern const char * locale_real_names[];
extern std::string ttx_font_file;

COsdSetup::COsdSetup(bool wizard_mode)
{
	colorSetupNotifier = new CColorSetupNotifier();
	fontsizenotifier = new CFontSizeNotifier;
	osd_menu = NULL;
	submenu_menus = NULL;

	is_wizard = wizard_mode;

	width = w_max (40, 10); //%
	show_menu_hints = 0;
	show_tuner_icon = 0;
}

COsdSetup::~COsdSetup()
{
	delete colorSetupNotifier;
	delete fontsizenotifier;
}

//font settings
const SNeutrinoSettings::FONT_TYPES channellist_font_sizes[5] =
{
	SNeutrinoSettings::FONT_TYPE_CHANNELLIST,
	SNeutrinoSettings::FONT_TYPE_CHANNELLIST_DESCR,
	SNeutrinoSettings::FONT_TYPE_CHANNELLIST_NUMBER,
	SNeutrinoSettings::FONT_TYPE_CHANNELLIST_EVENT,
	SNeutrinoSettings::FONT_TYPE_CHANNEL_NUM_ZAP
};

const SNeutrinoSettings::FONT_TYPES eventlist_font_sizes[5] =
{
	SNeutrinoSettings::FONT_TYPE_EVENTLIST_TITLE,
	SNeutrinoSettings::FONT_TYPE_EVENTLIST_ITEMLARGE,
	SNeutrinoSettings::FONT_TYPE_EVENTLIST_ITEMSMALL,
	SNeutrinoSettings::FONT_TYPE_EVENTLIST_DATETIME,
	SNeutrinoSettings::FONT_TYPE_EVENTLIST_EVENT
};

const SNeutrinoSettings::FONT_TYPES infobar_font_sizes[4] =
{
	SNeutrinoSettings::FONT_TYPE_INFOBAR_NUMBER,
	SNeutrinoSettings::FONT_TYPE_INFOBAR_CHANNAME,
	SNeutrinoSettings::FONT_TYPE_INFOBAR_INFO,
	SNeutrinoSettings::FONT_TYPE_INFOBAR_SMALL
};

const SNeutrinoSettings::FONT_TYPES epg_font_sizes[4] =
{
	SNeutrinoSettings::FONT_TYPE_EPG_TITLE,
	SNeutrinoSettings::FONT_TYPE_EPG_INFO1,
	SNeutrinoSettings::FONT_TYPE_EPG_INFO2,
	SNeutrinoSettings::FONT_TYPE_EPG_DATE
};

const SNeutrinoSettings::FONT_TYPES gamelist_font_sizes[2] =
{
	SNeutrinoSettings::FONT_TYPE_GAMELIST_ITEMLARGE,
	SNeutrinoSettings::FONT_TYPE_GAMELIST_ITEMSMALL
};

const SNeutrinoSettings::FONT_TYPES other_font_sizes[5] =
{
	SNeutrinoSettings::FONT_TYPE_MENU_TITLE,
	SNeutrinoSettings::FONT_TYPE_MENU,
	SNeutrinoSettings::FONT_TYPE_MENU_INFO,
	SNeutrinoSettings::FONT_TYPE_MENU_HINT,
	SNeutrinoSettings::FONT_TYPE_FILEBROWSER_ITEM
};

font_sizes_groups font_sizes_groups[6] =
{
	{LOCALE_FONTMENU_MENU       , 5, other_font_sizes      , "fontsize.doth", LOCALE_MENU_HINT_MENU_FONTS },
	{LOCALE_FONTMENU_CHANNELLIST, 5, channellist_font_sizes, "fontsize.dcha", LOCALE_MENU_HINT_CHANNELLIST_FONTS },
	{LOCALE_FONTMENU_EVENTLIST  , 5, eventlist_font_sizes  , "fontsize.deve", LOCALE_MENU_HINT_EVENTLIST_FONTS },
	{LOCALE_FONTMENU_EPG        , 4, epg_font_sizes        , "fontsize.depg", LOCALE_MENU_HINT_EPG_FONTS },
	{LOCALE_FONTMENU_INFOBAR    , 4, infobar_font_sizes    , "fontsize.dinf", LOCALE_MENU_HINT_INFOBAR_FONTS },
	{LOCALE_FONTMENU_GAMELIST   , 2, gamelist_font_sizes   , "fontsize.dgam", LOCALE_MENU_HINT_GAMELIST_FONTS }
};

#define FONT_STYLE_REGULAR 0
#define FONT_STYLE_BOLD    1
#define FONT_STYLE_ITALIC  2

font_sizes_struct neutrino_font[SNeutrinoSettings::FONT_TYPE_COUNT] =
{
	{LOCALE_FONTSIZE_MENU               ,  20, FONT_STYLE_BOLD   , 0},
	{LOCALE_FONTSIZE_MENU_TITLE         ,  30, FONT_STYLE_BOLD   , 0},
	{LOCALE_FONTSIZE_MENU_INFO          ,  16, FONT_STYLE_REGULAR, 0},
	{LOCALE_FONTSIZE_EPG_TITLE          ,  25, FONT_STYLE_REGULAR, 1},
	{LOCALE_FONTSIZE_EPG_INFO1          ,  17, FONT_STYLE_ITALIC , 2},
	{LOCALE_FONTSIZE_EPG_INFO2          ,  17, FONT_STYLE_REGULAR, 2},
	{LOCALE_FONTSIZE_EPG_DATE           ,  15, FONT_STYLE_REGULAR, 2},
	{LOCALE_FONTSIZE_EVENTLIST_TITLE    ,  30, FONT_STYLE_REGULAR, 0},
	{LOCALE_FONTSIZE_EVENTLIST_ITEMLARGE,  20, FONT_STYLE_BOLD   , 1},
	{LOCALE_FONTSIZE_EVENTLIST_ITEMSMALL,  14, FONT_STYLE_REGULAR, 1},
	{LOCALE_FONTSIZE_EVENTLIST_DATETIME ,  16, FONT_STYLE_REGULAR, 1},
	{LOCALE_FONTSIZE_EVENTLIST_EVENT    ,  17, FONT_STYLE_REGULAR, 1},
	{LOCALE_FONTSIZE_GAMELIST_ITEMLARGE ,  20, FONT_STYLE_BOLD   , 1},
	{LOCALE_FONTSIZE_GAMELIST_ITEMSMALL ,  16, FONT_STYLE_REGULAR, 1},
	{LOCALE_FONTSIZE_CHANNELLIST        ,  20, FONT_STYLE_BOLD   , 1},
	{LOCALE_FONTSIZE_CHANNELLIST_DESCR  ,  20, FONT_STYLE_REGULAR, 1},
	{LOCALE_FONTSIZE_CHANNELLIST_NUMBER ,  14, FONT_STYLE_BOLD   , 2},
	{LOCALE_FONTSIZE_CHANNELLIST_EVENT  ,  17, FONT_STYLE_REGULAR, 2},
	{LOCALE_FONTSIZE_CHANNEL_NUM_ZAP    ,  40, FONT_STYLE_BOLD   , 0},
	{LOCALE_FONTSIZE_INFOBAR_NUMBER     ,  50, FONT_STYLE_BOLD   , 0},
	{LOCALE_FONTSIZE_INFOBAR_CHANNAME   ,  30, FONT_STYLE_BOLD   , 0},
	{LOCALE_FONTSIZE_INFOBAR_INFO       ,  20, FONT_STYLE_REGULAR, 1},
	{LOCALE_FONTSIZE_INFOBAR_SMALL      ,  14, FONT_STYLE_REGULAR, 1},
	{LOCALE_FONTSIZE_FILEBROWSER_ITEM   ,  16, FONT_STYLE_BOLD   , 1},
	{LOCALE_FONTSIZE_MENU_HINT          ,  16, FONT_STYLE_REGULAR, 0}
};

int COsdSetup::exec(CMenuTarget* parent, const std::string &actionKey)
{
	dprintf(DEBUG_DEBUG, "init osd setup\n");

	printf("COsdSetup::exec:: action  %s\n", actionKey.c_str());
	if(parent != NULL)
		parent->hide();

	if(actionKey == "select_font")
	{
		CFileBrowser fileBrowser;
		CFileFilter fileFilter;
		fileFilter.addFilter("ttf");
		fileBrowser.Filter = &fileFilter;
		if (fileBrowser.exec(FONTDIR) == true)
		{
			strcpy(g_settings.font_file, fileBrowser.getSelectedFile()->Name.c_str());
			printf("[neutrino] new font file %s\n", fileBrowser.getSelectedFile()->Name.c_str());
			CNeutrinoApp::getInstance()->SetupFonts();
		}
		return menu_return::RETURN_REPAINT;
	}
	else if(actionKey == "ttx_font")
	{
		CFileBrowser fileBrowser;
		CFileFilter fileFilter;
		fileFilter.addFilter("ttf");
		fileBrowser.Filter = &fileFilter;
		if (fileBrowser.exec(FONTDIR) == true)
		{
			strcpy(g_settings.ttx_font_file, fileBrowser.getSelectedFile()->Name.c_str());
			ttx_font_file = fileBrowser.getSelectedFile()->Name;
			printf("[neutrino] ttx font file %s\n", fileBrowser.getSelectedFile()->Name.c_str());
			CNeutrinoApp::getInstance()->SetupFonts();
		}
		return menu_return::RETURN_REPAINT;
	}
	else if (actionKey == "font_scaling") {
		int xre = g_settings.screen_xres;
		int yre = g_settings.screen_yres;
		char val_x[4] = {0};
		char val_y[4] = {0};
		snprintf(val_x,sizeof(val_x), "%03d",g_settings.screen_xres);
		snprintf(val_y,sizeof(val_y), "%03d",g_settings.screen_yres);

		CMenuWidget fontscale(LOCALE_FONTMENU_HEAD, NEUTRINO_ICON_COLORS, width, MN_WIDGET_ID_OSDSETUP_FONTSCALE);
		fontscale.addIntroItems(LOCALE_FONTMENU_SCALING);

		CStringInput xres_count(LOCALE_FONTMENU_SCALING_X, val_x,50,200, 3, LOCALE_FONTMENU_SCALING, LOCALE_FONTMENU_SCALING_X_HINT2, "0123456789 ");
		CMenuForwarder *m_x = new CMenuForwarder(LOCALE_FONTMENU_SCALING_X, true, val_x, &xres_count);

		CStringInput yres_count(LOCALE_FONTMENU_SCALING_Y, val_y,50,200, 3, LOCALE_FONTMENU_SCALING, LOCALE_FONTMENU_SCALING_Y_HINT2, "0123456789 ");
		CMenuForwarder *m_y = new CMenuForwarder(LOCALE_FONTMENU_SCALING_Y, true, val_y, &yres_count);

		fontscale.addItem(m_x);
		fontscale.addItem(m_y);
		int res = fontscale.exec(NULL, "");
		xre = atoi(val_x);
		yre = atoi(val_y);
		//fallback for min/max bugs ;)
		if( xre < 50 || xre > 200 ){
			xre = g_settings.screen_xres;
			snprintf(val_x,sizeof(val_x), "%03d",g_settings.screen_xres);
		}
		if( yre < 50 || yre > 200 ){
			yre = g_settings.screen_yres;
			snprintf(val_y,sizeof(val_y), "%03d",g_settings.screen_yres);
		}

		if (xre != g_settings.screen_xres || yre != g_settings.screen_yres) {
			printf("[neutrino] new font scale settings x: %d%% y: %d%%\n", xre, yre);
			g_settings.screen_xres = xre;
			g_settings.screen_yres = yre;
			CNeutrinoApp::getInstance()->SetupFonts();
		}
		//return menu_return::RETURN_REPAINT;
		return res;
	}
	else if(actionKey=="osd.def") {
		for (int i = 0; i < SNeutrinoSettings::TIMING_SETTING_COUNT; i++)
			g_settings.timing[i] = timing_setting[i].default_timing;

		CNeutrinoApp::getInstance()->SetupTiming();
		return menu_return::RETURN_REPAINT;
	}
	else if(actionKey=="logo_dir") {
		const char *action_str = "logo";
		chooserDir(g_settings.logo_hdd_dir, false, action_str);
		return menu_return::RETURN_REPAINT;
	}
	else if(actionKey=="screenshot_dir") {
		const char *action_str = "screenshot";
		chooserDir(g_settings.screenshot_dir, true, action_str);
		return menu_return::RETURN_REPAINT;
	}
	else if(strncmp(actionKey.c_str(), "fontsize.d", 10) == 0) {
		for (int i = 0; i < 6; i++) {
			if (actionKey == font_sizes_groups[i].actionkey) {
				for (unsigned int j = 0; j < font_sizes_groups[i].count; j++) {
					SNeutrinoSettings::FONT_TYPES k = font_sizes_groups[i].content[j];
					CNeutrinoApp::getInstance()->getConfigFile()->setInt32(locale_real_names[neutrino_font[k].name], neutrino_font[k].defaultsize);
				}
				break;
			}
		}
		fontsizenotifier->changeNotify(NONEXISTANT_LOCALE, NULL);
		return menu_return::RETURN_REPAINT;
	}

	int res = showOsdSetup();

	//return menu_return::RETURN_REPAINT;
	return res;
}


#define OSD_PRESET_OPTIONS_COUNT 2
const CMenuOptionChooser::keyval OSD_PRESET_OPTIONS[OSD_PRESET_OPTIONS_COUNT] =
{
	{ 0, LOCALE_COLORMENU_SD_PRESET },
	{ 1, LOCALE_COLORMENU_HD_PRESET }
};

#define INFOBAR_CASYSTEM_MODE_OPTION_COUNT 4
const CMenuOptionChooser::keyval INFOBAR_CASYSTEM_MODE_OPTIONS[INFOBAR_CASYSTEM_MODE_OPTION_COUNT] =
{
	{ 0, LOCALE_OPTIONS_ON },
	{ 1, LOCALE_MISCSETTINGS_INFOBAR_CASYSTEM_MODE },
	{ 2, LOCALE_MISCSETTINGS_INFOBAR_CASYSTEM_MINI },
	{ 3, LOCALE_OPTIONS_OFF  },
};

#define SHOW_INFOMENU_MODE_OPTION_COUNT 2
const CMenuOptionChooser::keyval SHOW_INFOMENU_MODE_OPTIONS[SHOW_INFOMENU_MODE_OPTION_COUNT] =
{
	{ 0, LOCALE_MAINMENU_HEAD },
	{ 1, LOCALE_MAINMENU_SERVICE },
};

#define MENU_CORNERSETTINGS_TYPE_OPTION_COUNT 2
const CMenuOptionChooser::keyval MENU_CORNERSETTINGS_TYPE_OPTIONS[MENU_CORNERSETTINGS_TYPE_OPTION_COUNT] =
{
	{ 0, LOCALE_EXTRA_ROUNDED_CORNERS_OFF },
	{ 1, LOCALE_EXTRA_ROUNDED_CORNERS_ON  }
};

#define INFOBAR_SUBCHAN_DISP_POS_OPTIONS_COUNT 5
const CMenuOptionChooser::keyval  INFOBAR_SUBCHAN_DISP_POS_OPTIONS[INFOBAR_SUBCHAN_DISP_POS_OPTIONS_COUNT]=
{
	{ 0 , LOCALE_SETTINGS_POS_TOP_RIGHT },
	{ 1 , LOCALE_SETTINGS_POS_TOP_LEFT },
	{ 2 , LOCALE_SETTINGS_POS_BOTTOM_LEFT },
	{ 3 , LOCALE_SETTINGS_POS_BOTTOM_RIGHT },
	{ 4 , LOCALE_INFOVIEWER_SUBCHAN_INFOBAR }
};

#define VOLUMEBAR_DISP_POS_OPTIONS_COUNT 6
const CMenuOptionChooser::keyval  VOLUMEBAR_DISP_POS_OPTIONS[VOLUMEBAR_DISP_POS_OPTIONS_COUNT]=
{
	{ 0 , LOCALE_SETTINGS_POS_TOP_RIGHT },
	{ 1 , LOCALE_SETTINGS_POS_TOP_LEFT },
	{ 2 , LOCALE_SETTINGS_POS_BOTTOM_LEFT },
	{ 3 , LOCALE_SETTINGS_POS_BOTTOM_RIGHT },
	{ 4 , LOCALE_SETTINGS_POS_TOP_CENTER },
	{ 5 , LOCALE_SETTINGS_POS_BOTTOM_CENTER }
};

#define MENU_DISP_POS_OPTIONS_COUNT 5
const CMenuOptionChooser::keyval  MENU_DISP_POS_OPTIONS[MENU_DISP_POS_OPTIONS_COUNT]=
{
        { 0 , LOCALE_SETTINGS_POS_DEFAULT_CENTER },
        { 1 , LOCALE_SETTINGS_POS_TOP_LEFT },
        { 2 , LOCALE_SETTINGS_POS_TOP_RIGHT },
        { 3 , LOCALE_SETTINGS_POS_BOTTOM_LEFT },
        { 4 , LOCALE_SETTINGS_POS_BOTTOM_RIGHT }
};

#define INFOBAR_SHOW_RES_MODE_OPTION_COUNT 3
const CMenuOptionChooser::keyval INFOBAR_SHOW_RES_MODE_OPTIONS[INFOBAR_SHOW_RES_MODE_OPTION_COUNT] =
{
	{ 0, LOCALE_OPTIONS_ON },
	{ 1, LOCALE_MISCSETTINGS_INFOBAR_SHOW_RES_SIMPLE },
	{ 2, LOCALE_OPTIONS_OFF }
};

#define CHANNELLIST_ADDITIONAL_OPTION_COUNT 3
const CMenuOptionChooser::keyval CHANNELLIST_ADDITIONAL_OPTIONS[CHANNELLIST_ADDITIONAL_OPTION_COUNT] =
{
	{ 0, LOCALE_CHANNELLIST_ADDITIONAL_OFF },
	{ 1, LOCALE_CHANNELLIST_ADDITIONAL_ON },
	{ 2, LOCALE_CHANNELLIST_ADDITIONAL_ON_MINITV }
};

#define CHANNELLIST_FOOT_OPTIONS_COUNT 3
const CMenuOptionChooser::keyval  CHANNELLIST_FOOT_OPTIONS[CHANNELLIST_FOOT_OPTIONS_COUNT]=
{
	{ 0 , LOCALE_CHANNELLIST_FOOT_FREQ },
	{ 1 , LOCALE_CHANNELLIST_FOOT_NEXT },
	{ 2 , LOCALE_CHANNELLIST_FOOT_OFF }
};

#define CHANNELLIST_EPGTEXT_ALIGN_RIGHT_OPTIONS_COUNT 2
const CMenuOptionChooser::keyval  CHANNELLIST_EPGTEXT_ALIGN_RIGHT_OPTIONS[CHANNELLIST_EPGTEXT_ALIGN_RIGHT_OPTIONS_COUNT]=
{
	{ 0 , LOCALE_CHANNELLIST_EPGTEXT_ALIGN_LEFT },
	{ 1 , LOCALE_CHANNELLIST_EPGTEXT_ALIGN_RIGHT }
};

#define OPTIONS_COLORED_EVENTS_OPTION_COUNT 3
const CMenuOptionChooser::keyval OPTIONS_COLORED_EVENTS_OPTIONS[OPTIONS_COLORED_EVENTS_OPTION_COUNT] =
{
	{ 0, LOCALE_MISCSETTINGS_COLORED_EVENTS_0 },	//none
	{ 1, LOCALE_MISCSETTINGS_COLORED_EVENTS_1 },	//current
	{ 2, LOCALE_MISCSETTINGS_COLORED_EVENTS_2 },	//next
};


//show osd setup
int COsdSetup::showOsdSetup()
{
	int shortcut = 1;

	//osd main menu
	osd_menu = new CMenuWidget(LOCALE_MAINMENU_SETTINGS, NEUTRINO_ICON_COLORS, width, MN_WIDGET_ID_OSDSETUP);
	osd_menu->setWizardMode(is_wizard);

	//menu colors
	CMenuWidget osd_menu_colors(LOCALE_MAINMENU_SETTINGS, NEUTRINO_ICON_COLORS, width, MN_WIDGET_ID_OSDSETUP_MENUCOLORS);

	//intro with subhead and back button
	osd_menu->addIntroItems(LOCALE_MAINSETTINGS_OSD);

	//item menu colors
	showOsdMenueColorSetup(&osd_menu_colors);
	CMenuForwarder * mf = new CMenuForwarder(LOCALE_COLORMENU_MENUCOLORS, true, NULL, &osd_menu_colors, NULL, CRCInput::RC_red, NEUTRINO_ICON_BUTTON_RED);
	mf->setHint("", LOCALE_MENU_HINT_COLORS);
	osd_menu->addItem(mf);

	//fonts
	CMenuWidget osd_menu_fonts(LOCALE_MAINMENU_SETTINGS, NEUTRINO_ICON_COLORS, width, MN_WIDGET_ID_OSDSETUP_FONT);
	showOsdFontSizeSetup(&osd_menu_fonts);
	mf = new CMenuForwarder(LOCALE_FONTMENU_HEAD, true, NULL, &osd_menu_fonts, NULL, CRCInput::RC_green, NEUTRINO_ICON_BUTTON_GREEN);
	mf->setHint("", LOCALE_MENU_HINT_FONTS);
	osd_menu->addItem(mf);

	//timeouts
	CMenuWidget osd_menu_timing(LOCALE_MAINMENU_SETTINGS, NEUTRINO_ICON_SETTINGS, width, MN_WIDGET_ID_OSDSETUP_TIMEOUT);
	showOsdTimeoutSetup(&osd_menu_timing);
	mf = new CMenuForwarder(LOCALE_COLORMENU_TIMING, true, NULL, &osd_menu_timing, NULL, CRCInput::RC_yellow, NEUTRINO_ICON_BUTTON_YELLOW);
	mf->setHint("", LOCALE_MENU_HINT_TIMEOUTS);
	osd_menu->addItem(mf);

	//screen
	CScreenSetup screensetup;
	mf = new CMenuForwarder(LOCALE_VIDEOMENU_SCREENSETUP, true, NULL, &screensetup, NULL, CRCInput::RC_blue, NEUTRINO_ICON_BUTTON_BLUE);
	mf->setHint("", LOCALE_MENU_HINT_SCREEN_SETUP);
	osd_menu->addItem(mf);

	//menus
	CMenuWidget osd_menu_menus(LOCALE_MAINMENU_SETTINGS, NEUTRINO_ICON_SETTINGS, width, MN_WIDGET_ID_OSDSETUP_MENUS);
	showOsdMenusSetup(&osd_menu_menus);
	mf = new CMenuForwarder(LOCALE_SETTINGS_MENUS, true, NULL, &osd_menu_menus, NULL, CRCInput::convertDigitToKey(shortcut++));
	mf->setHint("", LOCALE_MENU_HINT_MENUS);
	osd_menu->addItem(mf);

	//progressbar
	mf = new CMenuForwarder(LOCALE_MISCSETTINGS_PROGRESSBAR, true, NULL, new CProgressbarSetup(), NULL, CRCInput::convertDigitToKey(shortcut++));
	mf->setHint("", LOCALE_MENU_HINT_PROGRESSBAR);
	osd_menu->addItem(mf);

	//infobar
	CMenuWidget osd_menu_infobar(LOCALE_MAINMENU_SETTINGS, NEUTRINO_ICON_SETTINGS, width, MN_WIDGET_ID_OSDSETUP_INFOBAR);
	showOsdInfobarSetup(&osd_menu_infobar);
	mf = new CMenuForwarder(LOCALE_MISCSETTINGS_INFOBAR, true, NULL, &osd_menu_infobar, NULL, CRCInput::convertDigitToKey(shortcut++));
	mf->setHint("", LOCALE_MENU_HINT_INFOBAR_SETUP);
	osd_menu->addItem(mf);

	//channellist
	CMenuWidget osd_menu_chanlist(LOCALE_MAINMENU_SETTINGS, NEUTRINO_ICON_SETTINGS, width, MN_WIDGET_ID_OSDSETUP_CHANNELLIST);
	showOsdChanlistSetup(&osd_menu_chanlist);
	mf = new CMenuForwarder(LOCALE_MISCSETTINGS_CHANNELLIST, true, NULL, &osd_menu_chanlist, NULL, CRCInput::convertDigitToKey(shortcut++));
	mf->setHint("", LOCALE_MENU_HINT_CHANNELLIST_SETUP);
	osd_menu->addItem(mf);

	//eventlist
	CMenuWidget osd_menu_eventlist(LOCALE_MAINMENU_SETTINGS, NEUTRINO_ICON_SETTINGS, width, MN_WIDGET_ID_OSDSETUP_EVENTLIST);
	showOsdEventlistSetup(&osd_menu_eventlist);
	mf = new CMenuForwarder(LOCALE_EVENTLIST_NAME, true, NULL, &osd_menu_eventlist, NULL, CRCInput::convertDigitToKey(shortcut++));
	mf->setHint("", LOCALE_MENU_HINT_EVENTLIST_SETUP);
	osd_menu->addItem(mf);

	//volume
	CMenuWidget osd_menu_volume(LOCALE_MAINMENU_SETTINGS, NEUTRINO_ICON_SETTINGS, width, MN_WIDGET_ID_OSDSETUP_VOLUME);
	showOsdVolumeSetup(&osd_menu_volume);
	mf = new CMenuForwarder(LOCALE_MISCSETTINGS_VOLUME, true, NULL, &osd_menu_volume, NULL, CRCInput::convertDigitToKey(shortcut++));
	mf->setHint("", LOCALE_MENU_HINT_VOLUME);
	osd_menu->addItem(mf);

	//screenshot
	CMenuWidget osd_menu_screenshot(LOCALE_MAINMENU_SETTINGS, NEUTRINO_ICON_SETTINGS, width, MN_WIDGET_ID_OSDSETUP_SCREENSHOT);
	showOsdScreenShotSetup(&osd_menu_screenshot);
	mf = new CMenuForwarder(LOCALE_SCREENSHOT_MENU, true, NULL, &osd_menu_screenshot, NULL, CRCInput::convertDigitToKey(shortcut++));
	mf->setHint("", LOCALE_MENU_HINT_SCREENSHOT_SETUP);
	osd_menu->addItem(mf);

	osd_menu->addItem(GenericMenuSeparatorLine);

	//monitor
	CMenuOptionChooser * mc = new CMenuOptionChooser(LOCALE_COLORMENU_OSD_PRESET, &g_settings.screen_preset, OSD_PRESET_OPTIONS, OSD_PRESET_OPTIONS_COUNT, true, this);
	mc->setHint("", LOCALE_MENU_HINT_OSD_PRESET);
	osd_menu->addItem(mc);

	// round corners
	int rounded_corners = g_settings.rounded_corners;
	mc = new CMenuOptionChooser(LOCALE_EXTRA_ROUNDED_CORNERS, &rounded_corners, MENU_CORNERSETTINGS_TYPE_OPTIONS, MENU_CORNERSETTINGS_TYPE_OPTION_COUNT, true, this);
	mc->setHint("", LOCALE_MENU_HINT_ROUNDED_CORNERS);
	osd_menu->addItem(mc);

	// fade windows
	mc = new CMenuOptionChooser(LOCALE_COLORMENU_FADE, &g_settings.widget_fade, OPTIONS_OFF0_ON1_OPTIONS, OPTIONS_OFF0_ON1_OPTION_COUNT, true );
	mc->setHint("", LOCALE_MENU_HINT_FADE);
	osd_menu->addItem(mc);

	// big windows
	mc = new CMenuOptionChooser(LOCALE_EXTRA_BIGWINDOWS, &g_settings.big_windows, OPTIONS_OFF0_ON1_OPTIONS, OPTIONS_OFF0_ON1_OPTION_COUNT, true);
	mc->setHint("", LOCALE_MENU_HINT_BIGWINDOWS);
	osd_menu->addItem(mc);

	osd_menu->addItem(GenericMenuSeparatorLine);

	// scrambled
	mc = new CMenuOptionChooser(LOCALE_EXTRA_SCRAMBLED_MESSAGE, &g_settings.scrambled_message, OPTIONS_OFF0_ON1_OPTIONS, OPTIONS_OFF0_ON1_OPTION_COUNT, true);
	mc->setHint("", LOCALE_MENU_HINT_SCRAMBLED_MESSAGE);
	osd_menu->addItem(mc);

	// subchannel menu position
	mc = new CMenuOptionChooser(LOCALE_INFOVIEWER_SUBCHAN_DISP_POS, &g_settings.infobar_subchan_disp_pos, INFOBAR_SUBCHAN_DISP_POS_OPTIONS, INFOBAR_SUBCHAN_DISP_POS_OPTIONS_COUNT, true);
	mc->setHint("", LOCALE_MENU_HINT_SUBCHANNEL_POS);
	osd_menu->addItem(mc);

	int res = osd_menu->exec(NULL, "");

	delete osd_menu;
	return res;
}

//menue colors
void COsdSetup::showOsdMenueColorSetup(CMenuWidget *menu_colors)
{
	menu_colors->addIntroItems(LOCALE_COLORMENU_MENUCOLORS);

	CMenuForwarder * mf = new CMenuDForwarder(LOCALE_COLORMENU_THEMESELECT, true, NULL, new CThemes(), NULL, CRCInput::RC_red, NEUTRINO_ICON_BUTTON_RED);
	mf->setHint("", LOCALE_MENU_HINT_THEME);
	menu_colors->addItem(mf);

	CColorChooser* chHeadcolor = new CColorChooser(LOCALE_COLORMENU_BACKGROUND, &g_settings.menu_Head_red, &g_settings.menu_Head_green, &g_settings.menu_Head_blue,
			&g_settings.menu_Head_alpha, colorSetupNotifier);
	CColorChooser* chHeadTextcolor = new CColorChooser(LOCALE_COLORMENU_TEXTCOLOR, &g_settings.menu_Head_Text_red, &g_settings.menu_Head_Text_green, &g_settings.menu_Head_Text_blue,
			NULL, colorSetupNotifier);
	CColorChooser* chContentcolor = new CColorChooser(LOCALE_COLORMENU_BACKGROUND, &g_settings.menu_Content_red, &g_settings.menu_Content_green, &g_settings.menu_Content_blue,
			&g_settings.menu_Content_alpha, colorSetupNotifier);
	CColorChooser* chContentTextcolor = new CColorChooser(LOCALE_COLORMENU_TEXTCOLOR, &g_settings.menu_Content_Text_red, &g_settings.menu_Content_Text_green, &g_settings.menu_Content_Text_blue,
			NULL, colorSetupNotifier);
	CColorChooser* chContentSelectedcolor = new CColorChooser(LOCALE_COLORMENU_BACKGROUND, &g_settings.menu_Content_Selected_red, &g_settings.menu_Content_Selected_green, &g_settings.menu_Content_Selected_blue,
			&g_settings.menu_Content_Selected_alpha, colorSetupNotifier);
	CColorChooser* chContentSelectedTextcolor = new CColorChooser(LOCALE_COLORMENU_TEXTCOLOR, &g_settings.menu_Content_Selected_Text_red, &g_settings.menu_Content_Selected_Text_green, &g_settings.menu_Content_Selected_Text_blue,
			NULL, colorSetupNotifier);
	CColorChooser* chContentInactivecolor = new CColorChooser(LOCALE_COLORMENU_BACKGROUND, &g_settings.menu_Content_inactive_red, &g_settings.menu_Content_inactive_green, &g_settings.menu_Content_inactive_blue,
			&g_settings.menu_Content_inactive_alpha, colorSetupNotifier);
	CColorChooser* chContentInactiveTextcolor = new CColorChooser(LOCALE_COLORMENU_TEXTCOLOR, &g_settings.menu_Content_inactive_Text_red, &g_settings.menu_Content_inactive_Text_green, &g_settings.menu_Content_inactive_Text_blue,
			NULL, colorSetupNotifier);

	menu_colors->addItem( new CMenuSeparator(CMenuSeparator::LINE | CMenuSeparator::STRING, LOCALE_COLORMENUSETUP_MENUHEAD));

	mf = new CMenuDForwarder(LOCALE_COLORMENU_BACKGROUND, true, NULL, chHeadcolor );
	mf->setHint("", LOCALE_MENU_HINT_HEAD_BACK);
	menu_colors->addItem(mf);

	mf = new CMenuDForwarder(LOCALE_COLORMENU_TEXTCOLOR, true, NULL, chHeadTextcolor );
	mf->setHint("", LOCALE_MENU_HINT_HEAD_TEXTCOLOR);
	menu_colors->addItem(mf);

	menu_colors->addItem( new CMenuSeparator(CMenuSeparator::LINE | CMenuSeparator::STRING, LOCALE_COLORMENUSETUP_MENUCONTENT));
	mf = new CMenuDForwarder(LOCALE_COLORMENU_BACKGROUND, true, NULL, chContentcolor );
	mf->setHint("", LOCALE_MENU_HINT_CONTENT_BACK);
	menu_colors->addItem(mf);

	mf = new CMenuDForwarder(LOCALE_COLORMENU_TEXTCOLOR, true, NULL, chContentTextcolor );
	mf->setHint("", LOCALE_MENU_HINT_CONTENT_TEXTCOLOR);
	menu_colors->addItem(mf);

	menu_colors->addItem( new CMenuSeparator(CMenuSeparator::LINE | CMenuSeparator::STRING, LOCALE_COLORMENUSETUP_MENUCONTENT_INACTIVE));
	mf = new CMenuDForwarder(LOCALE_COLORMENU_BACKGROUND, true, NULL, chContentInactivecolor );
	mf->setHint("", LOCALE_MENU_HINT_INACTIVE_BACK);
	menu_colors->addItem(mf);

	mf = new CMenuDForwarder(LOCALE_COLORMENU_TEXTCOLOR, true, NULL, chContentInactiveTextcolor);
	mf->setHint("", LOCALE_MENU_HINT_INACTIVE_TEXTCOLOR);
	menu_colors->addItem(mf);

	menu_colors->addItem( new CMenuSeparator(CMenuSeparator::LINE | CMenuSeparator::STRING, LOCALE_COLORMENUSETUP_MENUCONTENT_SELECTED));
	mf = new CMenuDForwarder(LOCALE_COLORMENU_BACKGROUND, true, NULL, chContentSelectedcolor );
	mf->setHint("", LOCALE_MENU_HINT_SELECTED_BACK);
	menu_colors->addItem(mf);

	mf = new CMenuDForwarder(LOCALE_COLORMENU_TEXTCOLOR, true, NULL, chContentSelectedTextcolor );
	mf->setHint("", LOCALE_MENU_HINT_SELECTED_TEXT);
	menu_colors->addItem(mf);

	CColorChooser* chInfobarcolor = new CColorChooser(LOCALE_COLORMENU_BACKGROUND, &g_settings.infobar_red,
			&g_settings.infobar_green, &g_settings.infobar_blue, &g_settings.infobar_alpha, colorSetupNotifier);
	CColorChooser* chInfobarTextcolor = new CColorChooser(LOCALE_COLORMENU_TEXTCOLOR, &g_settings.infobar_Text_red,
			&g_settings.infobar_Text_green, &g_settings.infobar_Text_blue, NULL, colorSetupNotifier);

	menu_colors->addItem( new CMenuSeparator(CMenuSeparator::LINE | CMenuSeparator::STRING, LOCALE_COLORSTATUSBAR_TEXT));
	mf = new CMenuDForwarder(LOCALE_COLORMENU_BACKGROUND, true, NULL, chInfobarcolor );
	mf->setHint("", LOCALE_MENU_HINT_INFOBAR_BACK);
	menu_colors->addItem(mf);

	mf = new CMenuDForwarder(LOCALE_COLORMENU_TEXTCOLOR, true, NULL, chInfobarTextcolor );
	mf->setHint("", LOCALE_MENU_HINT_INFOBAR_TEXTCOLOR);
	menu_colors->addItem(mf);

	CColorChooser* chColored_Events = new CColorChooser(LOCALE_COLORMENU_TEXTCOLOR,	&g_settings.colored_events_red,
			&g_settings.colored_events_green, &g_settings.colored_events_blue, NULL, colorSetupNotifier);

	menu_colors->addItem( new CMenuSeparator(CMenuSeparator::LINE | CMenuSeparator::STRING, LOCALE_MISCSETTINGS_INFOBAR_COLORED_EVENTS));
	mf = new CMenuDForwarder(LOCALE_COLORMENU_TEXTCOLOR, true, NULL, chColored_Events );
	mf->setHint("", LOCALE_MENU_HINT_EVENT_TEXTCOLOR);
	menu_colors->addItem(mf);
}

/* for font size setup */
class CMenuNumberInput : public CMenuForwarder, CMenuTarget, CChangeObserver
{
private:
	CChangeObserver * observer;
	CConfigFile     * configfile;
	int32_t           defaultvalue;
	char              value[11];

protected:

	virtual const char * getOption(void)
		{
			sprintf(value, "%u", configfile->getInt32(locale_real_names[text], defaultvalue));
			return value;
		}

	virtual bool changeNotify(const neutrino_locale_t OptionName, void * Data)
		{
			configfile->setInt32(locale_real_names[text], atoi(value));
			return observer->changeNotify(OptionName, Data);
		}


public:
	CMenuNumberInput(const neutrino_locale_t Text, const int32_t DefaultValue, CChangeObserver * const Observer, CConfigFile * const Configfile) : CMenuForwarder(Text, true, NULL, this)
		{
			observer     = Observer;
			configfile   = Configfile;
			defaultvalue = DefaultValue;
		}

	int exec(CMenuTarget * parent, const std::string & action_Key)
		{
			CStringInput input(text, (char *)getOption(), 3, LOCALE_IPSETUP_HINT_1, LOCALE_IPSETUP_HINT_2, "0123456789 ", this);
			return input.exec(parent, action_Key);
		}
};

void COsdSetup::AddFontSettingItem(CMenuWidget &font_Settings, const SNeutrinoSettings::FONT_TYPES number_of_fontsize_entry)
{
	font_Settings.addItem(new CMenuNumberInput(neutrino_font[number_of_fontsize_entry].name, neutrino_font[number_of_fontsize_entry].defaultsize, fontsizenotifier, CNeutrinoApp::getInstance()->getConfigFile()));
}


//font settings menu
void COsdSetup::showOsdFontSizeSetup(CMenuWidget *menu_fonts)
{
	CMenuWidget *fontSettings = menu_fonts;
	CMenuForwarder * mf;

	fontSettings->addIntroItems(LOCALE_FONTMENU_HEAD);

	// select gui font file
	mf = new CMenuForwarder(LOCALE_COLORMENU_FONT, true, NULL, this, "select_font", CRCInput::RC_red, NEUTRINO_ICON_BUTTON_RED);
	mf->setHint("", LOCALE_MENU_HINT_FONT_GUI);
	fontSettings->addItem(mf);

	// select teletext font file
	mf = new CMenuForwarder(LOCALE_COLORMENU_FONT_TTX, true, NULL, this, "ttx_font",  CRCInput::RC_green, NEUTRINO_ICON_BUTTON_GREEN);
	mf->setHint("", LOCALE_MENU_HINT_FONT_TTX);
	fontSettings->addItem(mf);

	// contrast fonts
	CMenuOptionChooser * mc = new CMenuOptionChooser(LOCALE_COLORMENU_CONTRAST_FONTS, &g_settings.contrast_fonts, OPTIONS_OFF0_ON1_OPTIONS, OPTIONS_OFF0_ON1_OPTION_COUNT, true, this, CRCInput::RC_yellow, NEUTRINO_ICON_BUTTON_YELLOW);
	mc->setHint("", LOCALE_MENU_HINT_CONTRAST_FONTS);
	fontSettings->addItem(mc);

	fontSettings->addItem(new CMenuSeparator(CMenuSeparator::LINE | CMenuSeparator::STRING, LOCALE_FONTMENU_SIZES));

	//submenu font scaling
	mf = new CMenuForwarder(LOCALE_FONTMENU_SCALING, true, NULL, this, "font_scaling",  CRCInput::RC_blue, NEUTRINO_ICON_BUTTON_BLUE);
	mf->setHint("", LOCALE_MENU_HINT_FONT_SCALING);
	fontSettings->addItem(mf);

	//fontSettings->addItem( new CMenuForwarder(LOCALE_EPGPLUS_SELECT_FONT_NAME, true, NULL, this, "select_font"));

	mn_widget_id_t w_index = MN_WIDGET_ID_OSDSETUP_FONTSIZE_MENU;
	for (int i = 0; i < 6; i++)
	{
		CMenuWidget *fontSettingsSubMenu = new CMenuWidget(LOCALE_FONTMENU_HEAD, NEUTRINO_ICON_KEYBINDING, width, w_index);

		fontSettingsSubMenu->addIntroItems(font_sizes_groups[i].groupname);

		for (unsigned int j = 0; j < font_sizes_groups[i].count; j++)
		{
			AddFontSettingItem(*fontSettingsSubMenu, font_sizes_groups[i].content[j]);
		}
		fontSettingsSubMenu->addItem(GenericMenuSeparatorLine);
		fontSettingsSubMenu->addItem(new CMenuForwarder(LOCALE_OPTIONS_DEFAULT, true, NULL, this, font_sizes_groups[i].actionkey));

		mf = new CMenuDForwarder(font_sizes_groups[i].groupname, true, NULL, fontSettingsSubMenu, "", CRCInput::convertDigitToKey(i+1));
		mf->setHint("", font_sizes_groups[i].hint);
		fontSettings->addItem(mf);
		w_index++;
	}
}

//osd timeouts
void COsdSetup::showOsdTimeoutSetup(CMenuWidget* menu_timeout)
{
	menu_timeout->addIntroItems(LOCALE_COLORMENU_TIMING);

	for (int i = 0; i < SNeutrinoSettings::TIMING_SETTING_COUNT; i++)
	{
		CStringInput * timing_item = new CStringInput(timing_setting[i].name, g_settings.timing_string[i], 3, LOCALE_TIMING_HINT_1, LOCALE_TIMING_HINT_2, "0123456789 ", &timingsettingsnotifier);
		menu_timeout->addItem(new CMenuDForwarder(timing_setting[i].name, true, g_settings.timing_string[i], timing_item));
	}

	menu_timeout->addItem(GenericMenuSeparatorLine);
	menu_timeout->addItem(new CMenuForwarder(LOCALE_OPTIONS_DEFAULT, true, NULL, this, "osd.def", CRCInput::RC_red, NEUTRINO_ICON_BUTTON_RED));
}

#define LOCALE_MISCSETTINGS_INFOBAR_DISP_OPTIONS_COUNT 7
const CMenuOptionChooser::keyval  LOCALE_MISCSETTINGS_INFOBAR_DISP_OPTIONS[LOCALE_MISCSETTINGS_INFOBAR_DISP_OPTIONS_COUNT]=
{
   { 0 , LOCALE_MISCSETTINGS_INFOBAR_DISP_0 },
   { 1 , LOCALE_MISCSETTINGS_INFOBAR_DISP_1 },
   { 2 , LOCALE_MISCSETTINGS_INFOBAR_DISP_2 },
   { 3 , LOCALE_MISCSETTINGS_INFOBAR_DISP_3 },
   { 4 , LOCALE_MISCSETTINGS_INFOBAR_DISP_4 },
   { 5 , LOCALE_MISCSETTINGS_INFOBAR_DISP_5 },
   { 6 , LOCALE_MISCSETTINGS_INFOBAR_DISP_6 }
};

//menus
void COsdSetup::showOsdMenusSetup(CMenuWidget *menu_menus)
{
	submenu_menus = menu_menus;
	CMenuOptionChooser * mc;

	submenu_menus->addIntroItems(LOCALE_SETTINGS_MENUS);

	// menu position
	mc = new CMenuOptionChooser(LOCALE_SETTINGS_MENU_POS, &g_settings.menu_pos, MENU_DISP_POS_OPTIONS, MENU_DISP_POS_OPTIONS_COUNT, true, this);
	mc->setHint("", LOCALE_MENU_HINT_MENU_POS);
	submenu_menus->addItem(mc);

	// menu hints
	show_menu_hints = g_settings.show_menu_hints;
	mc = new CMenuOptionChooser(LOCALE_SETTINGS_MENU_HINTS, &show_menu_hints, OPTIONS_OFF0_ON1_OPTIONS, OPTIONS_OFF0_ON1_OPTION_COUNT, true, this);
	mc->setHint("", LOCALE_MENU_HINT_MENU_HINTS);
	submenu_menus->addItem(mc);
}

//infobar
void COsdSetup::showOsdInfobarSetup(CMenuWidget *menu_infobar)
{
	menu_infobar->addIntroItems(LOCALE_MISCSETTINGS_INFOBAR);

	CMenuOptionChooser * mc;

	// CA system
	mc = new CMenuOptionChooser(LOCALE_MISCSETTINGS_INFOBAR_CASYSTEM_DISPLAY, &g_settings.casystem_display, INFOBAR_CASYSTEM_MODE_OPTIONS, INFOBAR_CASYSTEM_MODE_OPTION_COUNT, true, this);
	mc->setHint("", LOCALE_MENU_HINT_INFOBAR_CASYS);
	menu_infobar->addItem(mc);

	// logo
	mc = new CMenuOptionChooser(LOCALE_MISCSETTINGS_INFOBAR_DISP_LOG, &g_settings.infobar_show_channellogo, LOCALE_MISCSETTINGS_INFOBAR_DISP_OPTIONS, LOCALE_MISCSETTINGS_INFOBAR_DISP_OPTIONS_COUNT, true);
	mc->setHint("", LOCALE_MENU_HINT_INFOBAR_LOGO);
	menu_infobar->addItem(mc);

	// logo directory
	CMenuForwarder * mf = new CMenuForwarder(LOCALE_MISCSETTINGS_INFOBAR_LOGO_HDD_DIR, true, g_settings.logo_hdd_dir, this, "logo_dir");
	mf->setHint("", LOCALE_MENU_HINT_INFOBAR_LOGO_DIR);
	menu_infobar->addItem(mf);

	// satellite
	mc = new CMenuOptionChooser(LOCALE_MISCSETTINGS_INFOBAR_SAT_DISPLAY, &g_settings.infobar_sat_display, OPTIONS_OFF0_ON1_OPTIONS, OPTIONS_OFF0_ON1_OPTION_COUNT, true);
	mc->setHint("", LOCALE_MENU_HINT_INFOBAR_SAT);
	menu_infobar->addItem(mc);

	// flash/hdd progress
	mc = new CMenuOptionChooser(LOCALE_MISCSETTINGS_INFOBAR_SHOW_SYSFS_HDD, &g_settings.infobar_show_sysfs_hdd, OPTIONS_OFF0_ON1_OPTIONS, OPTIONS_OFF0_ON1_OPTION_COUNT, true);
	mc->setHint("", LOCALE_MENU_HINT_INFOBAR_FILESYS);
	menu_infobar->addItem(mc);

	// resolution
	mc = new CMenuOptionChooser(LOCALE_MISCSETTINGS_INFOBAR_SHOW_RES, &g_settings.infobar_show_res, INFOBAR_SHOW_RES_MODE_OPTIONS, INFOBAR_SHOW_RES_MODE_OPTION_COUNT, true);
	mc->setHint("", LOCALE_MENU_HINT_INFOBAR_RES);
	menu_infobar->addItem(mc);

	// DD icon
	mc = new CMenuOptionChooser(LOCALE_MISCSETTINGS_INFOBAR_SHOW_DD_AVAILABLE, &g_settings.infobar_show_dd_available, OPTIONS_OFF0_ON1_OPTIONS, OPTIONS_OFF0_ON1_OPTION_COUNT, true);
	mc->setHint("", LOCALE_MENU_HINT_INFOBAR_DD);
	menu_infobar->addItem(mc);

	// tuner icon
	bool mc_active = false;
	show_tuner_icon = 0;
	// show possible option if we in single box mode, but don't touch the real settings
	int *p_show_tuner_icon = &show_tuner_icon;
	if (CFEManager::getInstance()->getFrontendCount() > 1) {
		mc_active = true;
		// use the real value of g_settings.infobar_show_tuner
		p_show_tuner_icon = &g_settings.infobar_show_tuner;
	}
	mc = new CMenuOptionChooser(LOCALE_MISCSETTINGS_INFOBAR_SHOW_TUNER, p_show_tuner_icon, OPTIONS_OFF0_ON1_OPTIONS, OPTIONS_OFF0_ON1_OPTION_COUNT, mc_active);
	mc->setHint("", LOCALE_MENU_HINT_INFOBAR_TUNER);
	menu_infobar->addItem(mc);

	// show on epg change
	mc = new CMenuOptionChooser(LOCALE_MISCSETTINGS_INFOBAR_SHOW, &g_settings.infobar_show, OPTIONS_OFF0_ON1_OPTIONS, OPTIONS_OFF0_ON1_OPTION_COUNT, true);
	mc->setHint("", LOCALE_MENU_HINT_INFOBAR_ON_EPG);
	menu_infobar->addItem(mc);

	// colored event
	mc = new CMenuOptionChooser(LOCALE_MISCSETTINGS_INFOBAR_COLORED_EVENTS, &g_settings.colored_events_infobar, OPTIONS_COLORED_EVENTS_OPTIONS, OPTIONS_COLORED_EVENTS_OPTION_COUNT, true);
	mc->setHint("", LOCALE_MENU_HINT_INFOBAR_COLORED_EVENT);
	menu_infobar->addItem(mc);

	// radiotext
	mc = new CMenuOptionChooser(LOCALE_MISCSETTINGS_RADIOTEXT, &g_settings.radiotext_enable, OPTIONS_OFF0_ON1_OPTIONS, OPTIONS_OFF0_ON1_OPTION_COUNT, true, this);
	mc->setHint("", LOCALE_MENU_HINT_INFOBAR_RADIOTEXT);
 	menu_infobar->addItem(mc);
}

//channellist
void COsdSetup::showOsdChanlistSetup(CMenuWidget *menu_chanlist)
{
	CMenuOptionChooser * mc;

	menu_chanlist->addIntroItems(LOCALE_MISCSETTINGS_CHANNELLIST);

	// channellist additional
	mc = new CMenuOptionChooser(LOCALE_CHANNELLIST_ADDITIONAL, &g_settings.channellist_additional, CHANNELLIST_ADDITIONAL_OPTIONS, CHANNELLIST_ADDITIONAL_OPTION_COUNT, true);
	mc->setHint("", LOCALE_MENU_HINT_CHANNELLIST_ADDITIONAL);
	menu_chanlist->addItem(mc);

	// epg align
	mc = new CMenuOptionChooser(LOCALE_MISCSETTINGS_CHANNELLIST_EPGTEXT_ALIGN, &g_settings.channellist_epgtext_align_right, CHANNELLIST_EPGTEXT_ALIGN_RIGHT_OPTIONS, CHANNELLIST_EPGTEXT_ALIGN_RIGHT_OPTIONS_COUNT, true);
	mc->setHint("", LOCALE_MENU_HINT_CHANNELLIST_EPG_ALIGN);
	menu_chanlist->addItem(mc);

	// extended channel list
	mc = new CMenuOptionChooser(LOCALE_CHANNELLIST_EXTENDED, &g_settings.channellist_extended, OPTIONS_OFF0_ON1_OPTIONS, OPTIONS_OFF0_ON1_OPTION_COUNT, true);
	mc->setHint("", LOCALE_MENU_HINT_CHANNELLIST_EXTENDED);
	menu_chanlist->addItem(mc);

	// foot
	mc = new CMenuOptionChooser(LOCALE_CHANNELLIST_FOOT, &g_settings.channellist_foot, CHANNELLIST_FOOT_OPTIONS, CHANNELLIST_FOOT_OPTIONS_COUNT, true);
	mc->setHint("", LOCALE_MENU_HINT_CHANNELLIST_FOOT);
	menu_chanlist->addItem(mc);

	// colored event
	mc = new CMenuOptionChooser(LOCALE_MISCSETTINGS_CHANNELLIST_COLORED_EVENTS, &g_settings.colored_events_channellist, OPTIONS_COLORED_EVENTS_OPTIONS, OPTIONS_COLORED_EVENTS_OPTION_COUNT, true);
	mc->setHint("", LOCALE_MENU_HINT_CHANNELLIST_COLORED);
	menu_chanlist->addItem(mc);
}

//eventlist
void COsdSetup::showOsdEventlistSetup(CMenuWidget *menu_eventlist)
{
	CMenuOptionChooser * mc;

	menu_eventlist->addIntroItems(LOCALE_EVENTLIST_NAME);

	// eventlist additional
	mc = new CMenuOptionChooser(LOCALE_EVENTLIST_ADDITIONAL, &g_settings.eventlist_additional, OPTIONS_OFF0_ON1_OPTIONS, OPTIONS_OFF0_ON1_OPTION_COUNT, true);
	mc->setHint("", LOCALE_MENU_HINT_EVENTLIST_ADDITIONAL);
	menu_eventlist->addItem(mc);
}

// volume
void COsdSetup::showOsdVolumeSetup(CMenuWidget *menu_volume)
{
	CMenuOptionChooser * mc;

	menu_volume->addIntroItems(LOCALE_MISCSETTINGS_VOLUME);

	// volume position
	mc = new CMenuOptionChooser(LOCALE_EXTRA_VOLUME_POS, &g_settings.volume_pos, VOLUMEBAR_DISP_POS_OPTIONS, VOLUMEBAR_DISP_POS_OPTIONS_COUNT, true, this);
	mc->setHint("", LOCALE_MENU_HINT_VOLUME_POS);
	menu_volume->addItem(mc);

	// volume digits
	mc = new CMenuOptionChooser(LOCALE_EXTRA_VOLUME_DIGITS, &g_settings.volume_digits, OPTIONS_OFF0_ON1_OPTIONS, OPTIONS_OFF0_ON1_OPTION_COUNT, true, this);
	mc->setHint("", LOCALE_MENU_HINT_VOLUME_DIGITS);
	menu_volume->addItem(mc);

	// show mute at volume 0
	mc = new CMenuOptionChooser(LOCALE_EXTRA_SHOW_MUTE_ICON, &g_settings.show_mute_icon, OPTIONS_OFF0_ON1_OPTIONS, OPTIONS_OFF0_ON1_OPTION_COUNT, true);
	mc->setHint("", LOCALE_MENU_HINT_SHOW_MUTE_ICON);
	menu_volume->addItem(mc);
}

bool COsdSetup::changeNotify(const neutrino_locale_t OptionName, void * data)
{
	if(ARE_LOCALES_EQUAL(OptionName, LOCALE_COLORMENU_CONTRAST_FONTS))
		return true;
	else if(ARE_LOCALES_EQUAL(OptionName, LOCALE_SETTINGS_MENU_POS)) {
		submenu_menus->hide();
		return true;
	}
	else if (ARE_LOCALES_EQUAL(OptionName, LOCALE_SETTINGS_MENU_HINTS)) {
		/* change option after hide, to let hide clear hint */
		submenu_menus->hide();
		g_settings.show_menu_hints = * (int*) data;
		return true;
	}
	else if((ARE_LOCALES_EQUAL(OptionName, LOCALE_MISCSETTINGS_INFOBAR_CASYSTEM_DISPLAY)) ||
		(ARE_LOCALES_EQUAL(OptionName, LOCALE_MISCSETTINGS_INFOBAR_SHOW_TUNER))) {
		if (g_InfoViewer == NULL)
			g_InfoViewer = new CInfoViewer;
		g_InfoViewer->changePB();
		return false;
	}
	else if(ARE_LOCALES_EQUAL(OptionName, LOCALE_COLORMENU_OSD_PRESET)) {
		int preset = * (int *) data;
		printf("preset %d (setting %d)\n", preset, g_settings.screen_preset);

		g_settings.screen_StartX = g_settings.screen_preset ? g_settings.screen_StartX_lcd : g_settings.screen_StartX_crt;
		g_settings.screen_StartY = g_settings.screen_preset ? g_settings.screen_StartY_lcd : g_settings.screen_StartY_crt;
		g_settings.screen_EndX = g_settings.screen_preset ? g_settings.screen_EndX_lcd : g_settings.screen_EndX_crt;
		g_settings.screen_EndY = g_settings.screen_preset ? g_settings.screen_EndY_lcd : g_settings.screen_EndY_crt;
		osd_menu->hide();
		if (g_InfoViewer == NULL)
			g_InfoViewer = new CInfoViewer;
		g_InfoViewer->changePB();
		return true;
	}
	else if (ARE_LOCALES_EQUAL(OptionName, LOCALE_EXTRA_ROUNDED_CORNERS)) {
		osd_menu->hide();
		g_settings.rounded_corners = * (int*) data;
		return true;
	}
	else if(ARE_LOCALES_EQUAL(OptionName, LOCALE_MISCSETTINGS_RADIOTEXT)) {
		if (g_settings.radiotext_enable) {
			if (g_Radiotext == NULL)
				g_Radiotext = new CRadioText;
			if (g_Radiotext && ((CNeutrinoApp::getInstance()->getMode()) == NeutrinoMessages::mode_radio))
				g_Radiotext->setPid(g_RemoteControl->current_PIDs.APIDs[g_RemoteControl->current_PIDs.PIDs.selected_apid].pid);
		} else {
			if (g_Radiotext)
				g_Radiotext->radiotext_stop();
			delete g_Radiotext;
			g_Radiotext = NULL;
		}
	}
	else if(ARE_LOCALES_EQUAL(OptionName, LOCALE_EXTRA_VOLUME_DIGITS)) {
		CVolumeHelper::getInstance()->refresh();
		return false;
	}
	return false;
}

int COsdSetup::showContextChanlistMenu()
{
	static int cselected = -1;

	CMenuWidget * menu_chanlist = new CMenuWidget(LOCALE_MAINMENU_SETTINGS, NEUTRINO_ICON_SETTINGS, width);
	menu_chanlist->enableSaveScreen(true);
	menu_chanlist->enableFade(false);
	menu_chanlist->setSelected(cselected);

	CMenuOptionChooser * mc;

	menu_chanlist->addIntroItems(LOCALE_MISCSETTINGS_CHANNELLIST);//, NONEXISTANT_LOCALE, CMenuWidget::BTN_TYPE_CANCEL);

	mc = new CMenuOptionChooser(LOCALE_CHANNELLIST_ADDITIONAL, &g_settings.channellist_additional, CHANNELLIST_ADDITIONAL_OPTIONS, CHANNELLIST_ADDITIONAL_OPTION_COUNT, true);
	mc->setHint("", LOCALE_MENU_HINT_CHANNELLIST_ADDITIONAL);
	menu_chanlist->addItem(mc);

	mc = new CMenuOptionChooser(LOCALE_MISCSETTINGS_CHANNELLIST_EPGTEXT_ALIGN, &g_settings.channellist_epgtext_align_right, CHANNELLIST_EPGTEXT_ALIGN_RIGHT_OPTIONS, CHANNELLIST_EPGTEXT_ALIGN_RIGHT_OPTIONS_COUNT, true);
	mc->setHint("", LOCALE_MENU_HINT_CHANNELLIST_EPG_ALIGN);
	menu_chanlist->addItem(mc);

	mc = new CMenuOptionChooser(LOCALE_CHANNELLIST_EXTENDED, &g_settings.channellist_extended, OPTIONS_OFF0_ON1_OPTIONS, OPTIONS_OFF0_ON1_OPTION_COUNT, true);
	mc->setHint("", LOCALE_MENU_HINT_CHANNELLIST_EXTENDED);
	menu_chanlist->addItem(mc);

	mc = new CMenuOptionChooser(LOCALE_CHANNELLIST_FOOT, &g_settings.channellist_foot, CHANNELLIST_FOOT_OPTIONS, CHANNELLIST_FOOT_OPTIONS_COUNT, true);
	mc->setHint("", LOCALE_MENU_HINT_CHANNELLIST_FOOT);
	menu_chanlist->addItem(mc);

	mc = new CMenuOptionChooser(LOCALE_MISCSETTINGS_CHANNELLIST_COLORED_EVENTS, &g_settings.colored_events_channellist, OPTIONS_COLORED_EVENTS_OPTIONS, OPTIONS_COLORED_EVENTS_OPTION_COUNT, true);
	mc->setHint("", LOCALE_MENU_HINT_CHANNELLIST_COLORED);
	menu_chanlist->addItem(mc);

	menu_chanlist->addItem(new CMenuSeparator(CMenuSeparator::LINE));

	CMenuWidget *fontSettingsSubMenu = new CMenuWidget(LOCALE_FONTMENU_HEAD, NEUTRINO_ICON_KEYBINDING);
	fontSettingsSubMenu->enableSaveScreen(true);
	fontSettingsSubMenu->enableFade(false);

	int i = 1;
	fontSettingsSubMenu->addIntroItems(font_sizes_groups[i].groupname);//, NONEXISTANT_LOCALE, CMenuWidget::BTN_TYPE_CANCEL);

	for (unsigned int j = 0; j < font_sizes_groups[i].count; j++)
	{
		AddFontSettingItem(*fontSettingsSubMenu, font_sizes_groups[i].content[j]);
	}
	fontSettingsSubMenu->addItem(GenericMenuSeparatorLine);
	fontSettingsSubMenu->addItem(new CMenuForwarder(LOCALE_OPTIONS_DEFAULT, true, NULL, this, font_sizes_groups[i].actionkey));

	CMenuForwarder * mf = new CMenuDForwarder(LOCALE_FONTMENU_HEAD, true, NULL, fontSettingsSubMenu, NULL, CRCInput::RC_red, NEUTRINO_ICON_BUTTON_RED);
	mf->setHint("", LOCALE_MENU_HINT_FONTS);
	menu_chanlist->addItem(mf);

	int res = menu_chanlist->exec(NULL, "");
	cselected = menu_chanlist->getSelected();
	delete menu_chanlist;
	return res;
}

//screenshot
#define SCREENSHOT_FMT_OPTION_COUNT 3
const CMenuOptionChooser::keyval_ext SCREENSHOT_FMT_OPTIONS[SCREENSHOT_FMT_OPTION_COUNT] =
{
	{ CScreenShot::FORMAT_PNG,   NONEXISTANT_LOCALE, "PNG"  },
	{ CScreenShot::FORMAT_JPG,   NONEXISTANT_LOCALE, "JPEG" },
	{ CScreenShot::FORMAT_BMP,   NONEXISTANT_LOCALE, "BMP" }
};
#define SCREENSHOT_OPTION_COUNT 2
const CMenuOptionChooser::keyval SCREENSHOT_OPTIONS[SCREENSHOT_OPTION_COUNT] =
{
	{ 0, LOCALE_SCREENSHOT_TV },
	{ 1, LOCALE_SCREENSHOT_OSD   }
};

void COsdSetup::showOsdScreenShotSetup(CMenuWidget *menu_screenshot)
{
	menu_screenshot->addIntroItems(LOCALE_SCREENSHOT_MENU);
	if((uint)g_settings.key_screenshot == CRCInput::RC_nokey)
		menu_screenshot->addItem( new CMenuSeparator(CMenuSeparator::LINE | CMenuSeparator::STRING, LOCALE_SCREENSHOT_INFO));

	CMenuForwarder * mf = new CMenuForwarder(LOCALE_SCREENSHOT_DEFDIR, true, g_settings.screenshot_dir, this, "screenshot_dir");
	mf->setHint("", LOCALE_MENU_HINT_SCREENSHOT_DIR);
	menu_screenshot->addItem(mf);

	CMenuOptionNumberChooser * nc = new CMenuOptionNumberChooser(LOCALE_SCREENSHOT_COUNT, &g_settings.screenshot_count, true, 1, 5, NULL);
	nc->setHint("", LOCALE_MENU_HINT_SCREENSHOT_COUNT);
	menu_screenshot->addItem(nc);

	CMenuOptionChooser * mc = new CMenuOptionChooser(LOCALE_SCREENSHOT_FORMAT, &g_settings.screenshot_format, SCREENSHOT_FMT_OPTIONS, SCREENSHOT_FMT_OPTION_COUNT, true);
	mc->setHint("", LOCALE_MENU_HINT_SCREENSHOT_FORMAT);
	menu_screenshot->addItem(mc);

	mc = new CMenuOptionChooser(LOCALE_SCREENSHOT_RES, &g_settings.screenshot_mode, SCREENSHOT_OPTIONS, SCREENSHOT_OPTION_COUNT, true);
	mc->setHint("", LOCALE_MENU_HINT_SCREENSHOT_RES);
	menu_screenshot->addItem(mc);

	mc = new CMenuOptionChooser(LOCALE_SCREENSHOT_VIDEO, &g_settings.screenshot_video, OPTIONS_OFF0_ON1_OPTIONS, OPTIONS_OFF0_ON1_OPTION_COUNT, true);
	mc->setHint("", LOCALE_MENU_HINT_SCREENSHOT_VIDEO);
	menu_screenshot->addItem(mc);

	mc = new CMenuOptionChooser(LOCALE_SCREENSHOT_SCALE, &g_settings.screenshot_scale, OPTIONS_OFF0_ON1_OPTIONS, OPTIONS_OFF0_ON1_OPTION_COUNT, true);
	mc->setHint("", LOCALE_MENU_HINT_SCREENSHOT_SCALE);
	menu_screenshot->addItem(mc);

	mc = new CMenuOptionChooser(LOCALE_SCREENSHOT_COVER, &g_settings.screenshot_cover, OPTIONS_OFF0_ON1_OPTIONS, OPTIONS_OFF0_ON1_OPTION_COUNT, true);
	mc->setHint("", LOCALE_MENU_HINT_SCREENSHOT_COVER);
	menu_screenshot->addItem(mc);
}
