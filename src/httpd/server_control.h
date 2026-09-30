/*
 * server_control.h - the server seam's nhttpd half, declared without nhttpd
 *
 * Copyright (C) 2026 Thilo Graf
 *
 * License: GPL-2.0-or-later
 *
 * The three calls the webserver's restart seam is driven by
 * (stubs/server_nhttpd.cpp). A header of their own, light enough for
 * everyone who needs one of them: neutrino.cpp installs the two hands,
 * yhttpd.cpp says which port is bound, the connection code hands a sent
 * answer's reload path to the runner - none of which may drag the whole
 * bridge header in.
 */

#ifndef __httpd_server_control_h__
#define __httpd_server_control_h__

#include <string>

namespace httpd
{

/* The attach's half of server.h's boundPort(): nhttpd knows the port it
   bound, server.h has no way to ask it, so the webserver says it here when
   it comes up and says nought when it goes down. */
void setBoundPortFromBridge(int port);

/* What the webserver said last through the call above, 0 while nothing
   listens. Declared in server.h for the layer as well; repeated here so a
   start in nhttpd can wait for its server's own word. */
int boundPort();

/* The two hands the seam drives the webserver with, installed once by the
   program (neutrino.cpp; yhttpd.cpp defines them). Where nothing installed
   them - openapi_dump, the unit suite - start() answers false and a reload
   answer says the server did not come back. */
void installNhttpdServerControl(int (*start_thread)(int port, const char *bind),
                                bool (*stop_thread)(void));

/* Carries a sent answer's reload path off the connection thread: a
   webserver cannot be stopped from a thread it owns, so a runner of its
   own does it once the bytes are gone. */
void scheduleReloadAndRestart(const std::string &path);

} // namespace httpd

#endif
