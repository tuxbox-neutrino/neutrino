#ifndef __bluez_client_h__
#define __bluez_client_h__

/*
 * Bluetooth devices through BlueZ
 *
 * Talks to bluetoothd on the system bus: finds devices, pairs, connects,
 * disconnects and removes them. Meant for what gets used at a set-top box:
 * speakers, headphones, keyboards, mice and gamepads.
 *
 * The pairing agent only exists while pair() runs and only answers BlueZ
 * about the device that is being paired, so nothing can pair with the box
 * from outside. The adapter is never made discoverable.
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

#include "dbus_objects.h"

struct bluez_device
{
	enum
	{
		KIND_OTHER,
		KIND_AUDIO,
		KIND_KEYBOARD,
		KIND_MOUSE,
		KIND_GAMEPAD,
		KIND_INPUT
	};

	std::string path;	/* D-Bus object of the device */
	std::string address;
	std::string name;
	int kind;
	bool paired;
	bool trusted;
	bool connected;
	bool has_signal;
	int signal;		/* dBm, only known while the device is being seen */
};

/* what the pairing agent needs from whoever sits in front of the screen */
class CBluezPairingUI
{
	public:
		virtual ~CBluezPairingUI() {}
		/* show a code that is to be typed on the device; stays until hideCode() */
		virtual void displayCode(const std::string &code) = 0;
		virtual void hideCode() = 0;
		/* both sides show this code: is it the same? */
		virtual bool confirmCode(const std::string &code) = 0;
		/* a device without a display wants its PIN */
		virtual bool requestPin(std::string &pin) = 0;
};

class CBluezClient
{
	public:
		enum
		{
			RESULT_OK,
			RESULT_FAILED,		/* the device did not answer or refused */
			RESULT_AUTH_FAILED,	/* wrong code, or pairing was rejected */
			RESULT_NO_PROFILE,	/* paired, but nothing here can use the device */
			RESULT_GONE,		/* the device is not seen any more */
			RESULT_UNAVAILABLE	/* no BlueZ or no adapter */
		};

		static CBluezClient *getInstance();

		/* bluetoothd answers on the system bus and has an adapter */
		bool available();
		bool powered();
		bool setPowered(bool on);

		/* look for devices for this long */
		bool scan(int seconds = 10);
		/* paired devices first, then what was found, strongest first */
		bool getDevices(std::vector<bluez_device> &devices);
		/* number of connected devices, for the menu entry */
		int connectedCount();

		/* pair, trust and connect */
		int pair(const bluez_device &device, CBluezPairingUI *ui);
		int connect(const bluez_device &device);
		bool disconnect(const bluez_device &device);
		/* forget the device and its keys */
		bool remove(const bluez_device &device);

		/* entry point of the D-Bus dispatcher: BlueZ asks its agent */
		DBusMessage *agentRequest(DBusMessage *msg);

	private:
		CDBusObjects bus;
		DBusConnection *agent_conn;
		std::string adapter_path;
		std::string bluez_owner;
		std::string pairing_path;
		CBluezPairingUI *pairing_ui;
		bool agent_registered;

		CBluezClient();
		~CBluezClient();

		bool refresh();
		bool registerAgent();
		void unregisterAgent();
		static int kindOf(const std::string &icon);
		static bool wanted(const std::string &icon);
};

#endif /* __bluez_client_h__ */
