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


#include "osd_setup.h"
#include "alphasetup.h"
#include "themes.h"
#include "screensetup.h"
#include "osdlang_setup.h"
#include "themes.h"
#include "filebrowser.h"

#include <global.h>
#include <neutrino.h>
#include <mymenu.h>

#include <gui/widget/icons.h>
#include <gui/widget/colorchooser.h>
#include <gui/widget/stringinput.h>

#include <driver/screen_max.h>

#include <system/debug.h>

static CTimingSettingsNotifier timingsettingsnotifier;

extern const char * locale_real_names[];
extern std::string ttx_font_file;

COsdSetup::COsdSetup(bool wizard_mode)
{
	frameBuffer = CFrameBuffer::getInstance();

	colorSetupNotifier = new CColorSetupNotifier();
	colorSetupNotifier->changeNotify(NONEXISTANT_LOCALE, NULL);

	fontsizenotifier = new CFontSizeNotifier;

	is_wizard = wizard_mode;
	
	width = w_max (30, 10); //%
	hheight = g_Font[SNeutrinoSettings::FONT_TYPE_MENU_TITLE]->getHeight();
	mheight = g_Font[SNeutrinoSettings::FONT_TYPE_MENU]->getHeight();
	height 	= hheight+13*mheight+ 10;
	x	= getScreenStartX (width);
	y	= getScreenStartY (height);
	
}

COsdSetup::~COsdSetup()
{
	delete colorSetupNotifier;
	delete fontsizenotifier;
}

void COsdSetup::hide()
{
	frameBuffer->paintBackgroundBoxRel(x,y, width,height);
}


int COsdSetup::exec(CMenuTarget* parent, const std::string &actionKey)
{
	dprintf(DEBUG_DEBUG, "init osd setup\n");

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
		
		CMenuWidget fontscale(LOCALE_FONTMENU_HEAD, NEUTRINO_ICON_COLORS);
		fontscale.addIntroItems(LOCALE_FONTMENU_SCALING);
		fontscale.addItem(new CMenuOptionNumberChooser(LOCALE_FONTMENU_SCALING_X, &xre, true, 50, 200));
		fontscale.addItem(new CMenuOptionNumberChooser(LOCALE_FONTMENU_SCALING_Y, &yre, true, 50, 200));
		fontscale.exec(NULL, "");
		
		if (xre != g_settings.screen_xres || yre != g_settings.screen_yres) {
			printf("[neutrino] new font scale settings x: %d%% y: %d%%\n", xre, yre);
			g_settings.screen_xres = xre;
			g_settings.screen_yres = yre;
			CNeutrinoApp::getInstance()->SetupFonts();
		}
		return menu_return::RETURN_REPAINT;
	}
	else if(actionKey=="osd.def") {
		for (int i = 0; i < TIMING_SETTING_COUNT; i++)
			g_settings.timing[i] = default_timing[i];

		CNeutrinoApp::getInstance()->SetupTiming();
		return menu_return::RETURN_REPAINT;
	}

	showOsdSetup();
	
	return menu_return::RETURN_REPAINT;	
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
	{ 4 , LOCALE_SETTINGS_POS_DEFAULT_CENTER },
	{ 5 , LOCALE_SETTINGS_POS_HIGHER_CENTER }
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


//show osd setup
void COsdSetup::showOsdSetup()
{
	//osd main menu
	CMenuWidget *osd_menu = new CMenuWidget(LOCALE_MAINMENU_SETTINGS, NEUTRINO_ICON_COLORS, width);
	osd_menu->setWizardMode(is_wizard);
	
	//menu colors
	CMenuWidget *osd_menu_colors = new CMenuWidget(LOCALE_MAINMENU_SETTINGS, NEUTRINO_ICON_COLORS, width);
	
	//intro with subhead and back button
	osd_menu->addIntroItems(LOCALE_MAINSETTINGS_OSD);
	
	//item menu colors
	showOsdMenueColorSetup(osd_menu_colors);
	osd_menu->addItem(new CMenuForwarder(LOCALE_COLORMENU_MENUCOLORS, true, NULL, osd_menu_colors, NULL, CRCInput::RC_red, NEUTRINO_ICON_BUTTON_RED));
	
	//fonts
	CMenuWidget *osd_menu_fonts = new CMenuWidget(LOCALE_MAINMENU_SETTINGS, NEUTRINO_ICON_COLORS, width);
	showOsdFontSizeSetup(osd_menu_fonts);
	osd_menu->addItem(new CMenuForwarder(LOCALE_FONTMENU_HEAD, true, NULL, osd_menu_fonts, NULL, CRCInput::RC_green, NEUTRINO_ICON_BUTTON_GREEN));
	
	//timeouts
	CMenuWidget *osd_menu_timing = new CMenuWidget(LOCALE_MAINMENU_SETTINGS, NEUTRINO_ICON_SETTINGS, width);
	showOsdTimeoutSetup(osd_menu_timing);
	osd_menu->addItem(new CMenuForwarder(LOCALE_COLORMENU_TIMING, true, NULL, osd_menu_timing, NULL, CRCInput::RC_yellow, NEUTRINO_ICON_BUTTON_YELLOW));
	
	//screen
	osd_menu->addItem(new CMenuForwarder(LOCALE_VIDEOMENU_SCREENSETUP, true, NULL, new CScreenSetup(), NULL, CRCInput::RC_blue, NEUTRINO_ICON_BUTTON_BLUE));
	
	//infobar
	CMenuWidget *osd_menu_infobar = new CMenuWidget(LOCALE_MAINMENU_SETTINGS, NEUTRINO_ICON_SETTINGS, width);
	showOsdInfobarSetup(osd_menu_infobar);
	osd_menu->addItem(new CMenuForwarder(LOCALE_MISCSETTINGS_INFOBAR, true, NULL, osd_menu_infobar, NULL, CRCInput::RC_1, NEUTRINO_ICON_BUTTON_1));
	
	//monitor
 	CScreenPresetNotifier * presetNotify = new CScreenPresetNotifier();
	osd_menu->addItem(new CMenuOptionChooser(LOCALE_COLORMENU_OSD_PRESET, &g_settings.screen_preset, OSD_PRESET_OPTIONS, OSD_PRESET_OPTIONS_COUNT, true, presetNotify));
	
	osd_menu->addItem(GenericMenuSeparatorLine);
	//options
	osd_menu->addItem(new CMenuOptionChooser(LOCALE_EXTRA_ROUNDED_CORNERS, &g_settings.rounded_corners, MENU_CORNERSETTINGS_TYPE_OPTIONS, MENU_CORNERSETTINGS_TYPE_OPTION_COUNT, true));
	osd_menu->addItem(new CMenuOptionChooser(LOCALE_EXTRA_SCRAMBLED_MESSAGE, &g_settings.scrambled_message, OPTIONS_OFF0_ON1_OPTIONS, OPTIONS_OFF0_ON1_OPTION_COUNT, true));
	osd_menu->addItem(new CMenuOptionChooser(LOCALE_INFOVIEWER_SUBCHAN_DISP_POS, &g_settings.infobar_subchan_disp_pos, INFOBAR_SUBCHAN_DISP_POS_OPTIONS, INFOBAR_SUBCHAN_DISP_POS_OPTIONS_COUNT, true));
	osd_menu->addItem(new CMenuOptionChooser(LOCALE_EXTRA_VOLUME_POS, &g_settings.volume_pos, VOLUMEBAR_DISP_POS_OPTIONS, VOLUMEBAR_DISP_POS_OPTIONS_COUNT, true));
	osd_menu->addItem(new CMenuOptionChooser(LOCALE_SETTINGS_MENU_POS, &g_settings.menu_pos, MENU_DISP_POS_OPTIONS, MENU_DISP_POS_OPTIONS_COUNT, true));
	osd_menu->addItem(new CMenuOptionChooser(LOCALE_COLORMENU_FADE, &g_settings.widget_fade, OPTIONS_OFF0_ON1_OPTIONS, OPTIONS_OFF0_ON1_OPTION_COUNT, true ));

	
	osd_menu->exec(NULL, "");
	osd_menu->hide();
	delete osd_menu;
}
	
//menue colors
void COsdSetup::showOsdMenueColorSetup(CMenuWidget *menu_colors)
{
	menu_colors->addIntroItems(LOCALE_COLORMENU_MENUCOLORS);
	
	menu_colors->addItem( new CMenuForwarder(LOCALE_COLORMENU_THEMESELECT, true, NULL, new CThemes(), NULL, CRCInput::RC_red, NEUTRINO_ICON_BUTTON_RED) );

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
	menu_colors->addItem( new CMenuForwarder(LOCALE_COLORMENU_BACKGROUND, true, NULL, chHeadcolor ));
	menu_colors->addItem( new CMenuForwarder(LOCALE_COLORMENU_TEXTCOLOR, true, NULL, chHeadTextcolor ));
	menu_colors->addItem( new CMenuSeparator(CMenuSeparator::LINE | CMenuSeparator::STRING, LOCALE_COLORMENUSETUP_MENUCONTENT));
	menu_colors->addItem( new CMenuForwarder(LOCALE_COLORMENU_BACKGROUND, true, NULL, chContentcolor ));
	menu_colors->addItem( new CMenuForwarder(LOCALE_COLORMENU_TEXTCOLOR, true, NULL, chContentTextcolor ));
	menu_colors->addItem( new CMenuSeparator(CMenuSeparator::LINE | CMenuSeparator::STRING, LOCALE_COLORMENUSETUP_MENUCONTENT_INACTIVE));
	menu_colors->addItem( new CMenuForwarder(LOCALE_COLORMENU_BACKGROUND, true, NULL, chContentInactivecolor ));
	menu_colors->addItem( new CMenuForwarder(LOCALE_COLORMENU_TEXTCOLOR, true, NULL, chContentInactiveTextcolor));
	menu_colors->addItem( new CMenuSeparator(CMenuSeparator::LINE | CMenuSeparator::STRING, LOCALE_COLORMENUSETUP_MENUCONTENT_SELECTED));
	menu_colors->addItem( new CMenuForwarder(LOCALE_COLORMENU_BACKGROUND, true, NULL, chContentSelectedcolor ));
	menu_colors->addItem( new CMenuForwarder(LOCALE_COLORMENU_TEXTCOLOR, true, NULL, chContentSelectedTextcolor ));

	CColorChooser* chInfobarcolor = new CColorChooser(LOCALE_COLORMENU_BACKGROUND, &g_settings.infobar_red, 
			&g_settings.infobar_green, &g_settings.infobar_blue, &g_settings.infobar_alpha, colorSetupNotifier);
	CColorChooser* chInfobarTextcolor = new CColorChooser(LOCALE_COLORMENU_TEXTCOLOR, &g_settings.infobar_Text_red, 
			&g_settings.infobar_Text_green, &g_settings.infobar_Text_blue, NULL, colorSetupNotifier);

	menu_colors->addItem( new CMenuSeparator(CMenuSeparator::LINE | CMenuSeparator::STRING, LOCALE_COLORSTATUSBAR_TEXT));
	menu_colors->addItem( new CMenuForwarder(LOCALE_COLORMENU_BACKGROUND, true, NULL, chInfobarcolor ));
	menu_colors->addItem( new CMenuForwarder(LOCALE_COLORMENU_TEXTCOLOR, true, NULL, chInfobarTextcolor ));
}


//font settings
const SNeutrinoSettings::FONT_TYPES channellist_font_sizes[4] =
{
	SNeutrinoSettings::FONT_TYPE_CHANNELLIST,
	SNeutrinoSettings::FONT_TYPE_CHANNELLIST_DESCR,
	SNeutrinoSettings::FONT_TYPE_CHANNELLIST_NUMBER,
	SNeutrinoSettings::FONT_TYPE_CHANNEL_NUM_ZAP
};

const SNeutrinoSettings::FONT_TYPES eventlist_font_sizes[4] =
{
	SNeutrinoSettings::FONT_TYPE_EVENTLIST_TITLE,
	SNeutrinoSettings::FONT_TYPE_EVENTLIST_ITEMLARGE,
	SNeutrinoSettings::FONT_TYPE_EVENTLIST_ITEMSMALL,
	SNeutrinoSettings::FONT_TYPE_EVENTLIST_DATETIME,
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

const SNeutrinoSettings::FONT_TYPES other_font_sizes[4] =
{
	SNeutrinoSettings::FONT_TYPE_MENU,
	SNeutrinoSettings::FONT_TYPE_MENU_TITLE,
	SNeutrinoSettings::FONT_TYPE_MENU_INFO,
	SNeutrinoSettings::FONT_TYPE_FILEBROWSER_ITEM
};

font_sizes_groups font_sizes_groups[6] =
{
	{LOCALE_FONTMENU_MENU       , 4, other_font_sizes      , "fontsize.doth"},
	{LOCALE_FONTMENU_CHANNELLIST, 4, channellist_font_sizes, "fontsize.dcha"},
	{LOCALE_FONTMENU_EVENTLIST  , 4, eventlist_font_sizes  , "fontsize.deve"},
	{LOCALE_FONTMENU_EPG        , 4, epg_font_sizes        , "fontsize.depg"},
	{LOCALE_FONTMENU_INFOBAR    , 4, infobar_font_sizes    , "fontsize.dinf"},
	{LOCALE_FONTMENU_GAMELIST   , 2, gamelist_font_sizes   , "fontsize.dgam"}
};

#define FONT_STYLE_REGULAR 0
#define FONT_STYLE_BOLD    1
#define FONT_STYLE_ITALIC  2

font_sizes_struct neutrino_font[FONT_TYPE_COUNT] =
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
	{LOCALE_FONTSIZE_GAMELIST_ITEMLARGE ,  20, FONT_STYLE_BOLD   , 1},
	{LOCALE_FONTSIZE_GAMELIST_ITEMSMALL ,  16, FONT_STYLE_REGULAR, 1},
	{LOCALE_FONTSIZE_CHANNELLIST        ,  20, FONT_STYLE_BOLD   , 1},
	{LOCALE_FONTSIZE_CHANNELLIST_DESCR  ,  20, FONT_STYLE_REGULAR, 1},
	{LOCALE_FONTSIZE_CHANNELLIST_NUMBER ,  14, FONT_STYLE_BOLD   , 2},
	{LOCALE_FONTSIZE_CHANNEL_NUM_ZAP    ,  40, FONT_STYLE_BOLD   , 0},
	{LOCALE_FONTSIZE_INFOBAR_NUMBER     ,  50, FONT_STYLE_BOLD   , 0},
	{LOCALE_FONTSIZE_INFOBAR_CHANNAME   ,  30, FONT_STYLE_BOLD   , 0},
	{LOCALE_FONTSIZE_INFOBAR_INFO       ,  20, FONT_STYLE_REGULAR, 1},
	{LOCALE_FONTSIZE_INFOBAR_SMALL      ,  14, FONT_STYLE_REGULAR, 1},
	{LOCALE_FONTSIZE_FILEBROWSER_ITEM   ,  16, FONT_STYLE_BOLD   , 1}
};

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

	fontSettings->addIntroItems(LOCALE_FONTMENU_HEAD);

	fontSettings->addItem( new CMenuForwarder(LOCALE_COLORMENU_FONT, true, NULL, this, "select_font", CRCInput::RC_red, NEUTRINO_ICON_BUTTON_RED));
	fontSettings->addItem( new CMenuForwarder(LOCALE_COLORMENU_FONT_TTX, true, NULL, this, "ttx_font",  CRCInput::RC_green, NEUTRINO_ICON_BUTTON_GREEN));

	fontSettings->addItem(new CMenuSeparator(CMenuSeparator::LINE | CMenuSeparator::STRING, LOCALE_FONTMENU_SIZES));
	
	//submenu font scaling
	fontSettings->addItem(new CMenuForwarder(LOCALE_FONTMENU_SCALING, true, NULL, this, "font_scaling",  CRCInput::RC_green, NEUTRINO_ICON_BUTTON_YELLOW));
	//fontSettings->addItem( new CMenuForwarder(LOCALE_EPGPLUS_SELECT_FONT_NAME, true, NULL, this, "select_font"));
	for (int i = 0; i < 6; i++)
	{
		CMenuWidget *fontSettingsSubMenu = new CMenuWidget(LOCALE_FONTMENU_HEAD, NEUTRINO_ICON_KEYBINDING);
		
		fontSettingsSubMenu->addIntroItems(font_sizes_groups[i].groupname);
		
		for (unsigned int j = 0; j < font_sizes_groups[i].count; j++)
		{
			AddFontSettingItem(*fontSettingsSubMenu, font_sizes_groups[i].content[j]);
		}
		fontSettingsSubMenu->addItem(GenericMenuSeparatorLine);
		fontSettingsSubMenu->addItem(new CMenuForwarder(LOCALE_OPTIONS_DEFAULT, true, NULL, this, font_sizes_groups[i].actionkey));

		fontSettings->addItem(new CMenuForwarder(font_sizes_groups[i].groupname, true, NULL, fontSettingsSubMenu));
	}
}

//osd timeouts
void COsdSetup::showOsdTimeoutSetup(CMenuWidget* menu_timeout)
{
	menu_timeout->addIntroItems(LOCALE_COLORMENU_TIMING);
	
	for (int i = 0; i < TIMING_SETTING_COUNT; i++)
	{
		CStringInput * timing_item = new CStringInput(timing_setting_name[i], g_settings.timing_string[i], 3, LOCALE_TIMING_HINT_1, LOCALE_TIMING_HINT_2, "0123456789 ", &timingsettingsnotifier);
		menu_timeout->addItem(new CMenuForwarder(timing_setting_name[i], true, g_settings.timing_string[i], timing_item));
	}

	menu_timeout->addItem(GenericMenuSeparatorLine);
	menu_timeout->addItem(new CMenuForwarder(LOCALE_OPTIONS_DEFAULT, true, NULL, this, "osd.def", CRCInput::RC_red, NEUTRINO_ICON_BUTTON_RED));
}

#define LOCALE_MISCSETTINGS_INFOBAR_DISP_OPTIONS_COUNT 4
const CMenuOptionChooser::keyval  LOCALE_MISCSETTINGS_INFOBAR_DISP_OPTIONS[LOCALE_MISCSETTINGS_INFOBAR_DISP_OPTIONS_COUNT]=
{
   { 0 , LOCALE_MISCSETTINGS_INFOBAR_DISP_0 },
   { 1 , LOCALE_MISCSETTINGS_INFOBAR_DISP_1 },
   { 2 , LOCALE_MISCSETTINGS_INFOBAR_DISP_2 },
   { 3 , LOCALE_MISCSETTINGS_INFOBAR_DISP_3 }
};

void COsdSetup::showOsdInfobarSetup(CMenuWidget *menu_infobar)
{
	menu_infobar->addIntroItems(LOCALE_MISCSETTINGS_INFOBAR);
	
	menu_infobar->addItem(new CMenuOptionChooser(LOCALE_PROGRESSBAR_COLOR, &g_settings.progressbar_color, OPTIONS_OFF0_ON1_OPTIONS, OPTIONS_OFF0_ON1_OPTION_COUNT, true));

	menu_infobar->addItem(new CMenuOptionChooser(LOCALE_MISCSETTINGS_INFOBAR_CASYSTEM_DISPLAY, &g_settings.casystem_display, INFOBAR_CASYSTEM_MODE_OPTIONS, INFOBAR_CASYSTEM_MODE_OPTION_COUNT, true));

	menu_infobar->addItem(new CMenuOptionChooser(LOCALE_MISCSETTINGS_INFOBAR_DISP_LOG, &g_settings.infobar_show_channellogo, LOCALE_MISCSETTINGS_INFOBAR_DISP_OPTIONS, LOCALE_MISCSETTINGS_INFOBAR_DISP_OPTIONS_COUNT, true));
	menu_infobar->addItem(new CMenuOptionChooser(LOCALE_MISCSETTINGS_VIRTUAL_ZAP_MODE, &g_settings.virtual_zap_mode, OPTIONS_OFF0_ON1_OPTIONS, OPTIONS_OFF0_ON1_OPTION_COUNT, true));
	menu_infobar->addItem(new CMenuOptionChooser(LOCALE_MISCSETTINGS_INFOBAR_SAT_DISPLAY, &g_settings.infobar_sat_display, OPTIONS_OFF0_ON1_OPTIONS, OPTIONS_OFF0_ON1_OPTION_COUNT, true));
	menu_infobar->addItem(new CMenuOptionChooser(LOCALE_MISCSETTINGS_INFOBAR_SHOW_VAR_HDD, &g_settings.infobar_show_var_hdd, OPTIONS_OFF0_ON1_OPTIONS, OPTIONS_OFF0_ON1_OPTION_COUNT, true));
	menu_infobar->addItem(new CMenuOptionChooser(LOCALE_MISCSETTINGS_INFOBAR_SHOW, &g_settings.infobar_show, OPTIONS_OFF0_ON1_OPTIONS, OPTIONS_OFF0_ON1_OPTION_COUNT, true));
}




