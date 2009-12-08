//=============================================================================
// YHTTPD
// Request
//=============================================================================

#ifndef __yhttpd_request_h__
#define __yhttpd_request_h__

// C++
#include <string>
#include <map>

// yhttpd
#include "yconfig.h"
#include "ytypes_globals.h"

// forward declaration
class CWebserver;
class CWebserverConnection;

//-----------------------------------------------------------------------------
class CWebserverRequest
{
private:
	bool HandlePost();
	unsigned int HandlePostBoundary(std::string boundary, unsigned int content_len);
	bool ParseStartLine(std::string start_line);
	bool ParseParams(std::string param_string);
	bool ParseHeader(std::string header);

protected:
	std::string rawbuffer;

	// request control
public:
	class CWebserver *Webserver;
	class CWebserverConnection *Connection;

	// Request Data
	CStringList ParameterList;
	CStringList HeaderList;
	CStringList UrlData;	// url, fullurl, path, filename, fileext, paramstring 
	void analyzeURL(std::string);

	// constructor & destructor
	CWebserverRequest() {};
	CWebserverRequest(CWebserver *pWebserver);
	// Handler
	bool HandleRequest(void);
};

#endif /* __yhttpd_request_h__ */
