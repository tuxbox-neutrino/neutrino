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

#ifndef __bluetooth_setup__
#define __bluetooth_setup__

#include <gui/widget/menue.h>
#include <system/bluez_client.h>

#include <string>
#include <vector>

class CHintBox;

class CBluetoothSetup : public CMenuTarget, public CBluezPairingUI
{
	private:
		CBluezClient *bluez;
		CMenuWidget *menu;
		CHintBox *hint;
		std::vector<bluez_device> devices;
		std::vector<std::string> options;
		std::string shown_code;
		std::string power_state;
		std::string select_path;
		int width;
		int first_device;
		bool reload;
		bool rescan;

		int show();
		void togglePower();
		void activate(const bluez_device &device);
		void removeSelected();
		void showResult(int result);
		void showHint(const std::string &text);
		void hideHint();

		/* CBluezPairingUI */
		void displayCode(const std::string &code);
		void hideCode();
		bool confirmCode(const std::string &code);
		bool requestPin(std::string &pin);

	public:
		CBluetoothSetup();
		~CBluetoothSetup();

		int exec(CMenuTarget *parent, const std::string &actionKey);

		/* there is an adapter and BlueZ lets us use it */
		static bool available();
		/* "off", or the connected devices, for the menu entry */
		static std::string status();
};

#endif
