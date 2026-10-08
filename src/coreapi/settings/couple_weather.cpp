/*
 * couple_weather.cpp - the weather place and its coordinates
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

#include "couple.h"

namespace coreapi
{
namespace settings
{

/* Only the coordinates reach the weather, so a place written without them renames the
   forecast and leaves it where it was. A place picked from the list also clears the
   postal code a search had entered, which no longer describes it; a postal code the same
   write names with text is refused with the place, while an empty one is no contradiction. */
const KeyPair kWeatherPairs[] =
{
	{ "weather_city", "weather_location" }
};

const size_t kWeatherPairCount = sizeof(kWeatherPairs) / sizeof(kWeatherPairs[0]);

void coupleWeather(CoupledBatch &b)
{
	if (!b.requirePair(kWeatherPairs[0].first, kWeatherPairs[0].second))
		return;
	b.imply("weather_postalcode", "", "weather_city");
	b.link("weather_postalcode", "weather_location");
}

} // namespace settings
} // namespace coreapi
