//=============================================================================
// YHTTPD
// Module: CacheManager (mod_cache)
//=============================================================================

// system
#include <stdio.h>
#include <pthread.h>
#include <sys/stat.h>
// yhttpd
#include "yconfig.h"
#include "ytypes_globals.h"
#include "helper.h"
#include "mod_cache.h"

//=============================================================================
// Initialization of static variables
//=============================================================================
pthread_mutex_t CmodCache::mutex = PTHREAD_MUTEX_INITIALIZER;
TCacheList CmodCache::CacheList;

//-----------------------------------------------------------------------------
// HOOK: Response Prepare Handler
// Response Prepare Check.
// Is it in cache?
//-----------------------------------------------------------------------------
THandleStatus CmodCache::Hook_PrepareResponse(CyhookHandler *hh) {
	hh->status = HANDLED_NONE;

	log_level_printf(4, "mod_cache prepare hook start url:%s\n",
			hh->UrlData["fullurl"].c_str());
	std::string url = hh->UrlData["fullurl"];
	if (CacheList.find(url) != CacheList.end()) // is in Cache. Rewrite URL or not modified
	{
		pthread_mutex_lock(&mutex); // yeah, its mine

		// Check if modified
		time_t if_modified_since = (time_t) - 1;
		if (hh->HeaderList["If-Modified-Since"] != "") // Have If-Modified-Since Requested by Browser?
		{
			struct tm mod;
			if (strptime(hh->HeaderList["If-Modified-Since"].c_str(),
					RFC1123FMT, &mod) != NULL) {
				mod.tm_isdst = 0; // daylight saving flag!
				if_modified_since = mktime(&mod); // Date given
			}
		}

		// normalize obj_last_modified to GMT
		struct tm *tmp = gmtime(&(CacheList[url].created));
		time_t obj_last_modified_gmt = mktime(tmp);
		bool modified = (if_modified_since == (time_t) - 1)
				|| (if_modified_since < obj_last_modified_gmt);

		// Send file or not-modified header
		if (modified) {
			hh->SendFile(CacheList[url].filename);
			hh->ResponseMimeType = CacheList[url].mime_type;
		} else
			hh->SetHeader(HTTP_NOT_MODIFIED, CacheList[url].mime_type,
					HANDLED_READY);
		pthread_mutex_unlock(&mutex);
	}
	log_level_printf(4, "mod_cache hook prepare end status:%d\n",
			(int) hh->status);

	return hh->status;
}
//-----------------------------------------------------------------------------
// HOOK: Response Send Handler
// If an other Hook has set HookVarList["CacheCategory"] then the Conntent
// in hh->yresult should be cached into a file.
// Remeber: url, filename, mimetype, category, createdate
//-----------------------------------------------------------------------------
THandleStatus CmodCache::Hook_SendResponse(CyhookHandler *hh) {
	hh->status = HANDLED_NONE;
	std::string url = hh->UrlData["fullurl"];
	log_level_printf(4, "mod_cache hook start url:%s\n", url.c_str());

	std::string category = hh->HookVarList["CacheCategory"];
	if (!(hh->HookVarList["CacheCategory"]).empty()) // Category set = cache it
	{
		AddToCache(hh, url, hh->yresult, hh->HookVarList["CacheMimeType"],
				category); // create cache file and add to cache list
		hh->ContentLength = (hh->yresult).length();
		hh->SendFile(CacheList[url].filename); // Send as file
		hh->ResponseMimeType = CacheList[url].mime_type; // remember mime
	} else if (hh->UrlData["path"] == "/y/") // /y/ commands
	{
		hh->status = HANDLED_READY;
		if (hh->UrlData["filename"] == "cache-info")
			yshowCacheInfo(hh);
		else if (hh->UrlData["filename"] == "cache-clear")
			yCacheClear(hh);
		else
			hh->status = HANDLED_CONTINUE; // y-calls can be implemented anywhere
	}
	log_level_printf(4, "mod_cache hook end status:%d\n", (int) hh->status);

	return hh->status;
}

//-----------------------------------------------------------------------------
// HOOK: Hook_ReadConfig
// This hook ist called from ReadConfig
//-----------------------------------------------------------------------------
THandleStatus CmodCache::Hook_ReadConfig(CConfigFile *Config,
		CStringList &ConfigList) {
	cache_directory = Config->getString("mod_cache.cache_directory", CACHE_DIR);
	ConfigList["mod_cache.cache_directory"] = cache_directory;
	return HANDLED_CONTINUE;
}

//-------------------------------------------------------------------------
// Build and Add a cache item
//-------------------------------------------------------------------------
void CmodCache::AddToCache(CyhookHandler *, std::string url,
		std::string content, std::string mime_type, std::string category) {
	FILE *fd = NULL;
	pthread_mutex_lock(&mutex);
	std::string filename = cache_directory + "/" + itoa(CacheList.size()); // build cache filename
	mkdir(cache_directory.c_str(), 0777); // Create Cache directory
	if ((fd = fopen(filename.c_str(), "w")) != NULL) // open file
	{
		if (fwrite(content.c_str(), content.length(), 1, fd) == 1) // write cache file
		{
			CacheList[url].filename = filename; // add cache data item
			CacheList[url].mime_type = mime_type;
			CacheList[url].category = category;
			CacheList[url].created = time(NULL);
			std::string test = CacheList[url].filename;
		}
		fflush(fd); // flush and close file
		fclose(fd);
	}
	pthread_mutex_unlock(&mutex); // Free
}
//-------------------------------------------------------------------------
// Delete URL from cachelist
//-------------------------------------------------------------------------
void CmodCache::RemoveURLFromCache(std::string url) {
	pthread_mutex_lock(&mutex); // yeah, its mine
	if (CacheList.find(url) != CacheList.end()) {
		remove((CacheList[url].filename).c_str()); // delete file
		CacheList.erase(url); // remove from list
	}
	pthread_mutex_unlock(&mutex); // Free
}
//-------------------------------------------------------------------------
//-------------------------------------------------------------------------
void CmodCache::RemoveCategoryFromCache(std::string category) {
	pthread_mutex_lock(&mutex);
	bool restart = false;
	do {
		restart = false;
		TCacheList::iterator i = CacheList.begin();
		for (; i != CacheList.end(); i++) {
			TCache *item = &((*i).second);
			if (item->category == category) {
				CacheList.erase(((*i).first));
				restart = true;
				break;
			}
		}
	} while (restart);
	pthread_mutex_unlock(&mutex);
}

//-------------------------------------------------------------------------
//
//-------------------------------------------------------------------------
void CmodCache::DeleteCache(void) {
	pthread_mutex_lock(&mutex); // yeah, its mine
	CacheList.clear(); // Clear entire list
	pthread_mutex_unlock(&mutex); // Free
}

//-------------------------------------------------------------------------
// y-Call : show cache Information
//-------------------------------------------------------------------------
void CmodCache::yshowCacheInfo(CyhookHandler *hh) {
	std::string yresult;

	hh->SendHTMLHeader("Cache Information");
	yresult += string_printf("<b>Cache Information</b><br/>\n<code>\n");
	yresult += string_printf("Cache Module...: %s<br/>\n",
			(getHookName()).c_str());
	yresult += string_printf("Cache Version..: %s<br/>\n",
			(getHookVersion()).c_str());
	yresult += string_printf("Cache Directory: %s<br/>\n",
			cache_directory.c_str());
	yresult += string_printf("</code>\n<br/><b>CACHE</b><br/>\n");

	// cache list
	yresult += string_printf("<table border=\"1\">\n");
	yresult
			+= string_printf(
					"<tr><td>URL</td><td>Mime</td><td>Filename</td><td>Category</td><td>Created</td><td>Remove</td></tr>\n");
	pthread_mutex_lock(&mutex);
	TCacheList::iterator i = CacheList.begin();
	for (; i != CacheList.end(); i++) {
		TCache *item = &((*i).second);
		char timeStr[80];
		strftime(timeStr, sizeof(timeStr), RFC1123FMT, gmtime(&(item->created)));
		yresult
				+= string_printf(
						"<tr><td>%s</td><td>%s</td><td>%s</td><td>%s</td><td>%s</td>"
							"<td><a href=\"/y/cache-clear?url=%s\">url</a>&nbsp;"
							"<a href=\"/y/cache-clear?category=%s\">category</a></td></tr>\n",
						((*i).first).c_str(), item->mime_type.c_str(),
						item->filename.c_str(), item->category.c_str(),
						timeStr, ((*i).first).c_str(), item->category.c_str());
	}
	pthread_mutex_unlock(&mutex); // Free
	yresult += string_printf("</table>\n");
	yresult += string_printf(
			"<a href=\"/y/cache-clear\">Delete Cache</a><br/>\n");
	hh->addResult(yresult, HANDLED_READY);
	hh->SendHTMLFooter();
}
//-------------------------------------------------------------------------
// y-Call : clear cache
//-------------------------------------------------------------------------
void CmodCache::yCacheClear(CyhookHandler *hh) {
	std::string result = "";
	if (hh->ParamList["category"] != "") {
		RemoveCategoryFromCache(hh->ParamList["category"]);
		result = string_printf("Category (%s) removed from cache.</br>",
				hh->ParamList["category"].c_str());
	} else if (hh->ParamList["url"] != "") {
		RemoveURLFromCache(hh->ParamList["url"]);
		result = string_printf("URL (%s) removed from cache.</br>",
				hh->ParamList["url"].c_str());
	} else {
		DeleteCache();
		result = string_printf("Cache deleted.</br>");
	}
	hh->SendHTMLHeader("Cache deletion");
	hh->WriteLn(result);
	hh->SendHTMLFooter();

}

