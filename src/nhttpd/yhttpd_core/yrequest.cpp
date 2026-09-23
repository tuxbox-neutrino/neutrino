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
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <errno.h>
#include <unistd.h>

// system
#include <fcntl.h>
#include <sys/socket.h>

// yhttpd
#include "yrequest.h"
#include "yconnection.h"
#include "helper.h"
#include "yhook.h"
#include "ysocket.h"

//=============================================================================
// Constructor & Destructor
//=============================================================================
CWebserverRequest::CWebserverRequest(CWebserver *pWebserver) {
	RawBodyTooLarge = false;
	Webserver = pWebserver;
	CWebserverRequest();
	Connection=NULL;
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
	std::string start_line = "";
	// read first line
	do {
		start_line = Connection->sock->ReceiveLine();
		if (!Connection->sock->isValid)
			return false;
		if (start_line.empty()) // Socket empty
		{
			log_level_printf(1, "HandleRequest: End of line not found\n");
			Connection->Response.SendError(HTTP_INTERNAL_SERVER_ERROR);
			Connection->RequestCanceled = true;
			return false;
		}
	} while (start_line == "\r\n"); // ignore empty lines at begin on start-line

	start_line = trim(start_line);
	log_level_printf(1, "Request: %s\n", start_line.c_str());
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
		//read header (speed up: read rest of request in blockmode)
		std::string tmp_line = Connection->sock->ReceiveBlock();
		if (!Connection->sock->isValid) {
			Connection->Response.SendError(HTTP_INTERNAL_SERVER_ERROR);
			return false;
		}

		if (tmp_line.empty()) {
			Connection->Response.SendError(HTTP_INTERNAL_SERVER_ERROR);
			return false;
		}
		ParseHeader(tmp_line);
	}
	// Other Methods
	if (Connection->Method == M_DELETE || Connection->Method == M_PUT
			|| Connection->Method == M_TRACE) {
		//todo: implement
		aprintf("HTTP Method not implemented :%d\n", Connection->Method);
		Connection->Response.SendError(HTTP_NOT_IMPLEMENTED);
		return false;
	}
	// handle POST (read header & body)
	if (Connection->Method == M_POST) {
		Connection->Response.Write("HTTP/1.1 100 Continue\r\n\r\n"); // POST Requests requires CONTINUE in HTTP/1.1
		return HandlePost();
	}
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

	log_level_printf(8, "<ParseStartLine>: line: %s\n", start_line.c_str());
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
			else {
				log_level_printf(1, "Unknown Method or invalid request\n");
				Connection->Response.SendError(HTTP_NOT_IMPLEMENTED);
				log_level_printf(3, "Request: '%s'\n", rawbuffer.c_str());
				return false;
			}
			log_level_printf(3, "Request: FullURL: %s\n", UrlData["fullurl"].c_str());
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
		log_level_printf(7, "ParseParams: name: %s value: %s\n", name.c_str(), value.c_str());
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
		log_level_printf(8, "ParseHeader: name: %s value: %s\n", name.c_str(), value.c_str());
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
// Handle Post (Form and ONE file upload)
//-----------------------------------------------------------------------------
bool CWebserverRequest::HandlePost() {
	//read header: line by line
	std::string raw_header = "", tmp_line = "";
	// FIXME: a peer that closes before the empty line makes ReceiveLine()
	// hand back one byte it never received (isValid false, bytes_gotten
	// stepped once), so this loop never sees an empty line and grows
	// raw_header until the box is out of memory. Left as it is here on
	// purpose: the legacy path is byte-identical by contract (WORK-291);
	// the API path reads its header through readApiHeader() instead.
	do {
		tmp_line = Connection->sock->ReceiveLine();
		if (tmp_line.empty()) // Socket empty
		{
			log_level_printf(1,
					"HandleRequest: (Header) End of line not found: %s\n",
					strerror(errno));
			Connection->Response.SendError(HTTP_INTERNAL_SERVER_ERROR);
			return false;
		}
		raw_header.append(tmp_line);
	} while (tmp_line != "\r\n"); // header ends with first empty line
	ParseHeader(raw_header);

	// read meesage body
	unsigned int content_len = 0;
	if (!HeaderList["Content-Length"].empty())
		content_len = atoi(HeaderList["Content-Length"].c_str());

	// Get Rest of Request from Socket
	log_level_printf(2, "Connection->Method Post !\n");
	log_level_printf(9, "Conntent Type:%s\n", (HeaderList["Content-Type"]).c_str());
	log_level_printf(8, "Post Content-Length:%d as string:(%s)\n", content_len, HeaderList["Content-Length"].c_str());

	static const std::string t = "multipart/form-data; boundary=";
	if (HeaderList["Content-Type"].compare(0, t.length(), t) == 0) // this a a multpart POST, normallly: file upload
	{
#ifdef Y_CONFIG_FEATURE_UPLOAD
		std::string boundary = "--" + HeaderList["Content-Type"].substr(t.length(),HeaderList["Content-Type"].length() - t.length());
		do
		{
			content_len = HandlePostBoundary(boundary, content_len);
		}
		while(content_len > 0 && Connection->sock->isValid);
		// At the end of the connection a part comes back empty and uses up
		// nothing of the length: asking again would never end. What did
		// not arrive whole is not handed on.
		if (!Connection->sock->isValid) {
			log_level_printf(1, "HandlePost: caller gone inside the upload\n");
			Connection->Response.SendError(HTTP_INTERNAL_SERVER_ERROR);
			return false;
		}
#else
		Connection->Response.SendError(HTTP_NOT_IMPLEMENTED);
		return false;
#endif
	} else if (HeaderList["Content-Type"].compare(
			"application/x-www-form-urlencoded") == 0) //this is a normal POST with form-data (no upload)
	{
		// handle normal form POST
		log_level_printf(6, "Handle POST application/x-www-form-urlencoded\n");

		// get message-body
		std::string post_header = Connection->sock->ReceiveBlock();
		while (post_header.length() < content_len) {
			std::string more = Connection->sock->ReceiveBlock();
			if (more.empty()) {
				// Nothing waiting: wait for the next byte instead of asking
				// again at once. A caller that went away ends the body here,
				// short of what it announced.
				char next;
				int got = Connection->sock->Read(&next, 1);
				if (got < 0 && errno == EINTR)
					continue;
				if (got != 1) {
					Connection->sock->isValid = false;
					aprintf("POST form less data then expected\n");
					Connection->Response.SendError(HTTP_INTERNAL_SERVER_ERROR);
					return false;
				}
				more.assign(1, next);
			}
			post_header += more;
		}
		// parse the params in post_header (message-body) an add them to ParameterList
		ParseParams(post_header);
	}
	return true;
}
//-----------------------------------------------------------------------------
// POST multipart ! FILE UPLOAD!
//
// No 'Content-type: multipart/mixed' now supported
// designed for recursion for different boundaries.
//
// 	from RFC 1867:
//	2.  HTML forms with file submission
//
//	   The current HTML specification defines eight possible values for the
//	   attribute TYPE of an INPUT element: CHECKBOX, HIDDEN, IMAGE,
//	   PASSWORD, RADIO, RESET, SUBMIT, TEXT.
//
//	   In addition, it defines the default ENCTYPE attribute of the FORM
//	   element using the POST METHOD to have the default value
//	   "application/x-www-form-urlencoded"
//
//	6. Examples
//
//	   Suppose the server supplies the following HTML:
//
//	     <FORM ACTION="http://server.dom/cgi/handle"
//	           ENCTYPE="multipart/form-data"
//	           METHOD=POST>
//	     What is your name? <INPUT TYPE=TEXT NAME=submitter>
//	     What files are you sending? <INPUT TYPE=FILE NAME=pics>
//	     </FORM>
//
//	   and the user types "Joe Blow" in the name field, and selects a text
//	   file "file1.txt" for the answer to 'What files are you sending?'
//
//	   The client might send back the following data:
//
//	        Content-type: multipart/form-data, boundary=AaB03x
//
//	        --AaB03x
//	        content-disposition: form-data; name="field1"
//
//	        Joe Blow
//	        --AaB03x
//	        content-disposition: form-data; name="pics"; filename="file1.txt"
//	        Content-Type: text/plain
//
//	         ... contents of file1.txt ...
//	        --AaB03x--
//
//	7. Registration of multipart/form-data
//
//	   The media-type multipart/form-data follows the rules of all multipart
//	   MIME data streams as outlined in RFC 1521. It is intended for use in
//	   returning the data that comes about from filling out a form. In a
//	   form (in HTML, although other applications may also use forms), there
//	   are a series of fields to be supplied by the user who fills out the
//	   form. Each field has a name. Within a given form, the names are
//	   unique.
//
//	   multipart/form-data contains a series of parts. Each part is expected
//	   to contain a content-disposition header where the value is "form-
//	   data" and a name attribute specifies the field name within the form,
//	   e.g., 'content-disposition: form-data; name="xxxxx"', where xxxxx is
//	   the field name corresponding to that field. Field names originally in
//	   non-ASCII character sets may be encoded using the method outlined in
//	   RFC 1522.
//
//	   As with all multipart MIME types, each part has an optional Content-
//	   Type which defaults to text/plain.  If the contents of a file are
//	   returned via filling out a form, then the file input is identified as
//	   application/octet-stream or the appropriate media type, if known.  If
//	   multiple files are to be returned as the result of a single form
//	   entry, they can be returned as multipart/mixed embedded within the
//	   multipart/form-data.
//
//	   Each part may be encoded and the "content-transfer-encoding" header
//	   supplied if the value of that part does not conform to the default
//	   encoding.
//
//	   File inputs may also identify the file name. The file name may be
//	   described using the 'filename' parameter of the "content-disposition"
//	   header. This is not required, but is strongly recommended in any case
//	   where the original filename is known. This is useful or necessary in
//	   many applications.
//-----------------------------------------------------------------------------
unsigned int CWebserverRequest::HandlePostBoundary(std::string boundary,
		unsigned int content_len) {

	// read boundary
	std::string tmp_line = Connection->sock->ReceiveLine();
	content_len -= tmp_line.length();

	log_level_printf(2, "<POST Boundary> Start\n");
	if (tmp_line.find(boundary) != std::string::npos) {
		// is it the boudary end?
		if (tmp_line.find(boundary + "--") != std::string::npos) {
			log_level_printf(7, "<POST Boundary> Boundary END found\n");
			return 0;
		}
		log_level_printf(7, "<POST Boundary> Boundary START found\n");

		// read content-disposition: ...
		tmp_line = Connection->sock->ReceiveLine();
		content_len -= tmp_line.length();
		if (tmp_line.find("Content-Disposition:") == std::string::npos) {
			log_level_printf(7, "<POST Boundary> no content-disposition found. line:(%s)\n", tmp_line.c_str());
			return 0;
		}
		if (tmp_line.find("filename") != std::string::npos) {
#ifdef Y_CONFIG_FEATURE_UPLOAD
			// this part is a file
			log_level_printf(2,"<POST Boundary> disposition !!this is a file!! found. line:(%s)\n", tmp_line.c_str());
			// get para from 'content-disposition: form-data; name="pics"; filename="file1.txt"'
			// set to ParameterList["<name>"]="<filename>"
			std::string left = "", right = "", var_name = "", var_value = "";
			if(!ySplitStringExact(tmp_line, "name=\"", left, right))
			{
				log_level_printf(7,"<POST Boundary> no var_name START found. line:(%s)\n", tmp_line.c_str());
				return 0;
			}
			if(!ySplitStringExact(right, "\"", var_name, right))
			{
				log_level_printf(7,"<POST Boundary> no var_name END found. line:(%s)\n", tmp_line.c_str());
				return 0;
			}
			if(!ySplitStringExact(right, "filename=\"", left, right))
			{
				log_level_printf(7,"<POST Boundary> no filename START found. line:(%s)\n", tmp_line.c_str());
				return 0;
			}
			if(!ySplitStringExact(right, "\"", var_value, right))
			{
				log_level_printf(7,"<POST Boundary> no filename END found. line:(%s)\n", tmp_line.c_str());
				return 0;
			}
			var_value = trim(var_value);
			ParameterList[var_name] = var_value;
			log_level_printf(7,"<POST Boundary> filename found. name:(%s) value:(%s)\n", var_name.c_str(), var_value.c_str());

			//read 'Content-Type: <mime>'
			tmp_line = Connection->sock->ReceiveLine();
			content_len -= tmp_line.length();
			// Get Content-Type: put it to ParameterList["<name>_mime"]="<mime>"
			if(!ySplitStringExact(tmp_line, "Content-Type:", left, right))
			{
				log_level_printf(7,"<POST Boundary> no Content-Type found. line:(%s)\n", tmp_line.c_str());
				return 0;
			}
			var_value = trim(right);
			ParameterList[var_name+"_mime"] = var_value;
			log_level_printf(7,"<POST Boundary> Content-Type found. name:(%s_mime) value:(%s)\n", var_name.c_str(), var_value.c_str());

			//read empty line as separator
			tmp_line = Connection->sock->ReceiveLine();
			content_len -= tmp_line.length();
			if(tmp_line != "\r\n")
			{
				log_level_printf(7,"<POST Boundary> no empty line found. line:(%s)\n", tmp_line.c_str());
				return 0;

			}
			log_level_printf(7,"<POST Boundary> read file Start\n");

			std::string upload_filename = UPLOAD_TMP_FILE;
			// Hook for Filename naming
			Connection->HookHandler.Hooks_UploadSetFilename(upload_filename);
			// Set upload filename to ParameterList["<name>_upload_filename"]="<upload_filename>"
			ParameterList[var_name+"_upload_filename"] = upload_filename;

			// open file for write
			int fd = open(upload_filename.c_str(), O_WRONLY|O_CREAT|O_TRUNC, 0644);
			if (fd<=0)
			{
				aprintf("cannot open file %s: ", upload_filename.c_str());
				dperror("");
				return 0;
			}

			// ASSUMPTION: the complete multipart has no more then SEARCH_BOUNDARY_LEN bytes after the file.
			// It only works, if no multipart/mixed is used (e.g. in file attachments). Not nessesary in embedded systems.
			// To speed up uploading, read content_len - SEARCH_BOUNDARY_LEN bytes in blockmode.
			// To save memory, write them direct into the file.
#define SEARCH_BOUNDARY_LEN 2*RECEIVE_BLOCK_LEN // >= RECEIVE_BLOCK_LEN in ySocket
			unsigned int _readbytes = 0;
			if((int)content_len - SEARCH_BOUNDARY_LEN >0)
			{
				_readbytes = Connection->sock->ReceiveFileGivenLength(fd, content_len - SEARCH_BOUNDARY_LEN);
				content_len -= _readbytes;
				log_level_printf(8,"<POST Boundary> read block (already:%d all:%d)\n", _readbytes, content_len);
			}

			// read rest of file and check for boundary end
			_readbytes = 0;
			bool is_CRLF = false;

			bool found_end_boundary = false;
			do
			{
				// read line by line
				tmp_line = Connection->sock->ReceiveLine();
				_readbytes += tmp_line.length();

				// is this line a boundary?
				if(tmp_line.find(boundary) != std::string::npos)
				{
					if(tmp_line.find(boundary+"--") != std::string::npos)
					found_end_boundary = true; // it is the end! of POST request!
					break; // boundary found. end of file.
				}
				else // no Boundary: write CRFL if found in last line
				{
					if(is_CRLF)
					if ((unsigned int)write(fd, "\r\n", 2) != 2)
					{
						perror("write file failed\n");
						return 0;
					}
				}
				// normal line: write it to file
				// CRLF at end? Maybe CRLF before boundary. Can not decide yet
				is_CRLF = (tmp_line.length()>=2 && tmp_line[tmp_line.length()-2]=='\r' && tmp_line[tmp_line.length()-1]=='\n');
				int write_len = is_CRLF ? tmp_line.length()-2 : tmp_line.length();
				if (write(fd, tmp_line.c_str(), write_len) != write_len)
				{
					perror("write file failed\n");
					return 0;
				}
				log_level_printf(2,"<POST Boundary> read file (already:%d all:%d)\n", _readbytes, content_len);
			}
			while((_readbytes < content_len) && (!tmp_line.empty()));
			content_len -= _readbytes;
			close(fd);
			log_level_printf(2,"<POST Boundary> read file End\n");
			if(found_end_boundary) // upload ok?
			{

				Connection->HookHandler.Hooks_UploadReady(upload_filename);
				return 0;
			}
#endif // Y_CONFIG_FEATURE_UPLOAD
		} else
		// this part is a POST variable/parameter
		{
			// get var_name from 'content-disposition: form-data; name="var_name"'
			std::string left = "", right = "", var_name = "", var_value = "";
			if (!ySplitStringExact(tmp_line, "name=\"", left, right)) {
				log_level_printf(7, "<POST Boundary> no var_name START found. line:(%s)\n", tmp_line.c_str());
				return 0;
			}
			if (!ySplitStringExact(right, "\"", var_name, right)) {
				log_level_printf(7, "<POST Boundary> no var_name END found. line:(%s)\n", tmp_line.c_str());
				return 0;
			}

			//read empty line as separator
			tmp_line = Connection->sock->ReceiveLine();
			content_len -= tmp_line.length();
			if (tmp_line != "\r\n") {
				log_level_printf(7, "<POST Boundary> no empty line found. line:(%s)\n", tmp_line.c_str());
				return 0;

			}
			//read var_value line
			// ASSUMPTION!!!! Only one Line for value, new line is a boundary again
			// ATTENTION!! var_name must not be unique. So Parameters are store by number too.
			var_value = Connection->sock->ReceiveLine();
			content_len -= tmp_line.length();
			var_value = trim(decodeString(var_value));
			ParameterList[var_name] = var_value;
			log_level_printf(7, "<POST Boundary> Parameter found. name:(%s) value:(%s)\n", var_name.c_str(), var_value.c_str());
		}
	}
	return content_len;
}

//-----------------------------------------------------------------------------
// API target: header line by line until the empty line, then the body for
// POST and PUT by declared length and nothing else. TRACE stays what it is
// everywhere. A body this server will not frame ends the request with 400;
// one above the ceiling is not read and left for the hook to refuse with
// 413; one that stops arriving ends at the deadline with 408.
//-----------------------------------------------------------------------------
#define API_BODY_LIMIT		(1024 * 1024)
#define API_HEADER_LIMIT	(64 * 1024)
// How much of a refused body is still read and dropped, so the 413 reaches
// the caller instead of a reset. Past this the caller is left with the reset.
#define API_DRAIN_CAP		(16 * 1024 * 1024)
#define API_BODY_TIMEOUT_MS	10000

bool CWebserverRequest::HandleApiRequest() {
	RawBody.clear();
	RawBodyTooLarge = false;

	if (Connection->Method == M_TRACE) {
		Connection->Response.SendError(HTTP_NOT_IMPLEMENTED);
		return false;
	}

	std::string raw_header;
	if (!readApiHeader(*Connection->sock, raw_header, API_HEADER_LIMIT, API_BODY_TIMEOUT_MS)) {
		log_level_printf(1, "HandleApiRequest: header not read whole: %s\n", strerror(errno));
		Connection->Response.SendError(HTTP_BAD_REQUEST);
		Connection->RequestCanceled = true;
		return false;
	}
	ParseHeader(raw_header);

	long long length = 0;
	switch (apiBodyDecision(Connection->Method, raw_header, length)) {
	case API_BODY_BAD:
		Connection->Response.SendError(HTTP_BAD_REQUEST);
		Connection->RequestCanceled = true;
		return false;
	case API_BODY_NONE:
		return true;
	case API_BODY_TOO_LARGE:
		// answered 413 by the hook; drained first, because a close with the
		// body still unread resets the connection and takes the answer with it
		RawBodyTooLarge = true;
		Connection->sock->Discard((size_t)(length > API_DRAIN_CAP ? API_DRAIN_CAP : length), API_BODY_TIMEOUT_MS);
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
		long long parsed = strtoll(text.c_str(), NULL, 10);
		if (seen && parsed != value)
			return BODY_BAD;
		seen = true;
		value = parsed;
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
	std::string rest;
	if (!sock.ReceiveUntil("\r\n\r\n", rest, limit, timeout_ms))
		return false;
	// the reader's contract is "each line, the empty one last", which is
	// what the line-by-line legacy path produced
	raw_header = rest;
	return true;
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
	const bool takes_body = (method == M_POST || method == M_PUT);
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
