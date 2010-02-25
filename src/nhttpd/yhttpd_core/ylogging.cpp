//=============================================================================
// YHTTPD
// Logging & Debugging
//=============================================================================

// c
#include <cstdarg>
#include <cstdio>
#include <cstdlib>

// yhttpd
#include "yconfig.h"
#include "ytypes_globals.h"
#include "ylogging.h"
#include "yconnection.h"
//=============================================================================
// Instance Handling - like Singelton Pattern
//=============================================================================
//-----------------------------------------------------------------------------
// Init as Singelton
//-----------------------------------------------------------------------------
CLogging *CLogging::instance = NULL;

//-----------------------------------------------------------------------------
// There is only one Instance
//-----------------------------------------------------------------------------
CLogging *CLogging::getInstance(void) {
	if (!instance)
		instance = new CLogging();
	return instance;
}

//-----------------------------------------------------------------------------
void CLogging::deleteInstance(void) {
	if (instance)
		delete instance;
	instance = NULL;
}

//-----------------------------------------------------------------------------
// Constructor
//-----------------------------------------------------------------------------
CLogging::CLogging(void) {
	Debug = false;
	LogToFile = false; //not implemented
	LogLevel = 0;
	Logfile = NULL;
	pthread_mutex_init(&Log_mutex, NULL);
}

//-----------------------------------------------------------------------------
CLogging::~CLogging(void) {
}
//=============================================================================

//-----------------------------------------------------------------------------
void CLogging::setDebug(bool _debug) {
	Debug = _debug;
}
//-----------------------------------------------------------------------------
bool CLogging::getDebug(void) {
	return Debug;
}
//=============================================================================
// Logging Calls
// use mutex controlled calls to output resources
// Normal Logging to Stdout, if "Log" is true then Log to file
//=============================================================================
#define bufferlen 1024*8
//-----------------------------------------------------------------------------
void CLogging::printf(const char *fmt, ...) {
	char buffer[bufferlen];

	va_list arglist;
	va_start(arglist, fmt);
	vsnprintf(buffer, bufferlen, fmt, arglist);
	va_end(arglist);

	pthread_mutex_lock(&Log_mutex);
	::printf(buffer);
	if (LogToFile) {
		; //FIXME Logging to File
	}
	pthread_mutex_unlock(&Log_mutex);
}

