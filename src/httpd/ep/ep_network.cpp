/*
 * ep_network.cpp - what the box does on its network for somebody else
 *
 * Copyright (C) 2026 Thilo Graf
 *
 * License: GPL-2.0-or-later
 */

#include "httpd/endpoints.h"

#include "httpd/endpoint.h"
#include "httpd/http.h"
#include "httpd/status.h"

#include "coreapi/base/errors.h"
#include "coreapi/base/result.h"
#include "coreapi/network.h"

#include <string>

namespace httpd
{

namespace
{

Response wakeDevice(const Request &r)
{
	coreapi::Result<void> done = coreapi::network::wake(r.asString("mac"));
	if (!done.ok())
		return problemFor(done.error());
	return noContent();
}

const Param kWakeParams[] = {
	HTTPD_BODY_REQUIRED_TEXT("mac",
		"the hardware address of the device to wake, six pairs of hexadecimal digits separated by colons, "
		"dashes or nothing", 17),
};

const RouteRefusal kWakeRefusals[] = {
	HTTPD_REFUSES(InvalidArgument, BadString,
		"a hardware address is six pairs of hexadecimal digits, separated by colons, dashes or nothing"),
	HTTPD_REFUSES(Internal, WakeNotSent,
		"the wake-up packet could not be sent: the box is on no network with a broadcast address"),
};

const Endpoint kNetworkEndpoints[] = {
	{ Method::Post, "/api/v1/network/wake", AuthLevel::Write,
	  "wakes a device on the box's network",
	  "Sends the wake-up packet for one device on the box's network: six bytes of `0xff` and the "
	  "device's hardware address sixteen times over, to UDP port 9 at the broadcast address of every "
	  "network the box is on (a tunnel, the loopback and a network that is down are passed over), "
	  "where a network card set up for it listens while its machine sleeps. `204` says the packet "
	  "left the box on at least one network and no more: whether the device wakes is nothing the box "
	  "can see.\n\n"
	  "**Refusals:**\n"
	  "- `400 bad-string`: `mac` is not six pairs of hexadecimal digits, separated by colons, dashes "
	  "or nothing.\n"
	  "- `500 wake-not-sent`: the packet left on no network; the detail says why.\n\n"
	  "**Related:** `GET /api/v1/webui/settings`, where the web interface keeps the devices it "
	  "offers to wake.",
	  HTTPD_PARAMS(kWakeParams), NULL, &wakeDevice, false,
	  Answers204, HTTPD_REFUSALS_AND_BODY(kWakeRefusals, "{\"mac\":\"00:11:22:33:44:55\"}") },
};

} // namespace

extern const RouteTable networkTable = {
	HTTPD_TABLE("network", kNetworkEndpoints)
};

} // namespace httpd
