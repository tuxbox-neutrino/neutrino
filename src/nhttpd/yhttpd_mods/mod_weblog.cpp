//=============================================================================
// YHTTPD
// mod_weblog : Logging of HTTPD-Requests/Responses
//-----------------------------------------------------------------------------
// Normally
//=============================================================================
#include <cstdarg>
#include <cstring>
#include <cstdlib>
#include <stdio.h>
#include <unistd.h>

#include "mod_weblog.h"
#include <helper.h>

//=============================================================================
// Initialization of static variables
//=============================================================================
	pthread_mutex_t CmWebLog::WebLog_mutex = PTHREAD_MUTEX_INITIALIZER;
	FILE *CmWebLog::WebLogFile = NULL;
	int CmWebLog::RefCounter = 0;
	std::string CmWebLog::WebLogFilename=LOG_FILE;
	std::string CmWebLog::LogFormat =LOG_FORMAT;
//=============================================================================
// Constructor & Destructor
//=============================================================================
CmWebLog::CmWebLog(void) {
	pthread_mutex_lock(&WebLog_mutex); // yea, its mine
	RefCounter++;
	pthread_mutex_unlock(&WebLog_mutex);
}
//-----------------------------------------------------------------------------
CmWebLog::~CmWebLog(void) {
	pthread_mutex_lock(&WebLog_mutex); // yea, its mine
	--RefCounter;
	if (RefCounter <= 0)
		CloseLogFile();
	pthread_mutex_unlock(&WebLog_mutex);
}
//=============================================================================
// HOOKS
//=============================================================================
//-----------------------------------------------------------------------------
// HOOK: Hook_EndConnection
//-----------------------------------------------------------------------------
THandleStatus CmWebLog::Hook_EndConnection(CyhookHandler *hh) {
	if (LogFormat == "CLF")
		AddLogEntry_CLF(hh);
	else if (LogFormat == "ELF")
		AddLogEntry_ELF(hh);
	return HANDLED_CONTINUE; // even on Log-Error: continue
}

//-----------------------------------------------------------------------------
// HOOK: Hook_ReadConfig
// This hook ist called from ReadConfig
//-----------------------------------------------------------------------------
THandleStatus CmWebLog::Hook_ReadConfig(CConfigFile *Config, CStringList &) {
	LogFormat = Config->getString("mod_weblog.log_format", LOG_FORMAT);
	WebLogFilename = Config->getString("mod_weblog.logfile", LOG_FILE);
	return HANDLED_CONTINUE;
}
//-----------------------------------------------------------------------------
bool CmWebLog::OpenLogFile() {
	if (WebLogFilename == "")
		return false;
	if (WebLogFile == NULL) {
		bool isNew = false;
		pthread_mutex_lock(&WebLog_mutex); // yeah, its mine
		if (access(WebLogFilename.c_str(), 4) != 0)
			isNew = true;
		WebLogFile = fopen(WebLogFilename.c_str(), "a");
		if (isNew) {
			if (LogFormat == "ELF") {
				printf("#Version: 1.0\n");
				printf("#Remarks: yhttpd" WEBSERVERNAME "\n");
				printf("#Fields: c-ip username date time x-request cs-uri sc-status cs-method bytes time-taken x-time-request x-time-response cached\n");
			}
		}
		pthread_mutex_unlock(&WebLog_mutex);
	}
	return (WebLogFile != NULL);
}
//-----------------------------------------------------------------------------
void CmWebLog::CloseLogFile() {
	if (WebLogFile != NULL) {
		pthread_mutex_lock(&WebLog_mutex); // yeah, its mine
		fclose( WebLogFile);
		WebLogFile = NULL;
		pthread_mutex_unlock(&WebLog_mutex);
	}
}
//-----------------------------------------------------------------------------
#define bufferlen 1024*8
bool CmWebLog::printf(const char *fmt, ...) {
	if (!OpenLogFile())
		return false;
	bool success = false;
	if (WebLogFile != NULL) {
		char buffer[bufferlen]={0};
		pthread_mutex_lock(&WebLog_mutex); // yeah, its mine
		va_list arglist;
		va_start(arglist, fmt);
		vsnprintf(buffer, bufferlen, fmt, arglist);
		va_end(arglist);
		unsigned int len = strlen(buffer);
		success = (fwrite(buffer, len, 1, WebLogFile) == len);
		fflush( WebLogFile);
		pthread_mutex_unlock(&WebLog_mutex);
	}
	return success;
}

//-----------------------------------------------------------------------------
//	CLF - Common Logfile Format
//
//	Example: 127.0.0.1 - frank [10/Oct/2000:13:55:36 -0700] "GET /apache_pb.gif HTTP/1.0" 200 2326
//
//	The common logfile format is as follows:
//	    remotehost rfc931 authuser [date] "request" status bytes
//
//	remotehost:	Remote hostname (or IP number if DNS hostname is not available, or if DNSLookup is Off.
//	rfc931:		The remote logname of the user.
//	authuser:	The username as which the user has authenticated himself.
//	[date]:		Date and time of the request.
//	"request":	The request line exactly as it came from the client.
//	status:		The HTTP status code returned to the client.
//	bytes:		The content-length of the document transferred.
//-----------------------------------------------------------------------------
void CmWebLog::AddLogEntry_CLF(CyhookHandler *hh)
{
#if 0
//never used
	std::string cs_method;
	switch (hh->Method)
	{
		case M_GET:	cs_method = "GET";	break;
		case M_POST:	cs_method = "POST";	break;
		case M_HEAD:	cs_method = "HEAD";	break;

		default:
			cs_method = "unknown";
			break;
	}
#endif
	std::string c_ip = 			hh->UrlData["clientaddr"].c_str();
	std::string request_startline = 	hh->UrlData["startline"].c_str();
	int s_status = hh->httpStatus;
	off_t bytes	= hh->GetContentLength();

	struct tm *time_now;
	time_t now = time(NULL);
	char request_time[80];

	time_now = localtime(&now);
	strftime(request_time, 80, "[%d/%b/%Y:%H:%M:%S]", time_now);

	printf("%s - - %s \"%s\" %d %lld\n",
		c_ip.c_str(),
		request_time,
		request_startline.c_str(),
		s_status,
		(long long) bytes);
}

//-----------------------------------------------------------------------------
/*
	Definition: (ELF) Extended Log File Format
	W3C Working Draft WD-logfile-960323 (http://www.w3.org/pub/WWW/TR/WD-logfile-960323.html)

	An extended log file contains a sequence of lines containing ASCII
	characters terminated by either the sequence LF or CRLF.
	Log file generators should follow the line termination convention for
	the platform on which they are executed. Analyzers should accept either form.
	Each line may contain either a directive or an entry.

	Entries consist of a sequence of fields relating to a single HTTP
	transaction. Fields are separated by whitespace, the use of tab characters
	for this purpose is encouraged. If a field is unused in a particular
	entry dash "-" marks the omitted field. Directives record information about
	the logging process itself.

	Lines beginning with the # character contain directives.
	The following directives are defined:

	- Version: <integer>.<integer>
			The version of the extended log file format used.
			This draft defines version 1.0.
	- Fields: [<specifier>...]
			Specifies the fields recorded in the log.
	- Software: string
			Identifies the software which generated the log.
	- Start-Date: <date> <time>
			The date and time at which the log was started.
	- End-Date:<date> <time>
			The date and time at which the log was finished.
	- Date:<date> <time>
			The date and time at which the entry was added.
	- Remark: <text>
			Comment information. Data recorded in this field should be
			ignored by analysis tools.

	The directives Version and Fields are required and should precede all
	entries in the log. The Fields directive specifies the data recorded in the
	fields of each entry.

//-----------------------------------------------------------------------------
	Example
	The following is an example file in the extended log format:

	#Version: 1.0
	#Date: 12-Jan-1996 00:00:00
	#Fields: time cs-method cs-uri
	00:34:23 GET /foo/bar.html
	12:21:16 GET /foo/bar.html
	12:45:52 GET /foo/bar.html
	12:57:34 GET /foo/bar.html

//-----------------------------------------------------------------------------
	Fields
	The #Fields directive lists a sequence of field identifiers specifying
	the information recorded in each entry. Field identifiers may have one
	of the following forms:

	- identifier
		Identifier relates to the transaction as a whole.
	- prefix-identifier
		Identifier relates to information transfer between parties
		defined by the value prefix.
	- prefix(header)
		Identifies the value of the HTTP header field header for transfer
		between parties defined by the value prefix. Fields specified in
		this manner always have the value <string>.

	The following prefixes are defined:

		c	: Client
		s 	: Server
		r 	: Remote
		cs 	: Client to Server.
		sc 	: Server to Client.
		sr	: Server to Remote Server, this prefix is used by proxies.
		rs	: Remote Server to Server, this prefix is used by proxies.
		x 	: Application specific identifier.

		The identifier cs-method thus refers to the method in the request
		sent by the client to the server while sc(Referer) refers to the
		referer: field of the reply. The identifier c-ip refers to the
		client's ip address.

	Identifiers. (The following identifiers do not require a prefix)

		date		: Date at which transaction completed, field has type <date>
		time 		: Time at which transaction completed, field has type <time>
		time-taken	: Time taken for transaction to complete in
				  seconds, field has type <fixed>
		bytes 		: bytes transferred, field has type <integer>
		cached 		: Records whether a cache hit occurred, field
				  has type <integer> 0 indicates a cache miss.

	Identifiers. (The following identifiers require a prefix)

		ip 		: IP address and port, field has type <address>
		dns 		: DNS name, field has type <name>
		status 		: Status code, field has type <integer>
		comment 	: Comment returned with status code, field has type <text>
		method 		: Method, field has type <name>
		uri 		: URI, field has type <uri>
		uri-stem 	: Stem portion alone of URI (omitting query), field has type <uri>
		uri-query	: Query portion alone of URI, field has type <uri>

	Special fields for log summaries.
	Analysis tools may generate log summaries. A log summary entry begins with a count specifying the number of times a particular even occurred. For example a site may be interested in a count of the number of requests for a particular URI with a given referer: field but not be interested in recording information about individual requests such as the IP address.

	The following field is mandatory and must precede all others:

	count
	The number of entries for which the listed data, field has type <integer>
	The following fields may be used in place of time to allow aggregation of log file entries over intervals of time.

	time-from
	Time at which sampling began, field has type <time>
	time-to
	Time at which sampling ended, field has type <time>
	interval
	Time over which sampling occurred in seconds, field has type <integer>
	-----------------------------------------------------------------------------
	actual Implementation:

		c-ip
		date
		tile
		request
		cs-uri
		sc-status
		cs-method
		bytes
		time-taken
		x-time-request
		x-time-response
		cached
*/

void CmWebLog::AddLogEntry_ELF(CyhookHandler *hh)
{
	std::string cs_method;
	switch (hh->Method)
	{
		case M_GET:	cs_method = "GET";	break;
		case M_POST:	cs_method = "POST";	break;
		case M_HEAD:	cs_method = "HEAD";	break;

		default:
			cs_method = "unknown";
			break;
	}
	std::string c_ip = 			hh->UrlData["clientaddr"].c_str();
	std::string request_startline = 	hh->UrlData["startline"].c_str();
	std::string cs_uri			= hh->UrlData["fullurl"];
	int sc_status = hh->httpStatus;
	off_t bytes	= hh->GetContentLength();
	int cached = (hh->HookVarList["CacheCategory"].empty()) ? 0 : 1;

	struct tm *time_now;
	time_t now = time(NULL);
	time_now = localtime(&now);

	char request_time[80];
	strftime(request_time, 80, "[%d/%b/%Y:%H:%M:%S]", time_now);

	char _date[11];
	strftime(_date, 11, "%Y-%m-%d", time_now);

	char _time[11];
	strftime(_time, 11, "%H:%M:%S", time_now);

	std::string time_taken_request = hh->HookVarList["enlapsed_request"];
	std::string time_taken_response = hh->HookVarList["enlapsed_response"];
	long time_taken = atoi(time_taken_request.c_str()) + atoi(time_taken_response.c_str());

	printf("%s %s %s \"%s\" %s %d %s %d %ld %s %s %lld\n",
		c_ip.c_str(),
		_date,
		_time,
		request_startline.c_str(),
		cs_uri.c_str(),
		sc_status,
		cs_method.c_str(),
		(long long) bytes,
		time_taken,
		time_taken_request.c_str(),
		time_taken_response.c_str(),
		cached
		);
}
