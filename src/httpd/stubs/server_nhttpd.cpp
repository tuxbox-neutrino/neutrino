/*
 * server_nhttpd.cpp - server.h answered for a tree whose server is nhttpd
 *
 * Copyright (C) 2026 Thilo Graf
 *
 * License: GPL-2.0-or-later
 *
 * server.h describes the source's own daemon. On this tree the server that
 * answers is nhttpd, and this file maps the header's promises onto it:
 * the bound port is what the attach said, and start/stop drive the
 * webserver thread through two hands the program installs (yhttpd.cpp,
 * installed from neutrino.cpp beside the first start). This library
 * cannot name those hands itself: its other consumers - openapi_dump and
 * the unit suite - must not link the server, and where nothing installed
 * them start() stays false, which is the header's word for a start that
 * did not happen, and reloadAndRestart() says the server did not come
 * back.
 *
 * defaultConfig() carries the source's shipped values as written (their
 * server.cpp): the migration reader falls back to these where the old
 * file names nothing, and a different number here would migrate a box
 * to a place nobody wrote down.
 */

#include "httpd/server.h"
#include "httpd/nhttpd_bridge.h"
#include "httpd/webconfig.h"

#include <pthread.h>

#include <cstdio>
#include <memory>
#include <string>

namespace httpd
{

namespace
{

// What the attach said, and nought before it said anything - the header's
// own reading for "nothing is bound".
int bound_port = 0;

// Every start that came up, read by request threads.
unsigned started = 0;

// The two hands, empty until the program installs them.
int  (*start_hand)(int port, const char *bind) = 0;
bool (*stop_hand)(void) = 0;
// nhttpd's own reading of its file, empty until the program installs it.
void (*config_reload_hand)(void) = 0;

/* The runner a sent reload answer is carried by. Its own thread, because
   the connection thread that sent the answer belongs to the webserver
   being stopped: stopping from there would be the server waiting for a
   thread that is waiting for the server. Detached like every connection
   thread; the path travels as an owned copy. */
/* One reload at a time, in the order they were asked for. Two answers
   whose reloads ran side by side would be two stops racing one start -
   and the second reader of the same final file is idempotent anyway, so
   waiting is all the ordering this needs. */
pthread_mutex_t reload_lock = PTHREAD_MUTEX_INITIALIZER;

struct ReloadTurn
{
	ReloadTurn() { pthread_mutex_lock(&reload_lock); }
	~ReloadTurn() { pthread_mutex_unlock(&reload_lock); }
};

/* A thread's entry, so nothing may leave it: an exception out of here ends
   the process. The copy of the path and the turn are released on every way
   out, the one an exception takes among them. */
void *reloadRunner(void *arg)
{
	try
	{
		std::unique_ptr<std::string> path((std::string *) arg);
		ReloadTurn turn;
		reloadAndRestart(*path);
	}
	catch (...)
	{
		std::fprintf(stderr, "httpd: the reload an answer asked for did not finish\n");
	}
	return 0;
}

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

/* The details of the transport's own two refusals, which the API document
   states for every route that can meet them. Here beside the rest of
   server.h, so that openapi_dump and the suite state them without the
   bridge, and the bridge sends the same words. The size is the bridge's
   body limit, the one defaultConfig() names. */
const char *tooLargeDetail()
{
	return "the body is larger than this server keeps for one request, 1 MiB";
}

const char *rangeRefusedDetail()
{
	return "the stretch that was asked for is not in this file";
}

const char *heldBackDetail()
{
	return "nothing is handed out under this name";
}

bool start(const ServerConfig &c)
{
	if (start_hand == 0)
		return false;

	const int bound = start_hand(c.port, c.bind_address.c_str());
	if (bound <= 0)
		return false;

	__atomic_store_n(&bound_port, bound, __ATOMIC_SEQ_CST);
	__atomic_add_fetch(&started, 1u, __ATOMIC_SEQ_CST);
	return true;
}

unsigned startCount()
{
	return __atomic_load_n(&started, __ATOMIC_SEQ_CST);
}

void stop()
{
	if (stop_hand != 0)
		stop_hand();
	__atomic_store_n(&bound_port, 0, __ATOMIC_SEQ_CST);
}

// Written by the webserver's thread and read by the reload runner and the
// request threads, so atomic.
bool running()
{
	return __atomic_load_n(&bound_port, __ATOMIC_SEQ_CST) > 0;
}

int boundPort()
{
	return __atomic_load_n(&bound_port, __ATOMIC_SEQ_CST);
}

void setBoundPortFromBridge(int port)
{
	__atomic_store_n(&bound_port, (port > 0) ? port : 0, __ATOMIC_SEQ_CST);
}

void installNhttpdServerControl(int (*start_thread)(int port, const char *bind),
                                bool (*stop_thread)(void))
{
	start_hand = start_thread;
	stop_hand = stop_thread;
}

void installNhttpdConfigReload(void (*reload)(void))
{
	config_reload_hand = reload;
}

void reloadNhttpdConfig()
{
	if (config_reload_hand)
		config_reload_hand();
}

void scheduleReloadAndRestart(const std::string &path)
{
	// called from an nhttpd connection thread, which is built without
	// exceptions: the copy's allocation is the one thing here that throws
	try
	{
		std::string *owned = new std::string(path);
		pthread_t t;
		pthread_attr_t attr;
		pthread_attr_init(&attr);
		pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
		if (pthread_create(&t, &attr, &reloadRunner, owned) != 0)
			delete owned;
		pthread_attr_destroy(&attr);
	}
	catch (...)
	{
		std::fprintf(stderr, "httpd: no reload scheduled for %s\n", path.c_str());
	}
}

} // namespace httpd
