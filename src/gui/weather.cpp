/*
	Copyright (C) 2017, 2018, 2019, 2020 TangoCash

	“Powered by OpenWeather” https://openweathermap.org/api/one-call-api

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

#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <errno.h>
#include <fstream>

#include <set>
#include <string>

#include "system/set_threadname.h"
#include "gui/widget/hintbox.h"

#include <driver/screen_max.h>
#include <driver/fontrenderer.h>

#include <global.h>
#include <json/json.h>

#include "weather.h"

#define UPDATE_CYCLE 15 // minutes

CWeather *weather = NULL;

CWeather *CWeather::getInstance()
{
	if (!weather)
		weather = new CWeather();
	return weather;
}

CWeather::CWeather()
{
	key = g_settings.weather_api_key;
	v_forecast.clear();
	last_time = 0;
	coords = "";
	city = "";

	form = NULL;
}

CWeather::~CWeather()
{
	v_forecast.clear();
	hide();
}

void CWeather::setCoords(std::string new_coords, std::string new_city)
{
	if (coords.compare(new_coords))
	{
		coords = new_coords;
		city = new_city;
		checkUpdate(true);
	}
}

bool CWeather::checkUpdate(bool forceUpdate)
{
	time_t current_time = time(NULL);
	if (forceUpdate || (difftime(current_time, last_time) > (UPDATE_CYCLE * 60)))
		return GetWeatherDetails();
	else
		return false;
}

bool CWeather::GetWeatherDetails()
{
	printf("[CWeather]: %s\n", __func__);

	last_time = time(NULL);

	if (!g_settings.weather_enabled)
		return false;

	std::string lat = coords.substr(0,coords.find_first_of(','));
	std::string lon = coords.substr(coords.find_first_of(',')+1);

	std::string data = "https://api.openweathermap.org/data/2.5/onecall?lat=" + lat + "&lon=" + lon + "&units=metric&lang=de&exclude=minutely,hourly,flags,alerts&appid=" + key;
	JSONCPP_STRING answer;
	JSONCPP_STRING formattedErrors;

	double found = 0;

	v_forecast.clear();

	Json::CharReaderBuilder builder;
	Json::CharReader * reader = builder.newCharReader();
	Json::Value DataValues;

	answer.clear();
	if (!getUrl(data, answer))
	{
		delete reader;
		return false;
	}

	bool parsedSuccess = reader->parse(answer.c_str(), answer.c_str() + answer.size(), &DataValues, &formattedErrors);
	delete reader;

	if (!parsedSuccess)
	{
		printf("Failed to parse JSON\n");
		printf("%s\n", formattedErrors.c_str());
		return false;
	}

	found = DataValues["current"].get("dt", 0).asDouble();

	printf("[CWeather]: results found: %lf\n", found);

	if (found > 0)
	{
		timezone = DataValues["timezone"].asString();
		current.timestamp = DataValues["current"].get("dt", 0).asDouble();
		current.temperature = DataValues["current"].get("temp", "").asFloat();
		current.pressure = DataValues["current"].get("pressure", "").asFloat();
		current.humidity = DataValues["current"].get("humidity", "").asFloat();
		current.windSpeed = DataValues["current"].get("wind_speed", "").asFloat();
		current.windBearing = DataValues["current"].get("wind_deg", "").asDouble();
		current.icon = DataValues["current"]["weather"][0].get("icon", "").asString();
		if (current.icon.empty())
			current.icon = "unknown.png";
		else
			current.icon = current.icon + ".png";

		if (current.icon_only_name.empty())
			current.icon_only_name = "unknown";

		printf("[CWeather]: temp in %s (%s): %.1f - %s\n", city.c_str(), timezone.c_str(), current.temperature, current.icon.c_str());

		forecast_data daily_data;
		Json::Value elements = DataValues["daily"];
		for (unsigned int i = 0; i < elements.size(); i++)
		{
			daily_data.timestamp = elements[i].get("dt", 0).asDouble();
			daily_data.weekday = (int)(localtime(&daily_data.timestamp)->tm_wday);
			daily_data.icon = elements[i]["weather"][0].get("icon", "").asString();
			if (daily_data.icon.empty())
				daily_data.icon = "unknown.png";
			else
				daily_data.icon = daily_data.icon + ".png";
			daily_data.temperatureMin = elements[i]["temp"].get("min", "").asFloat();
			daily_data.temperatureMax = elements[i]["temp"].get("max", "").asFloat();
			daily_data.sunriseTime = elements[i].get("sunrise", 0).asDouble();
			daily_data.sunsetTime = elements[i].get("sunset", 0).asDouble();
			daily_data.windSpeed = elements[i].get("wind_speed", 0).asFloat();
			daily_data.windBearing = elements[i].get("wind_deg", 0).asDouble();

			struct tm *timeinfo;
			timeinfo = localtime(&daily_data.timestamp);

			printf("[CWeather]: temp %d.%d.%d: min %.1f - max %.1f -> %s\n", timeinfo->tm_mday, timeinfo->tm_mon + 1, timeinfo->tm_year + 1900, daily_data.temperatureMin, daily_data.temperatureMax, daily_data.icon.c_str());
			v_forecast.push_back(daily_data);
		}
		return true;
	}
	return false;
}

std::string CWeather::getDirectionString(int degree)
{
	std::string direction("n/a");

	float step = 11.25;
	float deg = degree;

	     if (deg <=      step) { direction = g_Locale->getText(LOCALE_WEATHER_DIRECTION_N); }
	else if (deg <=  3 * step) { direction = g_Locale->getText(LOCALE_WEATHER_DIRECTION_NNE); }
	else if (deg <=  5 * step) { direction = g_Locale->getText(LOCALE_WEATHER_DIRECTION_NE); }
	else if (deg <=  7 * step) { direction = g_Locale->getText(LOCALE_WEATHER_DIRECTION_ENE); }
	else if (deg <=  9 * step) { direction = g_Locale->getText(LOCALE_WEATHER_DIRECTION_E); }
	else if (deg <= 11 * step) { direction = g_Locale->getText(LOCALE_WEATHER_DIRECTION_ESE); }
	else if (deg <= 13 * step) { direction = g_Locale->getText(LOCALE_WEATHER_DIRECTION_SE); }
	else if (deg <= 15 * step) { direction = g_Locale->getText(LOCALE_WEATHER_DIRECTION_SSE); }
	else if (deg <= 17 * step) { direction = g_Locale->getText(LOCALE_WEATHER_DIRECTION_S); }
	else if (deg <= 19 * step) { direction = g_Locale->getText(LOCALE_WEATHER_DIRECTION_SSW); }
	else if (deg <= 21 * step) { direction = g_Locale->getText(LOCALE_WEATHER_DIRECTION_SW); }
	else if (deg <= 23 * step) { direction = g_Locale->getText(LOCALE_WEATHER_DIRECTION_WSW); }
	else if (deg <= 25 * step) { direction = g_Locale->getText(LOCALE_WEATHER_DIRECTION_W); }
	else if (deg <= 27 * step) { direction = g_Locale->getText(LOCALE_WEATHER_DIRECTION_WNW); }
	else if (deg <= 29 * step) { direction = g_Locale->getText(LOCALE_WEATHER_DIRECTION_NW); }
	else if (deg <= 31 * step) { direction = g_Locale->getText(LOCALE_WEATHER_DIRECTION_NNW); }
	else                       { direction = g_Locale->getText(LOCALE_WEATHER_DIRECTION_N); }

	return direction;
}

bool CWeather::FindCoords(std::string postalcode, std::string country)
{
	std::string data = "http://api.openweathermap.org/geo/1.0/zip?zip=" + postalcode + "," + country + "&appid=" + key;
	JSONCPP_STRING answer;
	JSONCPP_STRING formattedErrors;
	Json::CharReaderBuilder builder;
	Json::CharReader *reader = builder.newCharReader();
	Json::Value DataValues;
	answer.clear();

	if (!getUrl(data, answer))
	{
		delete reader;
		return false;
	}

	bool parsedSuccess = reader->parse(answer.c_str(), answer.c_str() + answer.size(), &DataValues, &formattedErrors);
	delete reader;

	if (!parsedSuccess)
	{
		printf("Failed to parse JSON\n");
		printf("%s\n", formattedErrors.c_str());
		return false;
	}

	if (DataValues["message"].asString() == "not found")
		return false;

	float lat = DataValues["lat"].asFloat();
	float lon = DataValues["lon"].asFloat();
	g_settings.weather_city = DataValues["name"].asString();
	g_settings.weather_location = to_string(lat) + "," + to_string(lon);
	return true;
}

void CWeather::show(int x, int y)
{
	checkUpdate();

	if (form == NULL)
		form = new CComponentsForm();

	if (!g_settings.weather_enabled || coords.empty())
		return;

	CComponentsPicture *ptmp = new CComponentsPicture(RADIUS_MID, RADIUS_MID, getCurrentIcon());
	ptmp->setColorBody(form->getColorBody());
	form->addCCItem(ptmp);

	CComponentsText *temp = new CComponentsText(ptmp->getWidth() + 2*RADIUS_MID, ptmp->getHeight()/2 + RADIUS_MID - g_Font[SNeutrinoSettings::FONT_TYPE_INFOBAR_CHANNAME]->getHeight()/2, 0, 0, getCurrentTemperature() + "°C", CTextBox::AUTO_WIDTH, g_Font[SNeutrinoSettings::FONT_TYPE_INFOBAR_CHANNAME]);
	temp->doPaintBg(false);
	temp->setTextColor(COL_INFOBAR_TEXT);
	form->addCCItem(temp);

	int height = std::max(ptmp->getHeight(),temp->getHeight());
	form->setDimensionsAll(x, y, ptmp->getWidth() + temp->getWidth() + 2*RADIUS_MID, height + 2*RADIUS_MID);
	form->enableShadow();
	form->paint();
}

void CWeather::hide()
{
	if (!form)
		return;

	if (form->isPainted())
	{
		form->hide();
		delete form;
		form = NULL;
	}
}
