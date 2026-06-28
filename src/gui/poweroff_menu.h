/*
	Neutrino - Power Off Menu

	Copyright (C) 2026 T. Graf 'dbt'

	CPowerOffMenu  - submenu with standby / shutdown / reboot + power-integration plugins
	CPowerOffDirect - direct shutdown (legacy / personalize alternative)

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
	along with this program. If not, see <http://www.gnu.org/licenses/>.

*/

#ifndef __poweroff_menu_h__
#define __poweroff_menu_h__

#include <gui/widget/menue.h>
#include <string>

/*
 * CPowerOffMenu
 * Shows a submenu with power-off options. The last selected entry
 * is remembered (stored in g_settings.power_off_selected) and
 * pre-selected on next invocation.
 * Plugins with integration type PLUGIN_INTEGRATION_POWER are
 * appended automatically.
 */
class CPowerOffMenu : public CMenuTarget
{
public:
	int exec(CMenuTarget *parent, const std::string &actionKey);
};

/*
 * CPowerOffDirect
 * Replicates the classic single-entry shutdown behaviour for users
 * who prefer it via the personalize menu.
 */
class CPowerOffDirect : public CMenuTarget
{
public:
	int exec(CMenuTarget *parent, const std::string &actionKey);
	std::string& getValue(void);
};

#endif // __poweroff_menu_h__
