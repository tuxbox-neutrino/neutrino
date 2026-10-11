#ifndef __wlan_client_h__
#define __wlan_client_h__

/*
 * Wireless networks for the setup menu, through iwd or wpa_supplicant
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <string>
#include <vector>

struct wireless_network
{
	std::string id;		/* what the backend knows the network by */
	std::string name;	/* SSID, for display only */
	std::string type;	/* open, psk, 8021x, wep */
	std::string known_id;	/* set when the backend has stored the network */
	int signal;		/* 100 * dBm */
	bool connected;
};

class CWlanClient
{
	public:
		enum
		{
			CONNECT_OK,
			CONNECT_FAILED,
			CONNECT_NOT_SUPPORTED,
			CONNECT_UNAVAILABLE
		};

		/* the backend neutrino was built with */
		static CWlanClient *getInstance();
		virtual ~CWlanClient() {}

		/* the backend answers and has a wireless device */
		virtual bool available() = 0;
		virtual std::string deviceName() = 0;
		/* SSID of the connected network, empty when there is none */
		virtual std::string connectedNetwork() = 0;

		virtual bool scan(int timeout_ms = 15000) = 0;
		virtual bool getNetworks(std::vector<wireless_network> &networks) = 0;

		/* an empty passphrase is right for open and for stored networks;
		 * it is wiped once it has been handed on */
		virtual int connect(const wireless_network &network, std::string &passphrase) = 0;
		virtual int connectHidden(const std::string &ssid, std::string &passphrase) = 0;
		virtual bool disconnect() = 0;
		virtual bool forget(const wireless_network &network) = 0;

		/* overwrite a string that held a secret before releasing it */
		static void wipe(std::string &secret);
};

#endif /* __wlan_client_h__ */
