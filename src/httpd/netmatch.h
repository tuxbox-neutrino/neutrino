/*
 * netmatch.h - whether an address belongs to the local network
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

#ifndef __httpd_netmatch_h__
#define __httpd_netmatch_h__

#include <string>
#include <vector>

#include <sys/socket.h>

namespace httpd
{

/* One network, as bits rather than as the text it was written as. An aggregate with
   no constructor of its own, for the reason a route is one. A value nobody has filled
   in carries a family of zero, which is read as a network that matches no address at
   all, so the uninitialised case fails closed. */
struct NetPrefix
{
	int           family;    // AF_INET or AF_INET6, anything else matches nothing
	unsigned char bits[16];  // network order, the high len of them meaningful
	int           len;       // 0 to 32 for v4, 0 to 128 for v6
};

/* One line of configuration, "10.0.0.0/8" or "fc00::/7", read into the above.

   A length is required. A bare address without one is refused rather than taken as a
   single host, because the two readings differ by a factor of millions and the wrong
   one is wrong in the direction of letting more in.

   The bits past the length are cleared rather than the line refused, so "10.1.2.3/8"
   and "10.0.0.0/8" are the same network.

   out is cleared before anything is read, so a caller that keeps the answer without
   reading the return value holds a prefix that matches nothing. */
bool parsePrefix(const std::string &text, NetPrefix *out);

/* Whether a textual address is inside a network. The comparison is over whole bytes
   and one masked remainder byte, and never over the text: a text comparison makes one
   address match another that merely contains it and makes an empty entry match
   everything.

   A v6 network never answers for a v4 address and a v4 network never for a v6 one, the
   mapped form above excepted. */
bool addressInPrefix(const std::string &addr, const NetPrefix &p);

// Any of them, and false for an empty list: a list nobody filled in permits
// nothing rather than everything.
bool addressInAnyPrefix(const std::string &addr, const std::vector<NetPrefix> &list);

/* What a box is seeded with: 127.0.0.0/8, ::1/128, 10.0.0.0/8, 172.16.0.0/12,
   192.168.0.0/16 and fc00::/7, in that order. Loopback is two entries because it is two
   addresses, and a host that has both resolves the name of itself to the v6 one first.

   Empty rather than short if any of them fails to read, because a list missing one of
   its entries refuses part of the local network while looking configured on purpose. */
std::vector<NetPrefix> defaultLanPrefixes();

/* The same list as the text a file would carry, in the same order.

   A configuration this program writes names the default of every key it leaves
   out, so that whoever opens the file knows what the box is running on without
   reading the source. That text has to come off the one table the list itself
   is built from, or the file would document a list the box does not use. */
std::vector<std::string> defaultLanPrefixTexts();

} // namespace httpd

#endif
