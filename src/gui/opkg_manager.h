/*
	Based up Neutrino-GUI - Tuxbox-Project
	Copyright (C) 2001 by Steffen Hehn 'McClean'

	OPKG-Manager Class for Neutrino-GUI

	Implementation:
	Copyright (C) 2012 T. Graf 'dbt'
	www.dbox2-tuning.net

	Adaptions:
	Copyright (C) 2013 martii
	gitorious.org/neutrino-mp/martiis-neutrino-mp

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

#ifndef __OPKG_MANAGER__
#define __OPKG_MANAGER__

#include <gui/widget/menue.h>
#include <driver/framebuffer.h>

#include <string>
#include <vector>
#include <map>

class COPKGManager : public CMenuTarget
{
	private:
		int width;

		CFrameBuffer *frameBuffer;

		struct pkg;

		std::map<std::string,pkg> pkg_map;
		std::vector<pkg*> pkg_vec;

		CMenuWidget *menu;
		CMenuForwarder *upgrade_forwarder;
		bool list_installed_done;
		bool list_upgradeable_done;
		bool installed;
		bool expert_mode;
		int menu_offset;

		int execCmd(const char* cmdstr, bool verbose = false, bool acknowledge = false);
		int execCmd(std::string cmdstr, bool verbose = false, bool acknowledge = false) {
			return execCmd(cmdstr.c_str(), verbose, acknowledge);
		};
		void getPkgData(const int pkg_content_id);
		static std::string getBlankPkgName(const std::string& line);
		int showMenu();
		void updateMenu();
		void refreshMenu();

		struct pkg {
			std::string name;
			std::string desc;
			bool installed;
			bool upgradable;
			CMenuForwarder *forwarder;
			pkg() { }
			pkg(std::string &_name, std::string &_desc)
				: name(_name), desc(_desc), installed(false), upgradable(false) { }
		};
	public:
		COPKGManager();
		~COPKGManager();

		int exec(CMenuTarget* parent, const std::string & actionKey);
		static bool hasOpkgSupport();
};
#endif
