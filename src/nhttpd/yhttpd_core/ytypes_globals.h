//=============================================================================
// YHTTPD
// Type Definitions and Global Variables
//=============================================================================

//-----------------------------------------------------------------------------
#ifndef __yhttpd_types_globals_h__
#define __yhttpd_types_globals_h__

// c++
#include <string>
#include <map>
#include <vector>

//=============================================================================
// Global Types
//=============================================================================
typedef std::map<std::string, std::string> CStringList;
typedef std::vector<std::string> CStringArray;
typedef std::vector<std::string> CStringVector;

//=============================================================================
// HTTP Protocol Definitions
//=============================================================================
// HTTP-Status codes
typedef enum
{
	HTTP_NIL			= -1,
	HTTP_CONTINUE			= 100,
	HTTP_SWITCHING_PROTOCOLS	= 101, // not used
	HTTP_OK 			= 200, // use: everything is right. request (and response) handled
	HTTP_CREATED			= 201, // not used
	HTTP_ACCEPTED			= 202, // use: idicate accept of request. request take more time.
	HTTP_NON_AUTHORITATIVE_INFO 	= 203,
	HTTP_NO_CONTENT 		= 204,
	HTTP_RESET_CONTENT 		= 205, // not used
	HTTP_PARTIAL_CONTENT 		= 206, // not used
	HTTP_MULTIBLE_CHOICES 		= 300, // not used
	HTTP_MOVED_PERMANENTLY 		= 301,
	HTTP_MOVED_TEMPORARILY 		= 302,
	HTTP_SEE_OTHER	 		= 303, // not used
	HTTP_NOT_MODIFIED 		= 304,
	HTTP_USE_PROXY 			= 305, // not used
	HTTP_TEMPORARY_REDIRECT 	= 307, // not used
	HTTP_BAD_REQUEST 		= 400,
	HTTP_UNAUTHORIZED 		= 401,
	HTTP_PAYMENT_REQUIRED 		= 402, // not used
	HTTP_FORBIDDEN 			= 403,
	HTTP_NOT_FOUND 			= 404,
	HTTP_METHOD_NOT_ALLOWED		= 405, // not used
	HTTP_NOT_ACCEPTABLE 		= 406, // not used
	HTTP_PROXY_AUTHENTICATION_REQUIRED= 407, // not used
	HTTP_REQUEST_TIMEOUT 		= 408, // not used
	HTTP_CONFLICT			= 409, // not used
	HTTP_GONE 			= 410, // not used
	HTTP_LENGTH_REQUIRED 		= 411, // not used
	HTTP_PRECONDITION_FAILED 	= 412, // not used
	HTTP_REQUEST_ENTITY_TOO_LARGE 	= 413, // not used
	HTTP_REQUEST_URI_TOO_LARGE 	= 414, // not used
	HTTP_UNSUPPORTED_MEDIA_TYPE 	= 415, // not used
	HTTP_REQUEST_RANGE_NOT_SATISFIABLE= 416, // not used
	HTTP_EXPECTAION_FAILED 		= 417, // not used
	HTTP_INTERNAL_SERVER_ERROR 	= 500,
	HTTP_NOT_IMPLEMENTED 		= 501,
	HTTP_BAD_GATEWAY 		= 502, // not used
	HTTP_SERVICE_UNAVAILABLE 	= 503, // not used
	HTTP_GATEWAY_TIMEOUT 		= 504, // not used
	HTTP_HTTP_VERSION_NOT_SUPPORTED	= 505, // not used
} HttpResponseType;

typedef struct
{
	HttpResponseType type;
	const char *name;
	const char *info;
} HttpEnumString;

static const HttpEnumString httpResponseNames[] = {
	{ HTTP_CONTINUE, 		"Continue" },
	{ HTTP_SWITCHING_PROTOCOLS, 	"Switching Protocols" },
	{ HTTP_OK, 			"OK" },
	{ HTTP_CREATED, 		"Created" },
	{ HTTP_ACCEPTED, 		"Accepted" },
	{ HTTP_NON_AUTHORITATIVE_INFO, 	"No Authorative Info" },
	{ HTTP_NO_CONTENT, 		"No Content" },
	{ HTTP_RESET_CONTENT, 		"Reset Content" },
	{ HTTP_PARTIAL_CONTENT, 	"Partial Content" },
	{ HTTP_MULTIBLE_CHOICES, 	"Multiple Choices" },
	{ HTTP_MOVED_PERMANENTLY, 	"Moved Permanently" },
	{ HTTP_MOVED_TEMPORARILY, 	"Moved Temporarily" },
	{ HTTP_SEE_OTHER, 		"See Other" },
	{ HTTP_NOT_MODIFIED, 		"Not Modified" },
	{ HTTP_USE_PROXY, 		"Use Proxy" },
	{ HTTP_TEMPORARY_REDIRECT, 	"Temporary Redirect" },
	{ HTTP_BAD_REQUEST, 		"Bad Request", "Unsupported method." },
	{ HTTP_UNAUTHORIZED, 		"Unauthorized", "Access denied." },
	{ HTTP_PAYMENT_REQUIRED, 	"Payment Required" },
	{ HTTP_FORBIDDEN, 		"Forbidden", "" },
	{ HTTP_NOT_FOUND, 		"Not Found", "The requested URL was not found on this server." },
	{ HTTP_METHOD_NOT_ALLOWED, 	"Method Not Allowed" },
	{ HTTP_NOT_ACCEPTABLE, 		"Not Acceptable" },
	{ HTTP_PROXY_AUTHENTICATION_REQUIRED,"Proxy Authentication Required" },
	{ HTTP_REQUEST_TIMEOUT, 	"Request Time-out" },
	{ HTTP_CONFLICT, 		"Conflict" },
	{ HTTP_GONE, 			"Gone" },
	{ HTTP_LENGTH_REQUIRED, 	"Length Required" },
	{ HTTP_PRECONDITION_FAILED, 	"Precondition Failed" },
	{ HTTP_REQUEST_ENTITY_TOO_LARGE,"Request Entity Too Large" },
	{ HTTP_REQUEST_URI_TOO_LARGE, 	"Request-URI Too Large" },
	{ HTTP_UNSUPPORTED_MEDIA_TYPE, 	"Unsupported Media Type" },
	{ HTTP_REQUEST_RANGE_NOT_SATISFIABLE,"Requested range not satisfiable" },
	{ HTTP_EXPECTAION_FAILED, 	"Expectation Failed" },
	{ HTTP_INTERNAL_SERVER_ERROR,	"Internal Server Error", "Internal Server Error" },
	{ HTTP_NOT_IMPLEMENTED, 	"Not Implemented", "The requested method is not recognized by this server." },
	{ HTTP_BAD_GATEWAY, 		"Bad Gateway" },
	{ HTTP_SERVICE_UNAVAILABLE, 	"Service Unavailable" },
	{ HTTP_GATEWAY_TIMEOUT, 	"Gateway Time-out" },
	{ HTTP_HTTP_VERSION_NOT_SUPPORTED,"HTTP Version not supported" },
};

// HTTP-methods
typedef enum
{
	M_UNKNOWN	= 0,
	M_POST		= 1,
	M_GET,
	M_HEAD,
	M_PUT,
	M_DELETE,
	M_TRACE
} THttp_Method;

// Date Format RFC 1123
static const char RFC1123FMT[] = "%a, %d %b %Y %H:%M:%S GMT";

// mime-table
typedef struct
{
	const char *fileext;
	const char *mime;
} TMimePair;
static const TMimePair MimeFileExtensions[] = {
	{"xml",		"text/xml"},
	{"htm",		"text/html"},
	{"html",	"text/html"},
	{"yhtm",	"text/html"},
	{"yhtml",	"text/html"},
	{"jpg",		"image/jpeg"},
	{"jpeg",	"image/jpeg"},
	{"gif",		"image/gif"},
	{"png",		"image/png"},
	{"txt",		"image/plain"},
	{"css",		"text/css"},

};

#endif // __yhttpd_types_globals_h__
