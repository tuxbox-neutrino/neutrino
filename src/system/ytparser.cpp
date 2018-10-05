/*
        Copyright (C) 2013 CoolStream International Ltd

        License: GPLv2

        This program is free software; you can redistribute it and/or modify
        it under the terms of the GNU General Public License as published by
        the Free Software Foundation;

        This program is distributed in the hope that it will be useful,
        but WITHOUT ANY WARRANTY; without even the implied warranty of
        MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
        GNU General Public License for more details.

        You should have received a copy of the GNU General Public License
        along with this program; if not, write to the Free Software
        Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <errno.h>
#include <fstream>

#include <set>
#include <map>
#include <vector>
#include <bitset>
#include <string>

#include <OpenThreads/ScopedLock>
#include "settings.h"
#include "helpers.h"
#include "helpers-json.h"
#include "set_threadname.h"
#include <global.h>
#include <json/json.h>

#include "ytparser.h"
#include "ytcache.h"

#if LIBCURL_VERSION_NUM < 0x071507
#include <curl/types.h>
#endif

#define URL_TIMEOUT 60
static int itags[] = { 37 /* 1080p MP4 */, 22 /* 720p MP4 */, 18 /* 270p/360p MP4 */, 0 };


std::string cYTVideoUrl::GetUrl()
{
	std::string fullurl = url;
	//fullurl += "&signature=";
	//fullurl += sig;
	return fullurl;
}

void cYTVideoInfo::Dump()
{
	printf("id: %s\n", id.c_str());
	printf("author: %s\n", author.c_str());
	printf("title: %s\n", title.c_str());
	printf("duration: %d\n", duration);
	//printf("description: %s\n", description.c_str());
	printf("urls: %d\n", (int)formats.size());
	for (yt_urlmap_iterator_t it = formats.begin(); it != formats.end(); ++it) {
		printf("format %d type [%s] url %s\n", it->first, it->second.type.c_str(), it->second.GetUrl().c_str());
	}
	printf("===================================================================\n");
}

std::string cYTVideoInfo::GetUrl(int *fmt, bool mandatory)
{
	int default_fmt = 0;
	if (!*fmt)
		fmt = &default_fmt;

	yt_urlmap_iterator_t it;
	if (fmt) {
		if ((it = formats.find(*fmt)) != formats.end()) {
			return it->second.GetUrl();
		}
		if (mandatory) {
			*fmt = 0;
			return "";
		}
	}

	for (int *fmtp = itags; *fmtp; fmtp++)
		if ((it = formats.find(*fmtp)) != formats.end()) {
			*fmt = *fmtp;
			return it->second.GetUrl();
		}
	return "";
}

cYTFeedParser::cYTFeedParser()
{
	thumbnail_dir = "/tmp/ytparser";
	parsed = false;
	feedmode = -1;
	tquality = "mqdefault";
	max_results = 25;
	concurrent_downloads = 2;
	curl_handle = curl_easy_init();
#ifdef YOUTUBE_DEV_ID
	key = YOUTUBE_DEV_ID;
#else
	key = g_settings.youtube_dev_id;
#endif
}

cYTFeedParser::~cYTFeedParser()
{
	curl_easy_cleanup(curl_handle);
}

size_t cYTFeedParser::CurlWriteToString(void *ptr, size_t size, size_t nmemb, void *data)
{
	if (size * nmemb > 0) {
		std::string* pStr = (std::string*) data;
		pStr->append((char*) ptr, nmemb);
	}
        return size*nmemb;
}

bool cYTFeedParser::getUrl(std::string &url, std::string &answer, CURL *_curl_handle)
{
	if (!_curl_handle)
		_curl_handle = curl_handle;

	curl_easy_setopt(_curl_handle, CURLOPT_URL, url.c_str());
	curl_easy_setopt(_curl_handle, CURLOPT_WRITEFUNCTION, &cYTFeedParser::CurlWriteToString);
	curl_easy_setopt(_curl_handle, CURLOPT_FILE, (void *)&answer);
	curl_easy_setopt(_curl_handle, CURLOPT_FAILONERROR, 1);
	curl_easy_setopt(_curl_handle, CURLOPT_TIMEOUT, URL_TIMEOUT);
	curl_easy_setopt(_curl_handle, CURLOPT_NOSIGNAL, (long)1);
	curl_easy_setopt(_curl_handle, CURLOPT_SSL_VERIFYPEER, false);

	if(!g_settings.softupdate_proxyserver.empty()) {
		curl_easy_setopt(_curl_handle, CURLOPT_PROXY, g_settings.softupdate_proxyserver.c_str());
		if(!g_settings.softupdate_proxyusername.empty()) {
			std::string tmp = g_settings.softupdate_proxyusername + ":" + g_settings.softupdate_proxypassword;
			curl_easy_setopt(_curl_handle, CURLOPT_PROXYUSERPWD, tmp.c_str());
		}
	}

	char cerror[CURL_ERROR_SIZE] = {0};
	curl_easy_setopt(_curl_handle, CURLOPT_ERRORBUFFER, cerror);

	printf("try to get [%s] ...\n", url.c_str());
	CURLcode httpres = curl_easy_perform(_curl_handle);

	printf("http: res %d size %d\n", httpres, (int)answer.size());

	if (httpres != 0 || answer.empty()) {
		printf("error: %s\n", cerror);
		return false;
	}
	return true;
}

bool cYTFeedParser::DownloadUrl(std::string &url, std::string &file, CURL *_curl_handle)
{
	if (!_curl_handle)
		_curl_handle = curl_handle;

	FILE * fp = fopen(file.c_str(), "wb");
	if (fp == NULL) {
		perror(file.c_str());
		return false;
	}
	curl_easy_setopt(_curl_handle, CURLOPT_URL, url.c_str());
	curl_easy_setopt(_curl_handle, CURLOPT_WRITEFUNCTION, NULL);
	curl_easy_setopt(_curl_handle, CURLOPT_FILE, fp);
	curl_easy_setopt(_curl_handle, CURLOPT_FAILONERROR, 1);
	curl_easy_setopt(_curl_handle, CURLOPT_TIMEOUT, URL_TIMEOUT);
	curl_easy_setopt(_curl_handle, CURLOPT_NOSIGNAL, (long)1);
	curl_easy_setopt(_curl_handle, CURLOPT_SSL_VERIFYPEER, false);

	if(!g_settings.softupdate_proxyserver.empty()) {
		curl_easy_setopt(_curl_handle, CURLOPT_PROXY, g_settings.softupdate_proxyserver.c_str());
		if(!g_settings.softupdate_proxyusername.empty()) {
			std::string tmp = g_settings.softupdate_proxyusername + ":" + g_settings.softupdate_proxypassword;
			curl_easy_setopt(_curl_handle, CURLOPT_PROXYUSERPWD, tmp.c_str());
		}
	}

	char cerror[CURL_ERROR_SIZE] = {0};
	curl_easy_setopt(_curl_handle, CURLOPT_ERRORBUFFER, cerror);

	printf("try to get [%s] ...\n", url.c_str());
	CURLcode httpres = curl_easy_perform(_curl_handle);

	double dsize;
	curl_easy_getinfo(_curl_handle, CURLINFO_SIZE_DOWNLOAD, &dsize);
	fclose(fp);

	printf("http: res %d size %g.\n", httpres, dsize);

	if (httpres != 0) {
		printf("curl error: %s\n", cerror);
		unlink(file.c_str());
		return false;
	}
	return true;
}

void cYTFeedParser::decodeUrl(std::string &url)
{
	char * str = curl_easy_unescape(curl_handle, url.c_str(), 0, NULL);
	if(str)
		url = str;
	curl_free(str);
}

void cYTFeedParser::encodeUrl(std::string &txt)
{
	char * str = curl_easy_escape(curl_handle, txt.c_str(), txt.length());
	if(str)
		txt = str;
	curl_free(str);
}

void cYTFeedParser::splitString(std::string &str, std::string delim, std::vector<std::string> &strlist, int start)
{
	strlist.clear();
	std::string::size_type end = 0;
	while ((end = str.find(delim, start)) != std::string::npos) {
		strlist.push_back(str.substr(start, end - start));
		start = end + delim.size();
	}
	strlist.push_back(str.substr(start));
}

void cYTFeedParser::splitString(std::string &str, std::string delim, std::map<std::string,std::string> &strmap, int start)
{
	std::string::size_type end = 0;
	if ((end = str.find(delim, start)) != std::string::npos) {
		strmap[str.substr(start, end - start)] = str.substr(end - start + delim.size());
	}
}

bool cYTFeedParser::saveToFile(const char * name, std::string str)
{
	FILE * fp = fopen(name, "w+");
	if (fp) {
		fprintf(fp, "%s", str.c_str());
		fclose(fp);
		return false;
	}
	printf("cYTFeedParser::saveToFile: failed to open %s\n", name);
	return false;
}

std::string cYTFeedParser::getXmlName(xmlNodePtr node)
{
	std::string result;
	const char * name = xmlGetName(node);
	if (name)
		result = name;
	return result;
}

std::string cYTFeedParser::getXmlAttr(xmlNodePtr node, const char * attr)
{
	std::string result;
	const char * value = xmlGetAttribute(node, attr);
	if (value)
		result = value;
	return result;
}

std::string cYTFeedParser::getXmlData(xmlNodePtr node)
{
	std::string result;
	const char * value = xmlGetData(node);
	if (value)
		result = value;
	return result;
}

bool cYTFeedParser::parseFeedJSON(std::string &answer)
{
	std::string errMsg = "";
	Json::Value root;

	std::ostringstream ss;
	std::ifstream fh(curfeedfile.c_str(),std::ifstream::in);
	ss << fh.rdbuf();
	std::string filedata = ss.str();

	bool parsedSuccess = parseJsonFromString(filedata, &root, NULL);

	if(!parsedSuccess)
	{
		parsedSuccess = parseJsonFromString(answer, &root, &errMsg);
	}

	if(!parsedSuccess)
	{
		printf("Failed to parse JSON\n");
		printf("%s\n", errMsg.c_str());
		return false;
	}

	next.clear();
	prev.clear();
	//TODO
	total.clear();
	start.clear();

	next = root.get("nextPageToken", "").asString();
	prev = root.get("prevPageToken", "").asString();

	cYTVideoInfo vinfo;
	Json::Value elements = root["items"];
	for(unsigned int i=0; i<elements.size();++i)
	{
		OnProgress(i, elements.size(), g_Locale->getText(LOCALE_MOVIEBROWSER_SCAN_FOR_VIDEOS));
#ifdef DEBUG_PARSER
		printf("=========================================================\n");
		printf("Element %d in elements\n", i);
		printf("%s\n", elements[i]);
#endif
		if(elements[i]["id"].type() == Json::objectValue) {
			vinfo.id = elements[i]["id"].get("videoId", "").asString();
		}
		else if(elements[i]["id"].type() == Json::stringValue) {
			vinfo.id = elements[i].get("id", "").asString();
		}

		vinfo.title		= elements[i]["snippet"].get("title", "").asString();
		vinfo.description	= elements[i]["snippet"].get("description", "").asString();
		vinfo.published		= elements[i]["snippet"].get("publishedAt", "").asString().substr(0, 10);
		std::string thumbnail	= elements[i]["snippet"]["thumbnails"]["default"].get("url", "").asString();
		// save thumbnail "default", if "high" not found
		vinfo.thumbnail		= elements[i]["snippet"]["thumbnails"]["high"].get("url", thumbnail).asString();
		vinfo.author		= elements[i]["snippet"].get("channelTitle", "unkown").asString();
		vinfo.category		= "";
		parseFeedDetailsJSON(&vinfo);

#ifdef DEBUG_PARSER
		printf("prevPageToken: %s\n", prevPageToken.c_str());
		printf("nextPageToken: %s\n", nextPageToken.c_str());
		printf("vinfo.id: %s\n", vinfo.id.c_str());
		printf("vinfo.description: %s\n", vinfo.description.c_str());
		printf("vinfo.published: %s\n", vinfo.published.c_str());
		printf("vinfo.title: %s\n", vinfo.title.c_str());
		printf("vinfo.thumbnail: %s\n", vinfo.thumbnail.c_str());
#endif
		if (!vinfo.id.empty()) {
			vinfo.ret = false;
			videos.push_back(vinfo);
		}
	}

	GetVideoUrls();

	std::vector<cYTVideoInfo>::iterator pos = videos.begin();
	while (pos != videos.end())
		if ((*pos).ret)
			++pos;
		else
			pos = videos.erase(pos);

	parsed = !videos.empty();
	return parsed;
}

bool cYTFeedParser::parseFeedDetailsJSON(cYTVideoInfo* vinfo)
{
	vinfo->duration = 0;
	// See at https://developers.google.com/youtube/v3/docs/videos
	std::string url	= "https://www.googleapis.com/youtube/v3/videos?id=" + vinfo->id + "&part=contentDetails&key=" + key;
	std::string answer;
	if (!getUrl(url, answer))
		return false;

	std::string errMsg = "";
	Json::Value root;
	bool parsedSuccess = parseJsonFromString(answer, &root, &errMsg);
	if (!parsedSuccess) {
		printf("Failed to parse JSON\n");
		printf("%s\n", errMsg.c_str());
		return false;
	}


	Json::Value elements = root["items"];
	std::string duration = elements[0]["contentDetails"].get("duration", "").asString();
	if (duration.find("PT") != std::string::npos) {
		int h=0, m=0, s=0;
		if (duration.find("H") != std::string::npos) {
			sscanf(duration.c_str(), "PT%dH%dM%dS", &h, &m, &s);
		}
		else if (duration.find("M") != std::string::npos) {
			sscanf(duration.c_str(), "PT%dM%dS", &m, &s);
		}
		else if (duration.find("S") != std::string::npos) {
			sscanf(duration.c_str(), "PT%dS", &s);
		}
//		printf(">>>>> duration: %s, h: %d, m: %d, s: %d\n", duration.c_str(), h, m, s);
		vinfo->duration = h*3600 + m*60 + s;
	}
	return true;
}

bool cYTFeedParser::supportedFormat(int fmt)
{
	for (int *fmtp = itags; *fmtp; fmtp++)
		if (*fmtp == fmt)
			return true;
	return false;
}

bool cYTFeedParser::decodeVideoInfo(std::string &answer, cYTVideoInfo &vinfo)
{
	bool ret = false;
	decodeUrl(answer);
#if 0
	std::string infofile = thumbnail_dir;
	infofile += "/";
	infofile += vinfo.id;
	infofile += ".txt";
	saveToFile(infofile.c_str(), answer);
#endif
	if(answer.find("token=") == std::string::npos)
		return ret;

	//FIXME check expire
	std::vector<std::string> ulist;
	std::string::size_type fmt = answer.find("url_encoded_fmt_stream_map=");
	if (fmt != std::string::npos) {
		fmt = answer.find("=", fmt);
		splitString(answer, ",", ulist, fmt+1);
		for (unsigned i = 0; i < ulist.size(); i++) {
#if 0 // to decode all params
			decodeUrl(ulist[i]);
			printf("URL: %s\n", ulist[i].c_str());
#endif
			std::map<std::string,std::string> smap;
			std::vector<std::string> uparams;
			splitString(ulist[i], "&", uparams);
			if (uparams.size() < 3)
				continue;
			for (unsigned j = 0; j < uparams.size(); j++) {
				decodeUrl(uparams[j]);
#ifdef DEBUG_PARSER
				printf("	param: %s\n", uparams[j].c_str());
#endif
				splitString(uparams[j], "=", smap);
			}
#ifdef DEBUG_PARSER
			printf("=========================================================\n");
#endif
			cYTVideoUrl yurl;
			yurl.url = smap["url"];

			std::string::size_type ptr = smap["url"].find("signature=");
			if (ptr != std::string::npos)
			{
				ptr = smap["url"].find("=", ptr);
				smap["url"].erase(0,ptr+1);

				if((ptr = smap["url"].find("&")) != std::string::npos)
					yurl.sig = smap["url"].substr(0,ptr);
			}

			int id = atoi(smap["itag"].c_str());
			if (supportedFormat(id) && !yurl.url.empty() && !yurl.sig.empty()) {
				yurl.quality = smap["quality"];
				yurl.type = smap["type"];
				vinfo.formats.insert(yt_urlmap_pair_t(id, yurl));
				ret = true;
			}
		}
	}
	return ret;
}

bool cYTFeedParser::ParseFeed(std::string &url)
{
	videos.clear();

	std::string answer;
	curfeedfile = thumbnail_dir;
	curfeedfile += "/";
	curfeedfile += curfeed;
	curfeedfile += ".xml";
#ifdef CACHE_FILES
	if(!DownloadUrl(url, cfile))
		return false;
#else
	if (!getUrl(url, answer))
		return false;
#endif
	return parseFeedJSON(answer);
}

bool cYTFeedParser::ParseFeed(yt_feed_mode_t mode, std::string search, std::string vid, yt_feed_orderby_t orderby)
{
	std::string answer;
	std::string url = "https://www.googleapis.com/youtube/v3/search?";
	bool append_res = true;
	std::string trailer;
	if (mode < FEED_LAST) {
		switch(mode) {
			//FIXME APIv3: we dont have the parameter "time".
			case MOST_POPULAR:
			default:
				//trailer = "&time=today";
				curfeed = "&chart=mostPopular";
				break;
			case MOST_POPULAR_ALL_TIME:
				curfeed = "&chart=mostPopular";
				break;
		}
		url = "https://www.googleapis.com/youtube/v3/videos?part=snippet";
		if (!region.empty()) {
			url += "&regionCode=";
			url += region;
		}
		url += curfeed;
	}
	else if (mode == NEXT) {
		if (next.empty())
			return false;
		url = nextprevurl;
		url += "&pageToken=";
		url += next;
		append_res = false;
	}
	else if (mode == PREV) {
		if (prev.empty())
			return false;
		url = nextprevurl;
		url += "&pageToken=";
		url += prev;
		append_res = false;
	}
	else if (mode == RELATED) {
		if (vid.empty())
			return false;
		url = "https://www.googleapis.com/youtube/v3/videos/";
		url += vid;
		url += "/related?";
	}
	else if (mode == SEARCH) {
		if (search.empty())
			return false;
		encodeUrl(search);
		url = "https://www.googleapis.com/youtube/v3/search?q=";
		url += search;
		url += "&part=snippet";
		//FIXME locale for "title" and "videoCount"
		const char *orderby_values[] = { "date","relevance","viewCount","rating","title","videoCount"};
		url += "&order=" + std::string(orderby_values[orderby & 3]);
	}

	feedmode = mode;
	if (append_res) {
		url += "&maxResults=";
		char res[10];
		sprintf(res, "%d", max_results);
		url+= res;
		url += "&key=" + key;
		nextprevurl = url;
	}

	return ParseFeed(url);
}

bool cYTFeedParser::ParseVideoInfo(cYTVideoInfo &vinfo, CURL *_curl_handle)
{
	bool ret = false;
	std::vector<std::string> estr;
	estr.push_back("&el=embedded");
	estr.push_back("&el=vevo");
	estr.push_back("&el=detailpage");

	for (unsigned i = 0; i < estr.size(); i++) {
		std::string vurl = "http://www.youtube.com/get_video_info?video_id=";
		vurl += vinfo.id;
		vurl += estr[i];
		vurl += "&ps=default&eurl=&gl=US&hl=en";
		printf("cYTFeedParser::ParseVideoInfo: get [%s]\n", vurl.c_str());
		std::string answer;
		if (!getUrl(vurl, answer, _curl_handle))
			continue;
		ret = decodeVideoInfo(answer, vinfo);
		if (ret)
			break;
	}
	vinfo.ret = ret;
	return ret;
}

void *cYTFeedParser::DownloadThumbnailsThread(void *arg)
{
	set_threadname("YT::DownloadThumbnails");
	bool ret = true;
	cYTFeedParser *caller = (cYTFeedParser *)arg;
	CURL *c = curl_easy_init();
	unsigned int i;
	do {
		OpenThreads::ScopedLock<OpenThreads::Mutex> m_lock(caller->mutex);
		i = caller->worker_index++;
	} while (i < caller->videos.size() && ((ret &= caller->DownloadThumbnail(caller->videos[i], c)) || true));
	curl_easy_cleanup(c);
	pthread_exit(&ret);
}

bool cYTFeedParser::DownloadThumbnail(cYTVideoInfo &vinfo, CURL *_curl_handle)
{
	if (!_curl_handle)
		_curl_handle = curl_handle;
	bool found = false;
	if (!vinfo.thumbnail.empty()) {
		std::string fname = thumbnail_dir + "/" + vinfo.id + ".jpg";
		found = !access(fname, F_OK);
		if (!found) {
			for (int *fmtp = itags; *fmtp && !found; fmtp++)
				found = cYTCache::getInstance()->getNameIfExists(fname, vinfo.id, *fmtp);
		}
		if (!found)
			found = DownloadUrl(vinfo.thumbnail, fname, _curl_handle);
		if (found)
			vinfo.tfile = fname;
	}
	return found;
}

bool cYTFeedParser::DownloadThumbnails()
{
	bool ret = true;
	if (mkdir(thumbnail_dir.c_str(), 0755) && errno != EEXIST) {
		perror(thumbnail_dir.c_str());
		return false;
	}
	unsigned int max_threads = concurrent_downloads;
	if (videos.size() < max_threads)
		max_threads = videos.size();
	pthread_t ta[max_threads];
	worker_index = 0;
	for (unsigned i = 0; i < max_threads; i++)
		pthread_create(&ta[i], NULL, cYTFeedParser::DownloadThumbnailsThread, this);
	for (unsigned i = 0; i < max_threads; i++) {
		void *r;
		pthread_join(ta[i], &r);
		ret &= *((bool *)r);
	}
	return ret;
}

void *cYTFeedParser::GetVideoUrlsThread(void *arg)
{
	set_threadname("YT::GetVideoUrls");
	int ret = 0;
	cYTFeedParser *caller = (cYTFeedParser *)arg;
	CURL *c = curl_easy_init();
	unsigned int i;
	do {
		OpenThreads::ScopedLock<OpenThreads::Mutex> m_lock(caller->mutex);
		i = caller->worker_index++;
	} while (i < caller->videos.size() && ((ret |= caller->ParseVideoInfo(caller->videos[i], c)) || true));
	curl_easy_cleanup(c);
	pthread_exit(&ret);
}

bool cYTFeedParser::GetVideoUrls()
{
	int ret = 0;
	unsigned int max_threads = concurrent_downloads;
	if (videos.size() < max_threads)
		max_threads = videos.size();
	pthread_t ta[max_threads];
	worker_index = 0;
	for (unsigned i = 0; i < max_threads; i++)
		pthread_create(&ta[i], NULL, cYTFeedParser::GetVideoUrlsThread, this);
	for (unsigned i = 0; i < max_threads; i++) {
		void *r;
		pthread_join(ta[i], &r);
		ret |= *((int *)r);
	}
	return ret;
}

void cYTFeedParser::Cleanup(bool delete_thumbnails)
{
	printf("cYTFeedParser::Cleanup: %d videos\n", (int)videos.size());
	if (delete_thumbnails) {
		for (unsigned i = 0; i < videos.size(); i++) {
			unlink(videos[i].tfile.c_str());
		}
	}
	unlink(curfeedfile.c_str());
	videos.clear();
	parsed = false;
	feedmode = -1;
}

void cYTFeedParser::SetThumbnailDir(std::string &_thumbnail_dir)
{
	thumbnail_dir = _thumbnail_dir;
}

void cYTFeedParser::Dump()
{
	printf("feed: %d videos\n", (int)videos.size());
	for (unsigned i = 0; i < videos.size(); i++)
		videos[i].Dump();
}
