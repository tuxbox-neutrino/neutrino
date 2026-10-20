#ifndef __iwd_client_h__
#define __iwd_client_h__

/*
 * Wireless networks through iwd's D-Bus API (net.connman.iwd)
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

#include "dbus_objects.h"
#include "wlan_client.h"

struct DBusConnection;
struct DBusMessage;

/* a network's id is its D-Bus object, its known_id the one of the stored network */
class CIwdClient : public CWlanClient
{
	public:
		static CIwdClient *getInstance();

		/* iwd answers on the system bus, lets us in and has a wireless device */
		bool available();
		std::string deviceName();
		/* SSID of the connected network, empty when there is none */
		std::string connectedNetwork();

		bool scan(int timeout_ms = 15000);
		bool getNetworks(std::vector<wireless_network> &networks);

		/* the passphrase is only handed to iwd when it asks for it, an empty
		 * one is right for open and for stored networks */
		int connect(const wireless_network &network, std::string &passphrase);
		int connectHidden(const std::string &ssid, std::string &passphrase);
		bool disconnect();
		bool forget(const wireless_network &network);

		/* entry point of the D-Bus dispatcher: iwd asks its agent */
		DBusMessage *agentRequest(DBusMessage *msg);

	private:
		CDBusObjects bus;
		DBusConnection *agent_conn;
		std::string station_path;
		std::string iwd_owner;
		std::string pending_passphrase;
		std::string pending_network;
		bool agent_registered;

		CIwdClient();
		~CIwdClient();

		bool refresh();
		bool registerAgent();
		void unregisterAgent();
		int connectCall(const std::string &path, const char *iface, const char *method, const char *arg, std::string &passphrase, const std::string &network);
};

#endif /* __iwd_client_h__ */
