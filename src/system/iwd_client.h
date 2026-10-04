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

struct DBusConnection;
struct DBusMessage;

struct iwd_network
{
	std::string path;	/* the network's D-Bus object, what every call refers to */
	std::string name;	/* SSID, for display only */
	std::string type;	/* open, psk, 8021x, wep */
	std::string known_path;	/* set when iwd has stored the network */
	int signal;		/* 100 * dBm */
	bool connected;
};

class CIwdClient
{
	public:
		enum
		{
			CONNECT_OK,
			CONNECT_FAILED,
			CONNECT_NOT_SUPPORTED,
			CONNECT_UNAVAILABLE
		};

		static CIwdClient *getInstance();

		/* iwd answers on the system bus, lets us in and has a wireless device */
		bool available();
		std::string deviceName();
		/* SSID of the connected network, empty when there is none */
		std::string connectedNetwork();

		bool scan(int timeout_ms = 15000);
		bool getNetworks(std::vector<iwd_network> &networks);

		/* the passphrase is only handed to iwd when it asks for it, an empty
		 * one is right for open and for stored networks */
		int connect(const iwd_network &network, std::string &passphrase);
		int connectHidden(const std::string &ssid, std::string &passphrase);
		bool disconnect();
		bool forget(const iwd_network &network);

		/* overwrite a string that held a secret before releasing it */
		static void wipe(std::string &secret);

		/* entry point of the D-Bus dispatcher: iwd asks its agent */
		DBusMessage *agentRequest(DBusMessage *msg);

	private:
		typedef std::map<std::string, std::string> props_t;
		typedef std::map<std::string, props_t> ifaces_t;
		typedef std::map<std::string, ifaces_t> objects_t;

		DBusConnection *conn;
		objects_t objects;
		std::string station_path;
		std::string iwd_owner;
		std::string pending_passphrase;
		std::string pending_network;
		bool agent_registered;

		CIwdClient();
		~CIwdClient();

		bool open();
		void close();
		bool refresh();
		std::string prop(const std::string &path, const char *iface, const char *name);
		DBusMessage *call(const std::string &path, const char *iface, const char *method, const char *arg = NULL, bool arg_is_path = false, int timeout_ms = 10000);
		bool simpleCall(const std::string &path, const char *iface, const char *method);
		bool registerAgent();
		void unregisterAgent();
		int connectCall(const std::string &path, const char *iface, const char *method, const char *arg, std::string &passphrase, const std::string &network);
};

#endif /* __iwd_client_h__ */
