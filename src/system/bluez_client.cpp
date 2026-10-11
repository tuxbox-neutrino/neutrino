/*
 * Bluetooth devices through BlueZ
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
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <string.h>
#include <unistd.h>

#include <dbus/dbus.h>

#include "bluez_client.h"

#define BLUEZ_SERVICE		"org.bluez"
#define BLUEZ_ADAPTER		"org.bluez.Adapter1"
#define BLUEZ_DEVICE		"org.bluez.Device1"
#define BLUEZ_AGENT		"org.bluez.Agent1"
#define BLUEZ_AGENT_MANAGER	"org.bluez.AgentManager1"
#define BLUEZ_AGENT_MANAGER_PATH "/org/bluez"
#define BLUEZ_ERROR		"org.bluez.Error."
#define AGENT_PATH		"/org/tuxbox/neutrino/bluez_agent"
#define AGENT_CAPABILITY	"KeyboardDisplay"
/* BlueZ gives up on its own before these run out */
#define PAIR_TIMEOUT_MS		90000
#define CONNECT_TIMEOUT_MS	45000

CBluezClient::CBluezClient() : bus(BLUEZ_SERVICE)
{
	agent_conn = NULL;
	pairing_ui = NULL;
	agent_registered = false;
}

CBluezClient::~CBluezClient()
{
}

CBluezClient *CBluezClient::getInstance()
{
	static CBluezClient *client = NULL;

	if (!client)
		client = new CBluezClient();
	return client;
}

/* the tree anew, and the adapter in it */
bool CBluezClient::refresh()
{
	adapter_path.clear();
	if (!bus.refresh())
		return false;
	adapter_path = bus.find(BLUEZ_ADAPTER);
	return true;
}

bool CBluezClient::available()
{
	return refresh() && !adapter_path.empty();
}

bool CBluezClient::powered()
{
	return available() && bus.prop(adapter_path, BLUEZ_ADAPTER, "Powered") == "1";
}

bool CBluezClient::setPowered(bool on)
{
	if (!available())
		return false;
	if (!bus.setProperty(adapter_path, BLUEZ_ADAPTER, "Powered", on))
		return false;

	/* the controller takes a moment to follow */
	for (int i = 0; i < 20; i++)
	{
		if (powered() == on)
			return true;
		usleep(100000);
	}
	return false;
}

bool CBluezClient::scan(int seconds)
{
	if (!powered())
		return false;

	std::string error;
	if (!bus.simpleCall(adapter_path, BLUEZ_ADAPTER, "StartDiscovery", NULL, false, &error) &&
	    error != BLUEZ_ERROR "InProgress")
		return false;

	sleep(seconds);

	bus.simpleCall(adapter_path, BLUEZ_ADAPTER, "StopDiscovery");
	return true;
}

int CBluezClient::kindOf(const std::string &icon)
{
	if (icon.compare(0, 6, "audio-") == 0)
		return bluez_device::KIND_AUDIO;
	if (icon == "input-keyboard")
		return bluez_device::KIND_KEYBOARD;
	if (icon == "input-mouse" || icon == "input-tablet")
		return bluez_device::KIND_MOUSE;
	if (icon == "input-gaming")
		return bluez_device::KIND_GAMEPAD;
	if (icon.compare(0, 6, "input-") == 0)
		return bluez_device::KIND_INPUT;
	return bluez_device::KIND_OTHER;
}

/* phones, computers, printers and the like are nothing to connect a set-top
 * box to; a device that does not say what it is gets the benefit of the doubt */
bool CBluezClient::wanted(const std::string &icon)
{
	return icon.empty() || kindOf(icon) != bluez_device::KIND_OTHER;
}

static bool device_order(const bluez_device &a, const bluez_device &b)
{
	if (a.paired != b.paired)
		return a.paired;
	if (a.has_signal != b.has_signal)
		return a.has_signal;
	if (a.has_signal && a.signal != b.signal)
		return a.signal > b.signal;
	return a.name < b.name;
}

bool CBluezClient::getDevices(std::vector<bluez_device> &devices)
{
	devices.clear();
	if (!available())
		return false;

	const std::string prefix = adapter_path + "/";
	const CDBusObjects::objects_t &objects = bus.objects();
	for (CDBusObjects::objects_t::const_iterator o = objects.begin(); o != objects.end(); ++o)
	{
		if (o->first.compare(0, prefix.length(), prefix) != 0)
			continue;
		CDBusObjects::ifaces_t::const_iterator i = o->second.find(BLUEZ_DEVICE);
		if (i == o->second.end())
			continue;

		const std::string &path = o->first;
		std::string icon = bus.prop(path, BLUEZ_DEVICE, "Icon");
		std::string rssi = bus.prop(path, BLUEZ_DEVICE, "RSSI");
		bluez_device d;
		d.path = path;
		d.address = bus.prop(path, BLUEZ_DEVICE, "Address");
		d.name = bus.prop(path, BLUEZ_DEVICE, "Name");
		d.kind = kindOf(icon);
		d.paired = bus.prop(path, BLUEZ_DEVICE, "Paired") == "1";
		d.trusted = bus.prop(path, BLUEZ_DEVICE, "Trusted") == "1";
		d.connected = bus.prop(path, BLUEZ_DEVICE, "Connected") == "1";
		d.has_signal = !rssi.empty();
		d.signal = atoi(rssi.c_str());

		if (d.paired)
		{
			if (d.name.empty())
				d.name = d.address;
		}
		/* beacons and other things without a name are not for us */
		else if (d.name.empty() || !wanted(icon))
			continue;

		devices.push_back(d);
	}
	std::sort(devices.begin(), devices.end(), device_order);
	return true;
}

int CBluezClient::connectedCount()
{
	std::vector<bluez_device> devices;
	int count = 0;

	if (!getDevices(devices))
		return 0;
	for (size_t i = 0; i < devices.size(); i++)
		if (devices[i].connected)
			count++;
	return count;
}

static DBusMessage *agent_error(DBusMessage *msg, const char *name)
{
	return dbus_message_new_error(msg, name, "");
}

/* BlueZ calls back while it pairs */
DBusMessage *CBluezClient::agentRequest(DBusMessage *msg)
{
	/* only the owner of the BlueZ name gets an answer */
	const char *sender = dbus_message_get_sender(msg);
	if (!sender || bluez_owner.empty() || bluez_owner != sender)
		return dbus_message_new_error(msg, DBUS_ERROR_ACCESS_DENIED, "not BlueZ");

	if (dbus_message_is_method_call(msg, BLUEZ_AGENT, "Release"))
	{
		agent_registered = false;
		return dbus_message_new_method_return(msg);
	}
	if (dbus_message_is_method_call(msg, BLUEZ_AGENT, "Cancel"))
	{
		if (pairing_ui)
			pairing_ui->hideCode();
		return dbus_message_new_method_return(msg);
	}

	/* everything else is about a device: only the one we are pairing */
	DBusMessageIter args;
	const char *device = NULL;
	if (!dbus_message_iter_init(msg, &args) || dbus_message_iter_get_arg_type(&args) != DBUS_TYPE_OBJECT_PATH)
		return agent_error(msg, BLUEZ_ERROR "Rejected");
	dbus_message_iter_get_basic(&args, &device);
	if (!device || pairing_path.empty() || pairing_path != device || !pairing_ui)
		return agent_error(msg, BLUEZ_ERROR "Rejected");
	dbus_message_iter_next(&args);

	char code[16];

	if (dbus_message_is_method_call(msg, BLUEZ_AGENT, "RequestPinCode"))
	{
		std::string pin;
		if (!pairing_ui->requestPin(pin) || pin.empty() || pin.length() > 16)
			return agent_error(msg, BLUEZ_ERROR "Canceled");

		DBusMessage *reply = dbus_message_new_method_return(msg);
		const char *p = pin.c_str();
		if (reply)
			dbus_message_append_args(reply, DBUS_TYPE_STRING, &p, DBUS_TYPE_INVALID);
		return reply;
	}
	if (dbus_message_is_method_call(msg, BLUEZ_AGENT, "DisplayPinCode"))
	{
		const char *pin = NULL;
		if (dbus_message_iter_get_arg_type(&args) != DBUS_TYPE_STRING)
			return agent_error(msg, BLUEZ_ERROR "Rejected");
		dbus_message_iter_get_basic(&args, &pin);
		pairing_ui->displayCode(pin ? pin : "");
		return dbus_message_new_method_return(msg);
	}
	if (dbus_message_is_method_call(msg, BLUEZ_AGENT, "DisplayPasskey"))
	{
		/* comes again with every key that is typed on the device */
		dbus_uint32_t passkey = 0;
		if (dbus_message_iter_get_arg_type(&args) != DBUS_TYPE_UINT32)
			return agent_error(msg, BLUEZ_ERROR "Rejected");
		dbus_message_iter_get_basic(&args, &passkey);
		snprintf(code, sizeof(code), "%06u", (unsigned)passkey);
		pairing_ui->displayCode(code);
		return dbus_message_new_method_return(msg);
	}
	if (dbus_message_is_method_call(msg, BLUEZ_AGENT, "RequestConfirmation"))
	{
		dbus_uint32_t passkey = 0;
		if (dbus_message_iter_get_arg_type(&args) != DBUS_TYPE_UINT32)
			return agent_error(msg, BLUEZ_ERROR "Rejected");
		dbus_message_iter_get_basic(&args, &passkey);
		snprintf(code, sizeof(code), "%06u", (unsigned)passkey);
		if (!pairing_ui->confirmCode(code))
			return agent_error(msg, BLUEZ_ERROR "Rejected");
		return dbus_message_new_method_return(msg);
	}
	/* the user asked for this pairing a moment ago */
	if (dbus_message_is_method_call(msg, BLUEZ_AGENT, "RequestAuthorization") ||
	    dbus_message_is_method_call(msg, BLUEZ_AGENT, "AuthorizeService"))
		return dbus_message_new_method_return(msg);

	/* RequestPasskey: a number to type here that the device shows; none of
	 * the devices this is for has a display */
	return agent_error(msg, BLUEZ_ERROR "Rejected");
}

static DBusHandlerResult agent_message(DBusConnection *connection, DBusMessage *msg, void *data)
{
	if (dbus_message_get_type(msg) != DBUS_MESSAGE_TYPE_METHOD_CALL)
		return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;

	DBusMessage *reply = ((CBluezClient *)data)->agentRequest(msg);
	if (reply)
	{
		dbus_connection_send(connection, reply, NULL);
		dbus_message_unref(reply);
	}
	return DBUS_HANDLER_RESULT_HANDLED;
}

bool CBluezClient::registerAgent()
{
	if (!bus.open())
		return false;

	DBusConnection *conn = bus.connection();
	if (agent_registered && agent_conn == conn)
		return true;
	agent_registered = false;

	/* remember who BlueZ is, the agent answers nobody else */
	bluez_owner = bus.owner();
	if (bluez_owner.empty())
		return false;

	static const DBusObjectPathVTable vtable = { NULL, agent_message, NULL, NULL, NULL, NULL };
	void *registered = NULL;
	if (!dbus_connection_get_object_path_data(conn, AGENT_PATH, &registered) || !registered)
	{
		if (!dbus_connection_register_object_path(conn, AGENT_PATH, &vtable, this))
			return false;
	}

	/* not the default agent: it is used for what this connection asks for */
	DBusMessage *msg = dbus_message_new_method_call(BLUEZ_SERVICE, BLUEZ_AGENT_MANAGER_PATH, BLUEZ_AGENT_MANAGER, "RegisterAgent");
	const char *path = AGENT_PATH;
	const char *capability = AGENT_CAPABILITY;
	if (!msg)
		return false;
	dbus_message_append_args(msg, DBUS_TYPE_OBJECT_PATH, &path, DBUS_TYPE_STRING, &capability, DBUS_TYPE_INVALID);
	DBusMessage *reply = dbus_connection_send_with_reply_and_block(conn, msg, 10000, NULL);
	dbus_message_unref(msg);
	if (!reply)
		return false;
	dbus_message_unref(reply);

	agent_registered = true;
	agent_conn = conn;
	return true;
}

void CBluezClient::unregisterAgent()
{
	if (!agent_registered || agent_conn != bus.connection())
		return;

	bus.simpleCall(BLUEZ_AGENT_MANAGER_PATH, BLUEZ_AGENT_MANAGER, "UnregisterAgent", AGENT_PATH, true);
	agent_registered = false;
}

static int connect_result(const std::string &error)
{
	if (error.empty() || error == BLUEZ_ERROR "AlreadyConnected")
		return CBluezClient::RESULT_OK;
	if (error == DBUS_ERROR_UNKNOWN_OBJECT || error == DBUS_ERROR_UNKNOWN_METHOD)
		return CBluezClient::RESULT_GONE;
	if (error == BLUEZ_ERROR "NotAvailable" || error == BLUEZ_ERROR "NotSupported")
		return CBluezClient::RESULT_NO_PROFILE;
	return CBluezClient::RESULT_FAILED;
}

int CBluezClient::connect(const bluez_device &device)
{
	if (!powered())
		return RESULT_UNAVAILABLE;
	if (!bus.has(device.path, BLUEZ_DEVICE))
		return RESULT_GONE;

	std::string error;
	bus.callDispatching(device.path, BLUEZ_DEVICE, "Connect", NULL, CONNECT_TIMEOUT_MS, error);
	int result = connect_result(error);

	/* one profile of several may fail while the others come up, and a
	 * keyboard or a gamepad connects on its own once it is paired, which
	 * makes our attempt fail: a device that is connected a moment later
	 * is good enough */
	if (result == RESULT_FAILED)
	{
		for (int i = 0; i < 10 && result != RESULT_OK; i++)
		{
			if (i)
				bus.dispatch(500);
			refresh();
			if (bus.prop(device.path, BLUEZ_DEVICE, "Connected") == "1")
				result = RESULT_OK;
		}
	}
	return result;
}

int CBluezClient::pair(const bluez_device &device, CBluezPairingUI *ui)
{
	if (!powered())
		return RESULT_UNAVAILABLE;

	/* BlueZ drops a device it has not paired half a minute after the search */
	if (!bus.has(device.path, BLUEZ_DEVICE))
	{
		if (!scan() || !refresh() || !bus.has(device.path, BLUEZ_DEVICE))
			return RESULT_GONE;
	}

	int result = RESULT_OK;
	bool agent = false;

	if (bus.prop(device.path, BLUEZ_DEVICE, "Paired") != "1")
	{
		if (!registerAgent())
			return RESULT_UNAVAILABLE;

		agent = true;
		pairing_path = device.path;
		pairing_ui = ui;

		/* the agent is served while the reply is outstanding */
		std::string error;
		bool ok = bus.callDispatching(device.path, BLUEZ_DEVICE, "Pair", NULL, PAIR_TIMEOUT_MS, error);
		if (!ok && error.empty())
			bus.simpleCall(device.path, BLUEZ_DEVICE, "CancelPairing");

		if (ui)
			ui->hideCode();

		if (!ok && error != BLUEZ_ERROR "AlreadyExists")
		{
			if (error == DBUS_ERROR_UNKNOWN_OBJECT || error == DBUS_ERROR_UNKNOWN_METHOD)
				result = RESULT_GONE;
			else if (error.compare(0, strlen(BLUEZ_ERROR "Authentication"), BLUEZ_ERROR "Authentication") == 0)
				result = RESULT_AUTH_FAILED;
			else
				result = RESULT_FAILED;
		}
	}

	if (result == RESULT_OK)
	{
		/* trusted devices may connect on their own from now on */
		bus.setProperty(device.path, BLUEZ_DEVICE, "Trusted", true);

		/* a gamepad or a keyboard opens its connection the moment it is
		 * paired, before it is trusted, and BlueZ asks the agent whether it
		 * may: the agent stays until we are connected, or the question
		 * would go unanswered and leave the device half connected */
		result = connect(device);
	}

	if (agent)
	{
		pairing_path.clear();
		pairing_ui = NULL;
		unregisterAgent();
	}
	return result;
}

bool CBluezClient::disconnect(const bluez_device &device)
{
	if (!available() || !bus.has(device.path, BLUEZ_DEVICE))
		return false;
	return bus.simpleCall(device.path, BLUEZ_DEVICE, "Disconnect");
}

bool CBluezClient::remove(const bluez_device &device)
{
	if (!available() || !bus.has(device.path, BLUEZ_DEVICE))
		return false;
	return bus.simpleCall(adapter_path, BLUEZ_ADAPTER, "RemoveDevice", device.path.c_str(), true);
}
