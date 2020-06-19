/*
        digital clock  -   DBoxII-Project

        Copyright (C) 2001 Steffen Hehn 'McClean',
                      2003 thegoodguy

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
#include <driver/digitalclock.h>
#include <system/helpers.h>

#include <driver/pictureviewer/pictureviewer.h>

enum digits
{
        TIME_ZERO = 0,
        TIME_ONE = 1,
	TIME_TWO = 2,
        TIME_THREE = 3,
        TIME_FOUR = 4,
	TIME_FIVE = 5,
	TIME_SIX = 6,
	TIME_SEVEN = 7,
	TIME_EIGHT = 8,
	TIME_NINE = 9,
	TIME_DOTS = 10
};

const char * const digit_name[LCD_NUMBER_OF_DIGITS] =
{
	"time_zero",
        "time_one",
        "time_two",
        "time_three",
        "time_four",
	"time_five",
	"time_six",
	"time_seven",
	"time_eight",
	"time_nine",
	"time_dots"
};

#define NUMBER_OF_PATHS 2
const char * const digit_path[NUMBER_OF_PATHS] =
{
        LCDDIR_VAR "/oled/clock/",
        DATADIR "/oled/clock/"
};

static std::string digit[LCD_NUMBER_OF_DIGITS] = {""};

void InitDigitalClock(void)
{
	for (int i = 0; i < LCD_NUMBER_OF_DIGITS; i++)
	{
		std::string digit_file;
		for (int j = 0; j < NUMBER_OF_PATHS; j++)
		{
			std::string file_jpg = digit_path[j];
			file_jpg += digit_name[i];
			file_jpg += ".jpg";
			if (file_exists(file_jpg.c_str()))
			{
				digit_file = file_jpg;
				goto found;
			}
			std::string file_jpeg = digit_path[j];
			file_jpeg += digit_name[i];
			file_jpeg += ".jpeg";
			if (file_exists(file_jpeg.c_str()))
			{
				digit_file = file_jpeg;
				goto found;
			}
			std::string file_png = digit_path[j];
			file_png += digit_name[i];
			file_png += ".png";
			if (file_exists(file_png.c_str()))
			{
				digit_file = file_png;
				goto found;
			}
			std::string file_bmp = digit_path[j];
			file_bmp += digit_name[i];
			file_bmp += ".bmp";
			if (file_exists(file_bmp.c_str()))
			{
				digit_file = file_bmp;
				goto found;
			}
			std::string file_gif = digit_path[j];
			file_gif += digit_name[i];
			file_gif += ".gif";
			if (file_exists(file_gif.c_str()))
			{
				digit_file = file_gif;
				goto found;
			}
		}
found:
		printf("[%s:%s] found file: %s\n", __file__, __func__, digit_file.c_str());
		digit[i] += std::string(digit_file);
	}
	printf("[%s:%s] finish initialization\n", __file__, __func__);
}

void RenderTimeDigit(int _digit, int x, int y)
{
	cGLCD *cglcd = cGLCD::getInstance();
	if (g_settings.glcd_standby_weather)
		cglcd->imageShow(digit[_digit], x, y, 0, 0, false, false, false, false, false);
	else
		cglcd->imageShow(digit[_digit], x, y, 0, 0, false, false, false, false, true);
}

void RenderDots(int x, int y)
{
	cGLCD *cglcd = cGLCD::getInstance();
	if (g_settings.glcd_standby_weather)
		cglcd->imageShow(digit[TIME_DOTS], x, y, 0, 0, false, false, false, true, false);
	else
		cglcd->imageShow(digit[TIME_DOTS], x, y, 0, 0, false, false, false, true, true);
}

void ShowDigitalClock(int hour, int minute)
{
	cGLCD *cglcd = cGLCD::getInstance();
	SNeutrinoGlcdTheme &t = g_settings.glcd_theme;
	int y = g_settings.glcd_standby_weather ? t.glcd_digital_clock_y_position : cglcd->bitmap->Height() / 2;

	int a =  10;
	int b = 117;
	int c = cglcd->bitmap->Width() / 2; //center dots
	int d = 258;
	int e = 365;

	RenderTimeDigit(hour/10, a, y);
	RenderTimeDigit(hour%10, b, y);
	RenderDots(c ,(g_settings.glcd_standby_weather ? (y + 35) : y));
	RenderTimeDigit(minute/10, d, y);
	RenderTimeDigit(minute%10, e, y);
}
