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

	Module Name: mb_selection.cpp

	Description: Selection and filter helpers for moviebrowser

	Date:	   Nov 2005
	Author: Guenther@tuxbox.berlios.org
		based on code of Steffen Hehn 'McClean'

	(C) 2009-2015 Stefan Seyfried
	(C) 2016      Sven Hoefer

	outsourced:
	(C) 2026 Thilo Graf 'dbt'
*/

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "mb.h"
#include "mb_functions.h"

#include <global.h>

#include <system/helpers.h>

#include <algorithm>
#include <inttypes.h>
#include <stdio.h>
#include <time.h>


bool CMovieBrowser::onSortMovieInfoHandleList(std::vector<MI_MOVIE_INFO*>& handle_list, MB_INFO_ITEM sort_item, MB_DIRECTION direction)
{
	if (handle_list.empty())
		return false;

	if (sort_item >= MB_INFO_MAX_NUMBER || sortBy[sort_item] == NULL)
		sort_item = MB_INFO_TITLE;

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

	sort(handle_list.begin(), handle_list.end(), sortBy[sort_item]);

	return (true);
}

void CMovieBrowser::updateMovieSelection(void)
{
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
			if (m_currentRecordSelection != old_movie_selection)
				new_selection = true;

			if (m_currentRecordSelection < m_vHandleRecordList.size())
				m_movieSelectionHandler = m_vHandleRecordList[m_currentRecordSelection];
		}
	}

	if (new_selection == true)
	{
		refreshHDDLevel();
		refreshMovieInfo();
		refreshLCD();
	}
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
	if (m_FilterLines.lineArray[0].v_text.empty()) return;

	bool result = true;
	int selected_line = m_pcFilter->getSelectedLine();
	if (selected_line > 0)
		selected_line--;

	if (m_settings.filter.item == MB_INFO_FILEPATH)
	{
		m_settings.filter.optionString = m_FilterLines.lineArray[0].v_text[selected_line+1];
		m_settings.filter.optionVar = selected_line;
	}
	else if (m_settings.filter.item == MB_INFO_INFO1)
	{
		m_settings.filter.optionString = m_FilterLines.lineArray[0].v_text[selected_line+1];
	}
	else if (m_settings.filter.item == MB_INFO_MAJOR_GENRE)
	{
		m_settings.filter.optionString = g_Locale->getText(GENRE_ALL[selected_line].value);
		m_settings.filter.optionVar = GENRE_ALL[selected_line].key;
	}
	else if (m_settings.filter.item == MB_INFO_SERIE)
	{
		m_settings.filter.optionString = m_FilterLines.lineArray[0].v_text[selected_line+1];
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

std::string CMovieBrowser::replaceInGUI(std::string text)
{
	std::string t(text);

	t = str_replace("\u000a", ", ", t);
	t = str_replace("\u000d", ", ", t);

	return t;
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
		case MB_INFO_FILENAME: 				// = 0,
			*item_string = movie_info.file.getFileName();
			break;
		case MB_INFO_FILEPATH: 				// = 1,
			if (!m_dirNames.empty())
				*item_string = m_dirNames[movie_info.dirItNr];
			break;
		case MB_INFO_TITLE: 				// = 2,
			*item_string = movie_info.epgTitle;
			if (strcmp("not available",movie_info.epgTitle.c_str()) == 0)
				result = false;
			if (movie_info.epgTitle.empty())
				result = false;
			break;
		case MB_INFO_SERIE: 				// = 3,
			*item_string = movie_info.serieName;
			break;
		case MB_INFO_INFO1:				// = 4,
			*item_string = replaceInGUI(movie_info.epgInfo1);
			break;
		case MB_INFO_MAJOR_GENRE: 			// = 5,
			snprintf(str_tmp, sizeof(str_tmp),"%2d",movie_info.genreMajor);
			*item_string = str_tmp;
			break;
		case MB_INFO_MINOR_GENRE: 			// = 6,
			snprintf(str_tmp, sizeof(str_tmp),"%2d",movie_info.genreMinor);
			*item_string = str_tmp;
			break;
		case MB_INFO_INFO2: 				// = 7,
			*item_string = movie_info.epgInfo2;
			break;
		case MB_INFO_PARENTAL_LOCKAGE: 			// = 8,
			snprintf(str_tmp, sizeof(str_tmp),"%2d",movie_info.parentalLockAge);
			*item_string = str_tmp;
			break;
		case MB_INFO_CHANNEL: 				// = 9,
			*item_string = movie_info.channelName;
			break;
		case MB_INFO_BOOKMARK: 				// = 10,
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
		case MB_INFO_QUALITY: 				// = 11,
			snprintf(str_tmp, sizeof(str_tmp),"%d",movie_info.quality);
			*item_string = str_tmp;
			break;
		case MB_INFO_PREVPLAYDATE: 			// = 12,
			if (movie_info.dateOfLastPlay == 0)
			{
				*item_string = "---";
			}
			else
			{
				tm_tmp = localtime(&movie_info.dateOfLastPlay);
				snprintf(str_tmp, sizeof(str_tmp),"%02d.%02d.%02d",tm_tmp->tm_mday,(tm_tmp->tm_mon)+ 1, tm_tmp->tm_year >= 100 ? tm_tmp->tm_year-100 : tm_tmp->tm_year);
				*item_string = str_tmp;
			}
			break;
		case MB_INFO_RECORDDATE: 			// = 13,
			tm_tmp = localtime(&movie_info.file.Time);
			snprintf(str_tmp, sizeof(str_tmp),"%02d.%02d.%02d",tm_tmp->tm_mday,(tm_tmp->tm_mon) + 1,tm_tmp->tm_year >= 100 ? tm_tmp->tm_year-100 : tm_tmp->tm_year);
			*item_string = str_tmp;
			break;
		case MB_INFO_PRODDATE: 				// = 14,
			snprintf(str_tmp, sizeof(str_tmp),"%d",movie_info.productionDate);
			*item_string = str_tmp;
			break;
		case MB_INFO_COUNTRY: 				// = 15,
			*item_string = movie_info.productionCountry;
			break;
		case MB_INFO_GEOMETRIE: 			// = 16,
			result = false;
			break;
		case MB_INFO_AUDIO: 				// = 17,
			// we just return the number of audiopids
			snprintf(str_tmp, sizeof(str_tmp), "%d", (int)movie_info.audioPids.size());
			*item_string = str_tmp;
			break;
		case MB_INFO_LENGTH: 				// = 18,
		{
			snprintf(str_tmp, sizeof(str_tmp),"%dh %02dm", movie_info.length/60, movie_info.length%60);
			*item_string = str_tmp;
			break;
		}
		case MB_INFO_SIZE: 				// = 19,
			snprintf(str_tmp, sizeof(str_tmp),"%4" PRIu64 "",movie_info.file.Size>>20);
			*item_string = str_tmp;
			break;
		case MB_INFO_RATING: 				// = 20,
			if (movie_info.rating)
			{
				snprintf(str_tmp, sizeof(str_tmp),"%d,%d",movie_info.rating/10, movie_info.rating%10);
				*item_string = str_tmp;
			}
			break;
		case MB_INFO_SPACER: 				// = 21,
			*item_string="";
			break;
		case MB_INFO_RECORDTIME: 			// = 22,
			if (show_mode == MB_SHOW_RECORDS)
			{
				tm_tmp = localtime(&movie_info.file.Time);
				snprintf(str_tmp, sizeof(str_tmp),"%02d:%02d", tm_tmp->tm_hour, tm_tmp->tm_min);
				*item_string = str_tmp;
			}
			break;
		case MB_INFO_PERCENT_ELAPSED:	// = 23,
			*item_string = "";
			if (movie_info.bookmarks.lastPlayStop > 0 && movie_info.length > 0)
			{
				int pos = movie_info.bookmarks.lastPlayStop * 100 / (movie_info.length * 60);
				if (pos > 100)
					pos = 100;
				*item_string = to_string(pos);
			}
			break;
		case MB_INFO_MAX_NUMBER:  // = 24,
		default:
			*item_string="";
			result = false;
			break;
	}
	return(result);
}
