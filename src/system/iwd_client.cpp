/*
 * Wireless networks through iwd's D-Bus API (net.connman.iwd)
 *
 * iwd keeps the credentials and brings the interface up, here is only what
 * a setup menu needs: scan, list, connect, disconnect, forget.
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
#include <config.h>
#include <cstdio>
#include <cstdlib>
#include <string.h>
#include <unistd.h>

#include <dbus/dbus.h>

#include "iwd_client.h"

#define IWD_SERVICE		"net.connman.iwd"
#define IWD_AGENT_MANAGER_PATH	"/net/connman/iwd"
#define IWD_AGENT_MANAGER	"net.connman.iwd.AgentManager"
#define IWD_AGENT		"net.connman.iwd.Agent"
#define IWD_AGENT_CANCELED	"net.connman.iwd.Agent.Error.Canceled"
#define IWD_DEVICE		"net.connman.iwd.Device"
#define IWD_STATION		"net.connman.iwd.Station"
#define IWD_NETWORK		"net.connman.iwd.Network"
#define IWD_KNOWN_NETWORK	"net.connman.iwd.KnownNetwork"
#define AGENT_PATH		"/org/tuxbox/neutrino/iwd_agent"
/* iwd gives up on a connection attempt long before this */
#define CONNECT_TIMEOUT_MS	90000

CIwdClient::CIwdClient() : bus(IWD_SERVICE)
{
	agent_registered = false;
	agent_conn = NULL;
}

CIwdClient::~CIwdClient()
{
}

CIwdClient *CIwdClient::getInstance()
{
	static CIwdClient *client = NULL;

	if (!client)
		client = new CIwdClient();
	return client;
}

void CIwdClient::wipe(std::string &secret)
{
	volatile char *p = secret.empty() ? NULL : &secret[0];
	for (size_t i = 0; p && i < secret.length(); i++)
		p[i] = 0;
	secret.clear();
}

/* the tree anew, and the station in it */
bool CIwdClient::refresh()
{
	station_path.clear();
	if (!bus.refresh())
		return false;
	station_path = bus.find(IWD_STATION);
	return true;
}

bool CIwdClient::available()
{
	return refresh() && !station_path.empty();
}

std::string CIwdClient::deviceName()
{
	return bus.prop(station_path, IWD_DEVICE, "Name");
}

std::string CIwdClient::connectedNetwork()
{
	if (!available())
		return "";

	std::string network = bus.prop(station_path, IWD_STATION, "ConnectedNetwork");
	return network.empty() ? "" : bus.prop(network, IWD_NETWORK, "Name");
}

bool CIwdClient::scan(int timeout_ms)
{
	if (!available())
		return false;

	/* "busy" means a scan is running already, waiting for it is just as good */
	DBusMessage *reply = bus.call(station_path, IWD_STATION, "Scan");
	if (reply)
		dbus_message_unref(reply);

	for (int waited = 0; waited < timeout_ms; waited += 250)
	{
		usleep(250000);
		if (!refresh())
			return false;
		if (bus.prop(station_path, IWD_STATION, "Scanning") != "1")
			return true;
	}
	return true;
}

bool CIwdClient::getNetworks(std::vector<iwd_network> &networks)
{
	networks.clear();
	if (!available())
		return false;

	/* a(on): the networks in the order iwd prefers them, with their signal */
	DBusMessage *reply = bus.call(station_path, IWD_STATION, "GetOrderedNetworks");
	if (!reply)
		return false;

	DBusMessageIter top, entry;
	if (dbus_message_iter_init(reply, &top) && dbus_message_iter_get_arg_type(&top) == DBUS_TYPE_ARRAY)
	{
		for (dbus_message_iter_recurse(&top, &entry); dbus_message_iter_get_arg_type(&entry) == DBUS_TYPE_STRUCT; dbus_message_iter_next(&entry))
		{
			DBusMessageIter fields;
			const char *path = NULL;
			dbus_int16_t signal = 0;

			dbus_message_iter_recurse(&entry, &fields);
			if (dbus_message_iter_get_arg_type(&fields) != DBUS_TYPE_OBJECT_PATH)
				continue;
			dbus_message_iter_get_basic(&fields, &path);
			if (dbus_message_iter_next(&fields) && dbus_message_iter_get_arg_type(&fields) == DBUS_TYPE_INT16)
				dbus_message_iter_get_basic(&fields, &signal);

			iwd_network n;
			n.path = path;
			n.name = bus.prop(n.path, IWD_NETWORK, "Name");
			n.type = bus.prop(n.path, IWD_NETWORK, "Type");
			n.known_path = bus.prop(n.path, IWD_NETWORK, "KnownNetwork");
			n.connected = bus.prop(n.path, IWD_NETWORK, "Connected") == "1";
			n.signal = signal;
			if (!n.name.empty())
				networks.push_back(n);
		}
	}
	dbus_message_unref(reply);
	return true;
}

bool CIwdClient::disconnect()
{
	return available() && bus.simpleCall(station_path, IWD_STATION, "Disconnect");
}

bool CIwdClient::forget(const iwd_network &network)
{
	if (network.known_path.empty())
		return false;
	return bus.simpleCall(network.known_path, IWD_KNOWN_NETWORK, "Forget");
}

/* iwd calls back for the passphrase while it connects */
DBusMessage *CIwdClient::agentRequest(DBusMessage *msg)
{
	/* only the owner of the iwd name gets an answer */
	const char *sender = dbus_message_get_sender(msg);
	if (!sender || iwd_owner.empty() || iwd_owner != sender)
		return dbus_message_new_error(msg, DBUS_ERROR_ACCESS_DENIED, "not iwd");

	if (dbus_message_is_method_call(msg, IWD_AGENT, "RequestPassphrase"))
	{
		const char *network = NULL;
		if (!dbus_message_get_args(msg, NULL, DBUS_TYPE_OBJECT_PATH, &network, DBUS_TYPE_INVALID) ||
		    pending_passphrase.empty() ||
		    (!pending_network.empty() && pending_network != network))
			return dbus_message_new_error(msg, IWD_AGENT_CANCELED, "no passphrase");

		DBusMessage *reply = dbus_message_new_method_return(msg);
		const char *passphrase = pending_passphrase.c_str();
		if (reply)
			dbus_message_append_args(reply, DBUS_TYPE_STRING, &passphrase, DBUS_TYPE_INVALID);
		return reply;
	}
	if (dbus_message_is_method_call(msg, IWD_AGENT, "Release"))
	{
		agent_registered = false;
		return dbus_message_new_method_return(msg);
	}
	if (dbus_message_is_method_call(msg, IWD_AGENT, "Cancel"))
		return dbus_message_new_method_return(msg);

	/* user names and private keys of enterprise networks are not handled */
	return dbus_message_new_error(msg, IWD_AGENT_CANCELED, "not supported");
}

static DBusHandlerResult agent_message(DBusConnection *connection, DBusMessage *msg, void *data)
{
	if (dbus_message_get_type(msg) != DBUS_MESSAGE_TYPE_METHOD_CALL)
		return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;

	DBusMessage *reply = ((CIwdClient *)data)->agentRequest(msg);
	if (reply)
	{
		if (!dbus_message_get_no_reply(msg))
			dbus_connection_send(connection, reply, NULL);
		dbus_message_unref(reply);
	}
	return DBUS_HANDLER_RESULT_HANDLED;
}

bool CIwdClient::registerAgent()
{
	if (!bus.open())
		return false;

	DBusConnection *conn = bus.connection();
	if (agent_registered && agent_conn == conn)
		return true;
	agent_registered = false;

	/* remember who iwd is, the agent answers nobody else */
	iwd_owner = bus.owner();
	if (iwd_owner.empty())
		return false;

	static const DBusObjectPathVTable vtable = { NULL, agent_message, NULL, NULL, NULL, NULL };
	void *registered = NULL;
	if (!dbus_connection_get_object_path_data(conn, AGENT_PATH, &registered) || !registered)
	{
		if (!dbus_connection_register_object_path(conn, AGENT_PATH, &vtable, this))
			return false;
	}

	if (!bus.simpleCall(IWD_AGENT_MANAGER_PATH, IWD_AGENT_MANAGER, "RegisterAgent", AGENT_PATH, true))
		return false;

	agent_registered = true;
	agent_conn = conn;
	return true;
}

void CIwdClient::unregisterAgent()
{
	if (!agent_registered || agent_conn != bus.connection())
		return;

	bus.simpleCall(IWD_AGENT_MANAGER_PATH, IWD_AGENT_MANAGER, "UnregisterAgent", AGENT_PATH, true);
	agent_registered = false;
}

int CIwdClient::connectCall(const std::string &path, const char *iface, const char *method, const char *arg, std::string &passphrase, const std::string &network)
{
	int result = CONNECT_FAILED;

	if (!registerAgent())
	{
		wipe(passphrase);
		return CONNECT_UNAVAILABLE;
	}

	pending_passphrase = passphrase;
	pending_network = network;
	wipe(passphrase);

	/* the agent is served while the reply is outstanding */
	std::string error;
	if (bus.callDispatching(path, iface, method, arg, CONNECT_TIMEOUT_MS, error))
		result = CONNECT_OK;
	else if (error == IWD_SERVICE ".NotSupported" || error == IWD_SERVICE ".NotConfigured")
		result = CONNECT_NOT_SUPPORTED;

	wipe(pending_passphrase);
	pending_network.clear();
	unregisterAgent();
	return result;
}

int CIwdClient::connect(const iwd_network &network, std::string &passphrase)
{
	if (!available())
	{
		wipe(passphrase);
		return CONNECT_UNAVAILABLE;
	}
	return connectCall(network.path, IWD_NETWORK, "Connect", NULL, passphrase, network.path);
}

int CIwdClient::connectHidden(const std::string &ssid, std::string &passphrase)
{
	if (!available())
	{
		wipe(passphrase);
		return CONNECT_UNAVAILABLE;
	}
	/* the network object does not exist yet, so any request is ours */
	return connectCall(station_path, IWD_STATION, "ConnectHiddenNetwork", ssid.c_str(), passphrase, "");
}
