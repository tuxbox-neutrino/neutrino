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

CUpdateCheck::CUpdateCheck():CComponentsTimer(1000*60/*1000*6*60*/)
{
	tm_thread_name	= "n:update_check";

	//init slot for package check
	OnTimer.connect(sigc::mem_fun(*this, &CUpdateCheck::check4PackageUpdates));
}

CUpdateCheck* CUpdateCheck::getInstance()
{
	static CUpdateCheck * uc = NULL;
	if (!uc)
		uc = new CUpdateCheck();

	return uc;
}

void CUpdateCheck::check4PackageUpdates()
{
	if (!g_settings.softupdate_autocheck)
		return;

	COPKGManager man;
	if (!man.hasOpkgSupport())
		return;

	man.setUpdateCheckResult(false);
}
