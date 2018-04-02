//=============================================================================
// YHTTPD
// Hook and HookHandler
//=============================================================================
// C
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <strings.h>

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
	cached = false;
	keep_alive = 0;
	_outIndent = 0;
	status = HANDLED_NONE;
	Method = M_UNKNOWN;
	httpStatus = HTTP_NIL;
	outType = plain;
	nonPair = false;
	LastModified=0;
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
	status = HANDLED_NONE;
	httpStatus = HTTP_OK;
	ContentLength = 0;
	LastModified = (time_t) - 1;
	keep_alive = _keep_alive;
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

	switch (httpStatus) {
	case HTTP_UNAUTHORIZED:
		result += "WWW-Authenticate: Basic realm=\"";
		result += AUTH_NAME_MSG "\r\n";
		break;

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
			time_t x_time = time(NULL);
			struct tm *ptm = gmtime(&x_time);
			ptm->tm_mday += 1;
			x_time = mktime(ptm);
			strftime(timeStr, sizeof(timeStr), RFC1123FMT, gmtime(&x_time));
			result += string_printf("Expires: %s\r\n", timeStr);
		}
		result += "Server: " WEBSERVERNAME "\r\n";
		// actual date
		strftime(timeStr, sizeof(timeStr), RFC1123FMT, gmtime(&timer));
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
		if (httpStatus == HTTP_NOT_MODIFIED || httpStatus == HTTP_NOT_FOUND || httpStatus == HTTP_REQUEST_RANGE_NOT_SATISFIABLE)
			result += "Content-Length: 0\r\n";
		else if (GetContentLength() > 0) {
			time_t mod_time = time(NULL);
			if (LastModified != (time_t) - 1)
				mod_time = LastModified;

			strftime(timeStr, sizeof(timeStr), RFC1123FMT, gmtime(&mod_time));
			result += string_printf("Last-Modified: %s\r\n", timeStr);
			if (status == HANDLED_SENDFILE && !cached) {
				result += string_printf("Accept-Ranges: bytes\r\n");
				result += string_printf("Content-Length: %lld\r\n", RangeEnd - RangeStart + 1);
				if (httpStatus == HTTP_PARTIAL_CONTENT)
					result += string_printf("Content-Range: bytes %lld-%lld/%lld\r\n", RangeStart, RangeEnd, ContentLength);
			} else
				result += string_printf("Content-Length: %lld\r\n", GetContentLength());
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
		result = outIndent() + "\"" + _key + "\": \"" + _content + "\"";
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
std::string CyhookHandler::outValue(std::string _content) {
	std::string result = "";
	switch (outType) {
	case xml:
		result += "<![CDATA[";
		result += convert_UTF8_To_UTF8_XML(utf8_check_is_valid(_content) ? _content.c_str() : iso_8859_1_to_utf8(_content).c_str());
		result += "]]>";
		break;
	case json:
		result = json_convert_string(_content);
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
