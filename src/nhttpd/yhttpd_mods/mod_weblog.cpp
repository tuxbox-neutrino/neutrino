//=============================================================================
// YHTTPD
// mod_weblog : Logging of HTTPD-Requests/Responses
//-----------------------------------------------------------------------------
// Normally
//=============================================================================
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <ctime>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

#include "mod_weblog.h"
#include <helper.h>
#include <ylogging.h>
#include <yprotect.h>

//=============================================================================
// Initialization of static variables
//=============================================================================
	pthread_mutex_t CmWebLog::WebLog_mutex = PTHREAD_MUTEX_INITIALIZER;
	FILE *CmWebLog::WebLogFile = NULL;
	int CmWebLog::RefCounter = 0;
	std::string CmWebLog::WebLogFilename=LOG_FILE;
	std::string CmWebLog::LogFormat =LOG_FORMAT;
	std::string CmWebLog::Refusal;
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
		closeLocked();
	pthread_mutex_unlock(&WebLog_mutex);
}
//=============================================================================
// HOOKS
//=============================================================================
//-----------------------------------------------------------------------------
// HOOK: Hook_EndConnection
//-----------------------------------------------------------------------------
THandleStatus CmWebLog::Hook_EndConnection(CyhookHandler *hh) {
	pthread_mutex_lock(&WebLog_mutex);
	const std::string format = LogFormat;
	pthread_mutex_unlock(&WebLog_mutex);
	if (format == "CLF")
		AddLogEntry_CLF(hh);
	else if (format == "ELF")
		AddLogEntry_ELF(hh);
	return HANDLED_CONTINUE; // even on Log-Error: continue
}

//-----------------------------------------------------------------------------
// HOOK: Hook_ReadConfig
// This hook ist called from ReadConfig
//-----------------------------------------------------------------------------
THandleStatus CmWebLog::Hook_ReadConfig(CConfigFile *Config, CStringList &) {
	pthread_mutex_lock(&WebLog_mutex);
	LogFormat = Config->getString("mod_weblog.log_format", LOG_FORMAT);
	WebLogFilename = Config->getString("mod_weblog.logfile", LOG_FILE);
	// the file this reading names is decided on at the next record
	closeLocked();
	Refusal.clear();
	pthread_mutex_unlock(&WebLog_mutex);
	return HANDLED_CONTINUE;
}
//-----------------------------------------------------------------------------
std::string CmWebLog::logDirectory() {
	const std::string dflt = LOG_FILE;
	const size_t slash = dflt.rfind('/');
	if (slash == std::string::npos)
		return ".";
	return (slash == 0) ? std::string("/") : dflt.substr(0, slash);
}
//-----------------------------------------------------------------------------
namespace {
// errno by its number: strerror hands out a buffer every thread shares
std::string errnoText(int err) {
	char said[24];
	snprintf(said, sizeof(said), "errno %d", err);
	return said;
}

// a failure that passes by itself: no descriptor, no memory or no space left
// for a moment, or a call interrupted
bool passes(int err) {
	return err == EMFILE || err == ENFILE || err == ENOMEM || err == EINTR
		|| err == EAGAIN || err == ENOSPC;
}

// when the server last said that a record waits for a passing failure; with
// WebLog_mutex held
time_t passingSaid = 0;
} // namespace

int CmWebLog::openIn(const std::string &path, const std::string &dir, std::string &why, bool *passing) {
	if (passing != NULL)
		*passing = false;
	const std::string named = ylog::text(path);
	if (path.empty() || path[0] != '/') {
		why = named + " is not an absolute name";
		return -1;
	}
	const size_t slash = path.rfind('/');
	const std::string parent = (slash == 0) ? std::string("/") : path.substr(0, slash);
	const std::string base = path.substr(slash + 1);
	if (base.empty() || base == "." || base == "..") {
		why = named + " names no file";
		return -1;
	}

	// The file is opened in the directory as this descriptor holds it, so a
	// name naming another directory, or one changed after the look below,
	// cannot carry the log out of it.
	const int dirfd = open(dir.c_str(), O_RDONLY | O_DIRECTORY | O_CLOEXEC);
	if (dirfd < 0) {
		const int err = errno;
		if (passing != NULL)
			*passing = passes(err);
		why = "the log directory " + ylog::text(dir) + " cannot be opened: " + errnoText(err);
		return -1;
	}
	struct stat in, d;
	const bool looked = stat(parent.c_str(), &in) == 0 && fstat(dirfd, &d) == 0;
	const int lookErr = errno;
	if (!looked && passes(lookErr)) {
		close(dirfd);
		if (passing != NULL)
			*passing = true;
		why = named + " cannot be looked at: " + errnoText(lookErr);
		return -1;
	}
	if (!looked || in.st_dev != d.st_dev || in.st_ino != d.st_ino) {
		close(dirfd);
		why = named + " is not directly in " + ylog::text(dir) + ", the one directory a log is written in";
		return -1;
	}
	// before the open, which would create a file that is not there yet
	if (yprotect::namesProtected(path)) {
		close(dirfd);
		why = named + " is a file that holds who may reach this box";
		return -1;
	}
	const int fd = openat(dirfd, base.c_str(),
			O_WRONLY | O_APPEND | O_CREAT | O_NOFOLLOW | O_NOCTTY | O_NONBLOCK | O_CLOEXEC,
			S_IRUSR | S_IWUSR);
	const int err = errno;
	close(dirfd);
	if (fd < 0) {
		if (passing != NULL)
			*passing = passes(err);
		why = (err == ELOOP) ? named + " is a link"
			: named + " cannot be opened: " + errnoText(err);
		return -1;
	}

	// what was opened, and not the name: a second name of a file, or a file
	// put in its place by somebody else, is no log of this server
	struct stat st;
	const char *refused = NULL;
	if (fstat(fd, &st) != 0) {
		if (passing != NULL)
			*passing = true;
		refused = "cannot be looked at once opened";
	} else if (yprotect::isProtected(fd))
		refused = "is a file that holds who may reach this box";
	else if (!S_ISREG(st.st_mode))
		refused = "is not a regular file";
	else if (st.st_nlink != 1)
		refused = "has another name besides this one";
	// not driven by the unit suite, which would need a file of a second user
	else if (st.st_uid != geteuid())
		refused = "belongs to another user";
	if (refused != NULL) {
		close(fd);
		why = named + " " + refused;
		return -1;
	}
	fcntl(fd, F_SETFL, fcntl(fd, F_GETFL) & ~O_NONBLOCK);
	return fd;
}
//-----------------------------------------------------------------------------
std::string CmWebLog::refusal() {
	pthread_mutex_lock(&WebLog_mutex);
	const std::string why = Refusal;
	pthread_mutex_unlock(&WebLog_mutex);
	return why;
}
//-----------------------------------------------------------------------------
// With WebLog_mutex held. A file turned down is said once and not tried again
// until the configuration is read anew. A failure that passes by itself (no
// descriptor or memory for a moment) is tried again with the next record and
// said at most once a minute.
bool CmWebLog::openLocked() {
	if (WebLogFile != NULL)
		return true;
	if (WebLogFilename.empty() || !Refusal.empty())
		return false;
	std::string why;
	bool passing = false;
	int fd = openIn(WebLogFilename, logDirectory(), why, &passing);
	FILE *file = NULL;
	if (fd >= 0) {
		file = fdopen(fd, "a");
		if (file == NULL) {
			const int err = errno;
			close(fd);
			fd = -1;
			passing = true;
			why = ylog::text(WebLogFilename) + " cannot be written: " + errnoText(err);
		}
	}
	if (fd < 0) {
		if (!passing) {
			Refusal = why;
			aprintf("mod_weblog: no log is written: %s\n", why.c_str());
		} else {
			const time_t now = time(NULL);
			if (passingSaid == 0 || now < passingSaid || now - passingSaid >= 60) {
				passingSaid = now;
				aprintf("mod_weblog: a record is not written, the next one tries again: %s\n",
					why.c_str());
			}
		}
		return false;
	}
	struct stat st;
	const bool fresh = (fstat(fd, &st) == 0 && st.st_size == 0);
	WebLogFile = file;
	if (fresh && LogFormat == "ELF") {
		fputs("#Version: 1.0\n", WebLogFile);
		fputs("#Remarks: " WEBSERVERNAME "\n", WebLogFile);
		fputs("#Fields: c-ip username date time x-request cs-uri sc-status cs-method bytes time-taken x-time-request x-time-response cached\n", WebLogFile);
		fflush(WebLogFile);
	}
	return true;
}
//-----------------------------------------------------------------------------
void CmWebLog::closeLocked() {
	if (WebLogFile != NULL) {
		fclose(WebLogFile);
		WebLogFile = NULL;
	}
}
//-----------------------------------------------------------------------------
bool CmWebLog::addRecord(const std::string &record) {
	bool success = false;
	pthread_mutex_lock(&WebLog_mutex); // yeah, its mine
	if (openLocked()) {
		success = fwrite(record.data(), 1, record.size(), WebLogFile) == record.size();
		fflush(WebLogFile);
	}
	pthread_mutex_unlock(&WebLog_mutex);
	return success;
}

namespace {
std::string number(long long n) {
	char buf[24];
	snprintf(buf, sizeof(buf), "%lld", n);
	return buf;
}
} // namespace

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
//	"request":	The request line as it came from the client, with the
//			values of its query left out (ylog::startLine).
//	status:		The HTTP status code returned to the client.
//	bytes:		The content-length of the document transferred.
//-----------------------------------------------------------------------------
std::string CmWebLog::recordCLF(CyhookHandler *hh, time_t now)
{
	struct tm when;
	localtime_r(&now, &when);
	char request_time[80];
	strftime(request_time, sizeof(request_time), "[%d/%b/%Y:%H:%M:%S]", &when);

	return ylog::text(hh->UrlData["clientaddr"]) + " - - " + request_time
		+ " \"" + ylog::startLine(hh->UrlData["startline"]) + "\" "
		+ number(hh->httpStatus) + " "
		+ number((long long) hh->GetContentLength()) + "\n";
}

void CmWebLog::AddLogEntry_CLF(CyhookHandler *hh)
{
	addRecord(recordCLF(hh, time(NULL)));
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


std::string CmWebLog::recordELF(CyhookHandler *hh, time_t now)
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
	int cached = (hh->HookVarList["CacheCategory"].empty()) ? 0 : 1;

	struct tm when;
	localtime_r(&now, &when);

	char _date[11];
	strftime(_date, sizeof(_date), "%Y-%m-%d", &when);

	char _time[11];
	strftime(_time, sizeof(_time), "%H:%M:%S", &when);

	std::string time_taken_request = hh->HookVarList["enlapsed_request"];
	std::string time_taken_response = hh->HookVarList["enlapsed_response"];
	long time_taken = atoi(time_taken_request.c_str()) + atoi(time_taken_response.c_str());

	// cs-uri as the request line carries it, before any rewrite, with the
	// names of its query and none of their values
	return ylog::text(hh->UrlData["clientaddr"]) + " " + _date + " " + _time
		+ " \"" + ylog::startLine(hh->UrlData["startline"]) + "\" "
		+ ylog::url(hh->UrlData["target"]) + " "
		+ number(hh->httpStatus) + " "
		+ cs_method + " "
		+ number((long long) hh->GetContentLength()) + " "
		+ number(time_taken) + " "
		+ ylog::text(time_taken_request) + " "
		+ ylog::text(time_taken_response) + " "
		+ number(cached) + "\n";
}

void CmWebLog::AddLogEntry_ELF(CyhookHandler *hh)
{
	addRecord(recordELF(hh, time(NULL)));
}
