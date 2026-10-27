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
//	Note (RFC 2616 / 14.25): some servers use an exact date comparison
//	rather than a less-than function; clients are advised to send back the
//	exact Last-Modified string. This handler compares exactly, so a newer
//	file carrying an earlier date (clock set back) goes out again.
//
//	yjogol: ASSUMPTION Date-Format is ONLY RFC 1123 compatible!
//=============================================================================

// system
#include <cstdio>
#include <fcntl.h>
#include <pthread.h>
#include <sys/stat.h>
#include <unistd.h>
#include <inttypes.h>
#include <system/helpers.h>
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
	if (((!mime.empty()) || (hh->WebserverConfigList["mod_sendfile.sendAll"] == "true"))
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
				// Packages and images clamp their files to one reference time
				// per build, and two builds can share it: an update can bring
				// a file with the modification time of the one it replaces.
				// The change time cannot be set from user space; a file
				// unpacked on the box carries the time of that unpacking, so
				// the later of the two tells the versions apart as long as
				// the box's clock was past the reference time at installation.
				hh->LastModified = (statbuf.st_ctime > statbuf.st_mtime)
						? statbuf.st_ctime : statbuf.st_mtime;
			}
			close(filed);

			// check If-Modified-Since
			time_t if_modified_since = (time_t) - 1;
			if (!hh->HeaderList["If-Modified-Since"].empty()) {
				struct tm mod;
				if (strptime(hh->HeaderList["If-Modified-Since"].c_str(),
						RFC1123FMT, &mod) != NULL) {
					mod.tm_isdst = 0; // daylight saving flag!
					if_modified_since = mktime(&mod);
				}
			}

			// normalize obj_last_modified to GMT
			struct tm lm_tm;
			time_t LastModifiedGMT = mktime(gmtime_r(&(hh->LastModified), &lm_tm));
			// A copy is current only if it carries exactly the date this
			// file goes out with: the client sends back the date it was
			// given. Any other date, earlier or later, belongs to another
			// version; once the clock has been set back, a newer file can
			// carry an earlier date.
			bool modified = (if_modified_since == (time_t) - 1)
					|| (if_modified_since != LastModifiedGMT);

			// Send normal or not-modified header
			if (modified) {
				hh->RangeStart = 0;
				hh->RangeEnd = hh->ContentLength - 1;
				const char *range = (hh->HeaderList["Range"].empty()) ? NULL : hh->HeaderList["Range"].c_str();
				/* bytes=first-last, bytes=first- up to the end, and bytes=-n
				   for the last n bytes; a last byte past the end means the
				   end. A Range that cannot be read, in another unit or with
				   its last byte before its first, is ignored and the whole
				   file goes out (RFC 7233); one that starts past the end, or
				   asks for no bytes at all, cannot be served. */
				bool satisfiable = true;
				if (range) {
					int64_t first = 0, last = hh->ContentLength - 1, suffix = 0;
					bool readable = true;
					if (sscanf(range, "bytes=-%" SCNd64, &suffix) == 1) {
						satisfiable = suffix > 0;
						if (satisfiable)
							first = (suffix < hh->ContentLength) ? hh->ContentLength - suffix : 0;
					} else if (sscanf(range, "bytes=%" SCNd64 "-%" SCNd64, &first, &last) == 2) {
						if (first < 0 || last < first)
							readable = false;
						else if (last > hh->ContentLength - 1)
							last = hh->ContentLength - 1;
					} else if (sscanf(range, "bytes=%" SCNd64 "-", &first) != 1 || first < 0)
						readable = false;
					if (!readable)
						range = NULL;
					else {
						if (first > last)
							satisfiable = false;
						hh->RangeStart = first;
						hh->RangeEnd = last;
					}
				}
				if (!satisfiable) {
					// handled here: without HANDLED_ERROR the next modules
					// take the request on and it ends as 404
					hh->SetError(HTTP_REQUEST_RANGE_NOT_SATISFIABLE, HANDLED_ERROR);
					aprintf("mod_sendfile: Client requested range '%s' which is outside of [0,%lld]\n", range, hh->ContentLength - 1);
				} else {
					hh->SendFile(fullfilename);
					hh->ResponseMimeType = mime;
					/* Every range that was asked for and is served goes out as
					   206 with its Content-Range, the one from the first byte and
					   the one up to the last included: a 200 says that the body
					   is the whole file, and a player seeking in a recording
					   takes a part of it for all of it. */
					if (range)
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
	if (access(hh->WebserverConfigList["WebsiteMain.override_directory"] + tmpfilename, R_OK) == 0)
		tmpfilename = hh->WebserverConfigList["WebsiteMain.override_directory"] + tmpfilename;
	else if (access(hh->WebserverConfigList["WebsiteMain.override_directory"] + tmpfilename + ".gz", R_OK) == 0)
		tmpfilename = hh->WebserverConfigList["WebsiteMain.override_directory"] + tmpfilename + ".gz";
	else if (access(hh->WebserverConfigList["WebsiteMain.directory"] + tmpfilename, R_OK) == 0)
		tmpfilename = hh->WebserverConfigList["WebsiteMain.directory"] + tmpfilename;
	else if (access(hh->WebserverConfigList["WebsiteMain.directory"] + tmpfilename + ".gz", R_OK) == 0)
		tmpfilename = hh->WebserverConfigList["WebsiteMain.directory"] + tmpfilename + ".gz";
#ifdef Y_CONFIG_FEATUE_SENDFILE_CAN_ACCESS_ALL
	else if(access(tmpfilename,R_OK) == 0)
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
	if (!fullfilename.empty()) {
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
