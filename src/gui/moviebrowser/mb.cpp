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

	Module Name: mb.cpp

	Description: Implementation of the CMovieBrowser class
	             This class provides a filebrowser window to view, select and start a movies from HD.
	             This class does replace the Filebrowser

	Date:	   Nov 2005

	Author: Guenther@tuxbox.berlios.org
		based on code of Steffen Hehn 'McClean'

	(C) 2009-2015 Stefan Seyfried
	(C) 2016      Sven Hoefer

	outsourced:
	(C) 2016, Thilo Graf 'dbt'
*/

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <global.h>
#include <driver/screen_max.h>

#include <algorithm>
//#include <cstdlib>
#include "mb.h"
#include "mb_functions.h"
#include "mb_help.h"
#include <gui/filebrowser.h>
#include <gui/tmdb.h>
#include <gui/epgview.h>
#include <gui/widget/hintbox.h>
#include <gui/widget/icons.h>
#include <gui/components/cc.h>
#include <gui/widget/msgbox.h>
#include <gui/widget/stringinput.h>
#include <gui/widget/stringinput_ext.h>
#include <gui/widget/keyboard_input.h>
#include <dirent.h>
#include <sys/stat.h>
#include <gui/nfs.h>
#include <neutrino.h>
#include <sys/vfs.h> // for statfs
#include <sys/mount.h>
#include <utime.h>
//#include <unistd.h>
#include <gui/pictureviewer.h>
#include <driver/record.h>
#include <driver/display.h>
//#include <system/helpers.h>
#include <system/ytcache.h>
#include <zapit/debug.h>
#include <driver/moviecut.h>

#include <timerdclient/timerdclient.h>
#include <system/hddstat.h>

extern CPictureViewer * g_PicViewer;

#define my_scandir scandir64
#define my_alphasort alphasort64
typedef struct stat64 stat_struct;
typedef struct dirent64 dirent_struct;
#define my_stat stat64
#define TRACE  printf

#define NUMBER_OF_MOVIES_LAST 40 // This is the number of movies shown in last recored and last played list
#define MOVIE_SMSKEY_TIMEOUT 800

#define MESSAGEBOX_BROWSER_ROW_ITEM_COUNT 22
const CMenuOptionChooser::keyval MESSAGEBOX_BROWSER_ROW_ITEM[MESSAGEBOX_BROWSER_ROW_ITEM_COUNT] =
{
	{ MB_INFO_FILENAME,		LOCALE_MOVIEBROWSER_INFO_FILENAME },
	{ MB_INFO_FILEPATH,		LOCALE_MOVIEBROWSER_INFO_PATH },
	{ MB_INFO_TITLE,		LOCALE_MOVIEBROWSER_INFO_TITLE },
	{ MB_INFO_SERIE,		LOCALE_MOVIEBROWSER_INFO_SERIE },
	{ MB_INFO_INFO1,		LOCALE_MOVIEBROWSER_INFO_INFO1 },
	{ MB_INFO_MAJOR_GENRE,		LOCALE_MOVIEBROWSER_INFO_GENRE_MAJOR },
	{ MB_INFO_MINOR_GENRE,		LOCALE_MOVIEBROWSER_INFO_GENRE_MINOR },
	{ MB_INFO_INFO2,		LOCALE_MOVIEBROWSER_INFO_INFO2 },
	{ MB_INFO_PARENTAL_LOCKAGE,	LOCALE_MOVIEBROWSER_INFO_PARENTAL_LOCKAGE },
	{ MB_INFO_CHANNEL,		LOCALE_MOVIEBROWSER_INFO_CHANNEL },
	{ MB_INFO_BOOKMARK,		LOCALE_MOVIEBROWSER_MENU_MAIN_BOOKMARKS },
	{ MB_INFO_QUALITY,		LOCALE_MOVIEBROWSER_INFO_QUALITY },
	{ MB_INFO_PREVPLAYDATE,		LOCALE_MOVIEBROWSER_INFO_PREVPLAYDATE },
	{ MB_INFO_RECORDDATE,		LOCALE_MOVIEBROWSER_INFO_RECORDDATE },
	{ MB_INFO_PRODDATE,		LOCALE_MOVIEBROWSER_INFO_PRODYEAR },
	{ MB_INFO_COUNTRY,		LOCALE_MOVIEBROWSER_INFO_PRODCOUNTRY },
	{ MB_INFO_GEOMETRIE,		LOCALE_MOVIEBROWSER_INFO_VIDEOFORMAT },
	{ MB_INFO_AUDIO,		LOCALE_MOVIEBROWSER_INFO_AUDIO },
	{ MB_INFO_LENGTH,		LOCALE_MOVIEBROWSER_INFO_LENGTH },
	{ MB_INFO_SIZE,			LOCALE_MOVIEBROWSER_INFO_SIZE },
	{ MB_INFO_RATING,		LOCALE_MOVIEBROWSER_INFO_RATING },
	{ MB_INFO_SPACER,		LOCALE_MOVIEBROWSER_INFO_SPACER }
};

#define MESSAGEBOX_YES_NO_OPTIONS_COUNT 2
const CMenuOptionChooser::keyval MESSAGEBOX_YES_NO_OPTIONS[MESSAGEBOX_YES_NO_OPTIONS_COUNT] =
{
	{ 0, LOCALE_MESSAGEBOX_NO },
	{ 1, LOCALE_MESSAGEBOX_YES }
};

#define MESSAGEBOX_PARENTAL_LOCK_OPTIONS_COUNT 3
const CMenuOptionChooser::keyval MESSAGEBOX_PARENTAL_LOCK_OPTIONS[MESSAGEBOX_PARENTAL_LOCK_OPTIONS_COUNT] =
{
	{ 1, LOCALE_MOVIEBROWSER_MENU_PARENTAL_LOCK_ACTIVATED_YES },
	{ 0, LOCALE_MOVIEBROWSER_MENU_PARENTAL_LOCK_ACTIVATED_NO },
	{ 2, LOCALE_MOVIEBROWSER_MENU_PARENTAL_LOCK_ACTIVATED_NO_TEMP }
};

#define MESSAGEBOX_PARENTAL_LOCKAGE_OPTION_COUNT 6
const CMenuOptionChooser::keyval MESSAGEBOX_PARENTAL_LOCKAGE_OPTIONS[MESSAGEBOX_PARENTAL_LOCKAGE_OPTION_COUNT] =
{
	{ 0,  LOCALE_MOVIEBROWSER_INFO_PARENTAL_LOCKAGE_0YEAR },
	{ 6,  LOCALE_MOVIEBROWSER_INFO_PARENTAL_LOCKAGE_6YEAR },
	{ 12, LOCALE_MOVIEBROWSER_INFO_PARENTAL_LOCKAGE_12YEAR },
	{ 16, LOCALE_MOVIEBROWSER_INFO_PARENTAL_LOCKAGE_16YEAR },
	{ 18, LOCALE_MOVIEBROWSER_INFO_PARENTAL_LOCKAGE_18YEAR },
	{ 99, LOCALE_MOVIEBROWSER_INFO_PARENTAL_LOCKAGE_ALWAYS }
};

#define TITLE_BACKGROUND_COLOR ((CFBWindow::color_t)COL_MENUHEAD_PLUS_0)
#define TITLE_FONT_COLOR COL_MENUHEAD_TEXT

#define TITLE_FONT g_Font[SNeutrinoSettings::FONT_TYPE_MENU_TITLE]
#define FOOT_FONT g_Font[SNeutrinoSettings::FONT_TYPE_MENU_FOOT]

const neutrino_locale_t m_localizedItemName[MB_INFO_MAX_NUMBER+1] =
{
	LOCALE_MOVIEBROWSER_SHORT_FILENAME,
	LOCALE_MOVIEBROWSER_SHORT_PATH,
	LOCALE_MOVIEBROWSER_SHORT_TITLE,
	LOCALE_MOVIEBROWSER_SHORT_SERIE,
	LOCALE_MOVIEBROWSER_SHORT_INFO1,
	LOCALE_MOVIEBROWSER_SHORT_GENRE_MAJOR,
	LOCALE_MOVIEBROWSER_SHORT_GENRE_MINOR,
	LOCALE_MOVIEBROWSER_SHORT_INFO2,
	LOCALE_MOVIEBROWSER_SHORT_PARENTAL_LOCKAGE,
	LOCALE_MOVIEBROWSER_SHORT_CHANNEL,
	LOCALE_MOVIEBROWSER_SHORT_BOOK,
	LOCALE_MOVIEBROWSER_SHORT_QUALITY,
	LOCALE_MOVIEBROWSER_SHORT_PREVPLAYDATE,
	LOCALE_MOVIEBROWSER_SHORT_RECORDDATE,
	LOCALE_MOVIEBROWSER_SHORT_PRODYEAR,
	LOCALE_MOVIEBROWSER_SHORT_COUNTRY,
	LOCALE_MOVIEBROWSER_SHORT_FORMAT,
	LOCALE_MOVIEBROWSER_SHORT_AUDIO,
	LOCALE_MOVIEBROWSER_SHORT_LENGTH,
	LOCALE_MOVIEBROWSER_SHORT_SIZE,
	LOCALE_MOVIEBROWSER_SHORT_RATING,
	LOCALE_MOVIEBROWSER_SHORT_SPACER,
	NONEXISTANT_LOCALE
};

/* default row size in percent for any element */
#define	MB_ROW_WIDTH_FILENAME		22
#define	MB_ROW_WIDTH_FILEPATH		22
#define	MB_ROW_WIDTH_TITLE		35
#define	MB_ROW_WIDTH_SERIE		15
#define	MB_ROW_WIDTH_INFO1		15
#define	MB_ROW_WIDTH_MAJOR_GENRE	15
#define	MB_ROW_WIDTH_MINOR_GENRE	8
#define	MB_ROW_WIDTH_INFO2		25
#define	MB_ROW_WIDTH_PARENTAL_LOCKAGE	4
#define	MB_ROW_WIDTH_CHANNEL		15
#define	MB_ROW_WIDTH_BOOKMARK		6
#define	MB_ROW_WIDTH_QUALITY		10
#define	MB_ROW_WIDTH_PREVPLAYDATE	12
#define	MB_ROW_WIDTH_RECORDDATE		12
#define	MB_ROW_WIDTH_PRODDATE		8
#define	MB_ROW_WIDTH_COUNTRY		8
#define	MB_ROW_WIDTH_GEOMETRIE		8
#define	MB_ROW_WIDTH_AUDIO		8
#define	MB_ROW_WIDTH_LENGTH		10
#define	MB_ROW_WIDTH_SIZE 		12
#define	MB_ROW_WIDTH_RATING		5
#define	MB_ROW_WIDTH_SPACER		1

const int m_defaultRowWidth[MB_INFO_MAX_NUMBER+1] =
{
	MB_ROW_WIDTH_FILENAME,
	MB_ROW_WIDTH_FILEPATH,
	MB_ROW_WIDTH_TITLE,
	MB_ROW_WIDTH_SERIE,
	MB_ROW_WIDTH_INFO1,
	MB_ROW_WIDTH_MAJOR_GENRE,
	MB_ROW_WIDTH_MINOR_GENRE,
	MB_ROW_WIDTH_INFO2,
	MB_ROW_WIDTH_PARENTAL_LOCKAGE,
	MB_ROW_WIDTH_CHANNEL,
	MB_ROW_WIDTH_BOOKMARK,
	MB_ROW_WIDTH_QUALITY,
	MB_ROW_WIDTH_PREVPLAYDATE,
	MB_ROW_WIDTH_RECORDDATE,
	MB_ROW_WIDTH_PRODDATE,
	MB_ROW_WIDTH_COUNTRY,
	MB_ROW_WIDTH_GEOMETRIE,
	MB_ROW_WIDTH_AUDIO,
	MB_ROW_WIDTH_LENGTH,
	MB_ROW_WIDTH_SIZE,
	MB_ROW_WIDTH_RATING,
	MB_ROW_WIDTH_SPACER,
	0 //MB_ROW_WIDTH_MAX_NUMBER
};
static MI_MOVIE_INFO* playing_info;
//------------------------------------------------------------------------
// sorting
//------------------------------------------------------------------------
#define FILEBROWSER_NUMBER_OF_SORT_VARIANTS 5



CMovieBrowser::CMovieBrowser(): configfile ('\t')
{
	init();
}

CMovieBrowser::~CMovieBrowser()
{
	//TRACE("[mb] del\n");

	m_dir.clear();

	m_dirNames.clear();

	m_vMovieInfo.clear();
	m_vHandleBrowserList.clear();
	m_vHandleRecordList.clear();
	m_vHandlePlayList.clear();
	m_vHandleSerienames.clear();

	clearListLines();

	if (m_detailsLine)
		delete m_detailsLine;

	if (m_channelLogo)
		delete m_channelLogo;

	if (m_movieCover)
		delete m_movieCover;

	if (m_header)
		delete m_header;
}

void CMovieBrowser::clearListLines()
{
	for (int i = 0; i < MB_MAX_ROWS; i++)
	{
		m_browserListLines.lineArray[i].clear();
		m_FilterLines.lineArray[i].clear();
	}
	m_browserListLines.Icon.clear();
	m_browserListLines.marked.clear();

	for (int i = 0; i < 3; i++)
	{
		m_recordListLines.lineArray[i].clear();
		m_playListLines.lineArray[i].clear();
	}
	m_recordListLines.marked.clear();
	m_playListLines.marked.clear();
}

void CMovieBrowser::clearSelection()
{
	//TRACE("[mb]->%s\n", __func__);
	for (unsigned i = 0; i < m_vMovieInfo.size(); i++)
		m_vMovieInfo[i].marked = false;

	m_pcBrowser->clearMarked();
	m_pcLastPlay->clearMarked();
	m_pcLastRecord->clearMarked();
}

void CMovieBrowser::fileInfoStale(void)
{
	m_file_info_stale = true;
	m_seriename_stale = true;

	 // Also release memory buffers, since we have to reload this stuff next time anyhow
	m_dirNames.clear();

	m_vMovieInfo.clear();
	m_vHandleBrowserList.clear();
	m_vHandleRecordList.clear();
	m_vHandlePlayList.clear();
	m_vHandleSerienames.clear();

	clearListLines();
}

void CMovieBrowser::init(void)
{
	bool reinit_rows = false;
	int i = 0;
	//TRACE("[mb]->init\n");
	initGlobalSettings();
	loadSettings(&m_settings);

	m_file_info_stale = true;
	m_seriename_stale = true;

	framebuffer = CFrameBuffer::getInstance();
	m_pcBrowser = NULL;
	m_pcLastPlay = NULL;
	m_pcLastRecord = NULL;
	m_pcInfo1 = NULL;
	m_pcInfo2 = NULL;
	m_pcFilter = NULL;
	m_header = NULL;
	m_windowFocus = MB_FOCUS_BROWSER;

	m_textTitle = g_Locale->getText(LOCALE_MOVIEBROWSER_HEAD);

	m_currentStartPos = 0;

	m_movieSelectionHandler = NULL;
	m_currentBrowserSelection = 0;
	m_currentRecordSelection = 0;
	m_currentPlaySelection = 0;
	m_prevBrowserSelection = 0;
	m_prevRecordSelection = 0;
	m_prevPlaySelection = 0;

	m_storageType = MB_STORAGE_TYPE_NFS;

	m_parentalLock = m_settings.parentalLock;

	// check g_setting values
	if (m_settings.gui >= MB_GUI_MAX_NUMBER)
		m_settings.gui = MB_GUI_MOVIE_INFO;

	if (m_settings.sorting.direction >= MB_DIRECTION_MAX_NUMBER)
		m_settings.sorting.direction = MB_DIRECTION_DOWN;
	if (m_settings.sorting.item >= MB_INFO_MAX_NUMBER)
		m_settings.sorting.item = MB_INFO_TITLE;

	if (m_settings.filter.item >= MB_INFO_MAX_NUMBER)
		m_settings.filter.item = MB_INFO_MAX_NUMBER;

	if (m_settings.parentalLockAge >= MI_PARENTAL_MAX_NUMBER)
		m_settings.parentalLockAge = MI_PARENTAL_OVER18;
	if (m_settings.parentalLock >= MB_PARENTAL_LOCK_MAX_NUMBER)
		m_settings.parentalLock = MB_PARENTAL_LOCK_OFF;

	/* convert from old pixel-based to new percent values */
	if (m_settings.browserFrameHeight > 100)
		m_settings.browserFrameHeight = 50;

	if (m_settings.browserFrameHeight < MIN_BROWSER_FRAME_HEIGHT)
		m_settings.browserFrameHeight = MIN_BROWSER_FRAME_HEIGHT;
	if (m_settings.browserFrameHeight > MAX_BROWSER_FRAME_HEIGHT)
		m_settings.browserFrameHeight = MAX_BROWSER_FRAME_HEIGHT;

	/* the old code had row widths in pixels, not percent. Check if we have
	 * an old configuration (one of the rows hopefully was larger than 100 pixels... */
	for (i = 0; i < m_settings.browserRowNr; i++)
	{
		if (m_settings.browserRowWidth[i] > 100)
		{
			printf("[moviebrowser] old row config detected - converting...\n");
			reinit_rows = true;
			break;
		}
	}
	if (reinit_rows)
	{
		for (i = 0; i < m_settings.browserRowNr; i++)
			m_settings.browserRowWidth[i] = m_defaultRowWidth[m_settings.browserRowItem[i]];
	}

	initFrames();
	initRows();

	/* save settings here, because exec() will load them again... */
	if (reinit_rows)
		saveSettings(&m_settings);

	refreshLastPlayList();
	refreshLastRecordList();
	refreshBrowserList();
	refreshFilterList();
	g_PicViewer->getSupportedImageFormats(PicExts);
	show_mode = MB_SHOW_RECORDS; //FIXME

	filelist.clear();
	filelist_it = filelist.end();
	movielist.clear();

	m_detailsLine = NULL;
	m_channelLogo = NULL;
	m_movieCover = NULL;

	old_EpgId = 0;
	m_doRefresh = false;
	m_doLoadMovies = false;
}

void CMovieBrowser::initGlobalSettings(void)
{
	//TRACE("[mb]->initGlobalSettings\n");

	m_settings.gui = MB_GUI_MOVIE_INFO;

	m_settings.lastPlayMaxItems = NUMBER_OF_MOVIES_LAST;
	m_settings.lastRecordMaxItems = NUMBER_OF_MOVIES_LAST;

	m_settings.browser_serie_mode = 0;
	m_settings.serie_auto_create = 0;

	m_settings.sorting.item 	= MB_INFO_TITLE;
	m_settings.sorting.direction = MB_DIRECTION_DOWN;

	m_settings.filter.item = MB_INFO_MAX_NUMBER;
	m_settings.filter.optionString = g_Locale->getText(LOCALE_OPTIONS_OFF);
	m_settings.filter.optionVar = 0;

	m_settings.parentalLockAge = MI_PARENTAL_OVER18;
	m_settings.parentalLock = MB_PARENTAL_LOCK_OFF;

	m_settings.storageDirMovieUsed = true;
	m_settings.storageDirRecUsed = true;

	m_settings.reload = true;
	m_settings.remount = false;

	for (int i = 0; i < MB_MAX_DIRS; i++)
	{
		m_settings.storageDir[i] = "";
		m_settings.storageDirUsed[i] = 0;
	}

	/***** Browser List **************/
	m_settings.browserFrameHeight = 65; /* percent */

	m_settings.browserRowNr = 6;
	m_settings.browserRowItem[0] = MB_INFO_CHANNEL;
	m_settings.browserRowItem[1] = MB_INFO_TITLE;
	m_settings.browserRowItem[2] = MB_INFO_RECORDDATE;
	m_settings.browserRowItem[3] = MB_INFO_SIZE;
	m_settings.browserRowItem[4] = MB_INFO_LENGTH;
	m_settings.browserRowItem[5] = MB_INFO_INFO1;
	m_settings.browserRowItem[6] = MB_INFO_MAX_NUMBER;
	m_settings.browserRowItem[7] = MB_INFO_MAX_NUMBER;
	m_settings.browserRowItem[8] = MB_INFO_MAX_NUMBER;

	m_settings.browserRowWidth[0] = m_defaultRowWidth[m_settings.browserRowItem[0]];
	m_settings.browserRowWidth[1] = m_defaultRowWidth[m_settings.browserRowItem[1]];
	m_settings.browserRowWidth[2] = m_defaultRowWidth[m_settings.browserRowItem[2]];
	m_settings.browserRowWidth[3] = m_defaultRowWidth[m_settings.browserRowItem[3]];
	m_settings.browserRowWidth[4] = m_defaultRowWidth[m_settings.browserRowItem[4]];
	m_settings.browserRowWidth[5] = m_defaultRowWidth[m_settings.browserRowItem[5]];
	m_settings.browserRowWidth[6] = m_defaultRowWidth[m_settings.browserRowItem[6]];
	m_settings.browserRowWidth[7] = m_defaultRowWidth[m_settings.browserRowItem[7]];
	m_settings.browserRowWidth[8] = m_defaultRowWidth[m_settings.browserRowItem[8]];

	m_settings.browserAdditional = 0;

	m_settings.ts_only = 1;
	m_settings.ytmode = cYTFeedParser::MOST_POPULAR;
	m_settings.ytorderby = cYTFeedParser::ORDERBY_PUBLISHED;
	m_settings.ytresults = 10;
	m_settings.ytregion = "default";
	m_settings.ytquality = 37;
	m_settings.ytconcconn = 4;
	m_settings.ytsearch_history_max = 0;
	m_settings.ytsearch_history_size = 0;
	m_settings.ytthumbnaildir = "/tmp/ytparser";
}

void CMovieBrowser::initFrames(void)
{
	m_pcFontFoot = FOOT_FONT;
	m_pcFontTitle = TITLE_FONT;

	//TRACE("[mb]->%s\n", __func__);
	m_cBoxFrame.iWidth = 			framebuffer->getScreenWidthRel();
	m_cBoxFrame.iHeight = 			framebuffer->getScreenHeightRel();
	m_cBoxFrame.iX = 			getScreenStartX(m_cBoxFrame.iWidth);
	m_cBoxFrame.iY = 			getScreenStartY(m_cBoxFrame.iHeight);

	m_cBoxFrameTitleRel.iX =		0;
	m_cBoxFrameTitleRel.iY = 		0;
	m_cBoxFrameTitleRel.iWidth = 		m_cBoxFrame.iWidth;
	m_cBoxFrameTitleRel.iHeight = 		m_pcFontTitle->getHeight();

	const int pic_h = 39;
	m_cBoxFrameTitleRel.iHeight = std::max(m_cBoxFrameTitleRel.iHeight, pic_h);

	m_cBoxFrameBrowserList.iX = 		m_cBoxFrame.iX;
	m_cBoxFrameBrowserList.iY = 		m_cBoxFrame.iY + m_cBoxFrameTitleRel.iHeight;
	if (m_settings.browserAdditional)
		m_cBoxFrameBrowserList.iWidth =	m_cBoxFrame.iWidth / 3 * 2;
	else
		m_cBoxFrameBrowserList.iWidth =	m_cBoxFrame.iWidth;
	m_cBoxFrameBrowserList.iHeight = 	m_cBoxFrame.iHeight * m_settings.browserFrameHeight / 100;


	m_cBoxFrameFootRel.iX = 		m_cBoxFrameBrowserList.iX;
	m_cBoxFrameFootRel.iHeight = 		refreshFoot(false);
	m_cBoxFrameFootRel.iY = 		m_cBoxFrameBrowserList.iY + m_cBoxFrameBrowserList.iHeight;
	m_cBoxFrameFootRel.iWidth = 		m_cBoxFrame.iWidth;

	m_cBoxFrameLastPlayList.iX = 		m_cBoxFrameBrowserList.iX;
	m_cBoxFrameLastPlayList.iY = 		m_cBoxFrameBrowserList.iY ;
	m_cBoxFrameLastPlayList.iWidth = 	m_cBoxFrame.iWidth / 2;
	m_cBoxFrameLastPlayList.iHeight = 	m_cBoxFrameBrowserList.iHeight;

	m_cBoxFrameLastRecordList.iX = 		m_cBoxFrameLastPlayList.iX + m_cBoxFrameLastPlayList.iWidth;
	m_cBoxFrameLastRecordList.iY = 		m_cBoxFrameLastPlayList.iY;
	m_cBoxFrameLastRecordList.iWidth = 	m_cBoxFrame.iWidth - m_cBoxFrameLastPlayList.iWidth;
	m_cBoxFrameLastRecordList.iHeight =	m_cBoxFrameLastPlayList.iHeight;

	m_cBoxFrameInfo1.iX = 			m_cBoxFrameBrowserList.iX;
	m_cBoxFrameInfo1.iY = 			m_cBoxFrameFootRel.iY + m_cBoxFrameFootRel.iHeight + OFFSET_INTER;
	m_cBoxFrameInfo1.iWidth = 		m_cBoxFrame.iWidth;
	m_cBoxFrameInfo1.iHeight = 		m_cBoxFrame.iHeight - m_cBoxFrameBrowserList.iHeight - OFFSET_INTER - m_cBoxFrameFootRel.iHeight - m_cBoxFrameTitleRel.iHeight;

	m_cBoxFrameInfo2.iX = 			m_cBoxFrameBrowserList.iX + m_cBoxFrameBrowserList.iWidth;
	m_cBoxFrameInfo2.iY = 			m_cBoxFrameBrowserList.iY;
	m_cBoxFrameInfo2.iWidth = 		m_cBoxFrame.iWidth - m_cBoxFrameBrowserList.iWidth;
	m_cBoxFrameInfo2.iHeight = 		m_cBoxFrameBrowserList.iHeight;

	m_cBoxFrameFilter.iX = 			m_cBoxFrameInfo1.iX;
	m_cBoxFrameFilter.iY = 			m_cBoxFrameInfo1.iY;
	m_cBoxFrameFilter.iWidth = 		m_cBoxFrameInfo1.iWidth;
	m_cBoxFrameFilter.iHeight = 		m_cBoxFrameInfo1.iHeight;
}

void CMovieBrowser::initRows(void)
{
	//TRACE("[mb]->%s\n", __func__);

	/*
	   The "last played" / "last recorded" windows have only half the width, so
	   multiply the relative width with 2 and add 1 percent for safety to date row.
	   This addition is just usefull for l a r g e font settings.
	*/

	/***** Last Play List **************/
	m_settings.lastPlayRowNr = 3;
	m_settings.lastPlayRow[0] = MB_INFO_TITLE;
	m_settings.lastPlayRow[1] = MB_INFO_SPACER;
	m_settings.lastPlayRow[2] = MB_INFO_PREVPLAYDATE;
	m_settings.lastPlayRowWidth[2] = m_defaultRowWidth[m_settings.lastPlayRow[2]] * 2 + 1;
	m_settings.lastPlayRowWidth[1] = m_defaultRowWidth[m_settings.lastPlayRow[1]] * 2;
	m_settings.lastPlayRowWidth[0] = 100 - m_settings.lastPlayRowWidth[1] - m_settings.lastPlayRowWidth[2];

	/***** Last Record List **************/
	m_settings.lastRecordRowNr = 3;
	m_settings.lastRecordRow[0] = MB_INFO_TITLE;
	m_settings.lastRecordRow[1] = MB_INFO_SPACER;
	m_settings.lastRecordRow[2] = MB_INFO_RECORDDATE;
	m_settings.lastRecordRowWidth[2] = m_defaultRowWidth[m_settings.lastRecordRow[2]] * 2 + 1;
	m_settings.lastRecordRowWidth[1] = m_defaultRowWidth[m_settings.lastRecordRow[1]] * 2;
	m_settings.lastRecordRowWidth[0] = 100 - m_settings.lastRecordRowWidth[1] - m_settings.lastRecordRowWidth[2];
}

void CMovieBrowser::defaultSettings(MB_SETTINGS* /*settings*/)
{
	unlink(MOVIEBROWSER_SETTINGS_FILE);
	configfile.clear();
	initGlobalSettings();
}

bool CMovieBrowser::loadSettings(MB_SETTINGS* settings)
{
	//TRACE("[mb]->%s\n", __func__);
	bool result = configfile.loadConfig(MOVIEBROWSER_SETTINGS_FILE);
	if (!result) {
		TRACE("CMovieBrowser::loadSettings failed\n");
		return result;
	}

	settings->gui = (MB_GUI)configfile.getInt32("mb_gui", MB_GUI_MOVIE_INFO);

	settings->lastPlayMaxItems = configfile.getInt32("mb_lastPlayMaxItems", NUMBER_OF_MOVIES_LAST);
	settings->lastRecordMaxItems = configfile.getInt32("mb_lastRecordMaxItems", NUMBER_OF_MOVIES_LAST);
	settings->browser_serie_mode = configfile.getInt32("mb_browser_serie_mode", 0);
	settings->serie_auto_create = configfile.getInt32("mb_serie_auto_create", 0);
	settings->ts_only = configfile.getInt32("mb_ts_only", 1);

	settings->sorting.item = (MB_INFO_ITEM)configfile.getInt32("mb_sorting_item", MB_INFO_RECORDDATE);
	settings->sorting.direction = (MB_DIRECTION)configfile.getInt32("mb_sorting_direction", MB_DIRECTION_UP);

	settings->filter.item = (MB_INFO_ITEM)configfile.getInt32("mb_filter_item", MB_INFO_MAX_NUMBER);
	settings->filter.optionString = configfile.getString("mb_filter_optionString", g_Locale->getText(LOCALE_OPTIONS_OFF));
	settings->filter.optionVar = configfile.getInt32("mb_filter_optionVar", 0);

	if (settings->filter.item == MB_INFO_FILEPATH)
	{
		struct stat info;
		if (!(stat(settings->filter.optionString.c_str(), &info) == 0 && S_ISDIR(info.st_mode)))
		{
			//reset filter if directory not exists
			settings->filter.item = MB_INFO_MAX_NUMBER;
			settings->filter.optionString = g_Locale->getText(LOCALE_OPTIONS_OFF);
			settings->filter.optionVar = 0;
		}
	}

	settings->parentalLockAge = (MI_PARENTAL_LOCKAGE)configfile.getInt32("mb_parentalLockAge", MI_PARENTAL_OVER18);
	settings->parentalLock = (MB_PARENTAL_LOCK)configfile.getInt32("mb_parentalLock", MB_PARENTAL_LOCK_ACTIVE);

	settings->storageDirRecUsed = (bool)configfile.getInt32("mb_storageDir_rec", true);
	settings->storageDirMovieUsed = (bool)configfile.getInt32("mb_storageDir_movie", true);

	settings->reload = (bool)configfile.getInt32("mb_reload", true);
	settings->remount = (bool)configfile.getInt32("mb_remount", false);

	for (int i = 0; i < MB_MAX_DIRS; i++)
	{
		settings->storageDir[i] = configfile.getString("mb_dir_" + to_string(i), "");
		settings->storageDirUsed[i] = configfile.getInt32("mb_dir_used" + to_string(i), false);
	}
	/* these variables are used for the listframes */
	settings->browserFrameHeight = configfile.getInt32("mb_browserFrameHeight", 50);
	settings->browserRowNr = configfile.getInt32("mb_browserRowNr", 0);
	for (int i = 0; i < MB_MAX_ROWS && i < settings->browserRowNr; i++)
	{
		settings->browserRowItem[i] = (MB_INFO_ITEM)configfile.getInt32("mb_browserRowItem_" + to_string(i), MB_INFO_MAX_NUMBER);
		settings->browserRowWidth[i] = configfile.getInt32("mb_browserRowWidth_" + to_string(i), 50);
	}
	settings->browserAdditional = configfile.getInt32("mb_browserAdditional", 0);

	settings->ytmode = configfile.getInt32("mb_ytmode", cYTFeedParser::MOST_POPULAR);
	settings->ytorderby = configfile.getInt32("mb_ytorderby", cYTFeedParser::ORDERBY_PUBLISHED);
	settings->ytresults = configfile.getInt32("mb_ytresults", 10);
	settings->ytquality = configfile.getInt32("mb_ytquality", 37); // itag value (MP4, 1080p)
	settings->ytconcconn = configfile.getInt32("mb_ytconcconn", 4); // concurrent connections
	settings->ytregion = configfile.getString("mb_ytregion", "default");
	settings->ytsearch = configfile.getString("mb_ytsearch", "");
	settings->ytthumbnaildir = configfile.getString("mb_ytthumbnaildir", "/tmp/ytparser");
	settings->ytvid = configfile.getString("mb_ytvid", "");
	settings->ytsearch_history_max = configfile.getInt32("mb_ytsearch_history_max", 10);
	settings->ytsearch_history_size = configfile.getInt32("mb_ytsearch_history_size", 0);
	if (settings->ytsearch_history_size > settings->ytsearch_history_max)
		settings->ytsearch_history_size = settings->ytsearch_history_max;
	settings->ytsearch_history.clear();
	for (int i = 0; i < settings->ytsearch_history_size; i++) {
		std::string s = configfile.getString("mb_ytsearch_history_" + to_string(i));
		if (!s.empty())
			settings->ytsearch_history.push_back(configfile.getString("mb_ytsearch_history_" + to_string(i), ""));
	}
	settings->ytsearch_history_size = settings->ytsearch_history.size();
	return (result);
}

bool CMovieBrowser::saveSettings(MB_SETTINGS* settings)
{
	bool result = true;
	TRACE("[mb]->%s\n", __func__);

	configfile.setInt32("mb_lastPlayMaxItems", settings->lastPlayMaxItems);
	configfile.setInt32("mb_lastRecordMaxItems", settings->lastRecordMaxItems);
	configfile.setInt32("mb_browser_serie_mode", settings->browser_serie_mode);
	configfile.setInt32("mb_serie_auto_create", settings->serie_auto_create);
	configfile.setInt32("mb_ts_only", settings->ts_only);

	configfile.setInt32("mb_gui", settings->gui);

	configfile.setInt32("mb_sorting_item", settings->sorting.item);
	configfile.setInt32("mb_sorting_direction", settings->sorting.direction);

	configfile.setInt32("mb_filter_item", settings->filter.item);
	configfile.setString("mb_filter_optionString", settings->filter.optionString);
	configfile.setInt32("mb_filter_optionVar", settings->filter.optionVar);

	configfile.setInt32("mb_storageDir_rec", settings->storageDirRecUsed);
	configfile.setInt32("mb_storageDir_movie", settings->storageDirMovieUsed);

	configfile.setInt32("mb_parentalLockAge", settings->parentalLockAge);
	configfile.setInt32("mb_parentalLock", settings->parentalLock);

	configfile.setInt32("mb_reload", settings->reload);
	configfile.setInt32("mb_remount", settings->remount);

	for (int i = 0; i < MB_MAX_DIRS; i++)
	{
		configfile.setString("mb_dir_" + to_string(i), settings->storageDir[i]);
		configfile.setInt32("mb_dir_used" + to_string(i), settings->storageDirUsed[i]); // do not save this so far
	}
	/* these variables are used for the listframes */
	configfile.setInt32("mb_browserFrameHeight", settings->browserFrameHeight);
	configfile.setInt32("mb_browserRowNr",settings->browserRowNr);
	for (int i = 0; i < MB_MAX_ROWS && i < settings->browserRowNr; i++)
	{
		configfile.setInt32("mb_browserRowItem_" + to_string(i), settings->browserRowItem[i]);
		configfile.setInt32("mb_browserRowWidth_" + to_string(i), settings->browserRowWidth[i]);
	}
	configfile.setInt32("mb_browserAdditional", settings->browserAdditional);

	configfile.setInt32("mb_ytmode", settings->ytmode);
	configfile.setInt32("mb_ytorderby", settings->ytorderby);
	configfile.setInt32("mb_ytresults", settings->ytresults);
	configfile.setInt32("mb_ytquality", settings->ytquality);
	configfile.setInt32("mb_ytconcconn", settings->ytconcconn);
	configfile.setString("mb_ytregion", settings->ytregion);
	configfile.setString("mb_ytsearch", settings->ytsearch);
	configfile.setString("mb_ytthumbnaildir", settings->ytthumbnaildir);
	configfile.setString("mb_ytvid", settings->ytvid);

	settings->ytsearch_history_size = settings->ytsearch_history.size();
	if (settings->ytsearch_history_size > settings->ytsearch_history_max)
		settings->ytsearch_history_size = settings->ytsearch_history_max;
	configfile.setInt32("mb_ytsearch_history_max", settings->ytsearch_history_max);
	configfile.setInt32("mb_ytsearch_history_size", settings->ytsearch_history_size);
	std::list<std::string>:: iterator it = settings->ytsearch_history.begin();
	for (int i = 0; i < settings->ytsearch_history_size; i++, ++it)
		configfile.setString("mb_ytsearch_history_" + to_string(i), *it);

	if (configfile.getModifiedFlag())
		configfile.saveConfig(MOVIEBROWSER_SETTINGS_FILE);
	return (result);
}

int CMovieBrowser::exec(CMenuTarget* parent, const std::string & actionKey)
{
	int returnval = menu_return::RETURN_REPAINT;

	if (actionKey == "loaddefault")
	{
		defaultSettings(&m_settings);
	}
	else if (actionKey == "show_movie_info_menu")
	{
		if (m_movieSelectionHandler != NULL)
			return showMovieInfoMenu(m_movieSelectionHandler);
	}
	else if (actionKey == "show_movie_cut_menu")
	{
		if (m_movieSelectionHandler != NULL)
		{
			if (parent)
				parent->hide();
			return showMovieCutMenu();
		}
	}
	else if (actionKey == "save_movie_info")
	{
		m_movieInfo.saveMovieInfo(*m_movieSelectionHandler);
	}
	else if (actionKey == "save_movie_info_all")
	{
		std::vector<MI_MOVIE_INFO*> * current_list=NULL;

		if (m_windowFocus == MB_FOCUS_BROWSER)          current_list = &m_vHandleBrowserList;
		else if (m_windowFocus == MB_FOCUS_LAST_PLAY)   current_list = &m_vHandlePlayList;
		else if (m_windowFocus == MB_FOCUS_LAST_RECORD) current_list = &m_vHandleRecordList ;

		if (current_list == NULL || m_movieSelectionHandler == NULL)
			return returnval;

		CHintBox loadBox(LOCALE_MOVIEBROWSER_HEAD,g_Locale->getText(LOCALE_MOVIEBROWSER_INFO_HEAD_UPDATE));
		loadBox.paint();
		for (unsigned int i = 0; i< current_list->size();i++)
		{
			if (!((*current_list)[i]->parentalLockAge != 0 && movieInfoUpdateAllIfDestEmptyOnly == true) &&
					movieInfoUpdateAll[MB_INFO_TITLE])
				(*current_list)[i]->parentalLockAge = m_movieSelectionHandler->parentalLockAge;

			if (!(!(*current_list)[i]->serieName.empty() && movieInfoUpdateAllIfDestEmptyOnly == true) &&
					movieInfoUpdateAll[MB_INFO_SERIE])
				(*current_list)[i]->serieName = m_movieSelectionHandler->serieName;

			if (!(!(*current_list)[i]->productionCountry.empty() && movieInfoUpdateAllIfDestEmptyOnly == true) &&
					movieInfoUpdateAll[MB_INFO_COUNTRY])
				(*current_list)[i]->productionCountry = m_movieSelectionHandler->productionCountry;

			if (!((*current_list)[i]->genreMajor!=0 && movieInfoUpdateAllIfDestEmptyOnly == true) &&
					movieInfoUpdateAll[MB_INFO_MAJOR_GENRE])
				(*current_list)[i]->genreMajor = m_movieSelectionHandler->genreMajor;

			if (!((*current_list)[i]->rating!=0 && movieInfoUpdateAllIfDestEmptyOnly == true) &&
					movieInfoUpdateAll[MB_INFO_RATING])
				(*current_list)[i]->rating = m_movieSelectionHandler->rating;

			if (!((*current_list)[i]->quality!=0 && movieInfoUpdateAllIfDestEmptyOnly == true) &&
					movieInfoUpdateAll[MB_INFO_QUALITY])
				(*current_list)[i]->quality = m_movieSelectionHandler->quality;

			m_movieInfo.saveMovieInfo(*((*current_list)[i]));
		}
		loadBox.hide();
	}
	else if (actionKey == "reload_movie_info")
	{
		loadMovies(false);
		updateMovieSelection();
	}
	else if (actionKey == "run")
	{
		if (parent) parent->hide();
		exec(NULL);
	}
	else if (actionKey == "book_clear_all")
	{
		m_movieSelectionHandler->bookmarks.start =0;
		m_movieSelectionHandler->bookmarks.end =0;
		m_movieSelectionHandler->bookmarks.lastPlayStop =0;
		for (int i = 0; i < MI_MOVIE_BOOK_USER_MAX; i++)
		{
			m_movieSelectionHandler->bookmarks.user[i].name = g_Locale->getText(LOCALE_MOVIEBROWSER_BOOK_NEW);
			m_movieSelectionHandler->bookmarks.user[i].length =0;
			m_movieSelectionHandler->bookmarks.user[i].pos =0;
		}
	}
	else if(actionKey == "show_menu")
	{
		showMenu(true);
	}
	else if(actionKey == "show_ytmenu")
	{
		showYTMenu(true);
		saveSettings(&m_settings);
	}
	else if (actionKey == "copy_onefile" || actionKey == "copy_several")
	{
		bool onefile = (actionKey == "copy_onefile");
		if ((show_mode == MB_SHOW_RECORDS) && (ShowMsg(LOCALE_MESSAGEBOX_INFO, onefile ? LOCALE_MOVIEBROWSER_COPY : LOCALE_MOVIEBROWSER_COPIES, CMsgBox::mbrNo, CMsgBox::mbYes | CMsgBox::mbNo) == CMsgBox::mbrYes))
		{
			CHintBox hintBox(LOCALE_MESSAGEBOX_INFO, LOCALE_MOVIEBROWSER_COPYING);
			hintBox.paint();
			sleep(1); //???
			hintBox.hide();

			framebuffer->paintBackground(); // clear screen
			CMovieCut mc;
			bool res = mc.copyMovie(m_movieSelectionHandler, onefile);
			//g_RCInput->clearRCMsg();
			if (res == 0)
				ShowMsg(LOCALE_MESSAGEBOX_ERROR, LOCALE_MOVIEBROWSER_COPY_FAILED, CMsgBox::mbrCancel, CMsgBox::mbCancel, NEUTRINO_ICON_ERROR);
			else
				m_doLoadMovies = true;
			m_doRefresh = true;
		}
	}
	else if (actionKey == "cut")
	{
#if 0
		if ((m_movieSelectionHandler == playing_info) && (NeutrinoMessages::mode_ts == CNeutrinoApp::getInstance()->getMode()))
			ShowMsg(LOCALE_MESSAGEBOX_ERROR, "Impossible to cut playing movie.", CMsgBox::mbrCancel, CMsgBox::mbCancel, NEUTRINO_ICON_ERROR);
		else
#endif
		if ((show_mode == MB_SHOW_RECORDS) && (ShowMsg(LOCALE_MESSAGEBOX_INFO, LOCALE_MOVIEBROWSER_CUT, CMsgBox::mbrNo, CMsgBox::mbYes | CMsgBox::mbNo) == CMsgBox::mbrYes))
		{
			CHintBox hintBox(LOCALE_MESSAGEBOX_INFO, LOCALE_MOVIEBROWSER_CUTTING);
			hintBox.paint();
			sleep(1); //???
			hintBox.hide();

			framebuffer->paintBackground(); // clear screen
			CMovieCut mc;
			bool res = mc.cutMovie(m_movieSelectionHandler);
			//g_RCInput->clearRCMsg();
			if (!res)
				ShowMsg(LOCALE_MESSAGEBOX_ERROR, LOCALE_MOVIEBROWSER_CUT_FAILED, CMsgBox::mbrCancel, CMsgBox::mbCancel, NEUTRINO_ICON_ERROR);
			else
				m_doLoadMovies = true;
			m_doRefresh = true;
		}
	}
	else if (actionKey == "truncate")
	{
		if ((show_mode == MB_SHOW_RECORDS) && m_movieSelectionHandler != NULL)
		{
			if ((m_movieSelectionHandler == playing_info) && (NeutrinoMessages::mode_ts == CNeutrinoApp::getInstance()->getMode()))
				ShowMsg(LOCALE_MESSAGEBOX_ERROR, LOCALE_MOVIEBROWSER_TRUNCATE_FAILED_PLAYING, CMsgBox::mbrCancel, CMsgBox::mbCancel, NEUTRINO_ICON_ERROR);
			else if (m_movieSelectionHandler->bookmarks.end == 0)
				ShowMsg(LOCALE_MESSAGEBOX_ERROR, LOCALE_MOVIEBROWSER_BOOK_NO_END, CMsgBox::mbrCancel, CMsgBox::mbCancel, NEUTRINO_ICON_ERROR);
			else
			{
				if (ShowMsg(LOCALE_MESSAGEBOX_INFO, LOCALE_MOVIEBROWSER_TRUNCATE, CMsgBox::mbrNo, CMsgBox::mbYes | CMsgBox::mbNo) == CMsgBox::mbrYes)
				{
					CHintBox hintBox(LOCALE_MESSAGEBOX_INFO, LOCALE_MOVIEBROWSER_TRUNCATING);
					hintBox.paint();
					CMovieCut mc;
					bool res = mc.truncateMovie(m_movieSelectionHandler);
					hintBox.hide();

					g_RCInput->clearRCMsg();
					if (!res)
						ShowMsg(LOCALE_MESSAGEBOX_ERROR, LOCALE_MOVIEBROWSER_TRUNCATE_FAILED, CMsgBox::mbrCancel, CMsgBox::mbCancel, NEUTRINO_ICON_ERROR);
					else
						m_doLoadMovies = true;
					m_doRefresh = true;
				}
			}
		}
	}
	else if (actionKey == "delete_movie")
	{
		if (m_movieSelectionHandler != NULL)
		{
			if (onDelete(true /*cursor only*/))
			{
				m_doLoadMovies = true;
				m_doRefresh = true;
				returnval = menu_return::RETURN_EXIT;
			}
		}
	}
	return returnval;
}

int CMovieBrowser::exec(const char* path)
{
	bool res = false;
	menu_ret = menu_return::RETURN_REPAINT;

	TRACE("[mb]->%s\n", __func__);
	int returnDefaultOnTimeout = true;
	neutrino_msg_t msg;
	neutrino_msg_data_t data;

	CVFD::getInstance()->setMode(CVFD::MODE_MENU_UTF8, g_Locale->getText(LOCALE_MOVIEBROWSER_HEAD));
	loadSettings(&m_settings);
	initFrames();

	// Clear all, to avoid 'jump' in screen
	m_vHandleBrowserList.clear();
	m_vHandleRecordList.clear();
	m_vHandlePlayList.clear();

	clearListLines();

	m_selectedDir = path;

	if (m_settings.remount == true)
	{
		TRACE("[mb] remount\n");
		/* FIXME: add hintbox ? */
		//umount automount dirs
		for (int i = 0; i < NETWORK_NFS_NR_OF_ENTRIES; i++)
		{
			if (g_settings.network_nfs[i].automount)
				umount2(g_settings.network_nfs[i].local_dir.c_str(), MNT_FORCE);
		}
		CFSMounter::automount();
	}

	if (paint() == false)
		return menu_ret;// paint failed due to less memory, exit

	bool loop = true;
	bool result;
	int timeout = g_settings.timing[SNeutrinoSettings::TIMING_FILEBROWSER];
	uint64_t timeoutEnd = CRCInput::calcTimeoutEnd(timeout);
	while (loop)
	{
		framebuffer->blit();
		g_RCInput->getMsgAbsoluteTimeout(&msg, &data, &timeoutEnd);

		result = onButtonPress(msg);
		if (result == false)
		{
			if (msg == CRCInput::RC_timeout && returnDefaultOnTimeout)
			{
				TRACE("[mb] Timerevent\n");
				loop = false;
			}
			else if (msg == CRCInput::RC_ok)
			{
				for (unsigned int i = 0; i < m_vMovieInfo.size(); i++) {
					if (m_vMovieInfo[i].marked) {
						TRACE("[mb] has selected\n");
						res = true;
						break;
					}
				}
				if (res)
					break;

				m_currentStartPos = 0;

				if (m_movieSelectionHandler != NULL)
				{
					// If there is any available bookmark, show the bookmark menu
					if (m_movieSelectionHandler->bookmarks.lastPlayStop != 0 ||
							m_movieSelectionHandler->bookmarks.start != 0)
					{
						TRACE("[mb] stop: %d start:%d \n",m_movieSelectionHandler->bookmarks.lastPlayStop,m_movieSelectionHandler->bookmarks.start);
						m_currentStartPos = showStartPosSelectionMenu(); // display start menu m_currentStartPos =
					}

					if (show_mode == MB_SHOW_YT)
						cYTCache::getInstance()->useCachedCopy(m_movieSelectionHandler);

					if (m_currentStartPos >= 0) {
						playing_info = m_movieSelectionHandler;
						TRACE("[mb] start pos: %d s\n",m_currentStartPos);
						res = true;
						loop = false;
					} else
						refresh();
				}
			}
			else if ((show_mode == MB_SHOW_YT) && (msg == (neutrino_msg_t) g_settings.key_record) && m_movieSelectionHandler)
			{
				m_movieSelectionHandler->source = (show_mode == MB_SHOW_YT) ? MI_MOVIE_INFO::YT : MI_MOVIE_INFO::NK;
				if (cYTCache::getInstance()->addToCache(m_movieSelectionHandler)) {
					const char *format = g_Locale->getText(LOCALE_MOVIEBROWSER_YT_CACHE_ADD);
					char buf[1024];
					snprintf(buf, sizeof(buf), format, m_movieSelectionHandler->file.Name.c_str());
					CHintBox hintBox(LOCALE_MOVIEBROWSER_YT_CACHE, buf);
					hintBox.paint();
					sleep(1); //???
					hintBox.hide();
				}
			}
			else if (msg == CRCInput::RC_home)
			{
				loop = false;
			}
			else if (CNeutrinoApp::getInstance()->listModeKey(msg)) {
				// do nothing
			}
			else if (msg == NeutrinoMessages::STANDBY_ON ||
					msg == NeutrinoMessages::LEAVE_ALL ||
					msg == NeutrinoMessages::SHUTDOWN ||
					msg == NeutrinoMessages::SLEEPTIMER)
			{
				menu_ret = menu_return::RETURN_EXIT_ALL;
				loop = false;
				g_RCInput->postMsg(msg, data);
			}
			else if (CNeutrinoApp::getInstance()->handleMsg(msg, data) & messages_return::cancel_all)
			{
				TRACE("[mb]->exec: getInstance\n");
				menu_ret = menu_return::RETURN_EXIT_ALL;
				loop = false;
			}
		}

		if (msg <= CRCInput::RC_MaxRC)
			timeoutEnd = CRCInput::calcTimeoutEnd(timeout); // calcualate next timeout
	}
	hide();
	framebuffer->blit();
	//TRACE(" return %d\n",res);

	m_prevBrowserSelection = m_currentBrowserSelection;
	m_prevRecordSelection = m_currentRecordSelection;
	m_prevPlaySelection = m_currentPlaySelection;

	saveSettings(&m_settings);

	// make stale if we should reload the next time, but not if movie has to be played
	if (m_settings.reload == true && res == false)
	{
		TRACE("[mb] force reload next time\n");
		fileInfoStale();
	}

	//CVFD::getInstance()->setMode(CVFD::MODE_TVRADIO);
	return (res);
}

void CMovieBrowser::hide(void)
{
	//TRACE("[mb]->%s\n", __func__);
	if (m_channelLogo){
		delete m_channelLogo; m_channelLogo = NULL;
	}
	if (m_header)	{
		delete m_header; m_header = NULL;
	}
	old_EpgId = 0;
	framebuffer->paintBackground();
	if (m_pcFilter != NULL)
		m_currentFilterSelection = m_pcFilter->getSelectedLine();

	delete m_pcFilter;
	m_pcFilter = NULL;

	if (m_pcBrowser != NULL)
		m_currentBrowserSelection = m_pcBrowser->getSelectedLine();

	delete m_pcBrowser;
	m_pcBrowser = NULL;

	if (m_pcLastPlay != NULL)
		m_currentPlaySelection = m_pcLastPlay->getSelectedLine();

	delete m_pcLastPlay;
	m_pcLastPlay = NULL;

	if (m_pcLastRecord != NULL)
		m_currentRecordSelection = m_pcLastRecord->getSelectedLine();

	delete m_pcLastRecord;
	m_pcLastRecord = NULL;
	delete m_pcInfo1;
	m_pcInfo1 = NULL;
	delete m_pcInfo2;
	m_pcInfo2 = NULL;
}

int CMovieBrowser::paint(void)
{
	TRACE("[mb]->%s\n", __func__);

	//CVFD::getInstance()->setMode(CVFD::MODE_MENU_UTF8, g_Locale->getText(LOCALE_MOVIEBROWSER_HEAD));

	Font* font = g_Font[SNeutrinoSettings::FONT_TYPE_MOVIEBROWSER_LIST];
	m_movieSelectionHandler = NULL;

	m_pcBrowser = new CListFrame(&m_browserListLines, font, CListFrame::SCROLL | CListFrame::HEADER_LINE,
			&m_cBoxFrameBrowserList, NULL,
			g_Font[SNeutrinoSettings::FONT_TYPE_MOVIEBROWSER_HEAD]);
	m_pcLastPlay = new CListFrame(&m_playListLines, font, CListFrame::SCROLL | CListFrame::HEADER_LINE | CListFrame::TITLE,
			&m_cBoxFrameLastPlayList, g_Locale->getText(LOCALE_MOVIEBROWSER_HEAD_PLAYLIST),
			g_Font[SNeutrinoSettings::FONT_TYPE_MOVIEBROWSER_HEAD]);
	m_pcLastRecord = new CListFrame(&m_recordListLines, font, CListFrame::SCROLL | CListFrame::HEADER_LINE | CListFrame::TITLE,
			&m_cBoxFrameLastRecordList, g_Locale->getText(LOCALE_MOVIEBROWSER_HEAD_RECORDLIST),
			g_Font[SNeutrinoSettings::FONT_TYPE_MOVIEBROWSER_HEAD]);
	m_pcFilter = new CListFrame(&m_FilterLines, g_Font[SNeutrinoSettings::FONT_TYPE_MOVIEBROWSER_INFO], CListFrame::SCROLL | CListFrame::TITLE,
			&m_cBoxFrameFilter, g_Locale->getText(LOCALE_MOVIEBROWSER_HEAD_FILTER),
			g_Font[SNeutrinoSettings::FONT_TYPE_MOVIEBROWSER_HEAD]);
	m_pcInfo1 = new CTextBox(" ", g_Font[SNeutrinoSettings::FONT_TYPE_MOVIEBROWSER_INFO], CTextBox::TOP | CTextBox::SCROLL, &m_cBoxFrameInfo1);
	m_pcInfo2 = new CTextBox(" ", g_Font[SNeutrinoSettings::FONT_TYPE_MOVIEBROWSER_INFO], CTextBox::TOP | CTextBox::SCROLL, &m_cBoxFrameInfo2);


	if (m_pcBrowser == NULL || m_pcLastPlay == NULL ||
			m_pcLastRecord == NULL || m_pcInfo1 == NULL || m_pcInfo2 == NULL || m_pcFilter == NULL)
	{
		TRACE("[mb] paint, ERROR: not enought memory to allocate windows");
		if (m_pcFilter != NULL)delete m_pcFilter;
		if (m_pcBrowser != NULL)delete m_pcBrowser;
		if (m_pcLastPlay != NULL) delete m_pcLastPlay;
		if (m_pcLastRecord != NULL)delete m_pcLastRecord;
		if (m_pcInfo1 != NULL) delete m_pcInfo1;
		if (m_pcInfo2 != NULL) delete m_pcInfo2;

		m_pcInfo1 = NULL;
		m_pcInfo2 = NULL;
		m_pcLastPlay = NULL;
		m_pcLastRecord = NULL;
		m_pcBrowser = NULL;
		m_pcFilter = NULL;

		return (false);
	}

	m_pcFilter->setBackGroundRadius(RADIUS_LARGE);

	m_pcInfo1->setBackGroundColor(COL_MENUCONTENTDARK_PLUS_0);
	m_pcInfo1->setTextColor(COL_MENUCONTENTDARK_TEXT);
	m_pcInfo1->setBackGroundRadius(RADIUS_LARGE);
	//m_pcInfo1->setTextBorderWidth(OFFSET_INNER_MID, OFFSET_INNER_SMALL);
	m_pcInfo2->setTextBorderWidth(OFFSET_INNER_MID, OFFSET_INNER_SMALL);

	clearSelection();
	if (m_file_info_stale == true) {
		loadMovies();
	} else {
		refreshBrowserList();
		refreshLastPlayList();
		refreshLastRecordList();
		refreshFilterList();
	}

	// get old movie selection and set position in windows
	m_currentBrowserSelection = m_prevBrowserSelection;
	m_currentRecordSelection = m_prevRecordSelection;
	m_currentPlaySelection = m_prevPlaySelection;

	m_pcBrowser->setSelectedLine(m_currentBrowserSelection);
	m_pcLastRecord->setSelectedLine(m_currentRecordSelection);
	m_pcLastPlay->setSelectedLine(m_currentPlaySelection);

	updateMovieSelection();

	refreshTitle();
	refreshFoot();
	refreshLCD();
	if (m_settings.gui == MB_GUI_FILTER)
		m_settings.gui = MB_GUI_MOVIE_INFO;
	if (show_mode == MB_SHOW_YT)
		onSetGUIWindow(MB_GUI_MOVIE_INFO);
	else
		onSetGUIWindow(m_settings.gui);

	return (true);
}

void CMovieBrowser::refresh(void)
{
	TRACE("[mb]->%s\n", __func__);

	refreshTitle();
	if (m_pcBrowser != NULL && m_showBrowserFiles == true)
		 m_pcBrowser->refresh();
	if (m_pcLastPlay != NULL && m_showLastPlayFiles == true)
		m_pcLastPlay->refresh();
	if (m_pcLastRecord != NULL && m_showLastRecordFiles == true)
		 m_pcLastRecord->refresh();
	if (m_pcInfo1 != NULL && m_pcInfo2 != NULL && m_showMovieInfo == true)
		refreshMovieInfo();
	if (m_pcFilter != NULL && m_showFilter == true)
		m_pcFilter->refresh();

	refreshFoot();
	refreshLCD();

	m_doRefresh = false;
}

std::string CMovieBrowser::getCurrentDir(void)
{
	return(m_selectedDir);
}

CFile* CMovieBrowser::getSelectedFile(void)
{
	//TRACE("[mb]->%s: %s\n", __func__, m_movieSelectionHandler->file.Name.c_str());

	if (m_movieSelectionHandler != NULL)
		return(&m_movieSelectionHandler->file);
	else
		return(NULL);
}

bool CMovieBrowser::getSelectedFiles(CFileList &flist, P_MI_MOVIE_LIST &mlist)
{
	flist.clear();
	mlist.clear();
	P_MI_MOVIE_LIST *handle_list = &m_vHandleBrowserList;

	if (m_windowFocus == MB_FOCUS_LAST_PLAY)
		handle_list = &m_vHandlePlayList;
	if (m_windowFocus == MB_FOCUS_LAST_RECORD)
		handle_list = &m_vHandleRecordList;

	for (unsigned int i = 0; i < handle_list->size(); i++) {
		if ((*handle_list)[i]->marked) {
			flist.push_back((*handle_list)[i]->file);
			mlist.push_back((*handle_list)[i]);
		}
	}
	return (!flist.empty());
}

std::string CMovieBrowser::getScreenshotName(std::string movie, bool is_dir)
{
	std::string ext;
	std::string ret;
	size_t found;

	if (is_dir)
		found = movie.size();
	else
		found = movie.find_last_of(".");

	if (found == string::npos)
		return "";

	vector<std::string>::iterator it = PicExts.begin();
	while (it < PicExts.end()) {
		ret = movie;
		ext = *it;
		ret.replace(found, ret.length() - found, ext);
		++it;
		if (!access(ret, F_OK))
			return ret;
	}
	return "";
}

void CMovieBrowser::refreshChannelLogo(void)
{
	TRACE("[mb]->%s:%d\n", __func__, __LINE__);

	int w_logo_max = m_cBoxFrameTitleRel.iWidth / 4;
	int h_logo_max = m_cBoxFrameTitleRel.iHeight - 2*OFFSET_INNER_MIN;
	short pb_hdd_offset = 100 + OFFSET_INNER_MID;

	if (show_mode == MB_SHOW_YT)
		pb_hdd_offset = 0;

	if (m_channelLogo && (old_EpgId != m_movieSelectionHandler->epgId >> 16))
	{
		if (newHeader)
			m_channelLogo->clearFbData(); // reset logo screen data
		else
			m_channelLogo->hide();
		delete m_channelLogo;
		m_channelLogo = NULL;
	}

	if (old_EpgId != m_movieSelectionHandler->epgId >> 16)
	{
		if (m_channelLogo == NULL)
			m_channelLogo = new CComponentsChannelLogoScalable(0, 0, m_movieSelectionHandler->channelName, m_movieSelectionHandler->epgId >>16); //TODO: add logo into header as item
		old_EpgId = m_movieSelectionHandler->epgId >> 16;
	}

	if (m_channelLogo && m_channelLogo->hasLogo())
	{
		// TODO: move into an own handler, eg. header, so channel logo should be paint in header object
		m_channelLogo->setWidth(min(m_channelLogo->getWidth(), w_logo_max), true);
		if (m_channelLogo->getHeight() > h_logo_max)
			m_channelLogo->setHeight(h_logo_max, true);

		int x = m_cBoxFrame.iX + m_cBoxFrameTitleRel.iX + m_cBoxFrameTitleRel.iWidth - m_channelLogo->getWidth() - OFFSET_INNER_MID;
		int y = m_cBoxFrame.iY + m_cBoxFrameTitleRel.iY + (m_cBoxFrameTitleRel.iHeight - m_channelLogo->getHeight())/2;
		m_channelLogo->setXPos(x - pb_hdd_offset - m_header->getContextBtnObject()->getWidth());
		m_channelLogo->setYPos(y);
		m_channelLogo->hide();
		m_channelLogo->paint();
		newHeader = false;
	}
}

void CMovieBrowser::initMovieCover(void)
{
	TRACE("[mb]->%s:%d\n", __func__, __LINE__);

	CBox movieCoverBox;

	int cover_w = 0;
	int cover_h = 0;

	if (m_windowFocus == MB_FOCUS_BROWSER && m_settings.browserAdditional)
	{
		movieCoverBox = m_cBoxFrameInfo2;

		// we have to align cover to box width
		cover_w = movieCoverBox.iWidth - 2*OFFSET_INNER_MID - OFFSET_SHADOW;
	}
	else
	{
		movieCoverBox = m_cBoxFrameInfo1;

		// we have to align cover to box height
		cover_h = movieCoverBox.iHeight - 2*OFFSET_INNER_MID - OFFSET_SHADOW;
	}

	std::string cover_file;
	if (show_mode == MB_SHOW_YT)
	{
		cover_file = m_movieSelectionHandler->tfile;
	}
	else
	{
		cover_file = getScreenshotName(m_movieSelectionHandler->file.Name, S_ISDIR(m_movieSelectionHandler->file.Mode));
		if ((cover_file.empty()) && (m_movieSelectionHandler->file.Name.length() > 18))
		{
			std::string cover = m_movieSelectionHandler->file.Name;
			cover.replace((cover.length()-18), 15, ""); //covername without yyyymmdd_hhmmss
			cover_file = getScreenshotName(cover);
		}
	}

	bool got_cover = !cover_file.empty();

	if (m_movieCover)
	{
		if (m_movieCover->getPictureName() != cover_file || !got_cover)
		{
			delete m_movieCover;
			m_movieCover = NULL;
		}
	}

	if (m_settings.gui != MB_GUI_FILTER && got_cover)
	{
		if (m_movieCover == NULL)
		{
			m_movieCover = new CComponentsPicture(0, 0, cover_file, NULL, CC_SHADOW_ON, COL_MENUCONTENTDARK_PLUS_0);

			m_movieCover->enableFrame(true, 1);
			m_movieCover->doPaintBg(false);
		}

		// always align positions and dimensions
		if (m_windowFocus == MB_FOCUS_BROWSER && m_settings.browserAdditional)
		{
			if (m_movieCover->getWidth() < m_movieCover->getHeight())
				cover_w /= 2; // cover is upright, so we use just half width first

			m_movieCover->setHeight(0); // force recalculation
TRACE("[mb]->%s:%d m_movieCover->getHeight(): %d\n", __func__, __LINE__, m_movieCover->getHeight());
			m_movieCover->setWidth(cover_w, true);
			if (m_movieCover->getHeight() > movieCoverBox.iHeight/3)
				m_movieCover->setHeight(movieCoverBox.iHeight/3, true); // use maximal one third of box height

			m_movieCover->setXPos(movieCoverBox.iX + (movieCoverBox.iWidth - m_movieCover->getWidth())/2);
			m_movieCover->setYPos(movieCoverBox.iY + OFFSET_INNER_MID);
		}
		else
		{
			m_movieCover->setWidth(0); // force recalculation
TRACE("[mb]->%s:%d m_movieCover->getWidth(): %d\n", __func__, __LINE__, m_movieCover->getWidth());
			m_movieCover->setHeight(cover_h, true);
			if (m_movieCover->getWidth() > movieCoverBox.iWidth/3)
				m_movieCover->setWidth(movieCoverBox.iWidth/3, true); // use maximal one third of box width

			m_movieCover->setXPos(movieCoverBox.iX + movieCoverBox.iWidth - m_movieCover->getWidth() - 2*OFFSET_INNER_MID - OFFSET_SHADOW);
			m_movieCover->setYPos(movieCoverBox.iY + (movieCoverBox.iHeight - m_movieCover->getHeight())/2);
		}

		if (!m_movieSelectionHandler->epgInfo2.empty())
		{
			if (m_pcInfo1->OnAfterScrollPage.empty())
			{
				//m_movieCover->enableCache();
				m_pcInfo1->OnAfterScrollPage.connect(sigc::mem_fun(m_movieCover, &CComponentsPicture::paint0));
			}
		}
	}
}

void CMovieBrowser::refreshMovieCover(void)
{
	TRACE("[mb]->%s:%d\n", __func__, __LINE__);

	if (m_movieCover)
		m_movieCover->paint(CC_SAVE_SCREEN_YES);
}

void CMovieBrowser::hideMovieCover(void)
{
	TRACE("[mb]->%s:%d\n", __func__, __LINE__);

	if (m_movieCover)
		m_movieCover->hide();
}

void CMovieBrowser::refreshMovieInfo(void)
{
	TRACE("[mb]->%s m_vMovieInfo.size %d\n", __func__, (int)m_vMovieInfo.size());

	// clear m_pcInfo1 text before new init
	m_pcInfo1->clear();

	// clear m_pcInfo2 text, reset position and dimensions before new init
	m_pcInfo2->movePosition(m_cBoxFrameInfo2.iX, m_cBoxFrameInfo2.iY);
	m_pcInfo2->setWindowMaxDimensions(m_cBoxFrameInfo2.iWidth, m_cBoxFrameInfo2.iHeight);
	m_pcInfo2->setWindowMinDimensions(m_cBoxFrameInfo2.iWidth, m_cBoxFrameInfo2.iHeight);
	m_pcInfo2->clear();

	if (m_windowFocus == MB_FOCUS_BROWSER && m_settings.browserAdditional)
		m_pcInfo1->setTextFont(g_Font[SNeutrinoSettings::FONT_TYPE_MOVIEBROWSER_LIST]);
	else
		m_pcInfo1->setTextFont(g_Font[SNeutrinoSettings::FONT_TYPE_MOVIEBROWSER_INFO]);

	if (m_vMovieInfo.empty() || m_movieSelectionHandler == NULL)
		return;

	refreshChannelLogo();

	initMovieCover();
	int cover_x_offset = 0;
	int cover_y_offset = 0;
	if (m_movieCover)
	{
		cover_x_offset = m_movieCover->getWidth();
		cover_y_offset = m_movieCover->getHeight();
		if (cover_x_offset)
			cover_x_offset += 2*OFFSET_INNER_MID;
		if (cover_y_offset)
			cover_y_offset += 2*OFFSET_INNER_MID;
	}

	std::string pcInfo1_content = " ";
	if (m_windowFocus == MB_FOCUS_BROWSER && m_settings.browserAdditional)
	{
		m_pcInfo2->setWindowMaxDimensions(m_cBoxFrameInfo2.iWidth, m_cBoxFrameInfo2.iHeight - cover_y_offset);
		m_pcInfo2->setWindowMinDimensions(m_cBoxFrameInfo2.iWidth, m_cBoxFrameInfo2.iHeight - cover_y_offset);
		m_pcInfo2->movePosition(m_cBoxFrameInfo2.iX, m_cBoxFrameInfo2.iY + cover_y_offset);
		m_pcInfo2->setText(&m_movieSelectionHandler->epgInfo2);

		pcInfo1_content = m_movieSelectionHandler->epgInfo1;
		pcInfo1_content += "\n";
		pcInfo1_content += m_movieSelectionHandler->channelName;
	}
	else
	{
		pcInfo1_content  = m_movieSelectionHandler->epgInfo2;
	}
	m_pcInfo1->setText(&pcInfo1_content, m_cBoxFrameInfo1.iWidth - cover_x_offset);

	updateInfoSelection();

	refreshMovieCover();

	if (m_windowFocus == MB_FOCUS_BROWSER)
		refreshDetailsLine(m_pcBrowser->getSelectedLineRel());
}

void CMovieBrowser::hideDetailsLine()
{
	if (m_detailsLine)
		m_detailsLine->hide();
}

void CMovieBrowser::refreshDetailsLine(int pos)
{
	if (pos >= 0)
	{
		int fheight = g_Font[SNeutrinoSettings::FONT_TYPE_MOVIEBROWSER_LIST]->getHeight();
		int hheight = m_pcBrowser->getHeaderListHeight();
		int theight = m_pcBrowser->getTitleHeight();

		int xpos  = m_cBoxFrameBrowserList.iX - ConnectLineBox_Width;
		int ypos1 = m_cBoxFrameBrowserList.iY + hheight + theight + OFFSET_INNER_MID + pos*fheight + (fheight/2);
		int ypos2 = m_cBoxFrameInfo1.iY + (m_cBoxFrameInfo1.iHeight/2);

		if (m_detailsLine == NULL)
			m_detailsLine = new CComponentsDetailLine();

		m_detailsLine->setDimensionsAll(xpos, ypos1, ypos2, fheight/2, m_cBoxFrameInfo1.iHeight-2*RADIUS_LARGE);
		m_detailsLine->paint(true);
	}
}

void CMovieBrowser::info_hdd_level(bool paint_hdd)
{
	if (show_mode == MB_SHOW_YT)
		return;

	struct statfs s;
	long	blocks_percent_used =0;
	static long tmp_blocks_percent_used = 0;
	if (getSelectedFile() != NULL) {
		if (::statfs(getSelectedFile()->Name.c_str(), &s) == 0) {
			long blocks_used = s.f_blocks - s.f_bfree;
			blocks_percent_used = (blocks_used * 1000 / (blocks_used + s.f_bavail) + 5)/10;
		}
	}

	if (tmp_blocks_percent_used != blocks_percent_used || paint_hdd) {
		tmp_blocks_percent_used = blocks_percent_used;
		const short pbw = 100;
		const short border = m_cBoxFrameTitleRel.iHeight/4;
		CProgressBar pb(m_cBoxFrame.iX+ m_cBoxFrameFootRel.iWidth - m_header->getContextBtnObject()->getWidth() - pbw - border, m_cBoxFrame.iY+m_cBoxFrameTitleRel.iY + border, pbw, m_cBoxFrameTitleRel.iHeight/2);
		pb.setType(CProgressBar::PB_REDRIGHT);
		pb.setValues(blocks_percent_used, 100);
		pb.paint(false);
	}
}

void CMovieBrowser::refreshLCD(void)
{
	if (m_vMovieInfo.empty() || m_movieSelectionHandler == NULL)
		return;

	CVFD::getInstance()->showMenuText(0, m_movieSelectionHandler->epgTitle.c_str(), -1, true); // UTF-8
}

void CMovieBrowser::refreshFilterList(void)
{
	TRACE("[mb]->refreshFilterList %d\n",m_settings.filter.item);

	std::string string_item;

	m_FilterLines.rows = 1;
	m_FilterLines.lineArray[0].clear();
	m_FilterLines.rowWidth[0] = 100;
	m_FilterLines.lineHeader[0] = "";

	if (m_vMovieInfo.empty())
		return; // exit here if nothing else is to do

	if (m_settings.filter.item == MB_INFO_MAX_NUMBER)
	{
		// show Main List
		string_item = g_Locale->getText(LOCALE_MOVIEBROWSER_INFO_GENRE_MAJOR);
		m_FilterLines.lineArray[0].push_back(string_item);
		string_item = g_Locale->getText(LOCALE_MOVIEBROWSER_INFO_INFO1);
		m_FilterLines.lineArray[0].push_back(string_item);
		string_item = g_Locale->getText(LOCALE_MOVIEBROWSER_INFO_PATH);
		m_FilterLines.lineArray[0].push_back(string_item);
		string_item = g_Locale->getText(LOCALE_MOVIEBROWSER_INFO_SERIE);
		m_FilterLines.lineArray[0].push_back(string_item);
	}
	else
	{
		std::string tmp = g_Locale->getText(LOCALE_MOVIEBROWSER_FILTER_OFF);
		m_FilterLines.lineArray[0].push_back(tmp);

		if (m_settings.filter.item == MB_INFO_FILEPATH)
		{
			for (unsigned int i = 0 ; i < m_dirNames.size(); i++)
				m_FilterLines.lineArray[0].push_back(m_dirNames[i]);
		}
		else if (m_settings.filter.item == MB_INFO_INFO1)
		{
			for (unsigned int i = 0; i < m_vMovieInfo.size(); i++)
			{
				bool found = false;
				for (unsigned int t = 0; t < m_FilterLines.lineArray[0].size() && found == false; t++)
				{
					if (strcmp(m_FilterLines.lineArray[0][t].c_str(),m_vMovieInfo[i].epgInfo1.c_str()) == 0)
						found = true;
				}
				if (found == false)
					m_FilterLines.lineArray[0].push_back(m_vMovieInfo[i].epgInfo1);
			}
		}
		else if (m_settings.filter.item == MB_INFO_MAJOR_GENRE)
		{
			for (int i = 0; i < GENRE_ALL_COUNT; i++)
			{
				std::string tmpl = g_Locale->getText(GENRE_ALL[i].value);
				m_FilterLines.lineArray[0].push_back(tmpl);
			}
		}
		else if (m_settings.filter.item == MB_INFO_SERIE)
		{
			updateSerienames();
			for (unsigned int i = 0; i < m_vHandleSerienames.size(); i++)
				m_FilterLines.lineArray[0].push_back(m_vHandleSerienames[i]->serieName);
		}
	}
	m_pcFilter->setLines(&m_FilterLines);
}

void CMovieBrowser::refreshLastPlayList(void) //P2
{
	//TRACE("[mb]->refreshlastPlayList \n");
	std::string string_item;

	// Initialise and clear list array
	m_playListLines.rows = m_settings.lastPlayRowNr;
	for (int row = 0 ;row < m_settings.lastPlayRowNr; row++)
	{
		m_playListLines.lineArray[row].clear();
		m_playListLines.rowWidth[row] = m_settings.lastPlayRowWidth[row];
		m_playListLines.lineHeader[row] = g_Locale->getText(m_localizedItemName[m_settings.lastPlayRow[row]]);
	}
	m_playListLines.marked.clear();
	m_vHandlePlayList.clear();

	if (m_vMovieInfo.empty()) {
		if (m_pcLastPlay != NULL)
			m_pcLastPlay->setLines(&m_playListLines);
		return; // exit here if nothing else is to do
	}

	MI_MOVIE_INFO* movie_handle;
	// prepare Browser list for sorting and filtering
	for (unsigned int file = 0; file < m_vMovieInfo.size(); file++)
	{
		if (isParentalLock(m_vMovieInfo[file]) == false)
		{
			movie_handle = &(m_vMovieInfo[file]);
			m_vHandlePlayList.push_back(movie_handle);
		}
	}
	// sort the not filtered files
	onSortMovieInfoHandleList(m_vHandlePlayList,MB_INFO_PREVPLAYDATE,MB_DIRECTION_DOWN);

	for (int handle=0; handle < (int) m_vHandlePlayList.size() && handle < m_settings.lastPlayMaxItems ;handle++)
	{
		for (int row = 0; row < m_settings.lastPlayRowNr ;row++)
		{
			if (getMovieInfoItem(*m_vHandlePlayList[handle], m_settings.lastPlayRow[row], &string_item) == false)
			{
				string_item = "n/a";
				if (m_settings.lastPlayRow[row] == MB_INFO_TITLE)
					getMovieInfoItem(*m_vHandlePlayList[handle], MB_INFO_FILENAME, &string_item);
			}
			m_playListLines.lineArray[row].push_back(string_item);
		}
		m_playListLines.marked.push_back(m_vHandlePlayList[handle]->marked);
	}
	m_pcLastPlay->setLines(&m_playListLines);

	m_currentPlaySelection = m_pcLastPlay->getSelectedLine();
	// update selected movie if browser is in the focus
	if (m_windowFocus == MB_FOCUS_LAST_PLAY)
		updateMovieSelection();
}

void CMovieBrowser::refreshLastRecordList(void) //P2
{
	//TRACE("[mb]->refreshLastRecordList \n");
	std::string string_item;

	// Initialise and clear list array
	m_recordListLines.rows = m_settings.lastRecordRowNr;
	for (int row = 0 ;row < m_settings.lastRecordRowNr; row++)
	{
		m_recordListLines.lineArray[row].clear();
		m_recordListLines.rowWidth[row] = m_settings.lastRecordRowWidth[row];
		m_recordListLines.lineHeader[row] = g_Locale->getText(m_localizedItemName[m_settings.lastRecordRow[row]]);
	}
	m_recordListLines.marked.clear();
	m_vHandleRecordList.clear();

	if (m_vMovieInfo.empty()) {
		if (m_pcLastRecord != NULL)
			m_pcLastRecord->setLines(&m_recordListLines);
		return; // exit here if nothing else is to do
	}

	MI_MOVIE_INFO* movie_handle;
	// prepare Browser list for sorting and filtering
	for (unsigned int file = 0; file < m_vMovieInfo.size(); file++)
	{
		if (isParentalLock(m_vMovieInfo[file]) == false)
		{
			movie_handle = &(m_vMovieInfo[file]);
			m_vHandleRecordList.push_back(movie_handle);
		}
	}
	// sort the not filtered files
	onSortMovieInfoHandleList(m_vHandleRecordList,MB_INFO_RECORDDATE,MB_DIRECTION_DOWN);

	for (int handle=0; handle < (int) m_vHandleRecordList.size() && handle < m_settings.lastRecordMaxItems ;handle++)
	{
		for (int row = 0; row < m_settings.lastRecordRowNr ;row++)
		{
			if (getMovieInfoItem(*m_vHandleRecordList[handle], m_settings.lastRecordRow[row], &string_item) == false)
			{
				string_item = "n/a";
				if (m_settings.lastRecordRow[row] == MB_INFO_TITLE)
					getMovieInfoItem(*m_vHandleRecordList[handle], MB_INFO_FILENAME, &string_item);
			}
			m_recordListLines.lineArray[row].push_back(string_item);
		}
		m_recordListLines.marked.push_back(m_vHandleRecordList[handle]->marked);
	}

	m_pcLastRecord->setLines(&m_recordListLines);

	m_currentRecordSelection = m_pcLastRecord->getSelectedLine();
	// update selected movie if browser is in the focus
	if (m_windowFocus == MB_FOCUS_LAST_RECORD)
		updateMovieSelection();
}

void CMovieBrowser::refreshBrowserList(void) //P1
{
	TRACE("[mb]->%s\n", __func__);
	std::string string_item;

	// Initialise and clear list array
	m_browserListLines.rows = m_settings.browserRowNr;
	for (int row = 0; row < m_settings.browserRowNr; row++)
	{
		m_browserListLines.lineArray[row].clear();
		m_browserListLines.rowWidth[row] = m_settings.browserRowWidth[row];
		m_browserListLines.lineHeader[row] = g_Locale->getText(m_localizedItemName[m_settings.browserRowItem[row]]);
	}
	m_browserListLines.Icon.clear();
	m_browserListLines.marked.clear();
	m_vHandleBrowserList.clear();

	if (m_vMovieInfo.empty())
	{
		m_currentBrowserSelection = 0;
		m_movieSelectionHandler = NULL;
		if (m_pcBrowser != NULL)
			m_pcBrowser->setLines(&m_browserListLines);//FIXME last delete test
		return; // exit here if nothing else is to do
	}

	MI_MOVIE_INFO* movie_handle;
	// prepare Browser list for sorting and filtering
	for (unsigned int file=0; file < m_vMovieInfo.size(); file++)
	{
		if (isFiltered(m_vMovieInfo[file]) == false &&
				isParentalLock(m_vMovieInfo[file]) == false &&
				(m_settings.browser_serie_mode == 0 || m_vMovieInfo[file].serieName.empty() || m_settings.filter.item == MB_INFO_SERIE))
		{
			movie_handle = &(m_vMovieInfo[file]);
			m_vHandleBrowserList.push_back(movie_handle);
		}
	}
	// sort the not filtered files
	onSortMovieInfoHandleList(m_vHandleBrowserList,m_settings.sorting.item,MB_DIRECTION_AUTO);

	for (unsigned int handle=0; handle < m_vHandleBrowserList.size() ;handle++)
	{
		for (int row = 0; row < m_settings.browserRowNr ;row++)
		{
			if (getMovieInfoItem(*m_vHandleBrowserList[handle], m_settings.browserRowItem[row], &string_item) == false)
			{
				string_item = "n/a";
				if (m_settings.browserRowItem[row] == MB_INFO_TITLE)
					getMovieInfoItem(*m_vHandleBrowserList[handle], MB_INFO_FILENAME, &string_item);
			}

			m_browserListLines.lineArray[row].push_back(string_item);
		}
		if (CRecordManager::getInstance()->getRecordInstance(m_vHandleBrowserList[handle]->file.Name) != NULL)
			m_browserListLines.Icon.push_back(NEUTRINO_ICON_REC);
		else
			m_browserListLines.Icon.push_back("");
		m_browserListLines.marked.push_back(m_vHandleBrowserList[handle]->marked);
	}
	m_pcBrowser->setLines(&m_browserListLines);

	m_currentBrowserSelection = m_pcBrowser->getSelectedLine();
	// update selected movie if browser is in the focus
	if (m_windowFocus == MB_FOCUS_BROWSER)
		updateMovieSelection();
}

void CMovieBrowser::refreshTitle(void)
{
	std::string title = m_textTitle.c_str();
	const char *icon = NEUTRINO_ICON_MOVIEPLAYER;
	if (show_mode == MB_SHOW_YT) {
		title = g_Locale->getText(LOCALE_MOVIEPLAYER_YTPLAYBACK);
		title += " : ";
		neutrino_locale_t loc = getFeedLocale();
		title += g_Locale->getText(loc);
		if (loc == LOCALE_MOVIEBROWSER_YT_RELATED || loc == LOCALE_MOVIEBROWSER_YT_SEARCH)
			title += " \"" + m_settings.ytsearch + "\"";
		icon = NEUTRINO_ICON_YTPLAY;
	}

	TRACE("[mb]->refreshTitle : %s\n", title.c_str());

	int x = m_cBoxFrameTitleRel.iX + m_cBoxFrame.iX;
	int y = m_cBoxFrameTitleRel.iY + m_cBoxFrame.iY;
	int w = m_cBoxFrameTitleRel.iWidth;
	int h = m_cBoxFrameTitleRel.iHeight;

	if (!m_header){
		m_header = new CComponentsHeader(x, y, w, h, title.c_str(), icon, CComponentsHeader::CC_BTN_HELP);
	}
	m_header->paint(CC_SAVE_SCREEN_NO);
	newHeader = m_header->isPainted();

	info_hdd_level(true);
}

int CMovieBrowser::refreshFoot(bool show)
{
	//TRACE("[mb]->refreshButtonLine\n");
	int offset = (m_settings.gui != MB_GUI_LAST_PLAY && m_settings.gui != MB_GUI_LAST_RECORD) ? 0 : 2;
	neutrino_locale_t ok_loc = (m_settings.gui == MB_GUI_FILTER && m_windowFocus == MB_FOCUS_FILTER) ?  LOCALE_BOOKMARKMANAGER_SELECT : LOCALE_MOVIEBROWSER_FOOT_PLAY;
	int ok_loc_len = std::max(FOOT_FONT->getRenderWidth(g_Locale->getText(LOCALE_BOOKMARKMANAGER_SELECT), true),
				  FOOT_FONT->getRenderWidth(g_Locale->getText(LOCALE_MOVIEBROWSER_FOOT_PLAY), true));
	std::string filter_text = g_Locale->getText(LOCALE_MOVIEBROWSER_FOOT_FILTER);
	filter_text += " ";
	filter_text += m_settings.filter.optionString;
	std::string sort_text = g_Locale->getText(LOCALE_MOVIEBROWSER_FOOT_SORT);
	sort_text += " ";
	int sort_text_len = FOOT_FONT->getRenderWidth(sort_text, true);
	int len = 0;
	for (int i = 0; m_localizedItemName[i] != NONEXISTANT_LOCALE; i++)
		len = std::max(len, FOOT_FONT->getRenderWidth(g_Locale->getText(m_localizedItemName[i]), true));
	sort_text_len += len;
	sort_text += g_Locale->getText(m_localizedItemName[m_settings.sorting.item]);

	button_label_ext footerButtons[] = {
		{ NEUTRINO_ICON_BUTTON_RED,		NONEXISTANT_LOCALE,			sort_text.c_str(),	sort_text_len,	false },
		{ NEUTRINO_ICON_BUTTON_GREEN,		NONEXISTANT_LOCALE,			filter_text.c_str(),	0,		true  },
		{ NEUTRINO_ICON_BUTTON_YELLOW,		LOCALE_MOVIEBROWSER_FOOT_FOCUS,		NULL,			0,		false },
		{ NEUTRINO_ICON_BUTTON_BLUE,		LOCALE_MOVIEBROWSER_FOOT_REFRESH,	NULL,			0,		false },
		{ NEUTRINO_ICON_BUTTON_OKAY,		ok_loc,					NULL,			ok_loc_len,	false },
		{ NEUTRINO_ICON_BUTTON_MUTE_SMALL,	LOCALE_FILEBROWSER_DELETE,		NULL,			0,		false },
		{ NEUTRINO_ICON_BUTTON_PLAY,		LOCALE_FILEBROWSER_MARK,		NULL,			0,		false },
		{ NEUTRINO_ICON_BUTTON_MENU_SMALL,	LOCALE_MOVIEBROWSER_FOOT_OPTIONS,	NULL,			0,		false }
	};
	int cnt = sizeof(footerButtons) / sizeof(button_label_ext);

	if (show)
		return paintButtons(footerButtons + offset, cnt - offset, m_cBoxFrameFootRel.iX, m_cBoxFrameFootRel.iY, m_cBoxFrameFootRel.iWidth, m_cBoxFrameFootRel.iHeight, m_cBoxFrameFootRel.iWidth);
	else
		return paintButtons(footerButtons, cnt, 0, 0, 0, 0, 0, false, NULL, NULL);
}

bool CMovieBrowser::onButtonPress(neutrino_msg_t msg)
{
//	TRACE("[mb]->onButtonPress %d\n",msg);
	bool result = onButtonPressMainFrame(msg);
	if (result == false)
	{
		// if Main Frame didnot process the button, the focused window may do
		if (m_windowFocus == MB_FOCUS_BROWSER)
			result = onButtonPressBrowserList(msg);
		else if (m_windowFocus == MB_FOCUS_LAST_PLAY)
			result = onButtonPressLastPlayList(msg);
		else if (m_windowFocus == MB_FOCUS_LAST_RECORD)
			result = onButtonPressLastRecordList(msg);
		else if (m_windowFocus == MB_FOCUS_MOVIE_INFO1 || m_windowFocus == MB_FOCUS_MOVIE_INFO2)
			result = onButtonPressMovieInfoList(msg);
		else if (m_windowFocus == MB_FOCUS_FILTER)
			result = onButtonPressFilterList(msg);
	}
	return (result);
}

bool CMovieBrowser::onButtonPressMainFrame(neutrino_msg_t msg)
{
	//TRACE("[mb]->onButtonPressMainFrame: %d\n",msg);
	bool result = true;
	neutrino_msg_data_t data;

	if (msg == (neutrino_msg_t) g_settings.mbkey_copy_onefile
	 || msg == (neutrino_msg_t) g_settings.mbkey_copy_several
	 || msg == (neutrino_msg_t) g_settings.mbkey_cut
	 || msg == (neutrino_msg_t) g_settings.mbkey_truncate)
	{
		if (msg == (neutrino_msg_t) g_settings.mbkey_copy_onefile)
			exec(NULL, "copy_onefile");
		else
		if (msg == (neutrino_msg_t) g_settings.mbkey_copy_several)
			exec(NULL, "copy_several");
		else
		if (msg == (neutrino_msg_t) g_settings.mbkey_cut)
			exec(NULL, "cut");
		else
		if (msg == (neutrino_msg_t) g_settings.mbkey_truncate)
			exec(NULL, "truncate");

		if (m_doLoadMovies)
			loadMovies();
		if (m_doRefresh)
			refresh();
	}
	else if (msg == (neutrino_msg_t) g_settings.mbkey_cover)
	{
		if (m_movieSelectionHandler != NULL) {
			std::string cover_file = getScreenshotName(m_movieSelectionHandler->file.Name, S_ISDIR(m_movieSelectionHandler->file.Mode));
			if (!cover_file.empty())
			{
				//delete Cover
				if (ShowMsg(LOCALE_MESSAGEBOX_INFO, LOCALE_MOVIEBROWSER_DELETE_SCREENSHOT, CMsgBox::mbrNo, CMsgBox:: mbYes | CMsgBox::mbNo) == CMsgBox::mbrYes)
				{
					unlink(cover_file.c_str());
					refresh();
				}
			}
			else if (g_settings.tmdb_enabled)
			{
				//add TMDB Cover
				cover_file = m_movieSelectionHandler->file.Name.c_str();
				int ext_pos = 0;
				ext_pos = cover_file.rfind('.');
				if( ext_pos > 0) {
					std::string extension;
					extension = cover_file.substr(ext_pos + 1, cover_file.length() - ext_pos);
					extension = "." + extension;
					str_replace(extension, ".jpg", cover_file);
					printf("TMDB: %s : %s\n",m_movieSelectionHandler->file.Name.c_str(),cover_file.c_str());
					cTmdb* tmdb = new cTmdb(m_movieSelectionHandler->epgTitle);
					if ((tmdb->getResults() > 0) && (tmdb->hasCover())) {
						if (!cover_file.empty())
							if (tmdb->getSmallCover(cover_file))
								refresh();
					}
					if (tmdb)
						delete tmdb;
				}
			}
		}
	}
	else if (msg == CRCInput::RC_home)
	{
		if (m_settings.gui == MB_GUI_FILTER)
			onSetGUIWindow(MB_GUI_MOVIE_INFO);
		else
			result = false;
	}
	else if (msg == CRCInput::RC_left)
	{
		if (m_windowFocus == MB_FOCUS_MOVIE_INFO2 && m_settings.browserAdditional)
			onSetFocusNext();
		else if (show_mode != MB_SHOW_YT)
			onSetGUIWindowPrev();
	}
	else if (msg == CRCInput::RC_right)
	{
		if (m_windowFocus == MB_FOCUS_BROWSER && m_settings.browserAdditional)
			onSetFocusNext();
		else if (show_mode != MB_SHOW_YT)
			onSetGUIWindowNext();
	}
	else if (msg == CRCInput::RC_green)
	{
		if (m_settings.gui == MB_GUI_MOVIE_INFO)
			onSetGUIWindow(MB_GUI_FILTER);
		else if (m_settings.gui == MB_GUI_FILTER)
			onSetGUIWindow(MB_GUI_MOVIE_INFO);
		// no effect if gui is last play or record
	}
	else if (msg == CRCInput::RC_yellow)
	{
		onSetFocusNext();
	}
	else if (msg == CRCInput::RC_blue)
	{
		if (show_mode == MB_SHOW_YT)
			ytparser.Cleanup();

		loadMovies();
		refresh();
	}
	else if (msg == CRCInput::RC_red)
	{
		if (m_settings.gui != MB_GUI_LAST_PLAY && m_settings.gui != MB_GUI_LAST_RECORD)
		{
			// sorting is not avialable for last play and record

			int directkey = 1;
			int selected = -1;
			CMenuSelectorTarget * selector = new CMenuSelectorTarget(&selected);

			CMenuWidget m(LOCALE_MOVIEBROWSER_FOOT_SORT, NEUTRINO_ICON_SETTINGS);
			m.addIntroItems();

			// add PREVPLAYDATE/RECORDDATE sort buttons to footer
			m.addKey(CRCInput::RC_red, selector, to_string(MB_INFO_PREVPLAYDATE).c_str());
			m.addKey(CRCInput::RC_green, selector, to_string(MB_INFO_RECORDDATE).c_str());

			button_label footerButtons[] = {
				{ NEUTRINO_ICON_BUTTON_RED,	LOCALE_MOVIEBROWSER_INFO_PREVPLAYDATE},
				{ NEUTRINO_ICON_BUTTON_GREEN,	LOCALE_MOVIEBROWSER_INFO_RECORDDATE}
			};
			int footerButtonsCount = sizeof(footerButtons) / sizeof(button_label);

			m.setFooter(footerButtons, footerButtonsCount);

			// just show sorting options for displayed rows; sorted by rows
			for (int row = 0; row < MB_MAX_ROWS && row < m_settings.browserRowNr; row++)
			{
				for (unsigned int i = 0; i < MB_INFO_MAX_NUMBER; i++)
				{
					if (sortBy[i] == NULL)
						continue;

					// already added to footer
					if (i == MB_INFO_PREVPLAYDATE || i == MB_INFO_RECORDDATE)
						continue;

					if (m_settings.browserRowItem[row] == i)
						m.addItem(new CMenuForwarder(g_Locale->getText(m_localizedItemName[i]), true, NULL, selector, to_string(i).c_str(), CRCInput::convertDigitToKey(directkey++)));
				}
			}

			m.enableSaveScreen(true);
			m.exec(NULL, "");

			delete selector;

			if (selected >= 0)
			{
				m_settings.sorting.item = (MB_INFO_ITEM) selected;

				TRACE("[mb]->new sorting %d, %s\n", m_settings.sorting.item, g_Locale->getText(m_localizedItemName[m_settings.sorting.item]));

				refreshBrowserList();
				refreshMovieInfo();
				refreshFoot();
			}
		}
	}
	else if (msg == CRCInput::RC_spkr)
	{
		if (m_movieSelectionHandler != NULL)
		{
			onDelete();
		}
	}
	else if (msg == CRCInput::RC_help)
	{
		showHelp();
	}
	else if (msg == CRCInput::RC_info)
	{
		if (m_movieSelectionHandler != NULL)
		{
			framebuffer->paintBackground(); //clear whole screen
			g_EpgData->show_mp(m_movieSelectionHandler);
			refresh();
		}
	}
	else if (msg == CRCInput::RC_setup)
	{
		if (show_mode == MB_SHOW_YT)
			showYTMenu();
		else
		{
			showMenu();
			if (m_doLoadMovies)
				loadMovies();
			if (m_doRefresh)
				refresh();
		}

	}
	else if (g_settings.sms_movie && (msg >= CRCInput::RC_1) && (msg <= CRCInput::RC_9))
	{
		unsigned char smsKey = 0;
		SMSKeyInput smsInput;
		smsInput.setTimeout(MOVIE_SMSKEY_TIMEOUT);

		std::vector<MI_MOVIE_INFO*> *current_list = NULL;
		CListFrame *current_frame = NULL;

		if (m_windowFocus == MB_FOCUS_BROWSER)
		{
			current_list = &m_vHandleBrowserList;
			current_frame = m_pcBrowser;
		}
		else if (m_windowFocus == MB_FOCUS_LAST_PLAY)
		{
			current_list = &m_vHandlePlayList;
			current_frame = m_pcLastPlay;
		}
		else if (m_windowFocus == MB_FOCUS_LAST_RECORD)
		{
			current_list = &m_vHandleRecordList;
			current_frame = m_pcLastRecord;
		}

		if (current_list == NULL || current_frame == NULL)
			return result;

		do {
			smsKey = smsInput.handleMsg(msg);
			printf("SMS new key: %c\n", smsKey);
			g_RCInput->getMsg_ms(&msg, &data, MOVIE_SMSKEY_TIMEOUT-100);
		} while ((msg >= CRCInput::RC_1) && (msg <= CRCInput::RC_9));

		int selected = current_frame->getSelectedLine();
		if (msg == CRCInput::RC_timeout || msg == CRCInput::RC_nokey) {
			uint32_t i;
			for (i = selected+1; i < (*current_list).size(); i++) {

				char firstCharOfTitle = (*current_list)[i]->epgTitle.c_str()[0];
				if (tolower(firstCharOfTitle) == smsKey) {
					printf("SMS found selected=%d i=%d \"%s\"\n", selected, i, (*current_list)[i]->epgTitle.c_str());
					break;
				}
			}
			if (i >= (*current_list).size()) {
				for (i = 0; i < (*current_list).size(); i++) {
					char firstCharOfTitle = (*current_list)[i]->epgTitle.c_str()[0];
					if (tolower(firstCharOfTitle) == smsKey) {
						printf("SMS found selected=%d i=%d \"%s\"\n", selected, i, (*current_list)[i]->epgTitle.c_str());
						break;
					}
				}
			}
			if (i < (*current_list).size()) {
				current_frame->setSelectedLine(i);
				updateMovieSelection();
			}

			smsInput.resetOldKey();
		}
	}
	else
	{
		//TRACE("[mb]->onButtonPressMainFrame none\n");
		result = false;
	}

	return (result);
}

void CMovieBrowser::markItem(CListFrame *list)
{
	if(m_movieSelectionHandler != NULL){
		m_movieSelectionHandler->marked = !m_movieSelectionHandler->marked;
		list->setSelectedMarked(m_movieSelectionHandler->marked);
		list->scrollLineDown(1);
	}
}

void CMovieBrowser::scrollBrowserItem(bool next, bool page)
{
	int mode = -1;
	if (show_mode == MB_SHOW_YT && next && ytparser.HaveNext() && m_pcBrowser->getSelectedLine() == m_pcBrowser->getLines() - 1)
		mode = cYTFeedParser::NEXT;
	if (show_mode == MB_SHOW_YT && !next && ytparser.HavePrev() && m_pcBrowser->getSelectedLine() == 0)
		mode = cYTFeedParser::PREV;
	if (mode >= 0) {
		CHintBox loadBox(LOCALE_MOVIEPLAYER_YTPLAYBACK, g_Locale->getText(LOCALE_MOVIEBROWSER_SCAN_FOR_MOVIES));
		loadBox.paint();
		ytparser.Cleanup();
		loadYTitles(mode, m_settings.ytsearch, m_settings.ytvid);
		loadBox.hide();
		refreshBrowserList();
		refreshMovieInfo();
		g_RCInput->clearRCMsg();
		return;
	}
	if (next)
		page ? m_pcBrowser->scrollPageDown(1) : m_pcBrowser->scrollLineDown(1);
	else
		page ? m_pcBrowser->scrollPageUp(1) : m_pcBrowser->scrollLineUp(1);
}

bool CMovieBrowser::onButtonPressBrowserList(neutrino_msg_t msg)
{
	//TRACE("[mb]->onButtonPressBrowserList %d\n",msg);
	bool result = true;

	if (msg == CRCInput::RC_up)
		scrollBrowserItem(false, false);
	else if (msg == CRCInput::RC_down)
		scrollBrowserItem(true, false);
	else if (msg == (neutrino_msg_t)g_settings.key_pageup)
		scrollBrowserItem(false, true);
	else if (msg == (neutrino_msg_t)g_settings.key_pagedown)
		scrollBrowserItem(true, true);
	else if (msg == CRCInput::RC_play)
		markItem(m_pcBrowser);
	else
		result = false;

	if (result == true)
		updateMovieSelection();

	return (result);
}

bool CMovieBrowser::onButtonPressLastPlayList(neutrino_msg_t msg)
{
	//TRACE("[mb]->onButtonPressLastPlayList %d\n",msg);
	bool result = true;

	if (msg==CRCInput::RC_up)
		m_pcLastPlay->scrollLineUp(1);
	else if (msg == CRCInput::RC_down)
		m_pcLastPlay->scrollLineDown(1);
	else if (msg == (neutrino_msg_t)g_settings.key_pageup)
		m_pcLastPlay->scrollPageUp(1);
	else if (msg == (neutrino_msg_t)g_settings.key_pagedown)
		m_pcLastPlay->scrollPageDown(1);
	else if (msg == CRCInput::RC_play)
		markItem(m_pcLastPlay);
	else
		result = false;

	if (result == true)
		updateMovieSelection();

	return (result);
}

bool CMovieBrowser::onButtonPressLastRecordList(neutrino_msg_t msg)
{
	//TRACE("[mb]->onButtonPressLastRecordList %d\n",msg);
	bool result = true;

	if (msg == CRCInput::RC_up)
		m_pcLastRecord->scrollLineUp(1);
	else if (msg == CRCInput::RC_down)
		m_pcLastRecord->scrollLineDown(1);
	else if (msg == (neutrino_msg_t)g_settings.key_pageup)
		m_pcLastRecord->scrollPageUp(1);
	else if (msg == (neutrino_msg_t)g_settings.key_pagedown)
		m_pcLastRecord->scrollPageDown(1);
	else if (msg == CRCInput::RC_play)
		markItem(m_pcLastRecord);
	else
		result = false;

	if (result == true)
		updateMovieSelection();

	return (result);
}

bool CMovieBrowser::onButtonPressFilterList(neutrino_msg_t msg)
{
	//TRACE("[mb]->onButtonPressFilterList %d,%d\n",msg,m_settings.filter.item);
	bool result = true;

	if (msg==CRCInput::RC_up)
	{
		m_pcFilter->scrollLineUp(1);
	}
	else if (msg == CRCInput::RC_down)
	{
		m_pcFilter->scrollLineDown(1);
	}
	else if (msg == (neutrino_msg_t)g_settings.key_pageup)
	{
		m_pcFilter->scrollPageUp(1);
	}
	else if (msg == (neutrino_msg_t)g_settings.key_pagedown)
	{
		m_pcFilter->scrollPageDown(1);
	}
	else if (msg == CRCInput::RC_ok)
	{
		int selected_line = m_pcFilter->getSelectedLine();
		if (m_settings.filter.item == MB_INFO_MAX_NUMBER)
		{
			if (selected_line == 0) m_settings.filter.item = MB_INFO_MAJOR_GENRE;
			if (selected_line == 1) m_settings.filter.item = MB_INFO_INFO1;
			if (selected_line == 2) m_settings.filter.item = MB_INFO_FILEPATH;
			if (selected_line == 3) m_settings.filter.item = MB_INFO_SERIE;
			refreshFilterList();
			m_pcFilter->setSelectedLine(0);
		}
		else
		{
			if (selected_line == 0)
			{
				m_settings.filter.item = MB_INFO_MAX_NUMBER;
				m_settings.filter.optionString = g_Locale->getText(LOCALE_OPTIONS_OFF);
				m_settings.filter.optionVar = 0;
				refreshFilterList();
				m_pcFilter->setSelectedLine(0);
				refreshBrowserList();
				refreshLastPlayList();
				refreshLastRecordList();
				refreshFoot();
			}
			else
			{
				updateFilterSelection();
			}
		}
	}
	else
	{
		// default
		result = false;
	}

	return (result);
}

bool CMovieBrowser::onButtonPressMovieInfoList(neutrino_msg_t msg)
{
//	TRACE("[mb]->onButtonPressEPGInfoList %d\n",msg);
	bool result = true;

	if (msg == CRCInput::RC_up)
		if (m_windowFocus == MB_FOCUS_MOVIE_INFO2 && m_settings.browserAdditional)
			m_pcInfo2->scrollPageUp(1);
		else
			m_pcInfo1->scrollPageUp(1);
	else if (msg == CRCInput::RC_down)
		if (m_windowFocus == MB_FOCUS_MOVIE_INFO2 && m_settings.browserAdditional)
			m_pcInfo2->scrollPageDown(1);
		else
			m_pcInfo1->scrollPageDown(1);
	else
		result = false;

	updateInfoSelection();

	return (result);
}

std::string CMovieBrowser::formatDeleteMsg(MI_MOVIE_INFO *movieinfo, int msgFont, const int boxWidth)
{
	Font *msgFont_ = g_Font[msgFont];
	int msgWidth = boxWidth - 20;
	std::string msg = g_Locale->getText(LOCALE_FILEBROWSER_DODELETE1);
	msg += "\n";

	if (!movieinfo->epgTitle.empty()) {
		int titleW = msgFont_->getRenderWidth(movieinfo->epgTitle);
		int infoW = 0;
		int zW = 0;
		if (!movieinfo->epgInfo1.empty()) {
			infoW = msgFont_->getRenderWidth(movieinfo->epgInfo1);
			zW = msgFont_->getRenderWidth(" ()");
		}

		if ((titleW+infoW+zW) <= msgWidth) {
			/* one line */
			msg += trim(movieinfo->epgTitle);
			if (!movieinfo->epgInfo1.empty()) {
				msg += " (";
				msg += trim(movieinfo->epgInfo1);
				msg += ")";
			}
		}
		else {
			/* two lines */
			msg += cutString(movieinfo->epgTitle, msgFont, msgWidth);
			if (!movieinfo->epgInfo1.empty()) {
				msg += "\n(";
				msg += cutString(movieinfo->epgInfo1, msgFont, msgWidth);
				msg += ")";
			}
		}
	}
	else
		msg += cutString(movieinfo->file.Name, msgFont, msgWidth);

	msg += "\n";
	msg += g_Locale->getText(LOCALE_FILEBROWSER_DODELETE2);

	return msg;
}

bool CMovieBrowser::onDeleteFile(MI_MOVIE_INFO *movieinfo, bool skipAsk)
{
	//TRACE("[onDeleteFile] ");
	bool result = false;

	/* default font for ShowMsg */
	int msgFont = SNeutrinoSettings::FONT_TYPE_MENU;
	/* default width for ShowMsg */
	int msgBoxWidth = 450;

	std::string msg = formatDeleteMsg(movieinfo, msgFont, msgBoxWidth);
	if ((skipAsk || !movieinfo->delAsk) || (ShowMsg(LOCALE_FILEBROWSER_DELETE, msg, CMsgBox::mbrYes, CMsgBox::mbYes|CMsgBox::mbNo, NULL, msgBoxWidth)==CMsgBox::mbrYes))
	{
		CHintBox hintBox(LOCALE_MESSAGEBOX_INFO, g_Locale->getText(LOCALE_MOVIEBROWSER_DELETE_INFO));
		hintBox.paint();
		delFile(movieinfo->file);

		std::string cover_file = getScreenshotName(movieinfo->file.Name, S_ISDIR(movieinfo->file.Mode));
		if (!cover_file.empty())
			unlink(cover_file.c_str());

		CFile file_xml = movieinfo->file;
		if (m_movieInfo.convertTs2XmlName(file_xml.Name))
			unlink(file_xml.Name.c_str());

		hintBox.hide();
		g_RCInput->clearRCMsg();

		TRACE("List size: %d\n", (int)m_vMovieInfo.size());
		for (std::vector<MI_MOVIE_INFO>::iterator mi_it = m_vMovieInfo.begin(); mi_it != m_vMovieInfo.end(); ++mi_it)
		{
			if (
				   mi_it->file.Name == movieinfo->file.Name
				&& mi_it->epgTitle == movieinfo->epgTitle
				&& mi_it->epgInfo1 == movieinfo->epgInfo1
				&& mi_it->length == movieinfo->length
			)
				m_vMovieInfo.erase(mi_it--);
		}

		updateSerienames();
		refreshBrowserList();
		refreshLastPlayList();
		refreshLastRecordList();
		refreshMovieInfo();
		refresh();

		result = true;
	}
	return (result);
}

bool CMovieBrowser::onDelete(bool cursor_only)
{
	//TRACE("[onDelete] ");
	bool result = false;

	MI_MOVIE_INFO *movieinfo;
	movieinfo = NULL;
	filelist_it = filelist.end();
	if (!cursor_only && getSelectedFiles(filelist, movielist))
		filelist_it = filelist.begin();
	if (filelist.empty()) { //just add the m_movieSelectionHandler
		filelist.push_back(m_movieSelectionHandler->file);
		movielist.push_back(m_movieSelectionHandler);
	}

	MI_MOVIE_LIST dellist;
	MI_MOVIE_LIST::iterator dellist_it;
	dellist.clear();
	unsigned int dellist_cnt = 0;
	for (filelist_it = filelist.begin(); filelist_it != filelist.end(); ++filelist_it)
	{
		unsigned int idx = filelist_it - filelist.begin();
		movieinfo = movielist[idx];
		TRACE("[mb]-> try to delete %d:%s\n", idx, movieinfo->file.Name.c_str());

		if ((!m_vMovieInfo.empty()) && (movieinfo != NULL)) {
			bool toDelete = true;
			CRecordInstance* inst = CRecordManager::getInstance()->getRecordInstance(movieinfo->file.Name);
			if (inst != NULL) {
				std::string delName = movieinfo->epgTitle;
				if (delName.empty())
					delName = movieinfo->file.getFileName();
				char buf1[1024];
				snprintf(buf1, sizeof(buf1), g_Locale->getText(LOCALE_MOVIEBROWSER_ASK_REC_TO_DELETE), delName.c_str());
				if (ShowMsg(LOCALE_RECORDINGMENU_RECORD_IS_RUNNING, buf1,
						CMsgBox::mbrNo, CMsgBox::mbYes | CMsgBox::mbNo, NULL, 450, 30, false) == CMsgBox::mbrNo)
					toDelete = false;
				else {
					CTimerd::RecordingStopInfo recinfo;
					recinfo.channel_id = inst->GetChannelId();
					recinfo.eventID = inst->GetRecordingId();
					CRecordManager::getInstance()->Stop(&recinfo);
					g_Timerd->removeTimerEvent(recinfo.eventID);
					movieinfo->delAsk = false; //delete this file w/o any more question
				}
			}
			if (toDelete)
			{
				dellist.push_back(*movieinfo);
				dellist_cnt++;
			}
		}
	}
	if (!dellist.empty()) {
		bool skipAsk = false;
		if (dellist_cnt > 1)
			skipAsk = (ShowMsg(LOCALE_FILEBROWSER_DELETE, LOCALE_MOVIEBROWSER_DELETE_ALL, CMsgBox::mbrNo, CMsgBox:: mbYes | CMsgBox::mbNo) == CMsgBox::mbrYes);
		for (dellist_it = dellist.begin(); dellist_it != dellist.end(); ++dellist_it)
			result |= onDeleteFile((MI_MOVIE_INFO *)&(*dellist_it), skipAsk);
		dellist.clear();
	}
	return (result);
}

void CMovieBrowser::onSetGUIWindow(MB_GUI gui)
{
	TRACE("[mb]->onSetGUIWindow: gui %d -> %d\n", m_settings.gui, gui);
	m_settings.gui = gui;

	hideDetailsLine();

	m_showMovieInfo = true;
	if (gui == MB_GUI_MOVIE_INFO) {
		m_showBrowserFiles = true;

		m_showLastRecordFiles = false;
		m_showLastPlayFiles = false;
		m_showFilter = false;

		m_pcLastPlay->hide();
		m_pcLastRecord->hide();
		m_pcFilter->hide();
		m_pcBrowser->paint();
		onSetFocus(MB_FOCUS_BROWSER);
	} else if (gui == MB_GUI_LAST_PLAY) {
		clearSelection();

		m_showLastRecordFiles = true;
		m_showLastPlayFiles = true;

		m_showBrowserFiles = false;
		m_showFilter = false;

		m_pcBrowser->hide();
		m_pcFilter->hide();
		m_pcInfo2->hide();
		m_pcLastRecord->paint();
		m_pcLastPlay->paint();

		onSetFocus(MB_FOCUS_LAST_PLAY);
	} else if (gui == MB_GUI_LAST_RECORD) {
		clearSelection();

		m_showLastRecordFiles = true;
		m_showLastPlayFiles = true;

		m_showBrowserFiles = false;
		m_showFilter = false;

		m_pcBrowser->hide();
		m_pcFilter->hide();
		m_pcInfo2->hide();
		m_pcLastRecord->paint();
		m_pcLastPlay->paint();

		onSetFocus(MB_FOCUS_LAST_RECORD);
	} else if (gui == MB_GUI_FILTER) {
		m_showFilter = true;

		m_showMovieInfo = false;

		m_pcInfo1->hide();
		if (m_windowFocus == MB_FOCUS_BROWSER && m_settings.browserAdditional)
		{
			hideMovieCover();
			m_pcInfo2->clear();
		}
		m_pcFilter->paint();

		onSetFocus(MB_FOCUS_FILTER);
	}
	if (m_showMovieInfo) {
		m_pcInfo1->paint();
		if (m_windowFocus == MB_FOCUS_BROWSER && m_settings.browserAdditional)
			m_pcInfo2->paint();
		refreshMovieInfo();
	}
}

void CMovieBrowser::onSetGUIWindowNext(void)
{
	if (m_settings.gui == MB_GUI_MOVIE_INFO)
		onSetGUIWindow(MB_GUI_LAST_PLAY);
	else if (m_settings.gui == MB_GUI_LAST_PLAY)
		onSetGUIWindow(MB_GUI_LAST_RECORD);
	else
		onSetGUIWindow(MB_GUI_MOVIE_INFO);
}

void CMovieBrowser::onSetGUIWindowPrev(void)
{
	if (m_settings.gui == MB_GUI_MOVIE_INFO)
		onSetGUIWindow(MB_GUI_LAST_RECORD);
	else if (m_settings.gui == MB_GUI_LAST_RECORD)
		onSetGUIWindow(MB_GUI_LAST_PLAY);
	else
		onSetGUIWindow(MB_GUI_MOVIE_INFO);
}

void CMovieBrowser::onSetFocus(MB_FOCUS new_focus)
{
	TRACE("[mb]->onSetFocus: focus %d -> %d \n", m_windowFocus, new_focus);
	clearSelection();

	m_windowFocus = new_focus;
	m_pcBrowser->showSelection(false);
	m_pcLastRecord->showSelection(false);
	m_pcLastPlay->showSelection(false);
	m_pcFilter->showSelection(false);

	if (m_windowFocus == MB_FOCUS_BROWSER)
		m_pcBrowser->showSelection(true);
	else if (m_windowFocus == MB_FOCUS_LAST_PLAY)
		m_pcLastPlay->showSelection(true);
	else if (m_windowFocus == MB_FOCUS_LAST_RECORD)
		m_pcLastRecord->showSelection(true);
	else if (m_windowFocus == MB_FOCUS_FILTER)
		m_pcFilter->showSelection(true);

	updateMovieSelection();
	if (m_windowFocus != MB_FOCUS_FILTER)
		updateInfoSelection();
	refreshFoot();
}

void CMovieBrowser::onSetFocusNext(void)
{
	TRACE("[mb]->onSetFocusNext: gui %d\n", m_settings.gui);

	if (m_settings.gui == MB_GUI_FILTER)
	{
		if (m_windowFocus == MB_FOCUS_BROWSER)
			onSetFocus(MB_FOCUS_FILTER);
		else
			onSetFocus(MB_FOCUS_BROWSER);
	}
	else if (m_settings.gui == MB_GUI_MOVIE_INFO)
	{
		if (m_windowFocus == MB_FOCUS_BROWSER)
			if (m_settings.browserAdditional)
				onSetFocus(MB_FOCUS_MOVIE_INFO2);
			else
				onSetFocus(MB_FOCUS_MOVIE_INFO1);
		else
			onSetFocus(MB_FOCUS_BROWSER);
	}
	else if (m_settings.gui == MB_GUI_LAST_PLAY)
	{
		if (m_windowFocus == MB_FOCUS_MOVIE_INFO1 || m_windowFocus == MB_FOCUS_MOVIE_INFO2)
			onSetFocus(MB_FOCUS_LAST_PLAY);
		else if (m_windowFocus == MB_FOCUS_LAST_PLAY)
			onSetFocus(MB_FOCUS_MOVIE_INFO1);
	}
	else if (m_settings.gui == MB_GUI_LAST_RECORD)
	{
		if (m_windowFocus == MB_FOCUS_MOVIE_INFO1 || m_windowFocus == MB_FOCUS_MOVIE_INFO2)
			onSetFocus(MB_FOCUS_LAST_RECORD);
		else if (m_windowFocus == MB_FOCUS_LAST_RECORD)
			onSetFocus(MB_FOCUS_MOVIE_INFO1);
	}
}

bool CMovieBrowser::onSortMovieInfoHandleList(std::vector<MI_MOVIE_INFO*>& handle_list, MB_INFO_ITEM sort_item, MB_DIRECTION direction)
{
	//TRACE("sort: %d\n",direction);
	if (handle_list.empty())
		return false;
	if (sortBy[sort_item] == NULL)
		return false;

	if (direction == MB_DIRECTION_AUTO)
	{
		if (sort_item == MB_INFO_QUALITY || sort_item == MB_INFO_PARENTAL_LOCKAGE ||
				sort_item == MB_INFO_PREVPLAYDATE || sort_item == MB_INFO_RECORDDATE ||
				sort_item == MB_INFO_PRODDATE || sort_item == MB_INFO_SIZE ||
				sort_item == MB_INFO_RATING)
			sortDirection = 1;
		else
			sortDirection = 0;
	}
	else if (direction == MB_DIRECTION_UP)
	{
		sortDirection = 0;
	}
	else
	{
		sortDirection = 1;
	}

	//TRACE("sort: %d\n",sortDirection);
	sort(handle_list.begin(), handle_list.end(), sortBy[sort_item]);

	return (true);
}

void CMovieBrowser::updateDir(void)
{
	m_dir.clear();
#if 0
	// check if there is a movie dir and if we should use it
	if (g_settings.network_nfs_moviedir[0] != 0)
	{
		std::string name = g_settings.network_nfs_moviedir;
		addDir(name,&m_settings.storageDirMovieUsed);
	}
#endif
	// check if there is a record dir and if we should use it
	if (!g_settings.network_nfs_recordingdir.empty())
	{
		addDir(g_settings.network_nfs_recordingdir, &m_settings.storageDirRecUsed);
		cHddStat::getInstance()->statOnce();
	}

	for (int i = 0; i < MB_MAX_DIRS; i++)
	{
		if (!m_settings.storageDir[i].empty())
			addDir(m_settings.storageDir[i],&m_settings.storageDirUsed[i]);
	}
}

void CMovieBrowser::loadAllTsFileNamesFromStorage(void)
{
	//TRACE("[mb]->loadAllTsFileNamesFromStorage \n");
	int i,size;

	m_movieSelectionHandler = NULL;
	m_dirNames.clear();
	m_vMovieInfo.clear();

	updateDir();

	size = m_dir.size();
	for (i=0; i < size;i++)
	{
		if (*m_dir[i].used == true)
			loadTsFileNamesFromDir(m_dir[i].name);
	}

	TRACE("[mb] Dir%d, Files:%d\n", (int)m_dirNames.size(), (int)m_vMovieInfo.size());
}

bool CMovieBrowser::gotMovie(const char *rec_title)
{
	//TRACE("[mb]->gotMovie\n");

	m_doRefresh = false;
	loadAllTsFileNamesFromStorage();

	bool found = false;
	for (unsigned int i = 0; i < m_vMovieInfo.size(); i++)
	{
		//printf("[mb] search for %s in %s\n", rec_title, m_vMovieInfo[i].epgTitle.c_str());
		if (strcmp(rec_title, m_vMovieInfo[i].epgTitle.c_str()) == 0)
		{
			found = true;
			break;
		}
	}
	return found;
}

static const char * const ext_list[] =
{
	"avi", "mkv", "mp4", "flv", "mov", "mpg", "mpeg", "m2ts", "iso"
};

static int ext_list_size = sizeof(ext_list) / sizeof (char *);

bool CMovieBrowser::supportedExtension(CFile &file)
{
	std::string::size_type idx = file.getFileName().rfind('.');

	if (idx == std::string::npos)
		return false;

	std::string ext = file.getFileName().substr(idx+1);
	bool result = (ext == "ts");
	if (!result && !m_settings.ts_only) {
		for (int i = 0; i < ext_list_size; i++) {
			if (!strcasecmp(ext.c_str(), ext_list[i]))
				return true;
		}
	}
	return result;
}

bool CMovieBrowser::addFile(CFile &file, int dirItNr)
{
	if (!S_ISDIR(file.Mode) && !supportedExtension(file)) {
		//TRACE("[mb] not supported file: '%s'\n", file.Name.c_str());
		return false;
	}

	MI_MOVIE_INFO movieInfo;

	movieInfo.file = file;
	if(!m_movieInfo.loadMovieInfo(&movieInfo)) {
		movieInfo.channelName = string(g_Locale->getText(LOCALE_MOVIEPLAYER_HEAD));
		movieInfo.epgTitle = file.getFileName();
	}
	movieInfo.dirItNr = dirItNr;
	//TRACE("addFile dir [%s] : [%s]\n", m_dirNames[movieInfo.dirItNr].c_str(), movieInfo.file.Name.c_str());
	m_vMovieInfo.push_back(movieInfo);
	return true;
}

/************************************************************************
Note: this function is used recursive, do not add any return within the body due to the recursive counter
************************************************************************/
bool CMovieBrowser::loadTsFileNamesFromDir(const std::string & dirname)
{
	//TRACE("[mb]->loadTsFileNamesFromDir %s\n",dirname.c_str());

	static int recursive_counter = 0; // recursive counter to be used to avoid hanging
	bool result = false;

	if (recursive_counter > 10)
	{
		TRACE("[mb]loadTsFileNamesFromDir: return->recoursive error\n");
		return (false); // do not go deeper than 10 directories
	}

	/* check if directory was already searched once */
	for (int i = 0; i < (int) m_dirNames.size(); i++)
	{
		if (strcmp(m_dirNames[i].c_str(),dirname.c_str()) == 0)
		{
			// string is identical to previous one
			TRACE("[mb]Dir already in list: %s\n",dirname.c_str());
			return (false);
		}
	}
	/* FIXME hack to fix movie dir path on recursive scan.
	   dirs without files but with subdirs with files will be shown in path filter */
	m_dirNames.push_back(dirname);
	int dirItNr = m_dirNames.size() - 1;

	/* !!!!!! no return statement within the body after here !!!!*/
	recursive_counter++;

	CFileList flist;
	if (readDir(dirname, &flist) == true)
	{
		for (unsigned int i = 0; i < flist.size(); i++)
		{
			if (S_ISDIR(flist[i].Mode)) {
				if (m_settings.ts_only || !CFileBrowser::checkBD(flist[i])) {
					flist[i].Name += '/';
					result |= loadTsFileNamesFromDir(flist[i].Name);
				} else
					result |= addFile(flist[i], dirItNr);
			} else {
				result |= addFile(flist[i], dirItNr);
			}
		}
		//result = true;
	}
	if (!result)
		m_dirNames.pop_back();

	recursive_counter--;
	return (result);
}

bool CMovieBrowser::readDir(const std::string & dirname, CFileList* flist)
{
	bool result = true;
	//TRACE("readDir_std %s\n",dirname.c_str());
	stat_struct statbuf;
	dirent_struct **namelist;
	int n;

	n = my_scandir(dirname.c_str(), &namelist, 0, my_alphasort);
	if (n < 0)
	{
		perror(("[mb] scandir: "+dirname).c_str());
		return false;
	}
	CFile file;
	for (int i = 0; i < n;i++)
	{
		if (namelist[i]->d_name[0] != '.')
		{
			file.Name = dirname;
			file.Name += namelist[i]->d_name;

//			printf("file.Name: '%s', getFileName: '%s' getPath: '%s'\n",file.Name.c_str(),file.getFileName().c_str(),file.getPath().c_str());
			if (my_stat((file.Name).c_str(),&statbuf) != 0)
				fprintf(stderr, "stat '%s' error: %m\n", file.Name.c_str());
			else
			{
				file.Mode = statbuf.st_mode;
				file.Time = statbuf.st_mtime;
				file.Size = statbuf.st_size;
				flist->push_back(file);
			}
		}
		free(namelist[i]);
	}

	free(namelist);

	return(result);
}

bool CMovieBrowser::delFile(CFile& file)
{
	bool result = true;
	int err = unlink(file.Name.c_str());
	TRACE("  delete file: %s\r\n",file.Name.c_str());
	if (err)
		result = false;
	return(result);
}

void CMovieBrowser::updateMovieSelection(void)
{
	//TRACE("[mb]->updateMovieSelection %d\n",m_windowFocus);
	if (m_vMovieInfo.empty()) return;
	bool new_selection = false;

	unsigned int old_movie_selection;
	if (m_windowFocus == MB_FOCUS_BROWSER)
	{
		if (m_vHandleBrowserList.empty())
		{
			// There are no elements in the Filebrowser, clear all handles
			m_currentBrowserSelection = 0;
			m_movieSelectionHandler = NULL;
			new_selection = true;
		}
		else
		{
			old_movie_selection = m_currentBrowserSelection;
			m_currentBrowserSelection = m_pcBrowser->getSelectedLine();
			//TRACE("    sel1:%d\n",m_currentBrowserSelection);
			if (m_currentBrowserSelection != old_movie_selection)
				new_selection = true;

			if (m_currentBrowserSelection < m_vHandleBrowserList.size())
				m_movieSelectionHandler = m_vHandleBrowserList[m_currentBrowserSelection];
		}
	}
	else if (m_windowFocus == MB_FOCUS_LAST_PLAY)
	{
		if (m_vHandlePlayList.empty())
		{
			// There are no elements in the Filebrowser, clear all handles
			m_currentPlaySelection = 0;
			m_movieSelectionHandler = NULL;
			new_selection = true;
		}
		else
		{
			old_movie_selection = m_currentPlaySelection;
			m_currentPlaySelection = m_pcLastPlay->getSelectedLine();
			//TRACE("    sel2:%d\n",m_currentPlaySelection);
			if (m_currentPlaySelection != old_movie_selection)
				new_selection = true;

			if (m_currentPlaySelection < m_vHandlePlayList.size())
				m_movieSelectionHandler = m_vHandlePlayList[m_currentPlaySelection];
		}
	}
	else if (m_windowFocus == MB_FOCUS_LAST_RECORD)
	{
		if (m_vHandleRecordList.empty())
		{
			// There are no elements in the Filebrowser, clear all handles
			m_currentRecordSelection = 0;
			m_movieSelectionHandler = NULL;
			new_selection = true;
		}
		else
		{
			old_movie_selection = m_currentRecordSelection;
			m_currentRecordSelection = m_pcLastRecord->getSelectedLine();
			//TRACE("    sel3:%d\n",m_currentRecordSelection);
			if (m_currentRecordSelection != old_movie_selection)
				new_selection = true;

			if (m_currentRecordSelection < m_vHandleRecordList.size())
				m_movieSelectionHandler = m_vHandleRecordList[m_currentRecordSelection];
		}
	}

	if (new_selection == true)
	{
		//TRACE("new\n");
		info_hdd_level();
		refreshMovieInfo();
		refreshLCD();
	}
	//TRACE("\n");
}

void CMovieBrowser::updateInfoSelection(void)
{
	int col_frame;
	int thickness = 2;
	int radius = m_pcInfo1->getBackGroundRadius();

	/*
	   Maybe we should change background of box
	   instead of changing frame color.
	*/
	if (m_windowFocus == MB_FOCUS_MOVIE_INFO1)
		col_frame = COL_MENUCONTENTSELECTED_PLUS_2;
	else
		col_frame = COL_FRAME_PLUS_0;

	// CTextBox can't paint frames, so let's do it "manually"
	framebuffer->paintBoxFrame(m_cBoxFrameInfo1.iX, m_cBoxFrameInfo1.iY, m_cBoxFrameInfo1.iWidth, m_cBoxFrameInfo1.iHeight, thickness, col_frame, radius);
}

void CMovieBrowser::updateFilterSelection(void)
{
	//TRACE("[mb]->updateFilterSelection \n");
	if (m_FilterLines.lineArray[0].empty()) return;

	bool result = true;
	int selected_line = m_pcFilter->getSelectedLine();
	if (selected_line > 0)
		selected_line--;

	if (m_settings.filter.item == MB_INFO_FILEPATH)
	{
		m_settings.filter.optionString = m_FilterLines.lineArray[0][selected_line+1];
		m_settings.filter.optionVar = selected_line;
	}
	else if (m_settings.filter.item == MB_INFO_INFO1)
	{
		m_settings.filter.optionString = m_FilterLines.lineArray[0][selected_line+1];
	}
	else if (m_settings.filter.item == MB_INFO_MAJOR_GENRE)
	{
		m_settings.filter.optionString = g_Locale->getText(GENRE_ALL[selected_line].value);
		m_settings.filter.optionVar = GENRE_ALL[selected_line].key;
	}
	else if (m_settings.filter.item == MB_INFO_SERIE)
	{
		m_settings.filter.optionString = m_FilterLines.lineArray[0][selected_line+1];
	}
	else
	{
		result = false;
	}
	if (result == true)
	{
		refreshBrowserList();
		refreshLastPlayList();
		refreshLastRecordList();
		refreshFoot();
	}
}

bool CMovieBrowser::addDir(std::string& dirname, int* used)
{
	if (dirname.empty()) return false;
	if (dirname == "/") return false;

	MB_DIR newdir;
	newdir.name = dirname;

	if (newdir.name.rfind('/') != newdir.name.length()-1 || newdir.name.length() == 0)
		newdir.name += '/';

	int size = m_dir.size();
	for (int i = 0; i < size; i++)
	{
		if (strcmp(m_dir[i].name.c_str(),newdir.name.c_str()) == 0)
		{
			// string is identical to previous one
			TRACE("[mb] Dir already in list: %s\n",newdir.name.c_str());
			return (false);
		}
	}
	TRACE("[mb] new Dir: %s\n",newdir.name.c_str());
	newdir.used = used;
	m_dir.push_back(newdir);
	if (*used == true)
	{
		m_file_info_stale = true; // we got a new Dir, search again for all movies next time
		m_seriename_stale = true;
	}
	return (true);
}

void CMovieBrowser::loadMovies(bool doRefresh)
{
	TRACE("[mb] loadMovies: \n");

	CHintBox loadBox((show_mode == MB_SHOW_YT) ? LOCALE_MOVIEPLAYER_YTPLAYBACK : LOCALE_MOVIEBROWSER_HEAD, g_Locale->getText(LOCALE_MOVIEBROWSER_SCAN_FOR_MOVIES));
	loadBox.paint();

	if (show_mode == MB_SHOW_YT) {
		loadYTitles(m_settings.ytmode, m_settings.ytsearch, m_settings.ytvid);
	} else {
		loadAllTsFileNamesFromStorage(); // P1
		m_seriename_stale = true; // we reloded the movie info, so make sure the other list are updated later on as well
		updateSerienames();
		if (m_settings.serie_auto_create == 1)
			autoFindSerie();
	}
	m_file_info_stale = false;

	loadBox.hide();

	if (doRefresh)
	{
		refreshBrowserList();
		refreshLastPlayList();
		refreshLastRecordList();
		refreshFilterList();
	}

	m_doLoadMovies = false;
}

void CMovieBrowser::loadAllMovieInfo(void)
{
	//TRACE("[mb]->loadAllMovieInfo \n");
	for (unsigned int i=0; i < m_vMovieInfo.size();i++)
		m_movieInfo.loadMovieInfo(&(m_vMovieInfo[i]));
}

void CMovieBrowser::showHelp(void)
{
	CMovieHelp help;
	help.exec();
}

#define MAX_STRING 30
int CMovieBrowser::showMovieInfoMenu(MI_MOVIE_INFO* movie_info)
{
	/********************************************************************/
	/**  MovieInfo menu ******************************************************/

	/********************************************************************/
	/**  bookmark ******************************************************/

	CIntInput bookStartIntInput(LOCALE_MOVIEBROWSER_EDIT_BOOK, (int *)&movie_info->bookmarks.start,        5, LOCALE_MOVIEBROWSER_EDIT_BOOK_POS_INFO1, LOCALE_MOVIEBROWSER_EDIT_BOOK_POS_INFO2);
	CIntInput bookLastIntInput(LOCALE_MOVIEBROWSER_EDIT_BOOK,  (int *)&movie_info->bookmarks.lastPlayStop, 5, LOCALE_MOVIEBROWSER_EDIT_BOOK_POS_INFO1, LOCALE_MOVIEBROWSER_EDIT_BOOK_POS_INFO2);
	CIntInput bookEndIntInput(LOCALE_MOVIEBROWSER_EDIT_BOOK,   (int *)&movie_info->bookmarks.end,          5, LOCALE_MOVIEBROWSER_EDIT_BOOK_POS_INFO1, LOCALE_MOVIEBROWSER_EDIT_BOOK_POS_INFO2);

	CMenuWidget bookmarkMenu(LOCALE_MOVIEBROWSER_HEAD, NEUTRINO_ICON_MOVIEPLAYER);

	bookmarkMenu.addIntroItems(LOCALE_MOVIEBROWSER_BOOK_HEAD);
	bookmarkMenu.addItem(new CMenuForwarder(LOCALE_MOVIEBROWSER_BOOK_CLEAR_ALL, true, NULL, this, "book_clear_all", CRCInput::RC_red));
	bookmarkMenu.addItem(GenericMenuSeparatorLine);
	bookmarkMenu.addItem(new CMenuForwarder(LOCALE_MOVIEBROWSER_BOOK_MOVIESTART,    true, bookStartIntInput.getValue(), &bookStartIntInput));
	bookmarkMenu.addItem(new CMenuForwarder(LOCALE_MOVIEBROWSER_BOOK_MOVIEEND,      true, bookEndIntInput.getValue(),   &bookEndIntInput));
	bookmarkMenu.addItem(new CMenuForwarder(LOCALE_MOVIEBROWSER_BOOK_LASTMOVIESTOP, true, bookLastIntInput.getValue(),  &bookLastIntInput));
	bookmarkMenu.addItem(GenericMenuSeparatorLine);

	for (int li =0 ; li < MI_MOVIE_BOOK_USER_MAX && li < MAX_NUMBER_OF_BOOKMARK_ITEMS; li++)
	{
		if (movie_info->bookmarks.user[li].name.empty())
			movie_info->bookmarks.user[li].name = g_Locale->getText(LOCALE_MOVIEBROWSER_BOOK_NEW);

		CKeyboardInput *pBookNameInput = new CKeyboardInput(LOCALE_MOVIEBROWSER_EDIT_BOOK,   &movie_info->bookmarks.user[li].name,   20, NULL, NULL, LOCALE_MOVIEBROWSER_EDIT_BOOK_NAME_INFO1, LOCALE_MOVIEBROWSER_EDIT_BOOK_NAME_INFO2);
		CIntInput *pBookPosIntInput    = new CIntInput(LOCALE_MOVIEBROWSER_EDIT_BOOK, (int *)&movie_info->bookmarks.user[li].pos,     5, LOCALE_MOVIEBROWSER_EDIT_BOOK_POS_INFO1,  LOCALE_MOVIEBROWSER_EDIT_BOOK_POS_INFO2);
		CIntInput *pBookTypeIntInput   = new CIntInput(LOCALE_MOVIEBROWSER_EDIT_BOOK, (int *)&movie_info->bookmarks.user[li].length, 20, LOCALE_MOVIEBROWSER_EDIT_BOOK_TYPE_INFO1, LOCALE_MOVIEBROWSER_EDIT_BOOK_TYPE_INFO2);

		CMenuWidget* pBookItemMenu = new CMenuWidget(LOCALE_MOVIEBROWSER_BOOK_HEAD, NEUTRINO_ICON_MOVIEPLAYER);
		pBookItemMenu->addItem(GenericMenuSeparator);
		pBookItemMenu->addItem(new CMenuDForwarder(LOCALE_MOVIEBROWSER_BOOK_NAME,     true,  movie_info->bookmarks.user[li].name,    pBookNameInput));
		pBookItemMenu->addItem(new CMenuDForwarder(LOCALE_MOVIEBROWSER_BOOK_POSITION, true,  pBookPosIntInput->getValue(),  pBookPosIntInput));
		pBookItemMenu->addItem(new CMenuDForwarder(LOCALE_MOVIEBROWSER_BOOK_TYPE,     true,  pBookTypeIntInput->getValue(), pBookTypeIntInput));

		bookmarkMenu.addItem(new CMenuDForwarder("", true, pBookNameInput->getValue(), pBookItemMenu));
	}

	/********************************************************************/
	/**  serie******************************************************/
	CKeyboardInput serieUserInput(LOCALE_MOVIEBROWSER_EDIT_SERIE, &movie_info->serieName, 20);

	CMenuWidget serieMenu(LOCALE_MOVIEBROWSER_HEAD, NEUTRINO_ICON_MOVIEPLAYER);
	serieMenu.addIntroItems(LOCALE_MOVIEBROWSER_SERIE_HEAD);
	serieMenu.addItem(new CMenuForwarder(LOCALE_MOVIEBROWSER_SERIE_NAME, true, movie_info->serieName,&serieUserInput));
	serieMenu.addItem(GenericMenuSeparatorLine);
	for (unsigned int li = 0; li < m_vHandleSerienames.size(); li++)
		serieMenu.addItem(new CMenuSelector(m_vHandleSerienames[li]->serieName.c_str(), true, movie_info->serieName));

	/********************************************************************/
	/**  update movie info  ******************************************************/
	for (unsigned int i = 0; i < MB_INFO_MAX_NUMBER; i++)
		movieInfoUpdateAll[i] = 0;
	movieInfoUpdateAllIfDestEmptyOnly = true;

	CMenuWidget movieInfoMenuUpdate(LOCALE_MOVIEBROWSER_HEAD, NEUTRINO_ICON_MOVIEPLAYER);
	movieInfoMenuUpdate.addIntroItems(LOCALE_MOVIEBROWSER_INFO_HEAD_UPDATE);
	movieInfoMenuUpdate.addItem(new CMenuForwarder(LOCALE_MOVIEBROWSER_MENU_SAVE_ALL, true, NULL, this, "save_movie_info_all", CRCInput::RC_red));
	movieInfoMenuUpdate.addItem(GenericMenuSeparatorLine);
	movieInfoMenuUpdate.addItem(new CMenuOptionChooser(LOCALE_MOVIEBROWSER_UPDATE_IF_DEST_EMPTY_ONLY, (&movieInfoUpdateAllIfDestEmptyOnly), MESSAGEBOX_YES_NO_OPTIONS, MESSAGEBOX_YES_NO_OPTIONS_COUNT, true, NULL, CRCInput::RC_blue));
	movieInfoMenuUpdate.addItem(GenericMenuSeparatorLine);
	movieInfoMenuUpdate.addItem(new CMenuOptionChooser(LOCALE_MOVIEBROWSER_INFO_TITLE,             &movieInfoUpdateAll[MB_INFO_TITLE], MESSAGEBOX_YES_NO_OPTIONS, MESSAGEBOX_YES_NO_OPTIONS_COUNT, true, NULL, CRCInput::RC_1));
	movieInfoMenuUpdate.addItem(new CMenuOptionChooser(LOCALE_MOVIEBROWSER_INFO_SERIE,             &movieInfoUpdateAll[MB_INFO_SERIE], MESSAGEBOX_YES_NO_OPTIONS, MESSAGEBOX_YES_NO_OPTIONS_COUNT, true, NULL, CRCInput::RC_2));
	movieInfoMenuUpdate.addItem(new CMenuOptionChooser(LOCALE_MOVIEBROWSER_INFO_INFO1,             &movieInfoUpdateAll[MB_INFO_INFO1], MESSAGEBOX_YES_NO_OPTIONS, MESSAGEBOX_YES_NO_OPTIONS_COUNT, true, NULL, CRCInput::RC_3));
	movieInfoMenuUpdate.addItem(new CMenuOptionChooser(LOCALE_MOVIEBROWSER_INFO_GENRE_MAJOR,       &movieInfoUpdateAll[MB_INFO_MAJOR_GENRE], MESSAGEBOX_YES_NO_OPTIONS, MESSAGEBOX_YES_NO_OPTIONS_COUNT, true, NULL, CRCInput::RC_4));
	movieInfoMenuUpdate.addItem(new CMenuOptionChooser(LOCALE_MOVIEBROWSER_INFO_PARENTAL_LOCKAGE,  &movieInfoUpdateAll[MB_INFO_PARENTAL_LOCKAGE], MESSAGEBOX_YES_NO_OPTIONS, MESSAGEBOX_YES_NO_OPTIONS_COUNT, true, NULL, CRCInput::RC_5));
	movieInfoMenuUpdate.addItem(new CMenuOptionChooser(LOCALE_MOVIEBROWSER_INFO_PRODYEAR,          &movieInfoUpdateAll[MB_INFO_PRODDATE], MESSAGEBOX_YES_NO_OPTIONS, MESSAGEBOX_YES_NO_OPTIONS_COUNT, true, NULL, CRCInput::RC_6));
	movieInfoMenuUpdate.addItem(new CMenuOptionChooser(LOCALE_MOVIEBROWSER_INFO_PRODCOUNTRY,       &movieInfoUpdateAll[MB_INFO_COUNTRY], MESSAGEBOX_YES_NO_OPTIONS, MESSAGEBOX_YES_NO_OPTIONS_COUNT, true, NULL, CRCInput::RC_7));
	movieInfoMenuUpdate.addItem(new CMenuOptionChooser(LOCALE_MOVIEBROWSER_INFO_LENGTH,            &movieInfoUpdateAll[MB_INFO_LENGTH], MESSAGEBOX_YES_NO_OPTIONS, MESSAGEBOX_YES_NO_OPTIONS_COUNT, true, NULL, CRCInput::RC_8));
	movieInfoMenuUpdate.addItem(new CMenuOptionChooser(LOCALE_MOVIEBROWSER_INFO_RATING,            &movieInfoUpdateAll[MB_INFO_RATING], MESSAGEBOX_YES_NO_OPTIONS, MESSAGEBOX_YES_NO_OPTIONS_COUNT, true, NULL, CRCInput::RC_9));
	movieInfoMenuUpdate.addItem(new CMenuOptionChooser(LOCALE_MOVIEBROWSER_INFO_QUALITY,           &movieInfoUpdateAll[MB_INFO_QUALITY], MESSAGEBOX_YES_NO_OPTIONS, MESSAGEBOX_YES_NO_OPTIONS_COUNT, true, NULL, CRCInput::RC_0));

	/********************************************************************/
	/**  movieInfo  ******************************************************/
#define BUFFER_SIZE 100
	char dirItNr[BUFFER_SIZE];
	char size[BUFFER_SIZE];

	strncpy(dirItNr, m_dirNames[movie_info->dirItNr].c_str(),BUFFER_SIZE-1);
	snprintf(size,BUFFER_SIZE,"%5" PRIu64 "",movie_info->file.Size>>20);

	CKeyboardInput titelUserInput(LOCALE_MOVIEBROWSER_INFO_TITLE,          &movie_info->epgTitle, (movie_info->epgTitle.empty() || (movie_info->epgTitle.size() < MAX_STRING)) ? MAX_STRING:movie_info->epgTitle.size());
	CKeyboardInput channelUserInput(LOCALE_MOVIEBROWSER_INFO_CHANNEL,      &movie_info->channelName, MAX_STRING);
	CKeyboardInput epgUserInput(LOCALE_MOVIEBROWSER_INFO_INFO1,            &movie_info->epgInfo1, 20);
	CKeyboardInput countryUserInput(LOCALE_MOVIEBROWSER_INFO_PRODCOUNTRY,  &movie_info->productionCountry, 11);

	CDateInput     dateUserDateInput(LOCALE_MOVIEBROWSER_INFO_LENGTH,      &movie_info->dateOfLastPlay);
	CDateInput     recUserDateInput(LOCALE_MOVIEBROWSER_INFO_LENGTH,       &movie_info->file.Time);
	CIntInput      lengthUserIntInput(LOCALE_MOVIEBROWSER_INFO_LENGTH,     (int *)&movie_info->length, 3);
	CIntInput      yearUserIntInput(LOCALE_MOVIEBROWSER_INFO_PRODYEAR,     (int *)&movie_info->productionDate, 4);

	CMenuOptionNumberChooser *rate = new CMenuOptionNumberChooser(LOCALE_MOVIEBROWSER_INFO_RATING, &movie_info->rating, true, 0, 100, NULL);
	rate->setNumberFormat(rateFormat);

	CMenuWidget movieInfoMenu(LOCALE_MOVIEBROWSER_HEAD, NEUTRINO_ICON_MOVIEPLAYER);

	movieInfoMenu.addIntroItems(LOCALE_MOVIEBROWSER_INFO_HEAD);
	movieInfoMenu.addItem(new CMenuForwarder(LOCALE_MOVIEBROWSER_MENU_SAVE,           true, NULL, this,                    "save_movie_info",                  CRCInput::RC_red));
	movieInfoMenu.addItem(new CMenuForwarder(LOCALE_MOVIEBROWSER_INFO_HEAD_UPDATE,    true, NULL,                          &movieInfoMenuUpdate, NULL,         CRCInput::RC_green));
	movieInfoMenu.addItem(new CMenuForwarder(LOCALE_MOVIEBROWSER_BOOK_HEAD,           true, NULL,                          &bookmarkMenu, NULL,                CRCInput::RC_blue));
	movieInfoMenu.addItem(GenericMenuSeparatorLine);
	movieInfoMenu.addItem(new CMenuForwarder(LOCALE_MOVIEBROWSER_INFO_TITLE,          true, movie_info->epgTitle,          &titelUserInput, NULL,              CRCInput::RC_1));
	movieInfoMenu.addItem(new CMenuForwarder(LOCALE_MOVIEBROWSER_INFO_SERIE,          true, movie_info->serieName,         &serieMenu, NULL,                   CRCInput::RC_2));
	movieInfoMenu.addItem(new CMenuForwarder(LOCALE_MOVIEBROWSER_INFO_INFO1,          (movie_info->epgInfo1.size() <= MAX_STRING), movie_info->epgInfo1, &epgUserInput, NULL, CRCInput::RC_3));
	movieInfoMenu.addItem(new CMenuOptionChooser(LOCALE_MOVIEBROWSER_INFO_GENRE_MAJOR,      &movie_info->genreMajor, GENRE_ALL, GENRE_ALL_COUNT, true, NULL,   CRCInput::RC_4, "", true));
	movieInfoMenu.addItem(GenericMenuSeparatorLine);
	movieInfoMenu.addItem(new CMenuOptionChooser(LOCALE_MOVIEBROWSER_INFO_PARENTAL_LOCKAGE, &movie_info->parentalLockAge, MESSAGEBOX_PARENTAL_LOCKAGE_OPTIONS, MESSAGEBOX_PARENTAL_LOCKAGE_OPTION_COUNT, true, NULL, CRCInput::RC_5));
	movieInfoMenu.addItem(new CMenuForwarder(LOCALE_MOVIEBROWSER_INFO_PRODYEAR,       true, yearUserIntInput.getValue(),   &yearUserIntInput, NULL,            CRCInput::RC_6));
	movieInfoMenu.addItem(new CMenuForwarder(LOCALE_MOVIEBROWSER_INFO_PRODCOUNTRY,    true, movie_info->productionCountry, &countryUserInput, NULL,            CRCInput::RC_7));
	movieInfoMenu.addItem(new CMenuForwarder(LOCALE_MOVIEBROWSER_INFO_LENGTH,         true, lengthUserIntInput.getValue(), &lengthUserIntInput, NULL,          CRCInput::RC_8));
	movieInfoMenu.addItem(new CMenuForwarder(LOCALE_MOVIEBROWSER_INFO_CHANNEL,        true, movie_info->channelName,       &channelUserInput, NULL,            CRCInput::RC_9));
	movieInfoMenu.addItem(GenericMenuSeparatorLine);
	movieInfoMenu.addItem(new CMenuForwarder(LOCALE_MOVIEBROWSER_INFO_PATH,           false, dirItNr));
	movieInfoMenu.addItem(new CMenuForwarder(LOCALE_MOVIEBROWSER_INFO_PREVPLAYDATE,   false, dateUserDateInput.getValue()));
	movieInfoMenu.addItem(new CMenuForwarder(LOCALE_MOVIEBROWSER_INFO_RECORDDATE,     false, recUserDateInput.getValue()));
	movieInfoMenu.addItem(new CMenuForwarder(LOCALE_MOVIEBROWSER_INFO_SIZE,           false, size));
	movieInfoMenu.addItem(GenericMenuSeparatorLine);
	movieInfoMenu.addItem(rate);
	movieInfoMenu.addItem(new CMenuOptionNumberChooser(LOCALE_MOVIEBROWSER_INFO_QUALITY, &movie_info->quality, true, 0, 3, NULL));

	int res = movieInfoMenu.exec(NULL,"");

	return res;
}

int CMovieBrowser::showMovieCutMenu()
{
	CMenuWidget movieCutMenu(LOCALE_MOVIEBROWSER_HEAD, NEUTRINO_ICON_MOVIEPLAYER);
	movieCutMenu.addIntroItems(LOCALE_MOVIEBROWSER_MENU_CUT_HEAD);
	CMenuForwarder *mf;

#if 0
	mf = new CMenuForwarder(m_movieSelectionHandler->epgTitle, false);
	mf->setHint(NEUTRINO_ICON_HINT_MOVIE, NONEXISTANT_LOCALE);
	movieCutMenu.addItem(mf);
	movieCutMenu.addItem(GenericMenuSeparator);
#endif

	mf = new CMenuForwarder(LOCALE_MOVIEBROWSER_MENU_COPY_ONEFILE, true, NULL, this, "copy_onefile", CRCInput::RC_red);
	mf->setHint(NEUTRINO_ICON_HINT_MOVIE, LOCALE_MOVIEBROWSER_HINT_COPY_ONEFILE);
	movieCutMenu.addItem(mf);

	mf = new CMenuForwarder(LOCALE_MOVIEBROWSER_MENU_COPY_SEVERAL, true, NULL, this, "copy_several", CRCInput::RC_green);
	mf->setHint(NEUTRINO_ICON_HINT_MOVIE, LOCALE_MOVIEBROWSER_HINT_COPY_SEVERAL);
	movieCutMenu.addItem(mf);

	mf = new CMenuForwarder(LOCALE_MOVIEBROWSER_MENU_CUT, true, NULL, this, "cut", CRCInput::RC_yellow);
	mf->setHint(NEUTRINO_ICON_HINT_MOVIE, LOCALE_MOVIEBROWSER_HINT_CUT);
	movieCutMenu.addItem(mf);

	mf = new CMenuForwarder(LOCALE_MOVIEBROWSER_MENU_TRUNCATE, true, NULL, this, "truncate", CRCInput::RC_blue);
	mf->setHint(NEUTRINO_ICON_HINT_MOVIE, LOCALE_MOVIEBROWSER_HINT_TRUNCATE);
	movieCutMenu.addItem(mf);

	int res = movieCutMenu.exec(NULL,"");
	return res;
}

bool CMovieBrowser::showMenu(bool calledExternally)
{
	/* first clear screen */
	framebuffer->paintBackground();

	int i;
	/********************************************************************/
	/**  directory menu ******************************************************/
	CDirMenu dirMenu(&m_dir);

	/********************************************************************/
	/**  options menu **************************************************/

	/********************************************************************/
	/**  parental lock **************************************************/
	CMenuWidget parentalMenu(LOCALE_MOVIEBROWSER_HEAD, NEUTRINO_ICON_MOVIEPLAYER);
	parentalMenu.addIntroItems(LOCALE_MOVIEBROWSER_MENU_PARENTAL_LOCK_HEAD);
	parentalMenu.addItem(new CMenuOptionChooser(LOCALE_MOVIEBROWSER_MENU_PARENTAL_LOCK_ACTIVATED, (int*)(&m_parentalLock), MESSAGEBOX_PARENTAL_LOCK_OPTIONS, MESSAGEBOX_PARENTAL_LOCK_OPTIONS_COUNT, true));
	parentalMenu.addItem(new CMenuOptionChooser(LOCALE_MOVIEBROWSER_MENU_PARENTAL_LOCK_RATE_HEAD, (int*)(&m_settings.parentalLockAge), MESSAGEBOX_PARENTAL_LOCKAGE_OPTIONS, MESSAGEBOX_PARENTAL_LOCKAGE_OPTION_COUNT, true));


	/********************************************************************/
	/**  optionsVerzeichnisse  **************************************************/
	CMenuWidget optionsMenuDir(LOCALE_MOVIEBROWSER_HEAD, NEUTRINO_ICON_MOVIEPLAYER);
	optionsMenuDir.addIntroItems(LOCALE_MOVIEBROWSER_MENU_DIRECTORIES_HEAD);
	optionsMenuDir.addItem(new CMenuOptionChooser(LOCALE_MOVIEBROWSER_USE_REC_DIR,       (int*)(&m_settings.storageDirRecUsed), MESSAGEBOX_YES_NO_OPTIONS, MESSAGEBOX_YES_NO_OPTIONS_COUNT, true));
	optionsMenuDir.addItem(new CMenuForwarder(LOCALE_MOVIEBROWSER_DIR, false, g_settings.network_nfs_recordingdir));

	optionsMenuDir.addItem(new CMenuOptionChooser(LOCALE_MOVIEBROWSER_USE_MOVIE_DIR,     (int*)(&m_settings.storageDirMovieUsed), MESSAGEBOX_YES_NO_OPTIONS, MESSAGEBOX_YES_NO_OPTIONS_COUNT, true));
	optionsMenuDir.addItem(new CMenuForwarder(LOCALE_MOVIEBROWSER_DIR, false, g_settings.network_nfs_moviedir));
	optionsMenuDir.addItem(new CMenuSeparator(CMenuSeparator::LINE | CMenuSeparator::STRING, LOCALE_MOVIEBROWSER_DIRECTORIES_ADDITIONAL));

	COnOffNotifier* notifier[MB_MAX_DIRS];
	for (i = 0; i < MB_MAX_DIRS ; i++)
	{
		CFileChooser *dirInput =  new CFileChooser(&m_settings.storageDir[i]);
		CMenuForwarder *forwarder = new CMenuDForwarder(LOCALE_MOVIEBROWSER_DIR,        m_settings.storageDirUsed[i], m_settings.storageDir[i],      dirInput);
		notifier[i] =  new COnOffNotifier();
		notifier[i]->addItem(forwarder);
		CMenuOptionChooser *chooser =   new CMenuOptionChooser(LOCALE_MOVIEBROWSER_USE_DIR, &m_settings.storageDirUsed[i], MESSAGEBOX_YES_NO_OPTIONS, MESSAGEBOX_YES_NO_OPTIONS_COUNT, true,notifier[i]);
		optionsMenuDir.addItem(chooser);
		optionsMenuDir.addItem(forwarder);
		if (i != (MB_MAX_DIRS - 1))
			optionsMenuDir.addItem(GenericMenuSeparator);
	}

	/********************************************************************/
	/**  optionsMenuBrowser  **************************************************/
	int oldRowNr = m_settings.browserRowNr;
	int oldFrameHeight = m_settings.browserFrameHeight;
	int oldAdditional = m_settings.browserAdditional;
	CIntInput playMaxUserIntInput(LOCALE_MOVIEBROWSER_LAST_PLAY_MAX_ITEMS,      (int *)&m_settings.lastPlayMaxItems,    3, NONEXISTANT_LOCALE, NONEXISTANT_LOCALE);
	CIntInput recMaxUserIntInput(LOCALE_MOVIEBROWSER_LAST_RECORD_MAX_ITEMS,     (int *)&m_settings.lastRecordMaxItems,  3, NONEXISTANT_LOCALE, NONEXISTANT_LOCALE);
	CIntInput browserFrameUserIntInput(LOCALE_MOVIEBROWSER_BROWSER_FRAME_HIGH,  (int *)&m_settings.browserFrameHeight,  3, NONEXISTANT_LOCALE, NONEXISTANT_LOCALE);
	CIntInput browserRowNrIntInput(LOCALE_MOVIEBROWSER_BROWSER_ROW_NR,          (int *)&m_settings.browserRowNr,        1, NONEXISTANT_LOCALE, NONEXISTANT_LOCALE);

	CMenuWidget optionsMenuBrowser(LOCALE_MOVIEBROWSER_HEAD, NEUTRINO_ICON_MOVIEPLAYER);
	optionsMenuBrowser.addIntroItems(LOCALE_MOVIEBROWSER_OPTION_BROWSER);
	optionsMenuBrowser.addItem(new CMenuForwarder(LOCALE_MOVIEBROWSER_LAST_PLAY_MAX_ITEMS,    true, playMaxUserIntInput.getValue(),   &playMaxUserIntInput));
	optionsMenuBrowser.addItem(new CMenuForwarder(LOCALE_MOVIEBROWSER_LAST_RECORD_MAX_ITEMS,  true, recMaxUserIntInput.getValue(), &recMaxUserIntInput));
	optionsMenuBrowser.addItem(new CMenuForwarder(LOCALE_MOVIEBROWSER_BROWSER_FRAME_HIGH,     true, browserFrameUserIntInput.getValue(), &browserFrameUserIntInput));
	optionsMenuBrowser.addItem(new CMenuOptionChooser(LOCALE_MOVIEBROWSER_BROWSER_ADDITIONAL, (int*)(&m_settings.browserAdditional), MESSAGEBOX_YES_NO_OPTIONS, MESSAGEBOX_YES_NO_OPTIONS_COUNT, true));
	optionsMenuBrowser.addItem(new CMenuSeparator(CMenuSeparator::LINE | CMenuSeparator::STRING, LOCALE_MOVIEBROWSER_BROWSER_ROW_HEAD));
	optionsMenuBrowser.addItem(new CMenuForwarder(LOCALE_MOVIEBROWSER_BROWSER_ROW_NR,     true, browserRowNrIntInput.getValue(), &browserRowNrIntInput));
	optionsMenuBrowser.addItem(GenericMenuSeparator);
	for (i = 0; i < MB_MAX_ROWS; i++)
	{
		CIntInput* browserRowWidthIntInput = new CIntInput(LOCALE_MOVIEBROWSER_BROWSER_ROW_WIDTH,(int *)&m_settings.browserRowWidth[i], 3, NONEXISTANT_LOCALE, NONEXISTANT_LOCALE);
		optionsMenuBrowser.addItem(new CMenuOptionChooser(LOCALE_MOVIEBROWSER_BROWSER_ROW_ITEM, (int*)(&m_settings.browserRowItem[i]), MESSAGEBOX_BROWSER_ROW_ITEM, MESSAGEBOX_BROWSER_ROW_ITEM_COUNT, true, NULL, CRCInput::convertDigitToKey(i+1), NULL, true, true));
		optionsMenuBrowser.addItem(new CMenuDForwarder(LOCALE_MOVIEBROWSER_BROWSER_ROW_WIDTH,    true, browserRowWidthIntInput->getValue(),      browserRowWidthIntInput));
		if (i < MB_MAX_ROWS-1)
			optionsMenuBrowser.addItem(GenericMenuSeparator);
	}

	/********************************************************************/
	/**  options  **************************************************/

	CMenuWidget optionsMenu(LOCALE_MOVIEBROWSER_HEAD, NEUTRINO_ICON_MOVIEPLAYER);

	optionsMenu.addIntroItems(LOCALE_EPGPLUS_OPTIONS);
	optionsMenu.addItem(new CMenuForwarder(LOCALE_MOVIEBROWSER_LOAD_DEFAULT, true, NULL, this, "loaddefault",              CRCInput::RC_red));
	optionsMenu.addItem(new CMenuForwarder(LOCALE_MOVIEBROWSER_OPTION_BROWSER, true, NULL, &optionsMenuBrowser,NULL,       CRCInput::RC_green));
	optionsMenu.addItem(new CMenuForwarder(LOCALE_MOVIEBROWSER_DIRECTORIES, true, NULL, &optionsMenuDir,NULL,    CRCInput::RC_yellow));
	if (m_parentalLock != MB_PARENTAL_LOCK_OFF)
		optionsMenu.addItem(new CLockedMenuForwarder(LOCALE_MOVIEBROWSER_MENU_PARENTAL_LOCK_HEAD, g_settings.parentallock_pincode, true,  true, NULL, &parentalMenu,NULL,CRCInput::RC_blue));
	else
		optionsMenu.addItem(new CMenuForwarder(LOCALE_MOVIEBROWSER_MENU_PARENTAL_LOCK_HEAD,   true, NULL, &parentalMenu,NULL,CRCInput::RC_blue));
	optionsMenu.addItem(GenericMenuSeparatorLine);
	optionsMenu.addItem(new CMenuOptionChooser(LOCALE_MOVIEBROWSER_RELOAD_AT_START,   (int*)(&m_settings.reload), MESSAGEBOX_YES_NO_OPTIONS, MESSAGEBOX_YES_NO_OPTIONS_COUNT, true));
	optionsMenu.addItem(new CMenuOptionChooser(LOCALE_MOVIEBROWSER_REMOUNT_AT_START,  (int*)(&m_settings.remount), MESSAGEBOX_YES_NO_OPTIONS, MESSAGEBOX_YES_NO_OPTIONS_COUNT, true));
	optionsMenu.addItem(GenericMenuSeparatorLine);
	optionsMenu.addItem(new CMenuOptionChooser(LOCALE_MOVIEBROWSER_HIDE_SERIES,       (int*)(&m_settings.browser_serie_mode), MESSAGEBOX_YES_NO_OPTIONS, MESSAGEBOX_YES_NO_OPTIONS_COUNT, true));
	optionsMenu.addItem(new CMenuOptionChooser(LOCALE_MOVIEBROWSER_SERIE_AUTO_CREATE, (int*)(&m_settings.serie_auto_create), MESSAGEBOX_YES_NO_OPTIONS, MESSAGEBOX_YES_NO_OPTIONS_COUNT, true));
	int ts_only = m_settings.ts_only;
	optionsMenu.addItem( new CMenuOptionChooser(LOCALE_MOVIEBROWSER_TS_ONLY,           (int*)(&m_settings.ts_only), MESSAGEBOX_YES_NO_OPTIONS, MESSAGEBOX_YES_NO_OPTIONS_COUNT, true ));

	//optionsMenu.addItem(GenericMenuSeparator);

	/********************************************************************/
	/**  main menu ******************************************************/
	CNFSSmallMenu* nfs =    new CNFSSmallMenu();

	if (!calledExternally) {
		CMenuWidget mainMenu(LOCALE_MOVIEBROWSER_HEAD, NEUTRINO_ICON_MOVIEPLAYER);
		mainMenu.addIntroItems(LOCALE_MOVIEBROWSER_MENU_MAIN_HEAD);
		mainMenu.addItem(new CMenuForwarder(LOCALE_MOVIEBROWSER_INFO_HEAD,     (m_movieSelectionHandler != NULL), NULL, this, "show_movie_info_menu", CRCInput::RC_red));
		mainMenu.addItem(new CMenuForwarder(LOCALE_MOVIEBROWSER_MENU_CUT_HEAD, (m_movieSelectionHandler != NULL), NULL, this, "show_movie_cut_menu",  CRCInput::RC_green));
		mainMenu.addItem(new CMenuForwarder(LOCALE_FILEBROWSER_DELETE,         (m_movieSelectionHandler != NULL), NULL, this, "delete_movie",         CRCInput::RC_yellow));
		mainMenu.addItem(GenericMenuSeparatorLine);
		mainMenu.addItem(new CMenuForwarder(LOCALE_EPGPLUS_OPTIONS,                    true, NULL, &optionsMenu,NULL,                                  CRCInput::RC_1));
		mainMenu.addItem(new CMenuForwarder(LOCALE_MOVIEBROWSER_MENU_DIRECTORIES_HEAD, true, NULL, &dirMenu,    NULL,                                  CRCInput::RC_2));
		mainMenu.addItem(new CMenuForwarder(LOCALE_MOVIEBROWSER_SCAN_FOR_MOVIES,       true, NULL, this,        "reload_movie_info",                   CRCInput::RC_3));
		//mainMenu.addItem(new CMenuForwarder(LOCALE_MOVIEBROWSER_MENU_NFS_HEAD,       true, NULL, nfs,         NULL,                                  CRCInput::RC_setup));

		mainMenu.exec(NULL, " ");
	} else
		optionsMenu.exec(NULL, "");

	// post menu handling
	if (m_parentalLock != MB_PARENTAL_LOCK_OFF_TMP)
		m_settings.parentalLock = m_parentalLock;
	if (m_settings.browserFrameHeight < MIN_BROWSER_FRAME_HEIGHT)
		m_settings.browserFrameHeight = MIN_BROWSER_FRAME_HEIGHT;
	if (m_settings.browserFrameHeight > MAX_BROWSER_FRAME_HEIGHT)
		m_settings.browserFrameHeight = MAX_BROWSER_FRAME_HEIGHT;
	if (m_settings.browserRowNr > MB_MAX_ROWS)
		m_settings.browserRowNr = MB_MAX_ROWS;
	if (m_settings.browserRowNr < 1)
		m_settings.browserRowNr = 1;
	for (i = 0; i < m_settings.browserRowNr; i++)
	{
		if (m_settings.browserRowWidth[i] > 100)
			m_settings.browserRowWidth[i] = 100;
		if (m_settings.browserRowWidth[i] < 1)
			m_settings.browserRowWidth[i] = 1;
	}

	if (!calledExternally) {
		if (ts_only != m_settings.ts_only || dirMenu.isChanged())
			loadMovies(false);

		bool reInitFrames = (
				   m_settings.browserRowNr       != oldRowNr
				|| m_settings.browserFrameHeight != oldFrameHeight
				|| m_settings.browserAdditional  != oldAdditional
		);

		if (reInitFrames) {
#if 1
			if (oldAdditional != m_settings.browserAdditional)
			{
				/*
				   Bad 'hack' to force a smaller m_pcInfo1 box.
				   This can be reconfigured by user later.
				   It's just to align view to channellist's view.
				*/
				if (m_settings.browserAdditional)
					m_settings.browserFrameHeight = 75;
				else
					m_settings.browserFrameHeight = 65;
			}
#endif
			initFrames();
			hide();
			paint();
		} else {
			updateSerienames();
			refreshBrowserList();
			refreshLastPlayList();
			refreshLastRecordList();
			refreshFilterList();
			refreshTitle();
			refreshMovieInfo();
			refreshFoot();
			refreshLCD();
		}
		/* FIXME: refreshXXXList -> setLines -> CListFrame::refresh, too */
		//refresh();
	} else
		saveSettings(&m_settings);

	for (i = 0; i < MB_MAX_DIRS; i++)
		delete notifier[i];

	delete nfs;

	return(true);
}

int CMovieBrowser::showStartPosSelectionMenu(void) // P2
{
	const char *unit_short_minute = g_Locale->getText(LOCALE_UNIT_SHORT_MINUTE);
	//TRACE("[mb]->showStartPosSelectionMenu\n");
	int pos = -1;
	int result = 0;
	int menu_nr= 0;
	int position[MAX_NUMBER_OF_BOOKMARK_ITEMS] ={0};

	if (m_movieSelectionHandler == NULL) return(result);

	char start_pos[32]; snprintf(start_pos, sizeof(start_pos), "%3d %s",m_movieSelectionHandler->bookmarks.start/60, unit_short_minute);
	char play_pos[32]; 	snprintf(play_pos, sizeof(play_pos), "%3d %s",m_movieSelectionHandler->bookmarks.lastPlayStop/60, unit_short_minute);

	char book[MI_MOVIE_BOOK_USER_MAX][32];

	CMenuWidgetSelection startPosSelectionMenu(LOCALE_MOVIEBROWSER_HEAD, NEUTRINO_ICON_MOVIEPLAYER);
	startPosSelectionMenu.enableFade(false);

	startPosSelectionMenu.addIntroItems(LOCALE_MOVIEBROWSER_START_HEAD, NONEXISTANT_LOCALE, CMenuWidget::BTN_TYPE_CANCEL);

	int off = startPosSelectionMenu.getItemsCount();
	bool got_start_pos = false;

	if (m_movieSelectionHandler->bookmarks.start != 0)
	{
		got_start_pos = true;
		startPosSelectionMenu.addItem(new CMenuForwarder(LOCALE_MOVIEBROWSER_BOOK_MOVIESTART, true, start_pos), true);
		position[menu_nr++] = m_movieSelectionHandler->bookmarks.start;
	}
	if (m_movieSelectionHandler->bookmarks.lastPlayStop != 0)
	{
		startPosSelectionMenu.addItem(new CMenuForwarder(LOCALE_MOVIEBROWSER_BOOK_LASTMOVIESTOP, true, play_pos));
		position[menu_nr++] = m_movieSelectionHandler->bookmarks.lastPlayStop;
	}

	startPosSelectionMenu.addItem(new CMenuForwarder(LOCALE_MOVIEBROWSER_START_RECORD_START, true,NULL), got_start_pos ? false : true);
	position[menu_nr++] = 0;

	for (int i = 0; i < MI_MOVIE_BOOK_USER_MAX && menu_nr < MAX_NUMBER_OF_BOOKMARK_ITEMS; i++)
	{
		if (m_movieSelectionHandler->bookmarks.user[i].pos != 0)
		{
			if (m_movieSelectionHandler->bookmarks.user[i].length >= 0)
				position[menu_nr] = m_movieSelectionHandler->bookmarks.user[i].pos;
			else
				position[menu_nr] = m_movieSelectionHandler->bookmarks.user[i].pos + m_movieSelectionHandler->bookmarks.user[i].length;

			snprintf(book[i], sizeof(book[i]),"%5d %s",position[menu_nr]/60, unit_short_minute);
			startPosSelectionMenu.addItem(new CMenuForwarder(m_movieSelectionHandler->bookmarks.user[i].name.c_str(), 	true, book[i]));
			menu_nr++;
		}
	}

	startPosSelectionMenu.exec(NULL, "12345");
	/* check what menu item was ok'd  and set the appropriate play offset*/
	result = startPosSelectionMenu.getSelectedLine();
	result -= off; // sub-text, separator, back, separator-line

	if (result >= 0 && result <= MAX_NUMBER_OF_BOOKMARK_ITEMS)
		pos = position[result];

	TRACE("[mb] selected bookmark %d position %d \n", result, pos);

	return(pos) ;
}

bool CMovieBrowser::isParentalLock(MI_MOVIE_INFO& movie_info)
{
	bool result = false;
	if (m_parentalLock == MB_PARENTAL_LOCK_ACTIVE && m_settings.parentalLockAge <= movie_info.parentalLockAge)
		result = true;

	return (result);
}

bool CMovieBrowser::isFiltered(MI_MOVIE_INFO& movie_info)
{
	bool result = true;

	switch(m_settings.filter.item)
	{
		case MB_INFO_FILEPATH:
			if (m_settings.filter.optionVar == movie_info.dirItNr)
				result = false;
			break;
		case MB_INFO_INFO1:
			if (strcmp(m_settings.filter.optionString.c_str(),movie_info.epgInfo1.c_str()) == 0)
				result = false;
			break;
		case MB_INFO_MAJOR_GENRE:
			if (m_settings.filter.optionVar == movie_info.genreMajor)
				result = false;
			break;
		case MB_INFO_SERIE:
			if (strcmp(m_settings.filter.optionString.c_str(),movie_info.serieName.c_str()) == 0)
				result = false;
			break;
			break;
		default:
				result = false;
			break;
	}
	return (result);
}

bool CMovieBrowser::getMovieInfoItem(MI_MOVIE_INFO& movie_info, MB_INFO_ITEM item, std::string* item_string)
{
	#define MAX_STR_TMP 100
	char str_tmp[MAX_STR_TMP];
	bool result = true;
	*item_string="";
	tm* tm_tmp;

	int i=0;
	int counter=0;

	std::string b;
	bool s, e, u;

	switch(item)
	{
		case MB_INFO_FILENAME: 				// 		= 0,
			*item_string = movie_info.file.getFileName();
			break;
		case MB_INFO_FILEPATH: 				// 		= 1,
			if (!m_dirNames.empty())
				*item_string = m_dirNames[movie_info.dirItNr];
			break;
		case MB_INFO_TITLE: 				// 		= 2,
			*item_string = movie_info.epgTitle;
			if (strcmp("not available",movie_info.epgTitle.c_str()) == 0)
				result = false;
			if (movie_info.epgTitle.empty())
				result = false;
			break;
		case MB_INFO_SERIE: 				// 		= 3,
			*item_string = movie_info.serieName;
			break;
		case MB_INFO_INFO1: 			//		= 4,
			*item_string = movie_info.epgInfo1;
			break;
		case MB_INFO_MAJOR_GENRE: 			// 		= 5,
			snprintf(str_tmp, sizeof(str_tmp),"%2d",movie_info.genreMajor);
			*item_string = str_tmp;
			break;
		case MB_INFO_MINOR_GENRE: 			// 		= 6,
			snprintf(str_tmp, sizeof(str_tmp),"%2d",movie_info.genreMinor);
			*item_string = str_tmp;
			break;
		case MB_INFO_INFO2: 					// 		= 7,
			*item_string = movie_info.epgInfo2;
			break;
		case MB_INFO_PARENTAL_LOCKAGE: 					// 		= 8,
			snprintf(str_tmp, sizeof(str_tmp),"%2d",movie_info.parentalLockAge);
			*item_string = str_tmp;
			break;
		case MB_INFO_CHANNEL: 				// 		= 9,
			*item_string = movie_info.channelName;
			break;
		case MB_INFO_BOOKMARK: 				//		= 10,
			b = "";

			s = false;
			if (movie_info.bookmarks.start != 0)
			{
				s = true;
				b += "S";
			}

			e = false;
			if (movie_info.bookmarks.end != 0)
			{
				e = true;
				if (s)
					b += ",";
				b += "E";
			}

			// we just return the number of bookmarks
			for (i = 0; i < MI_MOVIE_BOOK_USER_MAX; i++)
			{
				if (movie_info.bookmarks.user[i].pos != 0)
					counter++;
			}
			u = (counter > 0);
			if (u)
			{
				if (s || e)
					b += ",";
				b += "U[";
				b += to_string(counter);
				b += "]";
			}

			*item_string = b;
			break;
		case MB_INFO_QUALITY: 				// 		= 11,
			snprintf(str_tmp, sizeof(str_tmp),"%d",movie_info.quality);
			*item_string = str_tmp;
			break;
		case MB_INFO_PREVPLAYDATE: 			// 		= 12,
			tm_tmp = localtime(&movie_info.dateOfLastPlay);
			snprintf(str_tmp, sizeof(str_tmp),"%02d.%02d.%02d",tm_tmp->tm_mday,(tm_tmp->tm_mon)+ 1, tm_tmp->tm_year >= 100 ? tm_tmp->tm_year-100 : tm_tmp->tm_year);
			*item_string = str_tmp;
			break;

		case MB_INFO_RECORDDATE: 			// 		= 13,
			if (show_mode == MB_SHOW_YT) {
				*item_string = movie_info.ytdate;
			} else {
				tm_tmp = localtime(&movie_info.file.Time);
				snprintf(str_tmp, sizeof(str_tmp),"%02d.%02d.%02d",tm_tmp->tm_mday,(tm_tmp->tm_mon) + 1,tm_tmp->tm_year >= 100 ? tm_tmp->tm_year-100 : tm_tmp->tm_year);
				*item_string = str_tmp;
			}
			break;
		case MB_INFO_PRODDATE: 				// 		= 14,
			snprintf(str_tmp, sizeof(str_tmp),"%d",movie_info.productionDate);
			*item_string = str_tmp;
			break;
		case MB_INFO_COUNTRY: 				// 		= 15,
			*item_string = movie_info.productionCountry;
			break;
		case MB_INFO_GEOMETRIE: 			// 		= 16,
			result = false;
			break;
		case MB_INFO_AUDIO: 				// 		= 17,
			// we just return the number of audiopids
			snprintf(str_tmp, sizeof(str_tmp), "%d", (int)movie_info.audioPids.size());
			*item_string = str_tmp;
			break;
		case MB_INFO_LENGTH: 				// 		= 18,
			snprintf(str_tmp, sizeof(str_tmp),"%dh %02dm", movie_info.length/60, movie_info.length%60);
			*item_string = str_tmp;
			break;
		case MB_INFO_SIZE: 					// 		= 19,
			snprintf(str_tmp, sizeof(str_tmp),"%4" PRIu64 "",movie_info.file.Size>>20);
			*item_string = str_tmp;
			break;
		case MB_INFO_RATING: 				// 		= 20,
			if (movie_info.rating)
			{
				snprintf(str_tmp, sizeof(str_tmp),"%d,%d",movie_info.rating/10, movie_info.rating%10);
				*item_string = str_tmp;
			}
			break;
		case MB_INFO_SPACER: 				// 		= 21,
			*item_string="";
			break;
		case MB_INFO_MAX_NUMBER: 			//		= 22
		default:
			*item_string="";
			result = false;
			break;
	}
	//TRACE("   getMovieInfoItem: %d,>%s<",item,*item_string.c_str());
	return(result);
}

void CMovieBrowser::updateSerienames(void)
{
	if (m_seriename_stale == false)
		return;

	m_vHandleSerienames.clear();
	for (unsigned int i = 0; i < m_vMovieInfo.size(); i++)
	{
		if (!m_vMovieInfo[i].serieName.empty())
		{
			// current series name is not empty, lets see if we already have it in the list, and if not save it to the list.
			bool found = false;
			for (unsigned int t = 0; t < m_vHandleSerienames.size() && found == false; t++)
			{
				if (strcmp(m_vHandleSerienames[t]->serieName.c_str(),m_vMovieInfo[i].serieName.c_str()) == 0)
					found = true;
			}
			if (found == false)
				m_vHandleSerienames.push_back(&m_vMovieInfo[i]);
		}
	}
	TRACE("[mb]->updateSerienames: %d\n", (int)m_vHandleSerienames.size());
	// TODO sort(m_serienames.begin(), m_serienames.end(), my_alphasort);
	m_seriename_stale = false;
}

void CMovieBrowser::autoFindSerie(void)
{
	TRACE("autoFindSerie\n");
	updateSerienames(); // we have to make sure that the seriename array is up to date, otherwise this does not work
	// if the array is not stale, the function is left immediately
	for (unsigned int i = 0; i < m_vMovieInfo.size(); i++)
	{
		// For all movie infos, which do not have a seriename, we try to find one.
		// We search for a movieinfo with seriename, and than we do check if the title is the same
		// in case of same title, we assume both belongs to the same serie
		//TRACE("%s ",m_vMovieInfo[i].serieName);
		if (m_vMovieInfo[i].serieName.empty())
		{
			for (unsigned int t=0; t < m_vHandleSerienames.size();t++)
			{
				//TRACE("%s ",m_vHandleSerienames[i].serieName);
				if (m_vMovieInfo[i].epgTitle == m_vHandleSerienames[t]->epgTitle)
				{
					//TRACE("x");
					m_vMovieInfo[i].serieName = m_vHandleSerienames[t]->serieName;
					break; // we  found a maching serie, nothing to do else, leave for(t=0)
				}
			}
			//TRACE("\n");
		}
	}
}

void CMovieBrowser::loadYTitles(int mode, std::string search, std::string id)
{
	printf("CMovieBrowser::loadYTitles: parsed %d old mode %d new mode %d region %s\n", ytparser.Parsed(), ytparser.GetFeedMode(), m_settings.ytmode, m_settings.ytregion.c_str());
	if (m_settings.ytregion == "default")
		ytparser.SetRegion("");
	else
		ytparser.SetRegion(m_settings.ytregion);

	ytparser.SetMaxResults(m_settings.ytresults);
	ytparser.SetConcurrentDownloads(m_settings.ytconcconn);
	ytparser.SetThumbnailDir(m_settings.ytthumbnaildir);

	if (!ytparser.Parsed() || (ytparser.GetFeedMode() != mode)) {
		if (ytparser.ParseFeed((cYTFeedParser::yt_feed_mode_t)mode, search, id, (cYTFeedParser::yt_feed_orderby_t)m_settings.ytorderby)) {
			ytparser.DownloadThumbnails();
		} else {
			//FIXME show error
			DisplayErrorMessage(g_Locale->getText(LOCALE_MOVIEBROWSER_YT_ERROR));
			return;
		}
	}
	m_vMovieInfo.clear();
	yt_video_list_t &ylist = ytparser.GetVideoList();
	for (unsigned i = 0; i < ylist.size(); i++) {
		MI_MOVIE_INFO movieInfo;
		movieInfo.channelName = ylist[i].author;
		movieInfo.epgTitle = ylist[i].title;
		movieInfo.epgInfo1 = ylist[i].category;
		movieInfo.epgInfo2 = ylist[i].description;
		movieInfo.length = ylist[i].duration/60 ;
		movieInfo.tfile = ylist[i].tfile;
		movieInfo.ytdate = ylist[i].published;
		movieInfo.ytid = ylist[i].id;
		movieInfo.file.Name = ylist[i].title;
		movieInfo.ytitag = m_settings.ytquality;
		movieInfo.file.Url = ylist[i].GetUrl(&movieInfo.ytitag, false);
		movieInfo.file.Time = toEpoch(movieInfo.ytdate);
		m_vMovieInfo.push_back(movieInfo);
	}
	m_currentBrowserSelection = 0;
	m_currentRecordSelection = 0;
	m_currentPlaySelection = 0;
	m_pcBrowser->setSelectedLine(m_currentBrowserSelection);
	m_pcLastRecord->setSelectedLine(m_currentRecordSelection);
	m_pcLastPlay->setSelectedLine(m_currentPlaySelection);
}

const CMenuOptionChooser::keyval YT_FEED_OPTIONS[] =
{
	{ cYTFeedParser::MOST_POPULAR_ALL_TIME, LOCALE_MOVIEBROWSER_YT_MOST_POPULAR_ALL_TIME },
	{ cYTFeedParser::MOST_POPULAR, LOCALE_MOVIEBROWSER_YT_MOST_POPULAR }
};

#define YT_FEED_OPTION_COUNT (sizeof(YT_FEED_OPTIONS)/sizeof(CMenuOptionChooser::keyval))

const CMenuOptionChooser::keyval YT_ORDERBY_OPTIONS[] =
{
	{ cYTFeedParser::ORDERBY_PUBLISHED, LOCALE_MOVIEBROWSER_YT_ORDERBY_PUBLISHED },
	{ cYTFeedParser::ORDERBY_RELEVANCE, LOCALE_MOVIEBROWSER_YT_ORDERBY_RELEVANCE },
	{ cYTFeedParser::ORDERBY_VIEWCOUNT, LOCALE_MOVIEBROWSER_YT_ORDERBY_VIEWCOUNT },
	{ cYTFeedParser::ORDERBY_RATING, LOCALE_MOVIEBROWSER_YT_ORDERBY_RATING }
};

#define YT_ORDERBY_OPTION_COUNT (sizeof(YT_ORDERBY_OPTIONS)/sizeof(CMenuOptionChooser::keyval))

neutrino_locale_t CMovieBrowser::getFeedLocale(void)
{
	neutrino_locale_t ret = LOCALE_MOVIEBROWSER_YT_MOST_POPULAR;

	if (m_settings.ytmode == cYTFeedParser::RELATED)
		return LOCALE_MOVIEBROWSER_YT_RELATED;

	if (m_settings.ytmode == cYTFeedParser::SEARCH)
		return LOCALE_MOVIEBROWSER_YT_SEARCH;

	for (unsigned i = 0; i < YT_FEED_OPTION_COUNT; i++) {
		if (m_settings.ytmode == YT_FEED_OPTIONS[i].key)
			return YT_FEED_OPTIONS[i].value;
	}
	return ret;
}

int CYTCacheSelectorTarget::exec(CMenuTarget* /*parent*/, const std::string & actionKey)
{
	MI_MOVIE_INFO::miSource source = (movieBrowser->show_mode == MB_SHOW_YT) ? MI_MOVIE_INFO::YT : MI_MOVIE_INFO::NK;

	int selected = movieBrowser->yt_menue->getSelected();
	if (actionKey == "cancel_all") {
		cYTCache::getInstance()->cancelAll(source);
	} else if (actionKey == "completed_clear") {
		cYTCache::getInstance()->clearCompleted(source);
	} else if (actionKey == "failed_clear") {
		cYTCache::getInstance()->clearFailed(source);
	} else if (actionKey == "rc_spkr" && movieBrowser->yt_pending_offset && selected >= movieBrowser->yt_pending_offset && selected < movieBrowser->yt_pending_end) {
		cYTCache::getInstance()->cancel(&movieBrowser->yt_pending[selected - movieBrowser->yt_pending_offset]);
	} else if (actionKey == "rc_spkr" && movieBrowser->yt_completed_offset && selected >= movieBrowser->yt_completed_offset && selected < movieBrowser->yt_completed_end) {
		cYTCache::getInstance()->remove(&movieBrowser->yt_completed[selected - movieBrowser->yt_completed_offset]);
	} else if (actionKey.empty()) {
		if (movieBrowser->yt_pending_offset && selected >= movieBrowser->yt_pending_offset && selected < movieBrowser->yt_pending_end) {
			if (ShowMsg (LOCALE_MOVIEBROWSER_YT_CACHE, g_Locale->getText(LOCALE_MOVIEBROWSER_YT_CANCEL_TRANSFER), CMsgBox::mbrNo, CMsgBox::mbYes | CMsgBox::mbNo) == CMsgBox::mbrYes)
				cYTCache::getInstance()->cancel(&movieBrowser->yt_pending[selected - movieBrowser->yt_pending_offset]);
			else
				return menu_return::RETURN_NONE;
		} else if (movieBrowser->yt_completed_offset && selected >= movieBrowser->yt_completed_offset && selected < movieBrowser->yt_completed_end) {
			// FIXME -- anything sensible to do here?
			return menu_return::RETURN_NONE;
		} else if (movieBrowser->yt_failed_offset && selected >= movieBrowser->yt_failed_offset && selected < movieBrowser->yt_failed_end){
			cYTCache::getInstance()->clearFailed(&movieBrowser->yt_failed[selected - movieBrowser->yt_failed_offset]);
			cYTCache::getInstance()->addToCache(&movieBrowser->yt_failed[selected - movieBrowser->yt_failed_offset]);
			const char *format = g_Locale->getText(LOCALE_MOVIEBROWSER_YT_CACHE_ADD);
			char buf[1024];
			snprintf(buf, sizeof(buf), format, movieBrowser->yt_failed[selected - movieBrowser->yt_failed_offset].file.Name.c_str());
			CHintBox hintBox(LOCALE_MOVIEBROWSER_YT_CACHE, buf);
			hintBox.paint();
			sleep(1); //???
			hintBox.hide();
		}
	} else
		return menu_return::RETURN_NONE;

	movieBrowser->refreshYTMenu();
	return menu_return::RETURN_REPAINT;
}

void CMovieBrowser::refreshYTMenu()
{
	for (u_int item_id = (u_int) yt_menue->getItemsCount() - 1; item_id > yt_menue_end - 1; item_id--) {
		CMenuItem* m = yt_menue->getItem(item_id);
		if (m && !m->isStatic)
			delete m;
		yt_menue->removeItem(item_id);
	}
	MI_MOVIE_INFO::miSource source = (show_mode == MB_SHOW_YT) ? MI_MOVIE_INFO::YT : MI_MOVIE_INFO::NK;
	double dltotal, dlnow;
	time_t dlstart;
	yt_pending = cYTCache::getInstance()->getPending(source, &dltotal, &dlnow, &dlstart);
	yt_completed = cYTCache::getInstance()->getCompleted(source);
	yt_failed = cYTCache::getInstance()->getFailed(source);

	yt_pending_offset = 0;
	yt_completed_offset = 0;
	yt_failed_offset = 0;
	yt_pending_end = 0;
	yt_completed_end = 0;
	yt_failed_end = 0;

	if (!yt_pending.empty()) {
		yt_menue->addItem(new CMenuSeparator(CMenuSeparator::LINE | CMenuSeparator::STRING, LOCALE_MOVIEBROWSER_YT_PENDING));
		yt_menue->addItem(new CMenuForwarder(LOCALE_MOVIEBROWSER_YT_CANCEL, true, NULL, ytcache_selector, "cancel_all"));
		yt_menue->addItem(GenericMenuSeparator);
		std::string progress;
		if (dlstart && (int64_t)dltotal && (int64_t)dlnow) {
			time_t done = time(NULL) - dlstart;
			time_t left = ((dltotal - dlnow) * done)/dlnow;
			progress = "(" + to_string(done) + "s/" + to_string(left) + "s)";
		}
		int i = 0;
		yt_pending_offset = yt_menue->getItemsCount();
		for (std::vector<MI_MOVIE_INFO>::iterator it = yt_pending.begin(); it != yt_pending.end(); ++it, ++i) {
			yt_menue->addItem(new CMenuForwarder((*it).file.Name, true, progress.c_str(), ytcache_selector));
			progress = "";
		}
		yt_pending_end = yt_menue->getItemsCount();
	}

	if (!yt_completed.empty()) {
		yt_menue->addItem(new CMenuSeparator(CMenuSeparator::LINE | CMenuSeparator::STRING, LOCALE_MOVIEBROWSER_YT_COMPLETED));
		yt_menue->addItem(new CMenuForwarder(LOCALE_MOVIEBROWSER_YT_CLEAR, true, NULL, ytcache_selector, "completed_clear"));
		yt_menue->addItem(GenericMenuSeparator);
		int i = 0;
		yt_completed_offset = yt_menue->getItemsCount();
		for (std::vector<MI_MOVIE_INFO>::iterator it = yt_completed.begin(); it != yt_completed.end(); ++it, ++i) {
			yt_menue->addItem(new CMenuForwarder((*it).file.Name.c_str(), true, NULL, ytcache_selector));
		}
		yt_completed_end = yt_menue->getItemsCount();
	}

	if (!yt_failed.empty()) {
		yt_menue->addItem(new CMenuSeparator(CMenuSeparator::LINE | CMenuSeparator::STRING, LOCALE_MOVIEBROWSER_YT_FAILED));
		yt_menue->addItem(new CMenuForwarder(LOCALE_MOVIEBROWSER_YT_CLEAR, true, NULL, ytcache_selector, "failed_clear"));
		yt_menue->addItem(GenericMenuSeparator);
		int i = 0;
		yt_failed_offset = yt_menue->getItemsCount();
		for (std::vector<MI_MOVIE_INFO>::iterator it = yt_failed.begin(); it != yt_failed.end(); ++it, ++i) {
			yt_menue->addItem(new CMenuForwarder((*it).file.Name.c_str(), true, NULL, ytcache_selector));
		}
		yt_failed_end = yt_menue->getItemsCount();
	}

	CFrameBuffer::getInstance()->Clear(); // due to possible width change
}

class CYTHistory : public CMenuTarget
{
	private:
		int width;
		int selected;
		std::string *search;
		MB_SETTINGS *settings;
	public:
		CYTHistory(MB_SETTINGS &_settings, std::string &_search);
		int exec(CMenuTarget* parent, const std::string & actionKey);
};

CYTHistory::CYTHistory(MB_SETTINGS &_settings, std::string &_search)
{
	width = 40;
	selected = -1;
	settings = &_settings;
	search = &_search;
}

int CYTHistory::exec(CMenuTarget* parent, const std::string &actionKey)
{
	if (actionKey.empty()) {
		if (parent)
			parent->hide();
		CMenuWidget* m = new CMenuWidget(LOCALE_MOVIEBROWSER_YT_HISTORY, NEUTRINO_ICON_MOVIEPLAYER, width);
		m->addKey(CRCInput::RC_spkr, this, "clearYThistory");
		m->setSelected(selected);
		m->addItem(GenericMenuSeparator);
		m->addItem(GenericMenuBack);
		m->addItem(GenericMenuSeparatorLine);
		std::list<std::string>::iterator it = settings->ytsearch_history.begin();
		for (int i = 0; i < settings->ytsearch_history_size; i++, ++it)
			m->addItem(new CMenuForwarder((*it).c_str(), true, NULL, this, (*it).c_str(), CRCInput::convertDigitToKey(i + 1)));
		m->exec(NULL, "");
		m->hide();
		delete m;
		return menu_return::RETURN_REPAINT;
	}
	if (actionKey == "clearYThistory") {
		settings->ytsearch_history.clear();
		settings->ytsearch_history_size = 0;
		return menu_return::RETURN_EXIT;
	}
	*search = actionKey;
	g_RCInput->postMsg((neutrino_msg_t) CRCInput::RC_blue, 0);
	return menu_return::RETURN_EXIT;
}

bool CMovieBrowser::showYTMenu(bool calledExternally)
{
	framebuffer->paintBackground();

	CMenuWidget mainMenu(LOCALE_MOVIEPLAYER_YTPLAYBACK, NEUTRINO_ICON_MOVIEPLAYER);
	mainMenu.addIntroItems(LOCALE_MOVIEBROWSER_OPTION_BROWSER);

	int select = -1;
	CMenuSelectorTarget * selector = new CMenuSelectorTarget(&select);

	char cnt[5];
	if (!calledExternally) {
		for (unsigned i = 0; i < YT_FEED_OPTION_COUNT; i++) {
			sprintf(cnt, "%d", YT_FEED_OPTIONS[i].key);
			mainMenu.addItem(new CMenuForwarder(YT_FEED_OPTIONS[i].value, true, NULL, selector, cnt, CRCInput::convertDigitToKey(i + 1)), m_settings.ytmode == (int) YT_FEED_OPTIONS[i].key);
		}
		mainMenu.addItem(GenericMenuSeparatorLine);

		bool enabled = (!m_vMovieInfo.empty()) && (m_movieSelectionHandler != NULL);
		sprintf(cnt, "%d", cYTFeedParser::RELATED);
		mainMenu.addItem(new CMenuForwarder(LOCALE_MOVIEBROWSER_YT_RELATED, enabled, NULL, selector, cnt, CRCInput::RC_red));

		mainMenu.addItem(GenericMenuSeparatorLine);
	}

	std::string search = m_settings.ytsearch;
	CKeyboardInput stringInput(LOCALE_MOVIEBROWSER_YT_SEARCH, &search);
	if (!calledExternally) {
		mainMenu.addItem(new CMenuForwarder(LOCALE_MOVIEBROWSER_YT_SEARCH, true, search, &stringInput, NULL, CRCInput::RC_green));
		mainMenu.addItem(new CMenuOptionChooser(LOCALE_MOVIEBROWSER_YT_ORDERBY, &m_settings.ytorderby, YT_ORDERBY_OPTIONS, YT_ORDERBY_OPTION_COUNT, true, NULL, CRCInput::RC_nokey, "", true));
		sprintf(cnt, "%d", cYTFeedParser::SEARCH);
		mainMenu.addItem(new CMenuForwarder(LOCALE_EVENTFINDER_START_SEARCH, true, NULL, selector, cnt, CRCInput::RC_yellow));
	}

	CYTHistory ytHistory(m_settings, search);
	if (!calledExternally) {
		if (m_settings.ytsearch_history_size > 0)
			mainMenu.addItem(new CMenuForwarder(LOCALE_MOVIEBROWSER_YT_HISTORY, true, NULL, &ytHistory, "", CRCInput::RC_blue));

		mainMenu.addItem(GenericMenuSeparatorLine);
	}
	mainMenu.addItem(new CMenuOptionNumberChooser(LOCALE_MOVIEBROWSER_YT_MAX_RESULTS, &m_settings.ytresults, true, 10, 50, NULL));
	mainMenu.addItem(new CMenuOptionNumberChooser(LOCALE_MOVIEBROWSER_YT_MAX_HISTORY, &m_settings.ytsearch_history_max, true, 10, 50, NULL));

	std::string rstr = m_settings.ytregion;
	CMenuOptionStringChooser * region = new CMenuOptionStringChooser(LOCALE_MOVIEBROWSER_YT_REGION, &rstr, true, NULL, CRCInput::RC_nokey, "", true);
	region->addOption("default");
	region->addOption("DE");
	region->addOption("PL");
	region->addOption("RU");
	region->addOption("NL");
	region->addOption("CZ");
	region->addOption("FR");
	region->addOption("HU");
	region->addOption("US");
	mainMenu.addItem(region);

	#define YT_QUALITY_OPTION_COUNT 3
	CMenuOptionChooser::keyval_ext YT_QUALITY_OPTIONS[YT_QUALITY_OPTION_COUNT] =
	{
		{ 18, NONEXISTANT_LOCALE, "MP4 270p/360p"},
		{ 22, NONEXISTANT_LOCALE, "MP4 720p"	 },
#if 0
		{ 34, NONEXISTANT_LOCALE, "FLV 360p"	 },
		{ 35, NONEXISTANT_LOCALE, "FLV 480p"	 },
#endif
		{ 37, NONEXISTANT_LOCALE, "MP4 1080p"	 }
	};
	mainMenu.addItem(new CMenuOptionChooser(LOCALE_MOVIEBROWSER_YT_PREF_QUALITY, &m_settings.ytquality, YT_QUALITY_OPTIONS, YT_QUALITY_OPTION_COUNT, true, NULL, CRCInput::RC_nokey, "", true));
	mainMenu.addItem(new CMenuOptionNumberChooser(LOCALE_MOVIEBROWSER_YT_CONCURRENT_CONNECTIONS, &m_settings.ytconcconn, true, 1, 8));

	CFileChooser fc(&m_settings.ytthumbnaildir);
	mainMenu.addItem(new CMenuForwarder(LOCALE_MOVIEBROWSER_CACHE_DIR, true, m_settings.ytthumbnaildir, &fc));

	yt_menue = &mainMenu;
	yt_menue_end = yt_menue->getItemsCount();
	CYTCacheSelectorTarget ytcache_sel(this);
	ytcache_selector = &ytcache_sel;
	yt_menue->addKey(CRCInput::RC_spkr, ytcache_selector, "rc_spkr");
	refreshYTMenu();

	mainMenu.exec(NULL, "");

	ytparser.SetConcurrentDownloads(m_settings.ytconcconn);
	ytparser.SetThumbnailDir(m_settings.ytthumbnaildir);

	delete selector;

	bool reload = false;
	int newmode = -1;
	if (rstr != m_settings.ytregion) {
		m_settings.ytregion = rstr;
		if (newmode < 0)
			newmode = m_settings.ytmode;
		reload = true;
		printf("change region to %s\n", m_settings.ytregion.c_str());
	}
	if (calledExternally)
		return true;

	printf("MovieBrowser::showYTMenu(): selected: %d\n", select);
	if (select >= 0) {
		newmode = select;
		if (select == cYTFeedParser::RELATED) {
			if (m_settings.ytvid != m_movieSelectionHandler->ytid) {
				printf("get related for: %s\n", m_movieSelectionHandler->ytid.c_str());
				m_settings.ytvid = m_movieSelectionHandler->ytid;
				m_settings.ytmode = newmode;
				reload = true;
			}
		}
		else if (select == cYTFeedParser::SEARCH) {
			printf("search for: %s\n", search.c_str());
			if (!search.empty()) {
				reload = true;
				m_settings.ytsearch = search;
				m_settings.ytmode = newmode;
				m_settings.ytsearch_history.push_front(search);
				std::list<std::string>::iterator it = m_settings.ytsearch_history.begin();
				++it;
				while (it != m_settings.ytsearch_history.end()) {
					if (*it == search)
						it = m_settings.ytsearch_history.erase(it);
					else
						++it;
				}
				if (m_settings.ytsearch_history.empty())
					m_settings.ytsearch_history_size = 0;
				else
					m_settings.ytsearch_history_size = m_settings.ytsearch_history.size();

				if (m_settings.ytsearch_history_size > m_settings.ytsearch_history_max)
					m_settings.ytsearch_history_size = m_settings.ytsearch_history_max;
			}
		}
		else if (m_settings.ytmode != newmode) {
			m_settings.ytmode = newmode;
			reload = true;
		}
	}

	if (reload) {
		CHintBox loadBox(LOCALE_MOVIEPLAYER_YTPLAYBACK, g_Locale->getText(LOCALE_MOVIEBROWSER_SCAN_FOR_MOVIES));
		loadBox.paint();
		ytparser.Cleanup();
		loadYTitles(newmode, m_settings.ytsearch, m_settings.ytvid);
		loadBox.hide();
	}
	refreshBrowserList();
	refreshLastPlayList();
	refreshLastRecordList();
	refreshFilterList();
	refresh();
	return true;
}

CMenuSelector::CMenuSelector(const char * OptionName, const bool Active, char * OptionValue, int* ReturnInt,int ReturnIntValue) : CMenuItem()
{
	height     = g_Font[SNeutrinoSettings::FONT_TYPE_MENU]->getHeight();
	optionValueString = NULL;
	optionName = 		OptionName;
	optionValue = 		OptionValue;
	active = 			Active;
	returnIntValue =	ReturnIntValue;
	returnInt = 		ReturnInt;
}

CMenuSelector::CMenuSelector(const char * OptionName, const bool Active, std::string& OptionValue, int* ReturnInt,int ReturnIntValue) : CMenuItem()
{
	height     = g_Font[SNeutrinoSettings::FONT_TYPE_MENU]->getHeight();
	optionValueString = &OptionValue;
	optionName =        OptionName;
	strncpy(buffer,OptionValue.c_str(),BUFFER_MAX-1);
	buffer[BUFFER_MAX-1] = 0;// terminate string
	optionValue =       buffer;
	active =            Active;
	returnIntValue =    ReturnIntValue;
	returnInt =         ReturnInt;
}

int CMenuSelector::exec(CMenuTarget* /*parent*/)
{
	if (returnInt != NULL)
		*returnInt= returnIntValue;

	if (optionValue != NULL && optionName != NULL)
	{
		if (optionValueString == NULL)
			strcpy(optionValue,optionName);
		else
			*optionValueString = optionName;
	}
	return menu_return::RETURN_EXIT;
}

int CMenuSelector::paint(bool selected)
{
	CFrameBuffer * frameBuffer = CFrameBuffer::getInstance();

	fb_pixel_t    color   = COL_MENUCONTENT_TEXT;
	fb_pixel_t    bgcolor = COL_MENUCONTENT_PLUS_0;
	if (selected)
	{
		color   = COL_MENUCONTENTSELECTED_TEXT;
		bgcolor = COL_MENUCONTENTSELECTED_PLUS_0;
	}
	if (!active)
	{
		color   = COL_MENUCONTENTINACTIVE_TEXT;
		bgcolor = COL_MENUCONTENTINACTIVE_PLUS_0;
	}

	frameBuffer->paintBoxRel(x, y, dx, height, bgcolor);

	int stringstartposName = x + offx + 10;
	g_Font[SNeutrinoSettings::FONT_TYPE_MENU]->RenderString(stringstartposName,   y+height,dx- (stringstartposName - x), optionName, color);

	if (selected)
		CVFD::getInstance()->showMenuText(0, optionName, -1, true); // UTF-8

	return y+height;
}


/////////////////////////////////////////////////
// MenuTargets
////////////////////////////////////////////////
int CFileChooser::exec(CMenuTarget* parent, const std::string & /*actionKey*/)
{
	if (parent != NULL)
		parent->hide();

	CFileBrowser browser;
	browser.Dir_Mode=true;
	std::string oldPath = *dirPath;
	if (browser.exec(dirPath->c_str())) {
		*dirPath = browser.getSelectedFile()->Name;
		if (check_dir(dirPath->c_str(), true))
			*dirPath = oldPath;
	}

	return menu_return::RETURN_REPAINT;
}

CDirMenu::CDirMenu(std::vector<MB_DIR>* dir_list)
{
	unsigned int i;
	changed = false;
	dirList = dir_list;

	if (dirList->empty())
		return;

	for (i = 0; i < MB_MAX_DIRS; i++)
		dirNfsMountNr[i] = -1;

	for (i = 0; i < dirList->size() && i < MB_MAX_DIRS; i++)
	{
		for (int nfs = 0; nfs < NETWORK_NFS_NR_OF_ENTRIES; nfs++)
		{
			int result = -1;
			if (!g_settings.network_nfs[nfs].local_dir.empty())
				result = (*dirList)[i].name.compare(0,g_settings.network_nfs[nfs].local_dir.size(),g_settings.network_nfs[nfs].local_dir) ;
			printf("[CDirMenu] (nfs%d) %s == (mb%d) %s (%d)\n",nfs,g_settings.network_nfs[nfs].local_dir.c_str(),i,(*dirList)[i].name.c_str(),result);

			if (result == 0)
			{
				dirNfsMountNr[i] = nfs;
				break;
			}
		}
	}
}

int CDirMenu::exec(CMenuTarget* parent, const std::string & actionKey)
{
	int returnval = menu_return::RETURN_REPAINT;

	if (parent)
		parent->hide();

	if (actionKey.empty())
	{
		changed = false;
		return show();
	}
	else if (actionKey.size() == 1)
	{
		printf("[CDirMenu].exec %s\n",actionKey.c_str());
		int number = atoi(actionKey.c_str());
		if (number < MB_MAX_DIRS)
		{
			if (dirState[number] == DIR_STATE_SERVER_DOWN)
			{
				printf("try to start server: %s %s\n","ether-wake", g_settings.network_nfs[dirNfsMountNr[number]].mac.c_str());
				if (my_system(2, "ether-wake", g_settings.network_nfs[dirNfsMountNr[number]].mac.c_str()) != 0)
					perror("ether-wake failed");

				dirOptionText[number] = "STARTE SERVER";
			}
			else if (dirState[number] == DIR_STATE_NOT_MOUNTED)
			{
				printf("[CDirMenu] try to mount %d,%d\n",number,dirNfsMountNr[number]);
				CFSMounter::MountRes res;
				res = CFSMounter::mount(g_settings.network_nfs[dirNfsMountNr[number]].ip,
						g_settings.network_nfs[dirNfsMountNr[number]].dir,
						g_settings.network_nfs[dirNfsMountNr[number]].local_dir,
						(CFSMounter::FSType)g_settings.network_nfs[dirNfsMountNr[number]].type,
						g_settings.network_nfs[dirNfsMountNr[number]].username,
						g_settings.network_nfs[dirNfsMountNr[number]].password,
						g_settings.network_nfs[dirNfsMountNr[number]].mount_options1,
						g_settings.network_nfs[dirNfsMountNr[number]].mount_options2);
				if (res == CFSMounter::MRES_OK) // if mount is successful we set the state to active in any case
					*(*dirList)[number].used = true;

				// try to mount
				updateDirState();
				changed = true;
			}
			else if (dirState[number] == DIR_STATE_MOUNTED)
			{
				if (*(*dirList)[number].used == true)
					*(*dirList)[number].used = false;
				else
					*(*dirList)[number].used = true;
				//CFSMounter::umount(g_settings.network_nfs_local_dir[dirNfsMountNr[number]]);
				updateDirState();
				changed = true;
			}
		}
	}
	return returnval;
}

extern int pinghost(const std::string &hostname, std::string *ip = NULL);
void CDirMenu::updateDirState(void)
{
	unsigned int drivefree = 0;
	struct statfs s;

	for (unsigned int i = 0; i < dirList->size() && i < MB_MAX_DIRS; i++)
	{
		dirOptionText[i] = "UNBEKANNT";
		dirState[i] = DIR_STATE_UNKNOWN;
		// 1st ping server
		printf("updateDirState: %d: state %d nfs %d\n", i, dirState[i], dirNfsMountNr[i]);
		if (dirNfsMountNr[i] != -1)
		{
			int retvalue = pinghost(g_settings.network_nfs[dirNfsMountNr[i]].ip);
			if (retvalue == 0)//LOCALE_PING_UNREACHABLE
			{
				dirOptionText[i] = "Server, offline";
				dirState[i] = DIR_STATE_SERVER_DOWN;
			}
			else if (retvalue == 1)//LOCALE_PING_OK
			{
				if (!CFSMounter::isMounted(g_settings.network_nfs[dirNfsMountNr[i]].local_dir))
				{
					dirOptionText[i] = "Not mounted";
					dirState[i] = DIR_STATE_NOT_MOUNTED;
				}
				else
				{
					dirState[i] = DIR_STATE_MOUNTED;
				}
			}
		}
		else
		{
			// not a nfs dir, probably IDE? we accept this so far
			dirState[i] = DIR_STATE_MOUNTED;
		}
		if (dirState[i] == DIR_STATE_MOUNTED)
		{
			if (*(*dirList)[i].used == true)
			{
				if (statfs((*dirList)[i].name.c_str(), &s) >= 0)
				{
					drivefree = (s.f_bfree * s.f_bsize)>>30;
					char tmp[20];
					snprintf(tmp, 19,"%3d GB free",drivefree);
					dirOptionText[i] = tmp;
				}
				else
				{
					dirOptionText[i] = "? GB free";
				}
			}
			else
			{
				dirOptionText[i] = "Inactive";
			}
		}
	}
}

int CDirMenu::show(void)
{
	if (dirList->empty())
		return menu_return::RETURN_REPAINT;

	char tmp[20];

	CMenuWidget dirMenu(LOCALE_MOVIEBROWSER_HEAD, NEUTRINO_ICON_MOVIEPLAYER);
	dirMenu.addIntroItems(LOCALE_MOVIEBROWSER_MENU_DIRECTORIES_HEAD);

	updateDirState();
	for (unsigned int i = 0; i < dirList->size() && i < MB_MAX_DIRS; i++)
	{
		snprintf(tmp, sizeof(tmp),"%d",i);
		dirMenu.addItem(new CMenuForwarder ((*dirList)[i].name.c_str(), (dirState[i] != DIR_STATE_UNKNOWN), dirOptionText[i], this, tmp));
	}
	int ret = dirMenu.exec(NULL," ");
	return ret;
}
