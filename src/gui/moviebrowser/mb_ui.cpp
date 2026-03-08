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

	Module Name: mb_ui.cpp

	Description: UI and render helpers for moviebrowser

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
#include "mb_constants.h"

#include <global.h>

#include <driver/display.h>
#include <driver/fontrenderer.h>
#include <driver/record.h>
#include <gui/components/cc.h>
#include <gui/widget/buttons.h>
#include <gui/widget/icons.h>
#include <system/helpers.h>
#include <timerdclient/timerdclient.h>

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <sys/stat.h>
#include <sys/vfs.h>
#include <unistd.h>

#ifdef ENABLE_LCD4LINUX
#include "driver/lcd4l.h"
#endif


#define TITLE_FONT g_Font[SNeutrinoSettings::FONT_TYPE_MENU_TITLE]
#define FOOT_FONT g_Font[SNeutrinoSettings::FONT_TYPE_MENU_FOOT]


void CMovieBrowser::hide(void)
{
	if (m_header)	{
		delete m_header; m_header = NULL;
	}
	if (m_detailsLine){
		delete m_detailsLine; m_detailsLine = NULL;
	}
	old_EpgId = 0;
	old_ChannelName.clear();
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
	dprintf(DEBUG_DEBUG, "[mb]->%s\n", __func__);

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
		dprintf(DEBUG_DEBUG, "[mb] paint, ERROR: not enought memory to allocate windows");
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
	onSetGUIWindow(m_settings.gui);

	return (true);
}

void CMovieBrowser::refresh(void)
{
	dprintf(DEBUG_DEBUG, "[mb]->%s\n", __func__);

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

std::string CMovieBrowser::getScreenshotName(const std::string &movie, bool is_dir)
{
	std::string ext;
	std::string ret;
	size_t found;

	if (movie.empty())
		return "";

	if (is_dir)
		found = movie.size();
	else
		found = movie.find_last_of(".");

	if (found == std::string::npos)
		return "";

	if (PicExts.empty())
		return "";

	for (const std::string &e : PicExts) {
		ret = movie;
		ext = e;
		ret.replace(found, ret.length() - found, ext);
		if (!access(ret, F_OK))
			return ret;
	}
	return "";
}

void CMovieBrowser::refreshChannelLogo(void)
{
	dprintf(DEBUG_DEBUG, "[mb]->%s:%d\n", __func__, __LINE__);

	// set channel logo
	if (g_settings.channellist_show_channellogo)
	{
		m_header->setChannelLogo(m_movieSelectionHandler->epgId >> 16, m_movieSelectionHandler->channelName, (CCHeaderTypes::cc_logo_alignment_t)g_settings.channellist_show_channellogo);
	}

	if (old_EpgId != m_movieSelectionHandler->epgId >> 16 || old_ChannelName != m_movieSelectionHandler->channelName)
	{
		refreshTitle();
	}
}

void CMovieBrowser::initMovieCover(void)
{
	dprintf(DEBUG_DEBUG, "[mb]->%s:%d\n", __func__, __LINE__);

	if (m_movieSelectionHandler == NULL) {
		return;
	}

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

	std::string cover_file = getScreenshotName(m_movieSelectionHandler->file.Name, S_ISDIR(m_movieSelectionHandler->file.Mode));
	if ((cover_file.empty()) && (m_movieSelectionHandler->file.Name.length() > 18))
	{
		std::string cover = m_movieSelectionHandler->file.Name;
		cover.replace((cover.length()-18), 15, ""); //covername without yyyymmdd_hhmmss
		cover_file = getScreenshotName(cover);
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
			m_movieCover = new CComponentsPicture(0, 0, cover_file, NULL, CC_SHADOW_ON, COL_MENUCONTENTDARK_PLUS_0, 0, COL_SHADOW_PLUS_0,  CFrameBuffer::TM_NONE);
			m_movieCover->enableFrame(true, 1);
		}

		// always align positions and dimensions
		if (m_windowFocus == MB_FOCUS_BROWSER && m_settings.browserAdditional)
		{
			if (m_movieCover->getWidth() < m_movieCover->getHeight())
				cover_w /= 2; // cover is upright, so we use just half width first

			m_movieCover->setHeight(0); // force recalculation
dprintf(DEBUG_DEBUG, "[mb]->%s:%d m_movieCover->getHeight(): %d\n", __func__, __LINE__, m_movieCover->getHeight());
			m_movieCover->setWidth(cover_w);
			if (m_movieCover->getHeight() > movieCoverBox.iHeight/3)
				m_movieCover->setHeight(movieCoverBox.iHeight/3); // use maximal one third of box height

			m_movieCover->setXPos(movieCoverBox.iX + (movieCoverBox.iWidth - m_movieCover->getWidth())/2);
			m_movieCover->setYPos(movieCoverBox.iY + OFFSET_INNER_MID);
		}
		else
		{
			m_movieCover->setWidth(0); // force recalculation
dprintf(DEBUG_DEBUG, "[mb]->%s:%d m_movieCover->getWidth(): %d\n", __func__, __LINE__, m_movieCover->getWidth());
			m_movieCover->setHeight(cover_h);
			if (m_movieCover->getWidth() > movieCoverBox.iWidth/3)
				m_movieCover->setWidth(movieCoverBox.iWidth/3); // use maximal one third of box width

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
	dprintf(DEBUG_DEBUG, "[mb]->%s:%d\n", __func__, __LINE__);

	if (m_movieCover)
		m_movieCover->paint(CC_SAVE_SCREEN_YES);
}

void CMovieBrowser::hideMovieCover(void)
{
	dprintf(DEBUG_DEBUG, "[mb]->%s:%d\n", __func__, __LINE__);

	if (m_movieCover)
		m_movieCover->hide();
}

void CMovieBrowser::refreshMovieInfo(void)
{
	dprintf(DEBUG_DEBUG, "[mb]->%s m_vMovieInfo.size %d\n", __func__, (int)m_vMovieInfo.size());

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

		int xpos  = m_cBoxFrameBrowserList.iX - DETAILSLINE_WIDTH;
		int ypos1 = m_cBoxFrameBrowserList.iY + hheight + theight + pos*fheight + (fheight/2);
		int ypos2 = m_cBoxFrameInfo1.iY + (m_cBoxFrameInfo1.iHeight/2);

		if (m_detailsLine == NULL)
			m_detailsLine = new CComponentsDetailsLine();

		m_detailsLine->setDimensionsAll(xpos, ypos1, ypos2, fheight/2, m_cBoxFrameInfo1.iHeight-2*RADIUS_LARGE);
		m_detailsLine->paint(true);
	}
}

void CMovieBrowser::refreshHDDLevel(bool show)
{
	dprintf(DEBUG_DEBUG, "[mb]->%s:%d\n", __func__, __LINE__);

	int percent_used = 0;
	struct statfs s;

	if (getSelectedFile() != NULL)
	{
		if (::statfs(getSelectedFile()->Name.c_str(), &s) == 0)
		{
			if (s.f_blocks > 0)
			{
				uint64_t bytes_total = s.f_blocks * s.f_bsize;
				uint64_t bytes_free  = s.f_bfree  * s.f_bsize;
				uint64_t bytes_used = bytes_total - bytes_free;
				percent_used = (bytes_used * 200 + bytes_total) / 2 / bytes_total;
			}
		}
	}

	if (show)
		m_header->enableProgessBar(percent_used);
	else
		m_header->setProgessBar(percent_used);
}

void CMovieBrowser::refreshLCD(void)
{
	if (m_vMovieInfo.empty() || m_movieSelectionHandler == NULL)
	{
#ifdef ENABLE_LCD4LINUX
		if (g_settings.lcd4l_support)
			CLCD4l::getInstance()->CreateEventFile("", g_settings.lcd4l_convert);
#endif
		CVFD::getInstance()->showMenuText(0, "", -1, true); // UTF-8
	}
	else
	{
#ifdef ENABLE_LCD4LINUX
		if (g_settings.lcd4l_support)
			CLCD4l::getInstance()->CreateEventFile(m_movieSelectionHandler->epgTitle.c_str(), g_settings.lcd4l_convert);
#endif
		CVFD::getInstance()->showMenuText(0, m_movieSelectionHandler->epgTitle.c_str(), -1, true); // UTF-8
	}
}

void CMovieBrowser::refreshFilterList(void)
{
	dprintf(DEBUG_DEBUG, "[mb]->refreshFilterList %d\n",m_settings.filter.item);

	std::string string_item;

	m_FilterLines.rows = 1;
	m_pcFilter->cleanupRow(&m_FilterLines, 0);
	m_FilterLines.rowWidth[0] = 100;
	m_FilterLines.lineHeader[0] = "";

	if (m_vMovieInfo.empty())
		return; // exit here if nothing else is to do

	if (m_settings.filter.item == MB_INFO_MAX_NUMBER)
	{
		// show Main List
		string_item = g_Locale->getText(LOCALE_MOVIEBROWSER_INFO_GENRE_MAJOR);
		m_pcFilter->addLine2Row(&m_FilterLines, 0, string_item);

		string_item = g_Locale->getText(LOCALE_MOVIEBROWSER_INFO_INFO1);
		m_pcFilter->addLine2Row(&m_FilterLines, 0, string_item);

		string_item = g_Locale->getText(LOCALE_MOVIEBROWSER_INFO_PATH);
		m_pcFilter->addLine2Row(&m_FilterLines, 0, string_item);

		string_item = g_Locale->getText(LOCALE_MOVIEBROWSER_INFO_SERIE);
		m_pcFilter->addLine2Row(&m_FilterLines, 0, string_item);
	}
	else
	{
		std::string tmp = g_Locale->getText(LOCALE_MOVIEBROWSER_FILTER_OFF);
		m_pcFilter->addLine2Row(&m_FilterLines, 0, tmp);

		if (m_settings.filter.item == MB_INFO_FILEPATH)
		{
			for (unsigned int i = 0 ; i < m_dirNames.size(); i++)
				m_pcFilter->addLine2Row(&m_FilterLines, 0, m_dirNames[i]);
		}
		else if (m_settings.filter.item == MB_INFO_INFO1)
		{
			for (unsigned int i = 0; i < m_vMovieInfo.size(); i++)
			{
				MI_MOVIE_INFO *mi = m_vMovieInfo[i].get();
				bool found = false;
				for (unsigned int t = 0; t < m_FilterLines.lineArray[0].v_text.size() && found == false; t++)
				{
					if (strcmp(m_FilterLines.lineArray[0].v_text[t].c_str(), mi->epgInfo1.c_str()) == 0)
						found = true;
				}
				if (found == false)
					m_pcFilter->addLine2Row(&m_FilterLines, 0, mi->epgInfo1);
			}
		}
		else if (m_settings.filter.item == MB_INFO_MAJOR_GENRE)
		{
			for (int i = 0; i < GENRE_ALL_COUNT; i++)
			{
				std::string tmpl = g_Locale->getText(GENRE_ALL[i].value);
				m_pcFilter->addLine2Row(&m_FilterLines, 0, tmpl);
			}
		}
		else if (m_settings.filter.item == MB_INFO_SERIE)
		{
			updateSerienames();
			for (unsigned int i = 0; i < m_vHandleSerienames.size(); i++)
				m_pcFilter->addLine2Row(&m_FilterLines, 0, m_vHandleSerienames[i]->serieName);
		}
	}
	m_pcFilter->setLines(&m_FilterLines);
}

void CMovieBrowser::refreshLastPlayList(void) //P2
{
	std::string string_item;

	// Initialise and clear list array
	m_playListLines.rows = m_settings.lastPlayRowNr;
	for (int row = 0 ;row < m_settings.lastPlayRowNr; row++)
	{
		m_pcLastPlay->cleanupRow(&m_playListLines, row);
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
		if (isParentalLock(*m_vMovieInfo[file]) == false)
		{
			movie_handle = m_vMovieInfo[file].get();
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
			m_pcLastPlay->addLine2Row(&m_playListLines, row, string_item);
		}
		m_playListLines.marked.push_back(m_vHandlePlayList[handle]->marked);
	}
	m_pcLastPlay->setLines(&m_playListLines);
	m_pcLastPlay->cutRowText(&m_settings.browserCutLongRowText);

	m_currentPlaySelection = m_pcLastPlay->getSelectedLine();
	// update selected movie if browser is in the focus
	if (m_windowFocus == MB_FOCUS_LAST_PLAY)
		updateMovieSelection();
}

void CMovieBrowser::refreshLastRecordList(void) //P2
{
	std::string string_item;

	// Initialise and clear list array
	m_recordListLines.rows = m_settings.lastRecordRowNr;
	for (int row = 0 ;row < m_settings.lastRecordRowNr; row++)
	{
		m_pcLastRecord->cleanupRow(&m_recordListLines, row);
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
		if (isParentalLock(*m_vMovieInfo[file]) == false)
		{
			movie_handle = m_vMovieInfo[file].get();
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
			m_pcLastRecord->addLine2Row(&m_recordListLines, row, string_item);
		}
		m_recordListLines.marked.push_back(m_vHandleRecordList[handle]->marked);
	}

	m_pcLastRecord->setLines(&m_recordListLines);
	m_pcLastRecord->cutRowText(&m_settings.browserCutLongRowText);

	m_currentRecordSelection = m_pcLastRecord->getSelectedLine();
	// update selected movie if browser is in the focus
	if (m_windowFocus == MB_FOCUS_LAST_RECORD)
		updateMovieSelection();
}

void CMovieBrowser::refreshBrowserList(void) //P1
{
	dprintf(DEBUG_DEBUG, "[mb]->%s\n", __func__);
	std::string string_item;

	// Initialise and clear list array
	m_browserListLines.rows = m_settings.browserRowNr;
	for (int row = 0; row < m_settings.browserRowNr; row++)
	{
		m_pcBrowser->cleanupRow(&m_browserListLines, row);
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
		if (isFiltered(*m_vMovieInfo[file]) == false &&
				isParentalLock(*m_vMovieInfo[file]) == false &&
				(m_settings.browser_serie_mode == 0 || m_vMovieInfo[file]->serieName.empty() || m_settings.filter.item == MB_INFO_SERIE))
		{
			movie_handle = m_vMovieInfo[file].get();
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

			CProgressBar* elapsed = NULL;
			CComponentsPicture* seen = NULL;
			if (m_settings.browserRowItem[row] == MB_INFO_PERCENT_ELAPSED)
			{
				getMovieInfoItem(*m_vHandleBrowserList[handle], MB_INFO_PERCENT_ELAPSED, &string_item);

				/*
				 * NOTE: Get threshold offset from record safety settings to trigger the "seen" tag.
				 * Better solutions are welcome!
				*/
				int pre = 0, post = 0;
				float trigger_offset = 0;
				g_Timerd->getRecordingSafety(pre,post);

				if (post < 120)
					post = 120;

				if (m_vHandleBrowserList[handle]->length * 60 > post)
					trigger_offset = round((float)post * 100.0 / (m_vHandleBrowserList[handle]->length * 60.0));

				int elapsed_percent = atoi(string_item);
				string_item.clear(); // reset not needed

				if ((float)elapsed_percent < 100.0-trigger_offset)
				{
					if (elapsed_percent > 0)
					{
						elapsed = new CProgressBar();
						elapsed->setHeight(m_pcBrowser->getLineHeight()/2);
						elapsed->setType(CProgressBar::PB_TIMESCALE);
						elapsed->setValues(elapsed_percent, 100);
						m_pcBrowser->addLine2Row(&m_browserListLines, row, string_item, elapsed);
					}
				}
				else
				{
					seen = new CComponentsPicture(0, 0, 0, m_pcBrowser->getLineHeight(), NEUTRINO_ICON_MARKER_DIALOG_OK);
					seen->doPaintBg(false);
					m_pcBrowser->addLine2Row(&m_browserListLines, row, string_item, seen);
				}
			}
			if (!elapsed && !seen)
				m_pcBrowser->addLine2Row(&m_browserListLines, row, string_item, NULL);
		}
		if (CRecordManager::getInstance()->getRecordInstance(m_vHandleBrowserList[handle]->file.Name) != NULL)
			m_browserListLines.Icon.push_back(NEUTRINO_ICON_MARKER_RECORD);
		else
			m_browserListLines.Icon.push_back("");
		m_browserListLines.marked.push_back(m_vHandleBrowserList[handle]->marked);
	}
	m_pcBrowser->setLines(&m_browserListLines);
	m_pcBrowser->cutRowText(&m_settings.browserCutLongRowText);

	m_currentBrowserSelection = m_pcBrowser->getSelectedLine();
	// update selected movie if browser is in the focus
	if (m_windowFocus == MB_FOCUS_BROWSER)
		updateMovieSelection();
}

void CMovieBrowser::refreshTitle(void)
{
	std::string title = m_textTitle.c_str();
	const char *icon = NEUTRINO_ICON_MOVIEPLAYER;

	dprintf(DEBUG_DEBUG, "[mb]->refreshTitle: %s\n", title.c_str());

	int x = m_cBoxFrameTitleRel.iX + m_cBoxFrame.iX;
	int y = m_cBoxFrameTitleRel.iY + m_cBoxFrame.iY;
	int w = m_cBoxFrameTitleRel.iWidth;
	int h = m_cBoxFrameTitleRel.iHeight;

	if (!m_header){
		m_header = new CComponentsHeader(x, y, w, h, title.c_str(), icon, CComponentsHeader::CC_BTN_LEFT | CComponentsHeader::CC_BTN_RIGHT | CComponentsHeader::CC_BTN_HELP);
	}else{
		m_header->setCaption(title.c_str());
	}

	if (timeset)
	{
		m_header->enableClock();
		if (m_header->getClockObject())
			m_header->getClockObject()->setCorner(RADIUS_LARGE, CORNER_TOP_RIGHT);
	}

	m_header->paint(CC_SAVE_SCREEN_NO);
	newHeader = m_header->isPainted();

	refreshHDDLevel(true);
}

int CMovieBrowser::refreshFoot(bool show)
{
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
