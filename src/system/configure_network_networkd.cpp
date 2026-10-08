/*
 * Network configuration through systemd-networkd
 *
 * One file per interface in /etc/systemd/network holds the settings made
 * here, networkd picks it up on "networkctl reload". A wireless interface
 * whose addresses iwd sets itself keeps them in the [IPv4] section of the
 * profile iwd has stored for the connected network instead.
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
#include <arpa/inet.h>
#include <ctype.h>
#include <cstdio>
#include <errno.h>
#include <stdint.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include "configure_network.h"
#include <lib/libnet/libnet.h>
#include <fstream>
#include <utility>
#include <vector>
#include <system/helpers.h>
#ifdef ENABLE_IWD
#include <system/iwd_client.h>
#endif

#define NETWORKD_CONFIG_DIR "/etc/systemd/network"
/* sorts before the distribution's catch-all files such as 80-wired.network */
#define NETWORKD_CONFIG_PREFIX "50-neutrino-"
#define NETWORKD_RUNTIME_DIR "/run/systemd/netif"
#define IWD_MAIN_CONFIG "/etc/iwd/main.conf"
#define IWD_STORAGE_DIR "/var/lib/iwd"

static std::string config_file(const std::string &ifname)
{
	return std::string(NETWORKD_CONFIG_DIR "/" NETWORKD_CONFIG_PREFIX) + ifname + ".network";
}

static std::string trim(const std::string &s)
{
	std::string::size_type b = s.find_first_not_of(" \t\r");
	if (b == std::string::npos)
		return "";
	return s.substr(b, s.find_last_not_of(" \t\r") - b + 1);
}

static int netmask_to_prefix(const std::string &netmask)
{
	struct in_addr in;
	if (inet_pton(AF_INET, netmask.c_str(), &in) != 1)
		return -1;

	uint32_t m = ntohl(in.s_addr);
	int prefix = 0;
	while (m & 0x80000000u)
	{
		prefix++;
		m <<= 1;
	}
	/* the bits of a netmask are contiguous */
	return m ? -1 : prefix;
}

static std::string prefix_to_netmask(int prefix)
{
	if (prefix < 0 || prefix > 32)
		return "";

	struct in_addr in;
	in.s_addr = htonl(prefix ? 0xffffffffu << (32 - prefix) : 0);
	char buf[INET_ADDRSTRLEN];
	return inet_ntop(AF_INET, &in, buf, sizeof(buf)) ? buf : "";
}

static bool is_wireless(const std::string &ifname)
{
	return access(("/sys/class/net/" + ifname + "/wireless").c_str(), F_OK) == 0;
}

/* iwd sets the addresses of its interfaces itself, networkd leaves them alone */
static bool iwd_configures_network(void)
{
	std::ifstream in(IWD_MAIN_CONFIG);
	std::string line, section;
	bool enabled = false;
	while (getline(in, line))
	{
		line = trim(line);
		if (line.empty() || line[0] == '#')
			continue;
		if (line[0] == '[')
		{
			section = line;
			continue;
		}
		std::string::size_type eq = line.find('=');
		if (section == "[General]" && eq != std::string::npos && trim(line.substr(0, eq)) == "EnableNetworkConfiguration")
			enabled = (trim(line.substr(eq + 1)) == "true");
	}
	return enabled;
}

/* the settings file iwd keeps for the network the interface is connected
 * to, the address settings of a network go into its [IPv4] section */
static std::string iwd_profile(const std::string &ifname)
{
#ifdef ENABLE_IWD
	CIwdClient *iwd = CIwdClient::getInstance();
	if (!iwd->available() || iwd->deviceName() != ifname)
		return "";

	std::vector<wireless_network> networks;
	iwd->getNetworks(networks);
	for (size_t i = 0; i < networks.size(); i++)
	{
		const wireless_network &n = networks[i];
		if (!n.connected || n.known_id.empty())
			continue;

		/* names with other characters than these are stored hex encoded */
		bool plain = true;
		for (size_t c = 0; c < n.name.length() && plain; c++)
			plain = isalnum((unsigned char)n.name[c]) || strchr(" _-", n.name[c]);
		std::string file = n.name;
		if (!plain)
		{
			file = "=";
			for (size_t c = 0; c < n.name.length(); c++)
			{
				char hex[3];
				snprintf(hex, sizeof(hex), "%02x", (unsigned char)n.name[c]);
				file += hex;
			}
		}
		file = std::string(IWD_STORAGE_DIR "/") + file + "." + n.type;
		return access(file.c_str(), R_OK | W_OK) == 0 ? file : "";
	}
#else
	(void)ifname;
#endif
	return "";
}

static bool write_file(const std::string &file, const std::string &content, mode_t mode)
{
	std::string tmp = file + ".new";
	unlink(tmp.c_str());
	int fd = open(tmp.c_str(), O_WRONLY | O_CREAT | O_EXCL | O_NOFOLLOW | O_CLOEXEC, mode);
	if (fd < 0)
	{
		perror(tmp.c_str());
		return false;
	}
	bool ok = write(fd, content.c_str(), content.length()) == (ssize_t)content.length();
	ok = (fchmod(fd, mode) == 0) && ok;
	ok = (fsync(fd) == 0) && ok;
	close(fd);
	if (!ok || rename(tmp.c_str(), file.c_str()) < 0)
	{
		perror(file.c_str());
		unlink(tmp.c_str());
		return false;
	}
	return true;
}

bool CNetworkConfig::canConfigure(void)
{
	if (systemManaged())
		return false;
	if (access(NETWORKD_RUNTIME_DIR, F_OK) != 0)
		return false;
	if (is_wireless(ifname) && iwd_configures_network())
		return !iwd_profile(ifname).empty();
	return true;
}

bool CNetworkConfig::hasAutomaticStart(void)
{
	return false;
}

static void read_iwd_profile(const std::string &file, std::string &address, std::string &netmask, std::string &gateway, std::string &nameserver)
{
	std::ifstream in(file.c_str());
	std::string line, section;
	while (getline(in, line))
	{
		line = trim(line);
		if (line.empty() || line[0] == '#')
			continue;
		if (line[0] == '[')
		{
			section = line;
			continue;
		}
		std::string::size_type eq = line.find('=');
		if (eq == std::string::npos || section != "[IPv4]")
			continue;

		std::string name = trim(line.substr(0, eq));
		std::string value = trim(line.substr(eq + 1));
		if (name == "Address")
			address = value;
		else if (name == "Netmask")
			netmask = value;
		else if (name == "Gateway")
			gateway = value;
		else if (name == "DNS")
			nameserver = value.substr(0, value.find(' '));
	}
}

/* the profile without the address settings, with new ones when static;
 * an [IPv4] section that is left empty goes */
static std::string iwd_profile_content(const std::string &file, const std::string &ipv4)
{
	std::ifstream in(file.c_str());
	std::vector<std::pair<std::string, std::string> > sections(1);
	std::string line;
	while (getline(in, line))
	{
		std::string t = trim(line);
		if (!t.empty() && t[0] == '[')
		{
			sections.push_back(std::make_pair(t, ""));
			continue;
		}
		std::string::size_type eq = t.find('=');
		if (sections.back().first == "[IPv4]" && eq != std::string::npos)
		{
			std::string name = trim(t.substr(0, eq));
			if (name == "Address" || name == "Netmask" || name == "Gateway" || name == "Broadcast" || name == "DNS")
				continue;
		}
		if (!t.empty())
			sections.back().second += line + "\n";
	}

	bool written = false;
	for (size_t i = 0; i < sections.size(); i++)
		if (sections[i].first == "[IPv4]" && !written)
		{
			sections[i].second = ipv4 + sections[i].second;
			written = true;
		}
	if (!written)
		sections.push_back(std::make_pair(std::string("[IPv4]"), ipv4));

	std::string out;
	for (size_t i = 0; i < sections.size(); i++)
	{
		if (sections[i].first.empty())
		{
			out += sections[i].second;
			continue;
		}
		if (sections[i].second.empty() && sections[i].first == "[IPv4]")
			continue;
		if (!out.empty())
			out += "\n";
		out += sections[i].first + "\n" + sections[i].second;
	}
	return out;
}

void CNetworkConfig::backendRead(void)
{
	inet_static = false;
	automatic_start = true;

	if (is_wireless(ifname) && iwd_configures_network())
	{
		std::string _address, _netmask, _gateway, _nameserver;
		read_iwd_profile(iwd_profile(ifname), _address, _netmask, _gateway, _nameserver);
		if (!validAddress(_address) || !validAddress(_netmask))
			return;
		inet_static = true;
		address = _address;
		netmask = _netmask;
		gateway = validAddress(_gateway) ? _gateway : "";
		nameserver = validAddress(_nameserver) ? _nameserver : "";
		struct in_addr a, m;
		inet_pton(AF_INET, address.c_str(), &a);
		inet_pton(AF_INET, netmask.c_str(), &m);
		a.s_addr |= ~m.s_addr;
		char buf[INET_ADDRSTRLEN];
		broadcast = inet_ntop(AF_INET, &a, buf, sizeof(buf)) ? buf : "";
		return;
	}

	std::ifstream in(config_file(ifname).c_str());
	if (!in.is_open())
		return;

	std::string line, section, _address, _netmask, _gateway, _nameserver;
	bool dhcp = false;
	while (getline(in, line))
	{
		line = trim(line);
		if (line.empty() || line[0] == '#' || line[0] == ';')
			continue;
		if (line[0] == '[')
		{
			section = line;
			continue;
		}
		std::string::size_type eq = line.find('=');
		if (eq == std::string::npos || section != "[Network]")
			continue;

		std::string name = trim(line.substr(0, eq));
		std::string value = trim(line.substr(eq + 1));
		if (name == "DHCP")
			dhcp = (value == "yes" || value == "ipv4" || value == "true");
		else if (name == "Address" && _address.empty())
		{
			std::string::size_type slash = value.find('/');
			if (slash == std::string::npos)
				continue;
			std::string a = value.substr(0, slash);
			std::string m = prefix_to_netmask(atoi(value.substr(slash + 1).c_str()));
			if (validAddress(a) && !m.empty())
			{
				_address = a;
				_netmask = m;
			}
		}
		else if (name == "Gateway" && _gateway.empty() && validAddress(value))
			_gateway = value;
		else if (name == "DNS" && _nameserver.empty() && validAddress(value))
			_nameserver = value;
	}

	if (dhcp || _address.empty())
		return;

	inet_static = true;
	address = _address;
	netmask = _netmask;
	gateway = _gateway;
	nameserver = _nameserver;

	struct in_addr a, m;
	inet_pton(AF_INET, address.c_str(), &a);
	inet_pton(AF_INET, netmask.c_str(), &m);
	a.s_addr |= ~m.s_addr;
	char buf[INET_ADDRSTRLEN];
	broadcast = inet_ntop(AF_INET, &a, buf, sizeof(buf)) ? buf : "";
}

void CNetworkConfig::backendCommit(bool modified, bool nameserver_changed)
{
	if (!modified && !nameserver_changed)
		return;

	if (!validInterface(ifname))
		return;

	if (is_wireless(ifname) && iwd_configures_network())
	{
		std::string profile = iwd_profile(ifname);
		if (profile.empty())
			return;
		std::string ipv4;
		if (inet_static)
		{
			if (!validAddress(address) || netmask_to_prefix(netmask) < 0 ||
			    (!gateway.empty() && !validAddress(gateway)) ||
			    (!nameserver.empty() && !validAddress(nameserver)))
			{
				printf("CNetworkConfig::commitConfig: invalid address, %s not written\n", profile.c_str());
				return;
			}
			ipv4 = "Address=" + address + "\nNetmask=" + netmask + "\n";
			if (!gateway.empty())
				ipv4 += "Gateway=" + gateway + "\n";
			if (!nameserver.empty())
				ipv4 += "DNS=" + nameserver + "\n";
		}
		write_file(profile, iwd_profile_content(profile, ipv4), 0600);
		return;
	}

	std::string conf = "# generated by neutrino\n";
	conf += "[Match]\n";
	conf += "Name=" + ifname + "\n";
	conf += "\n";
	conf += "[Link]\n";
	conf += "RequiredForOnline=no\n";
	conf += "\n";
	conf += "[Network]\n";
	if (inet_static)
	{
		int prefix = netmask_to_prefix(netmask);
		if (!validAddress(address) || prefix < 0 ||
		    (!gateway.empty() && !validAddress(gateway)) ||
		    (!nameserver.empty() && !validAddress(nameserver)))
		{
			printf("CNetworkConfig::commitConfig: invalid address, %s not written\n", ifname.c_str());
			return;
		}
		char p[16];
		snprintf(p, sizeof(p), "/%d", prefix);
		conf += "Address=" + address + p + "\n";
		if (!gateway.empty())
			conf += "Gateway=" + gateway + "\n";
		if (!nameserver.empty())
			conf += "DNS=" + nameserver + "\n";
	}
	else
	{
		conf += "DHCP=yes\n";
		conf += "\n";
		conf += "[DHCP]\n";
		conf += "UseMTU=yes\n";
		conf += "RouteMetric=10\n";
		conf += "ClientIdentifier=mac\n";
	}

	if (mkdir(NETWORKD_CONFIG_DIR, 0755) < 0 && errno != EEXIST)
	{
		perror(NETWORKD_CONFIG_DIR);
		return;
	}

	/* networkd only reads *.network, the temporary name is invisible to it */
	write_file(config_file(ifname), conf, 0644);
}

/* iwd takes the address settings of a network when it connects to it */
static void iwd_reconnect(const std::string &ifname)
{
#ifdef ENABLE_IWD
	CIwdClient *iwd = CIwdClient::getInstance();
	if (!iwd->available() || iwd->deviceName() != ifname)
		return;

	std::vector<wireless_network> networks;
	iwd->getNetworks(networks);
	for (size_t i = 0; i < networks.size(); i++)
	{
		if (!networks[i].connected)
			continue;
		std::string passphrase;
		iwd->disconnect();
		iwd->connect(networks[i], passphrase);
		return;
	}
#else
	(void)ifname;
#endif
}

void CNetworkConfig::startNetwork(void)
{
	if (!canConfigure() || !validInterface(ifname))
		return;

	if (is_wireless(ifname) && iwd_configures_network())
	{
		iwd_reconnect(ifname);
		waitForAddress();
		return;
	}

	std::string networkctl = find_executable("networkctl");
	if (networkctl.empty())
	{
		printf("CNetworkConfig::startNetwork: networkctl not found\n");
		return;
	}

	my_system(2, networkctl.c_str(), "reload");
	my_system(3, networkctl.c_str(), "reconfigure", ifname.c_str());

	if (inet_static)
		return;

	waitForAddress();
}

/* give the DHCP client a moment, the menu shows the address next */
void CNetworkConfig::waitForAddress(void)
{
	for (int i = 0; i < 32; i++)
	{
		std::string ip, mask, brd;
		netGetIP(ifname, ip, mask, brd);
		if (!ip.empty() && ip != "0.0.0.0")
			break;
		usleep(250000);
	}
	init_vars();
}

void CNetworkConfig::stopNetwork(void)
{
	/* networkd replaces the running setup on reconfigure */
}
