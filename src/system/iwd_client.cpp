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
#define DBUS_OBJECT_MANAGER	"org.freedesktop.DBus.ObjectManager"
#define DBUS_PROPERTIES		"org.freedesktop.DBus.Properties"
#define AGENT_PATH		"/org/tuxbox/neutrino/iwd_agent"
/* iwd gives up on a connection attempt long before this */
#define CONNECT_TIMEOUT_MS	90000

CIwdClient::CIwdClient()
{
	conn = NULL;
	agent_registered = false;
}

CIwdClient::~CIwdClient()
{
	close();
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

bool CIwdClient::open()
{
	if (conn && dbus_connection_get_is_connected(conn))
		return true;

	close();

	DBusError err;
	dbus_error_init(&err);
	/* a connection of our own, nothing else in the process dispatches it */
	conn = dbus_bus_get_private(DBUS_BUS_SYSTEM, &err);
	if (!conn)
	{
		dbus_error_free(&err);
		return false;
	}
	dbus_connection_set_exit_on_disconnect(conn, FALSE);
	return true;
}

void CIwdClient::close()
{
	if (!conn)
		return;

	dbus_connection_close(conn);
	dbus_connection_unref(conn);
	conn = NULL;
	agent_registered = false;
	objects.clear();
	station_path.clear();
}

DBusMessage *CIwdClient::call(const std::string &path, const char *iface, const char *method, const char *arg, bool arg_is_path, int timeout_ms)
{
	if (!open())
		return NULL;

	DBusMessage *msg = dbus_message_new_method_call(IWD_SERVICE, path.c_str(), iface, method);
	if (!msg)
		return NULL;

	if (arg && !dbus_message_append_args(msg, arg_is_path ? DBUS_TYPE_OBJECT_PATH : DBUS_TYPE_STRING, &arg, DBUS_TYPE_INVALID))
	{
		dbus_message_unref(msg);
		return NULL;
	}

	DBusError err;
	dbus_error_init(&err);
	DBusMessage *reply = dbus_connection_send_with_reply_and_block(conn, msg, timeout_ms, &err);
	dbus_message_unref(msg);
	if (!reply)
	{
		if (strcmp(method, "GetManagedObjects"))
			printf("[iwd] %s.%s: %s\n", iface, method, err.name ? err.name : "failed");
		dbus_error_free(&err);
	}
	return reply;
}

bool CIwdClient::simpleCall(const std::string &path, const char *iface, const char *method)
{
	DBusMessage *reply = call(path, iface, method);
	if (!reply)
		return false;

	dbus_message_unref(reply);
	return true;
}

static std::string variant_to_string(DBusMessageIter *variant)
{
	char buf[32];

	switch (dbus_message_iter_get_arg_type(variant))
	{
		case DBUS_TYPE_STRING:
		case DBUS_TYPE_OBJECT_PATH:
		{
			const char *s = NULL;
			dbus_message_iter_get_basic(variant, &s);
			return s ? s : "";
		}
		case DBUS_TYPE_BOOLEAN:
		{
			dbus_bool_t b = FALSE;
			dbus_message_iter_get_basic(variant, &b);
			return b ? "1" : "0";
		}
		case DBUS_TYPE_INT16:
		{
			dbus_int16_t i = 0;
			dbus_message_iter_get_basic(variant, &i);
			snprintf(buf, sizeof(buf), "%d", (int)i);
			return buf;
		}
		default:
			return "";
	}
}

/* read the whole object tree, a{oa{sa{sv}}}, in one go */
bool CIwdClient::refresh()
{
	objects.clear();
	station_path.clear();

	DBusMessage *reply = call("/", DBUS_OBJECT_MANAGER, "GetManagedObjects");
	if (!reply)
		return false;

	DBusMessageIter top, obj;
	if (!dbus_message_iter_init(reply, &top) || dbus_message_iter_get_arg_type(&top) != DBUS_TYPE_ARRAY)
	{
		dbus_message_unref(reply);
		return false;
	}

	for (dbus_message_iter_recurse(&top, &obj); dbus_message_iter_get_arg_type(&obj) == DBUS_TYPE_DICT_ENTRY; dbus_message_iter_next(&obj))
	{
		DBusMessageIter obj_entry, iface;
		const char *path = NULL;

		dbus_message_iter_recurse(&obj, &obj_entry);
		if (dbus_message_iter_get_arg_type(&obj_entry) != DBUS_TYPE_OBJECT_PATH)
			continue;
		dbus_message_iter_get_basic(&obj_entry, &path);
		if (!dbus_message_iter_next(&obj_entry) || dbus_message_iter_get_arg_type(&obj_entry) != DBUS_TYPE_ARRAY)
			continue;

		for (dbus_message_iter_recurse(&obj_entry, &iface); dbus_message_iter_get_arg_type(&iface) == DBUS_TYPE_DICT_ENTRY; dbus_message_iter_next(&iface))
		{
			DBusMessageIter iface_entry, property;
			const char *iface_name = NULL;

			dbus_message_iter_recurse(&iface, &iface_entry);
			if (dbus_message_iter_get_arg_type(&iface_entry) != DBUS_TYPE_STRING)
				continue;
			dbus_message_iter_get_basic(&iface_entry, &iface_name);
			if (!dbus_message_iter_next(&iface_entry) || dbus_message_iter_get_arg_type(&iface_entry) != DBUS_TYPE_ARRAY)
				continue;

			props_t &props = objects[path][iface_name];
			for (dbus_message_iter_recurse(&iface_entry, &property); dbus_message_iter_get_arg_type(&property) == DBUS_TYPE_DICT_ENTRY; dbus_message_iter_next(&property))
			{
				DBusMessageIter property_entry, variant;
				const char *name = NULL;

				dbus_message_iter_recurse(&property, &property_entry);
				if (dbus_message_iter_get_arg_type(&property_entry) != DBUS_TYPE_STRING)
					continue;
				dbus_message_iter_get_basic(&property_entry, &name);
				if (!dbus_message_iter_next(&property_entry) || dbus_message_iter_get_arg_type(&property_entry) != DBUS_TYPE_VARIANT)
					continue;
				dbus_message_iter_recurse(&property_entry, &variant);
				props[name] = variant_to_string(&variant);
			}

			if (!strcmp(iface_name, IWD_STATION) && station_path.empty())
				station_path = path;
		}
	}
	dbus_message_unref(reply);
	return true;
}

std::string CIwdClient::prop(const std::string &path, const char *iface, const char *name)
{
	objects_t::const_iterator o = objects.find(path);
	if (o == objects.end())
		return "";
	ifaces_t::const_iterator i = o->second.find(iface);
	if (i == o->second.end())
		return "";
	props_t::const_iterator p = i->second.find(name);
	return p == i->second.end() ? "" : p->second;
}

bool CIwdClient::available()
{
	return refresh() && !station_path.empty();
}

std::string CIwdClient::deviceName()
{
	return prop(station_path, IWD_DEVICE, "Name");
}

std::string CIwdClient::connectedNetwork()
{
	if (!available())
		return "";

	std::string network = prop(station_path, IWD_STATION, "ConnectedNetwork");
	return network.empty() ? "" : prop(network, IWD_NETWORK, "Name");
}

bool CIwdClient::scan(int timeout_ms)
{
	if (!available())
		return false;

	/* "busy" means a scan is running already, waiting for it is just as good */
	DBusMessage *reply = call(station_path, IWD_STATION, "Scan");
	if (reply)
		dbus_message_unref(reply);

	for (int waited = 0; waited < timeout_ms; waited += 250)
	{
		usleep(250000);
		if (!refresh())
			return false;
		if (prop(station_path, IWD_STATION, "Scanning") != "1")
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
	DBusMessage *reply = call(station_path, IWD_STATION, "GetOrderedNetworks");
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
			n.name = prop(n.path, IWD_NETWORK, "Name");
			n.type = prop(n.path, IWD_NETWORK, "Type");
			n.known_path = prop(n.path, IWD_NETWORK, "KnownNetwork");
			n.connected = prop(n.path, IWD_NETWORK, "Connected") == "1";
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
	return available() && simpleCall(station_path, IWD_STATION, "Disconnect");
}

bool CIwdClient::forget(const iwd_network &network)
{
	if (network.known_path.empty())
		return false;
	return simpleCall(network.known_path, IWD_KNOWN_NETWORK, "Forget");
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
	if (!open())
		return false;
	if (agent_registered)
		return true;

	/* remember who iwd is, the agent answers nobody else */
	iwd_owner.clear();
	DBusMessage *msg = dbus_message_new_method_call(DBUS_SERVICE_DBUS, DBUS_PATH_DBUS, DBUS_INTERFACE_DBUS, "GetNameOwner");
	const char *name = IWD_SERVICE;
	if (!msg)
		return false;
	dbus_message_append_args(msg, DBUS_TYPE_STRING, &name, DBUS_TYPE_INVALID);
	DBusMessage *reply = dbus_connection_send_with_reply_and_block(conn, msg, 5000, NULL);
	dbus_message_unref(msg);
	if (reply)
	{
		const char *owner = NULL;
		if (dbus_message_get_args(reply, NULL, DBUS_TYPE_STRING, &owner, DBUS_TYPE_INVALID) && owner)
			iwd_owner = owner;
		dbus_message_unref(reply);
	}
	if (iwd_owner.empty())
		return false;

	static const DBusObjectPathVTable vtable = { NULL, agent_message, NULL, NULL, NULL, NULL };
	void *registered = NULL;
	if (!dbus_connection_get_object_path_data(conn, AGENT_PATH, &registered) || !registered)
	{
		if (!dbus_connection_register_object_path(conn, AGENT_PATH, &vtable, this))
			return false;
	}

	reply = call(IWD_AGENT_MANAGER_PATH, IWD_AGENT_MANAGER, "RegisterAgent", AGENT_PATH, true);
	if (!reply)
		return false;

	dbus_message_unref(reply);
	agent_registered = true;
	return true;
}

void CIwdClient::unregisterAgent()
{
	if (!conn || !agent_registered)
		return;

	DBusMessage *reply = call(IWD_AGENT_MANAGER_PATH, IWD_AGENT_MANAGER, "UnregisterAgent", AGENT_PATH, true);
	if (reply)
		dbus_message_unref(reply);
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

	DBusMessage *msg = dbus_message_new_method_call(IWD_SERVICE, path.c_str(), iface, method);
	DBusPendingCall *pending = NULL;
	if (msg && arg)
		dbus_message_append_args(msg, DBUS_TYPE_STRING, &arg, DBUS_TYPE_INVALID);

	if (msg && dbus_connection_send_with_reply(conn, msg, &pending, CONNECT_TIMEOUT_MS) && pending)
	{
		/* the agent is served from this loop while the reply is outstanding */
		while (!dbus_pending_call_get_completed(pending))
		{
			if (!dbus_connection_read_write_dispatch(conn, 100))
				break;
		}

		DBusMessage *reply = dbus_pending_call_steal_reply(pending);
		if (reply)
		{
			if (dbus_message_get_type(reply) == DBUS_MESSAGE_TYPE_METHOD_RETURN)
				result = CONNECT_OK;
			else
			{
				const char *error = dbus_message_get_error_name(reply);
				printf("[iwd] %s: %s\n", method, error ? error : "failed");
				if (error && (!strcmp(error, IWD_SERVICE ".NotSupported") || !strcmp(error, IWD_SERVICE ".NotConfigured")))
					result = CONNECT_NOT_SUPPORTED;
			}
			dbus_message_unref(reply);
		}
		dbus_pending_call_unref(pending);
	}
	if (msg)
		dbus_message_unref(msg);

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
