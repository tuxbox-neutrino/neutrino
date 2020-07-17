/*
	Based up Neutrino-GUI - Tuxbox-Project
	Copyright (C) 2001 by Steffen Hehn 'McClean'

	OPKG-Manager Class for Neutrino-GUI

	Implementation:
	Copyright (C) 2012-2015 T. Graf 'dbt'
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
#include <configfile.h>
#include <string>
#include <vector>
#include <map>

#define OPKG_MAX_FEEDS 10

class COPKGManager : public CMenuTarget
{
	private:
		int width;
		std::string tmp_str;
		CConfigFile opkg_conf;
		void saveConfig();
		void loadConfig();
		struct pkg;
		void init();
		bool silent; // Controls some screen messages, eg, avoids unintended or disturbing messages on update checks at background.
		//config
		std::string config_src[OPKG_MAX_FEEDS];
		std::vector<std::string> config_dest;

		//filter
		std::vector<std::string> v_bad_pattern;

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
		typedef struct om_error_data_t
		{
			std::string	id;
			int		num;
		}om_error_struct_t;
		//error types
		enum
		{
			OM_UNKNOWN_ERR 		=-1,
			OM_SUCCESS		= 0,
			OM_UNSATISFIED_DEPS_ERR = 5,
			OM_DOWNLOAD_ERR		= 11,
			OM_CONFLICT_ERR		= 12,
			OM_OUT_OF_SPACE_ERR	= 15,
			OM_PREREM_SCRIPT_ERR	= 16,
		};
		om_error_data_t *err_list;
		void OM_ERRORS()
		{
			static om_error_data_t errlist[] = { 	{ "Cannot satisfy the following dependencies"	, OM_UNSATISFIED_DEPS_ERR	}, 
								{ "No space left on device"			, OM_OUT_OF_SPACE_ERR 		},
								{ "The following packages conflict"		, OM_CONFLICT_ERR		},
								{ "Only have"					, OM_OUT_OF_SPACE_ERR 		},
								{ "prerm script for package"			, OM_PREREM_SCRIPT_ERR		},
							} ;
			err_list = errlist;
		};
		void showErr(int* res);

		int execCmd(const char* cmdstr, int verbose_mode = 0);
		int execCmd(std::string cmdstr, int verbose_mode = 0) {
			return execCmd(cmdstr.c_str(), verbose_mode);
		};
		void getPkgData(const int pkg_content_id);
		std::string getBlankPkgName(const std::string& line);
		bool isInstalled(const std::string& pkg_name);
		bool isUpgradable(const std::string& pkg_name);

		/*!
		* Gets an info from opkg command info or status from a package via keywords as std::string
		* 1st parameter is name of package as string eg. "gdb", without file extension or version data
		* 2nd parameter needs a keyword like:
		* Package, Version, Depends, Status, Section, Architecture, Maintainer, MD5Sum, Size, Filename, Source, Description
		* These kewords are to find in the control package inside of the opkg package file and the package list.
		* 3rd parameter sets the sub command status or info. For more details, take a look to the opkg commands via command line.
		*/
		std::string getPkgInfo(const std::string& pkg_name, const std::string& pkg_key = std::string(), bool current_status = false);

		//Does the same like getPkgInfo(), but only for status
		std::string getPkgStatus(const std::string& pkg_name, const std::string& pkg_key){return getPkgInfo(pkg_name, pkg_key, true);}

		std::string getKeyInfo(const std::string& input, const std::string& pkg_info_key, const std::string& delimiters);
		int showMenu();
		void showMenuConfigFeed(CMenuWidget *feed_menu);
		void updateMenu();
		void refreshMenu();

		//!Returns a vector with possible filter entries from OPKG_BAD_PATTERN_LIST_FILE 
		static std::vector<std::string> getBadPackagePatternList();
		/*!
		* Returns true if found a ''bad'' package, Parameter: package name as std::string by rev
		* To detect bad packages, it must be exist a matching pattern list file.
		* Path is defined in OPKG_BAD_PATTERN_LIST_FILE.
		* This provides the option to filter some unwanted entries in the package list menue.
		* This makes sense eg. to hinder that the user could change important system packages.
		* NOTE: a sample file you should find here as : "/var/tuxbox/config/bad_package_pattern.list.sample"
		* If required, remove the ".sample" extension and change the entries for your requirements
		* howto: a simple way to filter a package is to add the pure name, if you want
		* to hide a package (even which name) then add this name to a line. Eg. if you want to hide
		* package wget then add this and this package is not displayed at the gui.
		* Also a few place holders should work, see the badpackage() function, but this
		* can be inaccurately because it could filter innocent packages.
		*/
		bool badpackage(std::string &s);

		void showError(const char* local_msg, char* err_msg = NULL, const std::string& additional_text = std::string());
		int doUpdate();
		void handleShellOutput(std::string* cur_line, int* res, bool* ok);

		std::string getInfoDir();
		std::string getPkgDescription(std::string pkgName, std::string pkgDesc="");

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

		int num_updates;
	public:
		COPKGManager();
		~COPKGManager();

		int exec(CMenuTarget* parent, const std::string & actionKey);
		static bool hasOpkgSupport();
		bool checkUpdates(const std::string & package_name = std::string(), bool show_progress = true);
		bool installPackage(const std::string& pkg_name, std::string options = std::string(), bool force_configure = false);
		bool checkSize(const std::string& pkg_name);
};
#endif
