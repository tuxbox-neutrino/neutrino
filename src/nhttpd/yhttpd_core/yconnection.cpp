//=============================================================================
// YHTTPD
// Connection
//=============================================================================

// system
#include <sys/time.h>
#include <errno.h>
#include <cstring>
// yhttpd
#include <yconfig.h>
#include "ytypes_globals.h"
#include "ywebserver.h"
#include "yconnection.h"
#include "helper.h"
//=============================================================================
// Initialization of static variables
//=============================================================================
long CWebserverConnection::GConnectionNumber = 0;

//=============================================================================
// Constructor & Destructor & Initialization
//=============================================================================
CWebserverConnection::CWebserverConnection(CWebserver *pWebserver) {
	sock = 0;
	ConnectionNumber = 0;
	enlapsed_request = 0;
	enlapsed_response = 0;

	Webserver = pWebserver;
	Request.Webserver = pWebserver;
	Request.Connection = this;
	Response.Webserver = pWebserver;
	Response.Connection = this;
	Method = M_UNKNOWN;
	HttpStatus = 0;
	RequestCanceled = false;
#ifdef Y_CONFIG_FEATURE_KEEP_ALIVE
	keep_alive = true;
#else
	keep_alive = false;
#endif
}
//-------------------------------------------------------------------------
CWebserverConnection::CWebserverConnection() {
	//	aprintf("test CWebserverConnection::CWebserverConnection()\n");
	Method = M_UNKNOWN;
	sock = 0;
	RequestCanceled = 0;
	keep_alive = 0;
	HttpStatus = 0;
	enlapsed_request = 0;
	enlapsed_response = 0;
	ConnectionNumber = ++GConnectionNumber;
}
//-------------------------------------------------------------------------
CWebserverConnection::~CWebserverConnection(void) {
}
//-------------------------------------------------------------------------
// End The Connection. Request and Response allready handled.
// do "after done" work, like create a www-Log entry.
// Use "Hooks_EndConnection()" Handler to write own Hooks.
//-------------------------------------------------------------------------
void CWebserverConnection::EndConnection() {
	HookHandler.HookVarList["enlapsed_request"] = itoa(enlapsed_request / 1000);
	HookHandler.HookVarList["enlapsed_response"] = itoa(enlapsed_response
			/ 1000);
	HookHandler.Hooks_EndConnection(); // Handle Hooks
	if (RequestCanceled) // Canceled
		keep_alive = false;
	RequestCanceled = true;
	//	sock->Flush();
#ifndef Y_CONFIG_FEATURE_KEEP_ALIVE
	sock->close();
#endif
}
//-------------------------------------------------------------------------
// Main
// Handle the Request, Handle (Send) Response), End the Connection
//-------------------------------------------------------------------------
void CWebserverConnection::HandleConnection() {
	gettimeofday(&tv_connection_start, &tz_connection_start);

	// get the request
	if (Request.HandleRequest()) {
		// determine time from Connection creation until now
		gettimeofday(&tv_connection_Response_start,
				&tz_connection_Response_start);
		enlapsed_request = ((tv_connection_Response_start.tv_sec
				- tv_connection_start.tv_sec) * 1000000
				+ (tv_connection_Response_start.tv_usec
						- tv_connection_start.tv_usec));

		// Keep-Alive checking
#ifdef Y_CONFIG_FEATURE_KEEP_ALIVE
		if(string_tolower(Request.HeaderList["Connection"]) == "close"
				|| (httprotocol != "HTTP/1.1" && string_tolower(Request.HeaderList["Connection"]) != "keep-alive")
				|| !Webserver->CheckKeepAliveAllowedByIP(sock->get_client_ip()))
#endif
		keep_alive = false;

		// Send a response
		Response.SendResponse();

		// determine time for SendResponse
		gettimeofday(&tv_connection_Response_end, &tz_connection_Response_end);
		enlapsed_response = ((tv_connection_Response_end.tv_sec
				- tv_connection_Response_start.tv_sec) * 1000000
				+ (tv_connection_Response_end.tv_usec
						- tv_connection_Response_start.tv_usec));

		// print production times			
		log_level_printf(1, "enlapsed time request:%ld response:%ld url:%s\n",
				enlapsed_request, enlapsed_response,
				(Request.UrlData["fullurl"]).c_str());

	} else {
		RequestCanceled = true;
		keep_alive = false; // close this connection socket
		//		dperror("Error while parsing request\n");
		log_level_printf(1, "request canceled: %s\n", strerror(errno));
	}
	EndConnection();
}

//-------------------------------------------------------------------------
void CWebserverConnection::ShowEnlapsedRequest(char *text) {

	long enlapsed = GetEnlapsedRequestTime() / 1000;
	log_level_printf(1, "enlapsed-f-start (%s) t:%ld url:%s\n", text, enlapsed,
			(Request.UrlData["fullurl"]).c_str());
}
//-------------------------------------------------------------------------
// Time from creation of socket until now in microseconds!
//-------------------------------------------------------------------------
long CWebserverConnection::GetEnlapsedRequestTime() {
	struct timeval tv_now;
	struct timezone tz_now;
	gettimeofday(&tv_now, &tz_now);
	return ((tv_now.tv_sec - tv_connection_start.tv_sec) * 1000000
			+ (tv_now.tv_usec - tv_connection_start.tv_usec));
}

//-------------------------------------------------------------------------
// Time from beginning of response until now in microseconds!
//-------------------------------------------------------------------------
long CWebserverConnection::GetEnlapsedResponseTime() {
	struct timeval tv_now;
	struct timezone tz_now;
	gettimeofday(&tv_now, &tz_now);
	return ((tv_now.tv_sec - tv_connection_Response_start.tv_sec) * 1000000
			+ (tv_now.tv_usec - tv_connection_Response_start.tv_usec));
}

