//=============================================================================
// YHTTPD
// Connection
//-----------------------------------------------------------------------------
// The Connection Class handles a given Socket-Connection for an arriving
// Request. Normally many Connections exists and they are Threaded!
// "HandleConnection()" calls "HandleRequest()" and "SendResponse()"
//=============================================================================
#ifndef __yhttpd_connection_h__
#define __yhttpd_connection_h__

// system
#include <sys/time.h>
// c++
#include <string>
// yhttpd
#include "yconfig.h"
#include "ytypes_globals.h"
#include "ywebserver.h"
#include "yrequest.h"
#include "yresponse.h"

// Forward definition
class CWebserver;

//-----------------------------------------------------------------------------
class CWebserverConnection
{
public:
	class CWebserver 	*Webserver;		// Backreference
	CWebserverRequest	Request;		// http Request
	CWebserverResponse	Response;		// http Response
	CyhookHandler 		HookHandler;		// One Instance per Connection

	CySocket		*sock;			// Connection Socket
	bool 			RequestCanceled;	// Canceled flag
	static long		GConnectionNumber;	// Number of Connection. Global counter
	long			ConnectionNumber;	// Number of Connection since Webserver start. Local number
	bool			keep_alive;		// should the response keep-alive socket?

	THttp_Method		Method;			// http Method (GET, POST,...)
	int 			HttpStatus;		// http response code
	std::string		httprotocol;		// HTTP 1.x

	long			enlapsed_request,	// time for request in usec
				enlapsed_response;	// time for response in usec

	CWebserverConnection(CWebserver *pWebserver);	// Constructor with given Backreferece
	CWebserverConnection();
	~CWebserverConnection(void);

	void HandleConnection();			// Main: Handle the Connecten Live time

	// performance/time measurement
	void ShowEnlapsedRequest(char *text);		// Print text and time difference to Connection creation
	long GetEnlapsedRequestTime();			// Get time in micro-seconds from Connection creation
	long GetEnlapsedResponseTime();			// Get time in micro-seconds after Request handles. Start Respnse.

private:
	void EndConnection();				// End Connection Handling
	
	// performance/time measurement
	struct 	timeval 	tv_connection_start,
				tv_connection_Response_start,
				tv_connection_Response_end;
	struct	timezone 	tz_connection_start,
				tz_connection_Response_start,
				tz_connection_Response_end;


};

#endif /* __yhttpd_connection_h__ */
