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
#include "ytypes_globals.h"
#include "ywebserver.h"

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
	static void version(FILE *dest);// Show Webserver Version
	static void usage(FILE *dest); 	// Show command line usage
	// Hooks
	void hooks_attach(); 			// Add a Hook-Class to HookList
	void hooks_detach(); 			// Remove a Hook-Class from HookList
	void ReadConfig(void); 			// Read the config file for the webserver
	void ReadLanguage(void); 		// Read Language Files
};

#endif // __yhttpd_h__
