/*
	Neutrino - Power Off Menu

	Copyright (C) 2026 T. Graf 'dbt'

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

#include <config.h>

#include <gui/poweroff_menu.h>
#include <gui/plugins.h>
#include <gui/sleeptimer.h>
#include <gui/miscsettings_menu.h>
#include <gui/widget/icons.h>
#include <gui/widget/menue.h>

#include <global.h>
#include <neutrino.h>
#include <plugin.h>
#include <system/locals.h>

// Option-string shown on the 'Herunterfahren' entry: only when recording
// protection (g_settings.shutdown_block_while_recording) is enabled, as a
// discreet opt-in confirmation. Empty string = nothing shown (default).
static std::string recordingProtectionOption(void)
{
	if (g_settings.shutdown_block_while_recording)
		return g_Locale->getText(LOCALE_POWEROFF_REC_PROTECTED);
	return "";
}

// ---------------------------------------------------------------------------
// CPowerOffMenu
// ---------------------------------------------------------------------------

int CPowerOffMenu::exec(CMenuTarget *parent, const std::string &actionKey)
{
	// Sub-actions: menu item was selected, now execute the action
	if (actionKey == "standby")
	{
		g_settings.power_off_selected = 0;
		g_RCInput->postMsg(NeutrinoMessages::STANDBY_ON, 0);
		return menu_return::RETURN_EXIT_ALL;
	}
	if (actionKey == "shutdown")
	{
		g_settings.power_off_selected = 1;
		g_RCInput->postMsg(NeutrinoMessages::SHUTDOWN, 0);
		return menu_return::RETURN_EXIT_ALL;
	}
	if (actionKey == "reboot")
	{
		g_settings.power_off_selected = 2;
		g_RCInput->postMsg(NeutrinoMessages::REBOOT, 0);
		return menu_return::RETURN_EXIT_ALL;
	}
	if (actionKey == "sleeptimer")
	{
		// permanent=false: arm a one-shot countdown (box → standby after N min)
		CSleepTimerWidget *st = new CSleepTimerWidget;
		st->exec(NULL, "");
		delete st;
		return menu_return::RETURN_REPAINT;
	}
	if (actionKey == "energy")
	{
		// open the full energy/shutdown settings menu (also reachable via extended settings)
		CMiscMenue misc;
		return misc.exec(parent, "energy_power");
	}
	if (actionKey == "last")
	{
		// RC_standby fallback: trigger the remembered default entry, so
		// Main menu → RC_standby → RC_standby reaches the last-used function with two presses.
		// Single source of truth: g_settings.power_off_selected (standby=0/shutdown=1/reboot=2).
		// Resolve to a concrete, currently-available action.
		int p = g_settings.power_off_selected;
		if (p == 0 && CNeutrinoApp::getInstance()->getMode() != NeutrinoModes::mode_standby)
			return exec(parent, "standby");
		if (p == 2)
			return exec(parent, "reboot");
		if (g_info.hw_caps->can_shutdown)	// p==1, or standby not available here
			return exec(parent, "shutdown");
		return exec(parent, "standby");
	}

	// No actionKey – open the submenu
	if (parent)
		parent->hide();

	CMenuWidget m(LOCALE_MAINMENU_POWEROFF_MENU, NEUTRINO_ICON_HINT_SHUTDOWN, 35);
	m.addIntroItems();

	// Order follows the legacy main-menu power group (SleepTimer, Reboot,
	// Standby, Shutdown) so users keep their muscle memory. Color keys stay
	// bound to their action regardless of position; RC_standby is added via
	// addKey() below so the physical power button also works.

	// --- SleepTimer (arm one-shot standby countdown) – RC_blue ---
	CMenuForwarder *fw_sleep = new CMenuForwarder(LOCALE_MAINMENU_SLEEPTIMER,
		true, NULL, this, "sleeptimer", CRCInput::RC_blue);
	fw_sleep->setHint(NEUTRINO_ICON_HINT_SLEEPTIMER, LOCALE_MENU_HINT_SLEEPTIMER);
	m.addItem(fw_sleep);

	// --- Reboot – RC_yellow ---
	const char *marker_reboot = (g_settings.power_off_selected == 2) ? NEUTRINO_ICON_BUTTON_POWER : NULL;
	CMenuForwarder *fw_reboot = new CMenuForwarder(LOCALE_MAINMENU_REBOOT,
		true, NULL, this, "reboot", CRCInput::RC_yellow, NULL, marker_reboot);
	fw_reboot->setHint(NEUTRINO_ICON_HINT_REBOOT, LOCALE_MENU_HINT_REBOOT);
	m.addItem(fw_reboot);

	// --- Standby – RC_green ---
	// Only shown when not already in standby mode.
	if (CNeutrinoApp::getInstance()->getMode() != NeutrinoModes::mode_standby)
	{
		const char *marker = (g_settings.power_off_selected == 0) ? NEUTRINO_ICON_BUTTON_POWER : NULL;
		CMenuForwarder *fw = new CMenuForwarder(LOCALE_MAINMENU_STANDBY,
			true, NULL, this, "standby", CRCInput::RC_green, NULL, marker);
		fw->setHint(NEUTRINO_ICON_HINT_SHUTDOWN, LOCALE_MENU_HINT_STANDBY);
		m.addItem(fw);
	}

	// --- Deep-Standby / Shutdown – RC_red ---
	if (g_info.hw_caps->can_shutdown)
	{
		const char *marker = (g_settings.power_off_selected == 1) ? NEUTRINO_ICON_BUTTON_POWER : NULL;
		CMenuForwarder *fw = new CMenuForwarder(LOCALE_MAINMENU_SHUTDOWN,
			true, NULL, this, "shutdown", CRCInput::RC_red, NULL, marker);
		fw->setHint(NEUTRINO_ICON_HINT_SHUTDOWN, LOCALE_MENU_HINT_SHUTDOWN);
		std::string rec_opt = recordingProtectionOption();
		if (!rec_opt.empty())
			fw->setOption(rec_opt);
		m.addItem(fw);
	}

	// Plugins registered with PLUGIN_INTEGRATION_POWER
	m.integratePlugins(PLUGIN_INTEGRATION_POWER);

	// --- Settings - shortcut to the energy/shutdown settings (last entry) ---
	// Still reachable via Settings → Extended Settings when this submenu is hidden.
	m.addItem(GenericMenuSeparatorLine);
	CMenuForwarder *fw_energy = new CMenuForwarder(LOCALE_MAINMENU_SETTINGS,
		true, NULL, this, "energy", CRCInput::RC_0);
	fw_energy->setHint(NEUTRINO_ICON_HINT_SETTINGS, LOCALE_MENU_HINT_MISC_ENERGY);
	m.addItem(fw_energy);

	// Physical power/standby button triggers the remembered default entry (see "last" above),
	// so Main menu → RC_standby → RC_standby reaches the last-used function with two presses.
	// RC_red/green/yellow stay the visible DirectKey icons; RC_standby is the functional fallback.
	m.addKey(CRCInput::RC_standby, this, "last");

	// Pre-select last used entry by name – robust against intro-item offset:
	// addIntroItems() inserts separator+back+separator before the power items,
	// so setSelected(conceptual_index) would land on intro items instead.
	{
		static const neutrino_locale_t preselect_locs[3] =
		{
			LOCALE_MAINMENU_STANDBY, LOCALE_MAINMENU_SHUTDOWN, LOCALE_MAINMENU_REBOOT
		};
		int p = g_settings.power_off_selected;
		m.setSelectedByName(preselect_locs[(p >= 0 && p < 3) ? p : 2]);
		// If item not found (e.g. standby hidden in standby mode),
		// setSelectedByName() is a no-op and initSelectable() picks the first
		// selectable item automatically.
	}

	int res = m.exec(NULL, "");
	// Update remembered position when user pressed Back (not an action):
	// map the actual item index back to conceptual index 0/1/2 by name.
	if (res != menu_return::RETURN_EXIT_ALL)
	{
		int sel = m.getSelected();
		if (sel >= 0 && sel < m.getItemsCount())
		{
			const char *name = m.getItem(sel)->getName();
			if (name)
			{
				if (strcmp(name, g_Locale->getText(LOCALE_MAINMENU_STANDBY))  == 0)
					g_settings.power_off_selected = 0;
				else if (strcmp(name, g_Locale->getText(LOCALE_MAINMENU_SHUTDOWN)) == 0)
					g_settings.power_off_selected = 1;
				else if (strcmp(name, g_Locale->getText(LOCALE_MAINMENU_REBOOT))   == 0)
					g_settings.power_off_selected = 2;
				// else: plugin or unknown entry – keep current value
			}
		}
	}

	return res;
}

// ---------------------------------------------------------------------------
// CPowerOffDirect
// ---------------------------------------------------------------------------

int CPowerOffDirect::exec(CMenuTarget * /*parent*/, const std::string & /*actionKey*/)
{
	g_RCInput->postMsg(NeutrinoMessages::SHUTDOWN, 0);
	return menu_return::RETURN_EXIT_ALL;
}

// Live option-string for the main-menu 'Herunterfahren' forwarder (built once
// in InitMenuMain): getOption() falls back to getValue() on every repaint, so
// the protection status stays current without rebuilding the menu.
std::string &CPowerOffDirect::getValue(void)
{
	valueStringTmp = recordingProtectionOption();
	return valueStringTmp;
}
