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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "update_check_packages.h"
#include "opkg_manager.h"
#include "widget/msgbox.h"
#include <system/debug.h>
#include <system/helpers.h>

CUpdateCheckPackages::CUpdateCheckPackages():CComponentsTimer(1000*60*60*(g_settings.softupdate_autocheck_packages <= 0 ? 1 : g_settings.softupdate_autocheck_packages))
{
	tm_thread_name	= "n:update_check";
	check_done = false;

	//init slot for package check
	OnTimer.connect(sigc::mem_fun(*this, &CUpdateCheckPackages::check4PackageUpdates));
}

CUpdateCheckPackages* CUpdateCheckPackages::getInstance()
{
	static CUpdateCheckPackages * uc = NULL;
	if (!uc)
		uc = new CUpdateCheckPackages();

	return uc;
}

void CUpdateCheckPackages::check4PackageUpdates()
{
	if (!g_settings.softupdate_autocheck_packages) //disabled
		return;

	if (g_settings.softupdate_autocheck_packages == -1 && check_done) //only on start
		return;

	COPKGManager man;
	if (!man.hasOpkgSupport())
	{
		check_done = true;
		return;
	}

	man.setUpdateCheckResult(false);

	check_done = true;
}
