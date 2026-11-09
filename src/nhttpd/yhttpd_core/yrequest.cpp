//=============================================================================
// YHTTPD
// Request
//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
// Request-Data
//	UrlData (CStringList)
//		startline	First line of http-request (GET <fullurl> HTTP/1.x)
//		fullurl		::= <url>?<paramstring>
//		paramstring	Is provided by String-Array ParameterList
//		url		::= <path>/<filename> "/" is last "/" in string
//		path		begin with /...
//		filename	::= <filenamepure>.<fileext>
//		filenamepure
//		fileext		Extension like html, jpg, ...
//=============================================================================

// c++
#include <algorithm>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <errno.h>
#include <time.h>
#include <unistd.h>
#include <vector>

// system
#include <fcntl.h>
#include <sys/socket.h>

// yhttpd
#include "yrequest.h"
#include "yconnection.h"
#include "helper.h"
#include "yhook.h"
#include "ysocket.h"
#include <yhttpd.h>
#include <httpd/authority.h>
#include <httpd/nhttpd_bridge.h>

//-----------------------------------------------------------------------------
// The head of a request, its request line and its header together: how long it
// may take to arrive and how many bytes of the wire it may take, empty lines
// before the request line and every line's ending as sent included, whatever
// the method and the target. A peer that connects and sends nothing, stops
// halfway through its header, or sends header lines without end is answered
// and let go at one of these, so it holds a connection thread, and without
// connection threads the server, no longer than this.
//-----------------------------------------------------------------------------
#define HEAD_TIMEOUT_MS		10000
#define HEAD_LIMIT		(64 * 1024)

// A clock a stepped system time cannot move: the head's deadline is kept on it.
static long long headClockMs() {
	struct timespec t;
	clock_gettime(CLOCK_MONOTONIC, &t);
	return (long long) t.tv_sec * 1000 + t.tv_nsec / 1000000;
}

//=============================================================================
// Constructor & Destructor
//=============================================================================
CWebserverRequest::CWebserverRequest(CWebserver *pWebserver) {
	RawBodyTooLarge = false;
	HeadStarted = 0;
	HeadBytes = 0;
	Webserver = pWebserver;
	CWebserverRequest();
	Connection=NULL;
}

// What is left of the head's deadline, in milliseconds; 0 once it has passed.
int CWebserverRequest::headTimeLeft() const {
	const long long left = (long long) HEAD_TIMEOUT_MS - (headClockMs() - HeadStarted);
	return left > 0 ? (int) left : 0;
}

//-----------------------------------------------------------------------------
// A head that did not arrive whole. A peer that went away is answered nothing;
// one still there gets 408 at the deadline and over_limit past the limit, and
// is let go by lingering, more of what it sends perhaps still coming.
//-----------------------------------------------------------------------------
bool CWebserverRequest::headRefused(HttpResponseType over_limit) {
	if (!Connection->sock->isValid)
		return false;
	log_level_printf(1, "HandleRequest: no whole request head within %d ms and %d bytes\n",
			HEAD_TIMEOUT_MS, HEAD_LIMIT);
	Connection->Response.SendError(headTimeLeft() > 0 ? over_limit : HTTP_REQUEST_TIMEOUT);
	Connection->sock->linger_on_close = true;
	Connection->RequestCanceled = true;
	return false;
}

//-----------------------------------------------------------------------------
// A header that arrived whole and is not one: answered 400 before any hook
// sees the request, and let go by lingering.
//-----------------------------------------------------------------------------
bool CWebserverRequest::headerRefused() {
	log_level_printf(1, "HandleRequest: header not well formed, refused\n");
	Connection->Response.SendError(HTTP_BAD_REQUEST);
	Connection->sock->linger_on_close = true;
	Connection->RequestCanceled = true;
	return false;
}

// A character a field name may carry (RFC 9110, 5.6.2: tchar).
static bool nameChar(unsigned char c) {
	if ((c >= '0' && c <= '9') || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z'))
		return true;
	switch (c) {
	case '!': case '#': case '$': case '%': case '&': case '\'': case '*': case '+':
	case '-': case '.': case '^': case '_': case '`': case '|': case '~':
		return true;
	default:
		return false;
	}
}

// The fields the rules about where a request came from read, each one value a
// request has (User-Agent, Origin, Referer, Sec-Fetch-*), lower-cased.
static bool provenanceField(const std::string &lower) {
	return lower == "user-agent" || lower == "origin" || lower == "referer" ||
	       lower.compare(0, 10, "sec-fetch-") == 0;
}

//-----------------------------------------------------------------------------
// Whether a header as read (every line ending in CRLF, the empty line last) is
// one: each line a field name of token characters, a colon right after it, and
// a value without a control character but the tab. So a line that continues
// the one before it (obs-fold, a space or a tab first), a space before the
// colon and a line without a colon are refused, as HTTP asks of a server
// (RFC 9112, 5.1 and 5.2), rather than read as something the line does not say.
// A field the rules about a request's origin read is one value: named twice,
// in any spelling, the header is refused too, as the reader keeps only one of
// them and a proxy in front may keep the other.
//-----------------------------------------------------------------------------
bool CWebserverRequest::headerWellFormed(const std::string &raw_header) {
	std::vector<std::string> named;
	size_t at = 0;
	while (at < raw_header.size()) {
		const size_t eol = raw_header.find("\r\n", at);
		if (eol == std::string::npos)
			return false;
		const size_t from = at;
		at = eol + 2;
		if (eol == from)
			return at == raw_header.size();
		const size_t colon = raw_header.find(':', from);
		if (colon == std::string::npos || colon >= eol || colon == from)
			return false;
		for (size_t i = from; i < colon; i++)
			if (!nameChar((unsigned char) raw_header[i]))
				return false;
		for (size_t i = colon + 1; i < eol; i++) {
			const unsigned char c = (unsigned char) raw_header[i];
			if ((c < 0x20 && c != '\t') || c == 0x7f)
				return false;
		}
		const std::string name = string_tolower(raw_header.substr(from, colon - from));
		if (provenanceField(name)) {
			if (std::find(named.begin(), named.end(), name) != named.end())
				return false;
			named.push_back(name);
		}
	}
	return false;
}

//=============================================================================
// Parsing Request
//=============================================================================
//-----------------------------------------------------------------------------
// Main Request Parsing
// RFC2616
//	generic-message = start-line
//		*(message-header CRLF)
//		CRLF
//		[ message-body ]
//	start-line      = Request-Line | Status-Line
//-----------------------------------------------------------------------------
bool CWebserverRequest::HandleRequest(void) {
	HeadStarted = headClockMs();
	HeadBytes = 0;
	std::string start_line = "";
	// read first line: up to its LF, empty lines before it skipped and counted
	do {
		if (!Connection->sock->ReceiveUntil("\n", start_line, HEAD_LIMIT - HeadBytes, headTimeLeft()))
			return headRefused(HTTP_REQUEST_URI_TOO_LARGE);
		HeadBytes += start_line.size();
	} while (start_line == "\r\n" || start_line == "\n");

	start_line = trim(start_line);
	log_level_printf(1, "Request: %s\n", ylog::startLine(start_line).c_str());
	UrlData["startline"] = start_line;
	if (!ParseStartLine(start_line))
		return false;

	// An API target takes its own path: the methods beyond GET/POST/HEAD,
	// a bounded raw body, a header read that waits for the whole header.
	// Everything else below is the legacy path and is left as it was.
	// Classified on the decoded url, which is what the bridge claims on; the
	// router gets the target as written (UrlData["target"]) and decides for
	// itself what an escape in it means.
	if (CyhookHandler::isApiTarget(UrlData["url"]))
		return HandleApiRequest();

	if (Connection->Method == M_GET || Connection->Method == M_HEAD) {
		/* The whole header, up to its empty line, before anything is decided on
		   it: the Host check, the sign-in and the rule that asks a browser first
		   read headers a request may carry anywhere in it, and a header read as
		   far as it had arrived could leave one of them out. */
		std::string raw_header;
		if (!readLegacyHeader(*Connection->sock, raw_header, HEAD_LIMIT - HeadBytes, headTimeLeft()))
			return headRefused(HTTP_BAD_REQUEST);
		if (!headerWellFormed(raw_header))
			return headerRefused();
		ParseHeader(raw_header);
	}
	// Other Methods. PATCH sits here with PUT: outside the API it is refused
	// the way PUT always was, and the API branch above never reaches this.
	if (Connection->Method == M_DELETE || Connection->Method == M_PUT
			|| Connection->Method == M_TRACE || Connection->Method == M_PATCH) {
		//todo: implement
		aprintf("HTTP Method not implemented :%d\n", Connection->Method);
		Connection->Response.SendError(HTTP_NOT_IMPLEMENTED);
		// header and body are still on their way in
		Connection->sock->linger_on_close = true;
		return false;
	}
	// handle POST (read header & body)
	if (Connection->Method == M_POST)
		return HandlePost();
	// if you are here, something went wrong

	return true;
}

//-----------------------------------------------------------------------------
// Parse the start-line
//	from RFC2616 / 5.1 Request-Line (start-line):
//	Request-Line   = Method SP Request-URI SP HTTP-Version CRLF (SP=Space)
//
//	Determine Reqest-Method, URL, HTTP-Version and Split Parameters
//	Split URL into path, filename, fileext .. UrlData[]
//-----------------------------------------------------------------------------
bool CWebserverRequest::ParseStartLine(std::string start_line) {
	std::string method = "", url = "", tmp = "";

	log_level_printf(8, "<ParseStartLine>: line: %s\n", ylog::startLine(start_line).c_str());
	if (ySplitString(start_line, " ", method, tmp)) {
		if (ySplitStringLast(tmp, " ", url, Connection->httprotocol)) {
			UrlData["target"] = url; // as written, undecoded, with query
			analyzeURL(url);
			UrlData["httprotocol"] = Connection->httprotocol;
			// determine http Method
			if (method.compare("POST") == 0)	Connection->Method = M_POST;
			else if (method.compare("GET") == 0)	Connection->Method = M_GET;
			else if (method.compare("HEAD") == 0)	Connection->Method = M_HEAD;
			else if (method.compare("PUT") == 0)	Connection->Method = M_PUT;
			else if (method.compare("DELETE") == 0)	Connection->Method = M_DELETE;
			else if (method.compare("TRACE") == 0)	Connection->Method = M_TRACE;
			else if (method.compare("PATCH") == 0)	Connection->Method = M_PATCH;
			else {
				log_level_printf(1, "Unknown Method or invalid request\n");
				Connection->Response.SendError(HTTP_NOT_IMPLEMENTED);
				// the rest of the request is still on its way in
				Connection->sock->linger_on_close = true;
				log_level_printf(3, "Request: '%s'\n", ylog::startLine(start_line).c_str());
				return false;
			}
			log_level_printf(3, "Request: FullURL: %s\n", ylog::url(UrlData["target"]).c_str());
			return true;
		}
	}
	return false;
}

//-----------------------------------------------------------------------------
// parse parameter string
//       parameter               = attribute "=" value
//       attribute               = token
//       value                   = token | quoted-string
//
// 	If parameter attribute is multiple times given, the values are stored like this:
// 		<attribute>=<value1>,<value2>,..,<value n>
//-----------------------------------------------------------------------------
bool CWebserverRequest::ParseParams(std::string param_string) {
	bool ende = false;
	std::string param, name = "", value = "", number = "";

	while (!ende) {
		if (!ySplitStringExact(param_string, "&", param, param_string))
			ende = true;
		if (ySplitStringExact(param, "=", name, value)) {
			name = decodeString(name);
			value = trim(decodeString(value));
			if (ParameterList[name].empty())
				ParameterList[name] = value;
			else {
				ParameterList[name] += ",";
				ParameterList[name] += value;
			}
		}
		else
			name = trim(decodeString(name));
		number = string_printf("%d", ParameterList.size() + 1);
		// a value by its length only: a query's values stay out of the log
		// whatever they are called, and a form's are no different
		log_level_printf(7, "ParseParams: name: %s value: %s\n", ylog::text(name).c_str(),
				ylog::withheld(value).c_str());
		ParameterList[number] = name;
	}
	return true;
}

//-----------------------------------------------------------------------------
// parse the header of the request
//	from RFC 2616 / 4.2 Message Header:
//	message-header = field-name ":" [ field-value ]
//	field-name     = token
//	field-value    = *( field-content | LWS )
//	field-content  = <the OCTETs making up the field-value
//		and consisting of either *TEXT or combinations
//		of token, separators, and quoted-string>
//-----------------------------------------------------------------------------
bool CWebserverRequest::ParseHeader(std::string header) {
	bool ende = false;
	std::string sheader = "", name = "", value = "";
	if(!HeaderList.empty())
		HeaderList.clear();

	while (!ende) {
		if (!ySplitStringExact(header, "\r\n", sheader, header))
			ende = true;
		if (ySplitStringExact(sheader, ":", name, value))
			HeaderList[name] = trim(value);
		log_level_printf(8, "ParseHeader: name: %s value: %s\n", ylog::text(name).c_str(),
				ylog::headerValue(name, value).c_str());
	}
	return true;
}

//-----------------------------------------------------------------------------
// Analyze URL
// Extract Filename, Path, FileExt
//	form RFC2616 / 3.2.2:
//	http_URL = "http:" "//" host [ ":" port ] [ abs_path [ "?" query ]]
// query data is splitted and stored in ParameterList
//-----------------------------------------------------------------------------
void CWebserverRequest::analyzeURL(std::string url) {
	std::string fullurl = "";
	if(!ParameterList.empty())
		ParameterList.clear();

	// URI decode
	fullurl = decodeString(url);
	fullurl = trim(fullurl, "\r\n"); // non-HTTP-Standard: allow \r or \n in URL. Delete it.
	UrlData["fullurl"] = fullurl;

	// split Params
	if (ySplitString(url, "?", UrlData["url"], UrlData["paramstring"])){ // split pure URL and all Params
		UrlData["url"] = decodeString(UrlData["url"]);
		ParseParams( UrlData["paramstring"]); // split params to ParameterList
	}else{
		// No Params
		UrlData["url"] = fullurl;
	}

	if (!ySplitStringLast(UrlData["url"], "/", UrlData["path"],
			UrlData["filename"])) {
		UrlData["path"] = "/"; // Set "/" if not contained
	} else
		UrlData["path"] += "/";
	if ((UrlData["url"].length() == 1)
			|| (UrlData["url"][UrlData["url"].length() - 1] == '/')) { // if "/" at end use index.html
		UrlData["path"] = UrlData["url"];
		UrlData["filename"] = "index.html";
	}
	ySplitStringLast(UrlData["filename"], ".", UrlData["filenamepure"],
			UrlData["fileext"]);
}

//-----------------------------------------------------------------------------
// How long a body has to arrive once its header is in: on an API address, and
// for a form outside the API alike.
//-----------------------------------------------------------------------------
#define API_BODY_TIMEOUT_MS	10000

//-----------------------------------------------------------------------------
// Handle Post (a form; no upload)
//-----------------------------------------------------------------------------
bool CWebserverRequest::HandlePost() {
	// the whole header, under the head's one deadline and one limit
	std::string raw_header;
	if (!readLegacyHeader(*Connection->sock, raw_header, HEAD_LIMIT - HeadBytes, headTimeLeft()))
		return headRefused(HTTP_BAD_REQUEST);
	if (!headerWellFormed(raw_header))
		return headerRefused();
	ParseHeader(raw_header);

	// The request boundary's question (CAuthorityGate), asked before the body
	// and not only by the hook: the hooks see a request once it has been read,
	// and a form body is in memory by then. Answered with the boundary's own
	// refusal.
	if (!httpd::hostHeaderAccepted(HeaderList)) {
		log_level_printf(1, "HandlePost: Host names another box, body left unread\n");
		CyhookHandler &hh = Connection->HookHandler;
		hh.session_init(ParameterList, UrlData, HeaderList, Cyhttpd::ConfigList, Connection->Method, false);
		if (CAuthorityGate::refuse(&hh) == HANDLED_ERROR) {
			Connection->Response.Write(hh.BuildHeader());
			Connection->Response.Write(hh.yresult);
		}
		// the body is still on its way in
		Connection->sock->linger_on_close = true;
		return false;
	}

	// This server takes no uploads: the web interface sends its files through
	// the API, and nothing here reads a file such a POST would bring. A
	// multipart body of any kind is refused before a byte of it is read.
	if (string_tolower(header_value(HeaderList, "Content-Type")).compare(0, 10, "multipart/") == 0) {
		log_level_printf(1, "HandlePost: multipart body refused unread\n");
		Connection->Response.SendError(HTTP_UNSUPPORTED_MEDIA_TYPE);
		// the body is still on its way in
		Connection->sock->linger_on_close = true;
		return false;
	}

	// read meesage body
	unsigned int content_len = 0;
	const bool form = HeaderList["Content-Type"].compare("application/x-www-form-urlencoded") == 0;
	if (form) {
		// A form is read into memory before any hook looks at the request,
		// so its length is settled first: one number of at most
		// POST_FORM_MAX_LENGTH, read the way an API body's is, so that a
		// negative or second length cannot pass for a short one. A form
		// without one is not read: only the end of the connection could say
		// where it ends.
		long long declared = 0;
		const ApiBody framing = apiContentLength(raw_header, declared);
		HttpResponseType refused = HTTP_OK;
		if (framing == BODY_BAD)
			refused = HTTP_BAD_REQUEST;
		else if (framing == BODY_NONE)
			refused = HTTP_LENGTH_REQUIRED;
		else if (declared > POST_FORM_MAX_LENGTH)
			refused = HTTP_REQUEST_ENTITY_TOO_LARGE;
		if (refused != HTTP_OK) {
			if (framing == BODY_OK)
				log_level_printf(1, "HandlePost: form of %lld bytes refused unread (%d)\n", declared, (int) refused);
			else
				log_level_printf(1, "HandlePost: form refused unread (%d)\n", (int) refused);
			Connection->Response.SendError(refused);
			// the body is still on its way in
			Connection->sock->linger_on_close = true;
			return false;
		}
		content_len = (unsigned int) declared;
	}

	// Get Rest of Request from Socket
	log_level_printf(2, "Connection->Method Post !\n");
	log_level_printf(9, "Conntent Type:%s\n", ylog::headerValue("Content-Type", HeaderList["Content-Type"]).c_str());
	log_level_printf(8, "Post Content-Length:%d as string:(%s)\n", content_len, ylog::headerValue("Content-Length", HeaderList["Content-Length"]).c_str());

	// POST Requests requires CONTINUE in HTTP/1.1: sent once the header has
	// passed the checks above, so a caller that waits for it before the body
	// sends nothing of a body refused there
	Connection->Response.Write("HTTP/1.1 100 Continue\r\n\r\n");

	if (form) //this is a normal POST with form-data (no upload)
	{
		// handle normal form POST
		log_level_printf(6, "Handle POST application/x-www-form-urlencoded\n");

		// The declared length and not a byte more, read as an API body is:
		// within the deadline, so a caller that stops sending, or went away
		// short of what it announced, ends the request instead of holding a
		// thread of the server.
		std::string post_header;
		if (!Connection->sock->ReceiveExact(post_header, content_len, API_BODY_TIMEOUT_MS)) {
			log_level_printf(1, "HandlePost: form of %u bytes did not arrive\n", content_len);
			Connection->Response.SendError(HTTP_REQUEST_TIMEOUT);
			return false;
		}
		// parse the params in post_header (message-body) an add them to ParameterList
		ParseParams(post_header);
	}
	return true;
}
//-----------------------------------------------------------------------------
// API target: header line by line until the empty line, then the body for
// POST and PUT by declared length and nothing else. TRACE stays what it is
// everywhere. A body this server will not frame ends the request with 400;
// one above the ceiling is not read and left for the hook to refuse with
// 413; one that stops arriving ends at the deadline with 408.
//-----------------------------------------------------------------------------
#define API_BODY_LIMIT		(1024 * 1024)
// a form outside the API takes as much as an API body does (yconfig.h)
#if POST_FORM_MAX_LENGTH != API_BODY_LIMIT
#error "POST_FORM_MAX_LENGTH and API_BODY_LIMIT are one ceiling"
#endif
// How much of a refused body is still read and dropped, so the 413 reaches
// the caller instead of a reset. Past this the caller is left with the reset.
#define API_DRAIN_CAP		(16 * 1024 * 1024)

bool CWebserverRequest::HandleApiRequest() {
	RawBody.clear();
	RawBodyTooLarge = false;

	if (Connection->Method == M_TRACE) {
		Connection->Response.SendError(HTTP_NOT_IMPLEMENTED);
		Connection->sock->linger_on_close = true;
		return false;
	}

	std::string raw_header;
	// within what the request line left of the head's deadline and limit
	if (!readApiHeader(*Connection->sock, raw_header, HEAD_LIMIT - HeadBytes, headTimeLeft()))
		return headRefused(HTTP_BAD_REQUEST);
	if (!headerWellFormed(raw_header))
		return headerRefused();
	ParseHeader(raw_header);

	long long length = 0;
	switch (apiBodyDecision(Connection->Method, raw_header, length)) {
	case API_BODY_BAD:
		Connection->Response.SendError(HTTP_BAD_REQUEST);
		// the body this refuses to frame is still on its way in
		Connection->sock->linger_on_close = true;
		Connection->RequestCanceled = true;
		return false;
	case API_BODY_NONE:
		return true;
	case API_BODY_TOO_LARGE:
		// answered 413 by the hook; drained first, because a close with the
		// body still unread resets the connection and takes the answer with it
		RawBodyTooLarge = true;
		Connection->sock->Discard((size_t)(length > API_DRAIN_CAP ? API_DRAIN_CAP : length), API_BODY_TIMEOUT_MS);
		// and past the drain cap, what is left goes at the close
		Connection->sock->linger_on_close = true;
		return true;
	case API_BODY_READ:
		break;
	}

	std::string expect = string_tolower(header_value(HeaderList, "Expect"));
	if (expect.find("100-continue") != std::string::npos)
		Connection->Response.Write("HTTP/1.1 100 Continue\r\n\r\n");

	if (!Connection->sock->ReceiveExact(RawBody, (size_t) length, API_BODY_TIMEOUT_MS)) {
		log_level_printf(1, "HandleApiRequest: body of %lld bytes did not arrive\n", length);
		Connection->Response.SendError(HTTP_REQUEST_TIMEOUT);
		Connection->RequestCanceled = true;
		return false;
	}
	return true;
}

//-----------------------------------------------------------------------------
// The body framing a raw header declares. Read from the raw text and not
// from HeaderList, which keeps one value per name: a second Content-Length
// with a different value has to be seen to be refused.
//-----------------------------------------------------------------------------
CWebserverRequest::ApiBody CWebserverRequest::apiContentLength(const std::string &raw_header, long long &length) {
	length = 0;
	bool seen = false;
	long long value = 0;
	size_t at = 0;
	while (at < raw_header.size()) {
		size_t eol = raw_header.find("\r\n", at);
		std::string line = raw_header.substr(at, eol == std::string::npos ? std::string::npos : eol - at);
		at = (eol == std::string::npos) ? raw_header.size() : eol + 2;
		size_t colon = line.find(':');
		if (colon == std::string::npos)
			continue;
		std::string name = string_tolower(trim(line.substr(0, colon)));
		std::string text = trim(line.substr(colon + 1));
		if (name == "transfer-encoding")
			return BODY_BAD;
		if (name != "content-length")
			continue;
		if (text.empty() || text.size() > 18)
			return BODY_BAD;
		for (size_t i = 0; i < text.size(); i++)
			if (text[i] < '0' || text[i] > '9')
				return BODY_BAD;
		// A second length is refused even where it repeats the first: a
		// reader in front of this server may frame the message differently,
		// and one length is all a request needs.
		if (seen)
			return BODY_BAD;
		seen = true;
		value = strtoll(text.c_str(), NULL, 10);
	}
	if (!seen)
		return BODY_NONE;
	length = value;
	return BODY_OK;
}

//-----------------------------------------------------------------------------
// The header off the socket: everything up to the empty line and nothing
// past it, bounded in bytes and in time. EOF before the empty line is a
// refusal; the line reader would otherwise hand back a byte that was never
// received, again and again.
//-----------------------------------------------------------------------------
bool CWebserverRequest::readApiHeader(CySocket &sock, std::string &raw_header, size_t limit, int timeout_ms) {
	// Line by line, because the request line already took its own CRLF: a
	// request without header lines has nothing left but the empty line, and
	// a search for a blank line's CRLF pair would sit out the deadline on
	// it. One deadline and one limit for all the lines together.
	raw_header.clear();
	struct timespec t0;
	clock_gettime(CLOCK_MONOTONIC, &t0);
	for (;;) {
		struct timespec now;
		clock_gettime(CLOCK_MONOTONIC, &now);
		const long long spent = (long long) (now.tv_sec - t0.tv_sec) * 1000 +
		                        (now.tv_nsec - t0.tv_nsec) / 1000000;
		if (spent >= timeout_ms)
			return false;
		std::string line;
		if (!sock.ReceiveUntil("\r\n", line, limit - raw_header.size(), (int) (timeout_ms - spent)))
			return false;
		// the reader's contract is "each line, the empty one last", which is
		// what the line-by-line legacy path produced
		raw_header += line;
		if (line == "\r\n")
			return true;
	}
}

//-----------------------------------------------------------------------------
// The header of a request to the old interface, the same way: one limit and one
// deadline for all its lines, nothing past the empty line. A line may also end
// in a LF alone, as the old interface always took it, and is kept with the
// CRLF the header's reader splits on.
//-----------------------------------------------------------------------------
bool CWebserverRequest::readLegacyHeader(CySocket &sock, std::string &raw_header, size_t limit, int timeout_ms) {
	raw_header.clear();
	const long long started = headClockMs();
	// the bytes as they came off the wire, before a LF alone is made a CRLF
	size_t taken = 0;
	for (;;) {
		const long long left = (long long) timeout_ms - (headClockMs() - started);
		if (left <= 0 || taken >= limit)
			return false;
		std::string line;
		if (!sock.ReceiveUntil("\n", line, limit - taken, (int) left))
			return false;
		taken += line.size();
		if (line.size() < 2 || line[line.size() - 2] != '\r')
			line.insert(line.size() - 1, "\r");
		raw_header += line;
		if (line == "\r\n")
			return true;
	}
}

//-----------------------------------------------------------------------------
// The decision about a body, per method. GET, HEAD and DELETE carry none
// here: one declared on them is refused rather than read or skipped, since
// a body left on the socket is the next thing somebody reads.
//-----------------------------------------------------------------------------
CWebserverRequest::ApiBodyDecision CWebserverRequest::apiBodyDecision(THttp_Method method, const std::string &raw_header, long long &length) {
	length = 0;
	long long declared = 0;
	const ApiBody framing = apiContentLength(raw_header, declared);
	if (framing == BODY_BAD)
		return API_BODY_BAD;
	const bool takes_body = (method == M_POST || method == M_PUT || method == M_PATCH);
	if (framing == BODY_NONE || declared == 0)
		return API_BODY_NONE;
	if (!takes_body)
		return API_BODY_BAD;
	if (declared > API_BODY_LIMIT) {
		length = declared;
		return API_BODY_TOO_LARGE;
	}
	length = declared;
	return API_BODY_READ;
}
