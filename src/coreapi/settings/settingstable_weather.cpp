/*
 * settingstable_weather.cpp - weather settings, one row per field
 *
 * Copyright (C) 2026 NI-Team
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include "settingstable.h"
#include "settingsfield.h"

namespace coreapi
{

namespace
{

/* The weather section, off src/gui/weather_setup.cpp. That screen is a submenu
   of the misc screen's online services, :94, beside the four services the misc
   section declares; it is a section of its own here because these six settings
   are asked for as one. Three of the six defaults are macros a header of the
   screen states, which this layer may not include, so the literals below carry
   them and cite the line. src/gui/weather_setup.h:24 */

/* The screen makes the three below editable only while the weather is on,
   which it states as the item's own state rather than through a notifier.
   src/gui/weather_setup.cpp:115 src/gui/weather_setup.cpp:119 */
constexpr Condition kWeatherOn[] =
{
	when("weather_enabled").isNot(0)
};

constexpr Descriptor kWeather[] =
{
	/* The screen offers this one only where the key beside it passes a check
	   that reads the key itself, which is a function call and not a comparison
	   against another setting, so it carries no condition. The loader turns it
	   off the same way at load, src/neutrino.cpp:1162. */
	// src/neutrino.cpp:1161 src/gui/weather_setup.cpp:97
	boolRow("weather_enabled")
		.section("weather")
		.label("weather.enabled")
		.hint("menu.hint_weather_enabled")
		.defaultValue(1)
		.field(COREAPI_NUMBER_FIELD(weather_enabled)),

#if ENABLE_WEATHER_KEY_MANAGE
	/* The two below sit behind the arm the program loads and saves them under,
	   src/neutrino.cpp:1157 and :2076. Where it is off the key is whatever the
	   build was configured with and a written value is lost at the next start,
	   so there is nothing here for a row to offer.

	   The key is a credential and is declared secret, so a read answers nothing
	   whatever is stored. It defaults to the placeholder below only where the
	   build carries no key of its own; one configured with a key falls back to
	   that key instead, which is not a constant this can carry.
	   src/neutrino.cpp:1155 */
	// src/neutrino.cpp:1158 src/gui/weather_setup.cpp:103
	textRow("weather_api_key")
		.section("weather")
		.label("weather.api_key")
		.hint("menu.hint_weather_api_key")
		.defaultValue("XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX")
		.text(kRuleKey32)
		.secret()
		.field(COREAPI_TEXT_FIELD(weather_api_key)),
	/* The item for this one is inside an arm the build does not take and it
	   binds a number of the screen's own rather than the field, which the
	   screen copies back in changeNotify, src/gui/weather_setup.cpp:215. Its
	   label comes from that dead item, which is the only statement of it there
	   is. The value is the version part of a URL,
	   src/gui/weather.cpp:121, so it is text and not a choice.
	   src/gui/weather_setup.cpp:110 */
	// src/neutrino.cpp:1159 src/gui/weather_setup.cpp:110
	textRow("weather_api_version")
		.section("weather")
		.label("weather.api_version")
		.hint("menu.hint_weather_api_version")
		.defaultValue("3.0")
		.field(COREAPI_TEXT_FIELD(weather_api_version)),
#endif

	/* The name of the place and its coordinates are one setting in two parts,
	   written together by the item that picks a place from the list the screen
	   read, src/gui/weather_setup.cpp:174. Only the coordinates reach the
	   weather itself, src/gui/weather.cpp:73, so writing the name alone renames
	   what the screen shows and leaves the forecast where it was. Both are
	   declared, because both are text this layer can carry; the screen has one
	   item for the pair and both rows name its label.
	   Default: src/gui/weather_setup.h:24 */
	// src/neutrino.cpp:1164 src/gui/weather_setup.cpp:115
	textRow("weather_city")
		.section("weather")
		.label("weather.location")
		.hint("menu.hint_weather_location")
		.defaultValue("Berlin")
		.changeableWhen(kWeatherOn)
		.field(COREAPI_TEXT_FIELD(weather_city)),
	// src/neutrino.cpp:1165 src/gui/weather_setup.cpp:115
	textRow("weather_location")
		.section("weather")
		.label("weather.location")
		.hint("menu.hint_weather_location")
		.defaultValue("52.52,13.40")
		.changeableWhen(kWeatherOn)
		.field(COREAPI_TEXT_FIELD(weather_location)),
	/* Five characters on the screen, src/gui/weather_setup.cpp:185, and a
	   String carries no bound, so that is stated nowhere a caller can read.
	   Default: src/gui/weather_setup.h:26 */
	// src/neutrino.cpp:1166 src/gui/weather_setup.cpp:119
	textRow("weather_postalcode")
		.section("weather")
		.label("weather.postalcode")
		.hint("menu.hint_weather_postalcode")
		.defaultValue("10178")
		.text(kRulePostalCode)
		.changeableWhen(kWeatherOn)
		.field(COREAPI_TEXT_FIELD(weather_postalcode)),
};

} // anonymous namespace

const Descriptor *settingsTableWeather(size_t &count)
{
	count = sizeof(kWeather) / sizeof(kWeather[0]);
	return kWeather;
}

} // namespace coreapi
