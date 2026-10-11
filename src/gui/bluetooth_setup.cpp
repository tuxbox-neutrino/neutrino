/*
	Bluetooth devices: search, pair, connect, disconnect and remove them,
	with BlueZ doing the work

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
	along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "bluetooth_setup.h"

#include <gui/widget/hintbox.h>
#include <gui/widget/icons.h>
#include <gui/widget/msgbox.h>
#include <gui/widget/stringinput.h>

#include <global.h>
#include <neutrino.h>
#include <driver/rcinput.h>

static const struct button_label CBluetoothSetupFooterButtons[] =
{
	{ NEUTRINO_ICON_BUTTON_RED,	LOCALE_BLUETOOTH_SCAN },
	{ NEUTRINO_ICON_BUTTON_GREEN,	LOCALE_BLUETOOTH_POWER },
	{ NEUTRINO_ICON_BUTTON_YELLOW,	LOCALE_BLUETOOTH_REMOVE }
};
#define CBluetoothSetupFooterButtonCount (sizeof(CBluetoothSetupFooterButtons)/sizeof(CBluetoothSetupFooterButtons[0]))

CBluetoothSetup::CBluetoothSetup()
{
	bluez = CBluezClient::getInstance();
	menu = NULL;
	hint = NULL;
	width = 50;
	first_device = 0;
	reload = false;
	rescan = true;
}

CBluetoothSetup::~CBluetoothSetup()
{
	hideHint();
}

bool CBluetoothSetup::available()
{
	return CBluezClient::getInstance()->available();
}

/* a device name is chosen by whoever has the device, show nothing but text */
static std::string display_name(const std::string &device_name)
{
	std::string name = device_name;
	for (size_t i = 0; i < name.length(); i++)
		if ((unsigned char)name[i] < 0x20 || name[i] == 0x7f)
			name[i] = '?';
	return name;
}

std::string CBluetoothSetup::status()
{
	CBluezClient *client = CBluezClient::getInstance();
	if (!client->powered())
		return g_Locale->getText(LOCALE_OPTIONS_OFF);

	std::vector<bluez_device> list;
	std::string result;
	client->getDevices(list);
	for (size_t i = 0; i < list.size(); i++)
	{
		if (!list[i].connected)
			continue;
		if (!result.empty())
			result += ", ";
		result += display_name(list[i].name);
	}
	return result.empty() ? g_Locale->getText(LOCALE_OPTIONS_ON) : result;
}

static neutrino_locale_t kind_locale(int kind)
{
	switch (kind)
	{
		case bluez_device::KIND_AUDIO:
			return LOCALE_BLUETOOTH_KIND_AUDIO;
		case bluez_device::KIND_KEYBOARD:
			return LOCALE_BLUETOOTH_KIND_KEYBOARD;
		case bluez_device::KIND_MOUSE:
			return LOCALE_BLUETOOTH_KIND_MOUSE;
		case bluez_device::KIND_GAMEPAD:
			return LOCALE_BLUETOOTH_KIND_GAMEPAD;
		case bluez_device::KIND_INPUT:
			return LOCALE_BLUETOOTH_KIND_INPUT;
		default:
			return NONEXISTANT_LOCALE;
	}
}

int CBluetoothSetup::exec(CMenuTarget *parent, const std::string &actionKey)
{
	if (actionKey == "scan")
	{
		rescan = true;
		reload = true;
		return menu_return::RETURN_EXIT;
	}
	if (actionKey == "power")
	{
		togglePower();
		reload = true;
		return menu_return::RETURN_EXIT;
	}
	if (actionKey == "remove")
	{
		removeSelected();
		reload = true;
		return menu_return::RETURN_EXIT;
	}
	if (!actionKey.empty())
	{
		size_t n = (size_t)atoi(actionKey.c_str());
		if (n < devices.size())
		{
			/* the menu comes back with the same device selected */
			select_path = devices[n].path;
			activate(devices[n]);
		}
		reload = true;
		return menu_return::RETURN_EXIT;
	}

	if (parent)
		parent->hide();

	rescan = true;
	return show();
}

int CBluetoothSetup::show()
{
	int res = menu_return::RETURN_REPAINT;

	do
	{
		reload = false;

		const bool on = bluez->powered();
		if (rescan && on)
		{
			showHint(g_Locale->getText(LOCALE_BLUETOOTH_SCAN_WAIT));
			bluez->scan();
			hideHint();
		}
		rescan = false;
		if (!bluez->getDevices(devices))
		{
			ShowMsg(LOCALE_MESSAGEBOX_ERROR, g_Locale->getText(LOCALE_BLUETOOTH_UNAVAILABLE), CMsgBox::mbrBack, CMsgBox::mbBack);
			return res;
		}
		if (!on)
			devices.clear();

		menu = new CMenuWidget(LOCALE_MAINSETTINGS_NETWORK, NEUTRINO_ICON_NETWORK, width);
		menu->addIntroItems(LOCALE_NETWORKMENU_BLUETOOTH);

		power_state = g_Locale->getText(on ? LOCALE_OPTIONS_ON : LOCALE_OPTIONS_OFF);
		CMenuForwarder *mf = new CMenuForwarder(LOCALE_BLUETOOTH_POWER, true, power_state, this, "power", CRCInput::RC_green);
		mf->setHint("", LOCALE_MENU_HINT_BLUETOOTH_POWER);
		menu->addItem(mf);
		mf = new CMenuForwarder(LOCALE_BLUETOOTH_SCAN, on, NULL, this, "scan", CRCInput::RC_red);
		mf->setHint("", LOCALE_MENU_HINT_BLUETOOTH_SCAN);
		menu->addItem(mf);
		if (on)
			menu->addItem(GenericMenuSeparatorLine);

		first_device = menu->getItemsCount();
		options.assign(devices.size(), "");
		for (size_t i = 0; i < devices.size(); i++)
		{
			char tmp[16];

			neutrino_locale_t kind = kind_locale(devices[i].kind);
			if (kind != NONEXISTANT_LOCALE)
				options[i] = g_Locale->getText(kind);
			if (devices[i].connected || devices[i].paired)
			{
				if (!options[i].empty())
					options[i] += ", ";
				options[i] += g_Locale->getText(devices[i].connected ? LOCALE_BLUETOOTH_CONNECTED : LOCALE_BLUETOOTH_PAIRED);
			}

			snprintf(tmp, sizeof(tmp), "%d", (int)i);
			mf = new CMenuForwarder(display_name(devices[i].name), true, options[i], this, tmp);
			mf->setItemButton(NEUTRINO_ICON_BUTTON_OKAY, true);
			mf->setHint("", devices[i].connected ? LOCALE_MENU_HINT_BLUETOOTH_DISCONNECT :
					(devices[i].paired ? LOCALE_MENU_HINT_BLUETOOTH_CONNECT : LOCALE_MENU_HINT_BLUETOOTH_PAIR));
			menu->addItem(mf, select_path.empty() ? devices[i].connected : devices[i].path == select_path);
		}
		if (on && devices.empty())
			menu->addItem(new CMenuForwarder(LOCALE_BLUETOOTH_NONE_FOUND, false));

		menu->setFooter(CBluetoothSetupFooterButtons, CBluetoothSetupFooterButtonCount);
		menu->addKey(CRCInput::RC_yellow, this, "remove");

		res = menu->exec(NULL, "");

		delete menu;
		menu = NULL;
	}
	while (reload);

	return res;
}

void CBluetoothSetup::showHint(const std::string &text)
{
	hideHint();
	hint = new CHintBox(LOCALE_NETWORKMENU_BLUETOOTH, text.c_str());
	hint->paint();
}

void CBluetoothSetup::hideHint()
{
	if (!hint)
		return;

	hint->hide();
	delete hint;
	hint = NULL;
	/* keys pressed while it took that long are not meant for what comes next */
	g_RCInput->clearRCMsg();
}

void CBluetoothSetup::togglePower()
{
	const bool on = !bluez->powered();
	if (!bluez->setPowered(on))
	{
		ShowMsg(LOCALE_MESSAGEBOX_ERROR, g_Locale->getText(LOCALE_BLUETOOTH_POWER_FAILED), CMsgBox::mbrBack, CMsgBox::mbBack);
		return;
	}
	rescan = on;
}

void CBluetoothSetup::showResult(int result)
{
	neutrino_locale_t text;

	switch (result)
	{
		case CBluezClient::RESULT_OK:
			return;
		case CBluezClient::RESULT_AUTH_FAILED:
			text = LOCALE_BLUETOOTH_AUTH_FAILED;
			break;
		case CBluezClient::RESULT_NO_PROFILE:
			text = LOCALE_BLUETOOTH_NO_PROFILE;
			break;
		case CBluezClient::RESULT_GONE:
			text = LOCALE_BLUETOOTH_GONE;
			break;
		case CBluezClient::RESULT_UNAVAILABLE:
			text = LOCALE_BLUETOOTH_UNAVAILABLE;
			break;
		default:
			text = LOCALE_BLUETOOTH_FAILED;
			break;
	}
	ShowMsg(LOCALE_MESSAGEBOX_ERROR, g_Locale->getText(text), CMsgBox::mbrBack, CMsgBox::mbBack);
}

void CBluetoothSetup::activate(const bluez_device &device)
{
	int result;

	if (device.connected)
	{
		bluez->disconnect(device);
		return;
	}

	if (device.paired)
	{
		showHint(g_Locale->getText(LOCALE_BLUETOOTH_CONNECTING));
		result = bluez->connect(device);
	}
	else
	{
		showHint(g_Locale->getText(LOCALE_BLUETOOTH_PAIRING));
		result = bluez->pair(device, this);
	}
	hideHint();
	shown_code.clear();

	showResult(result);
}

void CBluetoothSetup::removeSelected()
{
	if (!menu)
		return;

	int n = menu->getSelected() - first_device;
	if (n < 0 || n >= (int)devices.size() || !devices[n].paired)
		return;

	select_path = devices[n].path;
	std::string question = display_name(devices[n].name) + "\n" + g_Locale->getText(LOCALE_BLUETOOTH_REMOVE_ASK);
	if (ShowMsg(LOCALE_BLUETOOTH_REMOVE, question, CMsgBox::mbrNo, CMsgBox::mbYes | CMsgBox::mbNo, NEUTRINO_ICON_QUESTION) == CMsgBox::mbrYes)
		bluez->remove(devices[n]);
}

/* BlueZ repeats the code with every key that is typed on the device */
void CBluetoothSetup::displayCode(const std::string &code)
{
	if (code.empty() || code == shown_code)
		return;

	shown_code = code;
	showHint(std::string(g_Locale->getText(LOCALE_BLUETOOTH_TYPE_CODE)) + "\n" + display_name(code));
}

void CBluetoothSetup::hideCode()
{
	hideHint();
}

bool CBluetoothSetup::confirmCode(const std::string &code)
{
	hideHint();
	std::string question = std::string(g_Locale->getText(LOCALE_BLUETOOTH_CONFIRM_CODE)) + "\n" + display_name(code);
	return ShowMsg(LOCALE_NETWORKMENU_BLUETOOTH, question, CMsgBox::mbrNo, CMsgBox::mbYes | CMsgBox::mbNo, NEUTRINO_ICON_QUESTION) == CMsgBox::mbrYes;
}

bool CBluetoothSetup::requestPin(std::string &pin)
{
	hideHint();
	pin = "0000";
	CStringInput input(LOCALE_BLUETOOTH_PIN, &pin, 16, NONEXISTANT_LOCALE, NONEXISTANT_LOCALE, "0123456789 ");
	input.exec(this, "");

	/* the input pads with blanks */
	size_t end = pin.find_last_not_of(' ');
	pin = end == std::string::npos ? "" : pin.substr(0, end + 1);
	return !pin.empty();
}
