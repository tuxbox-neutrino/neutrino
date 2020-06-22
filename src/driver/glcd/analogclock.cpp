/*
        analog clock  -   DBoxII-Project

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
#include <math.h>
#include <cstdio>
#include "analogclock.h"
#include <system/helpers.h>

enum files
{
	ANALOG_CLOCK = 0,
	ANALOG_HOUR = 1,
	ANALOG_MIN = 2
};

const char * const file_name[] =
{
	"analog_clock",
	"analog_hour",
	"analog_min"
};
#define LCD_NUMBER_OF_FILES (sizeof(file_name)/sizeof(file_name[0]))

const char * const file_path[] =
{
	ICONSDIR "/oled/clock/",
	ICONSDIR_VAR "/oled/clock/"
};
#define NUMBER_OF_PATHS (sizeof(file_path)/sizeof(file_path[0]))

static std::string file[LCD_NUMBER_OF_FILES] = {""};

void InitAnalogClock(void)
{
	for (unsigned int i = 0; i < LCD_NUMBER_OF_FILES; i++)
	{
		std::string tmp_file;
		for (unsigned int j = 0; j < NUMBER_OF_PATHS; j++)
		{
			std::string file_jpg = file_path[j];
			file_jpg += file_name[i];
			file_jpg += ".jpg";
			if (file_exists(file_jpg.c_str()))
			{
				tmp_file = file_jpg;
				goto found;
			}
			std::string file_jpeg = file_path[j];
			file_jpeg += file_name[i];
			file_jpeg += ".jpeg";
			if (file_exists(file_jpeg.c_str()))
			{
				tmp_file = file_jpeg;
				goto found;
			}
			std::string file_png = file_path[j];
			file_png += file_name[i];
			file_png += ".png";
			if (file_exists(file_png.c_str()))
			{
				tmp_file = file_png;
				goto found;
			}
			std::string file_bmp = file_path[j];
			file_bmp += file_name[i];
			file_bmp += ".bmp";
			if (file_exists(file_bmp.c_str()))
			{
				tmp_file = file_bmp;
				goto found;
			}
			std::string file_gif = file_path[j];
			file_gif += file_name[i];
			file_gif += ".gif";
			if (file_exists(file_gif.c_str()))
			{
				tmp_file = file_gif;
				goto found;
			}
		}
found:
		printf("[%s:%s] found file: %s\n", __file__, __func__, tmp_file.c_str());
		file[i] += std::string(tmp_file);
	}
	printf("[%s:%s] finish initialization\n", __file__, __func__);
}

void RenderClock(int x, int y)
{
	cGLCD *cglcd = cGLCD::getInstance();
	cglcd->imageShow(file[ANALOG_CLOCK], x, y, 0, 0, false, true, false, false, false);
}

void RenderHands(int hour, int min, int sec, int posx, int posy, int hour_size, int min_size, int sec_size)
{
	cGLCD *cglcd = cGLCD::getInstance();

	int time_sec, time_min, time_hour, sec_x, sec_y, min_x, min_y, hour_x, hour_y, dia;
	double pi = 3.1415926535897932384626433832795, sAngleInRad, mAngleInRad, mAngleSave, hAngleInRad;

	time_sec = sec;
	time_min = min;
	time_hour = hour;

	dia = 180;

	sAngleInRad = ((6 * time_sec) * (2 * pi / 360));
	sAngleInRad -= pi / 2;

	sec_x = int((dia * 0.9 * cos(sAngleInRad)));
	sec_y = int((dia * 0.9 * sin(sAngleInRad)));

	mAngleInRad = ((6 * time_min) * (2 * pi / 360));
	mAngleSave = mAngleInRad;
	mAngleInRad -= pi/2;

	min_x = int((dia * 0.7 * cos(mAngleInRad)));
	min_y = int((dia * 0.7 * sin(mAngleInRad)));

	hAngleInRad = ((30 * time_hour) * (2 * pi / 360));
	hAngleInRad += mAngleSave/12;
	hAngleInRad -= pi/2;
	hour_x = int((dia * 0.5 * cos(hAngleInRad)));
	hour_y = int((dia * 0.5 * sin(hAngleInRad)));

	//hour
	for (int i = 0; i <= hour_size; i++)
	{
#if 1
		cglcd->bitmap->DrawLine(posx-i, posy-i, posx + hour_x,posy + hour_y, GLCD::cColor::White);
		cglcd->bitmap->DrawLine(posx+i, posy+i, posx + hour_x,posy + hour_y, GLCD::cColor::White);
#else
		cglcd->bitmap->DrawLine(posx-i, posy-i, posx + hour_x-i,posy + hour_y-i, t.glcd_color_fg);
		cglcd->bitmap->DrawLine(posx+i, posy+i, posx + hour_x+i,posy + hour_y+i, t.glcd_color_fg);
#endif
	}

	//min
	for (int i = 0; i <= min_size; i++)
	{
#if 1
		cglcd->bitmap->DrawLine(posx-i, posy-i, posx + min_x,posy + min_y, GLCD::cColor::White);
		cglcd->bitmap->DrawLine(posx+i, posy+i, posx + min_x,posy + min_y, GLCD::cColor::White);
#else
		cglcd->bitmap->DrawLine(posx-i, posy-i, posx + min_x-i,posy + min_y-i, t.glcd_color_fg);
		cglcd->bitmap->DrawLine(posx+i, posy+i, posx + min_x+i,posy + min_y+i, t.glcd_color_fg);
#endif
	}
}

void ShowAnalogClock(int hour, int min, int sec, int x, int y)
{
	RenderClock(0, 0);
	RenderHands(hour, min, sec, x, y, 5, 3, 1);
}
