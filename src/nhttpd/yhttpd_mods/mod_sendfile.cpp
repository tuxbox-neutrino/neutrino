//=============================================================================
// YHTTPD
// Module: SendFile (mod_sendfile)
//-----------------------------------------------------------------------------
// Send a File (main) with given path and filename.
// It procuced a Response-Header (SendHeader).
// It supports Client caching mechanism "If-Modified-Since".
//-----------------------------------------------------------------------------
// RFC 2616 / 14.25 If-Modified-Since
//
//	The If-Modified-Since request-header field is used with a method to
//	make it conditional: if the requested variant has not been modified
//	since the time specified in this field, an entity will not be
//	returned from the server; instead, a 304 (not modified) response will
//	be returned without any message-body.
//
//	If-Modified-Since = "If-Modified-Since" ":" HTTP-date
//	An example of the field is:
//
//		If-Modified-Since: Sat, 29 Oct 1994 19:43:31 GMT
//
//	A GET method with an If-Modified-Since header and no Range header
//	requests that the identified entity be transferred only if it has
//	been modified since the date given by the If-Modified-Since header.
//	The algorithm for determining this includes the following cases:
//
//		a)	If the request would normally result in anything other than a
//			200 (OK) status, or if the passed If-Modified-Since date is
//			invalid, the response is exactly the same as for a normal GET.
//			A date which is later than the server's current time is
//			invalid.
//
//		b)	If the variant has been modified since the If-Modified-Since
//			date, the response is exactly the same as for a normal GET.
//
//		c)	If the variant has not been modified since a valid If-
//			Modified-Since date, the server SHOULD return a 304 (Not
//			Modified) response.
//
//	yjogol: ASSUMPTION Date-Format is ONLY RFC 1123 compatible!
//=============================================================================

// system
#include <cstdio>
#include <fcntl.h>
#include <pthread.h>
#include <sys/stat.h>
#include <unistd.h>
// yhttpd
#include <yconfig.h>
#include <ytypes_globals.h>
#include <helper.h>
#include "mod_sendfile.h"

//=============================================================================
// Initialization of static variables
//=============================================================================
CStringList CmodSendfile::sendfileTypes;

//-----------------------------------------------------------------------------
// HOOK: Response Prepare Handler
// Response Prepare Check.
//-----------------------------------------------------------------------------
THandleStatus CmodSendfile::Hook_PrepareResponse(CyhookHandler *hh) {
	hh->status = HANDLED_NONE;

	log_level_printf(4, "mod_sendfile prepare hook start url:%s\n", hh->UrlData["fullurl"].c_str());

#ifdef Y_CONFIG_USE_HOSTEDWEB
	// for hosted webs: rewrite URL
	std::string _hosted=HOSTEDDOCUMENTURL;
	if((hh->UrlData["path"]).compare(0,_hosted.length(),HOSTEDDOCUMENTURL) == 0) // hosted Web ?
		hh->UrlData["path"]=hh->WebserverConfigList["WebsiteMain.hosted_directory"]+(hh->UrlData["path"]).substr(_hosted.length()-1);
#endif //Y_CONFIG_USE_HOSTEDWEB

	std::string mime = sendfileTypes[hh->UrlData["fileext"]];
	if (((mime != "") || (hh->WebserverConfigList["mod_sendfile.sendAll"] == "true"))
			&& !(hh->UrlData["fileext"] == "yhtm" || hh->UrlData["fileext"] == "yjs" || hh->UrlData["fileext"] == "ysh")) {
		//TODO: Check allowed directories / actually in GetFileName
		// build filename
		std::string fullfilename = GetFileName(hh, hh->UrlData["path"],
				hh->UrlData["filename"]);
		int filed;
		if ((filed = OpenFile(hh, fullfilename)) != -1) //can access file?
		{
			struct stat statbuf;
			hh->LastModified = (time_t) 0;
			// It is a regular file?
			fstat(filed, &statbuf);
			if (S_ISREG(statbuf.st_mode)) {
				// get file size and modify date
				hh->ContentLength = statbuf.st_size;
				hh->LastModified = statbuf.st_mtime;
			}
			close(filed);

			// check If-Modified-Since
			time_t if_modified_since = (time_t) - 1;
			if (hh->HeaderList["If-Modified-Since"] != "") {
				struct tm mod;
				if (strptime(hh->HeaderList["If-Modified-Since"].c_str(),
						RFC1123FMT, &mod) != NULL) {
					mod.tm_isdst = 0; // daylight saving flag!
					if_modified_since = mktime(&mod);
				}
			}

			// normalize obj_last_modified to GMT
			struct tm *tmp = gmtime(&(hh->LastModified));
			time_t LastModifiedGMT = mktime(tmp);
			bool modified = (if_modified_since == (time_t) - 1)
					|| (if_modified_since < LastModifiedGMT);

			// Send normal or not-modified header
			if (modified) {
				hh->RangeStart = 0;
				hh->RangeEnd = hh->ContentLength - 1;
				const char *range = (hh->HeaderList["Range"] == "") ? NULL : hh->HeaderList["Range"].c_str();
				if ((range &&
				     (2 != sscanf(range, "bytes=%lld-%lld", &hh->RangeStart, &hh->RangeEnd)) &&
				     (1 != sscanf(range, "bytes=%lld-", &hh->RangeStart)))
				 || (hh->RangeStart > hh->RangeEnd)
				 || (hh->RangeEnd > hh->ContentLength - 1)) {
					hh->SetError(HTTP_REQUEST_RANGE_NOT_SATISFIABLE);
					aprintf("mod_sendfile: Client requested range '%s' which is outside of [0,%lld]\n", range, hh->ContentLength - 1);
				} else {
					hh->SendFile(fullfilename);
					hh->ResponseMimeType = mime;
					if (hh->RangeStart && (hh->RangeEnd != hh->ContentLength - 1))
						hh->httpStatus = HTTP_PARTIAL_CONTENT;
				}
			} else
				hh->SetHeader(HTTP_NOT_MODIFIED, mime, HANDLED_READY);
		} else {
			aprintf("mod_sendfile: File not found. url:(%s) fullfilename:(%s)\n",
					hh->UrlData["url"].c_str(), fullfilename.c_str());
			hh->SetError(HTTP_NOT_FOUND);
		}
	}
	log_level_printf(4, "mod_sendfile prepare hook end status:%d\n",
			(int) hh->status);

	return hh->status;
}

//-----------------------------------------------------------------------------
// HOOK: Hook_ReadConfig
// This hook ist called from ReadConfig
//-----------------------------------------------------------------------------
THandleStatus CmodSendfile::Hook_ReadConfig(CConfigFile *Config,
		CStringList &ConfigList) {
	std::string exttypes = Config->getString("mod_sendfile.mime_types",
			HTTPD_SENDFILE_EXT);
	ConfigList["mod_sendfile.mime_types"] = exttypes;
	ConfigList["mod_sendfile.sendAll"] = Config->getString(
			"mod_sendfile.sendAll", HTTPD_SENDFILE_ALL);

	bool ende = false;
	std::string item, ext, mime;
	sendfileTypes.clear();
	while (!ende) {
		if (!ySplitStringExact(exttypes, ",", item, exttypes))
			ende = true;
		if (ySplitStringExact(item, ":", ext, mime)) {
			ext = trim(ext);
			sendfileTypes[ext] = trim(mime);
		}
	}
	return HANDLED_CONTINUE;
}

//-----------------------------------------------------------------------------
// Send File: Build Filename
// First Look at PublicDocumentRoot than PrivateDocumentRoot than pure path
//-----------------------------------------------------------------------------
std::string CmodSendfile::GetFileName(CyhookHandler *hh, std::string path, std::string filename) {
	std::string tmpfilename;
	if (path[path.length() - 1] != '/')
		tmpfilename = path + "/" + filename;
	else
		tmpfilename = path + filename;
	if (access(std::string(hh->WebserverConfigList["WebsiteMain.override_directory"] + tmpfilename).c_str(), 4) == 0)
		tmpfilename = hh->WebserverConfigList["WebsiteMain.override_directory"] + tmpfilename;
	else if (access(std::string(hh->WebserverConfigList["WebsiteMain.override_directory"] + tmpfilename + ".gz").c_str(), 4) == 0)
		tmpfilename = hh->WebserverConfigList["WebsiteMain.override_directory"] + tmpfilename + ".gz";
	else if (access(std::string(hh->WebserverConfigList["WebsiteMain.directory"] + tmpfilename).c_str(), 4) == 0)
		tmpfilename = hh->WebserverConfigList["WebsiteMain.directory"] + tmpfilename;
	else if (access(std::string(hh->WebserverConfigList["WebsiteMain.directory"] + tmpfilename + ".gz").c_str(), 4) == 0)
		tmpfilename = hh->WebserverConfigList["WebsiteMain.directory"] + tmpfilename + ".gz";
#ifdef Y_CONFIG_FEATUE_SENDFILE_CAN_ACCESS_ALL
	else if(access(tmpfilename.c_str(),4) == 0)
	;
#endif
	else {
		return "";
	}
	return tmpfilename;
}
//-----------------------------------------------------------------------------
// Send File: Open File and check file type
//-----------------------------------------------------------------------------
int CmodSendfile::OpenFile(CyhookHandler *, std::string fullfilename) {
	int fd = -1;
	if (fullfilename.length() > 0) {
		fd = open(fullfilename.c_str(), O_RDONLY | O_LARGEFILE);
		if (fd <= 0) {
			aprintf("cannot open file %s: ", fullfilename.c_str());
			dperror("");
		}
	}
	return fd;
}
//-----------------------------------------------------------------------------
// Send File: Determine MIME-Type for File-Extention
//-----------------------------------------------------------------------------
std::string CmodSendfile::GetContentType(std::string ext) {
	std::string ctype = "text/plain";
	ext = string_tolower(ext);
	for (unsigned int i = 0; i < (sizeof(MimeFileExtensions)
			/ sizeof(MimeFileExtensions[0])); i++)
		if (MimeFileExtensions[i].fileext == ext) {
			ctype = MimeFileExtensions[i].mime;
			break;
		}
	return ctype;
}
