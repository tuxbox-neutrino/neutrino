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
	/* The one other value a route reads: how many event streams this server
	   carries at once. The source's shipped default; value-initialised it would
	   be nought, which is a ceiling of none and a stream route that answers
	   nothing but 503. Eight of nhttpd's fifty connection threads may sit in
	   a stream for as long as a reader stays - a bound, not a reservation. */
	c.sse_max_streams = 8;
	return c;
}

namespace
{
WebConfig &held()
{
	static WebConfig current = defaultWebConfig();
	return current;
}
} // namespace

const WebConfig &config()
{
	return held();
}

// The seam the suite uses, same shape as the source's webconfig.cpp.
void setConfigForTest(const WebConfig &c)
{
	held() = c;
}

} // namespace httpd
