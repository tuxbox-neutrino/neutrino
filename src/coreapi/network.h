/*
 * network.h - the box's network interfaces, and what it does on its network
 * for somebody else
 *
 * Copyright (C) 2026 Thilo Graf
 *
 * License: GPL-2.0-or-later
 */

#ifndef __coreapi_network_h__
#define __coreapi_network_h__

#include "coreapi/base/result.h"

#include <stddef.h>

#include <string>
#include <vector>

namespace coreapi
{
namespace network
{

/* Wakes a device on the box's network: six bytes of 0xff and the device's
   hardware address sixteen times over, sent to port 9 at the broadcast address
   of every network the box is on, where a network card set up for it is
   listening while its machine sleeps. Whether the device wakes is nothing the
   box can see.

   The address is six pairs of hexadecimal digits, separated by colons or dashes
   or not at all. InvalidArgument BadString for anything else, Internal
   WakeNotSent, saying why, when the packet left on no network. */
Result<void> wake(const std::string &mac);

/* A network interface of the box: its name, its IPv4 address where it has one,
   its hardware address, and whether it is up. The loopback is left out. */
struct Interface
{
	std::string name;
	std::string ipv4;
	std::string mac;
	bool        up;
	Interface() : up(false) {}
};

// Every interface, by name; none for a box whose interfaces could not be read.
std::vector<Interface> interfaces();

namespace internal
{
/* Stands in for the socket, so a case can see the packet that would have gone
   out. NULL puts the real one back. */
typedef bool (*WakeSender)(const unsigned char *packet, size_t length);
void setWakeSenderForTest(WakeSender s);
} // namespace internal

} // namespace network
} // namespace coreapi

#endif
