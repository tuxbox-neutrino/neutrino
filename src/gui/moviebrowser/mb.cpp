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
	(C) 2016, 2026 Thilo Graf 'dbt'
*/

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <global.h>
#include <driver/screen_max.h>

#include <algorithm>
#include <cmath>
//#include <cstdlib>
#include "mb.h"
#include "mb_constants.h"
#include "mb_functions.h"
#include "mb_help.h"
#include <gui/filebrowser.h>
#include <gui/moviedb/mdb-imdb.h>
#include <gui/moviedb/mdb-tmdb.h>
#include <gui/epgview.h>
#include <gui/widget/hintbox.h>
#include <gui/widget/icons.h>
#include <gui/widget/menue_options.h>
#include <gui/components/cc.h>
#include <gui/widget/msgbox.h>
#include <gui/widget/stringinput.h>
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
#include <system/helpers.h>
#include <zapit/debug.h>
#include <driver/moviecut.h>
#include <driver/fontrenderer.h>

#include <timerdclient/timerdclient.h>
#include <system/hddstat.h>

#ifdef ENABLE_LCD4LINUX
#include "driver/lcd4l.h"
#endif

extern CPictureViewer * g_PicViewer;

MI_MOVIE_INFO* playing_info = NULL;
CMovieBrowser::CMovieBrowser(): configfile ('\t')
{
	init();
}

CMovieBrowser::~CMovieBrowser()
{
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

	if (m_movieCover)
		delete m_movieCover;

	if (m_header)
		delete m_header;
}

void CMovieBrowser::clearSelection()
{
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
	if (m_settings.sorting.item >= MB_INFO_MAX_NUMBER || sortBy[m_settings.sorting.item] == NULL)
		m_settings.sorting.item = MB_INFO_TITLE;

	if (m_settings.filter.item >= MB_INFO_MAX_NUMBER)
		m_settings.filter.item = MB_INFO_MAX_NUMBER;

	if (m_settings.parentalLockAge >= MI_PARENTAL_MAX_NUMBER)
		m_settings.parentalLockAge = MI_PARENTAL_OVER18;
	if (m_settings.parentalLock >= MB_PARENTAL_LOCK_MAX_NUMBER)
		m_settings.parentalLock = MB_PARENTAL_LOCK_OFF;

	m_settings.browserFrameHeight = m_settings.browserAdditional ? m_settings.browserFrameHeightAdditional : m_settings.browserFrameHeightGeneral;
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
	m_movieCover = NULL;

	old_EpgId = 0;
	old_ChannelName.clear();

	m_doRefresh = false;
	m_doLoadMovies = false;
}
