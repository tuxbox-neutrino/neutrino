/*
	Based up Neutrino-GUI - Tuxbox-Project
	Copyright (C) 2001 by Steffen Hehn 'McClean'

	License: GPL

	This program is free software; you can redistribute it and/or
	modify it under the terms of the GNU General Public
	License as published by the Free Software Foundation; either
	version 2 of the License, or (at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
	General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program. If not, see <http://www.gnu.org/licenses/>.

	***********************************************************

	Module Name: mb_functions.cpp

	Description: Sorting helpers for moviebrowser

	(C) 2016, 2026 Thilo Graf 'dbt'
*/

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "mb_functions.h"

#include <driver/movieinfo.h>

#include <algorithm>
#include <cctype>

bool sortDirection = 0;

static bool compare_to_lower(const char a, const char b)
{
	return std::tolower(static_cast<unsigned char>(a)) < std::tolower(static_cast<unsigned char>(b));
}

// sort operators
static bool sortByTitle(const MI_MOVIE_INFO* a, const MI_MOVIE_INFO* b)
{
	if (std::lexicographical_compare(a->epgTitle.begin(), a->epgTitle.end(), b->epgTitle.begin(), b->epgTitle.end(), compare_to_lower))
		return true;
	if (std::lexicographical_compare(b->epgTitle.begin(), b->epgTitle.end(), a->epgTitle.begin(), a->epgTitle.end(), compare_to_lower))
		return false;
	return a->file.Time < b->file.Time;
}
static bool sortByGenre(const MI_MOVIE_INFO* a, const MI_MOVIE_INFO* b)
{
	if (std::lexicographical_compare(a->epgInfo1.begin(), a->epgInfo1.end(), b->epgInfo1.begin(), b->epgInfo1.end(), compare_to_lower))
		return true;
	if (std::lexicographical_compare(b->epgInfo1.begin(), b->epgInfo1.end(), a->epgInfo1.begin(), a->epgInfo1.end(), compare_to_lower))
		return false;
	return sortByTitle(a, b);
}
static bool sortByChannel(const MI_MOVIE_INFO* a, const MI_MOVIE_INFO* b)
{
	if (std::lexicographical_compare(a->channelName.begin(), a->channelName.end(), b->channelName.begin(), b->channelName.end(), compare_to_lower))
		return true;
	if (std::lexicographical_compare(b->channelName.begin(), b->channelName.end(), a->channelName.begin(), a->channelName.end(), compare_to_lower))
		return false;
	return sortByTitle(a, b);
}
static bool sortByFileName(const MI_MOVIE_INFO* a, const MI_MOVIE_INFO* b)
{
	const std::string a_file_name = a->file.getFileName();
	const std::string b_file_name = b->file.getFileName();

	if (std::lexicographical_compare(a_file_name.begin(), a_file_name.end(), b_file_name.begin(), b_file_name.end(), compare_to_lower))
		return true;
	if (std::lexicographical_compare(b_file_name.begin(), b_file_name.end(), a_file_name.begin(), a_file_name.end(), compare_to_lower))
		return false;
	return a->file.Time < b->file.Time;
}
static bool sortByRecordDate(const MI_MOVIE_INFO* a, const MI_MOVIE_INFO* b)
{
	if (sortDirection)
		return a->file.Time > b->file.Time ;
	else
		return a->file.Time < b->file.Time ;
}
static bool sortBySize(const MI_MOVIE_INFO* a, const MI_MOVIE_INFO* b)
{
	if (sortDirection)
		return a->file.Size > b->file.Size;
	else
		return a->file.Size < b->file.Size;
}
static bool sortByAge(const MI_MOVIE_INFO* a, const MI_MOVIE_INFO* b)
{
	if (sortDirection)
		return a->parentalLockAge > b->parentalLockAge;
	else
		return a->parentalLockAge < b->parentalLockAge;
}
static bool sortByRating(const MI_MOVIE_INFO* a, const MI_MOVIE_INFO* b)
{
	if (sortDirection)
		return a->rating > b->rating;
	else
		return a->rating < b->rating;
}
static bool sortByQuality(const MI_MOVIE_INFO* a, const MI_MOVIE_INFO* b)
{
	if (sortDirection)
		return a->quality > b->quality;
	else
		return a->quality < b->quality;
}
static bool sortByDir(const MI_MOVIE_INFO* a, const MI_MOVIE_INFO* b)
{
	if (sortDirection)
		return a->dirItNr > b->dirItNr;
	else
		return a->dirItNr < b->dirItNr;
}
static bool sortByLastPlay(const MI_MOVIE_INFO* a, const MI_MOVIE_INFO* b)
{
	if (sortDirection)
		return a->dateOfLastPlay > b->dateOfLastPlay;
	else
		return a->dateOfLastPlay < b->dateOfLastPlay;
}

bool (* const sortBy[MB_INFO_MAX_NUMBER+1])(const MI_MOVIE_INFO* a, const MI_MOVIE_INFO* b) =
{
	&sortByFileName,	//MB_INFO_FILENAME		= 0,
	&sortByDir, 		//MB_INFO_FILEPATH		= 1,
	&sortByTitle, 		//MB_INFO_TITLE			= 2,
	NULL, 			//MB_INFO_SERIE 		= 3,
	&sortByGenre, 		//MB_INFO_INFO1			= 4,
	NULL, 			//MB_INFO_MAJOR_GENRE		= 5,
	NULL, 			//MB_INFO_MINOR_GENRE 		= 6,
	NULL, 			//MB_INFO_INFO2 		= 7,
	&sortByAge, 		//MB_INFO_PARENTAL_LOCKAGE	= 8,
	&sortByChannel, 	//MB_INFO_CHANNEL		= 9,
	NULL, 			//MB_INFO_BOOKMARK		= 10,
	&sortByQuality, 	//MB_INFO_QUALITY		= 11,
	&sortByLastPlay, 	//MB_INFO_PREVPLAYDATE 		= 12,
	&sortByRecordDate, 	//MB_INFO_RECORDDATE		= 13,
	NULL, 			//MB_INFO_PRODDATE 		= 14,
	NULL, 			//MB_INFO_COUNTRY 		= 15,
	NULL, 			//MB_INFO_GEOMETRIE 		= 16,
	NULL, 			//MB_INFO_AUDIO 		= 17,
	NULL, 			//MB_INFO_LENGTH 		= 18,
	&sortBySize, 		//MB_INFO_SIZE 			= 19,
	&sortByRating,		//MB_INFO_RATING		= 20,
	NULL,			//MB_INFO_SPACER		= 21,
	NULL,		 	//MB_INFO_RECORDTIME		= 22,
	NULL			//MB_INFO_MAX_NUMBER		= 23
};
