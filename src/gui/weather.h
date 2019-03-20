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

typedef struct
{
	std::string wicon;
	float min_temp;
	float max_temp;
	time_t timestamp;
} forecast_data;

class CWeather
{
	private:
		std::string coords;
		std::string city;
		std::string timezone;
		std::string act_wicon;
		float act_temp;
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
		void show(int x = 50, int y = 50);
		void hide();
		std::string getActTemp()
		{
			return to_string((int)(act_temp + 0.5)) + "°C";
		};
		std::string getActIcon()
		{
			return ICONSDIR"/weather/" + act_wicon;
		};
};

#endif
