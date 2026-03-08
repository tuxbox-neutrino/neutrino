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

	Module Name: mb_exec_loop.cpp

	Description: Event loop and run handling for moviebrowser

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

#include <driver/display.h>
#include <gui/infoviewer.h>
#include <gui/nfs.h>
#include <neutrino.h>

#include <sys/mount.h>

#ifdef ENABLE_LCD4LINUX
#include "driver/lcd4l.h"
#endif


int CMovieBrowser::exec(const char* path)
{
	bool res = false;
	menu_ret = menu_return::RETURN_REPAINT;

	dprintf(DEBUG_DEBUG, "[mb]->%s\n", __func__);
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
		dprintf(DEBUG_DEBUG, "[mb] remount\n");
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

#ifdef ENABLE_LCD4LINUX
	if (g_settings.lcd4l_support)
		CLCD4l::getInstance()->setActionKey("moviebrowser");
#endif

	bool loop = true;
	bool result;
	int timeout = g_settings.timing[SNeutrinoSettings::TIMING_FILEBROWSER];
	uint64_t timeoutEnd = CRCInput::calcTimeoutEnd(timeout);
	while (loop)
	{
		g_RCInput->getMsgAbsoluteTimeout(&msg, &data, &timeoutEnd);

		result = onButtonPress(msg);
		if (result == false)
		{
			if (msg == CRCInput::RC_timeout && returnDefaultOnTimeout)
			{
				dprintf(DEBUG_DEBUG, "[mb] Timerevent\n");
				loop = false;
			}
			else if ((msg == NeutrinoMessages::EVT_TIMER) && (data == g_InfoViewer->sec_timer_id))
			{
				if (timeset)
					refreshTitle();
			}
			else if (msg == CRCInput::RC_ok)
			{
				for (unsigned int i = 0; i < m_vMovieInfo.size(); i++) {
					if (m_vMovieInfo[i]->marked) {
						dprintf(DEBUG_DEBUG, "[mb] has selected\n");
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
						dprintf(DEBUG_DEBUG, "[mb] stop: %d start:%d \n",m_movieSelectionHandler->bookmarks.lastPlayStop,m_movieSelectionHandler->bookmarks.start);
						m_currentStartPos = showStartPosSelectionMenu(); // display start menu m_currentStartPos =
					}

					if (m_currentStartPos >= 0) {
						playing_info = m_movieSelectionHandler;
						dprintf(DEBUG_DEBUG, "[mb] start pos: %d s\n",m_currentStartPos);
						res = true;
						loop = false;
					} else
						refresh();
				}
			}
			else if (CNeutrinoApp::getInstance()->backKey(msg))
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
				dprintf(DEBUG_DEBUG, "[mb]->exec: getInstance\n");
				menu_ret = menu_return::RETURN_EXIT_ALL;
				loop = false;
			}
		}

		if (msg <= CRCInput::RC_MaxRC)
			timeoutEnd = CRCInput::calcTimeoutEnd(timeout); // calcualate next timeout
	}
	hide();

	m_prevBrowserSelection = m_currentBrowserSelection;
	m_prevRecordSelection = m_currentRecordSelection;
	m_prevPlaySelection = m_currentPlaySelection;

	saveSettings(&m_settings);

	// make stale if we should reload the next time, but not if movie has to be played
	if (m_settings.reload == true && res == false)
	{
		dprintf(DEBUG_DEBUG, "[mb] force reload next time\n");
		fileInfoStale();
	}

#ifdef ENABLE_LCD4LINUX
	if (g_settings.lcd4l_support)
		CLCD4l::getInstance()->clearActionKey();
#endif

	return (res);
}
