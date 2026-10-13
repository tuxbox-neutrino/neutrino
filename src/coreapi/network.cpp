/*
 * network.cpp - what the box does on its network for somebody else
 *
 * Copyright (C) 2026 Thilo Graf
 *
 * License: GPL-2.0-or-later
 */

#include "coreapi/network.h"

#include "coreapi/base/errors.h"

#include <cerrno>
#include <cstring>
#include <fstream>
#include <map>
#include <vector>

#include <arpa/inet.h>
#include <ifaddrs.h>
#include <net/if.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

namespace coreapi
{
namespace network
{

namespace
{

internal::WakeSender wake_sender = NULL;

int hexDigit(char c)
{
	if (c >= '0' && c <= '9')
		return c - '0';
	if (c >= 'a' && c <= 'f')
		return c - 'a' + 10;
	if (c >= 'A' && c <= 'F')
		return c - 'A' + 10;
	return -1;
}

// Six pairs of hexadecimal digits, with one kind of separator between all of them or none.
bool parseMac(const std::string &text, unsigned char out[6])
{
	char sep = 0;
	if (text.size() == 17)
	{
		sep = text[2];
		if (sep != ':' && sep != '-')
			return false;
	}
	else if (text.size() != 12)
		return false;
	const size_t step = sep ? 3 : 2;
	for (size_t i = 0; i < 6; ++i)
	{
		const size_t at = i * step;
		if (sep && i > 0 && text[at - 1] != sep)
			return false;
		const int hi = hexDigit(text[at]);
		const int lo = hexDigit(text[at + 1]);
		if (hi < 0 || lo < 0)
			return false;
		out[i] = (unsigned char) (hi * 16 + lo);
	}
	return true;
}

/* To the broadcast address of every network the box is on, one by one. The one
   broadcast address for all of them would go where the default route goes: on a
   box with no gateway it goes nowhere, and on one whose default route is a
   tunnel or another card it leaves the network the device sleeps on. A network
   that is not up, has no broadcast (a tunnel) or is the loopback is passed over.
   Sent when it left on at least one; otherwise why names what stood in the way. */
bool sendBroadcast(const unsigned char *packet, size_t length, std::string &why)
{
	struct ifaddrs *all = NULL;
	if (getifaddrs(&all) != 0)
	{
		why = std::string("the box's networks could not be read: ") + strerror(errno);
		return false;
	}
	const int fd = socket(AF_INET, SOCK_DGRAM | SOCK_CLOEXEC, 0);
	const int on = 1;
	if (fd < 0 || setsockopt(fd, SOL_SOCKET, SO_BROADCAST, &on, sizeof(on)) != 0)
	{
		why = std::string("no socket to send it on: ") + strerror(errno);
		if (fd >= 0)
			close(fd);
		freeifaddrs(all);
		return false;
	}
	bool sent = false;
	why = "the box is on no network with a broadcast address";
	std::vector<in_addr_t> done;
	for (struct ifaddrs *i = all; i != NULL; i = i->ifa_next)
	{
		if (i->ifa_addr == NULL || i->ifa_addr->sa_family != AF_INET || i->ifa_broadaddr == NULL)
			continue;
		if (!(i->ifa_flags & IFF_UP) || !(i->ifa_flags & IFF_BROADCAST) || (i->ifa_flags & IFF_LOOPBACK))
			continue;
		struct sockaddr_in to;
		memcpy(&to, i->ifa_broadaddr, sizeof(to));
		bool again = false;
		for (size_t d = 0; d < done.size(); ++d)
			again = again || done[d] == to.sin_addr.s_addr;
		if (again)
			continue;
		done.push_back(to.sin_addr.s_addr);
		to.sin_family = AF_INET;
		to.sin_port = htons(9);
		if (sendto(fd, packet, length, 0, (const struct sockaddr *) &to, sizeof(to)) == (ssize_t) length)
			sent = true;
		else
			why = std::string(i->ifa_name) + ": " + strerror(errno);
	}
	close(fd);
	freeifaddrs(all);
	return sent;
}

} // namespace

Result<void> wake(const std::string &mac)
{
	unsigned char hw[6];
	if (!parseMac(mac, hw))
		return fail(Status::InvalidArgument, ErrorCode::BadString,
			    "a hardware address is six pairs of hexadecimal digits, separated by colons, "
			    "dashes or nothing");

	unsigned char packet[6 + 16 * 6];
	memset(packet, 0xff, 6);
	for (size_t i = 0; i < 16; ++i)
		memcpy(packet + 6 + i * 6, hw, 6);

	std::string why = "the stand-in for the network turned it down";
	const bool sent = wake_sender != NULL ? wake_sender(packet, sizeof(packet))
	                                      : sendBroadcast(packet, sizeof(packet), why);
	if (!sent)
		return fail(Status::Internal, ErrorCode::WakeNotSent,
			    "the wake-up packet could not be sent: " + why);
	return ok();
}

namespace internal
{

void setWakeSenderForTest(WakeSender s)
{
	wake_sender = s;
}

} // namespace internal

std::vector<Interface> interfaces()
{
	std::map<std::string, Interface> by_name;
	struct ifaddrs *all = NULL;
	if (getifaddrs(&all) != 0)
		return std::vector<Interface>();
	for (struct ifaddrs *a = all; a != NULL; a = a->ifa_next)
	{
		if (a->ifa_name == NULL || (a->ifa_flags & IFF_LOOPBACK) != 0)
			continue;
		Interface &i = by_name[a->ifa_name];
		i.name = a->ifa_name;
		i.up = i.up || (a->ifa_flags & IFF_UP) != 0;
		if (a->ifa_addr != NULL && a->ifa_addr->sa_family == AF_INET && i.ipv4.empty())
		{
			char text[INET_ADDRSTRLEN];
			const struct sockaddr_in *in = reinterpret_cast<const struct sockaddr_in *>(a->ifa_addr);
			if (inet_ntop(AF_INET, &in->sin_addr, text, sizeof(text)) != NULL)
				i.ipv4 = text;
		}
	}
	freeifaddrs(all);
	std::vector<Interface> out;
	for (std::map<std::string, Interface>::iterator it = by_name.begin(); it != by_name.end(); ++it)
	{
		// the hardware address as the kernel writes it
		std::ifstream in(("/sys/class/net/" + it->first + "/address").c_str());
		std::getline(in, it->second.mac);
		out.push_back(it->second);
	}
	return out;
}

} // namespace network
} // namespace coreapi
