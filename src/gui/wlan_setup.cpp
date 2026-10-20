/*
	wireless network setup through iwd or wpa_supplicant - Neutrino-GUI

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
	along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "wlan_setup.h"

#include <gui/widget/hintbox.h>
#include <gui/widget/icons.h>
#include <gui/widget/keyboard_input.h>
#include <gui/widget/msgbox.h>

#include <global.h>
#include <neutrino.h>
#include <driver/rcinput.h>

static const struct button_label CWlanSetupFooterButtons[] =
{
	{ NEUTRINO_ICON_BUTTON_RED,	LOCALE_NETWORKMENU_SSID_SCAN },
	{ NEUTRINO_ICON_BUTTON_GREEN,	LOCALE_NETWORKMENU_WLAN_DISCONNECT },
	{ NEUTRINO_ICON_BUTTON_YELLOW,	LOCALE_NETWORKMENU_WLAN_FORGET },
	{ NEUTRINO_ICON_BUTTON_BLUE,	LOCALE_NETWORKMENU_WLAN_HIDDEN }
};
#define CWlanSetupFooterButtonCount (sizeof(CWlanSetupFooterButtons)/sizeof(CWlanSetupFooterButtons[0]))

CWlanSetup::CWlanSetup()
{
	wlan = CWlanClient::getInstance();
	menu = NULL;
	width = 50;
	first_network = 0;
	reload = false;
	rescan = true;
}

bool CWlanSetup::available()
{
	return CWlanClient::getInstance()->available();
}

std::string CWlanSetup::connectedNetwork()
{
	return CWlanClient::getInstance()->connectedNetwork();
}

/* an SSID is chosen by whoever runs the access point, show nothing but text */
static std::string display_name(const std::string &ssid)
{
	std::string name = ssid;
	for (size_t i = 0; i < name.length(); i++)
		if ((unsigned char)name[i] < 0x20 || name[i] == 0x7f)
			name[i] = '?';
	return name;
}

static int signal_percent(int signal)
{
	/* -100 dBm is the noise floor, -50 dBm as good as it gets */
	int percent = 2 * (signal / 100 + 100);
	return percent < 0 ? 0 : (percent > 100 ? 100 : percent);
}

int CWlanSetup::exec(CMenuTarget *parent, const std::string &actionKey)
{
	if (actionKey == "scan")
	{
		rescan = true;
		reload = true;
		return menu_return::RETURN_EXIT;
	}
	if (actionKey == "hidden")
	{
		connectHidden();
		reload = true;
		return menu_return::RETURN_EXIT;
	}
	if (actionKey == "disconnect")
	{
		disconnect();
		reload = true;
		return menu_return::RETURN_EXIT;
	}
	if (actionKey == "forget")
	{
		forgetSelected();
		reload = true;
		return menu_return::RETURN_EXIT;
	}
	if (!actionKey.empty())
	{
		size_t n = (size_t)atoi(actionKey.c_str());
		if (n < networks.size())
			connectTo(networks[n]);
		reload = true;
		return menu_return::RETURN_EXIT;
	}

	if (parent)
		parent->hide();

	rescan = true;
	return show();
}

int CWlanSetup::show()
{
	int res = menu_return::RETURN_REPAINT;

	do
	{
		reload = false;

		if (rescan)
		{
			CHintBox hint(LOCALE_MESSAGEBOX_INFO, g_Locale->getText(LOCALE_NETWORKMENU_SSID_SCAN_WAIT));
			hint.paint();
			wlan->scan();
			hint.hide();
			/* keys pressed while it took that long are not meant for what comes next */
			g_RCInput->clearRCMsg();
			rescan = false;
		}
		if (!wlan->getNetworks(networks))
		{
			ShowMsg(LOCALE_MESSAGEBOX_ERROR, g_Locale->getText(LOCALE_NETWORKMENU_SSID_SCAN_ERROR), CMsgBox::mbrBack, CMsgBox::mbBack);
			return res;
		}

		std::string connected;
		for (size_t i = 0; i < networks.size(); i++)
			if (networks[i].connected)
				connected = display_name(networks[i].name);

		menu = new CMenuWidget(LOCALE_MAINSETTINGS_NETWORK, NEUTRINO_ICON_NETWORK, width);
		menu->addIntroItems(LOCALE_NETWORKMENU_WLAN);

		CMenuForwarder *mf = new CMenuForwarder(LOCALE_NETWORKMENU_SSID_SCAN, true, NULL, this, "scan", CRCInput::RC_red);
		mf->setHint("", LOCALE_MENU_HINT_NET_SSID_SCAN);
		menu->addItem(mf);
		mf = new CMenuForwarder(LOCALE_NETWORKMENU_WLAN_DISCONNECT, !connected.empty(), connected, this, "disconnect", CRCInput::RC_green);
		menu->addItem(mf);
		mf = new CMenuForwarder(LOCALE_NETWORKMENU_WLAN_HIDDEN, true, NULL, this, "hidden", CRCInput::RC_blue);
		menu->addItem(mf);
		menu->addItem(GenericMenuSeparatorLine);

		first_network = menu->getItemsCount();
		options.assign(networks.size(), "");
		for (size_t i = 0; i < networks.size(); i++)
		{
			char tmp[16];

			snprintf(tmp, sizeof(tmp), "%d%%", signal_percent(networks[i].signal));
			options[i] = tmp;
			if (networks[i].connected)
				options[i] += std::string(", ") + g_Locale->getText(LOCALE_NETWORKMENU_WLAN_CONNECTED);
			else if (!networks[i].known_id.empty())
				options[i] += std::string(", ") + g_Locale->getText(LOCALE_NETWORKMENU_WLAN_KNOWN);

			snprintf(tmp, sizeof(tmp), "%d", (int)i);
			mf = new CMenuForwarder(display_name(networks[i].name), true, options[i], this, tmp,
						CRCInput::RC_nokey, NULL, networks[i].type == "open" ? NULL : NEUTRINO_ICON_MARKER_LOCK);
			mf->setItemButton(NEUTRINO_ICON_BUTTON_OKAY, true);
			menu->addItem(mf, networks[i].connected);
		}

		menu->setFooter(CWlanSetupFooterButtons, CWlanSetupFooterButtonCount);
		menu->addKey(CRCInput::RC_yellow, this, "forget");

		res = menu->exec(NULL, "");

		delete menu;
		menu = NULL;
	}
	while (reload);

	return res;
}

bool CWlanSetup::askPassphrase(std::string &passphrase)
{
	passphrase.clear();
	CKeyboardInput input(LOCALE_NETWORKMENU_PASSWORD, &passphrase, 63);
	input.setMasked(true);
	input.exec(this, "");

	if (passphrase.empty())
		return false;

	/* a WPA passphrase has 8 to 63 characters, the backends refuse anything else */
	if (passphrase.length() < 8 || passphrase.length() > 63)
	{
		CWlanClient::wipe(passphrase);
		ShowMsg(LOCALE_MESSAGEBOX_ERROR, g_Locale->getText(LOCALE_NETWORKMENU_WLAN_PASSPHRASE_INVALID), CMsgBox::mbrBack, CMsgBox::mbBack);
		return false;
	}
	return true;
}

void CWlanSetup::showResult(int result)
{
	switch (result)
	{
		case CWlanClient::CONNECT_OK:
			break;
		case CWlanClient::CONNECT_NOT_SUPPORTED:
			ShowMsg(LOCALE_MESSAGEBOX_ERROR, g_Locale->getText(LOCALE_NETWORKMENU_WLAN_UNSUPPORTED), CMsgBox::mbrBack, CMsgBox::mbBack);
			break;
		default:
			ShowMsg(LOCALE_MESSAGEBOX_ERROR, g_Locale->getText(LOCALE_NETWORKMENU_WLAN_FAILED), CMsgBox::mbrBack, CMsgBox::mbBack);
			break;
	}
}

void CWlanSetup::connectTo(const wireless_network &network)
{
	if (network.connected)
		return;

	if (network.type != "open" && network.type != "psk")
	{
		showResult(CWlanClient::CONNECT_NOT_SUPPORTED);
		return;
	}

	/* the backend has the passphrase of a network it knows */
	std::string passphrase;
	if (network.type == "psk" && network.known_id.empty() && !askPassphrase(passphrase))
		return;

	CHintBox hint(LOCALE_MESSAGEBOX_INFO, g_Locale->getText(LOCALE_NETWORKMENU_WLAN_CONNECTING));
	hint.paint();
	int result = wlan->connect(network, passphrase);
	hint.hide();
	g_RCInput->clearRCMsg();

	showResult(result);
}

void CWlanSetup::connectHidden()
{
	std::string ssid;
	CKeyboardInput input(LOCALE_NETWORKMENU_SSID, &ssid, 32);
	input.exec(this, "");
	if (ssid.empty())
		return;

	/* empty for an open network */
	std::string passphrase;
	CKeyboardInput key_input(LOCALE_NETWORKMENU_PASSWORD, &passphrase, 63);
	key_input.setMasked(true);
	key_input.exec(this, "");
	if (!passphrase.empty() && (passphrase.length() < 8 || passphrase.length() > 63))
	{
		CWlanClient::wipe(passphrase);
		ShowMsg(LOCALE_MESSAGEBOX_ERROR, g_Locale->getText(LOCALE_NETWORKMENU_WLAN_PASSPHRASE_INVALID), CMsgBox::mbrBack, CMsgBox::mbBack);
		return;
	}

	CHintBox hint(LOCALE_MESSAGEBOX_INFO, g_Locale->getText(LOCALE_NETWORKMENU_WLAN_CONNECTING));
	hint.paint();
	int result = wlan->connectHidden(ssid, passphrase);
	hint.hide();
	g_RCInput->clearRCMsg();

	showResult(result);
}

void CWlanSetup::forgetSelected()
{
	if (!menu)
		return;

	int n = menu->getSelected() - first_network;
	if (n < 0 || n >= (int)networks.size() || networks[n].known_id.empty())
		return;

	std::string question = display_name(networks[n].name) + "\n" + g_Locale->getText(LOCALE_NETWORKMENU_WLAN_FORGET_ASK);
	if (ShowMsg(LOCALE_NETWORKMENU_WLAN_FORGET, question, CMsgBox::mbrNo, CMsgBox::mbYes | CMsgBox::mbNo, NEUTRINO_ICON_QUESTION) == CMsgBox::mbrYes)
		wlan->forget(networks[n]);
}

void CWlanSetup::disconnect()
{
	wlan->disconnect();
}
