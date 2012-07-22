/*
	Neutrino-GUI  -   DBoxII-Project

	Copyright (C) 2011 CoolStream International Ltd

        License: GPLv2

	This program is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation;

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program; if not, write to the Free Software
	Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/

#ifndef __network_service__
#define __network_service__

#include <gui/widget/menue.h>

#include <string>

class CNetworkService : public CChangeObserver
{
	private:
		std::string command;
		std::string options;
		bool enabled;

		void Stop();
		void Start();
		void TouchFile();

	public:
		CNetworkService(std::string cmd, std::string options);
		bool Enabled() { return enabled; }

		bool changeNotify(const neutrino_locale_t OptionName, void * /*data*/);
};

class CNetworkServiceSetup : public CMenuTarget
{
	private:
		int width, selected;

		int showNetworkServiceSetup();

	public:
		CNetworkServiceSetup();
		~CNetworkServiceSetup();

		int exec(CMenuTarget* parent, const std::string & actionKey);
};

#endif
