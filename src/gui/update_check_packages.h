/*
	Based up Neutrino-GUI - Tuxbox-Project
	Copyright (C) 2001 by Steffen Hehn 'McClean'

	Update check for Neutrino-GUI

	Copyright (C) 2020 T. Graf 'dbt'

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

#ifndef __UPDATE_CHECK_PACKAGES_H__
#define __UPDATE_CHECK_PACKAGES_H__

#include "components/cc_timer.h"

class CUpdateCheckPackages : public CComponentsTimer
{
	private:
		void check4PackageUpdates();
		bool check_done;

	public:
		CUpdateCheckPackages();
		virtual ~CUpdateCheckPackages(){};
		static CUpdateCheckPackages* getInstance();
		void startThread() {initThread();}
};

#endif // __UPDATE_CHECK_PACKAGES_H__
