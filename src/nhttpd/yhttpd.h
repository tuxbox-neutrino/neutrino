//=============================================================================
// YHTTPD
// Main Class
//-----------------------------------------------------------------------------
// Cyhttpd
// Main Function and Class for Handling the Webserver-Application
// - Handles Command Line Input
// - Reads and Handles "ReadConfig" (inclusive Hooking)
// - Creates Webserver and start them listening
//=============================================================================
#ifndef __yhttpd_h__
#define __yhttpd_h__
// system
#include <signal.h>
#include <stdlib.h>
// yhttpd
#include "yconfig.h"
//#include <ytypes_globals.h>
#include <ywebserver.h>

//-----------------------------------------------------------------------------
class Cyhttpd {
private:
	CWebserver *webserver; 			// Aggregation of Webserver (now: only one)

public:
	bool flag_threading_off; 		// switch of Connection Threading
	static CStringList ConfigList; 	// Vars & Values from ReadConfig

	// signal handler
	static volatile sig_atomic_t sig_do_shutdown;

	// constructor & destructor
	Cyhttpd();
	~Cyhttpd();

	// Main Programm calls
	void run(); 					// Init Hooks, ReadConfig, Start Webserver
	bool Configure();
	void stop_webserver(); 			// Remove Hooks, Stop Webserver
	// Ends the accept loop and nothing else: run() then returns and does
	// the one stop_webserver() of this thread's life. For the restart
	// hands below, which may not stop the hooks twice.
	void stop_run();
	static void version(FILE *dest);// Show Webserver Version
	// Hooks
	void hooks_attach(); 			// Add a Hook-Class to HookList
	void hooks_detach(); 			// Remove a Hook-Class from HookList
	void ReadConfig(void); 			// Read the config file for the webserver
	void ReadLanguage(void); 		// Read Language Files
};

/* The two hands the API layer's server seam drives the webserver with
   (httpd::installNhttpdServerControl, installed from neutrino.cpp). Here
   and not in the seam's own library, because openapi_dump and the unit
   suite link that library and must not link this server.

   start writes the port and the address into nhttpd.conf - the file this
   server reads and the one place they are recorded - starts the webserver
   thread again and answers the bound port once something listens, nought
   where nothing came up, a bounded wait. stop ends the accept loop and
   waits for the thread to have carried its own teardown, as long as a
   request still being served takes: the start after it must come. */
int nhttpd_server_start(int port, const char *bind);
bool nhttpd_server_stop(void);

#endif // __yhttpd_h__
