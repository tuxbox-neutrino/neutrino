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
#include <yconfig.h>
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

	// API targets (CyhookHandler::isApiTarget): the raw body, bounded, or
	// the note that it was refused unread. Empty and false otherwise.
	std::string RawBody;
	bool RawBodyTooLarge;

	// What the request headers say about a body: none declared, one
	// declared length, or something this server will not read (a second
	// differing Content-Length, a value that is not a number, any
	// Transfer-Encoding).
	enum ApiBody { BODY_NONE, BODY_OK, BODY_BAD };
	static ApiBody apiContentLength(const std::string &raw_header, long long &length);

	// The whole decision about a body, per method, before a byte of it is
	// read: none declared (or a zero length), read this many, refused (a
	// body this server cannot frame, or one declared on a method that takes
	// none), or too large to keep.
	enum ApiBodyDecision { API_BODY_NONE, API_BODY_READ, API_BODY_BAD, API_BODY_TOO_LARGE };
	static ApiBodyDecision apiBodyDecision(THttp_Method method, const std::string &raw_header, long long &length);

	// The header of an API request, up to and including the empty line, off
	// the socket and nothing past it; false on EOF, past limit bytes, or
	// after timeout_ms.
	static bool readApiHeader(class CySocket &sock, std::string &raw_header, size_t limit, int timeout_ms);
	bool HandleApiRequest(void);

	// constructor & destructor
	CWebserverRequest() : RawBodyTooLarge(false) {};
	CWebserverRequest(CWebserver *pWebserver);
	// Handler
	bool HandleRequest(void);
};

#endif /* __yhttpd_request_h__ */
