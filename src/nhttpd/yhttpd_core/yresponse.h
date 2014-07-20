//=============================================================================
// YHTTPD
// Response
//=============================================================================

#ifndef __yhttpd_response_h__
#define __yhttpd_response_h__

// c++
#include <string>
// yhttpd
#include <yconfig.h>
#include "ytypes_globals.h"
#include "yhook.h"

// forward declaration
class CWebserver;
class CWebserverConnection;

//-----------------------------------------------------------------------------
class CWebserverResponse
{
private:

protected:
	bool WriteData(char const *data, long length);
	bool Sendfile(std::string filename, off_t start = 0, off_t end = -1);
	std::string	redirectURI;		// URI for redirection else: empty

public:
	class CWebserver *Webserver;
	class CWebserverConnection *Connection;
	
	// con/destructors
	CWebserverResponse();
	CWebserverResponse(CWebserver *pWebserver);

	// response control
	bool SendResponse(void);

	// output methods
	void printf(const char *fmt, ...);
	bool Write(char const *text);
	bool WriteLn(char const *text);
	bool Write(const std::string &text) { return Write(text.c_str()); }
	bool WriteLn(const std::string &text) { return WriteLn(text.c_str()); }

	// Headers
	void SendError(HttpResponseType responseType) {SendHeader(responseType, false, "text/html");}
	void SendHeader(HttpResponseType responseType, bool cache=false, std::string ContentType="text/html");

	// Helpers
	std::string GetContentType(std::string ext);
};

#endif /* __yhttpd_response_h__ */
