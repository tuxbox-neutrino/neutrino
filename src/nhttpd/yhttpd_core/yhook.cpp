//=============================================================================
// YHTTPD
// Hook and HookHandler
//=============================================================================
// C
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <strings.h>
#include <unistd.h>

// UTF8 convert
#include <zapit/zapit.h>
#include <system/helpers.h>

// yhttpd
#include "yhook.h"
#include "helper.h"

//=============================================================================
// Constructor & Ceconstructor
//=============================================================================
CyhookHandler::CyhookHandler()
{
	ContentLength = 0;
	RangeStart = 0;
	RangeEnd = -1;
	SendFd = -1;
	SendFdOffset = 0;
	SendFdLength = 0;
	LengthUnknown = false;
	cached = false;
	keep_alive = 0;
	_outIndent = 0;
	status = HANDLED_NONE;
	Method = M_UNKNOWN;
	httpStatus = HTTP_NIL;
	outType = plain;
	outTypeForced = false;
	nonPair = false;
	LastModified=0;
	// the request reader answers before session_init() runs; these must
	// read as the legacy defaults then
	api_mode = false;
	RawBodyTooLarge = false;
}

CyhookHandler::~CyhookHandler()
{
}

//=============================================================================
// Initialization of static variables
//=============================================================================
THookList CyhookHandler::HookList;

//=============================================================================
// Hook Handling
//=============================================================================
//-----------------------------------------------------------------------------
// Hook Dispatcher for Session Hooks
// Execute every Hook in HookList until State change != HANDLED_NONE
//-----------------------------------------------------------------------------
THandleStatus CyhookHandler::Hooks_SendResponse() {
	log_level_printf(4, "Response Hook-List Start\n");
	THandleStatus _status = HANDLED_NONE;
	THookList::iterator i = HookList.begin();
	for (; i != HookList.end(); ++i) {
		log_level_printf(4, "Response Hook-List (%s) Start\n",
				((*i)->getHookName()).c_str());
		// response Hook
		_status = (*i)->Hook_SendResponse(this);
		log_level_printf(4, "Response Hook-List (%s) End. Status (%d)\n",
				((*i)->getHookName()).c_str(), status);
		if ((_status != HANDLED_NONE) && (_status != HANDLED_CONTINUE))
			break;
	}
	log_level_printf(4, "Response Hook-List End\n");
	log_level_printf(8, "Response Hook-List Result:\n%s\n", yresult.c_str());
	status = _status;
	return _status;
}
//-----------------------------------------------------------------------------
// Hook Dispatcher for Session Hooks
// Execute every Hook in HookList until State change != HANDLED_NONE
//-----------------------------------------------------------------------------
THandleStatus CyhookHandler::Hooks_PrepareResponse() {
	log_level_printf(4, "PrepareResponse Hook-List Start\n");
	outType = getOutType();
	THandleStatus _status = HANDLED_NONE;
	THookList::iterator i = HookList.begin();
	for (; i != HookList.end(); ++i) {
		log_level_printf(4, "PrepareResponse Hook-List (%s) Start\n",
				((*i)->getHookName()).c_str());
		// response Hook
		_status = (*i)->Hook_PrepareResponse(this);
		log_level_printf(
				4,
				"PrepareResponse Hook-List (%s) End. Status (%d) HTTP Status (%d)\n",
				((*i)->getHookName()).c_str(), status, httpStatus);
		if ((_status != HANDLED_NONE) && (_status != HANDLED_CONTINUE))
			break;
	}
	log_level_printf(4, "PrepareResponse Hook-List End\n");
	log_level_printf(8, "PrepareResponse Hook-List Result:\n%s\n",
			yresult.c_str());
	status = _status;
	return _status;
}

//-----------------------------------------------------------------------------
// Hook Dispatcher for Server based Hooks
// Execute every Hook in HookList until State change != HANDLED_NONE and
// != HANDLED_CONTINUE
//-----------------------------------------------------------------------------
THandleStatus CyhookHandler::Hooks_ReadConfig(CConfigFile *Config,
		CStringList &ConfigList) {
	log_level_printf(4, "ReadConfig Hook-List Start\n");
	THandleStatus _status = HANDLED_NONE;
	THookList::iterator i = HookList.begin();
	for (; i != HookList.end(); ++i) {
		//		log_level_printf(4,"ReadConfig Hook-List (%s)  Start\n", ((*i)->getHookName()).c_str());
		// response Hook
		_status = (*i)->Hook_ReadConfig(Config, ConfigList);
		log_level_printf(4, "ReadConfig Hook-List (%s) Status (%d)\n",
				((*i)->getHookName()).c_str(), _status);
		if ((_status != HANDLED_NONE) && (_status != HANDLED_CONTINUE))
			break;
	}
	log_level_printf(4, "ReadConfig Hook-List End\n");
	return _status;
}
//-----------------------------------------------------------------------------
// Hook Dispatcher for EndConnection
//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
// The stream pass. Same walk as the response pass: the first hook that
// answers something other than NONE/CONTINUE owns the exchange.
//-----------------------------------------------------------------------------
THandleStatus CyhookHandler::Hooks_SendStream(CySocket *sock) {
	log_level_printf(4, "Stream Hook-List Start\n");
	THandleStatus _status = HANDLED_NONE;
	THookList::iterator i = HookList.begin();
	for (; i != HookList.end(); ++i) {
		_status = (*i)->Hook_SendStream(this, sock);
		if ((_status != HANDLED_NONE) && (_status != HANDLED_CONTINUE))
			break;
	}
	log_level_printf(4, "Stream Hook-List End. Status (%d)\n", _status);
	status = _status;
	return _status;
}

THandleStatus CyhookHandler::Hooks_EndConnection() {
	log_level_printf(4, "EndConnection Hook-List Start\n");
	THandleStatus _status = HANDLED_NONE;
	THookList::iterator i = HookList.begin();
	for (; i != HookList.end(); ++i) {
		log_level_printf(4, "EndConnection Hook-List (%s) Start\n",
				((*i)->getHookName()).c_str());
		// response Hook
		_status = (*i)->Hook_EndConnection(this);
		log_level_printf(4, "EndConnection Hook-List (%s) End. Status (%d)\n",
				((*i)->getHookName()).c_str(), _status);
		if ((_status != HANDLED_NONE) && (_status != HANDLED_CONTINUE))
			break;
	}
	log_level_printf(4, "EndConnection Hook-List End\n");
	status = _status;
	return _status;
}
//-----------------------------------------------------------------------------
// Hook Dispatcher for UploadSetFilename
//-----------------------------------------------------------------------------
THandleStatus CyhookHandler::Hooks_UploadSetFilename(std::string &Filename) {
	log_level_printf(4, "UploadSetFilename Hook-List Start. Filename:(%s)\n",
			Filename.c_str());
	THandleStatus _status = HANDLED_NONE;
	THookList::iterator i = HookList.begin();
	for (; i != HookList.end(); ++i) {
		log_level_printf(4, "UploadSetFilename Hook-List (%s) Start\n",
				((*i)->getHookName()).c_str());
		// response Hook
		_status = (*i)->Hook_UploadSetFilename(this, Filename);
		log_level_printf(4,
				"UploadSetFilename Hook-List (%s) End. Status (%d)\n",
				((*i)->getHookName()).c_str(), _status);
		if ((_status != HANDLED_NONE) && (_status != HANDLED_CONTINUE))
			break;
	}
	log_level_printf(4, "UploadSetFilename Hook-List End\n");
	status = _status;
	return _status;
}
//-----------------------------------------------------------------------------
// Hook Dispatcher for UploadSetFilename
//-----------------------------------------------------------------------------
THandleStatus CyhookHandler::Hooks_UploadReady(const std::string& Filename) {
	log_level_printf(4, "UploadReady Hook-List Start. Filename:(%s)\n",
			Filename.c_str());
	THandleStatus _status = HANDLED_NONE;
	THookList::iterator i = HookList.begin();
	for (; i != HookList.end(); ++i) {
		log_level_printf(4, "UploadReady Hook-List (%s) Start\n",
				((*i)->getHookName()).c_str());
		// response Hook
		_status = (*i)->Hook_UploadReady(this, Filename);
		log_level_printf(4, "UploadReady Hook-List (%s) End. Status (%d)\n",
				((*i)->getHookName()).c_str(), _status);
		if ((_status != HANDLED_NONE) && (_status != HANDLED_CONTINUE))
			break;
	}
	log_level_printf(4, "UploadReady Hook-List End\n");
	status = _status;
	return _status;
}

//=============================================================================
// Output helpers
//=============================================================================
void CyhookHandler::session_init(CStringList _ParamList, CStringList _UrlData,
		CStringList _HeaderList, CStringList& _ConfigList,
		THttp_Method _Method, bool _keep_alive) {
	ParamList = _ParamList;
	UrlData = _UrlData;
	HeaderList = _HeaderList;
	WebserverConfigList = _ConfigList;
	Method = _Method;
	RawBody.clear();
	RawBodyTooLarge = false;
	api_mode = false;
	// a descriptor a broken-off exchange left behind
	if (SendFd >= 0)
		close(SendFd);
	SendFd = -1;
	SendFdOffset = 0;
	SendFdLength = 0;
	LengthUnknown = false;
	ReloadAfter.clear();
	ResponseHeaders.clear();
	status = HANDLED_NONE;
	httpStatus = HTTP_OK;
	ContentLength = 0;
	LastModified = (time_t) - 1;
	keep_alive = _keep_alive;
	outType = plain;
	outTypeForced = false;
	HookVarList.clear();
}

//=============================================================================
// Build Header
//-----------------------------------------------------------------------------
// RFC 2616 / 6 Response (Header)
//
//   The first line of a Response message is the Status-Line, consisting
//   of the protocol version followed by a numeric status code and its
//   associated textual phrase, with each element separated by SP
//   characters. No CR or LF is allowed except in the final CRLF sequence.
//
//	Response       =Status-Line		; generated by SendHeader
//			*(( general-header
//			 | response-header
//			 | entity-header ) CRLF)
//			 CRLF			; generated by SendHeader
//			 [ message-body ]	; by HOOK Handling Loop or Sendfile
//
//	Status-Line = HTTP-Version SP Status-Code SP Reason-Phrase CRLF ; generated by SendHeader
//
//	general-header = Cache-Control             ; not implemented
//			| Connection               ; implemented
//			| Date                     ; implemented
//			| Pragma                   ; not implemented
//			| Trailer                  ; not implemented
//			| Transfer-Encoding        ; not implemented
//			| Upgrade                  ; not implemented
//			| Via                      ; not implemented
//			| Warning                  ; not implemented
//
//	response-header = Accept-Ranges          ; not implemented
//			| Age                     ; not implemented
//			| ETag                    ; not implemented
//			| Location                ; implemented (redirection / Object moved)
//			| Proxy-Authenticate      ; not implemented
//			| Retry-After             ; not implemented
//			| Server                  ; implemented
//			| Vary                    ; not implemented
//			| WWW-Authenticate        ; implemented (by mod_auth and SendHeader)
//
//	entity-header  = Allow                    ; not implemented
//			| Content-Encoding         ; not implemented
//			| Content-Language         ; not implemented
//			| Content-Length           ; implemented
//			| Content-Location         ; not implemented
//			| Content-MD5              ; not implemented
//			| Content-Range            ; not implemented
//			| Content-Type             ; implemented
//			| Expires                  ; not implemented
//			| Last-Modified            ; implemented for static files
//			| extension-header
//
//				extension-header = message-header
//=============================================================================
std::string CyhookHandler::BuildHeader(bool cache) {
	if (api_mode)
		return BuildApiHeader();

	std::string result = "";

	const char *responseString = "";
	//const char *infoString = 0;

	// get Info Index
	for (unsigned int i = 0; i < (sizeof(httpResponseNames)
			/ sizeof(httpResponseNames[0])); i++)
		if (httpResponseNames[i].type == httpStatus) {
			responseString = httpResponseNames[i].name;
			//infoString = httpResponseNames[i].info;
			break;
		}
	// print Status-line
	result = string_printf(HTTP_PROTOCOL " %d %s\r\nContent-Type: %s\r\n", httpStatus, responseString, ResponseMimeType.c_str());
	log_level_printf(2, "Response: HTTP/1.1 %d %s\r\nContent-Type: %s\r\n", httpStatus, responseString, ResponseMimeType.c_str());

	// 401 must still run through the default arm below: it carries the
	// header terminator, Connection and Content-Length — without them the
	// response is malformed and any 401 body is undefined behaviour
	if (httpStatus == HTTP_UNAUTHORIZED) {
		result += "WWW-Authenticate: Basic realm=\"";
		result += AUTH_NAME_MSG "\"\r\n";
	}

	switch (httpStatus) {
	case HTTP_MOVED_TEMPORARILY:
	case HTTP_MOVED_PERMANENTLY:
		// Status HTTP_*_TEMPORARILY (redirection)
		result += string_printf("Location: %s\r\n", NewURL.c_str());
		// fall through

	default:
		time_t timer = time(0);
		char timeStr[80];
		// cache
		if (!cache && (HookVarList["CacheCategory"]).empty())
			result += "Cache-Control: no-cache\r\n";
		else {
			/* gmtime_r and not gmtime throughout: every connection has a
			   thread of its own, and gmtime hands all of them the same
			   static struct. A day after now, counted in seconds: mktime
			   would read the GMT fields as local time and move the date
			   by the zone's offset. */
			time_t x_time = time(NULL) + 24 * 60 * 60;
			struct tm tm_buf;
			strftime(timeStr, sizeof(timeStr), RFC1123FMT, gmtime_r(&x_time, &tm_buf));
			result += string_printf("Expires: %s\r\n", timeStr);
		}
		result += "Server: " WEBSERVERNAME "\r\n";
		// actual date
		struct tm now_tm;
		strftime(timeStr, sizeof(timeStr), RFC1123FMT, gmtime_r(&timer, &now_tm));
		result += string_printf("Date: %s\r\n", timeStr);
		// connection type
#ifdef Y_CONFIG_FEATURE_KEEP_ALIVE
		if(keep_alive)
			result += "Connection: keep-alive\r\n";
		else
#endif
			result += "Connection: close\r\n";
		// gzipped ?
		if (UrlData["fileext"] == "gz")
			result += "Content-Encoding: gzip\r\n";
		// content-len, last-modified
		// HTTP_NOT_FOUND may carry a body (e.g. /api/v4/ JSON error objects);
		// announce its real length so clients do not discard the payload.
		if (httpStatus == HTTP_NOT_MODIFIED || httpStatus == HTTP_REQUEST_RANGE_NOT_SATISFIABLE
				|| (httpStatus == HTTP_NOT_FOUND && GetContentLength() == 0)
				|| (httpStatus == HTTP_UNAUTHORIZED && GetContentLength() == 0)) {
			result += "Content-Length: 0\r\n";
			// a refused range says how long the file is, for a range that fits
			if (httpStatus == HTTP_REQUEST_RANGE_NOT_SATISFIABLE)
				result += string_printf("Content-Range: bytes */%lld\r\n", ContentLength);
		}
		else if (GetContentLength() > 0) {
			time_t mod_time = time(NULL);
			if (LastModified != (time_t) - 1)
				mod_time = LastModified;

			struct tm mod_tm;
			strftime(timeStr, sizeof(timeStr), RFC1123FMT, gmtime_r(&mod_time, &mod_tm));
			result += string_printf("Last-Modified: %s\r\n", timeStr);
			if (status == HANDLED_SENDFILE && !cached) {
				result += string_printf("Accept-Ranges: bytes\r\n");
				result += string_printf("Content-Length: %lld\r\n", RangeEnd - RangeStart + 1);
				if (httpStatus == HTTP_PARTIAL_CONTENT)
					result += string_printf("Content-Range: bytes %lld-%lld/%lld\r\n", RangeStart, RangeEnd, ContentLength);
			} else
				result += string_printf("Content-Length: %lld\r\n", GetContentLength());
		}
		// hook-set cookie; cleared after emission because BuildHeader can
		// run more than once per response (see the do/while in yresponse)
		if (!(HookVarList["Set-Cookie"]).empty()) {
			result += "Set-Cookie: " + HookVarList["Set-Cookie"] + "\r\n";
			HookVarList["Set-Cookie"] = "";
		}
		result += "\r\n"; // End of Header
		break;
	}

	// Body
	if (Method != M_HEAD)
		switch (httpStatus) {
		case HTTP_OK:
		case HTTP_NOT_MODIFIED:
		case HTTP_CONTINUE:
		case HTTP_ACCEPTED:
		case HTTP_NO_CONTENT:
		case HTTP_NOT_FOUND:
		case HTTP_PARTIAL_CONTENT:
		case HTTP_REQUEST_RANGE_NOT_SATISFIABLE:
		case HTTP_INTERNAL_SERVER_ERROR:
			break;

		case HTTP_MOVED_TEMPORARILY:
		case HTTP_MOVED_PERMANENTLY:
			result += "<html><head><title>Object moved</title></head><body>";
			result
					+= string_printf(
							"302 : Object moved.<br/>If you dont get redirected click <a href=\"%s\">here</a></body></html>\n",
							NewURL.c_str());
			break;

		default:
			// Error pages
			break;
		}
	return result;
}

bool CyhookHandler::ParamList_exist(std::string keyword)
{
	bool exist = false;
	unsigned int s = ParamList.size();
	for (unsigned int i = 1; i <= s; i++)
	{
		exist = (ParamList[itoa(i)] == keyword);
		if (exist)
			break;
	}
	return exist;
}

//=============================================================================
// Output helpers
//=============================================================================
//-----------------------------------------------------------------------------
void CyhookHandler::SendHTMLHeader(const std::string& Titel) {
	WriteLn("<html>\n<head><title>" + Titel + "</title>\n");
	WriteLn("<meta http-equiv=\"cache-control\" content=\"no-cache\" />");
	WriteLn("<meta http-equiv=\"expires\" content=\"0\" />\n</head>\n<body>\n");
}
//-----------------------------------------------------------------------------
void CyhookHandler::SendHTMLFooter(void) {
	WriteLn("</body>\n</html>\n\n");
}

//-----------------------------------------------------------------------------
#define OUTBUFSIZE 4096
void CyhookHandler::printf(const char *fmt, ...) {
	char outbuf[OUTBUFSIZE];
	memset(outbuf,0, OUTBUFSIZE);
	va_list arglist;
	va_start(arglist, fmt);
	vsnprintf(outbuf, OUTBUFSIZE, fmt, arglist);
	va_end(arglist);
	Write(outbuf);
}
//-----------------------------------------------------------------------------
TOutType CyhookHandler::getOutType() {
	// api v4 presets the output type without touching ParamList, so
	// parameter-driven detection must not override it
	if (outTypeForced)
		return outType;
	TOutType _outType = plain;
	if(!(ParamList.empty())) {
		if ((ParamList.find("format") != ParamList.end() && ParamList["format"] == "json")
				|| (ParamList.find("json") != ParamList.end() && !(ParamList["json"].empty())) )
			_outType = json;
		else if ((ParamList.find("format") != ParamList.end() && ParamList["format"] == "xml")
				|| (ParamList.find("xml") != ParamList.end() && !(ParamList["xml"].empty())) )
			_outType = xml;
	}
	return _outType;
}
//-----------------------------------------------------------------------------
TOutType CyhookHandler::outStart(bool single) {
	// for compatibility
	nonPair = single;
	// get outType
	outType = getOutType();
	// set response header
	if (outType == xml)
		SetHeader(HTTP_OK, "text/xml; charset=UTF-8");
	else
		SetHeader(HTTP_OK, "text/plain; charset=UTF-8");

	return outType;
}

//-----------------------------------------------------------------------------
std::string CyhookHandler::outIndent() {
	return "";
}

//-----------------------------------------------------------------------------
std::string CyhookHandler::outSingle(std::string _content) {
	return _content + "\n";
}

//-----------------------------------------------------------------------------
std::string CyhookHandler::outPair(std::string _key, std::string _content, bool _next) {
	std::string result = "", _key_close = "", tmp;
	ySplitString(_key, " ", _key_close, tmp);
	switch (outType) {
	case xml:
		result = outIndent() + "<" + _key + ">" + _content + "</" + _key_close + ">";
		break;
	case json:
		/* The value is escaped here and only here. outValue() passes json
		   through untouched, so a caller that used it does not get escaped
		   twice: the quote escape lost its backslash partner and the whole
		   document stopped parsing. */
		result = outIndent() + "\"" + _key + "\": \"" + json_convert_string(_content) + "\"";
		if(_next)
			result += ",";
		break;
	default:
		if (nonPair)
			result = _content;
		else
			result = _key + "=" + _content;
		break;
	}
	return result + "\n";
}

//-----------------------------------------------------------------------------
std::string CyhookHandler::outArray(std::string _key, std::string _content, bool _next) {
	std::string result = "", _key_close = "", tmp;
	ySplitString(_key, " ", _key_close, tmp);
	switch (outType) {
	case xml:
		//TODO: xml check and DESC check
		result = outIndent() + "<" + _key + ">\n" + _content + "</" + _key_close + ">";
		result += "\n";
		break;
	case json:
		//TODO: json check
		result = outIndent() + "\"" + _key + "\": [" + _content + "]";
		if(_next)
			result += ",";
		result += "\n";
		break;
	default:
		result = _content;
		break;
	}
	return result;
}

//-----------------------------------------------------------------------------
std::string CyhookHandler::outArrayItem(std::string _key, std::string _content, bool _next) {
	std::string result = "", _key_close = "", tmp;
	ySplitString(_key, " ", _key_close, tmp);
	switch (outType) {
	case xml:
		//TODO: xml check and DESC check
		result = outIndent() + "<" + _key + ">\n" + _content + "</" + _key_close + ">";
		result += "\n";
		break;
	case json:
		//TODO: json check
		result = outIndent() + "{" + _content + "}";
		if(_next)
			result += ",";
		result += "\n";
		break;
	default:
		result = _content;
		break;
	}
	return result;
}
//-----------------------------------------------------------------------------
std::string CyhookHandler::outObject(std::string _key, std::string _content, bool _next) {
	std::string result = "", _key_close = "", tmp;
	ySplitString(_key, " ", _key_close, tmp);
	switch (outType) {
	case xml:
		//TODO: xml check and DESC check
		result = outIndent() + "<" + _key + ">\n" + _content + "</" + _key_close + ">";
		result += "\n";
		break;
	case json:
		//TODO: json check
		result = outIndent() + "\"" + _key + "\": {" + _content + "}";
		if(_next)
			result += ",";
		result += "\n";
		break;
	default:
		result = _content;
		break;
	}
	return result;
}

//-----------------------------------------------------------------------------
std::string CyhookHandler::outValue(std::string _content, bool _xml_cdata) {
	std::string result = "";
	switch (outType) {
	case xml:
		/* A CDATA section cannot carry "]]>" - fall back to entities for it. */
		if (_xml_cdata && _content.find("]]>") == std::string::npos)
		{
			result = "<![CDATA[" + _content + "]]>";
		}
		else
		{
			result = convert_UTF8_To_UTF8_XML(utf8_check_is_valid(_content) ? _content.c_str() : iso_8859_1_to_utf8(_content).c_str());
		}
		break;
	case json:
		/* Escaping happens in outPair(), so that callers with and without
		   outValue() end up with exactly one pass. */
		result = _content;
		break;

	default:
		result = _content;
		break;
	}
	return result;
}

std::string CyhookHandler::outNext() {
	if(outType == json)
		return ",";
	else
		return "";
}

//-----------------------------------------------------------------------------
void CyhookHandler::SendOk() {
	std::string result = "";
	switch (outType) {
	case xml:
		result = "<success>true</success>";
		break;
	case json:
		result = "{\"success\": \"true\"}";
		break;
	default:
		result = "ok";
		break;
	}
	Write(result);
}
//-----------------------------------------------------------------------------
void CyhookHandler::SendError(std::string error) {
	std::string result = "";
	switch (outType) {
	case xml:
		if (error.empty())
			result = "<success>false</success>";
		else
			result = "<success>false<error>" + error + "</error></success>";
		break;
	case json:
		if (error.empty())
			result = "{\"success\": \"false\"}";
		else
			result = "{\"success\": \"false\", \"error\":{\"msg\": \"" + error + "\"}}";
		break;
	default:
		if (error.empty())
			result = "error";
		else
			result = "error=" + error;
		break;
	}
	Write(result);
}
//-----------------------------------------------------------------------------
void CyhookHandler::SendResult(std::string _content) {
	std::string result = "";
	switch (outType) {
	case xml:
		result = _content;
		break;
	case json:
		result = json_out_success(_content);
		break;
	default:
		result = _content;
		break;
	}
	WriteLn(result);
}

//=============================================================================
// API namespace
//=============================================================================
std::vector<std::string> CyhookHandler::ApiPrefixes;
std::vector<std::string> CyhookHandler::ApiTargets;

void CyhookHandler::registerApiPrefix(const std::string &prefix) {
	if (prefix.empty() || prefix[0] != '/')
		return;
	for (size_t i = 0; i < ApiPrefixes.size(); i++)
		if (ApiPrefixes[i] == prefix)
			return;
	ApiPrefixes.push_back(prefix);
}

// One exact target and nothing below it: a document, not a directory. What
// lies under its name stays legacy ground, with the legacy authentication.
void CyhookHandler::registerApiTarget(const std::string &target) {
	if (target.empty() || target[0] != '/')
		return;
	for (size_t i = 0; i < ApiTargets.size(); i++)
		if (ApiTargets[i] == target)
			return;
	ApiTargets.push_back(target);
}

void CyhookHandler::clearApiPrefixes() {
	ApiPrefixes.clear();
	ApiTargets.clear();
}

bool CyhookHandler::isApiTarget(const std::string &raw_target) {
	const std::string path = raw_target.substr(0, raw_target.find('?'));
	for (size_t i = 0; i < ApiTargets.size(); i++)
		if (path == ApiTargets[i])
			return true;
	for (size_t i = 0; i < ApiPrefixes.size(); i++) {
		const std::string &p = ApiPrefixes[i];
		if (path == p)
			return true;
		if (path.size() > p.size() && path.compare(0, p.size(), p) == 0 && path[p.size()] == '/')
			return true;
	}
	return false;
}

//=============================================================================
// Header for an API answer: status line, the hook's own headers, the true
// length. What the legacy header decides from the URL or from the status
// (gzip by file extension, no-cache by default, a page for a redirect, a
// zero length for 404) is not decided here; the hook said what it wants.
//=============================================================================
std::string CyhookHandler::BuildApiHeader() {
	const char *responseString = "";
	for (unsigned int i = 0; i < (sizeof(httpResponseNames) / sizeof(httpResponseNames[0])); i++)
		if (httpResponseNames[i].type == httpStatus) {
			responseString = httpResponseNames[i].name;
			break;
		}

	std::string result = string_printf(HTTP_PROTOCOL " %d %s\r\n", httpStatus, responseString);
	log_level_printf(2, "Response (api): %d %s\n", httpStatus, responseString);

	if (!ResponseMimeType.empty())
		result += "Content-Type: " + ResponseMimeType + "\r\n";

	bool has_auth = false, has_location = false;
	for (size_t i = 0; i < ResponseHeaders.size(); i++) {
		if (strcasecmp(ResponseHeaders[i].first.c_str(), "WWW-Authenticate") == 0)
			has_auth = true;
		if (strcasecmp(ResponseHeaders[i].first.c_str(), "Location") == 0)
			has_location = true;
	}
	if (httpStatus == HTTP_UNAUTHORIZED && !has_auth)
		result += "WWW-Authenticate: Basic realm=\"" AUTH_NAME_MSG "\"\r\n";
	if ((httpStatus == HTTP_MOVED_PERMANENTLY || httpStatus == HTTP_MOVED_TEMPORARILY
			|| httpStatus == HTTP_TEMPORARY_REDIRECT) && !has_location && !NewURL.empty())
		result += "Location: " + NewURL + "\r\n";

	char timeStr[80];
	time_t timer = time(0);
	struct tm now_tm;
	strftime(timeStr, sizeof(timeStr), RFC1123FMT, gmtime_r(&timer, &now_tm));
	result += "Server: " WEBSERVERNAME "\r\n";
	result += string_printf("Date: %s\r\n", timeStr);
	result += "Connection: close\r\n";

	for (size_t i = 0; i < ResponseHeaders.size(); i++) {
		const char *name = ResponseHeaders[i].first.c_str();
		// the transport owns these
		if (strcasecmp(name, "Content-Length") == 0 || strcasecmp(name, "Content-Type") == 0
				|| strcasecmp(name, "Connection") == 0 || strcasecmp(name, "Transfer-Encoding") == 0)
			continue;
		if (ResponseHeaders[i].first.empty())
			continue;
		result += ResponseHeaders[i].first + ": " + ResponseHeaders[i].second + "\r\n";
	}
	// HEAD included: the length is that of the answer a GET would carry.
	// An answer that is a file states the stretch of it the transport sends.
	// A stream has no length at all: the body runs until the connection
	// closes, which Connection: close above already promises, and a HEAD
	// for one states none either (RFC 9110 8.6). A 204 has no content and
	// states no length (RFC 9110 15.3.5).
	if (status != HANDLED_STREAM && !LengthUnknown && httpStatus != HTTP_NO_CONTENT)
		result += string_printf("Content-Length: %lld\r\n",
				(SendFd >= 0) ? (long long) SendFdLength : (long long) yresult.length());
	result += "\r\n";
	return result;
}
