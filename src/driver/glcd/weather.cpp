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
	Foundation, see <http://www.gnu.org/licenses/>.
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

int  weather_percent;
int  weather_fontsize;

int  standby_weather_percent;
int  standby_weather_fontsize;

void WeatherUpdateFonts()
{
	cGLCD *cglcd = cGLCD::getInstance();
	SNeutrinoGlcdTheme &t = g_settings.glcd_theme;

	weather_percent = std::min(t.glcd_weather_percent, 100);
	int weather_fontsize_new = weather_percent * cglcd->lcd->Height() / 100;

	standby_weather_percent = std::min(t.glcd_standby_weather_percent, 100);
	int standby_weather_fontsize_new = standby_weather_percent * cglcd->lcd->Height() / 100;

	if (!fonts_initialized || (weather_fontsize_new != weather_fontsize))
	{
		weather_fontsize = weather_fontsize_new;
		if (!font_temperature.LoadFT2(t.glcd_font, "UTF-8", weather_fontsize))
		{
			font_temperature.LoadFT2(settingsText(g_settings.font_file), "UTF-8", weather_fontsize);
		}
	}

	if (!fonts_initialized || (standby_weather_fontsize_new != standby_weather_fontsize))
	{
		standby_weather_fontsize = standby_weather_fontsize_new;
		if (!font_temperature_standby.LoadFT2(t.glcd_font, "UTF-8", standby_weather_fontsize))
		{
			font_temperature_standby.LoadFT2(settingsText(g_settings.font_file), "UTF-8", standby_weather_fontsize);
		}
	}

	fonts_initialized = true;
}

void RenderWeather(bool standby)
{
	SNeutrinoGlcdTheme &t = g_settings.glcd_theme;

	int ctx, cix, ntx, nix, y;

	if (!standby)
	{
		ctx = t.glcd_weather_curr_temp_x_position;
		cix = t.glcd_weather_curr_icon_x_position;
		ntx = t.glcd_weather_next_temp_x_position;
		nix = t.glcd_weather_next_icon_x_position;
		y = t.glcd_weather_y_position;
	}
	else
	{
		ctx = t.glcd_standby_weather_curr_temp_x_position;
		cix = t.glcd_standby_weather_curr_icon_x_position;
		ntx = t.glcd_standby_weather_next_temp_x_position;
		nix = t.glcd_standby_weather_next_icon_x_position;
		y = t.glcd_standby_weather_y_position;
	}

	int forecast = CWeather::getLaterForecastIndex();

	cGLCD *cglcd = cGLCD::getInstance();

	if (g_settings.weather_enabled)
	{
		CWeather *weather = CWeather::getInstance();

		/* checkUpdate fetches for whichever caller comes first once the cycle is up
		   and answers false to every other one, lcd4l among them. A copy taken only
		   on true stayed at the old values for up to a cycle while the weather and
		   the other display had the new ones, so it only starts the fetch, and the
		   values are read from the weather every time. */
		weather->checkUpdate(ForceUpdate);

		// Nothing before the first fetch that came back, or every value would read 0.
		if (weather->getCurrentTimestamp() <= 0)
			return;

		std::string current_wcity = weather->getCity();
		std::string current_wtimestamp = to_string((int)weather->getCurrentTimestamp());
		std::string current_wtemp = weather->getCurrentTemperature();
		std::string current_wwind = weather->getCurrentWindSpeed();
		std::string current_wicon = weather->getCurrentIcon();

		std::string next_wcity = weather->getCity();
		std::string next_wtimestamp = to_string((int)weather->getForecastWeekday(forecast));
		//std::string next_wtemp = weather->getForecastTemperatureMin(forecast);
		//next_wtemp += "|" + weather->getForecastTemperatureMax(forecast);
		std::string next_wtemp = weather->getForecastTemperatureMax(forecast);
		std::string next_wwind = weather->getForecastWindBearing(forecast);
		std::string next_wicon = weather->getForecastIcon(forecast);

		if ((current_wicon != "") && cix)
		{
			if (!standby)
				cglcd->imageShow(current_wicon, cix, y, weather_fontsize, weather_fontsize, false, false, false, false, false);
			else
				cglcd->imageShow(current_wicon, cix, y, standby_weather_fontsize, standby_weather_fontsize, false, false, false, false, false);
		}
		if ((current_wtemp != "") && ctx)
		{
			//current_wtemp += "°";
			WeatherUpdateFonts();
			if (!standby)
			{
				cglcd->bitmap->DrawText(ctx, y, cglcd->bitmap->Width() - 1, current_wtemp,
					&font_temperature, cglcd->ColorConvert3to1(t.glcd_foreground_color_red, t.glcd_foreground_color_green, t.glcd_foreground_color_blue), GLCD::cColor::Transparent);
			}
			else
			{
				cglcd->bitmap->DrawText(ctx, y, cglcd->bitmap->Width() - 1, current_wtemp,
					&font_temperature_standby, cglcd->ColorConvert3to1(t.glcd_foreground_color_red, t.glcd_foreground_color_green, t.glcd_foreground_color_blue), GLCD::cColor::Transparent);
			}
		}

		if ((next_wicon != "") && nix)
		{
			if (!standby)
				cglcd->imageShow(next_wicon, nix, y, weather_fontsize, weather_fontsize, false, false, false, false, false);
			else
				cglcd->imageShow(next_wicon, nix, y, standby_weather_fontsize, standby_weather_fontsize, false, false, false, false, false);
		}
		if ((next_wtemp != "") && ntx)
		{
			//next_wtemp += "°";
			int offset;
			WeatherUpdateFonts();
			if (!standby)
			{
				offset = std::max(0, font_temperature.Width("88") - font_temperature.Width(next_wtemp));
				cglcd->bitmap->DrawText(ntx + offset, y, cglcd->bitmap->Width() - 1, next_wtemp,
					&font_temperature, cglcd->ColorConvert3to1(t.glcd_foreground_color_red, t.glcd_foreground_color_green, t.glcd_foreground_color_blue), GLCD::cColor::Transparent);
			}
			else
			{
				offset = std::max(0, font_temperature_standby.Width("88") - font_temperature_standby.Width(next_wtemp));
				cglcd->bitmap->DrawText(ntx + offset, y, cglcd->bitmap->Width() - 1, next_wtemp,
					&font_temperature_standby, cglcd->ColorConvert3to1(t.glcd_foreground_color_red, t.glcd_foreground_color_green, t.glcd_foreground_color_blue), GLCD::cColor::Transparent);
			}
		}
	}
}

void ShowWeather(bool standby)
{
	RenderWeather(standby);

	if (ForceUpdate)
		ForceUpdate = false;
}
