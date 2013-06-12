/*
	ytcache.cpp -- cache youtube movies

	Copyright (C) 2013 martii

	License: GPL

	This program is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation; either version 2 of the License, or
	(at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program; if not, write to the Free Software
	Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/

#include "ytcache.h"
#include <OpenThreads/ScopedLock>

#include <stdio.h>
#include <unistd.h>
#include <curl/curl.h>
#include <curl/easy.h>
#if LIBCURL_VERSION_NUM < 0x071507
#include <curl/types.h>
#endif
#define URL_TIMEOUT 60

#include "helpers.h"
#include "settings.h"
#include <global.h>

static cYTCache *instance = NULL;

cYTCache::cYTCache(void)
{
	cancelled = false;
	thread = 0;
}

cYTCache::~cYTCache(void)
{
	instance = NULL;
}

cYTCache *cYTCache::getInstance(void)
{
	if (!instance)
		instance = new cYTCache();
	return instance;
}

std::string cYTCache::getName(MI_MOVIE_INFO *mi, std::string ext)
{
	char ytitag[10];
	snprintf(ytitag, sizeof(ytitag), "%d", mi->ytitag);
	return g_settings.downloadcache_dir + "/" + mi->ytid + "-" + std::string(ytitag) + "." + ext;
}

bool cYTCache::useCachedCopy(MI_MOVIE_INFO *mi)
{
	std::string cache = getName(mi);
	if (!access(cache.c_str(), R_OK)) {
		mi->file.Url = cache;
		return true;
	}
	return false;
}

int cYTCache::curlProgress(void *clientp, double /*dltotal*/, double /*dlnow*/, double /*ultotal*/, double /*ulnow*/)
{
	cYTCache *caller = (cYTCache *) clientp;
	if (caller->cancelled)
		return 1;
	return 0;
}

bool cYTCache::download(MI_MOVIE_INFO *mi)
{
	std::string file = getName(mi);
	std::string tmpfile = file + ".tmp";

	if (!access(file.c_str(), R_OK) || !access(tmpfile.c_str(), R_OK))
		return true;

	FILE * fp = fopen(file.c_str(), "wb");
	if (!fp) {
		perror(file.c_str());
		return false;
	}

	CURL *curl = curl_easy_init();
	if (!curl) {
		fclose(fp);
		return false;
	}

	curl_easy_setopt(curl, CURLOPT_URL, mi->file.Url.c_str());
	curl_easy_setopt(curl, CURLOPT_FILE, fp);
	curl_easy_setopt(curl, CURLOPT_FAILONERROR, 1);
	curl_easy_setopt(curl, CURLOPT_TIMEOUT, URL_TIMEOUT);
	curl_easy_setopt(curl, CURLOPT_NOSIGNAL, (long)1);
	curl_easy_setopt(curl, CURLOPT_PROGRESSFUNCTION, cYTCache::curlProgress); 
	curl_easy_setopt(curl, CURLOPT_PROGRESSDATA, this); 
	curl_easy_setopt(curl, CURLOPT_NOPROGRESS, (long)0); 

	char cerror[CURL_ERROR_SIZE];
	curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, cerror);

	CURLcode res = curl_easy_perform(curl);
	curl_easy_cleanup(curl);

	fclose(fp);

	if (res) {
		fprintf(stderr, "curl error: %s\n", cerror);
		unlink(tmpfile.c_str());
		return false;
	}
	rename (tmpfile.c_str(), file.c_str());
	CMovieInfo cMovieInfo;
	CFile File;
	File.Name = getName(mi, "xml");
	cMovieInfo.convertTs2XmlName(&File.Name);
	cMovieInfo.saveMovieInfo(*mi, &File);
	std::string thumbnail_dst = getName(mi, "jpg");
	std::string thumbnail_src = "/tmp/ytparser/" + mi->ytid + ".jpg";
	CFileHelpers::getInstance()->copyFile(thumbnail_src.c_str(), thumbnail_dst.c_str(), 0644);
	return true;
}

void *cYTCache::downloadThread(void *arg) {
	cYTCache *caller = (cYTCache *)arg;

	while (caller->thread) {
		MI_MOVIE_INFO mi;
		{
			OpenThreads::ScopedLock<OpenThreads::Mutex> m_lock(caller->mutex);
			if (caller->pending.empty()) {
				caller->cancelled = false;
				caller->thread = 0;
				continue;
			}
			mi = caller->pending.front();
		}

		fprintf(stderr, "download start: %s\n", mi.file.Url.c_str());

		bool res = caller->download(&mi);

		fprintf(stderr, "download end: %s %s\n", mi.file.Url.c_str(), res ? "succeeded" : "failed");

		{
			OpenThreads::ScopedLock<OpenThreads::Mutex> m_lock(caller->mutex);
			if (res)
				caller->done.push_front(mi);
			else
				caller->failed.push_front(mi);
			caller->cancelled = false;
			if (caller->pending.empty())
				caller->thread = 0;
			else
				caller->pending.pop_front();
		}
	}
	pthread_exit(NULL);
}

bool cYTCache::addToCache(MI_MOVIE_INFO *mi)
{
	OpenThreads::ScopedLock<OpenThreads::Mutex> m_lock(mutex);
	pending.push_back(*mi);

	if (!thread) {
		if (pthread_create(&thread, NULL, downloadThread, this)) {
			perror("pthread_create");
			return false;
		}
		pthread_detach(thread);
	}
	return true;
}

void cYTCache::cancel(MI_MOVIE_INFO *mi)
{
	OpenThreads::ScopedLock<OpenThreads::Mutex> m_lock(mutex);
	if (pending.empty())
		return;

	if (compareMovieInfo(mi, &pending.front())) {
		cancelled = true;
		return;
	}
	for (std::list<MI_MOVIE_INFO>::iterator it = pending.begin(); it != pending.end(); ++it)
		if (compareMovieInfo(&(*it), mi)) {
			pending.erase(it);
			break;
		}
}

void cYTCache::cancelAll(void)
{
	OpenThreads::ScopedLock<OpenThreads::Mutex> m_lock(mutex);
	if (pending.empty())
		return;

	cancelled = true;
	while (thread)
		usleep(100000);
	cancelled = false;
	pending.clear();
}

std::list<MI_MOVIE_INFO> cYTCache::getFailed(bool clear)
{
	OpenThreads::ScopedLock<OpenThreads::Mutex> m_lock(mutex);
	std::list<MI_MOVIE_INFO> res = failed;
	if (clear)
		failed.clear();
	return res;
}

std::list<MI_MOVIE_INFO> cYTCache::getDone(bool clear)
{
	OpenThreads::ScopedLock<OpenThreads::Mutex> m_lock(mutex);
	std::list<MI_MOVIE_INFO> res = done;
	if (clear)
		done.clear();
	return res;
}

void cYTCache::clearFailed(void)
{
	OpenThreads::ScopedLock<OpenThreads::Mutex> m_lock(mutex);
	failed.clear();
}

void cYTCache::clearDone(void)
{
	OpenThreads::ScopedLock<OpenThreads::Mutex> m_lock(mutex);
	done.clear();
}

bool cYTCache::compareMovieInfo(MI_MOVIE_INFO *a, MI_MOVIE_INFO *b)
{
	return a->ytid == b->ytid && a->ytitag == b->ytitag;
	return true;
}
