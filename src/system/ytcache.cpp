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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "ytcache.h"
#include <OpenThreads/ScopedLock>

#include <stdio.h>
#include <unistd.h>
#include <curl/curl.h>
#include <curl/easy.h>
#if LIBCURL_VERSION_NUM < 0x071507
#include <curl/types.h>
#endif
#define URL_TIMEOUT 3600

#include "helpers.h"
#include "settings.h"
#include "set_threadname.h"
#include <global.h>
#include <driver/display.h>

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
	switch (mi->source) {
		case MI_MOVIE_INFO::YT:
			return g_settings.downloadcache_dir + "/" + mi->ytid + "-" + to_string(mi->ytitag) + "." + ext;
		case MI_MOVIE_INFO::NK:
			return g_settings.downloadcache_dir + "/nk-" + mi->ytid + "." + ext;
		default:
			return "";
	}
}

bool cYTCache::getNameIfExists(std::string &fname, const std::string &id, int itag, std::string ext)
{
	std::string f = g_settings.downloadcache_dir + "/" + id + "-" + to_string(itag) + "." + ext;
	if (access(f, R_OK))
		return false;
	fname = f;
	return true;
}

bool cYTCache::useCachedCopy(MI_MOVIE_INFO *mi)
{
	std::string file = getName(mi);
	if (access(file, R_OK))
		return false;
	std::string xml = getName(mi, "xml");
	if (!access(xml, R_OK)) {
		mi->file.Url = file;
		return true;
	}
	{
		OpenThreads::ScopedLock<OpenThreads::Mutex> m_lock(mutex);
		if (pending.empty())
			return false;
		MI_MOVIE_INFO m = pending.front();
		if (compareMovieInfo(&m, mi)) {
			mi->file.Url = file;
			return true;
		}
	}
	
	return false;
}

int cYTCache::curlProgress(void *clientp, double dltotal, double dlnow, double /*ultotal*/, double /*ulnow*/)
{
	cYTCache *caller = (cYTCache *) clientp;
	caller->dltotal = dltotal;
	caller->dlnow = dlnow;
	if (caller->cancelled)
		return 1;
	return 0;
}

bool cYTCache::download(MI_MOVIE_INFO *mi)
{
	std::string file = getName(mi);
	std::string xml = getName(mi, "xml");
	if (!access(file, R_OK) && !access(xml, R_OK)) {
		fprintf(stderr, "%s: %s already present and valid\n", __func__, file.c_str());
		return true;
	}

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
	curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1);

	char cerror[CURL_ERROR_SIZE] = {0};
	curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, cerror);

	if(!g_settings.softupdate_proxyserver.empty()) {
		curl_easy_setopt(curl, CURLOPT_PROXY, g_settings.softupdate_proxyserver.c_str());
		if(!g_settings.softupdate_proxyusername.empty()) {
			std::string tmp = g_settings.softupdate_proxyusername + ":" + g_settings.softupdate_proxypassword;
			curl_easy_setopt(curl, CURLOPT_PROXYUSERPWD, tmp.c_str());
		}
	}

	dltotal = 0;
	dlnow = 0;
	dlstart = time(NULL);

	fprintf (stderr, "downloading %s to %s\n", mi->file.Url.c_str(), file.c_str());
	CURLcode res = curl_easy_perform(curl);
	curl_easy_cleanup(curl);

	fclose(fp);

	if (res) {
		fprintf (stderr, "downloading %s to %s failed: %s\n", mi->file.Url.c_str(), file.c_str(), cerror);
		unlink(file.c_str());
		return false;
	}
	CMovieInfo cMovieInfo;
	CFile File;
	File.Name = xml;
	cMovieInfo.saveMovieInfo(*mi, &File);
	std::string thumbnail_dst = getName(mi, "jpg");
	CFileHelpers fh;
	fh.copyFile(mi->tfile.c_str(), thumbnail_dst.c_str(), 0644);
	return true;
}

void *cYTCache::downloadThread(void *arg) {
	fprintf(stderr, "%s starting\n", __func__);
	set_threadname("ytdownload");
	cYTCache *caller = (cYTCache *)arg;

	//CVFD::getInstance()->ShowIcon(FP_ICON_DOWNLOAD, true);

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


		bool res = caller->download(&mi);

		caller->cancelled = false;

		{
			OpenThreads::ScopedLock<OpenThreads::Mutex> m_lock(caller->mutex);
			if (res)
				caller->completed.insert(caller->completed.begin(), mi);
			else
				caller->failed.insert(caller->failed.begin(), mi);
			if (caller->pending.empty())
				caller->thread = 0;
			else
				caller->pending.erase(caller->pending.begin());
		}
	}

	//CVFD::getInstance()->ShowIcon(FP_ICON_DOWNLOAD, false);

	fprintf(stderr, "%s exiting\n", __func__);
	pthread_exit(NULL);
}

bool cYTCache::addToCache(MI_MOVIE_INFO *mi)
{
	{
		OpenThreads::ScopedLock<OpenThreads::Mutex> m_lock(mutex);
		pending.push_back(*mi);
	}

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
	mutex.lock();
	if (pending.empty())
		return;

	if (compareMovieInfo(mi, &pending.front())) {
		cancelled = true;
		mutex.unlock();
		while (cancelled)
			usleep(100000);
		return;
	} else {
		for (std::vector<MI_MOVIE_INFO>::iterator it = pending.begin(); it != pending.end(); ++it)
			if (compareMovieInfo(&(*it), mi)) {
				pending.erase(it);
				failed.push_back(*mi);
				break;
			}
	}
	mutex.unlock();
}

void cYTCache::remove(MI_MOVIE_INFO *mi)
{
	OpenThreads::ScopedLock<OpenThreads::Mutex> m_lock(mutex);
	if (completed.empty())
		return;

	for (std::vector<MI_MOVIE_INFO>::iterator it = completed.begin(); it != completed.end(); ++it)
		if (compareMovieInfo(&(*it), mi)) {
			completed.erase(it);
			unlink(getName(mi).c_str());
			unlink(getName(mi, "xml").c_str());
			unlink(getName(mi, "jpg").c_str());
			break;
		}
}

void cYTCache::cancelAll(MI_MOVIE_INFO::miSource source)
{
	{
		OpenThreads::ScopedLock<OpenThreads::Mutex> m_lock(mutex);
		if (pending.empty())
			return;
		if (pending.size() > 1)
			for (std::vector<MI_MOVIE_INFO>::iterator it = pending.begin() + 1; it != pending.end();){
				if ((*it).source == source) {
					failed.push_back(*it);
					it = pending.erase(it);
				} else
					++it;
			}
		if (pending.front().source != source)
			return;
	}

	cancelled = true;
	while (thread)
		usleep(100000);

	{
		OpenThreads::ScopedLock<OpenThreads::Mutex> m_lock(mutex);
		cancelled = false;
		if (!pending.empty()) {
			if (pthread_create(&thread, NULL, downloadThread, this)) {
				perror("pthread_create");
				return;
			}
			pthread_detach(thread);
		}
	}
}

std::vector<MI_MOVIE_INFO> cYTCache::getFailed(MI_MOVIE_INFO::miSource source)
{
	OpenThreads::ScopedLock<OpenThreads::Mutex> m_lock(mutex);
	std::vector<MI_MOVIE_INFO> res;
	for (std::vector<MI_MOVIE_INFO>::iterator it = failed.begin(); it != failed.end(); ++it)
		if ((*it).source == source)
			res.push_back(*it);
	return res;
}

std::vector<MI_MOVIE_INFO> cYTCache::getCompleted(MI_MOVIE_INFO::miSource source)
{
	OpenThreads::ScopedLock<OpenThreads::Mutex> m_lock(mutex);
	std::vector<MI_MOVIE_INFO> res;
	for (std::vector<MI_MOVIE_INFO>::iterator it = completed.begin(); it != completed.end(); ++it)
		if ((*it).source == source)
			res.push_back(*it);
	return res;
}

std::vector<MI_MOVIE_INFO> cYTCache::getPending(MI_MOVIE_INFO::miSource source, double *p_dltotal, double *p_dlnow, time_t *p_start)
{
	OpenThreads::ScopedLock<OpenThreads::Mutex> m_lock(mutex);
	std::vector<MI_MOVIE_INFO> res;
	for (std::vector<MI_MOVIE_INFO>::iterator it = pending.begin(); it != pending.end(); ++it)
		if ((*it).source == source)
			res.push_back(*it);
	if (!res.empty() && pending.front().source == source) {
		if (p_dltotal)
			*p_dltotal = dltotal;
		if (p_dlnow)
			*p_dlnow = dlnow;
		if (p_start)
			*p_start = dlstart;
	} else {
		if (p_dltotal)
			*p_dltotal = 0;
		if (p_dlnow)
			*p_dlnow = 0;
		if (p_start)
			*p_start = 0;
	}
	return res;
}

void cYTCache::clearFailed(MI_MOVIE_INFO *mi)
{
	OpenThreads::ScopedLock<OpenThreads::Mutex> m_lock(mutex);
	if (mi) {
		for (std::vector<MI_MOVIE_INFO>::iterator it = failed.begin(); it != failed.end(); ++it)
			if (compareMovieInfo(&(*it), mi)) {
				failed.erase(it);
				break;
		}
	} else
		failed.clear();
}

void cYTCache::clearCompleted(MI_MOVIE_INFO *mi)
{
	OpenThreads::ScopedLock<OpenThreads::Mutex> m_lock(mutex);
	if (mi) {
		for (std::vector<MI_MOVIE_INFO>::iterator it = completed.begin(); it != completed.end(); ++it)
			if (compareMovieInfo(&(*it), mi)) {
				completed.erase(it);
				break;
		}
	} else
		completed.clear();
}

void cYTCache::clearFailed(MI_MOVIE_INFO::miSource source)
{
	OpenThreads::ScopedLock<OpenThreads::Mutex> m_lock(mutex);
	for (std::vector<MI_MOVIE_INFO>::iterator it = failed.begin(); it != failed.end();)
		if ((*it).source == source)
			it = failed.erase(it);
		else
			++it;
}

void cYTCache::clearCompleted(MI_MOVIE_INFO::miSource source)
{
	OpenThreads::ScopedLock<OpenThreads::Mutex> m_lock(mutex);
	for (std::vector<MI_MOVIE_INFO>::iterator it = completed.begin(); it != completed.end();)
		if ((*it).source == source)
			it = completed.erase(it);
		else
			++it;
}

bool cYTCache::compareMovieInfo(MI_MOVIE_INFO *a, MI_MOVIE_INFO *b)
{
	return a->ytid == b->ytid && a->ytitag == b->ytitag && a->source == b->source;
}
