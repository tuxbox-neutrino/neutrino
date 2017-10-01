/*
	Neutrino-GUI  -   DBoxII-Project


	Copyright (C) 2009-2014 CoolStream International Ltd
	Copyright (C) 2013-2014 martii

	License: GPLv2

	This program is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation; version 2 of the License.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program; if not, write to the Free Software
	Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/


#ifndef __hdd_menu__
#define __hdd_menu__

#include "widget/menue.h"
#include <gui/widget/hintbox.h>

struct devtool_s {
	std::string fmt;
	std::string fsck;
	std::string fsck_options;
	std::string mkfs;
	std::string mkfs_options;
	bool fsck_supported;
	bool mkfs_supported;
};

class CHDDDestExec : public CMenuTarget
{
public:
        int exec(CMenuTarget* parent, const std::string&);
};

class CHDDMenuHandler : public CMenuTarget
{
	private:
		int width;
		std::string mount;
		std::string umount;
		bool show_menu;
		bool in_menu;
		bool lock_refresh;
		std::map<std::string, std::string> devtitle;
		struct hdd_s {
			std::string devname;
			std::string fmt;
			std::string desc;
			CMenuForwarder *cmf;
			bool mounted;
		};
		std::vector<hdd_s> hdd_list;
		struct cmp_hdd_by_name: public std::binary_function <const struct hdd_s, const struct hdd_s, bool>
		{
			bool operator() (const struct hdd_s &c1, const struct hdd_s &c2)
			{
				return std::lexicographical_compare(c1.devname.begin(), c1.devname.end(), c2.devname.begin(), c2.devname.end());
			};
		};

		static devtool_s devtools[];

		bool is_mounted(const char *dev);
		void getBlkIds();
		std::string getFmtType(std::string name, std::string part = "");
		std::string getDefaultPart(std::string dev);
		bool mount_dev(std::string name);
		bool umount_dev(std::string name);
		bool umount_all(std::string dev);
		bool add_dev(std::string dev, std::string part);
		bool waitfordev(std::string dev, int maxwait);
		void check_dev_tools();
		devtool_s * get_dev_tool(std::string fmt);

		int showDeviceMenu(std::string dev);
		int checkDevice(std::string dev);
		int formatDevice(std::string dev);
		void showError(neutrino_locale_t err);
		bool scanDevices();
		void showHint(std::string &messsage);
		void setRecordPath(std::string &dev);
		CHDDMenuHandler();

	public:
		~CHDDMenuHandler();

		static CHDDMenuHandler* getInstance();
		int exec( CMenuTarget* parent,  const std::string &actionkey);
		int doMenu();
		int filterDevName(const char * name);
		int handleMsg(const neutrino_msg_t _msg, neutrino_msg_data_t data);
};

#endif
