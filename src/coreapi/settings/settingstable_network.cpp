/*
 * settingstable_network.cpp - network settings, one row per field
 *
 * Copyright (C) 2026 NI-Team
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
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include "settingstable.h"
#include "settingsfield.h"

namespace coreapi
{

namespace
{

/* The network section. Two screens, and the descriptor carries no sub-section,
   so that structure is not in the table: the interface and the time server come
   from src/gui/network_setup.cpp, the three proxy fields from
   src/gui/proxyserver_setup.cpp.

   Most of the network screen is not here. The address, the netmask, the
   broadcast, the gateway, the name server, the host name, the wireless name and
   its key, the DHCP switch and the flag that brings the network up at start are
   all members of CNetworkConfig, which keeps a file of its own, and the screen
   copies them through members of its own besides,
   src/gui/network_setup.cpp:167.

   Two more screens the network menu opens declare nothing at all. The web
   server screen keeps every one of its values in CWebserverSetup and hands them
   to the layer that owns the server's own configuration file,
   src/gui/webserver_setup.cpp:78. The mount screens hold theirs in arrays of
   structures, src/system/settings.h:590 and :614, which no scalar descriptor
   reaches. */

/* Two values with words of their own rather than the program's on and off, so
   an Enum keeps the names and holds both the numbers and the words to the
   screen. A Bool is compared on the numbers alone, and which of the two means
   one would then rest on nothing.
   src/gui/network_setup.cpp:199 src/gui/network_setup.h:119 */
const EnumValue kNtpEnable[] =
{
	{ 0, "options.ntp_off" },
	{ 1, "options.ntp_on" }
};

const Descriptor kNetwork[] =
{
	/* The name of an interface, and what the screen offers is what the box has:
	   it reads /sys/class/net, src/gui/network_setup.cpp:236. A name nothing
	   there matches is dropped and replaced at the next load,
	   src/system/helpers.cpp:2276. */
	// src/neutrino.cpp:813 src/gui/network_setup.cpp:261
	{
		"ifname", ValueType::String, "network",
		"networkmenu.select_if", "menu.hint_net_if",
		0, 0, NULL, 0, 0, "", false, false, COREAPI_ALWAYS,
		COREAPI_TEXT_FIELD(ifname)
	},

	// time
	// src/neutrino.cpp:926 src/gui/network_setup.cpp:505
	{
		"network_ntpenable", ValueType::Enum, "network",
		"networkmenu.ntpenable", "menu.hint_net_ntpenable",
		0, 0, COREAPI_VALUES(kNtpEnable), 1, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(network_ntpenable)
	},
	// src/neutrino.cpp:927 src/gui/network_setup.cpp:501
	{
		"network_ntpserver", ValueType::String, "network",
		"networkmenu.ntpserver", "menu.hint_net_ntpserver",
		0, 0, NULL, 0, 0, "0.de.pool.ntp.org", false, false, COREAPI_ALWAYS,
		COREAPI_TEXT_FIELD(network_ntpserver)
	},
	/* Minutes, kept as text and turned into a number where it is used,
	   src/neutrino.cpp:2775. Text here because the field is, and a String
	   carries no bound, so the three digits and the ten characters the screen
	   takes are stated nowhere a caller can read.
	   src/gui/network_setup.cpp:503 */
	// src/neutrino.cpp:928 src/gui/network_setup.cpp:503
	{
		"network_ntprefresh", ValueType::String, "network",
		"networkmenu.ntprefresh", "menu.hint_net_ntprefresh",
		0, 0, NULL, 0, 0, "30", false, false, COREAPI_ALWAYS,
		COREAPI_TEXT_FIELD(network_ntprefresh)
	},

	/* The proxy. The three below are one setting in three parts: the loader
	   itself joins them into one URL, src/neutrino.cpp:1103, and so does every
	   reader, src/system/helpers.cpp:1976 and src/system/httptool.cpp:316.

	   The name and the password are the credential in that URL and are both
	   declared secret: a name that identifies an account against a password is
	   the half of a credential that names the account, the box shows neither,
	   and the screen types the password behind stars,
	   src/gui/proxyserver_setup.cpp:160. The server is not secret. Which proxy
	   a box goes through is not a credential, and hiding it would leave a
	   frontend unable to show whether one is set at all. */
	// src/neutrino.cpp:1099 src/gui/proxyserver_setup.cpp:128
	{
		"softupdate_proxyserver", ValueType::String, "network",
		"flashupdate.proxyserver", "menu.hint_net_proxyserver",
		0, 0, NULL, 0, 0, "", false, false, COREAPI_ALWAYS,
		COREAPI_TEXT_FIELD(softupdate_proxyserver)
	},
	// src/neutrino.cpp:1100 src/gui/proxyserver_setup.cpp:144
	{
		"softupdate_proxyusername", ValueType::String, "network",
		"flashupdate.proxyusername", "menu.hint_net_proxyuser",
		0, 0, NULL, 0, 0, "", false, true, COREAPI_ALWAYS,
		COREAPI_TEXT_FIELD(softupdate_proxyusername)
	},
	// src/neutrino.cpp:1101 src/gui/proxyserver_setup.cpp:161
	{
		"softupdate_proxypassword", ValueType::String, "network",
		"flashupdate.proxypassword", "menu.hint_net_proxypass",
		0, 0, NULL, 0, 0, "", false, true, COREAPI_ALWAYS,
		COREAPI_TEXT_FIELD(softupdate_proxypassword)
	},
};

} // anonymous namespace

const Descriptor *settingsTableNetwork(size_t &count)
{
	count = sizeof(kNetwork) / sizeof(kNetwork[0]);
	return kNetwork;
}

} // namespace coreapi
