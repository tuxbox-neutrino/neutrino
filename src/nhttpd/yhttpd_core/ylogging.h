//=============================================================================
// YHTTPD
// Logging & Debugging
//=============================================================================
#ifndef __yhttpd_logging_h__
#define __yhttpd_logging_h__

// system
#include <pthread.h>
// yhttpd
#include "yconfig.h"

// forward declaration
class CWebserverConnection;

//-----------------------------------------------------------------------------
// Log Level
//-----------------------------------------------------------------------------
//	1 : Connection Information (called URL)
//	2 : Parameters?? 
//	3 : yParser Main Infos
//	5 : ycmd: Every yParser Loop Command expansion
//	6 : yresult: Every yParser Loop Command expansion
//  	8 : Central Functions Detailed
//  	9 : Socket Operations 
//-----------------------------------------------------------------------------
class CLogging
{
	protected:
		pthread_mutex_t Log_mutex;
		FILE *Logfile;
		bool Debug;
		bool LogToFile;
				
		static CLogging *instance;

		CLogging(void);
		~CLogging(void);

	public:
		int LogLevel;

		// Instance Handling
		static CLogging *getInstance(void);
		static void deleteInstance(void);

		void setDebug(bool _debug);
		bool getDebug(void);

		// Logging
		void printf(const char *fmt, ...);
};

//-----------------------------------------------------------------------------
// definitions for easy use
//-----------------------------------------------------------------------------

// print always
#define aprintf(fmt, args...) \
	do { CLogging::getInstance()->printf("[yhttpd] " fmt, ## args); } while (0)
	
// print show file and linenumber
#define log_printfX(fmt, args...) \
	do { CLogging::getInstance()->printf("[yhttpd(%s:%d)] " fmt, __FILE__, __LINE__, ## args); } while (0)

//Set Watch Point (show file and linenumber and function)
#define WP() \
	do { CLogging::getInstance()->printf("[yhttpd(%s:%d)%s]\n", __FILE__, __LINE__, __FUNCTION__); } while (0)

// print if level matches
#define log_level_printf(level, fmt, args...) \
	do { if(CLogging::getInstance()->LogLevel>=level) CLogging::getInstance()->printf("[yhttpd#%d] " fmt, level, ## args); } while (0)

// print if level matches / show file and linenumber
#define log_level_printfX(level, fmt, args...) \
	do { if(CLogging::getInstance()->LogLevel>=level) CLogging::getInstance()->printf("[yhttpd#%d(%s:%d)] " fmt, level, __FILE__, __LINE__, ## args); } while (0)

// print only if debug is on
#define dprintf(fmt, args...) \
	do { if(CLogging::getInstance()->getDebug())CLogging::getInstance()->printf("[yhttpd] " fmt, ## args); } while (0)

// print string to stdandard error
#define dperror(str) \
	do { perror("[yhttpd] " str); } while (0)

#endif /* __yttpd_logging_h__ */
