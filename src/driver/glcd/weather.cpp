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

static bool ForceUpdate = true;
static bool fonts_initialized = false;

GLCD::cFont font_temperature;
GLCD::cFont font_temperature_standby;

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

int  weather_percent;
int  weather_fontsize;

int  standby_weather_percent;
int  standby_weather_fontsize;

void InitWeather()
{
	WeatherUpdateFonts();
}

void WeatherUpdateFonts()
{
	cGLCD *cglcd = cGLCD::getInstance();
	SNeutrinoGlcdTheme &t = g_settings.glcd_theme;

	weather_percent = std::min(t.glcd_weather_percent, 100);
	int weather_fontsize_new = weather_percent * cglcd->lcd->Height() / 100;

	standby_weather_percent = std::min(t.glcd_standby_weather_percent, 100);
	int standby_weather_fontsize_new = standby_weather_percent * cglcd->lcd->Height() / 100;

	if (!fonts_initialized || (weather_fontsize_new != weather_fontsize)) {
		weather_fontsize = weather_fontsize_new;
		if (!font_temperature.LoadFT2(t.glcd_font, "UTF-8", weather_fontsize)) {
			font_temperature.LoadFT2(g_settings.font_file, "UTF-8", weather_fontsize);
		}
	}

	if (!fonts_initialized || (standby_weather_fontsize_new != standby_weather_fontsize)) {
		standby_weather_fontsize = standby_weather_fontsize_new;
		if (!font_temperature_standby.LoadFT2(t.glcd_font, "UTF-8", standby_weather_fontsize)) {
			font_temperature_standby.LoadFT2(g_settings.font_file, "UTF-8", standby_weather_fontsize);
		}
	}

	fonts_initialized = true;
}

void RenderWeather(int cx, int cy, int nx, int ny, bool standby)
{
	int forecast = 0; // 0 is current day

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
			current_wicon = st_current_wicon = CWeather::getInstance()->getCurrentIcon();

			next_wcity = st_next_wcity = CWeather::getInstance()->getCity();
			next_wtimestamp = st_next_wtimestamp = to_string((int)CWeather::getInstance()->getForecastWeekday(forecast));
			//next_wtemp = st_next_wtemp = CWeather::getInstance()->getForecastTemperatureMin(forecast);
			//next_wtemp = st_next_wtemp += "|" + CWeather::getInstance()->getForecastTemperatureMax(forecast);
			next_wtemp = st_next_wtemp = CWeather::getInstance()->getForecastTemperatureMax(forecast);
			next_wwind = st_next_wwind = CWeather::getInstance()->getForecastWindBearing(forecast);
			next_wicon = st_next_wicon = CWeather::getInstance()->getForecastIcon(forecast);
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
				cglcd->imageShow(current_wicon, cx, cy, weather_fontsize, weather_fontsize, false, false, false, false, false);
			else
				cglcd->imageShow(current_wicon, cx, cy, standby_weather_fontsize, standby_weather_fontsize, false, false, false, false, false);
		}
		if (current_wtemp != "") {
			current_wtemp += "°";
			WeatherUpdateFonts();
			if (!standby)
				cglcd->bitmap->DrawText(cx + 5 + weather_fontsize, cy + ((weather_fontsize - font_temperature.TotalHeight()) / 2 ), cglcd->bitmap->Width() - 1, current_wtemp,
					&font_temperature, cglcd->ColorConvert3to1(t.glcd_foreground_color_red, t.glcd_foreground_color_green, t.glcd_foreground_color_blue), GLCD::cColor::Transparent);
			else
				cglcd->bitmap->DrawText(cx + 5 + standby_weather_fontsize, cy + ((standby_weather_fontsize - font_temperature_standby.TotalHeight()) / 2 ), cglcd->bitmap->Width() - 1, current_wtemp,
					&font_temperature_standby, cglcd->ColorConvert3to1(t.glcd_foreground_color_red, t.glcd_foreground_color_green, t.glcd_foreground_color_blue), GLCD::cColor::Transparent);
		}
		if (next_wicon != "") {
			if (!standby)
				cglcd->imageShow(next_wicon, nx, ny, weather_fontsize, weather_fontsize, false, false, false, false, false);
			else
				cglcd->imageShow(next_wicon, nx, ny, standby_weather_fontsize, standby_weather_fontsize, false, false, false, false, false);
		}
		if (next_wtemp != "") {
			next_wtemp += "°";
			WeatherUpdateFonts();
			if (!standby)
				cglcd->bitmap->DrawText(nx + 5 + weather_fontsize, ny + ((weather_fontsize - font_temperature.TotalHeight()) / 2 ), cglcd->bitmap->Width() - 1, next_wtemp,
					&font_temperature, cglcd->ColorConvert3to1(t.glcd_foreground_color_red, t.glcd_foreground_color_green, t.glcd_foreground_color_blue), GLCD::cColor::Transparent);
			else
				cglcd->bitmap->DrawText(nx + 5 + standby_weather_fontsize, ny + ((standby_weather_fontsize - font_temperature_standby.TotalHeight()) / 2 ), cglcd->bitmap->Width() - 1, next_wtemp,
					&font_temperature_standby, cglcd->ColorConvert3to1(t.glcd_foreground_color_red, t.glcd_foreground_color_green, t.glcd_foreground_color_blue), GLCD::cColor::Transparent);
		}
	}
}

void ShowWeather(bool standby)
{
	SNeutrinoGlcdTheme &t = g_settings.glcd_theme;

	if (!standby) {
		RenderWeather(t.glcd_weather_curr_x_position, t.glcd_weather_y_position, t.glcd_weather_next_x_position, t.glcd_weather_y_position, standby);
	} else {
		RenderWeather(t.glcd_standby_weather_curr_x_position, t.glcd_standby_weather_y_position, t.glcd_standby_weather_next_x_position, t.glcd_standby_weather_y_position, standby);
	}

	if (ForceUpdate)
		ForceUpdate = false;
}
