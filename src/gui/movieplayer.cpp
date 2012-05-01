/*
  Neutrino-GUI  -   DBoxII-Project

  Movieplayer (c) 2003, 2004 by gagga
  Based on code by Dirch, obi and the Metzler Bros. Thanks.
  (C) 2011 Stefan Seyfried

  Copyright (C) 2011 CoolStream International Ltd

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

#include <global.h>
#include <neutrino.h>

#include <system/safe_system.h>
#include <gui/movieplayer.h>
#include <gui/infoviewer.h>
#include <gui/bookmarkmanager.h>
#include <gui/timeosd.h>
#include <gui/moviebrowser.h>
#include <gui/widget/helpbox.h>
#include <gui/infoclock.h>
#include <gui/plugins.h>
#ifdef SCREENSHOT
#include <driver/screenshot.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <sys/timeb.h>

#include <video.h>
#include "libtuxtxt/teletext.h"
#include <zapit/zapit.h>

//extern CPlugins *g_PluginList;
#ifndef HAVE_COOL_HARDWARE
#define LCD_MODE CVFD::MODE_MOVIE
#else
#define LCD_MODE CVFD::MODE_MENU_UTF8
#endif

extern cVideo * videoDecoder;
extern CRemoteControl *g_RemoteControl;	/* neutrino.cpp */
extern CInfoClock *InfoClock;
extern bool has_hdd;

#define TIMESHIFT_SECONDS 3

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

CMoviePlayerGui::CMoviePlayerGui()
{
	Init();
}

CMoviePlayerGui::~CMoviePlayerGui()
{
	playback->Close();
	delete moviebrowser;
	delete filebrowser;
	delete bookmarkmanager;
	delete playback;
	instance_mp = NULL;
}

void CMoviePlayerGui::Init(void)
{
	playing = false;

	frameBuffer = CFrameBuffer::getInstance();

	playback = new cPlayback(3);
	moviebrowser = new CMovieBrowser();
	bookmarkmanager = new CBookmarkManager();

	tsfilefilter.addFilter("ts");
#if HAVE_TRIPLEDRAGON
	tsfilefilter.addFilter("vdr");
#else
	tsfilefilter.addFilter("avi");
	tsfilefilter.addFilter("mkv");
	tsfilefilter.addFilter("wav");
	tsfilefilter.addFilter("asf");
	tsfilefilter.addFilter("aiff");
#endif
	tsfilefilter.addFilter("mpg");
	tsfilefilter.addFilter("mpeg");
	tsfilefilter.addFilter("m2p");
	tsfilefilter.addFilter("mpv");
	tsfilefilter.addFilter("vob");
	tsfilefilter.addFilter("m2ts");
	tsfilefilter.addFilter("mp4");
	tsfilefilter.addFilter("mov");
#ifdef HAVE_SPARK_HARDWARE
	tsfilefilter.addFilter("vdr");
	tsfilefilter.addFilter("flv");
	tsfilefilter.addFilter("wmv");
#endif

	if (strlen(g_settings.network_nfs_moviedir) != 0)
		Path_local = g_settings.network_nfs_moviedir;
	else
		Path_local = "/";

	if (g_settings.filebrowser_denydirectoryleave)
		filebrowser = new CFileBrowser(Path_local.c_str());
	else
		filebrowser = new CFileBrowser();

	filebrowser->Filter = &tsfilefilter;
	filebrowser->Hide_records = true;

	speed = 1;
	timeshift = 0;
	numpida = 0;
}

void CMoviePlayerGui::cutNeutrino()
{
	if (playing)
		return;

	playing = true;
	g_Zapit->lockPlayBack();
	g_Sectionsd->setPauseScanning(true);

	CNeutrinoApp::getInstance()->handleMsg(NeutrinoMessages::CHANGEMODE, NeutrinoMessages::mode_ts);
	m_LastMode = (CNeutrinoApp::getInstance()->getLastMode() | NeutrinoMessages::norezap);
}

void CMoviePlayerGui::restoreNeutrino()
{
	if (!playing)
		return;

	playing = false;

	g_Zapit->unlockPlayBack();
	g_Sectionsd->setPauseScanning(false);

	CNeutrinoApp::getInstance()->handleMsg(NeutrinoMessages::CHANGEMODE, m_LastMode);
}

int CMoviePlayerGui::exec(CMenuTarget * parent, const std::string & actionKey)
{
	printf("[movieplayer] actionKey=%s\n", actionKey.c_str());

	if (parent)
		parent->hide();

	file_name = "";
	full_name = "";

	startposition = 0;

	puts("[movieplayer.cpp] executing " MOVIEPLAYER_START_SCRIPT ".");
	if (safe_system(MOVIEPLAYER_START_SCRIPT) != 0)
		perror(MOVIEPLAYER_START_SCRIPT " failed");
	
	isMovieBrowser = false;
	isBookmark = false;
	timeshift = 0;
	if (actionKey == "tsmoviebrowser") {
		isMovieBrowser = true;
	}
	else if (actionKey == "fileplayback") {
	}
	else if (actionKey == "timeshift") {
		timeshift = 1;
	}
	else if (actionKey == "ptimeshift") {
		timeshift = 2;
	}
	else if (actionKey == "rtimeshift") {
		timeshift = 3;
	}
#if 0 // TODO ?
	else if (actionKey == "bookmarkplayback") {
		isBookmark = true;
	}
#endif
	else {
		return menu_return::RETURN_REPAINT;
	}

	while(SelectFile()) {
		PlayFile();
		if(timeshift)
			break;
	}

	bookmarkmanager->flush();

	puts("[movieplayer.cpp] executing " MOVIEPLAYER_END_SCRIPT ".");
	if (safe_system(MOVIEPLAYER_END_SCRIPT) != 0)
		perror(MOVIEPLAYER_END_SCRIPT " failed");

	CVFD::getInstance()->setMode(CVFD::MODE_TVRADIO);

	if (timeshift){
		timeshift = 0;
		return menu_return::RETURN_EXIT_ALL;
	}
	return menu_return::RETURN_REPAINT;
}

void CMoviePlayerGui::updateLcd()
{
	char tmp[20];
	std::string lcd;
	std::string name;

	if (isMovieBrowser && strlen(p_movie_info->epgTitle.c_str()) && strncmp(p_movie_info->epgTitle.c_str(), "not", 3))
		name = p_movie_info->epgTitle;
	else
		name = file_name;

	switch (playstate) {
		case CMoviePlayerGui::PAUSE:
			lcd = "|| ";
			break;
		case CMoviePlayerGui::REW:
			sprintf(tmp, "%dx<< ", speed);
			lcd = tmp;
			break;
		case CMoviePlayerGui::FF:
			sprintf(tmp, "%dx>> ", speed);
			lcd = tmp;
			break;
		case CMoviePlayerGui::PLAY:
			lcd = "> ";
			break;
		default:
			break;
	}
	lcd += name;
	CVFD::getInstance()->setMode(LCD_MODE);
	CVFD::getInstance()->showMenuText(0, lcd.c_str(), -1, true);
}

void CMoviePlayerGui::fillPids()
{
	if(p_movie_info == NULL)
		return;

	numpida = 0; currentapid = 0;
	if(!p_movie_info->audioPids.empty()) {
		currentapid = p_movie_info->audioPids[0].epgAudioPid;
		currentac3 = p_movie_info->audioPids[0].atype;
	}
	for (int i = 0; i < (int)p_movie_info->audioPids.size(); i++) {
		apids[i] = p_movie_info->audioPids[i].epgAudioPid;
		ac3flags[i] = p_movie_info->audioPids[i].atype;
		numpida++;
		if (p_movie_info->audioPids[i].selected) {
			currentapid = p_movie_info->audioPids[i].epgAudioPid;
			currentac3 = p_movie_info->audioPids[i].atype;
		}
	}
	vpid = p_movie_info->epgVideoPid;
	vtype = p_movie_info->VideoType;
}

bool CMoviePlayerGui::SelectFile()
{
	bool ret = false;

	/*clear audiopids */
	for (int i = 0; i < numpida; i++) {
		apids[i] = 0;
		ac3flags[i] = 0;
		language[i].clear();
	}
	numpida = 0; currentapid = 0;

	is_file_player = false;
	p_movie_info = NULL;

	printf("CMoviePlayerGui::SelectFile: isBookmark %d timeshift %d isMovieBrowser %d\n", isBookmark, timeshift, isMovieBrowser);
	if (has_hdd)
		system("(rm /hdd/.wakeup; touch /hdd/.wakeup; sync) > /dev/null  2> /dev/null &");

	if (timeshift) {
		t_channel_id live_channel_id = CZapit::getInstance()->GetCurrentChannelID();
		p_movie_info = CRecordManager::getInstance()->GetMovieInfo(live_channel_id);
		full_name = CRecordManager::getInstance()->GetFileName(live_channel_id) + ".ts";
		fillPids();
		ret = true;
	}
#if 0 // TODO
	else if(isBookmark) {
		const CBookmark * theBookmark = bookmarkmanager->getBookmark(NULL);
		if (theBookmark == NULL) {
			bookmarkmanager->flush();
			return false;
		}
		full_name = theBookmark->getUrl();
		sscanf(theBookmark->getTime(), "%lld", &startposition);
		startposition *= 1000;
		ret = true;
	}
#endif
	else if (isMovieBrowser) {
		if (moviebrowser->exec(Path_local.c_str())) {
			// get the current path and file name
			Path_local = moviebrowser->getCurrentDir();
			CFile *file;
			if ((file = moviebrowser->getSelectedFile()) != NULL) {
				full_name = file->Name.c_str();

				// get the movie info handle (to be used for e.g. bookmark handling)
				p_movie_info = moviebrowser->getCurrentMovieInfo();
#if 0
				bool recfile = CRecordManager::getInstance()->RecordingStatus(p_movie_info->epgId) &&
					CRecordManager::getInstance()->GetFileName(p_movie_info->epgId) == file->Name;
#endif
				fillPids();
				// get the start position for the movie
				startposition = 1000 * moviebrowser->getCurrentStartPos();
				printf("CMoviePlayerGui::SelectFile: file %s start %d apid %X atype %d vpid %x vtype %d\n", full_name.c_str(), startposition, currentapid, currentac3, vpid, vtype);

				ret = true;
			}
		}
	} 
	else { // filebrowser
		if (filebrowser->exec(Path_local.c_str()) == true) {
			Path_local = filebrowser->getCurrentDir();
			CFile *file;
			if ((file = filebrowser->getSelectedFile()) != NULL) {
				is_file_player = true;
				full_name = file->Name.c_str();
				ret = true;
			}
		}
	}
	if(ret) {
		std::string::size_type pos = full_name.find_last_of('/');
		if(pos != std::string::npos) {
			file_name = full_name.substr(pos+1);
			std::replace(file_name.begin(), file_name.end(), '_', ' ');
		} else
			file_name = full_name;
		printf("CMoviePlayerGui::SelectFile: full_name [%s] file_name [%s]\n", full_name.c_str(), file_name.c_str());
	}
	//store last multiformat play dir
	if( (sizeof(g_settings.network_nfs_moviedir)) > Path_local.size() && (strcmp(g_settings.network_nfs_moviedir,Path_local.c_str()) != 0)){
		strcpy(g_settings.network_nfs_moviedir,Path_local.c_str());
	}

	return ret;
}

void CMoviePlayerGui::PlayFile(void)
{
	neutrino_msg_t msg;
	neutrino_msg_data_t data;

	int position = 0, duration = 0;
	bool first_start_timeshift = false;
	bool time_forced = false;
	bool update_lcd = true;
	int eof = 0;

	CTimeOSD FileTime;

	playstate = CMoviePlayerGui::STOPPED;
	printf("Startplay at %d seconds\n", startposition/1000);
	handleMovieBrowser(CRCInput::RC_nokey, position);

	cutNeutrino();
	playback->Open(is_file_player ? PLAYMODE_FILE : PLAYMODE_TS);

	printf("IS FILE PLAYER: %s\n", is_file_player ?  "true": "false" );

	if(p_movie_info != NULL)
		duration = p_movie_info->length * 60 * 1000;

	file_prozent = 0;
	if(!playback->Start((char *) full_name.c_str(), vpid, vtype, currentapid, currentac3, duration)) {
		playback->Close();
	} else {
		playstate = CMoviePlayerGui::PLAY;
		CVFD::getInstance()->ShowIcon(VFD_ICON_PLAY, true);
		if(timeshift) {
			first_start_timeshift = true;
			startposition = -1;
			int i;
			int towait = (timeshift == 1) ? TIMESHIFT_SECONDS+1 : TIMESHIFT_SECONDS;
			for(i = 0; i < 500; i++) {
				playback->GetPosition(position, duration);
				startposition = (duration - position);

				//printf("CMoviePlayerGui::PlayFile: waiting for data, position %d duration %d (%d), start %d\n", position, duration, towait, startposition);
				if(startposition > towait*1000)
					break;

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

		/* playback->Start() starts paused */
		if(timeshift == 3) {
			playback->SetSpeed(-1);
		} else if(!timeshift || !g_settings.timeshift_pause) {
			playback->SetSpeed(1);
		}
	}

	while (playstate >= CMoviePlayerGui::PLAY)
	{
		if (update_lcd) {
			update_lcd = false;
			updateLcd();
		}
		if (first_start_timeshift) {
			callInfoViewer(duration, position);
			first_start_timeshift = false;
		}

		g_RCInput->getMsg(&msg, &data, 10);	// 1 secs..

		if ((playstate >= CMoviePlayerGui::PLAY) && (timeshift || (playstate != CMoviePlayerGui::PAUSE))) {
			if(playback->GetPosition(position, duration)) {
				if(duration > 100)
					file_prozent = (unsigned char) (position / (duration / 100));
#if HAVE_TRIPLEDRAGON
				CVFD::getInstance()->showPercentOver(file_prozent, true, CVFD::MODE_MOVIE);
#endif

				playback->GetSpeed(speed);
#ifdef DEBUG
				printf("CMoviePlayerGui::PlayFile: speed %d position %d duration %d (%d, %d%%)\n", speed, position, duration, duration-position, file_prozent);
#endif
				if (duration - position < 1000 && !timeshift)
				{
					/* 10 seconds after end-of-file, stop */
					if (++eof > 10)
						g_RCInput->postMsg((neutrino_msg_t) g_settings.mpkey_stop, 0);
				}
				else
					eof = 0;
			}
			handleMovieBrowser(0, position);
			FileTime.update(position, duration);
		}

		if (msg == (neutrino_msg_t) g_settings.mpkey_plugin) {
			//g_PluginList->start_plugin_by_name (g_settings.movieplayer_plugin.c_str (), pidt);
		} else if (msg == (neutrino_msg_t) g_settings.mpkey_stop) {
			playstate = CMoviePlayerGui::STOPPED;
		} else if (msg == (neutrino_msg_t) g_settings.mpkey_play) {
			if (playstate > CMoviePlayerGui::PLAY) {
				update_lcd = true;
				playstate = CMoviePlayerGui::PLAY;
				speed = 1;
				playback->SetSpeed(speed);
				if (!timeshift)
					callInfoViewer(duration, position);
			}
			if (time_forced) {
				time_forced = false;
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
				callInfoViewer(duration, position);

		} else if (msg == (neutrino_msg_t) g_settings.mpkey_bookmark) {
			handleMovieBrowser((neutrino_msg_t) g_settings.mpkey_bookmark, position);
		} else if (msg == (neutrino_msg_t) g_settings.mpkey_audio) {
			selectAudioPid(is_file_player);
			update_lcd = true;
		} else if (msg == (neutrino_msg_t) g_settings.mpkey_time) {
			FileTime.switchMode(position, duration);
		} else if ((msg == (neutrino_msg_t) g_settings.mpkey_rewind) ||
				(msg == (neutrino_msg_t) g_settings.mpkey_forward)) {

			if (msg == (neutrino_msg_t) g_settings.mpkey_rewind) {
				speed = (speed >= 0) ? -1 : speed - 1;
				playstate = CMoviePlayerGui::REW;
			} else {
				speed = (speed <= 0) ? 2 : speed + 1;
				playstate = CMoviePlayerGui::FF;
			}
			/* if paused, playback->SetSpeed() start slow motion */
			playback->SetSpeed(speed);
			update_lcd = true;

			if (!timeshift)
				callInfoViewer(duration, position);

			if (!FileTime.IsVisible()) {
				FileTime.show(position);
				time_forced = true;
			}
		} else if (msg == CRCInput::RC_1) {	// Jump Backwards 1 minute
			playback->SetPosition(-60 * 1000);
		} else if (msg == CRCInput::RC_3) {	// Jump Forward 1 minute
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
			handleMovieBrowser(CRCInput::RC_0, position);
		} else if (msg == CRCInput::RC_help || msg == CRCInput::RC_info) {
			callInfoViewer(duration, position);
			update_lcd = true;
			//showHelpTS();
		} else if(timeshift && (msg == CRCInput::RC_text || msg == CRCInput::RC_epg || msg == NeutrinoMessages::SHOW_EPG)) {
			bool restore = FileTime.IsVisible();
			FileTime.hide();

			if( msg == CRCInput::RC_epg )
				g_EventList->exec(CNeutrinoApp::getInstance()->channelList->getActiveChannel_ChannelID(), CNeutrinoApp::getInstance()->channelList->getActiveChannelName());
			else if(msg == NeutrinoMessages::SHOW_EPG)
				g_EpgData->show(CNeutrinoApp::getInstance()->channelList->getActiveChannel_ChannelID());
			else {
				if(g_settings.cacheTXT)
					tuxtxt_stop();
				tuxtx_main(g_RCInput->getFileHandle(), g_RemoteControl->current_PIDs.PIDs.vtxtpid, 0, 2);
				frameBuffer->paintBackground();
			}
			if(restore)
				FileTime.show(position);
#ifdef SCREENSHOT
		} else if (msg == (neutrino_msg_t) g_settings.key_screenshot) {

			char ending[(sizeof(int)*2) + 6] = ".jpg";
			if(!g_settings.screenshot_cover)
				snprintf(ending, sizeof(ending) - 1, "_%x.jpg", position);

			std::string fname = full_name;
			std::string::size_type pos = fname.find_last_of('.');
			if(pos != std::string::npos) {
				fname.replace(pos, fname.length(), ending);
			} else
				fname += ending;

			if(!g_settings.screenshot_cover){
				pos = fname.find_last_of('/');
				if(pos != std::string::npos) {
					fname.replace(0, pos, g_settings.screenshot_dir);
				}
			}

#if 0 // TODO disable overwrite ?
			if(!access(fname.c_str(), F_OK)) {
			}
#endif
			CScreenShot * sc = new CScreenShot(fname);
			if(g_settings.screenshot_cover && !g_settings.screenshot_video)
				sc->EnableVideo(true);
			sc->Start();
#endif

		} else if ( msg == NeutrinoMessages::ANNOUNCE_RECORD ||
				msg == NeutrinoMessages::RECORD_START) {
			CNeutrinoApp::getInstance()->handleMsg(msg, data);
		} else if ( msg == NeutrinoMessages::ZAPTO ||
				msg == NeutrinoMessages::STANDBY_ON ||
				msg == NeutrinoMessages::SHUTDOWN ||
				msg == NeutrinoMessages::SLEEPTIMER) {	// Exit for Record/Zapto Timers
			printf("CMoviePlayerGui::PlayFile: ZAPTO etc..\n");
			playstate = CMoviePlayerGui::STOPPED;
			g_RCInput->postMsg(msg, data);
		} else if (msg == CRCInput::RC_timeout) {
			// nothing
		} else if (msg == CRCInput::RC_sat || msg == CRCInput::RC_favorites) {
			//FIXME do nothing ?
		} else {
			if (CNeutrinoApp::getInstance()->handleMsg(msg, data) & messages_return::cancel_all) {
				printf("CMoviePlayerGui::PlayFile: neutrino handleMsg messages_return::cancel_all\n");
				playstate = CMoviePlayerGui::STOPPED;
			}
			else if ( msg <= CRCInput::RC_MaxRC ) {
				update_lcd = true;
			}
		}

		if (playstate == CMoviePlayerGui::STOPPED) {
			printf("CMoviePlayerGui::PlayFile: exit, isMovieBrowser %d p_movie_info %x\n", isMovieBrowser, (int) p_movie_info);
			playstate = CMoviePlayerGui::STOPPED;
			handleMovieBrowser((neutrino_msg_t) g_settings.mpkey_stop, position);
		}
	}

	FileTime.hide();

	playback->SetSpeed(1);
	playback->Close();

	CVFD::getInstance()->ShowIcon(VFD_ICON_PLAY, false);
	CVFD::getInstance()->ShowIcon(VFD_ICON_PAUSE, false);

	restoreNeutrino();

	if (g_settings.mode_clock)
		InfoClock->StartClock();
}

void CMoviePlayerGui::callInfoViewer(const int duration, const int curr_pos)
{
	if(timeshift) {
		g_InfoViewer->showTitle(CNeutrinoApp::getInstance()->channelList->getActiveChannelNumber(),
				CNeutrinoApp::getInstance()->channelList->getActiveChannelName(),
				CNeutrinoApp::getInstance()->channelList->getActiveSatellitePosition(),
				CNeutrinoApp::getInstance()->channelList->getActiveChannel_ChannelID());
		return;
	}
	if (isMovieBrowser && p_movie_info) {
		g_InfoViewer->showMovieTitle(playstate, p_movie_info->epgChannel, p_movie_info->epgTitle, p_movie_info->epgInfo1,
					     duration, curr_pos);
		return;
	}

	/* not moviebrowser => use the filename as title */
	g_InfoViewer->showMovieTitle(playstate, file_name, "", "", duration, curr_pos);
}

bool CMoviePlayerGui::getAudioName(int apid, std::string &apidtitle)
{
	if (p_movie_info == NULL)
		return false;

	for (int i = 0; i < (int)p_movie_info->audioPids.size(); i++) {
		if (p_movie_info->audioPids[i].epgAudioPid == apid && !p_movie_info->audioPids[i].epgAudioPidName.empty()) {
			apidtitle = p_movie_info->audioPids[i].epgAudioPidName;
			return true;
		}
	}
	return false;
}

void CMoviePlayerGui::selectAudioPid(bool file_player)
{
	CMenuWidget APIDSelector(LOCALE_APIDSELECTOR_HEAD, NEUTRINO_ICON_AUDIO);
	APIDSelector.addIntroItems();

	int select = -1;
	CMenuSelectorTarget * selector = new CMenuSelectorTarget(&select);

	if(file_player && !numpida){
		playback->FindAllPids(apids, ac3flags, &numpida, language);
		/* fix current pid in case of file play */
		if(numpida)
			currentapid = apids[0];
	}
	for (unsigned int count = 0; count < numpida; count++) {
		bool name_ok = false;
		bool enabled = true;
		bool defpid = currentapid ? (currentapid == apids[count]) : (count == 0);
		std::string apidtitle;

		if(!file_player){
			name_ok = getAudioName(apids[count], apidtitle);
		}
		else if (!language[count].empty()){
			apidtitle = language[count];
			name_ok = true;
		}
		if (!name_ok) {
			char apidnumber[20];
			sprintf(apidnumber, "Stream %d %X", count + 1, apids[count]);
			apidtitle = apidnumber;
		}

		switch(ac3flags[count])
		{
			case 1: /*AC3,EAC3*/
				if (apidtitle.find("AC3") == std::string::npos || file_player)
					apidtitle.append(" (AC3)");
				break;
			case 2: /*teletext*/
				apidtitle.append(" (Teletext)");
				enabled = false;
				break;
			case 3: /*MP2*/
				apidtitle.append("( MP2)");
				break;
			case 4: /*MP3*/
				apidtitle.append(" (MP3)");
				break;
			case 5: /*AAC*/
				apidtitle.append(" (AAC)");
				break;
			case 6: /*DTS*/
				apidtitle.append(" (DTS)");
#ifndef HAVE_SPARK_HARDWARE
				enabled = false;
#endif
				break;
			case 7: /*MLP*/
				apidtitle.append(" (MLP)");
				break;
			default:
				break;
		}
		char cnt[5];
		sprintf(cnt, "%d", count);
		CMenuForwarderNonLocalized * item = new CMenuForwarderNonLocalized(apidtitle.c_str(), enabled, NULL, selector, cnt, CRCInput::convertDigitToKey(count + 1));
		APIDSelector.addItem(item, defpid);
	}

	APIDSelector.exec(NULL, "");
	delete selector;
	printf("CMoviePlayerGui::selectAudioPid: selected %d (%x) current %x\n", select, (select >= 0) ? apids[select] : -1, currentapid);
	if((select >= 0) && (currentapid != apids[select])) {
		currentapid = apids[select];
		currentac3 = ac3flags[select];
		playback->SetAPid(currentapid, currentac3);
		printf("[movieplayer] apid changed to %d type %d\n", currentapid, currentac3);
	}
}

void CMoviePlayerGui::handleMovieBrowser(neutrino_msg_t msg, int position)
{
	CMovieInfo cMovieInfo;	// funktions to save and load movie info

	static int jump_not_until = 0;	// any jump shall be avoided until this time (in seconds from moviestart)
	static MI_BOOKMARK new_bookmark;	// used for new movie info bookmarks created from the movieplayer

	static int width = 280;
	static int height = 65;

	static int x = frameBuffer->getScreenX() + (frameBuffer->getScreenWidth() - width) / 2;
	static int y = frameBuffer->getScreenY() + frameBuffer->getScreenHeight() - height - 20;

	static CBox boxposition(x, y, width, height);	// window position for the hint boxes

	static CTextBox endHintBox(g_Locale->getText(LOCALE_MOVIEBROWSER_HINT_MOVIEEND), NULL, CTextBox::CENTER /*CTextBox::AUTO_WIDTH | CTextBox::AUTO_HIGH */ , &boxposition);
	static CTextBox comHintBox(g_Locale->getText(LOCALE_MOVIEBROWSER_HINT_JUMPFORWARD), NULL, CTextBox::CENTER /*CTextBox::AUTO_WIDTH | CTextBox::AUTO_HIGH */ , &boxposition);
	static CTextBox loopHintBox(g_Locale->getText(LOCALE_MOVIEBROWSER_HINT_JUMPBACKWARD), NULL, CTextBox::CENTER /*CTextBox::AUTO_WIDTH | CTextBox::AUTO_HIGH */ , &boxposition);
	static CTextBox newLoopHintBox(g_Locale->getText(LOCALE_MOVIEBROWSER_HINT_NEWBOOK_BACKWARD), NULL, CTextBox::CENTER /*CTextBox::AUTO_WIDTH | CTextBox::AUTO_HIGH */ , &boxposition);
	static CTextBox newComHintBox(g_Locale->getText(LOCALE_MOVIEBROWSER_HINT_NEWBOOK_FORWARD), NULL, CTextBox::CENTER /*CTextBox::AUTO_WIDTH | CTextBox::AUTO_HIGH */ , &boxposition);

	static bool showEndHintBox = false;	// flag to check whether the box shall be painted
	static bool showComHintBox = false;	// flag to check whether the box shall be painted
	static bool showLoopHintBox = false;	// flag to check whether the box shall be painted

	int play_sec = position / 1000;	// get current seconds from moviestart
	if(msg == CRCInput::RC_nokey) {
		printf("CMoviePlayerGui::handleMovieBrowser: reset vars\n");
		// reset statics
		jump_not_until = 0;
		showEndHintBox = showComHintBox = showLoopHintBox = false;
		new_bookmark.pos = 0;
		// move in case osd position changed
		int newx = frameBuffer->getScreenX() + (frameBuffer->getScreenWidth() - width) / 2;
		int newy = frameBuffer->getScreenY() + frameBuffer->getScreenHeight() - height - 20;
		endHintBox.movePosition(newx, newy);
		comHintBox.movePosition(newx, newy);
		loopHintBox.movePosition(newx, newy);
		newLoopHintBox.movePosition(newx, newy);
		newComHintBox.movePosition(newx, newy);
		return;
	}
	else if (msg == (neutrino_msg_t) g_settings.mpkey_stop) {
		// if we have a movie information, try to save the stop position
		printf("CMoviePlayerGui::handleMovieBrowser: stop, isMovieBrowser %d p_movie_info %x\n", isMovieBrowser, (int) p_movie_info);
		if (isMovieBrowser && p_movie_info) {
			timeb current_time;
			ftime(&current_time);
			p_movie_info->dateOfLastPlay = current_time.time;
			current_time.time = time(NULL);
			p_movie_info->bookmarks.lastPlayStop = position / 1000;
			cMovieInfo.saveMovieInfo(*p_movie_info);
			//p_movie_info->fileInfoStale(); //TODO: we might to tell the Moviebrowser that the movie info has changed, but this could cause long reload times  when reentering the Moviebrowser
		}
	}
	else if((msg == 0) && isMovieBrowser && (playstate == CMoviePlayerGui::PLAY) && p_movie_info) {
		if (play_sec + 10 < jump_not_until || play_sec > jump_not_until + 10)
			jump_not_until = 0;	// check if !jump is stale (e.g. if user jumped forward or backward)

		// do bookmark activities only, if there is no new bookmark started
		if (new_bookmark.pos != 0)
			return;
#ifdef DEBUG
		//printf("CMoviePlayerGui::handleMovieBrowser: process bookmarks\n");
#endif
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
				TRACE("[mp]  user stop: play_sec %d bookmarks.end %d jump_not_until %d\n", play_sec, p_movie_info->bookmarks.end, jump_not_until);
				playstate = CMoviePlayerGui::STOPPED;
				return;
			}
		}
		// *************  Check for bookmark jumps *******************************
		showLoopHintBox = false;
		showComHintBox = false;
		for (int book_nr = 0; book_nr < MI_MOVIE_BOOK_USER_MAX; book_nr++) {
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
					int jumpseconds = p_movie_info->bookmarks.user[book_nr].length;

					// we are close behind the bookmark, do bookmark activity (if any)
					if (p_movie_info->bookmarks.user[book_nr].length < 0) {
						// if the jump back time is to less, it does sometimes cause problems (it does probably jump only 5 sec which will cause the next jump, and so on)
						if (jumpseconds > -15)
							jumpseconds = -15;

						playback->SetPosition(jumpseconds * 1000);
					} else if (p_movie_info->bookmarks.user[book_nr].length > 0) {
						// jump at least 15 seconds
						if (jumpseconds < 15)
							jumpseconds = 15;

						playback->SetPosition(jumpseconds * 1000);
					}
					TRACE("[mp]  do jump %d sec\r\n", jumpseconds);
					break;	// do no further bookmark checks
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

		return;
	} else if (msg == CRCInput::RC_0) {	// cancel bookmark jump
		printf("CMoviePlayerGui::handleMovieBrowser: CRCInput::RC_0\n");
		if (isMovieBrowser == true) {
			if (new_bookmark.pos != 0) {
				new_bookmark.pos = 0;	// stop current bookmark activity, TODO:  might bemoved to another key
				newLoopHintBox.hide();	// hide hint box if any
				newComHintBox.hide();
			}
			comHintBox.hide();
			loopHintBox.hide();
			jump_not_until = (position / 1000) + 10; // avoid bookmark jumping for the next 10 seconds, , TODO:  might be moved to another key
		}
		return;
	} 
	else if (msg == (neutrino_msg_t) g_settings.mpkey_bookmark) {
		if (newComHintBox.isPainted() == true) {
			// yes, let's get the end pos of the jump forward
			new_bookmark.length = play_sec - new_bookmark.pos;
			TRACE("[mp] commercial length: %d\r\n", new_bookmark.length);
			if (cMovieInfo.addNewBookmark(p_movie_info, new_bookmark) == true) {
				cMovieInfo.saveMovieInfo(*p_movie_info);	/* save immediately in xml file */
			}
			new_bookmark.pos = 0;	// clear again, since this is used as flag for bookmark activity
			newComHintBox.hide();
		} else if (newLoopHintBox.isPainted() == true) {
			// yes, let's get the end pos of the jump backward
			new_bookmark.length = new_bookmark.pos - play_sec;
			new_bookmark.pos = play_sec;
			TRACE("[mp] loop length: %d\r\n", new_bookmark.length);
			if (cMovieInfo.addNewBookmark(p_movie_info, new_bookmark) == true) {
				cMovieInfo.saveMovieInfo(*p_movie_info);	/* save immediately in xml file */
				jump_not_until = play_sec + 5;	// avoid jumping for this time
			}
			new_bookmark.pos = 0;	// clear again, since this is used as flag for bookmark activity
			newLoopHintBox.hide();
		} else {
			// very dirty usage of the menue, but it works and I already spent to much time with it, feel free to make it better ;-)
#define BOOKMARK_START_MENU_MAX_ITEMS 6
			CSelectedMenu cSelectedMenuBookStart[BOOKMARK_START_MENU_MAX_ITEMS];

			CMenuWidget bookStartMenu(LOCALE_MOVIEBROWSER_BOOK_NEW, NEUTRINO_ICON_STREAMING);
			bookStartMenu.addIntroItems();
#if 0 // not supported, TODO
			bookStartMenu.addItem(new CMenuForwarder(LOCALE_MOVIEPLAYER_HEAD, !isMovieBrowser, NULL, &cSelectedMenuBookStart[0]));
			bookStartMenu.addItem(GenericMenuSeparatorLine);
#endif
			bookStartMenu.addItem(new CMenuForwarder(LOCALE_MOVIEBROWSER_HEAD, isMovieBrowser, NULL, &cSelectedMenuBookStart[1]));
			bookStartMenu.addItem(new CMenuForwarder(LOCALE_MOVIEBROWSER_BOOK_TYPE_FORWARD, isMovieBrowser, NULL, &cSelectedMenuBookStart[2]));
			bookStartMenu.addItem(new CMenuForwarder(LOCALE_MOVIEBROWSER_BOOK_TYPE_BACKWARD, isMovieBrowser, NULL, &cSelectedMenuBookStart[3]));
			bookStartMenu.addItem(new CMenuForwarder(LOCALE_MOVIEBROWSER_BOOK_MOVIESTART, isMovieBrowser, NULL, &cSelectedMenuBookStart[4]));
			bookStartMenu.addItem(new CMenuForwarder(LOCALE_MOVIEBROWSER_BOOK_MOVIEEND, isMovieBrowser, NULL, &cSelectedMenuBookStart[5]));

			// no, nothing else to do, we open a new bookmark menu
			new_bookmark.name = "";	// use default name
			new_bookmark.pos = 0;
			new_bookmark.length = 0;

			// next seems return menu_return::RETURN_EXIT, if something selected
			bookStartMenu.exec(NULL, "none");
#if 0 // not supported, TODO
			if (cSelectedMenuBookStart[0].selected == true) {
				/* Movieplayer bookmark */
				if (bookmarkmanager->getBookmarkCount() < bookmarkmanager->getMaxBookmarkCount()) {
					char timerstring[200];
					sprintf(timerstring, "%lld", play_sec);
					std::string bookmarktime = timerstring;
					fprintf(stderr, "fileposition: %lld timerstring: %s bookmarktime: %s\n", play_sec, timerstring, bookmarktime.c_str());
					bookmarkmanager->createBookmark(full_name, bookmarktime);
				} else {
					fprintf(stderr, "too many bookmarks\n");
					DisplayErrorMessage(g_Locale->getText(LOCALE_MOVIEPLAYER_TOOMANYBOOKMARKS));	// UTF-8
				}
				cSelectedMenuBookStart[0].selected = false;	// clear for next bookmark menu
			} else 
#endif
			if (cSelectedMenuBookStart[1].selected == true) {
				/* Moviebrowser plain bookmark */
				new_bookmark.pos = play_sec;
				new_bookmark.length = 0;
				if (cMovieInfo.addNewBookmark(p_movie_info, new_bookmark) == true)
					cMovieInfo.saveMovieInfo(*p_movie_info);	/* save immediately in xml file */
				new_bookmark.pos = 0;	// clear again, since this is used as flag for bookmark activity
				cSelectedMenuBookStart[1].selected = false;	// clear for next bookmark menu
			} else if (cSelectedMenuBookStart[2].selected == true) {
				/* Moviebrowser jump forward bookmark */
				new_bookmark.pos = play_sec;
				TRACE("[mp] new bookmark 1. pos: %d\r\n", new_bookmark.pos);
				newComHintBox.paint();
				cSelectedMenuBookStart[2].selected = false;	// clear for next bookmark menu
			} else if (cSelectedMenuBookStart[3].selected == true) {
				/* Moviebrowser jump backward bookmark */
				new_bookmark.pos = play_sec;
				TRACE("[mp] new bookmark 1. pos: %d\r\n", new_bookmark.pos);
				newLoopHintBox.paint();
				cSelectedMenuBookStart[3].selected = false;	// clear for next bookmark menu
			} else if (cSelectedMenuBookStart[4].selected == true) {
				/* Moviebrowser movie start bookmark */
				p_movie_info->bookmarks.start = play_sec;
				TRACE("[mp] New movie start pos: %d\r\n", p_movie_info->bookmarks.start);
				cMovieInfo.saveMovieInfo(*p_movie_info);	/* save immediately in xml file */
				cSelectedMenuBookStart[4].selected = false;	// clear for next bookmark menu
			} else if (cSelectedMenuBookStart[5].selected == true) {
				/* Moviebrowser movie end bookmark */
				p_movie_info->bookmarks.end = play_sec;
				TRACE("[mp]  New movie end pos: %d\r\n", p_movie_info->bookmarks.start);
				cMovieInfo.saveMovieInfo(*p_movie_info);	/* save immediately in xml file */
				cSelectedMenuBookStart[5].selected = false;	// clear for next bookmark menu
			}
		}
	}
	return;
}

void CMoviePlayerGui::showHelpTS()
{
	Helpbox helpbox;
	helpbox.addLine(NEUTRINO_ICON_BUTTON_RED, g_Locale->getText(LOCALE_MOVIEPLAYER_TSHELP1));
	helpbox.addLine(NEUTRINO_ICON_BUTTON_GREEN, g_Locale->getText(LOCALE_MOVIEPLAYER_TSHELP2));
	helpbox.addLine(NEUTRINO_ICON_BUTTON_YELLOW, g_Locale->getText(LOCALE_MOVIEPLAYER_TSHELP3));
	helpbox.addLine(NEUTRINO_ICON_BUTTON_BLUE, g_Locale->getText(LOCALE_MOVIEPLAYER_TSHELP4));
	helpbox.addLine(NEUTRINO_ICON_BUTTON_MENU, g_Locale->getText(LOCALE_MOVIEPLAYER_TSHELP5));
	helpbox.addLine(NEUTRINO_ICON_BUTTON_1, g_Locale->getText(LOCALE_MOVIEPLAYER_TSHELP6));
	helpbox.addLine(NEUTRINO_ICON_BUTTON_3, g_Locale->getText(LOCALE_MOVIEPLAYER_TSHELP7));
	helpbox.addLine(NEUTRINO_ICON_BUTTON_4, g_Locale->getText(LOCALE_MOVIEPLAYER_TSHELP8));
	helpbox.addLine(NEUTRINO_ICON_BUTTON_6, g_Locale->getText(LOCALE_MOVIEPLAYER_TSHELP9));
	helpbox.addLine(NEUTRINO_ICON_BUTTON_7, g_Locale->getText(LOCALE_MOVIEPLAYER_TSHELP10));
	helpbox.addLine(NEUTRINO_ICON_BUTTON_9, g_Locale->getText(LOCALE_MOVIEPLAYER_TSHELP11));
	helpbox.addLine(g_Locale->getText(LOCALE_MOVIEPLAYER_TSHELP12));
	helpbox.show(LOCALE_MESSAGEBOX_INFO);
}
