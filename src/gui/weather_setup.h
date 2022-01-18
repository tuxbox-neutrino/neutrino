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

#ifndef __weather_setup__
#define __weather_setup__

#include <gui/widget/menue.h>

#include <string>

class CWeatherSetup : public CMenuTarget, CChangeObserver
{
	private:
		int width, selected;

		CMenuOptionChooser * weather_onoff;
		std::string weather_api_key_short;

		int showWeatherSetup();
		int showSelectWeatherLocation();


	public:	
		CWeatherSetup();
		~CWeatherSetup();
		int exec(CMenuTarget* parent, const std::string & actionKey);
		bool changeNotify(const neutrino_locale_t OptionName, void * /*data*/);
};


#endif
