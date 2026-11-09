//=============================================================================
// YHTTPD
// mod_weblog : Logging of HTTPD-Requests/Responses
//=============================================================================
#ifndef __yhttpd_mod_weblog_h__
#define __yhttpd_mod_weblog_h__

#include <ctime>
#include <string>

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
	static std::string	Refusal;	// why the configured file is not written

	static bool		openLocked();
	static void		closeLocked();
	static bool		addRecord(const std::string &record);

public:
	CmWebLog();
	~CmWebLog();

	void 		AddLogEntry_CLF(CyhookHandler *hh);
	void 		AddLogEntry_ELF(CyhookHandler *hh);

	// The one directory a log is written in: the one the default log file
	// (LOG_FILE) lies in.
	static std::string	logDirectory();

	// Opens path to add records to, as a file directly in dir and nowhere
	// else, and answers its descriptor; -1, with the reason in why, for a
	// name that is not absolute or not directly in dir (the directory itself
	// compared, however it is spelt), for a file that holds who may reach the
	// box (yprotect), by its name or by what was opened, and for a name that
	// is a link, or that leads to something other than a regular file of this
	// user with no second name. Opened without following a link, and the
	// file opened is what is checked, not the name. passing, where given, says
	// whether the failure passes by itself (no descriptor, memory or space for
	// a moment, a call interrupted) rather than turning the destination down.
	static int		openIn(const std::string &path, const std::string &dir, std::string &why,
				       bool *passing = NULL);

	// Why no log is written to the file the configuration names, as the
	// server said it once; empty while nothing has been turned down.
	static std::string	refusal();

	// One record of the exchange hh, in the common and in the extended
	// format: a single line, whatever the request carried (ylog), with the
	// values of the query left out.
	static std::string	recordCLF(CyhookHandler *hh, time_t now);
	static std::string	recordELF(CyhookHandler *hh, time_t now);

	// virtual functions for HookHandler/Hook
	virtual std::string 	getHookName(void) {return std::string("mod_weblog");}
	virtual std::string 	getHookVersion(void) {return std::string("$Revision$");}
	virtual THandleStatus 	Hook_EndConnection(CyhookHandler *hh);
	virtual THandleStatus 	Hook_ReadConfig(CConfigFile *Config, CStringList &ConfigList);
};
#endif // __yhttpd_mod_weblog_h__

