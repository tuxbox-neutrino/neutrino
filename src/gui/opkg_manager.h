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
		std::string tmp_str;
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
		std::string *local_dir;
		
		bool has_err;
		std::string err_msg;

		int execCmd(const char* cmdstr, bool verbose = false, bool acknowledge = false);
		int execCmd(std::string cmdstr, bool verbose = false, bool acknowledge = false) {
			return execCmd(cmdstr.c_str(), verbose, acknowledge);
		};
		void getPkgData(const int pkg_content_id);
		std::string getBlankPkgName(const std::string& line);

		/*
		* Gets an info from opkg command info or status from a package via keywords as std::string
		* 1st parameter is name of package as string eg. "gdb", without file extension or version data
		* 2nd parameter needs a keyword like:
		* Package, Version, Depends, Status, Section, Architecture, Maintainer, MD5Sum, Size, Filename, Source, Description
		* These kewords are to find in the control package inside of the opkg package file and the package list.
		* 3rd parameter sets the sub command status or info. For more details, take a look to the opkg commands via command line.
		*/
		std::string getPkgInfo(const std::string& pkg_name, const std::string& pkg_key, bool current_status = false);

		//Does the same like getPkgInfo(), but only for status
		std::string getPkgStatus(const std::string& pkg_name, const std::string& pkg_key){return getPkgInfo(pkg_name, pkg_key, true);}

		std::string getKeyInfo(const std::string& input, const std::string& pkg_info_key, const std::string& delimiters);
		int showMenu();
		void updateMenu();
		void refreshMenu();
		bool badpackage(std::string &s);
		void showError(const char* local_msg, char* err_msg, const std::string& command);
		int doUpdate();
		void handleShellOutput(std::string& cur_line);

		struct pkg {
			std::string name;
			std::string version;
			std::string desc;
			bool installed;
			bool upgradable;
			CMenuForwarder *forwarder;
			pkg() { }
			pkg(std::string &_name, std::string &_version, std::string &_desc)
				: name(_name), version(_version), desc(_desc), installed(false), upgradable(false) { }
		};
	public:
		COPKGManager();
		~COPKGManager();

		int exec(CMenuTarget* parent, const std::string & actionKey);
		static bool hasOpkgSupport();
		bool checkUpdates(const std::string & package_name = std::string(), bool show_progress = true);
		bool installPackage(const std::string& pkg_name, std::string options = std::string());
		bool checkSize(const std::string& pkg_name);
};
#endif
