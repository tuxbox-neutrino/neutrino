/*
 * Network configuration through /etc/network/interfaces, ifup and ifdown
 *
 * (C) 2003 by thegoodguy <thegoodguy@berlios.de>
 * (C) 2011 Stefan Seyfried
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
#include <fcntl.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include "configure_network.h"
#include <lib/libnet/libnet.h>             /* netSetNameserver */
#include <lib/libnet/network_interfaces.h> /* getInetAttributes, setInetAttributes */
#include <fstream>
#include <system/helpers.h>

#define WPA_CONFIG "/etc/wpa_supplicant.conf"

#ifndef ENABLE_IWD
/* the networks are wpa_supplicant's, chosen in the wireless menu; its config
 * needs the control socket for that menu and has to let it store networks */
static void prepareWpaConfig()
{
	std::string s, conf;
	bool ctrl = false, update = false;
	std::ifstream in(WPA_CONFIG);
	while (in.is_open() && getline(in, s))
	{
		if (s.compare(0, 15, "ctrl_interface=") == 0)
			ctrl = true;
		else if (s.compare(0, 14, "update_config=") == 0)
		{
			update = true;
			s = "update_config=1";
		}
		conf += s + "\n";
	}
	in.close();
	if (!update)
		conf = "update_config=1\n" + conf;
	if (!ctrl)
		conf = "ctrl_interface=/var/run/wpa_supplicant\n" + conf;

	std::string tmp = std::string(WPA_CONFIG) + ".tmp";
	FILE *f = fopen(tmp.c_str(), "w");
	if (!f)
	{
		perror(WPA_CONFIG " write error");
		return;
	}
	fchmod(fileno(f), 0600);
	fputs(conf.c_str(), f);
	if (fclose(f) == 0)
		rename(tmp.c_str(), WPA_CONFIG);
}
#endif

bool CNetworkConfig::canConfigure(void)
{
#ifdef ENABLE_IWD
	/* iwd brings its interfaces up and runs DHCP on them itself */
	if (wireless)
		return false;
#endif
	return !systemManaged();
}

bool CNetworkConfig::hasAutomaticStart(void)
{
	return true;
}

void CNetworkConfig::backendRead(void)
{
	inet_static = getInetAttributes(ifname, automatic_start, address, netmask, broadcast, gateway);
}

void CNetworkConfig::backendCommit(bool modified, bool nameserver_changed)
{
	if (modified)
	{
		addLoopbackDevice("lo", true);
		if (inet_static)
		{
			if (validAddress(address) && validAddress(netmask) && (gateway.empty() || validAddress(gateway)))
				setStaticAttributes(ifname, automatic_start, address, netmask, broadcast, gateway, wireless);
			else
				printf("CNetworkConfig::commitConfig: invalid address, %s not written\n", ifname.c_str());
		}
		else
			setDhcpAttributes(ifname, automatic_start, wireless);

#ifndef ENABLE_IWD
		if (wireless)
			prepareWpaConfig();
#endif
	}
	if (nameserver_changed && (nameserver.empty() || validAddress(nameserver)))
		netSetNameserver(nameserver);
}

void CNetworkConfig::startNetwork(void)
{
	std::string ifup = find_executable("ifup");
	if (ifup.empty())
	{
		printf("CNetworkConfig::startNetwork: ifup not found\n");
		return;
	}
	if (!validInterface(ifname))
		return;

	my_system(2, ifup.c_str(), ifname.c_str());

	if (!inet_static)
		init_vars();
}

void CNetworkConfig::stopNetwork(void)
{
	std::string ifdown = find_executable("ifdown");
	if (ifdown.empty())
	{
		printf("CNetworkConfig::stopNetwork: ifdown not found\n");
		return;
	}
	if (!validInterface(ifname))
		return;

	my_system(2, ifdown.c_str(), ifname.c_str());
}

