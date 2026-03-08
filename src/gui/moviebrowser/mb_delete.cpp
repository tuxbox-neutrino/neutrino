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

	Module Name: mb_delete.cpp

	Description: Delete helpers for moviebrowser

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

#include <global.h>

#include <driver/fontrenderer.h>
#include <driver/record.h>
#include <gui/widget/hintbox.h>
#include <gui/widget/msgbox.h>
#include <system/helpers.h>
#include <timerdclient/timerdclient.h>

#include <stdio.h>
#include <sys/stat.h>
#include <unistd.h>

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

		dprintf(DEBUG_DEBUG, "[mb] List size: %d\n", (int)m_vMovieInfo.size());
		for (auto mi_it = m_vMovieInfo.begin(); mi_it != m_vMovieInfo.end(); )
		{
			MI_MOVIE_INFO *mi = mi_it->get();
			if (
				   mi->file.Name == movieinfo->file.Name
				&& mi->epgTitle == movieinfo->epgTitle
				&& ( mi->epgInfo1 == movieinfo->epgInfo1 || (mi->epgInfo1 == " " && movieinfo->epgInfo1.empty()) ) //FIXME if movieinfo->epgInfo1 is empty, epgInfo1 in xml have whitespace
				&& mi->length == movieinfo->length
			)
				mi_it = m_vMovieInfo.erase(mi_it);
			else
				++mi_it;
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
	bool result = false;

	MI_MOVIE_INFO *movieinfo;
	movieinfo = NULL;

	getSelectedFiles(filelist, movielist);

	dprintf(DEBUG_DEBUG, "[mb] onDelete(%s) filelist  size: %zd\n", cursor_only ? "true" : "false", filelist.size());
	dprintf(DEBUG_DEBUG, "[mb] onDelete(%s) movielist size: %zd\n", cursor_only ? "true" : "false", movielist.size());

	if (cursor_only || (filelist.empty() || movielist.empty()))
	{
		dprintf(DEBUG_DEBUG, "[mb] onDelete(%s) clearing the lists\n", cursor_only ? "true" : "false");

		filelist.clear();
		movielist.clear();

		// just add the m_movieSelectionHandler
		filelist.push_back(m_movieSelectionHandler->file);
		movielist.push_back(m_movieSelectionHandler);

		dprintf(DEBUG_DEBUG, "[mb] onDelete(%s) filelist  size: %zd\n", cursor_only ? "true" : "false", filelist.size());
		dprintf(DEBUG_DEBUG, "[mb] onDelete(%s) movielist size: %zd\n", cursor_only ? "true" : "false", movielist.size());
	}

	MI_MOVIE_LIST dellist;
	MI_MOVIE_LIST::iterator dellist_it;
	dellist.clear();
	unsigned int dellist_cnt = 0;
	for (filelist_it = filelist.begin(); filelist_it != filelist.end(); ++filelist_it)
	{
		unsigned int idx = filelist_it - filelist.begin();
		movieinfo = movielist[idx];
		dprintf(DEBUG_DEBUG, "[mb] try to delete %d:%s\n", idx, movieinfo->file.Name.c_str());

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
