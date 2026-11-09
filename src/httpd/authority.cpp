/*
 * authority.cpp - the names a request may reach this box under
 *
 * Copyright (C) 2026 Thilo Graf
 *
 * License: GPL-2.0-or-later
 */

#include "authority.h"

#include "netmatch.h"
#include "webconfig.h"

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include <strings.h>
#include <time.h>
#include <unistd.h>

#include <sys/stat.h>

#include <arpa/inet.h>
#include <netinet/in.h>

#include <OpenThreads/Mutex>
#include <OpenThreads/ScopedLock>

namespace httpd
{

namespace
{

const char kResolvConf[] = "/etc/resolv.conf";

/* How long the host name, the resolver's domains and the access file's names
   are kept before they are looked at again. A file read per request would be
   the cost of every request; a lease that brings a new domain, and a name the
   owner adds to the access file, are taken within this. */
const time_t kReadEvery = 10;

// A resolver file larger than this is read only this far.
const size_t kResolvConfMax = 64 * 1024;

struct Sources
{
	bool        overridden;
	std::string host_name;
	std::string resolv_conf;
	time_t      clock;

	bool        read;
	time_t      read_at;
	BoxNames    held;

	/* The access file the names of extra were read from, and how it looked then:
	   read again only when it changed. Until it could be read once, extra is what
	   the configuration in effect holds. */
	std::string access_path;
	bool        access_read;
	struct stat access_stat;

	Sources() : overridden(false), clock(0), read(false), read_at(0), access_read(false)
	{
		std::memset(&access_stat, 0, sizeof(access_stat));
	}
};

// Built on first use: nothing in this directory may need a constructor before main.
Sources &sources()
{
	static Sources s;
	return s;
}

OpenThreads::Mutex &lock()
{
	static OpenThreads::Mutex m;
	return m;
}

time_t monotonicSeconds()
{
	if (sources().clock != 0)
		return sources().clock;
	struct timespec t;
	if (clock_gettime(CLOCK_MONOTONIC, &t) != 0)
		return 0;
	return t.tv_sec;
}

std::string lowered(const std::string &s)
{
	std::string out(s);
	for (size_t i = 0; i < out.size(); ++i)
		if (out[i] >= 'A' && out[i] <= 'Z')
			out[i] = (char) (out[i] - 'A' + 'a');
	return out;
}

// One to five digits and no more than the widest port.
bool portAcceptable(const std::string &port)
{
	if (port.empty() || port.size() > 5)
		return false;
	unsigned long v = 0;
	for (size_t i = 0; i < port.size(); ++i)
	{
		if (port[i] < '0' || port[i] > '9')
			return false;
		v = v * 10 + (unsigned long) (port[i] - '0');
	}
	return v <= 65535;
}

bool isAddressOf(int family, const std::string &text)
{
	if (text.empty() || text.size() >= INET6_ADDRSTRLEN || text.find('\0') != std::string::npos)
		return false;
	unsigned char bits[16];
	return inet_pton(family, text.c_str(), bits) == 1;
}

std::string fileText(const std::string &path)
{
	std::string out;
	std::FILE *f = std::fopen(path.c_str(), "r");
	if (f == NULL)
		return out;
	char buf[4096];
	size_t n;
	while (out.size() < kResolvConfMax && (n = std::fread(buf, 1, sizeof(buf), f)) > 0)
		out.append(buf, n);
	std::fclose(f);
	return out;
}

std::string systemHostName()
{
	char name[256];
	if (gethostname(name, sizeof(name)) != 0)
		return std::string();
	name[sizeof(name) - 1] = '\0';
	return name;
}

bool sameFile(const struct stat &a, const struct stat &b)
{
	return a.st_ino == b.st_ino && a.st_dev == b.st_dev && a.st_size == b.st_size &&
	       a.st_mtim.tv_sec == b.st_mtim.tv_sec && a.st_mtim.tv_nsec == b.st_mtim.tv_nsec;
}

/* The access file's names, read again when the file changed since they were
   read. A file that cannot be read now leaves what was read before. Called
   with the lock held. */
void readExtraLocked(Sources &s, const std::string &path)
{
	if (path != s.access_path)
	{
		s.access_path = path;
		s.access_read = false;
	}
	struct stat st;
	if (path.empty() || stat(path.c_str(), &st) != 0)
		return;
	if (s.access_read && sameFile(st, s.access_stat))
		return;
	std::vector<std::string> names;
	if (!readAllowedHosts(path, names))
		return;
	s.held.extra = names;
	s.access_stat = st;
	s.access_read = true;
}

bool isBlank(char c)
{
	return c == ' ' || c == '\t' || c == '\r';
}

/* The address the box answered on as the start of an address a person can open,
   under the scheme the server speaks, and empty where it is none or the loopback,
   which names nothing on the network. */
std::string openableAddress(const std::string &local_authority, bool https)
{
	const size_t colon = local_authority.rfind(':');
	const std::string ip = local_authority.substr(0, colon);
	if (!isAddressOf(AF_INET, ip) || ip.compare(0, 4, "127.") == 0)
		return std::string();
	const std::string scheme = https ? "https://" : "http://";
	if (colon == std::string::npos ||
	    local_authority.compare(colon + 1, std::string::npos, https ? "443" : "80") == 0)
		return scheme + ip + "/";
	return scheme + local_authority + "/";
}

/* Whether the page may name the box's address to this peer: one on the home
   network, with no proxy in front, which could forward a caller from anywhere. */
bool peerOnHomeNetwork(const std::string &peer)
{
	return !proxiesConfigured(config()) && addressInAnyPrefix(peer, defaultLanPrefixes());
}

} // namespace

std::string comparableName(const std::string &text)
{
	std::string out = lowered(text);
	if (!out.empty() && out[out.size() - 1] == '.')
		out.erase(out.size() - 1);
	return out;
}

bool isHostName(const std::string &text)
{
	std::string n = text;
	if (!n.empty() && n[n.size() - 1] == '.')
		n.erase(n.size() - 1);
	if (n.empty() || n.size() > 253)
		return false;

	size_t start = 0;
	for (;;)
	{
		const size_t dot = n.find('.', start);
		const size_t end = (dot == std::string::npos) ? n.size() : dot;
		if (end == start || end - start > 63)
			return false;
		if (n[start] == '-' || n[end - 1] == '-')
			return false;
		for (size_t i = start; i < end; ++i)
		{
			const char c = n[i];
			if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '-'))
				return false;
		}
		if (dot == std::string::npos)
			return true;
		start = dot + 1;
	}
}

std::vector<std::string> resolverSuffixes(const std::string &text)
{
	std::vector<std::string> out;
	size_t at = 0;
	while (at < text.size())
	{
		size_t end = text.find('\n', at);
		if (end == std::string::npos)
			end = text.size();
		const std::string line = text.substr(at, end - at);
		at = end + 1;

		std::vector<std::string> words;
		size_t i = 0;
		while (i < line.size())
		{
			while (i < line.size() && isBlank(line[i]))
				++i;
			const size_t from = i;
			while (i < line.size() && !isBlank(line[i]))
				++i;
			if (i > from)
				words.push_back(line.substr(from, i - from));
		}
		if (words.empty() || (words[0] != "search" && words[0] != "domain"))
			continue;

		for (size_t w = 1; w < words.size(); ++w)
		{
			if (words[w][0] == '#' || words[w][0] == ';')
				break;
			if (!isHostName(words[w]))
				continue;
			const std::string name = comparableName(words[w]);
			bool seen = false;
			for (size_t k = 0; k < out.size() && !seen; ++k)
				seen = out[k] == name;
			if (!seen)
				out.push_back(name);
		}
	}
	return out;
}

bool authorityAccepted(const std::string &host, const BoxNames &names)
{
	if (host.empty())
		return true;

	if (host[0] == '[')
	{
		const size_t close = host.find(']');
		if (close == std::string::npos)
			return false;
		if (close + 1 < host.size() &&
		    (host[close + 1] != ':' || !portAcceptable(host.substr(close + 2))))
			return false;
		return isAddressOf(AF_INET6, host.substr(1, close - 1));
	}

	std::string name = host;
	const size_t colon = host.find(':');
	if (colon != std::string::npos)
	{
		// a second colon is an IPv6 address without its brackets
		if (host.find(':', colon + 1) != std::string::npos || !portAcceptable(host.substr(colon + 1)))
			return false;
		name = host.substr(0, colon);
	}

	if (isAddressOf(AF_INET, name))
		return true;
	if (!isHostName(name))
		return false;

	/* A single label, localhost among them, and one label under a zone of the home
	   network: mDNS (.local) and each domain the resolver searches. The router or the
	   devices on the network answer those names and nobody outside can register one,
	   so another page cannot have one lead to the box; and every name the router or
	   another device gives the box is one of them. */
	const std::string n = comparableName(name);
	const size_t dot = n.find('.');
	if (dot == std::string::npos)
		return true;
	const std::string zone = n.substr(dot + 1);
	if (zone == "local")
		return true;
	for (size_t i = 0; i < names.suffixes.size(); ++i)
		if (zone == names.suffixes[i])
			return true;

	if (!names.host_name.empty() && n == names.host_name)
		return true;

	for (size_t i = 0; i < names.extra.size(); ++i)
		if (n == names.extra[i])
			return true;
	return false;
}

BoxNames boxNames()
{
	// the file the configuration in effect was read from; written only while no server runs
	const std::string access = configPath();

	OpenThreads::ScopedLock<OpenThreads::Mutex> held(lock());
	Sources &s = sources();
	const time_t now = monotonicSeconds();
	if (!s.read || now < s.read_at || now - s.read_at >= kReadEvery)
	{
		const std::string host = s.overridden ? s.host_name : systemHostName();
		s.held.host_name = isHostName(host) ? comparableName(host) : std::string();
		s.held.suffixes = resolverSuffixes(fileText(s.overridden ? s.resolv_conf : std::string(kResolvConf)));
		readExtraLocked(s, access);
		s.read = true;
		s.read_at = now;
	}
	if (access != s.access_path)
		s.access_read = false;

	BoxNames out = s.held;
	if (!s.access_read)
		out.extra = config().allowed_hosts;
	return out;
}

bool hostAccepted(const std::string &host)
{
	try
	{
		// an address, a single label, a .local name or no header at all, without reading any name
		if (authorityAccepted(host, BoxNames()))
			return true;
		return authorityAccepted(host, boxNames());
	}
	catch (...)
	{
		return false;
	}
}

bool hostHeaderAccepted(const std::map<std::string, std::string> &headers)
{
	try
	{
		size_t named = 0;
		std::string host;
		for (std::map<std::string, std::string>::const_iterator it = headers.begin(); it != headers.end(); ++it)
		{
			if (strcasecmp(it->first.c_str(), "Host") != 0)
				continue;
			++named;
			host = it->second;
		}
		return named <= 1 && hostAccepted(host);
	}
	catch (...)
	{
		return false;
	}
}

std::string unknownHostPage(const std::string &local_authority, const std::string &peer, bool https)
{
	try
	{
		const std::string at = peerOnHomeNetwork(peer) ? openableAddress(local_authority, https)
		                                               : std::string();
		// the file the box reads, where it was told one
		const std::string &file = configPath();
		const std::string in_en = file.empty()
			? std::string("nhttpd-access.conf in the configuration folder of the box") : file;
		const std::string in_de = file.empty()
			? std::string("nhttpd-access.conf im Konfigurationsordner der Box") : file;
		return std::string(
			"This box does not answer to this name.\n"
			"Open it by its IP address instead") + (at.empty() ? "." : ": " + at) + "\n"
			"To use this name, add it to the line allowed_hosts in " + in_en + ". The name works "
			"within 10 seconds, without a restart.\n"
			"\n"
			"Diese Box antwortet nicht auf diesen Namen.\n"
			"\xC3\x96" "ffne sie stattdessen \xC3\xBC" "ber ihre IP-Adresse" + (at.empty() ? "." : ": " + at) + "\n"
			"Um diesen Namen zu verwenden, trage ihn in " + in_de + " in die Zeile allowed_hosts ein. "
			"Der Name gilt nach sp\xC3\xA4" "testens 10 Sekunden, ohne Neustart.\n";
	}
	catch (...)
	{
		return std::string();
	}
}

const char *unknownHostDetail()
{
	return "this box does not answer to the name in the Host header: use its IP address, or add the "
	       "name to allowed_hosts in nhttpd-access.conf";
}

void setNameSourcesForTest(const std::string &host_name, const std::string &resolv_conf)
{
	OpenThreads::ScopedLock<OpenThreads::Mutex> held(lock());
	Sources &s = sources();
	s.overridden = !(host_name.empty() && resolv_conf.empty());
	s.host_name = host_name;
	s.resolv_conf = resolv_conf;
	s.read = false;
	s.access_path.clear();
	s.access_read = false;
}

void setNameClockForTest(time_t now)
{
	OpenThreads::ScopedLock<OpenThreads::Mutex> held(lock());
	sources().clock = now;
}

} // namespace httpd
