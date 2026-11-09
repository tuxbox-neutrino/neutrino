//=============================================================================
// YHTTPD
// Logging & Debugging
//=============================================================================
#ifndef __yhttpd_logging_h__
#define __yhttpd_logging_h__

// system
#include <pthread.h>
// c++
#include <string>
// yhttpd
#include <yconfig.h>

// forward declaration
class CWebserverConnection;

//-----------------------------------------------------------------------------
// Log Level
//-----------------------------------------------------------------------------
//	1 : Connection Information (called URL)
//	2 : Parameters?? 
//	3 : Configuration and requests
//	6 : Request bodies
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
	do { CLogging::getInstance()->printf("[yhttpd(%s:%d)] " fmt, __file__, __LINE__, ## args); } while (0)

//Set Watch Point (show file and linenumber and function)
#define WP() \
	do { CLogging::getInstance()->printf("[yhttpd(%s:%d)%s]\n", __file__, __LINE__, __FUNCTION__); } while (0)

// print if level matches
#define log_level_printf(level, fmt, args...) \
	do { if(CLogging::getInstance()->LogLevel>=level) CLogging::getInstance()->printf("[yhttpd#%d] " fmt, level, ## args); } while (0)

// print if level matches / show file and linenumber
#define log_level_printfX(level, fmt, args...) \
	do { if(CLogging::getInstance()->LogLevel>=level) CLogging::getInstance()->printf("[yhttpd#%d(%s:%d)] " fmt, level, __file__, __LINE__, ## args); } while (0)

// print only if debug is on
#define dprintf(fmt, args...) \
	do { if(CLogging::getInstance()->getDebug())CLogging::getInstance()->printf("[yhttpd] " fmt, ## args); } while (0)

// print string to stdandard error
#define dperror(str) \
	do { perror("[yhttpd] " str); } while (0)

//-----------------------------------------------------------------------------
// What of a request may stand in a log, and how. Every log line that carries
// a part of a request (its line, URL, headers, parameters, a name built from
// them) takes it through these; test/unit/scan/check-log-records.sh holds the
// sources to that.
//-----------------------------------------------------------------------------
namespace ylog
{

// s with every control character (below 0x20, and 0x7f), the backslash and
// the double quote written as an escape: \n, \r, \t, \\, \" or \xNN. A record
// built from it is one line, whatever the request carried.
std::string text(const std::string &s);

// Whether the header name carries a credential: Authorization,
// Proxy-Authorization, Cookie, Set-Cookie and X-CSRF-Token, in any case.
bool secretHeader(const std::string &name);

// Whether the parameter name says its value is a secret, in any case: a name
// that holds password, passwd, pincode, token, ticket or secret, that ends in
// _key or _dev_id, as the settings that hold API keys do, or in _pass_<n>, as
// the password kept for another box does, and the whole names current (the
// password a change of the account is asked with), pass, pw, pwd and pin.
// The names of /control/config ("mod_auth_dot_password") are read as they
// are written.
bool secretParam(const std::string &name);

// The value of a header, or of a parameter, as it may stand in a log:
// [redacted] where the name says it is a secret; else an address (a value
// with ://, a Referer among them) as url() writes it, and anything else as
// text().
std::string headerValue(const std::string &name, const std::string &value);
std::string paramValue(const std::string &name, const std::string &value);

// What stands in a log for a part of a request that may be anything, a value
// among it: its length only ("12 bytes").
std::string withheld(const std::string &s);

// A request target as the request line carries it, still encoded: the path
// as text(), and of a query the names only, each value left out
// (/a?x=&y=). A piece without a = is a value of its own (the old routes
// read it by position) and is left out as well. Of an absolute address, a
// user and password in front of the host stand as [redacted].
std::string url(const std::string &target);

// A request line, method, target and protocol, with the target as url()
// writes it.
std::string startLine(const std::string &line);

} // namespace ylog

#endif /* __yttpd_logging_h__ */
