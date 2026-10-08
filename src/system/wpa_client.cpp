/*
 * Wireless networks through the control interface of wpa_supplicant
 *
 * wpa_supplicant keeps the networks and the credentials in its config file
 * and is started for the interface by ifupdown, as boxes without iwd do it.
 * Requests go over its control socket, so neither D-Bus nor libwpa_client
 * is needed. The address comes from the DHCP client of ifupdown, which is
 * nudged once a network is connected.
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
#include <cstring>
#include <ctime>
#include <dirent.h>
#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <unistd.h>

#include <lib/libnet/libnet.h>
#include <lib/libnet/network_interfaces.h>
#include <system/helpers.h>

#include "wpa_client.h"

/* wpa_supplicant gives up on a network after a few failed attempts long before this */
#define CONNECT_TIMEOUT_MS	30000
#define REPLY_SIZE		65536

static const char *const ctrl_dirs[] = { "/var/run/wpa_supplicant", "/run/wpa_supplicant" };

static long long now_ms()
{
	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	return (long long)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}

static std::vector<std::string> split_fields(const std::string &s, char sep)
{
	std::vector<std::string> parts;
	size_t start = 0, pos;
	while ((pos = s.find(sep, start)) != std::string::npos)
	{
		parts.push_back(s.substr(start, pos - start));
		start = pos + 1;
	}
	parts.push_back(s.substr(start));
	return parts;
}

/* SSIDs come escaped as printf_encode() of wpa_supplicant writes them */
static std::string decode_ssid(const std::string &s)
{
	std::string out;
	for (size_t i = 0; i < s.length(); i++)
	{
		if (s[i] != '\\' || i + 1 >= s.length())
		{
			out += s[i];
			continue;
		}
		char c = s[++i];
		if (c == 'x' && i + 2 < s.length())
		{
			out += (char)strtol(s.substr(i + 1, 2).c_str(), NULL, 16);
			i += 2;
		}
		else if (c == 'n')
			out += '\n';
		else if (c == 'r')
			out += '\r';
		else if (c == 't')
			out += '\t';
		else if (c == 'e')
			out += '\033';
		else
			out += c;
	}
	return out;
}

/* a hex SSID takes any byte without quoting */
static std::string hex(const std::string &s)
{
	static const char digits[] = "0123456789abcdef";
	std::string out;
	for (size_t i = 0; i < s.length(); i++)
	{
		out += digits[(unsigned char)s[i] >> 4];
		out += digits[(unsigned char)s[i] & 15];
	}
	return out;
}

static std::string type_of(const std::string &flags)
{
	if (flags.find("EAP") != std::string::npos)
		return "8021x";
	if (flags.find("PSK") != std::string::npos || flags.find("SAE") != std::string::npos)
		return "psk";
	if (flags.find("WEP") != std::string::npos)
		return "wep";
	return "open";
}

CWpaClient *CWpaClient::getInstance()
{
	static CWpaClient *client = NULL;

	if (!client)
		client = new CWpaClient();
	return client;
}

/* the first socket in the control directory belongs to the wireless interface */
bool CWpaClient::findInterface()
{
	ifname.clear();
	ctrl_path.clear();
	for (size_t d = 0; d < sizeof(ctrl_dirs) / sizeof(ctrl_dirs[0]) && ifname.empty(); d++)
	{
		DIR *dir = opendir(ctrl_dirs[d]);
		if (!dir)
			continue;
		struct dirent *e;
		while ((e = readdir(dir)) != NULL)
		{
			std::string path = std::string(ctrl_dirs[d]) + "/" + e->d_name;
			struct stat st;
			if (e->d_name[0] == '.' || strncmp(e->d_name, "p2p-", 4) == 0)
				continue;
			if (stat(path.c_str(), &st) == 0 && S_ISSOCK(st.st_mode))
			{
				ifname = e->d_name;
				ctrl_path = path;
				break;
			}
		}
		closedir(dir);
	}
	return !ifname.empty();
}

int CWpaClient::open(bool attach)
{
	static int counter = 0;

	if (ctrl_path.empty() && !findInterface())
		return -1;

	int fd = socket(AF_UNIX, SOCK_DGRAM | SOCK_CLOEXEC, 0);
	if (fd < 0)
		return -1;

	/* wpa_supplicant answers to the address the request came from */
	struct sockaddr_un local;
	memset(&local, 0, sizeof(local));
	local.sun_family = AF_UNIX;
	snprintf(local.sun_path, sizeof(local.sun_path), "/tmp/neutrino-wpa-%d-%d", (int)getpid(), counter++);
	unlink(local.sun_path);
	if (bind(fd, (struct sockaddr *)&local, sizeof(local)) < 0)
	{
		::close(fd);
		return -1;
	}

	struct sockaddr_un dest;
	memset(&dest, 0, sizeof(dest));
	dest.sun_family = AF_UNIX;
	snprintf(dest.sun_path, sizeof(dest.sun_path), "%s", ctrl_path.c_str());
	if (::connect(fd, (struct sockaddr *)&dest, sizeof(dest)) < 0)
	{
		close(fd);
		ctrl_path.clear();
		return -1;
	}

	std::string reply;
	if (attach && (!request(fd, "ATTACH", reply) || reply.compare(0, 2, "OK") != 0))
	{
		close(fd);
		return -1;
	}
	return fd;
}

void CWpaClient::close(int fd)
{
	struct sockaddr_un local;
	socklen_t len = sizeof(local);
	if (getsockname(fd, (struct sockaddr *)&local, &len) == 0 && local.sun_path[0])
		unlink(local.sun_path);
	::close(fd);
}

/* the reply to a request; event messages of an attached socket start with '<' */
bool CWpaClient::request(int fd, const std::string &cmd, std::string &reply, int timeout_ms)
{
	reply.clear();
	if (send(fd, cmd.data(), cmd.length(), 0) < 0)
		return false;

	std::vector<char> buf(REPLY_SIZE);
	long long end = now_ms() + timeout_ms;
	for (;;)
	{
		int left = (int)(end - now_ms());
		if (left <= 0)
			return false;
		struct pollfd pfd = { fd, POLLIN, 0 };
		if (poll(&pfd, 1, left) <= 0)
			return false;
		ssize_t n = recv(fd, &buf[0], buf.size() - 1, 0);
		if (n < 0)
			return false;
		if (n > 0 && buf[0] == '<')
			continue;
		reply.assign(&buf[0], n);
		return true;
	}
}

bool CWpaClient::request(const std::string &cmd, std::string &reply, int timeout_ms)
{
	int fd = open(false);
	if (fd < 0)
		return false;
	bool ok = request(fd, cmd, reply, timeout_ms);
	close(fd);
	return ok;
}

bool CWpaClient::requestOk(const std::string &cmd)
{
	std::string reply;
	return request(cmd, reply) && reply.compare(0, 2, "OK") == 0;
}

/* the first of the events that comes, "<3>CTRL-EVENT-..." without the level */
bool CWpaClient::waitEvent(int fd, const std::vector<std::string> &events, std::string &event, int timeout_ms)
{
	std::vector<char> buf(REPLY_SIZE);
	long long end = now_ms() + timeout_ms;
	for (;;)
	{
		int left = (int)(end - now_ms());
		if (left <= 0)
			return false;
		struct pollfd pfd = { fd, POLLIN, 0 };
		if (poll(&pfd, 1, left) <= 0)
			return false;
		ssize_t n = recv(fd, &buf[0], buf.size() - 1, 0);
		if (n <= 0)
			return false;
		std::string msg(&buf[0], n);
		size_t gt = msg.find('>');
		if (msg[0] != '<' || gt == std::string::npos)
			continue;
		msg = msg.substr(gt + 1);
		for (size_t i = 0; i < events.size(); i++)
		{
			if (msg.compare(0, events[i].length(), events[i]) == 0)
			{
				event = msg;
				return true;
			}
		}
	}
}

std::map<std::string, std::string> CWpaClient::status()
{
	std::map<std::string, std::string> values;
	std::string reply;
	if (!request("STATUS", reply))
		return values;
	std::vector<std::string> lines = split_fields(reply, '\n');
	for (size_t i = 0; i < lines.size(); i++)
	{
		size_t eq = lines[i].find('=');
		if (eq != std::string::npos)
			values[lines[i].substr(0, eq)] = lines[i].substr(eq + 1);
	}
	return values;
}

bool CWpaClient::available()
{
	std::string reply;
	ctrl_path.clear();
	return findInterface() && request("PING", reply) && reply.compare(0, 4, "PONG") == 0;
}

std::string CWpaClient::deviceName()
{
	if (ifname.empty())
		findInterface();
	return ifname;
}

std::string CWpaClient::connectedNetwork()
{
	if (!available())
		return "";
	std::map<std::string, std::string> s = status();
	return s["wpa_state"] == "COMPLETED" ? decode_ssid(s["ssid"]) : "";
}

bool CWpaClient::scan(int timeout_ms)
{
	if (!available())
		return false;

	int fd = open(true);
	if (fd < 0)
		return false;

	/* "FAIL-BUSY" means a scan is running already, waiting for it is just as good */
	std::string reply;
	request(fd, "SCAN", reply);
	std::vector<std::string> events(1, "CTRL-EVENT-SCAN-RESULTS");
	events.push_back("CTRL-EVENT-SCAN-FAILED");
	std::string event;
	waitEvent(fd, events, event, timeout_ms);
	request(fd, "DETACH", reply);
	close(fd);
	return true;
}

bool CWpaClient::getNetworks(std::vector<wireless_network> &networks)
{
	networks.clear();
	flags.clear();
	if (!available())
		return false;

	std::string reply;
	if (!request("SCAN_RESULTS", reply))
		return false;

	std::map<std::string, std::string> s = status();
	const bool completed = s["wpa_state"] == "COMPLETED";
	const std::string current = decode_ssid(s["ssid"]);

	/* the stored networks by SSID */
	std::map<std::string, std::string> known;
	std::string list;
	if (request("LIST_NETWORKS", list))
	{
		std::vector<std::string> lines = split_fields(list, '\n');
		for (size_t i = 1; i < lines.size(); i++)
		{
			std::vector<std::string> f = split_fields(lines[i], '\t');
			if (f.size() >= 2)
				known[decode_ssid(f[1])] = f[0];
		}
	}

	/* bssid, frequency, signal level, flags, ssid; one line per access point,
	 * of a network with several the strongest counts */
	std::map<std::string, size_t> index;
	std::vector<std::string> lines = split_fields(reply, '\n');
	for (size_t i = 1; i < lines.size(); i++)
	{
		std::vector<std::string> f = split_fields(lines[i], '\t');
		if (f.size() < 5)
			continue;
		std::string name = decode_ssid(f[4]);
		if (name.empty() || name.find_first_not_of('\0') == std::string::npos)
			continue;
		int signal = atoi(f[2].c_str()) * 100;

		std::map<std::string, size_t>::iterator it = index.find(name);
		if (it != index.end())
		{
			if (signal > networks[it->second].signal)
				networks[it->second].signal = signal;
			continue;
		}

		wireless_network n;
		n.id = name;
		n.name = name;
		n.type = type_of(f[3]);
		n.known_id = known.count(name) ? known[name] : "";
		n.signal = signal;
		n.connected = completed && name == current;
		flags[name] = f[3];
		index[name] = networks.size();
		networks.push_back(n);
	}

	/* strongest first, as iwd orders them */
	for (size_t i = 1; i < networks.size(); i++)
		for (size_t j = i; j > 0 && networks[j].signal > networks[j - 1].signal; j--)
			std::swap(networks[j], networks[j - 1]);
	return true;
}

/* a new network in the list of wpa_supplicant, its number or -1 */
int CWpaClient::addNetwork(const std::string &ssid, std::string &passphrase, const std::string &type, bool hidden)
{
	std::string reply;
	if (!request("ADD_NETWORK", reply))
	{
		wipe(passphrase);
		return -1;
	}
	int id = atoi(reply.c_str());
	std::string n = reply.substr(0, reply.find_first_of("\r\n"));

	bool ok = requestOk("SET_NETWORK " + n + " ssid " + hex(ssid));
	if (ok && hidden)
		ok = requestOk("SET_NETWORK " + n + " scan_ssid 1");
	if (ok && type == "open")
		ok = requestOk("SET_NETWORK " + n + " key_mgmt NONE");
	else if (ok)
	{
		/* a network that offers SAE only needs it, with management frame protection */
		const std::string &f = flags[ssid];
		if (f.find("SAE") != std::string::npos && f.find("PSK") == std::string::npos)
			ok = requestOk("SET_NETWORK " + n + " key_mgmt SAE") && requestOk("SET_NETWORK " + n + " ieee80211w 2");
		std::string cmd = "SET_NETWORK " + n + " psk \"" + passphrase + "\"";
		ok = ok && requestOk(cmd);
		wipe(cmd);
	}
	wipe(passphrase);

	if (!ok)
	{
		requestOk("REMOVE_NETWORK " + n);
		return -1;
	}
	return id;
}

/* connects to a network over an attached socket and keeps it when it works */
int CWpaClient::select(int fd, const std::string &id, bool added)
{
	std::string reply;
	if (!request(fd, "SELECT_NETWORK " + id, reply) || reply.compare(0, 2, "OK") != 0)
		return CONNECT_FAILED;

	std::vector<std::string> events(1, "CTRL-EVENT-CONNECTED");
	events.push_back("CTRL-EVENT-SSID-TEMP-DISABLED");
	events.push_back("CTRL-EVENT-NETWORK-NOT-FOUND");
	std::string event;
	bool connected = waitEvent(fd, events, event, CONNECT_TIMEOUT_MS)
		&& event.compare(0, 20, "CTRL-EVENT-CONNECTED") == 0;

	/* SELECT_NETWORK disables every other network, they are to stay usable */
	if (!connected && added)
		request(fd, "REMOVE_NETWORK " + id, reply);
	request(fd, "ENABLE_NETWORK all", reply);
	if (!connected)
		return CONNECT_FAILED;

	request(fd, "SAVE_CONFIG", reply);
	startDhcp();
	return CONNECT_OK;
}

/* the DHCP client of ifupdown gave up while there was no network; busybox
 * ifupdown runs udhcpc with this pid file and stops it by it */
void CWpaClient::startDhcp()
{
	bool automatic_start;
	std::string address, netmask, broadcast, gateway;
	if (getInetAttributes(ifname, automatic_start, address, netmask, broadcast, gateway))
		return;

	std::string pidfile = "/var/run/udhcpc." + ifname + ".pid";
	FILE *f = fopen(pidfile.c_str(), "r");
	if (f)
	{
		int pid = 0;
		if (fscanf(f, "%d", &pid) == 1 && pid > 0 && kill(pid, SIGUSR1) == 0)
		{
			fclose(f);
			return;
		}
		fclose(f);
	}

	std::string udhcpc = find_executable("udhcpc");
	if (udhcpc.empty())
	{
		printf("CWpaClient::startDhcp: udhcpc not found\n");
		return;
	}
	my_system(8, udhcpc.c_str(), "-R", "-b", "-p", pidfile.c_str(), "-i", ifname.c_str(), "-S");
}

int CWpaClient::connect(const wireless_network &network, std::string &passphrase)
{
	if (!available())
	{
		wipe(passphrase);
		return CONNECT_UNAVAILABLE;
	}
	if (network.type != "open" && network.type != "psk")
	{
		wipe(passphrase);
		return CONNECT_NOT_SUPPORTED;
	}

	int fd = open(true);
	if (fd < 0)
	{
		wipe(passphrase);
		return CONNECT_UNAVAILABLE;
	}

	int result;
	if (!network.known_id.empty())
	{
		wipe(passphrase);
		result = select(fd, network.known_id, false);
	}
	else
	{
		int id = addNetwork(network.name, passphrase, network.type, false);
		char n[16];
		snprintf(n, sizeof(n), "%d", id);
		result = id < 0 ? CONNECT_FAILED : select(fd, n, true);
	}

	std::string reply;
	request(fd, "DETACH", reply);
	close(fd);
	return result;
}

int CWpaClient::connectHidden(const std::string &ssid, std::string &passphrase)
{
	if (!available())
	{
		wipe(passphrase);
		return CONNECT_UNAVAILABLE;
	}

	int fd = open(true);
	if (fd < 0)
	{
		wipe(passphrase);
		return CONNECT_UNAVAILABLE;
	}

	int id = addNetwork(ssid, passphrase, passphrase.empty() ? "open" : "psk", true);
	char n[16];
	snprintf(n, sizeof(n), "%d", id);
	int result = id < 0 ? CONNECT_FAILED : select(fd, n, true);

	std::string reply;
	request(fd, "DETACH", reply);
	close(fd);
	return result;
}

bool CWpaClient::disconnect()
{
	return available() && requestOk("DISCONNECT");
}

bool CWpaClient::forget(const wireless_network &network)
{
	if (network.known_id.empty() || !available())
		return false;
	if (!requestOk("REMOVE_NETWORK " + network.known_id))
		return false;
	requestOk("SAVE_CONFIG");
	return true;
}
