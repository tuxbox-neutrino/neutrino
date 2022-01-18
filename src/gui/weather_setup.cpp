/*
	Copyright (C) 2020 TangoCash

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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "weather_setup.h"

#include <global.h>
#include <neutrino.h>

#include <gui/widget/icons.h>
#include <gui/widget/menue_options.h>
#include <gui/widget/msgbox.h>
#include <gui/widget/stringinput.h>
#include <gui/widget/keyboard_input.h>

#include <gui/weather.h>

#include <driver/screen_max.h>

#include <system/debug.h>

CWeatherSetup::CWeatherSetup()
{
	width = 40;
	selected = -1;

	locations.clear();
	loadLocations(CONFIGDIR "/weather-favorites.xml");
	loadLocations(WEATHERDIR "/weather-locations.xml");
}

CWeatherSetup::~CWeatherSetup()
{
}

int CWeatherSetup::exec(CMenuTarget *parent, const std::string &actionKey)
{
	dprintf(DEBUG_DEBUG, "init weather setup menu\n");
	int res = menu_return::RETURN_REPAINT;

	if (parent)
		parent->hide();

	if (actionKey == "select_location")
	{
		return showSelectWeatherLocation();
	}

	res = showWeatherSetup();

	return res;
}

int CWeatherSetup::showWeatherSetup()
{
	CMenuWidget *ms_oservices = new CMenuWidget(LOCALE_MISCSETTINGS_HEAD, NEUTRINO_ICON_SETTINGS, width, MN_WIDGET_ID_MISCSETUP_ONLINESERVICES);
	ms_oservices->addIntroItems(LOCALE_MISCSETTINGS_ONLINESERVICES);

	weather_onoff = new CMenuOptionChooser(LOCALE_WEATHER_ENABLED, &g_settings.weather_enabled, OPTIONS_OFF0_ON1_OPTIONS, OPTIONS_OFF0_ON1_OPTION_COUNT, CApiKey::check_weather_api_key());
	weather_onoff->setHint(NEUTRINO_ICON_HINT_SETTINGS, LOCALE_MENU_HINT_WEATHER_ENABLED);
	ms_oservices->addItem(weather_onoff);

#if ENABLE_WEATHER_KEY_MANAGE
	changeNotify(LOCALE_WEATHER_API_KEY, NULL);
	CKeyboardInput weather_api_key_input(LOCALE_WEATHER_API_KEY, &g_settings.weather_api_key, 32, this);
	CMenuForwarder *mf_we = new CMenuForwarder(LOCALE_WEATHER_API_KEY, true, weather_api_key_short, &weather_api_key_input);
	mf_we->setHint(NEUTRINO_ICON_HINT_SETTINGS, LOCALE_MENU_HINT_WEATHER_API_KEY);
	ms_oservices->addItem(mf_we);
#endif

	CMenuForwarder *mf_wl = new CMenuForwarder(LOCALE_WEATHER_LOCATION, g_settings.weather_enabled, g_settings.weather_city, this, "select_location");
	mf_wl->setHint(NEUTRINO_ICON_HINT_SETTINGS, LOCALE_MENU_HINT_WEATHER_LOCATION);
	ms_oservices->addItem(mf_wl);

	int res = ms_oservices->exec(NULL, "");
	selected = ms_oservices->getSelected();
	delete ms_oservices;
	return res;
}

int CWeatherSetup::showSelectWeatherLocation()
{
	int select = 0;
	int res = 0;

	if (locations.empty())
	{
		// TODO: localize hint
		ShowHint("Warning", "Failed to load weather-favorites.xml or weather-locations.xml\nPlease press any key or wait some seconds! ...", 700, 10, NULL, NEUTRINO_ICON_HINT_IMAGEINFO, CComponentsHeader::CC_BTN_EXIT);
		g_settings.weather_location = "52.52,13.40";
		g_settings.weather_city = "Berlin";
		CWeather::getInstance()->setCoords(g_settings.weather_location, g_settings.weather_city);
		return menu_return::RETURN_REPAINT;
	}

	if (locations.size() > 0)
	{
		CMenuWidget *m = new CMenuWidget(LOCALE_WEATHER_LOCATION, NEUTRINO_ICON_LANGUAGE);
		CMenuSelectorTarget *selector = new CMenuSelectorTarget(&select);

		m->addItem(GenericMenuSeparator);

		CMenuForwarder *mf;
		for (size_t i = 0; i < locations.size(); i++)
		{
			mf = new CMenuForwarder(locations[i].city, true, NULL, selector, to_string(i).c_str());
			mf->setHint(NEUTRINO_ICON_HINT_SETTINGS, locations[i].coords.c_str());
			m->addItem(mf);
		}

		m->enableSaveScreen();
		res = m->exec(NULL, "");

		if (!m->gotAction())
			return res;

		delete selector;
	}

	g_settings.weather_location = locations[select].coords;
	g_settings.weather_city = std::string(locations[select].city);
	CWeather::getInstance()->setCoords(g_settings.weather_location, g_settings.weather_city);

	return res;
}

bool CWeatherSetup::changeNotify(const neutrino_locale_t OptionName, void */*data*/)
{
	int ret = menu_return::RETURN_NONE;

	if (ARE_LOCALES_EQUAL(OptionName, LOCALE_WEATHER_API_KEY))
	{
		g_settings.weather_enabled = g_settings.weather_enabled && CApiKey::check_weather_api_key();
		if (g_settings.weather_enabled)
			weather_api_key_short = g_settings.weather_api_key.substr(0, 8) + "...";
		else
			weather_api_key_short.clear();
		weather_onoff->setActive(CApiKey::check_weather_api_key());
	}
	return ret;
}

void CWeatherSetup::loadLocations(std::string filename)
{
	xmlDocPtr parser = parseXmlFile(filename.c_str());

	if (parser == NULL)
	{
		dprintf(DEBUG_INFO, "failed to load %s\n", filename.c_str());
		return;
	}

	xmlNodePtr l0 = xmlDocGetRootElement(parser);
	xmlNodePtr l1 = xmlChildrenNode(l0);

	if (l1)
	{
		while ((xmlGetNextOccurence(l1, "location")))
		{
			const char *country = xmlGetAttribute(l1, "country");
			const char *city = xmlGetAttribute(l1, "city");
			const char *latitude = xmlGetAttribute(l1, "latitude");
			const char *longitude = xmlGetAttribute(l1, "longitude");
			weather_loc loc;
			loc.country = strdup(country);
			loc.city = strdup(city);
			loc.coords = std::string(latitude) + "," + std::string(longitude);
			locations.push_back(loc);
			l1 = xmlNextNode(l1);
		}
	}

	xmlFreeDoc(parser);
}
