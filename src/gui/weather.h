/*
	Copyright (C) 2017, 2018, 2019 TangoCash

	“Powered by Dark Sky” https://darksky.net/poweredby/

	License: GPLv2

	This program is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation;

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program. If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef __WEATHER__
#define __WEATHER__

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string>
#include <time.h>
#include <vector>

#include "system/settings.h"
#include "system/helpers.h"

#include <gui/components/cc.h>

struct current_data
{
	time_t timestamp;
	std::string icon;
	float temperature;
	float windSpeed;
	int windBearing;

	current_data():
		timestamp(0),
		icon(""),
		temperature(0),
		windSpeed(0),
		windBearing(0)
	{}
};

typedef struct
{
	time_t timestamp;
	std::string icon;
	float temperatureMin;
	float temperatureMax;
	float windSpeed;
	int windBearing;
} forecast_data;

class CWeather
{
	private:
		std::string coords;
		std::string city;
		std::string timezone;
		current_data current;
		std::vector<forecast_data> v_forecast;
		CComponentsForm *form;
		std::string key;
		bool GetWeatherDetails();
		time_t last_time;

	public:
		static CWeather *getInstance();
		CWeather();
		~CWeather();
		bool checkUpdate(bool forceUpdate = false);
		void setCoords(std::string new_coords, std::string new_city = "Unknown");

		std::string getCity()
		{
			return city;
		};
#if 0
		std::string getCurrentTimestamp()
		{
			return to_string((int)(current.timestamp));
		};
#endif
		time_t getCurrentTimestamp()
		{
			return current.timestamp;
		};
		std::string getCurrentTemperature()
		{
			return to_string((int)(current.temperature + 0.5));
		};
		std::string getCurrentWindSpeed()
		{
			return to_string(current.windSpeed);
		};
		std::string getCurrentWindBearing()
		{
			return to_string(current.windBearing);
		};
		std::string getCurrentIcon()
		{
			return ICONSDIR"/weather/" + current.icon;
		};
		std::string getForecastTemperatureMin(int i = 0)
		{
			return to_string((int)(v_forecast[i].temperatureMin + 0.5));
		};
		std::string getForecastTemperatureMax(int i = 0)
		{
			return to_string((int)(v_forecast[i].temperatureMax + 0.5));
		};
		std::string getForecastWindSpeed(int i = 0)
		{
			return to_string(v_forecast[i].windSpeed);
		};
		std::string getForecastWindBearing(int i = 0)
		{
			return to_string(v_forecast[i].windBearing);
		};
		std::string getForecastIcon(int i = 0)
		{
			return ICONSDIR"/weather/" + v_forecast[i].icon;
		};

		void show(int x = 50, int y = 50);
		void hide();
};

#endif
