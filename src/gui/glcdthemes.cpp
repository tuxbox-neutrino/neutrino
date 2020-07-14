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
	configfile.setInt32("glcd_foreground_color_red", t.glcd_foreground_color_red);
	configfile.setInt32("glcd_foreground_color_green", t.glcd_foreground_color_green);
	configfile.setInt32("glcd_foreground_color_blue", t.glcd_foreground_color_blue);
	configfile.setInt32("glcd_background_color_red", t.glcd_background_color_red);
	configfile.setInt32("glcd_background_color_green", t.glcd_background_color_green);
	configfile.setInt32("glcd_background_color_blue", t.glcd_background_color_blue);
	configfile.setString("glcd_background_image", t.glcd_background_image);
	configfile.setString("glcd_font", t.glcd_font);
	configfile.setInt32("glcd_channel_percent", t.glcd_channel_percent);
	configfile.setInt32("glcd_channel_align", t.glcd_channel_align);
	configfile.setInt32("glcd_channel_x_position", t.glcd_channel_x_position);
	configfile.setInt32("glcd_channel_y_position", t.glcd_channel_y_position);
	configfile.setBool("glcd_logo", t.glcd_logo);
        configfile.setInt32("glcd_logo_percent", t.glcd_logo_percent);
        configfile.setInt32("glcd_logo_x_position", t.glcd_logo_x_position);
        configfile.setInt32("glcd_logo_y_position", t.glcd_logo_y_position);
	configfile.setInt32("glcd_epg_percent", t.glcd_epg_percent);
	configfile.setInt32("glcd_epg_align", t.glcd_epg_align);
	configfile.setInt32("glcd_epg_x_position", t.glcd_epg_x_position);
	configfile.setInt32("glcd_epg_y_position", t.glcd_epg_y_position);
	configfile.setBool("glcd_start", t.glcd_start);
	configfile.setInt32("glcd_start_percent", t.glcd_start_percent);
	configfile.setInt32("glcd_start_align", t.glcd_start_align);
	configfile.setInt32("glcd_start_x_position", t.glcd_start_x_position);
	configfile.setInt32("glcd_start_y_position", t.glcd_start_y_position);
	configfile.setBool("glcd_end", t.glcd_end);
	configfile.setInt32("glcd_end_percent", t.glcd_end_percent);
	configfile.setInt32("glcd_end_align", t.glcd_end_align);
	configfile.setInt32("glcd_end_x_position", t.glcd_end_x_position);
	configfile.setInt32("glcd_end_y_position", t.glcd_end_y_position);
	configfile.setBool("glcd_duration", t.glcd_duration);
	configfile.setInt32("glcd_duration_percent", t.glcd_duration_percent);
	configfile.setInt32("glcd_duration_align", t.glcd_duration_align);
	configfile.setInt32("glcd_duration_x_position", t.glcd_duration_x_position);
	configfile.setInt32("glcd_duration_y_position", t.glcd_duration_y_position);
	configfile.setBool("glcd_progressbar", t.glcd_progressbar);
	configfile.setInt32("glcd_progressbar_color_red", t.glcd_progressbar_color_red);
	configfile.setInt32("glcd_progressbar_color_green", t.glcd_progressbar_color_green);
	configfile.setInt32("glcd_progressbar_color_blue", t.glcd_progressbar_color_blue);
	configfile.setInt32("glcd_progressbar_percent", t.glcd_progressbar_percent);
	configfile.setInt32("glcd_progressbar_width", t.glcd_progressbar_width);
	configfile.setInt32("glcd_progressbar_x_position", t.glcd_progressbar_x_position);
	configfile.setInt32("glcd_progressbar_y_position", t.glcd_progressbar_y_position);
	configfile.setBool("glcd_time", t.glcd_time);
	configfile.setInt32("glcd_time_percent", t.glcd_time_percent);
	configfile.setInt32("glcd_time_align", t.glcd_time_align);
	configfile.setInt32("glcd_time_x_position", t.glcd_time_x_position);
	configfile.setInt32("glcd_time_y_position", t.glcd_time_y_position);
	configfile.setInt32("glcd_icons_percent", t.glcd_icons_percent);
	configfile.setInt32("glcd_icons_y_position", t.glcd_icons_y_position);
	configfile.setInt32("glcd_icon_ecm_x_position", t.glcd_icon_ecm_x_position);
	configfile.setInt32("glcd_icon_cam_x_position", t.glcd_icon_cam_x_position);
	configfile.setInt32("glcd_icon_txt_x_position", t.glcd_icon_txt_x_position);
	configfile.setInt32("glcd_icon_dd_x_position", t.glcd_icon_dd_x_position);
	configfile.setInt32("glcd_icon_mute_x_position", t.glcd_icon_mute_x_position);
	configfile.setInt32("glcd_icon_timer_x_position", t.glcd_icon_timer_x_position);
	configfile.setInt32("glcd_icon_rec_x_position", t.glcd_icon_rec_x_position);
	configfile.setInt32("glcd_icon_ts_x_position", t.glcd_icon_ts_x_position);
	configfile.setBool("glcd_weather", t.glcd_weather);
	configfile.setInt32("glcd_weather_curr_x_position", t.glcd_weather_curr_x_position);
	configfile.setInt32("glcd_weather_next_x_position", t.glcd_weather_next_x_position);
	configfile.setInt32("glcd_weather_y_position", t.glcd_weather_y_position);
	configfile.setInt32("glcd_standby_clock_digital_y_position", t.glcd_standby_clock_digital_y_position);
	configfile.setInt32("glcd_standby_clock_simple_size", t.glcd_standby_clock_simple_size);
	configfile.setInt32("glcd_standby_clock_simple_y_position", t.glcd_standby_clock_simple_y_position);
	configfile.setInt32("glcd_standby_weather_curr_x_position", t.glcd_standby_weather_curr_x_position);
	configfile.setInt32("glcd_standby_weather_next_x_position", t.glcd_standby_weather_next_x_position);
	configfile.setInt32("glcd_standby_weather_y_position", t.glcd_standby_weather_y_position);
	configfile.setInt32("glcd_position_settings", t.glcd_position_settings);
}

void CGLCDThemes::getTheme(CConfigFile &configfile)
{
	t.glcd_foreground_color_red = configfile.getInt32("glcd_foreground_color_red", 0xff);
	t.glcd_foreground_color_green = configfile.getInt32("glcd_foreground_color_green", 0xff);
	t.glcd_foreground_color_blue = configfile.getInt32("glcd_foreground_color_blue", 0xff);
	t.glcd_background_color_red = configfile.getInt32("glcd_background_color_red", 0x00);
	t.glcd_background_color_green = configfile.getInt32("glcd_background_color_green", 0x00);
	t.glcd_background_color_blue = configfile.getInt32("glcd_background_color_blue", 0x00);
	t.glcd_background_image = configfile.getString("glcd_background_image", "");
	t.glcd_font = configfile.getString("glcd_font", "");
	t.glcd_channel_percent = configfile.getInt32("glcd_channel_percent", 25);
	t.glcd_channel_align = configfile.getInt32("glcd_channel_align", 2);
	t.glcd_channel_x_position = configfile.getInt32("glcd_channel_x_position", 0);
	t.glcd_channel_y_position = configfile.getInt32("glcd_channel_y_position", 60);
	t.glcd_logo = configfile.getBool("glcd_logo", true);
	t.glcd_logo_percent = configfile.getInt32("glcd_logo_percent", 25);
	t.glcd_logo_x_position = configfile.getInt32("glcd_logo_x_position", 0);
	t.glcd_logo_y_position = configfile.getInt32("glcd_logo_y_position", 60);
	t.glcd_epg_percent = configfile.getInt32("glcd_epg_percent", 15);
	t.glcd_epg_align = configfile.getInt32("glcd_epg_align", 2);
	t.glcd_epg_x_position = configfile.getInt32("glcd_epg_x_position", 0);
	t.glcd_epg_y_position = configfile.getInt32("glcd_epg_y_position", 150);
	t.glcd_start = configfile.getBool("glcd_start", false);
	t.glcd_start_percent = configfile.getInt32("glcd_start_percent", 0);
	t.glcd_start_x_position = configfile.getInt32("glcd_start_x_position", 0);
	t.glcd_start_y_position = configfile.getInt32("glcd_start_y_position", 0);
	t.glcd_start_align = configfile.getInt32("glcd_start_align", 0);
	t.glcd_end = configfile.getBool("glcd_end", false);
	t.glcd_end_percent = configfile.getInt32("glcd_end_percent", 0);
	t.glcd_end_align = configfile.getInt32("glcd_end_align", 0);
	t.glcd_end_x_position = configfile.getInt32("glcd_end_x_position", 0);
	t.glcd_end_y_position = configfile.getInt32("glcd_end_y_position", 0);
	t.glcd_duration = configfile.getBool("glcd_duration", false);
	t.glcd_duration_percent = configfile.getInt32("glcd_duration_percent", 0);
	t.glcd_duration_align = configfile.getInt32("glcd_duration_align", 0);
	t.glcd_duration_x_position = configfile.getInt32("glcd_duration_x_position", 0);
	t.glcd_duration_y_position = configfile.getInt32("glcd_duration_y_position", 0);
	t.glcd_progressbar = configfile.getBool("glcd_progressbar", false);
	t.glcd_progressbar_color_red = configfile.getInt32("glcd_progressbar_color_red", 0x62);
	t.glcd_progressbar_color_green = configfile.getInt32("glcd_progressbar_color_green", 0x62);
	t.glcd_progressbar_color_blue = configfile.getInt32("glcd_progressbar_color_blue", 0x62);
	t.glcd_progressbar_percent = configfile.getInt32("glcd_progressbar_percent", 0);
	t.glcd_progressbar_width = configfile.getInt32("glcd_progressbar_width", 0);
	t.glcd_progressbar_x_position = configfile.getInt32("glcd_progressbar_x_position", 0);
	t.glcd_progressbar_y_position = configfile.getInt32("glcd_progressbar_y_position", 0);
	t.glcd_time = configfile.getBool("glcd_time", false);
	t.glcd_time_percent = configfile.getInt32("glcd_time_percent", 0);
	t.glcd_time_align = configfile.getInt32("glcd_time_align", 0);
	t.glcd_time_x_position = configfile.getInt32("glcd_time_x_position", 0);
	t.glcd_time_y_position = configfile.getInt32("glcd_time_y_position", 0);
	t.glcd_icons_percent = configfile.getInt32("glcd_icons_percent", 0);
	t.glcd_icons_y_position = configfile.getInt32("glcd_icons_y_position", 0);
	t.glcd_icon_ecm_x_position = configfile.getInt32("glcd_icon_ecm_x_position", 0);
	t.glcd_icon_cam_x_position = configfile.getInt32("glcd_icon_cam_x_position", 0);
	t.glcd_icon_txt_x_position = configfile.getInt32("glcd_icon_txt_x_position", 0);
	t.glcd_icon_dd_x_position = configfile.getInt32("glcd_icon_dd_x_position", 0);
	t.glcd_icon_mute_x_position = configfile.getInt32("glcd_icon_mute_x_position", 0);
	t.glcd_icon_timer_x_position = configfile.getInt32("glcd_icon_timer_x_position", 0);
	t.glcd_icon_rec_x_position = configfile.getInt32("glcd_icon_rec_x_position", 0);
	t.glcd_icon_ts_x_position = configfile.getInt32("glcd_icon_ts_x_position", 0);
	t.glcd_weather = configfile.getBool("glcd_weather", false);
	t.glcd_weather_curr_x_position = configfile.getInt32("glcd_weather_curr_x_position", 0);
	t.glcd_weather_next_x_position = configfile.getInt32("glcd_weather_next_x_position", 0);
	t.glcd_weather_y_position = configfile.getInt32("glcd_weather_y_position", 0);
	t.glcd_standby_clock_digital_y_position = configfile.getInt32("glcd_standby_clock_digital_y_position", 0);
	t.glcd_standby_clock_simple_size = configfile.getInt32("glcd_standby_clock_simple_size", 0);
	t.glcd_standby_clock_simple_y_position = configfile.getInt32("glcd_standby_clock_simple_y_position", 0);
	t.glcd_standby_weather_curr_x_position = configfile.getInt32("glcd_standby_weather_curr_x_position", 0);
	t.glcd_standby_weather_next_x_position = configfile.getInt32("glcd_standby_weather_next_x_position", 0);
	t.glcd_standby_weather_y_position = configfile.getInt32("glcd_standby_weather_y_position", 0);
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
