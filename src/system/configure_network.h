#ifndef __configure_network_h__
#define __configure_network_h__

/*
 * $port: configure_network.h,v 1.4 2009/11/20 22:44:19 tuxbox-cvs Exp $
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

#include <string>

class CNetworkConfig
{
 private:
	bool        orig_automatic_start;
	std::string orig_address;
	std::string orig_netmask;
	std::string orig_broadcast;
	std::string orig_gateway;
	std::string orig_nameserver;
	std::string orig_hostname;
	bool        orig_inet_static;
	std::string orig_ifname;
	std::string orig_ssid;
	std::string orig_key;

	void copy_to_orig(void);
	void init_vars(void);
	void readWpaConfig();
	void saveWpaConfig();

 public:
	bool        automatic_start;
	std::string address;
	std::string netmask;
	std::string broadcast;
	std::string gateway;
	std::string nameserver;
	std::string hostname;
	std::string mac_addr;
	std::string ifname;
	std::string ssid;
	std::string key;
	bool        inet_static;
	bool	    wireless;

	CNetworkConfig();
	~CNetworkConfig();

	static CNetworkConfig* getInstance();
	bool modified_from_orig(void);

	void readConfig(std::string iname);
	void commitConfig(void);

	void startNetwork(void);
	void stopNetwork(void);
	void setIfName(std::string name) { ifname = name;};
};

class CNetAdapter
{
	private:
		long mac_addr_sys ( u_char *addr);	
	public:
		std::string getMacAddr(void);
};

#endif /* __configure_network_h__ */
