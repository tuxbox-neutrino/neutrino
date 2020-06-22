/*
        weather  -   DBoxII-Project

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


#include <config.h>
#include <cstdio>
#include "weather.h"
#include <gui/weather.h>
#include <system/helpers.h>

enum weathers
{
        CLEAR_DAY = 0,
        CLEAR_NIGHT = 1,
	CLOUDY = 2,
        FOG = 3,
        PARTLY_CLOUDY_DAY = 4,
	PARTLY_CLOUDY_NIGHT = 5,
	RAIN = 6,
	SLEET = 7,
	SNOW = 8,
	WIND = 9,
	//WEATHER_UNKNOWN = 10
};

const char * const weather_name[] =
{
	"clear-day",
        "clear-night",
        "cloudy",
        "fog",
        "partly-cloudy-day",
	"partly-cloudy-night",
	"rain",
	"sleet",
	"snow",
	"wind",
	//"unknown"
};
#define LCD_NUMBER_OF_WEATHERS (sizeof(weather_name)/sizeof(weather_name[0]))

const char * const weather_path[] =
{
	ICONSDIR_VAR "/oled/weather/",
	ICONSDIR "/oled/weather/"
};
#define NUMBER_OF_PATHS (sizeof(weather_path)/sizeof(weather_path[0]))

static bool ForceUpdate = true;
static bool fonts_initialized = false;

GLCD::cFont font_temperature;

static std::string weather[LCD_NUMBER_OF_WEATHERS] = {""};

static std::string st_current_wcity = "";
static std::string st_current_wtimestamp = "";
static std::string st_current_wtemp = "";
static std::string st_current_wwind = "";
static std::string st_current_wicon = "";

static std::string st_next_wcity = "";
static std::string st_next_wtimestamp = "";
static std::string st_next_wtemp = "";
static std::string st_next_wwind = "";
static std::string st_next_wicon = "";

void InitWeather(void)
{
	for (unsigned int i = 0; i < LCD_NUMBER_OF_WEATHERS; i++)
	{
		std::string weather_file;
		for (unsigned int j = 0; j < NUMBER_OF_PATHS; j++)
		{
			std::string file_jpg = weather_path[j];
			file_jpg += weather_name[i];
			file_jpg += ".jpg";
			if (file_exists(file_jpg.c_str()))
			{
				weather_file = file_jpg;
				goto found;
			}
			std::string file_jpeg = weather_path[j];
			file_jpeg += weather_name[i];
			file_jpeg += ".jpeg";
			if (file_exists(file_jpeg.c_str()))
			{
				weather_file = file_jpeg;
				goto found;
			}
			std::string file_png = weather_path[j];
			file_png += weather_name[i];
			file_png += ".png";
			if (file_exists(file_png.c_str()))
			{
				weather_file = file_png;
				goto found;
			}
			std::string file_bmp = weather_path[j];
			file_bmp += weather_name[i];
			file_bmp += ".bmp";
			if (file_exists(file_bmp.c_str()))
			{
				weather_file = file_bmp;
				goto found;
			}
			std::string file_gif = weather_path[j];
			file_gif += weather_name[i];
			file_gif += ".gif";
			if (file_exists(file_gif.c_str()))
			{
				weather_file = file_gif;
				goto found;
			}
		}
found:
		printf("[%s:%s] found file: %s\n", __file__, __func__, weather_file.c_str());
		weather[i] += std::string(weather_file);
	}
	printf("[%s:%s] finish initialization\n", __file__, __func__);
}

void WeatherUpdateFonts(void)
{
	cGLCD *cglcd = cGLCD::getInstance();
	SNeutrinoGlcdTheme &t = g_settings.glcd_theme;
	int fontsize_temperature = 0;
	int percent_temperature = std::min(24, 100);
	int fontsize_temperature_new = percent_temperature * cglcd->lcd->Height() / 100;
	if (!fonts_initialized || (fontsize_temperature_new != fontsize_temperature)) {
		fontsize_temperature = fontsize_temperature_new;
		if (!font_temperature.LoadFT2(/*t.glcd_font*/FONTDIR "/pakenham.ttf", "UTF-8", fontsize_temperature)) {
			t.glcd_font = FONTDIR "/pakenham.ttf";
			font_temperature.LoadFT2(t.glcd_font, "UTF-8", fontsize_temperature);
		}
	}
	fonts_initialized = true;
}

int WeatherNameToNumber(std::string name)
{
	std::map<std::string, int> weather_name
	{
		{ "clear-day", 0 },
		{ "clear-night", 1 },
		{ "cloudy", 2 },
		{ "fog", 3 },
		{ "partly-cloudy-day", 4 },
		{ "partly-cloudy-night", 5 },
		{ "rain", 6 },
		{ "sleet", 7 },
		{ "snow", 8 },
		{ "wind", 9 },
		//{ "unknown", 10 },
	};

	const auto iter = weather_name.find(name);

	if (iter != weather_name.cend())
		return iter->second;
}

void RenderWeather(int cx, int cy, int nx, int ny, bool standby)
{
	int forecast = 0;

	std::string current_wcity = "";
	std::string current_wtimestamp = "";
	std::string current_wtemp = "";
	std::string current_wwind = "";
	std::string current_wicon = "";

	std::string next_wcity = "";
	std::string next_wtimestamp = "";
	std::string next_wtemp = "";
	std::string next_wwind = "";
	std::string next_wicon = "";

	cGLCD *cglcd = cGLCD::getInstance();
	SNeutrinoGlcdTheme &t = g_settings.glcd_theme;
	if (g_settings.weather_enabled)
	{
		if (CWeather::getInstance()->checkUpdate(ForceUpdate))
		{
			current_wcity = st_current_wcity = CWeather::getInstance()->getCity();
			current_wtimestamp = st_current_wtimestamp = to_string((int)CWeather::getInstance()->getCurrentTimestamp());
			current_wtemp = st_current_wtemp = CWeather::getInstance()->getCurrentTemperature();
			current_wwind = st_current_wwind = CWeather::getInstance()->getCurrentWindSpeed();
			current_wicon = st_current_wicon = CWeather::getInstance()->getCurrentIconOnlyName();

			next_wcity = st_next_wcity = CWeather::getInstance()->getCity();
			next_wtimestamp = st_next_wtimestamp = to_string((int)CWeather::getInstance()->getForecastWeekday(forecast));
			next_wtemp = st_next_wtemp = CWeather::getInstance()->getForecastTemperatureMin(forecast);
			next_wtemp = st_next_wtemp += "|" + CWeather::getInstance()->getForecastTemperatureMax(forecast);
			next_wwind = st_next_wwind = CWeather::getInstance()->getForecastWindBearing(forecast);
			next_wicon = st_next_wicon = CWeather::getInstance()->getForecastIconOnlyNane(forecast);
		}
		else
		{
			current_wcity = st_current_wcity;
			current_wtimestamp = st_current_wtimestamp;
			current_wtemp = st_current_wtemp;
			current_wwind = st_current_wwind;
			current_wicon = st_current_wicon;

			next_wcity = st_next_wcity;
			next_wtimestamp = st_next_wtimestamp;
			next_wtemp = st_next_wtemp;
			next_wwind = st_next_wwind;
			next_wicon = st_next_wicon;
		}

		if (current_wicon != "") {
			if (!standby)
				cglcd->imageShow(weather[WeatherNameToNumber(current_wicon)], cx, cy, 64, 64, false, false, false, false, false);
			else
				cglcd->imageShow(weather[WeatherNameToNumber(current_wicon)], cx, cy, 0, 0, false, false, false, false, false);
		}
		if (current_wtemp != "") {
			current_wtemp += "°";
			WeatherUpdateFonts();
			cglcd->bitmap->DrawText(170, 240, cglcd->bitmap->Width() - 1, current_wtemp,
				&font_temperature, cglcd->ColorConvert3to1(t.glcd_color_fg_red, t.glcd_color_fg_green, t.glcd_color_fg_blue), GLCD::cColor::Transparent);
		}
		if (next_wicon != "") {
			if (!standby)
				cglcd->imageShow(weather[WeatherNameToNumber(next_wicon)], nx, ny, 64, 64, false, false, false, false, false);
			else
				cglcd->imageShow(weather[WeatherNameToNumber(next_wicon)], nx, ny, 0, 0, false, false, false, false, false);
		}
		if (next_wtemp != "") {
			next_wtemp += "°";
			WeatherUpdateFonts();
			cglcd->bitmap->DrawText(270, 240, cglcd->bitmap->Width() - 1, next_wtemp,
				&font_temperature, cglcd->ColorConvert3to1(t.glcd_color_fg_red, t.glcd_color_fg_green, t.glcd_color_fg_blue), GLCD::cColor::Transparent);
		}
	}
}

void ShowWeather(bool standby)
{
	SNeutrinoGlcdTheme &t = g_settings.glcd_theme;

	if (!standby) {
		RenderWeather(t.glcd_weather_x_position_current, t.glcd_weather_y_position, t.glcd_weather_x_position_next, t.glcd_weather_y_position, standby);
	} else {
		RenderWeather(t.glcd_weather_x_position_current_standby, t.glcd_weather_y_position_standby, t.glcd_weather_x_position_next_standby, t.glcd_weather_y_position_standby, standby);
	}

	if (ForceUpdate)
		ForceUpdate = false;
}
