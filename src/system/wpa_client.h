#ifndef __wpa_client_h__
#define __wpa_client_h__

/*
 * Wireless networks through the control interface of wpa_supplicant
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

#include <map>
#include <string>
#include <vector>

#include "wlan_client.h"

/* a network's id is its SSID, its known_id the number of the stored network */
class CWpaClient : public CWlanClient
{
	public:
		static CWpaClient *getInstance();

		bool available();
		std::string deviceName();
		std::string connectedNetwork();

		bool scan(int timeout_ms = 15000);
		bool getNetworks(std::vector<wireless_network> &networks);

		int connect(const wireless_network &network, std::string &passphrase);
		int connectHidden(const std::string &ssid, std::string &passphrase);
		bool disconnect();
		bool forget(const wireless_network &network);

	private:
		std::string ifname;
		std::string ctrl_path;
		/* the flags of the last scan by SSID, they tell PSK from SAE */
		std::map<std::string, std::string> flags;

		CWpaClient() {}

		bool findInterface();
		int open(bool attach);
		void close(int fd);
		bool request(int fd, const std::string &cmd, std::string &reply, int timeout_ms = 10000);
		bool request(const std::string &cmd, std::string &reply, int timeout_ms = 10000);
		bool requestOk(const std::string &cmd);
		bool waitEvent(int fd, const std::vector<std::string> &events, std::string &event, int timeout_ms);
		std::map<std::string, std::string> status();
		int addNetwork(const std::string &ssid, std::string &passphrase, const std::string &type, bool hidden);
		int select(int fd, const std::string &id, bool added);
		void startDhcp();
};

#endif /* __wpa_client_h__ */
