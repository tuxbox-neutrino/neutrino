/*
 * $port: configure_network.cpp,v 1.7 2009/11/20 22:44:19 tuxbox-cvs Exp $
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
#include <cstdio>               /* perror... */
#include <sys/wait.h>
#include <sys/types.h>          /* u_char */
#include <string.h>
#include <unistd.h>
#include "configure_network.h"
#include <arpa/inet.h>
#include <lib/libnet/libnet.h>             /* netGetNameserver, netSetNameserver   */
#include <iostream>
#include <iomanip>
#include <sstream>
#include <fstream>
#include <system/helpers.h>

CNetworkConfig::CNetworkConfig()
{
	netGetNameserver(nameserver);
	ifname = "eth0";
	orig_automatic_start = false;
	orig_inet_static = false;
	automatic_start = false;
	inet_static = false;
	wireless = false;
}

CNetworkConfig* CNetworkConfig::getInstance()
{
	static CNetworkConfig* network_config = NULL;

	if(!network_config)
	{
		network_config = new CNetworkConfig();
		printf("[network config] Instance created\n");
	}
	return network_config;
}

CNetworkConfig::~CNetworkConfig()
{
}

void CNetworkConfig::readConfig(std::string iname)
{
	ifname = iname;
	nameserver = "";
	backendRead();
	if (nameserver.empty())
		netGetNameserver(nameserver);

	init_vars();
	copy_to_orig();
}

void CNetworkConfig::init_vars(void)
{
	std::string mask;
	std::string _broadcast;
	std::string router;
	std::string ip;
	unsigned char addr[6];

	netGetHostname(hostname);

	/* a static setup keeps the gateway it was configured with */
	if (!inet_static || gateway.empty()) {
		netGetDefaultRoute(router);
		gateway = router;
	}

	/* FIXME its enough to read IP for dhcp only ?
	 * static config should not be different from settings in etc/network/interfaces */
	if(!inet_static) {
		netGetIP(ifname, ip, mask, _broadcast);
		netmask = mask;
		broadcast = _broadcast;
		address = ip;
		/* an address iwd has set carries no broadcast, the one of the
		 * network is what the menu shows and compares against */
		struct in_addr na, nm;
		if ((broadcast.empty() || broadcast == "0.0.0.0")
		    && inet_pton(AF_INET, address.c_str(), &na) == 1 && inet_pton(AF_INET, netmask.c_str(), &nm) == 1) {
			struct in_addr in;
			char tmp[40];
			in.s_addr = na.s_addr | ~nm.s_addr;
			if (inet_ntop(AF_INET, &in, tmp, sizeof(tmp)))
				broadcast = tmp;
		}
	}

	netGetMacAddr(ifname, addr);

	std::stringstream mac_tmp;
	for(int i=0;i<6;++i)
		mac_tmp<<std::hex<<std::setfill('0')<<std::setw(2)<<(int)addr[i]<<':';

	mac_addr = mac_tmp.str().substr(0,17);

	wireless = 0;
	std::string tmp = "/sys/class/net/" + ifname + "/wireless";

	if(access(tmp, R_OK) == 0)
		wireless = 1;

	printf("CNetworkConfig: %s loaded, wireless %s\n", ifname.c_str(), wireless ? "yes" : "no");
}

void CNetworkConfig::copy_to_orig(void)
{
	orig_automatic_start = automatic_start;
	orig_address         = address;
	orig_netmask         = netmask;
	orig_broadcast       = broadcast;
	orig_gateway         = gateway;
	orig_nameserver      = nameserver;
	orig_inet_static     = inet_static;
	orig_hostname	     = hostname;
	orig_ifname	     = ifname;
}

bool CNetworkConfig::modified_from_orig(void)
{
#ifdef DEBUG
		if(orig_automatic_start != automatic_start)
			printf("CNetworkConfig::modified_from_orig: automatic_start changed\n");
		if(orig_address         != address        )
			printf("CNetworkConfig::modified_from_orig: address changed\n");
		if(orig_netmask         != netmask        )
			printf("CNetworkConfig::modified_from_orig: netmask changed\n");
		if(orig_broadcast       != broadcast      )
			printf("CNetworkConfig::modified_from_orig: broadcast changed\n");
		if(orig_gateway         != gateway        )
			printf("CNetworkConfig::modified_from_orig: gateway changed\n");
		if(orig_hostname        != hostname       )
			printf("CNetworkConfig::modified_from_orig: hostname changed\n");
		if(orig_inet_static     != inet_static    )
			printf("CNetworkConfig::modified_from_orig: inet_static changed\n");
		if(orig_ifname	      != ifname)
			printf("CNetworkConfig::modified_from_orig: ifname changed\n");
#endif
	/* check for following changes with dhcp enabled trigger apply question on menu quit, 
	 * even if apply already done */
	if (inet_static) {
		if ((orig_address         != address        ) ||
		    (orig_netmask         != netmask        ) ||
		    (orig_broadcast       != broadcast      ) ||
		    (orig_gateway         != gateway        ))
			return 1;
	}
	return (
		(orig_automatic_start != automatic_start) ||
		(orig_hostname        != hostname       ) ||
		(orig_inet_static     != inet_static    ) ||
		(orig_ifname	      != ifname)
		);

#if 0
	return (
		(orig_automatic_start != automatic_start) ||
		(orig_address         != address        ) ||
		(orig_netmask         != netmask        ) ||
		(orig_broadcast       != broadcast      ) ||
		(orig_gateway         != gateway        ) ||
		(orig_hostname        != hostname       ) ||
		(orig_inet_static     != inet_static    ) ||
		(orig_ifname	      != ifname)
		);
#endif
}

void CNetworkConfig::commitConfig(void)
{
	if (!canConfigure())
	{
		printf("CNetworkConfig::commitConfig: %s is not configured here\n", ifname.c_str());
		return;
	}

	bool modified = modified_from_orig();
	bool nameserver_changed = (nameserver != orig_nameserver);

	if (modified && orig_hostname != hostname)
	{
		if (validHostname(hostname))
			netSetHostname(hostname);
		else
		{
			printf("CNetworkConfig::commitConfig: invalid hostname, keeping the old one\n");
			hostname = orig_hostname;
		}
	}

	backendCommit(modified, nameserver_changed);

	if (modified || nameserver_changed)
		copy_to_orig();
}

bool CNetworkConfig::systemManaged(void)
{
	return geteuid() != 0;
}

/* letters, digits and hyphens in dot separated labels (RFC 1123) */
bool CNetworkConfig::validHostname(const std::string &name)
{
	if (name.empty() || name.length() > 64)
		return false;

	size_t label = 0;
	for (size_t i = 0; i < name.length(); i++)
	{
		const char c = name[i];
		if (c == '.')
		{
			if (label == 0 || name[i - 1] == '-')
				return false;
			label = 0;
			continue;
		}
		const bool alnum = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9');
		if (!alnum && !(c == '-' && label > 0))
			return false;
		if (++label > 63)
			return false;
	}
	return label > 0 && name[name.length() - 1] != '-';
}

bool CNetworkConfig::validAddress(const std::string &address)
{
	struct in_addr in;
	return inet_pton(AF_INET, address.c_str(), &in) == 1;
}

/* a name the kernel knows, made of characters that are harmless in a
 * file name and on a command line */
bool CNetworkConfig::validInterface(const std::string &name)
{
	if (name.empty() || name.length() > 15)
		return false;

	for (size_t i = 0; i < name.length(); i++)
	{
		const char c = name[i];
		const bool ok = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_' || c == '-' || c == '.';
		if (!ok || (i == 0 && (c == '.' || c == '-')))
			return false;
	}

	std::string sys = "/sys/class/net/" + name;
	return access(sys.c_str(), F_OK) == 0;
}
