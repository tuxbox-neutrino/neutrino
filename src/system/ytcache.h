/*
	ytcache.h -- cache youtube movies

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

#ifndef __YTCACHE_H__
#define __YTCACHE_H__

#include <config.h>
#include <string>
#include <vector>

#include <OpenThreads/Thread>
#include <OpenThreads/Condition>

#include <gui/movieinfo.h>

class cYTCache
{
	private:
		pthread_t thread;
		bool cancelled;
		std::vector<MI_MOVIE_INFO> pending;
		std::vector<MI_MOVIE_INFO> completed;
		std::vector<MI_MOVIE_INFO> failed;
		OpenThreads::Mutex mutex;
		bool download(MI_MOVIE_INFO *mi);
		std::string getName(MI_MOVIE_INFO *mi, std::string ext = "mp4");
		static void *downloadThread(void *arg);
		static int curlProgress(void *clientp, double dltotal, double dlnow, double ultotal, double ulnow);
		bool compareMovieInfo(MI_MOVIE_INFO *a, MI_MOVIE_INFO *b);
	public:
		static cYTCache *getInstance();
		cYTCache();
		~cYTCache();
		bool useCachedCopy(MI_MOVIE_INFO *mi);
		bool addToCache(MI_MOVIE_INFO *mi);
		void cancel(MI_MOVIE_INFO *mi);
		void remove(MI_MOVIE_INFO *mi);
		void cancelAll(void);
		std::vector<MI_MOVIE_INFO> getCompleted(void);
		std::vector<MI_MOVIE_INFO> getFailed(void);
		std::vector<MI_MOVIE_INFO> getPending(void);
		void clearCompleted(MI_MOVIE_INFO *mi = NULL);
		void clearFailed(MI_MOVIE_INFO *mi = NULL);
};
#endif
