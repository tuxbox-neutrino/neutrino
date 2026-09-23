/*
 * server_nhttpd.cpp - server.h answered for a tree whose server is nhttpd
 *
 * Copyright (C) 2026 Thilo Graf
 *
 * License: GPL-2.0-or-later
 *
 * server.h describes the source's own daemon. On this tree the server that
 * answers is nhttpd, so the header's promises are answered from nhttpd's
 * side of the bridge: the bound port is whatever the attach said it is,
 * and the start/stop pair is the seam the webserver table's
 * answer-then-restart will drive (slice 3, part s3). Until that part
 * lands nothing in the product calls either; webconfig.cpp links them
 * because its reloadAndRestart() does, and that caller arrives with the
 * same part.
 *
 * defaultConfig() carries the source's shipped values as written (their
 * server.cpp): the migration reader falls back to these where the old
 * file names nothing, and a different number here would migrate a box
 * to a place nobody wrote down.
 */

#include "httpd/server.h"

namespace httpd
{

namespace
{

// What the attach said, and nought before it said anything - the header's
// own reading for "nothing is bound".
int bound_port = 0;

} // anonymous namespace

ServerConfig defaultConfig()
{
	ServerConfig c;
	c.port = 80;
	c.bind_address = "0.0.0.0";
	c.thread_pool = 4;
	c.connection_limit = 64;
	c.connection_timeout_s = 30;
	c.max_body_bytes = 1u << 20;
	return c;
}

bool start(const ServerConfig &)
{
	// The restart seam is not wired yet (s3): nothing to put a server on,
	// and false is the header's word for a start that did not happen.
	return false;
}

void stop()
{
}

bool running()
{
	return false;
}

int boundPort()
{
	return bound_port;
}

// The attach's half of boundPort(), this tree's own: nhttpd knows the
// port it bound, server.h has no way to ask it, so the bridge says it
// here once the webserver stands.
void setBoundPortFromBridge(int port)
{
	bound_port = (port > 0) ? port : 0;
}

} // namespace httpd
