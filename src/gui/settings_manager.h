/*
	settings manager menue - Neutrino-GUI

	Copyright (C) 2001 Steffen Hehn 'McClean'
	and some other guys
	Homepage: http://dbox.cyberphoria.org/

	Copyright (C) 2011 T. Graf 'dbt'
	Homepage: http://www.dbox2-tuning.net/

	License: GPL

	This program is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation; either version 2 of the License, or
	(at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program; if not, write to the Free Software
	Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

*/

#ifndef __SETTINGS_MANAGER__
#define __SETTINGS_MANAGER__

#include <gui/widget/menue.h>


#include <string>


class CSettingsManager : public CMenuTarget
{
	private:
		int width;
		bool is_wizard;

		int showMenu();
		int showMenu_wizard();

	public:	
		enum SETTINGSMANAGER_MODE
		{
			SETTINGSMANAGER_MODE_WIZARD_NO	= 0,
			SETTINGSMANAGER_MODE_WIZARD	= 1
		};

		CSettingsManager(bool wizard_mode = SETTINGSMANAGER_MODE_WIZARD_NO);
		~CSettingsManager();
		
		int exec(CMenuTarget* parent, const std::string & actionKey = "");
};

#endif
