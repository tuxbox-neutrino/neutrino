/*
	$port: themes.cpp,v 1.16 2010/09/05 21:27:44 tuxbox-cvs Exp $
	
	Neutrino-GUI  -   DBoxII-Project

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

	Copyright (C) 2007, 2008, 2009 (flasher) Frank Liebelt

*/

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include <errno.h>
#include <sys/wait.h>
#include <global.h>
#include <neutrino.h>
#include "widget/menue.h"
#include <system/helpers.h>
#include <system/debug.h>
#include <system/setting_helpers.h>
#include <gui/widget/stringinput.h>
#include <gui/widget/stringinput_ext.h>
#include <gui/widget/keyboard_input.h>
#include <gui/widget/msgbox.h>
#include <driver/screen_max.h>

#include <sys/stat.h>
#include <sys/time.h>

#include "glcdthemes.h"

#define FILE_SUFFIX ".otheme"

static 	SNeutrinoGlcdTheme &t = g_settings.glcd_theme;

CGLCDThemes::CGLCDThemes()
: themefile('\t')
{
	width = 40;

	hasThemeChanged = false;
}

CGLCDThemes* CGLCDThemes::getInstance()
{
	static CGLCDThemes* th = NULL;

	if(!th) {
		th = new CGLCDThemes();
		dprintf(DEBUG_DEBUG, "CGLCDThemes Instance created\n");
	}
	return th;
}

int CGLCDThemes::exec(CMenuTarget* parent, const std::string & actionKey)
{
	int res = menu_return::RETURN_REPAINT;

	if( !actionKey.empty() )
	{
		if (actionKey=="default_theme")
		{
			if(!applyDefaultTheme())
				setupDefaultColors(); // fallback
			changeNotify(NONEXISTANT_LOCALE, NULL);
		}
		else
		{
			std::string themeFile = actionKey;
			if ( strstr(themeFile.c_str(), "{U}") != 0 ) 
			{
				themeFile.erase(0, 3);
				readFile(((std::string)THEMESDIR_VAR + "/oled/" + themeFile + FILE_SUFFIX).c_str());
			} 
			else
				readFile(((std::string)THEMESDIR + "/oled/" + themeFile + FILE_SUFFIX).c_str());
			g_settings.glcd_theme_name = themeFile;
		}
		OnAfterSelectTheme();
		CFrameBuffer::getInstance()->clearIconCache();
		return res;
	}


	if (parent)
		parent->hide();

	if ( !hasThemeChanged )
		rememberOldTheme( true );

	return Show();
}

void CGLCDThemes::initThemesMenu(CMenuWidget &themes)
{
	struct dirent **themelist;
	int n;
	const char *pfade[] = {THEMESDIR "/oled", THEMESDIR_VAR "/oled"};
	bool hasCVSThemes, hasUserThemes;
	hasCVSThemes = hasUserThemes = false;
	std::string userThemeFile = "";
	CMenuForwarder* oj;

	// only to visualize if we have a migrated theme
	if (g_settings.glcd_theme_name.empty() || g_settings.glcd_theme_name == MIGRATE_THEME_OLED_NAME)
	{
		themes.addItem(new CMenuSeparator(CMenuSeparator::LINE));
		themes.addItem(new CMenuForwarder(MIGRATE_THEME_OLED_NAME, false, "", this));
	}

	for(int p = 0;p < 2;p++)
	{
		n = scandir(pfade[p], &themelist, 0, alphasort);
		if(n < 0)
			perror("loading glcd themes: scandir");
		else
		{
			for(int count=0;count<n;count++)
			{
				char *file = themelist[count]->d_name;
				char *pos = strstr(file, ".otheme");
				if(pos != NULL)
				{
					if ( p == 0 && hasCVSThemes == false ) {
						themes.addItem(new CMenuSeparator(CMenuSeparator::LINE | CMenuSeparator::STRING, LOCALE_COLORTHEMEMENU_SELECT2));
						hasCVSThemes = true;
					} else if ( p == 1 && hasUserThemes == false ) {
						themes.addItem(new CMenuSeparator(CMenuSeparator::LINE | CMenuSeparator::STRING, LOCALE_COLORTHEMEMENU_SELECT1));
						hasUserThemes = true;
					}
					*pos = '\0';
					if ( p == 1 ) {
						userThemeFile = "{U}" + (std::string)file;
						oj = new CMenuForwarder(file, true, "", this, userThemeFile.c_str());
					} else
						oj = new CMenuForwarder(file, true, "", this, file);

					themes.addItem( oj );
				}
				free(themelist[count]);
			}
			free(themelist);
		}
	}

	//on first paint exec this methode to set marker to item
	markSelectedTheme(&themes);

	//for all next menu paints markers are setted with this slot inside exec()
	if (OnAfterSelectTheme.empty())
		OnAfterSelectTheme.connect(sigc::bind(sigc::mem_fun(this, &CGLCDThemes::markSelectedTheme), &themes));
}

int CGLCDThemes::Show()
{
	std::string file_name = "";

	CMenuWidget themes (LOCALE_COLORMENU_MENUCOLORS, NEUTRINO_ICON_SETTINGS, width);
	sigc::slot0<void> slot_repaint = sigc::mem_fun(themes, &CMenuWidget::hide); //we want to clean screen before repaint after changed Option
	themes.OnBeforePaint.connect(slot_repaint);
	
	themes.addIntroItems(LOCALE_COLORTHEMEMENU_HEAD2);
	
	//set default theme
	std::string default_theme = DEFAULT_OLED_THEME;
	if (default_theme.empty())
		default_theme = "Default";
	CMenuForwarder* fw = new CMenuForwarder(LOCALE_COLORTHEMEMENU_NEUTRINO_THEME, true, default_theme.c_str(), this, "default_theme", CRCInput::RC_red);
	themes.addItem(fw);
	fw->setHint("", LOCALE_COLORTHEMEMENU_NEUTRINO_THEME_HINT);
	
	initThemesMenu(themes);

	CKeyboardInput nameInput(LOCALE_COLORTHEMEMENU_NAME, &file_name);
	CMenuForwarder *m1 = new CMenuForwarder(LOCALE_COLORTHEMEMENU_SAVE, true , NULL, &nameInput, NULL, CRCInput::RC_green);

	if (!CFileHelpers::createDir(THEMESDIR_VAR "/oled")) {
		printf("[neutrino glcd theme] error creating %s\n", THEMESDIR_VAR "/oled");
	}

	if (access(THEMESDIR_VAR "/oled", F_OK) == 0 ) {
		themes.addItem(GenericMenuSeparatorLine);
		themes.addItem(m1);
	} else {
		delete m1;
		printf("[neutrino glcd theme] error accessing %s\n", THEMESDIR_VAR "/oled");
	}

	int res = themes.exec(NULL, "");

	if (!file_name.empty()) {
		saveFile(((std::string)THEMESDIR_VAR + "/oled/" + file_name + FILE_SUFFIX).c_str());
	}

	if (hasThemeChanged) {
		if (ShowMsg(LOCALE_MESSAGEBOX_INFO, LOCALE_COLORTHEMEMENU_QUESTION, CMsgBox::mbrYes, CMsgBox::mbYes | CMsgBox::mbNo, NEUTRINO_ICON_SETTINGS) != CMsgBox::mbrYes)
			rememberOldTheme( false );
		else
			hasThemeChanged = false;
	}
	return res;
}

void CGLCDThemes::rememberOldTheme(bool remember)
{
	if ( remember ) {
		oldTheme = t;
		oldTheme_name = g_settings.glcd_theme_name;
	} else {
		t = oldTheme;
		g_settings.glcd_theme_name = oldTheme_name;

		changeNotify(NONEXISTANT_LOCALE, NULL);
		hasThemeChanged = false;
	}
}

void CGLCDThemes::readFile(const char *themename)
{
	if(themefile.loadConfig(themename))
	{
		getTheme(themefile);

		changeNotify(NONEXISTANT_LOCALE, NULL);
		hasThemeChanged = true;
	}
	else
		printf("[neutrino glcd theme] %s not found\n", themename);
}

void CGLCDThemes::saveFile(const char *themename)
{
	setTheme(themefile);

	if (themefile.getModifiedFlag()){
		printf("[neutrino glcd theme] save theme into %s\n", themename);
		if (!themefile.saveConfig(themename))
			printf("[neutrino glcd theme] %s write error\n", themename);
	}
}

bool CGLCDThemes::applyDefaultTheme()
{
	g_settings.glcd_theme_name = DEFAULT_OLED_THEME;
	if (g_settings.glcd_theme_name.empty())
		g_settings.glcd_theme_name = "Default";
	std::string default_theme = THEMESDIR "/oled/" + g_settings.glcd_theme_name + ".otheme";
	if(themefile.loadConfig(default_theme)){
		getTheme(themefile);
		return true;
	}
	dprintf(DEBUG_NORMAL, "[CGLCDThemes]\t[%s - %d], No default theme found, creating migrated theme from current theme settings\n", __func__, __LINE__);
	return false;
}

// setup default Colors
void CGLCDThemes::setupDefaultColors()
{
	CConfigFile empty(':');
	getTheme(empty);
}

void CGLCDThemes::setTheme(CConfigFile &configfile)
{
	configfile.setInt32("glcd_color_fg_red", t.glcd_color_fg_red);
	configfile.setInt32("glcd_color_fg_green", t.glcd_color_fg_green);
	configfile.setInt32("glcd_color_fg_blue", t.glcd_color_fg_blue);
	configfile.setInt32("glcd_color_bg_red", t.glcd_color_bg_red);
	configfile.setInt32("glcd_color_bg_green", t.glcd_color_bg_green);
	configfile.setInt32("glcd_color_bg_blue", t.glcd_color_bg_blue);
	configfile.setInt32("glcd_color_bar_red", t.glcd_color_bar_red);
	configfile.setInt32("glcd_color_bar_green", t.glcd_color_bar_green);
	configfile.setInt32("glcd_color_bar_blue", t.glcd_color_bar_blue);
	configfile.setString("glcd_font", t.glcd_font);
	configfile.setString("glcd_background", t.glcd_background);
	configfile.setBool("glcd_show_progressbar", t.glcd_show_progressbar);
	configfile.setBool("glcd_show_duration", t.glcd_show_duration);
	configfile.setBool("glcd_show_start", t.glcd_show_start);
	configfile.setBool("glcd_show_end", t.glcd_show_end);
	configfile.setBool("glcd_show_time", t.glcd_show_time);
	configfile.setBool("glcd_show_weather", t.glcd_show_weather);
	configfile.setInt32("glcd_align_channel", t.glcd_align_channel);
	configfile.setInt32("glcd_align_epg", t.glcd_align_epg);
	configfile.setInt32("glcd_align_duration", t.glcd_align_duration);
	configfile.setInt32("glcd_align_start", t.glcd_align_start);
	configfile.setInt32("glcd_align_end", t.glcd_align_end);
	configfile.setInt32("glcd_align_time", t.glcd_align_time);
	configfile.setInt32("glcd_percent_channel", t.glcd_percent_channel);
	configfile.setInt32("glcd_channel_x_position", t.glcd_channel_x_position);
	configfile.setInt32("glcd_channel_y_position", t.glcd_channel_y_position);
	configfile.setInt32("glcd_percent_epg", t.glcd_percent_epg);
	configfile.setInt32("glcd_epg_x_position", t.glcd_epg_x_position);
	configfile.setInt32("glcd_epg_y_position", t.glcd_epg_y_position);
	configfile.setInt32("glcd_percent_duration", t.glcd_percent_duration);
	configfile.setInt32("glcd_duration_x_position", t.glcd_duration_x_position);
	configfile.setInt32("glcd_duration_y_position", t.glcd_duration_y_position);
	configfile.setInt32("glcd_percent_start", t.glcd_percent_start);
	configfile.setInt32("glcd_start_x_position", t.glcd_start_x_position);
	configfile.setInt32("glcd_start_y_position", t.glcd_start_y_position);
	configfile.setInt32("glcd_percent_end", t.glcd_percent_end);
	configfile.setInt32("glcd_end_x_position", t.glcd_end_x_position);
	configfile.setInt32("glcd_end_y_position", t.glcd_end_y_position);
	configfile.setInt32("glcd_percent_time", t.glcd_percent_time);
	configfile.setInt32("glcd_time_x_position", t.glcd_time_x_position);
	configfile.setInt32("glcd_time_y_position", t.glcd_time_y_position);
	configfile.setInt32("glcd_percent_bar", t.glcd_percent_bar);
	configfile.setInt32("glcd_bar_x_position", t.glcd_bar_x_position);
	configfile.setInt32("glcd_bar_y_position", t.glcd_bar_y_position);
	configfile.setInt32("glcd_bar_width", t.glcd_bar_width);
        configfile.setInt32("glcd_percent_logo", t.glcd_percent_logo);
        configfile.setInt32("glcd_logo_x_position", t.glcd_logo_x_position);
        configfile.setInt32("glcd_logo_y_position", t.glcd_logo_y_position);
	configfile.setInt32("glcd_percent_smalltext", t.glcd_percent_smalltext);
	configfile.setInt32("glcd_smalltext_y_position", t.glcd_smalltext_y_position);
	configfile.setInt32("glcd_rec_icon_x_position", t.glcd_rec_icon_x_position);
	configfile.setInt32("glcd_mute_icon_x_position", t.glcd_mute_icon_x_position);
	configfile.setInt32("glcd_ts_icon_x_position", t.glcd_ts_icon_x_position);
	configfile.setInt32("glcd_timer_icon_x_position", t.glcd_timer_icon_x_position);
	configfile.setInt32("glcd_ecm_icon_x_position", t.glcd_ecm_icon_x_position);
	configfile.setInt32("glcd_dd_icon_x_position", t.glcd_dd_icon_x_position);
	configfile.setInt32("glcd_txt_icon_x_position", t.glcd_txt_icon_x_position);
	configfile.setInt32("glcd_cam_icon_x_position", t.glcd_cam_icon_x_position);
	configfile.setInt32("glcd_digital_clock_y_position", t.glcd_digital_clock_y_position);
	configfile.setInt32("glcd_size_simple_clock", t.glcd_size_simple_clock);
	configfile.setInt32("glcd_simple_clock_y_position", t.glcd_simple_clock_y_position);
	configfile.setInt32("glcd_weather_x_position_current", t.glcd_weather_x_position_current);
	configfile.setInt32("glcd_weather_x_position_next", t.glcd_weather_x_position_next);
	configfile.setInt32("glcd_weather_y_position", t.glcd_weather_y_position);
	configfile.setInt32("glcd_weather_x_position_current_standby", t.glcd_weather_x_position_current_standby);
	configfile.setInt32("glcd_weather_x_position_next_standby", t.glcd_weather_x_position_next_standby);
	configfile.setInt32("glcd_weather_y_position_standby", t.glcd_weather_y_position_standby);
	configfile.setInt32("glcd_position_settings", t.glcd_position_settings);
}

void CGLCDThemes::getTheme(CConfigFile &configfile)
{
	t.glcd_color_fg_red = configfile.getInt32("glcd_color_fg_red", 0xff);
	t.glcd_color_fg_green = configfile.getInt32("glcd_color_fg_green", 0xff);
	t.glcd_color_fg_blue = configfile.getInt32("glcd_color_fg_blue", 0xff);
	t.glcd_color_bg_red = configfile.getInt32("glcd_color_bg_red", 0x00);
	t.glcd_color_bg_green = configfile.getInt32("glcd_color_bg_green", 0x00);
	t.glcd_color_bg_blue = configfile.getInt32("glcd_color_bg_blue", 0x00);
	t.glcd_color_bar_red = configfile.getInt32("glcd_color_bar_red", 0x62);
	t.glcd_color_bar_green = configfile.getInt32("glcd_color_bar_green", 0x62);
	t.glcd_color_bar_blue = configfile.getInt32("glcd_color_bar_blue", 0x62);
	t.glcd_font = configfile.getString("glcd_font", "");
	t.glcd_background = configfile.getString("glcd_background", "");
	t.glcd_show_progressbar = configfile.getBool("glcd_show_progressbar", false);
	t.glcd_show_duration = configfile.getBool("glcd_show_duration", false);
	t.glcd_show_start = configfile.getBool("glcd_show_start", false);
	t.glcd_show_end = configfile.getBool("glcd_show_end", false);
	t.glcd_show_time = configfile.getBool("glcd_show_time", false);
	t.glcd_show_weather = configfile.getBool("glcd_show_weather", false);
	t.glcd_align_channel = configfile.getInt32("glcd_align_channel", 0);
	t.glcd_align_epg = configfile.getInt32("glcd_align_epg", 0);
	t.glcd_align_duration = configfile.getInt32("glcd_align_duration", 0);
	t.glcd_align_start = configfile.getInt32("glcd_align_start", 0);
	t.glcd_align_end = configfile.getInt32("glcd_align_end", 0);
	t.glcd_align_time = configfile.getInt32("glcd_align_time", 0);
	t.glcd_percent_channel = configfile.getInt32("glcd_percent_channel", 0);
	t.glcd_channel_x_position = configfile.getInt32("glcd_channel_x_position", 0);
	t.glcd_channel_y_position = configfile.getInt32("glcd_channel_y_position", 0);
	t.glcd_percent_epg = configfile.getInt32("glcd_percent_epg", 0);
	t.glcd_epg_x_position = configfile.getInt32("glcd_epg_x_position", 0);
	t.glcd_epg_y_position = configfile.getInt32("glcd_epg_y_position", 0);
	t.glcd_percent_duration = configfile.getInt32("glcd_percent_duration", 0);
	t.glcd_duration_x_position = configfile.getInt32("glcd_duration_x_position", 0);
	t.glcd_duration_y_position = configfile.getInt32("glcd_duration_y_position", 0);
	t.glcd_percent_start = configfile.getInt32("glcd_percent_start", 0);
	t.glcd_start_x_position = configfile.getInt32("glcd_start_x_position", 0);
	t.glcd_start_y_position = configfile.getInt32("glcd_start_y_position", 0);
	t.glcd_percent_end = configfile.getInt32("glcd_percent_end", 0);
	t.glcd_end_x_position = configfile.getInt32("glcd_end_x_position", 0);
	t.glcd_end_y_position = configfile.getInt32("glcd_end_y_position", 0);
	t.glcd_percent_time = configfile.getInt32("glcd_percent_time", 0);
	t.glcd_time_x_position = configfile.getInt32("glcd_time_x_position", 0);
	t.glcd_time_y_position = configfile.getInt32("glcd_time_y_position", 0);
	t.glcd_percent_bar = configfile.getInt32("glcd_percent_bar", 0);
	t.glcd_bar_x_position = configfile.getInt32("glcd_bar_x_position", 0);
	t.glcd_bar_y_position = configfile.getInt32("glcd_bar_y_position", 0);
	t.glcd_bar_width = configfile.getInt32("glcd_bar_width", 0);
	t.glcd_percent_logo = configfile.getInt32("glcd_percent_logo", 0);
	t.glcd_logo_x_position = configfile.getInt32("glcd_logo_x_position", 0);
	t.glcd_logo_y_position = configfile.getInt32("glcd_logo_y_position", 0);
	t.glcd_percent_smalltext = configfile.getInt32("glcd_percent_smalltext", 0);
	t.glcd_smalltext_y_position = configfile.getInt32("glcd_smalltext_y_position", 0);
	t.glcd_rec_icon_x_position = configfile.getInt32("glcd_rec_icon_x_position", 0);
	t.glcd_mute_icon_x_position = configfile.getInt32("glcd_mute_icon_x_position", 0);
	t.glcd_ts_icon_x_position = configfile.getInt32("glcd_ts_icon_x_position", 0);
	t.glcd_timer_icon_x_position = configfile.getInt32("glcd_timer_icon_x_position", 0);
	t.glcd_ecm_icon_x_position = configfile.getInt32("glcd_ecm_icon_x_position", 0);
	t.glcd_dd_icon_x_position = configfile.getInt32("glcd_dd_icon_x_position", 0);
	t.glcd_txt_icon_x_position = configfile.getInt32("glcd_txt_icon_x_position", 0);
	t.glcd_cam_icon_x_position = configfile.getInt32("glcd_cam_icon_x_position", 0);
	t.glcd_digital_clock_y_position = configfile.getInt32("glcd_digital_clock_y_position", 0);
	t.glcd_size_simple_clock = configfile.getInt32("glcd_size_simple_clock", 0);
	t.glcd_simple_clock_y_position = configfile.getInt32("glcd_simple_clock_y_position", 0);
	t.glcd_weather_x_position_current = configfile.getInt32("glcd_weather_x_position_current", 0);
	t.glcd_weather_x_position_next = configfile.getInt32("glcd_weather_x_position_next", 0);
	t.glcd_weather_y_position = configfile.getInt32("glcd_weather_y_position", 0);
	t.glcd_weather_x_position_current_standby = configfile.getInt32("glcd_weather_x_position_current_standby", 0);
	t.glcd_weather_x_position_next_standby = configfile.getInt32("glcd_weather_x_position_next_standby", 0);
	t.glcd_weather_y_position_standby = configfile.getInt32("glcd_weather_y_position_standby", 0);
	t.glcd_position_settings = configfile.getInt32("glcd_position_settings", 0);

	if (g_settings.glcd_theme_name.empty())
		applyDefaultTheme();
}

void CGLCDThemes::markSelectedTheme(CMenuWidget *w)
{
	for (int i = 0; i < w->getItemsCount(); i++){
		w->getItem(i)->setInfoIconRight(NULL);
		if (g_settings.glcd_theme_name == w->getItem(i)->getName())
			w->getItem(i)->setInfoIconRight(NEUTRINO_ICON_MARKER_DIALOG_OK);
	}
}
