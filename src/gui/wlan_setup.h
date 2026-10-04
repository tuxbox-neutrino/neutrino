/*
	wireless network setup through iwd - Neutrino-GUI

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

#ifndef __wlan_setup__
#define __wlan_setup__

#include <gui/widget/menue.h>
#include <system/iwd_client.h>

#include <string>
#include <vector>

class CWlanSetup : public CMenuTarget
{
	private:
		CIwdClient *iwd;
		CMenuWidget *menu;
		std::vector<iwd_network> networks;
		std::vector<std::string> options;
		int width;
		int first_network;
		bool reload;
		bool rescan;

		int show();
		void connectTo(const iwd_network &network);
		void connectHidden();
		void forgetSelected();
		void disconnect();
		bool askPassphrase(std::string &passphrase);
		void showResult(int result);

	public:
		CWlanSetup();

		int exec(CMenuTarget *parent, const std::string &actionKey);

		/* there is a wireless device and iwd lets us use it */
		static bool available();
		/* name of the connected network for the menu entry */
		static std::string connectedNetwork();
};

#endif
