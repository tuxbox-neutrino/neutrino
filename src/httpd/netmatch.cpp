/*
 * netmatch.cpp - whether an address belongs to the local network
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

#include "netmatch.h"

#include <cstring>

#include <arpa/inet.h>
#include <netinet/in.h>

namespace httpd
{

namespace
{

const size_t kV4Bytes = 4;
const size_t kV6Bytes = 16;

/* The leading bytes of the range a v4 address is carried in when a v6 socket
   reports it, ::ffff:0:0/96. */
const unsigned char kMappedV4[12] =
{
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0xFF, 0xFF
};

/* The loopback entry is two entries, because loopback is two addresses. A dual stack
   host resolves the name of itself to ::1 before 127.0.0.1, so a list that seeds only
   the v4 half refuses the exemption to a browser or a script running on the box,
   which is the one caller the exemption exists for. */
const char *const kLanSeed[] =
{
	"127.0.0.0/8",
	"::1/128",
	"10.0.0.0/8",
	"172.16.0.0/12",
	"192.168.0.0/16",
	"fc00::/7"
};

size_t widthBytes(int family)
{
	return (family == AF_INET) ? kV4Bytes : kV6Bytes;
}

int widthBits(int family)
{
	return (family == AF_INET) ? 32 : 128;
}

/* Text to bits, through the library's own parser in both families. That parser is the
   whole point: it takes the four numbers of a v4 address and nothing that merely looks
   like them, so an entry that is not an address is refused here rather than compared
   as a string somewhere below.

   A dual stack listener reports a v4 peer as ::ffff:a.b.c.d, and reading that as a v6
   address would put every client on the local network outside every v4 network the box
   is configured with. So the mapped range is folded to the v4 address it names. Only
   that range: the deprecated ::a.b.c.d form is not folded, because ::1 is inside it
   and is the v6 loopback rather than 0.0.0.1. */
bool readAddress(const std::string &text, int &family, unsigned char *bits)
{
	/* A string is allowed to carry a zero byte and the parser below stops at
	   one, so "10.0.0.1\0anything" would otherwise read as 10.0.0.1 and be
	   judged on the half of itself that suits it. */
	if (text.empty() || text.find('\0') != std::string::npos)
		return false;

	struct in_addr v4;
	if (inet_pton(AF_INET, text.c_str(), &v4) == 1)
	{
		family = AF_INET;
		std::memcpy(bits, &v4.s_addr, kV4Bytes);
		std::memset(bits + kV4Bytes, 0, kV6Bytes - kV4Bytes);
		return true;
	}

	struct in6_addr v6;
	if (inet_pton(AF_INET6, text.c_str(), &v6) != 1)
		return false;

	if (std::memcmp(v6.s6_addr, kMappedV4, sizeof(kMappedV4)) == 0)
	{
		family = AF_INET;
		std::memcpy(bits, v6.s6_addr + sizeof(kMappedV4), kV4Bytes);
		std::memset(bits + kV4Bytes, 0, kV6Bytes - kV4Bytes);
		return true;
	}

	family = AF_INET6;
	std::memcpy(bits, v6.s6_addr, kV6Bytes);
	return true;
}

void clearBelow(unsigned char *bits, int len, size_t width)
{
	const size_t whole = static_cast<size_t>(len) / 8;
	const int rest = len % 8;

	if (rest != 0)
		bits[whole] = static_cast<unsigned char>(bits[whole] & static_cast<unsigned char>(0xFFu << (8 - rest)));

	for (size_t i = whole + ((rest != 0) ? 1u : 0u); i < width; ++i)
		bits[i] = 0;
}

} // namespace

bool parsePrefix(const std::string &text, NetPrefix *out)
{
	if (out == 0)
		return false;

	out->family = 0;
	std::memset(out->bits, 0, sizeof(out->bits));
	out->len = 0;

	const size_t slash = text.find('/');
	if (slash == std::string::npos)
		return false;

	const std::string addr = text.substr(0, slash);
	const std::string tail = text.substr(slash + 1);

	/* Three digits reach every length either family has, and a longer field is refused
	   before it is read rather than accumulated into a wrap. Inside those three, a
	   leading zero is read as part of a decimal number and never as an octal one, so
	   "/010" is ten and not eight: decimal is the reading that narrows the network
	   rather than widening it. The cost is that "/0008" is refused for its length
	   while "/008" is read, which is inconsistent to look at and safe either way. */
	if (addr.empty() || tail.empty() || tail.size() > 3)
		return false;

	int len = 0;
	for (size_t i = 0; i < tail.size(); ++i)
	{
		const char c = tail[i];
		if (c < '0' || c > '9')
			return false;
		len = (len * 10) + (c - '0');
	}

	int family = 0;
	unsigned char bits[kV6Bytes];
	if (!readAddress(addr, family, bits))
		return false;

	if (len > widthBits(family))
		return false;

	clearBelow(bits, len, widthBytes(family));

	out->family = family;
	std::memcpy(out->bits, bits, sizeof(out->bits));
	out->len = len;
	return true;
}

bool addressInPrefix(const std::string &addr, const NetPrefix &p)
{
	/* A prefix of no family matches nothing. That is what a failed parse leaves behind
	   and what a member nobody filled in holds, and either of those matching every
	   address is the one mistake this file exists to make impossible.

	   The first of two places that refuse it and not the one doing the work: the parse
	   only ever yields one of the two families. Both are kept because removing either
	   one leaves the property standing, which is the point of writing it twice. */
	if (p.family != AF_INET && p.family != AF_INET6)
		return false;
	if (p.len < 0 || p.len > widthBits(p.family))
		return false;

	int family = 0;
	unsigned char bits[kV6Bytes];
	if (!readAddress(addr, family, bits))
		return false;
	if (family != p.family)
		return false;

	const size_t whole = static_cast<size_t>(p.len) / 8;
	const int rest = p.len % 8;

	if (whole != 0 && std::memcmp(bits, p.bits, whole) != 0)
		return false;

	if (rest != 0)
	{
		const unsigned char mask = static_cast<unsigned char>(0xFFu << (8 - rest));
		if (((bits[whole] ^ p.bits[whole]) & mask) != 0)
			return false;
	}

	return true;
}

bool addressInAnyPrefix(const std::string &addr, const std::vector<NetPrefix> &list)
{
	for (size_t i = 0; i < list.size(); ++i)
	{
		if (addressInPrefix(addr, list[i]))
			return true;
	}
	return false;
}

std::vector<NetPrefix> defaultLanPrefixes()
{
	const size_t count = sizeof(kLanSeed) / sizeof(kLanSeed[0]);

	std::vector<NetPrefix> out;
	out.reserve(count);

	for (size_t i = 0; i < count; ++i)
	{
		NetPrefix p;
		if (!parsePrefix(kLanSeed[i], &p))
		{
			out.clear();
			return out;
		}
		out.push_back(p);
	}
	return out;
}

std::vector<std::string> defaultLanPrefixTexts()
{
	const size_t count = sizeof(kLanSeed) / sizeof(kLanSeed[0]);

	std::vector<std::string> out;
	out.reserve(count);

	for (size_t i = 0; i < count; ++i)
		out.push_back(kLanSeed[i]);

	return out;
}

} // namespace httpd
