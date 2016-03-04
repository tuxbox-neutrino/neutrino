/*
	Copyright (C) 2015 TangoCash

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

#ifndef __TMDB__
#define __TMDB__

#include <curl/curl.h>
#include <curl/easy.h>

#include <string>
#include <gui/components/cc.h>

typedef struct {
	std::string epgtitle;
	std::string poster_path;
	std::string overview;
	std::string original_title;
	std::string release_date;
	std::string vote_average;
	int         vote_count;
	int         id;
	std::string media_type;
	int         result;
	int         runtime;
	std::string runtimes;
	std::string genres;
	int         episodes;
	int         seasons;
	std::string cast;
}tmdbinfo;

class cTmdb
{
	private:
		CURL *curl_handle;
		CComponentsForm *form;
		tmdbinfo minfo;

		CFrameBuffer *frameBuffer;
		int           ox, oy, sx, sy, toph;

		static size_t CurlWriteToString(void *ptr, size_t size, size_t nmemb, void *data);
		std::string encodeUrl(std::string txt);
		std::string decodeUrl(std::string url);
		std::string key; // tmdb api key
		bool getUrl(std::string &url, std::string &answer, CURL *_curl_handle = NULL);
		bool DownloadUrl(std::string url, std::string file, CURL *_curl_handle = NULL);
		bool GetMovieDetails(std::string lang);

	public:
		cTmdb(std::string epgtitle);
		~cTmdb();
		void exec();
		std::string CreateEPGText();

		std::string getTitle()				{ return minfo.epgtitle;}
		std::string getOrgTitle()			{ return minfo.original_title;}
		std::string getReleaseDate()			{ return minfo.release_date;}
		std::string getDescription()			{ return minfo.overview;}
		std::string getVote()				{ return minfo.vote_average;}
		bool        hasCover()				{ return !minfo.poster_path.empty();}
		bool        getBigCover(std::string cover)	{ return DownloadUrl("http://image.tmdb.org/t/p/w342" + minfo.poster_path, cover);}
		bool        getSmallCover(std::string cover)	{ return DownloadUrl("http://image.tmdb.org/t/p/w185" + minfo.poster_path, cover);}
		int         getResults()			{ return minfo.result;}
		int         getStars()				{ return (int) (atof(minfo.vote_average.c_str())+0.5);}
};

#endif
