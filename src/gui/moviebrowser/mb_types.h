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

	Module Name: mb_types.h

	Description: Implementation of the CMovieBrowser class
	             This class provides a filebrowser window to view, select and start a movies from HD.
	             This class does replace the Filebrowser

	Date:	   Nov 2005

	Author: Guenther@tuxbox.berlios.org
		based on code of Steffen Hehn 'McClean'

	(C) 2009-2014 Stefan Seyfried
	(C) 2016      Sven Hoefer

	outsourced:
	(C) 2016, Thilo Graf 'dbt'
*/

#include <gui/widget/menue.h>

#ifndef __MB_TYPES__
#define __MB_TYPES__

/* !!!! Do NOT change the order of the enum, just add items at the end !!!! */
typedef enum
{
	MB_INFO_FILENAME 		= 0,
	MB_INFO_FILEPATH 		= 1,
	MB_INFO_TITLE 			= 2,
	MB_INFO_SERIE 			= 3,
	MB_INFO_INFO1 			= 4,
	MB_INFO_MAJOR_GENRE	 	= 5,
	MB_INFO_MINOR_GENRE	 	= 6,
	MB_INFO_INFO2 			= 7,
	MB_INFO_PARENTAL_LOCKAGE	= 8,
	MB_INFO_CHANNEL 		= 9,
	MB_INFO_BOOKMARK		= 10,
	MB_INFO_QUALITY 		= 11,
	MB_INFO_PREVPLAYDATE 		= 12,
	MB_INFO_RECORDDATE 		= 13,
	MB_INFO_PRODDATE 		= 14,
	MB_INFO_COUNTRY 		= 15,
	MB_INFO_GEOMETRIE 		= 16,
	MB_INFO_AUDIO 			= 17,
	MB_INFO_LENGTH 			= 18,
	MB_INFO_SIZE 			= 19,
	MB_INFO_RATING			= 20,
	MB_INFO_SPACER			= 21,
	MB_INFO_RECORDTIME 		= 22,
	MB_INFO_PERCENT_ELAPSED 	= 23,
	MB_INFO_MAX_NUMBER		= 24 	// MUST be allways the last item in the list
}MB_INFO_ITEM;

typedef enum
{
	MB_DIRECTION_AUTO = 0,
	MB_DIRECTION_UP = 1,
	MB_DIRECTION_DOWN = 2,
	MB_DIRECTION_MAX_NUMBER = 3	// MUST be allways the last item in the list
}MB_DIRECTION;

typedef struct
{
	MB_INFO_ITEM item;
	MB_DIRECTION direction;
}MB_SORTING;

typedef enum
{
	MB_STORAGE_TYPE_UNDEFINED = 0,
	MB_STORAGE_TYPE_NFS = 1,
	MB_STORAGE_TYPE_VLC = 2,
	MB_STORAGE_MAX_NUMBER = 3	// MUST be allways the last item in the list
}MB_STORAGE_TYPE;

typedef struct
{
	MB_INFO_ITEM item;
	std::string optionString;
	int optionVar;
}MB_FILTER;

typedef enum
{
	MB_FOCUS_BROWSER = 0,
	MB_FOCUS_LAST_PLAY = 1,
	MB_FOCUS_LAST_RECORD = 2,
	MB_FOCUS_MOVIE_INFO1 = 3,
	MB_FOCUS_MOVIE_INFO2 = 4,
	MB_FOCUS_FILTER = 5,
	MB_FOCUS_MAX_NUMBER = 6	// MUST be allways the last item in the list
}MB_FOCUS;

typedef enum
{
	MB_GUI_BROWSER_ONLY = 0,
	MB_GUI_MOVIE_INFO = 1,
	MB_GUI_LAST_PLAY = 2,
	MB_GUI_LAST_RECORD = 3,
	MB_GUI_FILTER = 4,
	MB_GUI_MAX_NUMBER = 5	// MUST be allways the last item in the list
}MB_GUI;

typedef enum
{
	MB_PARENTAL_LOCK_OFF = 0,
	MB_PARENTAL_LOCK_ACTIVE = 1,
	MB_PARENTAL_LOCK_OFF_TMP = 2, // use this to activate the lock temporarily until next dbox start up
	MB_PARENTAL_LOCK_MAX_NUMBER = 3  // MUST be allways the last item in the list
}MB_PARENTAL_LOCK;

typedef struct
{
    std::string name;
    int* used;
}MB_DIR;

typedef enum
{
	MB_SHOW_RECORDS,
	MB_SHOW_FILES,
	MB_SHOW_YT
} MB_SHOW_MODE;

typedef enum
{
	DIR_STATE_UNKNOWN = 0,
	DIR_STATE_SERVER_DOWN = 1,
	DIR_STATE_NOT_MOUNTED = 2,
	DIR_STATE_MOUNTED = 3,
	DIR_STATE_DISABLED = 4
} DIR_STATE;

// EPG Genre, taken from epgview, TODO: might be splitted in major/minor to increase handling
#define GENRE_ALL_COUNT 76
const CMenuOptionChooser::keyval GENRE_ALL[GENRE_ALL_COUNT] =
{
	{ 0x00, LOCALE_GENRE_UNKNOWN },
	{ 0x10, LOCALE_GENRE_MOVIE_0 },
	{ 0x11, LOCALE_GENRE_MOVIE_1 },
	{ 0x12, LOCALE_GENRE_MOVIE_2 },
	{ 0x13, LOCALE_GENRE_MOVIE_3 },
	{ 0x14, LOCALE_GENRE_MOVIE_4 },
	{ 0x15, LOCALE_GENRE_MOVIE_5 },
	{ 0x16, LOCALE_GENRE_MOVIE_6 },
	{ 0x17, LOCALE_GENRE_MOVIE_7 },
	{ 0x18, LOCALE_GENRE_MOVIE_8 },
	{ 0x20, LOCALE_GENRE_NEWS_0 },
	{ 0x21, LOCALE_GENRE_NEWS_1 },
	{ 0x22, LOCALE_GENRE_NEWS_2 },
	{ 0x23, LOCALE_GENRE_NEWS_3 },
	{ 0x24, LOCALE_GENRE_NEWS_4 },
	{ 0x30, LOCALE_GENRE_SHOW_0 },
	{ 0x31, LOCALE_GENRE_SHOW_1 },
	{ 0x32, LOCALE_GENRE_SHOW_2 },
	{ 0x33, LOCALE_GENRE_SHOW_3 },
	{ 0x40, LOCALE_GENRE_SPORTS_0 },
	{ 0x41, LOCALE_GENRE_SPORTS_1 },
	{ 0x42, LOCALE_GENRE_SPORTS_2 },
	{ 0x43, LOCALE_GENRE_SPORTS_3 },
	{ 0x44, LOCALE_GENRE_SPORTS_4 },
	{ 0x45, LOCALE_GENRE_SPORTS_5 },
	{ 0x46, LOCALE_GENRE_SPORTS_6 },
	{ 0x47, LOCALE_GENRE_SPORTS_7 },
	{ 0x48, LOCALE_GENRE_SPORTS_8 },
	{ 0x49, LOCALE_GENRE_SPORTS_9 },
	{ 0x4A, LOCALE_GENRE_SPORTS_10 },
	{ 0x4B, LOCALE_GENRE_SPORTS_11 },
	{ 0x50, LOCALE_GENRE_CHILDRENS_PROGRAMMES_0 },
	{ 0x51, LOCALE_GENRE_CHILDRENS_PROGRAMMES_1 },
	{ 0x52, LOCALE_GENRE_CHILDRENS_PROGRAMMES_2 },
	{ 0x53, LOCALE_GENRE_CHILDRENS_PROGRAMMES_3 },
	{ 0x54, LOCALE_GENRE_CHILDRENS_PROGRAMMES_4 },
	{ 0x55, LOCALE_GENRE_CHILDRENS_PROGRAMMES_5 },
	{ 0x60, LOCALE_GENRE_MUSIC_DANCE_0 },
	{ 0x61, LOCALE_GENRE_MUSIC_DANCE_1 },
	{ 0x62, LOCALE_GENRE_MUSIC_DANCE_2 },
	{ 0x63, LOCALE_GENRE_MUSIC_DANCE_3 },
	{ 0x64, LOCALE_GENRE_MUSIC_DANCE_4 },
	{ 0x65, LOCALE_GENRE_MUSIC_DANCE_5 },
	{ 0x66, LOCALE_GENRE_MUSIC_DANCE_6 },
	{ 0x70, LOCALE_GENRE_ARTS_0 },
	{ 0x71, LOCALE_GENRE_ARTS_1 },
	{ 0x72, LOCALE_GENRE_ARTS_2 },
	{ 0x73, LOCALE_GENRE_ARTS_3 },
	{ 0x74, LOCALE_GENRE_ARTS_4 },
	{ 0x75, LOCALE_GENRE_ARTS_5 },
	{ 0x76, LOCALE_GENRE_ARTS_6 },
	{ 0x77, LOCALE_GENRE_ARTS_7 },
	{ 0x78, LOCALE_GENRE_ARTS_8 },
	{ 0x79, LOCALE_GENRE_ARTS_9 },
	{ 0x7A, LOCALE_GENRE_ARTS_10 },
	{ 0x7B, LOCALE_GENRE_ARTS_11 },
	{ 0x80, LOCALE_GENRE_SOCIAL_POLITICAL_0 },
	{ 0x81, LOCALE_GENRE_SOCIAL_POLITICAL_1 },
	{ 0x82, LOCALE_GENRE_SOCIAL_POLITICAL_2 },
	{ 0x83, LOCALE_GENRE_SOCIAL_POLITICAL_3 },
	{ 0x90, LOCALE_GENRE_DOCUS_MAGAZINES_0 },
	{ 0x91, LOCALE_GENRE_DOCUS_MAGAZINES_1 },
	{ 0x92, LOCALE_GENRE_DOCUS_MAGAZINES_2 },
	{ 0x93, LOCALE_GENRE_DOCUS_MAGAZINES_3 },
	{ 0x94, LOCALE_GENRE_DOCUS_MAGAZINES_4 },
	{ 0x95, LOCALE_GENRE_DOCUS_MAGAZINES_5 },
	{ 0x96, LOCALE_GENRE_DOCUS_MAGAZINES_6 },
	{ 0x97, LOCALE_GENRE_DOCUS_MAGAZINES_7 },
	{ 0xA0, LOCALE_GENRE_TRAVEL_HOBBIES_0 },
	{ 0xA1, LOCALE_GENRE_TRAVEL_HOBBIES_1 },
	{ 0xA2, LOCALE_GENRE_TRAVEL_HOBBIES_2 },
	{ 0xA3, LOCALE_GENRE_TRAVEL_HOBBIES_3 },
	{ 0xA4, LOCALE_GENRE_TRAVEL_HOBBIES_4 },
	{ 0xA5, LOCALE_GENRE_TRAVEL_HOBBIES_5 },
	{ 0xA6, LOCALE_GENRE_TRAVEL_HOBBIES_6 },
	{ 0xA7, LOCALE_GENRE_TRAVEL_HOBBIES_7 }
};

#endif /*__MB_TYPES__*/
