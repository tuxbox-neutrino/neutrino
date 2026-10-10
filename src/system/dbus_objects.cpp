/*
 * The objects of a D-Bus service on the system bus
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

#include <dbus/dbus.h>

#include "dbus_objects.h"

#define DBUS_OBJECT_MANAGER	"org.freedesktop.DBus.ObjectManager"
#define DBUS_PROPERTIES		"org.freedesktop.DBus.Properties"

CDBusObjects::CDBusObjects(const char *_service)
{
	service = _service;
	conn = NULL;
}

CDBusObjects::~CDBusObjects()
{
	close();
}

bool CDBusObjects::open()
{
	if (conn && dbus_connection_get_is_connected(conn))
		return true;

	close();

	DBusError err;
	dbus_error_init(&err);
	conn = dbus_bus_get_private(DBUS_BUS_SYSTEM, &err);
	if (!conn)
	{
		dbus_error_free(&err);
		return false;
	}
	dbus_connection_set_exit_on_disconnect(conn, FALSE);
	return true;
}

void CDBusObjects::close()
{
	if (!conn)
		return;

	dbus_connection_close(conn);
	dbus_connection_unref(conn);
	conn = NULL;
	objs.clear();
}

DBusMessage *CDBusObjects::call(const std::string &path, const char *iface, const char *method,
				const char *arg, bool arg_is_path, int timeout_ms, std::string *error)
{
	if (error)
		error->clear();
	if (!open())
		return NULL;

	DBusMessage *msg = dbus_message_new_method_call(service.c_str(), path.c_str(), iface, method);
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
		if (error && err.name)
			*error = err.name;
		if (strcmp(method, "GetManagedObjects"))
			printf("[dbus] %s.%s: %s\n", iface, method, err.name ? err.name : "failed");
		dbus_error_free(&err);
	}
	return reply;
}

bool CDBusObjects::simpleCall(const std::string &path, const char *iface, const char *method,
			      const char *arg, bool arg_is_path, std::string *error)
{
	DBusMessage *reply = call(path, iface, method, arg, arg_is_path, 10000, error);
	if (!reply)
		return false;

	dbus_message_unref(reply);
	return true;
}

bool CDBusObjects::setProperty(const std::string &path, const char *iface, const char *name, bool value)
{
	if (!open())
		return false;

	DBusMessage *msg = dbus_message_new_method_call(service.c_str(), path.c_str(), DBUS_PROPERTIES, "Set");
	if (!msg)
		return false;

	DBusMessageIter args, variant;
	dbus_bool_t v = value;
	dbus_message_iter_init_append(msg, &args);
	dbus_message_iter_append_basic(&args, DBUS_TYPE_STRING, &iface);
	dbus_message_iter_append_basic(&args, DBUS_TYPE_STRING, &name);
	dbus_message_iter_open_container(&args, DBUS_TYPE_VARIANT, "b", &variant);
	dbus_message_iter_append_basic(&variant, DBUS_TYPE_BOOLEAN, &v);
	dbus_message_iter_close_container(&args, &variant);

	DBusError err;
	dbus_error_init(&err);
	DBusMessage *reply = dbus_connection_send_with_reply_and_block(conn, msg, 10000, &err);
	dbus_message_unref(msg);
	if (!reply)
	{
		printf("[dbus] set %s.%s: %s\n", iface, name, err.name ? err.name : "failed");
		dbus_error_free(&err);
		return false;
	}
	dbus_message_unref(reply);
	return true;
}

bool CDBusObjects::callDispatching(const std::string &path, const char *iface, const char *method,
				   const char *arg, int timeout_ms, std::string &error)
{
	bool ok = false;
	error.clear();
	if (!open())
		return false;

	DBusMessage *msg = dbus_message_new_method_call(service.c_str(), path.c_str(), iface, method);
	DBusPendingCall *pending = NULL;
	if (msg && arg)
		dbus_message_append_args(msg, DBUS_TYPE_STRING, &arg, DBUS_TYPE_INVALID);

	if (msg && dbus_connection_send_with_reply(conn, msg, &pending, timeout_ms) && pending)
	{
		while (!dbus_pending_call_get_completed(pending))
		{
			if (!dbus_connection_read_write_dispatch(conn, 100))
				break;
		}

		DBusMessage *reply = dbus_pending_call_steal_reply(pending);
		if (reply)
		{
			if (dbus_message_get_type(reply) == DBUS_MESSAGE_TYPE_METHOD_RETURN)
				ok = true;
			else
			{
				const char *name = dbus_message_get_error_name(reply);
				error = name ? name : "failed";
				printf("[dbus] %s.%s: %s\n", iface, method, error.c_str());
			}
			dbus_message_unref(reply);
		}
		dbus_pending_call_unref(pending);
	}
	if (msg)
		dbus_message_unref(msg);
	return ok;
}

void CDBusObjects::dispatch(int ms)
{
	if (!open())
		return;

	for (int i = 0; i < ms; i += 50)
	{
		if (!dbus_connection_read_write_dispatch(conn, 50))
			break;
	}
}

std::string CDBusObjects::owner()
{
	std::string result;
	if (!open())
		return result;

	DBusMessage *msg = dbus_message_new_method_call(DBUS_SERVICE_DBUS, DBUS_PATH_DBUS, DBUS_INTERFACE_DBUS, "GetNameOwner");
	const char *name = service.c_str();
	if (!msg)
		return result;
	dbus_message_append_args(msg, DBUS_TYPE_STRING, &name, DBUS_TYPE_INVALID);
	DBusMessage *reply = dbus_connection_send_with_reply_and_block(conn, msg, 5000, NULL);
	dbus_message_unref(msg);
	if (reply)
	{
		const char *o = NULL;
		if (dbus_message_get_args(reply, NULL, DBUS_TYPE_STRING, &o, DBUS_TYPE_INVALID) && o)
			result = o;
		dbus_message_unref(reply);
	}
	return result;
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
		case DBUS_TYPE_UINT16:
		{
			dbus_uint16_t i = 0;
			dbus_message_iter_get_basic(variant, &i);
			snprintf(buf, sizeof(buf), "%u", (unsigned)i);
			return buf;
		}
		case DBUS_TYPE_INT32:
		{
			dbus_int32_t i = 0;
			dbus_message_iter_get_basic(variant, &i);
			snprintf(buf, sizeof(buf), "%d", (int)i);
			return buf;
		}
		case DBUS_TYPE_UINT32:
		{
			dbus_uint32_t i = 0;
			dbus_message_iter_get_basic(variant, &i);
			snprintf(buf, sizeof(buf), "%u", (unsigned)i);
			return buf;
		}
		case DBUS_TYPE_BYTE:
		{
			unsigned char i = 0;
			dbus_message_iter_get_basic(variant, &i);
			snprintf(buf, sizeof(buf), "%u", (unsigned)i);
			return buf;
		}
		default:
			return "";
	}
}

/* a{oa{sa{sv}}} */
bool CDBusObjects::refresh()
{
	objs.clear();

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

			props_t &props = objs[path][iface_name];
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
		}
	}
	dbus_message_unref(reply);
	return true;
}

std::string CDBusObjects::prop(const std::string &path, const char *iface, const char *name) const
{
	objects_t::const_iterator o = objs.find(path);
	if (o == objs.end())
		return "";
	ifaces_t::const_iterator i = o->second.find(iface);
	if (i == o->second.end())
		return "";
	props_t::const_iterator p = i->second.find(name);
	return p == i->second.end() ? "" : p->second;
}

bool CDBusObjects::has(const std::string &path, const char *iface) const
{
	objects_t::const_iterator o = objs.find(path);
	return o != objs.end() && o->second.find(iface) != o->second.end();
}

std::string CDBusObjects::find(const char *iface) const
{
	for (objects_t::const_iterator o = objs.begin(); o != objs.end(); ++o)
		if (o->second.find(iface) != o->second.end())
			return o->first;
	return "";
}
