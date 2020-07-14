/*
        simple clock  -   DBoxII-Project

        Copyright (C) 2018 redblue

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
#include <cstdio>
#include "simpleclock.h"

GLCD::cFont font_time_standby;

void InitSimpleClock(void)
{
	printf("[%s:%s] finish initialization\n", __file__, __func__);
}

void SimpleClockUpdateFonts(int mode)
{
	cGLCD *cglcd = cGLCD::getInstance();
	SNeutrinoGlcdTheme &t = g_settings.glcd_theme;

	std::string font = t.glcd_font;

	switch (mode)
	{
		case cGLCD::CLOCK_LCD:
			font = FONTDIR "/oled/lcd.ttf";
			break;
		case cGLCD::CLOCK_LED:
			font = FONTDIR "/oled/led.ttf";
			break;
		case cGLCD::CLOCK_SIMPLE:
		default:
			font = t.glcd_font
			;
	}

	int fontsize_time_standby = 0;
	int percent_time_standby = std::min(t.glcd_standby_clock_simple_size, 100);
	int fontsize_time_standby_new = percent_time_standby * cglcd->lcd->Height() / 100;
	if (fontsize_time_standby_new != fontsize_time_standby) {
		fontsize_time_standby = fontsize_time_standby_new;
		if (!font_time_standby.LoadFT2(font, "UTF-8", fontsize_time_standby)) {
			font_time_standby.LoadFT2(g_settings.font_file, "UTF-8", fontsize_time_standby);
		}
	}
}

void RenderSimpleClock(std::string Time, int x, int y, int mode)
{
	(void) x;
	cGLCD *cglcd = cGLCD::getInstance();
	SNeutrinoGlcdTheme &t = g_settings.glcd_theme;
	SimpleClockUpdateFonts(mode);
	cglcd->bitmap->DrawText(std::max(2,(cglcd->bitmap->Width() - 4 - font_time_standby.Width(Time))/2),
		y, cglcd->bitmap->Width() - 1, Time,
		&font_time_standby, cglcd->ColorConvert3to1(t.glcd_foreground_color_red, t.glcd_foreground_color_green, t.glcd_foreground_color_blue), GLCD::cColor::Transparent);
}

void ShowSimpleClock(std::string Time, int mode)
{
	cGLCD *cglcd = cGLCD::getInstance();
	SNeutrinoGlcdTheme &t = g_settings.glcd_theme;
	int y = g_settings.glcd_standby_weather ? t.glcd_standby_clock_simple_y_position : (cglcd->bitmap->Height() - font_time_standby.Height(Time)) / 2;
	RenderSimpleClock(Time, 255, y, mode);
}
