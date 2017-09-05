//=============================================================================
// YHTTPD
// yParser
//=============================================================================

#ifndef __yhttpd_yparser_h__
#define __yhttpd_yparser_h__

// c++
#include <map>
#include <string>
// system
#include <fcntl.h>
#include <sys/stat.h>
#include <pthread.h>

#include <configfile.h>
// yhttpd
#include <helper.h>
#include <yconfig.h>
#include <ytypes_globals.h>
#include <yhook.h>

// forward declaration
class CWebserverConnection;

//-------------------------------------------------------------------------
// yParser definitions
//-------------------------------------------------------------------------
#define YPARSER_ESCAPE_START 	"{="
#define YPARSER_ESCAPE_END 	"=}"
#define YPARSER_VERSION 	"1.0.3"

//-------------------------------------------------------------------------
//class CyParser (it is a Hook-Class)
//-------------------------------------------------------------------------
class CyParser : public Cyhook
{
private:
	//-------------------------------------------------------------------------
	// Search folders for html files
	//-------------------------------------------------------------------------
	static const unsigned int HTML_DIR_COUNT = 2;
	static std::string HTML_DIRS[HTML_DIR_COUNT];
	static const unsigned int PLUGIN_DIR_COUNT = 4;
	static std::string PLUGIN_DIRS[PLUGIN_DIR_COUNT];

	// CGI call Dispatcher Array
	typedef void (CyParser::*TyCGIFunc)(CyhookHandler *hh);
	typedef struct
	{
		const char *func_name;
		TyCGIFunc pfunc;
		const char *mime_type;
	} TyCgiCall;
	const static TyCgiCall yCgiCallList[];

	// func dispatcher Array
	typedef std::string (CyParser::*TyFunc)(CyhookHandler *hh, std::string para);
	typedef struct
	{
		const char *func_name;
		TyFunc pfunc;
	} TyFuncCall;
	const static TyFuncCall yFuncCallList[];

	// local Session vars
	CStringList ycgi_vars;	//ycgi session vars
	CConfigFile *yConfig;

	// mutex for vars and caching (yCached_*)
	static pthread_mutex_t yParser_mutex;
	// ycgi globals
	static std::map<std::string, std::string> ycgi_global_vars;
	// caching globals
	static struct stat yCached_blocks_attrib;
	static std::string yCached_blocks_filename;
	static std::string yCached_blocks_content;

	// parsing engine
	std::string cgi_file_parsing(CyhookHandler *hh, std::string htmlfilename, bool ydebug);
	std::string cgi_cmd_parsing(CyhookHandler *hh, std::string html_template, bool ydebug);
	std::string YWeb_cgi_cmd(CyhookHandler *hh, std::string ycmd);

	// func
	std::string func_get_request_data(CyhookHandler *hh, std::string para);
	std::string func_get_header_data(CyhookHandler *hh, std::string para);
	std::string func_get_config_data(CyhookHandler *hh, std::string para);
	std::string func_do_reload_httpd_config(CyhookHandler *hh, std::string para);
	std::string func_change_httpd(CyhookHandler *hh, std::string para);
	std::string func_get_languages_as_dropdown(CyhookHandler *hh, std::string para);
	std::string func_set_language(CyhookHandler *, std::string para);

	// helpers
	std::string YWeb_cgi_get_ini(CyhookHandler *hh, std::string filename, std::string varname, std::string yaccess);
	void YWeb_cgi_set_ini(CyhookHandler *hh, std::string filename, std::string varname, std::string varvalue, std::string yaccess);
	std::string YWeb_cgi_include_block(std::string filename, std::string blockname, std::string ydefault);

	// CGIs
	void cgi(CyhookHandler *hh);
#ifdef Y_CONFIG_FEATURE_SHOW_SERVER_CONFIG
	void cgi_server_config(CyhookHandler *hh);
#endif

protected:
	void init(CyhookHandler *hh);
	void Execute(CyhookHandler *hh);
	void ParseAndSendFile(CyhookHandler *hh);
public:
	// constructor & deconstructor
	CyParser();
	~CyParser(void);

	// virtual functions for BaseClass
	virtual std::string 	YWeb_cgi_func(CyhookHandler *hh, std::string ycmd);

	// virtual functions for HookHandler/Hook
	virtual std::string 	getHookName(void) {return "mod_yparser";}
	virtual std::string 	getHookVersion(void) {return std::string("$Revision$");}
	virtual THandleStatus 	Hook_SendResponse(CyhookHandler *hh);
};

#endif /* __yhttpd_yparser_h__ */
