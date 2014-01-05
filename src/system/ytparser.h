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

#ifndef __YT_PARSER__
#define __YT_PARSER__

#include <curl/curl.h>
#include <curl/easy.h>

#include <vector>
#include <string>
#include <map>
#include <xmltree/xmlinterface.h>

#include <OpenThreads/Thread>
#include <OpenThreads/Condition>

class cYTVideoUrl
{
	public:
		std::string quality;
		std::string type;
		std::string sig;
		std::string url;

		std::string GetUrl();
};

typedef std::map<int, cYTVideoUrl> yt_urlmap_t;
typedef std::pair<int, cYTVideoUrl> yt_urlmap_pair_t;
typedef yt_urlmap_t::iterator yt_urlmap_iterator_t;

class cYTVideoInfo
{
	public:
		std::string id;
		std::string title;
		std::string author;
		std::string description;
		std::string category;
		std::string thumbnail;
		std::string tfile;
		std::string published;
		int duration;
		yt_urlmap_t formats;
		bool ret;

		void Dump();
		std::string GetUrl(int fmt = 0, bool mandatory = true);
		
};

typedef std::vector<cYTVideoInfo> yt_video_list_t;

class cYTFeedParser
{
	private:
		std::string error;
		std::string thumbnail_dir;
		std::string curfeed;
		std::string curfeedfile;
		std::string tquality; // thumbnail size
		std::string region; // more results
		std::string next; // next results
		std::string prev; // prev results
		std::string start; // start index
		std::string total; // total results

		int feedmode;
		int max_results;
		int concurrent_downloads;
		bool parsed;
		yt_video_list_t videos;

		std::string getXmlName(xmlNodePtr node);
		std::string getXmlAttr(xmlNodePtr node, const char * attr);
		std::string getXmlData(xmlNodePtr node);

		CURL *curl_handle;
		OpenThreads::Mutex mutex;
		int worker_index;
		static void* GetVideoUrlsThread(void*);
		static void* DownloadThumbnailsThread(void*);

		static size_t CurlWriteToString(void *ptr, size_t size, size_t nmemb, void *data);
		void encodeUrl(std::string &txt);
		void decodeUrl(std::string &url);
		static void splitString(std::string &str, std::string delim, std::vector<std::string> &strlist, int start = 0);
		static void splitString(std::string &str, std::string delim, std::map<std::string,std::string> &strmap, int start = 0);
		static bool saveToFile(const char * name, std::string str);
		bool getUrl(std::string &url, std::string &answer, CURL *_curl_handle = NULL);
		bool DownloadUrl(std::string &url, std::string &file, CURL *_curl_handle = NULL);
		bool parseFeedXml(std::string &answer);
		bool decodeVideoInfo(std::string &answer, cYTVideoInfo &vinfo);
		bool supportedFormat(int fmt);
		bool ParseFeed(std::string &url);
	public:
		enum yt_feed_mode_t
		{
			MOST_POPULAR,
			MOST_POPULAR_ALL_TIME,
			FEED_LAST,
			NEXT,
			PREV,
			RELATED,
			SEARCH,
			MODE_LAST
		};
		enum yt_feed_orderby_t
		{
			ORDERBY_PUBLISHED = 0,
			ORDERBY_RELEVANCE,
			ORDERBY_VIEWCOUNT,
			ORDERBY_RATING
		};
		cYTFeedParser();
		~cYTFeedParser();

		bool ParseFeed(yt_feed_mode_t mode = MOST_POPULAR, std::string search = "", std::string vid = "", yt_feed_orderby_t orderby = ORDERBY_PUBLISHED);
		bool ParseVideoInfo(cYTVideoInfo &vinfo, CURL *_curl_handle = NULL);
		bool DownloadThumbnail(cYTVideoInfo &vinfo, CURL *_curl_handle = NULL);
		bool GetVideoUrls();
		bool DownloadThumbnails();
		void Dump();
		void Cleanup(bool delete_thumbnails = true);

		yt_video_list_t &GetVideoList() { return videos; }
		bool Parsed() { return parsed; }
		int GetFeedMode() { return feedmode; }
		bool HaveNext(void) { return !next.empty(); }
		bool HavePrev(void) { return !prev.empty(); }
		std::string GetTotal(void) { return total; }
		std::string GetError(void) { return error; }

		void SetRegion(std::string reg) { region = reg; }
		void SetMaxResults(int count) { max_results = count; }
		void SetConcurrentDownloads(int count) { concurrent_downloads = count; }
};

#endif
