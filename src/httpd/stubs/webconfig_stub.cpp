/*
 * webconfig_stub.cpp - the web configuration this tree has
 *
 * Copyright (C) 2026 Thilo Graf
 *
 * License: GPL-2.0-or-later
 *
 * The source's webconfig.cpp reads a configuration file of its own server:
 * port, credentials, proxies, and whether channel logos are handed out.
 * On nhttpd the server is configured by nhttpd.conf and the routes are
 * told the one value they read, channel_logos, which is on: nhttpd serves
 * the logos it finds and the route answers 404 where there is none. The
 * rest of the struct
 * keeps its declared shape, value-initialised, so a route that reads a
 * field this tree does not set gets a defined answer.
 */

#include "httpd/webconfig.h"

namespace httpd
{

WebConfig defaultWebConfig()
{
	WebConfig c = WebConfig();
	c.channel_logos = true;
	return c;
}

const WebConfig &config()
{
	static WebConfig current = defaultWebConfig();
	return current;
}

} // namespace httpd
