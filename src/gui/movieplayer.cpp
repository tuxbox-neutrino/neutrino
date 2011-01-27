/*
  Neutrino-GUI  -   DBoxII-Project

  Movieplayer (c) 2003, 2004 by gagga
  Based on code by Dirch, obi and the Metzler Bros. Thanks.

  $Id: movieplayer.cpp,v 1.97 2004/07/18 00:54:52 thegoodguy Exp $

  Homepage: http://www.giggo.de/dbox2/movieplayer.html

  License: GPL

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; either version 2 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <gui/movieplayer.h>

#include <global.h>
#include <neutrino.h>

#include <driver/fontrenderer.h>
#include <driver/rcinput.h>
#include <driver/vcrcontrol.h>
#include <daemonc/remotecontrol.h>
#include <system/settings.h>

#include <gui/eventlist.h>
#include <gui/color.h>
#include <gui/infoviewer.h>
#include <gui/nfs.h>
#include <gui/bookmarkmanager.h>
#include <gui/timeosd.h>

#include <gui/widget/buttons.h>
#include <gui/widget/icons.h>
#include <gui/widget/messagebox.h>
#include <gui/widget/hintbox.h>
#include <gui/widget/stringinput.h>
#include <gui/widget/stringinput_ext.h>
#include <gui/widget/helpbox.h>
#include <gui/infoclock.h>
#include <gui/plugins.h>

#include <algorithm>
#include <fstream>
#include <sstream>
#include <limits>

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

#include <poll.h>
#include <sys/timeb.h>

#include <video.h>
#include <playback.h>
#include "libtuxtxt/teletext.h"


int dvbsub_start(int pid);
int dvbsub_pause();

extern cVideo * videoDecoder;
static cPlayback *playback;

extern CRemoteControl *g_RemoteControl;	/* neutrino.cpp */
void strReplace(std::string & orig, const char *fstr, const std::string rstr);
#define MOVIE_HINT_BOX_TIMER 5	// time to show bookmark hints in seconds
extern CPlugins *g_PluginList;
extern CInfoClock *InfoClock;

#define MINUTEOFFSET 117*262072
#define MP_TS_SIZE 262072	// ~0.5 sec

extern char rec_filename[512];
extern off64_t glob_limit;
extern int glob_splits;

#ifndef __USE_FILE_OFFSET64
#error not using 64 bit file offsets
#endif /* __USE_FILE__OFFSET64 */

static unsigned short g_apids[REC_MAX_APIDS];
static unsigned short g_ac3flags[REC_MAX_APIDS];
static unsigned short g_numpida = 0;
static unsigned short g_vpid = 0;
static unsigned short g_vtype = 0;
static std::string    g_language[REC_MAX_APIDS];
static unsigned short g_currentapid = 0, g_currentac3 = 0, apidchanged = 0;

bool get_movie_info_apid_name(int apid, MI_MOVIE_INFO * movie_info, std::string * apidtitle)
{
	if (movie_info == NULL || apidtitle == NULL)
		return false;

	for (int i = 0; i < (int)movie_info->audioPids.size(); i++) {
		if (movie_info->audioPids[i].epgAudioPid == apid && !movie_info->audioPids[i].epgAudioPidName.empty()) {
			*apidtitle = movie_info->audioPids[i].epgAudioPidName;
			return true;
		}
	}
	return false;
}

int CAPIDSelectExec::exec(CMenuTarget * /*parent*/, const std::string & actionKey)
{
	apidchanged = 0;
	unsigned int sel = atoi(actionKey.c_str());
	if (g_currentapid != g_apids[sel - 1]) {
		g_currentapid = g_apids[sel - 1];
		g_currentac3 = g_ac3flags[sel - 1];
		apidchanged = 1;
		printf("[movieplayer] apid changed to %d\n", g_apids[sel - 1]);
	}
	return menu_return::RETURN_EXIT;
}

CMoviePlayerGui* CMoviePlayerGui::instance_mp = NULL;

CMoviePlayerGui& CMoviePlayerGui::getInstance()
{
	if ( !instance_mp )
	{
		instance_mp = new CMoviePlayerGui();
		printf("[neutrino CMoviePlayerGui] Instance created...\n");
	}
	
	return *instance_mp;
}
 
void CMoviePlayerGui::Delete()
{
	if ( instance_mp )
	{
		delete instance_mp;
		printf("[neutrino CMoviePlayerGui] Instance removed...\n");
	}
	
	instance_mp = NULL;
}


CMoviePlayerGui::CMoviePlayerGui()
{
	Init();
}

void CMoviePlayerGui::Init(void)
{
	stopped = false;
	hintBox = new CHintBox(LOCALE_MESSAGEBOX_INFO, g_Locale->getText(LOCALE_MOVIEPLAYER_PLEASEWAIT));	// UTF-8

	frameBuffer = CFrameBuffer::getInstance();

	if (strlen(g_settings.network_nfs_moviedir) != 0)
		Path_local = g_settings.network_nfs_moviedir;
	else
		Path_local = "/";
	Path_vlc = "vlc://";
	Path_vlc += g_settings.streaming_server_startdir;
	Path_vlc_settings = g_settings.streaming_server_startdir;

	if (g_settings.filebrowser_denydirectoryleave)
		filebrowser = new CFileBrowser(Path_local.c_str());
	else
		filebrowser = new CFileBrowser();

	filebrowser->Multi_Select = false;
	filebrowser->Dirs_Selectable = false;
	filebrowser->Hide_records = true;
	playback = new cPlayback(3);
	moviebrowser = new CMovieBrowser();
	bookmarkmanager = 0;

	tsfilefilter.addFilter("ts");
	tsfilefilter.addFilter("avi");
	tsfilefilter.addFilter("mkv");
	tsfilefilter.addFilter("wav");
	tsfilefilter.addFilter("asf");
	tsfilefilter.addFilter("aiff");
	tsfilefilter.addFilter("mpg");
	tsfilefilter.addFilter("mpeg");
	tsfilefilter.addFilter("m2p");
	tsfilefilter.addFilter("mpv");
	tsfilefilter.addFilter("vob");
	tsfilefilter.addFilter("m2ts");
	tsfilefilter.addFilter("mp4");


	vlcfilefilter.addFilter("mpg");
	vlcfilefilter.addFilter("mpeg");
	vlcfilefilter.addFilter("m2p");
	vlcfilefilter.addFilter("avi");
	vlcfilefilter.addFilter("vob");
	pesfilefilter.addFilter("mpv");
	filebrowser->Filter = &tsfilefilter;
	rct = 0;
	speed = 1;
	slow = 0;
	jumpseconds = 0;
	showaudioselectdialog = false;
	timeshift = 0;
}

CMoviePlayerGui::~CMoviePlayerGui()
{
#if 0
	delete filebrowser;
	if (moviebrowser)
		delete moviebrowser;
	delete hintBox;
	if (bookmarkmanager)
		delete bookmarkmanager;
	g_Zapit->setStandby(false);
	g_Sectionsd->setPauseScanning(false);
#endif
	delete playback;
}

bool CMoviePlayerGui::Playing(void)
{
	return stopped;
}

void CMoviePlayerGui::cutNeutrino()
{
	if (stopped)
		return;

	stopped = true;
	//g_Zapit->setStandby(true);
	g_Zapit->lockPlayBack();
	g_Sectionsd->setPauseScanning(true);

	CNeutrinoApp::getInstance()->handleMsg(NeutrinoMessages::CHANGEMODE, NeutrinoMessages::mode_ts);
	m_LastMode = (CNeutrinoApp::getInstance()->getLastMode() | NeutrinoMessages::norezap);
}

void CMoviePlayerGui::restoreNeutrino()
{
	if (!stopped)
		return;

	stopped = false;

	//g_Zapit->setStandby(false);
	g_Zapit->unlockPlayBack();
	g_Sectionsd->setPauseScanning(false);

	CNeutrinoApp::getInstance()->handleMsg(NeutrinoMessages::CHANGEMODE, m_LastMode);
	//CVFD::getInstance()->showServicename(g_RemoteControl->getCurrentChannelName());
}

int CMoviePlayerGui::exec(CMenuTarget * parent, const std::string & actionKey)
{
	printf("[movieplayer] actionKey=%s\n", actionKey.c_str());

	if (Path_vlc_settings != g_settings.streaming_server_startdir) {
		Path_vlc = "vlc://";
		Path_vlc += g_settings.streaming_server_startdir;
		Path_vlc_settings = g_settings.streaming_server_startdir;
	}
	bookmarkmanager = new CBookmarkManager();

	dvbsub_pause();

	if (parent) {
		parent->hide();
	}

	bool usedBackground = frameBuffer->getuseBackground();
	if (usedBackground) {
		frameBuffer->saveBackgroundImage();
		frameBuffer->Clear();
	}

	const CBookmark *theBookmark = NULL;
	if (actionKey == "bookmarkplayback") {
		isBookmark = true;
		theBookmark = bookmarkmanager->getBookmark(NULL);
		if (theBookmark == NULL) {
			bookmarkmanager->flush();
			return menu_return::RETURN_REPAINT;
		}
	}

	isBookmark = false;
	startfilename = "";
	startposition = 0;

	isMovieBrowser = false;
	minuteoffset = MINUTEOFFSET;
	secondoffset = minuteoffset / 60;
#if 0
	if (actionKey == "fileplayback") {
		cutNeutrino();
		PlayStream(STREAMTYPE_FILE);
	} else if (actionKey == "dvdplayback") {
		cutNeutrino();
		PlayStream(STREAMTYPE_DVD);
	} else if (actionKey == "vcdplayback") {
		cutNeutrino();
		PlayStream(STREAMTYPE_SVCD);
	} else if (actionKey == "tsplayback") {
		timeshift = 0;
		PlayFile();
	} else
#endif
	if (actionKey == "tsmoviebrowser") {
		isMovieBrowser = true;
		timeshift = 0;
		PlayFile();
	}
	else if (actionKey == "fileplayback") {
		timeshift = 0;
		PlayFile();
	}
	else if (actionKey == "timeshift") {
		cutNeutrino();
		timeshift = 1;
		PlayFile();
	}
	else if (actionKey == "ptimeshift") {
		cutNeutrino();
		timeshift = 2;
		PlayFile();
	}
	else if (actionKey == "rtimeshift") {
		cutNeutrino();
		timeshift = 3;
		PlayFile();
	}
#if 0
	else if (actionKey == "bookmarkplayback") {
		isBookmark = true;
		if (theBookmark != NULL) {
			cutNeutrino();
			startfilename = theBookmark->getUrl();
			sscanf(theBookmark->getTime(), "%lld", &startposition);
			int vlcpos = startfilename.rfind("vlc://");
			if (vlcpos == 0) {
				PlayStream(STREAMTYPE_FILE);
			} else {
				timeshift = 0;
				PlayFile();
			}
		}
	}
#endif

	bookmarkmanager->flush();
	// Restore previous background
	if (usedBackground) {
		frameBuffer->restoreBackgroundImage();
		frameBuffer->useBackground(true);
		frameBuffer->paintBackground();
	}

	restoreNeutrino();
	CVFD::getInstance()->setMode(CVFD::MODE_TVRADIO);
	//g_RCInput->postMsg(NeutrinoMessages::SHOW_INFOBAR, 0);
	//dvbsub_start(0);

	if (bookmarkmanager)
		delete bookmarkmanager;
	if (timeshift) {
		timeshift = 0;
		return menu_return::RETURN_EXIT_ALL;
	}
	return menu_return::RETURN_REPAINT;
	//return menu_return::RETURN_EXIT_ALL;
}

void CMoviePlayerGui::updateLcd(const std::string & sel_filename)
{
	char tmp[20];
	std::string lcd;

	switch (playstate) {
	case CMoviePlayerGui::PAUSE:
		lcd = "|| ";
		lcd += sel_filename;
		//lcd += ')';
		break;
	case CMoviePlayerGui::REW:
		sprintf(tmp, "%dx<< ", speed);
		lcd = tmp;
		lcd += sel_filename;
		break;
	case CMoviePlayerGui::FF:
		sprintf(tmp, "%dx>> ", speed);
		lcd = tmp;
		lcd += sel_filename;
		break;
#if 0
	case CMoviePlayerGui::JF:
		sprintf(tmp, "%ds>> ", speed);
		lcd = tmp;
		lcd += sel_filename;
		break;
#endif
	default:
		if (slow) {
			sprintf(tmp, "%ds||> ", slow);
			lcd = tmp;
		} else
			lcd = "> ";
		lcd += sel_filename;
		break;
	}

	//CVFD::getInstance()->showServicename(lcd);
	CVFD::getInstance()->showMenuText(0, lcd.c_str(), -1, true);
}

extern bool has_hdd;
#define TIMESHIFT_SECONDS 3
void CMoviePlayerGui::PlayFile(void)
{
	neutrino_msg_t msg;
	neutrino_msg_data_t data;
	int position = 0, duration = 0;
	bool timeshift_paused = false;

	std::string sel_filename;
	CTimeOSD FileTime;
	bool update_lcd = true, open_filebrowser = true, start_play = false, exit = false;
	bool timesh = timeshift;
	bool was_file = false;
	bool time_forced = false;
	playstate = CMoviePlayerGui::STOPPED;
	bool is_file_player = false;
	timeb current_time;
	CMovieInfo cMovieInfo;	// funktions to save and load movie info
	MI_MOVIE_INFO *p_movie_info = NULL;	// movie info handle which comes from the MovieBrowser, if not NULL MoviePla yer is able to save new bookmarks

	if (has_hdd)
		system("(rm /hdd/.wakeup; touch /hdd/.wakeup; sync) > /dev/null  2> /dev/null &");

	if (timeshift) {
		CVCRControl::getInstance()->GetPids(&g_vpid, &g_vtype, &g_currentapid, &g_currentac3, &g_numpida, g_apids, g_ac3flags);
		p_movie_info = CVCRControl::getInstance()->GetMovieInfo();
	}

	int width = 280;
	int height = 65;
        int x = frameBuffer->getScreenX() + (frameBuffer->getScreenWidth() - width) / 2;
        //int y = frameBuffer->getScreenY() + (frameBuffer->getScreenHeight() - height) / 2;
        int y = frameBuffer->getScreenY() + frameBuffer->getScreenHeight() - height - 20;

	CBox boxposition(x, y, width, height);	// window position for the hint boxes

	CTextBox endHintBox(g_Locale->getText(LOCALE_MOVIEBROWSER_HINT_MOVIEEND), NULL, CTextBox::CENTER /*CTextBox::AUTO_WIDTH | CTextBox::AUTO_HIGH */ , &boxposition);
	CTextBox comHintBox(g_Locale->getText(LOCALE_MOVIEBROWSER_HINT_JUMPFORWARD), NULL, CTextBox::CENTER /*CTextBox::AUTO_WIDTH | CTextBox::AUTO_HIGH */ , &boxposition);
	CTextBox loopHintBox(g_Locale->getText(LOCALE_MOVIEBROWSER_HINT_JUMPBACKWARD), NULL, CTextBox::CENTER /*CTextBox::AUTO_WIDTH | CTextBox::AUTO_HIGH */ , &boxposition);
	CTextBox newLoopHintBox(g_Locale->getText(LOCALE_MOVIEBROWSER_HINT_NEWBOOK_BACKWARD), NULL, CTextBox::CENTER /*CTextBox::AUTO_WIDTH | CTextBox::AUTO_HIGH */ , &boxposition);
	CTextBox newComHintBox(g_Locale->getText(LOCALE_MOVIEBROWSER_HINT_NEWBOOK_FORWARD), NULL, CTextBox::CENTER /*CTextBox::AUTO_WIDTH | CTextBox::AUTO_HIGH */ , &boxposition);

	bool showEndHintBox = false;	// flag to check whether the box shall be painted
	bool showComHintBox = false;	// flag to check whether the box shall be painted
	bool showLoopHintBox = false;	// flag to check whether the box shall be painted
	int jump_not_until = 0;	// any jump shall be avoided until this time (in seconds from moviestart)
	MI_BOOKMARK new_bookmark;	// used for new movie info bookmarks created from the movieplayer
	new_bookmark.pos = 0;	// clear , since this is used as flag for bookmark activity
	new_bookmark.length = 0;

	// very dirty usage of the menue, but it works and I already spent to much time with it, feel free to make it better ;-)
#define BOOKMARK_START_MENU_MAX_ITEMS 6
	CSelectedMenu cSelectedMenuBookStart[BOOKMARK_START_MENU_MAX_ITEMS];

	CMenuWidget bookStartMenu(LOCALE_MOVIEBROWSER_BOOK_NEW, NEUTRINO_ICON_STREAMING);
	bookStartMenu.addItem(GenericMenuSeparator);
	bookStartMenu.addItem(new CMenuForwarder(LOCALE_MOVIEPLAYER_HEAD, true, NULL, &cSelectedMenuBookStart[0]));
	bookStartMenu.addItem(GenericMenuSeparatorLine);
	bookStartMenu.addItem(new CMenuForwarder(LOCALE_MOVIEBROWSER_HEAD, true, NULL, &cSelectedMenuBookStart[1]));
	bookStartMenu.addItem(new CMenuForwarder(LOCALE_MOVIEBROWSER_BOOK_TYPE_FORWARD, true, NULL, &cSelectedMenuBookStart[2]));
	bookStartMenu.addItem(new CMenuForwarder(LOCALE_MOVIEBROWSER_BOOK_TYPE_BACKWARD, true, NULL, &cSelectedMenuBookStart[3]));
	bookStartMenu.addItem(new CMenuForwarder(LOCALE_MOVIEBROWSER_BOOK_MOVIESTART, true, NULL, &cSelectedMenuBookStart[4]));
	bookStartMenu.addItem(new CMenuForwarder(LOCALE_MOVIEBROWSER_BOOK_MOVIEEND, true, NULL, &cSelectedMenuBookStart[5]));

	rct = 0;

 go_repeat:
	do {

		if (exit) {
			exit = false;
			printf("[movieplayer] stop\n");
			playstate = CMoviePlayerGui::STOPPED;
			break;
		}

		if (timesh) {
			char fname[255];
			int cnt = 10 * 1000000;
			while (!strlen(rec_filename)) {
				usleep(1000);
				cnt -= 1000;
				if (!cnt)
					break;
			}
			if (!strlen(rec_filename))
				return;

			sprintf(fname, "%s.ts", rec_filename);
			filename = fname;
			sel_filename = std::string(rindex(filename, '/') + 1);
			printf("Timeshift: %s\n", sel_filename.c_str());

			update_lcd = true;
			start_play = true;
			open_filebrowser = false;
			isBookmark = false;
			timesh = false;
			CVFD::getInstance()->setMode(CVFD::MODE_TVRADIO);
			FileTime.SetMode(CTimeOSD::MODE_DESC);
			FileTime.show(position / 1000);
			FileTime.updatePos(file_prozent);
		}

		if (isBookmark) {
			open_filebrowser = false;
			filename = startfilename.c_str();
			sel_filename = startfilename;
			update_lcd = true;
			start_play = true;
			isBookmark = false;
			timeshift = false;
			CVFD::getInstance()->setMode(CVFD::MODE_MENU_UTF8);
		}

		if (isMovieBrowser == true) {
			// do all moviebrowser stuff here ( like commercial jump ect.)
			if (playstate == CMoviePlayerGui::PLAY) {
				playback->GetPosition(position, duration);
				int play_sec = position / 1000;	// get current seconds from moviestart
				//TRACE(" %6ds\r\n",play_sec);

				if (play_sec + 10 < jump_not_until || play_sec > jump_not_until + 10)
					jump_not_until = 0;	// check if !jump is stale (e.g. if user jumped forward or backward)
				if (new_bookmark.pos == 0)	// do bookmark activities only, if there is no new bookmark started
				{
					if (p_movie_info != NULL)	// process bookmarks if we have any movie info
					{
						if (p_movie_info->bookmarks.end != 0) {
							// *********** Check for stop position *******************************
							if (play_sec >= p_movie_info->bookmarks.end - MOVIE_HINT_BOX_TIMER && play_sec < p_movie_info->bookmarks.end && play_sec > jump_not_until) {
								if (showEndHintBox == false) {
									endHintBox.paint();	// we are 5 sec before the end postition, show warning
									showEndHintBox = true;
									TRACE("[mp]  user stop in 5 sec...\r\n");
								}
							} else {
								if (showEndHintBox == true) {
									endHintBox.hide();	// if we showed the warning before, hide the box again
									showEndHintBox = false;
								}
							}

							if (play_sec >= p_movie_info->bookmarks.end && play_sec <= p_movie_info->bookmarks.end + 2 && play_sec > jump_not_until)	// stop playing
							{
								// *********** we ARE close behind the stop position, stop playing *******************************
								TRACE("[mp]  user stop\r\n");
								playstate = CMoviePlayerGui::STOPPED;
							}
						}
						// *************  Check for bookmark jumps *******************************
						int loop = true;
						showLoopHintBox = false;
						showComHintBox = false;
						for (int book_nr = 0; book_nr < MI_MOVIE_BOOK_USER_MAX && loop == true; book_nr++) {
							if (p_movie_info->bookmarks.user[book_nr].pos != 0 && p_movie_info->bookmarks.user[book_nr].length != 0) {
								// valid bookmark found, now check if we are close before or after it
								if (play_sec >= p_movie_info->bookmarks.user[book_nr].pos - MOVIE_HINT_BOX_TIMER && play_sec < p_movie_info->bookmarks.user[book_nr].pos && play_sec > jump_not_until) {
									if (p_movie_info->bookmarks.user[book_nr].length < 0)
										showLoopHintBox = true;	// we are 5 sec before , show warning
									else if (p_movie_info->bookmarks.user[book_nr].length > 0)
										showComHintBox = true;	// we are 5 sec before, show warning
									//else  // TODO should we show a plain bookmark infomation as well?
								}

								if (play_sec >= p_movie_info->bookmarks.user[book_nr].pos && play_sec <= p_movie_info->bookmarks.user[book_nr].pos + 2 && play_sec > jump_not_until)	//
								{
									//for plain bookmark, the following calc shall result in 0 (no jump)
									jumpseconds = p_movie_info->bookmarks.user[book_nr].length;

									// we are close behind the bookmark, do bookmark activity (if any)
									if (p_movie_info->bookmarks.user[book_nr].length < 0) {
										// if the jump back time is to less, it does sometimes cause problems (it does probably jump only 5 sec which will cause the next jump, and so on)
										if (jumpseconds > -15)
											jumpseconds = -15;

										jumpseconds = jumpseconds + p_movie_info->bookmarks.user[book_nr].pos;
										//playstate = CMoviePlayerGui::JPOS;	// bookmark  is of type loop, jump backward
										playback->SetPosition(jumpseconds * 1000);
									} else if (p_movie_info->bookmarks.user[book_nr].length > 0) {
										// jump at least 15 seconds
										if (jumpseconds < 15)
											jumpseconds = 15;
										jumpseconds = jumpseconds + p_movie_info->bookmarks.user[book_nr].pos;

										//playstate = CMoviePlayerGui::JPOS;	// bookmark  is of type loop, jump backward
										playback->SetPosition(jumpseconds * 1000);
									}
									TRACE("[mp]  do jump %d sec\r\n", jumpseconds);
									update_lcd = true;
									loop = false;	// do no further bookmark checks
								}
							}
						}
						// check if we shall show the commercial warning
						if (showComHintBox == true) {
							comHintBox.paint();
							TRACE("[mp]  com jump in 5 sec...\r\n");
						} else
							comHintBox.hide();

						// check if we shall show the loop warning
						if (showLoopHintBox == true) {
							loopHintBox.paint();
							TRACE("[mp]  loop jump in 5 sec...\r\n");
						} else
							loopHintBox.hide();
					}
				}
			}
		}		// isMovieBrowser == true

		if (open_filebrowser) {
			open_filebrowser = false;
			timeshift = false;
			FileTime.hide();
			/*clear audipopids */
			for (int i = 0; i < g_numpida; i++) {
				g_apids[i] = 0;
				g_ac3flags[i] = 0;
				g_language[i].clear();
			}
			g_numpida = 0; g_currentapid = 0;

			if (isMovieBrowser == true) {
				// start the moviebrowser instead of the filebrowser
				if (moviebrowser->exec(Path_local.c_str())) {
					// get the current path and file name
					Path_local = moviebrowser->getCurrentDir();
					CFile *file;

					if ((file = moviebrowser->getSelectedFile()) != NULL) {
						CFile::FileType ftype;
						ftype = file->getType();

						if(ftype == CFile::FILE_AVI || ftype == CFile::FILE_MKV) {
							is_file_player = true;	// Movie player AVI/MKV
						} else {
							is_file_player = false;	// Movie player AVI/MKV
						}
						filename = file->Name.c_str();
						sel_filename = file->getFileName();

						// get the movie info handle (to be used for e.g. bookmark handling)
						p_movie_info = moviebrowser->getCurrentMovieInfo();
						bool recfile = CNeutrinoApp::getInstance()->recordingstatus && !strncmp(rec_filename, filename, strlen(rec_filename));
						if (!recfile && p_movie_info->length) {
							minuteoffset = file->Size / p_movie_info->length;
							minuteoffset = (minuteoffset / MP_TS_SIZE) * MP_TS_SIZE;
							if (minuteoffset < 5000000 || minuteoffset > 190000000)
								minuteoffset = MINUTEOFFSET;
							secondoffset = minuteoffset / 60;
						}

						if(!p_movie_info->audioPids.empty()) {
							g_currentapid = p_movie_info->audioPids[0].epgAudioPid;	//FIXME
							g_currentac3 = p_movie_info->audioPids[0].atype;
						}
						for (int i = 0; i < (int)p_movie_info->audioPids.size(); i++) {
							g_apids[i] = p_movie_info->audioPids[i].epgAudioPid;
							g_ac3flags[i] = p_movie_info->audioPids[i].atype;
							g_numpida++;
							if (p_movie_info->audioPids[i].selected) {
								g_currentapid = p_movie_info->audioPids[i].epgAudioPid;	//FIXME
								g_currentac3 = p_movie_info->audioPids[i].atype;
								//break;
							}
						}
						g_vpid = p_movie_info->epgVideoPid;
						g_vtype = p_movie_info->VideoType;
						printf("CMoviePlayerGui::PlayFile: file %s apid %X atype %d vpid %x vtype %d\n", filename, g_currentapid, g_currentac3, g_vpid, g_vtype);
						printf("Bytes per minute: %lld\n", minuteoffset);
						// get the start position for the movie
						startposition = 1000 * moviebrowser->getCurrentStartPos();
						//TRACE("[mp] start pos %llu, %d s Name: %s\r\n", startposition, moviebrowser->getCurrentStartPos(), filename);

						update_lcd = true;
						start_play = true;
						was_file = true;
					}
				} else if (playstate == CMoviePlayerGui::STOPPED) {
					was_file = false;
					break;
				}
				CVFD::getInstance()->setMode(CVFD::MODE_MENU_UTF8);
			} else {
				filebrowser->Filter = &tsfilefilter;

				if (filebrowser->exec(Path_local.c_str()) == true) {
					Path_local = filebrowser->getCurrentDir();
					CFile *file;
					if ((file = filebrowser->getSelectedFile()) != NULL) {

						CFile::FileType ftype;
						ftype = file->getType();

						is_file_player = true;
#if 0
						if(ftype == CFile::FILE_AVI || ftype == CFile::FILE_MKV || ftype == CFile::FILE_WAV || ftype == CFile::FILE_ASF) {

							is_file_player = true;	// Movie player AVI/MKV

						} else {
							is_file_player = false;	// Movie player AVI/MKV
						}
#endif

						filename = file->Name.c_str();
						update_lcd = true;
						start_play = true;
						was_file = true;
						sel_filename = filebrowser->getSelectedFile()->getFileName();
						//playstate = CMoviePlayerGui::PLAY;
					}
				} else if (playstate == CMoviePlayerGui::STOPPED) {
					was_file = false;
					break;
				}

				CVFD::getInstance()->setMode(CVFD::MODE_MENU_UTF8);
			}
		}

		if (update_lcd) {
			update_lcd = false;
			if (isMovieBrowser && strlen(p_movie_info->epgTitle.c_str()) && strncmp(p_movie_info->epgTitle.c_str(), "not", 3))
				updateLcd(p_movie_info->epgTitle);
			else
				updateLcd(sel_filename);
		}

		if (showaudioselectdialog) {
			CMenuWidget APIDSelector(LOCALE_APIDSELECTOR_HEAD, NEUTRINO_ICON_AUDIO);
			APIDSelector.addItem(GenericMenuSeparator);
			CAPIDSelectExec *APIDChanger = new CAPIDSelectExec;
			bool enabled;
			bool defpid;
			if(is_file_player && !g_numpida){
				playback->FindAllPids(g_apids, g_ac3flags, &g_numpida, g_language);
			}
			for (unsigned int count = 0; count < g_numpida; count++) {
				bool name_ok = false;
				char apidnumber[10];
				sprintf(apidnumber, "%d %X", count + 1, g_apids[count]);
				enabled = true;
				defpid = g_currentapid ? (g_currentapid == g_apids[count]) : (count == 0);
				std::string apidtitle = "Stream ";
				if(!is_file_player){
					name_ok = get_movie_info_apid_name(g_apids[count], p_movie_info, &apidtitle);
				}
				else if (!g_language[count].empty()){
					apidtitle = g_language[count];
					name_ok = true;
				}
				if (!name_ok)
					apidtitle = "Stream ";

				switch(g_ac3flags[count])
				{
					case 1: /*AC3,EAC3*/
						if (apidtitle.find("AC3") == std::numeric_limits<size_t>::max() || is_file_player)
							apidtitle.append(" (AC3)");
						break;
					case 2: /*teletext*/
						apidtitle.append(" (Teletext)");
						enabled = false;
						break;
					case 3: /*MP2*/
						apidtitle.append(" (MP2)");
						break;
					case 4: /*MP3*/
						apidtitle.append(" (MP3)");
						break;
					case 5: /*AAC*/
						apidtitle.append(" (AAC)");
						break;
					case 6: /*DTS*/
						apidtitle.append(" (DTS)");
						enabled = false;
						break;
					case 7: /*MLP*/
						apidtitle.append(" (MLP)");
						break;
					default:
						break;
				}
				if (!name_ok)
					apidtitle.append(apidnumber);

				APIDSelector.addItem(new CMenuForwarderNonLocalized(apidtitle.c_str(), enabled, NULL, APIDChanger, apidnumber, CRCInput::convertDigitToKey(count + 1)), defpid);
			}

			apidchanged = 0;
			APIDSelector.exec(NULL, "");
			if (apidchanged) {
				if (g_currentapid == 0) {
					g_currentapid = g_apids[0];
					g_currentac3 = g_ac3flags[0];
				}
				playback->SetAPid(g_currentapid, g_currentac3);
				apidchanged = 0;
			}
			delete APIDChanger;
			showaudioselectdialog = false;
			CVFD::getInstance()->setMode(CVFD::MODE_MENU_UTF8);
			update_lcd = true;
		}

		if (FileTime.IsVisible() /*FIXME && playstate == CMoviePlayerGui::PLAY */ ) {
			if (FileTime.GetMode() == CTimeOSD::MODE_ASC) {
				FileTime.update(position / 1000);
			} else {
				FileTime.update((duration - position) / 1000);
			}
			FileTime.updatePos(file_prozent);
		}

		if (start_play) {
			printf("Startplay at %d seconds\n", startposition/1000);
			start_play = false;
			if (playstate >= CMoviePlayerGui::PLAY) {
				playstate = CMoviePlayerGui::STOPPED;
				playback->Close();
			}

			cutNeutrino();
			playback->Open(is_file_player ? PLAYMODE_FILE : PLAYMODE_TS);

			printf("IS FILE PLAYER: %s\n", is_file_player ?  "true": "false" );

			if(!playback->Start((char *)filename, g_vpid, g_vtype, g_currentapid, g_currentac3)) {
				playback->Close();
				restoreNeutrino();
			} else {
				playstate = CMoviePlayerGui::PLAY;
				CVFD::getInstance()->ShowIcon(VFD_ICON_PLAY, true);
				if(timeshift) {
					timeshift_paused = true;
					startposition = -1;
					int i;
					int towait = (timeshift == 1) ? TIMESHIFT_SECONDS+1 : TIMESHIFT_SECONDS;
					for(i = 0; i < 500; i++) {
						playback->GetPosition(position, duration);
						startposition = (duration - position);

						//printf("CMoviePlayerGui::PlayFile: waiting for data, position %d duration %d (%d)\n", position, duration, towait);
						if(startposition > towait*1000)
							break;
						//sleep(1);
						usleep(20000);
					}
					if(timeshift == 3) {
						startposition = duration;
					} else {
						if(g_settings.timeshift_pause)
							playstate = CMoviePlayerGui::PAUSE;
						if(timeshift == 1)
							startposition = 0;
						else
							startposition = duration - TIMESHIFT_SECONDS*1000;
					}
					printf("******************* Timeshift %d, position %d, seek to %d seconds\n", timeshift, position, startposition/1000);
				}
				if(!is_file_player && startposition >= 0)//FIXME no jump for file at start yet
					playback->SetPosition(startposition, true);

				if(timeshift == 3) {
					playback->SetSpeed(-1);
				} else if(!timeshift || !g_settings.timeshift_pause) {
					playback->SetSpeed(1);
				}
			}
		}

		g_RCInput->getMsg(&msg, &data, 10);	// 1 secs..

		if(timeshift && g_settings.timeshift_pause) {
			if(!timeshift_paused && !videoDecoder->getBlank()) {
				playback->SetSpeed(0);
				timeshift_paused = true;
				playstate = CMoviePlayerGui::PAUSE;
			}
		}
		if ((playstate >= CMoviePlayerGui::PLAY) && (timeshift || (playstate != CMoviePlayerGui::PAUSE))) {
			if(playback->GetPosition(position, duration)) {
				if(duration > 100)
					file_prozent = (unsigned char) (position / (duration / 100));
				playback->GetSpeed(speed);
				printf("CMoviePlayerGui::PlayFile: speed %d position %d duration %d (%d, %d%%)\n", speed, position, duration, duration-position, file_prozent);
			}
		}

		if (msg == (neutrino_msg_t) g_settings.mpkey_plugin) {
			//g_PluginList->start_plugin_by_name (g_settings.movieplayer_plugin.c_str (), pidt);
		} else if (msg == (neutrino_msg_t) g_settings.mpkey_stop) {
			//exit play
			playstate = CMoviePlayerGui::STOPPED;
			if (isMovieBrowser == true && p_movie_info != NULL) {
				// if we have a movie information, try to save the stop position
				ftime(&current_time);
				p_movie_info->dateOfLastPlay = current_time.time;
				current_time.time = time(NULL);
				p_movie_info->bookmarks.lastPlayStop = position / 1000;

				cMovieInfo.saveMovieInfo(*p_movie_info);
				//p_movie_info->fileInfoStale(); //TODO: we might to tell the Moviebrowser that the movie info has changed, but this could cause long reload times  when reentering the Moviebrowser
			}

			if (!was_file)
				exit = true;
		} else if (msg == (neutrino_msg_t) g_settings.mpkey_play) {
			if (playstate >= CMoviePlayerGui::PLAY) {
				update_lcd = true;
				playstate = CMoviePlayerGui::PLAY;
				speed = 1;
				playback->SetSpeed(speed);
				if (!timeshift)
					callInfoViewer(p_movie_info->epgTitle, p_movie_info->epgInfo1, p_movie_info->epgChannel, duration, position);
			} else if (!timeshift) {
				open_filebrowser = true;
			}
			if (time_forced) {
				time_forced = false;
				if (g_settings.mode_clock)
					InfoClock->StopClock();
				FileTime.hide();
			}
		} else if (msg == (neutrino_msg_t) g_settings.mpkey_pause) {
			update_lcd = true;
			if (playstate == CMoviePlayerGui::PAUSE) {
				playstate = CMoviePlayerGui::PLAY;
				//CVFD::getInstance()->ShowIcon(VFD_ICON_PAUSE, false);
				speed = 1;
				playback->SetSpeed(speed);
			} else {
				playstate = CMoviePlayerGui::PAUSE;
				//CVFD::getInstance()->ShowIcon(VFD_ICON_PAUSE, true);
				speed = 0;
				playback->SetSpeed(speed);
			}
			if (!timeshift)
				callInfoViewer(p_movie_info->epgTitle, p_movie_info->epgInfo1, p_movie_info->epgChannel, position, duration);

		} else if (msg == (neutrino_msg_t) g_settings.mpkey_bookmark) {
			// is there already a bookmark activity?
			if (isMovieBrowser != true) {
				if (bookmarkmanager->getBookmarkCount() < bookmarkmanager->getMaxBookmarkCount()) {
					char timerstring[200];
					fprintf(stderr, "fileposition: %lld\n", fullposition);
					sprintf(timerstring, "%lld", fullposition);
					fprintf(stderr, "timerstring: %s\n", timerstring);
					std::string bookmarktime = "";
					bookmarktime.append(timerstring);
					fprintf(stderr, "bookmarktime: %s\n", bookmarktime.c_str());
					bookmarkmanager->createBookmark(filename, bookmarktime);
					fprintf(stderr, "bookmark done\n");
				} else {
					fprintf(stderr, "too many bookmarks\n");
					DisplayErrorMessage(g_Locale->getText(LOCALE_MOVIEPLAYER_TOOMANYBOOKMARKS));	// UTF-8
				}
			} else {
				int pos_sec = position / 1000;
				if (newComHintBox.isPainted() == true) {
					// yes, let's get the end pos of the jump forward
					new_bookmark.length = pos_sec - new_bookmark.pos;
					TRACE("[mp] commercial length: %d\r\n", new_bookmark.length);
					if (cMovieInfo.addNewBookmark(p_movie_info, new_bookmark) == true) {
						cMovieInfo.saveMovieInfo(*p_movie_info);	/* save immediately in xml file */
					}
					new_bookmark.pos = 0;	// clear again, since this is used as flag for bookmark activity
					newComHintBox.hide();
				} else if (newLoopHintBox.isPainted() == true) {
					// yes, let's get the end pos of the jump backward
					new_bookmark.length = new_bookmark.pos - pos_sec;
					new_bookmark.pos = pos_sec;
					TRACE("[mp] loop length: %d\r\n", new_bookmark.length);
					if (cMovieInfo.addNewBookmark(p_movie_info, new_bookmark) == true) {
						cMovieInfo.saveMovieInfo(*p_movie_info);	/* save immediately in xml file */
						jump_not_until = pos_sec + 5;	// avoid jumping for this time
					}
					new_bookmark.pos = 0;	// clear again, since this is used as flag for bookmark activity
					newLoopHintBox.hide();
				} else {
					// no, nothing else to do, we open a new bookmark menu
					new_bookmark.name = "";	// use default name
					new_bookmark.pos = 0;
					new_bookmark.length = 0;

					// next seems return menu_return::RETURN_EXIT, if something selected
					bookStartMenu.exec(NULL, "none");
					if (cSelectedMenuBookStart[0].selected == true) {
						/* Movieplayer bookmark */
						if (bookmarkmanager->getBookmarkCount() < bookmarkmanager->getMaxBookmarkCount()) {
							char timerstring[200];
							fprintf(stderr, "fileposition: %lld\n", fullposition);
							sprintf(timerstring, "%lld", fullposition);
							fprintf(stderr, "timerstring: %s\n", timerstring);
							std::string bookmarktime = "";
							bookmarktime.append(timerstring);
							fprintf(stderr, "bookmarktime: %s\n", bookmarktime.c_str());
							bookmarkmanager->createBookmark(filename, bookmarktime);
						} else {
							fprintf(stderr, "too many bookmarks\n");
							DisplayErrorMessage(g_Locale->getText(LOCALE_MOVIEPLAYER_TOOMANYBOOKMARKS));	// UTF-8
						}
						cSelectedMenuBookStart[0].selected = false;	// clear for next bookmark menu
					} else if (cSelectedMenuBookStart[1].selected == true) {
						/* Moviebrowser plain bookmark */
						new_bookmark.pos = pos_sec;
						new_bookmark.length = 0;
						if (cMovieInfo.addNewBookmark(p_movie_info, new_bookmark) == true)
							cMovieInfo.saveMovieInfo(*p_movie_info);	/* save immediately in xml file */
						new_bookmark.pos = 0;	// clear again, since this is used as flag for bookmark activity
						cSelectedMenuBookStart[1].selected = false;	// clear for next bookmark menu
					} else if (cSelectedMenuBookStart[2].selected == true) {
						/* Moviebrowser jump forward bookmark */
						new_bookmark.pos = pos_sec;
						TRACE("[mp] new bookmark 1. pos: %d\r\n", new_bookmark.pos);
						newComHintBox.paint();

						cSelectedMenuBookStart[2].selected = false;	// clear for next bookmark menu
					} else if (cSelectedMenuBookStart[3].selected == true) {
						/* Moviebrowser jump backward bookmark */
						new_bookmark.pos = pos_sec;
						TRACE("[mp] new bookmark 1. pos: %d\r\n", new_bookmark.pos);
						newLoopHintBox.paint();
						cSelectedMenuBookStart[3].selected = false;	// clear for next bookmark menu
					} else if (cSelectedMenuBookStart[4].selected == true) {
						/* Moviebrowser movie start bookmark */
						p_movie_info->bookmarks.start = pos_sec;
						TRACE("[mp] New movie start pos: %d\r\n", p_movie_info->bookmarks.start);
						cMovieInfo.saveMovieInfo(*p_movie_info);	/* save immediately in xml file */
						cSelectedMenuBookStart[4].selected = false;	// clear for next bookmark menu
					} else if (cSelectedMenuBookStart[5].selected == true) {
						/* Moviebrowser movie end bookmark */
						p_movie_info->bookmarks.end = pos_sec;
						TRACE("[mp]  New movie end pos: %d\r\n", p_movie_info->bookmarks.start);
						cMovieInfo.saveMovieInfo(*p_movie_info);	/* save immediately in xml file */
						cSelectedMenuBookStart[5].selected = false;	// clear for next bookmark menu
					}
				}
			}
		} else if (msg == (neutrino_msg_t) g_settings.mpkey_audio) {
			showaudioselectdialog = true;
		} else if (msg == (neutrino_msg_t) g_settings.mpkey_time) {
			if (FileTime.IsVisible()) {
				if (FileTime.GetMode() == CTimeOSD::MODE_ASC) {
					FileTime.SetMode(CTimeOSD::MODE_DESC);
					FileTime.update((duration - position) / 1000);
					FileTime.updatePos(file_prozent);
				} else {
					if (g_settings.mode_clock)
						InfoClock->StopClock();
					FileTime.hide();
				}
			} else {
				if (g_settings.mode_clock)
					InfoClock->StartClock();
				FileTime.SetMode(CTimeOSD::MODE_ASC);
				FileTime.show(position / 1000);
				FileTime.updatePos(file_prozent);
			}
		} else if (msg == (neutrino_msg_t) g_settings.mpkey_rewind) {	// rewind
			if (speed >= 0)
				speed = -1;
			else
				speed --;

			playback->SetSpeed(speed);
			playstate = CMoviePlayerGui::REW;
			update_lcd = true;

			if (!timeshift)
				callInfoViewer(p_movie_info->epgTitle, p_movie_info->epgInfo1, p_movie_info->epgChannel, position, duration);

			if (!FileTime.IsVisible()) {
				if (g_settings.mode_clock)
					InfoClock->StartClock();
				FileTime.SetMode(CTimeOSD::MODE_ASC);
				FileTime.show(position / 1000);
				FileTime.updatePos(file_prozent);
				time_forced = true;
			}
		} else if (msg == (neutrino_msg_t) g_settings.mpkey_forward) {	// fast-forward
			if (speed <= 0)
				speed = 2;
			else
				speed ++;

			playback->SetSpeed(speed);

			update_lcd = true;
			playstate = CMoviePlayerGui::FF;

			if (!timeshift)
				callInfoViewer(p_movie_info->epgTitle, p_movie_info->epgInfo1, p_movie_info->epgChannel, duration, position);

			if (!FileTime.IsVisible()) {
				if (g_settings.mode_clock)
					InfoClock->StartClock();
				FileTime.SetMode(CTimeOSD::MODE_ASC);
				FileTime.show(position / 1000);
				FileTime.updatePos(file_prozent);
				time_forced = true;
			}
		} else if (msg == CRCInput::RC_help || msg == CRCInput::RC_info) {
			if (timeshift)
				g_InfoViewer->showTitle(CNeutrinoApp::getInstance()->channelList->getActiveChannelNumber(), CNeutrinoApp::getInstance()->channelList->getActiveChannelName(), CNeutrinoApp::getInstance()->channelList->getActiveSatellitePosition(), CNeutrinoApp::getInstance()->channelList->getActiveChannel_ChannelID());	// UTF-8

			else {
				callInfoViewer(p_movie_info->epgTitle, p_movie_info->epgInfo1, p_movie_info->epgChannel, duration, position);
				CVFD::getInstance()->setMode(CVFD::MODE_MENU_UTF8);
				update_lcd = true;
				//showHelpTS();
			}
		} else if (msg == CRCInput::RC_1) {	// Jump Backwards 1 minute
			//update_lcd = true;
			playback->SetPosition(-60 * 1000);
		} else if (msg == CRCInput::RC_3) {	// Jump Forward 1 minute
			//update_lcd = true;
			playback->SetPosition(60 * 1000);
		} else if (msg == CRCInput::RC_4) {	// Jump Backwards 5 minutes
			playback->SetPosition(-5 * 60 * 1000);
		} else if (msg == CRCInput::RC_6) {	// Jump Forward 5 minutes
			playback->SetPosition(5 * 60 * 1000);
		} else if (msg == CRCInput::RC_7) {	// Jump Backwards 10 minutes
			playback->SetPosition(-10 * 60 * 1000);
		} else if (msg == CRCInput::RC_9) {	// Jump Forward 10 minutes
			playback->SetPosition(10 * 60 * 1000);
		} else if (msg == CRCInput::RC_2) {	// goto start
			playback->SetPosition(0, true);
		} else if (msg == CRCInput::RC_5) {	// goto middle
			playback->SetPosition(duration/2, true);
		} else if (msg == CRCInput::RC_8) {	// goto end
			playback->SetPosition(duration - 60 * 1000, true);
		} else if (msg == CRCInput::RC_page_up) {
			playback->SetPosition(10 * 1000);
		} else if (msg == CRCInput::RC_page_down) {
			playback->SetPosition(-10 * 1000);
		} else if (msg == CRCInput::RC_0) {	// cancel bookmark jump
			if (isMovieBrowser == true) {
				if (new_bookmark.pos != 0) {
					new_bookmark.pos = 0;	// stop current bookmark activity, TODO:  might bemoved to another key
					newLoopHintBox.hide();	// hide hint box if any
					newComHintBox.hide();
				}
				jump_not_until = (position / 1000) + 10;	// avoid bookmark jumping for the next 10 seconds, , TODO:  might be moved to another key
			}
		} else if (msg == CRCInput::RC_up || msg == CRCInput::RC_down) {
			if (msg == CRCInput::RC_up) {
				if (slow == 2)
					slow = 0;
				if (slow > 0)
					slow--;
			} else if (msg == CRCInput::RC_down) {
				if (slow == 0)
					slow++;
				slow++;
			}
			//set_slow (slow);
			update_lcd = true;
		} else if (msg == CRCInput::RC_radio) {
			if (isMovieBrowser == true && p_movie_info != NULL) {
				std::string fname = p_movie_info->file.Name;
				strReplace(fname, ".ts", ".bmp");
				CVCRControl::getInstance()->Screenshot(0, (char *)fname.c_str());
			}
		}
#if 0
		else if (msg == CRCInput::RC_shift_radio) {
			if (isMovieBrowser == true && p_movie_info != NULL) {
				time_t t = time(NULL);
				char filename[512];
				sprintf(filename, "%s", p_movie_info->file.Name.c_str());
				int pos = strlen(filename);
				strftime(&(filename[pos - 3]), sizeof(filename) - pos - 1, "%Y%m%d_%H%M%S", localtime(&t));
				strcat(filename, ".bmp");
				CVCRControl::getInstance()->Screenshot(0, filename);
			}
		}
#endif
		else if (msg == CRCInput::RC_sat || msg == CRCInput::RC_favorites) {
			//FIXME do nothing ?
		}
		else if(timeshift && (msg == CRCInput::RC_text || msg == CRCInput::RC_epg)) {
			bool restore = FileTime.IsVisible();
			FileTime.hide();
			if (g_settings.mode_clock)
				InfoClock->StopClock();

			if( msg == CRCInput::RC_epg )
				g_EventList->exec(CNeutrinoApp::getInstance()->channelList->getActiveChannel_ChannelID(), CNeutrinoApp::getInstance()->channelList->getActiveChannelName());
			else {
				if(g_settings.cacheTXT)
					tuxtxt_stop();
				tuxtx_main(g_RCInput->getFileHandle(), g_RemoteControl->current_PIDs.PIDs.vtxtpid, 0, 2);
				frameBuffer->paintBackground();
			}
			if (g_settings.mode_clock)
				InfoClock->StartClock();
			if(restore) {
				FileTime.show(position / 1000);
				FileTime.updatePos(file_prozent);
			}
		}
		else if (msg == CRCInput::RC_timeout) {
			// nothing
		} else if ( msg == NeutrinoMessages::ANNOUNCE_RECORD ||
				msg == NeutrinoMessages::RECORD_START) {
				CNeutrinoApp::getInstance()->handleMsg(msg, data);
		} else if (timeshift && msg == NeutrinoMessages::SHOW_EPG ) {
			bool restore = FileTime.IsVisible();
			FileTime.hide();
			if (g_settings.mode_clock)
				InfoClock->StopClock();

				g_EpgData->show(CNeutrinoApp::getInstance()->channelList->getActiveChannel_ChannelID());

			if(restore) {
				FileTime.show(position / 1000);
				FileTime.updatePos(file_prozent);
			}


		} else if ( msg == NeutrinoMessages::ZAPTO ||
				msg == NeutrinoMessages::STANDBY_ON ||
				msg == NeutrinoMessages::SHUTDOWN ||
				msg == NeutrinoMessages::SLEEPTIMER) {	// Exit for Record/Zapto Timers
			exit = true;
			g_RCInput->postMsg(msg, data);
		} else {
			if (CNeutrinoApp::getInstance()->handleMsg(msg, data) & messages_return::cancel_all)
				exit = true;
			else if ( msg <= CRCInput::RC_MaxRC ) {
				CVFD::getInstance()->setMode(CVFD::MODE_MENU_UTF8);
				update_lcd = true;
			}
		}

		if (exit) {
printf("CMoviePlayerGui::PlayFile: exit, isMovieBrowser %d p_movie_info %x\n", isMovieBrowser, (int) p_movie_info);
			if (isMovieBrowser == true && p_movie_info != NULL) {
				// if we have a movie information, try to save the stop position
				ftime(&current_time);
				p_movie_info->dateOfLastPlay = current_time.time;
				current_time.time = time(NULL);
				p_movie_info->bookmarks.lastPlayStop = position / 1000;

				cMovieInfo.saveMovieInfo(*p_movie_info);
				//p_movie_info->fileInfoStale(); //TODO: we might to tell the Moviebrowser that the movie info has changed, but this could cause long reload times  when reentering the Moviebrowser
			}
		}
	} while (playstate >= CMoviePlayerGui::PLAY);

	FileTime.hide();

	playback->SetSpeed(1);
	playback->Close();

	CVFD::getInstance()->ShowIcon(VFD_ICON_PLAY, false);
	CVFD::getInstance()->ShowIcon(VFD_ICON_PAUSE, false);

	if (was_file) {
		restoreNeutrino();
		open_filebrowser = true;
		start_play = true;
		goto go_repeat;
	}

	if (g_settings.mode_clock)
		InfoClock->StartClock();
}

void CMoviePlayerGui::callInfoViewer(const std::string & epg_title, const std::string & epg_info1,
				     const std::string & epg_channel, const int duration, const int curr_pos)
{
	if (isMovieBrowser) {
		g_InfoViewer->showMovieTitle(playstate, epg_channel.c_str(), epg_title, epg_info1,
					     duration, curr_pos);
		return;
	}

	/* not moviebrowser => use the filename as title */
	char temp_name[255] = { 0 };
	const char *slash = strrchr(filename, '/');
	if (slash) {
		slash++;
		int len = strlen(slash);
		if (len > 254)
			len = 254;
		for (int i = 0; i < len; i++) {
			if (slash[i] == '_')
				temp_name[i] = ' ';
			else
				temp_name[i] = slash[i];
		}
		temp_name[len] = 0;
	}
	g_InfoViewer->showMovieTitle(playstate, temp_name, "", "", duration, curr_pos);
}

void CMoviePlayerGui::showHelpTS()
{
	Helpbox helpbox;
	helpbox.addLine(NEUTRINO_ICON_BUTTON_RED, g_Locale->getText(LOCALE_MOVIEPLAYER_TSHELP1));
	helpbox.addLine(NEUTRINO_ICON_BUTTON_GREEN, g_Locale->getText(LOCALE_MOVIEPLAYER_TSHELP2));
	helpbox.addLine(NEUTRINO_ICON_BUTTON_YELLOW, g_Locale->getText(LOCALE_MOVIEPLAYER_TSHELP3));
	helpbox.addLine(NEUTRINO_ICON_BUTTON_BLUE, g_Locale->getText(LOCALE_MOVIEPLAYER_TSHELP4));
	helpbox.addLine(NEUTRINO_ICON_BUTTON_DBOX, g_Locale->getText(LOCALE_MOVIEPLAYER_TSHELP5));
	helpbox.addLine(NEUTRINO_ICON_BUTTON_1, g_Locale->getText(LOCALE_MOVIEPLAYER_TSHELP6));
	helpbox.addLine(NEUTRINO_ICON_BUTTON_3, g_Locale->getText(LOCALE_MOVIEPLAYER_TSHELP7));
	helpbox.addLine(NEUTRINO_ICON_BUTTON_4, g_Locale->getText(LOCALE_MOVIEPLAYER_TSHELP8));
	helpbox.addLine(NEUTRINO_ICON_BUTTON_6, g_Locale->getText(LOCALE_MOVIEPLAYER_TSHELP9));
	helpbox.addLine(NEUTRINO_ICON_BUTTON_7, g_Locale->getText(LOCALE_MOVIEPLAYER_TSHELP10));
	helpbox.addLine(NEUTRINO_ICON_BUTTON_9, g_Locale->getText(LOCALE_MOVIEPLAYER_TSHELP11));
	helpbox.addLine(g_Locale->getText(LOCALE_MOVIEPLAYER_TSHELP12));
	helpbox.addLine("Version: $Revision: 1.97 $");
	helpbox.addLine("Movieplayer (c) 2003, 2004 by gagga");
	hide();
	helpbox.show(LOCALE_MESSAGEBOX_INFO);
}

void CMoviePlayerGui::showHelpVLC()
{
	Helpbox helpbox;
	helpbox.addLine(NEUTRINO_ICON_BUTTON_RED, g_Locale->getText(LOCALE_MOVIEPLAYER_VLCHELP1));
	helpbox.addLine(NEUTRINO_ICON_BUTTON_GREEN, g_Locale->getText(LOCALE_MOVIEPLAYER_VLCHELP2));
	helpbox.addLine(NEUTRINO_ICON_BUTTON_YELLOW, g_Locale->getText(LOCALE_MOVIEPLAYER_VLCHELP3));
	helpbox.addLine(NEUTRINO_ICON_BUTTON_BLUE, g_Locale->getText(LOCALE_MOVIEPLAYER_VLCHELP4));
	helpbox.addLine(NEUTRINO_ICON_BUTTON_DBOX, g_Locale->getText(LOCALE_MOVIEPLAYER_VLCHELP5));
	helpbox.addLine(NEUTRINO_ICON_BUTTON_1, g_Locale->getText(LOCALE_MOVIEPLAYER_VLCHELP6));
	helpbox.addLine(NEUTRINO_ICON_BUTTON_3, g_Locale->getText(LOCALE_MOVIEPLAYER_VLCHELP7));
	helpbox.addLine(NEUTRINO_ICON_BUTTON_4, g_Locale->getText(LOCALE_MOVIEPLAYER_VLCHELP8));
	helpbox.addLine(NEUTRINO_ICON_BUTTON_6, g_Locale->getText(LOCALE_MOVIEPLAYER_VLCHELP9));
	helpbox.addLine(NEUTRINO_ICON_BUTTON_7, g_Locale->getText(LOCALE_MOVIEPLAYER_VLCHELP10));
	helpbox.addLine(NEUTRINO_ICON_BUTTON_9, g_Locale->getText(LOCALE_MOVIEPLAYER_VLCHELP11));
	helpbox.addLine(g_Locale->getText(LOCALE_MOVIEPLAYER_VLCHELP12));
	helpbox.addLine("Version: $Revision: 1.97 $");
	helpbox.addLine("Movieplayer (c) 2003, 2004 by gagga");
	hide();
	helpbox.show(LOCALE_MESSAGEBOX_INFO);
}
