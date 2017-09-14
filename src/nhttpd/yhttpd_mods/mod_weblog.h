//=============================================================================
// YHTTPD
// mod_weblog : Logging of HTTPD-Requests/Responses
//=============================================================================
#ifndef __yhttpd_mod_weblog_h__
#define __yhttpd_mod_weblog_h__

#include <yconfig.h>
#include <yhook.h>
//-----------------------------------------------------------------------------
// Defaults
#ifndef LOG_FILE
#define LOG_FILE			"/tmp/yhttpd.log"
#define LOG_FORMAT			"off"
#endif

//-----------------------------------------------------------------------------
class CmWebLog : public Cyhook
{
	static pthread_mutex_t	WebLog_mutex;
	static FILE 		*WebLogFile;
	static std::string	WebLogFilename;
	static int 		RefCounter;	// Count Instances
	static std::string	LogFormat;

public:
	CmWebLog();
	~CmWebLog();

	bool 		OpenLogFile();
	void 		CloseLogFile();
	void 		AddLogEntry_CLF(CyhookHandler *hh);
	void 		AddLogEntry_ELF(CyhookHandler *hh);
	bool 		printf(const char *fmt, ...);

	// virtual functions for HookHandler/Hook
	virtual std::string 	getHookName(void) {return std::string("mod_weblog");}
	virtual std::string 	getHookVersion(void) {return std::string("$Revision$");}
	virtual THandleStatus 	Hook_EndConnection(CyhookHandler *hh);
	virtual THandleStatus 	Hook_ReadConfig(CConfigFile *Config, CStringList &ConfigList);
};
#endif // __yhttpd_mod_weblog_h__

