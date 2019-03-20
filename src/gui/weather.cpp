/*
	Copyright (C) 2017, 2018 ,2019 TangoCash

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

CWeather *CWeather::getInstance()
{
	static CWeather *weather = NULL;
	if (!weather)
		weather = new CWeather();
	return weather;
}

CWeather::CWeather()
{
	act_temp = 0;
	act_wicon = "unknown.png";
	key = g_settings.weather_api_key;
	form = NULL;
	v_forecast.clear();
	last_time = 0;
	coords = "";
	city = "";
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
		GetWeatherDetails();
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

	std::string data = "https://api.darksky.net/forecast/" + key + "/" + coords + "?units=si&lang=de&exclude=minutely,hourly,flags,alerts";
	std::string answer;
	double found = 0;

	v_forecast.clear();

	Json::Value DataValues;
	Json::Reader DataReader;
	bool parsedSuccess = false;

	answer = "";
	if (!getUrl(data, answer))
		return false;

	parsedSuccess = DataReader.parse(answer, DataValues, false);
	if (!parsedSuccess)
	{
		printf("Failed to parse JSON\n");
		printf("%s\n", DataReader.getFormattedErrorMessages().c_str());
		return false;
	}

	found = DataValues["currently"].get("time", 0).asDouble();

	printf("[CWeather]: results found: %lf\n", found);

	if (found > 0)
	{
		timezone = DataValues["timezone"].asString();
		act_temp = DataValues["currently"].get("temperature", "").asFloat();
		act_wicon = DataValues["currently"].get("icon", "").asString();
		if (act_wicon.empty())
			act_wicon = "unknown.png";
		else
			act_wicon = act_wicon + ".png";
		printf("[CWeather]: temp in %s (%s): %.1f - %s\n", city.c_str(), timezone.c_str(), act_temp, act_wicon.c_str());

		forecast_data weatherinfo;
		Json::Value elements = DataValues["daily"]["data"];
		for (unsigned int i = 0; i < elements.size(); i++)
		{
			weatherinfo.timestamp = elements[i].get("time", 0).asDouble();
			weatherinfo.wicon = elements[i].get("icon", "").asString();
			if (weatherinfo.wicon.empty())
				weatherinfo.wicon = "unknown.png";
			else
				weatherinfo.wicon = weatherinfo.wicon + ".png";
			weatherinfo.min_temp = elements[i].get("temperatureMin", "").asFloat();
			weatherinfo.max_temp = elements[i].get("temperatureMax", "").asFloat();

			struct tm *timeinfo;
			timeinfo = localtime(&weatherinfo.timestamp);

			printf("[CWeather]: temp %d.%d.%d: min %.1f - max %.1f -> %s\n", timeinfo->tm_mday, timeinfo->tm_mon, timeinfo->tm_year + 1900, weatherinfo.min_temp, weatherinfo.max_temp, weatherinfo.wicon.c_str());
			v_forecast.push_back(weatherinfo);
		}
		return true;
	}
	return false;
}

void CWeather::show(int x, int y)
{
	checkUpdate();

	if (form == NULL)
		form = new CComponentsForm();

	if (!g_settings.weather_enabled || coords.empty())
		return;

	CComponentsPicture *ptmp = new CComponentsPicture(RADIUS_MID, RADIUS_MID, getActIcon());
	ptmp->setColorBody(form->getColorBody());
	form->addCCItem(ptmp);

	CComponentsText *temp = new CComponentsText(ptmp->getWidth() + 2*RADIUS_MID, ptmp->getHeight()/2 + RADIUS_MID - g_Font[SNeutrinoSettings::FONT_TYPE_INFOBAR_NUMBER]->getHeight()/2, 0, 0, getActTemp(), CTextBox::AUTO_WIDTH, g_Font[SNeutrinoSettings::FONT_TYPE_INFOBAR_NUMBER]);
	temp->doPaintBg(false);
	temp->setTextColor(COL_INFOBAR_TEXT);
	form->addCCItem(temp);

	form->setDimensionsAll(x, y, ptmp->getWidth() + temp->getWidth() + 2*RADIUS_MID, ptmp->getHeight() + 2*RADIUS_MID);
	form->enableShadow();
	form->paint();
}

void CWeather::hide()
{
	if (form->isPainted())
	{
		form->hide();
		delete form;
		form = NULL;
	}
}
