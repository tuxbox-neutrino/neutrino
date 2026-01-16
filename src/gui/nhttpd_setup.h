/*
	Based up Neutrino-GUI - Tuxbox-Project
	Copyright (C) 2001 by Steffen Hehn 'McClean'

	nhttpd setup menu - Neutrino-GUI
	Copyright (C) 2026, Thilo Graf 'dbt'

	License: GPL

	This program is free software; you can redistribute it and/or
	modify it under the terms of the GNU General Public
	License as published by the Free Software Foundation; either
	version 2 of the License, or (at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
	General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program. If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef __nhttpd_setup__
#define __nhttpd_setup__

#include <gui/widget/menue.h>
#include <system/setting_helpers.h>

#include <string>

class CNhttpdSetup : public CMenuTarget, public CChangeObserver
{
	private:
		int width;
		int port;
		int auth_enabled;
		int threading;
		int loglevel;
		int ssl_enabled;

		std::string host;
		std::string username;
		std::string password;
		std::string no_auth_client;
		std::string ssl_pemfile;
		std::string ssl_ca_file;

		CGenericMenuActivate auth_items;
		CGenericMenuActivate ssl_items;

		void loadConfig();
		bool saveConfig();
		int showSetup();

	public:
		CNhttpdSetup();
		~CNhttpdSetup();

		int exec(CMenuTarget* parent, const std::string &actionKey);
		bool changeNotify(const neutrino_locale_t OptionName, void *Data);
};

#endif /* __nhttpd_setup__ */
