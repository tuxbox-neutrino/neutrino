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
#include "osd_helpers.h"
#include "themes.h"
#include "screensetup.h"
#include "osdlang_setup.h"
#include "filebrowser.h"
#include "osd_progressbar_setup.h"

#include <gui/audiomute.h>
#include <gui/color_custom.h>
#include <gui/infoclock.h>
#include <gui/timeosd.h>
#include <gui/widget/icons.h>
#include <gui/widget/colorchooser.h>
#include <gui/widget/stringinput.h>

#include <driver/screen_max.h>
#include <driver/neutrinofonts.h>
#include <driver/screen_max.h>
#include <driver/screenshot.h>
#include <driver/volume.h>
#include <driver/radiotext.h>

#include <zapit/femanager.h>
#include <system/debug.h>
#include <system/helpers.h>

extern CRemoteControl * g_RemoteControl;

extern const char * locale_real_names[];
extern std::string ttx_font_file;
extern CTimeOSD *FileTimeOSD;

COsdSetup::COsdSetup(int wizard_mode)
{
	frameBuffer = CFrameBuffer::getInstance();
	colorSetupNotifier = new CColorSetupNotifier();
	fontsizenotifier = new CFontSizeNotifier;
	colorInfoclockNotifier = NULL;
	screensaverNotifier = NULL;
	channellistNotifier = NULL;
	infobarHddNotifier = NULL;
	osd_menu = NULL;
	submenu_menus = NULL;
	mfFontFile = NULL;
	mfTtxFontFile = NULL;
	mfWindowSize = NULL;
	win_demo = NULL;
	osd_menu_colors = NULL;
	is_wizard = wizard_mode;

	width = 40;
	show_menu_hints = 0;
	show_tuner_icon = 0;
}

COsdSetup::~COsdSetup()
{
	delete colorSetupNotifier;
	delete fontsizenotifier;
	delete win_demo;
	if (osd_menu_colors)
		delete osd_menu_colors;
}

//font settings
const SNeutrinoSettings::FONT_TYPES channellist_font_sizes[] =
{
	SNeutrinoSettings::FONT_TYPE_CHANNELLIST,
	SNeutrinoSettings::FONT_TYPE_CHANNELLIST_DESCR,
	SNeutrinoSettings::FONT_TYPE_CHANNELLIST_NUMBER,
	SNeutrinoSettings::FONT_TYPE_CHANNELLIST_EVENT,
	SNeutrinoSettings::FONT_TYPE_CHANNEL_NUM_ZAP
};
size_t channellist_font_items = sizeof(channellist_font_sizes)/sizeof(channellist_font_sizes[0]);

const SNeutrinoSettings::FONT_TYPES eventlist_font_sizes[] =
{
	SNeutrinoSettings::FONT_TYPE_EVENTLIST_TITLE,
	SNeutrinoSettings::FONT_TYPE_EVENTLIST_ITEMLARGE,
	SNeutrinoSettings::FONT_TYPE_EVENTLIST_ITEMSMALL,
	SNeutrinoSettings::FONT_TYPE_EVENTLIST_DATETIME,
	SNeutrinoSettings::FONT_TYPE_EVENTLIST_EVENT
};
size_t eventlist_font_items = sizeof(eventlist_font_sizes)/sizeof(eventlist_font_sizes[0]);

const SNeutrinoSettings::FONT_TYPES infobar_font_sizes[] =
{
	SNeutrinoSettings::FONT_TYPE_INFOBAR_NUMBER,
	SNeutrinoSettings::FONT_TYPE_INFOBAR_CHANNAME,
	SNeutrinoSettings::FONT_TYPE_INFOBAR_INFO,
	SNeutrinoSettings::FONT_TYPE_INFOBAR_SMALL
};
size_t infobar_font_items = sizeof(infobar_font_sizes)/sizeof(infobar_font_sizes[0]);

const SNeutrinoSettings::FONT_TYPES epg_font_sizes[] =
{
	SNeutrinoSettings::FONT_TYPE_EPG_TITLE,
	SNeutrinoSettings::FONT_TYPE_EPG_INFO1,
	SNeutrinoSettings::FONT_TYPE_EPG_INFO2,
	SNeutrinoSettings::FONT_TYPE_EPG_DATE,
	SNeutrinoSettings::FONT_TYPE_EPGPLUS_ITEM
};
size_t epg_font_items = sizeof(epg_font_sizes)/sizeof(epg_font_sizes[0]);

const SNeutrinoSettings::FONT_TYPES menu_font_sizes[] =
{
	SNeutrinoSettings::FONT_TYPE_MENU_TITLE,
	SNeutrinoSettings::FONT_TYPE_MENU,
	SNeutrinoSettings::FONT_TYPE_MENU_INFO,
	SNeutrinoSettings::FONT_TYPE_MENU_FOOT,
	SNeutrinoSettings::FONT_TYPE_MENU_HINT
};
size_t menu_font_items = sizeof(menu_font_sizes)/sizeof(menu_font_sizes[0]);

const SNeutrinoSettings::FONT_TYPES moviebrowser_font_sizes[] =
{
	SNeutrinoSettings::FONT_TYPE_MOVIEBROWSER_HEAD,
	SNeutrinoSettings::FONT_TYPE_MOVIEBROWSER_LIST,
	SNeutrinoSettings::FONT_TYPE_MOVIEBROWSER_INFO
};
size_t moviebrowser_font_items = sizeof(moviebrowser_font_sizes)/sizeof(moviebrowser_font_sizes[0]);

const SNeutrinoSettings::FONT_TYPES other_font_sizes[] =
{
	SNeutrinoSettings::FONT_TYPE_SUBTITLES,
	SNeutrinoSettings::FONT_TYPE_FILEBROWSER_ITEM,
	SNeutrinoSettings::FONT_TYPE_BUTTON_TEXT
};
size_t other_font_items = sizeof(other_font_sizes)/sizeof(other_font_sizes[0]);

const SNeutrinoSettings::FONT_TYPES msgtext_font_sizes[] =
{
	SNeutrinoSettings::FONT_TYPE_MESSAGE_TEXT
};
size_t msgtext_font_items = sizeof(msgtext_font_sizes)/sizeof(msgtext_font_sizes[0]);


font_sizes_groups font_sizes_groups[] =
{
	{LOCALE_FONTMENU_MENU       , menu_font_items       , menu_font_sizes       , "fontsize.dmen", LOCALE_MENU_HINT_MENU_FONTS },
	{LOCALE_FONTMENU_CHANNELLIST, channellist_font_items, channellist_font_sizes, "fontsize.dcha", LOCALE_MENU_HINT_CHANNELLIST_FONTS },
	{LOCALE_FONTMENU_EVENTLIST  , eventlist_font_items  , eventlist_font_sizes  , "fontsize.deve", LOCALE_MENU_HINT_EVENTLIST_FONTS },
	{LOCALE_FONTMENU_EPG        , epg_font_items        , epg_font_sizes        , "fontsize.depg", LOCALE_MENU_HINT_EPG_FONTS },
	{LOCALE_FONTMENU_INFOBAR    , infobar_font_items    , infobar_font_sizes    , "fontsize.dinf", LOCALE_MENU_HINT_INFOBAR_FONTS },
	{LOCALE_FONTMENU_MOVIEBROWSER,moviebrowser_font_items,moviebrowser_font_sizes,"fontsize.dmbr", LOCALE_MENU_HINT_MOVIEBROWSER_FONTS },
	{LOCALE_FONTMENU_MESSAGES   , msgtext_font_items    , msgtext_font_sizes    , "fontsize.dmsg",  LOCALE_MENU_HINT_MESSAGE_FONTS },
	{LOCALE_FONTMENU_OTHER      , other_font_items      , other_font_sizes      , "fontsize.doth", LOCALE_MENU_HINT_OTHER_FONTS }
};
#define FONT_GROUP_COUNT (sizeof(font_sizes_groups)/sizeof(font_sizes_groups[0]))

font_sizes_struct neutrino_font[SNeutrinoSettings::FONT_TYPE_COUNT] =
{
	{LOCALE_FONTSIZE_MENU               ,  20, CNeutrinoFonts::FONT_STYLE_BOLD   , 0},
	{LOCALE_FONTSIZE_MENU_TITLE         ,  30, CNeutrinoFonts::FONT_STYLE_BOLD   , 0},
	{LOCALE_FONTSIZE_MENU_INFO          ,  16, CNeutrinoFonts::FONT_STYLE_REGULAR, 0},
	{LOCALE_FONTSIZE_MENU_FOOT          ,  14, CNeutrinoFonts::FONT_STYLE_REGULAR, 1},
	{LOCALE_FONTSIZE_EPG_TITLE          ,  25, CNeutrinoFonts::FONT_STYLE_REGULAR, 1},
	{LOCALE_FONTSIZE_EPG_INFO1          ,  17, CNeutrinoFonts::FONT_STYLE_ITALIC , 2},
	{LOCALE_FONTSIZE_EPG_INFO2          ,  17, CNeutrinoFonts::FONT_STYLE_REGULAR, 2},
	{LOCALE_FONTSIZE_EPG_DATE           ,  15, CNeutrinoFonts::FONT_STYLE_REGULAR, 2},
	{LOCALE_FONTSIZE_EPGPLUS_ITEM       ,  18, CNeutrinoFonts::FONT_STYLE_REGULAR, 2},
	{LOCALE_FONTSIZE_EVENTLIST_TITLE    ,  30, CNeutrinoFonts::FONT_STYLE_REGULAR, 0},
	{LOCALE_FONTSIZE_EVENTLIST_ITEMLARGE,  20, CNeutrinoFonts::FONT_STYLE_BOLD   , 1},
	{LOCALE_FONTSIZE_EVENTLIST_ITEMSMALL,  14, CNeutrinoFonts::FONT_STYLE_REGULAR, 1},
	{LOCALE_FONTSIZE_EVENTLIST_DATETIME ,  16, CNeutrinoFonts::FONT_STYLE_REGULAR, 1},
	{LOCALE_FONTSIZE_EVENTLIST_EVENT    ,  17, CNeutrinoFonts::FONT_STYLE_REGULAR, 1},
	{LOCALE_FONTSIZE_CHANNELLIST        ,  20, CNeutrinoFonts::FONT_STYLE_BOLD   , 1},
	{LOCALE_FONTSIZE_CHANNELLIST_DESCR  ,  20, CNeutrinoFonts::FONT_STYLE_REGULAR, 1},
	{LOCALE_FONTSIZE_CHANNELLIST_NUMBER ,  14, CNeutrinoFonts::FONT_STYLE_BOLD   , 2},
	{LOCALE_FONTSIZE_CHANNELLIST_EVENT  ,  17, CNeutrinoFonts::FONT_STYLE_REGULAR, 2},
	{LOCALE_FONTSIZE_CHANNEL_NUM_ZAP    ,  40, CNeutrinoFonts::FONT_STYLE_BOLD   , 0},
	{LOCALE_FONTSIZE_INFOBAR_NUMBER     ,  50, CNeutrinoFonts::FONT_STYLE_BOLD   , 0},
	{LOCALE_FONTSIZE_INFOBAR_CHANNAME   ,  30, CNeutrinoFonts::FONT_STYLE_BOLD   , 0},
	{LOCALE_FONTSIZE_INFOBAR_INFO       ,  20, CNeutrinoFonts::FONT_STYLE_REGULAR, 1},
	{LOCALE_FONTSIZE_INFOBAR_SMALL      ,  14, CNeutrinoFonts::FONT_STYLE_REGULAR, 1},
	{LOCALE_FONTSIZE_FILEBROWSER_ITEM   ,  16, CNeutrinoFonts::FONT_STYLE_BOLD   , 1},
	{LOCALE_FONTSIZE_MENU_HINT          ,  16, CNeutrinoFonts::FONT_STYLE_REGULAR, 0},
	{LOCALE_FONTSIZE_MOVIEBROWSER_HEAD  ,  15, CNeutrinoFonts::FONT_STYLE_REGULAR, 2},
	{LOCALE_FONTSIZE_MOVIEBROWSER_LIST  ,  17, CNeutrinoFonts::FONT_STYLE_REGULAR, 0},
	{LOCALE_FONTSIZE_MOVIEBROWSER_INFO  ,  17, CNeutrinoFonts::FONT_STYLE_REGULAR, 0},
	{LOCALE_FONTSIZE_SUBTITLES          ,  25, CNeutrinoFonts::FONT_STYLE_BOLD   , 0},
	{LOCALE_FONTSIZE_MESSAGE_TEXT       ,  20, CNeutrinoFonts::FONT_STYLE_REGULAR, 1},
	{LOCALE_FONTSIZE_BUTTON_TEXT        ,  14, CNeutrinoFonts::FONT_STYLE_REGULAR, 0}
};

int COsdSetup::exec(CMenuTarget* parent, const std::string &actionKey)
{
	dprintf(DEBUG_DEBUG, "init osd setup\n");

	printf("COsdSetup::exec:: action  %s\n", actionKey.c_str());
	if(parent != NULL)
		parent->hide();

	int res = menu_return::RETURN_REPAINT;
	neutrino_msg_t      msg;
	neutrino_msg_data_t data;

	if(actionKey == "select_font")
	{
		CFileBrowser fileBrowser;
		CFileFilter fileFilter;
		fileFilter.addFilter("ttf");
		fileBrowser.Filter = &fileFilter;
		if (fileBrowser.exec(FONTDIR) == true)
		{
			g_settings.font_file = fileBrowser.getSelectedFile()->Name;
			printf("[neutrino] new font file %s\n", fileBrowser.getSelectedFile()->Name.c_str());
			CNeutrinoApp::getInstance()->SetupFonts(CNeutrinoFonts::FONTSETUP_ALL);
			osdFontFile = "(" + getBaseName(fileBrowser.getSelectedFile()->Name) + ")";
			mfFontFile->setOption(osdFontFile.c_str());
		}
		return res;
	}
	else if(actionKey == "ttx_font")
	{
		CFileBrowser fileBrowser;
		CFileFilter fileFilter;
		fileFilter.addFilter("ttf");
		fileBrowser.Filter = &fileFilter;
		if (fileBrowser.exec(FONTDIR) == true)
		{
			g_settings.ttx_font_file = fileBrowser.getSelectedFile()->Name;
			ttx_font_file = fileBrowser.getSelectedFile()->Name;
			printf("[neutrino] ttx font file %s\n", fileBrowser.getSelectedFile()->Name.c_str());
			CNeutrinoApp::getInstance()->SetupFonts(CNeutrinoFonts::FONTSETUP_NEUTRINO_FONT | CNeutrinoFonts::FONTSETUP_NEUTRINO_FONT_INST);
			osdTtxFontFile = "(" + getBaseName(fileBrowser.getSelectedFile()->Name) + ")";
			mfTtxFontFile->setOption(osdTtxFontFile.c_str());
		}
		return res;
	}
	else if (actionKey == "font_scaling") {
		int fs_x = g_settings.font_scaling_x;
		int fs_y = g_settings.font_scaling_y;

		CMenuWidget fontscale(LOCALE_FONTMENU_HEAD, NEUTRINO_ICON_COLORS, width, MN_WIDGET_ID_OSDSETUP_FONTSCALE);
		fontscale.addIntroItems(LOCALE_FONTMENU_SCALING);

		CMenuOptionNumberChooser* mc = new CMenuOptionNumberChooser(LOCALE_FONTMENU_SCALING_X, &g_settings.font_scaling_x, true, 50, 200, this);
		mc->setNumericInput(true);
		mc->setNumberFormat("%d%%");
		fontscale.addItem(mc);

		mc = new CMenuOptionNumberChooser(LOCALE_FONTMENU_SCALING_Y, &g_settings.font_scaling_y, true, 50, 200, this);
		mc->setNumericInput(true);
		mc->setNumberFormat("%d%%");
		fontscale.addItem(mc);

		res = fontscale.exec(NULL, "");

		if (fs_x != g_settings.font_scaling_x || fs_y != g_settings.font_scaling_y) {
			printf("[neutrino] new font scale settings x: %d%% y: %d%%\n", g_settings.font_scaling_x, g_settings.font_scaling_y);
			CNeutrinoApp::getInstance()->SetupFonts(CNeutrinoFonts::FONTSETUP_NEUTRINO_FONT | CNeutrinoFonts::FONTSETUP_NEUTRINO_FONT_INST | CNeutrinoFonts::FONTSETUP_DYN_FONT);
			if (CNeutrinoApp::getInstance()->channelList)
				CNeutrinoApp::getInstance()->channelList->ResetModules(); //force re init of all modules
		}
		return res;
	}
	else if(actionKey=="window_size") {
		int old_window_width = g_settings.window_width;
		int old_window_height = g_settings.window_height;

		paintWindowSize(old_window_width, old_window_height);

		uint64_t timeoutEnd = CRCInput::calcTimeoutEnd(g_settings.timing[SNeutrinoSettings::TIMING_MENU] == 0 ? 0xFFFF : g_settings.timing[SNeutrinoSettings::TIMING_MENU]);

		bool loop=true;
		while (loop) {
			g_RCInput->getMsgAbsoluteTimeout(&msg, &data, &timeoutEnd, true);

			if ( msg <= CRCInput::RC_MaxRC )
				timeoutEnd = CRCInput::calcTimeoutEnd(g_settings.timing[SNeutrinoSettings::TIMING_MENU] == 0 ? 0xFFFF : g_settings.timing[SNeutrinoSettings::TIMING_MENU]);

			if ( msg == CRCInput::RC_ok ) {
				memset(window_size_value, 0, sizeof(window_size_value));
				snprintf(window_size_value, sizeof(window_size_value), "%d / %d", g_settings.window_width, g_settings.window_height);
				mfWindowSize->setOption(window_size_value);
				break;
			} else if ((msg == CRCInput::RC_home) || (msg == CRCInput::RC_timeout)) {
				g_settings.window_width = old_window_width;
				g_settings.window_height = old_window_height;
				loop = false;
			} else if ((msg == CRCInput::RC_page_up) || (msg == CRCInput::RC_page_down) ||
				(msg == CRCInput::RC_left) || (msg == CRCInput::RC_right) ||
				(msg == CRCInput::RC_up) || (msg == CRCInput::RC_down)) {

				int dir = 1;
				if ((msg == CRCInput::RC_page_down) || (msg == CRCInput::RC_left) || (msg == CRCInput::RC_down))
					dir = -1;

				int mask = 3;
				if ((msg == CRCInput::RC_left) || (msg == CRCInput::RC_right))
					mask = 1;
				else if ((msg == CRCInput::RC_up) || (msg == CRCInput::RC_down))
					mask = 2;
				if (mask & 1)
					g_settings.window_width += dir;
				if (mask & 2)
					g_settings.window_height += dir;

				paintWindowSize(g_settings.window_width, g_settings.window_height);

			} else if ((msg == CRCInput::RC_left) || (msg == CRCInput::RC_right)) {
			} else if (msg > CRCInput::RC_MaxRC) {
				if ( CNeutrinoApp::getInstance()->handleMsg( msg, data ) & messages_return::cancel_all ) {
					loop = false;
					res = menu_return::RETURN_EXIT_ALL;
				}
			}
		}
		win_demo->kill();

		return res;
	}
	else if(actionKey=="osd.def") {
		for (int i = 0; i < SNeutrinoSettings::TIMING_SETTING_COUNT; i++)
			g_settings.timing[i] = timing_setting[i].default_timing;
		return res;
	}
	else if(actionKey=="logo_dir") {
		const char *action_str = "logo";
		chooserDir(g_settings.logo_hdd_dir, false, action_str);
		return res;
	}
	else if(actionKey=="screenshot_dir") {
		const char *action_str = "screenshot";
		chooserDir(g_settings.screenshot_dir, true, action_str);
		return res;
	}
	else if(actionKey=="screensaver_dir") {
		const char *action_str = "screensaver";
		chooserDir(g_settings.screensaver_dir, false, action_str);
		return res;
	}
	else if(strncmp(actionKey.c_str(), "fontsize.d", 10) == 0) {
		for (unsigned int i = 0; i < FONT_GROUP_COUNT; i++) {
			if (actionKey == font_sizes_groups[i].actionkey) {
				for (unsigned int j = 0; j < font_sizes_groups[i].count; j++) {
					SNeutrinoSettings::FONT_TYPES k = font_sizes_groups[i].content[j];
					CNeutrinoApp::getInstance()->getConfigFile()->setInt32(locale_real_names[neutrino_font[k].name], neutrino_font[k].defaultsize);
				}
				break;
			}
		}
		fontsizenotifier->changeNotify(NONEXISTANT_LOCALE, NULL);
		return res;
	}

	res = showOsdSetup();

	//return menu_return::RETURN_REPAINT;
	return res;
}

#define OSD_PRESET_OPTIONS_COUNT 2
const CMenuOptionChooser::keyval_ext OSD_PRESET_OPTIONS[] =
{
	{ COsdSetup::PRESET_CRT, NONEXISTANT_LOCALE, "CRT" },
	{ COsdSetup::PRESET_LCD, NONEXISTANT_LOCALE, "LCD" }
};

#define INFOBAR_CASYSTEM_MODE_OPTION_COUNT 4
const CMenuOptionChooser::keyval INFOBAR_CASYSTEM_MODE_OPTIONS[INFOBAR_CASYSTEM_MODE_OPTION_COUNT] =
{
	{ 0, LOCALE_OPTIONS_ON },
	{ 1, LOCALE_MISCSETTINGS_INFOBAR_CASYSTEM_MODE },
	{ 2, LOCALE_MISCSETTINGS_INFOBAR_CASYSTEM_MINI },
	{ 3, LOCALE_OPTIONS_OFF  },
};
#if 0 //not used
#define SHOW_INFOMENU_MODE_OPTION_COUNT 2
const CMenuOptionChooser::keyval SHOW_INFOMENU_MODE_OPTIONS[SHOW_INFOMENU_MODE_OPTION_COUNT] =
{
	{ 0, LOCALE_MAINMENU_HEAD },
	{ 1, LOCALE_MAINMENU_SERVICE },
};
#endif
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

#define VOLUMEBAR_DISP_POS_OPTIONS_COUNT 7
const CMenuOptionChooser::keyval  VOLUMEBAR_DISP_POS_OPTIONS[VOLUMEBAR_DISP_POS_OPTIONS_COUNT]=
{
	{ CVolumeBar::VOLUMEBAR_POS_TOP_RIGHT    , LOCALE_SETTINGS_POS_TOP_RIGHT },
	{ CVolumeBar::VOLUMEBAR_POS_TOP_LEFT     , LOCALE_SETTINGS_POS_TOP_LEFT },
	{ CVolumeBar::VOLUMEBAR_POS_BOTTOM_LEFT  , LOCALE_SETTINGS_POS_BOTTOM_LEFT },
	{ CVolumeBar::VOLUMEBAR_POS_BOTTOM_RIGHT , LOCALE_SETTINGS_POS_BOTTOM_RIGHT },
	{ CVolumeBar::VOLUMEBAR_POS_TOP_CENTER   , LOCALE_SETTINGS_POS_TOP_CENTER },
	{ CVolumeBar::VOLUMEBAR_POS_BOTTOM_CENTER, LOCALE_SETTINGS_POS_BOTTOM_CENTER },
	{ CVolumeBar::VOLUMEBAR_POS_HIGHER_CENTER, LOCALE_SETTINGS_POS_HIGHER_CENTER }
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
#if 0 //not used
#define CHANNELLIST_EXTENDED_OPTIONS_COUNT 3
const CMenuOptionChooser::keyval CHANNELLIST_EXTENDED_OPTIONS[CHANNELLIST_EXTENDED_OPTIONS_COUNT]=
{
	{ 0, LOCALE_OPTIONS_OFF },			//none
	{ 1, LOCALE_CHANNELLIST_EXTENDED_SIMPLE },	//unicolor
	{ 2, LOCALE_CHANNELLIST_EXTENDED_COLORED }	//colored
};
#endif
#define OPTIONS_COLORED_EVENTS_OPTION_COUNT 3
const CMenuOptionChooser::keyval OPTIONS_COLORED_EVENTS_OPTIONS[OPTIONS_COLORED_EVENTS_OPTION_COUNT] =
{
	{ 0, LOCALE_MISCSETTINGS_COLORED_EVENTS_0 },	//none
	{ 1, LOCALE_MISCSETTINGS_COLORED_EVENTS_1 },	//current
	{ 2, LOCALE_MISCSETTINGS_COLORED_EVENTS_2 },	//next
};

#define OPTIONS_COL_GRADIENT_OPTIONS_COUNT CC_COLGRAD_TYPES 	//TODO: add modes for intensity
const CMenuOptionChooser::keyval OPTIONS_COL_GRADIENT_OPTIONS[OPTIONS_COL_GRADIENT_OPTIONS_COUNT] =
{
	{ CC_COLGRAD_OFF			, LOCALE_OPTIONS_OFF },
	{ CC_COLGRAD_COL_A_2_COL_B		, LOCALE_COLOR_GRADIENT_A2B  }, //color A to color B
	{ CC_COLGRAD_COL_B_2_COL_A		, LOCALE_COLOR_GRADIENT_B2A  }, //color B to color A
	{ CC_COLGRAD_LIGHT_2_DARK		, LOCALE_COLOR_GRADIENT_L2D  }, //light to dark
	{ CC_COLGRAD_DARK_2_LIGHT		, LOCALE_COLOR_GRADIENT_D2L  }, //dark to light
	{ CC_COLGRAD_COL_LIGHT_DARK_LIGHT	, LOCALE_COLOR_GRADIENT_LDL  }, //light dark light
	{ CC_COLGRAD_COL_DARK_LIGHT_DARK	, LOCALE_COLOR_GRADIENT_DLD  }, //dark light dark
};

#define OPTIONS_COL_GRADIENT_DIRECTION_OPTIONS_COUNT 2
const CMenuOptionChooser::keyval OPTIONS_COL_GRADIENT_DIRECTION_OPTIONS[OPTIONS_COL_GRADIENT_DIRECTION_OPTIONS_COUNT] =
{
	{ CFrameBuffer::gradientHorizontal	, LOCALE_COLOR_GRADIENT_MODE_DIRECTION_HOR  }, 	//horizontal
	{ CFrameBuffer::gradientVertical	, LOCALE_COLOR_GRADIENT_MODE_DIRECTION_VER }, 	//vertical
};

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

//show osd setup
int COsdSetup::showOsdSetup()
{
	int shortcut = 1;

	//osd main menu
	osd_menu = new CMenuWidget(LOCALE_MAINMENU_SETTINGS, NEUTRINO_ICON_COLORS, width, MN_WIDGET_ID_OSDSETUP);
	osd_menu->setWizardMode(is_wizard);

	//intro with subhead and back button
	osd_menu->addIntroItems(LOCALE_MAINSETTINGS_OSD);

	//item menu colors
	if (osd_menu_colors)
		delete osd_menu_colors;
	osd_menu_colors = new CMenuWidget(LOCALE_MAINMENU_SETTINGS, NEUTRINO_ICON_COLORS, width, MN_WIDGET_ID_OSDSETUP_MENUCOLORS);
	showOsdMenueColorSetup(osd_menu_colors);

	CMenuForwarder * mf = new CMenuForwarder(LOCALE_COLORMENU_MENUCOLORS, true, NULL, osd_menu_colors, NULL, CRCInput::RC_red);
	mf->setHint("", LOCALE_MENU_HINT_COLORS);
	osd_menu->addItem(mf);

	//fonts
	CMenuWidget osd_menu_fonts(LOCALE_MAINMENU_SETTINGS, NEUTRINO_ICON_COLORS, 50, MN_WIDGET_ID_OSDSETUP_FONT);
	showOsdFontSizeSetup(&osd_menu_fonts);
	mf = new CMenuForwarder(LOCALE_FONTMENU_HEAD, true, NULL, &osd_menu_fonts, NULL, CRCInput::RC_green);
	mf->setHint("", LOCALE_MENU_HINT_FONTS);
	osd_menu->addItem(mf);

	//timeouts
	CMenuWidget osd_menu_timing(LOCALE_MAINMENU_SETTINGS, NEUTRINO_ICON_SETTINGS, width, MN_WIDGET_ID_OSDSETUP_TIMEOUT);
	showOsdTimeoutSetup(&osd_menu_timing);
	mf = new CMenuForwarder(LOCALE_COLORMENU_TIMING, true, NULL, &osd_menu_timing, NULL, CRCInput::RC_yellow);
	mf->setHint("", LOCALE_MENU_HINT_TIMEOUTS);
	osd_menu->addItem(mf);

	//screen
	CScreenSetup screensetup;
	mf = new CMenuForwarder(LOCALE_VIDEOMENU_SCREENSETUP, true, NULL, &screensetup, NULL, CRCInput::RC_blue);
	mf->setHint("", LOCALE_MENU_HINT_SCREEN_SETUP);
	osd_menu->addItem(mf);

	//menus
	CMenuWidget osd_menu_menus(LOCALE_MAINMENU_SETTINGS, NEUTRINO_ICON_SETTINGS, width, MN_WIDGET_ID_OSDSETUP_MENUS);
	showOsdMenusSetup(&osd_menu_menus);
	mf = new CMenuForwarder(LOCALE_SETTINGS_MENUS, true, NULL, &osd_menu_menus, NULL, CRCInput::convertDigitToKey(shortcut++));
	mf->setHint("", LOCALE_MENU_HINT_MENUS);
	osd_menu->addItem(mf);

	//progressbar
	mf = new CMenuDForwarder(LOCALE_MISCSETTINGS_PROGRESSBAR, true, NULL, new CProgressbarSetup(), NULL, CRCInput::convertDigitToKey(shortcut++));
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

	//info clock
	CMenuWidget osd_menu_infoclock(LOCALE_MAINMENU_SETTINGS, NEUTRINO_ICON_SETTINGS, width, MN_WIDGET_ID_OSDSETUP_INFOCLOCK);
	showOsdInfoclockSetup(&osd_menu_infoclock);
	mf = new CMenuForwarder(LOCALE_MISCSETTINGS_INFOCLOCK, true, NULL, &osd_menu_infoclock, NULL, CRCInput::convertDigitToKey(shortcut++));
	mf->setHint("", LOCALE_MENU_HINT_INFOCLOCK_SETUP);
	osd_menu->addItem(mf);

	//screenshot
	CMenuWidget osd_menu_screenshot(LOCALE_MAINMENU_SETTINGS, NEUTRINO_ICON_SETTINGS, width, MN_WIDGET_ID_OSDSETUP_SCREENSHOT);
	showOsdScreenShotSetup(&osd_menu_screenshot);
	mf = new CMenuForwarder(LOCALE_SCREENSHOT_MENU, true, NULL, &osd_menu_screenshot, NULL, CRCInput::convertDigitToKey(shortcut++));
	mf->setHint("", LOCALE_MENU_HINT_SCREENSHOT_SETUP);
	osd_menu->addItem(mf);

	//screensaver
	CMenuWidget osd_menu_screensaver(LOCALE_MAINMENU_SETTINGS, NEUTRINO_ICON_SETTINGS, width, MN_WIDGET_ID_OSDSETUP_SCREENSAVER);
	showOsdScreensaverSetup(&osd_menu_screensaver);
	mf = new CMenuForwarder(LOCALE_SCREENSAVER_MENU, true, NULL, &osd_menu_screensaver, NULL, CRCInput::convertDigitToKey(shortcut++));
	mf->setHint("", LOCALE_MENU_HINT_SCREENSAVER_SETUP);
	osd_menu->addItem(mf);

	osd_menu->addItem(GenericMenuSeparatorLine);

#ifdef ENABLE_CHANGE_OSD_RESOLUTION
	// osd resolution
	size_t resCount = frameBuffer->osd_resolutions.size();
	struct CMenuOptionChooser::keyval_ext kext[resCount];
	char valname[resCount][255];
	if (resCount > 0) {
		for (size_t i = 0; i < resCount; i++) {
			kext[i].key = i;
			kext[i].value = NONEXISTANT_LOCALE;
			snprintf(valname[i], sizeof(valname[resCount]), "%dx%d", frameBuffer->osd_resolutions[i].xRes, frameBuffer->osd_resolutions[i].yRes);
			kext[i].valname = valname[i];
		}
	}
	else {
		kext[0].key = 0;
		kext[0].value = NONEXISTANT_LOCALE;
		kext[0].valname = "-";
		resCount = 1;
	}
	int videoSystem = COsdHelpers::getInstance()->getVideoSystem();
	bool enable = ((resCount > 1) &&
			COsdHelpers::getInstance()->isVideoSystem1080(videoSystem) &&
			(g_settings.video_Mode != VIDEO_STD_AUTO));
	CMenuOptionChooser * osd_res = new CMenuOptionChooser(LOCALE_COLORMENU_OSD_RESOLUTION, &g_settings.osd_resolution, kext, resCount, enable, this);
	osd_res->setHint("", LOCALE_MENU_HINT_OSD_RESOLUTION);
	osd_menu->addItem(osd_res);
#endif

	//monitor
	CMenuOptionChooser * mc = new CMenuOptionChooser(LOCALE_COLORMENU_OSD_PRESET, &g_settings.screen_preset, OSD_PRESET_OPTIONS, OSD_PRESET_OPTIONS_COUNT, true, this);
	mc->setHint("", LOCALE_MENU_HINT_OSD_PRESET);
	osd_menu->addItem(mc);

	osd_menu->addItem(GenericMenuSeparatorLine);

	// round corners
	mc = new CMenuOptionChooser(LOCALE_EXTRA_ROUNDED_CORNERS, &g_settings.rounded_corners, MENU_CORNERSETTINGS_TYPE_OPTIONS, MENU_CORNERSETTINGS_TYPE_OPTION_COUNT, true, this);
	mc->setHint("", LOCALE_MENU_HINT_ROUNDED_CORNERS);
	osd_menu->addItem(mc);

	// fade windows
	mc = new CMenuOptionChooser(LOCALE_COLORMENU_FADE, &g_settings.widget_fade, OPTIONS_OFF0_ON1_OPTIONS, OPTIONS_OFF0_ON1_OPTION_COUNT, true );
	mc->setHint("", LOCALE_MENU_HINT_FADE);
	osd_menu->addItem(mc);

	// window size
	memset(window_size_value, 0, sizeof(window_size_value));
	snprintf(window_size_value, sizeof(window_size_value), "%d / %d", g_settings.window_width, g_settings.window_height);
	mfWindowSize = new CMenuForwarder(LOCALE_WINDOW_SIZE, true, window_size_value, this, "window_size", CRCInput::convertDigitToKey(shortcut++));
	mfWindowSize->setHint("", LOCALE_MENU_HINT_WINDOW_SIZE);
	osd_menu->addItem(mfWindowSize);

	osd_menu->addItem(GenericMenuSeparatorLine);

	// scrambled
	mc = new CMenuOptionChooser(LOCALE_EXTRA_SCRAMBLED_MESSAGE, &g_settings.scrambled_message, OPTIONS_OFF0_ON1_OPTIONS, OPTIONS_OFF0_ON1_OPTION_COUNT, true);
	mc->setHint("", LOCALE_MENU_HINT_SCRAMBLED_MESSAGE);
	osd_menu->addItem(mc);

	// subchannel menu position
	mc = new CMenuOptionChooser(LOCALE_INFOVIEWER_SUBCHAN_DISP_POS, &g_settings.infobar_subchan_disp_pos, INFOBAR_SUBCHAN_DISP_POS_OPTIONS, INFOBAR_SUBCHAN_DISP_POS_OPTIONS_COUNT, true);
	mc->setHint("", LOCALE_MENU_HINT_SUBCHANNEL_POS);
	osd_menu->addItem(mc);

	int oldVolumeSize = g_settings.volume_size;
	int oldInfoClockSize = g_settings.infoClockFontSize;

	int res = osd_menu->exec(NULL, "");

	if (oldVolumeSize != g_settings.volume_size)
		CVolumeHelper::getInstance()->refresh();

	if (oldInfoClockSize != g_settings.infoClockFontSize) {
		CInfoClock::getInstance()->setHeight(g_settings.infoClockFontSize);
		CVolumeHelper::getInstance()->refresh();
		if (CNeutrinoApp::getInstance()->isMuted())
			CAudioMute::getInstance()->enableMuteIcon(true);
	}

	delete colorInfoclockNotifier;
	delete screensaverNotifier;
	delete channellistNotifier;
	delete infobarHddNotifier;
	delete osd_menu;
	return res;
}

//menue colors
void COsdSetup::showOsdMenueColorSetup(CMenuWidget *menu_colors)
{
	menu_colors->addIntroItems(LOCALE_COLORMENU_MENUCOLORS);

	CMenuForwarder * mf = new CMenuDForwarder(LOCALE_COLORMENU_THEMESELECT, true, NULL, new CThemes(), NULL, CRCInput::RC_red);
	mf->setHint("", LOCALE_MENU_HINT_THEME);
	menu_colors->addItem(mf);

	SNeutrinoTheme &t = g_settings.theme;
	sigc::slot0<void> slot_repaint = sigc::mem_fun(menu_colors, &CMenuWidget::paint); //we want to repaint after changed Option

	CColorChooser* chHeadcolor = new CColorChooser(LOCALE_COLORMENU_BACKGROUND, &t.menu_Head_red, &t.menu_Head_green, &t.menu_Head_blue,
			&t.menu_Head_alpha, colorSetupNotifier);
	chHeadcolor->setGradient(CColorChooser::gradient_head_body);
	CColorChooser* chHeadTextcolor = new CColorChooser(LOCALE_COLORMENU_TEXTCOLOR, &t.menu_Head_Text_red, &t.menu_Head_Text_green, &t.menu_Head_Text_blue,
			NULL, colorSetupNotifier);
	chHeadTextcolor->setGradient(CColorChooser::gradient_head_text);
	CColorChooser* chContentcolor = new CColorChooser(LOCALE_COLORMENU_BACKGROUND, &t.menu_Content_red, &t.menu_Content_green, &t.menu_Content_blue,
			&t.menu_Content_alpha, colorSetupNotifier);
	CColorChooser* chContentTextcolor = new CColorChooser(LOCALE_COLORMENU_TEXTCOLOR, &t.menu_Content_Text_red, &t.menu_Content_Text_green, &t.menu_Content_Text_blue,
			NULL, colorSetupNotifier);
	CColorChooser* chContentSelectedcolor = new CColorChooser(LOCALE_COLORMENU_BACKGROUND, &t.menu_Content_Selected_red, &t.menu_Content_Selected_green, &t.menu_Content_Selected_blue,
			&t.menu_Content_Selected_alpha, colorSetupNotifier);
	CColorChooser* chContentSelectedTextcolor = new CColorChooser(LOCALE_COLORMENU_TEXTCOLOR, &t.menu_Content_Selected_Text_red, &t.menu_Content_Selected_Text_green, &t.menu_Content_Selected_Text_blue,
			NULL, colorSetupNotifier);
	CColorChooser* chContentInactivecolor = new CColorChooser(LOCALE_COLORMENU_BACKGROUND, &t.menu_Content_inactive_red, &t.menu_Content_inactive_green, &t.menu_Content_inactive_blue,
			&t.menu_Content_inactive_alpha, colorSetupNotifier);
	CColorChooser* chContentInactiveTextcolor = new CColorChooser(LOCALE_COLORMENU_TEXTCOLOR, &t.menu_Content_inactive_Text_red, &t.menu_Content_inactive_Text_green, &t.menu_Content_inactive_Text_blue,
			NULL, colorSetupNotifier);
	CColorChooser* chFootcolor = new CColorChooser(LOCALE_COLORMENU_BACKGROUND, &t.menu_Foot_red, &t.menu_Foot_green, &t.menu_Foot_blue,
			&t.menu_Foot_alpha, colorSetupNotifier);
	CColorChooser* chFootTextcolor = new CColorChooser(LOCALE_COLORMENU_TEXTCOLOR, &t.menu_Foot_Text_red, &t.menu_Foot_Text_green, &t.menu_Foot_Text_blue,
			NULL, colorSetupNotifier);

	menu_colors->addItem( new CMenuSeparator(CMenuSeparator::LINE | CMenuSeparator::STRING, LOCALE_COLORMENUSETUP_MENUHEAD));

	CMenuOptionChooser *oj;
#if 0
	oj = new CMenuOptionChooser(LOCALE_COLOR_GRADIENT_C2C, &g_settings.theme.gradient_c2c, OPTIONS_OFF0_ON1_OPTIONS, OPTIONS_OFF0_ON1_OPTION_COUNT, true );
	oj->OnAfterChangeOption.connect(slot_repaint);
	oj->setHint("", LOCALE_MENU_HINT_COLOR_GRADIENT_C2C);
	menu_colors->addItem(oj);
	menu_colors->addItem( new CMenuSeparator(CMenuSeparator::LINE));
#endif

	mf = new CMenuDForwarder(LOCALE_COLORMENU_BACKGROUND, true, NULL, chHeadcolor );
	mf->setHint("", LOCALE_MENU_HINT_HEAD_BACK);
	menu_colors->addItem(mf);

	mf = new CMenuDForwarder(LOCALE_COLORMENU_TEXTCOLOR, true, NULL, chHeadTextcolor );
	mf->setHint("", LOCALE_MENU_HINT_HEAD_TEXTCOLOR);
	menu_colors->addItem(mf);

	// head color gradient //TODO: disable sub options if head gradient is disabled
	oj = new CMenuOptionChooser(LOCALE_COLOR_GRADIENT, &g_settings.theme.menu_Head_gradient, OPTIONS_COL_GRADIENT_OPTIONS, OPTIONS_COL_GRADIENT_OPTIONS_COUNT, true );
	oj->OnAfterChangeOption.connect(slot_repaint);
	oj->setHint("", LOCALE_MENU_HINT_COLOR_GRADIENT);
	menu_colors->addItem(oj);

	// head color gradient direction
	oj = new CMenuOptionChooser(LOCALE_COLOR_GRADIENT_MODE_DIRECTION, &g_settings.theme.menu_Head_gradient_direction, OPTIONS_COL_GRADIENT_DIRECTION_OPTIONS, OPTIONS_COL_GRADIENT_DIRECTION_OPTIONS_COUNT, true );
	oj->OnAfterChangeOption.connect(slot_repaint);
	oj->setHint("", LOCALE_MENU_HINT_COLOR_GRADIENT_DIRECTION);
	menu_colors->addItem(oj);

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

	// footer
	menu_colors->addItem( new CMenuSeparator(CMenuSeparator::LINE | CMenuSeparator::STRING, LOCALE_COLORMENUSETUP_MENUFOOT));
	mf = new CMenuDForwarder(LOCALE_COLORMENU_BACKGROUND, true, NULL, chFootcolor );
	mf->setHint("", LOCALE_MENU_HINT_FOOT_BACK);
	menu_colors->addItem(mf);

	// footer text
	mf = new CMenuDForwarder(LOCALE_COLORMENU_TEXTCOLOR, true, NULL, chFootTextcolor );
	mf->setHint("", LOCALE_MENU_HINT_FOOT_TEXTCOLOR);
	menu_colors->addItem(mf);

	// hintbox color gradient
	menu_colors->addItem( new CMenuSeparator(CMenuSeparator::LINE | CMenuSeparator::STRING, LOCALE_COLORTHEMEMENU_MENU_HINTS));
	oj = new CMenuOptionChooser(LOCALE_COLOR_GRADIENT, &t.menu_Hint_gradient, OPTIONS_COL_GRADIENT_OPTIONS, OPTIONS_COL_GRADIENT_OPTIONS_COUNT, true);
	oj->OnAfterChangeOption.connect(slot_repaint);
	oj->setHint("", LOCALE_MENU_HINT_COLOR_GRADIENT);
	menu_colors->addItem(oj);

	// hintbox color gradient direction
	oj = new CMenuOptionChooser(LOCALE_COLOR_GRADIENT_MODE_DIRECTION, &t.menu_Hint_gradient_direction, OPTIONS_COL_GRADIENT_DIRECTION_OPTIONS, OPTIONS_COL_GRADIENT_DIRECTION_OPTIONS_COUNT, true );
	oj->OnAfterChangeOption.connect(slot_repaint);
	oj->setHint("", LOCALE_MENU_HINT_COLOR_GRADIENT_DIRECTION);
	menu_colors->addItem(oj);

	// menue separator line gradient enable
	menu_colors->addItem( new CMenuSeparator(CMenuSeparator::LINE));
	oj = new CMenuOptionChooser(LOCALE_COLOR_GRADIENT_SEPARATOR_ENABLE, &t.menu_Separator_gradient_enable, OPTIONS_OFF0_ON1_OPTIONS, OPTIONS_OFF0_ON1_OPTION_COUNT, true );
	oj->OnAfterChangeOption.connect(slot_repaint);
	oj->setHint("", LOCALE_MENU_HINT_COLOR_GRADIENT_SEPARATOR_ENABLE);
	menu_colors->addItem(oj);

	// infoviewer color
	CColorChooser* chInfobarcolor = new CColorChooser(LOCALE_COLORMENU_BACKGROUND, &t.infobar_red,
			&t.infobar_green, &t.infobar_blue, &t.infobar_alpha, colorSetupNotifier);
	CColorChooser* chInfobarTextcolor = new CColorChooser(LOCALE_COLORMENU_TEXTCOLOR, &t.infobar_Text_red,
			&t.infobar_Text_green, &t.infobar_Text_blue, NULL, colorSetupNotifier);
	CColorChooser* chInfobarCASystem = new CColorChooser(LOCALE_COLORMENU_BACKGROUND, &t.infobar_casystem_red,
			&t.infobar_casystem_green, &t.infobar_casystem_blue, &t.infobar_casystem_alpha, colorSetupNotifier);

	menu_colors->addItem( new CMenuSeparator(CMenuSeparator::LINE | CMenuSeparator::STRING, LOCALE_COLORSTATUSBAR_TEXT));
	mf = new CMenuDForwarder(LOCALE_COLORMENU_BACKGROUND, true, NULL, chInfobarcolor );
	mf->setHint("", LOCALE_MENU_HINT_INFOBAR_BACK);
	menu_colors->addItem(mf);

	mf = new CMenuDForwarder(LOCALE_COLORMENU_TEXTCOLOR, true, NULL, chInfobarTextcolor );
	mf->setHint("", LOCALE_MENU_HINT_INFOBAR_TEXTCOLOR);
	menu_colors->addItem(mf);

	// infoviewer gradient top
	menu_colors->addItem( new CMenuSeparator(CMenuSeparator::LINE));
	oj = new CMenuOptionChooser(LOCALE_MISCSETTINGS_INFOBAR_GRADIENT_TOP, &t.infobar_gradient_top, OPTIONS_COL_GRADIENT_OPTIONS, OPTIONS_COL_GRADIENT_OPTIONS_COUNT, true);
	oj->setHint("", LOCALE_MENU_HINT_COLOR_GRADIENT);
	menu_colors->addItem(oj);

	// infoviewer gradient top direction
	oj = new CMenuOptionChooser(LOCALE_COLOR_GRADIENT_MODE_DIRECTION, &t.infobar_gradient_top_direction, OPTIONS_COL_GRADIENT_DIRECTION_OPTIONS, OPTIONS_COL_GRADIENT_DIRECTION_OPTIONS_COUNT, true );
	oj->setHint("", LOCALE_MENU_HINT_COLOR_GRADIENT_DIRECTION);
	menu_colors->addItem(oj);

	// infoviewer gradient body
	menu_colors->addItem( new CMenuSeparator(CMenuSeparator::LINE));
	oj = new CMenuOptionChooser(LOCALE_MISCSETTINGS_INFOBAR_GRADIENT_BODY, &t.infobar_gradient_body, OPTIONS_COL_GRADIENT_OPTIONS, OPTIONS_COL_GRADIENT_OPTIONS_COUNT, true);
	oj->setHint("", LOCALE_MENU_HINT_COLOR_GRADIENT);
	menu_colors->addItem(oj);

	// infoviewer gradient body direction
	oj = new CMenuOptionChooser(LOCALE_COLOR_GRADIENT_MODE_DIRECTION, &t.infobar_gradient_body_direction, OPTIONS_COL_GRADIENT_DIRECTION_OPTIONS, OPTIONS_COL_GRADIENT_DIRECTION_OPTIONS_COUNT, true );
	oj->setHint("", LOCALE_MENU_HINT_COLOR_GRADIENT_DIRECTION);
	menu_colors->addItem(oj);

	// infoviewer gradient bottom
	menu_colors->addItem( new CMenuSeparator(CMenuSeparator::LINE));
	oj = new CMenuOptionChooser(LOCALE_MISCSETTINGS_INFOBAR_GRADIENT_BOTTOM, &t.infobar_gradient_bottom, OPTIONS_COL_GRADIENT_OPTIONS, OPTIONS_COL_GRADIENT_OPTIONS_COUNT, true);
	oj->setHint("", LOCALE_MENU_HINT_COLOR_GRADIENT);
	menu_colors->addItem(oj);

	// infoviewer gradient bottom direction
	oj = new CMenuOptionChooser(LOCALE_COLOR_GRADIENT_MODE_DIRECTION, &t.infobar_gradient_bottom_direction, OPTIONS_COL_GRADIENT_DIRECTION_OPTIONS, OPTIONS_COL_GRADIENT_DIRECTION_OPTIONS_COUNT, true );
	oj->setHint("", LOCALE_MENU_HINT_COLOR_GRADIENT_DIRECTION);
	menu_colors->addItem(oj);

	// ca bar
	menu_colors->addItem( new CMenuSeparator(CMenuSeparator::LINE));
	mf = new CMenuDForwarder(LOCALE_MISCSETTINGS_INFOBAR_CASYSTEM_DISPLAY, g_settings.infobar_casystem_display < 2, NULL, chInfobarCASystem );
	mf->setHint("", LOCALE_MENU_HINT_INFOBAR_CASYS_COLOR);
	menu_colors->addItem(mf);

	// colored events
	CColorChooser* chColored_Events = new CColorChooser(LOCALE_COLORMENU_TEXTCOLOR,	&t.colored_events_red,
			&t.colored_events_green, &t.colored_events_blue, NULL, colorSetupNotifier);

	menu_colors->addItem( new CMenuSeparator(CMenuSeparator::LINE | CMenuSeparator::STRING, LOCALE_MISCSETTINGS_COLORED_EVENTS));

	mf = new CMenuDForwarder(LOCALE_COLORMENU_TEXTCOLOR, true, NULL, chColored_Events );
	mf->setHint("", LOCALE_MENU_HINT_COLORED_EVENTS_TEXTCOLOR);
	menu_colors->addItem(mf);

	// colored events channellist
	oj = new CMenuOptionChooser(LOCALE_MISCSETTINGS_COLORED_EVENTS_CHANNELLIST, &t.colored_events_channellist, OPTIONS_COLORED_EVENTS_OPTIONS, OPTIONS_COLORED_EVENTS_OPTION_COUNT, true);
	oj->setHint("", LOCALE_MENU_HINT_COLORED_EVENTS);
	menu_colors->addItem(oj);

	// colored events infobar
	oj = new CMenuOptionChooser(LOCALE_MISCSETTINGS_COLORED_EVENTS_INFOBAR, &t.colored_events_infobar, OPTIONS_COLORED_EVENTS_OPTIONS, OPTIONS_COLORED_EVENTS_OPTION_COUNT, true);
	oj->setHint("", LOCALE_MENU_HINT_COLORED_EVENTS);
	menu_colors->addItem(oj);
}

/* for font size setup */
class CMenuNumberInput : public CMenuForwarder, CMenuTarget, CChangeObserver
{
private:
	CChangeObserver * observer;
	CConfigFile     * configfile;
	int32_t           defaultvalue;
	std::string	value;

protected:

	std::string getOption(fb_pixel_t * bgcol __attribute__((unused)) = NULL) {
		return to_string(configfile->getInt32(locale_real_names[name], defaultvalue));
	}

	virtual bool changeNotify(const neutrino_locale_t OptionName, void * Data)
	{
		configfile->setInt32(locale_real_names[name], atoi(value.c_str()));
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
		value = getOption();
		while (value.length() < 3)
			value = " " + value;
		CStringInput input(name, &value, 3, LOCALE_IPSETUP_HINT_1, LOCALE_IPSETUP_HINT_2, "0123456789 ", this);
		input.forceSaveScreen(true);
		return input.exec(parent, action_Key);
	}

	std::string &getValue(void) {
		value = getOption();
		return value;
	}
};

void COsdSetup::AddFontSettingItem(CMenuWidget &font_Settings, const SNeutrinoSettings::FONT_TYPES number_of_fontsize_entry)
{
	CMenuNumberInput *ni = new CMenuNumberInput(neutrino_font[number_of_fontsize_entry].name, neutrino_font[number_of_fontsize_entry].defaultsize, fontsizenotifier, CNeutrinoApp::getInstance()->getConfigFile());
	font_Settings.addItem(ni);
}

//font settings menu
void COsdSetup::showOsdFontSizeSetup(CMenuWidget *menu_fonts)
{
	CMenuWidget *fontSettings = menu_fonts;
	CMenuForwarder * mf;

	fontSettings->addIntroItems(LOCALE_FONTMENU_HEAD);

	// select gui font file
	osdFontFile = g_settings.font_file;
	osdFontFile = "(" + getBaseName(osdFontFile) + ")";
	mfFontFile = new CMenuForwarder(LOCALE_COLORMENU_FONT, true, osdFontFile.c_str(), this, "select_font", CRCInput::RC_red);
	mfFontFile->setHint("", LOCALE_MENU_HINT_FONT_GUI);
	fontSettings->addItem(mfFontFile);

	// select teletext font file
	osdTtxFontFile = g_settings.ttx_font_file;
	osdTtxFontFile = "(" + getBaseName(osdTtxFontFile) + ")";
	mfTtxFontFile = new CMenuForwarder(LOCALE_COLORMENU_FONT_TTX, true, osdTtxFontFile.c_str(), this, "ttx_font",  CRCInput::RC_green);
	mfTtxFontFile->setHint("", LOCALE_MENU_HINT_FONT_TTX);
	fontSettings->addItem(mfTtxFontFile);

	fontSettings->addItem(new CMenuSeparator(CMenuSeparator::LINE | CMenuSeparator::STRING, LOCALE_FONTMENU_SIZES));

	//submenu font scaling
	mf = new CMenuForwarder(LOCALE_FONTMENU_SCALING, true, NULL, this, "font_scaling",  CRCInput::RC_blue);
	mf->setHint("", LOCALE_MENU_HINT_FONT_SCALING);
	fontSettings->addItem(mf);

	//fontSettings->addItem( new CMenuForwarder(LOCALE_EPGPLUS_SELECT_FONT_NAME, true, NULL, this, "select_font"));

	mn_widget_id_t w_index = MN_WIDGET_ID_OSDSETUP_FONTSIZE_MENU;
	for (unsigned int i = 0; i < FONT_GROUP_COUNT; i++)
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
	g_InfoViewer->ResetModules();
}

//osd timeouts
void COsdSetup::showOsdTimeoutSetup(CMenuWidget* menu_timeout)
{
	menu_timeout->addIntroItems(LOCALE_COLORMENU_TIMING);

	std::string nf("%d ");
	nf += g_Locale->getText(LOCALE_UNIT_SHORT_SECOND);
	for (int i = 0; i < SNeutrinoSettings::TIMING_SETTING_COUNT; i++)
	{
		CMenuOptionNumberChooser *ch = new CMenuOptionNumberChooser(timing_setting[i].name, &g_settings.timing[i], true, 0, 240);
		ch->setNumberFormat(nf);
		ch->setHint("", timing_setting[i].hint);
		menu_timeout->addItem(ch);
	}

	menu_timeout->addItem(GenericMenuSeparatorLine);
	menu_timeout->addItem(new CMenuForwarder(LOCALE_OPTIONS_DEFAULT, true, NULL, this, "osd.def", CRCInput::RC_red));
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

#define PROGRESSBAR_INFOBAR_POSITION_COUNT 4
const CMenuOptionChooser::keyval PROGRESSBAR_INFOBAR_POSITION_OPTIONS[PROGRESSBAR_INFOBAR_POSITION_COUNT]=
{
	{ 0 , LOCALE_MISCSETTINGS_PROGRESSBAR_INFOBAR_POSITION_0 },
	{ 1 , LOCALE_MISCSETTINGS_PROGRESSBAR_INFOBAR_POSITION_1 },
	{ 2 , LOCALE_MISCSETTINGS_PROGRESSBAR_INFOBAR_POSITION_2 },
	{ 3 , LOCALE_MISCSETTINGS_PROGRESSBAR_INFOBAR_POSITION_3 }
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

#define HDD_STATFS_OPTION_COUNT 3
const CMenuOptionChooser::keyval HDD_STATFS_OPTIONS[HDD_STATFS_OPTION_COUNT] =
{
	{ SNeutrinoSettings::HDD_STATFS_OFF,            LOCALE_OPTIONS_OFF },
	{ SNeutrinoSettings::HDD_STATFS_ALWAYS,         LOCALE_HDD_STATFS_ALWAYS },
	{ SNeutrinoSettings::HDD_STATFS_RECORDING,      LOCALE_HDD_STATFS_RECORDING }
};

//infobar
void COsdSetup::showOsdInfobarSetup(CMenuWidget *menu_infobar)
{
	menu_infobar->addIntroItems(LOCALE_MISCSETTINGS_INFOBAR);

	infobarHddNotifier = new COnOffNotifier();
	sigc::slot0<void> slot_ibar = sigc::mem_fun(g_InfoViewer, &CInfoViewer::ResetModules);

	CMenuOptionChooser * mc;
	CMenuForwarder * mf;

	// show on epg change
	mc = new CMenuOptionChooser(LOCALE_MISCSETTINGS_INFOBAR_SHOW, &g_settings.infobar_show, OPTIONS_OFF0_ON1_OPTIONS, OPTIONS_OFF0_ON1_OPTION_COUNT, true);
	mc->setHint("", LOCALE_MENU_HINT_INFOBAR_ON_EPG);
	menu_infobar->addItem(mc);

	// radiotext
	mc = new CMenuOptionChooser(LOCALE_MISCSETTINGS_RADIOTEXT, &g_settings.radiotext_enable, OPTIONS_OFF0_ON1_OPTIONS, OPTIONS_OFF0_ON1_OPTION_COUNT, true, this);
	mc->setHint("", LOCALE_MENU_HINT_INFOBAR_RADIOTEXT);
	menu_infobar->addItem(mc);

	// buttons usertitle
	mc = new CMenuOptionChooser(LOCALE_MISCSETTINGS_INFOBAR_BUTTONS_USERTITLE, &g_settings.infobar_buttons_usertitle, OPTIONS_OFF0_ON1_OPTIONS, OPTIONS_OFF0_ON1_OPTION_COUNT, true, this);
	mc->setHint("", LOCALE_MENU_HINT_INFOBAR_BUTTONS_USERTITLE);
	menu_infobar->addItem(mc);

	menu_infobar->addItem(GenericMenuSeparator);

	// display options
	mc = new CMenuOptionChooser(LOCALE_MISCSETTINGS_INFOBAR_DISP, &g_settings.infobar_show_channellogo, LOCALE_MISCSETTINGS_INFOBAR_DISP_OPTIONS, LOCALE_MISCSETTINGS_INFOBAR_DISP_OPTIONS_COUNT, true);
	mc->OnAfterChangeOption.connect(slot_ibar);
	mc->setHint("", LOCALE_MENU_HINT_INFOBAR_LOGO);
	menu_infobar->addItem(mc);

	// logo directory
	mf = new CMenuForwarder(LOCALE_MISCSETTINGS_INFOBAR_LOGO_HDD_DIR, true, g_settings.logo_hdd_dir, this, "logo_dir");
	mf->setHint("", LOCALE_MENU_HINT_INFOBAR_LOGO_DIR);
	menu_infobar->addItem(mf);

	// satellite/cable provider
	mc = new CMenuOptionChooser(LOCALE_MISCSETTINGS_INFOBAR_SAT_DISPLAY, &g_settings.infobar_sat_display, OPTIONS_OFF0_ON1_OPTIONS, OPTIONS_OFF0_ON1_OPTION_COUNT, true);
	mc->OnAfterChangeOption.connect(slot_ibar);
	mc->setHint("", LOCALE_MENU_HINT_INFOBAR_SAT);
	menu_infobar->addItem(mc);

	menu_infobar->addItem(GenericMenuSeparator);

	// CA system
	casystemActivate.Clear(); //ensure empty activator object -> cleanup before add new items, prevents possible segfault!
	mc = new CMenuOptionChooser(LOCALE_MISCSETTINGS_INFOBAR_CASYSTEM_DISPLAY, &g_settings.infobar_casystem_display, INFOBAR_CASYSTEM_MODE_OPTIONS, INFOBAR_CASYSTEM_MODE_OPTION_COUNT, true, this);
	mc->OnAfterChangeOption.connect(slot_ibar);
	mc->setHint("", LOCALE_MENU_HINT_INFOBAR_CASYS);
	menu_infobar->addItem(mc);
#if 1
	// CA system dotmatrix
	mc = new CMenuOptionChooser(LOCALE_MISCSETTINGS_INFOBAR_CASYSTEM_DOTMATRIX, &g_settings.infobar_casystem_dotmatrix, OPTIONS_OFF0_ON1_OPTIONS, OPTIONS_OFF0_ON1_OPTION_COUNT, g_settings.infobar_casystem_display < 2);
	mc->setHint("", LOCALE_MENU_HINT_INFOBAR_CASYS_DOTMATRIX);
	menu_infobar->addItem(mc);
	casystemActivate.Add(mc);
#endif
	
	// CA system frame
	mc = new CMenuOptionChooser(LOCALE_MISCSETTINGS_INFOBAR_CASYSTEM_FRAME, &g_settings.infobar_casystem_frame, OPTIONS_OFF0_ON1_OPTIONS, OPTIONS_OFF0_ON1_OPTION_COUNT, g_settings.infobar_casystem_display < 2);
	mc->OnAfterChangeOption.connect(slot_ibar);
	mc->setHint("", LOCALE_MENU_HINT_INFOBAR_CASYS_FRAME);
	menu_infobar->addItem(mc);
	casystemActivate.Add(mc);

	menu_infobar->addItem(GenericMenuSeparator);

	// flash/hdd statfs
	mc = new CMenuOptionChooser(LOCALE_MISCSETTINGS_INFOBAR_SHOW_SYSFS_HDD, &g_settings.infobar_show_sysfs_hdd, OPTIONS_OFF0_ON1_OPTIONS, OPTIONS_OFF0_ON1_OPTION_COUNT, true, infobarHddNotifier);
	mc->setHint("", LOCALE_MENU_HINT_INFOBAR_FILESYS);
	menu_infobar->addItem(mc);

	// hdd statfs update
	mc = new CMenuOptionChooser(LOCALE_HDD_STATFS, &g_settings.hdd_statfs_mode, HDD_STATFS_OPTIONS, HDD_STATFS_OPTION_COUNT, g_settings.infobar_show_sysfs_hdd);
	mc->setHint("", LOCALE_MENU_HINT_HDD_STATFS);
	menu_infobar->addItem(mc);
	infobarHddNotifier->addItem(mc);

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
	mc->OnAfterChangeOption.connect(slot_ibar);
	mc->setHint("", LOCALE_MENU_HINT_INFOBAR_TUNER);
	menu_infobar->addItem(mc);

	// resolution
	mc = new CMenuOptionChooser(LOCALE_MISCSETTINGS_INFOBAR_SHOW_RES, &g_settings.infobar_show_res, INFOBAR_SHOW_RES_MODE_OPTIONS, INFOBAR_SHOW_RES_MODE_OPTION_COUNT, true);
	mc->setHint("", LOCALE_MENU_HINT_INFOBAR_RES);
	menu_infobar->addItem(mc);

	// DD icon
	mc = new CMenuOptionChooser(LOCALE_MISCSETTINGS_INFOBAR_SHOW_DD_AVAILABLE, &g_settings.infobar_show_dd_available, OPTIONS_OFF0_ON1_OPTIONS, OPTIONS_OFF0_ON1_OPTION_COUNT, true);
	mc->setHint("", LOCALE_MENU_HINT_INFOBAR_DD);
	menu_infobar->addItem(mc);

	menu_infobar->addItem(new CMenuSeparator(CMenuSeparator::LINE | CMenuSeparator::STRING, LOCALE_MISCSETTINGS_PROGRESSBAR));
	// progressbar position
	mc = new CMenuOptionChooser(LOCALE_MISCSETTINGS_PROGRESSBAR_INFOBAR_POSITION, &g_settings.infobar_progressbar, PROGRESSBAR_INFOBAR_POSITION_OPTIONS, PROGRESSBAR_INFOBAR_POSITION_COUNT, true);
	mc->OnAfterChangeOption.connect(slot_ibar);
	mc->setHint("", LOCALE_MENU_HINT_PROGRESSBAR_INFOBAR_POSITION);
	menu_infobar->addItem(mc);
}

//channellist
void COsdSetup::showOsdChanlistSetup(CMenuWidget *menu_chanlist)
{
	CMenuOptionChooser * mc;

	menu_chanlist->addIntroItems(LOCALE_MISCSETTINGS_CHANNELLIST);
	channellistNotifier = new COnOffNotifier();

	// channellist additional
	mc = new CMenuOptionChooser(LOCALE_CHANNELLIST_ADDITIONAL, &g_settings.channellist_additional, CHANNELLIST_ADDITIONAL_OPTIONS, CHANNELLIST_ADDITIONAL_OPTION_COUNT, true);
	mc->setHint("", LOCALE_MENU_HINT_CHANNELLIST_ADDITIONAL);
	menu_chanlist->addItem(mc);

	// epg align
	mc = new CMenuOptionChooser(LOCALE_MISCSETTINGS_CHANNELLIST_EPGTEXT_ALIGN, &g_settings.channellist_epgtext_align_right, CHANNELLIST_EPGTEXT_ALIGN_RIGHT_OPTIONS, CHANNELLIST_EPGTEXT_ALIGN_RIGHT_OPTIONS_COUNT, true);
	mc->setHint("", LOCALE_MENU_HINT_CHANNELLIST_EPG_ALIGN);
	menu_chanlist->addItem(mc);

	// extended channel list
	mc = new CMenuOptionChooser(LOCALE_CHANNELLIST_EXTENDED, &g_settings.theme.progressbar_design_channellist, PROGRESSBAR_COLOR_OPTIONS, PROGRESSBAR_COLOR_OPTION_COUNT, true, this);
	mc->setHint("", LOCALE_MENU_HINT_CHANNELLIST_EXTENDED);
	menu_chanlist->addItem(mc);

	// show infobox
	mc = new CMenuOptionChooser(LOCALE_CHANNELLIST_SHOW_INFOBOX, &g_settings.channellist_show_infobox, OPTIONS_OFF0_ON1_OPTIONS, OPTIONS_OFF0_ON1_OPTION_COUNT, true, channellistNotifier);
	mc->setHint("", LOCALE_MENU_HINT_CHANNELLIST_SHOW_INFOBOX);
	menu_chanlist->addItem(mc);

	// foot
	mc = new CMenuOptionChooser(LOCALE_CHANNELLIST_FOOT, &g_settings.channellist_foot, CHANNELLIST_FOOT_OPTIONS, CHANNELLIST_FOOT_OPTIONS_COUNT, g_settings.channellist_show_infobox);
	mc->setHint("", LOCALE_MENU_HINT_CHANNELLIST_FOOT);
	menu_chanlist->addItem(mc);
	channellistNotifier->addItem(mc);

	//show channel logo
	mc = new CMenuOptionChooser(LOCALE_CHANNELLIST_SHOW_CHANNELLOGO, &g_settings.channellist_show_channellogo, OPTIONS_OFF0_ON1_OPTIONS, OPTIONS_OFF0_ON1_OPTION_COUNT, true);
	mc->setHint("", LOCALE_MENU_HINT_CHANNELLIST_SHOW_CHANNELLOGO);
	menu_chanlist->addItem(mc);

	//show numbers
	mc = new CMenuOptionChooser(LOCALE_CHANNELLIST_SHOW_CHANNELNUMBER, &g_settings.channellist_show_numbers, OPTIONS_OFF0_ON1_OPTIONS, OPTIONS_OFF0_ON1_OPTION_COUNT, true);
	mc->setHint("", LOCALE_MENU_HINT_CHANNELLIST_SHOW_CHANNELNUMBER);
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

	// epgplus in eventlist
	mc = new CMenuOptionChooser(LOCALE_EVENTLIST_EPGPLUS, &g_settings.eventlist_epgplus, OPTIONS_OFF0_ON1_OPTIONS, OPTIONS_OFF0_ON1_OPTION_COUNT, true);
	mc->setHint("", LOCALE_MENU_HINT_EVENTLIST_EPGPLUS);
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

	// volume size
	int vMin = CVolumeHelper::getInstance()->getVolIconHeight();
	g_settings.volume_size = max(g_settings.volume_size, vMin);
	CMenuOptionNumberChooser * nc = new CMenuOptionNumberChooser(LOCALE_EXTRA_VOLUME_SIZE, &g_settings.volume_size, true, vMin, 50);
	nc->setHint("", LOCALE_MENU_HINT_VOLUME_SIZE);
	menu_volume->addItem(nc);

	// volume digits
	mc = new CMenuOptionChooser(LOCALE_EXTRA_VOLUME_DIGITS, &g_settings.volume_digits, OPTIONS_OFF0_ON1_OPTIONS, OPTIONS_OFF0_ON1_OPTION_COUNT, true, this);
	mc->setHint("", LOCALE_MENU_HINT_VOLUME_DIGITS);
	menu_volume->addItem(mc);

	// show mute at volume 0
	mc = new CMenuOptionChooser(LOCALE_EXTRA_SHOW_MUTE_ICON, &g_settings.show_mute_icon, OPTIONS_OFF0_ON1_OPTIONS, OPTIONS_OFF0_ON1_OPTION_COUNT, true);
	mc->setHint("", LOCALE_MENU_HINT_SHOW_MUTE_ICON);
	menu_volume->addItem(mc);
}

//info clock
void COsdSetup::showOsdInfoclockSetup(CMenuWidget *menu_infoclock)
{
	menu_infoclock->addIntroItems(LOCALE_MISCSETTINGS_INFOCLOCK);

	CMenuOptionChooser *mc = new CMenuOptionChooser(LOCALE_MISCSETTINGS_INFOCLOCK, &g_settings.mode_clock, OPTIONS_OFF0_ON1_OPTIONS, OPTIONS_OFF0_ON1_OPTION_COUNT, true, this, CRCInput::RC_red);
	mc->setHint("", LOCALE_MENU_HINT_CLOCK_MODE);
	menu_infoclock->addItem(mc);

	menu_infoclock->addItem(GenericMenuSeparatorLine);

	// size of info clock
	CMenuOptionNumberChooser* mn = new CMenuOptionNumberChooser(LOCALE_CLOCK_SIZE_HEIGHT, &g_settings.infoClockFontSize, true, 30, 120, this);
	mn->setHint("", LOCALE_MENU_HINT_CLOCK_SIZE);
	menu_infoclock->addItem(mn);

	// clock with seconds
	mc = new CMenuOptionChooser(LOCALE_CLOCK_SECONDS, &g_settings.infoClockSeconds, OPTIONS_OFF0_ON1_OPTIONS, OPTIONS_OFF0_ON1_OPTION_COUNT, true, this);
	mc->setHint("", LOCALE_MENU_HINT_CLOCK_SECONDS);
	menu_infoclock->addItem(mc);

	colorInfoclockNotifier = new COnOffNotifier(1);

	// clock with background
	mc = new CMenuOptionChooser(LOCALE_CLOCK_BACKGROUND, &g_settings.infoClockBackground, OPTIONS_OFF0_ON1_OPTIONS, OPTIONS_OFF0_ON1_OPTION_COUNT, true, colorInfoclockNotifier);
	mc->setHint("", LOCALE_MENU_HINT_CLOCK_BACKGROUND);
	menu_infoclock->addItem(mc);

	// digit color
	CColorChooser* cc = new CColorChooser(LOCALE_COLORMENU_CLOCK_TEXTCOLOR, &g_settings.theme.clock_Digit_red, &g_settings.theme.clock_Digit_green, &g_settings.theme.clock_Digit_blue,
			NULL, colorSetupNotifier);
	CMenuDForwarder* mf = new CMenuDForwarder(LOCALE_COLORMENU_CLOCK_TEXTCOLOR, !g_settings.infoClockBackground, NULL, cc);
	mf->setHint("", LOCALE_MENU_HINT_CLOCK_TEXTCOLOR);
	menu_infoclock->addItem(mf);
	colorInfoclockNotifier->addItem(mf);

	//ensure clock reinit after setup
	CInfoClock::getInstance()->clear();
}

bool COsdSetup::changeNotify(const neutrino_locale_t OptionName, void * data)
{
	if (ARE_LOCALES_EQUAL(OptionName, LOCALE_SETTINGS_MENU_POS)) {
		submenu_menus->hide();
		return true;
	}
	else if (ARE_LOCALES_EQUAL(OptionName, LOCALE_SETTINGS_MENU_HINTS)) {
		/* change option after hide, to let hide clear hint */
		submenu_menus->hide();
		g_settings.show_menu_hints = * (int*) data;
		return true;
	}
	else if((ARE_LOCALES_EQUAL(OptionName, LOCALE_MISCSETTINGS_INFOBAR_CASYSTEM_DISPLAY))
		||(ARE_LOCALES_EQUAL(OptionName, LOCALE_MISCSETTINGS_INFOBAR_SHOW_TUNER))) {
		if (g_InfoViewer == NULL)
			g_InfoViewer = new CInfoViewer;
		g_InfoViewer->changePB();
		if (ARE_LOCALES_EQUAL(OptionName, LOCALE_MISCSETTINGS_INFOBAR_CASYSTEM_DISPLAY)){
			casystemActivate.Activate(g_settings.infobar_casystem_display < 2);
			return false;
		}
		return false;
	}
	else if(ARE_LOCALES_EQUAL(OptionName, LOCALE_SCREENSAVER_DELAY)) {
		screensaverActivate.Activate(g_settings.screensaver_delay != 0);
		screensaverOptActivate.Activate(g_settings.screensaver_delay != 0 && g_settings.screensaver_mode == 0);
		return false;
	}
	else if(ARE_LOCALES_EQUAL(OptionName, LOCALE_SCREENSAVER_MODE)) {
		screensaverOptActivate.Activate(g_settings.screensaver_mode == 0);
		return false;
	}
	else if(ARE_LOCALES_EQUAL(OptionName, LOCALE_COLORMENU_OSD_PRESET)) {
		int preset = * (int *) data;
		printf("preset %d (setting %d)\n", preset, g_settings.screen_preset);

		CNeutrinoApp::getInstance()->setScreenSettings();
		osd_menu->hide();
		if (g_InfoViewer == NULL)
			g_InfoViewer = new CInfoViewer;
		g_InfoViewer->changePB();
		return true;
	}
#ifdef ENABLE_CHANGE_OSD_RESOLUTION
        else if (ARE_LOCALES_EQUAL(OptionName, LOCALE_COLORMENU_OSD_RESOLUTION))
	{
		if (frameBuffer->osd_resolutions.empty())
			return true;
		osd_menu->hide();
		uint32_t osd_mode = (uint32_t)*(int*)data;
		COsdHelpers::getInstance()->g_settings_osd_resolution_save = osd_mode;
		COsdHelpers::getInstance()->changeOsdResolution(osd_mode);
#if 0
		if (frameBuffer->fullHdAvailable()) {
			if (frameBuffer->osd_resolutions.empty())
				return true;

			size_t index = (size_t)*(int*)data;
			size_t resCount = frameBuffer->osd_resolutions.size();
			if (index >= resCount)
				index = 0;

			uint32_t resW = frameBuffer->osd_resolutions[index].xRes;
			uint32_t resH = frameBuffer->osd_resolutions[index].yRes;
			uint32_t bpp  = frameBuffer->osd_resolutions[index].bpp;
			int switchFB = frameBuffer->setMode(resW, resH, bpp);

			if (switchFB == 0) {
//printf("\n>>>>>[%s:%d] New res: %dx%dx%d\n \n", __func__, __LINE__, resW, resH, bpp);
				osd_menu->hide();
				frameBuffer->Clear();
				CNeutrinoApp::getInstance()->setScreenSettings();
				CNeutrinoApp::getInstance()->SetupFonts(CNeutrinoFonts::FONTSETUP_NEUTRINO_FONT);
				CVolumeHelper::getInstance()->refresh();
				CInfoClock::getInstance()->ClearDisplay();
				FileTimeOSD->Init();
				if (CNeutrinoApp::getInstance()->channelList)
					CNeutrinoApp::getInstance()->channelList->ResetModules();
				if (g_InfoViewer)
					g_InfoViewer->ResetModules();
			}
		}
#endif
		return true;
	}
#endif
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
	else if ((ARE_LOCALES_EQUAL(OptionName, LOCALE_MISCSETTINGS_INFOCLOCK)) ||
		 (ARE_LOCALES_EQUAL(OptionName, LOCALE_CLOCK_SIZE_HEIGHT)) ||
		 (ARE_LOCALES_EQUAL(OptionName, LOCALE_CLOCK_SECONDS))) {
		CInfoClock::getInstance()->ClearDisplay();
		FileTimeOSD->Init();
	}
	return false;
}

int COsdSetup::showContextChanlistMenu(CChannelList *parent_channellist)
{
	static int cselected = -1;

	CMenuWidget * menu_chanlist = new CMenuWidget(LOCALE_MAINMENU_SETTINGS, NEUTRINO_ICON_SETTINGS, width);

	//using native callback to ensure stop header clock in parent channellist before paint this menu window
	if (parent_channellist && parent_channellist->getHeaderObject()->getClockObject())
		menu_chanlist->OnBeforePaint.connect(sigc::mem_fun(parent_channellist->getHeaderObject()->getClockObject(), &CComponentsFrmClock::block));

	menu_chanlist->enableSaveScreen(true);
	menu_chanlist->enableFade(false);
	menu_chanlist->setSelected(cselected);

	showOsdChanlistSetup(menu_chanlist);
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

	CMenuForwarder * mf = new CMenuDForwarder(LOCALE_FONTMENU_HEAD, true, NULL, fontSettingsSubMenu, NULL, CRCInput::RC_red);
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

#define SCREENSAVER_MODE_OPTION_COUNT 3
const CMenuOptionChooser::keyval SCREENSAVER_MODE_OPTIONS[SCREENSAVER_MODE_OPTION_COUNT] =
{
	{ 0, LOCALE_SCREENSAVER_MODE_IMAGE },
	{ 1, LOCALE_SCREENSAVER_MODE_CLOCK },
	{ 2, LOCALE_SCREENSAVER_MODE_CLOCK_COLOR }
};

void COsdSetup::showOsdScreensaverSetup(CMenuWidget *menu_screensaver)
{
	menu_screensaver->addIntroItems(LOCALE_SCREENSAVER_MENU);

	screensaverActivate.Clear();
	screensaverOptActivate.Clear();

	// screensaver delay
	CMenuOptionNumberChooser* nc = new CMenuOptionNumberChooser(LOCALE_SCREENSAVER_DELAY, &g_settings.screensaver_delay, true, 0, 999, this, CRCInput::RC_nokey, NULL, 0, 0, LOCALE_SCREENSAVER_OFF);
	nc->setNumberFormat(std::string("%d ") + g_Locale->getText(LOCALE_UNIT_SHORT_MINUTE));
	nc->setHint("", LOCALE_MENU_HINT_SCREENSAVER_DELAY);
	menu_screensaver->addItem(nc);

	// screensaver mode
	CMenuOptionChooser* oc = new CMenuOptionChooser(LOCALE_SCREENSAVER_MODE, &g_settings.screensaver_mode, SCREENSAVER_MODE_OPTIONS, SCREENSAVER_MODE_OPTION_COUNT, (g_settings.screensaver_delay != 0), this);
	oc->setHint("", LOCALE_MENU_HINT_SCREENSAVER_MODE);
	menu_screensaver->addItem(oc);
	screensaverActivate.Add(oc);

	// screensaver timeout
	nc = new CMenuOptionNumberChooser(LOCALE_SCREENSAVER_TIMEOUT, &g_settings.screensaver_timeout, (g_settings.screensaver_delay != 0), 0, 60, NULL, CRCInput::RC_nokey, NULL, 0, 0, LOCALE_OPTIONS_OFF);
	nc->setNumberFormat(std::string("%d ") + g_Locale->getText(LOCALE_UNIT_SHORT_SECOND));
	nc->setHint("", LOCALE_MENU_HINT_SCREENSAVER_TIMEOUT);
	menu_screensaver->addItem(nc);
	screensaverActivate.Add(nc);

	// screensaver_dir
	CMenuForwarder *mf = new CMenuForwarder(LOCALE_SCREENSAVER_DIR, (g_settings.screensaver_delay != 0 && g_settings.screensaver_mode == 0), g_settings.screensaver_dir, this, "screensaver_dir");
	mf->setHint("", LOCALE_MENU_HINT_SCREENSAVER_DIR);
	menu_screensaver->addItem(mf);
	screensaverOptActivate.Add(mf);

	// screensaver random mode
	oc = new CMenuOptionChooser(LOCALE_SCREENSAVER_RANDOM, &g_settings.screensaver_random, OPTIONS_OFF0_ON1_OPTIONS, OPTIONS_OFF0_ON1_OPTION_COUNT, (g_settings.screensaver_delay != 0 && g_settings.screensaver_mode == 0));
	oc->setHint("", LOCALE_MENU_HINT_SCREENSAVER_RANDOM);
	menu_screensaver->addItem(oc);
	screensaverOptActivate.Add(oc);
}

void COsdSetup::paintWindowSize(int w, int h)
{
	if (win_demo == NULL) {
		win_demo = new CComponentsShapeSquare(0, 0, 0, 0);
		win_demo->setFrameThickness(8);
		win_demo->disableShadow();
		win_demo->setColorBody(COL_BACKGROUND);
		win_demo->setColorFrame(COL_RED);
		win_demo->doPaintBg(true);
	}
	else {
		if (win_demo->isPainted())
			win_demo->kill();
	}
	
	g_settings.window_width = w;
	g_settings.window_height = h;
	if (g_settings.window_width > WINDOW_SIZE_MAX)	
		g_settings.window_width = WINDOW_SIZE_MAX;
	if (g_settings.window_width < WINDOW_SIZE_MIN)	
		g_settings.window_width = WINDOW_SIZE_MIN;
	if (g_settings.window_height > WINDOW_SIZE_MAX)	
		g_settings.window_height = WINDOW_SIZE_MAX;
	if (g_settings.window_height < WINDOW_SIZE_MIN)	
		g_settings.window_height = WINDOW_SIZE_MIN;

	win_demo->setWidth(frameBuffer->getScreenWidthRel());
	win_demo->setHeight(frameBuffer->getScreenHeightRel());
	win_demo->setXPos(getScreenStartX(win_demo->getWidth()));
	win_demo->setYPos(getScreenStartY(win_demo->getHeight()));

	win_demo->paint(CC_SAVE_SCREEN_NO);
}
