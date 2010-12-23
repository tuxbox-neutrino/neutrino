/*
 * $port: configure_network.cpp,v 1.7 2009/11/20 22:44:19 tuxbox-cvs Exp $
 *
 * (C) 2003 by thegoodguy <thegoodguy@berlios.de>
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
 *
 */
#include <config.h>
#include <cstdio>               /* perror... */
#include <sys/wait.h>
#include "configure_network.h"
#include "libnet.h"             /* netGetNameserver, netSetNameserver   */
#include "network_interfaces.h" /* getInetAttributes, setInetAttributes */
#include <stdlib.h>             /* system                               */

CNetworkConfig::CNetworkConfig(void)
{
	char our_nameserver[16];

	netGetNameserver(our_nameserver);
	nameserver = our_nameserver;
	inet_static = getInetAttributes("eth0", automatic_start, address, netmask, broadcast, gateway);

	init_vars();
	copy_to_orig();
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


void CNetworkConfig::init_vars(void)
{
	char mask[16];
	char _broadcast[16];
	char router[16];
	char ip[16];

	hostname = netGetHostname();

	netGetDefaultRoute(router);
	gateway = router;
	netGetIP((char *) "eth0", ip, mask, _broadcast);
	netmask = mask;
	broadcast = _broadcast;
	address = ip;
}

void CNetworkConfig::copy_to_orig(void)
{
	orig_automatic_start = automatic_start;
	orig_address         = address;
	orig_netmask         = netmask;
	orig_broadcast       = broadcast;
	orig_gateway         = gateway;
	orig_inet_static     = inet_static;
	orig_hostname	     = hostname;
}

bool CNetworkConfig::modified_from_orig(void)
{
	return (
		(orig_automatic_start != automatic_start) ||
		(orig_address         != address        ) ||
		(orig_netmask         != netmask        ) ||
		(orig_broadcast       != broadcast      ) ||
		(orig_gateway         != gateway        ) ||
		(orig_hostname        != hostname       ) ||
		(orig_inet_static     != inet_static    )
		);
}

void CNetworkConfig::commitConfig(void)
{
	if (modified_from_orig())
	{
		if(orig_hostname != hostname)
			netSetHostname((char *) hostname.c_str());

		copy_to_orig();

		if (inet_static)
		{
			addLoopbackDevice("lo", true);
			setStaticAttributes("eth0", automatic_start, address, netmask, broadcast, gateway);
		}
		else
		{
			addLoopbackDevice("lo", true);
			setDhcpAttributes("eth0", automatic_start);
		}
	}
	if (nameserver != orig_nameserver)
	{
		orig_nameserver = nameserver;
		netSetNameserver(nameserver.c_str());
	}
}

int mysystem(char * cmd, char * arg1, char * arg2)
{
        int pid, i;
        switch (pid = fork())
        {
                case -1: /* can't fork */
                        perror("fork");
                        return -1;

                case 0: /* child process */
                        for(i = 3; i < 256; i++)
                                close(i);
                        if(execlp(cmd, cmd, arg1, arg2, NULL))
                        {
                                perror("exec");
                        }
                        exit(0);
                default: /* parent returns to calling process */
                        break;
        }
	waitpid(pid, 0, 0);
	return 0;
}

void CNetworkConfig::startNetwork(void)
{
	system("/sbin/ifup -v eth0");
	if (!inet_static) {
		init_vars();
	}
	//mysystem((char *) "ifup",  (char *) "-v",  (char *) "eth0");
}

void CNetworkConfig::stopNetwork(void)
{
	//mysystem("ifdown eth0", NULL, NULL);
	system("/sbin/ifdown eth0");
}
