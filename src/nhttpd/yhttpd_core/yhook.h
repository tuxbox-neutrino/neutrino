//=============================================================================
// YHTTPD
// Hook and HookHandler
//-----------------------------------------------------------------------------
// Extentions for the Webserver can be implemented with Hooks.
// A "Hook"-Class must be inherited from Cyhook and can override the virtual
// functions from Cyhook.
// There are three types of Hooks.
// 1) Server based Hooks (exactly one Instance of Cyhttpd)
// 2) Webserver based Hooks (for each Instance of CWebserver, actually one; not now)
// 3) Connection based Hooks (for each Instance of CWebserverConnection)
//-----------------------------------------------------------------------------
// The CyhookHandler-Class itselfs calls the "attached" hooks.
// Hook-Classes must be attached/de-attached in Cyhttpd-Class attach()/detach().
// Hook-Class instances should be initialized in yhttpd.cpp.
//-----------------------------------------------------------------------------
// "Hook_SendResponse"
//-----------------------------------------------------------------------------
// "Hook_SendResponse": this is the most important Hook.
// The CyhookHandler implements an abstraction for Request and Response data.
// Every Hook which has implemented Hook_SendResponse could be called in order
// of the "attach" to HookHandler. The control flow, which Hook will be called
// depends on the return value "THandleStatus" from the hook called before.
// For example HANDLED_NONE indicates that the Hook has not handled by the call.
// For example HANDLED_READY indicates that the call is handled and HookHandler
// should finish the Hook-Loop. HANDLED_NOT_IMPLEMENTED indicated that the Hook
// should handle the call, but has not any function to do that.
// Several Hooks can work together (HANDLED_CONTINUE or HANDLED_REWRITE); so the
// next called Hook can work with the output (in yresult) from the before called
// Hook or controls the workflow of following Hooks.
// For example the CmAuth-Hook validates HTTP-Authentication and returns
// HANDLED_CONTINUE if the authentication fits.
// Each Hook_SendResponse is encapsulated from the webserver and should not work
// on Socket-Connections. The Response is written to yresult. There are many
// helper-function for Output and Status-Handling.
// The Input is encapsulated too: ParamList, HeaderList, UrlData, ...
// Look at Response.SendResponse()
//-----------------------------------------------------------------------------
// Other Hooks
//-----------------------------------------------------------------------------
// - Hook_ReadConfig: Every Hook can read OWN Variables from the yhttpd-Configfile
// - Hook_EndConnection: After Response is sent and before Connection Instance
//   is deleted
// - Hook_UploadSetFilename: this hook can set the filename for a file to upload
//   via POST (before upload)
// - Hook_UploadReady: this Hook is called after uploading a file
//=============================================================================
#ifndef __yhttpd_yhook_h__
#define __yhttpd_yhook_h__
#include <unistd.h>
// C++
#include <string>
#include <list>
// yhttpd
#include <yconfig.h>
#include "ytypes_globals.h"
#include "ylogging.h"
// tuxbox
#include <configfile.h>

//-----------------------------------------------------------------------------
// forward declarations
//-----------------------------------------------------------------------------
class CyhookHandler;
class Cyhook;

//-----------------------------------------------------------------------------
// Type definitions for Hooks
//-----------------------------------------------------------------------------
typedef enum
{
	HANDLED_NONE	= 0,	// Init
	HANDLED_READY,			// Handled
	HANDLED_ABORT,			// Abort Connection Fatal
	HANDLED_ERROR,			// Have Error like missing Parameter
	HANDLED_NOT_IMPLEMENTED,// URL should be handled but not implemented
	HANDLED_REDIRECTION,	// Set new URL and send HTTPD Object Moved
	HANDLED_SENDFILE,		// Set new URL and Send File
	HANDLED_REWRITE,		// Set new URL and call Hooks again
	HANDLED_CONTINUE,		// handled but go on

} THandleStatus;
typedef std::list<Cyhook *> THookList;

//-----------------------------------------------------------------------------
// Output Tyoe.
//-----------------------------------------------------------------------------
typedef enum
{
	plain	= 0,
	html,
	xml,
	json
} TOutType;

//-----------------------------------------------------------------------------
// An abstract Hook-Class. Custom Hook must be inherited.
//-----------------------------------------------------------------------------
class Cyhook
{
protected:
public:
	Cyhook(){};
	virtual ~Cyhook(){};
	// Informations & Control
	virtual std::string 	getHookVersion(void) {return std::string("0.0.0");}
	virtual std::string 	getHookName(void) {return std::string("Abstract Hook Class");}
	// CWebserverConnection based hooks
	virtual THandleStatus 	Hook_PrepareResponse(CyhookHandler *){return HANDLED_NONE;};
	virtual THandleStatus 	Hook_SendResponse(CyhookHandler *){return HANDLED_NONE;};
	virtual THandleStatus	Hook_EndConnection(CyhookHandler *){return HANDLED_NONE;}
	virtual THandleStatus	Hook_UploadSetFilename(CyhookHandler *, std::string &){return HANDLED_NONE;}
	virtual THandleStatus	Hook_UploadReady(CyhookHandler *, std::string){return HANDLED_NONE;}
	// Cyhttpd based hooks
	virtual THandleStatus 	Hook_ReadConfig(CConfigFile *, CStringList &){return HANDLED_NONE;};
};

//-----------------------------------------------------------------------------
// Hook Handling and Input & Output abstraction
//-----------------------------------------------------------------------------
class CyhookHandler
{
protected:
	static THookList HookList;
public:
	// Output
	std::string 	yresult;			// content for response output
	THandleStatus 	status; 			// status of Hook handling
	HttpResponseType httpStatus;		// http-status code for response
	std::string 	ResponseMimeType;	// mime-type for response
	std::string 	NewURL;				// new URL for Redirection
	off_t		ContentLength;			// Length of Response Body
	off_t		RangeStart;			// Start of range, used for sendfile only
	off_t		RangeEnd;			// End of range, used for sendfile only
	time_t 		LastModified;			// Last Modified Time of Item to send / -1 dynamic content
	std::string	Sendfile;				// Path & Name (local os style) of file to send
	bool		keep_alive;

	// Input
	CStringList 	ParamList;			// local copy of ParamList (Request)
	CStringList 	UrlData;			// local copy of UrlData (Request)
	CStringList 	HeaderList;			// local copy of HeaderList (Request)
	CStringList 	WebserverConfigList;// Reference (writable) to ConfigList
	CStringList 	HookVarList;		// Variables in Hook-Handling passing to other Hooks
	THttp_Method 	Method;				// HTTP Method (requested)
	// constructor & deconstructor
	CyhookHandler(){ContentLength = 0; RangeStart = 0; RangeEnd = -1; keep_alive = 0; _outIndent = 0;status = HANDLED_NONE;Method = M_UNKNOWN;httpStatus =  HTTP_NIL;outType = plain;};
	virtual ~CyhookHandler(){};

	// hook slot handler
	static void 	attach(Cyhook *yh)	// attach a Hook-Class to HookHandler
		{HookList.push_back(yh);};
	static void 	detach(Cyhook *yh)	// detach a Hook-Class to HookHandler
		{HookList.remove(yh);};

	// session handling
	void 		session_init(CStringList _ParamList, CStringList _UrlData, CStringList _HeaderList,
					CStringList& _ConfigList, THttp_Method _Method, bool _keep_alive);

	// Cyhttpd based hooks
	static THandleStatus	Hooks_ReadConfig(CConfigFile *Config, CStringList &ConfigList);
	// CWebserverConnection based hooks
	THandleStatus 		Hooks_PrepareResponse();// Hook for Response.SendResonse Dispatching
	THandleStatus 		Hooks_SendResponse();	// Hook for Response.SendResonse Dispatching
	THandleStatus 		Hooks_EndConnection();
	THandleStatus		Hooks_UploadSetFilename(std::string &Filename);
	THandleStatus		Hooks_UploadReady(const std::string& Filename);

	// status handling
	void SetHeader(HttpResponseType _httpStatus, std::string _ResponseMimeType)
		{httpStatus = _httpStatus; ResponseMimeType = _ResponseMimeType;}
	void SetHeader(HttpResponseType _httpStatus, std::string _ResponseMimeType, THandleStatus _status)
		{httpStatus = _httpStatus; ResponseMimeType = _ResponseMimeType; status = _status;}
	void SetError(HttpResponseType responseType)
		{SetHeader(responseType, "text/html");}
	void SetError(HttpResponseType responseType, THandleStatus _status)
		{SetError(responseType); status = _status;}
	// others
	off_t GetContentLength()
		{return (status==HANDLED_SENDFILE)?ContentLength : (off_t)yresult.length();}
	// output methods
	std::string BuildHeader(bool cache = false);
	void addResult(const std::string& result) 	{yresult += result;}
	void addResult(const std::string& result, THandleStatus _status)
						{yresult += result; status = _status;}
	void printf(const char *fmt, ...);
	void Write(const std::string& text)			{addResult(text);}
	void WriteLn(const std::string& text)		{addResult(text+"\r\n");}
	void Write(char const *text)				{Write(std::string(text));}
	void WriteLn(char const *text)				{WriteLn(std::string(text));}
	void SendHTMLHeader(const std::string& Titel);
	void SendHTMLFooter(void);
	void SendOk(void)							{(ParamList["response"]=="json") ? Write("{\"success\": \"true\"}") : Write("ok");}
	void SendError(void)						{(ParamList["response"]=="json") ? Write("{\"success\": \"false\"}") : Write("error");}
	void SendFile(const std::string& url)		{NewURL = url; status = HANDLED_SENDFILE;}
	void SendRedirect(const std::string& url)	{httpStatus=HTTP_MOVED_TEMPORARILY; NewURL = url; status = HANDLED_REDIRECTION;}
	void SendRewrite(const std::string& url)	{NewURL = url; status = HANDLED_REWRITE;}

	int _outIndent;
	TOutType outType;			// Outputtpe = plain (default)|xml|json
	TOutType outStart();
	TOutType checkOutput();
	std::string outIndent();
	std::string outPair(std::string _key, std::string _content, bool _next);
	std::string outArray(std::string _key, std::string _content);
	std::string outArrayItem(std::string _key, std::string _content, bool _next);
	std::string outCollection(std::string _key,std::string  _content);
	std::string outValue(std::string _content);
	std::string outNext();
	friend class CyParser;
};

#endif /*__yhttpd_yhook_h__*/
