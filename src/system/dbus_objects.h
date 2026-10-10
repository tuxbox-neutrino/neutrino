#ifndef __dbus_objects_h__
#define __dbus_objects_h__

/*
 * The objects of a D-Bus service on the system bus
 *
 * What iwd and BlueZ have in common: a tree of objects with properties
 * behind org.freedesktop.DBus.ObjectManager, methods to call on them, and an
 * agent object of ours that the service calls back while a method runs.
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

struct DBusConnection;
struct DBusMessage;

class CDBusObjects
{
	public:
		/* property name -> value as text: strings and object paths as they
		 * are, booleans as "1" and "0", integers in decimal */
		typedef std::map<std::string, std::string> props_t;
		typedef std::map<std::string, props_t> ifaces_t;
		typedef std::map<std::string, ifaces_t> objects_t;

		CDBusObjects(const char *service);
		~CDBusObjects();

		/* a connection of our own, nothing else in the process dispatches it */
		bool open();
		void close();
		DBusConnection *connection() { return conn; }

		/* read the whole tree in one go */
		bool refresh();
		const objects_t &objects() const { return objs; }
		std::string prop(const std::string &path, const char *iface, const char *name) const;
		bool has(const std::string &path, const char *iface) const;
		/* the first object that has this interface */
		std::string find(const char *iface) const;

		/* synchronous calls; the reply is the caller's to unref. error gets
		 * the D-Bus error name when the call fails. */
		DBusMessage *call(const std::string &path, const char *iface, const char *method,
				  const char *arg = NULL, bool arg_is_path = false, int timeout_ms = 10000,
				  std::string *error = NULL);
		bool simpleCall(const std::string &path, const char *iface, const char *method,
				const char *arg = NULL, bool arg_is_path = false, std::string *error = NULL);
		bool setProperty(const std::string &path, const char *iface, const char *name, bool value);

		/* a call that is waited for while this connection keeps being served,
		 * so that the service can call an object of ours in the meantime */
		bool callDispatching(const std::string &path, const char *iface, const char *method,
				     const char *arg, int timeout_ms, std::string &error);

		/* serve this connection for a while without a call of ours */
		void dispatch(int ms);

		/* the unique bus name of the service, to tell its calls from others */
		std::string owner();

	private:
		std::string service;
		DBusConnection *conn;
		objects_t objs;
};

#endif /* __dbus_objects_h__ */
